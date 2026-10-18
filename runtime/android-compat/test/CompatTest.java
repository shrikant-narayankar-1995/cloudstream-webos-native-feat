// SPDX-License-Identifier: GPL-3.0-or-later
// Host tests for the Android compat layer (plain JVM; run by runtime/build.sh). Expected values follow
// the Android SDK documentation for each API.
import android.content.SharedPreferences;
import android.net.Uri;

import org.csweb.compat.HeadlessContext;
import org.csweb.compat.MiniJson;

import java.io.File;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Map;

public final class CompatTest {
    private static int passed, failed;

    private static void eq(String name, Object want, Object got) {
        if (want == null ? got == null : want.equals(got)) {
            passed++;
        } else {
            failed++;
            System.out.println("FAIL " + name + ": want <" + want + "> got <" + got + ">");
        }
    }

    static void base64() {
        byte[] hello = "hello".getBytes();
        eq("b64 default ends with newline", "aGVsbG8=\n", android.util.Base64.encodeToString(hello, android.util.Base64.DEFAULT));
        eq("b64 no wrap", "aGVsbG8=", android.util.Base64.encodeToString(hello, android.util.Base64.NO_WRAP));
        eq("b64 no padding", "aGVsbG8", android.util.Base64.encodeToString(hello, android.util.Base64.NO_WRAP | android.util.Base64.NO_PADDING));
        eq("b64 url safe", "-_8=", android.util.Base64.encodeToString(new byte[]{(byte) 0xfb, (byte) 0xff}, android.util.Base64.NO_WRAP | android.util.Base64.URL_SAFE));
        eq("b64 crlf", "aGk=\r\n", android.util.Base64.encodeToString("hi".getBytes(), android.util.Base64.CRLF));
        byte[] big = new byte[60];
        String wrapped = android.util.Base64.encodeToString(big, android.util.Base64.DEFAULT);
        eq("b64 wraps at 76", 76, wrapped.indexOf('\n'));
        eq("b64 matches JDK when unwrapped", java.util.Base64.getEncoder().encodeToString(big), wrapped.replace("\n", ""));
        eq("b64 decode skips whitespace", "hello", new String(android.util.Base64.decode(" aGVs\nbG8= ", 0)));
        eq("b64 decode missing padding", "hello", new String(android.util.Base64.decode("aGVsbG8", 0)));
        eq("b64 decode url", "fbff", String.format("%02x%02x", android.util.Base64.decode("-_8", android.util.Base64.URL_SAFE)[0], android.util.Base64.decode("-_8", android.util.Base64.URL_SAFE)[1]));
        boolean threw = false;
        try {
            android.util.Base64.decode("aGVs=bG8", 0);
        } catch (IllegalArgumentException e) {
            threw = true;
        }
        eq("b64 misplaced padding throws", true, threw);
        threw = false;
        try {
            android.util.Base64.decode("a", 0);
        } catch (IllegalArgumentException e) {
            threw = true;
        }
        eq("b64 dangling char throws", true, threw);
        java.util.Random r = new java.util.Random(1);
        for (int n = 0; n < 50; n++) {
            byte[] b = new byte[r.nextInt(300)];
            r.nextBytes(b);
            eq("b64 roundtrip " + n, true, java.util.Arrays.equals(b, android.util.Base64.decode(android.util.Base64.encode(b, 0), 0)));
        }
    }

