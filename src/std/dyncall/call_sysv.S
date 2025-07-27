#ifdef __x86_64__
#include "call_amd64_sysv.S"
#elif defined(__aarch64__)
#include "call_arm64_sysv.S"
#elif defined(__arm__)
#include "call_armv7_sysv.S"
#elif defined(__i386__)
#include "call_i386_sysv.S"
#else
#error "Unsupported target architecture"
#endif