#include "text.h"

#include <string.h>

// Utility functions for characters and strings

char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
}

bool is_digit(char c) {
    return (c >= '0') && (c <= '9');
}

bool is_letter(char c) {
    return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}

bool is_space(char c) {
    return (c == ' ');
}

bool in_range(char c, char min, char max) {
    return (c >= min) && (c <= max);
}

bool starts_with(const char *string, const char *prefix) {
    return 0 == memcmp(string, prefix, strlen(prefix));
}

bool equals(const char *string1, const char *string2) {
    return 0 == strcmp(string1, string2);
}


// Text message formatting: 
//   - replaces lowercase letters with uppercase
//   - merges consecutive spaces into single space
void fmtmsg(char *msg_out, const char *msg_in) {
    char c;
    char last_out = 0;
    while ( (c = *msg_in) ) {
        if (c != ' ' || last_out != ' ') {
            last_out = to_upper(c);
            *msg_out = last_out;
            ++msg_out;
        }
        ++msg_in;
    }
    *msg_out = 0; // Add zero termination
}
