// SPDX-License-Identifier: GPL-3.0-or-later
// Compile-time signature only (XmlPull API v1). Never dexed or shipped: ART's libcore provides it.
package org.xmlpull.v1;

import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;

public interface XmlPullParser {
    String NO_NAMESPACE = "";
    int START_DOCUMENT = 0, END_DOCUMENT = 1, START_TAG = 2, END_TAG = 3, TEXT = 4, CDSECT = 5, ENTITY_REF = 6,
            IGNORABLE_WHITESPACE = 7, PROCESSING_INSTRUCTION = 8, COMMENT = 9, DOCDECL = 10;
    String FEATURE_PROCESS_NAMESPACES = "http://xmlpull.org/v1/doc/features.html#process-namespaces";

    void setFeature(String name, boolean state) throws XmlPullParserException;
    boolean getFeature(String name);
    void setProperty(String name, Object value) throws XmlPullParserException;
    Object getProperty(String name);
    void setInput(Reader in) throws XmlPullParserException;
    void setInput(InputStream inputStream, String inputEncoding) throws XmlPullParserException;
    String getInputEncoding();
    void defineEntityReplacementText(String entityName, String replacementText) throws XmlPullParserException;
    int getNamespaceCount(int depth) throws XmlPullParserException;
    String getNamespacePrefix(int pos) throws XmlPullParserException;
    String getNamespaceUri(int pos) throws XmlPullParserException;
    String getNamespace(String prefix);
    int getDepth();
    String getPositionDescription();
    int getLineNumber();
    int getColumnNumber();
    boolean isWhitespace() throws XmlPullParserException;
    String getText();
    char[] getTextCharacters(int[] holderForStartAndLength);
    String getNamespace();
    String getName();
    String getPrefix();
    boolean isEmptyElementTag() throws XmlPullParserException;
    int getAttributeCount();
    String getAttributeNamespace(int index);
    String getAttributeName(int index);
    String getAttributePrefix(int index);
    String getAttributeType(int index);
    boolean isAttributeDefault(int index);
    String getAttributeValue(int index);
    String getAttributeValue(String namespace, String name);
    int getEventType() throws XmlPullParserException;
    int next() throws XmlPullParserException, IOException;
    int nextToken() throws XmlPullParserException, IOException;
    void require(int type, String namespace, String name) throws XmlPullParserException, IOException;
    String nextText() throws XmlPullParserException, IOException;
    int nextTag() throws XmlPullParserException, IOException;
}
