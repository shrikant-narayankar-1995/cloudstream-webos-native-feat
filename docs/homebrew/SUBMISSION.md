# Homebrew Channel submission (draft, not submitted)

The listing is `org.csweb.tv.yml` here; it goes to `packages/` of
https://github.com/webosbrew/apps-repo by pull request. The repository's rules (checked 2026-10-20):

1. **No AI slop.** Apps made primarily by AI, without meaningful human development, testing and
   review, are rejected. AI use is allowed but must be disclosed in the pull request.
2. **No piracy.** Nothing related to piracy or breaking DRM; IPTV only if legal.
3. Do not break users' TVs.
4. Ports follow the original project's licence.

## Before submitting

- [ ] A full release (not a pre-release) made with the Release workflow, so that
      `releases/latest/download/org.csweb.tv.manifest.json` resolves. The `runtime-inputs-N`
      releases are pre-releases and never become "latest".
- [ ] That release installed through the Channel's manifest on a TV (Homebrew Channel can add a
      custom repository for testing) and checked by you: first start, profiles, adding a
      repository, installing an extension, playback, update over an older version.
- [ ] Screenshots from the TV, committed under `docs/homebrew/screenshots/`, showing only
      legitimate sources (YouTube, Internet Archive, Invidious, own media). Nothing in the listing,
      screenshots, README or app may name or link a piracy repository.
- [ ] The listing's description and `webosRelease` match what was tested.

## Pull request text (draft)

> **Add CSWeb TV (org.csweb.tv)**
>
> CSWeb TV is a native (SDL/GLES) webOS app that runs unmodified CloudStream extensions on the TV, in a
> bundled Android runtime (Alpine's `art_standalone`). It ships no content and no extensions: users add
> extension repositories themselves. Source: https://github.com/sostk/cloudstream-webos-native
> (GPL-3.0-or-later). The package carries the licence notices of everything it bundles; the sources of the
> runtime's third-party parts are published on the repository's `runtime-inputs-N` release.
>
> **AI use:** most of the code was written with an AI coding assistant (Claude) under my direction. I
> reviewed the changes, decided the design and tested the app on my TV (LG OLED65C1, webOS 6.5.3):
> [list what you tested]. CI builds and tests every change (host tests incl. AddressSanitizer, the
> runtime build, the kit's package check).
>
> **Content:** tested only with legitimate sources ([which]). The app does not include, recommend or
> link any extension repository.
>
> The package is about 78 MB, mostly the Android runtime (ART, ICU, a boot image compiled ahead of time
> so it starts quickly on the TV).

Fill in the bracketed parts yourself: they must describe what you actually did.
