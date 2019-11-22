#pragma once

#include <stdint.h>

namespace ft8 {

class CallsignHasher {
    virtual void save_callsign(void *obj, const char *callsign) = 0;
    virtual bool hash10(void *obj, uint16_t hash, char *result) = 0;
    virtual bool hash12(void *obj, uint16_t hash, char *result) = 0;
    virtual bool hash22(void *obj, uint32_t hash, char *result) = 0;
};

class EmptyHasher : public CallsignHasher {
    virtual void save_callsign(void *obj, const char *callsign) override {
    }
    virtual bool hash10(void *obj, uint16_t hash, char *result) override {
        strcpy(result, "...");
        return true;
    }
    virtual bool hash12(void *obj, uint16_t hash, char *result) override {
        strcpy(result, "...");
        return true;
    }
    virtual bool hash22(void *obj, uint32_t hash, char *result) override {
        strcpy(result, "...");
        return true;
    }
};

struct Message77 {
    uint8_t i3, n3;

    // 11 chars nonstd call + 2 chars <...>
    // 6 chars for grid/report/courtesy
    char field1[13 + 1];
    char field2[13 + 1];
    char field3[6 + 1];

    Message77();
    int unpack(const uint8_t *packed77);
    void pack(uint8_t *packed77);
    int str(char *buf, int buf_sz) const;

private:
};

} // namespace ft8
