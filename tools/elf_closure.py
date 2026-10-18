#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""DT_NEEDED closure of ELF files inside a root filesystem tree (uses binutils `readelf`, any arch).

usage: elf_closure.py <root> <search dirs, ':'-separated, relative to root> <ELF relative to root>...

Prints JSON: every ELF in the closure (relative path, size, needed list), unresolved SONAMEs, and for
each library the ELFs that pull it in, so large or unwanted dependencies can be traced.
"""
import json
import os
import subprocess
import sys


def needed(path):
    out = subprocess.run(["readelf", "-dW", path], capture_output=True, text=True, check=True).stdout
    return [line.split("[", 1)[1].split("]", 1)[0] for line in out.splitlines() if "(NEEDED)" in line]


def main(root, dirs, roots):
    search = [os.path.join(root, d) for d in dirs.split(":")]
    seen, unresolved, pulled_by = {}, {}, {}
    stack = [os.path.join(root, r) for r in roots]
    while stack:
        p = stack.pop()
        real = os.path.realpath(p)
        if real in seen:
            continue
        deps = needed(real)
        seen[real] = {"path": os.path.relpath(p, root), "size": os.path.getsize(real), "needed": deps}
        for so in deps:
            hit = next((os.path.join(d, so) for d in search if os.path.exists(os.path.join(d, so))), None)
            if hit is None:
                unresolved.setdefault(so, []).append(os.path.relpath(p, root))
                continue
            pulled_by.setdefault(os.path.relpath(hit, root), []).append(os.path.relpath(p, root))
            stack.append(hit)
    files = sorted(seen.values(), key=lambda e: -e["size"])
    json.dump({"files": files, "total_bytes": sum(f["size"] for f in files), "unresolved": unresolved,
               "pulled_by": pulled_by}, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
