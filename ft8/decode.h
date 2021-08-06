#ifndef _INCLUDE_DECODE_H_
#define _INCLUDE_DECODE_H_

#include <stdint.h>

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
} MagArray;

/// Output structure of find_sync() and input structure of extract_likelihood().
/// Holds the position of potential start of a message in time and frequency.
typedef struct
{
    int16_t score;       ///< Candidate score (non-negative number; higher score means higher likelihood)
    int16_t time_offset; ///< Index of the time block
    int16_t freq_offset; ///< Index of the frequency bin
    uint8_t time_sub;    ///< Index of the time subdivision used
    uint8_t freq_sub;    ///< Index of the frequency subdivision used
} Candidate;

/// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
/// We treat and organize the candidate list as a min-heap (empty initially).
/// @param[in] power Waterfall data collected during message slot
/// @param[in] sync_pattern Synchronization pattern
/// @param[in] num_candidates Number of maximum candidates (size of heap array)
/// @param[in,out] heap Array of Candidate type entries (with num_candidates allocated entries)
/// @param[in] min_score Minimal score allowed for trimming unlikely candidates (can be zero for no effect)
/// @return Number of candidates filled in the heap
int find_sync(const MagArray *power, int num_candidates, Candidate heap[], int min_score);

/// Compute log likelihood log(p(1) / p(0)) of 174 message bits for later use in soft-decision LDPC decoding
/// @param[in] power Waterfall data collected during message slot
/// @param[in] cand Candidate to extract the message from
/// @param[in] code_map Symbol encoding map
/// @param[out] log174 Output of decoded log likelihoods for each of the 174 message bits
void extract_likelihood(const MagArray *power, const Candidate *cand, float *log174);

#endif // _INCLUDE_DECODE_H_
