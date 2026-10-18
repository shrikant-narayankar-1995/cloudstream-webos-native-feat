// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * Activity objects for plugins whose load(Context) insists on an Activity type (CloudStream passes its
 * MainActivity there on Android). The instance is a genuine object of the requested class, e.g.
 * androidx.appcompat.app.AppCompatActivity, created without running its constructors (there is no
 * window, lifecycle or fragment host on webOS), with its base context attached. Context methods work
 * through the base context; the activity's own UI machinery is absent, so anything that tries to show
 * a screen fails, and Activity reports itself as finishing and destroyed so well-behaved UI code skips.
 */
public final class HeadlessActivities {
    private HeadlessActivities() {}

    public static Activity create(Class<?> type, Context base) throws ReflectiveOperationException {
        if (!Activity.class.isAssignableFrom(type)) throw new IllegalArgumentException(type + " is not an Activity");
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field f;
        try {
            f = unsafeClass.getDeclaredField("theUnsafe");
        } catch (NoSuchFieldException e) {
            f = unsafeClass.getDeclaredField("THE_ONE");  // Android's libcore name
        }
        f.setAccessible(true);
        Object unsafe = f.get(null);
        Method allocate = unsafeClass.getMethod("allocateInstance", Class.class);
        Activity a = (Activity) allocate.invoke(unsafe, type);
        Field mBase = ContextWrapper.class.getDeclaredField("mBase");
        mBase.setAccessible(true);
        mBase.set(a, base);
        return a;
    }
}
