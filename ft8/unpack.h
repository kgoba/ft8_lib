#ifndef _INCLUDE_UNPACK_H_
#define _INCLUDE_UNPACK_H_

#include <stdint.h>

// field1 - at least 14 bytes
// field2 - at least 14 bytes
// field3 - at least 7 bytes
int unpack77_fields(const uint8_t *a77, char *field1, char *field2, char *field3);

// message should have at least 35 bytes allocated (34 characters + zero terminator)
int unpack77(const uint8_t *a77, char *message);

#endif // _INCLUDE_UNPACK_H_
