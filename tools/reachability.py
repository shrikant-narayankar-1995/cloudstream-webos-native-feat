#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Static reachability of external APIs from a plugin's runtime paths (androguard; nothing is executed).

usage: reachability.py <out.json> <plugin.cs3>...

Rapid type analysis over the plugin's own DEX code:
- roots: the manifest plugin class's load(...) methods, and the provider operations
  (getMainPage, search, quickSearch, load, loadLinks, getVideoInterceptor, getUrl) of plugin classes
  extending MainAPI or ExtractorApi;
- a plugin class becomes *instantiated* when reachable code does new-instance on it, or reads its
  static INSTANCE singleton; virtual/interface calls dispatch to methods of instantiated classes with the
  same name and descriptor; methods of instantiated classes that external code calls back
  (invoke*, invokeSuspend, create, run, call, compare*, accept, apply, intercept, on*) are roots too;
- the object passed to Plugin.setOpenSettings is tracked separately: its code runs only when the user
  opens the plugin's settings, so it is reported as "settings" rather than "runtime".
External (non-plugin) classes are leaves; for each, the members referenced are recorded.
"""
import json
import re
import sys
import zipfile

from androguard.core.dex import DEX

OPS = {"getMainPage", "search", "quickSearch", "load", "loadLinks", "getVideoInterceptor", "getUrl"}
PROVIDER_BASES = {"Lcom/lagradost/cloudstream3/MainAPI;", "Lcom/lagradost/cloudstream3/utils/ExtractorApi;"}
CALLBACK = re.compile(r"^(invoke|invokeSuspend|create|run|call|compare|compareTo|accept|apply|intercept|on[A-Z].*|get|test|emit|resumeWith)$")
REG = re.compile(r"^[vp]\d+$")


def load_methods(path):
    with zipfile.ZipFile(path) as z:
        manifest = json.loads(z.read("manifest.json"))
        dexes = [DEX(z.read(n)) for n in z.namelist() if n.endswith(".dex")]
    methods, supers, by_class = {}, {}, {}
    for d in dexes:
        for c in d.get_classes():
            supers[c.get_name()] = c.get_superclassname()
            for m in c.get_methods():
                key = (c.get_name(), m.get_name(), m.get_descriptor().replace(" ", ""))
                methods[key] = m
                by_class.setdefault(c.get_name(), []).append(key)
    return manifest, methods, supers, by_class


def decode(m):
    code = m.get_code()
    if code is None:
        return []
    out = []
    for ins in code.get_bc().get_instructions():
        out.append((ins.get_name(), [p.strip() for p in ins.get_output().split(", ")]))
    return out


def ref_parts(ref):
    cls, _, rest = ref.partition("->")
    name, _, desc = rest.partition("(")
    return cls, name, ("(" + desc).replace(" ", "") if desc else ""


def analyse(path):
    manifest, methods, supers, by_class = load_methods(path)

    def chain(cls):
        seen = []
        while cls in supers and cls not in seen:
            seen.append(cls)
            cls = supers[cls]
        return seen + [cls]

    entry = "L" + manifest["pluginClassName"].replace(".", "/") + ";"
    runtime_roots = [k for k in methods if k[0] == entry and k[1] == "load"]
    runtime_roots += [k for k in methods if k[1] in OPS and k[0] != entry and PROVIDER_BASES & set(chain(k[0]))]

    decoded = {}

    def run(roots):
        reached, inst, vsigs, ext, settings = set(), set(), set(), {}, set()
        work = list(roots)
        changed = True
        while changed:
            changed = False
            while work:
                key = work.pop()
                if key in reached or key not in methods:
                    continue
                reached.add(key)
                changed = True
                if key not in decoded:
                    decoded[key] = decode(methods[key])
                regs = {}
                for op, args in decoded[key]:
                    target = args[-1] if args else ""
                    if op == "new-instance":
                        cls = target
                        regs[args[0]] = cls
                        if cls in supers:
                            if cls not in inst:
                                inst.add(cls)
                            work += [k for k in by_class.get(cls, []) if k[1] in ("<init>", "<clinit>")]
                        else:
                            ext.setdefault(cls, set()).add("<new>")
                    elif op.startswith("sget"):
                        cls = target.split("->")[0]
                        if len(args) >= 2:
                            regs[args[0]] = target.split(" ")[-1] if target.endswith(";") else None
                        if cls in supers:
                            inst.add(cls)
                            work += [k for k in by_class.get(cls, []) if k[1] == "<clinit>"]
                        else:
                            ext.setdefault(cls, set()).add(target.split("->")[1] if "->" in target else "<static>")
                    elif op.startswith(("sput", "iget", "iput")):
                        cls = target.split("->")[0]
                        if cls not in supers and cls.startswith("L"):
                            ext.setdefault(cls, set()).add(target.split("->")[1] if "->" in target else "<field>")
                    elif op in ("const-class", "check-cast", "instance-of"):
                        cls = target
                        if cls not in supers and cls.startswith("L"):
                            ext.setdefault(cls, set()).add("<type>")
                    elif op.startswith("invoke"):
                        cls, name, desc = ref_parts(target)
                        if name == "setOpenSettings":
                            arg = [a for a in args[:-1] if REG.match(a.strip("{}"))]
                            if len(arg) >= 2 and regs.get(arg[1].strip("{}")):
                                settings.add(regs[arg[1].strip("{}")])
                        # static/direct/super: exact target in this class or its plugin ancestors
                        hit = None
                        for c in chain(cls):
                            if (c, name, desc) in methods:
                                hit = (c, name, desc)
                                break
                        if hit and (op.startswith(("invoke-static", "invoke-direct", "invoke-super"))):
                            work.append(hit)
                        else:
                            if hit:
                                work.append(hit)
                            vsigs.add((name, desc))
                            if not hit and cls not in supers:
                                ext.setdefault(cls, set()).add(name + desc)
                        for a in args[:-1]:
                            regs.pop(a.strip("{}"), None)
            # dispatch virtual calls and framework callbacks into instantiated classes
            for cls in list(inst):
                if cls in settings:
                    continue
                for k in by_class.get(cls, []):
                    if k not in reached and ((k[1], k[2]) in vsigs or CALLBACK.match(k[1])):
                        work.append(k)
        return reached, inst, ext, settings

    reached, inst, ext, settings = run(runtime_roots)
    settings_roots = [k for c in settings for k in by_class.get(c, []) if k[1] != "<init>"]
    s_reached, _, s_ext, _ = run(settings_roots)
    return {
        "file": path.rsplit("/", 1)[-1],
        "entry": entry,
        "runtime_roots": len(runtime_roots),
        "reached_methods": len(reached),
        "total_methods": len(methods),
        "settings_classes": sorted(c for c in settings if c),
        "runtime_external": {k: sorted(v) for k, v in sorted(ext.items())},
        "settings_external_only": {k: sorted(v) for k, v in sorted(s_ext.items()) if k not in ext},
    }


def main(out, paths):
    res = []
    for p in paths:
        try:
            res.append(analyse(p))
        except Exception as e:  # keep going; one plugin must not hide the others
            res.append({"file": p.rsplit("/", 1)[-1], "error": repr(e)})
    with open(out, "w") as fh:
        json.dump(res, fh, indent=1)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
