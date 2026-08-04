#include "utf8.h"

#include <stdint.h>

int utf8_char_len(const char* s) {
    uint8_t b = (uint8_t)*s;
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;  // invalid continuation byte — treat as opaque single byte
}

int utf8_strlen(const char* s, int byte_len) {
    int str_len = 0;
    const char* ptr = s;
    while (ptr < s + byte_len) {
        ptr += utf8_char_len(ptr);
        str_len++;
    }

    return str_len;
}

uint32_t utf8_decode(const char** s) {
    const int nb = utf8_char_len(*s);
    static const uint8_t leader_mask[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t ch = leader_mask[nb] & (uint8_t)**s;
    for (int i = 1; i <= nb - 1; i++) {
        ch <<= 6;
        ch |= 0x3F & (uint8_t)(*(*s + i));
    }
    *s += nb;
    return ch;
}

uint32_t utf8_decode_peek(const char* s) {
    const int nb = utf8_char_len(s);
    static const uint8_t leader_mask[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t ch = leader_mask[nb] & (uint8_t)*s;
    for (int i = 1; i <= nb - 1; i++) {
        ch <<= 6;
        ch |= 0x3F & (uint8_t)(*(s + i));
    }
    return ch;
}

int utf8_byte_offset(const char* s, int byte_len, int cp_idx) {
    const char* ptr = s;
    const char* end = s + byte_len;
    for (int i = 0; i < cp_idx; i++) {
        if (ptr >= end) return -1;
        ptr += utf8_char_len(ptr);
    }
    if (ptr >= end) return -1;
    return (int)(ptr - s);
}
