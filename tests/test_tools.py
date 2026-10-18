# SPDX-License-Identifier: GPL-3.0-or-later
"""Host tests for tools/dexinfo.py and tools/check_api_refs.py (run: python3 -m unittest discover tests)."""
import io
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import check_api_refs  # noqa: E402
import dexinfo  # noqa: E402

API = """public final class com/x/Base {
\tpublic fun <init> ()V
\tpublic final fun foo (Ljava/lang/String;)I
}

public final class com/x/Child : com/x/Base {
\tpublic static final field BAR Ljava/lang/String;
}
"""


def write(tmp, name, text):
    path = os.path.join(tmp, name)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    return path


class CheckApiRefsTest(unittest.TestCase):
    def run_check(self, methods, fields=(), types=()):
        with tempfile.TemporaryDirectory() as tmp:
            api = write(tmp, "lib.api", API)
            info = write(tmp, "info.json", json.dumps({"dex": [{
                "external_types": list(types), "external_methods": list(methods),
                "external_fields": list(fields)}]}))
            out = io.StringIO()
            old, sys.stdout = sys.stdout, out
            try:
                rc = check_api_refs.main(info, api, "com/x/")
            finally:
                sys.stdout = old
            return rc, json.loads(out.getvalue())

    def test_present_and_inherited_members_resolve(self):
        rc, res = self.run_check(["Lcom/x/Child;->foo(Ljava/lang/String;)I", "Lcom/x/Base;-><init>()V"],
                                 ["Lcom/x/Child;->BAR:Ljava/lang/String;"], ["Lcom/x/Child;"])
        self.assertEqual(rc, 0)
        self.assertEqual(res["missing"], [])
        self.assertEqual(res["checked"], 4)

    def test_wrong_signature_is_reported(self):
        rc, res = self.run_check(["Lcom/x/Base;->foo(Ljava/lang/String;)J"])
        self.assertEqual(rc, 1)
        self.assertEqual(res["missing"], [["method", "Lcom/x/Base;->foo(Ljava/lang/String;)J"]])

    def test_unknown_class_is_reported(self):
        rc, res = self.run_check([], types=["Lcom/x/Gone;"])
        self.assertEqual(rc, 1)

    def test_other_packages_are_ignored(self):
        rc, res = self.run_check(["Ljava/lang/Object;-><init>()V"])
        self.assertEqual((rc, res["checked"]), (0, 0))


class DexInfoTest(unittest.TestCase):
    def test_rejects_non_dex(self):
        with self.assertRaises(ValueError):
            dexinfo.parse_dex(b"PK\x03\x04" + b"\0" * 200)

    def test_rejects_size_mismatch(self):
        data = bytearray(0x70)
        data[:8] = b"dex\n035\0"
        struct.pack_into("<I", data, 32, 999)
        with self.assertRaises(ValueError):
            dexinfo.parse_dex(bytes(data))

    def test_rejects_traversal_entry(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "bad.cs3")
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("../evil.dex", b"dex\n035\0")
            r = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "dexinfo.py"), path],
                               capture_output=True, text=True)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("refusing entry", r.stderr)

    def test_real_plugin_if_present(self):
        path = os.path.join(ROOT, "evidence", "plugins", "download", "InternetArchiveProvider.cs3")
        if not os.path.exists(path):
            self.skipTest("plugin not downloaded")
        with zipfile.ZipFile(path) as z:
            dex = dexinfo.parse_dex(z.read("classes.dex"))
        names = {c["class"] for c in dex["classes"]}
        self.assertIn("Lrecloudstream/InternetArchivePlugin;", names)


if __name__ == "__main__":
    unittest.main()
