# Plugin runtime inputs: pinned, mirrored, checked weekly

Status 2026-10-20. The runtime (`runtime-aarch64/` in the TV package) is built from third-party files
whose upstream copies do not stay put:

- CloudStream's `pre-release` release is replaced on every CloudStream build;
- Alpine edge drops a package's old build when a new one lands.

So the files the runtime was tested with on the TV are pinned and kept.

## What is pinned

`runtime/inputs.lock`: one line per file, `<sha256>  <path>  <upstream URL>`.

| Input | Files | Checked by |
|---|---|---|
| CloudStream pre-release (`eb962b4`) | `app-prerelease-release.apk`, `classes.jar`, `app-sources.jar` | SHA-256 |
| Alpine edge packages, aarch64 (the runtime) and x86_64 (host runs, trust store) | the 45 + 43 `.apk` files of `runtime/rootfs-<arch>.manifest` | SHA-256, then apk's own signature check |

Pinned in the scripts themselves:

- `runtime/build.sh`: Maven jars and R8 (SHA-256);
- `runtime/native/build-wolfssl.sh`: wolfSSL (SHA-512);
- `test-plugins/conformance/build.sh`: its toolchain (`out/TOOLCHAIN-SHA256`).

The build containers (`alpine:edge`, `eclipse-temurin:17-jdk`) and the compilers in them are not pinned. They
affect how our own small parts are compiled (the native helpers, wolfSSL, the compat DEX), not which
third-party files ship.

On 2026-10-20 every pinned Alpine package was compared with the root filesystem the TV runtime was
built from (apk's checksum of each package): all 88 identical. A root built from them has all 497
package files byte-identical to it.

## Getting them

`tools/fetch-runtime-inputs.sh [<dir>]` (default `~/.cache/csweb-runtime-inputs`) fetches each file:

1. from the mirror release named in `inputs.lock` (`runtime-inputs-N` on this repository; asset name =
   path with `/` → `_`);
2. failing that, from its upstream URL.

Every file is checked against the lock. `tools/build-runtime.sh` then builds the runtime from them in
the documented order:

1. rootfs (`make-rootfs.sh <arch> /apks`: no network, fails unless the result is the manifest);
2. trust store (`make-trust.sh`);
3. native helpers, wolfSSL, compat DEX;
4. `make_dist.py`;
5. ahead-of-time compilation for `/tmp/org.csweb.tv/rt`;
6. the licence check.

## Trust store

`runtime/make-trust.sh` extracts Mozilla's CA list from the pinned `ca-certificates` with p11-kit, as
Alpine's `java-cacerts` does. p11-kit stamps each entry with the current time, so the stamps are set to
the `ca-certificates` release date and the keystore digest recomputed. The file is then the same on
every build. Same 121 certificates and aliases as the store the TV was tested with; `keytool` accepts
its digest.

## Licence notices

`runtime/licenses/` (copied into `runtime-aarch64/licenses/`):

- `NOTICES.md`: every part of the runtime, its licence and where its source is;
- `packages.tsv`: the Alpine packages whose files ship, from apk's database: version, licence, aports
  commit;
- `texts/<origin>/`: the licence files from each package's own source.

Regenerate them after moving the pins:

```
python3 runtime/tools/notices.py packages runtime/rootfs/aarch64 runtime/dist/runtime-aarch64
docker run --rm -v $PWD:/p [-v <dir>:/sources] alpine:edge sh /p/runtime/collect-licenses.sh
python3 runtime/tools/notices.py render
```

`collect-licenses.sh` takes each recipe at its aports commit and lets `abuild` fetch and check its sources
(SHA-512 from the recipe). With `/sources` mounted it also keeps those recipes and sources, for publishing
next to the package as the GPL parts require. `notices.py check` (run by `build-runtime.sh`) fails when the
notices no longer describe what ships.

## CI

- `ci.yml` (pull requests, main): builds the runtime from the pinned inputs, cached until `runtime/` or
  the lock changes, then the full package.
- `release.yml` (by hand, with a version `x.y.z`): runs `ci.yml` with that version and publishes the
  package as release `v<version>` (from main only; the tag must be new).
- `upstream-latest.yml` (Mondays, or by hand): builds from the latest Alpine edge and CloudStream
  pre-release (`build-runtime.sh --latest`). It reports what moved in the run summary and uploads an
  untested package.

## Moving the pins

1. Install the weekly untested package on the TV and run the usual checks.
2. When it works: rebuild with `--latest` locally and regenerate the notices.
3. Update `inputs.lock` (new files, new SHA-256 values) and `evidence/cloudstream-app/` (SHA256SUMS,
   RELEASE_INFO).
4. Upload the new files to a new `runtime-inputs-N+1` release, and open a pull request.
