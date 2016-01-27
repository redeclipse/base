#include "engine.h"

VAR(IDF_PERSIST, textblinking, 0, 250, VAR_MAX);
float curtextscale = 1;
FVARF(IDF_PERSIST, textscale, FVAR_NONZERO, 1, FVAR_MAX, curtextscale = textscale);
VAR(IDF_PERSIST, textfaded, 0, 1, 1);
VAR(IDF_PERSIST, textminintensity, 0, 32, 255);

Texture *tkbbordertex = NULL, *tkbtex = NULL;
VARF(IDF_PERSIST, textkeybg, 0, 2, 2, changedkeys = totalmillis);
VARF(IDF_PERSIST, textkeyseps, 0, 1, 1, changedkeys = totalmillis);
VAR(IDF_PERSIST|IDF_HEX, textkeybgcolour, 0x000000, 0xC0C0C0, 0xFFFFFF);
VAR(IDF_PERSIST|IDF_HEX, textkeybgbordercolour, 0x000000, 0xFFFFFF, 0xFFFFFF);
VAR(IDF_PERSIST|IDF_HEX, textkeyfgcolour, 0x000000, 0x00FFFF, 0xFFFFFF);
FVAR(IDF_PERSIST, textkeybgblend, 0, 0.3f, 1);
FVAR(IDF_PERSIST, textkeybgborderblend, 0, 0.4f, 1);
FVAR(IDF_PERSIST, textkeyfgblend, 0, 1, 1);
TVARN(IDF_PERSIST|IDF_PRELOAD, textkeybgtex, "textures/guiskin", tkbtex, 0);
TVARN(IDF_PERSIST|IDF_PRELOAD, textkeybgbordertex, "textures/guiskinborder", tkbbordertex, 0);

Texture *tbgbordertex = NULL, *tbgtex = NULL;
VAR(IDF_PERSIST, textskin, 0, 2, 2);
FVAR(IDF_PERSIST, textskinblend, 0, 0.3f, 1);
FVAR(IDF_PERSIST, textskinfblend, 0, 1, 1);
FVAR(IDF_PERSIST, textskinbright, 0, 0.6f, 10);
FVAR(IDF_PERSIST, textskinfbright, 0, 1, 10);
FVAR(IDF_PERSIST, textskinborderbright, 0, 1, 10);
FVAR(IDF_PERSIST, textskinborderblend, 0, 0.4f, 1);
FVAR(IDF_PERSIST, textskinstretchx, -1, 0.5f, 1);
FVAR(IDF_PERSIST, textskinstretchy, -1, 0.001f, 1);
TVARN(IDF_PERSIST|IDF_PRELOAD, textskintex, "textures/textskin", tbgtex, 0);
TVARN(IDF_PERSIST|IDF_PRELOAD, textskinbordertex, "textures/textskinborder", tbgbordertex, 0);

static hashnameset<font> fonts;
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

float text_widthf(const char *str, int xpad, int ypad, int flags)
{
    float width, height;
    text_boundsf(str, width, height, xpad, ypad, -1, flags);
    return width;
}
ICOMMAND(0, textwidth, "siii", (char *s, int *f, int *x, int *y), floatret(text_widthf(s, *x, *y, *f)));

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

#define TVECA(ci, ca) (bvec4::fromcolor(ci).lighten(textminintensity).alpha(ca))
#define TVECX(cr, cg, cb, ca) (bvec4(cr, cg, cb, 255).lighten(textminintensity).alpha(ca))

static void text_color(char c, bvec4 *stack, int size, int &sp, bvec4 &color, int r, int g, int b, int a)
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
                copystring(value, start, min(size_t(end - start + 1), sizeof(value))); \
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
                copystring(value, start, min(size_t(end - start + 1), sizeof(value))); \
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
    float y = 0, x = 0, scale = curfont->scale/float(curfont->defaulth)*curtextscale;\
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
    #define TEXTKEY(ret) x += (textkeybg ? icon_width(textkeybgtex, scale)*0.6f : 0.f)+text_widthf(ret, 0, 0, flags);
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
    #define TEXTKEY(ret) x += (textkeybg ? icon_width(textkeybgtex, scale)*0.6f : 0.f)+text_widthf(ret, 0, 0, flags);
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

void text_boundsf(const char *str, float &width, float &height, int xpad, int ypad, int maxwidth, int flags)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTHEXCOLOR(ret)
    #define TEXTICON(ret) x += icon_width(ret, scale);
    #define TEXTKEY(ret) x += (textkeybg ? icon_width(textkeybgtex, scale)*0.6f : 0.f)+text_widthf(ret, 0, 0, flags);
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
    if(xpad) width += xpad*2;
    if(ypad) height += ypad*2;
}

