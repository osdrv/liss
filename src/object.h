#ifndef liss_object_h
#define liss_object_h

#include <stdint.h>

#include "chunk.h"
#include "natives.h"
#include "table.h"
#include "value.h"

// Forward declarations for structs used in object definitions.
typedef struct ObjString ObjString;
typedef struct VM VM;

// --- Base Object ---

typedef enum {
    OBJ_FUNCTION,
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_ERROR,
    OBJ_NATIVE,
    OBJ_PAIR,
    OBJ_LIST,
    OBJ_MODULE,
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

typedef struct {
    Obj obj;
    ObjFunction* function;  // The function this closure wraps
    ObjUpvalue**
        upvalues;  // Array of pointers to the upvalues this closure captures
    int upvalue_cnt;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* message;
} ObjError;

typedef struct {
    Obj obj;
    // Function pointer for the native function implementation. It takes a VM
    // pointer and an array of arguments, and returns a Value.
    ObjString* name;
    int arity;  // -1 for variadic functions
    NativeFn function;
} ObjNative;

typedef struct {
    Obj obj;
    Value first;
    Value second;
} ObjPair;

typedef struct {
    Obj obj;
    uint32_t len;
    Value head;
} ObjList;

typedef struct {
    Obj obj;
    ObjString* name;
    Table imports;
} ObjModule;

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
#define IS_PAIR(value) isObjType(value, OBJ_PAIR)
#define IS_LIST(value) isObjType(value, OBJ_LIST)
#define IS_MODULE(value) isObjType(value, OBJ_MODULE)

// Macros for casting a Value to a specific object type pointer.
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_UPVALUE(value) ((ObjUpvalue*)AS_OBJ(value))
#define AS_ERROR(value) ((ObjError*)AS_OBJ(value))
#define AS_NATIVE(value) ((ObjNative*)AS_OBJ(value))
#define AS_PAIR(value) ((ObjPair*)AS_OBJ(value))
#define AS_LIST(value) ((ObjList*)AS_OBJ(value))
#define AS_MODULE(value) ((ObjModule*)AS_OBJ(value))

// Helper function to compute the hash of a string.
uint32_t hashString(const char* key, int length);

// --- Function Prototypes ---

ObjFunction* newFunction(VM* vm);

ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
ObjError* newError(VM* vm, const char* message);
ObjNative* newNative(VM* vm, const char* name, int arity, NativeFn function);
ObjPair* newPair(VM* vm, Value first, Value second);
ObjList* newList(VM* vm, uint32_t len, Value head);
ObjModule* newModule(VM* vm, const char* name);

// Allocates an ObjString on the heap and returns a pointer to it.
ObjString* takeString(VM* vm, char* chars, int length);

// Allocates a new ObjString by copying the given characters.
ObjString* copyString(VM* vm, const char* chars, int length);

#endif
