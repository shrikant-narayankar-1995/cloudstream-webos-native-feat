# Feasibility decision

**Status: INCONCLUSIVE**

Date: 2026-10-19. Target: LG OLED65C1PTB, webOS 6.5.3, firmware 03.53.45, Developer Mode (unrooted).

## Named remaining test

**T1 → T3 of `docs/research/runtime-test-plan.md`**: run Alpine's prebuilt `art_standalone` (aarch64)
as a child process of the native app on the TV (T1), then load the unmodified
`DailymotionProvider.cs3` (sha256 `9036525a64e8b3c8fe04f94e9fe89a744c13d69af684ed0d2ad2a7eb0f332cd9`)
through the upstream loading sequence and execute a real `search` (T2 host, T3 TV).
T2 and T3 execute third-party plugin code and **need explicit approval** (see the plan).

## What is established

| Gate item | Status | Evidence |
|---|---|---|
| A process other than the app can run on the TV (exec from app dir and tmpfs) | proven `[HW]` | platform-capability-matrix.md |
| 64-bit AArch64 processes run on the TV | proven `[HW]` | `exec_aarch64 status=exit 42` |
| JIT (RWX, W^X) and AOT (file-backed exec mmap on tmpfs) mappings allowed | proven `[HW]` | `jit_rwx`, `jit_wx`, `file_exec_map_tmp` = ok |
| Threads, `/dev/urandom`, outbound TCP from a child | proven `[HW]` | probe log |
| An Android-compatible runtime that executes DEX on (musl) Linux exists, packaged, Apache-2.0 | proven on host only `[TEST]` | `evidence/runtime-host/artprobe-x86_64.log` |
| Test plugins need no Android framework classes | proven statically `[TEST]` | plugin-format-audit.md (0 `android.*` refs; `BasePlugin`) |
| Every CloudStream API signature they reference exists in the upstream JVM API | proven statically `[TEST]` | 172/172 references resolve |
| Runtime loads original DEX **on the TV** | **not yet tested** | T1/T3 |
| Original entry class, lifecycle, provider registration, real operation | **not yet tested** (anywhere) | T2/T3, approval required |
| Memory: ~436 MB available beside a small app | measured; runtime footprint **unknown** | T1 |
| Redistribution of runtime components | metadata audit only | dependency-inventory.md |

## Known risks found so far
- TLS: the candidate runtime's wolfSSL JSSE fails on archive.org's certificate chain (works for 7 other
  hosts). Blocks InternetArchiveProvider until fixed in the runtime layer; does not block Dailymotion.
- libcore `java.util.stream` collectors fail (`BootstrapMethodError`); impact on CloudStream code paths unknown.
- ART aborts on exit (shutdown thread); harmless if the harness halts explicitly.
- Packaging a non-ARM32 runtime requires a documented extension of the kit's package checks.
- Coverage: `Plugin` (Android `Context`) plugins and `requiresResources` plugins need an Android framework
  subset and are **not covered** by this design; many third-party repositories use them. The compatibility
  claim, if T3 passes, is limited to `BasePlugin` plugins without resources.

## Not done (by rule)
No product UI, repository manager or provider integration has been built. No plugin code has run.
