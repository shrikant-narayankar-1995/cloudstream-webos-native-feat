#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build runtime/trust/ca-bundle.pem for the legacy package (docs/legacy-build.md): the same Mozilla CA
# list as runtime/trust/cacerts (make-trust.sh), as a PEM bundle for the libcurl that package bundles
# (tools/build-libcurl-webos.sh compiles in its path inside runtime-armv7/). Not used by the aarch64 build.
#   docker run --rm -v <project>:/p -v <inputs dir>/alpine/x86_64:/apks:ro alpine:edge sh /p/runtime/make-trust-pem.sh
#   (without the /apks mount: the current packages from Alpine edge)
# A PEM bundle carries no time stamps, so the file is the same on every build.
set -eu
if [ -d /apks ]; then
    apk add -q --no-network /apks/libffi-*.apk /apks/libtasn1-*.apk /apks/p11-kit-0*.apk /apks/p11-kit-trust-*.apk \
        /apks/ca-certificates-2*.apk >/dev/null
else
    apk add -q p11-kit-trust ca-certificates >/dev/null
fi
mkdir -p /p/runtime/trust
rm -f /p/runtime/trust/ca-bundle.pem
trust extract --format=pem-bundle --filter=ca-anchors --purpose=server-auth /p/runtime/trust/ca-bundle.pem
echo "trust/ca-bundle.pem: $(grep -c 'BEGIN CERTIFICATE' /p/runtime/trust/ca-bundle.pem) CA certificates"
chown "$(stat -c %u:%g /p)" /p/runtime/trust /p/runtime/trust/ca-bundle.pem
