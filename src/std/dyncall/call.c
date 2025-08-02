#ifdef _MSC_VER
#define alloca _alloca
#else
#include <alloca.h>
#endif
#include <assert.h>
#include <hl.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _MSC_VER
#if defined(_M_X64)
enum { CPU_CALL_REGS = 4 };
enum { FPU_CALL_REGS = 4 };
#elif defined(_M_ARM64)
enum { CPU_CALL_REGS = 8 };
enum { FPU_CALL_REGS = 8 };
#elif defined(_M_IX86)
enum { CPU_CALL_REGS = 0 };
enum { FPU_CALL_REGS = 0 };
#endif

struct regs {
#ifndef _M_IX86
  size_t cpu[CPU_CALL_REGS];
  double fpu[FPU_CALL_REGS];
#else
  size_t cpu[1];
  double fpu[1];
#endif
};
#else
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
#endif

enum ret_flags {
  ret_void = 1,
  ret_int = 2,
  ret_float = 3,
  ret_double = 4,
  ret_ptr = 5,
  ret_int64 = 6,
};

void *static_call_impl(void *fn_ptr, void *stack_top, void *stack_bottom,
                       int ret_flags, void *ret_ptr);

enum arg_kind {
  arg_kind_rcpu,
  arg_kind_rfpu,
  arg_kind_stack,
};

union arg_loc {
  int rcpu;
  int rfpu;
  struct {
    int stack_offset;
    int stack_size;
  };
};

struct arg_pos {
  enum arg_kind kind;
  union arg_loc loc;
};

typedef struct {
#if defined(_MSC_VER) && defined(_M_X64)
  int nreg;
#else
  int ncpu;
  int nfpu;
#endif
  int stack_size;
} call_frame_layout;

static size_t compute_stack_offset(call_frame_layout *layout, size_t arg_size,
                                   size_t arg_align) {
  assert(arg_align <= sizeof(size_t));
  assert(sizeof(size_t) % arg_align == 0);
#if defined(__APPLE__) && defined(__aarch64__)
  size_t m = layout->stack_size % arg_align;
  if (m > 0) {
    layout->stack_size += (arg_align - m);
  }
  size_t offset = layout->stack_size;
  layout->stack_size += arg_size;
  return offset;
#else
  size_t m = arg_size % sizeof(size_t);
  if (m > 0) {
    arg_size += (arg_size - m);
  }
  size_t offset = layout->stack_size;
  layout->stack_size += arg_size;
  return offset;
#endif
}

static struct arg_pos push_type(call_frame_layout *layout, hl_type *t) {
  enum arg_kind kind;
  union arg_loc loc;
  switch (t->kind) {
  case HBOOL:
  case HUI8:
  case HUI16:
  case HI32:
  case HI64:
  case HGUID:
#if defined(_MSC_VER) && defined(_M_X64)
    if (layout->nreg < CPU_CALL_REGS) {
      kind = arg_kind_rcpu;
      loc.rcpu = layout->nreg++;
    } else {
      size_t arg_size = hl_type_size(t);
      size_t arg_align = arg_size;
      kind = arg_kind_stack;
      loc.stack_size = arg_size;
      loc.stack_offset = compute_stack_offset(layout, arg_size, arg_align);
    }
#else
    if (layout->ncpu < CPU_CALL_REGS) {
      kind = arg_kind_rcpu;
      loc.rcpu = layout->ncpu++;
    } else {
      size_t arg_size = hl_type_size(t);
      size_t arg_align = arg_size;
      kind = arg_kind_stack;
      loc.stack_size = arg_size;
      loc.stack_offset = compute_stack_offset(layout, arg_size, arg_align);
    }
#endif
    break;
  case HF32:
  case HF64:
#if defined(_MSC_VER) && defined(_M_X64)
    if (layout->nreg < FPU_CALL_REGS) {
      kind = arg_kind_rfpu;
      loc.rfpu = layout->nreg++;
    } else {
      size_t arg_size = hl_type_size(t);
      size_t arg_align = arg_size;
      kind = arg_kind_stack;
      loc.stack_size = arg_size;
      loc.stack_offset = compute_stack_offset(layout, arg_size, arg_align);
    }
#else
    if (layout->nfpu < FPU_CALL_REGS) {
      kind = arg_kind_rfpu;
      loc.rfpu = layout->nfpu++;
    } else {
      size_t arg_size = hl_type_size(t);
      size_t arg_align = arg_size;
      kind = arg_kind_stack;
      loc.stack_size = arg_size;
      loc.stack_offset = compute_stack_offset(layout, arg_size, arg_align);
    }
#endif
    break;
  default:
#if defined(_MSC_VER) && defined(_M_X64)
    if (layout->nreg < CPU_CALL_REGS) {
      kind = arg_kind_rcpu;
      loc.rcpu = layout->nreg++;
    } else {
      size_t arg_size = hl_type_size(t);
      size_t arg_align = arg_size;
      kind = arg_kind_stack;
      loc.stack_size = arg_size;
      loc.stack_offset = compute_stack_offset(layout, arg_size, arg_align);
    }
#else
    if (layout->ncpu < CPU_CALL_REGS) {
      kind = arg_kind_rcpu;
      loc.rcpu = layout->ncpu++;
    } else {
      size_t arg_size = hl_type_size(t);
      size_t arg_align = arg_size;
      kind = arg_kind_stack;
      loc.stack_size = arg_size;
      loc.stack_offset = compute_stack_offset(layout, arg_size, arg_align);
    }
#endif
    break;
  }
  return (struct arg_pos){kind, loc};
}

