// the player docs: Markdown pages from the docs package directory, parsed with md4c and laid out for the uidocument UI element
#include "engine.h"
#include "md4c/md4c.h"
extern "C" {
#include "md4c/entity.h"
}

extern float textspacescale;

namespace docs
{
    #define DOCS_ROOT "docs/"
    #define DOCS_ROOTLEN 5
    #define DOCS_VARDIR "docs/variables/" // the variables pages are made in game, not read from the docs
    #define DOCS_VARDIRLEN 15
    #define DOCS_VARIABLES DOCS_VARDIR "Variables.md"
    #define DOCS_WEBSITE "https://www.redeclipse.net/docs/"
    #define DOCS_PXSCALE (1/16.0f) // a line of text is this many website pixels high
    #define DOCS_ICONSCALE 0.8f
    #define DOCS_BULLETSCALE 0.4f
    #define DOCS_SUPSCALE 0.7f
    #define DOCS_CODEALPHA 0x60
    #define DOCS_HEADALPHA 0x28
    #define DOCS_ROWALPHA 0x10
    #define DOCS_MAXDEPTH 16

    SVAR(IDF_PERSIST, docfont, "exo/clear");
    SVAR(IDF_PERSIST, docfontbold, "exo/clear/bold");
    SVAR(IDF_PERSIST, docfontitalic, "exo/clear/italic");
    SVAR(IDF_PERSIST, docfontbolditalic, "exo/clear/bold/italic");
    SVAR(IDF_PERSIST, docfontmono, "mono/clear");
    SVAR(IDF_PERSIST, docfontmonobold, "mono/clear/bold");
    SVAR(IDF_PERSIST, docbullet, "textures/icons/dot");

    VAR(IDF_PERSIST|IDF_HEX, doctextcolour, 0, 0xE0E0E0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, docheadingcolour, 0, 0xFFFFFF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doclinkcolour, 0, 0x60C0FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doclinkhovercolour, 0, 0xFFC040, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doccodecolour, 0, 0xFFD890, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doccodebgcolour, 0, 0x000000, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, docrulecolour, 0, 0x707070, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, docquotecolour, 0, 0x808080, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doctablecolour, 0, 0x606060, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, docnotecolour, 0, 0x4090FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doctipcolour, 0, 0x40C060, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, docimportantcolour, 0, 0xA060FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, docwarningcolour, 0, 0xE0A020, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, doccautioncolour, 0, 0xF04040, 0xFFFFFF);

    FVAR(IDF_PERSIST, doclinespacing, 1, 1.3f, 4);
    FVAR(IDF_PERSIST, docparaspace, 0, 0.6f, 4);
    FVAR(IDF_PERSIST, docindent, 0, 1.5f, 8);
    FVAR(IDF_PERSIST, docimagescale, FVAR_NONZERO, 1, 8);

    static const float headingscales[6] = { 1.8f, 1.5f, 1.3f, 1.15f, 1.0f, 1.0f };

    // a code point drawn as a small inline image, or replaced by text, where the engine's charset has no glyph
    struct symbol
    {
        int code;
        char *tex, *text;
    };
    static vector<symbol> symbols;

    static symbol *findsymbol(int code)
    {
        loopv(symbols) if(symbols[i].code == code) return &symbols[i];
        return NULL;
    }

    static void setsymbol(int code, const char *tex, const char *text)
    {
        symbol *s = findsymbol(code);
        if(!s)
        {
            s = &symbols.add();
            s->code = code;
            s->tex = s->text = NULL;
        }
        DELETEA(s->tex);
        DELETEA(s->text);
        if(tex) s->tex = newstring(tex);
        if(text) s->text = newstring(text);
    }

    ICOMMAND(0, docsymbol, "is", (int *code, char *tex), setsymbol(*code, tex, NULL));
    ICOMMAND(0, docreplace, "is", (int *code, char *text), setsymbol(*code, NULL, text));

    enum { I_TEXT = 0, I_ICON, I_IMAGE, I_BREAK };
    enum { S_BOLD = 1<<0, S_ITALIC = 1<<1, S_CODE = 1<<2, S_STRIKE = 1<<3, S_SUP = 1<<4, S_SUB = 1<<5, S_NUM = 6 };
    enum { B_PARA = 0, B_HEADING, B_CODE, B_RULE, B_TABLE, B_TILES };
    enum { A_NONE = 0, A_NOTE, A_TIP, A_IMPORTANT, A_WARNING, A_CAUTION };
    enum { L_LEFT = 0, L_CENTER, L_RIGHT };

    struct item
    {
        uchar type, style;
        bool space; // a line may break before it
        int link, text; // text is an offset into the page's chars: the words, an icon's texture or an image's path
        float w, h; // an image's size in website pixels (negative is a percentage of the width), an icon's scale
    };

    struct cell
    {
        int start, end, align;
        bool header;
    };

    struct block
    {
        uchar type, level, depth, quote, alert;
        bool tight;
        int markstart, markend, start, end, cols;
        vector<cell> cells;
    };

    struct anchor
    {
        int name, block;
    };

    struct page
    {
        char *path;
        vector<char> chars;
        vector<item> items;
        vector<block> blocks;
        vector<int> links;
        vector<anchor> anchors;

        page() : path(NULL) {}
        ~page() { DELETEA(path); }

        int addstr(const char *s, int len = -1)
        {
            if(len < 0) len = strlen(s);
            int offset = chars.length();
            chars.put(s, len);
            chars.add('\0');
            return offset;
        }

        const char *str(int offset) const { return offset >= 0 && chars.inrange(offset) ? &chars[offset] : ""; }
    };

    static int pagegen = 1;
    struct cachedpage { char *path; page *p; };
    static vector<cachedpage> pages;
    static vector<char *> missing;

    // paths are kept inside the docs root: no absolute paths, no climbing out with ..
    static bool normalizepath(const char *base, const char *rel, string &out)
    {
        string buf;
        if(rel[0] == '/') copystring(buf, rel+1);
        else
        {
            copystring(buf, base);
            char *slash = strrchr(buf, '/');
            if(slash) slash[1] = '\0';
            else buf[0] = '\0';
            concatstring(buf, rel);
        }
        vector<char *> parts;
        char *p = buf;
        while(*p)
        {
            char *end = strchr(p, '/');
            if(end) *end = '\0';
            if(!*p || !strcmp(p, ".")) {}
            else if(!strcmp(p, ".."))
            {
                if(parts.empty()) return false;
                parts.pop();
            }
            else parts.add(p);
            if(!end) break;
            p = end+1;
        }
        out[0] = '\0';
        loopv(parts)
        {
            if(i) concatstring(out, "/");
            concatstring(out, parts[i]);
        }
        return out[0] != '\0';
    }

    static bool validpath(const char *path)
    {
        if(strncmp(path, DOCS_ROOT, DOCS_ROOTLEN)) return false;
        string norm;
        if(!normalizepath("", path, norm) || strcmp(norm, path)) return false;
        return !strchr(path, '\\') && !strchr(path, ':');
    }

    enum { VARPAGE_NONE = -3, VARPAGE_INDEX = -2, VARPAGE_OTHER = -1 };
    static int varpage(const char *path);

    static bool pageexists(const char *path)
    {
        if(!validpath(path)) return false;
        if(varpage(path) != VARPAGE_NONE) return true;
        return findfile(path, "r") && fileexists(findfile(path, "r"), "r");
    }

    // UTF-8 to code points
    static int nextcode(const uchar *&s, const uchar *end)
    {
        int c = *s++;
        if(c < 0x80) return c;
        int n = c >= 0xF0 ? 3 : (c >= 0xE0 ? 2 : (c >= 0xC0 ? 1 : 0));
        if(!n) return 0xFFFD;
        c &= 0x3F >> n;
        loopi(n)
        {
            if(s >= end || (*s & 0xC0) != 0x80) return 0xFFFD;
            c = (c << 6) | (*s++ & 0x3F);
        }
        return c;
    }

    static void putcode(vector<char> &buf, int c)
    {
        if(c < 0x80) buf.add(char(c));
        else if(c < 0x800) { buf.add(char(0xC0 | (c >> 6))); buf.add(char(0x80 | (c & 0x3F))); }
        else if(c < 0x10000) { buf.add(char(0xE0 | (c >> 12))); buf.add(char(0x80 | ((c >> 6) & 0x3F))); buf.add(char(0x80 | (c & 0x3F))); }
        else { buf.add(char(0xF0 | (c >> 18))); buf.add(char(0x80 | ((c >> 12) & 0x3F))); buf.add(char(0x80 | ((c >> 6) & 0x3F))); buf.add(char(0x80 | (c & 0x3F))); }
    }

    // decodes an entity such as &amp; or &#8212; into UTF-8, false if it isn't one
    static bool decodeentity(const char *s, int len, vector<char> &out)
    {
        if(len < 3 || s[0] != '&' || s[len-1] != ';') return false;
        if(s[1] == '#')
        {
            int c = s[2] == 'x' || s[2] == 'X' ? strtol(&s[3], NULL, 16) : strtol(&s[2], NULL, 10);
            if(c <= 0 || c > 0x10FFFF) c = 0xFFFD;
            putcode(out, c);
            return true;
        }
        const ENTITY *e = entity_lookup(s, len);
        if(!e) return false;
        loopi(2) if(e->codepoints[i]) putcode(out, e->codepoints[i]);
        return true;
    }

    // GitHub's heading anchors: lowercase, spaces to hyphens, punctuation and symbols dropped
    static void makeslug(const char *s, int len, vector<char> &out)
    {
        const uchar *p = (const uchar *)s, *end = p + len;
        while(p < end)
        {
            int c = nextcode(p, end);
            if(c < 0x80)
            {
                if(isalnum(c) || c == '_' || c == '-') out.add(char(tolower(c)));
                else if(c == ' ') out.add('-');
            }
            else if(c >= 0x2000 && c < 0x2C00) continue; // punctuation, arrows, symbols and dingbats
            else if(c == 0xA0) out.add('-');
            else if(c >= 0xC0) putcode(out, c);
        }
    }

    struct liststate
    {
        bool ordered, tight;
        int next;
    };

    // turns md4c's callbacks, and the raw HTML they pass through, into a page
    struct builder
    {
        page *p;
        int stylecount[S_NUM], quote, alert, curblock, table, tiles, tilecell, rowstart, htmlskip;
        bool space, inheading, incode, incell, inhtml, intile;
        vector<liststate> lists;
        vector<int> linkstack, marker;
        vector<char> headraw, htmlbuf, word;
        int htmllinks, htmllists;

