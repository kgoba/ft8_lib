#ifndef _INCLUDE_TEXT_H_
#define _INCLUDE_TEXT_H_

#include <stdbool.h>
#include <stdint.h>

// Utility functions for characters and strings

const char *ft8_trim_front(const char *str);
void ft8_trim_back(char *str);
char *ft8_trim(char *str);

char ft8_to_upper(char c);
bool ft8_is_digit(char c);
bool ft8_is_letter(char c);
bool ft8_is_space(char c);
bool ft8_in_range(char c, char min, char max);
bool ft8_starts_with(const char *string, const char *prefix);
bool ft8_equals(const char *string1, const char *string2);

int ft8_char_index(const char *string, char c);

// Text message formatting:
//   - replaces lowercase letters with uppercase
//   - merges consecutive spaces into single space
void ft8_fmtmsg(char *msg_out, const char *msg_in);

// Parse a 2 digit integer from string
int ft8_dd_to_int(const char *str, int length);

// Convert a 2 digit integer to string
void ft8_int_to_dd(char *str, int value, int width, bool full_sign);

char ft8_charn(int c, int table_idx);
int ft8_nchar(char c, int table_idx);

#endif // _INCLUDE_TEXT_H_
