// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat.res;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Parser for resources.arsc (ResTable, as produced by aapt/aapt2), including sparse, 16-bit-offset and
 * compact entries. Selects among configurations for the headless TV runtime: API 29, landscape,
 * television UI mode, night, xhdpi, the default locale.
 */
public final class ResourceTable {
    static final int RES_TABLE_TYPE = 0x0002;
    static final int RES_TABLE_PACKAGE_TYPE = 0x0200;
    static final int RES_TABLE_TYPE_TYPE = 0x0201;

    private static final int FLAG_COMPLEX = 0x0001;
    private static final int FLAG_COMPACT = 0x0008;
    private static final int TYPE_FLAG_SPARSE = 0x01;
    private static final int TYPE_FLAG_OFFSET16 = 0x02;
    private static final int NO_ENTRY = 0xffffffff;
    static final int SDK = 29;
    static final int DENSITY = 320;

    /** A typed value (Res_value). {@code string} is set for TYPE_STRING. */
    public static final class Value {
        public final int type;
        public final int data;
        public final String string;

        Value(int type, int data, String string) {
            this.type = type;
            this.data = data;
            this.string = string;
        }
    }

    /** A bag (complex entry): style, array, plurals... keys are attribute/index ids, in file order. */
    public static final class Bag {
        public final int parent;
        public final Map<Integer, Value> items;

        Bag(int parent, Map<Integer, Value> items) {
            this.parent = parent;
            this.items = items;
        }
    }

    private static final class Candidate {
        final byte[] config;
        final Value value;
        final Bag bag;

        Candidate(byte[] config, Value value, Bag bag) {
            this.config = config;
            this.value = value;
            this.bag = bag;
        }
    }

    private int packageId;
    private String packageName;
    private StringPool global;
    private final Map<Integer, List<Candidate>> entries = new HashMap<>();
    private final Map<String, Integer> byName = new HashMap<>();   // "type/name" -> id
    private final Map<Integer, String[]> names = new HashMap<>();  // id -> {type, name}

    public static ResourceTable parse(byte[] arsc) {
        ResourceTable t = new ResourceTable();
        ByteBuffer b = ByteBuffer.wrap(arsc).order(ByteOrder.LITTLE_ENDIAN);
        if (arsc.length < 12 || (b.getShort(0) & 0xffff) != RES_TABLE_TYPE) throw new IllegalArgumentException("not a resource table");
        int headerSize = b.getShort(2) & 0xffff;
        int size = b.getInt(4);
        if (size > arsc.length || size < headerSize) throw new IllegalArgumentException("truncated resource table");
        for (int p = headerSize; p + 8 <= size; ) {
            int type = b.getShort(p) & 0xffff;
            int chunk = b.getInt(p + 4);
            if (chunk < 8 || p + chunk > size) throw new IllegalArgumentException("bad chunk at " + p);
            if (type == StringPool.TYPE) {
                t.global = new StringPool(b, p);
            } else if (type == RES_TABLE_PACKAGE_TYPE) {
                if (t.packageName != null) throw new IllegalArgumentException("more than one package in a plugin");
                t.parsePackage(b, p, chunk);
            }
            p += chunk;
        }
        if (t.global == null || t.packageName == null) throw new IllegalArgumentException("resource table without strings or package");
        return t;
    }

    private void parsePackage(ByteBuffer b, int start, int chunk) {
        int headerSize = b.getShort(start + 2) & 0xffff;
        packageId = b.getInt(start + 8);
        StringBuilder name = new StringBuilder();
        for (int i = 0; i < 128; i++) {
            char c = b.getChar(start + 12 + 2 * i);
            if (c == 0) break;
            name.append(c);
        }
        packageName = name.toString();
        StringPool typeNames = new StringPool(b, start + b.getInt(start + 12 + 256));
        StringPool keyNames = new StringPool(b, start + b.getInt(start + 12 + 256 + 8));
        for (int p = start + headerSize; p + 8 <= start + chunk; ) {
            int type = b.getShort(p) & 0xffff;
            int size = b.getInt(p + 4);
            if (size < 8 || p + size > start + chunk) throw new IllegalArgumentException("bad package chunk at " + p);
            if (type == RES_TABLE_TYPE_TYPE) parseType(b, p, typeNames, keyNames);
            p += size;
        }
    }

