#include "engine.h"

VAR(IDF_PERSIST, textblinking, 0, 250, VAR_MAX);
float curtextscale = 1;
FVARF(IDF_PERSIST, textscale, FVAR_NONZERO, 1, FVAR_MAX, curtextscale = textscale);
FVAR(IDF_PERSIST, textlinespacing, FVAR_NONZERO, 1, FVAR_MAX);
VAR(IDF_PERSIST, textfaded, 0, 1, 1);
VAR(IDF_PERSIST, textminintensity, 0, 64, 255);
VAR(IDF_PERSIST, textwrapmin, 0, 10, VAR_MAX);
FVAR(IDF_PERSIST, textwraplimit, 0, 0.3f, 1);

VAR(IDF_PERSIST, textkeyimages, 0, 1, 1);
FVAR(IDF_PERSIST, textkeyimagescale, 0, 1, FVAR_MAX);
VAR(IDF_PERSIST, textkeyseps, 0, 1, 1);
VAR(IDF_PERSIST|IDF_HEX, textkeycolour, 0, 0x00FFFF, 0xFFFFFF);

static hashnameset<font> fonts;
static font *fontdef = NULL;
static int fontdeftex = 0;

vector<font *> fontstack;
font *curfont = NULL;
int curfonttex = 0;

void newfont(char *name, char *tex, int *defaultw, int *defaulth)
{
    font *f = &fonts[name];
    if(!f->name) f->name = newstring(name);
    f->texs.shrink(0);
    f->texs.add(textureload(tex));
    f->chars.shrink(0);

    f->charoffset = '!';
    f->mw = f->maxw = f->defaultw = *defaultw;
    f->mh = f->maxh = f->defaulth = f->scale = *defaulth;

    fontdef = f;
    fontdeftex = 0;
}

void fontoffset(char *c)
{
    if(!fontdef) return;

    fontdef->charoffset = c[0];
}

void fontscale(int *scale)
{
    if(!fontdef) return;

    fontdef->scale = *scale > 0 ? *scale : fontdef->defaulth;
    fontdef->mw = fontdef->maxw*fontdef->scale/float(fontdef->defaulth);
    fontdef->mh = fontdef->maxh*fontdef->scale/float(fontdef->defaulth);
}

void fonttex(char *s)
{
    if(!fontdef) return;

    Texture *t = textureload(s);
    loopv(fontdef->texs) if(fontdef->texs[i] == t) { fontdeftex = i; return; }
    fontdeftex = fontdef->texs.length();
    fontdef->texs.add(t);
}

void fontchar(float *x, float *y, float *w, float *h, float *offsetx, float *offsety, float *advance)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
    c.offsetx = *offsetx;
    c.offsety = *offsety;
    if(c.offsetx+c.w > fontdef->maxw) fontdef->maxw = fontdef->mw = c.offsetx+c.w;
    if(c.offsety+c.h > fontdef->maxh) fontdef->maxh = fontdef->mh = c.offsety+c.h;
    c.advance = *advance ? *advance : c.offsetx + c.w;
    c.tex = fontdeftex;
}

void fontskip(int *n)
{
    if(!fontdef) return;
    loopi(max(*n, 1))
    {
        font::charinfo &c = fontdef->chars.add();
        c.x = c.y = c.w = c.h = c.offsetx = c.offsety = c.advance = c.tex = 0;
    }
}

COMMANDN(0, font, newfont, "ssii");
COMMAND(0, fontoffset, "s");
COMMAND(0, fontscale, "i");
COMMAND(0, fonttex, "s");
COMMAND(0, fontchar, "fffffff");
COMMAND(0, fontskip, "i");

font *loadfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f)
    {
        defformatstring(n, "config/fonts/%s.cfg", name);
        if(execfile(n, false)) f = fonts.access(name);
    }
    return f;
}

