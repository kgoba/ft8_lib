#pragma once

#include "constants.h"

namespace ft8 {

    class BPDecoderState {
    public:
        void reset();
        int iterate(const float codeword[], uint8_t plain[]);

    private:
        float tov[ft8::N][3];
        float toc[ft8::M][7];
    };

    // codeword is 174 log-likelihoods.
    // plain is a return value, 174 ints, to be 0 or 1.
    // iters is how hard to try.
    // n_errors == 0 means success.
    void ldpc_decode(const float codeword[], int max_iters, uint8_t plain[], int *n_errors);

    void bp_decode(const float codeword[], int max_iters, uint8_t plain[], int *n_errors);

    // Packs a string of bits each represented as a zero/non-zero byte in plain[], 
    // as a string of packed bits starting from the MSB of the first byte of packed[]
    void pack_bits(const uint8_t plain[], int num_bits, uint8_t packed[]);

}
