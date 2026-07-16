#include "hamt.h"

#include <stdint.h>

#include "gc.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static HamtNode* allocNode(VM* vm) {
    HamtNode* node = (HamtNode*)reallocate(vm, NULL, 0, sizeof(HamtNode));
    node->obj.type = OBJ_HAMT_NODE;
    node->obj.isMarked = false;
    node->obj.next = vm->objects;
    vm->objects = (Obj*)node;
    node->is_collision = false;
    node->data_map = 0;
    node->node_map = 0;
    node->data = NULL;
    node->nodes = NULL;
    return node;
}

HamtNode* hamtNew(VM* vm) { return allocNode(vm); }

Value* hamtGet(HamtNode* node, Value key, uint64_t hash, int depth) {
    if (node == NULL) return NULL;

    if (node->is_collision) {
        for (int i = 0; i < node->cnt; i++) {
            if (valuesEqual(node->pairs[2 * i], key)) {
                return &node->pairs[2 * i + 1];
            }
        }
        return NULL;
    }

    uint32_t bit = 1u << hamt_chunk(hash, depth);

    if (node->data_map & bit) {
        int idx = hamt_idx(node->data_map, bit);
        if (valuesEqual(node->data[2 * idx], key)) {
            return &node->data[2 * idx + 1];
        }
        return NULL;
    }

    if (node->node_map & bit) {
        int idx = hamt_idx(node->node_map, bit);
        return hamtGet(node->nodes[idx], key, hash, depth + 1);
    }

    return NULL;
}

