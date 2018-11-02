#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "pack.h"
#include "encode.h"

#include "pack_77.h"
#include "encode_91.h"


// Convert a sequence of symbols (tones) into a sinewave of continuous phase (FSK).
// Symbol 0 gets encoded as a sine of frequency f0, the others are spaced in incresing
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
        phase += 2 * M_PI * f / signal_rate;
        signal[i] = sin(phase);
        t += dt;
        if (t >= dt_sym) {
            // Move to the next symbol
            t -= dt_sym;
            ++j;
        }
        ++i;
    }
}

// Save signal in floating point format (-1 .. +1) as a WAVE file using 16-bit signed integers.
void save_wav(const float *signal, int num_samples, int sample_rate, const char *path) {
    FILE *f = fopen(path, "wb");
    char subChunk1ID[4] = {'f', 'm', 't', ' '};
    uint32_t subChunk1Size = 16;    // 16 for PCM
    uint16_t audioFormat = 1;   // PCM = 1
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t sampleRate = sample_rate;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;
    uint32_t byteRate = sampleRate * blockAlign;

    char subChunk2ID[4] = {'d', 'a', 't', 'a'};
    uint32_t subChunk2Size = num_samples * blockAlign;

    char chunkID[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize = 4 + (8 + subChunk1Size) + (8 + subChunk2Size);
    char format[4] = {'W', 'A', 'V', 'E'};

    int16_t *raw_data = (int16_t *)malloc(num_samples * blockAlign);
    for (int i = 0; i < num_samples; i++) {
        float x = signal[i];
        if (x > 1.0) x = 1.0;
        else if (x < -1.0) x = -1.0;
        raw_data[i] = int(0.5 + (x * 32767.0));
    }

    // NOTE: works only on little-endian architecture
    fwrite(chunkID, sizeof(chunkID), 1, f);
    fwrite(&chunkSize, sizeof(chunkSize), 1, f);
    fwrite(format, sizeof(format), 1, f);

    fwrite(subChunk1ID, sizeof(subChunk1ID), 1, f);
    fwrite(&subChunk1Size, sizeof(subChunk1Size), 1, f);
    fwrite(&audioFormat, sizeof(audioFormat), 1, f);
    fwrite(&numChannels, sizeof(numChannels), 1, f);
    fwrite(&sampleRate, sizeof(sampleRate), 1, f);
    fwrite(&byteRate, sizeof(byteRate), 1, f);
    fwrite(&blockAlign, sizeof(blockAlign), 1, f);
    fwrite(&bitsPerSample, sizeof(bitsPerSample), 1, f);

    fwrite(subChunk2ID, sizeof(subChunk2ID), 1, f);
    fwrite(&subChunk2Size, sizeof(subChunk2Size), 1, f);

    fwrite(raw_data, blockAlign, num_samples, f);

    fclose(f);

    free(raw_data);
}


void usage() {
    printf("Generate a 15-second WAV file encoding a given message.\n");
    printf("Usage:\n");
    printf("\n");
    printf("gen_ft8 MESSAGE WAV_FILE\n");
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

    // First, pack the text data into 72-bit binary message
    uint8_t packed[10];
    //int rc = packmsg(message, packed);
    int rc = ft8_v2::pack77(message, packed);
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
    uint8_t tones[NN];          // NN = 79, lack of better name at the moment
    //genft8(packed, 0, tones);
    ft8_v2::genft8(packed, tones);

    printf("FSK tones: ");
    for (int j = 0; j < NN; ++j) {
        printf("%d", tones[j]);
    }
    printf("\n");

    // Third, convert the FSK tones into an audio signal
    const int num_samples = (int)(0.5 + NN / 6.25 * 12000);
    const int num_silence = (15 * 12000 - num_samples) / 2;
    float signal[num_silence + num_samples + num_silence];
    for (int i = 0; i < num_silence + num_samples + num_silence; i++) {
        signal[i] = 0;
    }

    synth_fsk(tones, NN, 1000, 6.25, 6.25, 12000, signal + num_silence);
    save_wav(signal, num_silence + num_samples + num_silence, 12000, wav_path);

    return 0;
}
