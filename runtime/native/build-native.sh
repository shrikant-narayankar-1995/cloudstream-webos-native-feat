#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the runtime's small native helpers with clang + lld (no sysroot needed).
#   docker run --rm -v <project>:/p alpine:edge sh /p/runtime/native/build-native.sh [<arch>...]
# Default: aarch64 and x86_64 (the TV runtime and host runs). armv7 (the legacy runtime,
# docs/legacy-build.md) only when named.
set -eu
apk add -q clang lld file >/dev/null 2>&1
cd /p/runtime/native
mkdir -p out
ARCHES=${*:-aarch64 x86_64}
for arch in $ARCHES; do
    case $arch in
        aarch64|x86_64) t="--target=$arch-linux-musl" ;;
        # Alpine's armv7 ABI: hard-float, VFPv3-D16; ARM (not Thumb) state for relaunch.c's r7 syscalls.
        armv7) t="--target=armv7-linux-musleabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm" ;;
        *) echo "build-native.sh: unsupported arch $arch" >&2; exit 2 ;;
    esac
    # shellcheck disable=SC2086
    clang $t -O2 -fPIC -shared -nostdlib -fuse-ld=lld -Wl,-soname,libEGL.so.1 -Wall -Wextra -Werror \
        -o out/libEGL.so.1.$arch egl_stub.c
    # shellcheck disable=SC2086
    clang $t -Os -static -nostdlib -fno-stack-protector -fno-builtin -ffreestanding -fuse-ld=lld \
        -Wall -Wextra -Werror -o out/relaunch.$arch relaunch.c
    # aarch64 only: ART low-4GB allocator replacement (see low4g.c); libc comes from the runtime musl at load time
    if [ "$arch" = aarch64 ]; then
        clang --target=aarch64-linux-musl -O2 -fPIC -shared -nostdlib -ffreestanding -fuse-ld=lld -Wl,-soname,low4g.so \
            -Wl,--allow-shlib-undefined -Wall -Wextra -Werror -o out/low4g.so.aarch64 low4g.c
    fi
done
file out/*
sha256sum out/*
