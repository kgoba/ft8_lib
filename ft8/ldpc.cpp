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
// cc -O3 libldpc.c -shared -fPIC -o libldpc.so
//

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "arrays.h"

int ldpc_check(int codeword[]);


// thank you Douglas Bagnall
// https://math.stackexchange.com/a/446411
float fast_tanh(float x)
{
    if (x < -4.97f)
    {
        return -1.0f;
    }
    if (x > 4.97f)
    {
        return 1.0f;
    }
    float x2 = x * x;
    float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}


// codeword is 174 log-likelihoods.
// plain is a return value, 174 ints, to be 0 or 1.
// iters is how hard to try.
// ok == 87 means success.
void ldpc_decode(float codeword[], int iters, int plain[], int *ok)
{
    float m[87][174];       // ~60 kB
    float e[87][174];       // ~60 kB
    int best_score = -1;
    int best_cw[174];

    for (int i = 0; i < 174; i++)
        for (int j = 0; j < 87; j++)
            m[j][i] = codeword[i];

    for (int i = 0; i < 174; i++)
        for (int j = 0; j < 87; j++)
            e[j][i] = 0.0f;

    for (int iter = 0; iter < iters; iter++)
    {
        for (int j = 0; j < 87; j++)
        {
            for (int ii1 = 0; ii1 < 7; ii1++)
            {
                int i1 = Nm[j][ii1] - 1;
                if (i1 < 0)
                    continue;
                float a = 1.0f;
                for (int ii2 = 0; ii2 < 7; ii2++)
                {
                    int i2 = Nm[j][ii2] - 1;
                    if (i2 >= 0 && i2 != i1)
                    {
                        a *= fast_tanh(m[j][i2] / 2.0f);
                    }
                }
                e[j][i1] = log((1 + a) / (1 - a));
            }
        }

        int cw[174];
        for (int i = 0; i < 174; i++)
        {
            float l = codeword[i];
            for (int j = 0; j < 3; j++)
                l += e[Mn[i][j] - 1][i];
            cw[i] = (l <= 0.0f);
        }
        int score = ldpc_check(cw);
        if (score == 87)
        {
            // Found a perfect answer
#if 0
      int cw1[174];
      for(int i = 0; i < 174; i++)
        cw1[i] = cw[colorder[i]];
      for(int i = 0; i < 87; i++)
        plain[i] = cw1[174-87+i];
#else
            for (int i = 0; i < 174; i++)
                plain[i] = cw[colorder[i]];
#endif
            *ok = 87;
            return;
        }

        if (score > best_score)
        {
            for (int i = 0; i < 174; i++)
                best_cw[i] = cw[i];
            best_score = score;
        }

        for (int i = 0; i < 174; i++)
        {
            for (int ji1 = 0; ji1 < 3; ji1++)
            {
                int j1 = Mn[i][ji1] - 1;
                float l = codeword[i];
                for (int ji2 = 0; ji2 < 3; ji2++)
                {
                    if (ji1 != ji2)
                    {
                        int j2 = Mn[i][ji2] - 1;
                        l += e[j2][i];
                    }
                }
                m[j1][i] = l;
            }
        }
    }

    // decode didn't work, return something anyway.
#if 0
  int cw1[174];
  for(int i = 0; i < 174; i++)
    cw1[i] = best_cw[colorder[i]];
  for(int i = 0; i < 87; i++)
    plain[i] = cw1[174-87+i];
#else
    for (int i = 0; i < 174; i++)
        plain[i] = best_cw[colorder[i]];
#endif

    *ok = best_score;
}


//
// does a 174-bit codeword pass the FT8's LDPC parity checks?
// returns the number of parity checks that passed.
// 87 means total success.
//
int ldpc_check(int codeword[])
{
    int score = 0;

    // Nm[87][7]
    for (int j = 0; j < 87; j++)
    {
        int x = 0;
        for (int ii1 = 0; ii1 < 7; ii1++)
        {
            int i1 = Nm[j][ii1] - 1;
            if (i1 >= 0)
            {
                x ^= codeword[i1];
            }
        }
        if (x == 0)
            score++;
    }
    return score;
}


/*
def bp_decode(codeword, max_iterations = 10):
    ## 174 codeword bits
    ## 87 parity checks

    mnx = numpy.array(Mn, dtype=numpy.int32)
    nmx = numpy.array(Nm, dtype=numpy.int32)

    ncw = 3
    tov = numpy.zeros( (3, N) )
    toc = numpy.zeros( (7, M) )
    tanhtoc = numpy.zeros( (7, M) )
    zn = numpy.zeros(N)
    nclast = 0
    ncnt = 0

    # initialize messages to checks
    for j in range(M):
        for i in range(nrw[j]):
            toc[i, j] = codeword[nmx[j, i] - 1]
    
    for iteration in range(max_iterations):
        # Update bit log likelihood ratios (tov=0 in iteration 0).
        #for i in range(N):
        #    zn[i] = codeword[i] + numpy.sum(tov[:,i])
        zn = codeword + numpy.sum(tov, axis = 0)
        #print(numpy.sum(tov, axis=0))

        # Check to see if we have a codeword (check before we do any iteration).
        cw = numpy.zeros(N, dtype=numpy.int32)
        cw[zn > 0] = 1
        ncheck = 0
        for i in range(M):
            synd = numpy.sum(cw[ nmx[i, :nrw[i]]-1 ])
            if synd % 2 > 0:
                ncheck += 1

        if ncheck == 0: 
            # we have a codeword - reorder the columns and return it
            codeword = cw[colorder]
            #nerr = 0
            #for i in range(N):
            #    if (2*cw[i]-1)*codeword[i] < 0:
            #        nerr += 1
            
            #print("DECODED!", nerr)
            return codeword[M:N]

        if iter > 0:
            # this code block implements an early stopping criterion
            nd = ncheck - nclast
            if nd < 0: # of unsatisfied parity checks decreased
                ncnt = 0  # reset counter
            else:
                ncnt += 1
            
            if ncnt >= 5 and iter >= 10 and ncheck >= 15:
                nharderror = -1
                #return numpy.array([])

        nclast = ncheck

        # Send messages from bits to check nodes 
        for j in range(M):
            for i in range(nrw[j]):
                ibj = nmx[j, i] - 1
                toc[i, j] = zn[ibj]
                for kk in range(ncw): # subtract off what the bit had received from the check
                    if mnx[ibj, kk] - 1 == j:
                        toc[i, j] -= tov[kk, ibj]

        # send messages from check nodes to variable nodes
        #for i in range(M):
        #    tanhtoc[:,i] = numpy.tanh(-toc[:,i] / 2)
        tanhtoc = numpy.tanh(-toc / 2)

        for j in range(N):
            for i in range(ncw):
                ichk = mnx[j, i] - 1  # Mn(:,j) are the checks that include bit j
                Tmn = 1.0
                for k in range(nrw[ichk]):
                    if nmx[ichk, k] - 1 == j: continue
                    Tmn *= tanhtoc[k, ichk]
                y = numpy.arctanh(-Tmn)
                #y = platanh(-Tmn)
                tov[i, j] = 2*y

    return numpy.array([])
*/