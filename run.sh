#!/bin/sh
rm -f src/jit.o src/main.o
CC="clang --target=aarch64-linux-gnu --sysroot=/usr/aarch64-linux-gnu" EXTRA_LFLAGS="-fuse-ld=lld" make libhl hl DEBUG=1
qemu-aarch64 -g 8080 -L /usr/aarch64-linux-gnu hl out.hl &
aarch64-linux-gnu-gdb hl -ex "set sysroot /usr/aarch64-linux-gnu" -ex "target remote 127.0.0.1:8080" 
# lldb --arch aarch64 -O "gdb-remote 8080" -O "image add hl" -O "image add libhl.so" -O "image add /usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1"