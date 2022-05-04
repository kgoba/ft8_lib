#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/wave.h"
#include "ft8.h"

#define FT4_SLOT_TIME 7.0f // total length of output waveform in seconds
#define FT8_SLOT_TIME 15.0f // total length of output waveform in seconds

int main(int argc, char **argv)
{
    int sample_rate = 8000;
    
    // Expect two command-line arguments
    if (argc == 3 || argc == 4) {
        float frequency = 1000.0;
        
        bool is_ft4 = (argc > 4) && (0 == strcmp(argv[4], "-ft4"));
        int num_samples = sample_rate * (is_ft4 ? FT4_SLOT_TIME : FT8_SLOT_TIME);
        float signal[num_samples];
        
        if (argc > 3) {
            frequency = atof(argv[3]);
        }
        
        int rc = is_ft4 ? ft4_encode(argv[1], signal, num_samples, frequency, sample_rate) : ft8_encode(argv[1], signal, num_samples, 1000.0, 8000.0);
        if (rc == 0) {
            save_wav(signal, num_samples, sample_rate, argv[2]);
        } else {
            LOG(LOG_ERROR, "Could not generate signal, rc = %d\n", rc);
        }
        return rc;
    }
    
    // wrong number of arguments
    printf("Generate a 15-second WAV file encoding a given message.\n");
    printf("Usage:\n");
    printf("\n");
    printf("gen_ft8 MESSAGE WAV_FILE [FREQUENCY]\n");
    printf("\n");
    printf("(Note that you might have to enclose your message in quote marks if it contains spaces)\n");
    return -1;
}
