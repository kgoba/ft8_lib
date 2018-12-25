#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "ft8/unpack_v2.h"
#include "ft8/ldpc.h"
#include "ft8/constants.h"

#include "common/wave.h"
#include "fft/kiss_fftr.h"

const int kMax_candidates = 100;
const int kLDPC_iterations = 20;

const int kMax_decoded_messages = 50;
const int kMax_message_length = 20;

void usage() {
    printf("Decode a 15-second WAV file.\n");
}


float hann_i(int i, int N) {
    float x = sinf((float)M_PI * i / (N - 1));
    return x*x;
}


float hamming_i(int i, int N) {
    const float a0 = (float)25 / 46;
    const float a1 = 1 - a0;

    float x1 = cosf(2 * (float)M_PI * i / (N - 1));
    return a0 - a1*x1;
}


float blackman_i(int i, int N) {
    const float alpha = 0.16f; // or 2860/18608
    const float a0 = (1 - alpha) / 2;
    const float a1 = 1.0f / 2;
    const float a2 = alpha / 2;

    float x1 = cosf(2 * (float)M_PI * i / (N - 1));
    float x2 = cosf(4 * (float)M_PI * i / (N - 1));

    return a0 - a1*x1 + a2*x2;
}


struct Candidate {
    int16_t      score;
    int16_t      time_offset;
    int16_t      freq_offset;
    uint8_t      time_sub;
    uint8_t      freq_sub;
};


