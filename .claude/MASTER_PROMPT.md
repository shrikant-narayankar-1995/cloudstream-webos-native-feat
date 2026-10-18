# MASTER PROMPT — CloudStream-Compatible Native LG webOS TV App

You are the lead engineer and implementation agent. Investigate, design, implement, test, and document a native LG webOS TV application based on the Native-Webos Native App Kit. The target is to run existing CloudStream `.cs3` plugins unchanged, entirely on the TV.

## Non-negotiable requirements
1. Use https://github.com/Native-Webos/webos-native-app-kit as the primary native application foundation. Inspect its current repository before coding. Do not invent APIs or replace its build/package workflow without evidence and a written reason.
2. Target retail LG webOS TV initially, with LG C1 as reference hardware where available.
3. The UI and application orchestration must use the kit's native C/C++/SDL/OpenGL ES stack and supported TV APIs wherever the kit permits.
4. Existing `.cs3` provider code and plugin packages must remain unchanged.
5. Plugin execution must occur on the TV. No remote runtime, companion PC, cloud service, or external execution server.
6. Do not claim the kit provides Android DEX/ART support. Prove all runtime capabilities.
7. A rewritten C provider, mock, synthetic class, or protocol-only demo is not success.
8. Do not implement the full UI before the runtime feasibility gate passes. A separate UI shell is permitted only if clearly labelled as not yet CloudStream-compatible.
9. Work in small phases, keep changes reviewable, and run tests after each change.
10. Do not run untrusted downloaded plugin code on a production TV without a test plan and explicit approval.

## Start-up
- Inspect the entire Native App Kit: README, `AGENTS.md`, `docs/AGENT-APP-GUIDE.md`, milestones, compatibility records, licensing docs, third-party notices, CMake files/presets, examples, tools and tests.
- Inspect CloudStream's plugin loader, plugin base/lifecycle, provider API, repository manager, manifest format, and an actual plugin artifact.
- Record exact commit IDs and inspection date.
- Verify commands against the current checkout instead of assuming this prompt is always up to date.
- Run the kit's documented doctor, tests, build, package verification, installation, launch, logging and device-test workflows where configured.

## Phase 0 — Evidence-based reconnaissance
Produce:
- `docs/research/kit-audit.md`
- `docs/research/kit-baseline.md`
- `docs/research/cloudstream-loader-audit.md`
- `docs/research/plugin-format-audit.md`
- `docs/research/platform-capability-matrix.md`
- `docs/research/dependency-inventory.md`

Cite source paths, symbols, commands, outputs or device observations. Mark facts, hypotheses, unknowns and assumptions distinctly.

## Phase 1 — Hard runtime feasibility gate
Determine whether an appropriate Android-compatible runtime can run on retail webOS under the actual install/privilege model. Verify:
- Android ART/Dalvik or another genuinely compatible runtime is available and can be packaged legally and technically.
- The runtime loads and executes the plugin's original DEX.
- CloudStream API classes and compatible signatures can be provided without modifying the plugin.
- Android framework APIs used by the plugin work.
- Kotlin stdlib, coroutines, serialization, HTTP/HTML/JavaScript libraries and transitive dependencies resolve.
- Reflection, class loading, lifecycle, resources, native libraries, threads and networking work as needed.
- ABI, memory, CPU, filesystem, sandbox and security constraints are acceptable.
- All runtime components can be redistributed under compatible licenses.

Build a minimal proof of concept with one real, inspected, unmodified `.cs3` plugin. Demonstrate actual entry-class loading, lifecycle invocation, provider registration and one real provider operation on the TV. Save commands, logs, versions, package hashes and results.

Insufficient evidence: ZIP parsing, manifest parsing, finding DEX magic bytes, JVM hello-world, synthetic class execution, desktop-only success, mocked results, translated providers, or remote execution.

