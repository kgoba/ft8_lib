#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include <ft8/decode.h>
#include <ft8/encode.h>

#include <common/common.h>
#include <common/wave.h>
#include <common/monitor.h>

#define LOG_LEVEL LOG_INFO
#include <ft8/debug.h>

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 120;
const int kLDPC_iterations = 20;

const int kMax_decoded_messages = 50;

const int kFreq_osr = 2; // Frequency oversampling rate (bin subdivision)
const int kTime_osr = 2; // Time oversampling rate (symbol subdivision)

void usage(void)
{
    fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
}

#ifdef USE_PORTAUDIO
#include "portaudio.h"

typedef struct
{
    PaTime startTime;
} audio_cb_context_t;

static audio_cb_context_t audio_cb_context;

static int audio_cb(void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData)
{
    audio_cb_context_t* context = (audio_cb_context_t*)userData;
    int16_t* samples_in = (int16_t*)inputBuffer;

    // PaTime time = data->startTime + timeInfo->inputBufferAdcTime;
    return 0;
}

void audio_list(void)
{
    PaError pa_rc;

    pa_rc = Pa_Initialize(); // Initialize PortAudio
    if (pa_rc != paNoError)
    {
        printf("Error initializing PortAudio.\n");
        printf("\tErrortext: %s\n\tNumber: %d\n", Pa_GetErrorText(pa_rc), pa_rc);
        return;
    }

    int numDevices;
    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
    {
        printf("ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
        return;
    }

    printf("%d audio devices found:\n", numDevices);
    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);

        PaStreamParameters inputParameters = {
            .device = i,
            .channelCount = 1, // 1 = mono, 2 = stereo
            .sampleFormat = paInt16,
            .suggestedLatency = 0.2,
            .hostApiSpecificStreamInfo = NULL
        };
        double sample_rate = 12000; // sample rate (frames per second)
        pa_rc = Pa_IsFormatSupported(&inputParameters, NULL, sample_rate);

        printf("%d: [%s] [%s]\n", (i + 1), deviceInfo->name, (pa_rc == paNoError) ? "OK" : "NOT SUPPORTED");
    }
}

int audio_open(const char* name)
{
    PaError pa_rc;

    pa_rc = Pa_Initialize(); // Initialize PortAudio
    if (pa_rc != paNoError)
    {
        printf("Error initializing PortAudio.\n");
        printf("\tErrortext: %s\n\tNumber: %d\n", Pa_GetErrorText(pa_rc), pa_rc);
        Pa_Terminate(); // I don't think we need this but...
        return -1;
    }

    PaDeviceIndex ndevice_in = -1;
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (0 == strcmp(deviceInfo->name, name))
        {
            ndevice_in = i;
            break;
        }
    }

    if (ndevice_in < 0)
    {
        printf("Could not find device [%s].\n", name);
        audio_list();
        return -1;
    }

    PaStream* instream;
    unsigned long nfpb = 1920 / 4; // frames per buffer
    double sample_rate = 12000;    // sample rate (frames per second)

    PaStreamParameters inputParameters = {
        .device = ndevice_in,
        .channelCount = 1, // 1 = mono, 2 = stereo
        .sampleFormat = paInt16,
        .suggestedLatency = 0.2,
        .hostApiSpecificStreamInfo = NULL
    };

    // Test if this configuration actually works, so we do not run into an ugly assertion
    pa_rc = Pa_IsFormatSupported(&inputParameters, NULL, sample_rate);
    if (pa_rc != paNoError)
    {
        printf("Error opening input audio stream.\n");
        printf("\tErrortext: %s\n\tNumber: %d\n", Pa_GetErrorText(pa_rc), pa_rc);
        return -2;
    }

    pa_rc = Pa_OpenStream(
        &instream, // address of stream
        &inputParameters,
        NULL,
        sample_rate, // Sample rate
        nfpb,        // Frames per buffer
        paNoFlag,
        (PaStreamCallback*)audio_cb, // Callback routine
        (void*)&audio_cb_context);   // address of data structure
    if (pa_rc != paNoError)
    { // We should have no error here usually
        printf("Error opening input audio stream:\n");
        printf("\tErrortext: %s\n\tNumber: %d\n", Pa_GetErrorText(pa_rc), pa_rc);
        return -3;
    }
    // printf("Successfully opened audio input.\n");

    pa_rc = Pa_StartStream(instream); // Start input stream
    if (pa_rc != paNoError)
    {
        printf("Error starting input audio stream!\n");
        printf("\tErrortext: %s\n\tNumber: %d\n", Pa_GetErrorText(pa_rc), pa_rc);
        return -4;
    }

    // while (Pa_IsStreamActive(instream))
    // {
    //     Pa_Sleep(100);
    // }
    // Pa_AbortStream(instream); // Abort stream
    // Pa_CloseStream(instream); // Close stream, we're done.

    return 0;
}
#endif

