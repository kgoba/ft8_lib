#include <string.h>
#include <cstdio>

#include "pack.h"


constexpr int32_t NBASE = 37*36*10*27*27*27L;
constexpr int32_t NGBASE = 180*180L;


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


// Returns integer encoding of a character (number/digit/space).
// Alphabet: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ +-./?"
//   - Digits are encoded as 0..9
//   - Letters a..z are encoded as 10..35 (case insensitive)
//   - Space is encoded as 36
uint8_t nchar(char c) {
    if (is_digit(c))
        return (c - '0');
    if (is_letter(c))
        return (to_upper(c) - 'A') + 10;

    switch (c) {
        case ' ': return 36;
        case '+': return 37;
        case '-': return 38;
        case '.': return 39;
        case '/': return 40;
        case '?': return 41;

        default:  return 36;    // Equal to ' '
    }
}


// Pack FT8 source/destination and grid data into 72 bits (stored as 9 bytes)
// [IN] nc1      - first callsign data (28 bits)
// [IN] nc2      - second callsign data (28 bits)
// [IN] ng       - grid data (16 bits)
// [OUT] payload - 9 byte array to store the 72 bit payload (MSB first)
void pack3_8bit(uint32_t nc1, uint32_t nc2, uint16_t ng, uint8_t *payload) {
    payload[0] = (uint8_t)(nc1 >> 20);
    payload[1] = (uint8_t)(nc1 >> 12);
    payload[2] = (uint8_t)(nc1 >> 4);
    payload[3] = (uint8_t)(nc1 << 4) | (uint8_t)(nc2 >> 24);
    payload[4] = (uint8_t)(nc2 >> 16);
    payload[5] = (uint8_t)(nc2 >> 8);
    payload[6] = (uint8_t)(nc2);
    payload[7] = (uint8_t)(ng >> 8);
    payload[8] = (uint8_t)(ng);
}


// Pack FT8 source/destination and grid data into 72 bits (stored as 12 bytes of 6-bit values)
// (Unused here, included for compatibility with WSJT-X and testing)
// [IN] nc1      - first callsign data (28 bits)
// [IN] nc2      - second callsign data (28 bits)
// [IN] ng       - grid data (16 bits)
// [OUT] payload - 12 byte array to store the 72 bit payload (MSB first)
void pack3_6bit(uint32_t nc1, uint32_t nc2, uint16_t ng, uint8_t *payload) {
    payload[0] = (nc1 >> 22) & 0x3f;    // 6 bits
    payload[1] = (nc1 >> 16) & 0x3f;    // 6 bits
    payload[2] = (nc1 >> 10) & 0x3f;    // 6 bits
    payload[3] = (nc1 >> 4) & 0x3f;     // 6 bits
    payload[4] = ((nc1 & 0xf) << 2) | ((nc2 >> 26) & 0x3);  // 4+2 bits
    payload[5] = (nc2 >> 20) & 0x3f;    // 6 bits
    payload[6] = (nc2 >> 14) & 0x3f;    // 6 bits
    payload[7] = (nc2 >> 8) & 0x3f;     // 6 bits
    payload[8] = (nc2 >> 2) & 0x3f;     // 6 bits
    payload[9] = ((nc2 & 0x3) << 4) | ((ng >> 12) & 0xf);    // 2+4 bits
    payload[10] = (ng >> 6) & 0x3f;     // 6 bits
    payload[11] = (ng >> 0) & 0x3f;     // 6 bits    
}


// Parse a 2 digit integer from string
int dd_to_int(const char *str, int length) {
    int result = 0;
    bool negative;
    int i;
    if (str[0] == '-') {
        negative = true;
        i = 1;                          // Consume the - sign
    }
    else {
        negative = false;
        i = (str[0] == '+') ? 1 : 0;    // Consume a + sign if found
    }
    
    while (i < length) {
        if (str[i] == 0) break;
        if (!is_digit(str[i])) break;
        result *= 10;
        result += (str[i] - '0');
        ++i;
    }

    return negative ? -result : result;
}


// Convert a 2 digit integer to string
void int_to_dd(char *str, int value, int width) {
    if (value < 0) {
        *str = '-';
        ++str;
        value = -value;
    }

    int divisor = 1;
    for (int i = 0; i < width; ++i) {
        divisor *= 10;
    }

    while (divisor > 1) {
        int digit = value / divisor;

        *str = '0' + digit;
        ++str;

        value -= digit * divisor;
        divisor /= 10;
    }
    *str = 0;   // Add zero terminator
}


