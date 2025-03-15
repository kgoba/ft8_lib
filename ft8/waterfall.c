#include "waterfall.h"

#include <math.h>
#include <complex.h>
#include <common/common.h>

#define LOG_LEVEL LOG_INFO
#include "debug.h"

WF_ELEM_T* wfall_get_element(const ftx_waterfall_t* wf, 
    int16_t time_offset, uint8_t time_sub, int16_t freq_offset, uint8_t freq_sub)
{
    int offset = time_offset;
    offset = (offset * wf->time_osr) + time_sub;
    offset = (offset * wf->freq_osr) + freq_sub;
    offset = (offset * wf->num_bins) + freq_offset;
    return wf->mag + offset;
}

static float cabs2f(complex float z)
{
    return crealf(z) * crealf(z) + cimagf(z) * cimagf(z);
}

static float score_sync_fine(const float* wave, int num_samples, int i0, float dfphi)
{
    float sync = 0;
    int num_sync = 0;

    // Sum over 7 Costas frequencies ...
    for (int i = 0; i < 7; ++i)
    {
        complex float csync2[FT8_DS_SYM_LEN];
        float dphi = dfphi + (float)TWO_PI * kFT8_Costas_pattern[i] / FT8_DS_SYM_LEN;
        float phi = 0;
        for (int k = 0; k < FT8_DS_SYM_LEN; ++k)
        {
            csync2[k] = cosf(phi) - I * sinf(phi);
            phi = fmodf(phi + dphi, (float)TWO_PI);
        }

        // ... and three Costas arrays
        for (int j = 0; j <= 72; j += 36)
        {
            int i1 = i0 + (i + j) * FT8_DS_SYM_LEN;
            
            complex float z = 0;
            for (int k = 0; k < FT8_DS_SYM_LEN; ++k)
            {
                if (i1 + k >= 0 && i1 + k < num_samples)
                    z += wave[i1 + k] * csync2[k];
            }
            sync += cabs2f(z);
            ++num_sync;
        }
    }
    return sync;
}

void find_sync_fine(const float* wave, int num_samples, float sample_rate, int offset_crude, int *offset_fine, float *df_fine)
{
    float score_max = 0;
    int i0_max = -1;
    for (int i0 = 0; i0 < FT8_DS_SYM_LEN; ++i0)
    {
        float score = score_sync_fine(wave, num_samples, offset_crude + i0, 0.0f);
        if ((i0_max < 0) || (score > score_max))
        {
            i0_max = i0;
            score_max = score;
        }
    }
    LOG(LOG_DEBUG, "i0_max = %d, score_max = %f\n", i0_max, score_max);

    float df_max = 0;
    for (float df = -3.2f; df <= 3.2f; df += 0.1f)
    {
        float dfphi = (float)TWO_PI * df / sample_rate;
        float score = score_sync_fine(wave, num_samples, offset_crude + i0_max, dfphi);
        if (score > score_max)
        {
            df_max = df;
            score_max = score;
        }
    }
    LOG(LOG_DEBUG, "df_max = %+.1f, score_max = %f\n", df_max, score_max);

    float dfphi = (float)TWO_PI * df_max / sample_rate;
    for (int i0 = 0; i0 < FT8_DS_SYM_LEN; ++i0)
    {
        float score = score_sync_fine(wave, num_samples, offset_crude + i0, dfphi);
        if (score > score_max)
        {
            i0_max = i0;
            score_max = score;
        }
    }
    LOG(LOG_DEBUG, "i0_max = %d, score_max = %f\n", i0_max, score_max);

    *offset_fine = i0_max;
    *df_fine = df_max;
}

static float max2(float a, float b)
{
    return (a >= b) ? a : b;
}

static float max4(float a, float b, float c, float d)
{
    return max2(max2(a, b), max2(c, d));
}

static void ftx_normalize_logl(float* log174)
{
    // Compute the variance of log174
    float sum = 0;
    float sum2 = 0;
    for (int i = 0; i < FTX_LDPC_N; ++i)
    {
        sum += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float inv_n = 1.0f / FTX_LDPC_N;
    float variance = (sum2 - (sum * sum * inv_n)) * inv_n;

    // Normalize log174 distribution and scale it with experimentally found coefficient
    float norm_factor = sqrtf(16.0f / variance);
    for (int i = 0; i < FTX_LDPC_N; ++i)
    {
        log174[i] *= norm_factor;
    }
}

void extract_likelihood_fine(const float* wave, int num_samples, int start_pos, float dfphi, float* log174)
{
    complex float dft[FT8_NUM_TONES][FT8_DS_SYM_LEN];

    for (int i = 0; i < FT8_NUM_TONES; ++i)
    {
        float dphi = dfphi + (float)TWO_PI * kFT8_Gray_map[i] / FT8_DS_SYM_LEN;
        float phi = 0;
        for (int k = 0; k < FT8_DS_SYM_LEN; ++k)
        {
            dft[i][k] = cosf(phi) - I * sinf(phi);
            phi = fmodf(phi + dphi, (float)TWO_PI);
        }
    }

    for (int k = 0; k < FT8_ND; ++k)
    {
        // Skip either 7 or 14 sync symbols
        // TODO: replace magic numbers with constants
        int sym_idx = k + ((k < 29) ? 7 : 14);
        int bit_idx = 3 * k;

        float s2[FT8_NUM_TONES];
        int wave_pos = start_pos + sym_idx * FT8_DS_SYM_LEN;
        for (int j = 0; j < FT8_NUM_TONES; ++j)
        {
            complex float sum = 0;
            for (int k = 0; k < FT8_DS_SYM_LEN; ++k)
            {
                if ((wave_pos + k >= 0) && (wave_pos + k < num_samples))
                {
                    sum += wave[wave_pos + k] * dft[j][k];
                }
            }
            float mag2 = cabs2f(sum);
            s2[j] = 10.0f * log10f(1E-12f + mag2);
        }

        log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
        log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
        log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
        // LOG(LOG_INFO, "log174[%d] @%d = %f, %f, %f\n", bit_idx, sym_idx, log174[bit_idx + 0], log174[bit_idx + 1], log174[bit_idx + 2]);
    }

    ftx_normalize_logl(log174);
}
