#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ft8.h"

#include "pack.h"
#include "constants.h"
#include "crc.h"
#include "common/debug.h"

#warning replace with typed constants
#define FT8_SYMBOL_RATE 6.25f // tone deviation (and symbol rate) in Hz
#define FT8_SYMBOL_BT 2.0f    // symbol smoothing filter bandwidth factor (BT)

#define FT4_SYMBOL_RATE 20.833333f // tone deviation (and symbol rate) in Hz
#define FT4_SYMBOL_BT 1.0f         // symbol smoothing filter bandwidth factor (BT)

// pi * sqrt(2 / log(2))
static const float kFT8_GFSK_const = 5.336446f;


// Returns 1 if an odd number of bits are set in x, zero otherwise
static uint8_t parity8(uint8_t x)
{
    x ^= x >> 4;  // a b c d ae bf cg dh
    x ^= x >> 2;  // a b ac bd cae dbf aecg bfdh
    x ^= x >> 1;  // a ab bac acbd bdcae caedbf aecgbfdh
    return x % 2; // modulo 2
}

// Encode a 91-bit message and return a 174-bit codeword.
// The generator matrix has dimensions (87,87).
// The code is a (174,91) regular LDPC code with column weight 3.
// Arguments:
// [IN] message   - array of 91 bits stored as 12 bytes (MSB first)
// [OUT] codeword - array of 174 bits stored as 22 bytes (MSB first)
static void encode174(const uint8_t *message, uint8_t *codeword)
{
    // This implementation accesses the generator bits straight from the packed binary representation in kFT8_LDPC_generator

    // Fill the codeword with message and zeros, as we will only update binary ones later
    for (int j = 0; j < FT8_LDPC_N_BYTES; ++j)
    {
        codeword[j] = (j < FT8_LDPC_K_BYTES) ? message[j] : 0;
    }

    // Compute the byte index and bit mask for the first checksum bit
    uint8_t col_mask = (0x80u >> (FT8_LDPC_K % 8u)); // bitmask of current byte
    uint8_t col_idx = FT8_LDPC_K_BYTES - 1;          // index into byte array

    // Compute the LDPC checksum bits and store them in codeword
    for (int i = 0; i < FT8_LDPC_M; ++i)
    {
        // Fast implementation of bitwise multiplication and parity checking
        // Normally nsum would contain the result of dot product between message and kFT8_LDPC_generator[i],
        // but we only compute the sum modulo 2.
        uint8_t nsum = 0;
        for (int j = 0; j < FT8_LDPC_K_BYTES; ++j)
        {
            uint8_t bits = message[j] & kFT8_LDPC_generator[i][j]; // bitwise AND (bitwise multiplication)
            nsum ^= parity8(bits);                                 // bitwise XOR (addition modulo 2)
        }

        // Set the current checksum bit in codeword if nsum is odd
        if (nsum % 2)
        {
            codeword[col_idx] |= col_mask;
        }

        // Update the byte index and bit mask for the next checksum bit
        col_mask >>= 1;
        if (col_mask == 0)
        {
            col_mask = 0x80u;
            ++col_idx;
        }
    }
}

static void genft8(const uint8_t *payload, uint8_t *tones)
{
    uint8_t a91[12]; // Store 77 bits of payload + 14 bits CRC

    // Compute and add CRC at the end of the message
    // a91 contains 77 bits of payload + 14 bits of CRC
    ft8_add_crc(payload, a91);

    uint8_t codeword[22];
    encode174(a91, codeword);

    // Message structure: S7 D29 S7 D29 S7
    // Total symbols: 79 (FT8_NN)

    uint8_t mask = 0x80u; // Mask to extract 1 bit from codeword
    int i_byte = 0;       // Index of the current byte of the codeword
    for (int i_tone = 0; i_tone < FT8_NN; ++i_tone)
    {
        if ((i_tone >= 0) && (i_tone < 7))
        {
            tones[i_tone] = kFT8_Costas_pattern[i_tone];
        }
        else if ((i_tone >= 36) && (i_tone < 43))
        {
            tones[i_tone] = kFT8_Costas_pattern[i_tone - 36];
        }
        else if ((i_tone >= 72) && (i_tone < 79))
        {
            tones[i_tone] = kFT8_Costas_pattern[i_tone - 72];
        }
        else
        {
            // Extract 3 bits from codeword at i-th position
            uint8_t bits3 = 0;

            if (codeword[i_byte] & mask)
                bits3 |= 4;
            if (0 == (mask >>= 1))
            {
                mask = 0x80u;
                i_byte++;
            }
            if (codeword[i_byte] & mask)
                bits3 |= 2;
            if (0 == (mask >>= 1))
            {
                mask = 0x80u;
                i_byte++;
            }
            if (codeword[i_byte] & mask)
                bits3 |= 1;
            if (0 == (mask >>= 1))
            {
                mask = 0x80u;
                i_byte++;
            }

            tones[i_tone] = kFT8_Gray_map[bits3];
        }
    }
}

