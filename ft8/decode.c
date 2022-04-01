#include <stdbool.h>
#include <math.h>

#include "ft8/ft8.h"

#include "constants.h"
#include "crc.h"
#include "ldpc.h"
#include "unpack.h"
#include "fft/kiss_fftr.h"
#include "common/debug.h"

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 120;
const int kLDPC_iterations = 20;

const int kMax_decoded_messages = 50;

const int kFreq_osr = 2;
const int kTime_osr = 2;

const float kFSK_dev = 6.25f; // tone deviation in Hz and symbol rate
const float kFSK_width = 8 * kFSK_dev; // bandwidth of FT8 signal

const float kSignalMin = 1e-12f;

const float fNoiseMinFreq = 350.0;
const float fNoiseMaxFreq = 2850.0;

/// Input structure to find_sync() function. This structure describes stored waterfall data over the whole message slot.
/// Fields time_osr and freq_osr specify additional oversampling rate for time and frequency resolution.
/// If time_osr=1, FFT magnitude data is collected once for every symbol transmitted, i.e. every 1/6.25 = 0.16 seconds.
/// Values time_osr > 1 mean each symbol is further subdivided in time.
/// If freq_osr=1, each bin in the FFT magnitude data corresponds to 6.25 Hz, which is the tone spacing.
/// Values freq_osr > 1 mean the tone spacing is further subdivided by FFT analysis.
typedef struct
{
    int num_blocks; ///< number of total blocks (symbols) in terms of 160 ms time periods
    int num_bins;   ///< number of FFT bins in terms of 6.25 Hz
    int time_osr;   ///< number of time subdivisions
    int freq_osr;   ///< number of frequency subdivisions
    uint8_t *mag;   ///< FFT magnitudes stored as uint8_t[blocks][time_osr][freq_osr][num_bins]
    float *pwr;     ///< averaged FFT power levels over all blocks
} waterfall_t;

/// Output structure of find_sync() and input structure of extract_likelihood().
/// Holds the position of potential start of a message in time and frequency.
typedef struct
{
    int16_t score;       ///< Candidate score (non-negative number; higher score means higher likelihood)
    int16_t time_offset; ///< Index of the time block
    int16_t freq_offset; ///< Index of the frequency bin
    uint8_t time_sub;    ///< Index of the time subdivision used
    uint8_t freq_sub;    ///< Index of the frequency subdivision used
} candidate_t;

/// Structure that holds the decoded message
typedef struct
{
    // TODO: check again that this size is enough
    char text[25]; // plain text
    uint16_t hash; // hash value to be used in hash table and quick checking for duplicates
} message_t;

/// Structure that contains the status of various steps during decoding of a message
typedef struct
{
    int ldpc_errors;
    uint16_t crc_extracted;
    uint16_t crc_calculated;
    int unpack_status;
} decode_status_t;

// forward declarations
static void extract_likelihood(const waterfall_t *power, const candidate_t *cand, float *log174);
static float max2(float a, float b);
static float max4(float a, float b, float c, float d);
static void heapify_down(candidate_t heap[], int heap_size);
static void heapify_up(candidate_t heap[], int heap_size);
static void decode_symbol(const uint8_t *power, int bit_idx, float *log174);
static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, int bit_idx, float *log174);

static int get_index(const waterfall_t *power, int block, int time_sub, int freq_sub, int bin)
{
    return ((((block * power->time_osr) + time_sub) * power->freq_osr + freq_sub) * power->num_bins) + bin;
}

