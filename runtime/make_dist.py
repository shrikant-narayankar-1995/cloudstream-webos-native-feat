#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble a relocatable plugin runtime from an Alpine root (runtime/make-rootfs.sh).

usage: make_dist.py <rootfs dir> <arch: aarch64|x86_64> <out dir>

Layout of <out dir> (every file a regular file, no symlinks, so it can travel in an ipk):
  lib/ld-musl-<arch>.so.1            musl dynamic loader (the runtime is started through it)
  usr/bin/dalvikvm, usr/bin/dex2oat  ART executables (unmodified)
  usr/lib/..., usr/lib/art/...       DT_NEEDED closure of the above and of the JNI libraries,
                                     libEGL.so.1 replaced by runtime/native's stub
  usr/lib/java/dex/art/*.jar, natives/   boot class path and its JNI libraries (unmodified)
  usr/share/icu/<ver>/icudt*.dat     ICU data (ICU_DATA)
  bin/dex2oat                        runtime/native relaunch helper (ART finds dex2oat on $PATH)
  trust/cacerts                      Java trust store from Alpine's ca-certificates (Mozilla)
  csweb/csweb-host.dex               Android compat layer + csloader (runtime/build.sh)
  csweb/cloudstream.jar              CloudStream's pre-release APK minus Android-only parts (res/, lib/,
                                     assets/, resources.arsc, manifest, signatures): classes*.dex and the
                                     Java resources (META-INF/services, *.kotlin_builtins...), byte for byte,
                                     stored 4-byte aligned so ART maps the DEX instead of copying it
  MANIFEST.json                      every file with size and SHA-256, plus the Alpine package list
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
SEARCH = ["lib", "usr/lib", "usr/lib/art", "usr/lib/java/dex/art/natives"]
BOOT_JARS = ["core-oj", "core-libart", "okhttp", "bouncycastle", "apache-xml", "apachehttp", "wolfssljni",
             "core-junit", "hamcrest", "junit-runner"]  # parsed_options.cc default boot class path


def needed(path):
    out = subprocess.run(["readelf", "-dW", path], capture_output=True, text=True, check=True).stdout
    return [line.split("[", 1)[1].split("]", 1)[0] for line in out.splitlines() if "(NEEDED)" in line]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


ANDROID_ONLY = ("res/", "lib/", "assets/", "resources.arsc", "AndroidManifest.xml")


def is_signature(name):
    return name.startswith("META-INF/") and (name.endswith((".SF", ".RSA", ".EC", ".DSA")) or name == "META-INF/MANIFEST.MF")


def repack_classpath_jar(apk, jar):
    """Copy every class-loader-visible entry of the APK unchanged, stored and 4-byte aligned."""
    with zipfile.ZipFile(apk) as src, zipfile.ZipFile(jar, "w", zipfile.ZIP_STORED) as dst:
        for info in src.infolist():
            name = info.filename
            if name.endswith("/") or name.startswith(ANDROID_ONLY) or is_signature(name):
                continue
            data = src.read(info)
            zi = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            header = dst.fp.tell() + 30 + len(name.encode())
            zi.extra = b"\0" * ((-header) % 4)  # pad the local header so the data starts 4-aligned
            dst.writestr(zi, data, compress_type=zipfile.ZIP_STORED)
        for zi in dst.infolist():
            if zi.filename.endswith(".dex"):
                assert (zi.header_offset + 30 + len(zi.filename.encode()) + len(zi.extra)) % 4 == 0, zi.filename


def main(rootfs, arch, out):
    native = os.path.join(HERE, "native", "out")
    overrides = {"libEGL.so.1": os.path.join(native, f"libEGL.so.1.{arch}")}
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)

    def put(src, rel):
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.realpath(src), dst)  # dereference: ipks carry no symlinks
        shutil.copymode(os.path.realpath(src), dst)
        return dst

    roots = [f"lib/ld-musl-{arch}.so.1", "usr/bin/dalvikvm", "usr/bin/dex2oat"]
    roots += [f"usr/lib/art/{n}" for n in sorted(os.listdir(os.path.join(rootfs, "usr/lib/art"))) if n.endswith(".so")]
    nat = "usr/lib/java/dex/art/natives"
    roots += [f"{nat}/{n}" for n in sorted(os.listdir(os.path.join(rootfs, nat))) if n.endswith(".so")]

    placed, stack, unresolved = {}, list(roots), []
    while stack:
        rel = stack.pop()
        if rel in placed:
            continue
        name = os.path.basename(rel)
        src = overrides.get(name, os.path.join(rootfs, rel))
        placed[rel] = put(src, rel)
        for so in needed(src):
            if so.startswith("libc.musl"):  # musl's libc is the loader itself
                so_rel = f"lib/{so}"
                if so_rel not in placed:
                    placed[so_rel] = put(os.path.join(rootfs, f"lib/ld-musl-{arch}.so.1"), so_rel)
                continue
            hit = next((f"{d}/{so}" for d in SEARCH if os.path.exists(os.path.join(rootfs, d, so)) or so in overrides), None)
            if hit is None:
                unresolved.append((rel, so))
            elif hit not in placed:
                stack.append(hit)
    if unresolved:
        sys.exit(f"unresolved libraries: {unresolved}")

    for jar in BOOT_JARS:
        put(os.path.join(rootfs, f"usr/lib/java/dex/art/{jar}-hostdex.jar"), f"usr/lib/java/dex/art/{jar}-hostdex.jar")
    icu = os.path.join(rootfs, "usr/share/icu")
    for ver in os.listdir(icu):
        for f in os.listdir(os.path.join(icu, ver)):
            if f.endswith(".dat"):
                put(os.path.join(icu, ver, f), f"usr/share/icu/{ver}/{f}")
    put(os.path.join(native, f"relaunch.{arch}"), "bin/dex2oat")
    put(os.path.join(HERE, "trust", "cacerts"), "trust/cacerts")
    put(os.path.join(HERE, "out", "csweb-host.dex"), "csweb/csweb-host.dex")

    apk = os.path.join(PROJECT, "evidence", "cloudstream-app", "app-prerelease-release.apk")
    jar = os.path.join(out, "csweb", "cloudstream.jar")
    repack_classpath_jar(apk, jar)

    files = []
    for d, _, names in os.walk(out):
        for n in names:
            p = os.path.join(d, n)
            files.append({"path": os.path.relpath(p, out), "size": os.path.getsize(p), "sha256": sha256(p)})
    files.sort(key=lambda f: f["path"])
    manifest = {"arch": arch, "total_bytes": sum(f["size"] for f in files), "files": files,
                "alpine_packages": open(os.path.join(HERE, f"rootfs-{arch}.manifest")).read().split(),
                "cloudstream_apk_sha256": sha256(apk)}
    with open(os.path.join(out, "MANIFEST.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    print(f"{arch}: {len(files)} files, {manifest['total_bytes'] / 2**20:.1f} MiB -> {out}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
