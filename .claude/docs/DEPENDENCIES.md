# Dependency Inventory

## Required inventory fields
For each dependency record exact version/commit, direct/transitive status, build-time/runtime role, Android/JVM/native requirements, ABI, source/license, redistribution status, whether used by the selected test plugin, and evidence of resolution on TV.

## Areas
### Native App Kit
Inspect CMake files/presets, third-party notices, licensing, NDK configuration, runtime library list, package scripts, media/FFmpeg build scripts and app guide. Reuse its dependency management.

### CloudStream
Inspect plugin loader, `BasePlugin`, `Plugin`, `MainAPI`, extractor APIs, repository manager and exact API artifacts expected by the plugin.

### Kotlin/JVM/Android
Investigate Kotlin stdlib, coroutines, serialization, reflection, class loading, Android framework APIs, resources, threading and JNI/native libraries.

### Provider dependencies
Inspect imports and DEX references for HTTP clients, HTML parsers, JSON libraries, JavaScript engines, crypto, browser/WebView APIs and plugin-specific libraries.

### Native libraries
Record `.so` ABI, symbols, Android bionic assumptions and compatibility with the target environment.

## Important
C replacements can support a ported provider, but do not make original compiled plugins compatible. Do not substitute C equivalents for required JVM/Android APIs under the unchanged-plugin requirement.