Create `docs/FEASIBILITY_DECISION.md` with exactly one status:
- `PASS`: original plugin executes on target TV unchanged and a real provider operation succeeds.
- `BLOCKED`: exact blocker and evidence.
- `INCONCLUSIVE`: a named test remains, with a reproducible next step.

Do not proceed to full product implementation until `PASS`. If blocked, stop and report the conflict between requirements. Do not silently relax constraints.

## Phase 2 — Native app skeleton (only after PASS)
Use the kit's real app shell, input/action model, focus manager, UI toolkit, async facilities, networking, storage, diagnostics and media interfaces. Keep app-specific code separate from kit core. Avoid duplicate services already provided by the kit.

Suggested app-specific modules, adapted to actual kit conventions:
- `app/`: startup, state, navigation and lifecycle
- `plugins/`: repository management, installation, updates and removal
- `plugin_archive/`: safe ZIP handling, manifest parsing and validation
- `runtime/`: local plugin execution and lifecycle bridge
- `cloudstream_compat/`: required API compatibility
- `provider/`: provider invocation and result mapping
- `net/`: headers, cookies, redirects, cancellation and request policy
- `catalog/`: search, catalog, details and episodes
- `extractor/`: extractor registration and stream resolution
- `player/`: native media integration
- `storage/`: settings and metadata
- `tests/`: host, integration, package and device tests

## Phase 3 — Repository and plugin manager
Implement:
- repository add, validation, refresh and removal
- repository metadata and plugin listing
- download to temporary file, supplied-hash verification, safe finalization and rollback
- safe ZIP path and size checks
- plugin install/update/enable/disable/uninstall
- persistent metadata and compatibility reporting
- clear errors for malformed archives, missing classes, missing dependencies and runtime exceptions

## Phase 4 — Provider operations
Invoke original provider APIs; do not rewrite providers:
- provider discovery, lifecycle and cleanup
- homepage/catalog
- search
- content details
- episodes
- link extraction
- subtitle extraction
- extractor registration
- cancellation, timeouts and structured errors

Detect unsupported operations rather than pretending all plugins implement everything.

## Phase 5 — Native TV experience and playback
Use the kit's actual supported native APIs:
- home/catalog screen
- provider/repository manager
- search and on-screen keyboard
- result grids/posters
- content details and episode selection
- player controls, subtitles and audio selection
- settings and diagnostics
- remote-control navigation and Back handling
- loading, empty, error and retry states

Preserve stream URLs and relevant headers/cookies/referer/user-agent where the playback backend supports them. Report unsupported formats accurately.

## Security and licensing
- Treat plugins as untrusted executable code.
- Hash verification only proves consistency with a supplied hash; it does not prove publisher trustworthiness.
- Never disable TLS verification.
- Reject ZIP traversal, unsafe links, excessive sizes and archive bombs.
- Never log tokens, cookies or credentials.
- Apply resource/time limits where possible and document limits of in-process isolation.
- Audit the kit's GPL obligations and every runtime/API/dependency license before redistribution.

## Testing and reporting
Maintain host tests, target builds, package verification, TV installation/launch, real plugin loading, real provider operation, performance measurements, security tests, and a compatibility matrix.

At the end of every work session report:
1. changes and files
2. commands and exit codes
3. tests passed/failed/skipped
4. device/firmware/toolchain
5. evidence and artifact paths
6. feasibility gate status
7. blockers and next tasks

## Final deliverables
- reproducible source/build
- setup instructions
- research and feasibility reports
- dependency/license inventory
- real plugin proof-of-concept logs
- native app and plugin management if the gate passes
- automated tests and real-device validation
- compatibility matrix
- troubleshooting guide
- verified release package

## Definition of done
An unmodified `.cs3` plugin loads and executes its original provider code locally on the target TV, returns actual results, and feeds them to the native app. No mock, port, remote execution, or modified provider satisfies this requirement.

Begin with repository reconnaissance. Do not begin by generating a large UI or claiming runtime compatibility. If the runtime is infeasible, stop at the gate and deliver a rigorous evidence-based report.
