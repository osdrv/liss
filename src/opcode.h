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
    OP_NEGATE,

    OP_TRUE,
    OP_FALSE,
    OP_NULL,

    OP_NOT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,

    OP_SET_GLOBAL,
    OP_GET_GLOBAL,

    OP_CALL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_TAIL_CALL,

    OP_TRY_START,
    OP_TRY_END,

    OP_LIST,
    OP_GET_MODULE_GLOBAL,
} OpCode;

#endif
