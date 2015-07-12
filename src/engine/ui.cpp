#include "engine.h"

int uimillis = -1;

VAR(IDF_READONLY, guilayoutpass, 1, 0, -1);
bool guiactionon = false;
int mouseaction[2] = {0};

static float firstx, firsty;

enum {FIELDCOMMIT, FIELDABORT, FIELDEDIT, FIELDSHOW, FIELDKEY};
static int fieldmode = FIELDSHOW;
static bool fieldsactive = false;

VAR(IDF_PERSIST, guishadow, 0, 2, 8);
VAR(IDF_PERSIST, guiclicktab, 0, 1, 1);
VAR(IDF_PERSIST, guitabborder, 0, 1, 2);
VAR(IDF_PERSIST, guitextblend, 1, 255, 255);
VAR(IDF_PERSIST, guitextfade, 1, 200, 255);
VAR(IDF_PERSIST, guisepsize, 1, 2, 128);
VAR(IDF_PERSIST, guispacesize, 1, 48, 128);
VAR(IDF_PERSIST, guiscaletime, 0, 250, VAR_MAX);

VAR(IDF_PERSIST, guitooltipwidth, -1, 768, VAR_MAX);
VAR(IDF_PERSIST, guistatuswidth, -1, 2048, VAR_MAX);

VAR(IDF_PERSIST, guiskinned, 0, 3, 3); // 0 = no backgrounds, 1 = drawn backgrounds, 2 = skinned backgrounds, 3 = skinned with overlay border
VARF(IDF_PERSIST, guiskinsize, 0, 64, VAR_MAX, if(guiskinsize) { int off = guiskinsize%4; if(off) guiskinsize += guiskinsize-off; }); // 0 = texture size, otherwise = size in pixels for skin scaling

VAR(IDF_PERSIST|IDF_HEX, guibgcolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guibgblend, 0, 0.7f, 1);
VAR(IDF_PERSIST|IDF_HEX, guibordercolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guiborderblend, 0, 1.f, 1);

VAR(IDF_PERSIST|IDF_HEX, guihovercolour, -1, 0xF0A0A0, 0xFFFFFF);
FVAR(IDF_PERSIST, guihoverscale, 0, 0.3f, 1);
FVAR(IDF_PERSIST, guihoverblend, 0, 0.9f, 1);

VAR(IDF_PERSIST, guistatusline, 0, 1, 1);
VAR(IDF_PERSIST, guitooltips, 0, 1, 1);
VAR(IDF_PERSIST, guitooltiptime, 0, 500, VAR_MAX);
VAR(IDF_PERSIST, guitooltipfade, 0, 500, VAR_MAX);
VAR(IDF_PERSIST|IDF_HEX, guitooltipcolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guitooltipblend, 0, 0.9f, 1);
VAR(IDF_PERSIST|IDF_HEX, guitooltipbordercolour, -1, 0x808080, 0xFFFFFF);
FVAR(IDF_PERSIST, guitooltipborderblend, 0, 1.f, 1);
VAR(IDF_PERSIST, guitooltipborderskin, 0, 1, 1);

VAR(IDF_PERSIST|IDF_HEX, guifieldbgcolour, -1, 0x404040, 0xFFFFFF);
FVAR(IDF_PERSIST, guifieldbgblend, 0, 0.7f, 1);
VAR(IDF_PERSIST|IDF_HEX, guifieldbordercolour, -1, 0xC0C0C0, 0xFFFFFF);
FVAR(IDF_PERSIST, guifieldborderblend, 0, 1.f, 1);
VAR(IDF_PERSIST|IDF_HEX, guifieldactivecolour, -1, 0xF04040, 0xFFFFFF);
FVAR(IDF_PERSIST, guifieldactiveblend, 0, 1.f, 1);

VAR(IDF_PERSIST, guislidersize, 1, 48, 128);
VAR(IDF_PERSIST|IDF_HEX, guislidercolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guisliderblend, 0, 0.3f, 1);
VAR(IDF_PERSIST|IDF_HEX, guisliderbordercolour, -1, 0xC0C0C0, 0xFFFFFF);
FVAR(IDF_PERSIST, guisliderborderblend, 0, 1.f, 1);
VAR(IDF_PERSIST, guisliderborderskin, 0, 2, 2);
VAR(IDF_PERSIST|IDF_HEX, guislidermarkcolour, -1, 0x808080, 0xFFFFFF);
FVAR(IDF_PERSIST, guislidermarkblend, 0, 1.f, 1);
VAR(IDF_PERSIST|IDF_HEX, guislidermarkbordercolour, -1, 0xC0C0C0, 0xFFFFFF);
FVAR(IDF_PERSIST, guislidermarkborderblend, 0, 1.f, 1);
VAR(IDF_PERSIST, guislidermarkborderskin, 0, 0, 2);
VAR(IDF_PERSIST|IDF_HEX, guislideractivecolour, -1, 0xF04040, 0xFFFFFF);
FVAR(IDF_PERSIST, guislideractiveblend, 0, 1.f, 1);

VAR(IDF_PERSIST|IDF_HEX, guiactivecolour, -1, 0xF02020, 0xFFFFFF);
FVAR(IDF_PERSIST, guiactiveblend, 0, 1.f, 1);

static bool needsinput = false, hastitle = true, hasbgfx = true, tooltipforce = false;
static char *statusstr = NULL, *tooltipstr = NULL, *tooltip = NULL;
static int lasttooltip = 0, statuswidth = 0, tooltipwidth = 0;

#include "textedit.h"
struct gui : guient
{
    struct list { int parent, w, h, springs, curspring, mouse[2]; };

    int nextlist;
    static vector<list> lists;
    static float hitx, hity;
    static int curdepth, curlist, xsize, ysize, curx, cury, fontdepth, mergelist, mergedepth;
    static bool hitfx;

    static void reset()
    {
        if(statusstr) DELETEA(statusstr);
        if(tooltipstr) DELETEA(tooltipstr);
        lists.shrink(0);
        statuswidth = tooltipwidth = 0;
        mergelist = mergedepth = -1;
        tooltipforce = false;
    }

    static int ty, tx, tpos, *tcurrent, tcolor; //tracking tab size and position since uses different layout method...

    void allowhitfx(bool on) { hitfx = on; }
    bool visibletab() { return !tcurrent || tpos == *tcurrent; }
    bool visible() { return !guilayoutpass && visibletab(); }