static void genft4(const uint8_t *payload, uint8_t *tones)
{
    uint8_t a91[12]; // Store 77 bits of payload + 14 bits CRC

    // Compute and add CRC at the end of the message
    // a91 contains 77 bits of payload + 14 bits of CRC
    ft8_add_crc(payload, a91);

    uint8_t codeword[22];
    encode174(a91, codeword); // 91 bits -> 174 bits

    // Message structure: R S4_1 D29 S4_2 D29 S4_3 D29 S4_4 R
    // Total symbols: 105 (FT4_NN)

    uint8_t mask = 0x80u; // Mask to extract 1 bit from codeword
    int i_byte = 0;       // Index of the current byte of the codeword
    for (int i_tone = 0; i_tone < FT4_NN; ++i_tone)
    {
        if ((i_tone == 0) || (i_tone == 104))
        {
            tones[i_tone] = 0; // R (ramp) symbol
        }
        else if ((i_tone >= 1) && (i_tone < 5))
        {
            tones[i_tone] = kFT4_Costas_pattern[0][i_tone - 1];
        }
        else if ((i_tone >= 34) && (i_tone < 38))
        {
            tones[i_tone] = kFT4_Costas_pattern[1][i_tone - 34];
        }
        else if ((i_tone >= 67) && (i_tone < 71))
        {
            tones[i_tone] = kFT4_Costas_pattern[2][i_tone - 67];
        }
        else if ((i_tone >= 100) && (i_tone < 104))
        {
            tones[i_tone] = kFT4_Costas_pattern[3][i_tone - 100];
        }
        else
        {
            // Extract 2 bits from codeword at i-th position
            uint8_t bits2 = 0;

            if (codeword[i_byte] & mask)
                bits2 |= 2;
            if (0 == (mask >>= 1))
            {
                mask = 0x80u;
                i_byte++;
            }
            if (codeword[i_byte] & mask)
                bits2 |= 1;
            if (0 == (mask >>= 1))
            {
                mask = 0x80u;
                i_byte++;
            }
            tones[i_tone] = kFT4_Gray_map[bits2];
        }
    }
}

/// Computes a GFSK smoothing pulse.
/// The pulse is theoretically infinitely long, however, here it's truncated at 3 times the symbol length.
/// This means the pulse array has to have space for 3*n_spsym elements.
/// @param[in] n_spsym Number of samples per symbol
/// @param[in] symbol_bt Shape parameter (values defined for FT8/FT4)
/// @param[out] pulse Output array of pulse samples
///
static void gfsk_pulse(int n_spsym, float symbol_bt, float *pulse)
{
    for (int i = 0; i < 3 * n_spsym; ++i)
    {
        float t = i / (float)n_spsym - 1.5f;
        float arg1 = kFT8_GFSK_const * symbol_bt * (t + 0.5f);
        float arg2 = kFT8_GFSK_const * symbol_bt * (t - 0.5f);
        pulse[i] = (erff(arg1) - erff(arg2)) / 2;
    }
}

