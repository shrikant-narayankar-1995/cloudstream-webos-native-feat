// SPDX-License-Identifier: GPL-3.0-or-later
// android.content.res.AssetManager for plugin archives. CloudStream's PluginManager creates one with
// the no-argument constructor and calls addAssetPath(<plugin .cs3>) by reflection; the archive's
// resources.arsc becomes the resource table, and assets/ and res/ files are read from the archive.
package android.content.res;

import org.csweb.compat.res.ResourceTable;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.TreeSet;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public final class AssetManager implements AutoCloseable {
    private static final long MAX_ENTRY = 16L * 1024 * 1024;

    private final List<String> paths = new ArrayList<>();
    private ResourceTable table;

    public AssetManager() {}

    /** Returns a cookie (> 0) on success and 0 on failure, as on Android. */
    public int addAssetPath(String path) {
        try {
            byte[] arsc = read(path, "resources.arsc");
            if (arsc != null) {
                if (table != null) throw new IllegalStateException("only one resource table per AssetManager");
                table = ResourceTable.parse(arsc);
            }
            paths.add(path);
            return paths.size();
        } catch (IOException | RuntimeException e) {
            android.util.Log.w("AssetManager", "addAssetPath failed for " + path + ": " + e);
            return 0;
        }
    }

    /** The resource table of the added archive, or null if it has none (compat-layer internal). */
    public ResourceTable resourceTable() {
        return table;
    }

    /** Bytes of an archive entry (e.g. "res/layout/settings.xml"), or null (compat-layer internal). */
    public byte[] readEntry(String name) throws IOException {
        for (String p : paths) {
            byte[] b = read(p, name);
            if (b != null) return b;
        }
        return null;
    }

    public InputStream open(String fileName) throws IOException {
        byte[] b = readEntry("assets/" + fileName);
        if (b == null) throw new FileNotFoundException(fileName);
        return new ByteArrayInputStream(b);
    }

    public String[] list(String path) throws IOException {
        String prefix = "assets/" + (path.isEmpty() || path.endsWith("/") ? path : path + "/");
        TreeSet<String> out = new TreeSet<>();
        for (String p : paths) {
            try (ZipFile z = new ZipFile(p)) {
                for (Enumeration<? extends ZipEntry> e = z.entries(); e.hasMoreElements(); ) {
                    String n = e.nextElement().getName();
                    if (n.startsWith(prefix) && n.length() > prefix.length()) {
                        String rest = n.substring(prefix.length());
                        out.add(rest.contains("/") ? rest.substring(0, rest.indexOf('/')) : rest);
                    }
                }
            }
        }
        return out.toArray(new String[0]);
    }

    @Override
    public void close() {}

    private static byte[] read(String zipPath, String name) throws IOException {
        if (name.startsWith("/") || name.contains("..")) throw new IOException("bad entry name " + name);
        try (ZipFile z = new ZipFile(zipPath)) {
            ZipEntry e = z.getEntry(name);
            if (e == null || e.isDirectory()) return null;
            if (e.getSize() > MAX_ENTRY) throw new IOException(name + " is larger than " + MAX_ENTRY + " bytes");
            try (InputStream in = z.getInputStream(e)) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                for (int n; (n = in.read(buf)) > 0; ) {
                    out.write(buf, 0, n);
                    if (out.size() > MAX_ENTRY) throw new IOException(name + " is larger than " + MAX_ENTRY + " bytes");
                }
                return out.toByteArray();
            }
        }
    }
}
