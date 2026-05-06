#ifndef liss_modules_io_h
#define liss_modules_io_h

typedef struct VM VM;
typedef struct ObjModule ObjModule;

void registerIONatives(VM* vm, ObjModule* module);

#endif
