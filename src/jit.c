#include <hl.h>

#ifdef HL_AARCH64
#include <jit_aarch64.c>
#elif defined(HL_X86_64)
#include <jit_x86_64.c>
#else
#error "JIT is not supported for this target (supported targets: x86,x64 and Aarch64)"
#endif