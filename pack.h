#pragma once

#include <stdint.h>


// Pack FT8 text message into 72 bits
// [IN] msg      - FT8 message (e.g. "CQ TE5T KN01")
// [OUT] packed  - 9 byte array to store the 72 bit payload (MSB first)
int packmsg(const char *msg, uint8_t *dat);