    private void parseType(ByteBuffer b, int start, StringPool typeNames, StringPool keyNames) {
        int headerSize = b.getShort(start + 2) & 0xffff;
        int typeId = b.get(start + 8) & 0xff;
        int flags = b.get(start + 9) & 0xff;
        int entryCount = b.getInt(start + 12);
        int entriesStart = start + b.getInt(start + 16);
        int configSize = b.getInt(start + 20);
        byte[] config = new byte[Math.max(configSize, 0)];
        for (int i = 0; i < config.length; i++) config[i] = b.get(start + 20 + i);
        String typeName = typeNames.get(typeId - 1);
        int offsets = start + headerSize;
        for (int i = 0; i < entryCount; i++) {
            int index, offset;
            if ((flags & TYPE_FLAG_SPARSE) != 0) {
                index = b.getShort(offsets + 4 * i) & 0xffff;
                offset = (b.getShort(offsets + 4 * i + 2) & 0xffff) * 4;
            } else if ((flags & TYPE_FLAG_OFFSET16) != 0) {
                index = i;
                int o = b.getShort(offsets + 2 * i) & 0xffff;
                if (o == 0xffff) continue;
                offset = o * 4;
            } else {
                index = i;
                offset = b.getInt(offsets + 4 * i);
                if (offset == NO_ENTRY) continue;
            }
            int e = entriesStart + offset;
            int eSize = b.getShort(e) & 0xffff;
            int eFlags = b.getShort(e + 2) & 0xffff;
            int key;
            Value value = null;
            Bag bag = null;
            if ((eFlags & FLAG_COMPACT) != 0) {
                key = eSize;
                value = value((eFlags >> 8) & 0xff, b.getInt(e + 4));
            } else if ((eFlags & FLAG_COMPLEX) != 0) {
                key = b.getInt(e + 4);
                int parent = b.getInt(e + 8);
                int count = b.getInt(e + 12);
                Map<Integer, Value> items = new LinkedHashMap<>();
                int m = e + eSize;
                for (int k = 0; k < count; k++, m += 12) {
                    items.put(b.getInt(m), value(b.get(m + 7) & 0xff, b.getInt(m + 8)));
                }
                bag = new Bag(parent, Collections.unmodifiableMap(items));
            } else {
                key = b.getInt(e + 4);
                int v = e + eSize;
                value = value(b.get(v + 3) & 0xff, b.getInt(v + 4));
            }
            int id = (packageId << 24) | (typeId << 16) | index;
            String entryName = keyNames.get(key);
            entries.computeIfAbsent(id, k -> new ArrayList<>()).add(new Candidate(config, value, bag));
            byName.put(typeName + "/" + entryName, id);
            names.put(id, new String[]{typeName, entryName});
        }
    }

    private Value value(int type, int data) {
        return new Value(type, data, type == TypedValues.TYPE_STRING ? global.get(data) : null);
    }

    // ---- configuration selection --------------------------------------------------------------

    private static int u16(byte[] c, int off) {
        return off + 1 < c.length ? (c[off] & 0xff) | (c[off + 1] & 0xff) << 8 : 0;
    }

    private static int u8(byte[] c, int off) {
        return off < c.length ? c[off] & 0xff : 0;
    }

    private static String lang(byte[] c) {
        if (c.length < 10 || c[8] == 0) return "";
        if ((c[8] & 0x80) != 0) return "?";  // packed three-letter code: never the default here
        return new String(new char[]{(char) c[8], (char) c[9]});
    }

    /** -1 if the configuration cannot apply to the TV runtime, else a score (higher is better). */
    static int score(byte[] c) {
        if (u16(c, 4) != 0 || u16(c, 6) != 0) return -1;                  // mcc/mnc: no SIM
        String l = lang(c);
        String want = Locale.getDefault().getLanguage();
        if (!l.isEmpty() && !l.equals(want)) return -1;
        int orientation = u8(c, 12);
        if (orientation != 0 && orientation != 2) return -1;               // landscape
        int touch = u8(c, 13);
        if (touch != 0 && touch != 1) return -1;                           // notouch
        int sdk = u16(c, 24);
        if (sdk > SDK) return -1;
        int uiMode = u8(c, 29);
        if ((uiMode & 0x0f) != 0 && (uiMode & 0x0f) != 4) return -1;      // television
        if ((uiMode & 0x30) != 0 && (uiMode & 0x30) != 0x20) return -1;   // night
        int density = u16(c, 14);
        int score = 0;
        if (!l.isEmpty()) score += 1 << 24;
        if ((uiMode & 0x0f) == 4) score += 1 << 22;
        score += sdk << 10;
        if (density != 0) score += 1000 - Math.min(999, Math.abs(density - DENSITY));
        return score;
    }

    private Candidate best(int id) {
        List<Candidate> list = entries.get(id);
        if (list == null) return null;
        Candidate best = null;
        int bestScore = -1;
        for (Candidate c : list) {
            int s = score(c.config);
            if (s > bestScore) {
                bestScore = s;
                best = c;
            }
        }
        return best;
    }

    // ---- lookups ---------------------------------------------------------------------------------

    public String packageName() {
        return packageName;
    }

    public int packageId() {
        return packageId;
    }

    /** 0 when there is no such resource, as Resources.getIdentifier answers. */
    public int identifier(String type, String name) {
        Integer id = byName.get(type + "/" + name);
        return id == null ? 0 : id;
    }

    public boolean has(int id) {
        return best(id) != null;
    }

    public String typeName(int id) {
        String[] n = names.get(id);
        return n == null ? null : n[0];
    }

    public String entryName(int id) {
        String[] n = names.get(id);
        return n == null ? null : n[1];
    }

    /** The simple value of {@code id} for the best configuration, references followed; null if none. */
    public Value value(int id) {
        for (int depth = 0; depth < 20; depth++) {
            Candidate c = best(id);
            if (c == null || c.value == null) return null;
            if (c.value.type != TypedValues.TYPE_REFERENCE || !has(c.value.data)) return c.value;
            id = c.value.data;
        }
        return null;
    }

    public Bag bag(int id) {
        Candidate c = best(id);
        return c == null ? null : c.bag;
    }

    /** Follow a reference found in a bag or XML attribute; other values come back unchanged. */
    public Value resolve(Value v) {
        if (v != null && v.type == TypedValues.TYPE_REFERENCE && has(v.data)) {
            Value r = value(v.data);
            return r != null ? r : v;
        }
        return v;
    }
}
