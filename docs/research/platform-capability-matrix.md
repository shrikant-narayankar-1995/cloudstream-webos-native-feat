# Platform capability matrix (runtime feasibility)

Tags: `[HW]` measured on our TV by this project; `[HW-kit]` measured on the same TV by the kit project
(`docs/compatibility/records.json` in the kit); `[TEST]` host run here; `[SRC]` source; `[INF]`
inference; `[UNK]` unknown.

## Target

| Item | Value |
|---|---|
| TV | LG OLED65C1PTB (2021), board O20_DVB |
| webOS | 6.5.3 (kisscurl-koli), firmware 03.53.45 |
| Kernel | 4.4.84-229.kcl4tv.6 **aarch64**; userland 32-bit ARMv7 EABI5 soft-float `[HW]` |
| CPU | 4 cores, ARMv8 (`fp asimd aes pmull sha1 sha2 crc32 atomics`) `[HW]` |
| Install mode | Developer Mode, unrooted, ares-cli-rs 0.7.0 `[HW-kit]` |
| Probe | `apps/runtime-probe` 0.1.0, ipk sha256 `10351717a32f5ee27d0ff9903943fdcb48294afc1c71ec62861adf5cf66d1a79` |
| Date | 2026-10-19 |
| Log | `evidence/device/20261019-070601-runtimeprobe/org.csweb.runtimeprobe-events-20261019-070623.log` |

## Process and code execution (all measured inside a child process started by the app)

| Capability | Result | Evidence |
|---|---|---|
| fork + execve of a packaged ARM ELF from the app dir | **supported** `[HW]` | `run exec_app_dir status=exit 0` |
| execve of a copy on tmpfs `/tmp/<appid>` (tmpfs not `noexec`) | **supported** `[HW]` | `run exec_tmpfs_copy status=exit 0`; mount `/tmp tmpfs rw,relatime` |
| **64-bit AArch64 static ELF** | **supported** `[HW]` | `run exec_aarch64 status=exit 42`, output `a64 ok` (host negative control: ENOEXEC) |
| Anonymous RWX mapping + call generated code (JIT) | supported `[HW]` | `child.jit_rwx=ok` |
| RW → mprotect RX (W^X JIT) | supported `[HW]` | `child.jit_wx=ok` |
| File-backed `PROT_EXEC` mmap from `/tmp` and `/dev/shm` (AOT .oat/.odex) | supported `[HW]` | `child.file_exec_map_tmp=ok`, `_shm=ok` |
| App dir filesystem | ext4 `nosuid`, read-only to the app (EACCES) `[HW]` `[HW-kit]` | mounts line |
| Raise thread priority | unsupported (no CAP_SYS_NICE) `[SRC]` | kit WEBOS-PLATFORM.md |
| Root, chroot, binder/ashmem, containers | unavailable (unrooted Developer Mode) `[SRC]` | kit AGENTS.md |

## Resources (child process, while the probe app's UI was running)

| Resource | Value |
|---|---|
| MemTotal / **MemAvailable** | 2678680 kB / **~436 MB** `[HW]` |
| CommitLimit / Committed_AS / overcommit | 1953736 kB / 10.2 GB / heuristic (0) `[HW]` |
| RLIMIT_AS, DATA, MEMLOCK | unlimited `[HW]` |
| RLIMIT_STACK | 8 MiB soft / unlimited hard `[HW]` |
| RLIMIT_NOFILE | 1024 soft / 4096 hard `[HW]` |
| RLIMIT_NPROC | 5597 `[HW]` |
| Threads | 64/64 created with 1 MiB stacks `[HW]` |
| tmpfs free | 1280 MiB `[HW]` (volatile; lost on reboot) |
| Durable writable filesystem | none; DB8 only `[HW-kit]` |

## Entropy, network

| Capability | Result |
|---|---|
| `/dev/urandom` | **readable** `[HW]` (contradicts the punktfunk note quoted in the kit's WEBOS-PLATFORM.md) |
| `/dev/random` | absent (ENOENT) `[HW]` |
| `getrandom(2)` | not tested: NDK glibc 2.12 headers lack `SYS_getrandom` `[UNK]` (kernel 4.4 has it `[INF]`) |
| DNS + TCP 443 to archive.org from a child | supported, IPv4 `[HW]` |

## Conclusions for a runtime

1. A **separate on-device runtime process** shipped in the ipk is technically allowed: exec, JIT and AOT
   mappings, threads, entropy and networking all work for a child of the app `[HW]`.
2. Both **armv7** (matching the userland) and **aarch64** (kernel supports 64-bit EL0) processes run `[HW]`.
   Either needs its own bundled C library and loader, because the TV's glibc is 32-bit and its 64-bit
   userland does not exist.
3. **Memory is the tightest budget**: ~430 MB available with a small native app running. An ART
   process for one small provider is expected to need tens of MB `[INF]`; must be measured (test T3 in
   `runtime-test-plan.md`).
4. Runtime caches (dalvik-cache) must live on tmpfs and are rebuilt after each reboot `[INF]`.
