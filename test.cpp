#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "ft8/text.h"
//#include "ft8/v1/pack.h"
//#include "ft8/v1/unpack.h"
//#include "ft8/v1/encode.h"
#include "ft8/pack.h"
#include "ft8/encode.h"
#include "ft8/constants.h"

#include "fft/kiss_fftr.h"
#include "common/debug.h"

#define LOG_LEVEL   LOG_INFO


void convert_8bit_to_6bit(uint8_t *dst, const uint8_t *src, int nBits) {
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

/*
bool test1() {
    //const char *msg = "CQ DL7ACA JO40"; // 62, 32, 32, 49, 37, 27, 59, 2, 30, 19, 49, 16
    const char *msg = "VA3UG   F1HMR 73"; // 52, 54, 60, 12, 55, 54, 7, 19, 2, 23, 59, 16
    //const char *msg = "RA3Y VE3NLS 73";   // 46, 6, 32, 22, 55, 20, 11, 32, 53, 23, 59, 16
    uint8_t a72[9];

    int rc = packmsg(msg, a72);
    if (rc < 0) return false;
  
    LOG(LOG_INFO, "8-bit packed: ");
    for (int i = 0; i < 9; ++i) {
        LOG(LOG_INFO, "%02x ", a72[i]);
    }
    LOG(LOG_INFO, "\n");

    uint8_t a72_6bit[12];
    convert_8bit_to_6bit(a72_6bit, a72, 72);
    LOG(LOG_INFO, "6-bit packed: ");
    for (int i = 0; i < 12; ++i) {
        LOG(LOG_INFO, "%d ", a72_6bit[i]);
    }
    LOG(LOG_INFO, "\n");

    char msg_out_raw[14];
    unpack(a72, msg_out_raw);

    char msg_out[14];
    fmtmsg(msg_out, msg_out_raw);
    LOG(LOG_INFO, "msg_out = [%s]\n", msg_out);
    return true;
}


void test2() {
    uint8_t test_in[11] = { 0xF1, 0x02, 0x03, 0x04, 0x05, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xFF };
    uint8_t test_out[22];

    encode174(test_in, test_out);

    for (int j = 0; j < 22; ++j) {
        LOG(LOG_INFO, "%02x ", test_out[j]);
    }
    LOG(LOG_INFO, "\n");
}


void test3() {
    uint8_t test_in2[10] = { 0x11, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x10, 0x04, 0x01, 0x00 };
    uint16_t crc1 = ft8_crc(test_in2, 76);  // Calculate CRC of 76 bits only
    LOG(LOG_INFO, "CRC: %04x\n", crc1);            // should be 0x0708
}
*/

void test_tones(float *log174) {
    // Just a test case
    for (int i = 0; i < ft8::ND; ++i) {
        const uint8_t inv_map[8] = {0, 1, 3, 2, 6, 4, 5, 7};
        uint8_t tone = ("0000000011721762454112705354533170166234757420515470163426"[i]) - '0';
        uint8_t b3 = inv_map[tone];
        log174[3 * i]     = (b3 & 4) ? +1.0 : -1.0;
        log174[3 * i + 1] = (b3 & 2) ? +1.0 : -1.0;
        log174[3 * i + 2] = (b3 & 1) ? +1.0 : -1.0;
    }
}


void test4() {
    const int nfft = 128;
    const float fft_norm = 2.0 / nfft;

    size_t  fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

    printf("N_FFT = %d\n", nfft);
    printf("FFT work area = %lu\n", fft_work_size);

    void        *fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);

    kiss_fft_scalar   window[nfft];
    for (int i = 0; i < nfft; ++i) {
        window[i] = sinf(i * 2 * (float)M_PI / nfft);
    }

    kiss_fft_cpx    freqdata[nfft/2 + 1];
    kiss_fftr(fft_cfg, window, freqdata);

    float mag_db[nfft];
    // Compute log magnitude in decibels
    for (int j = 0; j < nfft/2 + 1; ++j) {
        float mag2 = (freqdata[j].i * freqdata[j].i + freqdata[j].r * freqdata[j].r);
        mag_db[j] = 10.0f * log10f(1E-10f + mag2 * fft_norm * fft_norm);
    }    

    printf("F[0] = %.1f dB\n", mag_db[0]);
    printf("F[1] = %.3f dB\n", mag_db[1]);
}


int main() {
    //test1();
    test4();

    return 0;
}