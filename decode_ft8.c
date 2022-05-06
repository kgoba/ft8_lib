#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/wave.h"
#include "ft8.h"

static FILE *reference;
static bool identical;

// decode callback, called by ft8_decode() for each decoded message
static void ft8_decode_callback(char *message, float frequency, float time_dev, float snr, int score, void *ctx)
{
    char buf[256], buf2[256];
    printf("000000 %3d %+4.2f %4.1f %4.0f ~  %s\n", score, time_dev, snr, frequency, message);
    sprintf(buf, "000000 %3d %+4.2f %4.1f %4.0f ~  %s\n", score, time_dev, snr, frequency, message);
//    fgets(buf2, sizeof(buf2) - 1, reference);
    if (strcmp(buf, buf2) != 0) {
        identical = false;
//        printf("ERROR: %s\n", buf2);
    }
}

int main(int argc, char *argv[])
{
#if 1
    // Expect one command-line argument
    if (argc == 2)
    {
        int sample_rate = 12000;
        int num_samples = 15 * sample_rate;
        float signal[num_samples];
        
        int rc = load_wav(signal, &num_samples, &sample_rate, argv[1]);
        if (rc < 0)
        {
            printf("Could not load WAV file (check format and size)\n");
            return -1;
        }
        
        int n = ftx_decode(signal, num_samples, sample_rate, PROTO_FT8, ft8_decode_callback, NULL);
        
        printf("Decoded %d messages\n", n);
    }
    else
    {
        fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
        return -1;
    }
#else
    // Expect one command-line argument
    if (argc == 2) {
        int sample_rate = 8000;
        int num_samples = 15 * sample_rate;
        float signal[num_samples];
        int16_t pcm[num_samples];
        char buf[256];
        
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            printf("Could not load PCM (16 bit little endian) file (check format and size)\n");
            return -1;
        }
        
        reference = fopen("/Users/nadig/Desktop/reference.txt", "r");
        identical = true;
        
        int n = 0;
        while (read(fd, pcm, num_samples * sizeof(int16_t)) == num_samples * sizeof(int16_t)) {
            for (int i = 0; i < num_samples; i++) {
                signal[i] = pcm[i] / 32768.0f;
            }
            n += ftx_decode(signal, num_samples, sample_rate, PROTO_FT8, ft8_decode_callback, NULL);
        }
        printf("Decoded %d messages\n", n);
        
        if (identical && fgets(buf, sizeof(buf) - 1, reference) == NULL) {
            printf("OK\n");
        } else {
            printf("*** ERROR\n");
        }
        
    } else {
        fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
        return -1;
    }
#endif
    return 0;
}