        builder(page *p_) : p(p_), quote(0), alert(A_NONE), curblock(-1), table(-1), tiles(-1), tilecell(-1), rowstart(0), htmlskip(0),
            space(false), inheading(false), incode(false), incell(false), inhtml(false), intile(false), htmllinks(0), htmllists(0)
        {
            loopi(S_NUM) stylecount[i] = 0;
        }

        int style() const
        {
            int s = 0;
            loopi(S_NUM) if(stylecount[i] > 0) s |= 1<<i;
            return s;
        }

        void setstyle(int s, bool on)
        {
            loopi(S_NUM) if(s&(1<<i)) stylecount[i] = max(stylecount[i] + (on ? 1 : -1), 0);
        }

        int link() const { return linkstack.empty() ? -1 : linkstack[linkstack.length()-1]; }

        bool infloat() const { return incell || intile; }

        void openblock(int type, int level = 0)
        {
            closeblock();
            startblock(type, level);
        }

        void startblock(int type, int level = 0)
        {
            block &b = p->blocks.add();
            b.type = type;
            b.level = level;
            b.depth = min(lists.length() + htmllists, DOCS_MAXDEPTH);
            b.quote = min(quote, DOCS_MAXDEPTH);
            b.alert = alert;
            b.tight = !lists.empty() && lists.last().tight;
            b.cols = 0;
            b.markstart = p->items.length();
            loopv(marker)
            {
                item copy = p->items[marker[i]];
                p->items.add(copy);
            }
            marker.setsize(0);
            b.markend = b.start = b.end = p->items.length();
            curblock = p->blocks.length()-1;
            if(word.empty()) space = false;
        }

        void closeblock()
        {
            if(curblock < 0 && !word.empty() && !infloat() && table < 0) startblock(B_PARA); // a word left over gets a paragraph of its own
            if(curblock < 0) return;
            flushword();
            block &b = p->blocks[curblock];
            b.end = p->items.length();
            if(b.type == B_PARA && b.end == b.start && b.markend == b.markstart) p->blocks.pop();
            curblock = -1;
        }

        // the items for a list marker go to the end of the items, out of the flow, and are copied in front of the next block
        void setmarker(bool ordered, int num, bool task, bool checked)
        {
            marker.setsize(0);
            int start = p->items.length();
            if(task)
            {
                item &t = p->items.add();
                t.type = I_TEXT;
                t.style = S_CODE;
                t.space = false;
                t.link = -1;
                t.text = p->addstr(checked ? "[x]" : "[ ]");
                t.w = t.h = 0;
            }
            else if(ordered)
            {
                item &t = p->items.add();
                t.type = I_TEXT;
                t.style = 0;
                t.space = false;
                t.link = -1;
                defformatstring(num_s, "%d.", num);
                t.text = p->addstr(num_s);
                t.w = t.h = 0;
            }
            else
            {
                item &t = p->items.add();
                t.type = I_ICON;
                t.style = 0;
                t.space = false;
                t.link = -1;
                t.text = p->addstr(docbullet);
                t.w = DOCS_BULLETSCALE;
                t.h = 0;
            }
            for(int i = start; i < p->items.length(); i++) marker.add(i);
        }

        void ensureflow()
        {
            if(curblock < 0 && !infloat() && table < 0) startblock(B_PARA);
        }

        item &additem(int type)
        {
            item &t = p->items.add();
            t.type = type;
            t.style = style();
            t.space = space;
            t.link = link();
            t.text = -1;
            t.w = t.h = 0;
            space = false;
            return t;
        }

        void flushword()
        {
            if(word.empty()) return;
            ensureflow();
            item &t = additem(I_TEXT);
            t.text = p->addstr(word.getbuf(), word.length());
            word.setsize(0);
        }

        void addbreak()
        {
            flushword();
            ensureflow();
            additem(I_BREAK);
        }

        void addicon(const char *tex, float scale = DOCS_ICONSCALE)
        {
            flushword();
            ensureflow();
            item &t = additem(I_ICON);
            t.text = p->addstr(tex);
            t.w = scale;
        }

        void addimage(const char *src, float w, float h)
        {
            flushword();
            ensureflow();
            item &t = additem(I_IMAGE);
            string path;
            if(strstr(src, "://") || !normalizepath(p->path, src, path)) path[0] = '\0';
            t.text = p->addstr(path);
            t.w = w;
            t.h = h;
        }

        // UTF-8 text into words in the engine's charset
        void addtext(const char *s, int len, bool literal = false)
        {
            if(inheading) headraw.put(s, len);
            const uchar *u = (const uchar *)s, *end = u + len;
            while(u < end)
            {
                int c = nextcode(u, end);
                if(literal)
                {
                    if(c == '\n') { addbreak(); continue; }
                    if(c == '\r') continue;
                    if(c == '\t') { loopi(4) word.add(' '); continue; }
                    if(c == ' ') { word.add(' '); continue; }
                }
                else if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
                {
                    flushword();
                    space = true;
                    continue;
                }
                if(c == 0xA0) { word.add(' '); continue; }
                symbol *sym = c >= 0x80 ? findsymbol(c) : NULL;
                if(sym && sym->tex)
                {
                    addicon(sym->tex);
                    continue;
                }
                if(sym && sym->text)
                {
                    for(const char *r = sym->text; *r; r++) word.add(*r);
                    continue;
                }
                if(c < 0x20 || c == 0x7F) continue;
                uchar cc = uni2cube(c);
                if(!cc || cc == '\f' || iscubespace(cc)) cc = '?';
                word.add(char(cc));
            }
        }

        void pushlink(const char *href, int len)
        {
            flushword();
            p->links.add(p->addstr(href, len));
            linkstack.add(p->links.length()-1);
        }

        void poplink()
        {
            flushword();
            if(linkstack.length()) linkstack.pop();
        }

        void addanchor(const char *name, int len)
        {
            vector<char> slug;
            loopi(len) slug.add(char(tolower(name[i])));
            anchor &a = p->anchors.add();
            a.name = p->addstr(slug.getbuf(), slug.length());
            a.block = curblock >= 0 ? curblock : p->blocks.length();
        }

        bool hasanchor(const char *name)
        {
            loopv(p->anchors) if(!strcmp(p->str(p->anchors[i].name), name)) return true;
            return false;
        }

        void headinganchor()
        {
            vector<char> slug;
            makeslug(headraw.getbuf(), headraw.length(), slug);
            slug.add('\0');
            // repeats get -1, -2 and so on
            string name;
            copystring(name, slug.getbuf());
            for(int n = 1; hasanchor(name); n++) formatstring(name, "%s-%d", slug.getbuf(), n);
            anchor &a = p->anchors.add();
            a.name = p->addstr(name);
            a.block = curblock;
            headraw.setsize(0);
        }

        // HTML: a small tag scanner for the subset the docs use, the rest is ignored with its text kept
        static bool getattr(const char *tag, int len, const char *name, string &out)
        {
            int nlen = strlen(name);
            const char *end = tag + len;
            for(const char *s = tag; s < end; s++)
            {
                if(s > tag && !isspace(uchar(s[-1]))) continue;
                if(end - s <= nlen || strncasecmp(s, name, nlen)) continue;
                const char *v = s + nlen;
                while(v < end && isspace(uchar(*v))) v++;
                if(v >= end || *v != '=') continue;
                v++;
                while(v < end && isspace(uchar(*v))) v++;
                const char *vend;
                if(v < end && (*v == '"' || *v == '\''))
                {
                    char q = *v++;
                    vend = (const char *)memchr(v, q, end - v);
                    if(!vend) vend = end;
                }
                else
                {
                    vend = v;
                    while(vend < end && !isspace(uchar(*vend)) && *vend != '>' && *vend != '/') vend++;
                }
                copystring(out, v, min(int(vend - v) + 1, int(sizeof(string))));
                return true;
            }
            out[0] = '\0';
            return false;
        }

        static float getsize(const char *tag, int len, const char *name)
        {
            string val;
            if(!getattr(tag, len, name, val) || !val[0]) return 0;
            float f = atof(val);
            if(strchr(val, '%')) return -clamp(f, 0.f, 100.f);
            return max(f, 0.f);
        }

        void htmlblockbreak()
        {
            if(intile)
            {
                flushword();
                if(tilecell >= 0 && p->items.length() > p->blocks[tiles].cells[tilecell].start && p->items.last().type != I_BREAK) additem(I_BREAK);
            }
            else if(!incell) closeblock();
        }

        void starttiles()
        {
            closeblock();
            block &b = p->blocks.add();
            b.type = B_TILES;
            b.level = 0;
            b.depth = min(lists.length() + htmllists, DOCS_MAXDEPTH);
            b.quote = min(quote, DOCS_MAXDEPTH);
            b.alert = alert;
            b.tight = false;
            b.cols = 0;
            b.markstart = b.markend = b.start = b.end = p->items.length();
            tiles = p->blocks.length()-1;
        }

