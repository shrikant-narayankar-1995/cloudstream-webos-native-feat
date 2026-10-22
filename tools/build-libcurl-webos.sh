#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the libcurl the legacy package bundles (docs/legacy-build.md). Firmware older than webOS 5.3.1
# has no libcurl.so.4, which the kit dlopens for all HTTP (net/src/curl_api.cpp); the legacy package
# ships one in lib/ (found through RPATH $ORIGIN/lib, as the kit's dlopen runs from the executable).
#   tools/build-libcurl-webos.sh PREFIX CA_BUNDLE
#     PREFIX      output: PREFIX/lib/libcurl.so.4 (real file, SONAME libcurl.so.4), PREFIX/include, and
#                 PREFIX/licenses-libcurl/ (curl's and mbedTLS's licences, SOURCES.txt: what was built
#                 from which tarballs), shipped in the package as licenses-libcurl/
#     CA_BUNDLE   absolute path on the TV of the PEM bundle curl verifies against by default (the kit
#                 sets no CURLOPT_CAINFO): the legacy runtime's trust/ca-bundle.pem in the installed app
# Built with the webOS NDK (WEBOS_NDK, as the kit's tools/build-ffmpeg.sh), soft-float EABI5 against the
# NDK's glibc 2.12, so it loads on old firmware. TLS: mbedTLS, linked in statically; zlib from the TV
# (libz.so.1, present on every firmware the kit inventories). HTTP/HTTPS only.
# Not used by the main (aarch64) package, which takes libcurl from the TV.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/external/webos-native-app-kit/tools/common.sh"

CURL_VERSION=8.23.0
CURL_SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
CURL_URL="https://github.com/curl/curl/releases/download/curl-${CURL_VERSION//./_}/curl-$CURL_VERSION.tar.xz"
MBEDTLS_VERSION=3.6.7
MBEDTLS_SHA256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
MBEDTLS_URL="https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VERSION/mbedtls-$MBEDTLS_VERSION.tar.bz2"

