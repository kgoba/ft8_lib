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

    //return 0;

    const int nfft = 2 * (int)(sample_rate / 6.25);     // 2 bins per FSK tone

    size_t fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

    printf("N_FFT = %d\n", nfft);
    printf("FFT work area = %lu\n", fft_work_size);

    void *fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);

    kiss_fft_scalar timedata[nfft];
    kiss_fft_cpx    freqdata[nfft/2 + 1];
    kiss_fftr(fft_cfg, timedata, freqdata);

    return 0;
}