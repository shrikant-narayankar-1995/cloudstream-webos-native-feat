# Kit patch 0002: private runtimes in packages

Applies to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc` after patch 0001.
Patch: `0002-private-runtimes.patch` (cmake/WebOSPackage.cmake, tools/check-package.py, tools/verify.sh, tools/install.sh,
tests/tools/test_packaging.py).

## What
- `wnk_add_webos_package(... PRIVATE_RUNTIMES <dir>...)` copies each directory into the package as
  `<basename>/` and passes `--private-runtime <basename>` to `check-package.py` and `verify.sh`.
- `check-package.py --private-runtime DIR` (repeatable): ELF files under `DIR/` are exempt from the
  `lib/` bundling rules and are checked instead with a separate policy:
  - machine is ARM or AArch64 (for `--arch arm`);
  - every `DT_NEEDED` of every ELF in the tree resolves to a file name or SONAME inside the tree
    (the tree is self-contained: its own loader and C library);
  - no LG firmware library names (`DENY_IN_PRIVATE_RUNTIME`);
  - no ELF outside the tree has an RPATH/RUNPATH into it.
- `verify.sh --private-runtime DIR` passes the option to `check-package.py`; `install.sh PACKAGE.ipk --private-runtime DIR` passes it to `verify.sh`.

## Why
The CloudStream plugin runtime is Android's ART built for musl (Alpine `art_standalone`), run as a
separate process through its own bundled loader. The kit's rules against bundling `libstdc++`, `libgcc_s`,
`libc` or `libEGL` exist because a bundled copy on the app's library path would shadow the TV's copy
for LG's libraries. A private runtime never shares an address space with LG libraries and is never on the
app's library path, so those reasons do not apply; the self-containment and RPATH rules check exactly that.
LG libraries stay forbidden everywhere. Nothing changes for packages that do not declare a private runtime.

## Verification
- `python3 -m unittest discover -s tests/tools`: 33 passed (4 new tests: self-contained tree passes,
  missing dependency fails, LG library name fails, tree without ELF files fails).
- `org.csweb.runtimeprobe_0.2.0_arm.ipk`: check-package all checks passed; verify.sh 7/7.
- `org.wnk.examples.hello_0.8.0_arm.ipk` still verifies without the option.
