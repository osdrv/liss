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
        default:
            return "UNKNOWN_OPCODE";
    }
}
