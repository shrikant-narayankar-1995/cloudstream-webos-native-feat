#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run unmodified plugin load requests against the packaged ARMv7 runtime in CI."""
import argparse
import hashlib
import json
import os
import pathlib
import selectors
import shutil
import signal
import subprocess
import time


def snapshot(pid):
    tasks = []
    for task in pathlib.Path(f"/proc/{pid}/task").glob("*"):
        entry = {"tid": task.name}
        for name in ("comm", "stat", "wchan"):
            try:
                entry[name] = (task / name).read_text().strip()
            except OSError:
                pass
        tasks.append(entry)
    return tasks


def run_case(runtime, plugin, output, load_seconds=90, probes=False):
    state = pathlib.Path("/tmp/org.csweb.tv.legacy")
    if state.exists():
        raise RuntimeError(f"refusing to replace existing state: {state}")
    state.mkdir()
    for directory in ("art", "home", "plugin-data", "plugins"):
        (state / directory).mkdir()
    rt = state / "rt"
    rt.symlink_to(runtime, target_is_directory=True)
    aot = json.loads((runtime / "AOT.json").read_text())
    if aot["stable_path"] != str(rt):
        raise RuntimeError("runtime image was built for another stable path")
    bcp = ":".join(str(rt / jar) for jar in aot["bootclasspath"])
    digest = hashlib.sha256(plugin.read_bytes()).hexdigest()
    installed = state / "plugins" / (digest[:16] + ".cs3")
    shutil.copyfile(plugin, installed)
    env = {"PATH": str(rt / "bin"), "ANDROID_DATA": str(state / "art"),
           "HOME": str(state / "home"), "BOOTCLASSPATH": bcp}
    icu_file = runtime / "ICU_VERSION"
    if icu_file.exists():
        icu = rt / "usr/share/icu" / icu_file.read_text().strip()
        if icu.is_dir():
            env["ICU_DATA"] = str(icu)
    libs = ":".join(str(rt / part) for part in
                    ("usr/lib", "usr/lib/art", "usr/lib/java/dex/art/natives", "lib"))
    command = [str(rt / "lib/ld-musl-armhf.so.1"), "--library-path", libs,
               str(rt / "usr/bin/dalvikvm"), "-Xmx256m", "-Xnoimage-dex2oat",
               "-Xcompiler-option", "--compiler-filter=verify", "-Xint",
               "-Ximage:" + str(rt / aot.get("image", "boot/boot.art")), "-Xbootclasspath:" + bcp,
               "-Djavax.net.ssl.trustStore=" + str(rt / "trust/cacerts"),
               "-Djava.library.path=" + str(rt / "usr/lib/java/dex/art/natives"),
               "-cp", str(rt / "csweb/csweb-host.dex") + ":" + str(rt / "csweb/cloudstream.jar"),
               "org.csweb.loader.Main", "--serve", "1", "--data", str(state / "plugin-data")]
    if probes:
        # Each initializer gets its own VM and ART data directory. No signals or
        # eager initialization are added to the app's service path.
        for probe in ("math", "serialization", "jackson", "kotlinModule", "mapper"):
            probe_data = state / ("probe-" + probe)
            probe_data.mkdir()
            probe_env = dict(env, ANDROID_DATA=str(probe_data))
            probe_command = command[:command.index("org.csweb.loader.Main")] + [
                "org.csweb.loader.Main$LegacyProbe", probe]
            with (output / ("probe-" + probe + ".log")).open("wb") as log:
                proc = subprocess.Popen(probe_command, stdout=log, stderr=log,
                                        env=probe_env, start_new_session=True)
                timed_out = False
                try:
                    proc.wait(timeout=12)
                except subprocess.TimeoutExpired:
                    timed_out = True
                finally:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    proc.wait(timeout=5)
            print(json.dumps({"probe": probe, "timed_out": timed_out, "exit": proc.returncode}))
            print((output / ("probe-" + probe + ".log")).read_text(errors="replace"))
    summary = {"plugin": plugin.name, "sha256": digest, "command": command,
               "kernel": list(os.uname()), "ready": False, "loaded": False,
               "ok": False, "providers": [], "timed_out": False}
    start = time.monotonic()
    with (output / (plugin.stem + ".runtime.log")).open("wb") as stderr, \
            (output / (plugin.stem + ".protocol.jsonl")).open("wb") as protocol:
        proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=stderr, env=env, start_new_session=True)
        selector = selectors.DefaultSelector()
        selector.register(proc.stdout, selectors.EVENT_READ)
        pending = b""
        deadline = start + 30
        try:
            complete = False
            while time.monotonic() < deadline and not complete:
                for key, _ in selector.select(min(0.5, max(0, deadline - time.monotonic()))):
                    data = os.read(key.fd, 65536)
                    if not data:
                        complete = True
                        break
                    protocol.write(data)
                    protocol.flush()
                    pending += data
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        try:
                            event = json.loads(line)
                        except (ValueError, UnicodeError):
                            continue
                        if event.get("event") == "ready" and not summary["ready"]:
                            summary["ready"] = True
                            summary["ready_seconds"] = round(time.monotonic() - start, 3)
                            request = {"op": "load", "id": "test-load", "plugin": str(installed),
                                       "sha256": digest, "timeout": load_seconds}
                            proc.stdin.write((json.dumps(request) + "\n").encode())
                            proc.stdin.flush()
                            deadline = time.monotonic() + load_seconds + 5
                        if event.get("id") == "test-load":
                            if event.get("event") == "error" and event.get("stage") == "timeout":
                                summary["timed_out"] = True
                            if event.get("event") == "provider":
                                summary["providers"].append(event.get("name"))
                            if event.get("event") == "loaded":
                                summary["loaded"] = True
                            if event.get("event") == "done":
                                summary["ok"] = bool(event.get("ok")) and summary["loaded"] and bool(summary["providers"])
                                complete = True
                if proc.poll() is not None:
                    break
            summary["timed_out"] |= not complete and proc.poll() is None
            summary["elapsed_seconds"] = round(time.monotonic() - start, 3)
            summary["threads"] = snapshot(proc.pid)
            summary["exit_before_cleanup"] = proc.poll()
        finally:
            selector.close()
            # Reap the VM and any compiler descendants. No SIGQUIT or stack-dump mechanism.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait(timeout=5)
            proc.stdin.close()
            proc.stdout.close()
            shutil.rmtree(state)
    (output / (plugin.stem + ".summary.json")).write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({k: summary[k] for k in
                      ("plugin", "sha256", "ready", "loaded", "ok", "providers", "timed_out", "elapsed_seconds")}))
    lines = (output / (plugin.stem + ".runtime.log")).read_text(errors="replace").splitlines()
    print("Last runtime diagnostics for " + plugin.name + ":")
    print("\n".join(lines[-60:]))
    return summary["ok"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--probes", action="store_true", help="Run bounded initializer probes in separate VMs")
    parser.add_argument("plugins", nargs="+", type=pathlib.Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = [run_case(args.runtime.resolve(), p.resolve(), args.output, probes=args.probes and i == 0)
               for i, p in enumerate(args.plugins)]
    raise SystemExit(0 if all(results) else 1)