static int find_sync(const waterfall_t *power, int num_candidates, candidate_t heap[], int min_score)
{
    int heap_size = 0;
    int sym_stride = power->time_osr * power->freq_osr * power->num_bins;

    // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
    // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
    // sync symbols we included in the score, so the score is averaged.
    for (int time_sub = 0; time_sub < power->time_osr; ++time_sub)
    {
        for (int freq_sub = 0; freq_sub < power->freq_osr; ++freq_sub)
        {
            for (int time_offset = -12; time_offset < 24; ++time_offset)
            {
                for (int freq_offset = 0; freq_offset + 7 < power->num_bins; ++freq_offset)
                {
                    int score = 0;
                    int num_average = 0;

                    // Compute average score over sync symbols (m+k = 0-7, 36-43, 72-79)
                    for (int m = 0; m <= 72; m += 36)
                    {
                        for (int k = 0; k < 7; ++k)
                        {
                            int block = time_offset + m + k;
                            // Check for time boundaries
                            if (block < 0)
                                continue;
                            if (block >= power->num_blocks)
                                break;

                            int offset = get_index(power, block, time_sub, freq_sub, freq_offset);
                            const uint8_t *p8 = power->mag + offset;

                            // Weighted difference between the expected and all other symbols
                            // Does not work as well as the alternative score below
                            // score += 8 * p8[kFT8_Costas_pattern[k]] -
                            //          p8[0] - p8[1] - p8[2] - p8[3] -
                            //          p8[4] - p8[5] - p8[6] - p8[7];
                            // ++num_average;

                            // Check only the neighbors of the expected symbol frequency- and time-wise
                            int sm = kFT8_Costas_pattern[k]; // Index of the expected bin
                            if (sm > 0)
                            {
                                // look at one frequency bin lower
                                score += p8[sm] - p8[sm - 1];
                                ++num_average;
                            }
                            if (sm < 7)
                            {
                                // look at one frequency bin higher
                                score += p8[sm] - p8[sm + 1];
                                ++num_average;
                            }
                            if ((k > 0) && (block > 0))
                            {
                                // look one symbol back in time
                                score += p8[sm] - p8[sm - sym_stride];
                                ++num_average;
                            }
                            if ((k < 6) && ((block + 1) < power->num_blocks))
                            {
                                // look one symbol forward in time
                                score += p8[sm] - p8[sm + sym_stride];
                                ++num_average;
                            }
                        }
                    }

                    if (num_average > 0)
                        score /= num_average;

                    if (score < min_score)
                        continue;

                    // If the heap is full AND the current candidate is better than
                    // the worst in the heap, we remove the worst and make space
                    if (heap_size == num_candidates && score > heap[0].score)
                    {
                        heap[0] = heap[heap_size - 1];
                        --heap_size;

                        heapify_down(heap, heap_size);
                    }

                    // If there's free space in the heap, we add the current candidate
                    if (heap_size < num_candidates)
                    {
                        heap[heap_size].score = score;
                        heap[heap_size].time_offset = time_offset;
                        heap[heap_size].freq_offset = freq_offset;
                        heap[heap_size].time_sub = time_sub;
                        heap[heap_size].freq_sub = freq_sub;
                        ++heap_size;

                        heapify_up(heap, heap_size);
                    }
                }
            }
        }
    }

    // Sort the candidates by sync strength - here we benefit from the heap structure
    int len_unsorted = heap_size;
    while (len_unsorted > 1)
    {
        candidate_t tmp = heap[len_unsorted - 1];
        heap[len_unsorted - 1] = heap[0];
        heap[0] = tmp;
        len_unsorted--;
        heapify_down(heap, len_unsorted);
    }

    return heap_size;
}

