#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the runtime's two small native helpers for aarch64 and x86_64 with clang + lld (no sysroot needed).
#   docker run --rm -v <project>:/p alpine:edge sh /p/runtime/native/build-native.sh
set -eu
apk add -q clang lld file >/dev/null 2>&1
cd /p/runtime/native
mkdir -p out
for arch in aarch64 x86_64; do
    t=$arch-linux-musl
    clang --target=$t -O2 -fPIC -shared -nostdlib -fuse-ld=lld -Wl,-soname,libEGL.so.1 -Wall -Wextra -Werror \
        -o out/libEGL.so.1.$arch egl_stub.c
    clang --target=$t -Os -static -nostdlib -fno-stack-protector -fno-builtin -ffreestanding -fuse-ld=lld \
        -Wall -Wextra -Werror -o out/relaunch.$arch relaunch.c
done
# aarch64 only: ART low-4GB allocator replacement (see low4g.c); libc comes from the runtime musl at load time
clang --target=aarch64-linux-musl -O2 -fPIC -shared -nostdlib -ffreestanding -fuse-ld=lld -Wl,-soname,low4g.so \
    -Wl,--allow-shlib-undefined -Wall -Wextra -Werror -o out/low4g.so.aarch64 low4g.c
file out/*
sha256sum out/*
