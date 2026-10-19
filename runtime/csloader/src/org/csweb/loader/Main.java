// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.loader;

import com.lagradost.cloudstream3.APIHolder;
import com.lagradost.cloudstream3.MainAPI;
import com.lagradost.cloudstream3.MainAPIKt;
import com.lagradost.cloudstream3.plugins.BasePlugin;
import com.lagradost.cloudstream3.plugins.Plugin;

import dalvik.system.PathClassLoader;

import org.csweb.compat.HeadlessActivities;
import org.csweb.compat.HeadlessContext;
import org.csweb.compat.MiniJson;
import org.csweb.compat.Unsupported;
import org.csweb.compat.ui.DialogProtocol;
import org.csweb.compat.ui.UiHost;

import java.io.BufferedReader;
import java.io.InputStreamReader;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import kotlin.Unit;
import kotlin.coroutines.EmptyCoroutineContext;
import kotlinx.coroutines.BuildersKt;

/**
 * Loads one unmodified CloudStream plugin (.cs3) the way CloudStream's PluginManager.loadPlugin does,
 * runs one provider operation, and reports on stdout as JSON lines:
 *
 *   csloader --plugin F.cs3 --data DIR --op load-only|search|mainpage|details|links|settings [--query Q]
 *            [--url U] [--provider NAME] [--timeout SECONDS] [--limit N] [--sha256 HEX] [--ui stdin]
 *
 * Events: plugin, provider, result, link, subtitle, dialog, toast, unsupported, error, done. Diagnostics
 * (plugin logging) go to stderr. With --ui stdin, every dialog the plugin shows is described in a
 * "dialog" event and the runtime waits for the app's answer as one JSON line on stdin
 * (org.csweb.compat.ui.DialogProtocol); without it dialogs are reported and left unanswered. The process ends with Runtime.halt, because ART's shutdown thread aborts after
 * main returns in this runtime build.
 */
public final class Main {
    private static final PrintStream OUT = System.out;
    private static final Map<String, String> args = new LinkedHashMap<>();
    private static HeadlessContext appContext;

    public static void main(String[] argv) {
        long start = System.currentTimeMillis();
        int code = 1;
        try {
            for (int i = 0; i + 1 < argv.length; i += 2) {
                if (!argv[i].startsWith("--")) throw new IllegalArgumentException("bad argument " + argv[i]);
                args.put(argv[i].substring(2), argv[i + 1]);
            }
            startWatchdog(Integer.parseInt(args.getOrDefault("timeout", "120")));
            // As on Android an uncaught exception on any thread ends the process; report it first.
            Thread.setDefaultUncaughtExceptionHandler((thread, t) -> {
                error("thread:" + thread.getName(), t);
                emit(event("done", "ok", false));
                Runtime.getRuntime().halt(4);
            });
            UiHost.install(new LoaderUi());
            code = run() ? 0 : 2;
        } catch (Throwable t) {
            error("startup", t);
        }
        emit(event("done", "ok", code == 0, "elapsed_ms", System.currentTimeMillis() - start));
        OUT.flush();
        System.err.flush();
        Runtime.getRuntime().halt(code);
    }

