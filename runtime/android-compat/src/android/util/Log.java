// SPDX-License-Identifier: GPL-3.0-or-later
// android.util.Log writing to stderr ("<level>/<tag>: message"). The native app treats the child's
// stderr as diagnostics and never shows or uploads it.
package android.util;

public final class Log {
    public static final int VERBOSE = 2, DEBUG = 3, INFO = 4, WARN = 5, ERROR = 6, ASSERT = 7;

    private Log() {}

    private static int write(String level, String tag, String msg, Throwable tr) {
        StringBuilder sb = new StringBuilder(level).append('/').append(tag).append(": ").append(msg);
        if (tr != null) sb.append('\n').append(getStackTraceString(tr));
        System.err.println(sb);
        return sb.length();
    }

    public static int v(String tag, String msg) { return write("V", tag, msg, null); }
    public static int v(String tag, String msg, Throwable tr) { return write("V", tag, msg, tr); }
    public static int d(String tag, String msg) { return write("D", tag, msg, null); }
    public static int d(String tag, String msg, Throwable tr) { return write("D", tag, msg, tr); }
    public static int i(String tag, String msg) { return write("I", tag, msg, null); }
    public static int i(String tag, String msg, Throwable tr) { return write("I", tag, msg, tr); }
    public static int w(String tag, String msg) { return write("W", tag, msg, null); }
    public static int w(String tag, String msg, Throwable tr) { return write("W", tag, msg, tr); }
    public static int w(String tag, Throwable tr) { return write("W", tag, "", tr); }
    public static int e(String tag, String msg) { return write("E", tag, msg, null); }
    public static int e(String tag, String msg, Throwable tr) { return write("E", tag, msg, tr); }
    public static int wtf(String tag, String msg) { return write("A", tag, msg, null); }
    public static int println(int priority, String tag, String msg) { return write(String.valueOf(priority), tag, msg, null); }
    public static boolean isLoggable(String tag, int level) { return level >= INFO; }

    public static String getStackTraceString(Throwable tr) {
        if (tr == null) return "";
        java.io.StringWriter sw = new java.io.StringWriter();
        tr.printStackTrace(new java.io.PrintWriter(sw));
        return sw.toString();
    }
}