/// Synthesize waveform data using GFSK phase shaping.
/// The output waveform will contain n_sym symbols.
/// @param[in] symbols Array of symbols (tones) (0-7 for FT8)
/// @param[in] n_sym Number of symbols in the symbol array
/// @param[in] f0 Audio frequency in Hertz for the symbol 0 (base frequency)
/// @param[in] symbol_bt Symbol smoothing filter bandwidth (2 for FT8, 1 for FT4)
/// @param[in] symbol_rate Rate of symbols per second, Hertz
/// @param[in] signal_rate Sample rate of synthesized signal, Hertz
/// @param[out] signal Output array of signal waveform samples (should have space for n_sym*n_spsym samples)
///
static void synth_gfsk(const uint8_t *symbols, int n_sym, float f0, float symbol_bt, float symbol_rate, int signal_rate, float *signal)
{
    int n_spsym = (int)(0.5f + signal_rate / symbol_rate); // Samples per symbol
    int n_wave = n_sym * n_spsym;                          // Number of output samples
    float hmod = 1.0f;

    LOG(LOG_INFO, "n_spsym = %d\n", n_spsym);

    // Compute the smoothed frequency waveform.
    // Length = (nsym+2)*n_spsym samples, first and last symbols extended
    float dphi_peak = 2 * M_PI * hmod / n_spsym;
    float dphi[n_wave + 2 * n_spsym];

    // Shift frequency up by f0
    for (int i = 0; i < n_wave + 2 * n_spsym; ++i)
    {
        dphi[i] = 2 * M_PI * f0 / signal_rate;
    }

    float pulse[3 * n_spsym];
    gfsk_pulse(n_spsym, symbol_bt, pulse);

    for (int i = 0; i < n_sym; ++i)
    {
        int ib = i * n_spsym;
        for (int j = 0; j < 3 * n_spsym; ++j)
        {
            dphi[j + ib] += dphi_peak * symbols[i] * pulse[j];
        }
    }

    // Add dummy symbols at beginning and end with tone values equal to 1st and last symbol, respectively
    for (int j = 0; j < 2 * n_spsym; ++j)
    {
        dphi[j] += dphi_peak * pulse[j + n_spsym] * symbols[0];
        dphi[j + n_sym * n_spsym] += dphi_peak * pulse[j] * symbols[n_sym - 1];
    }
    
    // Calculate and insert the audio waveform
    float phi = 0;
    for (int k = 0; k < n_wave; ++k)
    { // Don't include dummy symbols
        signal[k] = sinf(phi);
        phi = fmodf(phi + dphi[k + n_spsym], 2 * M_PI);
    }
    
    // Apply envelope shaping to the first and last symbols
    int n_ramp = n_spsym / 8;
    for (int i = 0; i < n_ramp; ++i)
    {
        float env = (1 - cosf(2 * M_PI * i / (2 * n_ramp))) / 2;
        signal[i] *= env;
        signal[n_wave - 1 - i] *= env;
    }
}

// generate FT4 or FT8 signal for message
static int ftX_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate, bool is_ft4)
{
    // number of used samples in signal
    int num_tones = (is_ft4) ? FT4_NN : FT8_NN;
    float symbol_rate = (is_ft4) ? FT4_SYMBOL_RATE : FT8_SYMBOL_RATE;
    int signal_samples = 0.5f + num_tones / symbol_rate * sample_rate;
    float symbol_bt = (is_ft4) ? FT4_SYMBOL_BT : FT8_SYMBOL_BT;

    // check if we were provided with enough space
    if (num_samples >= signal_samples)
    {
        // first, pack the text data into binary message
        uint8_t packed[FT8_LDPC_K_BYTES];
        int rc = ft8_pack77(message, packed);
        if (rc >= 0)
        {
            // array of 79 tones (symbols)
            uint8_t tones[num_tones];
            
            if (LOG_LEVEL == LOG_DEBUG) {
                LOG(LOG_LEVEL, "Packed data: ");
                for (int j = 0; j < 10; ++j)
                {
                    LOG(LOG_LEVEL, "%02x ", packed[j]);
                }
                LOG(LOG_LEVEL, "\n");
            }
            
            // '[..] for FT4 only, in order to avoid transmitting a long string of zeros when sending CQ messages,
            // the assembled 77-bit message is bitwise exclusive-OR’ed with [a] pseudorandom sequence before computing the CRC and FEC parity bits'
            if (is_ft4) {
                for (int i = 0; i < 10; ++i)
                {
                    packed[i] ^= kFT4_XOR_sequence[i];
                }
            }
            
            // second, encode the binary message as a sequence of FSK tones
            if (is_ft4)
            {
                genft4(packed, tones);
            }
            else
            {
                genft8(packed, tones);
            }
            
            if (LOG_LEVEL == LOG_DEBUG) {
                LOG(LOG_LEVEL, "FSK tones: ");
                for (int j = 0; j < num_tones; ++j)
                {
                    LOG(LOG_LEVEL, "%d", tones[j]);
                }
                LOG(LOG_LEVEL, "\n");
            }
            
            // third, convert the FSK tones into an audio signal
            synth_gfsk(tones, num_tones, frequency, symbol_bt, symbol_rate, sample_rate, signal);
            
            // clear superfluous samples
            memset(signal + signal_samples, 0, (num_samples - signal_samples) * sizeof(float));
            return 0;
        }
        
        LOG(LOG_ERROR, "Cannot parse message '%s', rc = %d!\n", message, rc);
        return -2;
    }
    
    LOG(LOG_ERROR, "Not enough space for samples, please provide at least space for %d (provided: %d)\n", signal_samples, num_samples);
    return -1;
}

// generate FT4 signal for message
int ft4_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate)
{
    return ftX_encode(message, signal, num_samples, frequency, sample_rate, true);
}

// generate FT8 signal for message
int ft8_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate)
{
    return ftX_encode(message, signal, num_samples, frequency, sample_rate, false);
}
