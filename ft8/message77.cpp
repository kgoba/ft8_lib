#include "message77.h"
#include "unpack.h"

#include <string.h>

namespace ft8 {

Message77::Message77() {
    i3 = n3 = 0;
    field1[0] = field2[0] = field3[0] = '\0';
}

int Message77::str(char *buf, int buf_sz) const {
    // Calculate the available space sans the '\0' terminator
    int rem_sz = buf_sz - 1;
    int field1_sz = strlen(field1);
    int field2_sz = strlen(field2);
    int field3_sz = strlen(field3);
    int msg_sz = field1_sz + (field2_sz > 0 ? 1 : 0) + 
                 field2_sz + (field3_sz > 0 ? 1 : 0) + 
                 field3_sz;

    if (rem_sz < msg_sz) return -1;

    char *dst = buf;

    dst = stpcpy(dst, field1);
    *dst++ = ' ';
    dst = stpcpy(dst, field2);
    *dst++ = ' ';
    dst = stpcpy(dst, field3);
    *dst = '\0';

    return msg_sz;
}

int Message77::unpack(const uint8_t *a77) {
    // Extract n3 (bits 71..73) and i3 (bits 74..76)
    n3 = ((a77[8] << 2) & 0x04) | ((a77[9] >> 6) & 0x03);
    i3 = (a77[9] >> 3) & 0x07;

    int rc = unpack77_fields(a77, field1, field2, field3);
    if (rc < 0) {
        field1[0] = field2[0] = field3[0] = '\0';
    }
    return rc;
}

}   // namespace ft8