        void htmltag(const char *tag, int len)
        {
            // tag points past '<', len excludes '>'
            bool closing = len > 0 && tag[0] == '/';
            if(closing) { tag++; len--; }
            if(len > 0 && tag[len-1] == '/') len--;
            int nlen = 0;
            while(nlen < len && (isalnum(uchar(tag[nlen])))) nlen++;
            string name;
            copystring(name, tag, min(nlen + 1, int(sizeof(name))));
            loopi(nlen) name[i] = tolower(name[i]);
            const char *attrs = tag + nlen;
            int alen = len - nlen;

            #define ISTAG(n) (!strcmp(name, n))
            if(ISTAG("script") || ISTAG("style") || ISTAG("textarea") || ISTAG("title"))
            {
                htmlskip = max(htmlskip + (closing ? -1 : 1), 0);
                return;
            }
            if(ISTAG("br")) { if(!closing) addbreak(); }
            else if(ISTAG("img"))
            {
                if(closing) return;
                string src;
                if(getattr(attrs, alen, "src", src) && src[0]) addimage(src, getsize(attrs, alen, "width"), getsize(attrs, alen, "height"));
            }
            else if(ISTAG("a"))
            {
                if(closing)
                {
                    if(htmllinks > 0) { htmllinks--; poplink(); }
                    return;
                }
                string val;
                if(getattr(attrs, alen, "name", val) && val[0]) addanchor(val, strlen(val));
                if(getattr(attrs, alen, "id", val) && val[0]) addanchor(val, strlen(val));
                if(getattr(attrs, alen, "href", val) && val[0])
                {
                    vector<char> href;
                    decodetext(val, strlen(val), href);
                    pushlink(href.getbuf(), href.length());
                    htmllinks++;
                }
            }
            else if(ISTAG("b") || ISTAG("strong")) { flushword(); setstyle(S_BOLD, !closing); }
            else if(ISTAG("i") || ISTAG("em") || ISTAG("cite") || ISTAG("dfn") || ISTAG("var")) { flushword(); setstyle(S_ITALIC, !closing); }
            else if(ISTAG("s") || ISTAG("strike") || ISTAG("del")) { flushword(); setstyle(S_STRIKE, !closing); }
            else if(ISTAG("code") || ISTAG("tt") || ISTAG("kbd") || ISTAG("samp")) { flushword(); setstyle(S_CODE, !closing); }
            else if(ISTAG("sup")) { flushword(); setstyle(S_SUP, !closing); }
            else if(ISTAG("sub")) { flushword(); setstyle(S_SUB, !closing); }
            else if(ISTAG("table"))
            {
                if(!closing && tiles < 0) starttiles();
                else if(closing && tiles >= 0)
                {
                    flushword();
                    p->blocks[tiles].end = p->items.length();
                    tiles = -1;
                    tilecell = -1;
                    intile = false;
                }
            }
            else if(ISTAG("td") || ISTAG("th"))
            {
                if(tiles < 0) { htmlblockbreak(); return; }
                flushword();
                block &b = p->blocks[tiles];
                if(!closing)
                {
                    cell &c = b.cells.add();
                    c.start = c.end = p->items.length();
                    c.align = L_CENTER;
                    c.header = ISTAG("th");
                    tilecell = b.cells.length()-1;
                    intile = true;
                    space = false;
                }
                else if(tilecell >= 0)
                {
                    while(p->items.length() > b.cells[tilecell].start && p->items.last().type == I_BREAK) p->items.pop();
                    b.cells[tilecell].end = p->items.length();
                    tilecell = -1;
                    intile = false;
                }
            }
            else if(ISTAG("ul") || ISTAG("ol"))
            {
                htmlblockbreak();
                htmllists = max(htmllists + (closing ? -1 : 1), 0);
            }
            else if(ISTAG("li"))
            {
                htmlblockbreak();
                if(!closing) setmarker(false, 0, false, false);
            }
            else if(ISTAG("hr")) { if(!closing && !infloat()) { openblock(B_RULE); closeblock(); } }
            else if(ISTAG("p") || ISTAG("div") || ISTAG("blockquote") || ISTAG("tr") || ISTAG("details") || ISTAG("summary") || ISTAG("figure") || ISTAG("figcaption") || ISTAG("center") || ISTAG("dl") || ISTAG("dt") || ISTAG("dd") ||
                    (nlen == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6'))
            {
                htmlblockbreak();
                if(ISTAG("summary")) setstyle(S_BOLD, !closing);
            }
            #undef ISTAG
        }

        // decodes entities in HTML text and attribute values
        static void decodetext(const char *s, int len, vector<char> &out)
        {
            for(int i = 0; i < len; i++)
            {
                if(s[i] == '&')
                {
                    const char *semi = (const char *)memchr(&s[i], ';', min(len - i, 32));
                    if(semi && decodeentity(&s[i], int(semi - &s[i]) + 1, out))
                    {
                        i = int(semi - s);
                        continue;
                    }
                }
                out.add(s[i]);
            }
        }

        void html(const char *s, int len)
        {
            int i = 0;
            while(i < len)
            {
                if(s[i] == '<')
                {
                    if(len - i >= 4 && !strncmp(&s[i], "<!--", 4))
                    {
                        const char *end = NULL;
                        for(int j = i + 4; j + 2 < len; j++) if(!strncmp(&s[j], "-->", 3)) { end = &s[j]; break; }
                        i = end ? int(end - s) + 3 : len;
                        continue;
                    }
                    const char *close = (const char *)memchr(&s[i], '>', len - i);
                    if(!close) break;
                    if(i + 1 < len && (isalpha(uchar(s[i+1])) || s[i+1] == '/')) htmltag(&s[i+1], int(close - &s[i+1]));
                    i = int(close - s) + 1;
                    continue;
                }
                int start = i;
                while(i < len && s[i] != '<') i++;
                if(htmlskip) continue;
                vector<char> text;
                decodetext(&s[start], i - start, text);
                // whitespace between tags of a block of HTML is layout, not text
                bool blank = true;
                loopvj(text) if(!isspace(uchar(text[j]))) { blank = false; break; }
                if(blank)
                {
                    if(text.length()) { flushword(); space = true; }
                    continue;
                }
                addtext(text.getbuf(), text.length());
            }
        }

        void attrtext(const MD_ATTRIBUTE &a, vector<char> &out)
        {
            if(!a.text) return;
            if(!a.substr_offsets) { out.put(a.text, a.size); return; }
            for(int i = 0; a.substr_offsets[i] < a.size; i++)
            {
                int start = a.substr_offsets[i], end = a.substr_offsets[i+1];
                if(a.substr_types[i] == MD_TEXT_ENTITY)
                {
                    if(!decodeentity(&a.text[start], end - start, out)) out.put(&a.text[start], end - start);
                }
                else if(a.substr_types[i] != MD_TEXT_NULLCHAR) out.put(&a.text[start], end - start);
            }
        }

        static int alerttype(const MD_ATTRIBUTE &a)
        {
            static const char *names[] = { "note", "tip", "important", "warning", "caution" };
            loopi(sizeof(names)/sizeof(names[0])) if(a.size == strlen(names[i]) && !strncasecmp(a.text, names[i], a.size)) return A_NOTE + i;
            return A_NOTE;
        }

        int enterblock(MD_BLOCKTYPE type, void *detail)
        {
            switch(type)
            {
                case MD_BLOCK_QUOTE: closeblock(); quote++; break;
                case MD_BLOCK_ADMONITION:
                {
                    closeblock();
                    quote++;
                    alert = alerttype(((MD_BLOCK_ADMONITION_DETAIL *)detail)->type);
                    static const char *titles[] = { "", "Note", "Tip", "Important", "Warning", "Caution" };
                    openblock(B_PARA);
                    setstyle(S_BOLD, true);
                    addtext(titles[alert], strlen(titles[alert]));
                    setstyle(S_BOLD, false);
                    closeblock();
                    break;
                }
                case MD_BLOCK_UL:
                {
                    closeblock();
                    liststate &l = lists.add();
                    l.ordered = false;
                    l.tight = ((MD_BLOCK_UL_DETAIL *)detail)->is_tight != 0;
                    l.next = 1;
                    break;
                }
                case MD_BLOCK_OL:
                {
                    closeblock();
                    liststate &l = lists.add();
                    l.ordered = true;
                    l.tight = ((MD_BLOCK_OL_DETAIL *)detail)->is_tight != 0;
                    l.next = ((MD_BLOCK_OL_DETAIL *)detail)->start;
                    break;
                }
                case MD_BLOCK_LI:
                {
                    closeblock();
                    MD_BLOCK_LI_DETAIL *li = (MD_BLOCK_LI_DETAIL *)detail;
                    bool ordered = lists.length() && lists.last().ordered;
                    int num = lists.length() ? lists.last().next++ : 1;
                    setmarker(ordered, num, li->is_task != 0, li->task_mark == 'x' || li->task_mark == 'X');
                    break;
                }
                case MD_BLOCK_HR: if(!infloat()) { openblock(B_RULE); closeblock(); } break;
                case MD_BLOCK_H:
                    openblock(B_HEADING, clamp(int(((MD_BLOCK_H_DETAIL *)detail)->level), 1, 6));
                    inheading = true;
                    headraw.setsize(0);
                    break;
                case MD_BLOCK_CODE: openblock(B_CODE); incode = true; break;
                case MD_BLOCK_HTML: if(!infloat()) closeblock(); inhtml = true; htmlbuf.setsize(0); break;
                case MD_BLOCK_P: if(!infloat()) openblock(B_PARA); break;
                case MD_BLOCK_TABLE:
                {
                    openblock(B_TABLE);
                    table = curblock;
                    curblock = -1;
                    p->blocks[table].cols = max(int(((MD_BLOCK_TABLE_DETAIL *)detail)->col_count), 1);
                    break;
                }
                case MD_BLOCK_TR: if(table >= 0) rowstart = p->blocks[table].cells.length(); break;
                case MD_BLOCK_TH: case MD_BLOCK_TD:
                {
                    if(table < 0) break;
                    flushword();
                    cell &c = p->blocks[table].cells.add();
                    c.start = c.end = p->items.length();
                    MD_ALIGN a = ((MD_BLOCK_TD_DETAIL *)detail)->align;
                    c.align = a == MD_ALIGN_CENTER ? L_CENTER : (a == MD_ALIGN_RIGHT ? L_RIGHT : L_LEFT);
                    c.header = type == MD_BLOCK_TH;
                    incell = true;
                    space = false;
                    break;
                }
                case MD_BLOCK_FOOTNOTE_DEF_SECTION:
                    closeblock();
                    openblock(B_RULE);
                    closeblock();
                    break;
                case MD_BLOCK_FOOTNOTE_DEF:
                {
                    closeblock();
                    MD_BLOCK_FOOTNOTE_DEF_DETAIL *fn = (MD_BLOCK_FOOTNOTE_DEF_DETAIL *)detail;
                    defformatstring(name, "fn-%u", fn->id);
                    addanchor(name, strlen(name));
                    setmarker(true, fn->id, false, false);
                    break;
                }
                default: break;
            }
            return 0;
        }

        int leaveblock(MD_BLOCKTYPE type, void *detail)
        {
            switch(type)
            {
                case MD_BLOCK_QUOTE: closeblock(); quote = max(quote-1, 0); break;
                case MD_BLOCK_ADMONITION: closeblock(); quote = max(quote-1, 0); alert = A_NONE; break;
                case MD_BLOCK_UL: case MD_BLOCK_OL: closeblock(); if(lists.length()) lists.pop(); marker.setsize(0); break;
                case MD_BLOCK_LI: closeblock(); marker.setsize(0); break;
                case MD_BLOCK_H:
                    flushword();
                    if(curblock >= 0) headinganchor();
                    inheading = false;
                    closeblock();
                    break;
                case MD_BLOCK_CODE:
                    flushword();
                    while(curblock >= 0 && p->items.length() > p->blocks[curblock].start && p->items.last().type == I_BREAK) p->items.pop();
                    incode = false;
                    closeblock();
                    break;
                case MD_BLOCK_HTML:
                    inhtml = false;
                    html(htmlbuf.getbuf(), htmlbuf.length());
                    flushword();
                    if(!infloat() && tiles < 0) closeblock();
                    loopi(S_NUM) stylecount[i] = 0;
                    while(htmllinks > 0) { htmllinks--; poplink(); }
                    break;
                case MD_BLOCK_P: if(!infloat()) closeblock(); else { flushword(); space = true; } break;
                case MD_BLOCK_TH: case MD_BLOCK_TD:
                {
                    if(table < 0) break;
                    flushword();
                    p->blocks[table].cells.last().end = p->items.length();
                    incell = false;
                    loopi(S_NUM) stylecount[i] = 0;
                    break;
                }
                case MD_BLOCK_TR:
                {
                    if(table < 0) break;
                    block &b = p->blocks[table];
                    while(b.cells.length() - rowstart < b.cols)
                    {
                        cell &c = b.cells.add();
                        c.start = c.end = p->items.length();
                        c.align = L_LEFT;
                        c.header = false;
                    }
                    while(b.cells.length() - rowstart > b.cols) b.cells.pop();
                    break;
                }
                case MD_BLOCK_TABLE:
                    if(table >= 0) p->blocks[table].end = p->items.length();
                    table = -1;
                    break;
                case MD_BLOCK_FOOTNOTE_DEF: closeblock(); break;
                default: break;
            }
            return 0;
        }

        int enterspan(MD_SPANTYPE type, void *detail)
        {
            switch(type)
            {
                case MD_SPAN_EM: flushword(); setstyle(S_ITALIC, true); break;
                case MD_SPAN_STRONG: flushword(); setstyle(S_BOLD, true); break;
                case MD_SPAN_CODE: flushword(); setstyle(S_CODE, true); break;
                case MD_SPAN_DEL: flushword(); setstyle(S_STRIKE, true); break;
                case MD_SPAN_SUPERSCRIPT: flushword(); setstyle(S_SUP, true); break;
                case MD_SPAN_SUBSCRIPT: flushword(); setstyle(S_SUB, true); break;
                case MD_SPAN_A:
                {
                    vector<char> href;
                    attrtext(((MD_SPAN_A_DETAIL *)detail)->href, href);
                    pushlink(href.getbuf(), href.length());
                    break;
                }
                case MD_SPAN_IMG:
                {
                    vector<char> src;
                    attrtext(((MD_SPAN_IMG_DETAIL *)detail)->src, src);
                    src.add('\0');
                    addimage(src.getbuf(), 0, 0);
                    htmlskip++; // the alt text comes as text inside the span
                    break;
                }
                case MD_SPAN_FOOTNOTE_REF:
                {
                    MD_SPAN_FOOTNOTE_REF_DETAIL *fn = (MD_SPAN_FOOTNOTE_REF_DETAIL *)detail;
                    defformatstring(href, "#fn-%u", fn->id);
                    pushlink(href, strlen(href));
                    setstyle(S_SUP, true);
                    defformatstring(num, "[%u]", fn->id);
                    addtext(num, strlen(num));
                    htmlskip++; // the label comes as text inside the span
                    break;
                }
                default: break;
            }
            return 0;
        }

        int leavespan(MD_SPANTYPE type, void *detail)
        {
            switch(type)
            {
                case MD_SPAN_EM: flushword(); setstyle(S_ITALIC, false); break;
                case MD_SPAN_STRONG: flushword(); setstyle(S_BOLD, false); break;
                case MD_SPAN_CODE: flushword(); setstyle(S_CODE, false); break;
                case MD_SPAN_DEL: flushword(); setstyle(S_STRIKE, false); break;
                case MD_SPAN_SUPERSCRIPT: flushword(); setstyle(S_SUP, false); break;
                case MD_SPAN_SUBSCRIPT: flushword(); setstyle(S_SUB, false); break;
                case MD_SPAN_A: poplink(); break;
                case MD_SPAN_IMG: htmlskip = max(htmlskip-1, 0); break;
                case MD_SPAN_FOOTNOTE_REF: flushword(); setstyle(S_SUP, false); poplink(); htmlskip = max(htmlskip-1, 0); break;
                default: break;
            }
            return 0;
        }

        int text(MD_TEXTTYPE type, const char *s, int len)
        {
            if(inhtml) { htmlbuf.put(s, len); return 0; }
            if(htmlskip) return 0;
            switch(type)
            {
                case MD_TEXT_NULLCHAR: break;
                case MD_TEXT_BR: addbreak(); break;
                case MD_TEXT_SOFTBR: flushword(); space = true; if(inheading) headraw.add(' '); break;
                case MD_TEXT_ENTITY:
                {
                    vector<char> out;
                    if(decodeentity(s, len, out)) addtext(out.getbuf(), out.length());
                    else addtext(s, len);
                    break;
                }
                case MD_TEXT_HTML: html(s, len); break;
                case MD_TEXT_CODE:
                {
                    if(!incell || !memchr(s, '\\', len)) { addtext(s, len, incode); break; }
                    // a table cell's \| is a pipe, even in code, as GitHub's tables have it
                    vector<char> out;
                    loopi(len)
                    {
                        if(s[i] == '\\' && i+1 < int(len) && s[i+1] == '|') continue;
                        out.add(s[i]);
                    }
                    addtext(out.getbuf(), out.length(), incode);
                    break;
                }
                default: addtext(s, len, incode); break;
            }
            return 0;
        }

        void finish()
        {
            closeblock();
            if(tiles >= 0) { p->blocks[tiles].end = p->items.length(); tiles = -1; }
            loopv(p->blocks)
            {
                block &b = p->blocks[i];
                if(b.type == B_TILES) loopvj(b.cells) if(b.cells[j].end < b.cells[j].start) b.cells[j].end = b.cells[j].start;
            }
        }
    };

    static int md_enterblock(MD_BLOCKTYPE type, void *detail, void *data) { return ((builder *)data)->enterblock(type, detail); }
    static int md_leaveblock(MD_BLOCKTYPE type, void *detail, void *data) { return ((builder *)data)->leaveblock(type, detail); }
    static int md_enterspan(MD_SPANTYPE type, void *detail, void *data) { return ((builder *)data)->enterspan(type, detail); }
    static int md_leavespan(MD_SPANTYPE type, void *detail, void *data) { return ((builder *)data)->leavespan(type, detail); }
    static int md_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *data) { return ((builder *)data)->text(type, text, int(size)); }

