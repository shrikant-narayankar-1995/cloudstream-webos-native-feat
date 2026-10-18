// SPDX-License-Identifier: GPL-3.0-or-later
// Signatures follow the Android SDK (android.content.res.XmlResourceParser).
package android.content.res;

public interface XmlResourceParser extends org.xmlpull.v1.XmlPullParser, android.util.AttributeSet, AutoCloseable {
    @Override
    String getAttributeNamespace(int index);

    @Override
    void close();
}
