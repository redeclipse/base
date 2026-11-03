// JSON values, parsing and writing

#include "engine.h"

jsonvalue *jsonvalue::get(const char *name)
{
    if(type != JSON_OBJECT) return NULL;
    loopv(items) if(items[i]->key && !strcmp(items[i]->key, name)) return items[i];
    return NULL;
}

const char *jsonvalue::getstr(const char *name, const char *def)
{
    jsonvalue *v = get(name);
    return v && (v->type == JSON_STRING || v->type == JSON_NUMBER) && v->str ? v->str : def;
}

double jsonvalue::getnum(const char *name, double def)
{
    jsonvalue *v = get(name);
    if(!v) return def;
    if(v->type == JSON_NUMBER) return v->num;
    if(v->type == JSON_TRUE) return 1;
    if(v->type == JSON_FALSE) return 0;
    if(v->type == JSON_STRING && v->str) return atof(v->str);
    return def;
}

bool jsonvalue::getbool(const char *name, bool def)
{
    jsonvalue *v = get(name);
    return v ? v->istrue() : def;
}

namespace json
{
    struct parser
    {
        const char *p, *end, *error;
        int depth;

        parser(const char *s, int len) : p(s), end(s + len), error(NULL), depth(0) {}

        void skip()
        {
            while(p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        }

        bool fail(const char *msg)
        {
            if(!error) error = msg;
            return false;
        }

        static void pututf8(vector<char> &buf, uint c)
        {
            if(c < 0x80) buf.add(char(c));
            else if(c < 0x800) { buf.add(char(0xC0|(c>>6))); buf.add(char(0x80|(c&0x3F))); }
            else if(c < 0x10000) { buf.add(char(0xE0|(c>>12))); buf.add(char(0x80|((c>>6)&0x3F))); buf.add(char(0x80|(c&0x3F))); }
            else { buf.add(char(0xF0|(c>>18))); buf.add(char(0x80|((c>>12)&0x3F))); buf.add(char(0x80|((c>>6)&0x3F))); buf.add(char(0x80|(c&0x3F))); }
        }

        bool hex4(uint &v)
        {
            if(end - p < 4) return false;
            v = 0;
            loopi(4)
            {
                char c = *p++;
                v <<= 4;
                if(c >= '0' && c <= '9') v |= c - '0';
                else if(c >= 'a' && c <= 'f') v |= c - 'a' + 10;
                else if(c >= 'A' && c <= 'F') v |= c - 'A' + 10;
                else return false;
            }
            return true;
        }

        char *string()
        {
            if(p >= end || *p != '"') { fail("expected a string"); return NULL; }
            p++;
            vector<char> buf;
            while(p < end && *p != '"')
            {
                uchar c = *p++;
                if(c < 0x20) { fail("control character in a string"); return NULL; }
                if(c != '\\') { buf.add(char(c)); continue; }
                if(p >= end) break;
                switch(*p++)
                {
                    case '"': buf.add('"'); break;
                    case '\\': buf.add('\\'); break;
                    case '/': buf.add('/'); break;
                    case 'b': buf.add('\b'); break;
                    case 'f': buf.add('\f'); break;
                    case 'n': buf.add('\n'); break;
                    case 'r': buf.add('\r'); break;
                    case 't': buf.add('\t'); break;
                    case 'u':
                    {
                        uint v = 0;
                        if(!hex4(v)) { fail("bad \\u escape"); return NULL; }
                        if(v >= 0xD800 && v < 0xDC00)
                        {
                            uint lo = 0;
                            if(end - p < 6 || p[0] != '\\' || p[1] != 'u') { fail("unpaired surrogate"); return NULL; }
                            p += 2;
                            if(!hex4(lo) || lo < 0xDC00 || lo >= 0xE000) { fail("unpaired surrogate"); return NULL; }
                            v = 0x10000 + ((v - 0xD800)<<10) + (lo - 0xDC00);
                        }
                        else if(v >= 0xDC00 && v < 0xE000) { fail("unpaired surrogate"); return NULL; }
                        pututf8(buf, v);
                        break;
                    }
                    default: fail("bad escape"); return NULL;
                }
            }
            if(p >= end) { fail("unterminated string"); return NULL; }
            p++;
            return buf.length() ? newstring(buf.getbuf(), buf.length()) : newstring("");
        }

        jsonvalue *number()
        {
            const char *start = p;
            if(p < end && *p == '-') p++;
            if(p >= end || !isdigit(*p)) { fail("bad number"); return NULL; }
            if(*p == '0') p++;
            else while(p < end && isdigit(*p)) p++;
            if(p < end && *p == '.')
            {
                p++;
                if(p >= end || !isdigit(*p)) { fail("bad number"); return NULL; }
                while(p < end && isdigit(*p)) p++;
            }
            if(p < end && (*p == 'e' || *p == 'E'))
            {
                p++;
                if(p < end && (*p == '+' || *p == '-')) p++;
                if(p >= end || !isdigit(*p)) { fail("bad number"); return NULL; }
                while(p < end && isdigit(*p)) p++;
            }
            jsonvalue *v = new jsonvalue(JSON_NUMBER);
            v->str = newstring(start, p - start);
            v->num = strtod(v->str, NULL);
            return v;
        }

        bool literal(const char *word)
        {
            int len = strlen(word);
            if(end - p < len || strncmp(p, word, len)) return false;
            p += len;
            return true;
        }

        jsonvalue *value()
        {
            skip();
            if(p >= end) { fail("unexpected end"); return NULL; }
            switch(*p)
            {
                case '{': case '[':
                {
                    bool obj = *p == '{';
                    if(++depth > JSON_MAXDEPTH) { fail("nested too deeply"); return NULL; }
                    p++;
                    jsonvalue *v = new jsonvalue(obj ? JSON_OBJECT : JSON_ARRAY);
                    skip();
                    if(p < end && *p == (obj ? '}' : ']')) { p++; depth--; return v; }
                    for(;;)
                    {
                        char *key = NULL;
                        if(obj)
                        {
                            skip();
                            key = string();
                            if(!key) { delete v; return NULL; }
                            skip();
                            if(p >= end || *p != ':') { delete[] key; delete v; fail("expected ':'"); return NULL; }
                            p++;
                        }
                        jsonvalue *item = value();
                        if(!item) { DELETEA(key); delete v; return NULL; }
                        item->key = key;
                        v->items.add(item);
                        skip();
                        if(p < end && *p == ',') { p++; continue; }
                        if(p < end && *p == (obj ? '}' : ']')) { p++; break; }
                        delete v;
                        fail(obj ? "expected ',' or '}'" : "expected ',' or ']'");
                        return NULL;
                    }
                    depth--;
                    return v;
                }
                case '"':
                {
                    char *s = string();
                    if(!s) return NULL;
                    jsonvalue *v = new jsonvalue(JSON_STRING);
                    v->str = s;
                    return v;
                }
                case 't': if(literal("true")) return new jsonvalue(JSON_TRUE); break;
                case 'f': if(literal("false")) return new jsonvalue(JSON_FALSE); break;
                case 'n': if(literal("null")) return new jsonvalue(JSON_NULL); break;
                default: return number();
            }
            fail("unexpected character");
            return NULL;
        }
    };

