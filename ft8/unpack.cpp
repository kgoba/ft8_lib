#include "unpack.h"
#include "text.h"

#include <string.h>
#include "ft8/hashcall.h"

namespace ft8 {

//const uint32_t NBASE = 37L*36L*10L*27L*27L*27L;
const uint32_t MAX22    = 4194304L;
const uint32_t NTOKENS  = 2063592L;
const uint16_t MAXGRID4 = 32400L;

// convert integer index to ASCII character according to one of 5 tables:
// table 0: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"
// table 1: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// table 2: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// table 3: "0123456789"
// table 4: " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// table 5: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/"
char charn(int c, int table_idx) {
    if (table_idx == 0 || table_idx == 1 || table_idx == 4 || table_idx == 5 ) {
        if (c == 0) return ' ';
        c -= 1;
    }
    if (table_idx != 4) {
        if (c < 10) return '0' + c;
        c -= 10;
    }
    if (table_idx != 3) {
        if (c < 26) return 'A' + c;
        c -= 26;
    }
    if (table_idx == 0) {
        if (c < 5) return "+-./?" [c];
    }
    else if (table_idx == 5 && c == 0) {
        return '/';
    }

    return '_'; // unknown character, should never get here
}


// n28 is a 28-bit integer, e.g. n28a or n28b, containing all the
// call sign bits from a packed message.
int unpack28(uint32_t n28, uint8_t ip, uint8_t i3, char *result) {
    // Check for special tokens DE, QRZ, CQ, CQ_nnn, CQ_aaaa
    if (n28 < NTOKENS) {
        if (n28 <= 2) {
            if (n28 == 0) strcpy(result, "DE");
            if (n28 == 1) strcpy(result, "QRZ");
            if (n28 == 2) strcpy(result, "CQ");
            return 0;   // Success
        }
        if (n28 <= 1002) {
            // CQ_nnn with 3 digits
            strcpy(result, "CQ ");
            int_to_dd(result + 3, n28 - 3, 3);
            return 0;   // Success
        }
        if (n28 <= 532443L) {
            // CQ_aaaa with 4 alphanumeric symbols
            uint32_t n = n28 - 1003;
            char aaaa[5];

            aaaa[4] = '\0';
            aaaa[3] = charn(n % 27, 4);
            n /= 27;
            aaaa[2] = charn(n % 27, 4);
            n /= 27;
            aaaa[1] = charn(n % 27, 4);
            n /= 27;
            aaaa[0] = charn(n % 27, 4);

            // Skip leading whitespace
            int ws_len = 0;
            while (aaaa[ws_len] == ' ') {
                ws_len++;
            }

            strcpy(result, "CQ ");
            strcat(result, aaaa + ws_len);
            return 0;   // Success
        }
        // ? TODO: unspecified in the WSJT-X code
        return -1;
    }

    n28 = n28 - NTOKENS;
    if (n28 < MAX22) {
        // This is a 22-bit hash of a result
      	int n22 = n28;
      	hash22(n22, result);	//     !Retrieve result from hash table
        //printf("N22 %d, RESULT : %s", n22, result);
        return 0;
    }

    // Standard callsign
    uint32_t n = n28 - MAX22;

    char callsign[7];
    callsign[6] = '\0';
    callsign[5] = charn(n % 27, 4);
    n /= 27;
    callsign[4] = charn(n % 27, 4);
    n /= 27;
    callsign[3] = charn(n % 27, 4);
    n /= 27;
    callsign[2] = charn(n % 10, 3);
    n /= 10;
    callsign[1] = charn(n % 36, 2);
    n /= 36;
    callsign[0] = charn(n % 37, 1);

    // Skip trailing and leading whitespace in case of a short callsign
    int ws_len = 0;
    while (ws_len <= 5 && callsign[5 - ws_len] == ' ') {
        callsign[5 - ws_len] = '\0';
        ws_len++;
    }
    ws_len = 0;
    while (callsign[ws_len] == ' ') {
        ws_len++;
    }
    strcpy(result, callsign + ws_len);

    // Check if we should append /R or /P suffix
    if (ip) {
        if (i3 == 1) {
            strcat(result, "/R");
        }
        else if (i3 == 2) {
            strcat(result, "/P");
        }
    }
    return 0;   // Success
}


int unpack_type1(const uint8_t *a77, uint8_t i3, char *message) {
    uint32_t n28a, n28b;
    uint16_t igrid4;
    uint8_t  ir;
    
    // Extract packed fields
    // read(c77,1000) n28a,ipa,n28b,ipb,ir,igrid4,i3
    // 1000 format(2(b28,b1),b1,b15,b3)
    n28a  = (a77[0] << 21);
    n28a |= (a77[1] << 13);
    n28a |= (a77[2] << 5);
    n28a |= (a77[3] >> 3);
    n28b  = ((a77[3] & 0x07) << 26);
    n28b |= (a77[4] << 18);
    n28b |= (a77[5] << 10);
    n28b |= (a77[6] << 2);
    n28b |= (a77[7] >> 6);
    ir      = ((a77[7] & 0x20) >> 5);
    igrid4  = ((a77[7] & 0x1F) << 10);
    igrid4 |= (a77[8] << 2);
    igrid4 |= (a77[9] >> 6);

    // Unpack both callsigns
    char field_1[14];
    char field_2[14];
    if (unpack28(n28a >> 1, n28a & 0x01, i3, field_1) < 0) {
        return -1;
    }
    if (unpack28(n28b >> 1, n28b & 0x01, i3, field_2) < 0) {
        return -2;
    }
    // Fix "CQ_" to "CQ " -> already done in unpack28()
    // if (starts_with(field_1, "CQ_")) {
    //     field_1[2] = ' ';
    // }

    // Append first two fields to the result
    strcpy(message, field_1);
    strcat(message, " ");
    strcat(message, field_2);

    // TODO: add to recent calls
    if (field_1[0] != '<' && strlen(field_1) >= 4) {
        // add_call_to_recent_calls(field_1)
    }
    if (field_2[0] != '<' && strlen(field_2) >= 4) {
        // add_call_to_recent_calls(field_2)
    }

    char field_3[5];
    if (igrid4 <= MAXGRID4) {
        // Extract 4 symbol grid locator
        field_3[4] = '\0';

        uint16_t n = igrid4;
        field_3[3] = '0' + (n % 10);
        n /= 10;
        field_3[2] = '0' + (n % 10);
        n /= 10;
        field_3[1] = 'A' + (n % 18);
        n /= 18;
        field_3[0] = 'A' + (n % 18);

        if (ir != 0) {
            // In case of ir=1 add an " R " before grid
            strcat(message, " R ");
        }
    }
    else {
        // Extract report
        int irpt = igrid4 - MAXGRID4;

        // Check special cases first
        if (irpt == 1) field_3[0] = '\0';
        else if (irpt == 2) strcpy(field_3, "RRR");
        else if (irpt == 3) strcpy(field_3, "RR73");
        else if (irpt == 4) strcpy(field_3, "73");
        else if (irpt >= 5) {
            // Extract signal report as a two digit number with a + or - sign
            if (ir == 0) {
                int_to_dd(field_3, irpt - 35, 2, true);
            }
            else {
                field_3[0] = 'R';
                int_to_dd(field_3 + 1, irpt - 35, 2, true);
            }
        }
    }

    // Append the last field to the result
    if (strlen(field_3) > 0) {
        strcat(message, " ");
        strcat(message, field_3);
    }

    return 0;       // Success
}


int unpack_text(const uint8_t *a71, char *text) {
    // TODO: test
    uint8_t b71[9];

    for (int i = 0; i < 9; ++i) {
        b71[i] = a71[i];
    }

    for (int idx = 0; idx < 13; ++idx) {
        // Divide the long integer in b71 by 42
        uint16_t rem = 0;
        for (int i = 8; i >= 0; --i) {
            rem = (rem << 8) | b71[i];
            b71[i] = rem / 42;
            rem    = rem % 42;
        }
        text[idx] = charn(rem, 0);
    }

    text[13] = '\0';
    return 0;       // Success
}


int unpack_telemetry(const uint8_t *a71, char *telemetry) {
    uint8_t b71[9];

    // Shift bits in a71 right by 1
    uint8_t carry = 0;
    for (int i = 0; i < 9; ++i) {
        b71[i] = (carry << 7) | (a71[i] >> 1);
        carry = (a71[i] & 0x01);
    }

    // Convert b71 to hexadecimal string
    for (int i = 0; i < 9; ++i) {
        uint8_t nibble1 = (b71[i] >> 4);
        uint8_t nibble2 = (b71[i] & 0x0F);
        char c1 = (nibble1 > 9) ? (nibble1 - 10 + 'A') : nibble1 + '0';
        char c2 = (nibble2 > 9) ? (nibble2 - 10 + 'A') : nibble2 + '0';
        telemetry[i * 2] = c1;
        telemetry[i * 2 + 1] = c2;
    }

    telemetry[18] = '\0';
    return 0;
}


//none standard for wsjt-x 2.0
//by KD8CEC
int unpack_nonestandard(const uint8_t *a77, uint8_t i3, char *message) 
{
/*
	wsjt-x 2.1.0 rc5
     	read(c77,1050) n12,n58,iflip,nrpt,icq
	1050 format(b12,b58,b1,b2,b1)
*/
	uint32_t n12, iflip, nrpt, icq;
	uint64_t n58;
	n12 = (a77[0] << 4);   //11 ~4  : 8
	n12 |= (a77[1] >> 4);  //3~0 : 12

	n58 = ((uint64_t)(a77[1] & 0x0F) << 54); //57 ~ 54 : 4
	n58 |= ((uint64_t)a77[2] << 46); //53 ~ 46 : 12
	n58 |= ((uint64_t)a77[3] << 38); //45 ~ 38 : 12
	n58 |= ((uint64_t)a77[4] << 30); //37 ~ 30 : 12
	n58 |= ((uint64_t)a77[5] << 22); //29 ~ 22 : 12
	n58 |= ((uint64_t)a77[6] << 14); //21 ~ 14 : 12
	n58 |= ((uint64_t)a77[7] << 6);  //13 ~ 6 : 12
	n58 |= ((uint64_t)a77[8] >> 2);  //5 ~ 0 : 765432 10

	iflip = (a77[8] >> 1) & 0x01;	//76543210
	nrpt  = ((a77[8] & 0x01) << 1);
	nrpt  |= (a77[9] >> 7);	//76543210
	icq   = ((a77[9] >> 6) & 0x01);

	char c11[12];
	c11[11] = '\0';

	c11[10] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[9] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[8] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[7] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[6] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[5] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[4] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[3] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[2] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[1] = charn(n58 % 38, 0);
	n58 /= 38;
	c11[0] = charn(n58 % 38, 0);

	StrTrim(c11);

	char call_3[15];
	char *call_1, *call_2;
	hash12(n12, call_3);

	if (iflip == 0)
	{
		call_1 = call_3;
		call_2 = c11;
		save_hash_call(call_2);
	}
	else
	{
		call_1 = c11;
		call_2 = call_3;
		save_hash_call(call_1);
	}

	if (icq == 0)
	{
		strcpy(message, call_1);
		strcat(message, " ");
		strcat(message, call_2);
		if (nrpt == 1)
			strcat(message, " RRR");
		else if (nrpt == 2)
			strcat(message, " RR73");
		else if (nrpt == 3)
			strcat(message, " 73");

	}
	else
	{
		strcpy(message, "CQ ");
		strcat(message, c11);
	}

    return 0;
}

int unpack77(const uint8_t *a77, char *message) {
    uint8_t  n3, i3;

    // Extract n3 (bits 71..73) and i3 (bits 74..76)
    n3 = ((a77[8] << 2) & 0x04) | ((a77[9] >> 6) & 0x03);
    i3 = (a77[9] >> 3) & 0x07;

    if (i3 == 0 && n3 == 0) {
        // 0.0  Free text
        return unpack_text(a77, message);
    }
    // else if (i3 == 0 && n3 == 1) {
    //     // 0.1  K1ABC RR73; W9XYZ <KH1/KH7Z> -11   28 28 10 5       71   DXpedition Mode
    // }
    // else if (i3 == 0 && n3 == 2) {
    //     // 0.2  PA3XYZ/P R 590003 IO91NP           28 1 1 3 12 25   70   EU VHF contest
    // }
    // else if (i3 == 0 && (n3 == 3 || n3 == 4)) {
    //     // 0.3   WA9XYZ KA1ABC R 16A EMA            28 28 1 4 3 7    71   ARRL Field Day
    //     // 0.4   WA9XYZ KA1ABC R 32A EMA            28 28 1 4 3 7    71   ARRL Field Day
    // }
    else if (i3 == 0 && n3 == 5) {
        // 0.5   0123456789abcdef01                 71               71   Telemetry (18 hex)
        return unpack_telemetry(a77, message);
    }
    else if (i3 == 1 || i3 == 2) {
        // Type 1 (standard message) or Type 2 ("/P" form for EU VHF contest)
        return unpack_type1(a77, i3, message);
    }
    // else if (i3 == 3) {
    //     // Type 3: ARRL RTTY Contest
    // }
    else if (i3 == 4) {
    //     // Type 4: Nonstandard calls, e.g. <WA9XYZ> PJ4/KA1ABC RR73
    //     // One hashed call or "CQ"; one compound or nonstandard call with up
    //     // to 11 characters; and (if not "CQ") an optional RRR, RR73, or 73.
	return unpack_nonestandard(a77, i3, message);
    }
    else {
        // unknown type
        message[0] = '\0';
    }

    return 0;
}

} // namespace