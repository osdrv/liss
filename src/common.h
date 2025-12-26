#ifndef liss_common_h
#define liss_common_h

#include <stdio.h>

// Define a macro for debug logging that can be compiled out.
// To enable, compile with the -DLISS_DEBUG_BUILD flag.
#ifdef LISS_DEBUG_BUILD
#define DEBUG_LOG(format, ...)                                         \
    fprintf(stderr, "[DEBUG] %s:%d: " format "\n", __FILE__, __LINE__, \
            ##__VA_ARGS__)
#else
// In a release build, this macro does nothing.
#define DEBUG_LOG(format, ...) \
    do {                       \
    } while (0)
#endif

#endif