/// Compute log likelihood log(p(1) / p(0)) of 174 message bits for later use in soft-decision LDPC decoding
/// @param[in] power Waterfall data collected during message slot
/// @param[in] cand Candidate to extract the message from
/// @param[out] log174 Output of decoded log likelihoods for each of the 174 message bits
static void extract_likelihood(const waterfall_t *power, const candidate_t *cand, float *log174)
{
    int sym_stride = power->time_osr * power->freq_osr * power->num_bins;
    int offset = get_index(power, cand->time_offset, cand->time_sub, cand->freq_sub, cand->freq_offset);

    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    for (int k = 0; k < FT8_ND; k += n_syms)
    {
        // Add either 7 or 14 extra symbols to account for sync
        int sym_idx = (k < FT8_ND / 2) ? (k + 7) : (k + 14);
        int bit_idx = 3 * k;

        int block = cand->time_offset + sym_idx;

        // Check for time boundaries
        if ((block < 0) || (block >= power->num_blocks))
        {
            log174[bit_idx] = 0;
        }
        else
        {
            // Pointer to 8 bins of the current symbol
            const uint8_t *ps = power->mag + offset + (sym_idx * sym_stride);

            decode_symbol(ps, bit_idx, log174);
        }
    }

    // Compute the variance of log174
    float sum = 0;
    float sum2 = 0;
    for (int i = 0; i < FT8_LDPC_N; ++i)
    {
        sum += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float inv_n = 1.0f / FT8_LDPC_N;
    float variance = (sum2 - (sum * sum * inv_n)) * inv_n;

    // Normalize log174 distribution and scale it with experimentally found coefficient
    float norm_factor = sqrtf(24.0f / variance);
    for (int i = 0; i < FT8_LDPC_N; ++i)
    {
        log174[i] *= norm_factor;
    }
}

static bool decode(const waterfall_t *power, const candidate_t *cand, message_t *message, int max_iterations, decode_status_t *status)
{
    float log174[FT8_LDPC_N]; // message bits encoded as likelihood
    extract_likelihood(power, cand, log174);

    uint8_t plain174[FT8_LDPC_N]; // message bits (0/1)
    ft8_bp_decode(log174, max_iterations, plain174, &status->ldpc_errors);
    // ldpc_decode(log174, max_iterations, plain174, &status->ldpc_errors);

    if (status->ldpc_errors > 0)
    {
        return false;
    }

    // Extract payload + CRC (first FT8_LDPC_K bits) packed into a byte array
    uint8_t a91[FT8_LDPC_K_BYTES];
    ft8_pack_bits(plain174, FT8_LDPC_K, a91);

    // Extract CRC and check it
    status->crc_extracted = ft8_extract_crc(a91);
    // [1]: 'The CRC is calculated on the source-encoded message, zero-extended from 77 to 82 bits.'
    a91[9] &= 0xF8;
    a91[10] &= 0x00;
    status->crc_calculated = ft8_crc(a91, 96 - 14);

    if (status->crc_extracted != status->crc_calculated)
    {
        return false;
    }

    status->unpack_status = ft8_unpack77(a91, message->text);

    if (status->unpack_status < 0)
    {
        return false;
    }

    // Reuse binary message CRC as hash value for the message
    message->hash = status->crc_extracted;

    return true;
}

static float max2(float a, float b)
{
    return (a >= b) ? a : b;
}

static float max4(float a, float b, float c, float d)
{
    return max2(max2(a, b), max2(c, d));
}

static void heapify_down(candidate_t heap[], int heap_size)
{
    // heapify from the root down
    int current = 0;
    while (true)
    {
        int largest = current;
        int left = 2 * current + 1;
        int right = left + 1;

        if (left < heap_size && heap[left].score < heap[largest].score)
        {
            largest = left;
        }
        if (right < heap_size && heap[right].score < heap[largest].score)
        {
            largest = right;
        }
        if (largest == current)
        {
            break;
        }

        candidate_t tmp = heap[largest];
        heap[largest] = heap[current];
        heap[current] = tmp;
        current = largest;
    }
}

static void heapify_up(candidate_t heap[], int heap_size)
{
    // heapify from the last node up
    int current = heap_size - 1;
    while (current > 0)
    {
        int parent = (current - 1) / 2;
        if (heap[current].score >= heap[parent].score)
        {
            break;
        }

        candidate_t tmp = heap[parent];
        heap[parent] = heap[current];
        heap[current] = tmp;
        current = parent;
    }
}

// Compute unnormalized log likelihood log(p(1) / p(0)) of 3 message bits (1 FSK symbol)
static void decode_symbol(const uint8_t *power, int bit_idx, float *log174)
{
    // Cleaned up code for the simple case of n_syms==1
    float s2[8];

    for (int j = 0; j < 8; ++j)
    {
        s2[j] = (float)power[kFT8_Gray_map[j]];
    }

    log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
    log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
    log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
}

// Compute unnormalized log likelihood log(p(1) / p(0)) of bits corresponding to several FSK symbols at once
static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, int bit_idx, float *log174)
{
    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);

    float s2[n_tones];

    for (int j = 0; j < n_tones; ++j)
    {
        int j1 = j & 0x07;
        if (n_syms == 1)
        {
            s2[j] = (float)power[kFT8_Gray_map[j1]];
            continue;
        }
        int j2 = (j >> 3) & 0x07;
        if (n_syms == 2)
        {
            s2[j] = (float)power[kFT8_Gray_map[j2]];
            s2[j] += (float)power[kFT8_Gray_map[j1] + 4 * num_bins];
            continue;
        }
        int j3 = (j >> 6) & 0x07;
        s2[j] = (float)power[kFT8_Gray_map[j3]];
        s2[j] += (float)power[kFT8_Gray_map[j2] + 4 * num_bins];
        s2[j] += (float)power[kFT8_Gray_map[j1] + 8 * num_bins];
    }

    // Extract bit significance (and convert them to float)
    // 8 FSK tones = 3 bits
    for (int i = 0; i < n_bits; ++i)
    {
        if (bit_idx + i >= FT8_LDPC_N)
        {
            // Respect array size
            break;
        }

        uint16_t mask = (n_tones >> (i + 1));
        float max_zero = -1000, max_one = -1000;
        for (int n = 0; n < n_tones; ++n)
        {
            if (n & mask)
            {
                max_one = max2(max_one, s2[n]);
            }
            else
            {
                max_zero = max2(max_zero, s2[n]);
            }
        }

        log174[bit_idx + i] = max_one - max_zero;
    }
}

static float hann_i(int i, int N)
{
    float x = sinf((float)M_PI * i / N);
    return x * x;
}

