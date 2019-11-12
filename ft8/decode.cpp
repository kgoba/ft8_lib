#include "decode.h"

#include <math.h>

#include "constants.h"

namespace ft8 {

static float max2(float a, float b);
static float max4(float a, float b, float c, float d);
static void heapify_down(Candidate *heap, int heap_size);
static void heapify_up(Candidate *heap, int heap_size);
static void decode_symbol(const uint8_t *power, const uint8_t *code_map, int bit_idx, float *log174);
static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, const uint8_t *code_map, int bit_idx, float *log174);

static int get_index(const MagArray *power, int block, int time_sub, int freq_sub, int bin) {
    return ((((block * power->time_osr) + time_sub) * power->freq_osr + freq_sub) * power->num_bins) + bin; 
}

// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
// We treat and organize the candidate list as a min-heap (empty initially).
int find_sync(const MagArray *power, const uint8_t *sync_map, int num_candidates, Candidate *heap, int min_score) {
    int heap_size = 0;
    int num_alt = power->time_osr * power->freq_osr;

    // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
    // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
    // sync symbols we included in the score, so the score is averaged.
    for (int time_sub = 0; time_sub < power->time_osr; ++time_sub) {
        for (int freq_sub = 0; freq_sub < power->freq_osr; ++freq_sub) {
            for (int time_offset = -7; time_offset < power->num_blocks - ft8::NN + 7; ++time_offset) {
                for (int freq_offset = 0; freq_offset < power->num_bins - 8; ++freq_offset) {
                    int score = 0;

                    // Compute average score over sync symbols (m+k = 0-7, 36-43, 72-79)
                    int num_symbols = 0;
                    for (int m = 0; m <= 72; m += 36) {
                        for (int k = 0; k < 7; ++k) {
                            // Check for time boundaries
                            if (time_offset + k + m < 0) continue;
                            if (time_offset + k + m >= power->num_blocks) break;

                            // int offset = ((time_offset + k + m) * num_alt + alt) * power->num_bins + freq_offset;
                            int offset = get_index(power, time_offset + k + m, time_sub, freq_sub, freq_offset);
                            const uint8_t *p8 = power->mag + offset;

                            // Weighted difference between the expected and all other symbols
                            // Does not work as well as the alternative score below
                            // score += 8 * p8[sync_map[k]] -
                            //              p8[0] - p8[1] - p8[2] - p8[3] -
                            //              p8[4] - p8[5] - p8[6] - p8[7];

                            // Check only the neighbors of the expected symbol frequency- and time-wise
                            int sm = sync_map[k];   // Index of the expected bin
                            if (sm > 0) {
                                // look at one frequency bin lower
                                score += p8[sm] - p8[sm - 1];
                            }
                            if (sm < 7) {
                                // look at one frequency bin higher
                                score += p8[sm] - p8[sm + 1];
                            }
                            if (k > 0) {
                                // look one symbol back in time
                                score += p8[sm] - p8[sm - num_alt * power->num_bins];
                            }
                            if (k < 6) {
                                // look one symbol forward in time
                                score += p8[sm] - p8[sm + num_alt * power->num_bins];
                            }
                        
                            ++num_symbols;
                        }
                    }
                    score /= num_symbols;

                    if (score < min_score) continue;

                    // If the heap is full AND the current candidate is better than 
                    // the worst in the heap, we remove the worst and make space
                    if (heap_size == num_candidates && score > heap[0].score) {
                        heap[0] = heap[heap_size - 1];
                        --heap_size;

                        heapify_down(heap, heap_size);
                    }

                    // If there's free space in the heap, we add the current candidate
                    if (heap_size < num_candidates) {
                        heap[heap_size].score = score;
                        heap[heap_size].time_offset = time_offset;
                        heap[heap_size].freq_offset = freq_offset;
                        heap[heap_size].time_sub = time_sub;
                        heap[heap_size].freq_sub = freq_sub;
                        ++heap_size;

                        heapify_up(heap, heap_size);
                    }
                }
            }
        }
    }

    return heap_size;
}