    static page *parsepage(const char *path, const char *text, size_t len)
    {
        page *p = new page;
        p->path = newstring(path);
        MD_PARSER parser;
        memset(&parser, 0, sizeof(parser));
        parser.abi_version = 0;
        parser.flags = MD_DIALECT_GITHUB;
        parser.enter_block = md_enterblock;
        parser.leave_block = md_leaveblock;
        parser.enter_span = md_enterspan;
        parser.leave_span = md_leavespan;
        parser.text = md_text;
        builder b(p);
        md_parse(text, MD_SIZE(len), &parser, &b);
        b.finish();
        return p;
    }

    static char *genvariables(int page, size_t &len);

    static page *getpage(const char *path)
    {
        if(!path || !validpath(path)) return NULL;
        loopv(pages) if(!strcmp(pages[i].path, path)) return pages[i].p;
        loopv(missing) if(!strcmp(missing[i], path)) return NULL;
        size_t len = 0;
        char *text = NULL;
        int vp = varpage(path);
        if(vp != VARPAGE_NONE) text = genvariables(vp, len);
        else text = loadfile(path, &len, false);
        if(!text)
        {
            missing.add(newstring(path));
            return NULL;
        }
        page *p = parsepage(path, text, len);
        delete[] text;
        cachedpage &c = pages.add();
        c.path = newstring(path);
        c.p = p;
        return p;
    }

    void clearpages()
    {
        loopv(pages) { delete[] pages[i].path; delete pages[i].p; }
        pages.setsize(0);
        missing.deletearrays();
        pagegen++;
    }
    ICOMMAND(0, docsreload, "", (), clearpages());

    // images are loaded when a page is laid out and released when no page shows them
    struct imageref
    {
        char *name;
        int refs;
    };
    static vector<imageref> imagerefs;

    static Texture *acquireimage(const char *name)
    {
        Texture *t = textureload(name, 3, true, false, true);
        if(!t || t == notexture) return NULL;
        loopv(imagerefs) if(!strcmp(imagerefs[i].name, name)) { imagerefs[i].refs++; return t; }
        imageref &r = imagerefs.add();
        r.name = newstring(name);
        r.refs = 1;
        return t;
    }

    static void releaseimage(const char *name)
    {
        loopv(imagerefs) if(!strcmp(imagerefs[i].name, name))
        {
            if(--imagerefs[i].refs > 0) return;
            Texture *t = textureloaded(name);
            if(t && t->type&Texture::GC) cleanuptexture(t);
            delete[] imagerefs[i].name;
            imagerefs.remove(i);
            return;
        }
    }

    enum { P_TEXT = 0, P_IMAGE, P_RECT };
    enum { PF_STRIKE = 1<<0, PF_MONO = 1<<1 };

    struct piece
    {
        uchar type, flags;
        int link, text, colour, alpha;
        float x, y, w, h, k;
        font *f;
        Texture *tex;
    };

    struct band
    {
        float y1, y2;
        int start, end;
    };

    struct layout
    {
        page *p;
        int gen;
        char *path;
        float width, scale, height;
        vector<piece> pieces;
        vector<band> bands;
        vector<float> maxy, blocky;
        vector<char> chars;
        vector<char *> images;

        layout() : p(NULL), gen(0), path(NULL), width(0), scale(0), height(0) {}
        ~layout() { clear(true); DELETEA(path); }

        void clear(bool release)
        {
            pieces.setsize(0);
            bands.setsize(0);
            maxy.setsize(0);
            blocky.setsize(0);
            chars.setsize(0);
            height = 0;
            if(release)
            {
                loopv(images) releaseimage(images[i]);
                images.deletearrays();
            }
        }
    };

    layout *newlayout() { return new layout; }
    void freelayout(layout *l) { delete l; }

    static Texture *layoutimage(layout *l, const char *name)
    {
        loopv(l->images) if(!strcmp(l->images[i], name)) return textureloaded(name);
        Texture *t = acquireimage(name);
        if(t) l->images.add(newstring(name));
        return t;
    }

    struct fontinfo { font *f; float fh, space; bool mono; };

