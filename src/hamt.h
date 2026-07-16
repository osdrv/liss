#ifndef liss_hamt_h
#define liss_hamt_h

#include <string.h>

#include "object.h"
#include "value.h"

#define HAMT_BITS 5
#define HAMT_WIDTH (1u << HAMT_BITS)  // 32-way branching
#define HAMT_MASK (HAMT_WIDTH - 1)
#define HAMT_DEPTH_MAX 12  // 12 * 5 = 60 bits of 64-bit hash

typedef struct HamtNode HamtNode;
struct HamtNode {
    Obj obj;
    bool is_collision;
    union {
        struct {                // inner node
            uint32_t data_map;  // bit i: slot i has an inline kv pair here
            uint32_t node_map;  // bit i: slot i has a child subtrie
            Value* data;        // 2 * popcount(node_map) children
            HamtNode** nodes;
        };
        struct {
            int cnt;
            Value* pairs;  // 2 * cnt Values [k, v, k, v, ...]
        };
    };
};

static inline uint64_t hamtHash(Value v) {
    switch (v.type) {
        case VAL_NIL:
            return 0;
        case VAL_BOOL:
            return AS_BOOL(v) ? 1 : 2;
        case VAL_INT: {
            uint64_t n = (uint64_t)AS_INT(v);
            return n ^ (n >> 33) ^
                   (n &
                    0xff51afd7ed558ccdull);  // this constant is a murmurhash3
                                             // hash finalizer: each input bit
                                             // flip causes ~half of the output
                                             // bits to flip (aka the avalanche
                                             // effect) so the trie stays
                                             // balanced rather than skewing
                                             // left
        }
        case VAL_REAL: {
            uint64_t bits;
            memcpy(&bits, &v.as.real, 8);
            return bits ^ (bits >> 33);
        }
        case VAL_OBJ: {
            if (OBJ_TYPE(v) == OBJ_STRING)
                return AS_STRING(v)
                    ->hash;  // A string is fnv-1a-hashed. It is good enough to
                             // get a balanced hash value
            return (uint64_t)(uintptr_t)AS_OBJ(v);
        }
    }
    return 0;
}

static inline uint32_t hamt_chunk(uint64_t hash, int depth) {
    return (uint32_t)((hash >> (depth * HAMT_BITS)) & HAMT_MASK);
}

static inline int hamt_idx(uint32_t bitmap, uint32_t bit) {
    return __builtin_popcount(bitmap & (bit - 1));
}

struct VM;

HamtNode* hamtNew(struct VM* vm);
Value* hamtGet(HamtNode* node, Value key, uint64_t hash, int depth);
HamtNode* hamtPut(struct VM* vm, HamtNode* node, Value key, Value val,
                  uint64_t hash, int depth);
HamtNode* hamtDel(struct VM* vm, HamtNode* node, Value key, uint64_t hash,
                  int depth);
void hamtEach(HamtNode* node, void (*fn)(Value key, Value val, void* ctx),
              void* ctx);
void hamtMark(struct VM* vm, HamtNode* node);
void hamtFree(struct VM* vm, HamtNode* node);

#endif
