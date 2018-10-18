#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "pack.h"
#include "encode.h"


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


void synth_fsk(const uint8_t *symbols, int nSymbols, float f0, float spacing, float symbol_rate, float signal_rate, float *signal) {
    float phase = 0;
    float dt = 1/signal_rate;
    float dt_sym = 1/symbol_rate;
    float t = 0;
    int j = 0;
    int i = 0;
    while (j < nSymbols) {
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


void test1() {
    //const char *test_in3 = "CQ DL7ACA JO40"; // 62, 32, 32, 49, 37, 27, 59, 2, 30, 19, 49, 16
    //const char *test_in3 = "VA3UG   F1HMR 73"; // 52, 54, 60, 12, 55, 54, 7, 19, 2, 23, 59, 16
    const char *test_in3 = "RA3Y VE3NLS 73";   // 46, 6, 32, 22, 55, 20, 11, 32, 53, 23, 59, 16
    uint8_t test_out3[9];
    int rc = packmsg(test_in3, test_out3);
  
    printf("RC = %d\n", rc);
    
    for (int i = 0; i < 9; ++i) {
        printf("%02x ", test_out3[i]);
    }
    printf("\n");

    uint8_t test_out4[12];
    convert_8bit_to_6bit(test_out4, test_out3, 72);
    for (int i = 0; i < 12; ++i) {
        printf("%d ", test_out4[i]);
    }
    printf("\n");    
}

void test2() {
    uint8_t test_in[11] = { 0xF1, 0x02, 0x03, 0x04, 0x05, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xFF };
    uint8_t test_out[22];

    encode174(test_in, test_out);

    for (int j = 0; j < 22; ++j) {
        printf("%02x ", test_out[j]);
    }
    printf("\n");
}

void test3() {
    uint8_t test_in2[10] = { 0x11, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x10, 0x04, 0x01, 0x00 };
    uint16_t crc1 = ft8_crc(test_in2, 76);  // Calculate CRC of 76 bits only
    printf("CRC: %04x\n", crc1);            // should be 0x0708
}


int main(int argc, char **argv) {
    if (argc < 3) return -1;

    //const char *message = "G0UPL YL3JG 73";
    const char *message = argv[1];
    const char *wav_path = argv[2];

    uint8_t packed[9];
    int rc = packmsg(message, packed);
    if (rc < 0) {
        printf("Cannot parse message!\n");
        printf("RC = %d\n", rc);
        return -2;
    }

    printf("Packed data: ");
    for (int j = 0; j < 9; ++j) {
        printf("%02x ", packed[j]);
    }
    printf("\n");

    uint8_t tones[NN];
    genft8(packed, 0, tones);

    printf("FSK tones: ");
    for (int j = 0; j < NN; ++j) {
        printf("%d", tones[j]);
    }
    printf("\n");

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