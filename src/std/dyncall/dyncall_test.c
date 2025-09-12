#include <assert.h>
#include <hl.h>

extern hl_type hlt_ui8;

void *hl_static_call(void **fun, hl_type *ty, void **args, vdynamic *out);
void *hl_get_wrapper(hl_type *t);

void foo(int a, int b, int c, int d, int e, int f, int g, int h, char ac,
         char bc) {
  assert(a == 1);
  assert(b == 2);
  assert(c == 3);
  assert(d == 4);
  assert(e == 5);
  assert(f == 6);
  assert(g == 7);
  assert(h == 8);
  assert(ac == 53);
  assert(bc == 54);
}

hl_type foo_type = (hl_type){
    .kind = HFUN,
    .fun = &(hl_type_fun){.args =
                              (hl_type* []){
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_i32,
                                  &hlt_ui8,
                                  &hlt_ui8,
                              },
                          .nargs = 10,
                          .ret = &hlt_void},
};

void bar() {}

hl_type bar_type = {.kind = HFUN,
                    .fun = &(hl_type_fun){
                        .args = (hl_type *[]){},
                        .nargs = 0,
                        .ret = &hlt_void,
                    }};

int main() {
  // void *args[0];
  // vdynamic out;
  // void *f = bar;
  // hl_static_call((void**)&f, &bar_type, args, &out);
  vdynamic out = {0};
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;
  int e = 5;
  int f = 6;
  int g = 7;
  int h = 8;
  char ac = 53;
  char bc = 54;
  void *args[] = {
      &a, &b, &c, &d, &e, &f, &g, &h, &ac, &bc,
  };
  void *_f = foo;
  hl_static_call((void **)&_f, &foo_type, args, &out);
  return 0;
}