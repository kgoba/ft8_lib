#include "constants.h"
#include "crc.h"
#include "ft8.h"
#include "pack.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FT8_SYMBOL_BT 2.0f ///< symbol smoothing filter bandwidth factor (BT)
#define FT4_SYMBOL_BT 1.0f ///< symbol smoothing filter bandwidth factor (BT)

#define GFSK_CONST_K 5.336446f ///< == pi * sqrt(2 / log(2))

/// Computes a GFSK smoothing pulse.
/// The pulse is theoretically infinitely long, however, here it's truncated at 3 times the symbol length.
/// This means the pulse array has to have space for 3*n_spsym elements.
/// @param[in] n_spsym Number of samples per symbol
/// @param[in] symbol_bt Shape parameter (values defined for FT8/FT4)
/// @param[out] pulse Output array of pulse samples
///
void gfsk_pulse(int n_spsym, float symbol_bt, float *pulse)
{
    for (int i = 0; i < 3 * n_spsym; ++i) {
        float t = i / (float)n_spsym - 1.5f;
        float arg1 = GFSK_CONST_K * symbol_bt * (t + 0.5f);
        float arg2 = GFSK_CONST_K * symbol_bt * (t - 0.5f);
        pulse[i] = (erff(arg1) - erff(arg2)) / 2;
    }
}

/// Synthesize waveform data using GFSK phase shaping.
/// The output waveform will contain n_sym symbols.
/// @param[in] symbols Array of symbols (tones) (0-7 for FT8)
/// @param[in] n_sym Number of symbols in the symbol array
/// @param[in] f0 Audio frequency in Hertz for the symbol 0 (base frequency)
/// @param[in] symbol_bt Symbol smoothing filter bandwidth (2 for FT8, 1 for FT4)
/// @param[in] symbol_period Symbol period (duration), seconds
/// @param[in] signal_rate Sample rate of synthesized signal, Hertz
/// @param[out] signal Output array of signal waveform samples (should have space for n_sym*n_spsym samples)
///
void synth_gfsk(const uint8_t *symbols, int n_sym, float f0, float symbol_bt, float symbol_period, int signal_rate, float *signal)
{
    int n_spsym = (int)(0.5f + signal_rate * symbol_period); // Samples per symbol
    int n_wave = n_sym * n_spsym; // Number of output samples
    float hmod = 1.0f;

    // Compute the smoothed frequency waveform.
    // Length = (nsym+2)*n_spsym samples, first and last symbols extended
    float *dphi = malloc((n_wave + 2 * n_spsym) * sizeof(float));
    if (dphi != NULL) {
        float dphi_peak = 2 * M_PI * hmod / n_spsym;

        // Shift frequency up by f0
        for (int i = 0; i < n_wave + 2 * n_spsym; ++i) {
            dphi[i] = fmodf(2 * M_PI * f0 / signal_rate, 2.0 * M_PI);
        }

        float pulse[3 * n_spsym];
        gfsk_pulse(n_spsym, symbol_bt, pulse);

        for (int i = 0; i < n_sym; ++i) {
            int ib = i * n_spsym;
            for (int j = 0; j < 3 * n_spsym; ++j) {
                dphi[j + ib] += dphi_peak * symbols[i] * pulse[j];
            }
        }

        // Add dummy symbols at beginning and end with tone values equal to 1st and last symbol, respectively
        for (int j = 0; j < 2 * n_spsym; ++j) {
            dphi[j] += dphi_peak * pulse[j + n_spsym] * symbols[0];
            dphi[j + n_sym * n_spsym] += dphi_peak * pulse[j] * symbols[n_sym - 1];
        }

        // Calculate and insert the audio waveform
        float phi = 0;
        for (int k = 0; k < n_wave; ++k) { // Don't include dummy symbols
            signal[k] = sinf(phi);
            phi = fmodf(phi + dphi[k + n_spsym], 2 * M_PI);
        }

        // Apply envelope shaping to the first and last symbols
        int n_ramp = n_spsym / 8;
        for (int i = 0; i < n_ramp; ++i) {
            float env = (1 - cosf(2 * M_PI * i / (2 * n_ramp))) / 2;
            signal[i] *= env;
            signal[n_wave - 1 - i] *= env;
        }

        free(dphi);
    }
}
// Returns 1 if an odd number of bits are set in x, zero otherwise
static uint8_t parity8(uint8_t x)
{
    x ^= x >> 4; // a b c d ae bf cg dh
    x ^= x >> 2; // a b ac bd cae dbf aecg bfdh
    x ^= x >> 1; // a ab bac acbd bdcae caedbf aecgbfdh
    return x % 2; // modulo 2
}

