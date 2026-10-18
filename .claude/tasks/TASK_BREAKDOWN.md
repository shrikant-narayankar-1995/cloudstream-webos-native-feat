# Task Breakdown and Deliverables

## Phase 0 — Baseline and source audit (first)

### T0.1 Audit Native App Kit
Read README, AGENTS, agent app guide, milestones, compatibility docs, license/third-party notices, CMake presets, examples, build/test/package/install/log scripts.
**Deliver:** `docs/research/kit-audit.md`

### T0.2 Prove baseline
Run the kit's doctor, test, build and package-verification workflows. If a TV is configured, install/launch an example and gather logs.
**Deliver:** `docs/research/kit-baseline.md`

### T0.3 Audit CloudStream loading
Trace repository metadata → download → archive → manifest → class loader → constructor → lifecycle → provider registration → operations → unload. Record Android assumptions.
**Deliver:** `docs/research/cloudstream-loader-audit.md`

### T0.4 Inspect a real plugin
Select a safe test plugin. Record SHA-256, manifest, DEX files, entry class, referenced packages, native libraries and license. Do not execute until approved.
**Deliver:** `docs/research/plugin-format-audit.md`

### T0.5 Dependency/licensing inventory
Trace direct and transitive runtime dependencies and platform assumptions.
**Deliver:** `docs/research/dependency-inventory.md`

## Phase 1 — Hard runtime feasibility gate

### T1.1 Platform capability matrix
Determine runtime, ABI, permissions, process execution, memory/storage, networking and native library capabilities on target TV.
**Deliver:** `docs/research/platform-capability-matrix.md`

### T1.2 Minimal runtime experiment
Test the smallest meaningful DEX loading experiment on target. Avoid full UI.
**Deliver:** source, build steps, logs and results.

### T1.3 Real plugin execution
Load unmodified plugin; instantiate original entry class; invoke lifecycle; register provider; execute one real operation on TV.
**Deliver:** `docs/research/plugin-runtime-test.md`

### T1.4 Gate decision
Write `docs/FEASIBILITY_DECISION.md` with PASS/BLOCKED/INCONCLUSIVE and evidence.
**Deliver:** decision report.

**Stop condition:** If not PASS, stop product implementation and report the blocker. Do not weaken requirements.

## Phase 2 — App skeleton (after PASS)
Create app-specific modules using kit conventions. Reuse app shell, focus, input, UI, async, network, storage, diagnostics and media. Add build/package integration and diagnostics screen.
**Deliver:** buildable package and tests.

## Phase 3 — Repository/plugin manager
Repository add/validate/refresh/remove; metadata parsing; safe download/install; supplied-hash verification; safe ZIP handling; enable/disable/update/uninstall; persistent metadata; explicit errors.
**Deliver:** plugin manager with tests.

## Phase 4 — Runtime and CloudStream compatibility
Runtime lifecycle/capabilities; load/unload; provider registration; search/catalog/details/episodes/extraction/subtitles; cancellation/timeouts; structured errors/logs.
**Deliver:** real provider integration tests.

## Phase 5 — TV UI
Home/catalog; provider/repository manager; search/on-screen keyboard; results; details/episodes; stream selection; settings/diagnostics; remote navigation and Back; loading/empty/error/retry states.
**Deliver:** end-to-end native UI.

## Phase 6 — Playback
Pass URLs and supported headers/cookies/referer to kit media backend; subtitle/audio selection and controls; test actual TV codecs and formats; report unsupported streams.
**Deliver:** playback test matrix.

## Phase 7 — Hardening/release
Security review, resource limits, crash recovery, license audit, clean reproducible build, package verification, install/launch smoke test, compatibility matrix and troubleshooting docs.
**Deliver:** release candidate and release report.

## Required status report per task
Changes, files, commands, test results, device/toolchain, evidence, risks, blockers and next step.
