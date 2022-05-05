#ifndef _INCLUDE_UNPACK_H_
#define _INCLUDE_UNPACK_H_

#include <stdint.h>

// message should have at least 35 bytes allocated (34 characters + zero terminator)
int unpack77(const uint8_t *a77, char *message);

#endif // _INCLUDE_UNPACK_H_
