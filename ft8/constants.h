#pragma once

#include <stdint.h>

namespace ft8 {

    // Define FT8 symbol counts
    constexpr int ND = 58;      // Data symbols
    constexpr int NS = 21;      // Sync symbols (3 @ Costas 7x7)
    constexpr int NN = NS + ND;   // Total channel symbols (79)

    // Define the LDPC sizes
    constexpr int N = 174;      // Number of bits in the encoded message
    constexpr int K = 91;       // Number of payload bits
    constexpr int M = N - K;    // Number of checksum bits
    constexpr int K_BYTES = (K + 7) / 8;    // Number of whole bytes needed to store K bits

    // Define CRC parameters
    constexpr uint16_t CRC_POLYNOMIAL = 0x2757;  // CRC-14 polynomial without the leading (MSB) 1
    constexpr int      CRC_WIDTH = 14;

    // Costas 7x7 tone pattern
    extern const uint8_t kCostas_map[7];


    // Gray code map
    extern const uint8_t kGray_map[8];


    // Parity generator matrix for (174,91) LDPC code, stored in bitpacked format (MSB first)
    extern const uint8_t kGenerator[M][K_BYTES];


    // Column order (permutation) in which the bits in codeword are stored
    // (Not really used in FT8 v2 - instead the Nm, Mn and generator matrices are already permuted)
    extern const uint8_t kColumn_order[N];


    // this is the LDPC(174,91) parity check matrix.
    // 83 rows.
    // each row describes one parity check.
    // each number is an index into the codeword (1-origin).
    // the codeword bits mentioned in each row must xor to zero.
    // From WSJT-X's ldpc_174_91_c_reordered_parity.f90.
    extern const uint8_t kNm[M][7];


    // Mn from WSJT-X's bpdecode174.f90.
    // each row corresponds to a codeword bit.
    // the numbers indicate which three parity
    // checks (rows in Nm) refer to the codeword bit.
    // 1-origin.
    extern const uint8_t kMn[N][3];


    // Number of rows (columns in C/C++) in the array Nm.
    extern const uint8_t kNrw[M];

}