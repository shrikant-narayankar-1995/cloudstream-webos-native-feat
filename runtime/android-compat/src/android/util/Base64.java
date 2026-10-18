// SPDX-License-Identifier: GPL-3.0-or-later
// android.util.Base64 with the platform's documented behaviour:
// - encode: DEFAULT wraps lines at 76 characters and ends with a line terminator ("\n", or "\r\n" with
//   CRLF); NO_WRAP produces one line; NO_PADDING drops '='; URL_SAFE uses '-' and '_'.
// - decode: characters outside the alphabet (whitespace, line breaks...) are skipped; padding may be
//   absent; misplaced padding or a dangling single character throws IllegalArgumentException("bad base-64").
package android.util;

import java.io.ByteArrayOutputStream;

public class Base64 {
    public static final int DEFAULT = 0;
    public static final int NO_PADDING = 1;
    public static final int NO_WRAP = 2;
    public static final int CRLF = 4;
    public static final int URL_SAFE = 8;
    public static final int NO_CLOSE = 16;

    private static final char[] STD = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".toCharArray();
    private static final char[] URL = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_".toCharArray();
    private static final int LINE_GROUPS = 19;  // 19 * 4 = 76 characters per line

    private Base64() {}

    // ---- decode ----------------------------------------------------------------------------------

    public static byte[] decode(String str, int flags) {
        return decode(str.getBytes(java.nio.charset.StandardCharsets.US_ASCII), flags);
    }

    public static byte[] decode(byte[] input, int flags) {
        return decode(input, 0, input.length, flags);
    }

    public static byte[] decode(byte[] input, int offset, int len, int flags) {
        char[] alphabet = (flags & URL_SAFE) != 0 ? URL : STD;
        int[] value = new int[256];
        java.util.Arrays.fill(value, -1);
        for (int i = 0; i < alphabet.length; i++) value[alphabet[i]] = i;
        ByteArrayOutputStream out = new ByteArrayOutputStream(len * 3 / 4);
        int acc = 0, n = 0, pad = 0;
        for (int i = offset; i < offset + len; i++) {
            int c = input[i] & 0xff;
            if (c == '=') {
                pad++;
                continue;
            }
            int v = value[c];
            if (v < 0) continue;  // skip characters outside the alphabet
            if (pad > 0) throw new IllegalArgumentException("bad base-64");
            acc = (acc << 6) | v;
            if (++n == 4) {
                out.write(acc >> 16);
                out.write(acc >> 8);
                out.write(acc);
                acc = 0;
                n = 0;
            }
        }
        if (n == 1 || pad > 2 || (pad > 0 && n == 0) || (pad > 0 && n + pad != 4)) {
            throw new IllegalArgumentException("bad base-64");
        }
        if (n == 2) {
            out.write(acc >> 4);
        } else if (n == 3) {
            out.write(acc >> 10);
            out.write(acc >> 2);
        }
        return out.toByteArray();
    }

    // ---- encode ----------------------------------------------------------------------------------

    public static String encodeToString(byte[] input, int flags) {
        return new String(encode(input, flags), java.nio.charset.StandardCharsets.US_ASCII);
    }

    public static String encodeToString(byte[] input, int offset, int len, int flags) {
        return new String(encode(input, offset, len, flags), java.nio.charset.StandardCharsets.US_ASCII);
    }

    public static byte[] encode(byte[] input, int flags) {
        return encode(input, 0, input.length, flags);
    }

    public static byte[] encode(byte[] input, int offset, int len, int flags) {
        char[] alphabet = (flags & URL_SAFE) != 0 ? URL : STD;
        boolean pad = (flags & NO_PADDING) == 0;
        boolean wrap = (flags & NO_WRAP) == 0;
        String eol = (flags & CRLF) != 0 ? "\r\n" : "\n";
        StringBuilder sb = new StringBuilder(len * 4 / 3 + 4);
        int groups = 0;
        int i = offset, end = offset + len;
        for (; i + 2 < end; i += 3) {
            int v = ((input[i] & 0xff) << 16) | ((input[i + 1] & 0xff) << 8) | (input[i + 2] & 0xff);
            sb.append(alphabet[v >> 18]).append(alphabet[(v >> 12) & 63]).append(alphabet[(v >> 6) & 63]).append(alphabet[v & 63]);
            if (wrap && ++groups == LINE_GROUPS) {
                sb.append(eol);
                groups = 0;
            }
        }
        int rest = end - i;
        if (rest > 0) {
            int v = (input[i] & 0xff) << 16 | (rest == 2 ? (input[i + 1] & 0xff) << 8 : 0);
            sb.append(alphabet[v >> 18]).append(alphabet[(v >> 12) & 63]);
            if (rest == 2) sb.append(alphabet[(v >> 6) & 63]);
            if (pad) sb.append(rest == 2 ? "=" : "==");
            groups++;
        }
        if (wrap && groups > 0) sb.append(eol);
        return sb.toString().getBytes(java.nio.charset.StandardCharsets.US_ASCII);
    }
}
