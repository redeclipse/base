#include "engine.h"

VAR(IDF_PERSIST, textblinking, 0, 250, VAR_MAX);
FVAR(IDF_PERSIST, textscale, FVAR_NONZERO, 1, FVAR_MAX);
VAR(IDF_PERSIST, textfaded, 0, 1, 1);
VAR(IDF_PERSIST, textminintensity, 0, 32, 255);
VARF(IDF_PERSIST, textkeybg, 0, 1, 1, changedkeys = totalmillis);
VARF(IDF_PERSIST, textkeyseps, 0, 1, 1, changedkeys = totalmillis);
VAR(IDF_PERSIST|IDF_HEX, textkeybgcolour, 0x000000, 0xFFFFFF, 0xFFFFFF);
VAR(IDF_PERSIST|IDF_HEX, textkeyfgcolour, 0x000000, 0x00FFFF, 0xFFFFFF);
FVAR(IDF_PERSIST, textkeybgblend, 0, 0.25f, 1);
FVAR(IDF_PERSIST, textkeyfgblend, 0, 1, 1);
TVAR(IDF_PERSIST|IDF_PRELOAD, textkeybgtex, "textures/textkeybg", 3);

static inline bool htcmp(const char *key, const font &f) { return !strcmp(key, f.name); }

static hashset<font> fonts;
static font *fontdef = NULL;
static int fontdeftex = 0;

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
    f->maxw = f->defaultw = *defaultw;
    f->maxh = f->defaulth = f->scale = *defaulth;

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
}

void fonttex(char *s)
{
    if(!fontdef) return;

    Texture *t = textureload(s);
    loopv(fontdef->texs) if(fontdef->texs[i] == t) { fontdeftex = i; return; }
    fontdeftex = fontdef->texs.length();
    fontdef->texs.add(t);
}

void fontchar(int *x, int *y, int *w, int *h, int *offsetx, int *offsety, int *advance)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
    c.offsetx = *offsetx;
    c.offsety = *offsety;
    if(c.offsetx+c.w > fontdef->maxw) fontdef->maxw = c.offsetx+c.w;
    if(c.offsety+c.h > fontdef->maxh) fontdef->maxh = c.offsety+c.h;
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
COMMAND(0, fontchar, "iiiiiii");
COMMAND(0, fontskip, "i");

font *loadfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f)
    {
        defformatstring(n)("config/fonts/%s.cfg", name);
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
    d->maxh = s->maxh;

    fontdef = d;
    fontdeftex = d->texs.length()-1;
}

COMMAND(0, fontalias, "ss");

bool setfont(const char *name)
{
    font *f = loadfont(name);
    if(!f) return false;
    curfont = f;
    return true;
}

float text_widthf(const char *str, int flags)
{
    float width, height;
    text_boundsf(str, width, height, -1, flags);
    return width;
}
ICOMMAND(0, textwidth, "si", (char *s, int *f), floatret(text_widthf(s, *f)));

int text_fontw(const char *s)
{
    if(s && *s) pushfont(s);
    int w = FONTW;
    if(s && *s) popfont();
    return w;
}
ICOMMAND(0, fontwidth, "s", (char *s), intret(text_fontw(s)));

int text_fonth(const char *s)
{
    if(s && *s) pushfont(s);
    int h = FONTH;
    if(s && *s) popfont();
    return h;
}
ICOMMAND(0, fontheight, "s", (char *s), intret(text_fonth(s)));

#define TEXTTAB(x) (max((int((x)/FONTTAB)+1.0f)*FONTTAB, (x) + FONTW))

void tabify(const char *str, int *numtabs)
{
    int tw = max(*numtabs, 0)*FONTTAB-1, tabs = 0;
    for(float w = text_widthf(str); w <= tw; w = TEXTTAB(w)) ++tabs;
    int len = strlen(str);
    char *tstr = newstring(len + tabs);
    memcpy(tstr, str, len);
    memset(&tstr[len], '\t', tabs);
    tstr[len+tabs] = '\0';
    stringret(tstr);
}

COMMAND(0, tabify, "si");

int draw_textf(const char *fstr, int left, int top, ...)
{
    defvformatbigstring(str, top, fstr);
    return draw_text(str, left, top);
}

