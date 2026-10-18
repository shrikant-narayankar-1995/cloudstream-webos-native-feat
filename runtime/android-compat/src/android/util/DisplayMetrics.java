// SPDX-License-Identifier: GPL-3.0-or-later
// Display metrics of the TV UI plane: 1920x1080 px (measured on the reference TV), xhdpi (density 2.0,
// 320 dpi), the convention Android TV uses for 1080p.
package android.util;

public class DisplayMetrics {
    public static final int DENSITY_LOW = 120, DENSITY_MEDIUM = 160, DENSITY_TV = 213, DENSITY_HIGH = 240,
            DENSITY_XHIGH = 320, DENSITY_XXHIGH = 480, DENSITY_XXXHIGH = 640, DENSITY_DEFAULT = DENSITY_MEDIUM;

    public int widthPixels;
    public int heightPixels;
    public float density;
    public int densityDpi;
    public float scaledDensity;
    public float xdpi;
    public float ydpi;

    public void setToDefaults() {
        widthPixels = 1920;
        heightPixels = 1080;
        densityDpi = DENSITY_XHIGH;
        density = densityDpi / (float) DENSITY_DEFAULT;
        scaledDensity = density;
        xdpi = ydpi = densityDpi;
    }

    public void setTo(DisplayMetrics o) {
        widthPixels = o.widthPixels;
        heightPixels = o.heightPixels;
        density = o.density;
        densityDpi = o.densityDpi;
        scaledDensity = o.scaledDensity;
        xdpi = o.xdpi;
        ydpi = o.ydpi;
    }
}