// Encode via LDPC a 91-bit message and return a 174-bit codeword.
// The generator matrix has dimensions (87,87).
// The code is a (174,91) regular LDPC code with column weight 3.
// Arguments:
// [IN] message   - array of 91 bits stored as 12 bytes (MSB first)
// [OUT] codeword - array of 174 bits stored as 22 bytes (MSB first)
static void encode174(const uint8_t *message, uint8_t *codeword)
{
    // This implementation accesses the generator bits straight from the packed binary representation in kFTX_LDPC_generator

    // Fill the codeword with message and zeros, as we will only update binary ones later
    for (int j = 0; j < FTX_LDPC_N_BYTES; ++j) {
        codeword[j] = (j < FTX_LDPC_K_BYTES) ? message[j] : 0;
    }

    // Compute the byte index and bit mask for the first checksum bit
    uint8_t col_mask = (0x80u >> (FTX_LDPC_K % 8u)); // bitmask of current byte
    uint8_t col_idx = FTX_LDPC_K_BYTES - 1; // index into byte array

    // Compute the LDPC checksum bits and store them in codeword
    for (int i = 0; i < FTX_LDPC_M; ++i) {
        // Fast implementation of bitwise multiplication and parity checking
        // Normally nsum would contain the result of dot product between message and kFTX_LDPC_generator[i],
        // but we only compute the sum modulo 2.
        uint8_t nsum = 0;
        for (int j = 0; j < FTX_LDPC_K_BYTES; ++j) {
            uint8_t bits = message[j] & kFTX_LDPC_generator[i][j]; // bitwise AND (bitwise multiplication)
            nsum ^= parity8(bits); // bitwise XOR (addition modulo 2)
        }

        // Set the current checksum bit in codeword if nsum is odd
        if (nsum % 2) {
            codeword[col_idx] |= col_mask;
        }

        // Update the byte index and bit mask for the next checksum bit
        col_mask >>= 1;
        if (col_mask == 0) {
            col_mask = 0x80u;
            ++col_idx;
        }
    }
}

void ft8_encode(const uint8_t *payload, uint8_t *tones)
{
    uint8_t a91[FTX_LDPC_K_BYTES]; // Store 77 bits of payload + 14 bits CRC

    // Compute and add CRC at the end of the message
    // a91 contains 77 bits of payload + 14 bits of CRC
    ftx_add_crc(payload, a91);

    uint8_t codeword[FTX_LDPC_N_BYTES];
    encode174(a91, codeword);

    // Message structure: S7 D29 S7 D29 S7
    // Total symbols: 79 (FT8_NN)

    uint8_t mask = 0x80u; // Mask to extract 1 bit from codeword
    int i_byte = 0; // Index of the current byte of the codeword
    for (int i_tone = 0; i_tone < FT8_NN; ++i_tone) {
        if ((i_tone >= 0) && (i_tone < 7)) {
            tones[i_tone] = kFT8_Costas_pattern[i_tone];
        } else if ((i_tone >= 36) && (i_tone < 43)) {
            tones[i_tone] = kFT8_Costas_pattern[i_tone - 36];
        } else if ((i_tone >= 72) && (i_tone < 79)) {
            tones[i_tone] = kFT8_Costas_pattern[i_tone - 72];
        } else {
            // Extract 3 bits from codeword at i-th position
            uint8_t bits3 = 0;

            if (codeword[i_byte] & mask)
                bits3 |= 4;
            if (0 == (mask >>= 1)) {
                mask = 0x80u;
                i_byte++;
            }
            if (codeword[i_byte] & mask)
                bits3 |= 2;
            if (0 == (mask >>= 1)) {
                mask = 0x80u;
                i_byte++;
            }
            if (codeword[i_byte] & mask)
                bits3 |= 1;
            if (0 == (mask >>= 1)) {
                mask = 0x80u;
                i_byte++;
            }

            tones[i_tone] = kFT8_Gray_map[bits3];
        }
    }
}

