// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat.res;

import android.content.res.XmlResourceParser;

import org.xmlpull.v1.XmlPullParserException;

import java.io.InputStream;
import java.io.Reader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * XmlResourceParser over Android binary XML (RES_XML_TYPE, as aapt2 compiles layouts and drawables).
 * Attribute values come back as Android's parser reports them: the raw string when one was kept,
 * otherwise the typed value coerced to text ("@<id>" for references, "16.0dip", "#ff2f8f5b"...).
 */
public final class BinaryXmlParser implements XmlResourceParser {
    private static final int RES_XML_TYPE = 0x0003;
    private static final int RES_XML_RESOURCE_MAP_TYPE = 0x0180;
    private static final int START_NAMESPACE = 0x0100, END_NAMESPACE = 0x0101, START_ELEMENT = 0x0102,
            END_ELEMENT = 0x0103, CDATA = 0x0104;
    private static final int NO_INDEX = 0xffffffff;
    private static final int MAX_DEPTH = 256;

    private static final class Attr {
        String ns, name, raw;
        int nameRes, type, data;
        String string;
    }

    private static final class Event {
        int type;
        String ns, name, text;
        int line, depth;
        List<Attr> attrs = new ArrayList<>();
        int idIndex = -1, classIndex = -1, styleIndex = -1;
        List<String[]> namespaces;  // prefix/uri pairs in scope
    }

    private final List<Event> events = new ArrayList<>();
    private final ResourceTable table;
    private int pos = 0;

    public BinaryXmlParser(byte[] xml, ResourceTable table) {
        this.table = table;
        ByteBuffer b = ByteBuffer.wrap(xml).order(ByteOrder.LITTLE_ENDIAN);
        if (xml.length < 8 || (b.getShort(0) & 0xffff) != RES_XML_TYPE) throw new IllegalArgumentException("not binary XML");
        int size = Math.min(b.getInt(4), xml.length);
        StringPool strings = null;
        int[] resMap = new int[0];
        List<String[]> ns = new ArrayList<>();
        int depth = 0;
        Event start = new Event();
        start.type = START_DOCUMENT;
        start.namespaces = new ArrayList<>(ns);
        events.add(start);
        for (int p = b.getShort(2) & 0xffff; p + 8 <= size; ) {
            int type = b.getShort(p) & 0xffff;
            int headerSize = b.getShort(p + 2) & 0xffff;
            int chunk = b.getInt(p + 4);
            if (chunk < 8 || p + chunk > size) throw new IllegalArgumentException("bad XML chunk at " + p);
            int ext = p + headerSize;
            if (type == StringPool.TYPE) {
                strings = new StringPool(b, p);
            } else if (type == RES_XML_RESOURCE_MAP_TYPE) {
                resMap = new int[(chunk - headerSize) / 4];
                for (int i = 0; i < resMap.length; i++) resMap[i] = b.getInt(ext + 4 * i);
            } else if (strings != null && type >= START_NAMESPACE && type <= CDATA) {
                int line = b.getInt(p + 8);
                if (type == START_NAMESPACE) {
                    ns.add(new String[]{str(strings, b.getInt(ext)), str(strings, b.getInt(ext + 4))});
                } else if (type == END_NAMESPACE) {
                    if (!ns.isEmpty()) ns.remove(ns.size() - 1);
                } else if (type == START_ELEMENT) {
                    if (++depth > MAX_DEPTH) throw new IllegalArgumentException("XML nested too deeply");
                    Event e = new Event();
                    e.type = START_TAG;
                    e.line = line;
                    e.depth = depth;
                    e.ns = str(strings, b.getInt(ext));
                    e.name = str(strings, b.getInt(ext + 4));
                    int attrStart = b.getShort(ext + 8) & 0xffff;
                    int attrSize = b.getShort(ext + 10) & 0xffff;
                    int count = b.getShort(ext + 12) & 0xffff;
                    e.idIndex = (b.getShort(ext + 14) & 0xffff) - 1;
                    e.classIndex = (b.getShort(ext + 16) & 0xffff) - 1;
                    e.styleIndex = (b.getShort(ext + 18) & 0xffff) - 1;
                    for (int i = 0; i < count; i++) {
                        int a = ext + attrStart + i * attrSize;
                        Attr at = new Attr();
                        at.ns = str(strings, b.getInt(a));
                        int nameIdx = b.getInt(a + 4);
                        at.name = str(strings, nameIdx);
                        at.nameRes = nameIdx >= 0 && nameIdx < resMap.length ? resMap[nameIdx] : 0;
                        at.raw = str(strings, b.getInt(a + 8));
                        at.type = b.get(a + 15) & 0xff;
                        at.data = b.getInt(a + 16);
                        at.string = at.type == TypedValues.TYPE_STRING ? strings.get(at.data) : null;
                        e.attrs.add(at);
                    }
                    e.namespaces = new ArrayList<>(ns);
                    events.add(e);
                } else if (type == END_ELEMENT) {
                    Event e = new Event();
                    e.type = END_TAG;
                    e.line = line;
                    e.depth = depth--;
                    e.ns = str(strings, b.getInt(ext));
                    e.name = str(strings, b.getInt(ext + 4));
                    e.namespaces = new ArrayList<>(ns);
                    events.add(e);
                } else {
                    Event e = new Event();
                    e.type = TEXT;
                    e.line = line;
                    e.depth = depth;
                    e.text = str(strings, b.getInt(ext));
                    e.namespaces = new ArrayList<>(ns);
                    events.add(e);
                }
            }
            p += chunk;
        }
        Event end = new Event();
        end.type = END_DOCUMENT;
        end.namespaces = new ArrayList<>();
        events.add(end);
    }

