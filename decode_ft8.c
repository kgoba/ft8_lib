#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "ft8/unpack.h"
#include "ft8/ldpc.h"
#include "ft8/decode.h"
#include "ft8/constants.h"
#include "ft8/encode.h"
#include "ft8/crc.h"

#include "common/wave.h"
#include "common/debug.h"
#include "fft/kiss_fftr.h"

#define LOG_LEVEL LOG_INFO

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 120;
const int kLDPC_iterations = 20;

const int kMax_decoded_messages = 50;

const int kFreq_osr = 2;
const int kTime_osr = 2;

const float kFSK_dev = 6.25f; // tone deviation in Hz and symbol rate

void usage()
{
    fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
}

float hann_i(int i, int N)
{
    float x = sinf((float)M_PI * i / N);
    return x * x;
}

float hamming_i(int i, int N)
{
    const float a0 = (float)25 / 46;
    const float a1 = 1 - a0;

    float x1 = cosf(2 * (float)M_PI * i / N);
    return a0 - a1 * x1;
}

float blackman_i(int i, int N)
{
    const float alpha = 0.16f; // or 2860/18608
    const float a0 = (1 - alpha) / 2;
    const float a1 = 1.0f / 2;
    const float a2 = alpha / 2;

    float x1 = cosf(2 * (float)M_PI * i / N);
    float x2 = 2 * x1 * x1 - 1; // Use double angle formula

    return a0 - a1 * x1 + a2 * x2;
}

static float max2(float a, float b)
{
    return (a >= b) ? a : b;
}

// Compute FFT magnitudes (log power) for each timeslot in the signal
void extract_power(const float signal[], waterfall_t *power, int block_size)
{
    const int subblock_size = block_size / power->time_osr;
    const int nfft = block_size * power->freq_osr;
    const float fft_norm = 2.0f / nfft;
    const int len_window = 1.8f * block_size; // hand-picked and optimized

    float window[nfft];
    for (int i = 0; i < nfft; ++i)
    {
        // window[i] = 1;
        // window[i] = hann_i(i, nfft);
        // window[i] = blackman_i(i, nfft);
        // window[i] = hamming_i(i, nfft);
        window[i] = (i < len_window) ? hann_i(i, len_window) : 0;
    }

    size_t fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

    LOG(LOG_INFO, "Block size = %d\n", block_size);
    LOG(LOG_INFO, "Subblock size = %d\n", subblock_size);
    LOG(LOG_INFO, "N_FFT = %d\n", nfft);
    LOG(LOG_INFO, "FFT work area = %lu\n", fft_work_size);

    void *fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);

    int offset = 0;
    float max_mag = -120.0f;
    for (int idx_block = 0; idx_block < power->num_blocks; ++idx_block)
    {
        // Loop over two possible time offsets (0 and block_size/2)
        for (int time_sub = 0; time_sub < power->time_osr; ++time_sub)
        {
            kiss_fft_scalar timedata[nfft];
            kiss_fft_cpx freqdata[nfft / 2 + 1];
            float mag_db[nfft / 2 + 1];

            // Extract windowed signal block
            for (int pos = 0; pos < nfft; ++pos)
            {
                timedata[pos] = window[pos] * signal[(idx_block * block_size) + (time_sub * subblock_size) + pos];
            }

            kiss_fftr(fft_cfg, timedata, freqdata);

            // Compute log magnitude in decibels
            for (int idx_bin = 0; idx_bin < nfft / 2 + 1; ++idx_bin)
            {
                float mag2 = (freqdata[idx_bin].i * freqdata[idx_bin].i) + (freqdata[idx_bin].r * freqdata[idx_bin].r);
                mag_db[idx_bin] = 10.0f * log10f(1E-12f + mag2 * fft_norm * fft_norm);
            }

            // Loop over two possible frequency bin offsets (for averaging)
            for (int freq_sub = 0; freq_sub < power->freq_osr; ++freq_sub)
            {
                for (int pos = 0; pos < power->num_bins; ++pos)
                {
                    float db = mag_db[pos * power->freq_osr + freq_sub];
                    // Scale decibels to unsigned 8-bit range and clamp the value
                    // Range 0-240 covers -120..0 dB in 0.5 dB steps
                    int scaled = (int)(2 * db + 240);

                    power->mag[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
                    ++offset;

                    if (db > max_mag)
                        max_mag = db;
                }
            }
        }
    }

    LOG(LOG_INFO, "Max magnitude: %.1f dB\n", max_mag);
    free(fft_work);
}

int main(int argc, char **argv)
{
    // Expect one command-line argument
    if (argc < 2)
    {
        usage();
        return -1;
    }

    const char *wav_path = argv[1];

    int sample_rate = 12000;
    int num_samples = 15 * sample_rate;
    float signal[num_samples];

    int rc = load_wav(signal, &num_samples, &sample_rate, wav_path);
    if (rc < 0)
    {
        return -1;
    }

    // Compute DSP parameters that depend on the sample rate
    const int num_bins = (int)(sample_rate / (2 * kFSK_dev)); // number bins of FSK tone width that the spectrum can be divided into
    const int block_size = (int)(sample_rate / kFSK_dev);     // samples corresponding to one FSK symbol
    const int subblock_size = block_size / kTime_osr;
    const int nfft = block_size * kFreq_osr;
    const int num_blocks = (num_samples - nfft + subblock_size) / block_size;

    LOG(LOG_INFO, "Sample rate %d Hz, %d blocks, %d bins\n", sample_rate, num_blocks, num_bins);

    // Compute FFT over the whole signal and store it
    uint8_t mag_power[num_blocks * kFreq_osr * kTime_osr * num_bins];
    waterfall_t power = {
        .num_blocks = num_blocks,
        .num_bins = num_bins,
        .time_osr = kTime_osr,
        .freq_osr = kFreq_osr,
        .mag = mag_power};
    extract_power(signal, &power, block_size);

    // Find top candidates by Costas sync score and localize them in time and frequency
    candidate_t candidate_list[kMax_candidates];
    int num_candidates = find_sync(&power, kMax_candidates, candidate_list, kMin_score);

    // Hash table for decoded messages (to check for duplicates)
    int num_decoded = 0;
    message_t decoded[kMax_decoded_messages];
    message_t *decoded_hashtable[kMax_decoded_messages];

    // Initialize hash table pointers
    for (int i = 0; i < kMax_decoded_messages; ++i)
    {
        decoded_hashtable[i] = NULL;
    }

    // Go over candidates and attempt to decode messages
    for (int idx = 0; idx < num_candidates; ++idx)
    {
        const candidate_t *cand = &candidate_list[idx];
        if (cand->score < kMin_score)
            continue;

        float freq_hz = (cand->freq_offset + (float)cand->freq_sub / kFreq_osr) * kFSK_dev;
        float time_sec = (cand->time_offset + (float)cand->time_sub / kTime_osr) / kFSK_dev;

        message_t message;
        decode_status_t status;
        if (!decode(&power, cand, &message, kLDPC_iterations, &status))
        {
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
            int snr = 0; // TODO: compute SNR
            printf("000000 %3d %+4.2f %4.0f ~  %s\n", cand->score, time_sec, freq_hz, message.text);
        }
    }
    LOG(LOG_INFO, "Decoded %d messages\n", num_decoded);

    return 0;
}
