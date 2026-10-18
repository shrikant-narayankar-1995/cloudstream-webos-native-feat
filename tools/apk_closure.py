#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Resolve the runtime dependency closure of Alpine packages from APKINDEX files (static metadata only).

usage: apk_closure.py <APKINDEX>... -- <package>...
Prints JSON: each package in the closure with version, licence, sizes, origin repo and the provider
chosen for every so:/cmd: dependency. Nothing is downloaded or executed.
"""
import json
import sys


def parse(path, repo):
    pkgs, cur = [], {}
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line:
            if cur:
                pkgs.append(cur)
            cur = {}
            continue
        k, _, v = line.partition(":")
        cur[k] = v
    if cur:
        pkgs.append(cur)
    for p in pkgs:
        p["_repo"] = repo
    return pkgs


def main(argv):
    sep = argv.index("--")
    indexes, roots = argv[:sep], argv[sep + 1:]
    pkgs = []
    for path in indexes:
        pkgs += parse(path, path.split("/")[-2])
    by_name = {p["P"]: p for p in pkgs}
    provides = {}
    for p in pkgs:
        for prov in p.get("p", "").split():
            provides.setdefault(prov.split("=")[0], p["P"])
    seen, order, missing = set(), [], []
    stack = list(roots)
    while stack:
        name = stack.pop()
        if name in seen:
            continue
        seen.add(name)
        p = by_name.get(name)
        if not p:
            missing.append(name)
            continue
        order.append(name)
        for dep in p.get("D", "").split():
            if dep.startswith("!"):
                continue
            dep = dep.split(">")[0].split("<")[0].split("=")[0].split("~")[0]
            stack.append(by_name[dep]["P"] if dep in by_name else provides.get(dep, dep))
    out = [{"name": n, "version": by_name[n]["V"], "licence": by_name[n].get("L"), "repo": by_name[n]["_repo"],
            "size": int(by_name[n].get("S", 0)), "installed": int(by_name[n].get("I", 0)),
            "checksum": by_name[n].get("C")} for n in sorted(order)]
    json.dump({"roots": roots, "packages": out, "unresolved": missing,
               "total_installed": sum(p["installed"] for p in out)}, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main(sys.argv[1:])
