# Agent Status Report

Date/time: 2026-10-19
Branch/commit: `main` (project repository; see `git log`)
Native App Kit commit: `e10f35ecd6e27987cc866b28caea6be66a023fcc` + `docs/kit-patches/0001`, `0002`
CloudStream: source `eb962b4fb7b1387b8de42ec58000cee0a01fe741`; pre-release APK sha256 `add4b760…6d83`
Runtime: Alpine art_standalone 0_git20251009-r3 (aarch64 on the TV, x86_64 on the host)
Target TV / firmware: LG OLED65C1PTB, webOS 6.5.3 (kisscurl-koli), firmware 03.53.45, Developer Mode
Toolchain / NDK: arm-webos-linux-gnueabi gcc 12.2.0; OpenJDK 17 + D8 8.5.35 and clang/lld in alpine:edge containers

## Current phase and gate
Phase: 1 complete (feasibility gate)
Runtime feasibility: **PASS** (scoped; see docs/FEASIBILITY_DECISION.md)
Reason: unmodified Dailymotion and LibriVoxAudiobook plugins loaded, registered and returned real
search/details/links results inside the bundled ART runtime on the TV.

## Completed work
- Phase 0 audits, TV platform probe, runtime evaluation (earlier commits).
- Android compat layer + csloader (CloudStream's loadPlugin sequence) with 165 host tests.
- CNCVerse survey: 37 plugins, all Android `Plugin`; load-only pass 38/40 load and register (after the
  headless-Activity reload).
- Plugin resources (`requiresResources`: resources.arsc, binary XML) and a conformance test plugin.
- Plugin settings/dialog bridge: `--op settings`, AlertDialog/LayoutInflater/framework widgets described
  to the app and answered over stdin (docs/runtime/dialog-protocol.md); verified on host ART with the
  conformance plugin (prefs written by the plugin, visible on next load).
- Relocatable, pruned aarch64 runtime (50 MiB + 62 MiB CloudStream DEX), kit private-runtime packaging.
- On-TV plugin execution (probe 0.2.0).

## Commands executed (this phase)
| Command | Exit | Notes |
|---|---:|---|
| `docker run … runtime/build.sh` | 0 | compat tests 165/165, csweb-host.dex |
| `docker run … runtime/make-rootfs.sh aarch64/x86_64` | 0 | apk signature-checked roots |
| `python3 runtime/make_dist.py …` | 0 | dists 110 / 113 MiB |
| `runtime/run-batch-host.sh evidence/plugins/all …` | 0 | load-only 38/40 |
| `runtime/dist-run.sh runtime/dist/runtime-x86_64 … --op settings --ui stdin` | 0 | conformance settings answered, prefs saved |
| `python3 -m unittest discover -s tests/tools` (kit) | 0 | 33 passed |
| `tools/build-webos.sh` (with probe) | 0 | 0 warnings, check-package passed |
| `tools/verify.sh … --private-runtime runtime-aarch64` | 0 | 7/7 |
| `tools/install.sh … --private-runtime runtime-aarch64`, `launch.sh`, `logs.sh` | 0 | TV run |

## Tests
- Passed: compat 165; project tools 8; kit tool tests 33; kit host suite incl. csweb probe test; TV runtime tests 4/5
- Failed: TV InternetArchive search (wolfSSL vs archive.org chain, known runtime defect)
- Skipped: provider operations for plugins sourcing unlicensed content (by decision); webosbrew-ipk-verify (not installed)

## Evidence
- TV: `evidence/device/20261019-075624-runtimeprobe-0.2/`
- Host runs: `evidence/runtime-host/runs/`, `load-only/`, `relocated/`
- Plugin hashes: `evidence/plugins/*/SHA256SUMS`; probe ipk `evidence/baseline/probe-0.2-ipk-sha256.txt`

## Risks/blockers
- ~10 s per operation with a fresh runtime; memory ~120 MiB per process.
- TLS failures for some chains (archive.org); WebView, custom/AndroidX views and fragments unsupported.
- Resources, headless Activity and settings are verified on host ART only; not yet re-run on the TV.
- Licence audit of the bundled runtime still metadata-only (needed before any redistribution).
- Kit patches 0001/0002 live in this repo; consider proposing 0002 upstream.

## Next tasks
0. Rebuild the probe package with the new runtime and run conformance (resources, settings) on the TV.
1. Long-lived runtime service (one ART process, JSON requests over stdin/stdout) and a host-precompiled arm64 boot image.
2. Fix TLS for archive.org in the runtime layer (BouncyCastle JSSE provider or wolfSSL build).
3. Phase 2: native app skeleton on the kit (repositories, plugin install with hash checks, search UI) using the runtime service.
