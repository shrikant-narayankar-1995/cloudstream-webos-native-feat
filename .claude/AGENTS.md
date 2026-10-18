# Agent Rules

## Purpose
Build a native LG webOS TV app based on `https://github.com/Native-Webos/webos-native-app-kit`, investigating execution of unchanged CloudStream `.cs3` plugins entirely on-device.

## Workflow
- Read `MASTER_PROMPT.md`, architecture, feasibility, dependency and task documents before implementation.
- Inspect the current kit checkout and its own agent guide, README, milestones, compatibility records, licensing and scripts.
- Follow the kit's real build, test, package and device workflows.
- Record exact repository commits, device model, firmware, toolchain and commands.
- Work in small phases and update project status after each phase.
- Never skip the runtime feasibility gate.

## Hard rules
- No remote/companion runtime.
- No rewritten or modified providers.
- No mocks presented as plugin execution.
- No compatibility claim without an unmodified real `.cs3` running on target hardware.
- Do not assume a JVM is equivalent to Android ART or loads DEX directly.
- Do not assume a kit API exists; inspect source and examples.
- Do not replace kit build infrastructure without documented justification.
- Do not execute downloaded plugin code on a production device without an explicit test plan.
- Never log secrets, cookies, passwords or sensitive headers.

## Code quality
Follow project formatting and compiler warning conventions. Prefer kit facilities over duplicating networking, storage, async, UI, media or diagnostics. Add tests for changes. Validate archive paths, sizes and metadata. Keep platform-specific code behind clear interfaces. Report unsupported capabilities explicitly.

## Required task report
At the end of each task: summary, files changed, commands/results, tests/failures, evidence, risks/blockers and next step.

## Licensing
Review Native App Kit GPL-3.0-or-later and third-party notices. Audit CloudStream/API and all bundled runtime/dependency licenses before redistribution.
