#ifndef liss_common_h
#define liss_common_h

#include <stdio.h>

#define LISS_FILE_EXT ".liss"

// Define a macro for debug logging that can be compiled out.
// To enable, compile with the -DLISS_DEBUG_BUILD flag.
#ifdef LISS_DEBUG_BUILD

#define DEBUG_LOG(format, ...)                               \
    fprintf(stdout, "[DEBUG] %s:%d: " format "\n", __FILE__, \
            __LINE__ __VA_OPT__(, ) __VA_ARGS__)

#define ERROR_LOG(format, ...)                               \
    fprintf(stdout, "[ERROR] %s:%d: " format "\n", __FILE__, \
            __LINE__ __VA_OPT__(, ) __VA_ARGS__)

#else
// In a release build, this macro does nothing.
#define DEBUG_LOG(format, ...) \
    do {                       \
    } while (0)
#define ERROR_LOG(format, ...) \
    do {                       \
    } while (0)
#endif

#define PRINTF(format, ...) fprintf(stdout, format, ##__VA_ARGS__)

char* readLissFile(const char* path);

#endif
