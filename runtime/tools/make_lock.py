#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pin an architecture's Alpine packages: write runtime/inputs-<arch>.lock from rootfs-<arch>.manifest.

usage: make_lock.py <arch> [<inputs dir>]     e.g. make_lock.py armv7

For each package in runtime/rootfs-<arch>.manifest (written by an unpinned make-rootfs.sh run, which is
what tools/build-runtime.sh --arch <arch> does until this lock exists), the .apk is downloaded from Alpine
edge (main, community, then testing), hashed, and kept in <inputs dir>/alpine/<arch>/ (default
~/.cache/csweb-runtime-inputs). The lock has the same format as runtime/inputs.lock and is read by
tools/fetch-runtime-inputs.sh. Alpine edge drops old builds, so run this soon after the build it pins,
then upload the files to a mirror release and add its "# mirror: <url>" line (docs/legacy-build.md).

Only used for the legacy (armv7) runtime; runtime/inputs.lock (aarch64) is not touched.
"""
import hashlib
import os
import sys
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
RUNTIME = os.path.dirname(HERE)
CDN = "https://dl-cdn.alpinelinux.org/alpine/edge"
REPOS = ("main", "community", "testing")


def fetch(url):
    try:
        with urllib.request.urlopen(url, timeout=120) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise


def main(arch, inputs):
    if arch == "aarch64":
        sys.exit("make_lock.py: aarch64 is pinned in runtime/inputs.lock; this tool is for other arches")
    manifest = os.path.join(RUNTIME, f"rootfs-{arch}.manifest")
    if not os.path.exists(manifest):
        sys.exit(f"make_lock.py: no {manifest}: run tools/build-runtime.sh --arch {arch} first")
    out_dir = os.path.join(inputs, "alpine", arch)
    os.makedirs(out_dir, exist_ok=True)
    lines = [
        "# SPDX-License-Identifier: GPL-3.0-or-later",
        f"# Alpine edge packages of the {arch} plugin runtime (legacy package, docs/legacy-build.md).",
        "# Written by runtime/tools/make_lock.py from runtime/rootfs-%s.manifest; read by tools/fetch-runtime-inputs.sh." % arch,
        "#   <sha256>  <path in the inputs mirror>  <upstream URL>",
        "# Add '# mirror: <release download URL>' once the files are uploaded to a runtime-inputs-N release.",
    ]
    for pkg in open(manifest).read().split():
        name = f"{pkg}.apk"
        for repo in REPOS:
            url = f"{CDN}/{repo}/{arch}/{name}"
            data = fetch(url)
            if data is not None:
                break
        else:
            sys.exit(f"make_lock.py: {name} is no longer on Alpine edge; rebuild unpinned and run again")
        with open(os.path.join(out_dir, name), "wb") as f:
            f.write(data)
        lines.append(f"{hashlib.sha256(data).hexdigest()}  alpine/{arch}/{name}  {url}")
        print(f"{name}: {repo}")
    lock = os.path.join(RUNTIME, f"inputs-{arch}.lock")
    with open(lock, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{lock}: {len(lines) - 5} packages")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2] if len(sys.argv) == 3 else os.path.expanduser("~/.cache/csweb-runtime-inputs"))
