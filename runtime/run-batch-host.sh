#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Load-only compatibility pass: every .cs3 in a directory, one fresh dalvikvm process each, in one
# alpine:edge container. Writes <out>/<plugin>.jsonl and .stderr.
#   run-batch-host.sh <plugin dir> <out dir> [op args...]   (default op: load-only)
set -eu
P=$(cd "$(dirname "$0")/.." && pwd)
src=$(cd "$1" && pwd); mkdir -p "$2"; out=$(cd "$2" && pwd); shift 2
[ $# -gt 0 ] || set -- load-only
exec docker run --rm \
    -v "$P/runtime/out:/rt:ro" -v "$P/evidence/cloudstream-app:/cs:ro" -v "$src:/plugins:ro" -v "$out:/out" \
    -v csweb-art-cache:/cache -e ANDROID_DATA=/cache/android-data -e HOME=/cache/home \
    alpine:edge sh -c '
        echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories
        apk add -q art_standalone >/dev/null 2>&1
        mkdir -p "$ANDROID_DATA" "$HOME"
        for f in /plugins/*.cs3; do
            n=$(basename "$f" .cs3)
            rm -rf /tmp/pd && mkdir -p /tmp/pd
            dalvikvm -Xmx512m -cp /rt/csweb-host.dex:/cs/app-prerelease-release.apk org.csweb.loader.Main \
                --plugin "$f" --data /tmp/pd --timeout 60 --op "$@" > "/out/$n.jsonl" 2> "/out/$n.stderr"
            echo "$n rc=$?"
        done' "$@"