// Compute log likelihood log(p(1) / p(0)) of 174 message bits 
// for later use in soft-decision LDPC decoding
void extract_likelihood(const MagArray *power, const Candidate & cand, const uint8_t *code_map, float *log174) {
    int num_alt = power->time_osr * power->freq_osr;
    // int offset = (cand.time_offset * num_alt + cand.time_sub * power->freq_osr + cand.freq_sub) * power->num_bins + cand.freq_offset;
    int offset = get_index(power, cand.time_offset, cand.time_sub, cand.freq_sub, cand.freq_offset);

    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);
    for (int k = 0; k < ft8::ND; k += n_syms) {
        // Add either 7 or 14 extra symbols to account for sync
        int sym_idx = (k < ft8::ND / 2) ? (k + 7) : (k + 14);
        int bit_idx = 3 * k;

        // Pointer to 8 bins of the current symbol
        const uint8_t *ps = power->mag + (offset + sym_idx * num_alt * power->num_bins);

        decode_symbol(ps, code_map, bit_idx, log174);
    }

    // Compute the variance of log174
    float sum   = 0;
    float sum2  = 0;
    float inv_n = 1.0f / ft8::N;
    for (int i = 0; i < ft8::N; ++i) {
        sum  += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float variance = (sum2 - sum * sum * inv_n) * inv_n;

    // Normalize log174 such that sigma = 2.83 (Why? It's in WSJT-X, ft8b.f90)
    // Seems to be 2.83 = sqrt(8). Experimentally sqrt(16) works better.
    float norm_factor = sqrtf(16.0f / variance);
    for (int i = 0; i < ft8::N; ++i) {
        log174[i] *= norm_factor;
    }
}


static float max2(float a, float b) {
    return (a >= b) ? a : b;
}


static float max4(float a, float b, float c, float d) {
    return max2(max2(a, b), max2(c, d));
}


static void heapify_down(Candidate *heap, int heap_size) {
    // heapify from the root down
    int current = 0;
    while (true) {
        int largest = current;
        int left = 2 * current + 1;
        int right = left + 1;

        if (left < heap_size && heap[left].score < heap[largest].score) {
            largest = left;
        }
        if (right < heap_size && heap[right].score < heap[largest].score) {
            largest = right;
        }
        if (largest == current) {
            break;
        }

        Candidate tmp = heap[largest];
        heap[largest] = heap[current];
        heap[current] = tmp;
        current = largest;
    }
}


static void heapify_up(Candidate *heap, int heap_size) {
    // heapify from the last node up
    int current = heap_size - 1;
    while (current > 0) {
        int parent = (current - 1) / 2;
        if (heap[current].score >= heap[parent].score) {
            break;
        }

        Candidate tmp = heap[parent];
        heap[parent] = heap[current];
        heap[current] = tmp;
        current = parent;
    }
}


// Compute unnormalized log likelihood log(p(1) / p(0)) of 3 message bits (1 FSK symbol)
static void decode_symbol(const uint8_t *power, const uint8_t *code_map, int bit_idx, float *log174) {
    // Cleaned up code for the simple case of n_syms==1
    float s2[8];

    for (int j = 0; j < 8; ++j) {
        s2[j] = (float)power[code_map[j]];
    }

    log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
    log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
    log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
}


// Compute unnormalized log likelihood log(p(1) / p(0)) of bits corresponding to several FSK symbols at once
static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, const uint8_t *code_map, int bit_idx, float *log174) {
    // The following section implements what seems to be multiple-symbol decode at one go,
    // corresponding to WSJT-X's ft8b.f90. Experimentally found not to be any better than 
    // 1-symbol decode.

    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);
    
    float s2[n_tones];

    for (int j = 0; j < n_tones; ++j) {
        int j1 = j & 0x07;
        if (n_syms == 1) {
            s2[j] = (float)power[code_map[j1]];
            continue;
        }
        int j2 = (j >> 3) & 0x07;
        if (n_syms == 2) {
            s2[j] = (float)power[code_map[j2]];
            s2[j] += (float)power[code_map[j1] + 4 * num_bins];
            continue;
        }
        int j3 = (j >> 6) & 0x07;
        s2[j] = (float)power[code_map[j3]];
        s2[j] += (float)power[code_map[j2] + 4 * num_bins];
        s2[j] += (float)power[code_map[j1] + 8 * num_bins];
    }
    // No need to go back to linear scale any more. Works better in dB.
    // for (int j = 0; j < n_tones; ++j) {
    //     s2[j] = powf(10.0f, 0.1f * s2[j]);
    // }

    // Extract bit significance (and convert them to float)
    // 8 FSK tones = 3 bits
    for (int i = 0; i < n_bits; ++i) {
        if (bit_idx + i >= ft8::N) {
            // Respect array size
            break;
        }

        uint16_t mask = (n_tones >> (i + 1));
        float max_zero = -1000, max_one = -1000;
        for (int n = 0; n < n_tones; ++n) {
            if (n & mask) {
                max_one = max2(max_one, s2[n]);
            }
            else {
                max_zero = max2(max_zero, s2[n]);
            }
        }

        log174[bit_idx + i] = max_one - max_zero;
    }    
}

} // namespace
