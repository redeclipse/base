#include "engine.h"

VARF(IDF_PERSIST, textsupersample, 0, 1, 2, initwarning("Text Supersampling", INIT_LOAD, CHANGE_SHADERS));

VAR(IDF_PERSIST, textblinking, 0, 250, VAR_MAX);
float curtextscale = 1;
FVARF(IDF_PERSIST, textscale, FVAR_NONZERO, 1, FVAR_MAX, curtextscale = textscale);
FVAR(IDF_PERSIST, textlinespacing, FVAR_NONZERO, 1, FVAR_MAX);
VAR(IDF_PERSIST, textfaded, 0, 1, 1);
VAR(IDF_PERSIST, textwrapmin, 0, 10, VAR_MAX);
FVAR(IDF_PERSIST, textwraplimit, 0, 0.3f, 1);
FVAR(IDF_PERSIST, textspacescale, 0, 0.5f, 10);

FVAR(IDF_PERSIST, textimagescale, 0, 0.8f, FVAR_MAX);
VAR(IDF_PERSIST, textkeyimages, 0, 1, 1);
FVAR(IDF_PERSIST, textkeyimagescale, 0, 0.8f, FVAR_MAX);
SVAR(IDF_PERSIST, textkeyprefix, "<invert>textures/keys/");
VAR(IDF_PERSIST, textkeyseps, 0, 1, 1);
VAR(IDF_PERSIST|IDF_HEX, textkeycolour, 0, 0x00FFFF, 0xFFFFFF);
SVAR(IDF_PERSIST, textfontdef, "exo/clear");
SVAR(IDF_PERSIST, textfontbold, "exo/clear/bold");
SVAR(IDF_PERSIST, textfontitalic, "exo/clear/italic");
SVAR(IDF_PERSIST, textfontbolditalic, "exo/clear/bold/italic");
SVAR(IDF_PERSIST, textfontmono, "mono/clear");
SVAR(IDF_PERSIST, textfontmonobold, "mono/clear/bold");
SVAR(IDF_PERSIST, textfontmonoitalic, "mono/clear/italic");
SVAR(IDF_PERSIST, textfontmonobolditalic, "mono/clear/bold/italic");
SVAR(IDF_PERSIST, textfontlogo, "exo/clear");
SVAR(IDF_PERSIST, textfontoutline, "exo/outline");
SVAR(IDF_PERSIST, textfontoutlinebold, "exo/outline/bold");
SVAR(IDF_PERSIST, textfontoutlineitalic, "exo/outline/italic");
SVAR(IDF_PERSIST, textfontoutlinebolditalic, "exo/outline/bold/italic");
SVAR(IDF_PERSIST, textfontoutlinemono, "mono/outline");
SVAR(IDF_PERSIST, textfontoutlinemonobold, "mono/outline/bold");
SVAR(IDF_PERSIST, textfontoutlinemonoitalic, "mono/outline/italic");
SVAR(IDF_PERSIST, textfontoutlinemonobolditalic, "mono/outline/bold/italic");
SVAR(IDF_PERSIST, textfonttool, "tess");
VAR(IDF_PERSIST, textparsed, 0, 1, 1);

SVAR(0, fontloading, "");
static hashnameset<font> fonts;
static hashset<const char *> missingfonts;
static font *fontdef = NULL;
static int fontdeftex = 0;

vector<font *> fontstack;
font *curfont = NULL;
int curfontpass = 0;
bool wantfontpass = false;

void fontscale(float *scale)
{
    if(!fontdef) return;

    fontdef->scale = *scale > 0 ? *scale : fontdef->defaulth;
    fontdef->mw = fontdef->maxw*fontdef->scale/fontdef->defaultw;
    fontdef->mh = fontdef->maxh*fontdef->scale/fontdef->defaulth;
}

void newfont(char *name, char *tex, float *defaultw, float *defaulth, float *scale)
{
    const char *loading = fontloading && *fontloading ? fontloading : name;
    font *f = &fonts[loading];
    if(!f->name) f->name = newstring(name);
    f->texs.shrink(0);
    f->texs.add(textureload(tex, 3));
    f->chars.shrink(0);

    f->charoffset = '!';
    f->mw = f->maxw = f->defaultw = *defaultw;
    f->mh = f->maxh = f->defaulth = *defaulth;
    f->bordermin = 0.49f;
    f->bordermax = 0.5f;
    f->outlinemin = -1;
    f->outlinemax = 0;
    f->mono = false;

    if(const char **m = missingfonts.access(loading))
    {
        const char *old = *m;
        missingfonts.remove(loading);
        delete[] old;
    }

    fontdef = f;
    fontdeftex = 0;

    fontscale(scale);
}

