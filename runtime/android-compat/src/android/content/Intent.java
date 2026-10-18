// SPDX-License-Identifier: GPL-3.0-or-later
// android.content.Intent as a plain value object (action, data URI string, extras). Delivering an
// intent (startActivity) is unsupported; building one is harmless.
package android.content;

import java.util.HashMap;
import java.util.Map;

public class Intent {
    public static final String ACTION_VIEW = "android.intent.action.VIEW";

    private String action;
    private final Map<String, Object> extras = new HashMap<>();

    public Intent() {}

    public Intent(String action) {
        this.action = action;
    }

    public String getAction() {
        return action;
    }

    public Intent setAction(String action) {
        this.action = action;
        return this;
    }

    public Intent putExtra(String name, String value) {
        extras.put(name, value);
        return this;
    }

    public String getStringExtra(String name) {
        Object v = extras.get(name);
        return v instanceof String ? (String) v : null;
    }
}
