#include "unpack.h"
#include "text.h"

#include <string.h>

namespace ft8 {

//const uint32_t NBASE = 37L*36L*10L*27L*27L*27L;
const uint32_t MAX22    = 4194304L;
const uint32_t NTOKENS  = 2063592L;
const uint16_t MAXGRID4 = 32400L;


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
            for (int i = 3; /* */; --i) {
                aaaa[i] = charn(n % 27, 4);
                if (i == 0) break;
                n /= 27;
            }

            strcpy(result, "CQ ");
            strcat(result, trim_front(aaaa));
            return 0;   // Success
        }
        // ? TODO: unspecified in the WSJT-X code
        return -1;
    }

    n28 = n28 - NTOKENS;
    if (n28 < MAX22) {
        // This is a 22-bit hash of a result
        //call hash22(n22,c13)     !Retrieve result from hash table
        // TODO: implement
        // strcpy(result, "<...>");
        result[0] = '<';
        int_to_dd(result + 1, n28, 7);
        result[8] = '>';
        result[9] = '\0';
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
    strcpy(result, trim(callsign));
    if (strlen(result) == 0) return -1;

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


int unpack_type1(const uint8_t *a77, uint8_t i3, char *field1, char *field2, char *field3) {
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
    if (unpack28(n28a >> 1, n28a & 0x01, i3, field1) < 0) {
        return -1;
    }
    if (unpack28(n28b >> 1, n28b & 0x01, i3, field2) < 0) {
        return -2;
    }
    // Fix "CQ_" to "CQ " -> already done in unpack28()

    // TODO: add to recent calls
    // if (field1[0] != '<' && strlen(field1) >= 4) {
    //     save_hash_call(field1)
    // }
    // if (field2[0] != '<' && strlen(field2) >= 4) {
    //     save_hash_call(field2)
    // }

    if (igrid4 <= MAXGRID4) {
        // Extract 4 symbol grid locator
        char *dst = field3;
        uint16_t n = igrid4;
        if (ir > 0) {
            // In case of ir=1 add an "R" before grid
            dst = stpcpy(dst, "R ");
        }

        dst[4] = '\0';
        dst[3] = '0' + (n % 10);
        n /= 10;
        dst[2] = '0' + (n % 10);
        n /= 10;
        dst[1] = 'A' + (n % 18);
        n /= 18;
        dst[0] = 'A' + (n % 18);
        // if(msg(1:3).eq.'CQ ' .and. ir.eq.1) unpk77_success=.false.
        // if (ir > 0 && strncmp(field1, "CQ", 2) == 0) return -1;
    }
    else {
        // Extract report
        int irpt = igrid4 - MAXGRID4;

        // Check special cases first
        if (irpt == 1) field3[0] = '\0';
        else if (irpt == 2) strcpy(field3, "RRR");
        else if (irpt == 3) strcpy(field3, "RR73");
        else if (irpt == 4) strcpy(field3, "73");
        else if (irpt >= 5) {
            char *dst = field3;
            // Extract signal report as a two digit number with a + or - sign
            if (ir > 0) {
                *dst++ = 'R'; // Add "R" before report
            }
            int_to_dd(dst, irpt - 35, 2, true);
        }
        // if(msg(1:3).eq.'CQ ' .and. irpt.ge.2) unpk77_success=.false.
        // if (irpt >= 2 && strncmp(field1, "CQ", 2) == 0) return -1;
    }

    return 0;       // Success
}


int unpack_text(const uint8_t *a71, char *text) {
    // TODO: test
    uint8_t b71[9];

    uint8_t carry = 0;
    for (int i = 0; i < 9; ++i) {
        b71[i] = carry | (a71[i] >> 1);
        carry = (a71[i] & 1) ? 0x80 : 0;
    }

	char c14[14];
	c14[13] = 0;
    for (int idx = 12; idx >= 0; --idx) {
        // Divide the long integer in b71 by 42
        uint16_t rem = 0;
        for (int i = 0; i < 9; ++i) {
            rem = (rem << 8) | b71[i];
            b71[i] = rem / 42;
            rem    = rem % 42;
        }
        c14[idx] = charn(rem, 0);
    }

	strcpy(text, trim(c14));
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
int unpack_nonstandard(const uint8_t *a77, char *field1, char *field2, char *field3) 
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

    for (int i = 10; /* no condition */ ; --i) {
    	c11[i] = charn(n58 % 38, 5);
        if (i == 0) break;
    	n58 /= 38;        
    }

	char call_3[15];
    // should replace with hash12(n12, call_3);
    // strcpy(call_3, "<...>");
    call_3[0] = '<';
    int_to_dd(call_3 + 1, n12, 4);
    call_3[5] = '>';
    call_3[6] = '\0';

	char * call_1 = (iflip) ? c11 : call_3;
    char * call_2 = (iflip) ? call_3 : c11;
	//save_hash_call(c11_trimmed);

	if (icq == 0) {
		strcpy(field1, trim(call_1));
		if (nrpt == 1)
			strcpy(field3, "RRR");
		else if (nrpt == 2)
			strcpy(field3, "RR73");
		else if (nrpt == 3)
			strcpy(field3, "73");
        else {
            field3[0] = '\0';
        }
	} else {
		strcpy(field1, "CQ");
        field3[0] = '\0';
	}
    strcpy(field2, trim(call_2));

    return 0;
}

int unpack77_fields(const uint8_t *a77, char *field1, char *field2, char *field3) {
    uint8_t  n3, i3;

    // Extract n3 (bits 71..73) and i3 (bits 74..76)
    n3 = ((a77[8] << 2) & 0x04) | ((a77[9] >> 6) & 0x03);
    i3 = (a77[9] >> 3) & 0x07;

    field1[0] = field2[0] = field3[0] = '\0';

    if (i3 == 0 && n3 == 0) {
        // 0.0  Free text
        return unpack_text(a77, field1);
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
        return unpack_telemetry(a77, field1);
    }
    else if (i3 == 1 || i3 == 2) {
        // Type 1 (standard message) or Type 2 ("/P" form for EU VHF contest)
        return unpack_type1(a77, i3, field1, field2, field3);
    }
    // else if (i3 == 3) {
    //     // Type 3: ARRL RTTY Contest
    // }
    else if (i3 == 4) {
    //     // Type 4: Nonstandard calls, e.g. <WA9XYZ> PJ4/KA1ABC RR73
    //     // One hashed call or "CQ"; one compound or nonstandard call with up
    //     // to 11 characters; and (if not "CQ") an optional RRR, RR73, or 73.
	    return unpack_nonstandard(a77, field1, field2, field3);
    }
    // else if (i3 == 5) {
    //     // Type 5: TU; W9XYZ K1ABC R-09 FN             1 28 28 1 7 9       74   WWROF contest
    // }

    // unknown type, should never get here
    return -1;
}

int unpack77(const uint8_t *a77, char *message) {
    char field1[14];
    char field2[14];
    char field3[7];
    int rc = unpack77_fields(a77, field1, field2, field3);
    if (rc < 0) return rc;

    char *dst = message;
    // int msg_sz = strlen(field1) + strlen(field2) + strlen(field3) + 2;

    dst = stpcpy(dst, field1);
    *dst++ = ' ';
    dst = stpcpy(dst, field2);
    *dst++ = ' ';
    dst = stpcpy(dst, field3);
    *dst = '\0';

    return 0;
}

} // namespace