// Pack a valid callsign into a 28-bit integer.
int32_t packcall(const char *callsign) {
    //LOG("Callsign = [%s]\n", callsign);
    if (equals(callsign, "CQ")) { 
        // TODO: support 'CQ nnn' frequency specification
        //if (callsign(4:4).ge.'0' .and. callsign(4:4).le.'9' .and.        &
        //    callsign(5:5).ge.'0' .and. callsign(5:5).le.'9' .and.      &
        //    callsign(6:6).ge.'0' .and. callsign(6:6).le.'9') then
        //      read(callsign(4:6),*) nfreq
        //      ncall=NBASE + 3 + nfreq
        //endif
        return NBASE + 1;
    }
    if (equals(callsign, "QRZ")) { 
        return NBASE + 2;
    }
    if (equals(callsign, "DE")) {
        return 267796945;
    }
    
    int len = strlen(callsign);
    if (len > 6) {
        return -1;
    }
    
    char callsign2[7] = {' ', ' ', ' ', ' ', ' ', ' ', 0};    // 6 spaces with zero terminator

    // Work-around for Swaziland prefix (see WSJT-X code):
    if (starts_with(callsign, "3DA0")) {
        // callsign='3D0'//callsign(5:6)
        memcpy(callsign2, "3D0", 3);
        memcpy(callsign2 + 3, callsign + 4, 2);
    }
    // Work-around for Guinea prefixes (see WSJT-X code):
    else if (starts_with(callsign, "3X") && is_letter(callsign[2])) {
        //callsign='Q'//callsign(3:6)
        memcpy(callsign2, "Q", 1);
        memcpy(callsign2 + 1, callsign + 2, 4);
    }
    else {
        // Just copy, no modifications needed
        // Check for callsigns with 1 symbol prefix
        if (!is_digit(callsign[2]) && is_digit(callsign[1])) {
            if (len > 5) {
                return -1;
            }
            // Leave one space at the beginning as padding
            memcpy(callsign2 + 1, callsign, len);
        }
        else {
            memcpy(callsign2, callsign, len);
        }
    }
 
    // Check if the callsign consists of valid characters
    if (!is_digit(callsign2[0]) && !is_letter(callsign2[0]) && !is_space(callsign2[0])) 
        return -3;
    if (!is_digit(callsign2[1]) && !is_letter(callsign2[1])) 
        return -3;
    if (!is_digit(callsign2[2]))
        return -3;
    if (!is_letter(callsign2[3]) && !is_space(callsign2[3]))
        return -3;
    if (!is_letter(callsign2[4]) && !is_space(callsign2[4]))
        return -3;
    if (!is_letter(callsign2[5]) && !is_space(callsign2[5]))
        return -3;

    // Form a 28 bit integer from callsign parts
    int32_t ncall = nchar(callsign2[0]);
    ncall = 36*ncall + nchar(callsign2[1]);
    ncall = 10*ncall + nchar(callsign2[2]);
    ncall = 27*ncall + nchar(callsign2[3]) - 10;
    ncall = 27*ncall + nchar(callsign2[4]) - 10;
    ncall = 27*ncall + nchar(callsign2[5]) - 10;
    
    return ncall;
}


// Pack a valid grid locator into an integer.
int16_t packgrid(const char *grid) {
    //LOG("Grid = [%s]\n", grid);
    int len = strlen(grid);

    if (len == 0) {
        // Blank grid is OK
        return NGBASE + 1;
    }
    
    // Check for RO, RRR, or 73 in the message field normally used for grid
    if (equals(grid, "RO")) {
        return NGBASE + 62;
    }
    if (equals(grid, "RRR")) {
        return NGBASE + 63;
    }
    if (equals(grid, "73")) {
        return NGBASE + 64;
    }
      
    // Attempt to parse signal reports (e.g. "-07", "R+20")
    char c1 = grid[0];
    int n;
    if (c1 == 'R') {
        n = dd_to_int(grid + 1, 3); // read(grid(2:4),*,err=30,end=30) n        
    }
    else {
        n = dd_to_int(grid, 3);  // read(grid,*,err=20,end=20) n
    }

    // First, handle signal reports in the original range, -01 to -30 dB
    if (n >= -30 && n <= -1) {
        if (c1 == 'R') {
            return NGBASE + 31 + (-n);
        }
        else {
            return NGBASE + 1 + (-n);
        }
    }

    char grid4[4];
    memcpy(grid4, grid, 4);

    // TODO: Check for extended-range signal reports: -50 to -31, and 0 to +49
    // if (n >= -50 && n <= 49) {
    //     if (c1 == 'R') {
    //          // write(grid,1002) n+50   1002    format('LA',i2.2)
    //     }
    //     else {
    //          // write(grid,1003) n+50   1003    format('KA',i2.2)
    //     }
    //     // go to 40
    // }
    // else {
    //     // error
    //     return -1;
    // }

    // Check if the grid locator is properly formatted 
    if (len != 4) return -1;
    if (grid4[0] < 'A' || grid4[0] > 'R') return -1;
    if (grid4[1] < 'A' || grid4[1] > 'R') return -1;
    if (grid4[2] < '0' || grid4[2] > '9') return -1;
    if (grid4[3] < '0' || grid4[3] > '9') return -1;

    // Extract latitude and longitude
    int lng = (grid4[0] - 'A') * 20;
    lng += (grid4[2] - '0') * 2;
    lng = 179 - lng;

    int lat = (grid4[1] - 'A') * 10;
    lat += (grid4[3] - '0') * 1;
    lat -= 90;

    // Convert latitude and longitude into single number
    int16_t ng = (lng + 180) / 2;
    ng *= 180;
    ng += lat + 90;

    return ng;
}