static float draw_char(Texture *&tex, int c, float x, float y, float scale)
{
    font::charinfo &info = curfont->chars[c-curfont->charoffset];
    if(tex != curfont->texs[info.tex])
    {
        xtraverts += varray::end();
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

    varray::attrib<float>(x1, y1); varray::attrib<float>(tx1, ty1);
    varray::attrib<float>(x2, y1); varray::attrib<float>(tx2, ty1);
    varray::attrib<float>(x2, y2); varray::attrib<float>(tx2, ty2);
    varray::attrib<float>(x1, y2); varray::attrib<float>(tx1, ty2);

    return scale*info.advance;
}

#define TVECA(ci, ca) (cvec::from24c(ci).mul(255-textminintensity).div(255).add(textminintensity).min(255).alpha(ca))
#define TVECX(cr, cg, cb, ca) (cvec(cr, cg, cb).mul(255-textminintensity).div(255).add(textminintensity).min(255).alpha(ca))

static void text_color(char c, cvec *stack, int size, int &sp, cvec &color, int r, int g, int b, int a)
{
    int alpha = stack[sp].a;
    switch(c)
    {
        case 'g': case '0': stack[sp] = color = TVECX( 64, 255,  64, alpha); break; // green
        case 'b': case '1': stack[sp] = color = TVECX(86,  92,  255, alpha); break; // blue
        case 'y': case '2': stack[sp] = color = TVECX(255, 255,   0, alpha); break; // yellow
        case 'r': case '3': stack[sp] = color = TVECX(255,  64,  64, alpha); break; // red
        case 'a': case '4': stack[sp] = color = TVECX(192, 192, 192, alpha); break; // grey
        case 'm': case '5': stack[sp] = color = TVECX(255, 186, 255, alpha); break; // magenta
        case 'o': case '6': stack[sp] = color = TVECX(255,  64,   0, alpha); break; // orange
        case 'w': case '7': stack[sp] = color = TVECX(255, 255, 255, alpha); break; // white
        case 'k': case '8': stack[sp] = color = TVECX(0,     0,   0, alpha); break; // black
        case 'c': case '9': stack[sp] = color = TVECX(64,  255, 255, alpha); break; // cyan
        case 'v': stack[sp] = color = TVECX(192,  96, 255, alpha); break; // violet
        case 'p': stack[sp] = color = TVECX(224,  64, 224, alpha); break; // purple
        case 'n': stack[sp] = color = TVECX(164,  72,  56, alpha); break; // brown
        case 'G': stack[sp] = color = TVECX( 86, 164,  56, alpha); break; // dark green
        case 'B': stack[sp] = color = TVECX( 56,  64, 172, alpha); break; // dark blue
        case 'Y': stack[sp] = color = TVECX(172, 172,   0, alpha); break; // dark yellow
        case 'R': stack[sp] = color = TVECX(172,  56,  56, alpha); break; // dark red
        case 'M': stack[sp] = color = TVECX(172,  72, 172, alpha); break; // dark magenta
        case 'O': stack[sp] = color = TVECX(172,  56,   0, alpha); break; // dark orange
        case 'C': stack[sp] = color = TVECX(48,  172, 172, alpha); break; // dark cyan
        case 'A': case 'd': stack[sp] = color = TVECX(102, 102, 102, alpha); break; // dark grey
        case 'P': stack[sp] = color = TVECX(255, 128, 128, alpha); break; // pink
        case 'e': case 'E': (stack[sp] = color = stack[sp]).alpha(c != 'E' ? a/2 : a/4); break;
        case 'u': case 'Z': stack[sp] = color = TVECX(r, g, b, a); break; // default colour
        case 's': // save
        {
            if(sp < size-1) stack[++sp] = color;
            break;
        }
        case 'S': // restore
        {
            if(sp > 0) --sp;
            color = stack[sp];
            break;
        }
        default: color = stack[sp]; break; // everything else
    }
    xtraverts += varray::end();
    glColor4ub((uchar)color.r, (uchar)color.g, (uchar)color.b, (uchar)color.a);
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
        xtraverts += varray::end();
        tex = t;
        glBindTexture(GL_TEXTURE_2D, tex->id);
    }
    float h = curfont->maxh*scale, w = (t->w*h)/float(t->h);
    varray::attrib<float>(x,     y    ); varray::attrib<float>(0, 0);
    varray::attrib<float>(x + w, y    ); varray::attrib<float>(1, 0);
    varray::attrib<float>(x + w, y + h); varray::attrib<float>(1, 1);
    varray::attrib<float>(x,     y + h); varray::attrib<float>(0, 1);
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
    return (t->w*curfont->maxh*scale)/t->h;
}

