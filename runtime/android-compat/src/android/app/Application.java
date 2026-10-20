// SPDX-License-Identifier: GPL-3.0-or-later
package android.app;

/** CloudStream 4.x plugins use com.lagradost.cloudstream3.CloudStreamApp (getContext/setKey/getKey), which
 *  extends Application: without this class every plugin touching it fails with NoClassDefFoundError. */
public class Application extends android.content.ContextWrapper {
    public Application() { super(null); }
    public void onCreate() {}
    public void onTerminate() {}
    public void onLowMemory() {}
    public void onTrimMemory(int level) {}
}