static float hamming_i(int i, int N)
{
    const float a0 = (float)25 / 46;
    const float a1 = 1 - a0;

    float x1 = cosf(2 * (float)M_PI * i / N);
    return a0 - a1 * x1;
}

static float blackman_i(int i, int N)
{
    const float alpha = 0.16f; // or 2860/18608
    const float a0 = (1 - alpha) / 2;
    const float a1 = 1.0f / 2;
    const float a2 = alpha / 2;

    float x1 = cosf(2 * (float)M_PI * i / N);
    float x2 = 2 * x1 * x1 - 1; // Use double angle formula

    return a0 - a1 * x1 + a2 * x2;
}

// Compute FFT magnitudes (log power) for each timeslot in the signal
static void extract_power(const float signal[], waterfall_t *power, int block_size)
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

    // clear pwr values
    memset(power->pwr, 0, power->num_blocks * power->freq_osr * sizeof(float));
    
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
                mag_db[idx_bin] = 10.0f * log10f(mag2 * fft_norm * fft_norm + kSignalMin);
                power->pwr[idx_bin] += mag2 / (nfft / 2.0);
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

static float calc_avg_power(waterfall_t power, int sample_rate, float start, float end)
{
    const int start_bin = power.freq_osr * start / kFSK_dev;
    const int end_bin = power.freq_osr * end / kFSK_dev + power.freq_osr;
    float sum = 0.0;
    
    for (int bin = start_bin; bin < end_bin; bin++) {
        sum += power.pwr[bin];
    }
    
    return sum / (end_bin - start_bin);
}

int ft8_decode(float *signal, int num_samples, int sample_rate, ft8_decode_callback_t callback, void *ctx)
{
    // compute DSP parameters that depend on the sample rate
    const int num_bins = (int)(sample_rate / (2 * kFSK_dev)); // number bins of FSK tone width that the spectrum can be divided into
    const int block_size = (int)(sample_rate / kFSK_dev);     // samples corresponding to one FSK symbol
    const int subblock_size = block_size / kTime_osr;
    const int nfft = block_size * kFreq_osr;
    const int num_blocks = (num_samples - nfft + subblock_size) / block_size;
    int num_decoded = 0;
    
    // Compute FFT over the whole signal and store it
    waterfall_t power = {
        .num_blocks = num_blocks,
        .num_bins = num_bins,
        .time_osr = kTime_osr,
        .freq_osr = kFreq_osr,
        .mag = malloc(num_blocks * kFreq_osr * kTime_osr * num_bins),
        .pwr = malloc(kFreq_osr * num_bins * sizeof(float))
    };
    
    if (power.mag != NULL && power.pwr != NULL) {
        float noise;

        extract_power(signal, &power, block_size);
        
        // Find top candidates by Costas sync score and localize them in time and frequency
        candidate_t candidate_list[kMax_candidates];
        int num_candidates = find_sync(&power, kMax_candidates, candidate_list, kMin_score);
        
        // Hash table for decoded messages (to check for duplicates)
        message_t decoded[kMax_decoded_messages];
        message_t *decoded_hashtable[kMax_decoded_messages];
        
        // Initialize hash table pointers
        for (int i = 0; i < kMax_decoded_messages; ++i)
        {
            decoded_hashtable[i] = NULL;
        }
        
        // calculate noise - avoid division by 0 later
        noise = calc_avg_power(power, sample_rate, fNoiseMinFreq, fNoiseMaxFreq) + kSignalMin;
        
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
            
            int idx_hash = message.hash % kMax_decoded_messages;
            bool found_empty_slot = false;
            bool found_duplicate = false;
            do
            {
                if (decoded_hashtable[idx_hash] == NULL)
                {
                    found_empty_slot = true;
                }
                else if ((decoded_hashtable[idx_hash]->hash == message.hash) && (0 == strcmp(decoded_hashtable[idx_hash]->text, message.text)))
                {
                    found_duplicate = true;
                }
                else
                {
                    // move on to check the next entry in hash table
                    idx_hash = (idx_hash + 1) % kMax_decoded_messages;
                }
            } while (!found_empty_slot && !found_duplicate);
            
            if (found_empty_slot)
            {
                // calculate signal for SNR calculation
                float signal = calc_avg_power(power, sample_rate, freq_hz, freq_hz + kFSK_width);
                
                // fill the empty hashtable slot
                memcpy(&decoded[idx_hash], &message, sizeof(message));
                decoded_hashtable[idx_hash] = &decoded[idx_hash];
                ++num_decoded;
                
                // report message through callback
                callback(message.text, freq_hz, time_sec, 10.0 * log10f(signal / noise), cand->score, ctx);
            }
        }
    }
    
    return num_decoded;
}
