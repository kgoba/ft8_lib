#include "encode.h"
#include "constants.h"
#include "crc.h"

#include <stdio.h>

// Returns 1 if an odd number of bits are set in x, zero otherwise
uint8_t parity8(uint8_t x)
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
void encode174(const uint8_t *message, uint8_t *codeword)
{
    // This implementation accesses the generator bits straight from the packed binary representation in kFT8_LDPC_generator

    // Fill the codeword with message and zeros, as we will only update binary ones later
    for (int j = 0; j < FT8_N_BYTES; ++j)
    {
        codeword[j] = (j < FT8_K_BYTES) ? message[j] : 0;
    }

    // Compute the byte index and bit mask for the first checksum bit
    uint8_t col_mask = (0x80u >> (FT8_K % 8u)); // bitmask of current byte
    uint8_t col_idx = FT8_K_BYTES - 1;          // index into byte array

    // Compute the LDPC checksum bits and store them in codeword
    for (int i = 0; i < FT8_M; ++i)
    {
        // Fast implementation of bitwise multiplication and parity checking
        // Normally nsum would contain the result of dot product between message and kFT8_LDPC_generator[i],
        // but we only compute the sum modulo 2.
        uint8_t nsum = 0;
        for (int j = 0; j < FT8_K_BYTES; ++j)
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

// Generate FT8 tone sequence from payload data
// [IN] payload - 10 byte array consisting of 77 bit payload (MSB first)
// [OUT] itone  - array of NN (79) bytes to store the generated tones (encoded as 0..7)
void genft8(const uint8_t *payload, uint8_t *itone)
{
    uint8_t a91[12]; // Store 77 bits of payload + 14 bits CRC

    // Copy 77 bits of payload data
    for (int i = 0; i < 10; i++)
        a91[i] = payload[i];

    // Clear 3 bits after the payload to make 80 bits
    a91[9] &= 0xF8u;
    a91[10] = 0;
    a91[11] = 0;

    // Compute and add CRC at the end of the message
    // a91 contains 77 bits of payload + 14 bits of CRC
    add_crc(a91);

    uint8_t codeword[22];
    encode174(a91, codeword);

    // Message structure: S7 D29 S7 D29 S7
    for (int i = 0; i < 7; ++i)
    {
        itone[i] = kFT8_Costas_pattern[i];
        itone[36 + i] = kFT8_Costas_pattern[i];
        itone[72 + i] = kFT8_Costas_pattern[i];
    }

    int k = 7; // Skip over the first set of Costas symbols

    uint8_t mask = 0x80u;
    int i_byte = 0;
    for (int j = 0; j < FT8_ND; ++j)
    {
        if (j == 29)
        {
            k += 7; // Skip over the second set of Costas symbols
        }

        // Extract 3 bits from codeword at i-th position
        uint8_t bits3 = 0;

        if (codeword[i_byte] & mask)
            bits3 |= 4;
        if (0 == (mask >>= 1))
        {
            mask = 0x80;
            i_byte++;
        }
        if (codeword[i_byte] & mask)
            bits3 |= 2;
        if (0 == (mask >>= 1))
        {
            mask = 0x80;
            i_byte++;
        }
        if (codeword[i_byte] & mask)
            bits3 |= 1;
        if (0 == (mask >>= 1))
        {
            mask = 0x80;
            i_byte++;
        }

        itone[k] = kFT8_Gray_map[bits3];
        ++k;
    }
}
