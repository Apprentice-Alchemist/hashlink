#include <hl.h>

#ifdef HL_AARCH64
#include "jit_aarch64.c"
#elif defined(HL_X86)
#include "jit_x86_64.c"
#else
#error "Unsupported architecture, please use HL/C or contribute a new JIT implementation."
#endif