    static void resources() throws Exception {
        java.io.File cs3 = new java.io.File(System.getProperty("conformance", ""));
        if (!cs3.isFile()) {
            System.out.println("SKIP resources: conformance plugin not built (test-plugins/conformance/build.sh)");
            return;
        }
        android.content.res.AssetManager am = android.content.res.AssetManager.class.getDeclaredConstructor().newInstance();
        eq("addAssetPath cookie", 1, am.getClass().getMethod("addAssetPath", String.class).invoke(am, cs3.getPath()));
        android.content.res.Resources sys = android.content.res.Resources.getSystem();
        android.content.res.Resources r = new android.content.res.Resources(am, sys.getDisplayMetrics(), sys.getConfiguration());
        String pkg = "org.csweb.conformance";
        int name = r.getIdentifier("provider_name", "string", pkg);
        eq("identifier found", true, name != 0);
        eq("identifier forms agree", name, r.getIdentifier(pkg + ":string/provider_name", null, null));
        eq("identifier type/name", name, r.getIdentifier("string/provider_name", null, pkg));
        eq("other package -> 0", 0, r.getIdentifier("provider_name", "string", "android"));
        eq("missing -> 0", 0, r.getIdentifier("nope", "string", pkg));
        eq("string", "Conformance", r.getString(name));
        eq("escaped apostrophe", true, r.getString(r.getIdentifier("plot", "string", pkg)).contains("plugin's"));
        eq("format", "Conformance!", r.getString(name) + "!");
        eq("resource name", pkg + ":string/provider_name", r.getResourceName(name));
        eq("string array", Arrays.asList("Conformance Alpha", "Conformance Beta", "Conformance Gamma"),
                Arrays.asList(r.getStringArray(r.getIdentifier("titles", "array", pkg))));
        eq("color", 0xFF2F8F5B, r.getColor(r.getIdentifier("accent", "color", pkg)));
        int pad = r.getIdentifier("padding", "dimen", pkg);
        eq("dimension 16dp at xhdpi", 32.0f, r.getDimension(pad));
        eq("dimension pixel size", 32, r.getDimensionPixelSize(pad));
        boolean threw = false;
        try {
            r.getString(0x7f7f0000);
        } catch (android.content.res.Resources.NotFoundException e) {
            threw = true;
        }
        eq("unknown id throws NotFoundException", true, threw);
        threw = false;
        try {
            r.getDrawable(r.getIdentifier("icon", "drawable", pkg));
        } catch (org.csweb.compat.Unsupported e) {
            threw = true;
        }
        eq("drawable reported unsupported", true, threw);

        android.content.res.XmlResourceParser x = r.getLayout(r.getIdentifier("settings", "layout", pkg));
        String android = "http://schemas.android.com/apk/res/android";
        java.util.List<String> tags = new java.util.ArrayList<>();
        String orientation = null, text = null, inputType = null, padding = null;
        int textRes = 0, idRes = 0;
        for (int ev = x.getEventType(); ev != org.xmlpull.v1.XmlPullParser.END_DOCUMENT; ev = x.next()) {
            if (ev != org.xmlpull.v1.XmlPullParser.START_TAG) continue;
            tags.add(x.getName() + "@" + x.getDepth());
            if (x.getName().equals("LinearLayout")) {
                orientation = x.getAttributeValue(android, "orientation");
                padding = x.getAttributeValue(android, "padding");
            }
            if (x.getName().equals("TextView")) {
                text = x.getAttributeValue(android, "text");
                textRes = x.getAttributeResourceValue(android, "text", 0);
                idRes = x.getIdAttributeResourceValue(0);
            }
            if (x.getName().equals("EditText")) inputType = x.getAttributeValue(android, "inputType");
        }
        eq("layout tags", Arrays.asList("LinearLayout@1", "TextView@2", "EditText@2", "CheckBox@2"), tags);
        eq("enum attribute as int text", "1", orientation);
        eq("reference attribute text", "@" + r.getIdentifier("label_name", "string", pkg), text);
        eq("reference attribute value", r.getIdentifier("label_name", "string", pkg), textRes);
        eq("id attribute", r.getIdentifier("label_name", "id", pkg), idRes);
        eq("padding attribute references dimen", "@" + pad, padding);
        eq("flag attribute", "0x1", inputType);
    }

    public static void main(String[] args) throws Exception {
        resources();
        base64();
        uri();
        json();
        prefs();
        System.out.println("compat tests: " + passed + " passed, " + failed + " failed");
        if (failed > 0) System.exit(1);
    }