    void skin(int x1, int y1, int x2, int y2, int c1 = -1, float b1 = -1.f, int c2 = -1, float b2 = -1.f, bool skinborder = false)
    {
        int colour1 = c1 >= 0 ? c1 : (guibgcolour >= 0 ? guibgcolour : (c2 >= 0 ? c2 : 0x000000)),
            colour2 = c2 >= 0 ? c2 : (guibordercolour >= 0 ? guibordercolour : 0x808080);
        float blend1 = b1 >= 0 ? b1 : guibgblend, blend2 = b2 >= 0 ? b2 : guiborderblend;
        switch(guiskinned)
        {
            case 2: case 3:
            {
                bool drawn = false;
                loopk(guiskinned == 3 || skinborder ? 2 : 1)
                {
                    int colour = colour1;
                    float blend = blend1;
                    Texture *t = NULL;
                    switch(k)
                    {
                        case 1:
                            colour = colour2;
                            blend = blend2;
                            if(!skinbordertex) skinbordertex = textureload(guiskinbordertex, 3, true, false);
                            if(skinbordertex && skinbordertex != notexture) t = skinbordertex;
                            break;
                        case 0: default:
                            if(!skintex) skintex = textureload(guiskintex, 3, true, false);
                            if(skintex && skintex != notexture) t = skintex;
                            break;
                    }
                    if(!t) break;
                    int w = x2-x1, h = y2-y1, tw = guiskinsize ? guiskinsize : t->w, th = guiskinsize ? guiskinsize : t->h;
                    float pw = tw*0.25f, ph = th*0.25f, qw = tw*0.5f, qh = th*0.5f, px = 0, py = 0, tx = 0, ty = 0;
                    int cw = max(int(floorf(w/qw))-1, 0), ch = max(int(floorf(h/qh))+1, 2);

                    glBindTexture(GL_TEXTURE_2D, t->id);
                    glColor4f((colour>>16)/255.f, ((colour>>8)&0xFF)/255.f, (colour&0xFF)/255.f, blend);
                    glBegin(GL_QUADS);

                    loopi(ch)
                    {
                        bool cond = !i || i == ch-1;
                        float vph = cond ? ph : qh, vth = cond ? 0.25f : 0.5f;
                        if(i && cond)
                        {
                            float off = h-py;
                            if(off > vph)
                            {
                                float part = off/vph;
                                vph *= part;
                                vth *= part;
                            }
                            ty = 1-vth;
                        }
                        loopj(3) switch(j)
                        {
                            case 0: case 2:
                            {
                                glTexCoord2f(tx, ty); glVertex2f(x1+px, y1+py);
                                glTexCoord2f(tx+0.25f, ty); glVertex2f(x1+px+pw, y1+py);
                                glTexCoord2f(tx+0.25f, ty+vth); glVertex2f(x1+px+pw, y1+py+vph);
                                glTexCoord2f(tx, ty+vth); glVertex2f(x1+px, y1+py+vph);
                                tx += 0.25f;
                                px += pw;
                                xtraverts += 4;
                                break;
                            }
                            case 1:
                            {
                                for(int xx = 0; xx < cw; xx++)
                                {
                                    glTexCoord2f(tx, ty); glVertex2f(x1+px, y1+py);
                                    glTexCoord2f(tx+0.5f, ty); glVertex2f(x1+px+qw, y1+py);
                                    glTexCoord2f(tx+0.5f, ty+vth); glVertex2f(x1+px+qw, y1+py+vph);
                                    glTexCoord2f(tx, ty+vth); glVertex2f(x1+px, y1+py+vph);
                                    px += qw;
                                    xtraverts += 4;
                                }
                                float want = w-pw, off = want-px;
                                if(off > 0)
                                {
                                    float part = 0.5f*off/qw;
                                    glTexCoord2f(tx, ty); glVertex2f(x1+px, y1+py);
                                    glTexCoord2f(tx+part, ty); glVertex2f(x1+want, y1+py);
                                    glTexCoord2f(tx+part, ty+vth); glVertex2f(x1+want, y1+py+vph);
                                    glTexCoord2f(tx, ty+vth); glVertex2f(x1+px, y1+py+vph);
                                    px = want;
                                }
                                tx += 0.5f;
                                break;
                            }
                            default: break;
                        }
                        px = tx = 0;
                        py += vph;
                        if(!i) ty += vth;
                    }
                    glEnd();
                    drawn = true;
                }
                if(drawn) break; // otherwise fallback
            }
            case 1:
            {
                x1++; y1++; x2--; y2--; // offset these slightly like a skin
                if(colour1 >= 0)
                {
                    notextureshader->set();
                    glColor4f((colour1>>16)/255.f, ((colour1>>8)&0xFF)/255.f, (colour1&0xFF)/255.f, blend1);
                    glBegin(GL_TRIANGLE_STRIP);
                    glVertex2f(x1, y1);
                    glVertex2f(x2, y1);
                    glVertex2f(x1, y2);
                    glVertex2f(x2, y2);
                    xtraverts += 4;
                    glEnd();
                    defaultshader->set();
                }
                if(skinborder && colour2 >= 0)
                {
                    notextureshader->set();
                    glColor4f((colour2>>16)/255.f, ((colour2>>8)&0xFF)/255.f, (colour2&0xFF)/255.f, blend2);
                    glBegin(GL_LINE_LOOP);
                    glVertex2f(x1, y1);
                    glVertex2f(x2, y1);
                    glVertex2f(x2, y2);
                    glVertex2f(x1, y2);
                    xtraverts += 4;
                    glEnd();
                    defaultshader->set();
                }
                break;
            }
            case 0: default: break;
        }
    }

    //tab is always at top of page
    void tab(const char *name, int color, bool front)
    {
        if(curdepth != 0) return;
        tpos++;
        if(front && tcurrent && *tcurrent != tpos) *tcurrent = tpos;
        if(!hastitle)
        {
            if(guilayoutpass)
            {
                ty = max(ty, ysize);
                ysize = 0;
            }
            else cury = -ysize;
            return;
        }
        if(color) tcolor = color;
        if(!name) name = intstr(tpos);
        gui::pushfont("super");
        int width = 0, height = 0;
        text_bounds(name, width, height);
        if(guilayoutpass)
        {
            ty = max(ty, ysize);
            ysize = 0;
        }
        else
        {
            cury = -ysize;
            int x1 = curx+tx, x2 = x1+width+guispacesize, y1 = cury-guispacesize-height, y2 = cury-guispacesize*3/4, alpha = guitextblend, border = -1;
            if(!visibletab())
            {
                if(tcurrent && hitx>=x1 && hity>=y1 && hitx<x2 && hity<y2)
                {
                    if(!guiclicktab || mouseaction[0]&GUI_UP) *tcurrent = tpos; // switch tab
                    tcolor = guiactivecolour;
                    alpha = max(alpha, guitextfade);
                    if(guitabborder) border = tcolor;
                }
                else
                {
                    tcolor = vec::hexcolor(tcolor).mul(0.25f).tohexcolor();
                    if(guitabborder == 2) border = tcolor;
                }
            }
            else if(guitabborder == 2) border = guibordercolour;
            if(hasbgfx) skin(x1, y1, x2, y2, guibgcolour, guibgblend, border >= 0 ? border : guibordercolour, guiborderblend, border >= 0);
            text_(name, x1+guispacesize/2, y1+guispacesize/8, tcolor, alpha, visible());
        }
        tx += width+guispacesize*3/2;
        gui::popfont();
    }

    void uibuttons()
    {
        gui::pushfont("super");
        tx += FONTH+guispacesize*2; // acts like a tab
        if(!guilayoutpass)
        {
            cury = -ysize;
            int x1 = curx+(xsize-FONTH+guispacesize/4), x2 = x1+FONTH+guispacesize/4, y1 = cury-guispacesize-FONTH, y2 = cury-guispacesize*3/4;
            //int x1 = curx+(xsize-FONTH), x2 = x1+FONTH*3/2, y1 = cury-FONTH*225/100, y2 = cury-FONTH*3/4;
            #define uibtn(a,b) \
            { \
                int border = -1; \
                bool hit = false; \
                if(hitx>=x1 && hity>=y1 && hitx<x2 && hity<y2) \
                { \
                    if(mouseaction[0]&GUI_UP) { b; } \
                    hit = true; \
                    if(guitabborder) border = guiactivecolour; \
                } \
                else if(guitabborder == 2) border = vec::hexcolor(guibordercolour).mul(0.25f).tohexcolor(); \
                if(hasbgfx) skin(x1, y1, x2, y2, guibgcolour, guibgblend, border >= 0 ? border : guibordercolour, guiborderblend, border >= 0); \
                x1 += guispacesize/8; \
                y1 += guispacesize/8; \
                icon_(a, false, x1, y1, FONTH, hit, 0xFFFFFF); \
                y1 += FONTH*3/2; \
            }
            if(!exittex) exittex = textureload(guiexittex, 3, true, false); \
            uibtn(exittex, cleargui(1));
        }
        gui::popfont();
    }

    bool ishorizontal() const { return curdepth&1; }
    bool isvertical() const { return !ishorizontal(); }

