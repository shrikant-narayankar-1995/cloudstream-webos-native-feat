# Native App Kit baseline

Run 2026-10-19 on the development host against kit commit `e10f35ecd6e27987cc866b28caea6be66a023fcc`
(unmodified for steps 1–4; the `WNK_EXTRA_APP_DIRS` hook was added before step 5).

## Host and toolchain

| Item | Value |
|---|---|
| Host | Ubuntu 24.04 x86_64, 12 CPUs, 15 GiB RAM, **26 GiB free disk** |
| cmake / c++ / python | 3.28.3 / GCC 13.3.0 / 3.12.3 |
| Host SDL2 / GLES | 2.30.0 / 3.2 (Mesa) |
| webOS NDK | `~/webos-ndk/arm-webos-linux-gnueabi_sdk-buildroot`, `arm-webos-linux-gnueabi-gcc` 12.2.0 (Buildroot -gd7ed7ee3-dirty) |
| Firmware inventory | `~/.cache/plxnative/fwsym/data` (14 images) |
| ares-cli-rs | in `~/.local/bin`; device `tv` (default), `prisoner@<LAN>:9922`, profile ose |
| Not installed | `webosbrew-ipk-verify` (optional second opinion), JDK, adb, qemu |

## Commands and results

| # | Command (in the kit checkout) | Exit | Result | Log |
|---|---|---:|---|---|
| 1 | `tools/doctor.sh` | 0 | host build requirements present; NDK ok; TV configured | (console) |
| 2 | `tools/test.sh` | 0 | **89/89 tests passed** (unit, Python packaging, renderer pixel, e2e A–D), 212.9 s | `evidence/baseline/test.log` |
| 3 | `tools/build-webos.sh` | 0 | 4 ipks built, each `check-package: all checks passed` | `evidence/baseline/build-webos.log` |
| 4 | `tools/verify.sh build/webos-release/dist/org.wnk.examples.hello_0.8.0_arm.ipk --runtime-libs cmake/sdl-webos-runtime-libs.txt` | 0 | **7/7 firmware images loadable** (5.3.1 … 11.2.0); symbol presence only | `evidence/baseline/verify-hello.log` |
| 5 | `WNK_EXTRA_APP_DIRS=…/apps/runtime-probe tools/build-webos.sh` | 0 | probe ipk + 4 examples, all checks passed, 0 compiler warnings | `evidence/baseline/build-webos-probe.log` |
| 6 | `tools/verify.sh …/org.csweb.runtimeprobe_0.1.0_arm.ipk --runtime-libs …` | 0 | 7/7 | `evidence/baseline/verify-probe.log` |
| 7 | host `ctest -R csweb` (preset host-debug) | 0 | `csweb.probe.child_host` passed | — |
| 8 | `tools/install.sh …/org.csweb.runtimeprobe_0.1.0_arm.ipk` | 0 | "Installed package org.csweb.runtimeprobe!" | `evidence/device/20261019-070601-runtimeprobe/install.log` |
| 9 | `tools/launch.sh org.csweb.runtimeprobe` | 0 | launched | `…/launch.log` |
| 10 | `tools/logs.sh org.csweb.runtimeprobe <dir>` | 0 | events.log pulled as `prisoner` | `…/org.csweb.runtimeprobe-events-20261019-070623.log` |

ipk SHA-256 values: `evidence/baseline/ipk-sha256.txt` (examples), `evidence/baseline/probe-ipk-sha256.txt` (probe).

## Device

The probe's own log records the device as `OLED65C1PTB webOS 6.5.3 (kisscurl-koli) firmware 03.53.45
board O20_DVB ram 3 GB kernel 4.4.84-229.kcl4tv.6 aarch64`, the same TV as the kit's compatibility
records. Install, launch and log retrieval through the kit tools work unmodified on it `[HW]`. The kit's
example apps were not reinstalled in this session; the kit records their device runs (2026-10-11 … 10-19).

## Notes
- `tools/logs.sh` works as `prisoner` (answers the kit's open question "whether the Developer Mode shell
  user can read it": yes, for this app's 0640 log) `[HW]`.
- One documented kit change: `docs/kit-patches/0001-extra-app-dirs.md`.
