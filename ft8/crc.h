#ifndef _INCLUDE_CRC_H_
#define _INCLUDE_CRC_H_

#include <stdint.h>
#include <stdbool.h>

// Compute 14-bit CRC for a sequence of given number of bits
// [IN] message  - byte sequence (MSB first)
// [IN] num_bits - number of bits in the sequence
uint16_t ft8_crc(const uint8_t message[], int num_bits);

/// Check the FT8 CRC of a packed message (during decoding)
uint16_t extract_crc(uint8_t a91[]);

/// Add the FT8 CRC to a packed message (during encoding)
void add_crc(uint8_t a91[]);

#endif // _INCLUDE_CRC_H_