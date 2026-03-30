#ifndef liss_chunk_h
#define liss_chunk_h

#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "opcode.h"
#include "value.h"

// A dynamic array for storing constants.
typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

// A chunk of bytecode. This is the heart of our executable code.
typedef struct {
    int count;
    int capacity;
    uint8_t* code;  // The portable bytecode emitted by the compiler.
    ValueArray constants;
    // We can add a dynamic array for line numbers here later.
} Chunk;

typedef struct VM
    VM;  // Forward declaration of VM for memory management functions.

#define DEBUG_CHUNK(fmt, chunk)          \
    do {                                 \
        char* strc = sprintChunk(chunk); \
        DEBUG_LOG(fmt, strc);            \
        free(strc);                      \
    } while (0)

void initValueArray(VM* vm, ValueArray* array);
void writeValueArray(VM* vm, ValueArray* array, Value value);
void freeValueArray(VM* vm, ValueArray* array);

void initChunk(VM* vm, Chunk* chunk);
void freeChunk(VM* vm, Chunk* chunk);

// Appends a byte to the end of the chunk.
void writeChunk(VM* vm, Chunk* chunk, uint8_t byte);

// Adds a constant to the chunk's constant pool and returns its index.
int addConstant(VM* vm, Chunk* chunk, Value value);

char* sprintChunk(const Chunk* chunk);

#endif
