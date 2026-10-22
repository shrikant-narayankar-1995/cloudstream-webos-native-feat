# Device compatibility

Compatibility is a combination of TV hardware, webOS/firmware, the installed app build, extension
versions and media formats. Please report devices that work **and** devices that fail.

A successful install, a visible home screen and a loaded provider are separate milestones.
Record each operation you tried; do not mark untested features as working.

## Where to report

Use [GitHub Discussions](https://github.com/sostk/cloudstream-webos-native/discussions) for device
experiences and follow-up questions. Choose **Compatibility** if that category is available,
then start one discussion per device/build combination. Search for your model first and add an
update to an existing matching report where possible.

Discussions is enabled for this repository. Reproducible crashes or regressions should have a
separate [bug issue](https://github.com/sostk/cloudstream-webos-native/issues/new) linked from the
device report.

## What has been demonstrated

| Device / environment | Variant | Evidence and scope |
| --- | --- | --- |
| LG OLED65C1PTB, webOS 6.5.3, firmware 03.53.45 | Main / AArch64 | Recorded on-device runtime execution and real provider operations; see [service protocol evidence](runtime/service-protocol.md#evidence) and [status](STATUS.md). This does not cover every extension, codec or later build. |
| Older TV running webOS 4.4.3 with an ARMv7 kernel | Legacy | Target of ongoing compatibility work. Do not treat the build target or native ARM CI results as a complete TV pass; see [legacy guide](legacy-build.md) and [PR #11](https://github.com/sostk/cloudstream-webos-native/pull/11). |

Community reports should always name the tested app version or commit. Success on one firmware does
not establish compatibility with every TV sold under the same model series.

## Test checklist

1. Install and launch the chosen package. Note whether the UI responds to the remote.
2. Check whether the runtime becomes ready and native HTTP is supported in `events.log`.
3. Add a repository you trust; record whether its extension list loads.
4. Install and load an extension. Record its name/version and whether providers appear.
5. Try a provider home page, search and details.
6. Try playback of media you are authorized to access. Record resolution, codec/container if known,
   audio, seeking, subtitles and any error.
7. Try closing and relaunching the app; note whether settings and installed extensions persist.

Only mark steps you actually performed. For example, “provider appears; playback not tested” is
**partial / not fully tested**, not a playback success.

## Report template

Suggested title: `[Working / Partial / Not working] LG <model> — webOS <version> — <main / legacy>`

Copy this into a discussion or issue:

```text
TV model:
webOS version:
Firmware version:
Kernel architecture (if known):
Installed app version:
Package variant and app ID (main / legacy):
Download link, CI run or source commit:
Test date:

Overall result: Working for tested steps / Partial / Not working / Not fully tested

Mark each as Pass / Fail / Not tested / Not applicable:
- Install and launch:
- Remote navigation:
- Runtime ready:
- Repository / HTTP:
- Extension install:
- Provider load:
- Provider home page:
- Search:
- Details / episodes:
- Playback:
- Audio:
- Seek / resume:
- Subtitles:
- Persistence after relaunch:

Extension and provider names / versions:
Media resolution, codec/container and DRM if known:
Exact failure or error:
Steps to reproduce:
Previous working build, if any:
Logs or screenshots (with private information removed):
```

The `.github/DISCUSSION_TEMPLATE/compatibility.yml` form supplies the same core fields when a
maintainer creates a discussion category with the **Compatibility** name / `compatibility` slug.

## Collect useful logs

Use Dev Manager's file browser, or the kit's log tools on a configured Developer Mode TV.
These paths are temporary, so collect them before rebooting:

| Variant | Directory |
| --- | --- |
| Main (`org.csweb.tv`) | `/tmp/org.csweb.tv/` |
| Legacy (`org.csweb.tv.legacy`) | `/tmp/org.csweb.tv.legacy/` |

Attach `events.log` and, when available, `runtime.log`. Include `crash.log` or `stalls.log`
if generated. The initial device line in `events.log` usually contains the model, webOS release,
firmware and kernel architecture; shell commands are not required just to submit a report.

Older builds may make `runtime.log` unreadable to Dev Manager because the app and Developer Mode
shell run as different users. Report the permission error and your exact build, and attach the
readable `events.log`. New legacy runtime-log permission changes are tracked in
[PR #11](https://github.com/sostk/cloudstream-webos-native/pull/11); do not assume an older package
contains them. Rooting the TV or changing system permissions is not required for a useful report.

Review logs and screenshots before posting. Remove credentials, cookies, authorization headers,
access tokens, PINs and private stream URLs. Keep the timestamps and surrounding error lines.
Do not send SIGQUIT to ART for a thread dump: its signal-dump path has crashed in the legacy build.

## For maintainers: set up compatibility discussions

Repository settings are separate from a pull request. Discussions is enabled; merging these files
does not create a category or publish a thread.

1. Add a **Compatibility** category with the `compatibility` slug and an open discussion format.
   Keep the category open for both successful and failing device reports.
2. Once this PR is merged, verify that “New discussion → Compatibility” shows the report form.
3. Publish [the prepared welcome thread](community/device-compatibility-discussion.md) in that category
   and pin it if desired. Link that thread here once its real URL is known.

Keep reports scoped to their exact builds. When a failure becomes reproducible, link a bug issue
and update the original report after the reporter retests the fix.