HamtNode* hamtPut(VM* vm, HamtNode* node, Value key, Value val, uint64_t hash,
                  int depth) {
    // Case 1: an empty subtrie
    if (node == NULL) {
        HamtNode* n = allocNode(vm);
        if (depth > HAMT_DEPTH_MAX) {
            n->is_collision = true;
            n->cnt = 1;
            n->pairs = (Value*)reallocate(vm, NULL, 0, 2 * sizeof(Value));
            n->pairs[0] = key;
            n->pairs[1] = val;
        } else {
            uint32_t bit = 1u << hamt_chunk(hash, depth);
            n->data_map = bit;
            n->data = (Value*)reallocate(vm, NULL, 0, 2 * sizeof(Value));
            n->data[0] = key;
            n->data[1] = val;
        }
        return n;
    }

    // Case 2: collision node
    if (node->is_collision) {
        for (int i = 0; i < node->cnt; i++) {
            if (valuesEqual(node->pairs[2 * i], key)) {
                HamtNode* n = allocNode(vm);
                n->is_collision = true;
                n->cnt = node->cnt;
                n->pairs =
                    (Value*)reallocate(vm, NULL, 0, 2 * n->cnt * sizeof(Value));
                memcpy(n->pairs, node->pairs, 2 * n->cnt * sizeof(Value));
                n->pairs[2 * i + 1] = val;
                return n;
            }
        }
        HamtNode* n = allocNode(vm);
        n->is_collision = true;
        n->cnt = node->cnt + 1;
        n->pairs = (Value*)reallocate(vm, NULL, 0, 2 * n->cnt * sizeof(Value));
        memcpy(n->pairs, node->pairs, 2 * node->cnt * sizeof(Value));
        n->pairs[2 * node->cnt] = key;
        n->pairs[2 * node->cnt + 1] = val;
        return n;
    }

    uint32_t bit = 1u << hamt_chunk(hash, depth);
    int dc = __builtin_popcount(node->data_map);
    int nc = __builtin_popcount(node->node_map);

    // Case 3: slot has an inline pair
    if (node->data_map & bit) {
        int didx = hamt_idx(node->data_map, bit);

        // 3a: same key, we need to update the value
        if (valuesEqual(node->data[2 * didx], key)) {
            HamtNode* n = allocNode(vm);
            n->data_map = node->data_map;
            n->node_map = node->node_map;
            n->data = (Value*)reallocate(vm, NULL, 0, dc * 2 * sizeof(Value));
            memcpy(n->data, node->data, dc * 2 * sizeof(Value));
            n->data[2 * didx + 1] = val;
            if (nc > 0) {
                n->nodes =
                    (HamtNode**)reallocate(vm, NULL, 0, nc * sizeof(HamtNode*));
                memcpy(n->nodes, node->nodes, nc * sizeof(HamtNode*));
            }
            return n;
        }

        // 3b: different key, push both down into a subtrie
        Value ex_key = node->data[2 * didx];
        Value ex_val = node->data[2 * didx + 1];
        uint64_t ex_hash = hamtHash(ex_key);
        HamtNode* sub = hamtPut(vm, NULL, ex_key, ex_val, ex_hash, depth + 1);
        push(vm, OBJ_VAL(sub));
        sub = hamtPut(vm, sub, key, val, hash, depth + 1);
        vm->stack_top[-1] = OBJ_VAL(sub);

        int nidx = hamt_idx(node->node_map, bit);
        HamtNode* n = allocNode(vm);
        n->data_map = node->data_map & ~bit;
        n->node_map = node->node_map | bit;
        if (dc - 1 > 0) {
            n->data =
                (Value*)reallocate(vm, NULL, 0, (dc - 1) * 2 * sizeof(Value));
            memcpy(n->data, node->data, didx * 2 * sizeof(Value));
            memcpy(&n->data[2 * didx], &node->data[2 * (didx + 1)],
                   (dc - didx - 1) * 2 * sizeof(Value));
        }
        n->nodes =
            (HamtNode**)reallocate(vm, NULL, 0, (nc + 1) * sizeof(HamtNode*));
        memcpy(n->nodes, node->nodes, nidx * sizeof(HamtNode*));
        n->nodes[nidx] = sub;
        memcpy(&n->nodes[nidx + 1], &node->nodes[nidx],
               (nc - nidx) * sizeof(HamtNode*));
        pop(vm);
        return n;
    }

    // Case 4: slot has a child subtrie
    if (node->node_map & bit) {
        int nidx = hamt_idx(node->node_map, bit);
        HamtNode* child =
            hamtPut(vm, node->nodes[nidx], key, val, hash, depth + 1);
        push(vm, OBJ_VAL(child));
        HamtNode* n = allocNode(vm);
        n->data_map = node->data_map;
        n->node_map = node->node_map;
        if (dc > 0) {
            n->data = (Value*)reallocate(vm, NULL, 0, dc * 2 * sizeof(Value));
            memcpy(n->data, node->data, dc * 2 * sizeof(Value));
        }
        n->nodes = (HamtNode**)reallocate(vm, NULL, 0, nc * sizeof(HamtNode*));
        memcpy(n->nodes, node->nodes, nc * sizeof(HamtNode*));
        n->nodes[nidx] = child;
        pop(vm);
        return n;
    }

    // Case 5: slot is empty, insert inline pair
    int didx = hamt_idx(node->data_map, bit);
    HamtNode* n = allocNode(vm);
    n->data_map = node->data_map | bit;
    n->node_map = node->node_map;
    n->data = (Value*)reallocate(vm, NULL, 0, (dc + 1) * 2 * sizeof(Value));
    memcpy(n->data, node->data, didx * 2 * sizeof(Value));
    n->data[2 * didx] = key;
    n->data[2 * didx + 1] = val;
    memcpy(&n->data[2 * (didx + 1)], &node->data[2 * didx],
           (dc - didx) * 2 * sizeof(Value));
    if (nc > 0) {
        n->nodes = (HamtNode**)reallocate(vm, NULL, 0, nc * sizeof(HamtNode*));
        memcpy(n->nodes, node->nodes, nc * sizeof(HamtNode*));
    }
    return n;
}