    private static boolean run() throws Exception {
        File file = new File(required("plugin"));
        File data = new File(required("data"));
        String op = args.getOrDefault("op", "load-only");
        String hashBefore = sha256(file);
        String expected = args.get("sha256");
        if (expected != null && !expected.equalsIgnoreCase(hashBefore)) {
            // A supplied hash proves only that the bytes are the ones the caller vetted, not who made them.
            emit(event("error", "stage", "integrity", "type", "HashMismatch", "message",
                    "plugin SHA-256 " + hashBefore + " does not match the supplied " + expected));
            return false;
        }

        // --- PluginManager.loadPlugin, step by step -------------------------------------------------
        BasePlugin plugin = loadPlugin(file, data, hashBefore, null);
        if (plugin == null) return false;
        if (!answerDialogs("load")) return false;
        if (op.equals("settings")) {
            boolean ok = settings(plugin);
            if (!sha256(file).equals(hashBefore)) {
                emit(event("error", "stage", "integrity", "type", "PluginChanged", "message", "plugin file hash changed"));
                return false;
            }
            return ok;
        }

        List<MainAPI> providers = new ArrayList<>();
        for (MainAPI api : APIHolder.INSTANCE.getAllProviders()) {
            if (file.getAbsolutePath().equals(api.getSourcePlugin())) providers.add(api);
        }
        for (MainAPI api : providers) {
            emit(event("provider", "name", api.getName(), "mainUrl", api.getMainUrl(), "lang", api.getLang(),
                    "hasMainPage", api.getHasMainPage(), "hasQuickSearch", api.getHasQuickSearch(),
                    "supportedTypes", String.valueOf(api.getSupportedTypes())));
        }
        if (providers.isEmpty()) {
            emit(event("error", "stage", "register", "type", "NoProvider", "message", "plugin registered no MainAPI"));
            return false;
        }
        MainAPI api = providers.get(0);
        if (args.containsKey("provider")) {
            api = null;
            for (MainAPI p : providers) if (p.getName().equals(args.get("provider"))) api = p;
            if (api == null) throw new IllegalArgumentException("no provider named " + args.get("provider"));
        }

        boolean ok = op.equals("load-only") || operation(api, op);
        ok &= answerDialogs("operation:" + op);
        String hashAfter = sha256(file);
        if (!hashAfter.equals(hashBefore)) {
            emit(event("error", "stage", "integrity", "type", "PluginChanged", "message", "plugin file hash changed"));
            return false;
        }
        return ok;
    }

    /**
     * One pass of CloudStream's loadPlugin. Plugins get the headless application context, as plugins that
     * check `context is Activity` before showing UI must not see an Activity. A plugin whose load() casts the
     * context to an Activity type is unloaded the way PluginManager.unloadPlugin does it and loaded again,
     * in a fresh class loader, with a headless instance of exactly that type (CloudStream passes its
     * MainActivity on Android).
     */
    private static BasePlugin loadPlugin(File file, File data, String hash, Class<?> activityType) throws Exception {
        PathClassLoader loader = new PathClassLoader(file.getAbsolutePath(), Main.class.getClassLoader());
        String manifestText;
        try (InputStream in = loader.getResourceAsStream("manifest.json")) {
            if (in == null) {
                emit(event("error", "stage", "manifest", "type", "MissingManifest", "message", "No manifest found"));
                return null;
            }
            manifestText = new String(readAll(in, 64 * 1024), StandardCharsets.UTF_8);
        }
        BasePlugin.Manifest manifest = MainAPIKt.getMapper().readValue(manifestText, BasePlugin.Manifest.class);
        Class<?> pluginClass = loader.loadClass(manifest.getPluginClassName());
        BasePlugin plugin = (BasePlugin) pluginClass.getDeclaredConstructor().newInstance();
        plugin.setFilename(file.getAbsolutePath());
        if (activityType == null) {
            emit(event("plugin", "name", manifest.getName(), "version", manifest.getVersion(),
                    "class", manifest.getPluginClassName(), "base", plugin instanceof Plugin ? "Plugin" : "BasePlugin",
                    "requiresResources", manifest.getRequiresResources(), "sha256", hash));
        }
        HeadlessContext app = new HeadlessContext(data, "com.lagradost.cloudstream3");
        appContext = app;
        if (manifest.getRequiresResources()) {
            // PluginManager.loadPlugin: AssetManager by reflection, addAssetPath(<.cs3>), then Resources over it.
            try {
                android.content.res.AssetManager assets =
                        android.content.res.AssetManager.class.getDeclaredConstructor().newInstance();
                java.lang.reflect.Method addAssetPath =
                        android.content.res.AssetManager.class.getMethod("addAssetPath", String.class);
                addAssetPath.invoke(assets, file.getAbsolutePath());
                if (plugin instanceof Plugin) {
                    ((Plugin) plugin).setResources(new android.content.res.Resources(assets,
                            app.getResources().getDisplayMetrics(), app.getResources().getConfiguration()));
                }
                if (activityType == null) {
                    emit(event("resources", "package", assets.resourceTable() == null ? null : assets.resourceTable().packageName()));
                }
            } catch (Throwable t) {
                error("resources", t);
                return null;
            }
        }
        try {
            if (plugin instanceof Plugin) {
                ((Plugin) plugin).load(activityType == null ? app : HeadlessActivities.create(activityType, app));
            } else {
                plugin.load();
            }
        } catch (ClassCastException e) {
            Class<?> wanted = activityTypeFrom(e, loader);
            if (activityType != null || wanted == null) {
                error("lifecycle", e);
                return null;
            }
            unload(plugin);
            emit(event("compat", "context", "headless-activity", "type", wanted.getName(),
                    "reason", "load(Context) casts its context to an Activity"));
            return loadPlugin(file, data, hash, wanted);
        } catch (Throwable t) {
            error("lifecycle", t);
            return null;
        }
        if (plugin instanceof Plugin && ((Plugin) plugin).getOpenSettings() != null) {
            emit(event("settings", "available", true));
        }
        return plugin;
    }

