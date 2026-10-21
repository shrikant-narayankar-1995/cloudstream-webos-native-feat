#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Rebuild Alpine's pinned ART recipe against wolfSSL 5.9.4 for the legacy runtime.
# Run in an armv7 Alpine container (binfmt/qemu-arm on an x86_64 host):
#   docker run --rm --platform linux/arm/v7 -v <project>:/p alpine:edge \
#       sh /p/runtime/build-art-armv7.sh
# Outputs runtime/native/out/art-armv7/{art_standalone-*.apk,keys/,APKBUILD,PROVENANCE}.
# The temporary signing PRIVATE key stays in the disposable container.
set -eu
[ "$(apk --print-arch)" = armv7 ] || { echo 'build-art-armv7: needs an armv7 container' >&2; exit 2; }
APORTS=8ac8f5cb2e5c2206a93de5498f6368190625b3cc
OUT=/p/runtime/native/out/art-armv7
WORK=/tmp/aports/testing/art_standalone
mkdir -p "$WORK"
printf '%s\n' https://dl-cdn.alpinelinux.org/alpine/edge/main \
    https://dl-cdn.alpinelinux.org/alpine/edge/community \
    https://dl-cdn.alpinelinux.org/alpine/edge/testing > /etc/apk/repositories
apk add --no-cache alpine-sdk curl wolfssl-jni-dev=5.9.4-r0
cd "$WORK"
curl -fSL --retry 3 -o APKBUILD \
    "https://raw.githubusercontent.com/alpinelinux/aports/$APORTS/testing/art_standalone/APKBUILD"
echo '839f8e4dd93453e423fe52777fc7e10a7720d2985449fc15e8e3b70cd315f972  APKBUILD' | sha256sum -c -
# Preserve the upstream recipe commit in the package database for licence collection.
export ABUILD_LAST_COMMIT="$APORTS" SOURCE_DATE_EPOCH=1792147193
export PACKAGER='CSWeb TV Legacy <legacy@csweb.invalid>'
export SUDO_APK=apk
export ABUILD_USERDIR=/tmp/art-signing
export ABUILD_USERCONF="$ABUILD_USERDIR/abuild.conf"
abuild-keygen -a -n
cp "$ABUILD_USERDIR"/*.pub /etc/apk/keys/
# -F: this is a disposable container; avoid fakeroot under qemu-user.
# -r: abuild resolves build dependencies and verifies the source archive's SHA-512.
abuild -F -r -K -P /tmp/packages
APK=/tmp/packages/testing/armv7/art_standalone-0_git20251009-r3.apk
test -s "$APK"
# Resolve the built APK against current edge, without executing package scripts.
apk add --root /tmp/check-art --initdb --no-scripts --keys-dir /etc/apk/keys \
    --repositories-file /etc/apk/repositories "$APK"
scanelf --recursive --needed --nobanner /tmp/check-art/usr > /tmp/art-needed
grep -q 'libwolfssl.so.46' /tmp/art-needed
if grep -q 'libwolfssl.so.45' /tmp/art-needed; then
    echo 'build-art-armv7: rebuilt ART still needs wolfSSL ABI 45' >&2
    exit 1
fi
# Publish only after the build and dependency checks have succeeded.
rm -rf "$OUT"
mkdir -p "$OUT/keys"
cp "$APK" "$OUT/"
cp "$ABUILD_USERDIR"/*.pub "$OUT/keys/"
cp APKBUILD "$OUT/"
printf 'aports=%s\nwolfssl=5.9.4-r0\n' "$APORTS" > "$OUT/PROVENANCE"
apk info -v | sort >> "$OUT/PROVENANCE"
sha256sum "$OUT"/*.apk >> "$OUT/PROVENANCE"
chown -R "$(stat -c %u:%g /p)" "$OUT"
