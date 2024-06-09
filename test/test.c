#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "ft8/text.h"
#include "ft8/encode.h"
#include "ft8/constants.h"

#include "fft/kiss_fftr.h"
#include "common/common.h"
#include "ft8/message.h"

#define LOG_LEVEL LOG_INFO
#include "ft8/debug.h"

#ifndef HAVE_STPCPY
#define HAVE_STPCPY

static char *stpcpy(char *dest, const char *src) {
    strcpy(dest, src);
    return dest + strlen(src);
}

#endif

// void convert_8bit_to_6bit(uint8_t* dst, const uint8_t* src, int nBits)
// {
//     // Zero-fill the destination array as we will only be setting bits later
//     for (int j = 0; j < (nBits + 5) / 6; ++j)
//     {
//         dst[j] = 0;
//     }

//     // Set the relevant bits
//     uint8_t mask_src = (1 << 7);
//     uint8_t mask_dst = (1 << 5);
//     for (int i = 0, j = 0; nBits > 0; --nBits)
//     {
//         if (src[i] & mask_src)
//         {
//             dst[j] |= mask_dst;
//         }
//         mask_src >>= 1;
//         if (mask_src == 0)
//         {
//             mask_src = (1 << 7);
//             ++i;
//         }
//         mask_dst >>= 1;
//         if (mask_dst == 0)
//         {
//             mask_dst = (1 << 5);
//             ++j;
//         }
//     }
// }

/*
bool test1() {
    //const char *msg = "CQ DL7ACA JO40"; // 62, 32, 32, 49, 37, 27, 59, 2, 30, 19, 49, 16
    const char *msg = "VA3UG   F1HMR 73"; // 52, 54, 60, 12, 55, 54, 7, 19, 2, 23, 59, 16
    //const char *msg = "RA3Y VE3NLS 73";   // 46, 6, 32, 22, 55, 20, 11, 32, 53, 23, 59, 16
    uint8_t a72[9];

    int rc = packmsg(msg, a72);
    if (rc < 0) return false;

    LOG(LOG_INFO, "8-bit packed: ");
    for (int i = 0; i < 9; ++i) {
        LOG(LOG_INFO, "%02x ", a72[i]);
    }
    LOG(LOG_INFO, "\n");

    uint8_t a72_6bit[12];
    convert_8bit_to_6bit(a72_6bit, a72, 72);
    LOG(LOG_INFO, "6-bit packed: ");
    for (int i = 0; i < 12; ++i) {
        LOG(LOG_INFO, "%d ", a72_6bit[i]);
    }
    LOG(LOG_INFO, "\n");

    char msg_out_raw[14];
    unpack(a72, msg_out_raw);

    char msg_out[14];
    fmtmsg(msg_out, msg_out_raw);
    LOG(LOG_INFO, "msg_out = [%s]\n", msg_out);
    return true;
}


void test2() {
    uint8_t test_in[11] = { 0xF1, 0x02, 0x03, 0x04, 0x05, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xFF };
    uint8_t test_out[22];

    encode174(test_in, test_out);

    for (int j = 0; j < 22; ++j) {
        LOG(LOG_INFO, "%02x ", test_out[j]);
    }
    LOG(LOG_INFO, "\n");
}


void test3() {
    uint8_t test_in2[10] = { 0x11, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x10, 0x04, 0x01, 0x00 };
    uint16_t crc1 = ftx_compute_crc(test_in2, 76);  // Calculate CRC of 76 bits only
    LOG(LOG_INFO, "CRC: %04x\n", crc1);            // should be 0x0708
}
*/

