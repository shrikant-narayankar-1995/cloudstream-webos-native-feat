# Runtime Feasibility Gate

## Why
The Native App Kit provides native application infrastructure; it does not establish Android DEX/ART compatibility. CloudStream plugins may require Android bytecode, CloudStream APIs, Kotlin libraries and Android framework classes.

## Required test
Use an inspected, unmodified `.cs3` package. Record SHA-256, source URL, manifest, entry class, DEX inventory, dependencies and license. Use a test device and explicit approval before executing third-party code.

On the target TV prove:
1. A suitable runtime is available or can be packaged legally and technically.
2. Original DEX loads and executes.
3. Original entry class resolves.
4. CloudStream API classes resolve with compatible signatures.
5. Required Android framework calls work.
6. Kotlin and third-party dependencies resolve.
7. Constructor and original lifecycle execute.
8. Original provider registers.
9. A real provider method returns actual results.
10. All execution occurs on TV.
11. Resource and security constraints are documented.

## Insufficient evidence
ZIP parsing, manifest parsing, DEX magic-byte detection, JVM hello-world, synthetic classes, desktop-only success, mocked results, translated providers, or remote execution.

## Gate decision
- `PASS`: all steps proven on target TV.
- `BLOCKED`: a specific required capability is unavailable or prohibited, with evidence.
- `INCONCLUSIVE`: a named test remains with reproducible next steps.

If blocked, stop product implementation and report the incompatibility honestly. Do not relax constraints without user approval.