int main(int argc, char** argv)
{
    // Accepted arguments
    const char* wav_path = NULL;
    bool is_ft8 = true;

    // Parse arguments one by one
    int arg_idx = 1;
    while (arg_idx < argc)
    {
        // Check if the current argument is an option (-xxx)
        if (argv[arg_idx][0] == '-')
        {
            // Check agaist valid options
            if (0 == strcmp(argv[arg_idx], "-ft4"))
            {
                is_ft8 = false;
            }
            else
            {
                usage();
                return -1;
            }
        }
        else
        {
            if (wav_path == NULL)
            {
                wav_path = argv[arg_idx];
            }
            else
            {
                usage();
                return -1;
            }
        }
        ++arg_idx;
    }
    // Check if all mandatory arguments have been received
    if (wav_path == NULL)
    {
        usage();
        return -1;
    }

    audio_list();

    int sample_rate = 12000;
    int num_samples = 15 * sample_rate;
    float signal[num_samples];

    int rc = load_wav(signal, &num_samples, &sample_rate, wav_path);
    if (rc < 0)
    {
        return -1;
    }

    LOG(LOG_INFO, "Sample rate %d Hz, %d samples, %.3f seconds\n", sample_rate, num_samples, (double)num_samples / sample_rate);

    // Compute FFT over the whole signal and store it
    monitor_t mon;
    monitor_config_t mon_cfg = {
        .f_min = 200,
        .f_max = 3000,
        .sample_rate = sample_rate,
        .time_osr = kTime_osr,
        .freq_osr = kFreq_osr,
        .protocol = is_ft8 ? PROTO_FT8 : PROTO_FT4
    };
    monitor_init(&mon, &mon_cfg);
    LOG(LOG_DEBUG, "Waterfall allocated %d symbols\n", mon.wf.max_blocks);
    for (int frame_pos = 0; frame_pos + mon.block_size <= num_samples; frame_pos += mon.block_size)
    {
        // Process the waveform data frame by frame - you could have a live loop here with data from an audio device
        monitor_process(&mon, signal + frame_pos);
    }
    LOG(LOG_DEBUG, "Waterfall accumulated %d symbols\n", mon.wf.num_blocks);
    LOG(LOG_INFO, "Max magnitude: %.1f dB\n", mon.max_mag);

    // Find top candidates by Costas sync score and localize them in time and frequency
    candidate_t candidate_list[kMax_candidates];
    int num_candidates = ft8_find_sync(&mon.wf, kMax_candidates, candidate_list, kMin_score);

    // Hash table for decoded messages (to check for duplicates)
    int num_decoded = 0;
    message_t decoded[kMax_decoded_messages];
    message_t* decoded_hashtable[kMax_decoded_messages];

    // Initialize hash table pointers
    for (int i = 0; i < kMax_decoded_messages; ++i)
    {
        decoded_hashtable[i] = NULL;
    }

    // Go over candidates and attempt to decode messages
    for (int idx = 0; idx < num_candidates; ++idx)
    {
        const candidate_t* cand = &candidate_list[idx];
        if (cand->score < kMin_score)
            continue;

        float freq_hz = (mon.min_bin + cand->freq_offset + (float)cand->freq_sub / mon.wf.freq_osr) / mon.symbol_period;
        float time_sec = (cand->time_offset + (float)cand->time_sub / mon.wf.time_osr) * mon.symbol_period;

        message_t message;
        decode_status_t status;
        if (!ft8_decode(&mon.wf, cand, &message, kLDPC_iterations, NULL, &status))
        {
            // printf("000000 %3d %+4.2f %4.0f ~  ---\n", cand->score, time_sec, freq_hz);
            if (status.ldpc_errors > 0)
            {
                LOG(LOG_DEBUG, "LDPC decode: %d errors\n", status.ldpc_errors);
            }
            else if (status.crc_calculated != status.crc_extracted)
            {
                LOG(LOG_DEBUG, "CRC mismatch!\n");
            }
            else if (status.unpack_status != 0)
            {
                LOG(LOG_DEBUG, "Error while unpacking!\n");
            }
            continue;
        }

        LOG(LOG_DEBUG, "Checking hash table for %4.1fs / %4.1fHz [%d]...\n", time_sec, freq_hz, cand->score);
        int idx_hash = message.hash % kMax_decoded_messages;
        bool found_empty_slot = false;
        bool found_duplicate = false;
        do
        {
            if (decoded_hashtable[idx_hash] == NULL)
            {
                LOG(LOG_DEBUG, "Found an empty slot\n");
                found_empty_slot = true;
            }
            else if ((decoded_hashtable[idx_hash]->hash == message.hash) && (0 == strcmp(decoded_hashtable[idx_hash]->text, message.text)))
            {
                LOG(LOG_DEBUG, "Found a duplicate [%s]\n", message.text);
                found_duplicate = true;
            }
            else
            {
                LOG(LOG_DEBUG, "Hash table clash!\n");
                // Move on to check the next entry in hash table
                idx_hash = (idx_hash + 1) % kMax_decoded_messages;
            }
        } while (!found_empty_slot && !found_duplicate);

        if (found_empty_slot)
        {
            // Fill the empty hashtable slot
            memcpy(&decoded[idx_hash], &message, sizeof(message));
            decoded_hashtable[idx_hash] = &decoded[idx_hash];
            ++num_decoded;

            // Fake WSJT-X-like output for now
            float snr = cand->score * 0.5f; // TODO: compute better approximation of SNR
            printf("000000 %2.1f %+4.2f %4.0f ~  %s\n", snr, time_sec, freq_hz, message.text);
        }
    }
    LOG(LOG_INFO, "Decoded %d messages\n", num_decoded);

    monitor_free(&mon);

    return 0;
}
