// SPDX-License-Identifier: GPL-3.0-or-later
// Compile-time signature only. Never dexed or shipped: ART's libcore provides the real class.
package dalvik.system;

public class PathClassLoader extends ClassLoader {
    public PathClassLoader(String dexPath, ClassLoader parent) {
        super(parent);
        throw new UnsupportedOperationException("compile stub");
    }
}
