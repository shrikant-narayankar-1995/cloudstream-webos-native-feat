# Ahead-of-time compiled runtime

Status 2026-10-19. `runtime/make-aot.sh`, `runtime/make_dist.py --manifest`, probe 0.5.0.

## What

Until now the TV ran ART imageless: no boot image, every class of the core library and of CloudStream
set up and verified at each start (the app directory is read-only and tmpfs is RAM, so nothing was
compiled on the TV). The runtime's own unmodified `dex2oat` now compiles, at build time:

| Output (in the dist) | Contents | Size (arm64) |
|---|---|---|
| `boot/arm64/boot*.{art,oat,vdex}` | boot image of the default boot class path (core-oj, core-libart, okhttp, bouncycastle, …) | 66.7 MB |
| `csweb/oat/arm64/{csweb-host,cloudstream}.{odex,vdex}` | the class path, `verify` filter (verification done at build time; code still interpreted/JITed) | 1.2 MB |
| `AOT.json` | stable path, ISA and features, boot class path, filter, dex2oat SHA-256, qemu version | |

The aarch64 `dex2oat` runs on the x86_64 build host under `qemu-aarch64` (user mode, inside the build
container only; the host's binfmt configuration is not changed). Instruction set features are the ones the
runtime selects on the TV (`-a53,-crc,-lse,-fp16,-dotprod`). Build: 40 s.

```
python3 runtime/make_dist.py runtime/rootfs/aarch64 aarch64 runtime/dist/runtime-aarch64
docker run --rm -v $PWD:/p alpine:edge sh /p/runtime/make-aot.sh aarch64 /tmp/<app id>/rt
python3 runtime/make_dist.py --manifest runtime/dist/runtime-aarch64
```

## Stable path

ART stores the boot class path locations in the image and accepts the image only when the runtime's boot
class path matches them. libcore also reads boot-jar resources through those locations, so they cannot be
made-up names (tried: resource loading fails). The image is therefore compiled against
`/tmp/<app id>/rt/...`, and the app creates that symlink to its installed runtime before starting ART
(`prepare_aot` in `apps/runtime-probe/main.cpp`: an existing non-symlink is never replaced). The image
does not depend on where webOS installs the package; it does depend on the app id.

Launch options added: `-Ximage:/tmp/<app id>/rt/boot/boot.art -Xbootclasspath:<the jars under the same
path>`, with every other runtime path also taken through the symlink so the odex files are found
(`csweb/oat/arm64/` next to the dex files). If the path does not match, ART falls back to imageless
(tested on host).

## Results on the TV (LG OLED65C1PTB, webOS 6.5.3, probe 0.5.0)

`evidence/device/20261019-113259-runtimeprobe-0.5/`, same run, same plugins:

| | Imageless | AOT |
|---|---|---|
| One-shot conformance search (local, no network) | 7.2 s, peak RSS 113 MiB | 1.7 s, 57 MiB |
| One-shot Dailymotion / LibriVox search | 10.1 / 10.3 s (probe 0.4) | 3.5 / 3.5 s, 69-70 MiB |
| Service: first plugin load | 3.45 s | 1.55 s |
| Service: 14-request session | 11.0 s, 126 MiB | 4.1 s, 75 MiB |

Memory drops because the boot image is mapped from the package (clean, file-backed pages) instead of
being built on the Java heap at each start.

## Costs and limits

- Package: ipk 54 → 76 MB; installed runtime 113 → 181 MiB.
- The image is tied to the app id (stable path) and to the exact runtime files (rebuilt with the dist).
- ART still tries to compile each plugin at load (dex2oat started from `$PATH`); that child fails in
  ~8 ms because it is not given the boot class path, and the plugin runs interpreted/JITed as before.
- Plugins themselves are not precompiled: they are installed at run time, and compiling them on the TV
  would put the output on tmpfs, i.e. in RAM. Not attempted.