int draw_key(Texture *&tex, const char *str, float sx, float sy, float sc, bvec4 &cl, int flags)
{
    float swidth = text_widthf(str, 0, 0, flags), ss = swidth, sp = 0;
    if(tex)
    {
        xtraverts += gle::end();
        tex = NULL;
    }
    if(textkeybg)
    {
        sp += FONTW*0.5f;
        ss += FONTW;
        loopk(textkeybg)
        {
            int colour = textkeybgcolour;
            float blend = textkeybgblend;
            Texture *t = NULL;
            switch(k)
            {
                case 1:
                    colour = textkeybgbordercolour;
                    blend = textkeybgborderblend;
                    if(!tkbbordertex) tkbbordertex = textureload(textkeybgbordertex, 0, true, false);
                    t = tkbbordertex;
                    break;
                case 0: default:
                    if(!tkbtex) tkbtex = textureload(textkeybgtex, 0, true, false);
                    t = tkbtex;
                    break;
            }
            drawskin(t, sx, sy, sx+ss, sy+(curfont->maxh+4)*sc, colour, blend*(cl.a/255.f), 0, textmatrix);
        }
    }

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
            xtraverts += gle::end(); \
            gle::color(color); \
        }
    #define TEXTICON(ret) x += draw_icon(tex, ret, left+x, top+y, scale);
    #define TEXTKEY(ret) x += draw_key(tex, ret, left+x, top+y, scale, color, flags);
    #define TEXTCHAR(idx) { draw_char(tex, c, left+x, top+y, scale); x += cw; }
    #define TEXTWORD TEXTWORDSKELETON
    bool usecolor = true;
    int fade = textkeyfgblend*cl.a, r = (textkeyfgcolour>>16)&0xFF, g = (textkeyfgcolour>>8)&0xFF, b = textkeyfgcolour&0xFF,
        colorpos = 1, ly = 0, left = sx + sp, top = sy, cursor = -1, maxwidth = -1;
    bvec4 colorstack[16], color = TVECX(r, g, b, fade);
    loopi(16) colorstack[i] = color;
    gle::color(color);
    TEXTSKELETON
    TEXTEND(cursor)
    xtraverts += gle::end();

    gle::color(cl);

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
            xtraverts += gle::end(); \
            gle::color(color); \
        }
    #define TEXTICON(ret) x += draw_icon(tex, ret, left+x, top+y, scale);
    #define TEXTKEY(ret) x += draw_key(tex, ret, left+x, top+y, scale, color, flags);
    #define TEXTCHAR(idx) { draw_char(tex, c, left+x, top+y, scale); x += cw; }
    #define TEXTWORD TEXTWORDSKELETON
    int fade = a;
    bool usecolor = true, hasfade = false;
    if(fade < 0) { usecolor = false; fade = -a; }
    int colorpos = 1, ly = 0, left = rleft, top = rtop;
    float cx = -FONTW, cy = 0;
    bvec4 colorstack[16], color = TVECX(r, g, b, fade);
    loopi(16) colorstack[i] = color;
    gle::color(color);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Texture *tex = curfont->texs[0];
    glBindTexture(GL_TEXTURE_2D, tex->id);
    gle::defvertex(textmatrix ? 3 : 2);
    gle::deftexcoord0();
    gle::begin(GL_QUADS);
    TEXTSKELETON
    TEXTEND(cursor)
    xtraverts += gle::end();
    if(cursor >= 0)
    {
        gle::colorub(255, 255, 255, int(255*clamp(1.f-(float(totalmillis%500)/500.f), 0.5f, 1.f)));
        draw_char(tex, '_', left+cx, top+cy, scale);
        xtraverts += gle::end();
    }
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

int draw_textx(const char *fstr, int left, int top, int xpad, int ypad, int r, int g, int b, int a, int flags, int cursor, int maxwidth, ...)
{
    defvformatbigstring(str, maxwidth, fstr);

    int width = 0, height = 0;
    text_bounds(str, width, height, xpad, ypad, maxwidth, flags);
    if(flags&TEXT_ALIGN) switch(flags&TEXT_ALIGN)
    {
        case TEXT_CENTERED: left -= width/2; break;
        case TEXT_RIGHT_JUSTIFY: left -= width; break;
        default: break;
    }
    if(flags&TEXT_BALLOON) top -= height/2;
    else if(flags&TEXT_UPWARD) top -= height;
    if(xpad) left += xpad;
    if(ypad) top += ypad;
    if(flags&TEXT_SKIN && textskin)
    {
        int stretchx = int(FONTW*textskinstretchx), stretchy = int(FONTH*textskinstretchy);
        loopk(textskin)
        {
            Texture *t = NULL;
            float blend = a/255.f, bright = 1;
            switch(k)
            {
                case 1:
                    bright *= textskinborderbright;
                    blend *= textskinborderblend;
                    if(!tbgbordertex) tbgbordertex = textureload(textskinbordertex, 0, true, false);
                    t = tbgbordertex;
                    break;
                case 0: default:
                    bright *= textskinbright;
                    blend *= textskinblend;
                    if(!tbgtex) tbgtex = textureload(textskintex, 0, true, false);
                    t = tbgtex;
                    break;
            }
            drawskin(t, left-stretchx, top-stretchy, left+width+stretchx, top+height+stretchy, bvec(int(r*bright), int(g*bright), int(b*bright)).min(255).tohexcolor(), blend);
        }
        r = int(255*textskinfbright);
        g = int(255*textskinfbright);
        b = int(255*textskinfbright);
        a = int(a*textskinfblend);
    }
    if(xpad) left += xpad;
    if(ypad) top += ypad;
    if(flags&TEXT_SHADOW) draw_text(str, left-2, top-2, 0, 0, 0, a, flags, cursor, maxwidth);
    draw_text(str, left, top, r, g, b, a, flags, cursor, maxwidth);
    return height;
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
