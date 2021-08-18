#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "common/wave.h"
#include "common/debug.h"
#include "ft8/pack.h"
#include "ft8/encode.h"
#include "ft8/constants.h"

#define LOG_LEVEL LOG_INFO

#define FT8_SLOT_TIME 15.0f   // total length of output waveform in seconds
#define FT8_SYMBOL_RATE 6.25f // tone deviation (and symbol rate) in Hz
#define FT8_SYMBOL_BT 2.0f    // symbol smoothing filter bandwidth factor (BT)

#define FT4_SLOT_TIME 7.5f         // total length of output waveform in seconds
#define FT4_SYMBOL_RATE 20.833333f // tone deviation (and symbol rate) in Hz
#define FT4_SYMBOL_BT 1.0f         // symbol smoothing filter bandwidth factor (BT)

#define GFSK_CONST_K 5.336446f // pi * sqrt(2 / log(2))

/// Computes a GFSK smoothing pulse.
/// The pulse is theoretically infinitely long, however, here it's truncated at 3 times the symbol length.
/// This means the pulse array has to have space for 3*n_spsym elements.
/// @param[in] n_spsym Number of samples per symbol
/// @param[in] b Shape parameter (values defined for FT8/FT4)
/// @param[out] pulse Output array of pulse samples
///
void gfsk_pulse(int n_spsym, float symbol_bt, float *pulse)
{
    for (int i = 0; i < 3 * n_spsym; ++i)
    {
        float t = i / (float)n_spsym - 1.5f;
        float arg1 = GFSK_CONST_K * symbol_bt * (t + 0.5f);
        float arg2 = GFSK_CONST_K * symbol_bt * (t - 0.5f);
        pulse[i] = (erff(arg1) - erff(arg2)) / 2;
    }
}

/// Synthesize waveform data using GFSK phase shaping.
/// The output waveform will contain n_sym symbols.
/// @param[in] symbols Array of symbols (tones) (0-7 for FT8)
/// @param[in] n_sym Number of symbols in the symbol array
/// @param[in] f0 Audio frequency in Hertz for the symbol 0 (base frequency)
/// @param[in] symbol_bt Symbol smoothing filter bandwidth (2 for FT8, 1 for FT4)
/// @param[in] symbol_rate Rate of symbols per second, Hertz
/// @param[in] signal_rate Sample rate of synthesized signal, Hertz
/// @param[out] signal Output array of signal waveform samples (should have space for n_sym*n_spsym samples)
///
void synth_gfsk(const uint8_t *symbols, int n_sym, float f0, float symbol_bt, float symbol_rate, int signal_rate, float *signal)
{
    int n_spsym = (int)(0.5f + signal_rate / symbol_rate); // Samples per symbol
    int n_wave = n_sym * n_spsym;                          // Number of output samples
    float hmod = 1.0f;

    LOG(LOG_DEBUG, "n_spsym = %d\n", n_spsym);
    // Compute the smoothed frequency waveform.
    // Length = (nsym+2)*n_spsym samples, first and last symbols extended
    float dphi_peak = 2 * M_PI * hmod / n_spsym;
    float dphi[n_wave + 2 * n_spsym];

    // Shift frequency up by f0
    for (int i = 0; i < n_wave + 2 * n_spsym; ++i)
    {
        dphi[i] = 2 * M_PI * f0 / signal_rate;
    }

    float pulse[3 * n_spsym];
    gfsk_pulse(n_spsym, symbol_bt, pulse);

    for (int i = 0; i < n_sym; ++i)
    {
        int ib = i * n_spsym;
        for (int j = 0; j < 3 * n_spsym; ++j)
        {
            dphi[j + ib] += dphi_peak * symbols[i] * pulse[j];
        }
    }

    // Add dummy symbols at beginning and end with tone values equal to 1st and last symbol, respectively
    for (int j = 0; j < 2 * n_spsym; ++j)
    {
        dphi[j] += dphi_peak * pulse[j + n_spsym] * symbols[0];
        dphi[j + n_sym * n_spsym] += dphi_peak * pulse[j] * symbols[n_sym - 1];
    }

    // Calculate and insert the audio waveform
    float phi = 0;
    for (int k = 0; k < n_wave; ++k)
    { // Don't include dummy symbols
        signal[k] = sinf(phi);
        phi = fmodf(phi + dphi[k + n_spsym], 2 * M_PI);
    }

    // Apply envelope shaping to the first and last symbols
    int n_ramp = n_spsym / 8;
    for (int i = 0; i < n_ramp; ++i)
    {
        float env = (1 - cosf(2 * M_PI * i / (2 * n_ramp))) / 2;
        signal[i] *= env;
        signal[n_wave - 1 - i] *= env;
    }
}

