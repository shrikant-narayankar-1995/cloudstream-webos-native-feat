// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Minimal JSON for the compat layer's own files and the loader's output protocol, so that neither
 * depends on a library the plugin might also load. Supports objects, arrays, strings, numbers,
 * booleans and null.
 */
public final class MiniJson {
    private MiniJson() {}

    // ---- encoding ------------------------------------------------------------------------------

    public static String quote(String s) {
        StringBuilder sb = new StringBuilder(s.length() + 2).append('"');
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"': sb.append("\\\""); break;
                case '\\': sb.append("\\\\"); break;
                case '\n': sb.append("\\n"); break;
                case '\r': sb.append("\\r"); break;
                case '\t': sb.append("\\t"); break;
                default:
                    if (c < 0x20) sb.append(String.format("\\u%04x", (int) c));
                    else sb.append(c);
            }
        }
        return sb.append('"').toString();
    }

    public static String encode(Object v) {
        if (v == null) return "null";
        if (v instanceof String) return quote((String) v);
        if (v instanceof Boolean || v instanceof Integer || v instanceof Long) return v.toString();
        if (v instanceof Number) {
            double d = ((Number) v).doubleValue();
            return Double.isFinite(d) ? v.toString() : "null";
        }
        if (v instanceof Map) {
            StringBuilder sb = new StringBuilder("{");
            for (Map.Entry<?, ?> e : ((Map<?, ?>) v).entrySet()) {
                if (sb.length() > 1) sb.append(',');
                sb.append(quote(String.valueOf(e.getKey()))).append(':').append(encode(e.getValue()));
            }
            return sb.append('}').toString();
        }
        if (v instanceof Iterable) {
            StringBuilder sb = new StringBuilder("[");
            for (Object o : (Iterable<?>) v) {
                if (sb.length() > 1) sb.append(',');
                sb.append(encode(o));
            }
            return sb.append(']').toString();
        }
        return quote(v.toString());
    }

    /** Preferences file: every value wrapped as {"t": type, "v": value} so types survive a round trip. */
    public static String encodeTagged(Map<String, Object> values) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (Map.Entry<String, Object> e : values.entrySet()) {
            Object v = e.getValue();
            String t = v instanceof String ? "s" : v instanceof Boolean ? "b" : v instanceof Integer ? "i"
                    : v instanceof Long ? "l" : v instanceof Float ? "f" : v instanceof Set ? "set" : null;
            if (t == null) continue;
            Map<String, Object> w = new LinkedHashMap<>();
            w.put("t", t);
            w.put("v", v instanceof Long ? v.toString() : v);  // longs as strings: no precision loss
            out.put(e.getKey(), w);
        }
        return encode(out);
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> decodeTagged(String json) {
        Map<String, Object> out = new LinkedHashMap<>();
        Object root = decode(json);
        if (!(root instanceof Map)) throw new IllegalArgumentException("preferences: not an object");
        for (Map.Entry<String, Object> e : ((Map<String, Object>) root).entrySet()) {
            if (!(e.getValue() instanceof Map)) continue;
            Map<String, Object> w = (Map<String, Object>) e.getValue();
            Object v = w.get("v");
            switch (String.valueOf(w.get("t"))) {
                case "s": if (v instanceof String) out.put(e.getKey(), v); break;
                case "b": if (v instanceof Boolean) out.put(e.getKey(), v); break;
                case "i": if (v instanceof Number) out.put(e.getKey(), ((Number) v).intValue()); break;
                case "l": if (v instanceof String) out.put(e.getKey(), Long.parseLong((String) v)); break;
                case "f": if (v instanceof Number) out.put(e.getKey(), ((Number) v).floatValue()); break;
                case "set":
                    if (v instanceof List) {
                        Set<String> s = new LinkedHashSet<>();
                        for (Object o : (List<Object>) v) if (o instanceof String) s.add((String) o);
                        out.put(e.getKey(), s);
                    }
                    break;
                default: break;
            }
        }
        return out;
    }

    // ---- decoding ------------------------------------------------------------------------------

    public static Object decode(String s) {
        Parser p = new Parser(s);
        Object v = p.value();
        p.ws();
        if (p.i != s.length()) throw new IllegalArgumentException("trailing data at " + p.i);
        return v;
    }

    private static final class Parser {
        final String s;
        int i;
        int depth;

        Parser(String s) { this.s = s; }

        void ws() { while (i < s.length() && Character.isWhitespace(s.charAt(i))) i++; }

        IllegalArgumentException err(String m) { return new IllegalArgumentException(m + " at " + i); }

        Object value() {
            ws();
            if (i >= s.length()) throw err("unexpected end");
            if (++depth > 64) throw err("too deep");
            try {
                char c = s.charAt(i);
                if (c == '{') return object();
                if (c == '[') return array();
                if (c == '"') return string();
                if (s.startsWith("true", i)) { i += 4; return Boolean.TRUE; }
                if (s.startsWith("false", i)) { i += 5; return Boolean.FALSE; }
                if (s.startsWith("null", i)) { i += 4; return null; }
                return number();
            } finally {
                depth--;
            }
        }

        Map<String, Object> object() {
            Map<String, Object> m = new LinkedHashMap<>();
            i++;
            ws();
            if (i < s.length() && s.charAt(i) == '}') { i++; return m; }
            while (true) {
                ws();
                if (i >= s.length() || s.charAt(i) != '"') throw err("expected key");
                String k = string();
                ws();
                if (i >= s.length() || s.charAt(i++) != ':') throw err("expected ':'");
                m.put(k, value());
                ws();
                if (i >= s.length()) throw err("unterminated object");
                char c = s.charAt(i++);
                if (c == '}') return m;
                if (c != ',') throw err("expected ',' or '}'");
            }
        }

        List<Object> array() {
            List<Object> l = new ArrayList<>();
            i++;
            ws();
            if (i < s.length() && s.charAt(i) == ']') { i++; return l; }
            while (true) {
                l.add(value());
                ws();
                if (i >= s.length()) throw err("unterminated array");
                char c = s.charAt(i++);
                if (c == ']') return l;
                if (c != ',') throw err("expected ',' or ']'");
            }
        }

        String string() {
            StringBuilder sb = new StringBuilder();
            i++;
            while (true) {
                if (i >= s.length()) throw err("unterminated string");
                char c = s.charAt(i++);
                if (c == '"') return sb.toString();
                if (c != '\\') { sb.append(c); continue; }
                if (i >= s.length()) throw err("bad escape");
                char e = s.charAt(i++);
                switch (e) {
                    case 'n': sb.append('\n'); break;
                    case 'r': sb.append('\r'); break;
                    case 't': sb.append('\t'); break;
                    case 'b': sb.append('\b'); break;
                    case 'f': sb.append('\f'); break;
                    case 'u':
                        if (i + 4 > s.length()) throw err("bad \\u escape");
                        sb.append((char) Integer.parseInt(s.substring(i, i + 4), 16));
                        i += 4;
                        break;
                    default: sb.append(e);
                }
            }
        }

        Number number() {
            int start = i;
            while (i < s.length() && "+-0123456789.eE".indexOf(s.charAt(i)) >= 0) i++;
            String n = s.substring(start, i);
            if (n.isEmpty()) throw err("unexpected character");
            if (n.indexOf('.') < 0 && n.indexOf('e') < 0 && n.indexOf('E') < 0) {
                long l = Long.parseLong(n);
                return l == (int) l ? (Number) (int) l : (Number) l;
            }
            return Double.parseDouble(n);
        }
    }
}
