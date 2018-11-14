//
// LDPC decoder for FT8.
//
// given a 174-bit codeword as an array of log-likelihood of zero,
// return a 174-bit corrected codeword, or zero-length array.
// last 87 bits are the (systematic) plain-text.
// this is an implementation of the sum-product algorithm
// from Sarah Johnson's Iterative Error Correction book.
// codeword[i] = log ( P(x=0) / P(x=1) )
//

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "arrays.h"

int ldpc_check(int codeword[]);

const int N = 174;
const int M = 87;

// https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
// http://functions.wolfram.com/ElementaryFunctions/ArcTanh/10/0001/
// https://mathr.co.uk/blog/2017-09-06_approximating_hyperbolic_tangent.html


// thank you Douglas Bagnall
// https://math.stackexchange.com/a/446411
float fast_tanh(float x) {
    if (x < -4.97f) {
        return -1.0f;
    }
    if (x > 4.97f) {
        return 1.0f;
    }
    float x2 = x * x;
    //float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    //float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    //float a = x * (10395.0f + x2 * (1260.0f + x2 * 21.0f));
    //float b = 10395.0f + x2 * (4725.0f + x2 * (210.0f + x2));
    float a = x * (945.0f + x2 * (105.0f + x2));
    float b = 945.0f + x2 * (420.0f + x2 * 15.0f);
    return a / b;
}


float fast_atanh(float x) {
    float x2 = x * x;
    //float a = x * (-15015.0f + x2 * (19250.0f + x2 * (-5943.0f + x2 * 256.0f)));
    //float b = (-15015.0f + x2 * (24255.0f + x2 * (-11025.0f + x2 * 1225.0f)));
    //float a = x * (-1155.0f + x2 * (1190.0f + x2 * -231.0f));
    //float b = (-1155.0f + x2 * (1575.0f + x2 * (-525.0f + x2 * 25.0f)));
    float a = x * (945.0f + x2 * (-735.0f + x2 * 64.0f));
    float b = (945.0f + x2 * (-1050.0f + x2 * 225.0f));
    return a / b;
}


float pltanh(float x) {
    float isign = +1;
    if (x < 0) {
        isign = -1;
        x = -x;
    }
    if (x < 0.8f) {
        return isign * 0.83 * x;
    }
    if (x < 1.6f) {
        return isign * (0.322f * x + 0.4064f);
    }
    if (x < 3.0f) {
        return isign * (0.0524f * x + 0.8378f);
    }
    if (x < 7.0f) {
        return isign * (0.0012f * x + 0.9914f);
    }
    return isign*0.9998f;
}


float platanh(float x) {
    float isign = +1;
    if (x < 0) {
        isign = -1;
        x = -x;
    }
    if (x < 0.664f) {
        return isign * x / 0.83f;
    }
    if (x < 0.9217f) {
        return isign * (x - 0.4064f) / 0.322f;
    }
    if (x < 0.9951f) {
        return isign * (x - 0.8378f) / 0.0524f;
    }
    if (x < 0.9998f) {
        return isign * (x - 0.9914f) / 0.0012f;
    }
    return isign * 7.0f;
}