    void pushlist(bool merge)
    {
        if(guilayoutpass)
        {
            if(curlist>=0)
            {
                lists[curlist].w = xsize;
                lists[curlist].h = ysize;
            }
            list &l = lists.add();
            l.parent = curlist;
            l.springs = 0;
            curlist = lists.length()-1;
            l.mouse[0] = l.mouse[1] = xsize = ysize = 0;
        }
        else
        {
            curlist = nextlist++;
            if(curlist >= lists.length()) // should never get here unless script code doesn't use same amount of lists in layout and render passes
            {
                list &l = lists.add();
                l.parent = curlist;
                l.springs = 0;
                l.w = l.h = l.mouse[0] = l.mouse[1] = 0;
            }
            list &l = lists[curlist];
            l.curspring = 0;
            if(l.springs > 0)
            {
                if(ishorizontal()) xsize = l.w; else ysize = l.h;
            }
            else
            {
                xsize = l.w;
                ysize = l.h;
            }
        }
        curdepth++;
        if(!guilayoutpass && visible() && ishit(xsize, ysize)) loopi(2) lists[curlist].mouse[i] = mouseaction[i]|GUI_ROLLOVER;
        if(merge)
        {
            mergelist = curlist;
            mergedepth = curdepth;
        }
    }

    int poplist()
    {
        if(!lists.inrange(curlist)) return 0;
        list &l = lists[curlist];
        if(guilayoutpass)
        {
            l.w = xsize;
            l.h = ysize;
        }
        curlist = l.parent;
        curdepth--;
        if(mergelist >= 0 && curdepth < mergedepth) mergelist = mergedepth = -1;
        if(lists.inrange(curlist))
        {
            int w = xsize, h = ysize;
            if(ishorizontal()) cury -= h; else curx -= w;
            list &p = lists[curlist];
            xsize = p.w;
            ysize = p.h;
            if(!guilayoutpass && p.springs > 0)
            {
                list &s = lists[p.parent];
                if(ishorizontal()) xsize = s.w; else ysize = s.h;
            }
            return layout(w, h);
        }
        return 0;
    }

    void setstatus(const char *fmt, int width, ...)
    {
        if(statusstr) DELETEA(statusstr);
        defvformatstring(str, width, fmt);
        statusstr = newstring(str);
        if(width) statuswidth = width;
    }

    void settooltip(const char *fmt, int width, ...)
    {
        if(tooltipforce) return; // overridden in code
        if(tooltipstr) DELETEA(tooltipstr);
        defvformatstring(str, width, fmt);
        tooltipstr = newstring(str);
        if(width) tooltipwidth = width;
    }

    void pushfont(const char *font) { ::pushfont(font); fontdepth++; }
    void popfont() { if(fontdepth) { ::popfont(); fontdepth--; } }

    int text(const char *text, int color, const char *icon, int icolor, int wrap)
    {
        return button_(text, color, icon, icolor, false, wrap, false);
    }
    int button(const char *text, int color, const char *icon, int icolor, int wrap, bool faded)
    {
        return button_(text, color, icon, icolor, true, wrap, faded);
    }

    void separator() { line_(guisepsize); }

    //use to set min size (useful when you have progress bars)
    void strut(float size) { layout(isvertical() ? int(size*FONTW) : 0, isvertical() ? 0 : int(size*FONTH)); }
    //add space between list items
    void space(float size) { layout(isvertical() ? 0 : int(size*FONTW), isvertical() ? int(size*FONTH) : 0); }

    void spring(int weight)
    {
        if(curlist < 0) return;
        list &l = lists[curlist];
        if(guilayoutpass) { if(l.parent >= 0) l.springs += weight; return; }
        int nextspring = min(l.curspring + weight, l.springs);
        if(nextspring <= l.curspring) return;
        if(ishorizontal())
        {
            int w = xsize - l.w;
            layout((w*nextspring)/l.springs - (w*l.curspring)/l.springs, 0);
        }
        else
        {
            int h = ysize - l.h;
            layout(0, (h*nextspring)/l.springs - (h*l.curspring)/l.springs);
        }
        l.curspring = nextspring;
    }

    int layout(int w, int h)
    {
        if(guilayoutpass)
        {
            if(ishorizontal())
            {
                xsize += w;
                ysize = max(ysize, h);
            }
            else
            {
                xsize = max(xsize, w);
                ysize += h;
            }
        }
        else
        {
            bool hit = ishit(w, h);
            if(ishorizontal()) curx += w;
            else cury += h;
            if(hit && visible()) return mouseaction[0]|GUI_ROLLOVER;
        }
        return 0;
    }

    bool ishit(int w, int h, int x = curx, int y = cury)
    {
        if(passthrough) return false;
        if(mergelist >= 0 && curdepth >= mergedepth && lists[mergelist].mouse[0]) return true;
        if(ishorizontal()) h = ysize;
        else w = xsize;
        return hitx>=x && hity>=y && hitx<x+w && hity<y+h;
    }

    int image(Texture *t, float scale, bool overlaid, int icolor)
    {
        if(scale == 0) scale = 1;
        int size = (int)(scale*2*FONTH)-guishadow;
        if(visible()) icon_(t, overlaid, curx, cury, size, ishit(size+guishadow, size+guishadow), icolor);
        return layout(size+guishadow, size+guishadow);
    }

    int texture(VSlot &vslot, float scale, bool overlaid)
    {
        if(scale==0) scale = 1;
        int size = (int)(scale*2*FONTH)-guishadow;
        if(visible()) previewslot(vslot, overlaid, curx, cury, size, ishit(size+guishadow, size+guishadow));
        return layout(size+guishadow, size+guishadow);
    }

