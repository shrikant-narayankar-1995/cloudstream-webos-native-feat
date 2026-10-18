// SPDX-License-Identifier: GPL-3.0-or-later
// android.net.Uri with the platform's documented behaviour: parse() is lenient and never validates,
// getters return decoded values (getEncoded* return them raw), encode() leaves unreserved characters
// "A-Za-z0-9_-!.~'()*" alone and percent-encodes everything else as UTF-8, getQueryParameter() treats
// '+' as a space. Not Parcelable (there is no Binder in the headless runtime).
package android.net;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public final class Uri implements Comparable<Uri> {
    public static final Uri EMPTY = new Uri("");
    private static final String HEX = "0123456789ABCDEF";
    private static final String UNRESERVED = "_-!.~'()*";

    private final String uri;

    private Uri(String uri) {
        this.uri = uri;
    }

    public static Uri parse(String uriString) {
        if (uriString == null) throw new NullPointerException("uriString");
        return new Uri(uriString);
    }

    public static Uri withAppendedPath(Uri base, String pathSegment) {
        return base.buildUpon().appendEncodedPath(pathSegment).build();
    }

    // ---- structure (indices into the raw string) ----------------------------------------------

    private int schemeSep() {
        for (int i = 0; i < uri.length(); i++) {
            char c = uri.charAt(i);
            if (c == ':') return i;
            if (c == '/' || c == '?' || c == '#') return -1;
        }
        return -1;
    }

    private int fragmentSep() {
        return uri.indexOf('#', Math.max(schemeSep(), 0));
    }

    public String getScheme() {
        int s = schemeSep();
        return s == -1 ? null : uri.substring(0, s);
    }

    public String getEncodedSchemeSpecificPart() {
        int f = fragmentSep();
        return uri.substring(schemeSep() + 1, f == -1 ? uri.length() : f);
    }

    public String getSchemeSpecificPart() {
        return decode(getEncodedSchemeSpecificPart());
    }

    public boolean isHierarchical() {
        int s = schemeSep();
        return s == -1 || (uri.length() > s + 1 && uri.charAt(s + 1) == '/');
    }

    public boolean isOpaque() {
        return !isHierarchical();
    }

    public boolean isRelative() {
        return getScheme() == null;
    }

    public boolean isAbsolute() {
        return !isRelative();
    }

    // Hierarchical parts: [scheme:][//authority][path][?query][#fragment]
    private int[] parts() {  // {authStart, authEnd, pathEnd, queryEnd}
        int start = schemeSep() + 1;
        int f = fragmentSep();
        int end = f == -1 ? uri.length() : f;
        int authStart = -1, authEnd = start;
        if (uri.startsWith("//", start)) {
            authStart = start + 2;
            authEnd = authStart;
            while (authEnd < end && "/?".indexOf(uri.charAt(authEnd)) < 0) authEnd++;
        }
        int q = uri.indexOf('?', authEnd);
        int pathEnd = q == -1 || q > end ? end : q;
        return new int[]{authStart, authEnd, pathEnd, end};
    }

    public String getEncodedAuthority() {
        if (!isHierarchical()) return null;
        int[] p = parts();
        return p[0] == -1 ? null : uri.substring(p[0], p[1]);
    }

    public String getAuthority() {
        String a = getEncodedAuthority();
        return a == null ? null : decode(a);
    }

    public String getEncodedUserInfo() {
        String a = getEncodedAuthority();
        if (a == null) return null;
        int at = a.lastIndexOf('@');
        return at == -1 ? null : a.substring(0, at);
    }

    public String getUserInfo() {
        String u = getEncodedUserInfo();
        return u == null ? null : decode(u);
    }

    private String hostPort() {
        String a = getEncodedAuthority();
        if (a == null) return null;
        return a.substring(a.lastIndexOf('@') + 1);
    }

    public String getHost() {
        String hp = hostPort();
        if (hp == null) return null;
        int colon = hp.startsWith("[") ? hp.indexOf(':', hp.indexOf(']')) : hp.lastIndexOf(':');
        return decode(colon == -1 ? hp : hp.substring(0, colon));
    }

    public int getPort() {
        String hp = hostPort();
        if (hp == null) return -1;
        int colon = hp.startsWith("[") ? hp.indexOf(':', hp.indexOf(']')) : hp.lastIndexOf(':');
        if (colon == -1) return -1;
        try {
            return Integer.parseInt(decode(hp.substring(colon + 1)));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    public String getEncodedPath() {
        if (!isHierarchical()) return null;
        int[] p = parts();
        return uri.substring(p[1], p[2]);
    }

    public String getPath() {
        String p = getEncodedPath();
        return p == null ? null : decode(p);
    }

    public String getEncodedQuery() {
        if (!isHierarchical()) return null;
        int[] p = parts();
        return p[2] < p[3] ? uri.substring(p[2] + 1, p[3]) : null;
    }

    public String getQuery() {
        String q = getEncodedQuery();
        return q == null ? null : decode(q);
    }

    public String getEncodedFragment() {
        int f = fragmentSep();
        return f == -1 ? null : uri.substring(f + 1);
    }

    public String getFragment() {
        String f = getEncodedFragment();
        return f == null ? null : decode(f);
    }

    public List<String> getPathSegments() {
        String p = getEncodedPath();
        if (p == null) return Collections.emptyList();
        List<String> out = new ArrayList<>();
        for (String s : p.split("/")) if (!s.isEmpty()) out.add(decode(s));
        return Collections.unmodifiableList(out);
    }

    public String getLastPathSegment() {
        List<String> s = getPathSegments();
        return s.isEmpty() ? null : s.get(s.size() - 1);
    }

    public Set<String> getQueryParameterNames() {
        if (isOpaque()) throw new UnsupportedOperationException("This isn't a hierarchical URI.");
        String q = getEncodedQuery();
        Set<String> names = new LinkedHashSet<>();
        if (q == null) return names;
        for (String pair : q.split("&", -1)) {
            int eq = pair.indexOf('=');
            names.add(decode(eq == -1 ? pair : pair.substring(0, eq)));
        }
        return Collections.unmodifiableSet(names);
    }

    public List<String> getQueryParameters(String key) {
        if (isOpaque()) throw new UnsupportedOperationException("This isn't a hierarchical URI.");
        if (key == null) throw new NullPointerException("key");
        List<String> out = new ArrayList<>();
        String q = getEncodedQuery();
        if (q == null) return out;
        String ek = encode(key, null);
        for (String pair : q.split("&", -1)) {
            int eq = pair.indexOf('=');
            String k = eq == -1 ? pair : pair.substring(0, eq);
            if (k.equals(ek)) out.add(eq == -1 ? "" : decodePlus(pair.substring(eq + 1)));
        }
        return out;
    }

    public String getQueryParameter(String key) {
        List<String> v = getQueryParameters(key);
        return v.isEmpty() ? null : v.get(0);
    }

    public boolean getBooleanQueryParameter(String key, boolean defaultValue) {
        String v = getQueryParameter(key);
        if (v == null) return defaultValue;
        v = v.toLowerCase(java.util.Locale.ROOT);
        return !"false".equals(v) && !"0".equals(v);
    }

    public Uri normalizeScheme() {
        String s = getScheme();
        if (s == null || s.equals(s.toLowerCase(java.util.Locale.ROOT))) return this;
        return new Uri(s.toLowerCase(java.util.Locale.ROOT) + uri.substring(s.length()));
    }

    public Builder buildUpon() {
        Builder b = new Builder().scheme(getScheme());
        if (isOpaque()) return b.encodedOpaquePart(getEncodedSchemeSpecificPart()).encodedFragment(getEncodedFragment());
        return b.encodedAuthority(getEncodedAuthority()).encodedPath(getEncodedPath())
                .encodedQuery(getEncodedQuery()).encodedFragment(getEncodedFragment());
    }

    @Override public String toString() { return uri; }

    @Override public boolean equals(Object o) { return o instanceof Uri && uri.equals(((Uri) o).uri); }

    @Override public int hashCode() { return uri.hashCode(); }

    @Override public int compareTo(Uri other) { return uri.compareTo(other.uri); }

    // ---- encoding ------------------------------------------------------------------------------

    public static String encode(String s) {
        return encode(s, null);
    }

    public static String encode(String s, String allow) {
        if (s == null) return null;
        StringBuilder sb = null;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            boolean keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                    || UNRESERVED.indexOf(c) >= 0 || (allow != null && allow.indexOf(c) >= 0);
            if (keep) {
                if (sb != null) sb.append(c);
                continue;
            }
            if (sb == null) sb = new StringBuilder(s.substring(0, i));
            int end = i + 1;
            if (Character.isHighSurrogate(c) && end < s.length()) end++;
            for (byte b : s.substring(i, end).getBytes(StandardCharsets.UTF_8)) sb.append('%').append(HEX.charAt((b >> 4) & 0xF)).append(HEX.charAt(b & 0xF));
            i = end - 1;
        }
        return sb == null ? s : sb.toString();
    }

    public static String decode(String s) {
        return decode(s, false);
    }

    private static String decodePlus(String s) {
        return decode(s, true);
    }

    private static String decode(String s, boolean plusAsSpace) {
        if (s == null) return null;
        if (s.indexOf('%') < 0 && !(plusAsSpace && s.indexOf('+') >= 0)) return s;
        StringBuilder sb = new StringBuilder(s.length());
        ByteArrayOutputStream pending = new ByteArrayOutputStream();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '%' && i + 2 < s.length() && hex(s.charAt(i + 1)) >= 0 && hex(s.charAt(i + 2)) >= 0) {
                pending.write(hex(s.charAt(i + 1)) * 16 + hex(s.charAt(i + 2)));
                i += 2;
                continue;
            }
            if (pending.size() > 0) {
                sb.append(new String(pending.toByteArray(), StandardCharsets.UTF_8));  // invalid bytes become U+FFFD
                pending.reset();
            }
            sb.append(plusAsSpace && c == '+' ? ' ' : c);
        }
        if (pending.size() > 0) sb.append(new String(pending.toByteArray(), StandardCharsets.UTF_8));
        return sb.toString();
    }

    private static int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // ---- builder -------------------------------------------------------------------------------

    public static final class Builder {
        private String scheme, opaque, authority, path = "", query, fragment;

        public Builder() {}

        public Builder scheme(String scheme) { this.scheme = scheme; return this; }

        public Builder opaquePart(String opaquePart) { return encodedOpaquePart(encode(opaquePart, null)); }

        public Builder encodedOpaquePart(String opaquePart) { this.opaque = opaquePart; return this; }

        public Builder authority(String authority) { return encodedAuthority(encode(authority, "@:[]")); }

        public Builder encodedAuthority(String authority) { opaque = null; this.authority = authority; return this; }

        public Builder path(String path) { return encodedPath(path == null ? null : encode(path, "/")); }

        public Builder encodedPath(String path) { opaque = null; this.path = path == null ? "" : path; return this; }

        public Builder appendPath(String newSegment) { return appendEncodedPath(encode(newSegment, null)); }

        public Builder appendEncodedPath(String newSegment) {
            opaque = null;
            if (path.isEmpty() || !path.endsWith("/")) path = path + (newSegment.startsWith("/") ? "" : "/");
            path = path + (path.endsWith("/") && newSegment.startsWith("/") ? newSegment.substring(1) : newSegment);
            return this;
        }

        public Builder query(String query) { return encodedQuery(query == null ? null : encode(query, "=&")); }

        public Builder encodedQuery(String query) { opaque = null; this.query = query; return this; }

        public Builder appendQueryParameter(String key, String value) {
            opaque = null;
            String pair = encode(key, null) + "=" + encode(value, null);
            query = query == null || query.isEmpty() ? pair : query + "&" + pair;
            return this;
        }

        public Builder clearQuery() { query = null; return this; }

        public Builder fragment(String fragment) { return encodedFragment(fragment == null ? null : encode(fragment, null)); }

        public Builder encodedFragment(String fragment) { this.fragment = fragment; return this; }

        public Uri build() { return new Uri(toString()); }

        @Override public String toString() {
            StringBuilder sb = new StringBuilder();
            if (scheme != null) sb.append(scheme).append(':');
            if (opaque != null) {
                sb.append(opaque);
            } else {
                if (authority != null) sb.append("//").append(authority);
                if (authority != null && !path.isEmpty() && !path.startsWith("/")) sb.append('/');
                sb.append(path);
                if (query != null) sb.append('?').append(query);
            }
            if (fragment != null) sb.append('#').append(fragment);
            return sb.toString();
        }
    }
}
