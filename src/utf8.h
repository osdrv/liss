#ifndef liss_utf8_h
#define liss_utf8_h

#include <stdint.h>

int utf8_char_len(const char* s);

uint32_t utf8_decode(const char** s);

uint32_t utf8_decode_peek(const char* s);

int utf8_strlen(const char* s, int byte_len);

int utf8_byte_offset(const char* s, int byte_len, int cp_idx);

#endif