    int playerpreview(int model, int color, int team, int weap, const char *vanity, float sizescale, bool overlaid, float scale, float blend)
    {
        if(sizescale==0) sizescale = 1;
        int size = (int)(sizescale*2*FONTH)-guishadow;
        if(visible())
        {
            bool hit = ishit(size+guishadow, size+guishadow);
            float xs = size, ys = size, xi = curx, yi = cury, xpad = 0, ypad = 0;
            if(overlaid)
            {
                xpad = xs/32;
                ypad = ys/32;
                xi += xpad;
                yi += ypad;
                xs -= 2*xpad;
                ys -= 2*ypad;
            }
            int x1 = int(floor(screen->w*(xi*uiscale.x+uiorigin.x))), y1 = int(floor(screen->h*(1 - ((yi+ys)*uiscale.y+uiorigin.y)))),
                x2 = int(ceil(screen->w*((xi+xs)*uiscale.x+uiorigin.x))), y2 = int(ceil(screen->h*(1 - (yi*uiscale.y+uiorigin.y))));
            glViewport(x1, y1, x2-x1, y2-y1);
            glScissor(x1, y1, x2-x1, y2-y1);
            glEnable(GL_SCISSOR_TEST);
            glDisable(GL_BLEND);
            modelpreview::start(overlaid);
            game::renderplayerpreview(model, color, team, weap, vanity, scale, blend);
            modelpreview::end();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, screen->w, screen->h);
            if(overlaid)
            {
                if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
                const vec &ocolor = hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1);
                glColor3fv(ocolor.v);
                glBindTexture(GL_TEXTURE_2D, overlaytex->id);
                rect_(xi - xpad, yi - ypad, xs + 2*xpad, ys + 2*ypad, 0);
            }
        }
        return layout(size+guishadow, size+guishadow);
    }

    int modelpreview(const char *name, int anim, float sizescale, bool overlaid, float scale, float blend)
    {
        if(sizescale==0) sizescale = 1;
        int size = (int)(sizescale*2*FONTH)-guishadow;
        if(visible())
        {
            bool hit = ishit(size+guishadow, size+guishadow);
            float xs = size, ys = size, xi = curx, yi = cury, xpad = 0, ypad = 0;
            if(overlaid)
            {
                xpad = xs/32;
                ypad = ys/32;
                xi += xpad;
                yi += ypad;
                xs -= 2*xpad;
                ys -= 2*ypad;
            }
            int x1 = int(floor(screen->w*(xi*uiscale.x+uiorigin.x))), y1 = int(floor(screen->h*(1 - ((yi+ys)*uiscale.y+uiorigin.y)))),
                x2 = int(ceil(screen->w*((xi+xs)*uiscale.x+uiorigin.x))), y2 = int(ceil(screen->h*(1 - (yi*uiscale.y+uiorigin.y))));
            glViewport(x1, y1, x2-x1, y2-y1);
            glScissor(x1, y1, x2-x1, y2-y1);
            glEnable(GL_SCISSOR_TEST);
            glDisable(GL_BLEND);
            modelpreview::start(overlaid);
            model *m = loadmodel(name);
            if(m)
            {
                entitylight light;
                light.color = vec(1, 1, 1);
                light.dir = vec(0, -1, 2).normalize();
                vec center, radius;
                m->boundbox(center, radius);
                float dist =  2.0f*max(radius.magnitude2(), 1.1f*radius.z),
                      yaw = fmod(lastmillis/10000.0f*360.0f, 360.0f);
                vec o(-center.x, dist - center.y, -0.1f*dist - center.z);
                rendermodel(&light, name, anim, o, yaw, 0, 0, NULL, NULL, 0, scale, blend);
            }
            modelpreview::end();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, screen->w, screen->h);
            if(overlaid)
            {
                if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
                const vec &ocolor = hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1);
                glColor3fv(ocolor.v);
                glBindTexture(GL_TEXTURE_2D, overlaytex->id);
                rect_(xi - xpad, yi - ypad, xs + 2*xpad, ys + 2*ypad, 0);
            }
        }
        return layout(size+guishadow, size+guishadow);
    }

    int slice(Texture *t, float scale, float start, float end, const char *text)
    {
        if(scale == 0) scale = 1;
        int size = (int)(scale*2*FONTH);
        if(t!=notexture && visible()) slice_(t, curx, cury, size, start, end, text);
        return layout(size, size);
    }

    void progress(float percent, float scale)
    {
        if(scale == 0) scale = 1;
        int size = (int)(scale*2*FONTH);
        slice_(textureload(hud::progringtex, 3, true, false), curx, cury, size, (SDL_GetTicks()%1000)/1000.f, 0.1f);
        string s; if(percent > 0) formatstring(s, "\fg%d%%", int(percent*100)); else formatstring(s, "\fg...");
        slice_(textureload(hud::progresstex, 3, true, false), curx, cury, size, 0, percent, s);
        layout(size, size);
    }

    int slider(int &val, int vmin, int vmax, int colour, const char *label, bool reverse, bool scroll, int style, int scolour)
    {
        int x = curx, y = cury;
        float percent = (val-vmin)/float(max(vmax-vmin, 1));
        bool hit = false;
        int space = slider_(guislidersize, percent, ishorizontal() ? FONTW*3 : FONTH, hit, style, scolour);
        if(visible())
        {
            if(hit)
            {
                if(!label) label = intstr(val);
                settooltip("\f[%d]%s", -1, colour, label);
                tooltipforce = true;
                if(mouseaction[0]&GUI_PRESSED)
                {
                    int vnew = vmax-vmin+1;
                    if(ishorizontal()) vnew = int((vnew*(reverse ? hity-y-guislidersize/2 : y+ysize-guislidersize/2-hity))/(ysize-guislidersize));
                    else vnew = int((vnew*(reverse ? x+xsize-guislidersize/2-hitx : hitx-x-guislidersize/2))/(xsize-guislidersize));
                    vnew += vmin;
                    vnew = clamp(vnew, vmin, vmax);
                    if(vnew != val) val = vnew;
                }
                else if(mouseaction[1]&GUI_UP)
                {
                    int vval = val+(reverse == !(mouseaction[1]&GUI_ALT) ? -1 : 1),
                        vnew = clamp(vval, vmin, vmax);
                    if(vnew != val) val = vnew;
                }
            }
            else if(scroll && lists[curlist].mouse[1]&GUI_UP)
            {
                int vval = val+(reverse == !(lists[curlist].mouse[1]&GUI_ALT) ? -1 : 1),
                    vnew = clamp(vval, vmin, vmax);
                if(vnew != val) val = vnew;
            }
        }
        return space;
    }

    char *field(const char *name, int color, int length, int height, const char *initval, int initmode, bool focus, const char *parent, const char *prompt, bool immediate)
    {
        return field_(name, color, length, height, initval, initmode, FIELDEDIT, focus, parent, prompt, immediate);
    }

    char *keyfield(const char *name, int color, int length, int height, const char *initval, int initmode, bool focus, const char *parent, const char *prompt, bool immediate)
    {
        return field_(name, color, length, height, initval, initmode, FIELDKEY, focus, parent, prompt, immediate);
    }

    char *field_(const char *name, int color, int length, int height, const char *initval, int initmode, int fieldtype = FIELDEDIT, bool focus = false, const char *parent = NULL, const char *prompt = NULL, bool immediate = false)
    {
        editor *e = useeditor(name, initmode, false, initval, parent); // generate a new editor if necessary
        if(guilayoutpass)
        {
            if(initval && (e->lines.empty() || (e->mode == EDITORFOCUSED && (e != currentfocus() || fieldmode == FIELDSHOW) && strcmp(e->lines[0].text, initval))))
                e->clear(initval);
            e->linewrap = (length < 0);
            e->maxx = e->linewrap ? -1 : length;
            e->maxy = (height <= 0) ? 1 : -1;
            e->pixelwidth = abs(length)*FONTW;
            if(e->linewrap && e->maxy == 1)
            {
                int temp = 0;
                text_bounds(e->lines[0].text, temp, e->pixelheight, e->pixelwidth); //only single line editors can have variable height
            }
            else e->pixelheight = FONTH*max(height, 1);
        }
        int h = e->pixelheight, hpad = FONTH/4, w = e->pixelwidth, wpad = FONTW;
        h += hpad;
        w += wpad;

        bool wasvertical = isvertical();
        if(wasvertical && e->maxy != 1) pushlist(false);

        char *result = NULL;
        if(visible())
        {
            e->rendered = true;
            if(focus && e->unfocus) focus = false;
            bool hit = ishit(w, h) && e->mode != EDITORREADONLY, clrs = fieldtype == FIELDKEY,
                 editing = (fieldmode != FIELDSHOW) && e == currentfocus() && e->mode != EDITORREADONLY;
            if(mouseaction[0]&GUI_UP && mergedepth >= 0 && hit) mouseaction[0] &= ~GUI_UP;
            if(mouseaction[0]&GUI_DOWN) //mouse request focus
            {
                if(hit)
                {
                    focus = true;
                    if(mouseaction[0]&GUI_ALT) clrs = true;
                    if(e->unfocus) e->unfocus = false;
                }
                else if(editing) fieldmode = FIELDCOMMIT;
            }
            if(focus)
            {
                if(clrs) e->clear();
                useeditor(e->name, initmode, true, initval, parent);
                e->mark(false);
                if(fieldmode != FIELDCOMMIT && fieldmode != FIELDABORT) fieldmode = fieldtype;
            }
            if(hit && editing && (mouseaction[0]&GUI_PRESSED) && fieldtype == FIELDEDIT)
                e->hit(int(floor(hitx-(curx+wpad/2))), int(floor(hity-(cury+hpad/2))), (mouseaction[0]&GUI_DRAGGED)!=0); //mouse request position
            if(editing && (fieldmode == FIELDCOMMIT || fieldmode == FIELDABORT)) // commit field if user pressed enter
            {
                if(fieldmode == FIELDCOMMIT) result = e->currentline().text;
                e->active = (e->mode != EDITORFOCUSED);
                fieldmode = FIELDSHOW;
            }
            else
            {
                if(editing && immediate) result = e->currentline().text;
                fieldsactive = true;
            }
            skin(curx, cury, curx+w, cury+h, guifieldbgcolour, guifieldbgblend, editing ? guifieldactivecolour : guifieldbordercolour, editing ? guifieldactiveblend : guifieldborderblend, true);
            e->draw(curx+wpad/2, cury+hpad/2, color, editing, prompt);
        }
        else if(e->unfocus) e->unfocus = false;
        layout(w, h);
        if(e->maxy != 1)
        {
            int slines = e->limitscrolly();
            if(slines > 0)
            {
                int oldpos = e->scrolly == editor::SCROLLEND ? slines : e->scrolly, newpos = oldpos;
                slider(newpos, 0, slines, color, NULL, true, true, 0, -1);
                if(oldpos != newpos)
                {
                    e->cy = newpos;
                    e->scrolly = e->mode == EDITORREADONLY && newpos >= slines ? editor::SCROLLEND : newpos;
                }
            }
            if(wasvertical) poplist();
        }
        return result;
    }

    void rect_(float x, float y, float w, float h, bool lines = false)
    {
        glBegin(lines ? GL_LINE_LOOP : GL_TRIANGLE_STRIP);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        if(lines) glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        if(!lines) glVertex2f(x + w, y + h);
        glEnd();
        xtraverts += 4;
    }

    void rect_(float x, float y, float w, float h, int usetc)
    {
        glBegin(GL_TRIANGLE_STRIP);
        static const GLfloat tc[5][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}};
        glTexCoord2fv(tc[usetc]); glVertex2f(x, y);
        glTexCoord2fv(tc[usetc+1]); glVertex2f(x + w, y);
        glTexCoord2fv(tc[usetc+3]); glVertex2f(x, y + h);
        glTexCoord2fv(tc[usetc+2]); glVertex2f(x + w, y + h);
        glEnd();
        xtraverts += 4;
    }

    void text_(const char *text, int x, int y, int color, int alpha, bool shadow, bool force = false, int wrap = -1)
    {
        if(shadow) draw_text(text, x+guishadow, y+guishadow, 0x00, 0x00, 0x00, -0xC0*alpha/255, TEXT_NO_INDENT, -1, wrap > 0 ? wrap : -1);
        draw_text(text, x, y, color>>16, (color>>8)&0xFF, color&0xFF, force ? -alpha : alpha, TEXT_NO_INDENT, -1, wrap > 0 ? wrap : -1);
    }

    void fill(int color, int inheritw, int inherith)
    {
        if(!visible()) return;
        notextureshader->set();
        glColor4ub(color>>16, (color>>8)&0xFF, color&0xFF, 0x80);
        int w = xsize, h = ysize;
        if(inheritw>0)
        {
            int parentw = curlist, parentdepth = 0;
            for(;parentdepth < inheritw && lists[parentw].parent>=0; parentdepth++)
                parentw = lists[parentw].parent;
            list &p = lists[parentw];
            w = p.springs > 0 && (curdepth-parentdepth)&1 ? lists[p.parent].w : p.w;
        }
        if(inherith>0)
        {
            int parenth = curlist, parentdepth = 0;
            for(;parentdepth < inherith && lists[parenth].parent>=0; parentdepth++)
                parenth = lists[parenth].parent;
            list &p = lists[parenth];
            h = p.springs > 0 && !((curdepth-parentdepth)&1) ? lists[p.parent].h : p.h;
        }
        rect_(curx, cury, w, h, false);
        defaultshader->set();
    }

    void outline(int color, int inheritw, int inherith, int offsetx, int offsety)
    {
        if(!visible()) return;
        notextureshader->set();
        glColor4ub(color>>16, (color>>8)&0xFF, color&0xFF, 0x80);
        int w = xsize, h = ysize;
        if(inheritw>0)
        {
            int parentw = curlist, parentdepth = 0;
            for(;parentdepth < inheritw && lists[parentw].parent>=0; parentdepth++)
                parentw = lists[parentw].parent;
            list &p = lists[parentw];
            w = p.springs > 0 && (curdepth-parentdepth)&1 ? lists[p.parent].w : p.w;
        }
        if(inherith>0)
        {
            int parenth = curlist, parentdepth = 0;
            for(;parentdepth < inherith && lists[parenth].parent>=0; parentdepth++)
                parenth = lists[parenth].parent;
            list &p = lists[parenth];
            h = p.springs > 0 && !((curdepth-parentdepth)&1) ? lists[p.parent].h : p.h;
        }
        rect_(curx+offsetx, cury+offsety, w-offsetx*2, h-offsety*2, true);
        defaultshader->set();
    }

    void background(int colour1, float blend1, int colour2, float blend2, bool skinborder, int inheritw, int inherith)
    {
        if(!visible()) return;
        int w = xsize, h = ysize;
        if(inheritw>0)
        {
            int parentw = curlist, parentdepth = 0;
            for(;parentdepth < inheritw && lists[parentw].parent>=0; parentdepth++)
                parentw = lists[parentw].parent;
            list &p = lists[parentw];
            w = p.springs > 0 && (curdepth-parentdepth)&1 ? lists[p.parent].w : p.w;
        }
        if(inherith>0)
        {
            int parenth = curlist, parentdepth = 0;
            for(;parentdepth < inherith && lists[parenth].parent>=0; parentdepth++)
                parenth = lists[parenth].parent;
            list &p = lists[parenth];
            h = p.springs > 0 && !((curdepth-parentdepth)&1) ? lists[p.parent].h : p.h;
        }
        skin(curx, cury, curx+w, cury+h, colour1, blend1, colour2, blend2, skinborder);
    }

    void icon_(Texture *t, bool overlaid, int x, int y, int size, bool hit, int icolor)
    {
        static const float tc[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        float xs = 0, ys = 0;
        int textureid = -1;
        if(t)
        {
            float scale = float(size)/max(t->xs, t->ys); //scale and preserve aspect ratio
            xs = t->xs*scale;
            ys = t->ys*scale;
            x += int((size-xs)/2);
            y += int((size-ys)/2);
            textureid = t->id;
        }
        else
        {
            if(lightmapping && lmprogtex)
            {
                float scale = float(size)/256; //scale and preserve aspect ratio
                xs = 256*scale; ys = 256*scale;
                x += int((size-xs)/2);
                y += int((size-ys)/2);
                textureid = lmprogtex;
            }
            else
            {
                defformatstring(texname, "%s", mapname);
                if((t = textureload(texname, 3, true, false)) == notexture) t = textureload(emblemtex, 3, true, false);
                float scale = float(size)/max(t->xs, t->ys); //scale and preserve aspect ratio
                xs = t->xs*scale; ys = t->ys*scale;
                x += int((size-xs)/2);
                y += int((size-ys)/2);
                textureid = t->id;
            }
        }
        float xi = x, yi = y, xpad = 0, ypad = 0;
        if(overlaid)
        {
            xpad = xs/32;
            ypad = ys/32;
            xi += xpad;
            yi += ypad;
            xs -= 2*xpad;
            ys -= 2*ypad;
        }
        if(hit && hitfx)
        {
            float offx = xs*guihoverscale, offy = ys*guihoverscale;
            if(!hovertex) hovertex = textureload(guihovertex, 3, true, false);
            glBindTexture(GL_TEXTURE_2D, hovertex->id);
            glColor4f((guihovercolour>>16)/255.f, ((guihovercolour>>8)&0xFF)/255.f, (guihovercolour&0xFF)/255.f, guihoverblend);
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2fv(tc[0]); glVertex2f(xi-offx,    yi-offy);
            glTexCoord2fv(tc[1]); glVertex2f(xi+xs+offx, yi-offy);
            glTexCoord2fv(tc[3]); glVertex2f(xi-offx,    yi+ys+offy);
            glTexCoord2fv(tc[2]); glVertex2f(xi+xs+offx, yi+ys+offy);
            glEnd();
        }
        glBindTexture(GL_TEXTURE_2D, textureid);
        vec color = vec::hexcolor(icolor);
        //if(hit && hitfx && !overlaid) color.div(2);
        glColor3fv(color.v);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2fv(tc[0]); glVertex2f(xi,    yi);
        glTexCoord2fv(tc[1]); glVertex2f(xi+xs, yi);
        glTexCoord2fv(tc[3]); glVertex2f(xi,    yi+ys);
        glTexCoord2fv(tc[2]); glVertex2f(xi+xs, yi+ys);
        glEnd();
        if(overlaid)
        {
            if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
            const vec &ocolor = hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1);
            glColor3fv(ocolor.v);
            glBindTexture(GL_TEXTURE_2D, overlaytex->id);
            rect_(xi - xpad, yi - ypad, xs + 2*xpad, ys + 2*ypad, 0);
        }
    }

    void previewslot(VSlot &vslot, bool overlaid, int x, int y, int size, bool hit)
    {
        Slot &slot = *vslot.slot;
        if(slot.sts.empty()) return;
        VSlot *layer = NULL;
        Texture *t = NULL, *glowtex = NULL, *layertex = NULL;
        if(slot.loaded)
        {
            t = slot.sts[0].t;
            if(t == notexture) return;
            Slot &slot = *vslot.slot;
            if(slot.texmask&(1<<TEX_GLOW)) { loopvj(slot.sts) if(slot.sts[j].type==TEX_GLOW) { glowtex = slot.sts[j].t; break; } }
            if(vslot.layer)
            {
                layer = &lookupvslot(vslot.layer);
                if(!layer->slot->sts.empty()) layertex = layer->slot->sts[0].t;
            }
        }
        else if(slot.thumbnail && slot.thumbnail != notexture) t = slot.thumbnail;
        else return;
        float xt = min(1.0f, t->xs/(float)t->ys), yt = min(1.0f, t->ys/(float)t->xs), xs = size, ys = size;
        float xi = x, yi = y, xpad = 0, ypad = 0;
        if(overlaid)
        {
            xpad = xs/32;
            ypad = ys/32;
            xi += xpad;
            yi += ypad;
            xs -= 2*xpad;
            ys -= 2*ypad;
        }
        SETSHADER(rgbonly);
        const vec &color = hit && hitfx && !overlaid ? vec(1.25f, 1.25f, 1.25f) : vec(1, 1, 1);
        float tc[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        int xoff = vslot.offset.x, yoff = vslot.offset.y;
        if(vslot.rotation)
        {
            if((vslot.rotation&5) == 1) { swap(xoff, yoff); loopk(4) swap(tc[k][0], tc[k][1]); }
            if(vslot.rotation >= 2 && vslot.rotation <= 4) { xoff *= -1; loopk(4) tc[k][0] *= -1; }
            if(vslot.rotation <= 2 || vslot.rotation == 5) { yoff *= -1; loopk(4) tc[k][1] *= -1; }
        }
        loopk(4) { tc[k][0] = tc[k][0]/xt - float(xoff)/t->xs; tc[k][1] = tc[k][1]/yt - float(yoff)/t->ys; }
        if(slot.loaded)
        {
            vec colorscale = vslot.getcolorscale();
            glColor3f(color.x*colorscale.x, color.y*colorscale.y, color.z*colorscale.z);
        }
        else glColor3fv(color.v);
        glBindTexture(GL_TEXTURE_2D, t->id);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2fv(tc[0]); glVertex2f(xi,    yi);
        glTexCoord2fv(tc[1]); glVertex2f(xi+xs, yi);
        glTexCoord2fv(tc[3]); glVertex2f(xi,    yi+ys);
        glTexCoord2fv(tc[2]); glVertex2f(xi+xs, yi+ys);
        glEnd();
        if(glowtex)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBindTexture(GL_TEXTURE_2D, glowtex->id);
            vec glowcolor = vslot.getglowcolor();
            if(hit || overlaid) glColor3f(color.x*glowcolor.x, color.y*glowcolor.y, color.z*glowcolor.z);
            else glColor3fv(glowcolor.v);
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2fv(tc[0]); glVertex2f(x,    y);
            glTexCoord2fv(tc[1]); glVertex2f(x+xs, y);
            glTexCoord2fv(tc[3]); glVertex2f(x,    y+ys);
            glTexCoord2fv(tc[2]); glVertex2f(x+xs, y+ys);
            glEnd();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        if(layertex)
        {
            vec layerscale = layer->getcolorscale();
            glBindTexture(GL_TEXTURE_2D, layertex->id);
            glColor3f(color.x*layerscale.x, color.y*layerscale.y, color.z*layerscale.z);
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2fv(tc[0]); glVertex2f(x+xs/2, y+ys/2);
            glTexCoord2fv(tc[1]); glVertex2f(x+xs,   y+ys/2);
            glTexCoord2fv(tc[3]); glVertex2f(x+xs/2, y+ys);
            glTexCoord2fv(tc[2]); glVertex2f(x+xs,   y+ys);
            glEnd();
        }

        defaultshader->set();
        if(overlaid)
        {
            if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
            const vec &ocolor = hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1);
            glColor3fv(ocolor.v);
            glBindTexture(GL_TEXTURE_2D, overlaytex->id);
            rect_(xi - xpad, yi - ypad, xs + 2*xpad, ys + 2*ypad, 0);
        }
    }

    void slice_(Texture *t, int x, int y, int size, float start = 0, float end = 1, const char *text = NULL)
    {
        float scale = float(size)/max(t->xs, t->ys), xs = t->xs*scale, ys = t->ys*scale, fade = 1;
        if(start == end) { end = 1; fade = 0.5f; }
        glBindTexture(GL_TEXTURE_2D, t->id);
        glColor4f(1, 1, 1, fade);
        int s = max(xs,ys)/2;
        drawslice(start, end, x+s/2, y+s/2, s);
        if(text && *text)
        {
            int w = text_width(text);
            text_(text, x+s/2-w/2, y+s/2-FONTH/2, 0xFFFFFF, guitextblend, false);
        }
    }

    int slider_(int size, float percent, int space, bool &hit, int style, int colour)
    {
        space = max(max(space, FONTW), size);
        if(visible())
        {
            int x = ishorizontal() ? curx+space/2-size/2 : curx, w = ishorizontal() ? size : xsize,
                y = ishorizontal() ? cury : cury+space/2-size/2, h = ishorizontal() ? ysize : size;
            hit = ishit(w, h, x, y);
            skin(x, y, x+w, y+h, guislidercolour, guisliderblend, hit ? guislideractivecolour : guisliderbordercolour, hit ? guislideractiveblend : guisliderborderblend, guisliderborderskin >= (hit ? 1 : 2));
            if(percent >= 0 && percent <= 1)
            {
                int px = x+size/8, py = y+size/8, pw = size*3/4, ph = size*3/4;
                switch(style)
                {
                    case 1:
                        if(ishorizontal()) ph = (h-size/4)*percent;
                        else pw = (w-size/4)*percent;
                        break;
                    case 0: default:
                        if(ishorizontal()) py += (h-size)*percent;
                        else px += (w-size)*percent;
                        break;
                }
                skin(px, py, px+pw, py+ph, colour >= 0 ? colour : guislidermarkcolour, guislidermarkblend, hit ? guislideractivecolour : guislidermarkbordercolour, hit ? guislideractiveblend : guislidermarkborderblend, guislidermarkborderskin >= (hit ? 1 : 2));
            }
        }
        layout(ishorizontal() ? space : 0, ishorizontal() ? 0 : space);
        return space;
    }

    int line_(int size, int space = 0)
    {
        space = max(max(space, FONTW), size);
        if(visible())
        {
            int colour1 = guibgcolour >= 0 ? guibgcolour : (guibordercolour >= 0 ? guibordercolour : 0x000000),
                colour2 = guibordercolour >= 0 ? guibordercolour : 0x808080,
                x1 = ishorizontal() ? curx+space/2-size/2 : curx, x2 = x1+(ishorizontal() ? size : xsize),
                y1 = ishorizontal() ? cury : cury+space/2-size/2, y2 = y1+(ishorizontal() ? ysize : size);
            if(colour1 >= 0)
            {
                notextureshader->set();
                glColor4f((colour1>>16)/255.f, ((colour1>>8)&0xFF)/255.f, (colour1&0xFF)/255.f, guibgblend);
                glBegin(GL_TRIANGLE_STRIP);
                glVertex2f(x1, y1);
                glVertex2f(x2, y1);
                glVertex2f(x1, y2);
                glVertex2f(x2, y2);
                xtraverts += 4;
                glEnd();
                defaultshader->set();
            }
            if(colour2 >= 0)
            {
                notextureshader->set();
                glColor4f((colour2>>16)/255.f, ((colour2>>8)&0xFF)/255.f, (colour2&0xFF)/255.f, guiborderblend);
                glBegin(GL_LINE_LOOP);
                glVertex2f(x1, y1);
                glVertex2f(x2, y1);
                glVertex2f(x2, y2);
                glVertex2f(x1, y2);
                xtraverts += 4;
                glEnd();
                defaultshader->set();
            }
        }
        layout(ishorizontal() ? space : 0, ishorizontal() ? 0 : space);
        return space;
    }

    int button_(const char *text, int color, const char *icon, int icolor, bool clickable, int wrap = -1, bool faded = true)
    {
        int w = 0, h = 0;
        if(icon && *icon)
        {
            w += FONTH;
            if(text && *text) w += 8;
        }
        if(text && *text)
        {
            int tw = 0, th = 0;
            text_bounds(text, tw, th, wrap > 0 ? wrap : -1);
            w += tw;
            h += th;
        }

        if(visible())
        {
            bool hit = ishit(w, h), forcecolor = false;
            if(hit && hitfx && clickable) { forcecolor = true; color = guiactivecolour; }
            int x = curx;
            if(icon && *icon)
            {
                const char *tname = strstr(icon, "textures/") ? icon : makerelpath("textures", icon);
                icon_(textureload(tname, 3, true, false), false, x, cury, FONTH, clickable && hit, icolor);
                x += FONTH;
                if(text && *text) x += 8;
            }
            if(text && *text) text_(text, x, cury, color, (hit && hitfx) || !faded || !clickable ? guitextblend : guitextfade, hit && clickable, forcecolor, wrap > 0 ? wrap : -1);
        }
        return layout(w, h);
    }

    static Texture *skintex, *skinbordertex, *overlaytex, *exittex, *hovertex;

    vec uiorigin, uiscale;
    guicb *cb;

    static float basescale, maxscale;
    static bool passthrough;

    void adjustscale()
    {
        int w = xsize + FONTW*8, h = ysize + FONTH*6;
        float aspect = forceaspect ? 1.0f/forceaspect : float(screen->h)/float(screen->w), fit = 1.0f;
        if(w*aspect*basescale>1.0f) fit = 1.0f/(w*aspect*basescale);
        if(h*basescale*fit>maxscale) fit *= maxscale/(h*basescale*fit);
        uiscale = vec(aspect*uiscale.x*fit, uiscale.y*fit, 1);
        uiorigin = vec(0.5f - ((w-xsize)/2 - (FONTW*4))*uiscale.x, 0.5f + (0.5f*h-(FONTH*2))*uiscale.y, 0);
    }

    void start(int starttime, float initscale, int *tab, bool allowinput, bool wantstitle, bool wantsbgfx)
    {
        fontdepth = 0;
        gui::pushfont("reduced");
        initscale *= 0.025f;
        basescale = initscale;
        if(guilayoutpass)
            uiscale.x = uiscale.y = uiscale.z = guiscaletime ? min(basescale*(totalmillis-starttime)/float(guiscaletime), basescale) : basescale;
        needsinput = allowinput;
        hastitle = wantstitle;
        hasbgfx = wantsbgfx;
        passthrough = !allowinput;
        curdepth = curlist = mergedepth = mergelist = -1;
        tpos = ty = 0;
        tx = -FONTW;
        tcurrent = tab;
        tcolor = 0xFFFFFF;
        pushlist(false);
        if(guilayoutpass) nextlist = curlist;
        else
        {
            if(tcurrent && !*tcurrent) tcurrent = NULL;
            cury = -ysize;
            curx = -xsize/2;

            glPushMatrix();
            glTranslatef(uiorigin.x, uiorigin.y, uiorigin.z);
            glScalef(uiscale.x, uiscale.y, uiscale.z);
            if(hasbgfx)
            {
                int x1 = curx-FONTW, y1 = cury-FONTH/2, x2 = x1+xsize+FONTW*2, y2 = y1+ysize+FONTH;
                skin(x1, y1, x2, y2, guibgcolour, guibgblend, guibordercolour, guiborderblend);
            }
        }
    }

    void end()
    {
        if(guilayoutpass)
        {
            if(needsinput) uibuttons();
            xsize = max(tx, xsize);
            ysize = max(ty, ysize);
            ysize = max(ysize, FONTH);

            if(tcurrent) *tcurrent = max(1, min(*tcurrent, tpos));
            adjustscale();

            if(!passthrough)
            {
                hitx = (cursorx - uiorigin.x)/uiscale.x;
                hity = (cursory - uiorigin.y)/uiscale.y;
                if((mouseaction[0]&GUI_PRESSED) && (fabs(hitx-firstx) > 2 || fabs(hity - firsty) > 2)) mouseaction[0] |= GUI_DRAGGED;
            }
        }
        else
        {
            if(guistatusline && statusstr && *statusstr)
            {
                gui::pushfont("little");
                int width = 0, height = 0, tw = min(statuswidth ? statuswidth : (guistatuswidth ? guistatuswidth : -1), int(screen->w*(1/uiscale.y)));
                text_bounds(statusstr, width, height, tw, TEXT_CENTERED|TEXT_NO_INDENT);
                int w = width+FONTW*2, h = FONTH/2+height, x1 = -w/2, y1 = guispacesize, x2 = x1+w, y2 = y1+h;
                if(hasbgfx) skin(x1, y1, x2, y2, guibgcolour, guibgblend, guibordercolour, guiborderblend);
                draw_text(statusstr, x1+FONTW, y1+FONTH/4, 255, 255, 255, 255, TEXT_CENTERED|TEXT_NO_INDENT, -1, tw);
                gui::popfont();
            }
            if(needsinput) uibuttons();
            if((guitooltips || tooltipforce) && tooltipstr && *tooltipstr)
            {
                if(!tooltip || !lasttooltip || strcmp(tooltip, tooltipstr))
                {
                    if(tooltip) DELETEA(tooltip);
                    tooltip = newstring(tooltipstr);
                    lasttooltip = totalmillis;
                }
                if(tooltipforce || totalmillis-lasttooltip >= guitooltiptime)
                {
                    gui::pushfont("little");
                    int width, height, tw = min(tooltipwidth ? tooltipwidth : (guitooltipwidth ? guitooltipwidth : -1), int(screen->w*(1/uiscale.y)));
                    text_bounds(tooltipstr, width, height, tw, TEXT_NO_INDENT);
                    int w = width+FONTW*2, h = FONTH/2+height, x1 = hitx, y1 = hity-height-FONTH/2, x2 = x1+w, y2 = y1+h,
                        offset = totalmillis-lasttooltip-guitooltiptime;
                    float blend = tooltipforce ? 1.f : (offset > 0 ? (offset < guitooltipfade ? offset/float(guitooltipfade) : 1.f) : 0.f);
                    skin(x1, y1, x2, y2, guitooltipcolour, guitooltipblend*blend, guitooltipbordercolour, guitooltipborderblend*blend, guitooltipborderskin!=0);
                    draw_text(tooltip, x1+FONTW, y1+FONTH/4, 255, 255, 255, int(255*blend), TEXT_NO_INDENT, -1, tw);
                    gui::popfont();
                }
            }
            else
            {
                if(tooltip) DELETEA(tooltip);
                lasttooltip = 0;
            }
            glPopMatrix();
        }
        poplist();
        while(fontdepth) gui::popfont();
    }
};

