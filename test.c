#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ft8/constants.h"
#include "ft8/pack.h"
#include "ft8/text.h"

#include "common/debug.h"
#include "fft/kiss_fftr.h"

void convert_8bit_to_6bit(uint8_t *dst, const uint8_t *src, int nBits)
{
    // Zero-fill the destination array as we will only be setting bits later
    for (int j = 0; j < (nBits + 5) / 6; ++j) {
        dst[j] = 0;
    }
    
    // Set the relevant bits
    uint8_t mask_src = (1 << 7);
    uint8_t mask_dst = (1 << 5);
    for (int i = 0, j = 0; nBits > 0; --nBits) {
        if (src[i] & mask_src) {
            dst[j] |= mask_dst;
        }
        mask_src >>= 1;
        if (mask_src == 0) {
            mask_src = (1 << 7);
            ++i;
        }
        mask_dst >>= 1;
        if (mask_dst == 0) {
            mask_dst = (1 << 5);
            ++j;
        }
    }
}

void test_tones(float *log174)
{
    // Just a test case
    for (int i = 0; i < FT8_ND; ++i) {
        const uint8_t inv_map[8] = { 0, 1, 3, 2, 6, 4, 5, 7 };
        uint8_t tone = ("0000000011721762454112705354533170166234757420515470163426"[i]) - '0';
        uint8_t b3 = inv_map[tone];
        log174[3 * i] = (b3 & 4) ? +1.0 : -1.0;
        log174[3 * i + 1] = (b3 & 2) ? +1.0 : -1.0;
        log174[3 * i + 2] = (b3 & 1) ? +1.0 : -1.0;
    }
}

void test4(void)
{
    const int nfft = 128;
    const float fft_norm = 2.0 / nfft;
    
    size_t fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);
    
    printf("N_FFT = %d\n", nfft);
    printf("FFT work area = %lu\n", fft_work_size);
    
    void* fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);
    
    kiss_fft_scalar window[nfft];
    for (int i = 0; i < nfft; ++i) {
        window[i] = sinf(i * 2 * (float)M_PI / nfft);
    }
    
    kiss_fft_cpx freqdata[nfft / 2 + 1];
    kiss_fftr(fft_cfg, window, freqdata);
    
    float mag_db[nfft];
    // Compute log magnitude in decibels
    for (int j = 0; j < nfft / 2 + 1; ++j) {
        float mag2 = (freqdata[j].i * freqdata[j].i + freqdata[j].r * freqdata[j].r);
        mag_db[j] = 10.0f * log10f(1E-10f + mag2 * fft_norm * fft_norm);
    }
    
    printf("F[0] = %.1f dB\n", mag_db[0]);
    printf("F[1] = %.3f dB\n", mag_db[1]);
}

int main()
{
    // test1();
    test4();
    
    return 0;
}

