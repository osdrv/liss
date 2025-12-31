#include "chunk.h"

#include <stdlib.h>

#include "memory.h"  // We will create this new file for memory management helpers.

// --- ValueArray ---

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values =
            GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

// --- Chunk ---

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    // Return the index where the constant was appended.
    return chunk->constants.count - 1;
}

void printChunk(const Chunk* chunk) {
    for (int i = 0; i < chunk->count; i++) {
        printf("%04d ", i);
        uint8_t opcode = chunk->code[i];
        switch (opcode) {
            case OP_CONSTANT: {
                uint8_t const_index = chunk->code[i + 1];
                printf("OP_CONSTANT %d '", const_index);
                printValue(chunk->constants.values[const_index]);
                printf("'\n");
                i++;  // Skip the operand byte
                break;
            }
            case OP_RETURN:
                printf("OP_RETURN\n");
                break;
            case OP_POP:
                printf("OP_POP\n");
                break;
            case OP_JUMP: {
                uint16_t offset =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                printf("OP_JUMP %d\n", offset);
                i += 2;  // Skip the operand bytes
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset =
                    (uint16_t)(chunk->code[i + 1] << 8) | chunk->code[i + 2];
                printf("OP_JUMP_IF_FALSE %d\n", offset);
                i += 2;  // Skip the operand bytes
                break;
            }
            case OP_ADD:
                printf("OP_ADD\n");
                break;
            case OP_SUBTRACT:
                printf("OP_SUBTRACT\n");
                break;
            case OP_MULTIPLY:
                printf("OP_MULTIPLY\n");
                break;
            case OP_DIVIDE:
                printf("OP_DIVIDE\n");
                break;
            case OP_TRUE:
                printf("OP_TRUE\n");
                break;
            case OP_FALSE:
                printf("OP_FALSE\n");
                break;
            case OP_NULL:
                printf("OP_NULL\n");
                break;
            case OP_NOT:
                printf("OP_NOT\n");
                break;
            default:
                printf("Unknown opcode %d\n", opcode);
                break;
        }
    }
}