void ft4_encode(const uint8_t *payload, uint8_t *tones)
{
    uint8_t a91[FTX_LDPC_K_BYTES]; // Store 77 bits of payload + 14 bits CRC
    uint8_t payload_xor[10]; // Encoded payload data

    // '[..] for FT4 only, in order to avoid transmitting a long string of zeros when sending CQ messages,
    // the assembled 77-bit message is bitwise exclusive-OR’ed with [a] pseudorandom sequence before computing the CRC and FEC parity bits'
    for (int i = 0; i < 10; ++i) {
        payload_xor[i] = payload[i] ^ kFT4_XOR_sequence[i];
    }

    // Compute and add CRC at the end of the message
    // a91 contains 77 bits of payload + 14 bits of CRC
    ftx_add_crc(payload_xor, a91);

    uint8_t codeword[FTX_LDPC_N_BYTES];
    encode174(a91, codeword); // 91 bits -> 174 bits

    // Message structure: R S4_1 D29 S4_2 D29 S4_3 D29 S4_4 R
    // Total symbols: 105 (FT4_NN)

    uint8_t mask = 0x80u; // Mask to extract 1 bit from codeword
    int i_byte = 0; // Index of the current byte of the codeword
    for (int i_tone = 0; i_tone < FT4_NN; ++i_tone) {
        if ((i_tone == 0) || (i_tone == 104)) {
            tones[i_tone] = 0; // R (ramp) symbol
        } else if ((i_tone >= 1) && (i_tone < 5)) {
            tones[i_tone] = kFT4_Costas_pattern[0][i_tone - 1];
        } else if ((i_tone >= 34) && (i_tone < 38)) {
            tones[i_tone] = kFT4_Costas_pattern[1][i_tone - 34];
        } else if ((i_tone >= 67) && (i_tone < 71)) {
            tones[i_tone] = kFT4_Costas_pattern[2][i_tone - 67];
        } else if ((i_tone >= 100) && (i_tone < 104)) {
            tones[i_tone] = kFT4_Costas_pattern[3][i_tone - 100];
        } else {
            // Extract 2 bits from codeword at i-th position
            uint8_t bits2 = 0;

            if (codeword[i_byte] & mask)
                bits2 |= 2;
            if (0 == (mask >>= 1)) {
                mask = 0x80u;
                i_byte++;
            }
            if (codeword[i_byte] & mask)
                bits2 |= 1;
            if (0 == (mask >>= 1)) {
                mask = 0x80u;
                i_byte++;
            }
            tones[i_tone] = kFT4_Gray_map[bits2];
        }
    }
}

// generate FT4 or FT8 signal for message
int ftx_encode(char *message, float *signal, int num_samples, float frequency, int sample_rate, ftx_protocol_t protocol)
{
    // First, pack the text data into binary message
    uint8_t packed[FTX_LDPC_K_BYTES];
    int rc = pack77(message, packed);

    if (rc >= 0) {
        // Second, encode the binary message as a sequence of FSK tones
        const int num_tones = protocol == PROTO_FT4 ? FT4_NN : FT8_NN;
        const float symbol_period = protocol == PROTO_FT4 ? FT4_SYMBOL_PERIOD : FT8_SYMBOL_PERIOD;
        const float symbol_bt = protocol == PROTO_FT4 ? FT4_SYMBOL_BT : FT8_SYMBOL_BT;
        int real_num_samples = num_tones * symbol_period * sample_rate + 0.5;
        uint8_t tones[num_tones];

        // check if we have enough space
        if (num_samples >= real_num_samples) {

            if (protocol == PROTO_FT4) {
                ft4_encode(packed, tones);
            } else {
                ft8_encode(packed, tones);
            }

            // Third, convert the FSK tones into an audio signal
            // Synthesize waveform data (signal) and save it as WAV file
            synth_gfsk(tones, num_tones, frequency, symbol_bt, symbol_period, sample_rate, signal);

            // clear extra samples
            memset(signal + real_num_samples, 0, (num_samples - real_num_samples) * sizeof(float));

            return 0;
        }
    }
    return -1;
}

