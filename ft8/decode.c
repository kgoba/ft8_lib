#include "decode.h"
#include "constants.h"
#include "crc.h"
#include "ldpc.h"
#include "unpack.h"

#include <stdbool.h>
#include <math.h>

/// Compute log likelihood log(p(1) / p(0)) of 174 message bits for later use in soft-decision LDPC decoding
/// @param[in] power Waterfall data collected during message slot
/// @param[in] cand Candidate to extract the message from
/// @param[in] code_map Symbol encoding map
/// @param[out] log174 Output of decoded log likelihoods for each of the 174 message bits
static void ft8_extract_likelihood(const waterfall_t* power, const candidate_t* cand, float* log174);

/// Packs a string of bits each represented as a zero/non-zero byte in bit_array[],
/// as a string of packed bits starting from the MSB of the first byte of packed[]
/// @param[in] plain Array of bits (0 and nonzero values) with num_bits entires
/// @param[in] num_bits Number of bits (entries) passed in bit_array
/// @param[out] packed Byte-packed bits representing the data in bit_array
static void pack_bits(const uint8_t bit_array[], int num_bits, uint8_t packed[]);

static float max2(float a, float b);
static float max4(float a, float b, float c, float d);
static void heapify_down(candidate_t heap[], int heap_size);
static void heapify_up(candidate_t heap[], int heap_size);
static void decode_symbol(const uint8_t* power, int bit_idx, float* log174);
static void decode_multi_symbols(const uint8_t* power, int num_bins, int n_syms, int bit_idx, float* log174);

static int get_index(const waterfall_t* power, const candidate_t* candidate)
{
    int offset = candidate->time_offset;
    offset = (offset * power->time_osr) + candidate->time_sub;
    offset = (offset * power->freq_osr) + candidate->freq_sub;
    offset = (offset * power->num_bins) + candidate->freq_offset;
    return offset;
}

static int ft8_sync_score(const waterfall_t* power, const candidate_t* candidate)
{
    int score = 0;
    int num_average = 0;

    // Get the pointer to symbol 0 of the candidate
    const uint8_t* mag_cand = power->mag + get_index(power, candidate);

    // Compute average score over sync symbols (m+k = 0-7, 36-43, 72-79)
    for (int m = 0; m < FT8_NUM_SYNC; ++m)
    {
        for (int k = 0; k < FT8_LENGTH_SYNC; ++k)
        {
            int block = (FT8_SYNC_OFFSET * m) + k;          // relative to the message
            int block_abs = candidate->time_offset + block; // relative to the captured signal
            // Check for time boundaries
            if (block_abs < 0)
                continue;
            if (block_abs >= power->num_blocks)
                break;

            // Get the pointer to symbol 'block' of the candidate
            const uint8_t* p8 = mag_cand + (block * power->block_stride);

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
            if ((k > 0) && (block_abs > 0))
            {
                // look one symbol back in time
                score += p8[sm] - p8[sm - power->block_stride];
                ++num_average;
            }
            if (((k + 1) < FT8_LENGTH_SYNC) && ((block_abs + 1) < power->num_blocks))
            {
                // look one symbol forward in time
                score += p8[sm] - p8[sm + power->block_stride];
                ++num_average;
            }
        }
    }

    if (num_average > 0)
        score /= num_average;

    return score;
}

static int ft4_sync_score(const waterfall_t* power, const candidate_t* candidate)
{
    int score = 0;
    int num_average = 0;

    // Get the pointer to symbol 0 of the candidate
    const uint8_t* mag_cand = power->mag + get_index(power, candidate);

    // Compute average score over sync symbols (block = 1-4, 34-37, 67-70, 100-103)
    for (int m = 0; m < FT4_NUM_SYNC; ++m)
    {
        for (int k = 0; k < FT4_LENGTH_SYNC; ++k)
        {
            int block = 1 + (FT4_SYNC_OFFSET * m) + k;
            int block_abs = candidate->time_offset + block;
            // Check for time boundaries
            if (block_abs < 0)
                continue;
            if (block_abs >= power->num_blocks)
                break;

            // Get the pointer to symbol 'block' of the candidate
            const uint8_t* p4 = mag_cand + (block * power->block_stride);

            // Check only the neighbors of the expected symbol frequency- and time-wise
            int sm = kFT4_Costas_pattern[m][k]; // Index of the expected bin
            if (sm > 0)
            {
                // look at one frequency bin lower
                score += p4[sm] - p4[sm - 1];
                ++num_average;
            }
            if (sm < 3)
            {
                // look at one frequency bin higher
                score += p4[sm] - p4[sm + 1];
                ++num_average;
            }
            if ((k > 0) && (block_abs > 0))
            {
                // look one symbol back in time
                score += p4[sm] - p4[sm - power->block_stride];
                ++num_average;
            }
            if (((k + 1) < FT4_LENGTH_SYNC) && ((block_abs + 1) < power->num_blocks))
            {
                // look one symbol forward in time
                score += p4[sm] - p4[sm + power->block_stride];
                ++num_average;
            }
        }
    }

    if (num_average > 0)
        score /= num_average;

    return score;
}