    private static String str(StringPool s, int index) {
        return index == NO_INDEX ? null : s.get(index);
    }

    private Event cur() {
        return events.get(pos);
    }

    private Attr attr(int index) {
        Event e = cur();
        if (e.type != START_TAG || index < 0 || index >= e.attrs.size()) throw new IndexOutOfBoundsException("attribute " + index);
        return e.attrs.get(index);
    }

    private int find(String namespace, String name) {
        Event e = cur();
        if (e.type != START_TAG) return -1;
        for (int i = 0; i < e.attrs.size(); i++) {
            Attr a = e.attrs.get(i);
            if (a.name != null && a.name.equals(name) && (namespace == null || namespace.equals(a.ns == null ? "" : a.ns))) return i;
        }
        return -1;
    }

    // ---- XmlPullParser ---------------------------------------------------------------------------

    @Override public void setFeature(String name, boolean state) throws XmlPullParserException {
        if (FEATURE_PROCESS_NAMESPACES.equals(name) && state) return;
        throw new XmlPullParserException("unsupported feature " + name);
    }
    @Override public boolean getFeature(String name) { return FEATURE_PROCESS_NAMESPACES.equals(name); }
    @Override public void setProperty(String name, Object value) throws XmlPullParserException { throw new XmlPullParserException("setProperty() not supported"); }
    @Override public Object getProperty(String name) { return null; }
    @Override public void setInput(Reader in) throws XmlPullParserException { throw new XmlPullParserException("setInput() not supported"); }
    @Override public void setInput(InputStream in, String enc) throws XmlPullParserException { throw new XmlPullParserException("setInput() not supported"); }
    @Override public String getInputEncoding() { return "utf-8"; }
    @Override public void defineEntityReplacementText(String n, String t) throws XmlPullParserException { throw new XmlPullParserException("defineEntityReplacementText() not supported"); }
    @Override public int getNamespaceCount(int depth) { return cur().namespaces.size(); }
    @Override public String getNamespacePrefix(int pos) { return cur().namespaces.get(pos)[0]; }
    @Override public String getNamespaceUri(int pos) { return cur().namespaces.get(pos)[1]; }
    @Override public String getNamespace(String prefix) {
        List<String[]> ns = cur().namespaces;
        for (int i = ns.size() - 1; i >= 0; i--) if (ns.get(i)[0] != null && ns.get(i)[0].equals(prefix)) return ns.get(i)[1];
        return null;
    }
    @Override public int getDepth() { return cur().depth; }
    @Override public String getPositionDescription() { return "Binary XML file line #" + getLineNumber(); }
    @Override public int getLineNumber() { return cur().line; }
    @Override public int getColumnNumber() { return -1; }
    @Override public boolean isWhitespace() { return cur().type == TEXT && cur().text != null && cur().text.trim().isEmpty(); }
    @Override public String getText() { return cur().type == TEXT ? cur().text : null; }
    @Override public char[] getTextCharacters(int[] holder) {
        String t = getText();
        if (t == null) return null;
        holder[0] = 0;
        holder[1] = t.length();
        return t.toCharArray();
    }
    @Override public String getNamespace() { String n = cur().ns; return n == null ? "" : n; }
    @Override public String getName() { int t = cur().type; return t == START_TAG || t == END_TAG ? cur().name : null; }
    @Override public String getPrefix() { return null; }
    @Override public boolean isEmptyElementTag() { return false; }
    @Override public int getAttributeCount() { return cur().type == START_TAG ? cur().attrs.size() : -1; }
    @Override public String getAttributeNamespace(int index) { String n = attr(index).ns; return n == null ? "" : n; }
    @Override public String getAttributeName(int index) { return attr(index).name; }
    @Override public String getAttributePrefix(int index) { return null; }
    @Override public String getAttributeType(int index) { return "CDATA"; }
    @Override public boolean isAttributeDefault(int index) { return false; }
    @Override public String getAttributeValue(int index) {
        Attr a = attr(index);
        return a.raw != null ? a.raw : TypedValues.coerceToString(a.type, a.data, a.string);
    }
    @Override public String getAttributeValue(String namespace, String name) {
        int i = find(namespace, name);
        return i < 0 ? null : getAttributeValue(i);
    }
    @Override public int getEventType() { return cur().type; }
    @Override public int next() {
        if (pos < events.size() - 1) pos++;
        return cur().type;
    }
    @Override public int nextToken() { return next(); }
    @Override public void require(int type, String namespace, String name) throws XmlPullParserException {
        if (type != getEventType() || (namespace != null && !namespace.equals(getNamespace())) || (name != null && !name.equals(getName()))) {
            throw new XmlPullParserException("expected " + TYPES[type] + " " + name + " " + getPositionDescription());
        }
    }
    @Override public String nextText() throws XmlPullParserException {
        if (getEventType() != START_TAG) throw new XmlPullParserException("precondition: START_TAG " + getPositionDescription());
        int t = next();
        if (t == TEXT) {
            String text = getText();
            if (next() != END_TAG) throw new XmlPullParserException("END_TAG expected " + getPositionDescription());
            return text;
        }
        if (t == END_TAG) return "";
        throw new XmlPullParserException("parser must be on START_TAG or TEXT to read text " + getPositionDescription());
    }
    @Override public int nextTag() throws XmlPullParserException {
        int t = next();
        if (t == TEXT && isWhitespace()) t = next();
        if (t != START_TAG && t != END_TAG) throw new XmlPullParserException("expected start or end tag " + getPositionDescription());
        return t;
    }

