#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the plugin compatibility table (Markdown) from csloader JSON-lines results.

usage: compat_matrix.py <load-only dir> <static survey.json>...

For each plugin: base class, providers registered by its own load(), and the first error or
unsupported API, from <dir>/<plugin>.jsonl; plus the static facts (android.* types, WebView use)
from survey_plugins.py output.
"""
import json
import os
import sys


def events(path):
    out = []
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line.startswith("{"):
                try:
                    out.append(json.loads(line))
                except ValueError:
                    pass
    return out


def main(run_dir, surveys):
    static = {}
    for s in surveys:
        for r in json.load(open(s)):
            static[r["file"][:-4]] = r
    rows = []
    for name in sorted(static):
        ev = events(os.path.join(run_dir, name + ".jsonl"))
        plugin = next((e for e in ev if e["event"] == "plugin"), {})
        providers = [e["name"] for e in ev if e["event"] == "provider"]
        problem = next((e for e in ev if e["event"] == "error"), None)
        unsupported = sorted({e.get("api", "") for e in ev if e["event"] == "unsupported"})
        st = static[name]
        webview = any("webkit" in t for t in st.get("android_types", []))
        if providers:
            status = "LOADS"
        elif problem and "AppCompatActivity" in problem.get("message", ""):
            status = "NEEDS-ACTIVITY"
        elif problem and problem.get("type") == "NoProvider":
            status = "NO-PROVIDER"
        elif not ev:
            status = "UNTESTED"
        else:
            status = "FAILS"
        detail = ""
        if problem:
            detail = (problem.get("rootMessage") or problem.get("message") or "")[:90]
        if unsupported:
            detail = (detail + "; " if detail else "") + "unsupported: " + ", ".join(unsupported)
        rows.append((name, plugin.get("base", st.get("entry_super", "?").split("/")[-1].rstrip(";")),
                     status, ", ".join(providers)[:60], "yes" if webview else "", detail))
    print("| Plugin | Base | Load-only result | Providers registered | WebView refs | First problem |")
    print("|---|---|---|---|---|---|")
    for r in rows:
        print("| " + " | ".join(x.replace("|", "/") for x in r) + " |")
    counts = {}
    for r in rows:
        counts[r[2]] = counts.get(r[2], 0) + 1
    print()
    print("Totals: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())) + f" (of {len(rows)})")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2:])