void fontalias(const char *dst, const char *src)
{
    font *s = loadfont(src);
    if(!s) return;
    font *d = &fonts[dst];
    if(!d->name) d->name = newstring(dst);
    d->texs = s->texs;
    d->chars = s->chars;
    d->charoffset = s->charoffset;
    d->defaultw = s->defaultw;
    d->defaulth = s->defaulth;
    d->scale = s->scale;
    d->maxw = s->maxw;
    d->mw = s->mw;
    d->maxh = s->maxh;
    d->mh = s->mh;

    fontdef = d;
    fontdeftex = d->texs.length()-1;
}

COMMAND(0, fontalias, "ss");

font *findfont(const char *name)
{
    return fonts.access(name);
}

bool setfont(font *id)
{
    if(!id) return false;
    curfont = id;
    return true;
}

bool setfont(const char *name)
{
    return setfont(loadfont(name));
}

bool pushfont(font *id)
{
    if(!fontstack.length() && curfont)
        fontstack.add(curfont);

    if(setfont(id))
    {
        fontstack.add(curfont);
        return true;
    }
    return false;
}

bool pushfont(const char *name)
{
    return pushfont(loadfont(name));
}

bool popfont(int num)
{
    loopi(num)
    {
        if(fontstack.empty()) break;
        fontstack.pop();
    }
    if(fontstack.length()) { curfont = fontstack.last(); return true; }
    return setfont("default");
}

float text_widthf(const char *str, float xpad, float ypad, int flags, float linespace)
{
    float width, height;
    if(linespace <= 0) linespace = textlinespacing;
    text_boundsf(str, width, height, xpad, ypad, -1, flags, linespace);
    return width;
}
ICOMMAND(0, textwidth, "sifff", (char *s, int *f, float *x, float *y, float *p), floatret(text_widthf(s, *x, *y, *f, *p!=0 ? *p : 1.f)));

float text_fontw(const char *s)
{
    if(s && *s) pushfont(s);
    float w = FONTW;
    if(s && *s) popfont();
    return w;
}
ICOMMAND(0, fontwidth, "s", (char *s), floatret(text_fontw(s)));

float text_fonth(const char *s)
{
    if(s && *s) pushfont(s);
    float h = FONTH;
    if(s && *s) popfont();
    return h;
}
ICOMMAND(0, fontheight, "s", (char *s), floatret(text_fonth(s)));

#define TEXTTAB(x) (max((int((x)/FONTTAB)+1.0f)*FONTTAB, (x)+FONTW)-(x))

void tabify(const char *str, int *numtabs)
{
    int tw = max(*numtabs, 0)*FONTTAB-1, tabs = 0;
    for(float w = text_widthf(str); w <= tw; w += TEXTTAB(w)) ++tabs;
    int len = strlen(str);
    char *tstr = newstring(len + tabs);
    memcpy(tstr, str, len);
    memset(&tstr[len], '\t', tabs);
    tstr[len+tabs] = '\0';
    stringret(tstr);
}

COMMAND(0, tabify, "si");

const matrix4x3 *textmatrix = NULL;

static float draw_char(Texture *&tex, int c, float x, float y, float scale)
{
    font::charinfo &info = curfont->chars[c-curfont->charoffset];
    if(tex != curfont->texs[info.tex])
    {
        xtraverts += gle::end();
        tex = curfont->texs[info.tex];
        glBindTexture(GL_TEXTURE_2D, tex->id);
    }

    float x1 = x + scale*info.offsetx,
          y1 = y + scale*info.offsety,
          x2 = x + scale*(info.offsetx + info.w),
          y2 = y + scale*(info.offsety + info.h),
          tx1 = info.x / float(tex->xs),
          ty1 = info.y / float(tex->ys),
          tx2 = (info.x + info.w) / float(tex->xs),
          ty2 = (info.y + info.h) / float(tex->ys);

    if(textmatrix)
    {
        gle::attrib(textmatrix->transform(vec2(x1, y1))); gle::attribf(tx1, ty1);
        gle::attrib(textmatrix->transform(vec2(x2, y1))); gle::attribf(tx2, ty1);
        gle::attrib(textmatrix->transform(vec2(x2, y2))); gle::attribf(tx2, ty2);
        gle::attrib(textmatrix->transform(vec2(x1, y2))); gle::attribf(tx1, ty2);
    }
    else
    {
        gle::attribf(x1, y1); gle::attribf(tx1, ty1);
        gle::attribf(x2, y1); gle::attribf(tx2, ty1);
        gle::attribf(x2, y2); gle::attribf(tx2, ty2);
        gle::attribf(x1, y2); gle::attribf(tx1, ty2);
    }

    return scale*info.advance;
}