static void finish_layout(call_frame_layout *layout) {
  size_t m = layout->stack_size % (2 * sizeof(size_t));
  if (m > 0) {
    layout->stack_size += (2 * sizeof(size_t)) - m;
  }
}

static void set_arg(struct regs *regs, char *data, struct arg_pos pos,
                    hl_type *t, void *val_ptr) {
  switch (pos.kind) {
  case arg_kind_rcpu:
    memcpy(&regs->cpu[pos.loc.rcpu], val_ptr, hl_type_size(t));
    break;
  case arg_kind_rfpu:
    memcpy(&regs->fpu[pos.loc.rfpu], val_ptr, hl_type_size(t));
    break;
  case arg_kind_stack:
    memcpy(&data[pos.loc.stack_offset], val_ptr, hl_type_size(t));
    break;
  }
}

static void *get_arg(struct regs *regs, char *stack, struct arg_pos pos) {
  switch (pos.kind) {
  case arg_kind_rcpu:
    return &regs->cpu[pos.loc.rcpu];
  case arg_kind_rfpu:
    return &regs->fpu[pos.loc.rfpu];
  case arg_kind_stack:
    return &stack[pos.loc.stack_offset];
  }
}

void *hl_static_call(void **fun, hl_type *ty, void **args, vdynamic *out) {
  call_frame_layout layout = {0};
  {
    for (int i = 0; i < ty->fun->nargs; i++) {
      hl_type *arg_type = ty->fun->args[i];

      push_type(&layout, arg_type);
    }
  }
  finish_layout(&layout);

  size_t stack_size = layout.stack_size;
  size_t alloc_size = stack_size + sizeof(struct regs);
  size_t *data = alloca(alloc_size);
  memset(data, 0, alloc_size);
  struct regs regs = {0};
  call_frame_layout layout2 = {0};
  for (int i = 0; i < ty->fun->nargs; i++) {
    hl_type *arg_type = ty->fun->args[i];
    struct arg_pos pos = push_type(&layout2, arg_type);
    if (hl_is_ptr(arg_type)) {
      set_arg(&regs, (char *)data, pos, arg_type, &args[i]);
    } else {
      set_arg(&regs, (char *)data, pos, arg_type, args[i]);
    }
  }
  memcpy(data + layout.stack_size, &regs, sizeof(regs));
  enum ret_flags ret_flags;
  switch (ty->fun->ret->kind) {
  case HVOID:
    ret_flags = ret_void;
    break;
  case HBOOL:
  case HUI8:
  case HUI16:
  case HI32:
    ret_flags = ret_int;
    break;
  case HF32:
    ret_flags = ret_float;
    break;
  case HF64:
    ret_flags = ret_double;
    break;
  case HI64:
  case HGUID:
    ret_flags = ret_int64;
    break;
  default:
    ret_flags = ret_ptr;
    break;
  }

  return static_call_impl(*fun, data + layout.stack_size, data, ret_flags,
                          &out->v);
}

void *wrapper_inner(vclosure_wrapper *c, struct regs *regs, size_t *stack,
                    vdynamic *ret) {
  hl_type_fun *fun = c->cl.t->fun;
  void **args = alloca(fun->nargs * sizeof(void *));
  call_frame_layout layout = {0};
  push_type(&layout, &hlt_bytes);
  for (int i = 0; i < fun->nargs; i++) {
    hl_type *t = fun->args[i];
    struct arg_pos pos = push_type(&layout, t);
    if (hl_is_ptr(t)) {
      args[i] = *(void **)get_arg(regs, (char *)stack, pos);
    } else {
      args[i] = (void *)get_arg(regs, (char *)stack, pos);
    }
  }
  switch (fun->ret->kind) {
  case HBOOL:
  case HUI8:
  case HUI16:
  case HI32:
  case HI64:
  case HGUID:
    // Todo HI64 32-bit
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

void *hl_get_wrapper(hl_type *t) { return wrapper_call_impl; }
