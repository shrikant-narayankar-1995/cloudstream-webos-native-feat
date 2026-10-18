# Runtime feasibility test plan (Phase 1)

Status: **awaiting approval** for the steps marked ⚠ (they execute third-party code). Date 2026-10-19.

## Candidate design (from evidence so far)

```
webOS native app (kit shell, ARMv7 glibc, jailed uid)
  └─ fork/exec ──► runtime/aarch64/lib/ld-musl-aarch64.so.1  (bundled musl loader, 64-bit process)
                     runtime/aarch64/usr/bin/dalvikvm  (Alpine art_standalone, unmodified)
                       -Xbootclasspath: art_standalone libcore (core-oj, core-libart, okhttp, bouncycastle, wolfssljni…)
                       -cp: csloader.dex  +  cloudstream-api.dex (upstream JVM artifacts, dexed with D8, unmodified source)
                       csloader: PathClassLoader(<plugin>.cs3) → manifest.json → loadClass → newInstance
                                 → load() → APIHolder.allProviders → MainAPI.search/load/loadLinks
                       stdout: line-delimited JSON  ◄── read by the native app on a WorkerPool thread
  ANDROID_DATA=/tmp/<appid>/art  (dalvik-cache on tmpfs)
```

Why aarch64 first: the TV runs 64-bit processes `[HW]`, aarch64 is art_standalone's maintained
non-x86 target, and its Alpine package is the newest revision (`-r3`). armv7 (`-r2`, hard-float musl,
also self-contained) is the fallback.

The plugin `.cs3` is passed to the class loader byte-for-byte as downloaded; hashes are checked before
and after each run.

## Tests

| ID | Where | Executes plugin code | What it proves | Pass criterion |
|---|---|---|---|---|
| T1 | TV | no | The bundled ART starts on the TV, loads a DEX, does HTTPS; memory and time | `ArtProbe` prints `vm=`, `lambda=42`, `future=true`, `https=status=200` for an ECDSA host; child VmHWM and wall time recorded |
| T2 ⚠ | host (disposable `alpine:edge` x86_64 container, network only, no mounts except read-only inputs and an output dir) | **yes** | Unmodified plugin loads through the real upstream loading sequence and a real provider operation returns data | entry class resolved, `load()` ran, ≥1 `MainAPI` registered, `search()` returns ≥1 real result, plugin hash unchanged |
| T3 ⚠ | TV | **yes** | T2 on the target, launched by the native app, results shown natively | as T2, plus process tree shows the runtime is a child of the app; no network peer other than the provider's hosts |
| T4 | TV | yes (after T3) | unload/reload, timeout kill, malformed archive rejection | no crash of the app; errors reported |

T1 is safe to run without plugin approval but needs a packaging decision (below).

## Plugin execution safety (T2, T3)

- Plugins: `DailymotionProvider.cs3` (sha256 `9036525a…2cd9`) first, because its host's TLS chain works
  with the runtime; `InternetArchiveProvider.cs3` (sha256 `54f3de56…31eb`) after the archive.org TLS
  failure is fixed in the runtime layer. Both from the official `recloudstream/extensions` builds branch.
- Queries: Dailymotion `"nasa"`, Internet Archive `"nosferatu"` (1922, public domain).
- Host: throwaway container, `--rm`, inputs mounted read-only, no host credentials, no cookies.
- TV: runs as the app's unprivileged jailed uid; writes only tmpfs; no accounts, cookies or secrets.
- Hard timeout 120 s; the app kills the child process group.
- Logs record URLs only by host name; no headers, cookies or tokens are logged.
- After the session: `ares-install --remove` of the test app on request.

## Decisions needed

1. ⚠ Approval to execute the two unmodified official plugins in T2 (host container) and T3 (TV).
2. Runtime sourcing: ship Alpine's prebuilt, signed `art_standalone` aarch64 binaries (≈55 MiB) in the
   ipk, instead of building ART ourselves (upstream supports only self-hosted builds; host has no
   qemu/binfmt and 26 GiB free disk).
3. Packaging policy: the kit's `check-package.py` rejects any non-ARM32 or libstdc++-needing ELF. A
   bundled runtime needs a documented extension: ELFs under `runtime/<arch>/` are allowed to be AArch64
   (or armhf) and to need their own bundled `libstdc++.so.6`, because they run in a separate process
   with their own loader and never share an address space with LG's libraries (the reason for the
   rule). The checker would verify that tree separately (own loader present, every DT_NEEDED resolves
   inside the tree, no LG library names).
4. CloudStream API artifacts: build the `library` JVM jar from the pinned source in a Docker JDK
   container (reproducible; several GiB of Gradle downloads), or fetch the published JitPack artifact.

## Known runtime defects to work around or fix (runtime layer only, never the plugin)

| Defect | Plan |
|---|---|
| archive.org handshake fails in wolfSSL JSSE (ASN.1 parse, legacy SHA-1 cross-signed chain) | try BouncyCastle JSSE (`bctls`) as first provider, or a wolfSSL build/config fix; re-run TLS matrix |
| `java.util.stream` collectors in libcore throw `BootstrapMethodError` | measure whether CloudStream/OkHttp/Jackson/Jsoup paths hit it; if so, rebuild core-oj desugared or report upstream |
| SIGABRT in ART shutdown thread after `main` | harness flushes stdout and calls `Runtime.halt(0)`; report upstream |
| `libEGL.so.1` pulls Mesa/LLVM | provide a minimal `libEGL.so.1` (libglvnd dispatcher) or show it is never loaded |
