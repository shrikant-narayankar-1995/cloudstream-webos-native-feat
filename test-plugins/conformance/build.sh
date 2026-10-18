#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the conformance test plugin (a TEST FIXTURE, not a provider) into a standard .cs3:
# manifest.json + classes.dex + resources.arsc + res/ (binary XML), like CloudStream's plugin template.
#   docker run --rm -v <project>:/p eclipse-temurin:17-jdk sh /p/test-plugins/conformance/build.sh
# Toolchain (pinned): Kotlin 2.4.0 (CloudStream's), aapt2 9.4.1-15978811, Android platform 35 r02 (compile/link
# only, never shipped), D8 9.5.26. Compiles against upstream CloudStream classes.jar.
set -eu
P=/p; S=$P/test-plugins/conformance; T=/tmp/tc; O=$S/out
apt-get -qq update >/dev/null && apt-get -qq install -y unzip zip curl >/dev/null
mkdir -p $T $O
cd $T
curl -sfL -o kotlinc.zip https://github.com/JetBrains/kotlin/releases/download/v2.4.0/kotlin-compiler-2.4.0.zip
curl -sfL -o aapt2.jar https://dl.google.com/android/maven2/com/android/tools/build/aapt2/9.4.1-15978811/aapt2-9.4.1-15978811-linux.jar
curl -sfL -o platform.zip https://dl.google.com/android/repository/platform-35_r02.zip
curl -sfL -o r8.jar https://dl.google.com/android/maven2/com/android/tools/r8/9.5.26/r8-9.5.26.jar
sha256sum kotlinc.zip aapt2.jar platform.zip r8.jar > $O/TOOLCHAIN-SHA256
unzip -q kotlinc.zip && unzip -q -o aapt2.jar aapt2 && chmod +x aapt2
ANDROID_JAR=$(unzip -Z1 platform.zip | grep '/android.jar$'); unzip -q -j platform.zip "$ANDROID_JAR" -d .
# resources -> resources.arsc + binary XML (package org.csweb.conformance)
rm -rf flat res.apk && mkdir flat
./aapt2 compile --dir $S/res -o flat/
./aapt2 link -I android.jar --manifest $S/AndroidManifest.xml -o res.apk flat/*.flat --no-version-vectors
# Kotlin -> JVM classes -> DEX
rm -rf cls dex && mkdir cls dex
CP=android.jar:$P/evidence/cloudstream-app/classes.jar
./kotlinc/bin/kotlinc -nowarn -jvm-target 1.8 -cp "$CP" -d cls $S/src 2>&1 | grep -v '^warning' || true
java -cp r8.jar com.android.tools.r8.D8 --min-api 23 --lib android.jar --classpath $P/evidence/cloudstream-app/classes.jar \
    --classpath kotlinc/lib/kotlin-stdlib.jar \
    --output dex $(find cls -name '*.class')
# assemble the .cs3 (no Kotlin stdlib inside: plugins use the host app's)
rm -rf pkg && mkdir pkg && cd pkg
unzip -q ../res.apk -x AndroidManifest.xml
cp $S/manifest.json ../dex/classes.dex .
rm -f $O/CswebConformance.cs3 && zip -q -X -r $O/CswebConformance.cs3 manifest.json classes.dex resources.arsc res
unzip -l $O/CswebConformance.cs3
sha256sum $O/CswebConformance.cs3 | tee $O/SHA256SUMS
