#ifndef _INCLUDE_UNPACK_H_
#define _INCLUDE_UNPACK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    /// Called when a callsign is looked up by its 22 bit hash code
    void (*hash22)(uint32_t n22, char* callsign);
    /// Called when a callsign is looked up by its 12 bit hash code
    void (*hash12)(uint32_t n12, char* callsign);
    /// Called when a callsign should hashed and stored (both by its 22 and 12 bit hash code)
    void (*save_hash)(const char* callsign);
} unpack_hash_interface_t;

/// Unpack a 77 bit message payload into three fields (typically call_to, call_de and grid/report/other)
/// @param[in] a77 message payload in binary form (77 bits, MSB first)
/// @param[out] field1 at least 14 bytes (typically call_to)
/// @param[out] field2 at least 14 bytes (typically call_de)
/// @param[out] field3 at least 7 bytes (typically grid/report/other)
/// @param[in] hash_if hashing interface (can be NULL)
int unpack77_fields(const uint8_t* a77, char* field1, char* field2, char* field3, const unpack_hash_interface_t* hash_if);

/// Unpack a 77 bit message payload into text message
/// @param[in] a77 message payload in binary form (77 bits, MSB first)
/// @param[out] message should have at least 35 bytes allocated (34 characters + zero terminator)
/// @param[in] hash_if hashing interface (can be NULL)
int unpack77(const uint8_t* a77, char* message, const unpack_hash_interface_t* hash_if);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_UNPACK_H_