    // a space in a monospaced font is as wide as its other characters, not the font's narrower default
    struct monospaces
    {
        float old;
        monospaces(bool mono) : old(textspacescale)
        {
            if(!mono || !curfont || curfont->defaultw <= 0) return;
            int c = 'M' - curfont->charoffset;
            if(curfont->chars.inrange(c)) textspacescale = curfont->chars[c].advance/curfont->defaultw;
        }
        ~monospaces() { textspacescale = old; }
    };

    // the context a run of items is laid out in
    struct flowstyle
    {
        int style, colour, align;
        float mult;
        bool code, bands;
    };

    struct flower
    {
        layout *l;
        page *p;
        float scale;
        fontinfo fonts[6];

        const fontinfo &getfont(int style)
        {
            int idx = style&S_CODE ? (style&S_BOLD ? 5 : 4) : ((style&S_BOLD ? 1 : 0) | (style&S_ITALIC ? 2 : 0));
            fontinfo &fi = fonts[idx];
            if(!fi.f)
            {
                static const char * const *names[6] = { &docfont, &docfontbold, &docfontitalic, &docfontbolditalic, &docfontmono, &docfontmonobold };
                if(!pushfont(*names[idx]) && !pushfont(textfontdef)) pushfont();
                fi.f = curfont;
                fi.fh = FONTH;
                fi.mono = idx >= 4;
                monospaces ms(fi.mono);
                fi.space = text_widthf(" ");
                popfont();
            }
            return fi;
        }

        // lines are at least a pixel wide, or they vanish between pixels
        float pixel() const { return 1.f/max(hudh, 1); }

        float textscale(const flowstyle &fs, int style)
        {
            float m = fs.mult;
            if(style&(S_SUP|S_SUB)) m *= DOCS_SUPSCALE;
            return scale*m;
        }

        float measuretext(const char *s, const fontinfo &fi, float size)
        {
            pushfont(fi.f);
            float w;
            {
                monospaces ms(fi.mono);
                w = text_widthf(s);
            }
            popfont();
            return w*size/fi.fh;
        }

        int colourfor(const flowstyle &fs, const item &t)
        {
            if(t.link >= 0) return doclinkcolour;
            if(t.style&S_CODE && !fs.code) return doccodecolour;
            return fs.colour;
        }

        void imagesize(const item &t, float avail, float &w, float &h, Texture *&tex)
        {
            w = h = 0;
            tex = t.text >= 0 && p->str(t.text)[0] ? layoutimage(l, p->str(t.text)) : NULL;
            if(!tex) return;
            float px = scale*DOCS_PXSCALE*docimagescale, tw = tex->w, th = tex->h;
            float rw = t.w < 0 ? avail*(-t.w/100.f) : t.w*px, rh = t.h < 0 ? 0 : t.h*px;
            if(rw > 0 && rh > 0) { w = rw; h = rh; }
            else if(rw > 0) { w = rw; h = rw*th/tw; }
            else if(rh > 0) { h = rh; w = rh*tw/th; }
            else { w = tw*px; h = th*px; }
            if(w > avail && avail > 0) { h *= avail/w; w = avail; }
        }

        struct entry
        {
            int type, item;
            float w, h, size;
            bool space;
            Texture *tex;
        };

        // measures one item; text needs the font for its style
        void measure(const flowstyle &fs, const item &t, float avail, entry &e)
        {
            e.type = t.type;
            e.tex = NULL;
            e.space = t.space;
            int style = t.style | fs.style;
            e.size = textscale(fs, style);
            switch(t.type)
            {
                case I_TEXT:
                {
                    const fontinfo &fi = getfont(style);
                    e.w = measuretext(p->str(t.text), fi, e.size);
                    e.h = e.size*doclinespacing;
                    break;
                }
                case I_ICON:
                {
                    Texture *tex = textureload(p->str(t.text), 3, true, false);
                    if(!tex || tex == notexture) { e.w = e.h = 0; break; }
                    e.tex = tex;
                    e.h = e.size*(t.w > 0 ? t.w : DOCS_ICONSCALE);
                    e.w = e.h*tex->w/float(max(tex->h, 1));
                    break;
                }
                case I_IMAGE: imagesize(t, avail, e.w, e.h, e.tex); break;
                default: e.w = 0; e.h = e.size*doclinespacing; break;
            }
        }

        // a space between words is as wide as the text around it, even next to inline code
        float spacewidth(const flowstyle &fs, int style)
        {
            if(!fs.code) style &= ~S_CODE;
            const fontinfo &fi = getfont(style | fs.style);
            return fi.space*textscale(fs, style | fs.style)/fi.fh;
        }

        // the widths a run needs: its longest unbreakable stretch, and all of it on one line per break
        void naturalwidth(const flowstyle &fs, int start, int end, float &minw, float &natw)
        {
            minw = natw = 0;
            float line = 0, run = 0;
            for(int i = start; i < end; i++)
            {
                const item &t = p->items[i];
                if(t.type == I_BREAK) { natw = max(natw, line); minw = max(minw, run); line = run = 0; continue; }
                entry e;
                measure(fs, t, 0, e);
                if(t.type == I_IMAGE && e.w <= 0) continue;
                float sp = t.space && line > 0 ? spacewidth(fs, t.style) : 0;
                if(t.space) { minw = max(minw, run); run = 0; }
                line += sp + e.w;
                run += e.w;
            }
            natw = max(natw, line);
            minw = max(minw, run);
        }

        int addchars(const char *s, int len = -1)
        {
            if(len < 0) len = strlen(s);
            int offset = l->chars.length();
            l->chars.put(s, len);
            l->chars.add('\0');
            return offset;
        }

        piece &addpiece(int type, float x, float y, float w, float h, int colour, int alpha = 0xFF)
        {
            piece &pc = l->pieces.add();
            pc.type = type;
            pc.flags = 0;
            pc.link = -1;
            pc.text = -1;
            pc.colour = colour;
            pc.alpha = alpha;
            pc.x = x;
            pc.y = y;
            pc.w = w;
            pc.h = h;
            pc.k = 0;
            pc.f = NULL;
            pc.tex = NULL;
            return pc;
        }

        int addband(float y1, float y2, int start)
        {
            band &b = l->bands.add();
            b.y1 = y1;
            b.y2 = y2;
            b.start = start;
            b.end = l->pieces.length();
            return l->bands.length()-1;
        }

        vector<entry> line;

        // places the entries collected for one line and returns its height
        float placeline(const flowstyle &fs, float x0, float x1, float y, bool last)
        {
            float texth = scale*fs.mult*doclinespacing, lineh = 0, linew = 0;
            loopv(line)
            {
                entry &e = line[i];
                float sp = i && e.space ? spacewidth(fs, p->items[e.item].style) : 0;
                linew += sp + e.w;
                lineh = max(lineh, e.type == I_IMAGE ? e.h : max(e.h, texth));
            }
            if(line.empty()) lineh = texth;
            float x = x0;
            if(fs.align == L_CENTER) x += max((x1 - x0) - linew, 0.f)*0.5f;
            else if(fs.align == L_RIGHT) x += max((x1 - x0) - linew, 0.f);
            int start = l->pieces.length();
            float base = y + lineh; // text and images sit on the bottom of the line
            loopv(line)
            {
                entry &e = line[i];
                const item &t = p->items[e.item];
                int style = t.style | fs.style;
                float sp = i && e.space ? spacewidth(fs, t.style) : 0;
                switch(e.type)
                {
                    case I_TEXT:
                    {
                        const fontinfo &fi = getfont(style);
                        float th = e.size*doclinespacing, ty = base - texth + (texth - th)*0.5f;
                        if(style&S_SUP) ty = base - texth;
                        else if(style&S_SUB) ty = base - th;
                        int colour = colourfor(fs, t);
                        piece *prev = l->pieces.length() > start ? &l->pieces.last() : NULL;
                        bool codebg = style&S_CODE && !fs.code;
                        if(prev && prev->type == P_TEXT && prev->f == fi.f && prev->k == e.size/fi.fh && prev->link == t.link && prev->colour == colour &&
                           prev->flags == ((style&S_STRIKE ? PF_STRIKE : 0) | (fi.mono ? PF_MONO : 0)) && prev->y == ty && !codebg && prev->text >= 0 && prev->text + int(strlen(&l->chars[prev->text])) + 1 == l->chars.length())
                        {
                            l->chars.pop();
                            if(sp > 0) l->chars.add(' ');
                            const char *s = p->str(t.text);
                            l->chars.put(s, strlen(s));
                            l->chars.add('\0');
                            prev->w += sp + e.w;
                            x += sp + e.w;
                            break;
                        }
                        x += sp;
                        if(codebg) addpiece(P_RECT, x - e.size*0.1f, ty, e.w + e.size*0.2f, th, doccodebgcolour, DOCS_CODEALPHA);
                        piece &pc = addpiece(P_TEXT, x, ty, e.w, th, colour);
                        pc.f = fi.f;
                        pc.k = e.size/fi.fh;
                        pc.link = t.link;
                        pc.text = addchars(p->str(t.text));
                        if(style&S_STRIKE) pc.flags |= PF_STRIKE;
                        if(fi.mono) pc.flags |= PF_MONO;
                        x += e.w;
                        break;
                    }
                    case I_ICON:
                    {
                        x += sp;
                        float th = e.size*doclinespacing, iy = base - texth + (texth - e.h)*0.5f;
                        if(th > texth) iy = base - th + (th - e.h)*0.5f;
                        piece &pc = addpiece(P_IMAGE, x, iy, e.w, e.h, 0xFFFFFF);
                        pc.tex = e.tex;
                        pc.link = t.link;
                        x += e.w;
                        break;
                    }
                    case I_IMAGE:
                    {
                        x += sp;
                        if(e.tex)
                        {
                            piece &pc = addpiece(P_IMAGE, x, base - e.h, e.w, e.h, 0xFFFFFF);
                            pc.tex = e.tex;
                            pc.link = t.link;
                        }
                        x += e.w;
                        break;
                    }
                    default: break;
                }
            }
            if(fs.bands) addband(y, y + lineh, start);
            line.setsize(0);
            return lineh;
        }

