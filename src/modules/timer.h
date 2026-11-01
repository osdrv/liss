#ifndef liss_modules_timer_h
#define liss_modules_timer_h

typedef struct VM VM;
typedef struct ObjModule ObjModule;

void registerTimerNatives(VM* vm, ObjModule* module);

#endif