void fontborder(float *bordermin, float *bordermax)
{
    if(!fontdef) return;

    fontdef->bordermin = *bordermin;
    fontdef->bordermax = max(*bordermax, *bordermin+0.01f);
}

void fontoutline(float *outlinemin, float *outlinemax)
{
    if(!fontdef) return;

    fontdef->outlinemin = min(*outlinemin, *outlinemax-0.01f);
    fontdef->outlinemax = *outlinemax;
}

void fontmono(int *val)
{
    if(!fontdef) return;

    fontdef->mono = *val != 0;
}

void fontoffset(char *c)
{
    if(!fontdef) return;

    fontdef->charoffset = c[0];
}

void fonttex(char *s)
{
    if(!fontdef) return;

    Texture *t = textureload(s, 3);
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
    if(c.offsetx+c.w > fontdef->maxw)
    {
        fontdef->maxw = c.offsetx+c.w;
        fontdef->mw = fontdef->maxw*fontdef->scale/fontdef->defaultw;
    }
    if(c.offsety+c.h > fontdef->maxh)
    {
        fontdef->maxh = c.offsety+c.h;
        fontdef->mh = fontdef->maxh*fontdef->scale/fontdef->defaulth;
    }
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

COMMANDN(0, font, newfont, "ssfff");
COMMAND(0, fontborder, "ff");
COMMAND(0, fontoutline, "ff");
COMMAND(0, fontmono, "i");
COMMAND(0, fontoffset, "s");
COMMAND(0, fontscale, "f");
COMMAND(0, fonttex, "s");
COMMAND(0, fontchar, "fffffff");
COMMAND(0, fontskip, "i");

static bool ismonofontname(const char *name)
{
    if(!name || !*name) return false;
    static char * const *monos[8] = {
        &textfontmono, &textfontmonobold, &textfontmonoitalic, &textfontmonobolditalic,
        &textfontoutlinemono, &textfontoutlinemonobold, &textfontoutlinemonoitalic, &textfontoutlinemonobolditalic
    };
    loopi(8) if(*monos[i] && **monos[i] && !strcmp(name, *monos[i])) return true;
    return !strncmp(name, "mono/", 5);
}

static bool isoutlinefont(font *f)
{
    if(!f || !f->name) return false;
    static char * const *outlines[8] = {
        &textfontoutline, &textfontoutlinebold, &textfontoutlineitalic, &textfontoutlinebolditalic,
        &textfontoutlinemono, &textfontoutlinemonobold, &textfontoutlinemonoitalic, &textfontoutlinemonobolditalic
    };
    loopi(8) if(*outlines[i] && **outlines[i] && !strcmp(f->name, *outlines[i])) return true;
    return false;
}

font *loadfont(const char *name)
{
    if(!name || !*name) return NULL;
    font *f = fonts.access(name);
    if(!f && !missingfonts.access(name) && sandboxpath(name))
    {
        setsvar("fontloading", name);
        defformatstring(n, "fonts/%s/package.cfg", name);
        bool loaded = false;
        LIFTMAP(loaded = execfile(n, false));
        if(loaded) f = fonts.access(name);
        if(f)
        {
            if(!f->mono && ismonofontname(name)) f->mono = true;
        }
        else missingfonts.add(newstring(name));
        setsvar("fontloading", "");
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
    d->bordermin = s->bordermin;
    d->bordermax = s->bordermax;
    d->outlinemin = s->outlinemin;
    d->outlinemax = s->outlinemax;
    d->mono = s->mono;

    if(const char **m = missingfonts.access(dst))
    {
        const char *old = *m;
        missingfonts.remove(dst);
        delete[] old;
    }

    fontdef = d;
    fontdeftex = d->texs.length()-1;
}

COMMAND(0, fontalias, "ss");

font *findfont(const char *name)
{
    if(!name || !*name) return NULL;
    return fonts.access(name);
}

font *findstylefont(int style, font *base)
{
    if(!base) base = curfont;
    style &= TEXT_STYLE;
    if(!style) return base;
    bool outline = isoutlinefont(base);
    static char * const *clearfonts[8] = {
        &textfontdef, &textfontbold, &textfontitalic, &textfontbolditalic,
        &textfontmono, &textfontmonobold, &textfontmonoitalic, &textfontmonobolditalic
    };
    static char * const *outlinefonts[8] = {
        &textfontoutline, &textfontoutlinebold, &textfontoutlineitalic, &textfontoutlinebolditalic,
        &textfontoutlinemono, &textfontoutlinemonobold, &textfontoutlinemonoitalic, &textfontoutlinemonobolditalic
    };
    char * const * const *table = outline ? outlinefonts : clearfonts;
    int idx = ((style&TEXT_BOLD) ? 1 : 0) | ((style&TEXT_ITALIC) ? 2 : 0) | ((style&TEXT_MONO) ? 4 : 0);
    font *f = loadfont(*table[idx]);
    if(!f && (idx&3) == 3)
    {
        f = loadfont(*table[idx&~2]);
        if(!f) f = loadfont(*table[idx&~1]);
    }
    if(!f && (idx&4)) f = loadfont(*table[4]);
    return f ? f : base;
}

static font *resolvefonttag(const char *name, font *base)
{
    if(!name || !*name) return NULL;
    if(!strcmp(name, "bold")) return findstylefont(TEXT_BOLD, base);
    if(!strcmp(name, "italic")) return findstylefont(TEXT_ITALIC, base);
    if(!strcmp(name, "bolditalic") || !strcmp(name, "italicbold")) return findstylefont(TEXT_BOLD|TEXT_ITALIC, base);
    if(!strcmp(name, "mono")) return findstylefont(TEXT_MONO, base);
    if(!strcmp(name, "monobold") || !strcmp(name, "boldmono")) return findstylefont(TEXT_MONO|TEXT_BOLD, base);
    if(!strcmp(name, "monoitalic") || !strcmp(name, "italicmono")) return findstylefont(TEXT_MONO|TEXT_ITALIC, base);
    if(!strcmp(name, "monobolditalic") || !strcmp(name, "bolditalicmono")) return findstylefont(TEXT_MONO|TEXT_BOLD|TEXT_ITALIC, base);
    return loadfont(name);
}

static inline float text_spacew(font *f, float scale, int style)
{
    if((style&TEXT_MONO) || f->mono)
    {
        int mc = 'M' - f->charoffset;
        if(f->chars.inrange(mc) && f->chars[mc].advance > 0)
            return scale*f->chars[mc].advance;
        return scale*f->defaultw;
    }
    return scale*f->defaultw*textspacescale;
}

bool setfont(font *id)
{
    if(!id) return false;
    curfont = id;
    return true;
}

bool setfont(const char *name)
{
    return setfont(loadfont(name ? name : textfontdef));
}

bool pushfont(font *id)
{
    if(!id) return false;

    if(fontstack.empty() && curfont)
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
    return pushfont(loadfont(name ? name : textfontdef));
}

bool popfont(int num)
{
    if(num == -1) num = fontstack.length();
    loopi(num)
    {
        if(fontstack.empty()) break;
        fontstack.pop();
    }
    if(!fontstack.empty())
    {
        curfont = fontstack.last();
        return true;
    }
    return setfont(textfontdef);
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

#define TEXTTAB(x) (max((int((x) / FONTTAB) + 1.0f) * FONTTAB, (x) + FONTW) - (x))

void tabify(const char *str, int *numtabs)
{
    float tw = max(*numtabs, 0)* FONTTAB - 1;
    int tabs = 0;
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
    if(!curfontpass && curfont->texs.inrange(info.tex))
    {
        if(tex != curfont->texs[info.tex])
        {
            xtraverts += gle::end();
            tex = curfont->texs[info.tex];
            settexture(tex);
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
    }
    return scale*info.advance;
}

#define TVECR(ci, ca) (bvec4::fromcolor(ci).alpha(ca))
#define TVECA(ci, ca) (flags&TEXT_MODCOL ? (TVECR(ci, ca).muld(r, g, b, 255.f)) : (TVECR(ci, ca)))
#define TVECX(cr, cg, cb, ca) (bvec4(cr, cg, cb, 255).alpha(ca))

static void text_color(char c, bvec4 *stack, int size, int &sp, bvec4 &color, int r, int g, int b, int a, int flags)
{
    bvec4 oldcolor = color;
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
        case 'h': case 'H': stack[sp] = color = TVECA(c == 'H' ? colourdarkchartreuse : colourchartreuse, alpha); break;
        case 'e': case 'E': (stack[sp] = color = stack[sp]).alpha(c != 'E' ? a/2 : a/4); break;
        case 'u': case 'Z': stack[sp] = color = TVECX(r, g, b, a); break; // default colour
        case 's': // save
        {
            if(sp < size-1) stack[sp++] = color;
            return;
        }
        case 'S': // restore
        {
            if(sp > 0) sp--;
            color = stack[sp];
            break;
        }
        default: color = stack[sp]; break; // everything else
    }
    if(color != oldcolor)
    {
        xtraverts += gle::end();
        gle::color(color);
    }
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

static float draw_icon(Texture *&tex, const char *name, float x, float y)
{
    if(!name && !*name) return 0;
    const char *file = name;
    if(*file == '$') file = gettexvar(++file);
    if(!*file) return 0;
    Texture *t = textureload(file, 3, true, false);
    if(!t) return 0;
    float sh = curfont->scale*curtextscale, h = sh*textimagescale, w = (t->w*h)/float(t->h);
    if(curfontpass)
    {
        if(tex != t)
        {
            xtraverts += gle::end();
            tex = t;
            settexture(tex);
        }
        float oh = h-sh, oy = y-oh*0.5f;
        textvert(x,     oy    ); gle::attribf(0, 0);
        textvert(x + w, oy    ); gle::attribf(1, 0);
        textvert(x + w, oy + h); gle::attribf(1, 1);
        textvert(x,     oy + h); gle::attribf(0, 1);
    }
    else wantfontpass = true;
    return w;
}

static float icon_width(const char *name)
{
    if(!name && !*name) return 0;
    const char *file = name;
    if(*file == '$') file = gettexvar(++file);
    if(!*file) return 0;
    Texture *t = textureload(file, 3, true, false);
    if(!t) return 0;
    float w = (t->w*curfont->scale*curtextscale*textimagescale)/float(t->h);
    return w;
}

#define TEXTHEIGHT (FONTH*linespace)

struct fontguard
{
    font *orig;
    int depth;
    fontguard() : orig(curfont), depth(fontstack.length()) {}
    ~fontguard()
    {
        if(fontstack.length() > depth) popfont(fontstack.length() - depth);
        curfont = orig;
    }
};

enum
{
    MD_CODE         = 1<<0,
    MD_BOLD_STAR    = 1<<1,
    MD_ITALIC_STAR  = 1<<2,
    MD_BOLD_UNDER   = 1<<3,
    MD_ITALIC_UNDER = 1<<4
};

static inline int text_effstyle(int style, int mdstate)
{
    if(!mdstate) return style;
    return style
        | ((mdstate & (MD_BOLD_STAR|MD_BOLD_UNDER)) ? TEXT_BOLD : 0)
        | ((mdstate & (MD_ITALIC_STAR|MD_ITALIC_UNDER)) ? TEXT_ITALIC : 0)
        | ((mdstate & MD_CODE) ? TEXT_MONO : 0);
}

static bool text_md_has_closer(const char *str, int start, char delim, int count, bool boundary)
{
    bool incode = false;
    for(int j = start; str[j] && str[j] != '\n'; j++)
    {
        char ch = str[j];
        if(ch == '\f')
        {
            if(!str[j+1]) break;
            j++;
            if(str[j] == 'z') { if(str[j+1]) j++; if(str[j+1]) j++; }
            else if(str[j] == '[' || str[j] == '(' || str[j] == '{' || str[j] == '<')
            {
                char close = str[j] == '[' ? ']' : (str[j] == '(' ? ')' : (str[j] == '{' ? '}' : '>'));
                const char *end = strchr(&str[j+1], close);
                if(!end) break;
                j = int(end - str);
            }
            continue;
        }
        if(delim == '`')
        {
            if(ch == '`') return j > start;
            continue;
        }
        if(incode)
        {
            if(ch == '`') incode = false;
            continue;
        }
        if(ch == '\\' && (str[j+1] == '*' || str[j+1] == '_' || str[j+1] == '`'))
        {
            j++;
            continue;
        }
        if(ch == '`')
        {
            const char *end = strchr(&str[j+1], '`');
            const char *nl = strchr(&str[j+1], '\n');
            if(end && (!nl || end < nl) && end > &str[j+1]) { incode = true; continue; }
            continue;
        }
        if(ch == delim)
        {
            int run = 1;
            while(str[j+run] == delim) run++;
            if(run >= count && j > start && !isspace(uchar(str[j-1])))
            {
                if(!boundary || !str[j+count] || !isalnum(uchar(str[j+count])))
                    return true;
            }
            j += run - 1;
        }
    }
    return false;
}

static bool text_md_step(const char *str, int &i, int &mdstate)
{
    char ch = str[i];
    if(mdstate & MD_CODE)
    {
        if(ch == '`') { mdstate &= ~MD_CODE; return true; }
        return false;
    }
    if(ch == '\\')
    {
        char next = str[i+1];
        if(next == '*' || next == '_' || next == '`')
        {
            i++;
            return false;
        }
        return false;
    }
    if(ch == '`')
    {
        if(text_md_has_closer(str, i+1, '`', 1, false))
        {
            mdstate |= MD_CODE;
            return true;
        }
        return false;
    }
    if(ch == '*' || ch == '_')
    {
        bool under = (ch == '_');
        int boldbit = under ? MD_BOLD_UNDER : MD_BOLD_STAR;
        int italbit = under ? MD_ITALIC_UNDER : MD_ITALIC_STAR;
        int run = 1;
        while(run < 3 && str[i+run] == ch) run++;
        bool canclose = i > 0 && !isspace(uchar(str[i-1]));
        if(canclose)
        {
            if(run >= 3 && (mdstate & boldbit) && (mdstate & italbit) && (!under || !str[i+3] || !isalnum(uchar(str[i+3]))))
            {
                mdstate &= ~(boldbit|italbit);
                i += 2;
                return true;
            }
            if(run >= 2 && (mdstate & boldbit) && (!under || !str[i+2] || !isalnum(uchar(str[i+2]))))
            {
                mdstate &= ~boldbit;
                i += 1;
                return true;
            }
            if((mdstate & italbit) && (!under || !str[i+1] || !isalnum(uchar(str[i+1]))))
            {
                mdstate &= ~italbit;
                return true;
            }
        }
        bool canopen = (!under || i == 0 || !isalnum(uchar(str[i-1])));
        if(canopen)
        {
            if(run >= 3 && !(mdstate & (boldbit|italbit)) && str[i+3] && !isspace(uchar(str[i+3])) && (!under || str[i+3] != '_'))
            {
                if(text_md_has_closer(str, i+3, ch, 3, under))
                {
                    mdstate |= boldbit|italbit;
                    i += 2;
                    return true;
                }
            }
            if(run >= 2 && !(mdstate & boldbit) && str[i+2] && !isspace(uchar(str[i+2])) && (!under || str[i+2] != '_'))
            {
                if(text_md_has_closer(str, i+2, ch, 2, under))
                {
                    mdstate |= boldbit;
                    i += 1;
                    return true;
                }
            }
            if(!(mdstate & italbit) && str[i+1] && !isspace(uchar(str[i+1])) && (!under || str[i+1] != '_'))
            {
                if(text_md_has_closer(str, i+1, ch, 1, under))
                {
                    mdstate |= italbit;
                    return true;
                }
            }
        }
    }
    return false;
}

static inline bool text_has_md(const char *str)
{
    for(const char *p = str; *p; p++)
        if(*p == '*' || *p == '_' || *p == '`' || *p == '\\') return true;
    return false;
}

#define TEXTAPPLYSTYLE() \
{ \
    int eff = text_effstyle(style, mdstate); \
    font *target = eff ? findstylefont(eff, basefont) : basefont; \
    if(target && target != curfont) \
    { \
        curfont = target; \
        scale = curfont->scale/curfont->defaulth*curtextscale; \
        TEXTSYNCFONT(); \
    } \
}

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
    else if(str[h] == '<') \
    { \
        h++; \
        const char *start = &str[h]; \
        const char *end = strchr(start, '>'); \
        if(end) \
        { \
            if(end > start) \
            { \
                string value; \
                copystring(value, start, min(size_t(end - start + 1), sizeof(value))); \
                TEXTFONT(value); \
            } \
            h += end-start; \
        } \
        else break; \
    } \
    else if(str[h] == '*') \
    { \
        style ^= TEXT_BOLD; \
        TEXTAPPLYSTYLE(); \
    } \
    else if(str[h] == '/') \
    { \
        style ^= TEXT_ITALIC; \
        TEXTAPPLYSTYLE(); \
    } \
    else if(str[h] == '`') \
    { \
        style ^= TEXT_MONO; \
        TEXTAPPLYSTYLE(); \
    } \
    else \
    { \
        if(str[h] == 's') \
        { \
            if(stylepos < 15) stylestack[stylepos++] = style; \
        } \
        else if(str[h] == 'S') \
        { \
            if(stylepos > 0) stylepos--; \
            style = stylestack[stylepos]; \
            TEXTAPPLYSTYLE(); \
        } \
        if(s) TEXTCOLOR(h); \
    } \
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
    bool prev_est = textest; \
    textest = true; \
    font *est_curfont = curfont, *est_basefont = basefont; \
    float est_scale = scale; \
    int est_style = style, est_mdstate = mdstate, est_stylepos = stylepos, est_fdepth = fontstack.length(); \
    int est_stylestack[16]; \
    memcpy(est_stylestack, stylestack, sizeof(stylestack)); \
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
            float cw = text_spacew(curfont, scale, text_effstyle(style, mdstate)); \
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
                qi = fi; \
                qx = fx; \
            } \
        } \
        else \
        { \
            if(doparse && (qc == '*' || qc == '_' || qc == '`' || qc == '\\')) \
            { \
                int prevmd = mdstate; \
                if(text_md_step(str, qi, mdstate)) \
                { \
                    if(mdstate != prevmd) TEXTAPPLYSTYLE(); \
                    continue; \
                } \
                qc = uchar(str[qi]); \
            } \
            if(curfont->chars.inrange(qc-curfont->charoffset)) \
            { \
                float cw = scale*curfont->chars[qc-curfont->charoffset].advance; \
                if(cw <= 0) continue; \
                TEXTWIDTHEST(false); \
                qx += cw; \
            } \
        } \
    } \
    if(fontstack.length() > est_fdepth) popfont(fontstack.length() - est_fdepth); \
    curfont = est_curfont; \
    basefont = est_basefont; \
    scale = est_scale; \
    style = est_style; \
    mdstate = est_mdstate; \
    stylepos = est_stylepos; \
    memcpy(stylestack, est_stylestack, sizeof(stylestack)); \
    textest = prev_est; \
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

#define TEXTFONTBASE(ret) \
{ \
    if(!strcmp(ret, "~")) \
    { \
        if(fontstack.length() > oldfontdepth) \
        { \
            popfont(); \
            basefont = curfont; \
        } \
    } \
    else if(font *nf = resolvefonttag(ret, basefont)) \
    { \
        if(pushfont(nf)) basefont = curfont; \
    } \
    scale = curfont->scale/curfont->defaulth*curtextscale; \
    TEXTAPPLYSTYLE(); \
    TEXTSYNCFONT(); \
}

#define TEXTSKELETON \
    font *basefont = curfont; \
    int style = flags & TEXT_STYLE, mdstate = 0, stylepos = 1, stylestack[16]; \
    loopi(16) stylestack[i] = style; \
    bool textest = false; \
    bool doparse = textparsed && !(flags & TEXT_NO_PARSE) && TEXTPARSEOK && text_has_md(str); \
    float y = 0, x = 0, scale = curfont->scale/curfont->defaulth*curtextscale; \
    TEXTAPPLYSTYLE(); \
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
            float cw = text_spacew(curfont, scale, text_effstyle(style, mdstate)); \
            TEXTWIDTH; \
            x += cw; \
            TEXTWHITE(i); \
        } \
        else if(c == '\n') \
        { \
            if(mdstate) { mdstate = 0; TEXTAPPLYSTYLE(); } \
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
        else \
        { \
            if(doparse && (c == '*' || c == '_' || c == '`' || c == '\\')) \
            { \
                int prevmd = mdstate; \
                if(text_md_step(str, i, mdstate)) \
                { \
                    if(mdstate != prevmd) TEXTAPPLYSTYLE(); \
                    continue; \
                } \
                c = uchar(str[i]); \
            } \
            if(curfont->chars.inrange(c-curfont->charoffset)) \
            { \
                TEXTCHAR(i); \
                TEXTWIDTH; \
            } \
        } \
    }

