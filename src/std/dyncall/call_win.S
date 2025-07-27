#ifdef __x86_64__
#include "call_amd64_win64.S"
#elif defined(__i386__)
#include "call_i386_sysv.S"
#else
#error "Unknown processor"
#endif