#include <stdio.h>
#include "vm.h"
#include "common.h"

int main(int argc, const char* argv[]) {
    DEBUG_LOG("Liss VM\n");
    VM vm;
    initVM(&vm);
    freeVM(&vm);
    return 0;
}
