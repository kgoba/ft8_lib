#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "common/wave.h"
#include "common/debug.h"
//#include "ft8/v1/pack.h"
//#include "ft8/v1/encode.h"
#include "ft8/pack.h"
#include "ft8/encode.h"
#include "ft8/constants.h"

#define LOG_LEVEL   LOG_INFO

void gfsk_pulse(int n_spsym, float b, float *pulse) {
    const float c = M_PI * sqrtf(2 / logf(2));

    for (int i = 0; i < 3*n_spsym; ++i) {
        float t = i/(float)n_spsym - 1.5f;
        pulse[i] = (erff(c * b * (t + 0.5f)) - erff(c * b * (t - 0.5f))) / 2;
    }
}

// Same as synth_fsk, but uses GFSK phase shaping
void synth_gfsk(const uint8_t *symbols, int n_sym, float f0, int n_spsym, int signal_rate, float *signal) 
{
    LOG(LOG_DEBUG, "n_spsym = %d\n", n_spsym);
    int n_wave = n_sym * n_spsym;
    float hmod = 1.0f;

    // Compute the smoothed frequency waveform.
    // Length = (nsym+2)*nsps samples, first and last symbols extended 
    float dphi_peak = 2 * M_PI * hmod / n_spsym;
    float dphi[n_wave + 2*n_spsym];

    // Shift frequency up by f0
    for (int i = 0; i < n_wave + 2*n_spsym; ++i) {    
        dphi[i] = 2 * M_PI * f0 / signal_rate;
    }

    float pulse[3 * n_spsym];
    gfsk_pulse(n_spsym, 2.0f, pulse);

    for (int i = 0; i < n_sym; ++i) {
        int ib = i * n_spsym;
        for (int j = 0; j < 3*n_spsym; ++j) {
            dphi[j + ib] += dphi_peak*symbols[i]*pulse[j];
        }
    }

    // Add dummy symbols at beginning and end with tone values equal to 1st and last symbol, respectively
    for (int j = 0; j < 2*n_spsym; ++j) {
        dphi[j] += dphi_peak*pulse[j + n_spsym]*symbols[0];
        dphi[j + n_sym * n_spsym] += dphi_peak*pulse[j]*symbols[n_sym - 1];
    }

    // Calculate and insert the audio waveform
    float phi = 0;
    for (int k = 0; k < n_wave; ++k) { // Don't include dummy symbols
        signal[k] = sinf(phi);
        phi = fmodf(phi + dphi[k + n_spsym], 2*M_PI);
    }

    // Apply envelope shaping to the first and last symbols
    int n_ramp = n_spsym / 8;
    for (int i = 0; i < n_ramp; ++i) {
        float env = (1 - cosf(2 * M_PI * i / (2 * n_ramp))) / 2;
        signal[i] *= env;
        signal[n_wave - 1 - i] *= env;
    }
}


// Convert a sequence of symbols (tones) into a sinewave of continuous phase (FSK).
// Symbol 0 gets encoded as a sine of frequency f0, the others are spaced in increasing
// fashion.
void synth_fsk(const uint8_t *symbols, int num_symbols, float f0, float spacing, 
                float symbol_rate, float signal_rate, float *signal) {
    float phase = 0;
    float dt = 1/signal_rate;
    float dt_sym = 1/symbol_rate;
    float t = 0;
    int j = 0;
    int i = 0;
    while (j < num_symbols) {
        float f = f0 + symbols[j] * spacing;
        phase = fmodf(phase + 2 * M_PI * f / signal_rate, 2 * M_PI);
        signal[i] = sinf(phase);
        t += dt;
        if (t >= dt_sym) {
            // Move to the next symbol
            t -= dt_sym;
            ++j;
        }
        ++i;
    }
}


void usage() {
    printf("Generate a 15-second WAV file encoding a given message.\n");
    printf("Usage:\n");
    printf("\n");
    printf("gen_ft8 MESSAGE WAV_FILE [FREQUENCY]\n");
    printf("\n");
    printf("(Note that you might have to enclose your message in quote marks if it contains spaces)\n");
}


int main(int argc, char **argv) {
    // Expect two command-line arguments
    if (argc < 3) {
        usage();
        return -1;
    }

    const char *message = argv[1];
    const char *wav_path = argv[2];
    float frequency = 1000.0;
    if (argc > 3) {
       frequency = atof(argv[3]);
    }

    // First, pack the text data into binary message
    uint8_t packed[ft8::K_BYTES];
    //int rc = packmsg(message, packed);
    int rc = ft8::pack77(message, packed);
    if (rc < 0) {
        printf("Cannot parse message!\n");
        printf("RC = %d\n", rc);
        return -2;
    }

    printf("Packed data: ");
    for (int j = 0; j < 10; ++j) {
        printf("%02x ", packed[j]);
    }
    printf("\n");

    // Second, encode the binary message as a sequence of FSK tones
    uint8_t tones[ft8::NN];          // FT8_NN = 79, lack of better name at the moment
    //genft8(packed, 0, tones);
    ft8::genft8(packed, tones);

    printf("FSK tones: ");
    for (int j = 0; j < ft8::NN; ++j) {
        printf("%d", tones[j]);
    }
    printf("\n");

    // Third, convert the FSK tones into an audio signal
    const int sample_rate = 12000;
    const float symbol_rate = 6.25f;
    const int num_samples = (int)(0.5f + ft8::NN / symbol_rate * sample_rate);
    const int num_silence = (15 * sample_rate - num_samples) / 2;
    float signal[num_silence + num_samples + num_silence];
    for (int i = 0; i < num_silence + num_samples + num_silence; i++) {
        signal[i] = 0;
    }

    // synth_fsk(tones, ft8::NN, frequency, symbol_rate, symbol_rate, sample_rate, signal + num_silence);
    synth_gfsk(tones, ft8::NN, frequency, sample_rate / symbol_rate, sample_rate, signal + num_silence); 
    save_wav(signal, num_silence + num_samples + num_silence, sample_rate, wav_path);

    return 0;
}
