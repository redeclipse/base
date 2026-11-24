// page templates: web pages written in CubeScript, sharing their look with the master's mail templates

#include "engine.h"

scripttemplate::scripttemplate() : label(NULL), id(ID_ALIAS, "", 0, 0), running(0) { id.index = MAXARGS; }
scripttemplate::~scripttemplate() { clear(); }

bool scripttemplate::set(const char *name, const char *src)
{
    if(!clear()) return false;
    if(!src || !*src) return true;
    label = newstring(name);
    id.name = label;
    id.valtype = VAL_STR;
    id.val.s = newstring(src);
    id.code = compilecode(src);
    return true;
}

bool scripttemplate::clear()
{
    if(running) return false;
    if(id.code) { freecode(id.code); id.code = NULL; }
    if(id.valtype == VAL_STR) { DELETEA(id.val.s); }
    id.valtype = VAL_NULL;
    DELETEA(label);
    id.name = "";
    return true;
}

bool scripttemplate::empty() const { return id.valtype != VAL_STR || !id.val.s || !*id.val.s; }

char *scripttemplate::run(const tagval *inargs, int numargs)
{
    if(empty()) return NULL;
    tagval args[MAXARGS];
    int n = clamp(numargs, 0, int(MAXARGS));
    loopi(n)
    {
        if(inargs[i].type == VAL_INT) args[i].setint(inargs[i].i);
        else if(inargs[i].type == VAL_FLOAT) args[i].setfloat(inargs[i].f);
        else args[i].setstr(newstring(inargs[i].getstr()));
    }
    running++;
    char *out = executestr(&id, args, n, false);
    running--;
    return out;
}

static void htmlescape(vector<char> &out, const char *s)
{
    for(const char *p = s; *p; p++) switch(*p)
    {
        case '&': out.put("&amp;", 5); break;
        case '<': out.put("&lt;", 4); break;
        case '>': out.put("&gt;", 4); break;
        case '"': out.put("&quot;", 6); break;
        case '\'': out.put("&#39;", 5); break;
        default: out.add(*p); break;
    }
}

ICOMMAND(IDF_SAFE, htmlescape, "s", (char *s),
{
    vector<char> out;
    htmlescape(out, s);
    out.add('\0');
    result(out.getbuf());
});

// a quoted JSON string that is also safe inside a <script>, as < > & never appear in it
ICOMMAND(IDF_SAFE, jsonescape, "s", (char *s),
{
    static const char hex[] = "0123456789abcdef";
    vector<char> out;
    out.add('"');
    for(const uchar *p = (const uchar *)s; *p; p++) switch(*p)
    {
        case '"': out.put("\\\"", 2); break;
        case '\\': out.put("\\\\", 2); break;
        case '\n': out.put("\\n", 2); break;
        case '\r': out.put("\\r", 2); break;
        case '\t': out.put("\\t", 2); break;
        default:
            if(*p < 0x20 || *p == 0x7F || *p == '<' || *p == '>' || *p == '&') { out.put("\\u00", 4); out.add(hex[*p>>4]); out.add(hex[*p&15]); }
            else out.add(char(*p));
            break;
    }
    out.add('"');
    out.add('\0');
    result(out.getbuf());
});

struct htmlpage
{
    char *type, *builtin; // builtin: the source config/version.cfg gave it, which a rehash goes back to
    scripttemplate page;

    htmlpage(const char *t) : type(newstring(t)), builtin(NULL) {}
    ~htmlpage() { DELETEA(type); DELETEA(builtin); }

    void set(const char *src) { page.set(tempformatstring("htmlpagedef %s", type), src); }
};

static hashtable<const char *, htmlpage *> htmlpages;

static void removehtmlpage(htmlpage *p)
{
    htmlpages.remove(p->type);
    delete p;
}

static void htmlpagedef(char *type, char *src)
{
    if(!type || !*type) return;
    htmlpage **existing = htmlpages.access(type);
    htmlpage *p = existing ? *existing : NULL;
    if(p && p->page.running) { conoutf(colourred, "htmlpagedef: %s can't change while it renders", type); return; }
    if(!src || !*src)
    {
        // an operator emptying a built-in page keeps the built-in for the next rehash
        if(p && p->builtin && versioning != 2) p->page.clear();
        else if(p) removehtmlpage(p);
        return;
    }
    if(!p)
    {
        p = new htmlpage(type);
        htmlpages[p->type] = p;
    }
    p->set(src);
    if(versioning == 2) // config/version.cfg and what it runs, not servinit.cfg's own EXEC_VERSION
    {
        DELETEA(p->builtin);
        p->builtin = newstring(src);
    }
}
COMMAND(0, htmlpagedef, "ss");

void resethtmlpages()
{
    vector<htmlpage *> gone;
    enumerate(htmlpages, htmlpage *, p,
    {
        if(p->page.running) continue; // a rehash from inside a page leaves that page as it is
        if(p->builtin) p->set(p->builtin);
        else gone.add(p);
    });
    loopv(gone) removehtmlpage(gone[i]);
}

void cleanuphtmlpages()
{
    enumerate(htmlpages, htmlpage *, p, delete p);
    htmlpages.clear();
}

static char *runhtmlpage(const char *type, const tagval *args, int numargs)
{
    htmlpage **found = htmlpages.access(type);
    if(!found) return NULL;
    char *out = (*found)->page.run(args, numargs);
    if(out && !*out) DELETEA(out);
    return out;
}

char *renderhtmlpage(const char *type, const tagval *args, int numargs)
{
    char *out = runhtmlpage(type, args, numargs);
    if(!out) return NULL;
    vector<char> buf;
    http::utf8string(buf, out);
    DELETEA(out);
    buf.add('\0');
    return newstring(buf.getbuf());
}

// an httproute answering with just a page answers in HTML, unless it chose its own type
static void htmlpagecmd(tagval *args, int numargs)
{
    if(numargs < 1) { result(""); return; }
    char *out = runhtmlpage(args[0].getstr(), &args[1], numargs - 1);
    if(!out) { result(""); return; }
    http::scriptpage(out);
    stringret(out);
}
COMMANDN(0, htmlpage, htmlpagecmd, "V");
