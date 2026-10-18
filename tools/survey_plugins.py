#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Static compatibility survey of a directory of .cs3 plugins (nothing is loaded or executed).

usage: survey_plugins.py <dir with .cs3> <library.api> <out.json>

For each plugin: archive entries, manifest, entry-class base (BasePlugin vs Android Plugin),
android.* classes and members referenced, CloudStream API references missing from the JVM API dump,
and referenced packages outside the CloudStream library's dependency set. Writes a JSON report and
prints a summary table.
"""
import json
import os
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_api_refs  # noqa: E402
import dexinfo  # noqa: E402

MAX_ENTRIES = 1000
MAX_TOTAL = 256 * 1024 * 1024

# Packages the CloudStream library and its declared dependencies provide on the JVM
# (library/build.gradle.kts commonMain + jvmCommonMain) plus the Java platform.
PROVIDED_PREFIXES = (
    "java/", "javax/", "kotlin/", "kotlinx/", "com/lagradost/", "com/fasterxml/jackson/", "org/jsoup/",
    "okhttp3/", "okio/", "org/mozilla/javascript/", "com/fleeksoft/ksoup/", "io/ktor/",
    "dev/whyoleg/cryptography/", "org/schabi/newpipe/", "androidx/annotation/", "org/jetbrains/annotations/",
    "dalvik/annotation/", "com/android/tools/r8/annotations/", "org/intellij/lang/annotations/",
)


def classify_archive(path):
    with zipfile.ZipFile(path) as z:
        infos = z.infolist()
        if len(infos) > MAX_ENTRIES or sum(i.file_size for i in infos) > MAX_TOTAL:
            raise ValueError("archive too large")
        names = [i.filename for i in infos]
        for n in names:
            if n.startswith("/") or ".." in n.split("/"):
                raise ValueError(f"unsafe entry {n}")
        manifest = json.loads(z.read("manifest.json")) if "manifest.json" in names else None
    return names, manifest


def survey(path, api_classes):
    names, manifest = classify_archive(path)
    info = {"file": os.path.basename(path), "entries": names, "manifest": manifest}
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        dexinfo.main(path)
    dex = json.loads(buf.getvalue())["dex"]
    entry = (manifest or {}).get("pluginClassName", "")
    entry_desc = "L" + entry.replace(".", "/") + ";"
    supers = {c["class"]: c["super"] for d in dex for c in d["classes"]}
    info["entry_super"] = supers.get(entry_desc)
    info["dex_versions"] = sorted({d["dex_version"] for d in dex})
    defined = set(supers)

    android_types, android_members, foreign, missing = set(), set(), set(), []
    for d in dex:
        for t in d["external_types"]:
            name = t.lstrip("[")[1:-1]
            if name.startswith("android/"):
                android_types.add(name)
            elif not name.startswith(PROVIDED_PREFIXES):
                foreign.add(name.rsplit("/", 1)[0] if "/" in name else name)
        for m in d["external_methods"] + d["external_fields"]:
            if m.lstrip("[").startswith("Landroid/"):
                android_members.add(m)
        for kind, refs, table in (("method", d["external_methods"], "methods"), ("field", d["external_fields"], "fields")):
            for ref in refs:
                cls, rest = ref.split(";->", 1)
                cls = cls.lstrip("[")[1:]
                if cls.startswith("com/lagradost/cloudstream3/") and not check_api_refs.resolve(api_classes, cls, rest, table):
                    missing.append(ref)
        for t in d["external_types"]:
            name = t.lstrip("[")[1:-1]
            if name.startswith("com/lagradost/cloudstream3/") and name not in api_classes:
                missing.append(t)
    info["android_types"] = sorted(android_types)
    info["android_members"] = sorted(android_members)
    info["cloudstream_missing"] = sorted(set(missing))
    info["foreign_packages"] = sorted(foreign)
    info["native_libs"] = [n for n in names if n.endswith(".so")]
    info["has_resources"] = any(n == "resources.arsc" or n.startswith("res/") for n in names)
    info["dex_files"] = [n for n in names if n.endswith(".dex")]
    info["defined_classes"] = len(defined)

    tier = "A"
    reasons = []
    if info["entry_super"] != "Lcom/lagradost/cloudstream3/plugins/BasePlugin;":
        tier = "B"
        reasons.append(f"entry extends {info['entry_super']}")
    if android_types:
        tier = "B"
        reasons.append(f"{len(android_types)} android.* types")
    if (manifest or {}).get("requiresResources") or info["has_resources"]:
        tier = "B"
        reasons.append("resources")
    if info["cloudstream_missing"]:
        tier = "C" if tier == "A" else tier + "C"
        reasons.append(f"{len(info['cloudstream_missing'])} CloudStream refs not in JVM API")
    if info["native_libs"]:
        tier += "N"
        reasons.append("native libs")
    if foreign:
        reasons.append("other packages: " + ", ".join(sorted(foreign))[:120])
    info["tier"] = tier
    info["reasons"] = reasons
    return info


def main(directory, api, out):
    api_classes = check_api_refs.load_api(api)
    results = []
    for f in sorted(os.listdir(directory)):
        if f.endswith(".cs3"):
            try:
                results.append(survey(os.path.join(directory, f), api_classes))
            except Exception as e:  # report and continue: one bad archive must not hide the rest
                results.append({"file": f, "tier": "ERR", "reasons": [str(e)]})
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(results, fh, indent=1)
    for r in results:
        print(f"{r['tier']:4} {r['file']:34} {'; '.join(r['reasons'])}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