void heapify_down(Candidate *heap, int heap_size) {
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


void heapify_up(Candidate *heap, int heap_size) {
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


// Find top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
// We treat and organize the candidate list as a min-heap (empty initially).
int find_sync(const uint8_t *power, int num_blocks, int num_bins, const uint8_t *sync_map, int num_candidates, Candidate *heap) {
    int heap_size = 0;

    for (int alt = 0; alt < 4; ++alt) {
        for (int time_offset = -7; time_offset < num_blocks - FT8_NN + 7; ++time_offset) {
            for (int freq_offset = 0; freq_offset < num_bins - 8; ++freq_offset) {
                int score = 0;

                // Compute score over Costas symbols (0-7, 36-43, 72-79)
                int num_scores = 0;
                for (int m = 0; m <= 72; m += 36) {
                    for (int k = 0; k < 7; ++k) {
                        if (time_offset + k + m < 0) continue;
                        if (time_offset + k + m >= num_blocks) break;
                        int offset = ((time_offset + k + m) * 4 + alt) * num_bins + freq_offset;
                        score += 8 * (int)power[offset + sync_map[k]] -
                                    power[offset + 0] - power[offset + 1] - 
                                    power[offset + 2] - power[offset + 3] - 
                                    power[offset + 4] - power[offset + 5] - 
                                    power[offset + 6] - power[offset + 7];
                        ++num_scores;
                    }
                }
                score /= num_scores;

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
                    heap[heap_size].time_sub = alt / 2;
                    heap[heap_size].freq_sub = alt % 2;
                    ++heap_size;

                    heapify_up(heap, heap_size);
                }
            }
        }
    }

    return heap_size;
}


// Compute FFT magnitudes (log power) for each timeslot in the signal
void extract_power(const float *signal, int num_blocks, int num_bins, uint8_t *power) {
    const int block_size = 2 * num_bins;      // Average over 2 bins per FSK tone
    const int nfft = 2 * block_size;          // We take FFT of two blocks, advancing by one

    float   window[nfft];
    for (int i = 0; i < nfft; ++i) {
        window[i] = blackman_i(i, nfft);
    }

    size_t  fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

    printf("N_FFT = %d\n", nfft);
    printf("FFT work area = %lu\n", fft_work_size);

    void *       fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);

    int offset = 0;
    float fft_norm = 1.0f / nfft;
    float max_mag = -100.0f;
    for (int i = 0; i < num_blocks; ++i) {
        // Loop over two possible time offsets (0 and block_size/2)
        for (int time_sub = 0; time_sub <= block_size/2; time_sub += block_size/2) {
            kiss_fft_scalar timedata[nfft];
            kiss_fft_cpx    freqdata[nfft/2 + 1];
            float           mag_db[nfft/2 + 1];

            // Extract windowed signal block
            for (int j = 0; j < nfft; ++j) {
                timedata[j] = window[j] * signal[(i * block_size) + (j + time_sub)];
            }

            kiss_fftr(fft_cfg, timedata, freqdata);

            // Compute log magnitude in decibels
            for (int j = 0; j < nfft/2 + 1; ++j) {
                float mag2 = (freqdata[j].i * freqdata[j].i + freqdata[j].r * freqdata[j].r);
                mag_db[j] = 10.0f * log10f(1.0E-10f + mag2 * fft_norm);
            }

            // Loop over two possible frequency bin offsets (for averaging)
            for (int freq_sub = 0; freq_sub < 2; ++freq_sub) {                
                for (int j = 0; j < num_bins; ++j) {
                    float db1 = mag_db[j * 2 + freq_sub];
                    float db2 = mag_db[j * 2 + freq_sub + 1];
                    float db = (db1 + db2) / 2;

                    // Scale decibels to unsigned 8-bit range and clamp the value
                    int scaled = (int)(2 * (db + 100));
                    power[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
                    ++offset;

                    if (db > max_mag) max_mag = db;
                }
            }
        }
    }

    printf("Max magnitude: %.1f dB\n", max_mag);
    free(fft_work);
}


float max2(float a, float b) {
    return (a >= b) ? a : b;
}


float max4(float a, float b, float c, float d) {
    return max2(max2(a, b), max2(c, d));
}


// Compute log likelihood log(p(1) / p(0)) of 174 message bits 
// for later use in soft-decision LDPC decoding
void extract_likelihood(const uint8_t *power, int num_bins, const Candidate & cand, const uint8_t *code_map, float *log174) {
    int offset = (cand.time_offset * 4 + cand.time_sub * 2 + cand.freq_sub) * num_bins + cand.freq_offset;

    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);
    for (int k = 0; k < FT8_ND; k += n_syms) {
        int sym_idx = (k < FT8_ND / 2) ? (k + 7) : (k + 14);

        // Pointer to 8 bins of the current symbol
        const uint8_t *ps = power + (offset + sym_idx * 4 * num_bins);
        float s2[n_tones];

        for (int j = 0; j < n_tones; ++j) {
            int j1 = j & 0x07;
            s2[j] = (float)ps[code_map[j1]];
            //int j2 = (j >> 3) & 0x07;
            //s2[j] = (float)ps[code_map[j2]];
            //s2[j] += (float)ps[code_map[j1] + 4 * num_bins];
        }

        // Extract bit significance (and convert them to float)
        // 8 FSK tones = 3 bits
        int bit_idx = 3 * k;
        for (int i = 0; i < n_bits; ++i) {
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
            if (bit_idx + i >= 174) break;
            log174[bit_idx + i] = max_one - max_zero;
        }
        // log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
        // log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
        // log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
    }

    // Compute the variance of log174
    float sum   = 0;
    float sum2  = 0;
    float inv_n = 1.0f / FT8_N;
    for (int i = 0; i < FT8_N; ++i) {
        sum  += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float variance = (sum2 - sum * sum * inv_n) * inv_n;

    // Normalize log174 such that sigma = 2.83 (Why? It's in WSJT-X)
    float norm_factor = 3.83f / sqrtf(variance);
    for (int i = 0; i < FT8_N; ++i) {
        log174[i] *= norm_factor;
        //printf("%.1f ", log174[i]);
    }
    //printf("\n");
}


void test_tones(float *log174) {
    for (int i = 0; i < FT8_ND; ++i) {
        const uint8_t inv_map[8] = {0, 1, 3, 2, 6, 4, 5, 7};
        uint8_t tone = ("0000000011721762454112705354533170166234757420515470163426"[i]) - '0';
        uint8_t b3 = inv_map[tone];
        log174[3 * i]     = (b3 & 4) ? +1.0 : -1.0;
        log174[3 * i + 1] = (b3 & 2) ? +1.0 : -1.0;
        log174[3 * i + 2] = (b3 & 1) ? +1.0 : -1.0;
    }    
    // 3140652 00000000117217624541127053545 3140652 33170166234757420515470163426 3140652
    // 0000000011721762454112705354533170166234757420515470163426
    // 0000000011721762454112705454544170166344757430515470073537
    // 0000000011711761444111704343433170166233747320414370072427
    // 0000000011711761454111705353533170166233757320515370072527
}


void print_tones(const uint8_t *code_map, const float *log174) {
    for (int k = 0; k < 3 * FT8_ND; k += 3) {
        uint8_t max = 0;
        if (log174[k + 0] > 0) max |= 4;
        if (log174[k + 1] > 0) max |= 2;
        if (log174[k + 2] > 0) max |= 1;
        printf("%d", code_map[max]);
    }
    printf("\n");
}


int main(int argc, char **argv) {
    // Expect one command-line argument
    if (argc < 2) {
        usage();
        return -1;
    }

    const char *wav_path = argv[1];

    int sample_rate = 12000;
    int num_samples = 15 * sample_rate;
    float signal[num_samples];

    int rc = load_wav(signal, num_samples, sample_rate, wav_path);
    if (rc < 0) {
        return -1;
    }

    const float fsk_dev = 6.25f;    // tone deviation in Hz and symbol rate

    // Compute DSP parameters that depend on the sample rate
    const int num_bins = (int)(sample_rate / (2 * fsk_dev));
    const int block_size = 2 * num_bins;
    const int num_blocks = (num_samples - (block_size/2) - block_size) / block_size;

    printf("%d blocks, %d bins\n", num_blocks, num_bins);

    // Compute FFT over the whole signal and store it
    uint8_t power[num_blocks * 4 * num_bins];
    extract_power(signal, num_blocks, num_bins, power);

    Candidate heap[kMax_candidates];
    char    decoded[kMax_decoded_messages][kMax_message_length];
    int     num_decoded = 0;

    int num_candidates = find_sync(power, num_blocks, num_bins, kCostas_map, kMax_candidates, heap);

    for (int idx = 0; idx < num_candidates; ++idx) {
        Candidate &cand = heap[idx];

        float   log174[FT8_N];
        extract_likelihood(power, num_bins, cand, kGray_map, log174);

        // bp_decode() produces better decodes, uses way less memory
        uint8_t plain[FT8_N];
        int     n_errors = 0;
        bp_decode(log174, kLDPC_iterations, plain, &n_errors);
        //ldpc_decode(log174, num_iters, plain, &n_errors);

        if (n_errors > 0) {
            //printf("ldpc_decode() = %d\n", n_errors);
            continue;
        }

        float freq_hz  = (cand.freq_offset + cand.freq_sub / 2.0f) * fsk_dev;
        float time_sec = (cand.time_offset + cand.time_sub / 2.0f) / fsk_dev;

        // printf("%03d: score = %d freq = %.1f time = %.2f\n", idx, 
        //         cand.score, freq_hz, time_sec);

        //print_tones(kGray_map, log174);
        
        // Extract payload + CRC
        uint8_t a91[12];
        uint8_t mask = 0x80;
        int     byte_idx = 0;
        for (int i = 0; i < 12; ++i) {
            a91[i] = 0;
        }
        for (int i = 0; i < FT8_K; ++i) {
            if (plain[i]) {
                a91[byte_idx] |= mask;
            }
            mask >>= 1;
            if (!mask) {
                mask = 0x80;
                ++byte_idx;
            }
        }

        // TODO: check CRC

        // for (int i = 0; i < 12; ++i) {
        //     printf("%02x ", a91[i]);
        // }
        // printf("\n");

        char message[kMax_message_length];
        unpack77(a91, message);

        // Check for duplicate messages
        bool found = false;
        for (int i = 0; i < num_decoded; ++i) {
            if (0 == strcmp(decoded[i], message)) {
                found = true;
                break;
            }
        }

        if (!found && num_decoded < kMax_decoded_messages) {
            strcpy(decoded[num_decoded], message);
            ++num_decoded;

            // Fake WSJT-X-like output for now
            int snr = 0;    // TODO: compute SNR
            printf("000000 %3d %4.1f %4d ~  %s\n", snr, time_sec, (int)(freq_hz + 0.5f), message);
        }
    }
    printf("Decoded %d messages\n", num_decoded);

    return 0;
}