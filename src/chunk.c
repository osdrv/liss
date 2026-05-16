#include "chunk.h"

#include <stdint.h>
#include <stdlib.h>

#include "memory.h"  // We will create this new file for memory management helpers.
#include "object.h"
#include "opcode.h"
#include "value.h"
#include "vm.h"

// --- ValueArray ---

void initValueArray(__attribute__((unused)) VM* vm, ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(VM* vm, ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values =
            GROW_ARRAY(Value, vm, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(VM* vm, ValueArray* array) {
    FREE_ARRAY(Value, vm, array->values, array->capacity);
    initValueArray(vm, array);
}

// --- Chunk ---

void initChunk(VM* vm, Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initValueArray(vm, &chunk->constants);
}

void freeChunk(VM* vm, Chunk* chunk) {
    FREE_ARRAY(uint8_t, vm, chunk->code, chunk->capacity);
    freeValueArray(vm, &chunk->constants);
    initChunk(vm, chunk);
}

void writeChunk(VM* vm, Chunk* chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code =
            GROW_ARRAY(uint8_t, vm, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}

int addConstant(VM* vm, Chunk* chunk, Value value) {
    for (int i = 0; i < chunk->constants.count; i++) {
        if (valuesEqual(chunk->constants.values[i], value)) {
            return i;  // Return existing index if constant already exists
        }
    }
    writeValueArray(vm, &chunk->constants, value);
    // Return the index where the constant was appended.
    return chunk->constants.count - 1;
}

char* sprintChunk(const Chunk* chunk) {
    char* buffer = NULL;
    size_t buffer_size = 0;
    size_t offset = 0;

#define APPEND_TO_BUFFER(fmt, ...)                                     \
    do {                                                               \
        int needed = snprintf(NULL, 0, fmt, ##__VA_ARGS__);            \
        if (offset + needed + 1 > buffer_size) {                       \
            buffer_size = (buffer_size == 0) ? 128 : buffer_size * 2;  \
            buffer = realloc(buffer, buffer_size);                     \
        }                                                              \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, \
                           ##__VA_ARGS__);                             \
    } while (0)

    for (int i = 0; i < chunk->count; i++) {
        APPEND_TO_BUFFER("%04d ", i);
        uint8_t opcode = chunk->code[i];
        switch (opcode) {
            case OP_CONSTANT: {
                uint16_t const_index =
                    (uint16_t)(chunk->code[i + 1]) << 8 | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_CONSTANT %d '", const_index);
                char* value_str =
                    sprintValue(chunk->constants.values[const_index]);
                APPEND_TO_BUFFER("%s'\n", value_str);
                free(value_str);
                i += 2;  // Skip the operand byte
                break;
            }
            case OP_RETURN:
                APPEND_TO_BUFFER("OP_RETURN\n");
                break;
            case OP_POP:
                APPEND_TO_BUFFER("OP_POP\n");
                break;
            case OP_JUMP: {
                uint16_t offset =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_JUMP %d\n", offset);
                i += 2;  // Skip the operand bytes
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_JUMP_IF_FALSE %d\n", offset);
                i += 2;  // Skip the operand bytes
                break;
            }
            case OP_ADD:
                APPEND_TO_BUFFER("OP_ADD\n");
                break;
            case OP_SUBTRACT:
                APPEND_TO_BUFFER("OP_SUBTRACT\n");
                break;
            case OP_MULTIPLY:
                APPEND_TO_BUFFER("OP_MULTIPLY\n");
                break;
            case OP_DIVIDE:
                APPEND_TO_BUFFER("OP_DIVIDE\n");
                break;
            case OP_TRUE:
                APPEND_TO_BUFFER("OP_TRUE\n");
                break;
            case OP_FALSE:
                APPEND_TO_BUFFER("OP_FALSE\n");
                break;
            case OP_NULL:
                APPEND_TO_BUFFER("OP_NULL\n");
                break;
            case OP_NOT:
                APPEND_TO_BUFFER("OP_NOT\n");
                break;
            case OP_SET_GLOBAL:
                uint16_t set_index =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_SET_GLOBAL %d\n", set_index);
                i += 2;  // Skip the operand bytes
                break;
            case OP_GET_GLOBAL:
                uint16_t get_index =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_GET_GLOBAL %d\n", get_index);
                i += 2;  // Skip the operand bytes
                break;
            case OP_EQUAL:
                APPEND_TO_BUFFER("OP_EQUAL\n");
                break;
            case OP_GREATER:
                APPEND_TO_BUFFER("OP_GREATER\n");
                break;
            case OP_LESS:
                APPEND_TO_BUFFER("OP_LESS\n");
                break;
            case OP_CALL:
                uint8_t arg_count = chunk->code[i + 1];
                i++;
                APPEND_TO_BUFFER("OP_CALL %d\n", arg_count);
                break;
            case OP_GET_LOCAL:
                APPEND_TO_BUFFER("OP_GET_LOCAL %d\n", chunk->code[i + 1]);
                i++;
                break;
            case OP_SET_LOCAL:
                APPEND_TO_BUFFER("OP_SET_LOCAL %d\n", chunk->code[i + 1]);
                i++;
                break;
            case OP_CLOSURE: {
                uint16_t const_index =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_CLOSURE %d\n", const_index);
                i += 2;  // Skip the operand bytes
                ObjFunction* fn =
                    AS_FUNCTION(chunk->constants.values[const_index]);
                for (int j = 0; j < fn->upvalue_cnt; j++) {
                    uint8_t is_local = chunk->code[i + 1];
                    uint8_t index = chunk->code[i + 2];
                    APPEND_TO_BUFFER("    Upvalue %d: is_local=%d, index=%d\n",
                                     j, is_local, index);
                    i += 2;  // Skip the upvalue operand bytes
                }
                break;
                case OP_GET_UPVALUE:
                    APPEND_TO_BUFFER("OP_GET_UPVALUE %d\n", chunk->code[i + 1]);
                    i++;
                    break;
                case OP_SET_UPVALUE:
                    APPEND_TO_BUFFER("OP_SET_UPVALUE %d\n", chunk->code[i + 1]);
                    i++;
                    break;
            }
            case OP_TAIL_CALL:
                uint8_t tail_arg_count = chunk->code[i + 1];
                i++;
                APPEND_TO_BUFFER("OP_TAIL_CALL %d\n", tail_arg_count);
                break;
            case OP_TRY_START: {
                uint16_t handler_offset =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                APPEND_TO_BUFFER("OP_TRY_START %d\n", handler_offset);
                i += 2;  // Skip the operand bytes
                break;
            }
            case OP_TRY_END: {
                APPEND_TO_BUFFER("OP_TRY_END\n");
                break;
            }
            case OP_LIST: {
                uint8_t item_count = chunk->code[i + 1];
                i++;
                APPEND_TO_BUFFER("OP_LIST %d\n", item_count);
                break;
            }
            case OP_GET_MODULE_GLOBAL: {
                uint16_t module_const_ix =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                uint16_t const_ix =
                    (uint16_t)(chunk->code[i + 3] << 8) | chunk->code[i + 4];
                APPEND_TO_BUFFER("OP_GET_MODULE_GLOBAL %d %d\n",
                                 module_const_ix, const_ix);
                i += 4;  // Skip the operand bytes
                break;
            }
            case OP_PAIR: {
                APPEND_TO_BUFFER("OP_PAIR\n");
                break;
            }
            case OP_NEGATE: {
                APPEND_TO_BUFFER("OP_NEGATE\n");
                break;
            }
            default:
                APPEND_TO_BUFFER("Unknown opcode %d\n", opcode);
                break;
        }
    }
#undef APPEND_TO_BUFFER
    buffer[offset] = '\0';  // Null-terminate the string
    return buffer;
}