        // lays a run of items out between x0 and x1 from y, and returns the height used
        float flow(const flowstyle &fs, int start, int end, float x0, float x1, float y)
        {
            float top = y, x = 0, avail = max(x1 - x0, scale), slack = scale*0.01f; // columns are sized to fit their text exactly
            line.setsize(0);
            bool any = false;
            for(int i = start; i < end; i++)
            {
                item t = p->items[i]; // a copy, as wrapping code adds to the items
                if(t.type == I_BREAK)
                {
                    y += placeline(fs, x0, x1, y, false);
                    x = 0;
                    any = true;
                    continue;
                }
                entry e;
                measure(fs, t, avail, e);
                e.item = i;
                if(t.type == I_IMAGE && !e.tex) continue;
                if(fs.code && t.type == I_TEXT && e.w > avail)
                {
                    // code wraps by character, as it has no spaces to break at
                    vector<char> s;
                    s.put(p->str(t.text), strlen(p->str(t.text)));
                    int len = s.length(), per = max(int(len*avail/e.w), 1);
                    for(int pos = 0; pos < len; pos += per)
                    {
                        int n = min(per, len - pos);
                        item part = t;
                        part.text = p->addstr(&s[pos], n);
                        p->items.add(part);
                        entry pe;
                        measure(fs, part, avail, pe);
                        pe.item = p->items.length()-1;
                        if(!line.empty()) { y += placeline(fs, x0, x1, y, false); x = 0; }
                        line.add(pe);
                        x = pe.w;
                    }
                    any = true;
                    continue;
                }
                float sp = !line.empty() && t.space ? spacewidth(fs, t.style) : 0;
                if(!line.empty() && t.space && x + sp + e.w > avail + slack)
                {
                    y += placeline(fs, x0, x1, y, false);
                    x = 0;
                    sp = 0;
                }
                line.add(e);
                x += sp + e.w;
                any = true;
            }
            if(!line.empty() || !any) y += placeline(fs, x0, x1, y, true);
            return y - top;
        }

        int alertcolour(int alert)
        {
            switch(alert)
            {
                case A_NOTE: return docnotecolour;
                case A_TIP: return doctipcolour;
                case A_IMPORTANT: return docimportantcolour;
                case A_WARNING: return docwarningcolour;
                case A_CAUTION: return doccautioncolour;
                default: return docquotecolour;
            }
        }

        float layoutblock(int bi, float y, float width)
        {
            block &b = p->blocks[bi];
            float indent = scale*docindent, x0 = (b.depth + b.quote)*indent, x1 = width, top = y;
            flowstyle fs;
            fs.style = 0;
            fs.colour = doctextcolour;
            fs.align = L_LEFT;
            fs.mult = 1;
            fs.code = false;
            fs.bands = true;

            // the quote bars and code backgrounds are reserved first so they draw behind the text
            int deco = l->pieces.length(), decos = b.quote + (b.type == B_CODE ? 1 : 0);
            loopi(decos) addpiece(P_RECT, 0, 0, 0, 0, 0, 0);
            int decoband = decos ? addband(y, y, deco) : -1;

            switch(b.type)
            {
                case B_PARA:
                case B_HEADING:
                {
                    if(b.type == B_HEADING)
                    {
                        fs.style = S_BOLD;
                        fs.colour = docheadingcolour;
                        fs.mult = headingscales[clamp(int(b.level), 1, 6)-1];
                    }
                    if(b.markend > b.markstart)
                    {
                        flowstyle ms = fs;
                        ms.align = L_RIGHT;
                        ms.bands = true;
                        flow(ms, b.markstart, b.markend, x0 - indent, x0 - scale*0.3f, y);
                    }
                    y += flow(fs, b.start, b.end, x0, x1, y);
                    if(b.type == B_HEADING && b.level <= 2)
                    {
                        y += scale*0.2f;
                        int start = l->pieces.length();
                        float rh = max(scale*0.05f, pixel());
                        addpiece(P_RECT, x0, y, x1 - x0, rh, docrulecolour);
                        addband(y, y + rh, start);
                        y += rh;
                    }
                    break;
                }
                case B_CODE:
                {
                    float pad = scale*0.4f;
                    fs.style = S_CODE;
                    fs.colour = doccodecolour;
                    fs.code = true;
                    fs.mult = 0.9f;
                    y += pad;
                    y += flow(fs, b.start, b.end, x0 + pad, x1 - pad, y);
                    y += pad;
                    piece &bg = l->pieces[deco + b.quote];
                    bg.x = x0;
                    bg.y = top;
                    bg.w = x1 - x0;
                    bg.h = y - top;
                    bg.colour = doccodebgcolour;
                    bg.alpha = DOCS_CODEALPHA;
                    break;
                }
                case B_RULE:
                {
                    y += scale*0.4f;
                    int start = l->pieces.length();
                    float rh = max(scale*0.08f, pixel());
                    addpiece(P_RECT, x0, y, x1 - x0, rh, docrulecolour);
                    addband(y, y + rh, start);
                    y += rh + scale*0.4f;
                    break;
                }
                case B_TABLE: y += layouttable(b, x0, x1, y); break;
                case B_TILES: y += layouttiles(b, x0, x1, y); break;
            }

            loopi(b.quote)
            {
                piece &bar = l->pieces[deco + i];
                bar.x = (b.depth + i)*indent + indent*0.25f;
                bar.y = top;
                bar.w = scale*0.15f;
                bar.h = y - top;
                bar.colour = b.alert && i == b.quote-1 ? alertcolour(b.alert) : docquotecolour;
                bar.alpha = 0xFF;
            }
            if(decoband >= 0) l->bands[decoband].y2 = y;
            return y - top;
        }

        float layouttable(block &b, float x0, float x1, float y)
        {
            int cols = max(b.cols, 1), rows = b.cells.length()/cols;
            if(rows <= 0) return 0;
            float pad = scale*0.4f, vpad = scale*0.15f, line = max(scale*0.04f, pixel()), avail = x1 - x0;
            vector<float> minw, natw, colw;
            loopi(cols) { minw.add(0); natw.add(0); colw.add(0); }
            flowstyle fs;
            fs.colour = doctextcolour;
            fs.align = L_LEFT;
            fs.mult = 1;
            fs.code = false;
            fs.bands = false;
            loopv(b.cells)
            {
                cell &c = b.cells[i];
                fs.style = c.header ? S_BOLD : 0;
                float mn, nt;
                naturalwidth(fs, c.start, c.end, mn, nt);
                int col = i%cols;
                minw[col] = max(minw[col], mn + pad*2);
                natw[col] = max(natw[col], nt + pad*2);
            }
            float summin = 0, sumnat = 0;
            loopi(cols) { summin += minw[i]; sumnat += natw[i]; }
            if(sumnat <= avail) loopi(cols) colw[i] = natw[i];
            else if(summin >= avail) loopi(cols) colw[i] = minw[i];
            else
            {
                float extra = avail - summin, want = sumnat - summin;
                loopi(cols) colw[i] = minw[i] + (want > 0 ? extra*(natw[i] - minw[i])/want : 0);
            }
            float tablew = 0;
            loopi(cols) tablew += colw[i];
            float top = y;
            loopj(rows)
            {
                int start = l->pieces.length();
                piece &bg = addpiece(P_RECT, x0, y, tablew, 0, 0xFFFFFF, 0);
                bool header = b.cells[j*cols].header;
                if(header) bg.alpha = DOCS_HEADALPHA;
                else if(j%2 == 0) bg.alpha = DOCS_ROWALPHA;
                int bgidx = l->pieces.length()-1;
                float rowh = 0, cx = x0;
                loopi(cols)
                {
                    cell &c = b.cells[j*cols + i];
                    fs.style = c.header ? S_BOLD : 0;
                    fs.colour = c.header ? docheadingcolour : doctextcolour;
                    fs.align = c.align;
                    rowh = max(rowh, flow(fs, c.start, c.end, cx + pad, cx + colw[i] - pad, y + vpad));
                    cx += colw[i];
                }
                rowh += vpad*2;
                l->pieces[bgidx].h = rowh;
                // the grid
                addpiece(P_RECT, x0, y + rowh - line, tablew, line, doctablecolour);
                if(!j) addpiece(P_RECT, x0, y, tablew, line, doctablecolour);
                cx = x0;
                loopi(cols+1)
                {
                    addpiece(P_RECT, i < cols ? cx : x0 + tablew - line, y, line, rowh, doctablecolour);
                    if(i < cols) cx += colw[i];
                }
                addband(y, y + rowh, start);
                y += rowh;
            }
            return y - top;
        }

        float layouttiles(block &b, float x0, float x1, float y)
        {
            float gap = scale*0.6f, avail = x1 - x0, top = y, x = x0, rowh = 0;
            int rowband = -1;
            flowstyle fs;
            fs.style = 0;
            fs.colour = doctextcolour;
            fs.align = L_CENTER;
            fs.mult = 1;
            fs.code = false;
            fs.bands = false;
            loopv(b.cells)
            {
                cell &c = b.cells[i];
                float mn, nt;
                naturalwidth(fs, c.start, c.end, mn, nt);
                float tw = clamp(nt, min(mn, avail), avail);
                if(tw <= 0) continue;
                if(rowband >= 0 && x + tw > x1)
                {
                    l->bands[rowband].y2 = y + rowh;
                    l->bands[rowband].end = l->pieces.length();
                    y += rowh + gap;
                    x = x0;
                    rowh = 0;
                    rowband = -1;
                }
                if(rowband < 0) rowband = addband(y, y, l->pieces.length());
                rowh = max(rowh, flow(fs, c.start, c.end, x, x + tw, y));
                x += tw + gap;
            }
            if(rowband >= 0)
            {
                l->bands[rowband].y2 = y + rowh;
                l->bands[rowband].end = l->pieces.length();
                y += rowh;
            }
            return y - top;
        }

        float spacing(const block &prev, const block &b)
        {
            float para = scale*docparaspace;
            if(b.type == B_HEADING) return para*(b.level <= 2 ? 2.f : 1.5f);
            if(prev.type == B_HEADING) return para*0.6f;
            if(b.depth && prev.depth && b.tight) return para*0.25f;
            return para;
        }
    };

    static void dolayout(layout *l, float width, float scale)
    {
        l->clear(false);
        page *p = l->p;
        if(!p) return;
        int olditems = p->items.length(), oldchars = p->chars.length();
        flower fl;
        fl.l = l;
        fl.p = p;
        fl.scale = scale;
        loopi(6) fl.fonts[i].f = NULL;
        float y = 0;
        loopv(p->blocks)
        {
            if(i) y += fl.spacing(p->blocks[i-1], p->blocks[i]);
            l->blocky.add(y);
            y += fl.layoutblock(i, y, width);
        }
        l->blocky.add(y);
        l->height = y;
        // code wrapping adds parts to the page while laying out, they're only needed until it's done
        p->items.setsize(olditems);
        p->chars.setsize(oldchars);
        float m = 0;
        loopv(l->bands)
        {
            m = max(m, l->bands[i].y2);
            l->maxy.add(m);
        }
    }

