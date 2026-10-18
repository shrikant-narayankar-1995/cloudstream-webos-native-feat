// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.File;

/**
 * The Context handed to Plugin.load(Context) in the headless runtime. It is an application context,
 * never an Activity: plugin code that needs an Activity (dialogs, fragments) sees `context !is Activity`
 * and is expected to skip its UI, exactly as it would when CloudStream passes a non-Activity context.
 */
public final class HeadlessContext extends Context {
    private final File dataDir;
    private final String packageName;

    public HeadlessContext(File dataDir, String packageName) {
        this.dataDir = dataDir;
        this.packageName = packageName;
    }

    @Override public SharedPreferences getSharedPreferences(String name, int mode) {
        String safe = name == null ? "null" : name.replaceAll("[^A-Za-z0-9._-]", "_");
        return JsonSharedPreferences.open(new File(new File(dataDir, "shared_prefs"), safe + ".json"));
    }

    @Override public Context getApplicationContext() { return this; }

    @Override public String getPackageName() { return packageName; }

    @Override public File getFilesDir() { return mkdirs(new File(dataDir, "files")); }

    @Override public File getCacheDir() { return mkdirs(new File(dataDir, "cache")); }

    @Override public ClassLoader getClassLoader() { return HeadlessContext.class.getClassLoader(); }

    private static File mkdirs(File f) {
        if (!f.isDirectory()) f.mkdirs();
        return f;
    }
}
