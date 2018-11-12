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
    uint8_t      time_alt;
    uint8_t      freq_alt;
};


void find_candidates(int num_blocks, int num_bins, const uint8_t * power, 
                        int num_candidates, Candidate heap[num_candidates]) {
    // Costas 7x7 tone pattern
    const uint8_t ICOS7[] = { 2,5,6,0,4,1,3 };

    int heap_size = 0;

    for (int alt = 0; alt < 4; ++alt) {
        for (int i = 0; i < num_blocks - NN; ++i) {
            for (int j = 0; j < num_bins - 8; ++j) {
                int score = 0;

                // Compute score over bins 0-7, 36-43, 72-79
                for (int m = 0; m <= 72; m += 36) {
                    for (int k = 0; k < 7; ++k) {
                        int offset = ((i + k + m) * 4 + alt) * num_bins + j;
                        // score += 8 * (int)power[i + k + m][alt][j + ICOS7[k]] -
                        score += 8 * (int)power[offset + ICOS7[k]] -
                                    power[offset + 0] - power[offset + 1] - 
                                    power[offset + 2] - power[offset + 3] - 
                                    power[offset + 4] - power[offset + 5] - 
                                    power[offset + 6] - power[offset + 7];
                    }
                }

                // update the candidate list
                if (heap_size == num_candidates && score > heap[0].score) {
                    //printf("Removing score %d\n", heap[0].score);
                    // extract the least promising candidate
                    heap[0] = heap[heap_size - 1];
                    --heap_size;

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

                if (heap_size < num_candidates) {
                    // add the current candidate
                    //printf("Adding score %d\n", score);
                    heap[heap_size].score = score;
                    heap[heap_size].time_offset = i;
                    heap[heap_size].freq_offset = j;
                    ++heap_size;

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
            }
        }
    }
}


void extract_power(const float *signal, int num_samples, int num_bins, uint8_t * power) {
    const int block_size = 2 * num_bins;      // Average over 2 bins per FSK tone
    const int nfft = 2 * block_size;          // We take FFT of two blocks, advancing by one
    const int num_blocks = (num_samples - (block_size/2) - block_size) / block_size;

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
    for (int i = 0; i < num_blocks; ++i) {
        // Loop over two possible time offsets (0 and block_size/2)
        for (int time_offset = 0; time_offset <= block_size/2; time_offset += block_size/2) {
            kiss_fft_scalar timedata[nfft];
            kiss_fft_cpx    freqdata[nfft/2 + 1];
            float           mag_db[nfft/2 + 1];

            // Extract windowed signal block
            for (int j = 0; j < nfft; ++j) {
                timedata[j] = window[j] * signal[i * block_size + j + time_offset];
            }

            kiss_fftr(fft_cfg, timedata, freqdata);

            // Compute log magnitude in decibels
            for (int j = 0; j < nfft/2 + 1; ++j) {
                float mag2 = (freqdata[j].i * freqdata[j].i + freqdata[j].r * freqdata[j].r);
                mag_db[j] = 10.0f * logf(1.0E-10f + mag2);
            }

            // Loop over two possible frequency bin offsets (for averaging)
            for (int freq_offset = 0; freq_offset <= 1; ++freq_offset) {                
                for (int j = 0; j < num_bins; ++j) {
                    float db1 = mag_db[j * 2 + freq_offset];
                    float db2 = mag_db[j * 2 + freq_offset + 1];
                    float db = (db1 + db2) / 2;

                    // Scale decibels to unsigned 8-bit range
                    int scaled = (int)(0.5f + 2 * (db + 100));
                    power[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
                    ++offset;
                }
            }
        }
    }

    free(fft_work);
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

    const int num_bins = (int)(sample_rate / 2 / 6.25);
    const int block_size = 2 * num_bins;
    const int num_blocks = (num_samples - (block_size/2) - block_size) / block_size;

    uint8_t power[num_blocks * 4 * num_bins];   // [num_blocks][4][num_bins] ~ 200 KB

    extract_power(signal, num_samples, num_bins, power);

    const int num_candidates = 200;
    Candidate heap[num_candidates];

    find_candidates(num_blocks, num_bins, power, num_candidates, heap);

    return 0;
}