#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Inside an alpine:edge container: install art_standalone + JDK, compile ArtProbe to DEX with D8, run it.
# Usage: docker run --rm -v <this dir>:/src:ro -v <out>:/out alpine:edge sh /src/run-alpine.sh
set -e
echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories
apk add -q art_standalone openjdk17-jdk curl >/dev/null 2>&1
R8=8.5.35
curl -sfL -o /tmp/r8.jar https://dl.google.com/android/maven2/com/android/tools/r8/$R8/r8-$R8.jar
echo "r8 $R8 sha256 $(sha256sum /tmp/r8.jar | cut -d' ' -f1)"
mkdir -p /tmp/c /tmp/d
javac -nowarn --release 8 -d /tmp/c /src/ArtProbe.java 2>/dev/null
java -cp /tmp/r8.jar com.android.tools.r8.D8 --min-api 26 --output /tmp/d /tmp/c/*.class
cp /tmp/d/classes.dex /out/artprobe-classes.dex
export ANDROID_DATA=/tmp/ad
mkdir -p $ANDROID_DATA
set +e
dalvikvm -cp /tmp/d/classes.dex ArtProbe 2>/out/artprobe-stderr.log
echo "dalvikvm rc=$?"