    jsonvalue *parse(const char *str, int len, char *err, int errlen)
    {
        if(!str) return NULL;
        if(len < 0) len = strlen(str);
        parser ps(str, len);
        jsonvalue *v = ps.value();
        if(v)
        {
            ps.skip();
            if(ps.p < ps.end) { delete v; v = NULL; ps.fail("trailing characters"); }
        }
        if(!v && err && errlen > 0) nformatstring(err, errlen, "%s at offset %d", ps.error ? ps.error : "parse error", int(ps.p - str));
        return v;
    }

    void writestring(vector<char> &buf, const char *s, int len, bool cube)
    {
        static const char hex[] = "0123456789abcdef";
        if(len < 0) len = s ? strlen(s) : 0;
        buf.add('"');
        loopi(len)
        {
            uchar c = s[i];
            if(cube && c >= 0x80)
            {
                int u = cube2uni(c);
                if(!u) continue;
                parser::pututf8(buf, uint(u));
                continue;
            }
            switch(c)
            {
                case '"': buf.put("\\\"", 2); break;
                case '\\': buf.put("\\\\", 2); break;
                case '\n': buf.put("\\n", 2); break;
                case '\r': buf.put("\\r", 2); break;
                case '\t': buf.put("\\t", 2); break;
                case '\b': buf.put("\\b", 2); break;
                case '\f': buf.put("\\f", 2); break;
                default:
                    if(c < 0x20 || c == 0x7F) { buf.put("\\u00", 4); buf.add(hex[c>>4]); buf.add(hex[c&15]); }
                    else buf.add(char(c));
                    break;
            }
        }
        buf.add('"');
    }

