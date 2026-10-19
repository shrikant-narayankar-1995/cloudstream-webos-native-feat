# SPDX-License-Identifier: GPL-3.0-or-later
# A stand-in for the runtime service (docs/runtime/service-protocol.md) for test_runtime_client:
# prints ready, answers "search" with two results, "dialog" with a dialog it waits to have answered,
# "crash" by exiting, anything else with an error.
import json, sys

print(json.dumps({"event": "ready", "protocol": 1}), flush=True)
waiting = {}
for line in sys.stdin:
    req = json.loads(line)
    op, rid = req.get("op"), req.get("id")
    if op == "search":
        for n in ("One", "Two"):
            print(json.dumps({"id": rid, "event": "result", "name": n, "url": "u/" + n}), flush=True)
        print(json.dumps({"event": "done", "id": rid, "ok": True}), flush=True)
    elif op == "dialog":
        waiting[7] = rid
        print(json.dumps({"id": rid, "event": "dialog", "dialog": 7, "title": "T"}), flush=True)
    elif op == "answer":
        rid = waiting.pop(req.get("dialog"), None)
        ok = req.get("button") == "positive"
        print(json.dumps({"event": "done", "id": rid, "ok": ok}), flush=True)
    elif op == "crash":
        sys.exit(3)
    elif op == "cancel":
        pass
    else:
        print(json.dumps({"id": rid, "event": "error", "message": "unknown op " + str(op)}), flush=True)
        print(json.dumps({"event": "done", "id": rid, "ok": False}), flush=True)