#define TVECR(ci, ca) (bvec4::fromcolor(ci).lighten(textminintensity).alpha(ca))
#define TVECA(ci, ca) (flags&TEXT_MODCOL ? (TVECR(ci, ca).muld(r, g, b, 255.f)) : (TVECR(ci, ca)))
#define TVECX(cr, cg, cb, ca) (bvec4(cr, cg, cb, 255).lighten(textminintensity).alpha(ca))

#define COLOURDARK 0.45f
#define COLOURVAR(a,b) \
    VAR(IDF_PERSIST|IDF_HEX, colour##a, 0, b, 0xFFFFFF); \
    VAR(IDF_PERSIST|IDF_HEX, colourdark##a, 0, ((int(COLOURDARK*((b>>16)&0xFF)))<<16)|((int(COLOURDARK*((b>>8)&0xFF)))<<8)|(int(COLOURDARK*(b&0xFF))), 0xFFFFFF);

VAR(IDF_PERSIST|IDF_HEX, colourblack, 0, 0x000000, 0xFFFFFF);
VAR(IDF_PERSIST|IDF_HEX, colourwhite, 0, 0xFFFFFF, 0xFFFFFF);

COLOURVAR(green, 0x00FF00);
COLOURVAR(blue, 0x0000FF);
COLOURVAR(yellow, 0xFFFF00);
COLOURVAR(red, 0xFF0000);
COLOURVAR(grey, 0xB0B0B0);
COLOURVAR(magenta, 0xFF80FF);
COLOURVAR(orange, 0xFF4000);
COLOURVAR(cyan, 0x00FFFF);
COLOURVAR(pink, 0xFF8080);
COLOURVAR(violet, 0xB060FF);
COLOURVAR(purple, 0xFF00FF);
COLOURVAR(brown, 0xA05030);

static void text_color(char c, bvec4 *stack, int size, int &sp, bvec4 &color, int r, int g, int b, int a, int flags)
{
    int alpha = stack[sp].a;
    switch(c)
    {
        case '0': case 'g': case 'G': stack[sp] = color = TVECA(c == 'G' ? colourdarkgreen : colourgreen, alpha); break;
        case '1': case 'b': case 'B': stack[sp] = color = TVECA(c == 'B' ? colourdarkblue : colourblue, alpha); break;
        case '2': case 'y': case 'Y': stack[sp] = color = TVECA(c == 'Y' ? colourdarkyellow : colouryellow, alpha); break;
        case '3': case 'r': case 'R': stack[sp] = color = TVECA(c == 'R' ? colourdarkred : colourred, alpha); break;
        case '4': case 'a': case 'A': case 'W': stack[sp] = color = TVECA(c == 'A' ? colourdarkgrey : colourgrey, alpha); break;
        case '5': case 'm': case 'M': stack[sp] = color = TVECA(c == 'M' ? colourdarkmagenta : colourmagenta, alpha); break;
        case '6': case 'o': case 'O': stack[sp] = color = TVECA(c == 'O' ? colourdarkorange : colourorange, alpha); break;
        case '7': case 'w': stack[sp] = color = TVECA(colourwhite, alpha); break;
        case '8': case 'k': stack[sp] = color = TVECA(colourblack, alpha); break;
        case '9': case 'c': case 'C': stack[sp] = color = TVECA(c == 'C' ? colourdarkcyan : colourcyan, alpha); break;
        case 'i': case 'I': stack[sp] = color = TVECA(c == 'I' ? colourdarkpink : colourpink, alpha); break;
        case 'v': case 'V': stack[sp] = color = TVECA(c == 'V' ? colourdarkviolet : colourviolet, alpha); break;
        case 'p': case 'P': stack[sp] = color = TVECA(c == 'P' ? colourdarkpurple : colourpurple, alpha); break;
        case 'n': case 'N': stack[sp] = color = TVECA(c == 'N' ? colourdarkbrown : colourbrown, alpha); break;
        case 'e': case 'E': (stack[sp] = color = stack[sp]).alpha(c != 'E' ? a/2 : a/4); break;
        case 'u': case 'Z': stack[sp] = color = TVECX(r, g, b, a); break; // default colour
        case 's': // save
        {
            if(sp < size-1) stack[++sp] = color;
            return;
        }
        case 'S': // restore
        {
            if(sp > 0) --sp;
            color = stack[sp];
            break;
        }
        default: color = stack[sp]; break; // everything else
    }
    xtraverts += gle::end();
    gle::color(color);
}

static const char *gettexvar(const char *var)
{
    ident *id = getident(var);
    if(!id) return "";
    switch(id->type)
    {
        case ID_VAR: return intstr(*id->storage.i);
        case ID_FVAR: return floatstr(*id->storage.f);
        case ID_SVAR: return *id->storage.s;
        case ID_ALIAS: return id->getstr();
        default: return "";
    }
}

#define textvert(vx,vy) do { \
    if(textmatrix) gle::attrib(textmatrix->transform(vec2(vx, vy))); \
    else gle::attribf(vx, vy); \
} while(0)

static float draw_icon(Texture *&tex, const char *name, float x, float y, float scale)
{
    if(!name && !*name) return 0;
    const char *file = name;
    if(*file == '$') file = gettexvar(++file);
    if(!*file) return 0;
    Texture *t = textureload(file, 3, true, false);
    if(!t) return 0;
    if(tex != t)
    {
        xtraverts += gle::end();
        tex = t;
        glBindTexture(GL_TEXTURE_2D, tex->id);
    }
    float h = curfont->maxh*scale, w = (t->w*h)/float(t->h);
    textvert(x,     y    ); gle::attribf(0, 0);
    textvert(x + w, y    ); gle::attribf(1, 0);
    textvert(x + w, y + h); gle::attribf(1, 1);
    textvert(x,     y + h); gle::attribf(0, 1);
    return w;
}

static float icon_width(const char *name, float scale)
{
    if(!name && !*name) return 0;
    const char *file = name;
    if(*file == '$') file = gettexvar(++file);
    if(!*file) return 0;
    Texture *t = textureload(file, 3, true, false);
    if(!t) return 0;
    float w = (t->w*curfont->maxh*scale)/float(t->h);
    return w;
}

#define TEXTHEIGHT (FONTH*linespace)

#define TEXTCOLORIZE(h,s,q) \
{ \
    if(str[h] == 'z' && str[h+1]) \
    { \
        h++; \
        bool alt = textblinking && totalmillis%(textblinking*2) > textblinking; \
        if(s) TEXTCOLOR(h); \
        if(str[h+1]) \
        { \
            h++; \
            if(s && alt) TEXTCOLOR(h); \
        } \
    } \
    else if(str[h] == '[') \
    { \
        h++; \
        const char *start = &str[h]; \
        const char *end = strchr(start, ']'); \
        if(end) \
        { \
            if(s && end > start) { TEXTHEXCOLOR(parseint(start)); } \
            h += end-start; \
        } \
        else break; \
    } \
    else if(str[h] == '(') \
    { \
        h++; \
        const char *start = &str[h]; \
        const char *end = strchr(start, ')'); \
        if(end) \
        { \
            if(end > start) \
            { \
                string value; \
                copystring(value, start, min(size_t(end - start + 1), sizeof(value))); \
                TEXTICON(value, q, s); \
            } \
            h += end-start; \
        } \
        else break; \
    } \
    else if(str[h] == '{') \
    { \
        h++; \
        const char *start = &str[h]; \
        const char *end = strchr(start, '}'); \
        if(end) \
        { \
            if(end > start) \
            { \
                string value; \
                copystring(value, start, min(size_t(end - start + 1), sizeof(value))); \
                TEXTKEY(value, q, s); \
            } \
            h += end-start; \
        } \
        else break; \
    } \
    else if(s) TEXTCOLOR(h); \
}

#define TEXTWIDTHEST(s) \
{ \
    if(maxwidth > 0 && qx+cw > maxwidth) \
    { \
        if(qp >= 0 && qi-qp <= max(textwrapmin, int(qi*textwraplimit))) \
        { \
            wrappos = qp; \
            qx = min(qw, maxwidth); \
        } \
        else \
        { \
            wrappos = max(s ? qi : qi-1, 0); \
            qx = min(qx, maxwidth); \
        } \
        break; \
    } \
    else if(s) \
    { \
        qp = qi; \
        qw = qx; \
    } \
}

#define TEXTESTIMATE(tidx) \
{ \
    float qx = x, qw = x; \
    int qp = -1; \
    for(int qi = tidx; str[qi]; qi++) \
    { \
        int qc = uchar(str[qi]); \
        if(qc == '\t') \
        { \
            float cw = TEXTTAB(qx); \
            TEXTWIDTHEST(true); \
            qx += cw; \
        } \
        else if(qc == ' ') \
        { \
            float cw = scale*curfont->defaultw; \
            TEXTWIDTHEST(true); \
            qx += cw; \
        } \
        else if(qc == '\n') \
        { \
            break; \
        } \
        else if(qc == '\f') \
        { \
            if(str[qi+1]) \
            { \
                int fi = qi+1; \
                float fx = qx; \
                TEXTCOLORIZE(fi, false, fx); \
                float cw = fx-qx; \
                if(cw > 0) TEXTWIDTHEST(false); \
                qi++; \
                TEXTCOLORIZE(qi, false, qx); \
            } \
        } \
        else if(curfont->chars.inrange(qc-curfont->charoffset)) \
        { \
            float cw = scale*curfont->chars[qc-curfont->charoffset].advance; \
            if(cw <= 0) continue; \
            TEXTWIDTHEST(false); \
            qx += cw; \
        } \
    } \
    if((flags&TEXT_ALIGN) == TEXT_CENTERED) \
    { \
        if(tidx <= 0) maxwidth = qx; \
        else x += (maxwidth-qx)*0.5f; \
    } \
}

#define TEXTALIGN(aidx) \
{ \
    if(str[aidx]) \
    { \
        x = 0; \
        wrappos = -1; \
        if(!(flags&TEXT_NO_INDENT)) \
        { \
            if((flags&TEXT_ALIGN) == TEXT_LEFT_JUSTIFY) x += FONTTAB; \
            else if(!indents && ((flags&TEXT_ALIGN) == TEXT_RIGHT_JUSTIFY)) maxwidth -= FONTTAB; \
            indents++; \
        } \
        TEXTESTIMATE(aidx) \
        y += TEXTHEIGHT; \
    } \
}

#define TEXTWIDTH \
{ \
    if(wrappos >= 0 && i >= wrappos) \
    { \
        TEXTLINE(i); \
        TEXTALIGN(i+1); \
        continue; \
    } \
}

#define TEXTSKELETON \
    float y = 0, x = 0, scale = curfont->scale/float(curfont->defaulth)*curtextscale; \
    int i = 0, wrappos = -1, indents = 0; \
    for(i = 0; str[i]; i++) \
    { \
        if(!i) TEXTESTIMATE(i) \
        int c = uchar(str[i]); \
        TEXTINDEX(i) \
        if(c == '\t') \
        { \
            float cw = TEXTTAB(x); \
            TEXTWIDTH; \
            x += cw; \
            TEXTWHITE(i); \
        } \
        else if(c == ' ') \
        { \
            float cw = scale*curfont->defaultw; \
            TEXTWIDTH; \
            x += cw; \
            TEXTWHITE(i); \
        } \
        else if(c == '\n') \
        { \
            TEXTLINE(i); \
            TEXTALIGN(i+1); \
        } \
        else if(c == '\f') \
        { \
            if(str[i+1]) \
            { \
                i++; \
                TEXTCOLORIZE(i, true, x); \
            } \
        } \
        else if(curfont->chars.inrange(c-curfont->charoffset)) \
        { \
            float cw = scale*curfont->chars[c-curfont->charoffset].advance; \
            if(cw <= 0) continue; \
            TEXTCHAR(i); \
            TEXTWIDTH; \
        } \
    }

#define TEXTEND(csr) if(csr >= i) { do { TEXTINDEX(csr); } while(0); }

int text_visible(const char *str, float hitx, float hity, float maxwidth, int flags, float linespace)
{
    if(linespace <= 0) linespace = textlinespacing;
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret,q,s) q += icon_width(ret, scale);
    #define TEXTKEY(ret,q,s) q += key_widthf(ret);
    #define TEXTCHAR(idx) x += cw; TEXTWHITE(idx)
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
    return i;
}

