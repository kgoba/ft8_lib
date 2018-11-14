#pragma once

// codeword is 174 log-likelihoods.
// plain is a return value, 174 ints, to be 0 or 1.
// iters is how hard to try.
// ok == 87 means success.
void ldpc_decode(float codeword[], int max_iters, int plain[], int *ok);

void bp_decode(float codeword[], int max_iters, int plain[], int *ok);
