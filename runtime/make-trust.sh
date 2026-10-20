#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build runtime/trust/cacerts, the Java trust store ART's JSSE reads: Alpine's ca-certificates
# (Mozilla's CA list) extracted by p11-kit as java-cacerts does, from the pinned packages only.
#   docker run --rm -v <project>:/p -v <inputs dir>/alpine/x86_64:/apks:ro alpine:edge sh /p/runtime/make-trust.sh
#   (without the /apks mount: the current packages from Alpine edge)
# p11-kit stamps every entry with the current time; the stamps are set to the ca-certificates release
# date (and the keystore digest recomputed, password "changeit") so the file is the same on every build.
set -eu
if [ -d /apks ]; then
    apk add -q --no-network /apks/libffi-*.apk /apks/libtasn1-*.apk /apks/p11-kit-0*.apk /apks/p11-kit-trust-*.apk \
        /apks/ca-certificates-2*.apk >/dev/null
    ver=$(ls /apks/ca-certificates-2*.apk | sed -n 's#.*/ca-certificates-\([0-9]\{8\}\)-.*#\1#p' | head -1)
else
    apk add -q p11-kit-trust ca-certificates >/dev/null
    ver=$(apk list -I ca-certificates 2>/dev/null | sed -n 's#^ca-certificates-\([0-9]\{8\}\)-.*#\1#p' | head -1)
fi
apk add -q python3 >/dev/null
[ -n "$ver" ] || { echo "make-trust.sh: no ca-certificates version" >&2; exit 1; }
mkdir -p /p/runtime/trust
rm -f /p/runtime/trust/cacerts
trust extract --format=java-cacerts --filter=ca-anchors --purpose=server-auth /tmp/cacerts
python3 - /tmp/cacerts /p/runtime/trust/cacerts "$ver" <<'EOF'
import calendar, hashlib, struct, sys, time
src, dst, ver = sys.argv[1:]
stamp = calendar.timegm(time.strptime(ver, "%Y%m%d")) * 1000
d = bytearray(open(src, "rb").read())
assert d[:4] == b"\xfe\xed\xfe\xed", "not a JKS keystore"
count = struct.unpack(">I", d[8:12])[0]
off = 12
for _ in range(count):
    tag = struct.unpack(">I", d[off:off + 4])[0]; off += 4
    assert tag == 2, "only trusted certificate entries expected"
    n = struct.unpack(">H", d[off:off + 2])[0]; off += 2 + n          # alias
    d[off:off + 8] = struct.pack(">q", stamp); off += 8                # creation date
    n = struct.unpack(">H", d[off:off + 2])[0]; off += 2 + n          # certificate type
    n = struct.unpack(">I", d[off:off + 4])[0]; off += 4 + n          # certificate
assert off == len(d) - 20, "unexpected trailing data"
password = "changeit".encode("utf-16-be")
d[off:] = hashlib.sha1(password + b"Mighty Aphrodite" + bytes(d[:off])).digest()
open(dst, "wb").write(d)
print(f"trust/cacerts: {count} CA certificates, ca-certificates {ver}")
EOF
chown "$(stat -c %u:%g /p)" /p/runtime/trust /p/runtime/trust/cacerts
