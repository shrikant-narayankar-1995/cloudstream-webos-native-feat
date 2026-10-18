#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check a plugin's CloudStream method/field references against library/api/jvm/library.api.

Input: the JSON written by dexinfo.py and the Kotlin binary-compatibility-validator dump.
Only references into the given package prefix are checked. Inherited members are resolved
through the superclasses listed in the dump. This is a static signature check; it does not
prove the code runs.
"""
import json
import re
import sys

CLASS_RE = re.compile(r"^(?:public|protected)\b.*?\bclass (\S+)(?: : (.+?))? \{$")
MEMBER_RE = re.compile(r"^\t(?:public|protected)\b.*?\b(fun|field) (\S+) (?:(\(.*?\)\S+)|(\S+))$")


def load_api(path):
    classes = {}
    current = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            m = CLASS_RE.match(line)
            if m:
                current = m.group(1)
                supers = [s.strip() for s in (m.group(2) or "").split(",") if s.strip()]
                classes[current] = {"supers": supers, "methods": set(), "fields": set()}
                continue
            if line == "}":
                current = None
                continue
            m = MEMBER_RE.match(line)
            if m and current:
                kind, name, desc, ftype = m.groups()
                if kind == "fun":
                    classes[current]["methods"].add(name + desc)
                else:
                    classes[current]["fields"].add(name + ":" + ftype)
    return classes


def resolve(classes, cls, key, kind, seen=None):
    seen = seen or set()
    if cls in seen or cls not in classes:
        return False
    seen.add(cls)
    if key in classes[cls][kind]:
        return True
    return any(resolve(classes, s, key, kind, seen) for s in classes[cls]["supers"])


def main(dexinfo, api, prefix):
    classes = load_api(api)
    data = json.load(open(dexinfo, encoding="utf-8"))
    missing, checked = [], 0
    for dex in data["dex"]:
        for t in dex["external_types"]:
            name = t.lstrip("[")[1:-1]
            if name.startswith(prefix):
                checked += 1
                if name not in classes:
                    missing.append(("type", t))
        for ref in dex["external_methods"]:
            cls, rest = ref.split(";->", 1)
            cls = cls.lstrip("[")[1:]
            if not cls.startswith(prefix):
                continue
            checked += 1
            if not resolve(classes, cls, rest, "methods"):
                missing.append(("method", ref))
        for ref in dex["external_fields"]:
            cls, rest = ref.split(";->", 1)
            cls = cls.lstrip("[")[1:]
            if cls.startswith(prefix):
                checked += 1
                if not resolve(classes, cls, rest, "fields"):
                    missing.append(("field", ref))
    print(json.dumps({"dexinfo": dexinfo, "api": api, "prefix": prefix, "checked": checked,
                      "missing": missing}, indent=1))
    return 1 if missing else 0


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: check_api_refs.py <dexinfo.json> <library.api> <package prefix, e.g. com/lagradost/cloudstream3/>")
    sys.exit(main(*sys.argv[1:]))
