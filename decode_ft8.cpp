#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "common/wave.h"
#include "ft8/pack.h"
#include "ft8/encode.h"
#include "ft8/pack_v2.h"
#include "ft8/encode_v2.h"

#include "ft8/ldpc.h"
#include "fft/kiss_fftr.h"


void usage() {
    printf("Decode a 15-second WAV file.\n");
}


float hann_i(int i, int N) {
    float x = sinf((float)M_PI * i / (N - 1));
    return x*x;
}


struct Candidate {
    int16_t      score;
    uint16_t     time_offset;
    uint16_t     freq_offset;
    uint8_t      time_sub;
    uint8_t      freq_sub;
};


void heapify_down(Candidate * heap, int heap_size) {
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


void heapify_up(Candidate * heap, int heap_size) {
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
void find_sync(const uint8_t * power, int num_blocks, int num_bins, int num_candidates, Candidate * heap) {
    // Costas 7x7 tone pattern
    const uint8_t ICOS7[] = { 2,5,6,0,4,1,3 };

    int heap_size = 0;

    for (int alt = 0; alt < 4; ++alt) {
        for (int time_offset = 0; time_offset < num_blocks - NN; ++time_offset) {
            for (int freq_offset = 0; freq_offset < num_bins - 8; ++freq_offset) {
                int score = 0;

                // Compute score over Costas symbols (0-7, 36-43, 72-79)
                for (int m = 0; m <= 72; m += 36) {
                    for (int k = 0; k < 7; ++k) {
                        int offset = ((time_offset + k + m) * 4 + alt) * num_bins + freq_offset;
                        score += 8 * (int)power[offset + ICOS7[k]] -
                                    power[offset + 0] - power[offset + 1] - 
                                    power[offset + 2] - power[offset + 3] - 
                                    power[offset + 4] - power[offset + 5] - 
                                    power[offset + 6] - power[offset + 7];
                    }
                }

                // If the heap is full AND the current candidate is better than 
                // the worst of the heap, we remove the worst and make space
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
}


// Compute FFT magnitudes (log power) for each timeslot in the signal
void extract_power(const float * signal, int num_blocks, int num_bins, uint8_t * power) {
    const int block_size = 2 * num_bins;      // Average over 2 bins per FSK tone
    const int nfft = 2 * block_size;          // We take FFT of two blocks, advancing by one

    float   window[nfft];
    for (int i = 0; i < nfft; ++i) {
        window[i] = hann_i(i, nfft);
    }

    size_t  fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

    printf("N_FFT = %d\n", nfft);
    printf("FFT work area = %lu\n", fft_work_size);

    void *  fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);

    int offset = 0;
    float fft_norm = 1.0f / nfft;
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
                float mag2 = fft_norm * (freqdata[j].i * freqdata[j].i + freqdata[j].r * freqdata[j].r);
                mag_db[j] = 10.0f * log10f(1.0E-10f + mag2);
            }

            // Loop over two possible frequency bin offsets (for averaging)
            for (int freq_sub = 0; freq_sub < 2; ++freq_sub) {                
                for (int j = 0; j < num_bins; ++j) {
                    float db1 = mag_db[j * 2 + freq_sub];
                    float db2 = mag_db[j * 2 + freq_sub + 1];
                    float db = (db1 + db2) / 2;

                    // Scale decibels to unsigned 8-bit range
                    int scaled = (int)(2 * (db + 100));
                    power[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
                    ++offset;
                }
            }
        }
    }

    free(fft_work);
}


uint8_t max2(uint8_t a, uint8_t b) {
    return (a >= b) ? a : b;
}


uint8_t max4(uint8_t a, uint8_t b, uint8_t cand, uint8_t d) {
    return max2(max2(a, b), max2(cand, d));
}


// Compute log likelihood log(p(1) / p(0)) of 174 message bits 
// for later use in soft-decision LDPC decoding
void extract_likelihood(const uint8_t * power, int num_bins, const Candidate & cand, float * log174) {
    int offset = (cand.time_offset * 4 + cand.time_sub * 2 + cand.freq_sub) * num_bins + cand.freq_offset;

    int k = 0;
    // Go over FSK tones and skip Costas sync symbols
    for (int i = 7; i < NN - 7; ++i) {
        if (i == 36) i += 7;

        // Pointer to 8 bins of the current symbol
        const uint8_t * ps = power + (offset + i * 4 * num_bins);

        // Extract bit significance (and convert them to float)
        // 8 FSK tones = 3 bits
        log174[k + 0] = (int)max4(ps[4], ps[5], ps[6], ps[7]) - (int)max4(ps[0], ps[1], ps[2], ps[3]);
        log174[k + 1] = (int)max4(ps[2], ps[3], ps[6], ps[7]) - (int)max4(ps[0], ps[1], ps[4], ps[5]);
        log174[k + 2] = (int)max4(ps[1], ps[3], ps[5], ps[7]) - (int)max4(ps[0], ps[2], ps[4], ps[6]);
        // printf("%d %d %d %d %d %d %d %d : %.0f %.0f %.0f\n", 
        //         ps[0], ps[1], ps[2], ps[3], ps[4], ps[5], ps[6], ps[7], 
        //         log174[k + 0], log174[k + 1], log174[k + 2]);
        k += 3;
    }

    // Compute the variance of log174
    float sum   = 0;
    float sum2  = 0;
    float inv_n = 1.0f / (3 * ND);
    for (int i = 0; i < 3 * ND; ++i) {
        sum  += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float var = (sum2 - sum * sum * inv_n) * inv_n;

    // Normalize log174 such that sigma = 2.83 (Why? It's in WSJT-X)
    float norm_factor = 2.83f / sqrtf(var);

    for (int i = 0; i < 3 * ND; ++i) {
        log174[i] *= norm_factor;
        //printf("%.1f ", log174[i]);
    }
    //printf("\n");
}

int main(int argc, char ** argv) {
    // Expect one command-line argument
    if (argc < 2) {
        usage();
        return -1;
    }

    const char * wav_path = argv[1];

    int sample_rate = 12000;
    int num_samples = 15 * sample_rate;
    float signal[num_samples];

    int rc = load_wav(signal, num_samples, sample_rate, wav_path);
    if (rc < 0) {
        return -1;
    }

    const float fsk_dev = 6.25f;

    const int num_bins = (int)(sample_rate / (2 * fsk_dev));
    const int block_size = 2 * num_bins;
    const int num_blocks = (num_samples - (block_size/2) - block_size) / block_size;
    uint8_t power[num_blocks * 4 * num_bins];   // [num_blocks][4][num_bins] ~ 200 KB

    printf("%d blocks, %d bins\n", num_blocks, num_bins);

    extract_power(signal, num_blocks, num_bins, power);

    int num_candidates = 250;
    Candidate heap[num_candidates];

    find_sync(power, num_blocks, num_bins, num_candidates, heap);

    for (int idx = 0; idx < num_candidates; ++idx) {
        Candidate &cand = heap[idx];
        float freq_hz = (cand.freq_offset + cand.freq_sub / 2.0f) * fsk_dev;
        float time_sec = (cand.time_offset + cand.time_sub / 2.0f) / fsk_dev;
        // printf("%03d: score = %d freq = %.1f time = %.2f\n", i, 
        //         heap[i].score, freq_hz, time_sec);

        float log174[3 * ND];
        extract_likelihood(power, num_bins, cand, log174);

        const int num_iters = 20;
        int plain[3 * ND];
        int ok;

        bp_decode(log174, num_iters, plain, &ok);
        //ldpc_decode(log174, num_iters, plain, &ok);
        //printf("ldpc_decode() = %d\n", ok);
        if (ok == 87) {
            printf("%03d: score = %d freq = %.1f time = %.2f\n", idx, 
                    cand.score, freq_hz, time_sec);
        }
    }

    return 0;
}