    /** The Activity class named by "X cannot be cast to <class>", if it is one. */
    private static Class<?> activityTypeFrom(ClassCastException e, ClassLoader loader) {
        String m = String.valueOf(e.getMessage());
        int at = m.indexOf("cannot be cast to ");
        if (at < 0 || !m.startsWith(HeadlessContext.class.getName())) return null;
        try {
            Class<?> c = Class.forName(m.substring(at + "cannot be cast to ".length()).trim(), false, loader);
            return android.app.Activity.class.isAssignableFrom(c) ? c : null;
        } catch (ClassNotFoundException notFound) {
            return null;
        }
    }

    /** PluginManager.unloadPlugin: beforeUnload, then drop the APIs and extractors the plugin registered. */
    private static void unload(BasePlugin plugin) {
        try {
            plugin.beforeUnload();
        } catch (Throwable t) {
            error("beforeUnload", t);
        }
        String source = plugin.getFilename();
        List<MainAPI> mine = new ArrayList<>();
        for (MainAPI api : APIHolder.INSTANCE.getAllProviders()) if (source.equals(api.getSourcePlugin())) mine.add(api);
        for (MainAPI api : mine) APIHolder.INSTANCE.removePluginMapping(api);
        APIHolder.INSTANCE.getAllProviders().removeAll(mine);
        List<com.lagradost.cloudstream3.utils.ExtractorApi> ext = new ArrayList<>();
        for (com.lagradost.cloudstream3.utils.ExtractorApi x : com.lagradost.cloudstream3.utils.ExtractorApiKt.getExtractorApis()) {
            if (source.equals(x.getSourcePlugin())) ext.add(x);
        }
        com.lagradost.cloudstream3.utils.ExtractorApiKt.getExtractorApis().removeAll(ext);
    }

    // --- plugin UI: settings and dialogs ---------------------------------------------------------

    /**
     * CloudStream's settings button: openSettings(context), then the dialogs it shows. CloudStream passes
     * its Activity; the headless runtime passes the application context, so plugins that need an
     * Activity's fragment manager or custom views fail with an unsupported report.
     */
    private static boolean settings(BasePlugin plugin) {
        kotlin.jvm.functions.Function1<android.content.Context, Unit> open =
                plugin instanceof Plugin ? ((Plugin) plugin).getOpenSettings() : null;
        if (open == null) {
            emit(event("unsupported", "api", "plugin has no settings (openSettings is null)", "stage", "settings"));
            return false;
        }
        try {
            open.invoke(appContext);
        } catch (Throwable t) {
            error("settings", t);
            return false;
        }
        return answerDialogs("settings");
    }

    private static final class LoaderUi extends UiHost {
        final List<android.app.Dialog> showing = new ArrayList<>();
        final Map<android.app.Dialog, Integer> ids = new java.util.IdentityHashMap<>();
        final java.util.Set<android.app.Dialog> reported =
                java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<android.app.Dialog, Boolean>());
        int next = 1;

        @Override public synchronized void shown(android.app.Dialog d) {
            if (!ids.containsKey(d)) ids.put(d, next++);
            showing.add(d);
        }

        @Override public synchronized void dismissed(android.app.Dialog d) {
            showing.remove(d);
            emit(event("dialog_closed", "dialog", ids.get(d)));
        }

        @Override public void toast(CharSequence text, int duration) {
            emit(event("toast", "text", String.valueOf(text), "long", duration != 0));
        }

