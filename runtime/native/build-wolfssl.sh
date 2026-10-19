#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Rebuild Alpine's libwolfssl (the wolfssl-jni variant the runtime's JSSE provider uses) with one
# change: -DWOLFSSL_ASN_ALLOW_0_SERIAL. Stock wolfSSL rejects a certificate whose serial number is 0
# when it arrives in a peer's chain, and archive.org still sends the 2004 "Go Daddy Class 2" root
# (serial 0) after a chain that verifies through "Go Daddy Root G2": every handshake failed with
# ASN_PARSE_E (-140). The option only lets such a serial be parsed; signatures, dates, names and trust
# are checked as before (docs/runtime/tls.md).
#   docker run --rm -v <project>:/p alpine:edge sh /p/runtime/native/build-wolfssl.sh
# Output: runtime/native/out/libwolfssl.so.46.<arch> for aarch64 and x86_64. aarch64 is cross-built with
# clang against an Alpine aarch64 sysroot that apk only extracts (no foreign binary runs).
set -eu
VER=5.9.4
SHA512=a37624080dabb789f1f78acce758e9bdbbfad278bc73ab9ff3cdcd24d4aa9633daea1b82002bdf756219cd1d30da8083bdf1dfdc0c00d32565578ae288eb7e69
apk add -q build-base clang llvm lld autoconf automake libtool linux-headers curl file >/dev/null
OUT=/p/runtime/native/out
mkdir -p "$OUT"
cd /tmp
curl -sSfL -o wolfssl.tar.gz "https://github.com/wolfSSL/wolfssl/archive/v$VER-stable/wolfssl-$VER-stable.tar.gz"
echo "$SHA512  wolfssl.tar.gz" | sha512sum -c -

# Alpine's wolfssl APKBUILD (_build + --enable-jni), as of wolfssl-5.9.4-r0.
OPTS="--prefix=/usr --sysconfdir=/etc --enable-shared --disable-static --enable-reproducible-build
  --disable-opensslall --disable-opensslextra --enable-aescbc-length-checks --enable-curve25519 --enable-ed25519
  --enable-ed25519-stream --disable-oldtls --enable-base64encode --enable-tlsx --enable-scrypt --enable-examples
  --enable-keygen --enable-wolfssh --enable-jni"
CFLAGS_COMMON="-Os -fstack-clash-protection -DWOLFSSL_ASN_ALLOW_0_SERIAL"

SYSROOT=/tmp/sysroot-aarch64
mkdir -p "$SYSROOT/etc/apk/keys"
cp /usr/share/apk/keys/aarch64/* "$SYSROOT/etc/apk/keys/"
apk add -q --root "$SYSROOT" --arch aarch64 --initdb --no-scripts --update-cache --keys-dir "$SYSROOT/etc/apk/keys" \
    --repository https://dl-cdn.alpinelinux.org/alpine/edge/main musl-dev linux-headers gcc >/dev/null

for arch in x86_64 aarch64; do
    rm -rf "wolfssl-$VER-stable" && tar -xzf wolfssl.tar.gz && cd "wolfssl-$VER-stable"
    ./autogen.sh >/dev/null 2>&1
    if [ "$arch" = x86_64 ]; then
        # shellcheck disable=SC2086
        CFLAGS="$CFLAGS_COMMON" ./configure $OPTS >/dev/null
    else
        CC="clang --target=aarch64-alpine-linux-musl --sysroot=$SYSROOT -fuse-ld=lld"
        # shellcheck disable=SC2086
        # LD: otherwise libtool takes the sysroot's aarch64 ld, which cannot run here.
        LD=ld.lld CC="$CC" CFLAGS="$CFLAGS_COMMON" ./configure --host=aarch64-alpine-linux-musl $OPTS >/dev/null
    fi
    make -j"$(nproc)" src/libwolfssl.la >/tmp/make-$arch.log 2>&1 || { tail -30 /tmp/make-$arch.log; exit 1; }
    cp src/.libs/libwolfssl.so.46.0.0 "$OUT/libwolfssl.so.46.$arch"
    if [ "$arch" = x86_64 ]; then strip "$OUT/libwolfssl.so.46.$arch"; else llvm-strip "$OUT/libwolfssl.so.46.$arch"; fi
    cd /tmp
done
file "$OUT"/libwolfssl.so.46.*
sha256sum "$OUT"/libwolfssl.so.46.*
