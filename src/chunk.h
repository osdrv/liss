#ifndef liss_chunk_h
#define liss_chunk_h

#include <stdint.h>
#include "common.h"
#include "value.h"
#include "opcode.h"

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
    uint8_t* code;      // The portable bytecode emitted by the compiler.
    ValueArray constants;
    // We can add a dynamic array for line numbers here later.
} Chunk;


void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);

// Appends a byte to the end of the chunk.
void writeChunk(Chunk* chunk, uint8_t byte);

// Adds a constant to the chunk's constant pool and returns its index.
int addConstant(Chunk* chunk, Value value);

#endif
