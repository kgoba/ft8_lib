#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "ft8.h"
#include "common/wave.h"

// decode callback, called by ft8_decode() for each decoded message
static void ft8_decode_callback(char *message, float frequency, float time_dev, float snr, int score, void *ctx)
{
    printf("000000 %3d %+4.2f %4.1f %4.0f ~  %s\n", score, time_dev, snr, frequency, message);
}

int main(int argc, char **argv)
{
#if 0
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

        int n = ft8_decode(signal, num_samples, sample_rate, ft8_decode_callback, NULL);
        
        printf("Decoded %d messages\n", n);
    }
    else
    {
        fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
        return -1;
    }
#else
    // Expect one command-line argument
    if (argc == 2)
    {
        int sample_rate = 8000;
        int num_samples = 15 * sample_rate;
        float signal[num_samples];
        int16_t pcm[num_samples];
        
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            printf("Could not load WAV file (check format and size)\n");
            return -1;
        }

        lseek(fd, num_samples + 44, SEEK_SET);

        int n = 0;
        while (read(fd, pcm, num_samples * sizeof(int16_t)) == num_samples * sizeof(int16_t)) {
            for (int i = 0; i < num_samples; i++) {
                signal[i] = pcm[i] / 32768.0f;
            }
            n += ft8_decode(signal, num_samples, sample_rate, ft8_decode_callback, NULL);
        }
        
        printf("Decoded %d messages\n", n);
    }
    else
    {
        fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
        return -1;
    }
#endif
    return 0;
}