#define TEXTEND(csr) if(csr >= i) { do { TEXTINDEX(csr); } while(0); }

int text_visible(const char *str, float hitx, float hity, float maxwidth, int flags, float linespace)
{
    fontguard fg;
    int oldfontdepth = fg.depth;
    if(linespace <= 0) linespace = textlinespacing;
    #define TEXTPARSEOK true
    #define TEXTSYNCFONT()
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTFONT(ret) TEXTFONTBASE(ret)
    #define TEXTICON(ret,q,s) q += icon_width(ret);
    #define TEXTKEY(ret,q,s) q += key_widthf(ret);
    #define TEXTCHAR(idx) x += scale*curfont->chars[c-curfont->charoffset].advance; TEXTWHITE(idx)
    TEXTSKELETON
    #undef TEXTPARSEOK
    #undef TEXTSYNCFONT
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTFONT
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
    return i;
}

// inverse of text_visible
void text_posf(const char *str, int cursor, float &cx, float &cy, float maxwidth, int flags, float linespace)
{
    fontguard fg;
    int oldfontdepth = fg.depth;
    if(linespace <= 0) linespace = textlinespacing;
    #define TEXTPARSEOK true
    #define TEXTSYNCFONT()
    #define TEXTINDEX(idx) if(cursor == idx) { cx = x; cy = y; break; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTFONT(ret) TEXTFONTBASE(ret)
    #define TEXTICON(ret,q,s) q += icon_width(ret); if(i >= cursor) break;
    #define TEXTKEY(ret,q,s) q += key_widthf(ret); if(i >= cursor) break;
    #define TEXTCHAR(idx) x += scale*curfont->chars[c-curfont->charoffset].advance; if(i >= cursor) break;
    cx = cy = 0;
    TEXTSKELETON
    TEXTEND(cursor)
    #undef TEXTPARSEOK
    #undef TEXTSYNCFONT
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTFONT
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
}

