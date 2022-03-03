#include <hlmodule.h>

typedef struct jit_ctx {
  int unused;
} jit_ctx;

jit_ctx *hl_jit_alloc() { return NULL; }
void hl_jit_free(jit_ctx *ctx, h_bool can_reset) {}
void hl_jit_reset(jit_ctx *ctx, hl_module *m) {}
void hl_jit_init(jit_ctx *ctx, hl_module *m) {}
int hl_jit_function(jit_ctx *ctx, hl_module *m, hl_function *f) { return 0; }
void *hl_jit_code(jit_ctx *ctx, hl_module *m, int *codesize,
                  hl_debug_infos **debug, hl_module *previous) {
  return NULL;
}
void hl_jit_patch_method(void *old_fun, void **new_fun_table) {}