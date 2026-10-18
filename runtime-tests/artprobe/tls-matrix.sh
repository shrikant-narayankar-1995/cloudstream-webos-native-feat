#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Inside alpine:edge after run-alpine.sh's setup: HTTPS through the runtime's TLS stack against several hosts.
set -e
echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories
apk add -q art_standalone >/dev/null 2>&1
export ANDROID_DATA=/tmp/ad
mkdir -p $ANDROID_DATA
set +e
for u in ${URLS:-https://example.com/}; do
  r=$(dalvikvm -cp /out/artprobe-classes.dex ArtProbe "$u" 2>/dev/null | grep '^https=')
  echo "$u $r"
done
