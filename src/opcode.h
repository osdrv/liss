#ifndef liss_opcode_h
#define liss_opcode_h

typedef enum {
    OP_RETURN,
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    // Future opcodes (OP_ADD, OP_JUMP, etc.) will be added here.
} OpCode;

#endif
