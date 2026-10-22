#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copy csloader sources with progress logs for the opt-in armv7 build only."""
import pathlib
import shutil
import sys


def once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"legacy load trace: expected one occurrence of {old!r}")
    return source.replace(old, new, 1)


def instrument(source, output):
    source, output = pathlib.Path(source), pathlib.Path(output)
    # Never modify the repository's shared sources or accept an existing output tree.
    if source.resolve() == output.resolve() or source.resolve() in output.resolve().parents or output.exists():
        raise ValueError("legacy load trace: output must be a new directory")
    package = pathlib.Path("org/csweb/loader")
    main = (source / package / "Main.java").read_text()
    service = (source / package / "Service.java").read_text()
    helper = '''
    // Generated only for the legacy runtime. Fixed labels only: no plugin data or URLs.
    private static final long LEGACY_TRACE_START = System.nanoTime();
    static void legacyLoadStage(String stage) {
        System.err.println("[legacy-load] elapsed_ms=" +
                ((System.nanoTime() - LEGACY_TRACE_START) / 1000000L) +
                " thread=" + Thread.currentThread().getName() + " stage=" + stage);
        System.err.flush();
    }
'''
    main = once(main, "public final class Main {", "public final class Main {" + helper)

    def before_after(text, statement, stage):
        indent = statement[:len(statement) - len(statement.lstrip())]
        return once(text, statement,
                    f'{indent}legacyLoadStage("{stage}.begin");\n' + statement +
                    f'\n{indent}legacyLoadStage("{stage}.end");')

    main = before_after(main,
        "        PathClassLoader loader = new PathClassLoader(file.getAbsolutePath(), Main.class.getClassLoader());",
        "classloader")
    main = once(main, '        try (InputStream in = loader.getResourceAsStream("manifest.json")) {',
        '        legacyLoadStage("manifest.open.begin");\n' +
        '        try (InputStream in = loader.getResourceAsStream("manifest.json")) {\n' +
        '            legacyLoadStage("manifest.open.end");')
    main = before_after(main,
        "            manifestText = new String(readAll(in, 64 * 1024), StandardCharsets.UTF_8);",
        "manifest.read")
    main = once(main,
        "        BasePlugin.Manifest manifest = MainAPIKt.getMapper().readValue(manifestText, BasePlugin.Manifest.class);",
        '        legacyLoadStage("manifest.parse.begin");\n' +
        '        legacyLoadStage("mapper.initialize.begin");\n' +
        '        com.fasterxml.jackson.databind.ObjectMapper legacyManifestMapper = MainAPIKt.getMapper();\n' +
        '        legacyLoadStage("mapper.initialize.end");\n' +
        '        legacyLoadStage("manifest.deserialize.begin");\n' +
        '        BasePlugin.Manifest manifest = legacyManifestMapper.readValue(manifestText, BasePlugin.Manifest.class);\n' +
        '        legacyLoadStage("manifest.deserialize.end");\n' +
        '        legacyLoadStage("manifest.parse.end");')
    main = before_after(main,
        "        Class<?> pluginClass = loader.loadClass(manifest.getPluginClassName());", "class.resolve")
    main = before_after(main,
        "        BasePlugin plugin = (BasePlugin) pluginClass.getDeclaredConstructor().newInstance();",
        "class.initialize_and_construct")
    main = before_after(main,
        '        HeadlessContext app = new HeadlessContext(data, "com.lagradost.cloudstream3");', "context")
    main = once(main, "        if (manifest.getRequiresResources()) {",
        '        legacyLoadStage("resources.begin");\n        if (manifest.getRequiresResources()) {')
    main = once(main, "        try {\n            if (plugin instanceof Plugin) {\n                ((Plugin) plugin).load(",
        '        legacyLoadStage("resources.end");\n        legacyLoadStage("plugin.load.begin");\n' +
        "        try {\n            if (plugin instanceof Plugin) {\n                ((Plugin) plugin).load(")
    main = once(main, "        if (plugin instanceof Plugin && ((Plugin) plugin).getOpenSettings() != null) {",
        '        legacyLoadStage("plugin.load.end");\n        legacyLoadStage("settings.inspect.begin");\n' +
        "        if (plugin instanceof Plugin && ((Plugin) plugin).getOpenSettings() != null) {")
    main = once(main, "        return plugin;\n    }",
        '        legacyLoadStage("settings.inspect.end");\n        legacyLoadStage("load.return");\n        return plugin;\n    }')

    def service_stage(text, old, new):
        return once(text, old, new)

    service = service_stage(service, "        String hash = Main.sha256(file);",
        '        Main.legacyLoadStage("integrity.begin");\n        String hash = Main.sha256(file);\n' +
        '        Main.legacyLoadStage("integrity.end");')
    service = service_stage(service, "        BasePlugin plugin = Main.loadPlugin(file, data, hash, null);",
        '        Main.legacyLoadStage("load.begin");\n        BasePlugin plugin = Main.loadPlugin(file, data, hash, null);\n' +
        '        Main.legacyLoadStage("load.end");')
    service = service_stage(service, '        if (!Main.answerDialogs("load")) return false;',
        '        Main.legacyLoadStage("dialogs.begin");\n        if (!Main.answerDialogs("load")) return false;\n' +
        '        Main.legacyLoadStage("dialogs.end");')
    service = service_stage(service, "        int n = 0;\n        for (MainAPI api : APIHolder.INSTANCE.getAllProviders()) {",
        '        Main.legacyLoadStage("providers.begin");\n        int n = 0;\n' +
        "        for (MainAPI api : APIHolder.INSTANCE.getAllProviders()) {")
    service = service_stage(service, '        Main.emit(Main.event("loaded", "plugin", file.getPath(), "providers", n));',
        '        Main.legacyLoadStage("providers.end");\n' +
        '        Main.emit(Main.event("loaded", "plugin", file.getPath(), "providers", n));')
    service = service_stage(service, "            ok = handle(op, p);",
        '            if (op.equals("load")) Main.legacyLoadStage("request.begin");\n            ok = handle(op, p);\n' +
        '            if (op.equals("load")) Main.legacyLoadStage("request.end");')
    # An exclusive load can remain queued behind a stuck earlier request. Record receipt separately.
    service = service_stage(service, "            Running r = new Running();",
        '            if (op.equals("load")) Main.legacyLoadStage("request.queued");\n            Running r = new Running();')
    # Validate all insertion points before copying/writing anything.
    shutil.copytree(source, output)
    (output / package / "Main.java").write_text(main)
    (output / package / "Service.java").write_text(service)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: instrument-legacy-loader.py SOURCE_DIR NEW_OUTPUT_DIR")
    instrument(sys.argv[1], sys.argv[2])
