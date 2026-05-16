#include "opcode.h"

// opcode to string for debugging
const char* opcodeToString(OpCode opcode) {
    switch (opcode) {
        case OP_RETURN:
            return "OP_RETURN";
        case OP_CONSTANT:
            return "OP_CONSTANT";
        case OP_POP:
            return "OP_POP";
        case OP_JUMP:
            return "OP_JUMP";
        case OP_JUMP_IF_FALSE:
            return "OP_JUMP_IF_FALSE";
        case OP_ADD:
            return "OP_ADD";
        case OP_SUBTRACT:
            return "OP_SUBTRACT";
        case OP_MULTIPLY:
            return "OP_MULTIPLY";
        case OP_DIVIDE:
            return "OP_DIVIDE";
        case OP_NEGATE:
            return "OP_NEGATE";
        case OP_TRUE:
            return "OP_TRUE";
        case OP_FALSE:
            return "OP_FALSE";
        case OP_NULL:
            return "OP_NULL";
        case OP_NOT:
            return "OP_NOT";
        case OP_EQUAL:
            return "OP_EQUAL";
        case OP_GREATER:
            return "OP_GREATER";
        case OP_LESS:
            return "OP_LESS";
        case OP_SET_GLOBAL:
            return "OP_SET_GLOBAL";
        case OP_GET_GLOBAL:
            return "OP_GET_GLOBAL";
        case OP_CALL:
            return "OP_CALL";
        case OP_GET_LOCAL:
            return "OP_GET_LOCAL";
        case OP_SET_LOCAL:
            return "OP_SET_LOCAL";
        case OP_CLOSURE:
            return "OP_CLOSURE";
        case OP_GET_UPVALUE:
            return "OP_GET_UPVALUE";
        case OP_SET_UPVALUE:
            return "OP_SET_UPVALUE";
        case OP_TAIL_CALL:
            return "OP_TAIL_CALL";
        case OP_LIST:
            return "OP_LIST";
        case OP_PAIR:
            return "OP_PAIR";
        case OP_GET_MODULE_GLOBAL:
            return "OP_GET_MODULE_GLOBAL";
        default:
            return "UNKNOWN_OPCODE";
    }
}
