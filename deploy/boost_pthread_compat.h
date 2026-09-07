#pragma once

// Boost 1.69 evaluates PTHREAD_STACK_MIN in a preprocessor #if. Modern glibc
// defines it through sysconf(), which is not a preprocessor integer expression.
// The deployed Linux/amd64 ABI guarantees a 16 KiB minimum stack.
#include <pthread.h>
#if defined(__GLIBC__) && defined(PTHREAD_STACK_MIN)
#undef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif
