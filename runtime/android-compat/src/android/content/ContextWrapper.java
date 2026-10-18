// SPDX-License-Identifier: GPL-3.0-or-later
package android.content;

import java.io.File;

public class ContextWrapper extends Context {
    private Context base;

    public ContextWrapper(Context base) {
        this.base = base;
    }

    public Context getBaseContext() {
        return base;
    }

    @Override public SharedPreferences getSharedPreferences(String name, int mode) { return base.getSharedPreferences(name, mode); }
    @Override public Context getApplicationContext() { return base.getApplicationContext(); }
    @Override public String getPackageName() { return base.getPackageName(); }
    @Override public File getFilesDir() { return base.getFilesDir(); }
    @Override public File getCacheDir() { return base.getCacheDir(); }
    @Override public ClassLoader getClassLoader() { return base.getClassLoader(); }
}
