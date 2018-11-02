#include "wave.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <stdint.h>


// Save signal in floating point format (-1 .. +1) as a WAVE file using 16-bit signed integers.
void save_wav(const float *signal, int num_samples, int sample_rate, const char *path) {
    FILE *f = fopen(path, "wb");
    char subChunk1ID[4] = {'f', 'm', 't', ' '};
    uint32_t subChunk1Size = 16;    // 16 for PCM
    uint16_t audioFormat = 1;       // PCM = 1
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
