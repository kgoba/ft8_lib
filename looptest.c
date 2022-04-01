#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "ft8.h"

#define FT4_SLOT_TIME 7.0f      // total length of output waveform in seconds
#define FT8_SLOT_TIME 15.0f     // total length of output waveform in seconds

// white noise added - decoding errors start to show up around 12.0
#define NOISE_AMPLITUDE 0.0

#define RP(x, div, mod)     ((x / div) % mod)

// passed as context into decoder callback
struct context {
    char message[32];
    float frequency;
};

static char *random_callsign(char *callsign)
{
    int x = rand();
    switch (x >> 29) {
        case 0:
            sprintf(callsign, "%c%d%c%c", 'A' + RP(x, 26, 26), RP(x, 260, 10), 'A' + RP(x, 6760, 26), 'A' + RP(x, 175760, 26));
            break;
        case 1:
            sprintf(callsign, "%c%d%c%c%c", 'A' + RP(x, 26, 26), RP(x, 260, 10), 'A' + RP(x, 6760, 26), 'A' + RP(x, 175760, 26), 'A' + RP(x, 4569760, 26));
            break;
        case 2:
            sprintf(callsign, "%c%c%d%c%c", 'A' + RP(x, 1, 26), 'A' + RP(x, 26, 26), RP(x, 260, 10), 'A' + RP(x, 6760, 26), 'A' + RP(x, 175760, 26));
            break;
        default:
            sprintf(callsign, "%c%c%d%c%c%c", 'A' + RP(x, 1, 26), 'A' + RP(x, 26, 26), RP(x, 260, 10), 'A' + RP(x, 6760, 26), 'A' + RP(x, 175760, 26), 'A' + RP(x, 4569760, 26));
            break;
    }
    return callsign;
}

static char *random_locator(char *locator)
{
    int x = rand();
    sprintf(locator, "%c%c%d%d", 'A' + RP(x, 1, 18), 'A' + RP(x, 18, 18), RP(x, 180, 10), RP(x, 1800, 10));
    return locator;
}

static char *random_message(char *message)
{
    int x = rand();
    char callsign1[8], callsign2[8], locator[5];
    switch (x >> 28) {
        case 0:
            sprintf(message, "CQ %s %s", random_callsign(callsign1), random_locator(locator));
            break;
        case 1:
            sprintf(message, "%s %s %s", random_callsign(callsign1), random_callsign(callsign2), random_locator(locator));
            break;
        case 2:
            sprintf(message, "%s %s -%02d", random_callsign(callsign1), random_callsign(callsign2), RP(x, 1, 30) + 1);
            break;
        case 3:
            sprintf(message, "%s %s R-%02d", random_callsign(callsign1), random_callsign(callsign2), RP(x, 1, 30) + 1);
            break;
        case 4:
            sprintf(message, "%s %s RRR", random_callsign(callsign1), random_callsign(callsign2));
            break;
        default:
            sprintf(message, "%s %s 73", random_callsign(callsign1), random_callsign(callsign2));
            break;
    }
    return message;
}

// decode callback, called by ft8_decode() for each decoded message
static void ft8_decode_callback(char *message, float frequency, float time_dev, float snr, int score, void *ctx)
{
    struct context *context = ctx;
    bool ok = strcmp(context->message, message) == 0;
    printf("%-8s000000 %3d %+4.2f %4.0f ~  %s (%s)\n", ok ? "OK" : "ERROR", score, time_dev, frequency, message, context->message);
}

int main(int argc, char *argv[])
{
    int sample_rate = 8000;
    float frequency = 1200.0;
    int num_samples = FT8_SLOT_TIME * sample_rate;
    float signal[num_samples];
    
    // initialize random number generator
    srand((unsigned int)time(NULL));
    
    // run loop test
    for (int i = 0; i < 100; i++) {
        struct context ctx;

        // generate random but valid message
        random_message(ctx.message);
        ctx.frequency = frequency;
        
        if (ft8_encode(ctx.message, signal, num_samples, frequency, sample_rate) == 0) {
            // add noise
            for (float *fp = signal; fp < signal + num_samples; fp++) {
                *fp = (*fp + 2.0 * NOISE_AMPLITUDE * rand() / RAND_MAX - NOISE_AMPLITUDE) / (1.0 + NOISE_AMPLITUDE);
            }
            if (ft8_decode(signal, num_samples, sample_rate, ft8_decode_callback, &ctx) != 1) {
                printf("*** ERROR decoding (%s)\n", ctx.message);
            }
        } else {
            printf("*** ERROR encoding (%s)\n", ctx.message);
        }
    }
}