    void write(vector<char> &buf, jsonvalue *v)
    {
        if(!v) { buf.put("null", 4); return; }
        switch(v->type)
        {
            case JSON_FALSE: buf.put("false", 5); break;
            case JSON_TRUE: buf.put("true", 4); break;
            case JSON_NUMBER:
            {
                if(v->str) buf.put(v->str, strlen(v->str));
                else { defformatstring(n, "%.17g", v->num); buf.put(n, strlen(n)); }
                break;
            }
            case JSON_STRING: writestring(buf, v->str); break;
            case JSON_ARRAY: case JSON_OBJECT:
            {
                bool obj = v->type == JSON_OBJECT;
                buf.add(obj ? '{' : '[');
                loopv(v->items)
                {
                    if(i) buf.add(',');
                    if(obj) { writestring(buf, v->items[i]->key ? v->items[i]->key : ""); buf.add(':'); }
                    write(buf, v->items[i]);
                }
                buf.add(obj ? '}' : ']');
                break;
            }
            default: buf.put("null", 4); break;
        }
    }

    jsonvalue *find(jsonvalue *root, const char *path)
    {
        jsonvalue *v = root;
        for(const char *p = path; v && *p;)
        {
            int len = strcspn(p, ".");
            if(len)
            {
                if(v->type == JSON_ARRAY)
                {
                    char *end = NULL;
                    long n = strtol(p, &end, 10);
                    v = end == p + len ? v->at(int(n)) : NULL;
                }
                else if(v->type == JSON_OBJECT)
                {
                    jsonvalue *found = NULL;
                    loopv(v->items) if(v->items[i]->key && int(strlen(v->items[i]->key)) == len && !strncmp(v->items[i]->key, p, len)) { found = v->items[i]; break; }
                    v = found;
                }
                else v = NULL;
            }
            p += len;
            if(*p == '.') p++;
        }
        return v;
    }