//inverse of text_visible
void text_posf(const char *str, int cursor, float &cx, float &cy, float maxwidth, int flags, float linespace)
{
    if(linespace <= 0) linespace = textlinespacing;
    #define TEXTINDEX(idx) if(cursor == idx) { cx = x; cy = y; break; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret,q,s) q += icon_width(ret, scale); if(i >= cursor) break;
    #define TEXTKEY(ret,q,s) q += key_widthf(ret); if(i >= cursor) break;
    #define TEXTCHAR(idx) x += cw; if(i >= cursor) break;
    cx = cy = 0;
    TEXTSKELETON
    TEXTEND(cursor)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
    #undef TEXTCHAR
}

void text_boundsf(const char *str, float &width, float &height, float xpad, float ypad, float maxwidth, int flags, float linespace)
{
    if(linespace <= 0) linespace = textlinespacing;
    flags &= TEXT_ALIGN;
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret,q,s) q += icon_width(ret, scale);
    #define TEXTKEY(ret,q,s) q += key_widthf(ret);
    #define TEXTCHAR(idx) x += cw;
    width = height = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    if(xpad) width += xpad*2;
    if(ypad) height += ypad*2;
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
}

struct textkey
{
    char *name, *file;
    Texture *tex;
    textkey() : name(NULL), file(NULL), tex(NULL) {}
    textkey(char *n, char *f, Texture *t) : name(newstring(n)), file(newstring(f)), tex(t) {}
    ~textkey()
    {
        DELETEA(name);
        DELETEA(file);
    }
};
vector<textkey *> textkeys;

