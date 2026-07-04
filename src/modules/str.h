#ifndef liss_modules_str_h
#define liss_modules_str_h

typedef struct VM VM;
typedef struct ObjModule ObjModule;

void registerStrNatives(VM* vm, ObjModule* module);

#endif