#define TEXTCOLORIZE(h,s) \
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
            if(s && end > start) \
            { \
                bigstring value; \
                copybigstring(value, start, min(size_t(end - start + 1), sizeof(value))); \
                TEXTICON(value); \
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
            if(s && end > start) \
            { \
                bigstring value; \
                copybigstring(value, start, min(size_t(end - start + 1), sizeof(value))); \
                TEXTKEY(value); \
            } \
            h += end-start; \
        } \
        else break; \
    } \
    else if(s) TEXTCOLOR(h); \
}
#define TEXTALIGN \
{ \
    x = (!(flags&TEXT_RIGHT_JUSTIFY) && !(flags&TEXT_NO_INDENT) ? FONTTAB : 0); \
    if(!y && (flags&TEXT_RIGHT_JUSTIFY) && !(flags&TEXT_NO_INDENT)) maxwidth -= FONTTAB; \
    y += FONTH; \
}
#define TEXTSKELETON \
    float y = 0, x = 0, scale = curfont->scale/float(curfont->defaulth)*textscale;\
    int i = 0;\
    for(i = 0; str[i]; i++)\
    {\
        int c = uchar(str[i]);\
        TEXTINDEX(i)\
        if(c == '\t')      { x = TEXTTAB(x); TEXTWHITE(i) }\
        else if(c == ' ')  { x += scale*curfont->defaultw; TEXTWHITE(i) }\
        else if(c == '\n') { TEXTLINE(i) TEXTALIGN }\
        else if(c == '\f') { if(str[i+1]) { i++; TEXTCOLORIZE(i, true); } }\
        else if(curfont->chars.inrange(c-curfont->charoffset))\
        {\
            float cw = scale*(curfont->chars[c-curfont->charoffset].advance);\
            if(cw <= 0) continue;\
            if(maxwidth != -1)\
            {\
                int j = i;\
                float w = cw;\
                for(; str[i+1]; i++)\
                {\
                    int c = uchar(str[i+1]);\
                    if(c == '\f') { if(str[i+2]) { i += 2; TEXTCOLORIZE(i, false); } }\
                    if(i-j > 16) break;\
                    if(!curfont->chars.inrange(c-curfont->charoffset)) break;\
                    cw = scale*curfont->chars[c-curfont->charoffset].advance;\
                    if(cw <= 0 || w+cw > maxwidth) break; \
                    w += cw;\
                }\
                if(x+w > maxwidth && j != 0) { TEXTLINE(j-1) TEXTALIGN }\
                TEXTWORD\
            }\
            else { TEXTCHAR(i) }\
        }\
    }

//all the chars are guaranteed to be either drawable or color commands
#define TEXTWORDSKELETON \
    for(; j <= i; j++) \
    {\
        TEXTINDEX(j)\
        int c = uchar(str[j]);\
        if(c == '\f') { if(str[j+1]) { j++; TEXTCOLORIZE(j, true); } }\
        else { float cw = scale*curfont->chars[c-curfont->charoffset].advance; TEXTCHAR(j) }\
    }

#define TEXTEND(cursor) if(cursor >= i) { do { TEXTINDEX(cursor); } while(0); }

int text_visible(const char *str, float hitx, float hity, int maxwidth, int flags)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret) x += icon_width(ret, scale);
    #define TEXTKEY(ret) x += (textkeybg ? icon_width(textkeybgtex, scale)*0.6f : 0.f)+text_widthf(ret, flags);
    #define TEXTCHAR(idx) x += cw; TEXTWHITE(idx)
    #define TEXTWORD TEXTWORDSKELETON
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
    #undef TEXTWORD
    return i;
}

//inverse of text_visible
void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth, int flags)
{
    #define TEXTINDEX(idx) if(cursor == idx) { cx = x; cy = y; break; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret) x += icon_width(ret, scale);
    #define TEXTKEY(ret) x += (textkeybg ? icon_width(textkeybgtex, scale)*0.6f : 0.f)+text_widthf(ret, flags);
    #define TEXTCHAR(idx) x += cw;
    #define TEXTWORD TEXTWORDSKELETON if(i >= cursor) break;
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
    #undef TEXTWORD
}

