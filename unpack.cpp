#include "unpack.h"
#include "text.h"

#include <string.h>

constexpr uint32_t NBASE = 37L*36L*10L*27L*27L*27L;


// convert packed character to ASCII character
// 0..9 a..z space +-./?
char charn(uint8_t c) {
    if (c >= 0 && c <= 9)
        return '0' + c;
    if (c >= 10 && c < 36)
        return 'A' + c - 10;

    if (c < 42)
        return " +-./?" [c - 36];

    return ' ';
}


// nc is a 28-bit integer, e.g. nc1 or nc2, containing all the
// call sign bits from a packed message.
void unpackcall(uint32_t nc, char *callsign) {
    callsign[5] = charn((nc % 27) + 10); // + 10 b/c only alpha+space
    nc /= 27;
    callsign[4] = charn((nc % 27) + 10);
    nc /= 27;
    callsign[3] = charn((nc % 27) + 10);
    nc /= 27;
    callsign[2] = charn(nc % 10); // digit only
    nc /= 10;
    callsign[1] = charn(nc % 36); // letter or digit
    nc /= 36;
    callsign[0] = charn(nc);

    callsign[6] = 0;
}


// extract maidenhead locator
void unpackgrid(uint32_t ng, char *grid) {
    // // start of special grid locators for sig strength &c.
    // NGBASE = 180*180

    // if ng == NGBASE+1:
    //     return "    "
    // if ng >= NGBASE+1 and ng < NGBASE+31:
    //     return " -%02d" % (ng - (NGBASE+1)) // sig str, -01 to -30 DB
    // if ng >= NGBASE+31 and ng < NGBASE+62:
    //     return "R-%02d" % (ng - (NGBASE+31))
    // if ng == NGBASE+62:
    //     return "RO  "
    // if ng == NGBASE+63:
    //     return "RRR "
    // if ng == NGBASE+64:
    //     return "73  "

    // lat = (ng % 180) - 90
    // ng = int(ng / 180)
    // lng = (ng * 2) - 180

    // g = "%c%c%c%c" % (ord('A') + int((179-lng)/20),
    //                     ord('A') + int((lat+90)/10),
    //                     ord('0') + int(((179-lng)%20)/2),
    //                     ord('0') + (lat+90)%10)

    // if g[0:2] == "KA":
    //     // really + signal strength
    //     sig = int(g[2:4]) - 50
    //     return "+%02d" % (sig)

    // if g[0:2] == "LA":
    //     // really R+ signal strength
    //     sig = int(g[2:4]) - 50
    //     return "R+%02d" % (sig)    

    grid[0] = 0;
}


void unpacktext(uint32_t nc1, uint32_t nc2, uint16_t ng, char *text) {
    uint32_t nc3 = ng & 0x7FFF;

    if (nc1 & 1 != 0)
        nc3 |= 0x08000;
    nc1 >>= 1;

    if (nc2 & 1 != 0)
        nc3 |= 0x10000;
    nc2 >>= 1;

    for (int i = 4; i >= 0; --i) {
        text[i] = charn(nc1 % 42);
        nc1 /= 42;
    }

    for (int i = 9; i >= 5; --i) {
        text[i] = charn(nc2 % 42);
        nc2 /= 42;
    }

    for (int i = 12; i >= 10; --i) {
        text[i] = charn(nc3 % 42);
        nc3 /= 42;
    }

    text[13] = 0;
}


int unpack(const uint8_t *a72, char *message) {
    uint32_t nc1, nc2;
    uint16_t ng;

    nc1 = (a72[0] << 20);
    nc1 |= (a72[1] << 12);
    nc1 |= (a72[2] << 4);
    nc1 |= (a72[3] >> 4);
    
    nc2 = ((a72[3] & 0x0F) << 24);
    nc2 |= (a72[4] << 16);
    nc2 |= (a72[5] << 8);
    nc2 |= (a72[6]);

    ng = (a72[7] >> 8);
    ng |= (a72[8]);

    if (ng & 0x8000) {
        unpacktext(nc1, nc2, ng, message);
        return 0;
    }

    char c2[7];
    char grid[5];

    if (nc1 == NBASE+1) {
        // CQ with standard callsign
        unpackcall(nc2, c2);
        unpackgrid(ng, grid);
        strcpy(message, "CQ ");
        strcat(message, c2);
        strcat(message, " ");
        strcat(message, grid);
        return 0;
    }

    if (nc1 >= 267649090L && nc1 <= 267698374L) {
        // CQ with suffix (e.g. /QRP)
        uint32_t n = nc1 - 267649090L;
        char sf[4];
        sf[0] = charn(n % 37);
        n /= 37;
        sf[1] = charn(n % 37);
        n /= 37;
        sf[2] = charn(n % 37);

        unpackcall(nc2, c2);
        unpackgrid(ng, grid);

        strcpy(message, "CQ ");
        strcat(message, c2);
        strcat(message, "/");
        strcat(message, sf);
        strcat(message, " ");
        strcat(message, grid);
        return 0;
    }

    char c1[7];
    
    unpackcall(nc1, c1);
    if (equals(c1, "CQ9DX ")) {
        strcpy(c1, "CQ DX");
    }
    else if (starts_with(c1, " E9") && is_letter(c1[3]) && is_letter(c1[4]) && is_space(c1[5])) {
        strcpy(c1, "CQ ");
        c1[5] = 0;
    }
    unpackcall(nc2, c2);
    unpackgrid(ng, grid);

    strcpy(message, c1);
    strcat(message, " ");
    strcat(message, c2);
    strcat(message, " ");
    strcat(message, grid);

    //if "000AAA" in msg:
    //    return None
    return 0;
}
