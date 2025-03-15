#ifndef _INCLUDE_DECODE_H_
#define _INCLUDE_DECODE_H_

#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "waterfall.h"
#include "message.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// Structure that contains the status of various steps during decoding of a message
typedef struct
{
    float freq;
    float time;
    int ldpc_errors;         ///< Number of LDPC errors during decoding
    uint16_t crc_extracted;  ///< CRC value recovered from the message
    uint16_t crc_calculated; ///< CRC value calculated over the payload
    // int unpack_status;       ///< Return value of the unpack routine
} ftx_decode_status_t;

/// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
/// We treat and organize the candidate list as a min-heap (empty initially).
/// @param[in] wf Waterfall data collected during message slot
/// @param[in] sync_pattern Synchronization pattern
/// @param[in] num_candidates Number of maximum candidates (size of heap array)
/// @param[in,out] heap Array of ftx_candidate_t type entries (with num_candidates allocated entries)
/// @param[in] min_score Minimal score allowed for pruning unlikely candidates (can be zero for no effect)
/// @return Number of candidates filled in the heap
int ftx_find_candidates(const ftx_waterfall_t* wf, int num_candidates, ftx_candidate_t heap[], int min_score);

void ftx_extract_likelihood(const ftx_waterfall_t* wf, const ftx_candidate_t* cand, float* log174);

/// Attempt to decode a message candidate. Extracts the bit probabilities, runs LDPC decoder, checks CRC and unpacks the message in plain text.
/// @param[in] wf Waterfall data collected during message slot
/// @param[in] cand Candidate to decode
/// @param[in] max_iterations Maximum allowed LDPC iterations (lower number means faster decode, but less precise)
/// @param[out] message ftx_message_t structure that will receive the decoded message
/// @param[out] status ftx_decode_status_t structure that will be filled with the status of various decoding steps
/// @return True if the decoding was successful, false otherwise (check status for details)
// bool ftx_decode_candidate(const ftx_waterfall_t* wf, const ftx_candidate_t* cand, int max_iterations, ftx_message_t* message, ftx_decode_status_t* status);
bool ftx_decode_candidate(const float* log174, ftx_protocol_t protocol, int max_iterations, ftx_message_t* message, ftx_decode_status_t* status);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_DECODE_H_