// Pack a free-text message into 3 integers (28+28+15 bits)
void packtext(const char *msg, int32_t &nc1, int32_t &nc2, int16_t &ng) {
    int32_t nc3;
    nc1 = nc2 = ng = 0;

    // Pack 5 characters (42^5) into 27 bits
    for (int i = 0; i < 5; ++i) { // First 5 characters in nc1
        uint8_t j = nchar(msg[i]); // Get character code
        nc1 = 42*nc1 + j;
    }

    // Pack 5 characters (42^5) into 27 bits
    for (int i = 5; i < 10; ++i) { // Characters 6-10 in nc2
        uint8_t j = nchar(msg[i]); // Get character code
        nc2 = 42*nc2 + j;
    }

    // Pack 3 characters (42^3) into 17 bits
    for (int i = 10; i < 13; ++i) { // Characters 11-13 in nc3
        uint8_t j = nchar(msg[i]); // Get character code
        nc3 = 42*nc3 + j;
    }

    // We now have used 17 bits in nc3.  Must move one each to nc1 and nc2.
    nc1 <<= 1;
    if (nc3 & 0x08000) nc1 |= 1;
    nc2 <<= 1;
    if (nc3 & 0x10000) nc2 |= 1;
    ng = nc3 & 0x7FFF;
}


int packmsg(const char *msg, uint8_t *dat) {  // , itype, bcontest
    // TODO: check what is maximum allowed length?
    if (strlen(msg) > 22) {
        return -1;
    }
    
    char msg2[23];  // Including zero terminator!
    
    fmtmsg(msg2, msg);
    
    //LOG("msg2 = [%s]\n", msg2);

    // TODO: Change 'CQ n ' type messages to 'CQ 00n '
    //if(msg(1:3).eq.'CQ ' .and. msg(4:4).ge.'0' .and. msg(4:4).le.'9'   &
    //     .and. msg(5:5).eq.' ') msg='CQ 00'//msg(4:)

    if (starts_with(msg2, "CQ ")) {
        if (msg2[3] == 'D' && msg2[4] == 'X' && is_space(msg2[5])) {
            // Change 'CQ DX ' to 'CQ9DX '
            msg2[2] = '9';            
        }
        else if (is_letter(msg2[3]) && is_letter(msg2[4]) && is_space(msg2[5])) {
            // Change 'CQ xy ' type messages to 'E9xy '
            msg2[0] = 'E';
            msg2[1] = '9';
            // Delete the extra space
            char *ptr = msg2 + 2;
            while (*ptr) {
                ptr[0] = ptr[1];
                ++ptr;
            }
        }
    }
    
    int32_t nc1 = -1;
    int32_t nc2 = -1;
    int16_t ng = -1;

    // Try to split the message into three space-delimited fields
    //   by locating spaces and changing them to zero terminators

    // Locate the first delimiter in the message
    char *s1 = (char *)strchr(msg2, ' ');
    if (s1 != NULL) {
        *s1 = 0;    // Separate fields by zero terminator
        ++s1;       // s1 now points to the second field

        // Locate the second delimiter in the message
        char *s2 = (char *)strchr(s1 + 1, ' ');
        if (s2 == NULL) {
            // If the second space is not found, point to the end of string
            // to allow for blank grid (third field)
            s2 = msg2 + strlen(msg2);
        }
        else {
            *s2 = 0;// Separate fields by zero terminator
            ++s2;   // s2 now points to the third field
        }

        // TODO: process callsign prefixes/suffixes    

        // Pack message fields into integers
        nc1 = packcall(msg2);
        nc2 = packcall(s1);
        ng  = packgrid(s2);
    }

    // Check for success in all three fields
    if (nc1 < 0 || nc2 < 0 || ng < 0) {
        // Treat as plain text message
        packtext(msg2, nc1, nc2, ng);
        ng += 0x8000;   // Set bit 15 (we abuse signed int here)
    }

    //LOG("nc1 = %d [%04X], nc2 = %d [%04X], ng = %d\n", nc1, nc1, nc2, nc2, ng);

    // Originally the data was packed in bytes of 6 bits. 
    // This seems to waste memory unnecessary and complicate the code, so we pack it in 8 bit values.
    pack3_8bit((uint32_t)nc1, (uint32_t)nc2, (uint16_t)ng, dat);
    //pack3_6bit(nc1, nc2, ng, dat);

    return 0;   // Success!
}