textkey *findtextkey(const char *str)
{
    loopv(textkeys) if(!strcmp(textkeys[i]->name, str)) return textkeys[i];
    string key = "textures/keys/";
    int q = strlen(key);
    concatstring(key, str);
    for(int r = strlen(key); q < r; q++) key[q] = tolower(key[q]);
    textkey *t = new textkey;
    t->name = newstring(str);
    t->file = newstring(key);
    t->tex = textureload(t->file, 0, true, false);
    if(t->tex == notexture) t->tex = NULL;
    textkeys.add(t);
    return t;
}

struct tklookup
{
    char *name;
    int type;
    bindlist blist;
    tklookup() : name(NULL), type(0) {}
    tklookup(char *n, int t) : name(newstring(n)), type(t) {}
    ~tklookup()
    {
        DELETEA(name);
    }
};
vector<tklookup *> tklookups;

tklookup *findtklookup(const char *str, int type)
{
    loopv(tklookups) if(!strcmp(tklookups[i]->name, str) && tklookups[i]->type == type) return tklookups[i];
    tklookup *t = new tklookup;
    t->name = newstring(str);
    t->type = type;
    tklookups.add(t);
    return t;
}

static const char *gettklp(const char *str)
{
    int type = 0;
    if(isnumeric(str[0]) && str[1] == ':')
    {
        if(str[0] >= '1' && str[0] <= '9') type = str[0]-'0';
        str += 2;
    }
    tklookup *t = findtklookup(str, type);
    if(!t) return "";
    return t->blist.search(str, type, "", "", " ", " ", 5);
}

