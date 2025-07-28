#include <alloca.h>
#include <assert.h>
#include <hl.h>
#include <stdlib.h>

#ifdef __x86_64__
enum { CPU_CALL_REGS = 6 };
enum { FPU_CALL_REGS = 8 };
#elif defined(__aarch64__)
enum { CPU_CALL_REGS = 8 };
enum { FPU_CALL_REGS = 8 };
#elif defined(__i386__)
enum { CPU_CALL_REGS = 0 };
enum { FPU_CALL_REGS = 0 };
#endif

struct regs {
  size_t cpu[CPU_CALL_REGS];
  double fpu[FPU_CALL_REGS];
};

enum ret_flags {
  ret_void = 1,
  ret_int = 2,
  ret_float = 3,
  ret_ptr = 4,
};

void *static_call_impl(void *fn_ptr, void *stack_begin, void *stack_end,
                       int ret_flags, void **ret_ptr);

void *hlc_static_call(void *fun, hl_type *ty, void **args, vdynamic *out) {
  size_t stack_count = 0;
  {
    int ncpu = 0;
    int nfpu = 0;
    for (int i = 0; i < ty->fun->nargs; i++) {
      hl_type *arg_type = ty->fun->args[i];

      switch (arg_type->kind) {
      case HBOOL:
      case HUI8:
      case HUI16:
      case HI32:
      case HI64:
      case HGUID:
        if (ncpu < CPU_CALL_REGS) {
          ncpu++;
        } else {
          stack_count++;
        }
      case HF32:
      case HF64:
        if (nfpu < FPU_CALL_REGS) {
          nfpu++;
        } else {
          stack_count++;
        }
      default:
        if (ncpu < CPU_CALL_REGS) {
          ncpu++;
        } else {
          stack_count++;
        }
      }
    }
  }
  if (stack_count % 2 != 0) {
    stack_count++;
  }
  size_t stack_size = sizeof(size_t) * stack_count;
  size_t alloc_size = stack_size + sizeof(struct regs);
  size_t *data = alloca(alloc_size);
  memset(data, 0, alloc_size);
  struct regs regs = {0};
  int ncpu = 0;
  int nfpu = 0;
  int nstack = 0;
  for (int i = 0; i < ty->fun->nargs; i++) {
    hl_type *arg_type = ty->fun->args[i];

    switch (arg_type->kind) {
    case HBOOL:
    case HUI8:
    case HUI16:
    case HI32:
    case HI64:
    case HGUID:
      if (ncpu < CPU_CALL_REGS) {
        regs.cpu[ncpu++] = *(size_t *)args[i];
      } else {
        data[nstack++] = *(size_t *)args[i];
      }
      break;
    case HF32:
    case HF64:
      if (nfpu < FPU_CALL_REGS) {
        regs.fpu[nfpu++] = *(double *)args[i];
      } else {
        data[nstack++] = *(size_t *)args[i];
      }
      break;
    default:
      if (nfpu < CPU_CALL_REGS) {
        regs.cpu[ncpu++] = (size_t)args[i];
      } else {
        data[nstack++] = (size_t)args[i];
      }
      break;
    }
  }
  memcpy(data + stack_count, &regs, sizeof(regs));
  enum ret_flags ret_flags;
  switch (ty->fun->ret->kind) {
  case HVOID:
        ret_flags = ret_void;
        break;
  case HBOOL:
  case HUI8:
  case HUI16:
  case HI32:
  case HI64:
  case HGUID:
    ret_flags = ret_int;
    break;
  case HF32:
  case HF64:
    ret_flags = ret_float;
    break;
  default:
    ret_flags = ret_ptr;
    break;
  }

  return static_call_impl(fun, data + stack_count, data, ret_flags, &out->v.ptr);
}

void *wrapper_inner(vclosure_wrapper *c, struct regs *regs, size_t *stack,
                    vdynamic *ret) {
  hl_type_fun *fun = c->cl.t->fun;
  void **args = alloca(fun->nargs * sizeof(void *));
  int ncpu = 1;
  int nfpu = 0;
  int nstack = 0;
  for (int i = 0; i < fun->nargs; i++) {
    if (hl_is_dynamic(fun->args[i])) {
      if (ncpu < CPU_CALL_REGS) {
        args[i] = (void *)regs->cpu[ncpu++];
      } else {
        args[i] = (void *)stack[nstack++];
      }
    } else if (fun->args[i]->kind == HF32 || fun->args[i]->kind == HF64) {
      if (nfpu < FPU_CALL_REGS) {
        args[i] = &regs->fpu[nfpu++];
      } else {
        args[i] = (double *)&stack[nstack++];
      }
    } else {
      if (ncpu < FPU_CALL_REGS) {
        args[i] = &regs->cpu[ncpu++];
      } else {
        args[i] = &stack[nstack++];
      }
    }
  }

  switch (fun->ret->kind) {
  case HBOOL:
  case HUI8:
  case HUI16:
  case HI32:
  case HI64:
  case HGUID:
    hl_wrapper_call(c, args, ret);
    return ret->v.ptr;
  case HF32:
  case HF64:
    hl_wrapper_call(c, args, ret);
    return &ret->v;
  default:
    return hl_wrapper_call(c, args, NULL);
  }
}

void *wrapper_call_impl();

void *hlc_get_wrapper(hl_type *t) { return wrapper_call_impl; }