void text_boundsf(const char *str, float &width, float &height, int maxwidth, int flags)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret) x += icon_width(ret, scale);
    #define TEXTKEY(ret) x += (textkeybg ? icon_width(textkeybgtex, scale)*0.6f : 0.f)+text_widthf(ret, flags);
    #define TEXTCHAR(idx) x += cw;
    #define TEXTWORD TEXTWORDSKELETON
    width = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTICON
    #undef TEXTKEY
    #undef TEXTCHAR
    #undef TEXTWORD
}

int draw_key(Texture *&tex, const char *str, float sx, float sy, float sc, cvec &cl, int flags)
{
    float swidth = text_widthf(str, flags), ss = 0, sp = 0;
    if(textkeybg)
    {
        Texture *t = textureload(textkeybgtex, 3, true, false);
        if(tex != t)
        {
            xtraverts += varray::end();
            tex = t;
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }

        glColor4ub(uchar((textkeybgcolour>>16)&0xFF), uchar((textkeybgcolour>>8)&0xFF), uchar(textkeybgcolour&0xFF), uchar(textkeybgblend*cl.a));

        float sh = curfont->maxh*sc, sw = (t->w*sh)/float(t->h), w1 = sw*0.25f, w2 = sw*0.5f, amt = swidth/w2;
        int count = int(floorf(amt));
        varray::attrib<float>(sx + ss,     sy    ); varray::attrib<float>(0, 0);
        varray::attrib<float>(sx + ss + w1, sy    ); varray::attrib<float>(0.25f, 0);
        varray::attrib<float>(sx + ss + w1, sy + sh); varray::attrib<float>(0.25f, 1);
        varray::attrib<float>(sx + ss,     sy + sh); varray::attrib<float>(0, 1);
        sp = (ss += w1);
        loopi(count)
        {
            varray::attrib<float>(sx + ss,     sy    ); varray::attrib<float>(0.25f, 0);
            varray::attrib<float>(sx + ss + w2, sy    ); varray::attrib<float>(0.75f, 0);
            varray::attrib<float>(sx + ss + w2, sy + sh); varray::attrib<float>(0.75f, 1);
            varray::attrib<float>(sx + ss,     sy + sh); varray::attrib<float>(0.25f, 1);
            ss += w2;
        }
        float w3 = amt-float(count), w4 = w1 + w2*w3, w5 = 0.75f - 0.5f*w3;
        varray::attrib<float>(sx + ss,     sy    ); varray::attrib<float>(w5, 0);
        varray::attrib<float>(sx + ss + w4, sy    ); varray::attrib<float>(1, 0);
        varray::attrib<float>(sx + ss + w4, sy + sh); varray::attrib<float>(1, 1);
        varray::attrib<float>(sx + ss,     sy + sh); varray::attrib<float>(w5, 1);
        ss += w4;
    }
    else ss = swidth;
    xtraverts += varray::end();

    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) ly += FONTH;
    #define TEXTCOLOR(idx) text_color(str[idx], colorstack, sizeof(colorstack), colorpos, color, r, g, b, fade);
    #define TEXTHEXCOLOR(ret) \
        if(usecolor) \
        { \
            int alpha = colorstack[colorpos].a; \
            color = TVECA(ret, alpha); \
            colorstack[colorpos] = color; \
            xtraverts += varray::end(); \
            glColor4ub((uchar)color.r, (uchar)color.g, (uchar)color.b, (char)color.a); \
        }
    #define TEXTICON(ret) x += draw_icon(tex, ret, left+x, top+y, scale);
    #define TEXTKEY(ret) x += draw_key(tex, ret, left+x, top+y, scale, color, flags);
    #define TEXTCHAR(idx) { draw_char(tex, c, left+x, top+y, scale); x += cw; }
    #define TEXTWORD TEXTWORDSKELETON
    bool usecolor = true;
    int fade = textkeyfgblend*cl.a, r = (textkeyfgcolour>>16)&0xFF, g = (textkeyfgcolour>>8)&0xFF, b = textkeyfgcolour&0xFF,
        colorpos = 1, ly = 0, left = sx + sp, top = sy, cursor = -1, maxwidth = -1;
    cvec colorstack[16], color = TVECX(r, g, b, fade);
    loopi(16) colorstack[i] = color;
    glColor4ub((uchar)color.r, (uchar)color.g, (uchar)color.b, (uchar)color.a);
    TEXTSKELETON
    TEXTEND(cursor)
    xtraverts += varray::end();

    glColor4ub((uchar)cl.r, (uchar)cl.g, (uchar)cl.b, (uchar)cl.a);

    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
    return ss;
}

