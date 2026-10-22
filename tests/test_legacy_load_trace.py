# SPDX-License-Identifier: GPL-3.0-or-later
"""Legacy overlay must add only progress output, without editing shared Java sources."""
import importlib.util
import pathlib
import re
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("legacy_trace", ROOT / "tools/instrument-legacy-loader.py")
TRACE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRACE)
SOURCE = ROOT / "runtime/csloader/src"
PACKAGE = pathlib.Path("org/csweb/loader")


class LegacyLoadTraceTest(unittest.TestCase):
    def test_overlay_preserves_shared_sources_and_existing_statements(self):
        originals = {p.relative_to(SOURCE): p.read_bytes() for p in SOURCE.rglob("*") if p.is_file()}
        with tempfile.TemporaryDirectory() as directory:
            out = pathlib.Path(directory) / "traced"
            TRACE.instrument(SOURCE, out)
            self.assertEqual(set(originals), {p.relative_to(out) for p in out.rglob("*") if p.is_file()})
            for relative, original in originals.items():
                self.assertEqual((SOURCE / relative).read_bytes(), original)
                modified = (out / relative).read_text()
                if relative not in (PACKAGE / "Main.java", PACKAGE / "Service.java"):
                    self.assertEqual((out / relative).read_bytes(), original)
                    continue
                if relative.name == "Main.java":
                    modified = re.sub(
                        r'\n    // Generated only for the legacy runtime\.[\s\S]*?System\.err\.flush\(\);\n    }\n',
                        '', modified, count=1)
                modified = re.sub(r'^.*legacyLoadStage\("[^"]+"\);\n', '', modified, flags=re.MULTILINE)
                self.assertEqual(re.sub(r'\s+', '', modified), re.sub(r'\s+', '', original.decode()))

    def test_stages_are_ordered_and_no_signal_or_stack_dump_is_added(self):
        with tempfile.TemporaryDirectory() as directory:
            out = pathlib.Path(directory) / "traced"
            TRACE.instrument(SOURCE, out)
            main = (out / PACKAGE / "Main.java").read_text()
            service = (out / PACKAGE / "Service.java").read_text()
            stages = ["classloader", "manifest.open", "manifest.read", "manifest.parse", "class.resolve",
                      "class.initialize_and_construct", "context", "resources", "plugin.load", "settings.inspect"]
            previous = -1
            for stage in stages:
                begin = main.index(f'legacyLoadStage("{stage}.begin")')
                end = main.index(f'legacyLoadStage("{stage}.end")')
                self.assertLess(previous, begin)
                self.assertLess(begin, end)
                previous = end
            for stage in ("request.queued", "request.begin", "request.end", "integrity.begin", "integrity.end",
                          "load.begin", "load.end", "dialogs.begin", "dialogs.end", "providers.begin", "providers.end"):
                self.assertIn(f'legacyLoadStage("{stage}")', service)
            helper = main[main.index("private static final long LEGACY_TRACE_START"):main.index("System.err.flush();")]
            self.assertIn("System.err.println", helper)
            self.assertNotIn("System.out", helper)
            self.assertNotIn("getAllStackTraces", main + service)
            self.assertNotIn("SIGQUIT", main + service)

    def test_existing_output_and_changed_source_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                TRACE.instrument(SOURCE, pathlib.Path(directory))
            with self.assertRaises(ValueError):
                TRACE.instrument(SOURCE, SOURCE)
            source = pathlib.Path(directory) / "source"
            (source / PACKAGE).mkdir(parents=True)
            (source / PACKAGE / "Main.java").write_text("public final class Main {}")
            (source / PACKAGE / "Service.java").write_text("class Service {}")
            out = pathlib.Path(directory) / "invalid"
            with self.assertRaises(ValueError):
                TRACE.instrument(source, out)
            self.assertFalse(out.exists())


if __name__ == "__main__":
    unittest.main()