Texture *gui::skintex = NULL, *gui::skinbordertex, *gui::overlaytex = NULL, *gui::exittex = NULL, *gui::hovertex = NULL;
TVARN(IDF_PERSIST|IDF_PRELOAD, guiskintex, "textures/guiskin", gui::skintex, 0);
TVARN(IDF_PERSIST|IDF_PRELOAD, guiskinbordertex, "textures/guiskinborder", gui::skinbordertex, 0);
TVARN(IDF_PERSIST|IDF_PRELOAD, guioverlaytex, "textures/guioverlay", gui::overlaytex, 0);
TVARN(IDF_PERSIST|IDF_PRELOAD, guiexittex, "textures/guiexit", gui::exittex, 0);
TVARN(IDF_PERSIST|IDF_PRELOAD, guihovertex, "textures/guihover", gui::hovertex, 0);

vector<gui::list> gui::lists;
float gui::basescale, gui::maxscale = 1, gui::hitx, gui::hity;
bool gui::passthrough, gui::hitfx = true;
int gui::curdepth, gui::fontdepth, gui::curlist, gui::xsize, gui::ysize, gui::curx, gui::cury, gui::mergelist, gui::mergedepth;
int gui::ty, gui::tx, gui::tpos, *gui::tcurrent, gui::tcolor;
static vector<gui> guis;

