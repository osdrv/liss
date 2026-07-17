#ifndef liss_object_h
#define liss_object_h

#include <stdint.h>

#include "chunk.h"
#include "regex.h"
#include "table.h"
#include "value.h"

// Forward declarations for structs used in object definitions.
typedef struct ObjString ObjString;
typedef struct ObjModule ObjModule;
typedef struct HamtNode HamtNode;
typedef struct VM VM;

// The signature for all native functions
typedef Value (*NativeFn)(VM* vm, int arg_count, Value* args);

// --- Base Object ---

typedef enum {
    OBJ_FUNCTION,
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_ERROR,
    OBJ_NATIVE,
    OBJ_PAIR,
    OBJ_DICT,
    OBJ_LIST,
    OBJ_MODULE,
    OBJ_FILE,
    OBJ_RE,
    OBJ_HAMT_NODE,
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;  // For garbage collection
    // 'next' field for the garbage collector to create a linked list of all
    // objects.
    struct Obj* next;
};

// --- Function Object ---
typedef struct {
    Obj obj;
    int arity;
    int upvalue_cnt;
    Chunk chunk;
    ObjString* name;
    ObjModule*
        module;  // The module this function belongs to (for error reporting)
    void** loaded_code;
    size_t loaded_code_size;
} ObjFunction;

// --- String Object ---

typedef struct ObjString {
    Obj obj;  // Must be the first field to allow casting
    int length;
    char* chars;
    uint32_t hash;
} ObjString;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;  // Pointer to the variable this upvalue is capturing
                      // (either on the stack or closed)
    Value closed;  // If the upvalue is closed, this holds the value. Otherwise,
                   // it's unused.
    struct ObjUpvalue* next;  // For linking upvalues in a list (e.g., for GC)
} ObjUpvalue;

typedef struct ObjClosure {
    Obj obj;
    ObjFunction* function;  // The function this closure wraps
    ObjUpvalue**
        upvalues;  // Array of pointers to the upvalues this closure captures
    int upvalue_cnt;
} ObjClosure;

typedef struct ObjError {
    Obj obj;
    ObjString* message;
} ObjError;

typedef struct ObjNative {
    Obj obj;
    // Function pointer for the native function implementation. It takes a VM
    // pointer and an array of arguments, and returns a Value.
    ObjString* name;
    int arity;  // -1 for variadic functions
    NativeFn function;
} ObjNative;

typedef struct ObjPair {
    Obj obj;
    Value first;
    Value second;
} ObjPair;

typedef struct ObjDict {
    Obj obj;
    uint32_t count;
    HamtNode* root;
} ObjDict;

typedef struct ObjList {
    Obj obj;
    uint32_t len;
    Value head;
} ObjList;

typedef struct ObjModule {
    Obj obj;
    ObjString* name;
    Table symbols;
    Table imports;
} ObjModule;

typedef struct {
    const char* name;
    int arity;
    NativeFn fn;
} NativeReg;

typedef struct {
    Obj obj;
    FILE* file;
    bool is_closed;
} ObjFile;

typedef struct {
    Obj obj;
    ReProgram* program;
    ObjString* pattern;
} ObjRe;

// --- Helper Functions and Macros ---

// Safely checks if a Value is an object of a given ObjType.
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

// Macro to get the ObjType from a Value
#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// Macros for checking the specific object type of a Value.
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_UPVALUE(value) isObjType(value, OBJ_UPVALUE)
#define IS_ERROR(value) isObjType(value, OBJ_ERROR)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_LIST(value) isObjType(value, OBJ_LIST)
#define IS_PAIR(value) isObjType(value, OBJ_PAIR)
#define IS_DICT(value) isObjType(value, OBJ_DICT)
#define IS_MODULE(value) isObjType(value, OBJ_MODULE)
#define IS_FILE(value) isObjType(value, OBJ_FILE)
#define IS_RE(value) isObjType(value, OBJ_RE)

// Macros for casting a Value to a specific object type pointer.
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_UPVALUE(value) ((ObjUpvalue*)AS_OBJ(value))
#define AS_ERROR(value) ((ObjError*)AS_OBJ(value))
#define AS_NATIVE(value) ((ObjNative*)AS_OBJ(value))
#define AS_LIST(value) ((ObjList*)AS_OBJ(value))
#define AS_PAIR(value) ((ObjPair*)AS_OBJ(value))
#define AS_DICT(value) ((ObjDict*)AS_OBJ(value))
#define AS_MODULE(value) ((ObjModule*)AS_OBJ(value))
#define AS_FILE(value) ((ObjFile*)AS_OBJ(value))
#define AS_RE(value) ((ObjRe*)AS_OBJ(value))

// Helper function to compute the hash of a string.
uint32_t hashString(const char* key, int length);

// --- Function Prototypes ---

ObjFunction* newFunction(VM* vm, ObjModule* module);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
ObjError* newError(VM* vm, const char* message);
ObjNative* newNative(VM* vm, const char* name, int arity, NativeFn function);
ObjList* newList(VM* vm, uint32_t len, Value head);
ObjPair* newPair(VM* vm, Value first, Value second);
ObjDict* newDict(VM* vm);
ObjModule* newModule(VM* vm, const char* name);
ObjFile* newFile(VM* vm, FILE* file);
ObjRe* newRe(VM* vm, ObjString* pattern);

// Allocates an ObjString on the heap and returns a pointer to it.
ObjString* takeString(VM* vm, char* chars, int length);

// Allocates a new ObjString by copying the given characters.
ObjString* copyString(VM* vm, const char* chars, int length);

// Registers a native function with the VM
void defineNative(VM* vm, ObjModule* module, const char* name, int arity,
                  NativeFn function);

void defineNatives(VM* vm, ObjModule* module, const NativeReg* registry);

void defineConst(VM* vm, ObjModule* module, const char* name, Value value);

// A helper to create an error and set it as the current raise value
Value raiseErr(VM* vm, const char* message);

#endif