        synchronized android.app.Dialog top() {
            return showing.isEmpty() ? null : showing.get(showing.size() - 1);
        }
    }

    private static BufferedReader uiInput;

    /**
     * Describe the topmost showing dialog, wait for the app's answer, apply it; repeat until no dialog is
     * showing. Answers run on this (the plugin's main) thread, as dialog callbacks do on Android.
     */
    private static boolean answerDialogs(String stage) {
        LoaderUi ui = (LoaderUi) UiHost.get("dialogs");
        boolean interactive = "stdin".equals(args.get("ui"));
        for (int round = 0; round < 200; round++) {
            android.app.Dialog d = ui.top();
            if (d == null) return true;
            Map<String, Object> m = event("dialog", "dialog", ui.ids.get(d), "stage", stage);
            try {
                m.putAll(DialogProtocol.describe(d));
            } catch (Throwable t) {
                error("dialog", t);
                return false;
            }
            if (!interactive) {
                // Without an app attached the dialog is reported once and stays open, unanswered.
                synchronized (ui) {
                    for (android.app.Dialog open : ui.showing) {
                        if (!ui.reported.add(open)) continue;
                        Map<String, Object> o = event("dialog", "dialog", ui.ids.get(open), "stage", stage, "answerable", false);
                        o.putAll(DialogProtocol.describe(open));
                        emit(o);
                    }
                }
                return true;
            }
            emit(m);
            try {
                if (uiInput == null) uiInput = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
                String line = uiInput.readLine();
                if (line == null) {
                    emit(event("error", "stage", "dialog", "type", "EndOfInput", "message", "the app closed the dialog channel"));
                    return false;
                }
                if (line.length() > 1 << 20) throw new IllegalArgumentException("answer longer than 1 MiB");
                Object parsed = MiniJson.decode(line);
                if (!(parsed instanceof Map)) throw new IllegalArgumentException("answer is not a JSON object");
                @SuppressWarnings("unchecked") Map<String, Object> answer = (Map<String, Object>) parsed;
                if (!ui.ids.get(d).equals(answer.get("dialog"))) {
                    throw new IllegalArgumentException("answer for dialog " + answer.get("dialog") + ", expected " + ui.ids.get(d));
                }
                DialogProtocol.answer(d, answer);  // answer contents (typed text, passwords) are never logged
            } catch (Throwable t) {
                error("dialog", t);
                return false;
            }
        }
        emit(event("error", "stage", "dialog", "type", "TooManyDialogs", "message", "more than 200 dialog rounds"));
        return false;
    }

    // --- provider operations --------------------------------------------------------------------

    private interface Suspend {
        Object call(kotlin.coroutines.Continuation<? super Object> c) throws Exception;
    }

    private static Object await(Suspend s) throws Exception {
        return BuildersKt.runBlocking(EmptyCoroutineContext.INSTANCE, (scope, cont) -> {
            try {
                return s.call(cont);
            } catch (RuntimeException e) {
                throw e;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        });
    }

    private static boolean operation(MainAPI api, String op) {
        int limit = Integer.parseInt(args.getOrDefault("limit", "20"));
        try {
            switch (op) {
                case "search": {
                    String q = required("query");
                    Object r = await(c -> api.search(q, 1, c));
                    return items(r, "result", limit);
                }
                case "mainpage": {
                    if (api.getMainPage().isEmpty()) {
                        emit(event("unsupported", "api", api.getName() + ".getMainPage", "stage", "operation"));
                        return false;
                    }
                    com.lagradost.cloudstream3.MainPageData first = api.getMainPage().get(0);
                    com.lagradost.cloudstream3.MainPageRequest req = new com.lagradost.cloudstream3.MainPageRequest(
                            first.getName(), first.getData(), first.getHorizontalImages());
                    Object r = await(c -> api.getMainPage(1, req, c));
                    if (r == null) return items(null, "result", limit);
                    boolean any = false;
                    for (Object list : (List<?>) bean(r, "getItems")) {
                        emit(event("section", "name", bean(list, "getName")));
                        any |= items(bean(list, "getList"), "result", limit);
                    }
                    return any;
                }
                case "details": {
                    String url = required("url");
                    Object r = await(c -> api.load(url, c));
                    if (r == null) return items(null, "details", 1);
                    emit(describe("details", r));
                    Object eps = tryBean(r, "getEpisodes");
                    if (eps instanceof List) items(eps, "episode", limit);
                    return true;
                }
                case "links": {
                    String data = required("url");
                    int[] n = {0};
                    Object r = await(c -> api.loadLinks(data, false,
                            sub -> { emit(describe("subtitle", sub)); return Unit.INSTANCE; },
                            link -> { n[0]++; emit(describe("link", link)); return Unit.INSTANCE; }, c));
                    emit(event("links_returned", "value", String.valueOf(r), "count", n[0]));
                    return n[0] > 0;
                }
                default:
                    throw new IllegalArgumentException("unknown op " + op);
            }
        } catch (Throwable t) {
            error("operation:" + op, t);
            return false;
        }
    }

    private static boolean items(Object r, String kind, int limit) {
        Object list = r;
        if (r != null && !(r instanceof List)) list = tryBean(r, "getItems");
        if (!(list instanceof List) || ((List<?>) list).isEmpty()) {
            emit(event("empty", "kind", kind));
            return false;
        }
        int i = 0;
        for (Object o : (List<?>) list) {
            if (i++ >= limit) break;
            emit(describe(kind, o));
        }
        emit(event("count", "kind", kind, "value", ((List<?>) list).size()));
        return true;
    }

    // Plain properties of a result object (SearchResponse, LoadResponse, ExtractorLink, SubtitleFile...).
    private static final String[] PROPS = {"getName", "getUrl", "getApiName", "getType", "getPosterUrl", "getYear",
            "getPlot", "getDataUrl", "getSource", "getQuality", "getReferer", "isM3u8", "getLang", "getEpisode", "getSeason", "getData"};

    private static Map<String, Object> describe(String kind, Object o) {
        Map<String, Object> m = event(kind, "class", o.getClass().getName());
        for (String p : PROPS) {
            Object v = tryBean(o, p);
            if (v == null) continue;
            String key = p.startsWith("is") ? p : Character.toLowerCase(p.charAt(3)) + p.substring(4);
            m.put(key, v instanceof Number || v instanceof Boolean ? v : String.valueOf(v));
        }
        Object headers = tryBean(o, "getHeaders");
        if (headers instanceof Map) m.put("headerNames", new ArrayList<>(((Map<?, ?>) headers).keySet()));  // names only
        return m;
    }

    private static Object bean(Object o, String getter) throws Exception {
        Method m = o.getClass().getMethod(getter);
        return m.invoke(o);
    }

    private static Object tryBean(Object o, String getter) {
        try {
            return bean(o, getter);
        } catch (Throwable t) {
            return null;
        }
    }

    // --- output -----------------------------------------------------------------------------------

    private static Map<String, Object> event(String name, Object... kv) {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("event", name);
        for (int i = 0; i + 1 < kv.length; i += 2) m.put(String.valueOf(kv[i]), kv[i + 1]);
        return m;
    }

    private static synchronized void emit(Map<String, Object> m) {
        OUT.println(MiniJson.encode(m));
        OUT.flush();
    }

    private static void error(String stage, Throwable t) {
        for (Throwable c = t; c != null; c = c.getCause()) {
            if (c instanceof Unsupported) {
                emit(event("unsupported", "api", c.getMessage(), "stage", stage));
                break;
            }
        }
        Throwable root = t;
        while (root.getCause() != null && root.getCause() != root) root = root.getCause();
        emit(event("error", "stage", stage, "type", t.getClass().getName(), "message", String.valueOf(t.getMessage()),
                "rootType", root.getClass().getName(), "rootMessage", String.valueOf(root.getMessage())));
        t.printStackTrace(System.err);
    }

    private static void startWatchdog(int seconds) {
        Thread w = new Thread(() -> {
            try {
                Thread.sleep(seconds * 1000L);
            } catch (InterruptedException e) {
                return;
            }
            emit(event("error", "stage", "timeout", "type", "Timeout", "message", "no result after " + seconds + " s"));
            emit(event("done", "ok", false));
            Runtime.getRuntime().halt(3);
        }, "csloader-watchdog");
        w.setDaemon(true);
        w.start();
    }

    // --- helpers ----------------------------------------------------------------------------------

    private static String required(String key) {
        String v = args.get(key);
        if (v == null) throw new IllegalArgumentException("missing --" + key);
        return v;
    }

    private static byte[] readAll(InputStream in, int max) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        for (int n; (n = in.read(buf)) > 0; ) {
            out.write(buf, 0, n);
            if (out.size() > max) throw new IOException("entry larger than " + max + " bytes");
        }
        return out.toByteArray();
    }

    private static String sha256(File f) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[65536];
            for (int n; (n = in.read(buf)) > 0; ) md.update(buf, 0, n);
        }
        StringBuilder sb = new StringBuilder();
        for (byte b : md.digest()) sb.append(String.format("%02x", b));
        return sb.toString();
    }
}