namespace UI
{
    bool isopen = false;

    bool keypress(int code, bool isdown, int cooked)
    {
        editor *e = currentfocus();
        if(fieldmode == FIELDKEY && e && e->mode != EDITORREADONLY)
        {
            switch(code)
            {
                case SDLK_ESCAPE:
                    if(isdown)
                    {
                        fieldmode = FIELDCOMMIT;
                        e->unfocus = true;
                    }
                    return true;
            }
            const char *keyname = getkeyname(code);
            if(keyname && isdown)
            {
                if(e->lines.length()!=1 || !e->lines[0].empty()) e->insert(" ");
                e->insert(keyname);
            }
            return true;
        }

        if(code<0) switch(code)
        { // fall-through-o-rama
            case -5: mouseaction[1] |= GUI_ALT;
            case -4: mouseaction[1] |= isdown ? GUI_DOWN : GUI_UP;
                if(active()) return true;
                break;
            case -3: mouseaction[0] |= GUI_ALT;
            case -1: mouseaction[0] |= (guiactionon=isdown) ? GUI_DOWN : GUI_UP;
                if(isdown) { firstx = gui::hitx; firsty = gui::hity; }
                if(active()) return true;
                break;
            case -2:
                if(isdown) cleargui(1);
                if(active()) return true;
            default: break;
        }

        if(fieldmode == FIELDSHOW || !e || e->mode == EDITORREADONLY) return false;
        switch(code)
        {
            case SDLK_ESCAPE: //cancel editing without commit
                if(isdown)
                {
                    fieldmode = FIELDABORT;
                    e->unfocus = true;
                }
                return true;
            case SDLK_RETURN:
            case SDLK_TAB:
                if(cooked && (e->maxy != 1)) break;
            case SDLK_KP_ENTER:
                if(isdown) fieldmode = FIELDCOMMIT; //signal field commit (handled when drawing field)
                return true;
            case SDLK_HOME:
            case SDLK_END:
            case SDLK_DELETE:
            case SDLK_BACKSPACE:
            case SDLK_UP:
            case SDLK_DOWN:
            case SDLK_LEFT:
            case SDLK_RIGHT:
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
                break;
            case SDLK_PAGEUP:
            case SDLK_PAGEDOWN:
            case -4:
            case -5:
                if(e->parent && *e->parent)
                { // pass input on to parent
                    editor *f = findeditor(e->parent);
                    if(f) e = f;
                }
                break;
            default:
                if(!cooked || (code<32)) return false;
                break;
        }
        if(!isdown) return true;
        e->key(code, cooked);
        return true;
    }

