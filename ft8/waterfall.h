#ifndef _INCLUDE_WATERFALL_H_
#define _INCLUDE_WATERFALL_H_

#include "constants.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    float mag;
    float phase;
} waterfall_cpx_t;

#define WATERFALL_USE_PHASE

#ifdef WATERFALL_USE_PHASE
#define WF_ELEM_T          waterfall_cpx_t
#define WF_ELEM_MAG(x)     ((x).mag)
#define WF_ELEM_MAG_INT(x) (int)(2 * ((x).mag + 120.0f))
#else
#define WF_ELEM_T          uint8_t
#define WF_ELEM_MAG(x)     ((float)(x) * 0.5f - 120.0f)
#define WF_ELEM_MAG_INT(x) (int)(x)
#endif

/// Input structure to ftx_find_sync() function. This structure describes stored waterfall data over the whole message slot.
/// Fields time_osr and freq_osr specify additional oversampling rate for time and frequency resolution.
/// If time_osr=1, FFT magnitude data is collected once for every symbol transmitted, i.e. every 1/6.25 = 0.16 seconds.
/// Values time_osr > 1 mean each symbol is further subdivided in time.
/// If freq_osr=1, each bin in the FFT magnitude data corresponds to 6.25 Hz, which is the tone spacing.
/// Values freq_osr > 1 mean the tone spacing is further subdivided by FFT analysis.
typedef struct
{
    int max_blocks;          ///< number of blocks (symbols) allocated in the mag array
    int num_blocks;          ///< number of blocks (symbols) stored in the mag array
    int num_bins;            ///< number of FFT bins in terms of 6.25 Hz
    int time_osr;            ///< number of time subdivisions
    int freq_osr;            ///< number of frequency subdivisions
    WF_ELEM_T* mag;          ///< FFT magnitudes stored as uint8_t[blocks][time_osr][freq_osr][num_bins]
    int block_stride;        ///< Helper value = time_osr * freq_osr * num_bins
    ftx_protocol_t protocol; ///< Indicate if using FT4 or FT8
} ftx_waterfall_t;

/// Output structure of ftx_find_sync() and input structure of ftx_decode().
/// Holds the position of potential start of a message in time and frequency.
typedef struct
{
    int16_t score;       ///< Candidate score (non-negative number; higher score means higher likelihood)
    int16_t time_offset; ///< Index of the time block
    int16_t freq_offset; ///< Index of the frequency bin
    uint8_t time_sub;    ///< Index of the time subdivision used
    uint8_t freq_sub;    ///< Index of the frequency subdivision used
} ftx_candidate_t;


#define FT8_DS_SYM_LEN (32)
#define FT8_DS_RATE (200)

WF_ELEM_T* wfall_get_element(const ftx_waterfall_t* wf,
    int16_t time_offset, uint8_t time_sub, int16_t freq_offset, uint8_t freq_sub);

void find_sync_fine(const float* wave, int num_samples, float sample_rate, int offset_crude, int *offset_fine, float *df_fine);
void extract_likelihood_fine(const float* wave, int num_samples, int start_pos, float dfphi, float* log174);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_WATERFALL_H_