    static void uri() {
        Uri u = Uri.parse("https://user@Example.com:8443/a%20b/c.m3u8?x=1&q=hello+world%21&x=2#frag%20x");
        eq("scheme", "https", u.getScheme());
        eq("host", "Example.com", u.getHost());
        eq("port", 8443, u.getPort());
        eq("userInfo", "user", u.getUserInfo());
        eq("path decoded", "/a b/c.m3u8", u.getPath());
        eq("encoded path", "/a%20b/c.m3u8", u.getEncodedPath());
        eq("last segment", "c.m3u8", u.getLastPathSegment());
        eq("segments", Arrays.asList("a b", "c.m3u8"), u.getPathSegments());
        eq("query param plus", "hello world!", u.getQueryParameter("q"));
        eq("first of repeated", "1", u.getQueryParameter("x"));
        eq("all repeated", Arrays.asList("1", "2"), u.getQueryParameters("x"));
        eq("missing param", null, u.getQueryParameter("nope"));
        eq("names", new LinkedHashSet<>(Arrays.asList("x", "q")), u.getQueryParameterNames());
        eq("fragment", "frag x", u.getFragment());
        eq("toString unchanged", "https://user@Example.com:8443/a%20b/c.m3u8?x=1&q=hello+world%21&x=2#frag%20x", u.toString());
        eq("no port", -1, Uri.parse("http://h/p").getPort());
        eq("relative", null, Uri.parse("/just/path").getScheme());
        eq("relative path", "/just/path", Uri.parse("/just/path").getPath());
        eq("opaque", true, Uri.parse("mailto:a@b.c").isOpaque());
        eq("opaque ssp", "a@b.c", Uri.parse("mailto:a@b.c").getSchemeSpecificPart());
        eq("lenient garbage", "not a uri at all", Uri.parse("not a uri at all").toString());
        eq("ipv6 host", "[::1]", Uri.parse("http://[::1]:80/").getHost());
        eq("ipv6 port", 80, Uri.parse("http://[::1]:80/").getPort());
        eq("encode", "a%20b%2Fc%3F%C3%A9_-!.~'()*", Uri.encode("a b/c?é_-!.~'()*"));
        eq("encode allow", "a/b%20c", Uri.encode("a/b c", "/"));
        eq("decode", "a b/é", Uri.decode("a%20b%2F%C3%A9"));
        eq("decode keeps plus", "a+b", Uri.decode("a+b"));
        eq("decode bad escape kept", "100%", Uri.decode("100%"));
        Uri built = new Uri.Builder().scheme("https").authority("api.example.com").appendPath("v1")
                .appendPath("search items").appendQueryParameter("q", "a&b c").appendQueryParameter("page", "2").build();
        eq("builder", "https://api.example.com/v1/search%20items?q=a%26b%20c&page=2", built.toString());
        eq("builder roundtrip", "a&b c", built.getQueryParameter("q"));
        eq("buildUpon keeps", "https://h/p?a=1&b=2", Uri.parse("https://h/p?a=1").buildUpon().appendQueryParameter("b", "2").build().toString());
        eq("equals", Uri.parse("x:y"), Uri.parse("x:y"));
    }

    @SuppressWarnings("unchecked")
    static void json() {
        Object v = MiniJson.decode("{\"a\":[1,2.5,\"x\\n\\u00e9\",true,null,{}],\"b\":-3}");
        Map<String, Object> m = (Map<String, Object>) v;
        eq("json array", Arrays.asList(1, 2.5, "x\né", true, null, Collections.emptyMap()), m.get("a"));
        eq("json int", -3, m.get("b"));
        eq("json roundtrip", "{\"k\":\"q\\\"\\\\\\n\",\"n\":[1,true]}",
                MiniJson.encode(new java.util.LinkedHashMap<String, Object>() {{ put("k", "q\"\\\n"); put("n", Arrays.asList(1, true)); }}));
        boolean threw = false;
        try {
            MiniJson.decode("{\"a\":1} trailing");
        } catch (IllegalArgumentException e) {
            threw = true;
        }
        eq("json rejects trailing", true, threw);
        StringBuilder deep = new StringBuilder();
        for (int i = 0; i < 200; i++) deep.append('[');
        threw = false;
        try {
            MiniJson.decode(deep.toString());
        } catch (IllegalArgumentException e) {
            threw = true;
        }
        eq("json rejects deep nesting", true, threw);
    }

    static void prefs() throws Exception {
        File dir = Files.createTempDirectory("csweb-prefs").toFile();
        HeadlessContext ctx = new HeadlessContext(dir, "test");
        SharedPreferences p = ctx.getSharedPreferences("my/prefs", 0);
        p.edit().putString("s", "v").putInt("i", 7).putLong("l", 1L << 40).putFloat("f", 1.5f)
                .putBoolean("b", true).putStringSet("set", new LinkedHashSet<>(Arrays.asList("x", "y"))).commit();
        eq("file name sanitised", true, new File(dir, "shared_prefs/my_prefs.json").isFile());
        // a fresh process sees the same values with the same types
        java.lang.reflect.Field open = org.csweb.compat.JsonSharedPreferences.class.getDeclaredField("OPEN");
        open.setAccessible(true);
        ((Map<?, ?>) open.get(null)).clear();
        SharedPreferences q = ctx.getSharedPreferences("my/prefs", 0);
        eq("string", "v", q.getString("s", null));
        eq("int", 7, q.getInt("i", 0));
        eq("long", 1L << 40, q.getLong("l", 0));
        eq("float", 1.5f, q.getFloat("f", 0));
        eq("bool", true, q.getBoolean("b", false));
        eq("set", new LinkedHashSet<>(Arrays.asList("x", "y")), q.getStringSet("set", null));
        eq("default", "d", q.getString("missing", "d"));
        boolean threw = false;
        try {
            q.getInt("s", 0);
        } catch (ClassCastException e) {
            threw = true;
        }
        eq("wrong type throws like Android", true, threw);
        q.edit().remove("s").apply();
        eq("removed", false, q.contains("s"));
        String[] seen = new String[1];
        q.registerOnSharedPreferenceChangeListener((sp, key) -> seen[0] = key);
        q.edit().putString("z", "1").apply();
        eq("listener", "z", seen[0]);
    }
}