void text_boundsf(const char *str, float &width, float &height, float xpad, float ypad, float maxwidth, int flags, float linespace)
{
    fontguard fg;
    int oldfontdepth = fg.depth;
    if(linespace <= 0) linespace = textlinespacing;
    flags &= TEXT_ALIGN|TEXT_STYLE|TEXT_NO_PARSE;
    #define TEXTPARSEOK true
    #define TEXTSYNCFONT()
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTFONT(ret) TEXTFONTBASE(ret)
    #define TEXTICON(ret,q,s) q += icon_width(ret);
    #define TEXTKEY(ret,q,s) q += key_widthf(ret);
    #define TEXTCHAR(idx) x += scale*curfont->chars[c-curfont->charoffset].advance;
    width = height = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    if(xpad) width += xpad*2;
    if(ypad) height += ypad*2;
    #undef TEXTPARSEOK
    #undef TEXTSYNCFONT
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTFONT
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
    static string key;
    copystring(key, textkeyprefix);
    int q = strlen(key);
    concatstring(key, str);
    for(int r = strlen(key); q < r; q++) key[q] = tolower(key[q]);
    textkey *t = new textkey;
    t->name = newstring(str);
    t->file = newstring(key);
    t->tex = textureload(t->file, 3, true, false);
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
    return t->blist.search(str, type, 0, "", "", " ", " ", 5);
}