    float update(layout *l, const char *path, float width, float scale)
    {
        bool newpage = !l->path || !path || strcmp(l->path, path) || l->gen != pagegen;
        if(newpage)
        {
            l->clear(true);
            DELETEA(l->path);
            if(path) l->path = newstring(path);
            l->p = getpage(path);
            l->gen = pagegen;
            l->width = l->scale = 0;
        }
        if(l->p && (newpage || l->width != width || l->scale != scale))
        {
            l->width = width;
            l->scale = scale;
            dolayout(l, width, scale);
        }
        return l->height;
    }

    bool found(layout *l) { return l->p != NULL; }

    static int firstband(layout *l, float y)
    {
        int lo = 0, hi = l->bands.length();
        while(lo < hi)
        {
            int mid = (lo + hi)/2;
            if(l->maxy[mid] <= y) lo = mid + 1;
            else hi = mid;
        }
        return lo;
    }

    int hittest(layout *l, float x, float y)
    {
        for(int i = firstband(l, y); i < l->bands.length() && l->bands[i].y1 <= y; i++)
        {
            band &b = l->bands[i];
            for(int j = b.start; j < b.end; j++)
            {
                piece &pc = l->pieces[j];
                if(pc.link >= 0 && x >= pc.x && x < pc.x + pc.w && y >= pc.y && y < pc.y + pc.h) return pc.link;
            }
        }
        return -1;
    }

    const char *linkhref(layout *l, int link)
    {
        if(!l->p || !l->p->links.inrange(link)) return "";
        return l->p->str(l->p->links[link]);
    }

    float anchory(layout *l, const char *name)
    {
        if(!l->p || !name) return -1;
        page *p = l->p;
        loopv(p->anchors) if(!strcasecmp(p->str(p->anchors[i].name), name))
        {
            int b = p->anchors[i].block;
            return l->blocky.inrange(b) ? l->blocky[b] : l->height;
        }
        return -1;
    }

    static void drawrect(float x, float y, float w, float h, int colour, int alpha)
    {
        gle::colorub((colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF, alpha);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x + w, y);
        gle::attribf(x, y);
        gle::attribf(x + w, y + h);
        gle::attribf(x, y + h);
        gle::end();
    }

