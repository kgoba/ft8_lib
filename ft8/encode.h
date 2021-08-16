#ifndef _INCLUDE_ENCODE_H_
#define _INCLUDE_ENCODE_H_

#include <stdint.h>

/// Generate FT8 tone sequence from payload data
/// @param[in] payload - 10 byte array consisting of 77 bit payload
/// @param[out] itone  - array of NN (79) bytes to store the generated tones (encoded as 0..7)
void genft8(const uint8_t *payload, uint8_t *itone);

#endif // _INCLUDE_ENCODE_H_
