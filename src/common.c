#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"


char* readLissFile(const char* path) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s%s", path, LISS_FILE_EXT);
    FILE* file = fopen(buf, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* source = (char*)malloc(length + 1);
    if (source == NULL) {
        fclose(file);
        return NULL;
    }
    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);
    return source;
}
