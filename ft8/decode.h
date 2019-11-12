#pragma once

#include <stdint.h>

namespace ft8 {

struct MagArray {
	int num_blocks;	// number of total blocks (symbols)
	int num_bins;	// number of FFT bins
	int time_osr;	// number of time subdivisions
	int freq_osr;	// number of frequency subdivisions
	uint8_t * mag;	// FFT magnitudes as [blocks][time_sub][freq_sub][num_bins]
};

struct Candidate {
    int16_t      score;
    int16_t      time_offset;
    int16_t      freq_offset;
    uint8_t      time_sub;
    uint8_t      freq_sub;
};


// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
// We treat and organize the candidate list as a min-heap (empty initially).
int find_sync(const MagArray * power, const uint8_t *sync_map, int num_candidates, Candidate *heap, int min_score = 0);


// Compute log likelihood log(p(1) / p(0)) of 174 message bits 
// for later use in soft-decision LDPC decoding
void extract_likelihood(const MagArray *power, const Candidate & cand, const uint8_t *code_map, float *log174);

}
