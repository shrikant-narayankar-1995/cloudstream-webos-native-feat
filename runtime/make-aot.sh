#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Ahead-of-time compile a runtime dist with its own (unmodified) dex2oat, so the TV starts ART with a
# boot image and pre-verified class path instead of imageless:
#   docker run --rm -v <project>:/p alpine:edge sh /p/runtime/make-aot.sh <arch> <stable path>
#   e.g. make-aot.sh aarch64 /tmp/org.csweb.runtimeprobe/rt
# then: python3 runtime/make_dist.py --manifest runtime/dist/runtime-<arch>
#
# ART records boot class path locations in the image, and libcore reads boot-jar resources through those
# locations, so they must be real paths at run time. They are taken under <stable path>, a symlink the app
# creates to its installed runtime directory before starting ART; the image then does not depend on where
# the package is installed. The aarch64 dex2oat runs under qemu-aarch64 (user mode, inside this
# container only) on an x86_64 build host.
# Output, inside the dist:
#   boot/<isa>/boot*.{art,oat,vdex}          boot image for the default boot class path
#   csweb/oat/<isa>/<name>.{odex,vdex}       verify-filtered class path (csweb-host.dex, cloudstream.jar)
#   AOT.json                                 what was compiled, how, and the launch options it needs
set -eu
ARCH=$1; STABLE=$2
DIST=/p/runtime/dist/runtime-$ARCH
case $ARCH in aarch64) ISA=arm64; FEATURES=-a53,-crc,-lse,-fp16,-dotprod;; x86_64) ISA=x86_64; FEATURES=-ssse3,-sse4.1,-sse4.2,-avx,-avx2,-popcnt;; *) echo "bad arch $ARCH" >&2; exit 2;; esac
# The features the runtime itself selects on the TV (dalvikvm log: --instruction-set-features=...).
case $STABLE in /*) ;; *) echo "stable path must be absolute" >&2; exit 2;; esac
[ -d "$DIST/usr/bin" ] || { echo "no dist at $DIST (run make_dist.py first)" >&2; exit 2; }
RUN=
if [ "$(uname -m)" != "$ARCH" ]; then
    apk add -q qemu-aarch64 >/dev/null 2>&1
    RUN="qemu-aarch64"
fi
mkdir -p "$(dirname "$STABLE")"
ln -s "$DIST" "$STABLE"
D=$STABLE
LIBS="$D/usr/lib:$D/usr/lib/art:$D/usr/lib/java/dex/art/natives:$D/lib"
dex2oat() { env -i ANDROID_DATA=/tmp/aot-data HOME=/tmp/aot-home $RUN "$D/lib/ld-musl-$ARCH.so.1" --library-path "$LIBS" "$D/usr/bin/dex2oat" "$@"; }
mkdir -p /tmp/aot-data /tmp/aot-home
rm -rf "$DIST/boot" "$DIST/csweb/oat" "$DIST/AOT.json"
mkdir -p "$DIST/boot/$ISA" "$DIST/csweb/oat/$ISA"

# The default boot class path compiled into libart, in its order.
JARS="core-oj apachehttp apache-xml bouncycastle core-junit core-libart hamcrest junit-runner okhttp wolfssljni"
args=; BCP=
for j in $JARS; do
    p=$D/usr/lib/java/dex/art/$j-hostdex.jar
    args="$args --dex-file=$p --dex-location=$p"
    BCP="$BCP:$p"
done
BCP=${BCP#:}
log=/tmp/aot.log
check() { grep -E "dex2oat took" $log | sed 's/.*dex2oat took/  dex2oat took/'; if grep -qE " [FE] " $log; then grep -E " [FE] " $log >&2; exit 1; fi; }
echo "boot image ($ISA)"
# shellcheck disable=SC2086
dex2oat $args --image="$D/boot/$ISA/boot.art" --oat-file="$D/boot/$ISA/boot.oat" --instruction-set=$ISA \
    --instruction-set-features=$FEATURES --base=0x70000000 --runtime-arg -Xms64m --runtime-arg -Xmx512m > $log 2>&1 || { cat $log >&2; exit 1; }
check
# The app class path, as Main is started: -cp csweb-host.dex:cloudstream.jar (one PathClassLoader).
ctx=
for name in csweb-host.dex cloudstream.jar; do
    echo "$name (verify)"
    dex2oat --runtime-arg -Xbootclasspath:"$BCP" --boot-image="$D/boot/boot.art" --dex-file="$D/csweb/$name" \
        --dex-location="$D/csweb/$name" --oat-file="$D/csweb/oat/$ISA/${name%.*}.odex" --instruction-set=$ISA \
        --instruction-set-features=$FEATURES --compiler-filter=verify --classpath-dir="$D/csweb" \
        --class-loader-context="PCL[$ctx]" --runtime-arg -Xmx512m > $log 2>&1 || { cat $log >&2; exit 1; }
    check
    ctx=${ctx:+$ctx:}$name
done

{
    printf '{\n "stable_path": "%s",\n "isa": "%s",\n "instruction_set_features": "%s",\n' "$STABLE" "$ISA" "$FEATURES"
    printf ' "image": "boot/boot.art",\n "bootclasspath": ['
    sep=; for j in $JARS; do printf '%s"usr/lib/java/dex/art/%s-hostdex.jar"' "$sep" "$j"; sep=", "; done
    printf '],\n "classpath": ["csweb/csweb-host.dex", "csweb/cloudstream.jar"],\n "classpath_filter": "verify",\n'
    printf ' "dex2oat_sha256": "%s",\n "qemu": "%s"\n}\n' "$(sha256sum "$DIST/usr/bin/dex2oat" | cut -d' ' -f1)" \
        "$( [ -n "$RUN" ] && qemu-aarch64 --version | head -1 || echo none)"
} > "$DIST/AOT.json"
chown -R "$(stat -c %u:%g "$DIST")" "$DIST/boot" "$DIST/csweb/oat" "$DIST/AOT.json"
du -sh "$DIST/boot" "$DIST/csweb/oat"
