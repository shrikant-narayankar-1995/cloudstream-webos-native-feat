// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat.res;

import android.util.DisplayMetrics;

/** Res_value data types and the platform's conversions (android.util.TypedValue semantics). */
public final class TypedValues {
    public static final int TYPE_NULL = 0x00, TYPE_REFERENCE = 0x01, TYPE_ATTRIBUTE = 0x02, TYPE_STRING = 0x03,
            TYPE_FLOAT = 0x04, TYPE_DIMENSION = 0x05, TYPE_FRACTION = 0x06, TYPE_INT_DEC = 0x10, TYPE_INT_HEX = 0x11,
            TYPE_INT_BOOLEAN = 0x12, TYPE_INT_COLOR_ARGB8 = 0x1c, TYPE_INT_COLOR_RGB8 = 0x1d,
            TYPE_INT_COLOR_ARGB4 = 0x1e, TYPE_INT_COLOR_RGB4 = 0x1f;

    private static final float[] RADIX_MULTS = {1.0f / (1 << 8), 1.0f / (1 << 15), 1.0f / (1 << 23), 1.0f / (1 << 31)};
    private static final String[] UNITS = {"px", "dip", "sp", "pt", "in", "mm"};

    private TypedValues() {}

    public static float complexToFloat(int complex) {
        return (complex & 0xffffff00) * RADIX_MULTS[(complex >> 4) & 3];
    }

    public static float applyDimension(int unit, float value, DisplayMetrics m) {
        switch (unit) {
            case 0: return value;
            case 1: return value * m.density;
            case 2: return value * m.scaledDensity;
            case 3: return value * m.xdpi / 72f;
            case 4: return value * m.xdpi;
            case 5: return value * m.xdpi / 25.4f;
            default: return 0;
        }
    }

    public static float complexToDimension(int data, DisplayMetrics m) {
        return applyDimension(data & 0xf, complexToFloat(data), m);
    }

    public static int complexToDimensionPixelSize(int data, DisplayMetrics m) {
        float value = complexToFloat(data);
        float f = applyDimension(data & 0xf, value, m);
        int res = (int) (f >= 0 ? f + 0.5f : f - 0.5f);
        if (res != 0) return res;
        if (value == 0) return 0;
        return value > 0 ? 1 : -1;
    }

    public static boolean isColor(int type) {
        return type >= TYPE_INT_COLOR_ARGB8 && type <= TYPE_INT_COLOR_RGB4;
    }

    public static boolean isInt(int type) {
        return type >= TYPE_INT_DEC && type <= TYPE_INT_COLOR_RGB4;
    }

    /** The string Android's XML parser reports for a typed attribute without raw text. */
    public static String coerceToString(int type, int data, String string) {
        switch (type) {
            case TYPE_NULL: return null;
            case TYPE_STRING: return string;
            case TYPE_REFERENCE: return "@" + data;
            case TYPE_ATTRIBUTE: return "?" + data;
            case TYPE_FLOAT: return Float.toString(Float.intBitsToFloat(data));
            case TYPE_DIMENSION: {
                int unit = data & 0xf;
                return trim(complexToFloat(data)) + (unit < UNITS.length ? UNITS[unit] : "");
            }
            case TYPE_FRACTION: return trim(complexToFloat(data) * 100) + ((data & 0xf) == 0 ? "%" : "%p");
            case TYPE_INT_HEX: return "0x" + Integer.toHexString(data);
            case TYPE_INT_BOOLEAN: return data != 0 ? "true" : "false";
            default:
                if (isColor(type)) return "#" + String.format("%08x", data);
                return Integer.toString(data);
        }
    }

    private static String trim(float f) {
        return f == (long) f ? Long.toString((long) f) + ".0" : Float.toString(f);
    }
}
