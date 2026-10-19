# CSWeb TV

The native LG webOS app: browse, search and play from CloudStream plugins (`.cs3`) that the user
adds, run unchanged on the bundled ART runtime (`runtime/`, docs/runtime/). Built on the Native App
Kit (`external/webos-native-app-kit`) through `WNK_EXTRA_APP_DIRS`. Designs: `docs/design/screens/`.

The app ships no plugins and no content. Repositories and plugins are added by the user.

## Layout

```
apps/csweb-tv/
  README.md            this file: layout, naming, build
  CMakeLists.txt       targets: csweb_core (no UI), csweb_player (media), csweb-tv (executable), tests
  assets/icons/        SVG icons, white and "-dark" variants (packaged as icons/)
  packaging/           expected-needed.txt, icons/ (80×80 icon.png, 130×130 largeIcon.png)
  src/
    main.cpp           AppConfig and wnk::app::run only
    app/               the Delegate, the service bundle handed to screens, screen routing
    core/              small utilities with no kit UI dependency (sha256, child process, zip check,
                       signals, guarded callbacks, URL redaction)
    runtime/           the plugin runtime: launching ART, the JSON-lines service protocol, typed results,
                       CloudStream's episode grouping
    player/            which link plays (CloudStream's priorities), subtitle files, kit media pipeline
    plugins/           repositories, plugin lists, install/update/remove, persistence of .cs3 files
    library/           bookmarks, watch progress, history
    settings/          user settings (DB8-backed)
    ui/theme/          colours, sizes, type scale (one place)
    ui/widgets/        reusable views: nav bar, poster card, hero, shelf, buttons, badges
    ui/dialogs/        plugin dialogs (dialog-protocol.md) drawn with kit widgets
    ui/screens/        one file pair per screen
  tests/               host unit tests, one file per module: test_<module>.cpp
```

## Naming

- Directories and files: `lower_snake_case`. One main type per file pair, `foo_bar.hpp` / `foo_bar.cpp`
  declaring `FooBar`.
- Namespaces follow directories: `csweb::core`, `csweb::runtime`, `csweb::plugins`, `csweb::ui`.
- Screens end in `Screen` (`HomeScreen` in `ui/screens/home_screen.cpp`); widgets are nouns
  (`NavBar`, `PosterCard`).
- Members: `snake_case_` with a trailing underscore; constants `kName`; log category `"csweb"`.
- Store keys (DB8): `csweb.<area>.<name>` in `[a-z0-9._-]`; blob names `<area>-<id>` in `[a-z0-9_-]`.
- Every file starts with `// SPDX-License-Identifier: GPL-3.0-or-later` and a comment saying why it
  exists.

## Rules

- Kit rules apply (docs/AGENT-APP-GUIDE.md in the kit): one main thread for UI, blocking work on the
  pool, `Lifetime::Weak` in callbacks, DB8 for anything durable (no writable filesystem besides
  tmpfs in Developer Mode).
- Plugins are installed only with a SHA-256 recorded at install time and passed to the runtime on
  every load; downloads are size-limited and checked as ZIP archives before they are stored.
- Never log URLs with credentials, cookies, tokens or dialog answers.

## Behaviour and CloudStream

Each screen follows the CloudStream source it corresponds to (external/cloudstream, commit eb962b4);
the comment at the top of each screen names the classes. Known differences:

- Navigation: Home, Search, Library, Downloads, Settings (CloudStream's destinations), not the
  mockups' Movies/Series/Anime/Live tabs. Library and Downloads are not built yet and say so.
- Home's source picker lists every provider with a home page; CloudStream also filters by the
  media-type chips and puts pinned providers first.
- Links are ordered with CloudStream's default quality profile; per-profile priorities, hidden
  sources and quality profiles are not built yet. Torrent and magnet links are listed but cannot
  play (CloudStream's torrent server is not available).
- Plugin settings take effect by reloading the plugin when its settings dialog ends.
- Plugin data (SharedPreferences) lives on tmpfs and is lost at reboot; settings, repositories and
  plugins are in DB8.

## Host test runs

Scripted runs drive the real app with `WNK_CSWEB_KEYS` (kit `Script`), e.g. under Xvfb with
`vblank_mode=0 SDL_VIDEODRIVER=x11` so frames are not throttled. SDL text events carry at most 31
bytes: split typed text into several `text@` steps.

## Build

```
export WNK_EXTRA_APP_DIRS=$PWD/apps/csweb-tv
cd external/webos-native-app-kit
tools/test.sh                     # host build + kit tests + csweb tests
tools/build-webos.sh              # ARM build and ipk (bundles runtime/dist/runtime-aarch64)
```

The runtime directory must be built first (`runtime/build.sh`, `runtime/make_dist.py`,
`runtime/make-aot.sh aarch64 /tmp/org.csweb.tv/rt`).