    private static final String[] TYPES = {"START_DOCUMENT", "END_DOCUMENT", "START_TAG", "END_TAG", "TEXT"};

    // ---- AttributeSet ----------------------------------------------------------------------------

    @Override public int getAttributeNameResource(int index) { return attr(index).nameRes; }

    private ResourceTable.Value typed(int index) {
        Attr a = attr(index);
        ResourceTable.Value v = new ResourceTable.Value(a.type, a.data, a.string);
        return table != null ? table.resolve(v) : v;
    }

    /** The attribute's typed value with resource references followed (TypedArray.peekValue-like). */
    public ResourceTable.Value getAttributeTypedValue(int index) { return typed(index); }

    /** The plugin resource table attribute references resolve against; null for system XML. */
    public ResourceTable resourceTable() { return table; }

    @Override public int getAttributeListValue(String ns, String attribute, String[] options, int def) {
        int i = find(ns, attribute);
        return i < 0 ? def : getAttributeListValue(i, options, def);
    }
    @Override public boolean getAttributeBooleanValue(String ns, String attribute, boolean def) {
        int i = find(ns, attribute);
        return i < 0 ? def : getAttributeBooleanValue(i, def);
    }
    @Override public int getAttributeResourceValue(String ns, String attribute, int def) {
        int i = find(ns, attribute);
        return i < 0 ? def : getAttributeResourceValue(i, def);
    }
    @Override public int getAttributeIntValue(String ns, String attribute, int def) {
        int i = find(ns, attribute);
        return i < 0 ? def : getAttributeIntValue(i, def);
    }
    @Override public int getAttributeUnsignedIntValue(String ns, String attribute, int def) {
        int i = find(ns, attribute);
        return i < 0 ? def : getAttributeUnsignedIntValue(i, def);
    }
    @Override public float getAttributeFloatValue(String ns, String attribute, float def) {
        int i = find(ns, attribute);
        return i < 0 ? def : getAttributeFloatValue(i, def);
    }
    @Override public int getAttributeListValue(int index, String[] options, int def) {
        String v = getAttributeValue(index);
        if (v != null && options != null) for (int i = 0; i < options.length; i++) if (options[i].equals(v)) return i;
        return def;
    }
    @Override public boolean getAttributeBooleanValue(int index, boolean def) {
        ResourceTable.Value v = typed(index);
        if (v.type >= TypedValues.TYPE_INT_DEC && v.type <= TypedValues.TYPE_INT_BOOLEAN) return v.data != 0;
        return def;
    }
    @Override public int getAttributeResourceValue(int index, int def) {
        Attr a = attr(index);
        return a.type == TypedValues.TYPE_REFERENCE ? a.data : def;
    }
    @Override public int getAttributeIntValue(int index, int def) {
        ResourceTable.Value v = typed(index);
        return TypedValues.isInt(v.type) ? v.data : def;
    }
    @Override public int getAttributeUnsignedIntValue(int index, int def) { return getAttributeIntValue(index, def); }
    @Override public float getAttributeFloatValue(int index, float def) {
        ResourceTable.Value v = typed(index);
        return v.type == TypedValues.TYPE_FLOAT ? Float.intBitsToFloat(v.data) : def;
    }
    @Override public String getIdAttribute() { int i = cur().idIndex; return i < 0 ? null : getAttributeValue(i); }
    @Override public String getClassAttribute() { int i = cur().classIndex; return i < 0 ? null : getAttributeValue(i); }
    @Override public int getIdAttributeResourceValue(int def) { int i = cur().idIndex; return i < 0 ? def : getAttributeResourceValue(i, def); }
    @Override public int getStyleAttribute() { int i = cur().styleIndex; return i < 0 ? 0 : attr(i).data; }

    @Override public void close() {}
}
