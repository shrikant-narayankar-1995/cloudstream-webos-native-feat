#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Start csloader from a relocated runtime dist exactly as the TV app does: cleared environment, the
# bundled musl loader, explicit library path, runtime data on a scratch directory.
#   dist-run.sh <dist dir> <arch> <scratch dir> <csloader args...>
set -eu
D=$(cd "$1" && pwd); ARCH=$2; S=$3; shift 3
mkdir -p "$S/art" "$S/home" "$S/plugin-data"
LIBS="$D/usr/lib:$D/usr/lib/art:$D/usr/lib/java/dex/art/natives:$D/lib"
exec env -i PATH="$D/bin" ANDROID_DATA="$S/art" HOME="$S/home" ICU_DATA="$(ls -d "$D"/usr/share/icu/*)" \
    "$D/lib/ld-musl-$ARCH.so.1" --library-path "$LIBS" "$D/usr/bin/dalvikvm" -Xmx256m ${ART_OPTS:-} \
    -Djavax.net.ssl.trustStore="$D/trust/cacerts" -Djava.library.path="$D/usr/lib/java/dex/art/natives" \
    -cp "$D/csweb/csweb-host.dex:$D/csweb/cloudstream.jar" org.csweb.loader.Main --data "$S/plugin-data" "$@"
