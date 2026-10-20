#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the plugin runtime the TV package carries (runtime/dist/runtime-aarch64) from the pinned
# inputs only (runtime/inputs.lock), in the order docs/runtime/tls.md gives, each step in its own
# alpine:edge container as the step's script says. Needs docker, python3 and readelf.
#   tools/build-runtime.sh [<inputs dir>]     default: ~/.cache/csweb-runtime-inputs
#   tools/build-runtime.sh --latest           instead: the current Alpine edge packages and CloudStream
#                                             pre-release (untested; for the weekly upstream check). The
#                                             manifests are rewritten; the notices check reports, not fails.
# Also builds the conformance test plugin (the project's own fixture) that runtime/build.sh tests with.
# No plugin from a repository is downloaded or run.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LATEST=0
if [[ "${1:-}" == --latest ]]; then LATEST=1; shift; fi
INPUTS="${1:-$HOME/.cache/csweb-runtime-inputs}"
ARCH=aarch64
STABLE=/tmp/org.csweb.tv/rt  # where the app links its runtime before starting ART (runtime_launcher.cpp)
DIST="$ROOT/runtime/dist/runtime-$ARCH"
step() { echo "== build-runtime: $*"; }
run() { docker run --rm -v "$ROOT":/p "$@"; }

mkdir -p "$ROOT/evidence/cloudstream-app"
if (( LATEST )); then
    step "inputs: latest CloudStream pre-release"
    for f in app-prerelease-release.apk classes.jar app-sources.jar; do
        curl -fsSL --retry 3 -o "$ROOT/evidence/cloudstream-app/$f" \
            "https://github.com/recloudstream/cloudstream/releases/download/pre-release/$f"
    done
    APKS=() TRUST_APKS=()
else
    step "inputs"
    "$ROOT/tools/fetch-runtime-inputs.sh" "$INPUTS"
    cp "$INPUTS"/cloudstream/* "$ROOT/evidence/cloudstream-app/"
    APKS=(-v "$INPUTS/alpine/$ARCH":/apks:ro) TRUST_APKS=(-v "$INPUTS/alpine/x86_64":/apks:ro)
fi

if [[ ! -f "$ROOT/test-plugins/conformance/out/CswebConformance.cs3" ]]; then
    step "conformance test plugin"
    run eclipse-temurin:17-jdk sh /p/test-plugins/conformance/build.sh
fi

step "root filesystem ($ARCH)"
run "${APKS[@]}" alpine:edge sh /p/runtime/make-rootfs.sh "$ARCH" ${APKS:+/apks}
step "trust store"
run "${TRUST_APKS[@]}" alpine:edge sh /p/runtime/make-trust.sh
step "native helpers"
run alpine:edge sh /p/runtime/native/build-native.sh
step "wolfSSL"
run alpine:edge sh /p/runtime/native/build-wolfssl.sh
step "compat layer and csloader (DEX)"
run alpine:edge sh /p/runtime/build.sh

step "dist"
python3 "$ROOT/runtime/make_dist.py" "$ROOT/runtime/rootfs/$ARCH" "$ARCH" "$DIST"
step "ahead-of-time compilation (for $STABLE)"
run alpine:edge sh /p/runtime/make-aot.sh "$ARCH" "$STABLE"
python3 "$ROOT/runtime/make_dist.py" --manifest "$DIST"

step "licence notices"
if (( LATEST )); then
    python3 "$ROOT/runtime/tools/notices.py" check "$ROOT/runtime/rootfs/$ARCH" "$DIST" || true
else
    python3 "$ROOT/runtime/tools/notices.py" check "$ROOT/runtime/rootfs/$ARCH" "$DIST"
fi
echo "build-runtime: $DIST"
