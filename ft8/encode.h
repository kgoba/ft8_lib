#pragma once

#include <stdint.h>

namespace ft8 {

    // Generate FT8 tone sequence from payload data
    // [IN] payload - 9 byte array consisting of 72 bit payload
    // [OUT] itone  - array of NN (79) bytes to store the generated tones (encoded as 0..7)
    void genft8(const uint8_t *payload, uint8_t *itone);


    // Encode an 87-bit message and return a 174-bit codeword. 
    // The generator matrix has dimensions (87,87). 
    // The code is a (174,87) regular ldpc code with column weight 3.
    // The code was generated using the PEG algorithm.
    // After creating the codeword, the columns are re-ordered according to 
    // "colorder" to make the codeword compatible with the parity-check matrix 
    // Arguments:
    //   * message - array of 87 bits stored as 11 bytes (MSB first)
    //   * codeword - array of 174 bits stored as 22 bytes (MSB first)
    void encode174(const uint8_t *message, uint8_t *codeword);


    // Compute 14-bit CRC for a sequence of given number of bits
    // [IN] message  - byte sequence (MSB first)
    // [IN] num_bits - number of bits in the sequence
    uint16_t crc(uint8_t *message, int num_bits);
};
