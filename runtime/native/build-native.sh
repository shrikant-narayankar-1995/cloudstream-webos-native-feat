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
    if [ "$arch" = armv7 ]; then
        # Headers only: the JDK does not become a runtime dependency.
        apk add -q openjdk17-jdk >/dev/null 2>&1
        math_sysroot=/tmp/csweb-legacy-math-sysroot
        mkdir -p "$math_sysroot/etc/apk/keys"
        cp /usr/share/apk/keys/armv7/* "$math_sysroot/etc/apk/keys/"
        apk add -q --root "$math_sysroot" --arch armv7 --initdb --no-scripts --update-cache \
            --keys-dir "$math_sysroot/etc/apk/keys" \
            --repository https://dl-cdn.alpinelinux.org/alpine/edge/main musl-dev >/dev/null
        # shellcheck disable=SC2086
        clang $t -O2 -fno-builtin -fPIC -shared -nostdlib -fuse-ld=lld \
            --sysroot="$math_sysroot" \
            -I/usr/lib/jvm/java-17-openjdk/include -I/usr/lib/jvm/java-17-openjdk/include/linux \
            -Wl,-soname,libcsweb_legacy_math.so -Wl,--allow-shlib-undefined \
            -Wall -Wextra -Werror -o out/libcsweb_legacy_math.so.armv7 legacy_math.c
    fi
done
case " $ARCHES " in
    *" armv7 "*)
        # The legacy ART APK/cache lives in out/art-armv7/. Report only regular helper files.
        set --
        for output in out/*; do
            [ ! -f "$output" ] || set -- "$@" "$output"
        done
        file "$@"
        sha256sum "$@"
        ;;
    *)
        file out/*
        sha256sum out/*
        ;;
esac
