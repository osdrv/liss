#ifndef liss_object_h
#define liss_object_h

#include <stdint.h>

#include "chunk.h"
#include "value.h"

// Forward declarations for structs used in object definitions.
typedef struct ObjString ObjString;
typedef struct VM VM;

// --- Base Object ---

typedef enum {
    OBJ_FUNCTION,
    OBJ_STRING,
    // Future object types will be added here
    // OBJ_LIST,
    // etc.
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
    Chunk chunk;
    ObjString* name;
} ObjFunction;

// --- String Object ---

typedef struct ObjString {
    Obj obj;  // Must be the first field to allow casting
    int length;
    char* chars;
    uint32_t hash;
} ObjString;

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

// Macros for casting a Value to a specific object type pointer.
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

// --- Function Prototypes ---

ObjFunction* newFunction(VM* vm);

// Allocates an ObjString on the heap and returns a pointer to it.
ObjString* takeString(VM* vm, char* chars, int length);

// Allocates a new ObjString by copying the given characters.
ObjString* copyString(VM* vm, const char* chars, int length);

#endif