    bool active(bool pass) { return guis.length() && (!pass || needsinput); }
    void limitscale(float scale) {  gui::maxscale = scale; }

    void addcb(guicb *cb)
    {
        gui &g = guis.add();
        g.cb = cb;
        g.adjustscale();
    }

    void update()
    {
        bool p = active(false);
        if(isopen != p) uimillis = (isopen = p) ? totalmillis : -totalmillis;
        setsvar("guirollovername", "", true);
        setsvar("guirolloveraction", "", true);
        setsvar("guirollovertype", "", true);
    }

    void render()
    {
        if(guiactionon) mouseaction[0] |= GUI_PRESSED;

        gui::reset();
        guis.shrink(0);

        // call all places in the engine that may want to render a gui from here, they call addcb()
        if(progressing) progressmenu();
        else
        {
            texturemenu();
            hud::gamemenus();
            mainmenu();
        }

        readyeditors();
        bool wasfocused = (fieldmode!=FIELDSHOW);
        fieldsactive = false;

        needsinput = false;
        hastitle = hasbgfx = true;

        if(!guis.empty())
        {
            guilayoutpass = 1;
            //loopv(guis) guis[i].cb->gui(guis[i], true);
            guis.last().cb->gui(guis.last(), true);
            guilayoutpass = 0;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, 1, 1, 0, -1, 1);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            //loopvrev(guis) guis[i].cb->gui(guis[i], false);
            guis.last().cb->gui(guis.last(), false);

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();

            glDisable(GL_BLEND);
        }

