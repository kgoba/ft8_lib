#ifndef _INCLUDE_PACK_H_
#define _INCLUDE_PACK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Parse and pack FT8/FT4 text message into 77 bit binary payload
/// @param[in] msg   FT8/FT4 message (e.g. "CQ TE5T KN01")
/// @param[out] c77  10 byte array to store the 77 bit payload (MSB first)
/// @return Parsing result (0 - success, otherwise error)
int pack77(const char* msg, uint8_t* c77);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_PACK_H_
