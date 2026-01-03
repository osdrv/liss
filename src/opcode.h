#ifndef liss_opcode_h
#define liss_opcode_h

typedef enum {
    OP_RETURN,
    OP_CONSTANT,
    OP_POP,
    OP_JUMP,
    OP_JUMP_IF_FALSE,

    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,

    OP_TRUE,
    OP_FALSE,
    OP_NULL,

    OP_NOT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,

    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
} OpCode;

#endif