    void tocube(vector<char> &buf, const char *s, int len)
    {
        if(len < 0) len = strlen(s);
        uchar *dst = (uchar *)buf.reserve(len + 1).buf;
        size_t n = decodeutf8(dst, len, (const uchar *)s, len);
        buf.advance(int(n));
    }
}

void jsonwriter::key(const char *name)
{
    if(!first.empty())
    {
        if(first.last()) first.last() = false;
        else buf.add(',');
    }
    if(name) { json::writestring(buf, name, -1, true); buf.add(':'); }
}

void jsonwriter::beginobject(const char *name) { key(name); buf.add('{'); first.add(true); }
void jsonwriter::endobject() { if(!first.empty()) first.pop(); buf.add('}'); }
void jsonwriter::beginarray(const char *name) { key(name); buf.add('['); first.add(true); }
void jsonwriter::endarray() { if(!first.empty()) first.pop(); buf.add(']'); }
void jsonwriter::str(const char *name, const char *value, int len) { key(name); json::writestring(buf, value ? value : "", len, true); }
void jsonwriter::utf8(const char *name, const char *value, int len) { key(name); json::writestring(buf, value ? value : "", len, false); }
void jsonwriter::num(const char *name, int value) { key(name); defformatstring(n, "%d", value); buf.put(n, strlen(n)); }
void jsonwriter::num(const char *name, double value)
{
    key(name);
    if(value != value || value > 1e308 || value < -1e308) { buf.put("null", 4); return; }
    defformatstring(n, "%.10g", value);
    buf.put(n, strlen(n));
}
void jsonwriter::boolean(const char *name, bool value) { key(name); if(value) buf.put("true", 4); else buf.put("false", 5); }
void jsonwriter::null(const char *name) { key(name); buf.put("null", 4); }
void jsonwriter::raw(const char *name, const char *json, int len) { key(name); buf.put(json, len >= 0 ? len : strlen(json)); }
void jsonwriter::value(const char *name, jsonvalue *v) { key(name); json::write(buf, v); }

// CubeScript access; these only ever read or build values, nothing they touch is run
// JSON text in scripts is in the engine's encoding like any other script string, HTTP converts it at the edge
static void cuberesult(vector<char> &utf8)
{
    vector<char> buf;
    json::tocube(buf, utf8.getbuf(), utf8.length());
    buf.add('\0');
    result(buf.getbuf());
}

static void jsonresult(jsonvalue *v)
{
    if(!v) { result(""); return; }
    vector<char> buf;
    switch(v->type)
    {
        case JSON_STRING: json::tocube(buf, v->str); break;
        case JSON_NUMBER: buf.put(v->str, strlen(v->str)); break;
        case JSON_TRUE: buf.add('1'); break;
        case JSON_FALSE: buf.add('0'); break;
        case JSON_NULL: break;
        default: { vector<char> utf8; json::write(utf8, v); cuberesult(utf8); return; }
    }
    buf.add('\0');
    result(buf.getbuf());
}

// script strings are in the engine's encoding, JSON wants UTF-8
static jsonvalue *parsecube(const char *text)
{
    int len = strlen(text);
    vector<char> buf;
    uchar *dst = (uchar *)buf.reserve(len*3 + 1).buf;
    size_t n = encodeutf8(dst, len*3, (const uchar *)text, len);
    return json::parse((const char *)dst, int(n));
}

static jsonvalue *jsonlookup(const char *text, const char *path, jsonvalue *&root)
{
    root = parsecube(text);
    return root ? (path && *path ? json::find(root, path) : root) : NULL;
}

static void jsonget(char *text, char *path)
{
    jsonvalue *root = NULL, *v = jsonlookup(text, path, root);
    jsonresult(v);
    DELETEP(root);
}
COMMAND(0, jsonget, "ss");

static void jsontype(char *text, char *path)
{
    static const char * const names[] = { "null", "boolean", "boolean", "number", "string", "array", "object" };
    jsonvalue *root = NULL, *v = jsonlookup(text, path, root);
    result(v ? names[v->type] : "");
    DELETEP(root);
}
COMMAND(0, jsontype, "ss");

static void jsonlength(char *text, char *path)
{
    jsonvalue *root = NULL, *v = jsonlookup(text, path, root);
    intret(v ? (v->type == JSON_STRING ? int(strlen(v->str)) : v->length()) : 0);
    DELETEP(root);
}
COMMAND(0, jsonlength, "ss");

static void jsonkeys(char *text, char *path)
{
    jsonvalue *root = NULL, *v = jsonlookup(text, path, root);
    vector<char> buf;
    if(v && v->type == JSON_OBJECT) loopv(v->items)
    {
        if(i) buf.add(' ');
        vector<char> key;
        json::tocube(key, v->items[i]->key);
        key.add('\0');
        const char *esc = escapestring(key.getbuf());
        buf.put(esc, strlen(esc));
    }
    buf.add('\0');
    result(buf.getbuf());
    DELETEP(root);
}
COMMAND(0, jsonkeys, "ss");

static void jsonstring(char *s)
{
    vector<char> buf;
    json::writestring(buf, s, -1, true);
    cuberesult(buf);
}
COMMAND(0, jsonstring, "s");

// values that are JSON already are kept, anything else becomes a string; wrap a value in jsonstring to be sure
static void jsonitem(jsonwriter &w, const char *name, const char *value)
{
    jsonvalue *v = *value ? parsecube(value) : NULL;
    if(v) { w.value(name, v); delete v; }
    else w.str(name, value);
}

static void jsonobject(char *list)
{
    vector<char *> elems;
    explodelist(list, elems);
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    for(int i = 0; i + 1 < elems.length(); i += 2) jsonitem(w, elems[i], elems[i+1]);
    w.endobject();
    cuberesult(buf);
    elems.deletearrays();
}
COMMAND(0, jsonobject, "s");

static void jsonarray(char *list)
{
    vector<char *> elems;
    explodelist(list, elems);
    vector<char> buf;
    jsonwriter w(buf);
    w.beginarray();
    loopv(elems) jsonitem(w, NULL, elems[i]);
    w.endarray();
    cuberesult(buf);
    elems.deletearrays();
}
COMMAND(0, jsonarray, "s");
