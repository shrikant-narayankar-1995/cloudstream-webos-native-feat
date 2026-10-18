#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copy the test plugins named in testplugins/tests.json from evidence/plugins/ into testplugins/,
# refusing any file whose SHA-256 differs from the one in tests.json. The .cs3 files are not committed.
set -eu
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
python3 -I - "$here/testplugins/tests.json" "$root/evidence/plugins" "$here/testplugins" <<'PY'
import hashlib, json, os, shutil, sys
spec, src_root, dst = sys.argv[1:]
for t in {(t["plugin"], t["sha256"]) for t in json.load(open(spec))}:
    name, want = t
    hits = [os.path.join(d, name) for d, _, files in os.walk(src_root) if name in files]
    ok = [h for h in hits if hashlib.sha256(open(h, "rb").read()).hexdigest() == want]
    if not ok:
        sys.exit(f"{name}: no copy with sha256 {want} under {src_root}")
    shutil.copyfile(ok[0], os.path.join(dst, name))
    print(f"{name} {want}")
PY
