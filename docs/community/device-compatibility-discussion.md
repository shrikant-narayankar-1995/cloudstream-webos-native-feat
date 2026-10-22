# Device compatibility: report working and failing LG TVs

Help us find out which combinations of TV, firmware and CSWeb TV build work.

Please start a **new discussion in Compatibility** for your device, using the report form.
If a matching report already exists for your model and build, add your results there instead.
A suitable title is:

`[Working / Partial / Not working] LG <model> — webOS <version> — <main / legacy>`

Include your full TV model, webOS and firmware versions, package variant, exact app build/link,
extension versions, and test date. Mark install, runtime readiness, repository access, provider load,
search, details and playback separately as **Pass / Fail / Not tested / Not applicable**.
For playback, also record audio, seeking and subtitles.

**Both successes and failures are useful.** “The app opens” or “the extension installs” does not
establish that providers or playback work. A CI pass is not an on-TV compatibility report.

Attach `events.log` and `runtime.log` if available, collected before rebooting:

- Main: `/tmp/org.csweb.tv/`
- Legacy: `/tmp/org.csweb.tv.legacy/`

Remove credentials, tokens, cookies, PINs and private stream URLs before posting. If a log is
unreadable, report the permission error and attach the files you can read; no root is needed.

See the [compatibility guide](https://github.com/sostk/cloudstream-webos-native/blob/main/docs/compatibility.md)
for the full checklist and copyable template. For a reproducible crash or regression, open a bug
issue and link it from your device report. Update your original report when a newer build fixes it.