void usage()
{
    printf("Generate a 15-second WAV file encoding a given message.\n");
    printf("Usage:\n");
    printf("\n");
    printf("gen_ft8 MESSAGE WAV_FILE [FREQUENCY]\n");
    printf("\n");
    printf("(Note that you might have to enclose your message in quote marks if it contains spaces)\n");
}

int main(int argc, char **argv)
{
    // Expect two command-line arguments
    if (argc < 3)
    {
        usage();
        return -1;
    }

    const char *message = argv[1];
    const char *wav_path = argv[2];
    float frequency = 1000.0;
    if (argc > 3)
    {
        frequency = atof(argv[3]);
    }
    bool is_ft4 = (argc > 4) && (0 == strcmp(argv[4], "-ft4"));

    // First, pack the text data into binary message
    uint8_t packed[FT8_LDPC_K_BYTES];
    int rc = pack77(message, packed);
    if (rc < 0)
    {
        printf("Cannot parse message!\n");
        printf("RC = %d\n", rc);
        return -2;
    }

    printf("Packed data: ");
    for (int j = 0; j < 10; ++j)
    {
        printf("%02x ", packed[j]);
    }
    printf("\n");

    if (is_ft4)
    {
        // '[..] for FT4 only, in order to avoid transmitting a long string of zeros when sending CQ messages,
        // the assembled 77-bit message is bitwise exclusive-OR’ed with [a] pseudorandom sequence before computing the CRC and FEC parity bits'
        for (int i = 0; i < 10; ++i)
        {
            packed[i] ^= kFT4_XOR_sequence[i];
        }
    }

    int num_tones = (is_ft4) ? FT4_NN : FT8_NN;
    float symbol_rate = (is_ft4) ? FT4_SYMBOL_RATE : FT8_SYMBOL_RATE;
    float symbol_bt = (is_ft4) ? FT4_SYMBOL_BT : FT8_SYMBOL_BT;
    float slot_time = (is_ft4) ? FT4_SLOT_TIME : FT8_SLOT_TIME;

    // Second, encode the binary message as a sequence of FSK tones
    uint8_t tones[num_tones]; // Array of 79 tones (symbols)
    if (is_ft4)
    {
        genft4(packed, tones);
    }
    else
    {
        genft8(packed, tones);
    }

    printf("FSK tones: ");
    for (int j = 0; j < num_tones; ++j)
    {
        printf("%d", tones[j]);
    }
    printf("\n");

    // Third, convert the FSK tones into an audio signal
    int sample_rate = 12000;
    int num_samples = (int)(0.5f + num_tones / symbol_rate * sample_rate); // Number of samples in the data signal
    int num_silence = (slot_time * sample_rate - num_samples) / 2;         // Silence padding at both ends to make 15 seconds
    int num_total_samples = num_silence + num_samples + num_silence;       // Number of samples in the padded signal
    float signal[num_total_samples];
    for (int i = 0; i < num_silence; i++)
    {
        signal[i] = 0;
        signal[i + num_samples + num_silence] = 0;
    }

    // Synthesize waveform data (signal) and save it as WAV file
    synth_gfsk(tones, num_tones, frequency, symbol_bt, symbol_rate, sample_rate, signal + num_silence);
    save_wav(signal, num_total_samples, sample_rate, wav_path);

    return 0;
}