#define defformatkey(dest, key) defformatstring((dest), "\fs\fa[\fS\fs\f[%d]%s\fS\fs\fa]\fS", textkeycolour, (key))

float key_widthf(const char *str)
{
    const char *keyn = str;
    if(*str == '=') keyn = gettklp(++str);
    vector<char *> list;
    explodelist(keyn, list);
    float width = 0, scale = curfont->maxh*curfont->scale/float(curfont->defaulth)*curtextscale*textkeyimagescale;
    loopv(list)
    {
        if(i && textkeyseps) width += text_widthf(" or ");
        if(textkeyimages)
        {
            textkey *t = findtextkey(list[i]);
            if(t && t->tex)
            {
                width += (t->tex->w*scale)/float(t->tex->h);
                continue;
            }
            // fallback if not found
        }
        defformatkey(keystr, list[i]);
        width += text_widthf(keystr);
    }
    list.deletearrays();
    return width;
}

static float draw_key(Texture *&tex, const char *str, float sx, float sy, bvec4 &color)
{
    Texture *oldtex = tex;
    const char *keyn = str;
    if(*str == '=') keyn = gettklp(++str);
    vector<char *> list;
    explodelist(keyn, list);
    float width = 0, sh = curfont->maxh*curfont->scale/float(curfont->defaulth)*curtextscale, h = sh*textkeyimagescale;
    loopv(list)
    {
        if(i && textkeyseps)
        {
            if(tex != oldtex)
            {
                xtraverts += gle::end();
                tex = oldtex;
                glBindTexture(GL_TEXTURE_2D, tex->id);
            }
            draw_text(" or ", sx + width, sy, color.r, color.g, color.b, color.a, 0, -1, -1, 1);
            width += text_widthf(" or ");
        }
        if(textkeyimages)
        {
            textkey *t = findtextkey(list[i]);
            if(t && t->tex)
            {
                if(tex != t->tex)
                {
                    xtraverts += gle::end();
                    tex = t->tex;
                    glBindTexture(GL_TEXTURE_2D, tex->id);
                }
                float w = (tex->w*h)/float(tex->h), oh = h-sh, oy = sy-oh*0.5f;
                textvert(sx + width,     oy    ); gle::attribf(0, 0);
                textvert(sx + width + w, oy    ); gle::attribf(1, 0);
                textvert(sx + width + w, oy + h); gle::attribf(1, 1);
                textvert(sx + width,     oy + h); gle::attribf(0, 1);
                width += w;
                continue;
            }
            // fallback if not found
        }
        if(tex != oldtex)
        {
            xtraverts += gle::end();
            tex = oldtex;
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }
        defformatkey(keystr, list[i]);
        draw_text(keystr, sx + width, sy, color.r, color.g, color.b, color.a, 0, -1, -1, 1);
        width += text_widthf(keystr);
    }
    list.deletearrays();
    return width;
}