HamtNode* hamtDel(VM* vm, HamtNode* node, Value key, uint64_t hash, int depth) {
    if (node == NULL) {
        return NULL;
    }

    // Case 2: collision node
    if (node->is_collision) {
        for (int i = 0; i < node->cnt; i++) {
            if (valuesEqual(node->pairs[2 * i], key)) {
                if (node->cnt == 1) {
                    return NULL;
                }
                HamtNode* n = allocNode(vm);
                n->is_collision = true;
                n->cnt = node->cnt - 1;
                n->pairs =
                    (Value*)reallocate(vm, NULL, 0, 2 * n->cnt * sizeof(Value));
                memcpy(n->pairs, node->pairs, i * 2 * sizeof(Value));
                memcpy(&n->pairs[2 * i], &node->pairs[2 * (i + 1)],
                       (node->cnt - i - 1) * 2 * sizeof(Value));
                return n;
            }
        }
        return node;  // not found
    }

    uint32_t bit = 1u << hamt_chunk(hash, depth);
    int dc = __builtin_popcount(node->data_map);
    int nc = __builtin_popcount(node->node_map);

    // Case 3: slot has an inline pair
    if (node->data_map & bit) {
        int didx = hamt_idx(node->data_map, bit);
        if (!valuesEqual(node->data[2 * didx], key)) {
            return node;  // not found
        }
        if (dc == 1 && nc == 0) {
            return NULL;  // node would turn empty
        }
        HamtNode* n = allocNode(vm);
        n->data_map = node->data_map & ~bit;
        n->node_map = node->node_map;
        if (dc - 1 > 0) {
            n->data =
                (Value*)reallocate(vm, NULL, 0, (dc - 1) * 2 * sizeof(Value));
            memcpy(n->data, node->data, didx * 2 * sizeof(Value));
            memcpy(&n->data[2 * didx], &node->data[2 * (didx + 1)],
                   (dc - didx - 1) * 2 * sizeof(Value));
        }
        if (nc > 0) {
            n->nodes =
                (HamtNode**)reallocate(vm, NULL, 0, nc * sizeof(HamtNode*));
            memcpy(n->nodes, node->nodes, nc * sizeof(HamtNode*));
        }
        return n;
    }

    // Case 4: slot has a child subtrie
    if (node->node_map & bit) {
        int nidx = hamt_idx(node->node_map, bit);
        HamtNode* child = hamtDel(vm, node->nodes[nidx], key, hash, depth + 1);
        if (child == node->nodes[nidx]) {
            return node;  // not found
        }

        if (child == NULL) {
            if (dc == 0 && nc == 1) {
                return NULL;
            }
            HamtNode* n = allocNode(vm);
            n->data_map = node->data_map;
            n->node_map = node->node_map & ~bit;
            if (dc > 0) {
                n->data =
                    (Value*)reallocate(vm, NULL, 0, dc * 2 * sizeof(Value));
                memcpy(n->data, node->data, dc * 2 * sizeof(Value));
            }
            if (nc - 1 > 0) {
                n->nodes = (HamtNode**)reallocate(vm, NULL, 0,
                                                  (nc - 1) * sizeof(HamtNode*));
                memcpy(n->nodes, node->nodes, nidx * sizeof(HamtNode*));
                memcpy(&n->nodes[nidx], &node->nodes[nidx + 1],
                       (nc - nidx - 1) * sizeof(HamtNode*));
            }
            return n;
        }

        push(vm, OBJ_VAL(child));
        HamtNode* n = allocNode(vm);
        n->data_map = node->data_map;
        n->node_map = node->node_map;
        if (dc > 0) {
            n->data = (Value*)reallocate(vm, NULL, 0, dc * 2 * sizeof(Value));
            memcpy(n->data, node->data, dc * 2 * sizeof(Value));
        }
        n->nodes = (HamtNode**)reallocate(vm, NULL, 0, nc * sizeof(HamtNode*));
        memcpy(n->nodes, node->nodes, nc * sizeof(HamtNode*));
        n->nodes[nidx] = child;
        pop(vm);
        return n;
    }

    return node;  // slot is empty, the key is not in the trie
}

void hamtEach(HamtNode* node, void (*fn)(Value key, Value val, void* ctx),
              void* ctx) {
    if (node == NULL) return;
    if (node->is_collision) {
        for (int i = 0; i < node->cnt; i++) {
            fn(node->pairs[2 * i], node->pairs[2 * i + 1], ctx);
        }
        return;
    }
    int dc = __builtin_popcount(node->data_map);
    for (int i = 0; i < dc; i++) {
        fn(node->data[2 * i], node->data[2 * i + 1], ctx);
    }
    int nc = __builtin_popcount(node->node_map);
    for (int i = 0; i < nc; i++) {
        hamtEach(node->nodes[i], fn, ctx);
    }
}

void hamtMark(VM* vm, HamtNode* node) {
    if (node == NULL) return;
    if (node->is_collision) {
        for (int i = 0; i < node->cnt; i++) {
            markValue(vm, node->pairs[2 * i]);
            markValue(vm, node->pairs[2 * i + 1]);
        }
        return;
    }
    int dc = __builtin_popcount(node->data_map);
    for (int i = 0; i < dc; i++) {
        markValue(vm, node->data[2 * i]);
        markValue(vm, node->data[2 * i + 1]);
    }
    int nc = __builtin_popcount(node->node_map);
    for (int i = 0; i < nc; i++) {
        markObject(vm, (Obj*)node->nodes[i]);
    }
}

void hamtFree(VM* vm, HamtNode* node) {
    if (node->is_collision) {
        reallocate(vm, node->pairs, 2 * node->cnt * sizeof(Value), 0);
    } else {
        int dc = __builtin_popcount(node->data_map);
        int nc = __builtin_popcount(node->node_map);
        if (node->data) reallocate(vm, node->data, dc * 2 * sizeof(Value), 0);
        if (node->nodes) reallocate(vm, node->nodes, nc * sizeof(HamtNode*), 0);
    }
}
