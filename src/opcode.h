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

    OP_BAND,
    OP_BOR,
    OP_BXOR,
    OP_BNOT,
    OP_LSHIFT,
    OP_RSHIFT,

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
    OP_PAIR,
    OP_GET_MODULE_GLOBAL,

    OP_DUP,
    OP_IS_ERROR,
    OP_ERROR_MSG,
    OP_IS_PAIR,
    OP_UNPACK_PAIR,
    OP_SLIDE,

    OP_SWAP,
    OP_JUMP_IF_ERR,
} OpCode;

#endif
