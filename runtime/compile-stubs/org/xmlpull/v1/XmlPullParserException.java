// SPDX-License-Identifier: GPL-3.0-or-later
// Compile-time signature only (XmlPull API v1). Never dexed or shipped: ART's libcore provides it.
package org.xmlpull.v1;

public class XmlPullParserException extends Exception {
    private static final long serialVersionUID = 1L;

    public XmlPullParserException(String s) {
        super(s);
    }

    public XmlPullParserException(String msg, XmlPullParser parser, Throwable chain) {
        super(msg, chain);
    }
}
