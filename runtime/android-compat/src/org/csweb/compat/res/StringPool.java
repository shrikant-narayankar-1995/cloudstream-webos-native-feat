// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat.res;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

/** ResStringPool (RES_STRING_POOL_TYPE 0x0001) as written by aapt/aapt2: UTF-8 or UTF-16 strings. */
final class StringPool {
    static final int TYPE = 0x0001;
    private static final int UTF8_FLAG = 1 << 8;

    private final String[] strings;

    /** buf positioned anywhere; chunk starts at {@code start}. */
    StringPool(ByteBuffer buf, int start) {
        ByteBuffer b = buf.duplicate().order(ByteOrder.LITTLE_ENDIAN);
        int type = b.getShort(start) & 0xffff;
        int headerSize = b.getShort(start + 2) & 0xffff;
        int chunkSize = b.getInt(start + 4);
        if (type != TYPE) throw new IllegalArgumentException("not a string pool: 0x" + Integer.toHexString(type));
        int count = b.getInt(start + 8);
        int flags = b.getInt(start + 16);
        int stringsStart = b.getInt(start + 20);
        if (count < 0 || count > 1_000_000 || chunkSize < headerSize) throw new IllegalArgumentException("bad string pool");
        boolean utf8 = (flags & UTF8_FLAG) != 0;
        strings = new String[count];
        int base = start + stringsStart;
        for (int i = 0; i < count; i++) {
            int off = base + b.getInt(start + headerSize + 4 * i);
            if (off < start || off >= start + chunkSize) throw new IllegalArgumentException("string offset out of range");
            strings[i] = utf8 ? utf8At(b, off) : utf16At(b, off);
        }
    }

    private static String utf8At(ByteBuffer b, int off) {
        int p = off;
        int u16len = b.get(p++) & 0xff;  // UTF-16 length (unused, but must be skipped)
        if ((u16len & 0x80) != 0) p++;
        int len = b.get(p++) & 0xff;
        if ((len & 0x80) != 0) len = ((len & 0x7f) << 8) | (b.get(p++) & 0xff);
        byte[] bytes = new byte[len];
        for (int i = 0; i < len; i++) bytes[i] = b.get(p + i);
        return new String(bytes, StandardCharsets.UTF_8);
    }

    private static String utf16At(ByteBuffer b, int off) {
        int p = off;
        int len = b.getShort(p) & 0xffff;
        p += 2;
        if ((len & 0x8000) != 0) {
            len = ((len & 0x7fff) << 16) | (b.getShort(p) & 0xffff);
            p += 2;
        }
        char[] c = new char[len];
        for (int i = 0; i < len; i++) c[i] = b.getChar(p + 2 * i);
        return new String(c);
    }

    String get(int index) {
        return index >= 0 && index < strings.length ? strings[index] : null;
    }

    int size() {
        return strings.length;
    }

    int indexOf(String s) {
        for (int i = 0; i < strings.length; i++) if (strings[i].equals(s)) return i;
        return -1;
    }
}