        flusheditors();
        if(!fieldsactive) fieldmode = FIELDSHOW; //didn't draw any fields, so lose focus - mainly for menu closed
        if((fieldmode!=FIELDSHOW) != wasfocused)
        {
            SDL_EnableUNICODE(fieldmode!=FIELDSHOW);
            keyrepeat(fieldmode!=FIELDSHOW);
        }
        loopi(2) mouseaction[i] = 0;
    }

    editor *geteditor(const char *name, int mode, const char *init, const char *parent)
    {
        return useeditor(name, mode, false, init, parent);
    }

    void editorline(editor *e, const char *str, int limit)
    {
        if(!e) return;
        if(e->lines.length() != 1 || !e->lines[0].empty()) e->lines.add();
        e->lines.last().set(str);
        if(limit >= 0 && e->lines.length() > limit)
        {
            int n = e->lines.length()-limit;
            e->removelines(0, n);
            e->cy = max(e->cy - n, 0);
            if(e->scrolly != editor::SCROLLEND) e->scrolly = max(e->scrolly - n, 0);
        }
        e->mark(false);
    }

    void editorclear(editor *e, const char *init)
    {
        if(!e) return;
        e->clear(init);
    }

    void editoredit(editor *e)
    {
        if(!e) return;
        useeditor(e->name, e->mode, true);
        e->clear();
        fieldmode = FIELDEDIT;
    }
};