#define defformatkey(dest, key) defformatstring(dest, "\fs\fa[\fS\fs\f[%d]%s\fS\fs\fa]\fS", textkeycolour, (key))

float key_widthf(const char *str)
{
    const char *keyn = str;
    if(*str == '=') keyn = gettklp(++str);
    vector<char *> list;
    explodelist(keyn, list);
    float width = 0, scale = curfont->scale*curtextscale*textkeyimagescale;
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
        width += text_widthf(keystr, 0, 0, TEXT_NO_PARSE);
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
    float width = 0;
    loopv(list)
    {
        if(i && textkeyseps)
        {
            if(!curfontpass)
            {
                if(tex != oldtex)
                {
                    xtraverts += gle::end();
                    tex = oldtex;
                    settexture(tex);
                }
                draw_text(" or ", sx + width, sy, color.r, color.g, color.b, color.a, 0, -1, -1, 1);
            }
            width += text_widthf(" or ");
        }
        if(textkeyimages)
        {
            textkey *t = findtextkey(list[i]);
            if(t && t->tex)
            {
                float sh = curfont->scale*curtextscale, h = sh*textkeyimagescale, w = (t->tex->w*h)/float(t->tex->h);
                if(curfontpass)
                {
                    if(tex != t->tex)
                    {
                        xtraverts += gle::end();
                        tex = t->tex;
                        settexture(tex);
                    }
                    float oh = h-sh, oy = sy-oh*0.5f;
                    textvert(sx + width,     oy    ); gle::attribf(0, 0);
                    textvert(sx + width + w, oy    ); gle::attribf(1, 0);
                    textvert(sx + width + w, oy + h); gle::attribf(1, 1);
                    textvert(sx + width,     oy + h); gle::attribf(0, 1);
                }
                else wantfontpass = true;
                width += w;
                continue;
            }
            // fallback if not found
        }
        defformatkey(keystr, list[i]);
        if(!curfontpass)
        {
            if(tex != oldtex)
            {
                xtraverts += gle::end();
                tex = oldtex;
                settexture(tex);
            }
            draw_text(keystr, sx + width, sy, color.r, color.g, color.b, color.a, TEXT_NO_PARSE, -1, -1, 1);
        }
        width += text_widthf(keystr, 0, 0, TEXT_NO_PARSE);
    }
    list.deletearrays();
    return width;
}