#define CHECK(condition)                                       \
    if (!(condition))                                          \
    {                                                          \
        printf("FAIL: Condition \'" #condition "' failed!\n"); \
        return;                                                \
    }

#define TEST_END printf("Test OK\n\n")

#define CALLSIGN_HASHTABLE_SIZE 256

struct
{
    char callsign[12];
    uint32_t hash;
} callsign_hashtable[CALLSIGN_HASHTABLE_SIZE];

void hashtable_init(void)
{
    // for (int idx = 0; idx < CALLSIGN_HASHTABLE_SIZE; ++idx)
    // {
    //     callsign_hashtable[idx]->callsign[0] = '\0';
    // }
    memset(callsign_hashtable, 0, sizeof(callsign_hashtable));
}

void hashtable_add(const char* callsign, uint32_t hash)
{
    int idx_hash = (hash * 23) % CALLSIGN_HASHTABLE_SIZE;
    while (callsign_hashtable[idx_hash].callsign[0] != '\0')
    {
        if ((callsign_hashtable[idx_hash].hash == hash) && (0 == strcmp(callsign_hashtable[idx_hash].callsign, callsign)))
        {
            LOG(LOG_DEBUG, "Found a duplicate [%s]\n", callsign);
            return;
        }
        else
        {
            LOG(LOG_DEBUG, "Hash table clash!\n");
            // Move on to check the next entry in hash table
            idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_SIZE;
        }
    }
    strncpy(callsign_hashtable[idx_hash].callsign, callsign, 11);
    callsign_hashtable[idx_hash].callsign[11] = '\0';
    callsign_hashtable[idx_hash].hash = hash;
}

bool hashtable_lookup(ftx_callsign_hash_type_t hash_type, uint32_t hash, char* callsign)
{
    uint32_t hash_mask = (hash_type == FTX_CALLSIGN_HASH_10_BITS) ? 0x3FFu : (hash_type == FTX_CALLSIGN_HASH_12_BITS ? 0xFFFu : 0x3FFFFFu);
    int idx_hash = (hash * 23) % CALLSIGN_HASHTABLE_SIZE;
    while (callsign_hashtable[idx_hash].callsign[0] != '\0')
    {
        if ((callsign_hashtable[idx_hash].hash & hash_mask) == hash)
        {
            strcpy(callsign, callsign_hashtable[idx_hash].callsign);
            return true;
        }
        // Move on to check the next entry in hash table
        idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_SIZE;
    }
    callsign[0] = '\0';
    return false;
}

ftx_callsign_hash_interface_t hash_if = {
    .lookup_hash = hashtable_lookup,
    .save_hash = hashtable_add
};

void test_std_msg(const char* call_to_tx, const char* call_de_tx, const char* extra_tx)
{
    ftx_message_t msg;
    ftx_message_init(&msg);

    ftx_message_rc_t rc_encode = ftx_message_encode_std(&msg, &hash_if, call_to_tx, call_de_tx, extra_tx);
    CHECK(rc_encode == FTX_MESSAGE_RC_OK);
    printf("Encoded [%s] [%s] [%s]\n", call_to_tx, call_de_tx, extra_tx);

    char call_to[14];
    char call_de[14];
    char extra[14];
    ftx_message_rc_t rc_decode = ftx_message_decode_std(&msg, &hash_if, call_to, call_de, extra);
    CHECK(rc_decode == FTX_MESSAGE_RC_OK);
    printf("Decoded [%s] [%s] [%s]\n", call_to, call_de, extra);
    CHECK(0 == strcmp(call_to, call_to_tx));
    CHECK(0 == strcmp(call_de, call_de_tx));
    CHECK(0 == strcmp(extra, extra_tx));
    // CHECK(1 == 2);
    TEST_END;
}

void test_msg(const char* call_to_tx, const char* call_de_tx, const char* extra_tx)
{
    char message_text[12 + 12 + 20];
    char* copy_ptr = message_text;
    copy_ptr = stpcpy(copy_ptr, call_to_tx);
    copy_ptr = stpcpy(copy_ptr, " ");
    copy_ptr = stpcpy(copy_ptr, call_de_tx);
    if (strlen(extra_tx) > 0)
    {
        copy_ptr = stpcpy(copy_ptr, " ");
        copy_ptr = stpcpy(copy_ptr, extra_tx);
    }
    printf("Testing [%s]\n", message_text);

    ftx_message_t msg;
    ftx_message_init(&msg);

    ftx_message_rc_t rc_encode = ftx_message_encode(&msg, NULL, message_text);
    CHECK(rc_encode == FTX_MESSAGE_RC_OK);

    char call_to[14];
    char call_de[14];
    char extra[14];
    ftx_message_rc_t rc_decode = ftx_message_decode_std(&msg, NULL, call_to, call_de, extra);
    CHECK(rc_decode == FTX_MESSAGE_RC_OK);
    CHECK(0 == strcmp(call_to, call_to_tx));
    CHECK(0 == strcmp(call_de, call_de_tx));
    CHECK(0 == strcmp(extra, extra_tx));
    // CHECK(1 == 2);
    TEST_END;
}

#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof((x)[0]))

int main()
{
    // test1();
    // test4();
    const char* callsigns[] = { "YL3JG", "W1A", "W1A/R", "W5AB", "W8ABC", "DE6ABC", "DE6ABC/R", "DE7AB", "DE9A", "3DA0X", "3DA0XYZ", "3DA0XYZ/R", "3XZ0AB", "3XZ0A" };
    const char* tokens[] = { "CQ", "QRZ" };
    const char* grids[] = { "KO26", "RR99", "AA00", "RR09", "AA01", "RRR", "RR73", "73", "R+10", "R+05", "R-12", "R-02", "+10", "+05", "-02", "-02", "" };

    for (int idx_grid = 0; idx_grid < SIZEOF_ARRAY(grids); ++idx_grid)
    {
        for (int idx_callsign = 0; idx_callsign < SIZEOF_ARRAY(callsigns); ++idx_callsign)
        {
            for (int idx_callsign2 = 0; idx_callsign2 < SIZEOF_ARRAY(callsigns); ++idx_callsign2)
            {
                test_std_msg(callsigns[idx_callsign], callsigns[idx_callsign2], grids[idx_grid]);
            }
        }
        for (int idx_token = 0; idx_token < SIZEOF_ARRAY(tokens); ++idx_token)
        {
            for (int idx_callsign2 = 0; idx_callsign2 < SIZEOF_ARRAY(callsigns); ++idx_callsign2)
            {
                test_std_msg(tokens[idx_token], callsigns[idx_callsign2], grids[idx_grid]);
            }
        }
    }

    // test_std_msg("YOMAMA", "MYMAMA/QRP", "73");

    return 0;
}
