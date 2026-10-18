#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Static, read-only inventory of the DEX files inside a CloudStream .cs3 archive.

Parses the DEX header, string/type/proto/method/field id tables and class_defs
(Dalvik executable format, https://source.android.com/docs/core/runtime/dex-format).
It never loads or executes plugin code. Output: JSON with defined classes,
superclasses, and every external type/method the plugin references, grouped by package.
"""
import json
import struct
import sys
import zipfile

MAX_ENTRY = 64 * 1024 * 1024
NO_INDEX = 0xFFFFFFFF
HEADER_SIZE = 0x70


def uleb(buf, off):
    result = shift = 0
    while True:
        b = buf[off]
        off += 1
        result |= (b & 0x7F) << shift
        if b < 0x80:
            return result, off
        shift += 7


def mutf8(buf, off):
    _, off = uleb(buf, off)
    end = buf.index(b"\0", off)
    return buf[off:end].decode("utf-8", errors="surrogateescape")


def parse_dex(d):
    if d[:4] != b"dex\n" or len(d) < HEADER_SIZE:
        raise ValueError("not a DEX file")
    (magic, _cksum, _sig, file_size, _hsz, _endian, _lsz, _loff, _moff,
     s_n, s_off, t_n, t_off, p_n, p_off, f_n, f_off, m_n, m_off, c_n, c_off, _dsz, _doff) = \
        struct.unpack_from("<8sI20s20I", d, 0)
    if file_size != len(d):
        raise ValueError(f"header file_size {file_size} != {len(d)}")
    strings = [mutf8(d, struct.unpack_from("<I", d, s_off + 4 * i)[0]) for i in range(s_n)]
    types = [strings[struct.unpack_from("<I", d, t_off + 4 * i)[0]] for i in range(t_n)]

    def type_list(off):
        if off == 0:
            return []
        n = struct.unpack_from("<I", d, off)[0]
        return [types[struct.unpack_from("<H", d, off + 4 + 2 * i)[0]] for i in range(n)]

    protos = []
    for i in range(p_n):
        _shorty, ret, params = struct.unpack_from("<III", d, p_off + 12 * i)
        protos.append((types[ret], type_list(params)))
    methods = []
    for i in range(m_n):
        cls, proto, name = struct.unpack_from("<HHI", d, m_off + 8 * i)
        ret, params = protos[proto]
        methods.append((types[cls], strings[name], f"({''.join(params)}){ret}"))
    fields = []
    for i in range(f_n):
        cls, typ, name = struct.unpack_from("<HHI", d, f_off + 8 * i)
        fields.append((types[cls], strings[name], types[typ]))
    classes = []
    for i in range(c_n):
        cls, flags, sup, ifaces, _src, _ann, _data, _sv = struct.unpack_from("<8I", d, c_off + 32 * i)
        classes.append({
            "class": types[cls],
            "access_flags": flags,
            "super": types[sup] if sup != NO_INDEX else None,
            "interfaces": type_list(ifaces),
        })
    return {"version": magic[4:7].decode(), "strings": strings, "types": types,
            "methods": methods, "fields": fields, "classes": classes}


def pkg(t):
    t = t.lstrip("[")
    if not t.startswith("L"):
        return "<primitive>"
    name = t[1:-1]
    return name.rsplit("/", 1)[0].replace("/", ".") if "/" in name else "<default>"


def main(path):
    out = {"archive": path, "dex": []}
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            if not info.filename.endswith(".dex"):
                continue
            if info.file_size > MAX_ENTRY or ".." in info.filename or info.filename.startswith("/"):
                raise ValueError(f"refusing entry {info.filename}")
            dex = parse_dex(z.read(info))
            defined = {c["class"] for c in dex["classes"]}
            ext_methods = sorted({m for m in dex["methods"] if m[0].lstrip("[") not in defined})
            ext_fields = sorted({f for f in dex["fields"] if f[0].lstrip("[") not in defined})
            ext_types = sorted({t for t in dex["types"] if t.lstrip("[") not in defined and t.lstrip("[").startswith("L")})
            by_pkg = {}
            for t in ext_types:
                by_pkg.setdefault(pkg(t), []).append(t)
            out["dex"].append({
                "entry": info.filename,
                "dex_version": dex["version"],
                "counts": {k: len(dex[k]) for k in ("strings", "types", "methods", "fields", "classes")},
                "classes": dex["classes"],
                "external_packages": {k: len(v) for k, v in sorted(by_pkg.items())},
                "external_types": ext_types,
                "external_methods": [f"{c}->{n}{s}" for c, n, s in ext_methods],
                "external_fields": [f"{c}->{n}:{t}" for c, n, t in ext_fields],
            })
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: dexinfo.py <plugin.cs3>")
    main(sys.argv[1])