PREFIX="${1:?usage: build-libcurl-webos.sh PREFIX CA_BUNDLE}"
CA_BUNDLE="${2:?usage: build-libcurl-webos.sh PREFIX CA_BUNDLE}"
[[ $CA_BUNDLE == /* ]] || die "CA_BUNDLE must be an absolute path on the TV"
CACHE="${WNK_BUILD_CACHE:-$HOME/.cache/wnk}/libcurl"
mkdir -p "$CACHE" "$PREFIX"
PREFIX=$(cd "$PREFIX" && pwd)

resolve_ndk
TOOLCHAIN_FILE="$WEBOS_NDK/share/buildroot/toolchainfile.cmake"
CC_ID="$("$WEBOS_NDK/bin/arm-webos-linux-gnueabi-gcc" --version | head -1)"
# -tno-glibc-polyfill: as tools/build-ffmpeg.sh, keep the NDK's polyfill archive out of what we ship.
LDFLAGS_ARM="-tno-glibc-polyfill -Wl,--build-id=sha1"
CFLAGS_ARM="-Os -fPIC -ffunction-sections -fdata-sections"

KEY=$(printf '%s|%s|%s|%s|%s|%s' "$CURL_VERSION" "$CURL_SHA256" "$MBEDTLS_VERSION" "$CA_BUNDLE" "$CC_ID" \
    "$(sha256sum "${BASH_SOURCE[0]}" "$ROOT/tools/patch-curl-diagnostics.py" | sha256sum | cut -c1-16)" | sha256sum | cut -c1-16)
STAMP="$PREFIX/.csweb-libcurl"
if [[ -f "$STAMP" && "$(cat "$STAMP")" == "$KEY" && -f "$PREFIX/lib/libcurl.so.4" ]]; then
    note "libcurl $CURL_VERSION up to date in $PREFIX"
    exit 0
fi

fetch() {  # url sha256 file
    local out="$CACHE/$3"
    if [[ -f "$out" && "$(sha256sum "$out" | cut -d' ' -f1)" == "$2" ]]; then return 0; fi
    need curl "to download $3"
    note "downloading $3"
    curl -fsSL --retry 3 -o "$out.part" "$1"
    [[ "$(sha256sum "$out.part" | cut -d' ' -f1)" == "$2" ]] || die "$3: SHA-256 mismatch (expected $2)"
    mv "$out.part" "$out"
}
fetch "$MBEDTLS_URL" "$MBEDTLS_SHA256" "mbedtls-$MBEDTLS_VERSION.tar.bz2"
fetch "$CURL_URL" "$CURL_SHA256" "curl-$CURL_VERSION.tar.xz"

WORK="$CACHE/work-$KEY"
rm -rf "$WORK"
mkdir -p "$WORK"
tar -xjf "$CACHE/mbedtls-$MBEDTLS_VERSION.tar.bz2" -C "$WORK"
tar -xJf "$CACHE/curl-$CURL_VERSION.tar.xz" -C "$WORK"
# Preserve detailed initialization failures for the legacy app's events.log.
need python3 "to instrument legacy curl initialization"
python3 "$ROOT/tools/patch-curl-diagnostics.py" "$WORK/curl-$CURL_VERSION" "$WORK/mbedtls-$MBEDTLS_VERSION"
DEPS="$WORK/deps"

note "building mbedTLS $MBEDTLS_VERSION (static, PIC)"
cmake -S "$WORK/mbedtls-$MBEDTLS_VERSION" -B "$WORK/mbedtls-build" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX="$DEPS" -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_C_FLAGS="$CFLAGS_ARM" -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF \
    -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON -DMBEDTLS_FATAL_WARNINGS=OFF \
    > "$WORK/mbedtls-configure.log" 2>&1 || { tail -30 "$WORK/mbedtls-configure.log" >&2; die "mbedTLS configure failed"; }
cmake --build "$WORK/mbedtls-build" -j"$(nproc)" --target install > "$WORK/mbedtls-build.log" 2>&1 ||
    { tail -30 "$WORK/mbedtls-build.log" >&2; die "mbedTLS build failed"; }

note "building curl $CURL_VERSION (shared, HTTP/HTTPS, mbedTLS)"
cmake -S "$WORK/curl-$CURL_VERSION" -B "$WORK/curl-build" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=/csweb -DCMAKE_C_FLAGS="$CFLAGS_ARM" \
    -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS_ARM -Wl,--gc-sections -Wl,--exclude-libs,ALL" \
    -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF -DBUILD_CURL_EXE=OFF -DBUILD_LIBCURL_DOCS=OFF \
    -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
    -DHTTP_ONLY=ON -DCURL_DISABLE_WEBSOCKETS=OFF -DENABLE_UNIX_SOCKETS=OFF -DCURL_ENABLE_SSL=ON \
    -DCURL_USE_MBEDTLS=ON -DCURL_USE_OPENSSL=OFF -DMBEDTLS_INCLUDE_DIR="$DEPS/include" \
    -DMBEDTLS_LIBRARY="$DEPS/lib/libmbedtls.a" -DMBEDX509_LIBRARY="$DEPS/lib/libmbedx509.a" \
    -DMBEDCRYPTO_LIBRARY="$DEPS/lib/libmbedcrypto.a" \
    -DCURL_ZLIB=ON -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF -DUSE_LIBIDN2=OFF \
    -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_USE_LIBSSH=OFF -DCURL_USE_GSSAPI=OFF \
    -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DENABLE_ARES=OFF -DENABLE_THREADED_RESOLVER=ON \
    -DCURL_CA_BUNDLE="$CA_BUNDLE" -DCURL_CA_PATH=none -DCURL_CA_FALLBACK=OFF \
    > "$WORK/curl-configure.log" 2>&1 || { tail -40 "$WORK/curl-configure.log" >&2; die "curl configure failed"; }
cmake --build "$WORK/curl-build" -j"$(nproc)" > "$WORK/curl-build.log" 2>&1 ||
    { tail -30 "$WORK/curl-build.log" >&2; die "curl build failed"; }
rm -rf "$WORK/destdir"
DESTDIR="$WORK/destdir" cmake --install "$WORK/curl-build" > "$WORK/curl-install.log" 2>&1

rm -rf "$PREFIX/lib" "$PREFIX/include" "$PREFIX/licenses-libcurl"
mkdir -p "$PREFIX/lib" "$PREFIX/licenses-libcurl"
cp -R "$WORK/destdir/csweb/include" "$PREFIX/include"
cp "$(readlink -f "$WORK/destdir/csweb/lib/libcurl.so.4")" "$PREFIX/lib/libcurl.so.4"
{
    echo "lib/libcurl.so.4: curl $CURL_VERSION (curl licence), with mbedTLS $MBEDTLS_VERSION (Apache-2.0) linked in,"
    echo "built by tools/build-libcurl-webos.sh of this repository from:"
    echo "    $CURL_URL"
    echo "    sha256 $CURL_SHA256"
    echo "    $MBEDTLS_URL"
    echo "    sha256 $MBEDTLS_SHA256"
    echo "Local modifications: initialization diagnostics, ARM EABI getrandom support for old NDK headers,"
    echo "and /dev/urandom fallback (tools/patch-curl-diagnostics.py)."
    echo "Default CA bundle: $CA_BUNDLE"
} > "$PREFIX/licenses-libcurl/SOURCES.txt"
cp "$WORK/curl-$CURL_VERSION/COPYING" "$PREFIX/licenses-libcurl/curl-COPYING.txt"
cp "$WORK/mbedtls-$MBEDTLS_VERSION/LICENSE" "$PREFIX/licenses-libcurl/mbedtls-Apache-2.0.txt"
printf '%s' "$KEY" > "$STAMP"
rm -rf "$WORK"
note "libcurl $CURL_VERSION installed in $PREFIX"
