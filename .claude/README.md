# CloudStream webOS Native App — Agent Pack

This pack is for Claude Code or another coding agent. It contains a master prompt, repository-level agent rules, architecture, feasibility gate, dependency strategy, task breakdown, testing criteria, and status template.

## Goal
Build a native LG webOS TV application using https://github.com/Native-Webos/webos-native-app-kit, with the goal of running existing CloudStream `.cs3` providers unchanged, entirely on the TV.

## Critical feasibility warning
The native app kit does not itself establish Android DEX/ART compatibility. CloudStream plugins may contain Android DEX bytecode and require CloudStream APIs, Kotlin libraries, Android framework APIs, and third-party dependencies. A standard JVM does not automatically execute Android DEX or provide Android APIs.

Therefore, prove that a real, unmodified `.cs3` plugin executes on the actual target TV before building the full product. If this cannot be done under the on-device constraints, report the blocker honestly. Do not substitute a C port, mock provider, synthetic class, remote service, or modified plugin and call it success.

## Start here
1. Read `MASTER_PROMPT.md`.
2. Read `AGENTS.md`.
3. Read `docs/ARCHITECTURE.md`, `docs/FEASIBILITY_GATE.md`, `docs/DEPENDENCIES.md`, and `tasks/TASK_BREAKDOWN.md`.
4. Clone and inspect the Native App Kit. Follow its current README and `docs/AGENT-APP-GUIDE.md`.
5. Run the kit's documented doctor, tests, build, package verification, and device workflow before modifying anything.
6. Begin with Phase 0 and Phase 1 only.

## Primary repositories
- Native foundation: https://github.com/Native-Webos/webos-native-app-kit
- CloudStream: https://github.com/recloudstream/cloudstream
- Plugin loader: https://github.com/recloudstream/cloudstream/blob/master/app/src/main/java/com/lagradost/cloudstream3/plugins/PluginManager.kt
- Plugin base: https://github.com/recloudstream/cloudstream/blob/master/app/src/main/java/com/lagradost/cloudstream3/plugins/Plugin.kt
- Provider API: https://github.com/recloudstream/cloudstream/blob/master/library/src/commonMain/kotlin/com/lagradost/cloudstream3/MainAPI.kt
- Repository manager: https://github.com/recloudstream/cloudstream/blob/master/app/src/main/java/com/lagradost/cloudstream3/plugins/RepositoryManager.kt

## Package contents
- `MASTER_PROMPT.md`: paste into Claude Code to start the project.
- `AGENTS.md`: persistent agent instructions.
- `docs/ARCHITECTURE.md`: proposed modular architecture.
- `docs/FEASIBILITY_GATE.md`: mandatory runtime proof.
- `docs/DEPENDENCIES.md`: dependency inventory method.
- `docs/TESTING_AND_ACCEPTANCE.md`: test strategy.
- `tasks/TASK_BREAKDOWN.md`: phased tasks and deliverables.
- `tasks/DEFINITION_OF_DONE.md`: acceptance checklist.
- `tasks/STATUS_TEMPLATE.md`: status report template.

## Licensing
Review the kit's current GPL-3.0-or-later license, `LICENSING.md`, and `THIRD-PARTY-NOTICES.md` before redistribution. Review CloudStream, the runtime, plugin API, and all dependency licenses separately.
