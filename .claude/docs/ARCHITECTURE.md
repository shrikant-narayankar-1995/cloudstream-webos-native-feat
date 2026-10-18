# Architecture Proposal

## Principle
Reuse the Native App Kit's actual application shell, SDL/input/focus/UI, networking, storage, diagnostics and media facilities. Keep CloudStream-specific features in an app-specific layer. Do not duplicate kit services without evidence.

## Desired flow (not proof of runtime feasibility)
```text
Native webOS app (kit shell + native UI)
  ├── Repository/plugin manager
  ├── Provider UI and catalog model
  ├── CloudStream compatibility facade
  │     └── Local Android-compatible plugin runtime (feasibility gate)
  │           ├── CloudStream API classes
  │           ├── Kotlin runtime/dependencies
  │           ├── Android framework dependencies
  │           └── Unmodified .cs3 DEX/provider code
  ├── Native networking/storage where appropriate
  └── Kit media controller / supported TV playback backend
```

This is the target design, not an assertion that Android runtime support exists on webOS.

## Suggested app modules
Adapt to kit conventions after source inspection:
- `app/`: lifecycle, navigation, state
- `plugins/`: repositories and installed metadata
- `archive/`: safe ZIP and manifest processing
- `runtime/`: capability probe and local execution bridge
- `compat/`: required CloudStream API compatibility
- `provider/`: operation dispatch and results
- `catalog/`: search/catalog/detail/episode models
- `extractor/`: extractor registry and stream metadata
- `player/`: playback adapter and subtitles
- `tests/`: host and device tests

## Runtime interface
A versioned interface may support capability discovery, load/unload, provider metadata, search, catalog, details, episodes, extraction, subtitles, cancellation and structured errors. IPC is acceptable only if it stays on-device and is supported by the target. A JSON protocol alone does not implement CloudStream or Android APIs.

## Kit integration
Inspect the current source and docs for app examples, UI/input/focus, async, networking, storage, media, diagnostics, CMake, and build/verify/install/launch/log/device-test scripts. Use actual APIs after inspection; this document intentionally does not invent function signatures.

## Error categories
Invalid archive; missing manifest; unsupported bytecode/runtime; missing class/dependency; constructor/lifecycle/provider failure; network failure; no playable stream; unsupported media; cancellation/timeout.

## Security boundary
Plugins are executable code. Unless the runtime provides a genuine sandbox, assume plugins may have the app's effective permissions.