    void draw(layout *l, float sx, float sy, float y1, float y2, int hover)
    {
        if(!l->p) return;
        int mode = -1;
        for(int i = firstband(l, y1); i < l->bands.length() && l->bands[i].y1 < y2; i++)
        {
            band &b = l->bands[i];
            for(int j = b.start; j < b.end; j++)
            {
                piece &pc = l->pieces[j];
                if(pc.y + pc.h < y1 || pc.y > y2) continue;
                switch(pc.type)
                {
                    case P_RECT:
                        if(pc.alpha <= 0 || pc.w <= 0 || pc.h <= 0) break;
                        if(mode != P_RECT) { hudnotextureshader->set(); gle::defvertex(2); mode = P_RECT; }
                        drawrect(sx + pc.x, sy + pc.y, pc.w, pc.h, pc.colour, pc.alpha);
                        break;
                    case P_IMAGE:
                        if(!pc.tex) break;
                        if(mode != P_IMAGE) { hudshader->set(); gle::defvertex(2); gle::deftexcoord0(); mode = P_IMAGE; }
                        settexture(pc.tex);
                        gle::colorf(1, 1, 1, 1);
                        gle::begin(GL_TRIANGLE_STRIP);
                        gle::attribf(sx + pc.x + pc.w, sy + pc.y); gle::attribf(1, 0);
                        gle::attribf(sx + pc.x, sy + pc.y); gle::attribf(0, 0);
                        gle::attribf(sx + pc.x + pc.w, sy + pc.y + pc.h); gle::attribf(1, 1);
                        gle::attribf(sx + pc.x, sy + pc.y + pc.h); gle::attribf(0, 1);
                        gle::end();
                        break;
                    case P_TEXT:
                    {
                        if(pc.text < 0 || !pc.f) break;
                        bool hovered = pc.link >= 0 && pc.link == hover;
                        int colour = hovered ? doclinkhovercolour : pc.colour;
                        pushfont(pc.f);
                        pushhudmatrix();
                        hudmatrix.translate(sx + pc.x, sy + pc.y, 0);
                        hudmatrix.scale(pc.k, pc.k, 1);
                        flushhudmatrix();
                        {
                            monospaces ms((pc.flags&PF_MONO) != 0);
                            draw_text(&l->chars[pc.text], 0, 0, (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF, 0xFF, TEXT_NO_INDENT);
                        }
                        pophudmatrix();
                        popfont();
                        mode = P_TEXT;
                        if(pc.flags&PF_STRIKE || hovered)
                        {
                            hudnotextureshader->set();
                            gle::defvertex(2);
                            mode = P_RECT;
                            float lh = pc.h/doclinespacing, ly = pc.flags&PF_STRIKE ? pc.y + pc.h*0.5f : pc.y + (pc.h + lh)*0.5f;
                            drawrect(sx + pc.x, sy + ly, pc.w, max(lh*0.06f, 1.f/max(hudh, 1)), colour, 0xFF);
                        }
                        break;
                    }
                }
            }
        }
        hudshader->set();
        gle::colorf(1, 1, 1);
    }

    // where a link goes: a docs page, an image, or a web address
    static void resolve(const char *base, const char *href, vector<char> &out)
    {
        while(*href && isspace(uchar(*href))) href++;
        string target, anchor;
        anchor[0] = '\0';
        const char *hash = strchr(href, '#');
        string rel;
        copystring(rel, href, hash ? min(int(hash - href) + 1, int(sizeof(rel))) : int(sizeof(rel)));
        if(hash) copystring(anchor, hash + 1);
        const char *type = "url";
        if(!rel[0] && hash)
        {
            type = "page";
            copystring(target, base);
        }
        else if(strstr(rel, "://") || !strncasecmp(rel, "mailto:", 7))
        {
            copystring(target, href);
            static const char * const prefixes[] = { "https://www.redeclipse.net/docs/", "http://www.redeclipse.net/docs/", "https://redeclipse.net/docs/", "http://redeclipse.net/docs/" };
            loopi(sizeof(prefixes)/sizeof(prefixes[0])) if(!strncasecmp(rel, prefixes[i], strlen(prefixes[i])))
            {
                string local, norm;
                formatstring(local, "%s%s", DOCS_ROOT, rel + strlen(prefixes[i]));
                int len = strlen(local);
                while(len > DOCS_ROOTLEN && local[len-1] == '/') local[--len] = '\0';
                if(normalizepath("", local, norm))
                {
                    if(!strcmp(norm, "docs")) copystring(norm, DOCS_ROOT "Home.md");
                    else concatstring(norm, ".md");
                    if(pageexists(norm)) { type = "page"; copystring(target, norm); }
                }
                break;
            }
        }
        else
        {
            string norm;
            if(normalizepath(base, rel, norm) && !strncmp(norm, DOCS_ROOT, DOCS_ROOTLEN))
            {
                const char *ext = strrchr(norm, '.'), *slash = strrchr(norm, '/');
                if(ext && slash && ext < slash) ext = NULL;
                if(!ext)
                {
                    string md;
                    formatstring(md, "%s.md", norm);
                    if(pageexists(md)) { type = "page"; copystring(target, md); }
                    else formatstring(target, "%s%s", DOCS_WEBSITE, norm + DOCS_ROOTLEN);
                }
                else if(!strcasecmp(ext, ".md")) { type = "page"; copystring(target, norm); }
                else if(!strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg")) { type = "image"; copystring(target, norm); }
                else formatstring(target, "%s%s", DOCS_WEBSITE, norm + DOCS_ROOTLEN);
            }
            else copystring(target, DOCS_WEBSITE); // outside the docs
        }
        const char *parts[3] = { type, target, anchor };
        loopi(3)
        {
            if(i) out.add(' ');
            const char *esc = escapestring(parts[i]);
            out.put(esc, strlen(esc));
        }
        out.add('\0');
    }

    static void docsresolve(char *base, char *href)
    {
        vector<char> out;
        resolve(base, href, out);
        result(out.getbuf());
    }
    COMMAND(0, docsresolve, "ss");

    ICOMMAND(0, docsexists, "s", (char *path), intret(pageexists(path) && getpage(path) ? 1 : 0));

    // a page's title from its path, the way the website names it: <Subsection> - <Name>, every word of the folder capitalised,
    // hyphens and underscores as spaces, and an Index.md by its folder alone
    static void docstitle(char *path)
    {
        string rel;
        copystring(rel, strncmp(path, DOCS_ROOT, DOCS_ROOTLEN) ? path : path + DOCS_ROOTLEN);
        char *name = strrchr(rel, '/'), *ext = strrchr(rel, '.');
        if(ext && (!name || ext > name)) *ext = '\0';
        vector<char> out;
        if(name)
        {
            *name++ = '\0';
            bool word = true;
            for(const char *c = rel; *c; c++)
            {
                if(*c == '/') { out.put(" - ", 3); word = true; }
                else if(*c == '-' || *c == '_') { out.add(' '); word = true; }
                else { out.add(word ? cubeupper(*c) : *c); word = false; }
            }
            if(!strcmp(name, "Index")) name = NULL;
            else out.put(" - ", 3);
        }
        else name = rel;
        if(name) for(const char *c = name; *c; c++) out.add(*c == '-' || *c == '_' ? ' ' : *c);
        out.add('\0');
        result(out.getbuf());
    }
    COMMAND(0, docstitle, "s");

    // every page under the docs root, from the home directory, the working directory and the package directories
    static void listpages(const char *dir, vector<char *> &out, int depth = 0)
    {
        if(depth > 4) return;
        vector<char *> files, dirs;
        listfiles(dir, "md", files, LIST_FILES);
        loopv(files)
        {
            defformatstring(path, "%s/%s.md", dir, files[i]);
            bool dupe = false;
            loopvj(out) if(!strcmp(out[j], path)) { dupe = true; break; }
            if(!dupe && validpath(path)) out.add(newstring(path));
        }
        files.deletearrays();
        listfiles(dir, NULL, dirs, LIST_DIRS);
        loopv(dirs) if(dirs[i][0] != '.' && strcmp(dirs[i], "images"))
        {
            defformatstring(sub, "%s/%s", dir, dirs[i]);
            listpages(sub, out, depth + 1);
        }
        dirs.deletearrays();
    }

    static void listvarpages(vector<char *> &out);

    // the words of a run of items, as plain text in the engine's charset
    static void itemtext(page *p, int start, int end, vector<char> &out)
    {
        for(int i = start; i < end; i++)
        {
            const item &t = p->items[i];
            if(t.type == I_BREAK || (t.space && out.length())) out.add(' ');
            if(t.type == I_TEXT) { const char *s = p->str(t.text); out.put(s, strlen(s)); }
        }
    }

    static void blocktext(page *p, const block &b, vector<char> &out)
    {
        if(b.type == B_TABLE || b.type == B_TILES) loopv(b.cells)
        {
            if(out.length()) out.add(' ');
            itemtext(p, b.cells[i].start, b.cells[i].end, out);
        }
        else itemtext(p, b.start, b.end, out);
        out.add('\0');
    }

    // the text of the heading an anchor names on a page, or nothing when it isn't a heading's
    static void docsheading(char *path, char *name)
    {
        page *p = name[0] ? getpage(path) : NULL;
        if(p) loopv(p->anchors) if(!strcasecmp(p->str(p->anchors[i].name), name))
        {
            int b = p->anchors[i].block;
            if(!p->blocks.inrange(b) || p->blocks[b].type != B_HEADING) break;
            vector<char> text;
            blocktext(p, p->blocks[b], text);
            result(text.getbuf());
            return;
        }
        result("");
    }
    COMMAND(0, docsheading, "ss");

    #define DOCS_SEARCHMAX 50
    #define DOCS_SNIPPET 40

    // finds text in every page: a list of the page, the anchor of the heading above, that heading and a snippet, four to a result
    static void docssearch(char *query)
    {
        vector<char> results;
        int found = 0;
        if(strlen(query) >= 2)
        {
            vector<char *> paths;
            listpages("docs", paths);
            listvarpages(paths);
            paths.sort();
            loopv(paths)
            {
                page *p = getpage(paths[i]);
                if(!p) continue;
                int heading = -1, lastadded = -2;
                loopvj(p->blocks)
                {
                    const block &b = p->blocks[j];
                    if(b.type == B_HEADING) heading = j;
                    vector<char> text;
                    blocktext(p, b, text);
                    const char *hit = cubecasefind(text.getbuf(), query);
                    if(!hit || heading == lastadded || found >= DOCS_SEARCHMAX) continue;
                    lastadded = heading; // one result per section
                    string anchorname = "", headtext = "", snippet;
                    if(heading >= 0)
                    {
                        loopvk(p->anchors) if(p->anchors[k].block == heading) { copystring(anchorname, p->str(p->anchors[k].name)); break; }
                        vector<char> ht;
                        blocktext(p, p->blocks[heading], ht);
                        copystring(headtext, ht.getbuf());
                    }
                    int pos = int(hit - text.getbuf()), from = max(pos - DOCS_SNIPPET, 0), to = min(pos + int(strlen(query)) + DOCS_SNIPPET, text.length()-1);
                    while(from > 0 && text[from-1] != ' ') from--;
                    formatstring(snippet, "%s%.*s%s", from > 0 ? "..." : "", to - from, &text[from], to < text.length()-1 ? "..." : "");
                    const char *parts[4] = { paths[i], anchorname, headtext, snippet };
                    loopk(4)
                    {
                        if(results.length()) results.add(' ');
                        const char *esc = escapestring(parts[k]);
                        results.put(esc, strlen(esc));
                    }
                    found++;
                }
            }
            paths.deletearrays();
        }
        results.add('\0');
        result(results.getbuf());
    }
    COMMAND(0, docssearch, "s");

    // the variables pages are made from the descriptions the game has, rather than read from the docs
    // text in the engine's charset as UTF-8, with Markdown's own characters escaped so they read as written
    static void putmd(vector<char> &out, const char *s, bool cell = true)
    {
        for(; *s; s++)
        {
            uchar c = uchar(*s);
            if(c == '\n') { if(cell) { out.put("<br>", 4); } else out.add(' '); continue; }
            if(c < 0x20) continue;
            if(strchr("\\`*_[]<>|#~", c)) out.add('\\');
            if(c < 0x80) out.add(char(c));
            else putcode(out, cube2uni(c));
        }
    }

    static void putcodespan(vector<char> &out, const char *s)
    {
        out.add('`');
        for(; *s; s++)
        {
            uchar c = uchar(*s);
            if(c < 0x20 || c == '`') continue;
            if(c == '|') { out.put("\\|", 2); continue; }
            if(c < 0x80) out.add(char(c));
            else putcode(out, cube2uni(c));
        }
        out.add('`');
    }

    static void putrow(vector<char> &out, ident *id)
    {
        out.put("| ", 2);
        putcodespan(out, id->name);
        out.put(" | ", 3);
        putmd(out, id->desc);
        out.put(" | ", 3);
        if(id->fields.empty()) out.add('-');
        loopv(id->fields)
        {
            if(i) out.add(' ');
            putcodespan(out, id->fields[i]);
        }
        out.put(" |\n", 3);
    }

    // the file a page of the variables reference is, named after its section: letters and digits, with hyphens between words
    static void varpagefile(int page, string file)
    {
        int level = 0, parent = -1;
        const char *about = NULL, *name = page >= 0 ? getdescsection(page, level, parent, about) : "Other";
        copystring(file, DOCS_VARDIR, MAXSTRLEN);
        int len = DOCS_VARDIRLEN;
        bool gap = false;
        for(const char *c = name; *c && len < MAXSTRLEN-5; c++)
        {
            if(isalnum(uchar(*c)))
            {
                if(gap && len > DOCS_VARDIRLEN) file[len++] = '-';
                file[len++] = *c;
                gap = false;
            }
            else gap = true;
        }
        copystring(&file[len], ".md", MAXSTRLEN-len);
    }

    // whether a page has descriptions under it, of its own or under its headings
    static bool varpagehas(int page)
    {
        int total = numdescs();
        loopi(total)
        {
            int section, level = 0, parent = -1;
            const char *about = NULL;
            ident *id = getdesc(i, section);
            if(!id || !id->desc) continue;
            if(section == page) return true;
            if(page >= 0 && section >= 0) { getdescsection(section, level, parent, about); if(level && parent == page) return true; }
        }
        return false;
    }

    static bool isvarpage(int s)
    {
        if(s < 0) return s == VARPAGE_OTHER && varpagehas(VARPAGE_OTHER);
        int level = 0, parent = -1;
        const char *about = NULL;
        getdescsection(s, level, parent, about);
        return !level && varpagehas(s);
    }

    // which variables page a path is: the index, a section's page, the page of descriptions under no section, or none
    static int varpage(const char *path)
    {
        if(strncmp(path, DOCS_VARDIR, DOCS_VARDIRLEN)) return VARPAGE_NONE;
        if(!strcmp(path, DOCS_VARIABLES)) return VARPAGE_INDEX;
        for(int s = -1; s < numdescsections(); s++) if(isvarpage(s))
        {
            string file;
            varpagefile(s, file);
            if(!strcmp(file, path) && strcmp(file, DOCS_VARIABLES)) return s;
        }
        return VARPAGE_NONE;
    }

    // the variables pages the docs on disk don't have, for searching
    static void listvarpages(vector<char *> &out)
    {
        for(int s = VARPAGE_INDEX; s < numdescsections(); s++) if(s == VARPAGE_INDEX || isvarpage(s))
        {
            string file;
            if(s == VARPAGE_INDEX) copystring(file, DOCS_VARIABLES);
            else varpagefile(s, file);
            bool dupe = false;
            loopv(out) if(!strcmp(out[i], file)) { dupe = true; break; }
            if(!dupe) out.add(newstring(file));
        }
    }

    // the table of descriptions under one section
    static void putvartable(vector<char> &out, int section)
    {
        static const char header[] = "| Variable / Command | Description | Arguments / Fields |\n| :--- | :--- | :--- |\n";
        bool any = false;
        int total = numdescs();
        loopi(total)
        {
            int s;
            ident *id = getdesc(i, s);
            if(!id || !id->desc || s != section) continue;
            if(!any) { out.put(header, strlen(header)); any = true; }
            putrow(out, id);
        }
        if(any) out.add('\n');
    }

    // the variables pages are made from the descriptions the game has, rather than read from the docs:
    // the index lists the pages, and each page has its own descriptions then those under its headings
    static char *genvariables(int page, size_t &len)
    {
        vector<char> out;
        if(page == VARPAGE_INDEX)
        {
            static const char intro[] = "Variables and commands can be used to change almost everything about how the game works. The game makes these pages from the descriptions it has, so they match the version you're playing. Type them in the console, such as `/fov 100`, or put them in your `autoexec.cfg`.\n\n";
            out.put(intro, strlen(intro));
            for(int s = 0; s <= numdescsections(); s++)
            {
                int p = s < numdescsections() ? s : VARPAGE_OTHER;
                if(!isvarpage(p)) continue;
                int level = 0, parent = -1;
                const char *about = "Descriptions that aren't under any of the other pages", *name = p >= 0 ? getdescsection(p, level, parent, about) : "Other";
                string file;
                varpagefile(p, file);
                out.put("- [", 3);
                putmd(out, name, false);
                out.put("](", 2);
                out.put(&file[DOCS_VARDIRLEN], strlen(&file[DOCS_VARDIRLEN]));
                out.add(')');
                if(about && *about) { out.put(": ", 2); putmd(out, about, false); }
                out.add('\n');
            }
        }
        else
        {
            int level = 0, parent = -1;
            const char *about = NULL;
            if(page >= 0)
            {
                getdescsection(page, level, parent, about);
                if(about && *about) { putmd(out, about, false); out.put(".\n\n", 3); }
            }
            putvartable(out, page);
            if(page >= 0) loopi(numdescsections())
            {
                const char *name = getdescsection(i, level, parent, about);
                if(!level || parent != page) continue;
                bool any = false;
                int total = numdescs();
                loopj(total)
                {
                    int s;
                    ident *id = getdesc(j, s);
                    if(id && id->desc && s == i) { any = true; break; }
                }
                if(!any) continue;
                out.put("## ", 3);
                putmd(out, name, false);
                out.put("\n\n", 2);
                putvartable(out, i);
            }
        }
        len = out.length();
        char *text = new char[len + 1];
        memcpy(text, out.getbuf(), len);
        text[len] = '\0';
        return text;
    }
}

