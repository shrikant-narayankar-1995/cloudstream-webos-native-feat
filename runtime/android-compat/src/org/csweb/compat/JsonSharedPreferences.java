// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat;

import android.content.SharedPreferences;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * SharedPreferences persisted as one small JSON object per file, replaced atomically (write to a
 * temporary file, then rename). Values: String, Boolean, Integer, Long, Float, Set<String>; numbers
 * are tagged so they read back with their original type. Thread-safe.
 */
public final class JsonSharedPreferences implements SharedPreferences {
    private static final Map<String, JsonSharedPreferences> OPEN = new HashMap<>();

    private final File file;
    private final Map<String, Object> values = new HashMap<>();
    private final List<OnSharedPreferenceChangeListener> listeners = new ArrayList<>();

    public static synchronized JsonSharedPreferences open(File file) {
        return OPEN.computeIfAbsent(file.getAbsolutePath(), k -> new JsonSharedPreferences(file));
    }

    private JsonSharedPreferences(File file) {
        this.file = file;
        if (file.isFile()) {
            try (InputStream in = new FileInputStream(file)) {
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                for (int n; (n = in.read(buf)) > 0; ) bytes.write(buf, 0, n);
                values.putAll(MiniJson.decodeTagged(new String(bytes.toByteArray(), StandardCharsets.UTF_8)));
            } catch (IOException | RuntimeException e) {
                android.util.Log.w("csweb", "ignoring unreadable preferences " + file.getName() + ": " + e);
            }
        }
    }

    private synchronized Object get(String key) {
        return values.get(key);
    }

    @Override public synchronized Map<String, ?> getAll() { return new HashMap<>(values); }

    @Override public String getString(String key, String def) {
        Object v = get(key);
        return v instanceof String ? (String) v : def;
    }

    @SuppressWarnings("unchecked")
    @Override public Set<String> getStringSet(String key, Set<String> def) {
        Object v = get(key);
        return v instanceof Set ? new LinkedHashSet<>((Set<String>) v) : def;
    }

    @Override public int getInt(String key, int def) {
        Object v = get(key);
        if (v != null && !(v instanceof Integer)) throw new ClassCastException(key + " is not an int");
        return v == null ? def : (Integer) v;
    }

    @Override public long getLong(String key, long def) {
        Object v = get(key);
        if (v != null && !(v instanceof Long)) throw new ClassCastException(key + " is not a long");
        return v == null ? def : (Long) v;
    }

    @Override public float getFloat(String key, float def) {
        Object v = get(key);
        if (v != null && !(v instanceof Float)) throw new ClassCastException(key + " is not a float");
        return v == null ? def : (Float) v;
    }

    @Override public boolean getBoolean(String key, boolean def) {
        Object v = get(key);
        if (v != null && !(v instanceof Boolean)) throw new ClassCastException(key + " is not a boolean");
        return v == null ? def : (Boolean) v;
    }

    @Override public synchronized boolean contains(String key) { return values.containsKey(key); }

    @Override public Editor edit() { return new EditorImpl(); }

    @Override public synchronized void registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener l) { listeners.add(l); }

    @Override public synchronized void unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener l) { listeners.remove(l); }

    private boolean commitChanges(Map<String, Object> changes, Set<String> removals, boolean clear) {
        List<OnSharedPreferenceChangeListener> notify;
        Set<String> changed = new HashSet<>();
        boolean ok;
        synchronized (this) {
            if (clear) {
                changed.addAll(values.keySet());
                values.clear();
            }
            for (String k : removals) if (values.remove(k) != null) changed.add(k);
            for (Map.Entry<String, Object> e : changes.entrySet()) {
                values.put(e.getKey(), e.getValue());
                changed.add(e.getKey());
            }
            ok = write();
            notify = new ArrayList<>(listeners);
        }
        for (OnSharedPreferenceChangeListener l : notify) for (String k : changed) l.onSharedPreferenceChanged(this, k);
        return ok;
    }

    private boolean write() {
        File dir = file.getParentFile();
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) return false;
        File tmp = new File(file.getPath() + ".tmp");
        try (FileOutputStream out = new FileOutputStream(tmp)) {
            out.write(MiniJson.encodeTagged(values).getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        } catch (IOException e) {
            android.util.Log.w("csweb", "cannot write preferences " + file.getName() + ": " + e);
            return false;
        }
        return tmp.renameTo(file);
    }

    private final class EditorImpl implements Editor {
        private final Map<String, Object> changes = new HashMap<>();
        private final Set<String> removals = new HashSet<>();
        private boolean clear;

        private Editor put(String key, Object value) {
            if (value == null) return remove(key);
            removals.remove(key);
            changes.put(key, value);
            return this;
        }

        @Override public Editor putString(String key, String value) { return put(key, value); }
        @Override public Editor putStringSet(String key, Set<String> values) { return put(key, values == null ? null : new LinkedHashSet<>(values)); }
        @Override public Editor putInt(String key, int value) { return put(key, value); }
        @Override public Editor putLong(String key, long value) { return put(key, value); }
        @Override public Editor putFloat(String key, float value) { return put(key, value); }
        @Override public Editor putBoolean(String key, boolean value) { return put(key, value); }

        @Override public Editor remove(String key) {
            changes.remove(key);
            removals.add(key);
            return this;
        }

        @Override public Editor clear() {
            clear = true;
            return this;
        }

        @Override public boolean commit() { return commitChanges(changes, removals, clear); }

        @Override public void apply() { commitChanges(changes, removals, clear); }
    }
}