int draw_text(const char *str, int rleft, int rtop, int r, int g, int b, int a, int flags, int cursor, int maxwidth)
{
    #define TEXTINDEX(idx) \
        if(cursor >= 0 && idx == cursor) \
        { \
            cx = x; \
            cy = y; \
            if(!hasfade && usecolor && textfaded) \
            { \
                text_color('e', colorstack, sizeof(colorstack), colorpos, color, r, g, b, fade); \
                hasfade = true; \
            } \
        }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) ly += FONTH;
    #define TEXTCOLOR(idx) if(usecolor) { text_color(str[idx], colorstack, sizeof(colorstack), colorpos, color, r, g, b, fade); }
    #define TEXTHEXCOLOR(ret) \
        if(usecolor) \
        { \
            int alpha = colorstack[colorpos].a; \
            color = TVECA(ret, alpha); \
            colorstack[colorpos] = color; \
            xtraverts += varray::end(); \
            glColor4ub((uchar)color.r, (uchar)color.g, (uchar)color.b, (char)color.a); \
        }
    #define TEXTICON(ret) x += draw_icon(tex, ret, left+x, top+y, scale);
    #define TEXTKEY(ret) x += draw_key(tex, ret, left+x, top+y, scale, color, flags);
    #define TEXTCHAR(idx) { draw_char(tex, c, left+x, top+y, scale); x += cw; }
    #define TEXTWORD TEXTWORDSKELETON
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Texture *tex = curfont->texs[0];
    glBindTexture(GL_TEXTURE_2D, tex->id);
    varray::enable();
    varray::defattrib(varray::ATTRIB_VERTEX, 2, GL_FLOAT);
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
    varray::begin(GL_QUADS);
    int fade = a;
    bool usecolor = true, hasfade = false;
    if(fade < 0) { usecolor = false; fade = -a; }
    int colorpos = 1, ly = 0, left = rleft, top = rtop;
    float cx = -FONTW, cy = 0;
    cvec colorstack[16], color = TVECX(r, g, b, fade);
    loopi(16) colorstack[i] = color;
    glColor4ub((uchar)color.r, (uchar)color.g, (uchar)color.b, (uchar)color.a);
    TEXTSKELETON
    TEXTEND(cursor)
    xtraverts += varray::end();
    if(cursor >= 0)
    {
        glColor4ub(255, 255, 255, int(255*clamp(1.f-(float(totalmillis%500)/500.f), 0.5f, 1.f)));
        draw_char(tex, '_', left+cx, top+cy, scale);
        xtraverts += varray::end();
    }
    varray::disable();
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTHEXCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
    return ly + FONTH;
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        loopv(f.texs) if(!reloadtexture(f.texs[i])) fatal("failed to reload font texture");
    );
}


int draw_textx(const char *fstr, int left, int top, int r, int g, int b, int a, int flags, int cursor, int maxwidth, ...)
{
    defvformatbigstring(str, maxwidth, fstr);

    int width = 0, height = 0;
    text_bounds(str, width, height, maxwidth, flags);
    if(flags&TEXT_ALIGN) switch(flags&TEXT_ALIGN)
    {
        case TEXT_CENTERED: left -= width/2; break;
        case TEXT_RIGHT_JUSTIFY: left -= width; break;
        default: break;
    }
    if(flags&TEXT_BALLOON) top -= height/2;
    else if(flags&TEXT_UPWARD) top -= height;
    if(flags&TEXT_SHADOW) draw_text(str, left-2, top-2, 0, 0, 0, a, flags, cursor, maxwidth);
    return draw_text(str, left, top, r, g, b, a, flags, cursor, maxwidth);
}

vector<font *> fontstack;

bool pushfont(const char *name)
{
    if(!fontstack.length() && curfont)
        fontstack.add(curfont);

    if(setfont(name))
    {
        fontstack.add(curfont);
        return true;
    }
    return false;
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