// codeword is 174 log-likelihoods.
// plain is a return value, 174 ints, to be 0 or 1.
// max_iters is how hard to try.
// ok == 87 means success.
void ldpc_decode(float codeword[], int max_iters, int plain[], int *ok) {
    float m[M][N];       // ~60 kB
    float e[M][N];       // ~60 kB
    int best_score = -1;

    for (int j = 0; j < M; j++) {
        for (int i = 0; i < N; i++) {
            m[j][i] = codeword[i];
            e[j][i] = 0.0f;
        }
    }

    for (int iter = 0; iter < max_iters; iter++) {
        for (int j = 0; j < M; j++) {
            for (int ii1 = 0; ii1 < nrw[j]; ii1++) {
                int i1 = Nm[j][ii1] - 1;
                float a = 1.0f;
                for (int ii2 = 0; ii2 < nrw[j]; ii2++) {
                    int i2 = Nm[j][ii2] - 1;
                    if (i2 != i1) {
                        a *= fast_tanh(-m[j][i2] / 2.0f);
                    }
                }
                e[j][i1] = logf((1 - a) / (1 + a));
            }
        }

        int cw[N];
        for (int i = 0; i < N; i++) {
            float l = codeword[i];
            for (int j = 0; j < 3; j++)
                l += e[Mn[i][j] - 1][i];
            cw[i] = (l > 0) ? 1 : 0;
        }

        int score = ldpc_check(cw);

        if (score > best_score) {
            for (int i = 0; i < N; i++) {
                plain[i] = cw[colorder[i]];
            }
            best_score = score;

            if (score == M) {
                // Found a perfect answer
                break;
            }
        }


        for (int i = 0; i < N; i++) {
            for (int ji1 = 0; ji1 < 3; ji1++) {
                int j1 = Mn[i][ji1] - 1;
                float l = codeword[i];
                for (int ji2 = 0; ji2 < 3; ji2++) {
                    if (ji1 != ji2) {
                        int j2 = Mn[i][ji2] - 1;
                        l += e[j2][i];
                    }
                }
                m[j1][i] = l;
            }
        }
    }

    *ok = best_score;
}


//
// does a 174-bit codeword pass the FT8's LDPC parity checks?
// returns the number of parity checks that passed.
// 87 means total success.
//
int ldpc_check(int codeword[]) {
    int score = 0;

    // Nm[87][7]
    for (int j = 0; j < M; ++j) {
        int x = 0;
        for (int ii1 = 0; ii1 < nrw[j]; ++ii1) {
            x ^= codeword[Nm[j][ii1] - 1];
        }
        if (x == 0) {
            ++score;
        }
    }
    return score;
}


void bp_decode(float codeword[], int max_iters, int plain[], int *ok) {
    float tov[N][3];
    float toc[M][7];

    int best_score = -1;

    int nclast = 0;
    int ncnt = 0;

    // initialize messages to checks
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < nrw[i]; ++j) {
            toc[i][j] = codeword[Nm[i][j] - 1];
        }
    }

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < 3; ++j) {
            tov[i][j] = 0;
        }
    }

    for (int iter = 0; iter < max_iters; ++iter) {
        float   zn[N];
        int     cw[N];

        // Update bit log likelihood ratios (tov=0 in iter 0)
        for (int i = 0; i < N; ++i) {
            zn[i] = codeword[i] + tov[i][0] + tov[i][1] + tov[i][2];
            cw[i] = (zn[i] > 0) ? 1 : 0;
        }

        // Check to see if we have a codeword (check before we do any iter)
        int score = ldpc_check(cw);

        if (score > best_score) {
            // we have a better guess - reorder the columns and store it
            for (int i = 0; i < N; i++) {
                plain[i] = cw[colorder[i]];
            }
            best_score = score;

            if (score == M) {
                break;
            }
        }

        // Send messages from bits to check nodes 
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < nrw[i]; ++j) {
                int ibj = Nm[i][j] - 1;
                toc[i][j] = zn[ibj];
                for (int kk = 0; kk < 3; ++kk) { 
                    // subtract off what the bit had received from the check
                    if (Mn[ibj][kk] - 1 == i) {
                        toc[i][j] -= tov[ibj][kk];
                    }
                }
            }
        }
        
        // send messages from check nodes to variable nodes
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < nrw[i]; ++j) {
                //toc[i][j] = pltanh(-toc[i][j] / 2);
                toc[i][j] = fast_tanh(-toc[i][j] / 2);
                //toc[i][j] = tanhf(-toc[i][j] / 2);
            }
        }

        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < 3; ++j) {
                int ichk = Mn[i][j] - 1; // Mn(:,j) are the checks that include bit j
                float Tmn = 1.0f;
                for (int k = 0; k < nrw[ichk]; ++k) {
                    if (Nm[ichk][k] - 1 != i) {
                        Tmn *= toc[ichk][k];
                    }
                }
                //tov[i][j] = 2 * platanh(-Tmn);
                tov[i][j] = 2 * fast_atanh(-Tmn);
                //tov[i][j] = 2 * atanhf(-Tmn);
            }
        }
    }

    *ok = best_score;
}