Shader *textshader = NULL;

float draw_text(const char *str, float rleft, float rtop, int r, int g, int b, int a, int flags, int cursor, float maxwidth, float linespace)
{
    if(curfont->texs.empty())
    {
        float lw = 0, lh = 0;
        text_boundsf(str, lw, lh, 0.f, 0.f, maxwidth, flags, linespace);
        return lh;
    }
    fontguard fg;
    int oldfontdepth = fg.depth;
    if(linespace <= 0) linespace = textlinespacing;
    font *lastfont = curfont;
    #define TEXTPARSEOK (cursor < 0)
    #define TEXTSYNCFONT() \
        if(!textest && !curfontpass && curfont != lastfont) \
        { \
            if(!curfont->texs.empty()) \
            { \
                xtraverts += gle::end(); \
                LOCALPARAMF(textparams, curfont->bordermin, curfont->bordermax, curfont->outlinemin, curfont->outlinemax); \
                tex = curfont->texs[0]; \
                settexture(tex); \
            } \
            lastfont = curfont; \
        }
    #define TEXTINDEX(idx) \
        if(cursor >= 0 && idx == cursor) \
        { \
            cx = x; \
            cy = y; \
            if(!hasfade && usecolor && textfaded) \
            { \
                text_color('e', colorstack, 16, colorpos, color, r, g, b, fade, flags); \
                hasfade = true; \
            } \
        }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) ly += TEXTHEIGHT;
    #define TEXTCOLOR(idx) if(usecolor) { text_color(str[idx], colorstack, 16, colorpos, color, r, g, b, fade, flags); }
    #define TEXTHEXCOLOR(ret) \
        if(usecolor) \
        { \
            int alpha = colorstack[colorpos].a; \
            color = TVECA(getpulsehexcol(ret), alpha); \
            colorstack[colorpos] = color; \
            xtraverts += gle::end(); \
            gle::color(color); \
        }
    #define TEXTFONT(ret) TEXTFONTBASE(ret)
    #define TEXTICON(ret,q,s) q += s ? draw_icon(tex, ret, left+x, top+y) : icon_width(ret);
    #define TEXTKEY(ret,q,s) q += s ? draw_key(tex, ret, left+x, top+y, color) : key_widthf(ret);
    #define TEXTCHAR(idx) { x += draw_char(tex, c, left+x, top+y, scale); }
    int fade = a;
    bool usecolor = true, hasfade = false;
    if(fade < 0) { usecolor = false; fade = -a; }
    int colorpos = 1, ly = 0;
    float left = rleft, top = rtop, cx = -FONTW, cy = 0;
    if(r < 0) r = (colourwhite>>16)&0xFF;
    if(g < 0) g = (colourwhite>>8)&0xFF;
    if(b < 0) b = colourwhite&0xFF;
    bvec4 colorstack[16], color = TVECX(r, g, b, fade);
    loopi(16) colorstack[i] = color;
    Texture *tex = curfont->texs[0];
    (textshader ? textshader : hudtextshader)->set();
    LOCALPARAMF(textparams, curfont->bordermin, curfont->bordermax, curfont->outlinemin, curfont->outlinemax);
    wantfontpass = false;
    curfontpass = 0;
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    settexture(tex);
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
    if(wantfontpass)
    {
        if(fontstack.length() > oldfontdepth) popfont(fontstack.length() - oldfontdepth);
        curfont = fg.orig;
        lastfont = curfont;
        curfontpass = 1;
        fade = a;
        usecolor = true;
        hasfade = false;
        if(fade < 0) { usecolor = false; fade = -a; }
        colorpos = 1;
        ly = 0;
        left = rleft;
        top = rtop;
        cx = -FONTW;
        cy = 0;
        color = TVECX(r, g, b, fade);
        loopi(16) colorstack[i] = color;
        hudshader->set();
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gle::color(color);
        gle::defvertex(textmatrix ? 3 : 2);
        gle::deftexcoord0();
        gle::begin(GL_QUADS);
        TEXTSKELETON
        //TEXTEND(cursor)
        xtraverts += gle::end();
        (textshader ? textshader : hudtextshader)->set();
        LOCALPARAMF(textparams, fg.orig->bordermin, fg.orig->bordermax, fg.orig->outlinemin, fg.orig->outlinemax);
        wantfontpass = false;
    }
    curfontpass = 0;
    #undef TEXTPARSEOK
    #undef TEXTSYNCFONT
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTFONT
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
    return ly + fg.orig->scale*curtextscale;
}

void reloadfonts()
{
    enumerate(missingfonts, const char *, s, delete[] s);
    missingfonts.clear();
    enumerate(fonts, font, f,
        loopv(f.texs) if(!reloadtexture(f.texs[i])) fatal("Failed to reload font texture");
    );
}
COMMAND(0, reloadfonts, "");

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
