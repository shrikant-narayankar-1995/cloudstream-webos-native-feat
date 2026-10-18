#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Install Alpine edge art_standalone (+ dependency closure) for one architecture into an empty root,
# with apk's own signature checks and WITHOUT running package scripts or any foreign binary.
#   docker run --rm -v <project>:/p alpine:edge sh /p/runtime/make-rootfs.sh <aarch64|x86_64|armv7>
# Output: /p/runtime/rootfs/<arch>/ and rootfs-<arch>.manifest (package list with versions and checksums).
set -eu
ARCH=$1
ROOT=/p/runtime/rootfs/$ARCH
rm -rf "$ROOT"; mkdir -p "$ROOT/etc/apk"
mkdir -p "$ROOT/etc/apk/keys"
cp /usr/share/apk/keys/"$ARCH"/* "$ROOT/etc/apk/keys/"   # signing keys for the target architecture
printf '%s\n' https://dl-cdn.alpinelinux.org/alpine/edge/main https://dl-cdn.alpinelinux.org/alpine/edge/community \
    https://dl-cdn.alpinelinux.org/alpine/edge/testing > "$ROOT/etc/apk/repositories"
apk add -q --root "$ROOT" --arch "$ARCH" --initdb --no-scripts --update-cache --keys-dir "$ROOT/etc/apk/keys" \
    --repositories-file "$ROOT/etc/apk/repositories" art_standalone
apk info --root "$ROOT" -v 2>/dev/null | sort > "/p/runtime/rootfs-$ARCH.manifest"
wc -l < "/p/runtime/rootfs-$ARCH.manifest"
du -sh "$ROOT"