int ft8_find_sync(const waterfall_t* power, int num_candidates, candidate_t heap[], int min_score)
{
    int heap_size = 0;
    candidate_t candidate;

    // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
    // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
    // sync symbols we included in the score, so the score is averaged.
    for (candidate.time_sub = 0; candidate.time_sub < power->time_osr; ++candidate.time_sub)
    {
        for (candidate.freq_sub = 0; candidate.freq_sub < power->freq_osr; ++candidate.freq_sub)
        {
            for (candidate.time_offset = -12; candidate.time_offset < 24; ++candidate.time_offset)
            {
                for (candidate.freq_offset = 0; (candidate.freq_offset + 7) < power->num_bins; ++candidate.freq_offset)
                {
                    candidate.score = ft8_sync_score(power, &candidate);

                    if (candidate.score < min_score)
                        continue;

                    // If the heap is full AND the current candidate is better than
                    // the worst in the heap, we remove the worst and make space
                    if (heap_size == num_candidates && candidate.score > heap[0].score)
                    {
                        heap[0] = heap[heap_size - 1];
                        --heap_size;
                        heapify_down(heap, heap_size);
                    }

                    // If there's free space in the heap, we add the current candidate
                    if (heap_size < num_candidates)
                    {
                        heap[heap_size] = candidate;
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

void ft8_extract_likelihood(const waterfall_t* power, const candidate_t* cand, float* log174)
{
    const uint8_t* mag_cand = power->mag + get_index(power, cand);

    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);
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
            const uint8_t* ps = mag_cand + (sym_idx * power->block_stride);

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

bool ft8_decode(const waterfall_t* power, const candidate_t* cand, message_t* message, int max_iterations, decode_status_t* status)
{
    float log174[FT8_LDPC_N]; // message bits encoded as likelihood
    ft8_extract_likelihood(power, cand, log174);

    uint8_t plain174[FT8_LDPC_N]; // message bits (0/1)
    bp_decode(log174, max_iterations, plain174, &status->ldpc_errors);
    // ldpc_decode(log174, max_iterations, plain174, &status->ldpc_errors);

    if (status->ldpc_errors > 0)
    {
        return false;
    }

    // Extract payload + CRC (first FT8_LDPC_K bits) packed into a byte array
    uint8_t a91[FT8_LDPC_K_BYTES];
    pack_bits(plain174, FT8_LDPC_K, a91);

    // Extract CRC and check it
    status->crc_extracted = ftx_extract_crc(a91);
    // [1]: 'The CRC is calculated on the source-encoded message, zero-extended from 77 to 82 bits.'
    a91[9] &= 0xF8;
    a91[10] &= 0x00;
    status->crc_calculated = ftx_compute_crc(a91, 96 - 14);

    if (status->crc_extracted != status->crc_calculated)
    {
        return false;
    }

    status->unpack_status = unpack77(a91, message->text);

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
static void decode_symbol(const uint8_t* power, int bit_idx, float* log174)
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
static void decode_multi_symbols(const uint8_t* power, int num_bins, int n_syms, int bit_idx, float* log174)
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

// Packs a string of bits each represented as a zero/non-zero byte in plain[],
// as a string of packed bits starting from the MSB of the first byte of packed[]
static void pack_bits(const uint8_t bit_array[], int num_bits, uint8_t packed[])
{
    int num_bytes = (num_bits + 7) / 8;
    for (int i = 0; i < num_bytes; ++i)
    {
        packed[i] = 0;
    }

    uint8_t mask = 0x80;
    int byte_idx = 0;
    for (int i = 0; i < num_bits; ++i)
    {
        if (bit_array[i])
        {
            packed[byte_idx] |= mask;
        }
        mask >>= 1;
        if (!mask)
        {
            mask = 0x80;
            ++byte_idx;
        }
    }
}