Shader *textshader = NULL;

float draw_text(const char *str, float rleft, float rtop, int r, int g, int b, int a, int flags, int cursor, float maxwidth, float linespace)
{
    if(linespace <= 0) linespace = textlinespacing;
    #define TEXTINDEX(idx) \
        if(cursor >= 0 && idx == cursor) \
        { \
            cx = x; \
            cy = y; \
            if(!hasfade && usecolor && textfaded) \
            { \
                text_color('e', colorstack, sizeof(colorstack), colorpos, color, r, g, b, fade, flags); \
                hasfade = true; \
            } \
        }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) ly += TEXTHEIGHT;
    #define TEXTCOLOR(idx) if(usecolor) { text_color(str[idx], colorstack, sizeof(colorstack), colorpos, color, r, g, b, fade, flags); }
    #define TEXTHEXCOLOR(ret) \
        if(usecolor) \
        { \
            int alpha = colorstack[colorpos].a; \
            color = TVECA(ret, alpha); \
            colorstack[colorpos] = color; \
            xtraverts += gle::end(); \
            gle::color(color); \
        }
    #define TEXTICON(ret,q,s) q += s ? draw_icon(tex, ret, left+x, top+y, scale) : icon_width(ret, scale);
    #define TEXTKEY(ret,q,s) q += s ? draw_key(tex, ret, left+x, top+y, color) : key_widthf(ret);
    #define TEXTCHAR(idx) { draw_char(tex, c, left+x, top+y, scale); x += cw; }
    int fade = a;
    bool usecolor = true, hasfade = false;
    if(fade < 0) { usecolor = false; fade = -a; }
    int colorpos = 1, ly = 0, left = rleft, top = rtop;
    float cx = -FONTW, cy = 0;
    if(r < 0) r = (colourwhite>>16)&0xFF;
    if(g < 0) g = (colourwhite>>8)&0xFF;
    if(b < 0) b = colourwhite&0xFF;
    bvec4 colorstack[16], color = TVECX(r, g, b, fade);
    loopi(16) colorstack[i] = color;
    Texture *tex = curfont->texs[0];
    (textshader ? textshader : hudtextshader)->set();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    gle::color(color);
    gle::defvertex(textmatrix ? 3 : 2);
    gle::deftexcoord0();
    gle::begin(GL_QUADS);
    TEXTSKELETON
    TEXTEND(cursor)
    xtraverts += gle::end();
    if(cursor >= 0)
    {
        gle::colorub(255, 255, 255, int(clamp(1.f-(float(totalmillis%500)/500.f), 0.5f, 1.f)*255));
        draw_char(tex, '_', left+cx, top+cy, scale);
        xtraverts += gle::end();
    }
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTCHAR
    return ly + FONTH;
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        loopv(f.texs) if(!reloadtexture(f.texs[i])) fatal("Failed to reload font texture");
    );
}

float draw_textf(const char *fstr, float left, float top, float xpad, float ypad, int r, int g, int b, int a, int flags, int cursor, float maxwidth, int linespace, ...)
{
    defvformathugestring(str, linespace, fstr);
    float linespacef = linespace <= 0 ? textlinespacing : linespace, width = 0, height = 0;
    text_boundsf(str, width, height, xpad, ypad, maxwidth, flags, linespacef);
    if(flags&TEXT_ALIGN) switch(flags&TEXT_ALIGN)
    {
        case TEXT_CENTERED: left -= width*0.5f-xpad; break;
        case TEXT_RIGHT_JUSTIFY: left -= width-xpad*0.5f; break;
        default: break;
    }
    if(flags&TEXT_BALLOON) top -= height*0.5f;
    else if(flags&TEXT_UPWARD) top -= height;
    if(xpad) left += xpad;
    if(ypad) top += ypad;
    if(flags&TEXT_SHADOW) draw_text(str, left+2, top+2, 0, 0, 0, a, flags, cursor, maxwidth, linespacef);
    draw_text(str, left, top, r, g, b, a, flags, cursor, maxwidth, linespacef);
    return height;
}
