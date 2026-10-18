#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Run one unmodified plugin through csloader on Alpine's art_standalone (host, x86_64 container).
#   run-host.sh <plugin.cs3> <op> [loader args...]     e.g. run-host.sh plugins/X.cs3 search --query nasa
# Inputs are mounted read-only; ART caches and plugin data go to a named volume (csweb-art-cache).
set -eu
P=$(cd "$(dirname "$0")/.." && pwd)
plugin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); op=$2; shift 2
exec docker run --rm \
    -v "$P/runtime/out:/rt:ro" -v "$P/evidence/cloudstream-app:/cs:ro" -v "$(dirname "$plugin"):/plugins:ro" \
    -v csweb-art-cache:/cache -e ANDROID_DATA=/cache/android-data -e HOME=/cache/home \
    alpine:edge sh -c '
        echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories
        apk add -q art_standalone >/dev/null 2>&1
        mkdir -p "$ANDROID_DATA" "$HOME" /cache/plugin-data
        exec dalvikvm -Xmx512m -cp /rt/csweb-host.dex:/cs/app-prerelease-release.apk org.csweb.loader.Main \
            --plugin "/plugins/$0" --data /cache/plugin-data --op "$@"' "$(basename "$plugin")" "$op" "$@"
