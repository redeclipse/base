#include "engine.h"

int uimillis = -1;

VAR(IDF_READONLY, guilayoutpass, 1, 0, -1);
bool guiactionon = false;
int mouseaction[2] = {0};

static float firstx, firsty;

enum {FIELDCOMMIT, FIELDABORT, FIELDEDIT, FIELDSHOW, FIELDKEY};
static int fieldmode = FIELDSHOW;
static bool fieldsactive = false;

FVAR(IDF_PERSIST, guiscale, FVAR_NONZERO, 0.00055f, VAR_MAX);
FVAR(IDF_PERSIST, guitextscale, FVAR_NONZERO, 1, VAR_MAX);
VAR(IDF_PERSIST, guiskinsize, 0, 96, VAR_MAX); // 0 = texture size, otherwise = size in pixels for skin scaling
VAR(IDF_PERSIST, guislidersize, 1, 58, VAR_MAX);
VAR(IDF_PERSIST, guisepsize, 1, 6, VAR_MAX);
VAR(IDF_PERSIST, guispacesize, 1, 48, VAR_MAX);
VAR(IDF_PERSIST, guitooltipwidth, -1, -1, VAR_MAX);
VAR(IDF_PERSIST, guistatuswidth, -1, -1, VAR_MAX);

VAR(IDF_PERSIST, guishadow, 0, 2, 8);
VAR(IDF_PERSIST, guiclicktab, 0, 1, 1);
VAR(IDF_PERSIST, guitabborder, 0, 1, 2);
VAR(IDF_PERSIST, guitextblend, 1, 255, 255);
VAR(IDF_PERSIST, guitextfade, 1, 200, 255);
VAR(IDF_PERSIST, guiscaletime, 0, 250, VAR_MAX);

VAR(IDF_PERSIST, guiskinned, 0, 3, 3); // 0 = no backgrounds, 1 = drawn backgrounds, 2 = skinned backgrounds, 3 = skinned with overlay border

VAR(IDF_PERSIST|IDF_HEX, guibgcolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guibgblend, 0, 0.8f, 1);
VAR(IDF_PERSIST|IDF_HEX, guibordercolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guiborderblend, 0, 0.6f, 1);

VAR(IDF_PERSIST|IDF_HEX, guihovercolour, -1, 0xF0A0A0, 0xFFFFFF);
FVAR(IDF_PERSIST, guihoverscale, 0, 0.3f, 1);
FVAR(IDF_PERSIST, guihoverblend, 0, 0.9f, 1);

VAR(IDF_PERSIST, guistatusline, 0, 1, 1);
VAR(IDF_PERSIST, guitooltips, 0, 1, 1);
VAR(IDF_PERSIST, guitooltiptime, 0, 500, VAR_MAX);
VAR(IDF_PERSIST, guitooltipfade, 0, 500, VAR_MAX);
VAR(IDF_PERSIST|IDF_HEX, guitooltipcolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guitooltipblend, 0, 0.8f, 1);
VAR(IDF_PERSIST|IDF_HEX, guitooltipbordercolour, -1, 0x808080, 0xFFFFFF);
FVAR(IDF_PERSIST, guitooltipborderblend, 0, 0.6f, 1);
VAR(IDF_PERSIST, guitooltipborderskin, 0, 1, 1);

VAR(IDF_PERSIST|IDF_HEX, guifieldbgcolour, -1, 0x202020, 0xFFFFFF);
FVAR(IDF_PERSIST, guifieldbgblend, 0, 0.3f, 1);
VAR(IDF_PERSIST|IDF_HEX, guifieldbordercolour, -1, 0xA0A0A0, 0xFFFFFF);
FVAR(IDF_PERSIST, guifieldborderblend, 0, 0.6f, 1);

VAR(IDF_PERSIST|IDF_HEX, guifieldactivecolour, -1, 0xF04040, 0xFFFFFF);
FVAR(IDF_PERSIST, guifieldactiveblend, 0, 0.8f, 1);

VAR(IDF_PERSIST|IDF_HEX, guislidercolour, -1, 0x000000, 0xFFFFFF);
FVAR(IDF_PERSIST, guisliderblend, 0, 0.3f, 1);
VAR(IDF_PERSIST|IDF_HEX, guisliderbordercolour, -1, 0xC0C0C0, 0xFFFFFF);
FVAR(IDF_PERSIST, guisliderborderblend, 0, 0.6f, 1);
VAR(IDF_PERSIST, guisliderborderskin, 0, 2, 2);
VAR(IDF_PERSIST|IDF_HEX, guislidermarkcolour, -1, 0x808080, 0xFFFFFF);
FVAR(IDF_PERSIST, guislidermarkblend, 0, 0.5f, 1);
VAR(IDF_PERSIST|IDF_HEX, guislidermarkbordercolour, -1, 0xC0C0C0, 0xFFFFFF);
FVAR(IDF_PERSIST, guislidermarkborderblend, 0, 0.6f, 1);
VAR(IDF_PERSIST, guislidermarkborderskin, 0, 0, 2);
VAR(IDF_PERSIST|IDF_HEX, guislideractivecolour, -1, 0xF04040, 0xFFFFFF);
FVAR(IDF_PERSIST, guislideractiveblend, 0, 0.8f, 1);

VAR(IDF_PERSIST, guiactiveskin, 0, 1, 1);
VAR(IDF_PERSIST|IDF_HEX, guiactivecolour, -1, 0xF02020, 0xFFFFFF);

VAR(IDF_PERSIST|IDF_HEX, guicheckboxcolour, -1, 0x20F020, 0xFFFFFF);
VAR(IDF_PERSIST|IDF_HEX, guicheckboxtwocolour, -1, 0xF020F0, 0xFFFFFF);
VAR(IDF_PERSIST|IDF_HEX, guiradioboxcolour, -1, 0xF02020, 0xFFFFFF);

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
    static bool hitfx, skinfx;

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
    void allowskinfx(bool on) { skinfx = on; }
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
                            if(!skinbordertex) skinbordertex = textureload(guiskinbordertex, 0, true, false);
                            t = skinbordertex;
                            break;
                        case 0: default:
                            if(!skintex) skintex = textureload(guiskintex, 0, true, false);
                            t = skintex;
                            break;
                    }
                    drawskin(t, x1, y1, x2, y2, colour, blend, guiskinsize);
                }
                break;
            }
            case 1:
            {
                if(colour1 >= 0)
                {
                    hudnotextureshader->set();
                    gle::color(vec::hexcolor(colour1), blend1);
                    gle::defvertex(2);
                    gle::begin(GL_TRIANGLE_STRIP);
                    gle::attribf(x1, y1);
                    gle::attribf(x2, y1);
                    gle::attribf(x1, y2);
                    gle::attribf(x2, y2);
                    xtraverts += gle::end();
                    hudshader->set();
                }
                if(skinborder && colour2 >= 0)
                {
                    hudnotextureshader->set();
                    gle::color(vec::hexcolor(colour2), blend2);
                    gle::defvertex(2);
                    gle::begin(GL_LINE_LOOP);
                    gle::attribf(x1, y1);
                    gle::attribf(x2, y1);
                    gle::attribf(x2, y2);
                    gle::attribf(x1, y2);
                    xtraverts += gle::end();
                    hudshader->set();
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
            text_(name, x1+guispacesize/2, y1+guispacesize/8, tcolor, alpha);
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

    int text(const char *text, int color, const char *icon, int icolor, int wrap, bool faded, const char *oicon, int ocolor)
    {
        return button_(text, color, icon, icolor, false, wrap, faded, oicon, ocolor);
    }
    int button(const char *text, int color, const char *icon, int icolor, int wrap, bool faded, const char *oicon, int ocolor)
    {
        return button_(text, color, icon, icolor, true, wrap, faded, oicon, ocolor);
    }

    void separator(int size, int space, int colour, int border) { line_(size > 0 ? size : guisepsize, space > 0 ? space : 0, colour, border); }

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
            int x1 = int(floor(screenw*(xi*uiscale.x+uiorigin.x))), y1 = int(floor(screenh*(1 - ((yi+ys)*uiscale.y+uiorigin.y)))),
                x2 = int(ceil(screenw*((xi+xs)*uiscale.x+uiorigin.x))), y2 = int(ceil(screenh*(1 - (yi*uiscale.y+uiorigin.y))));
            glDisable(GL_BLEND);
            modelpreview::start(x1, y1, x2-x1, y2-y1, overlaid);
            game::renderplayerpreview(model, color, team, weap, vanity, scale, blend);
            modelpreview::end();
            hudshader->set();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            if(overlaid)
            {
                if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
                gle::color(hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1));
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
            int x1 = int(floor(screenw*(xi*uiscale.x+uiorigin.x))), y1 = int(floor(screenh*(1 - ((yi+ys)*uiscale.y+uiorigin.y)))),
                x2 = int(ceil(screenw*((xi+xs)*uiscale.x+uiorigin.x))), y2 = int(ceil(screenh*(1 - (yi*uiscale.y+uiorigin.y))));
            glDisable(GL_BLEND);
            modelpreview::start(x1, y1, x2-x1, y2-y1, overlaid);
            model *m = loadmodel(name);
            if(m)
            {
                entitylight light;
                light.color = vec(1, 1, 1);
                light.dir = vec(0, -1, 2).normalize();
                vec center, radius;
                m->boundbox(center, radius);
                float yaw;
                vec o = calcmodelpreviewpos(radius, yaw).sub(center);
                rendermodel(&light, name, anim, o, yaw, 0, 0, NULL, NULL, 0, scale, blend);
            }
            modelpreview::end();
            hudshader->set();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            if(overlaid)
            {
                if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
                gle::color(hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1));
                glBindTexture(GL_TEXTURE_2D, overlaytex->id);
                rect_(xi - xpad, yi - ypad, xs + 2*xpad, ys + 2*ypad, 0);
            }
        }
        return layout(size+guishadow, size+guishadow);
    }

    int prefabpreview(const char *prefab, const vec &color, float sizescale, bool overlaid)
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
            int x1 = int(floor(screenw*(xi*uiscale.x+uiorigin.x))), y1 = int(floor(screenh*(1 - ((yi+ys)*uiscale.y+uiorigin.y)))),
                x2 = int(ceil(screenw*((xi+xs)*uiscale.x+uiorigin.x))), y2 = int(ceil(screenh*(1 - (yi*uiscale.y+uiorigin.y))));
            glDisable(GL_BLEND);
            modelpreview::start(x1, y1, x2-x1, y2-y1, overlaid);
            previewprefab(prefab, color);
            modelpreview::end();
            hudshader->set();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            if(overlaid)
            {
                if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
                gle::color(hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1));
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
            e->pixelwidth = (abs(length)+1)*FONTW;
            if(e->linewrap && e->maxy == 1)
            {
                int temp = 0;
                text_bounds(e->lines[0].text, temp, e->pixelheight, 0, 0, e->pixelheight, TEXT_NO_INDENT); // only single line editors can have variable height
                int ph = e->pixelheight%FONTH;
                if(ph) e->pixelheight += FONTH-ph;
            }
            else e->pixelheight = max(height, 1)*FONTH;
        }
        int hpad = FONTH/4, wpad = FONTW, h = e->pixelheight+hpad, w = e->pixelwidth+wpad;

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
        gle::defvertex(2);
        gle::begin(lines ? GL_LINE_LOOP : GL_TRIANGLE_STRIP);
        gle::attribf(x, y);
        gle::attribf(x + w, y);
        if(lines) gle::attribf(x + w, y + h);
        gle::attribf(x, y + h);
        if(!lines) gle::attribf(x + w, y + h);
        xtraverts += gle::end();
    }

    void rect_(float x, float y, float w, float h, int usetc)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        static const vec2 tc[5] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1), vec2(0, 0) };
        gle::attribf(x, y); gle::attrib(tc[usetc]);
        gle::attribf(x + w, y); gle::attrib(tc[usetc+1]);
        gle::attribf(x, y + h); gle::attrib(tc[usetc+3]);
        gle::attribf(x + w, y + h); gle::attrib(tc[usetc+2]);
        xtraverts += gle::end();
    }

    void text_(const char *text, int x, int y, int color, int alpha, bool hit = false, int wrap = -1)
    {
        int flags = TEXT_NO_INDENT;
        if(hit)
        {
            if(guiactiveskin && skinfx)
            {
                flags |= TEXT_SKIN;
                color = guiactivecolour;
            }
            else if(hitfx) color = guiactivecolour;
        }
        draw_textx(text, x, y, 0, 0, color>>16, (color>>8)&0xFF, color&0xFF, alpha, flags, -1, wrap > 0 ? wrap : -1);
    }

    void fill(int color, int inheritw, int inherith)
    {
        if(!visible()) return;
        hudnotextureshader->set();
        gle::colorub(color>>16, (color>>8)&0xFF, color&0xFF, 0x80);
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
        hudshader->set();
    }

    void outline(int color, int inheritw, int inherith, int offsetx, int offsety)
    {
        if(!visible()) return;
        hudnotextureshader->set();
        gle::colorub(color>>16, (color>>8)&0xFF, color&0xFF, 0x80);
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
        hudshader->set();
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

    void icon_(Texture *t, bool overlaid, int x, int y, int size, bool hit, int icolor = 0xFFFFFF, Texture *o = NULL, int ocolor = 0xFFFFFF)
    {
        static const vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
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
        gle::defvertex(2);
        gle::deftexcoord0();
        if(hit && hitfx)
        {
            float offx = xs*guihoverscale, offy = ys*guihoverscale;
            if(!hovertex) hovertex = textureload(guihovertex, 3, true, false);
            glBindTexture(GL_TEXTURE_2D, hovertex->id);
            gle::color(vec::hexcolor(guihovercolour), guihoverblend);
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(xi-offx,    yi-offy);    gle::attrib(tc[0]);
            gle::attribf(xi+xs+offx, yi-offy);    gle::attrib(tc[1]);
            gle::attribf(xi-offx,    yi+ys+offy); gle::attrib(tc[3]);
            gle::attribf(xi+xs+offx, yi+ys+offy); gle::attrib(tc[2]);
            xtraverts += gle::end();
        }
        glBindTexture(GL_TEXTURE_2D, textureid);
        gle::color(vec::hexcolor(icolor), 1.f);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(xi,    yi);    gle::attrib(tc[0]);
        gle::attribf(xi+xs, yi);    gle::attrib(tc[1]);
        gle::attribf(xi,    yi+ys); gle::attrib(tc[3]);
        gle::attribf(xi+xs, yi+ys); gle::attrib(tc[2]);
        xtraverts += gle::end();
        if(o)
        {
            glBindTexture(GL_TEXTURE_2D, o->id);
            gle::color(vec::hexcolor(ocolor), 1.f);
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(xi,    yi);    gle::attrib(tc[0]);
            gle::attribf(xi+xs, yi);    gle::attrib(tc[1]);
            gle::attribf(xi,    yi+ys); gle::attrib(tc[3]);
            gle::attribf(xi+xs, yi+ys); gle::attrib(tc[2]);
            xtraverts += gle::end();
        }
        if(overlaid)
        {
            if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
            gle::color(hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1), 1.f);
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
        SETSHADER(hudrgb);
        gle::defvertex(2);
        gle::deftexcoord0();
        const vec &color = hit && hitfx && !overlaid ? vec(1.25f, 1.25f, 1.25f) : vec(1, 1, 1);
        vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
        int xoff = vslot.offset.x, yoff = vslot.offset.y;
        if(vslot.rotation)
        {
            if((vslot.rotation&5) == 1) { swap(xoff, yoff); loopk(4) swap(tc[k].x, tc[k].y); }
            if(vslot.rotation >= 2 && vslot.rotation <= 4) { xoff *= -1; loopk(4) tc[k].x *= -1; }
            if(vslot.rotation <= 2 || vslot.rotation == 5) { yoff *= -1; loopk(4) tc[k].y *= -1; }
        }
        loopk(4) { tc[k].x = tc[k].x/xt - xoff/t->xs; tc[k].y = tc[k].y/yt - yoff/t->ys; }
        if(slot.loaded) gle::color(vec(color).mul(vslot.getcolorscale()));
        else gle::color(color);
        glBindTexture(GL_TEXTURE_2D, t->id);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(xi,    yi);    gle::attrib(tc[0]);
        gle::attribf(xi+xs, yi);    gle::attrib(tc[1]);
        gle::attribf(xi,    yi+ys); gle::attrib(tc[3]);
        gle::attribf(xi+xs, yi+ys); gle::attrib(tc[2]);
        gle::end();
        if(glowtex)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBindTexture(GL_TEXTURE_2D, glowtex->id);
            vec glowcolor = vslot.getglowcolor();
            if(hit || overlaid) gle::color(vec(glowcolor).mul(color));
            else gle::color(glowcolor);
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(x,    y);    gle::attrib(tc[0]);
            gle::attribf(x+xs, y);    gle::attrib(tc[1]);
            gle::attribf(x,    y+ys); gle::attrib(tc[3]);
            gle::attribf(x+xs, y+ys); gle::attrib(tc[2]);
            gle::end();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        if(layertex)
        {
            glBindTexture(GL_TEXTURE_2D, layertex->id);
            gle::color(vec(color).mul(layer->getcolorscale()));
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(x+xs/2, y+ys/2); gle::attrib(tc[0]);
            gle::attribf(x+xs,   y+ys/2); gle::attrib(tc[1]);
            gle::attribf(x+xs/2, y+ys);   gle::attrib(tc[3]);
            gle::attribf(x+xs,   y+ys);   gle::attrib(tc[2]);
            gle::end();
        }

        hudshader->set();
        if(overlaid)
        {
            if(!overlaytex) overlaytex = textureload(guioverlaytex, 3, true, false);
            gle::color(hit && hitfx ? vec::hexcolor(guiactivecolour) : vec(1, 1, 1));
            glBindTexture(GL_TEXTURE_2D, overlaytex->id);
            rect_(xi - xpad, yi - ypad, xs + 2*xpad, ys + 2*ypad, 0);
        }
    }

    void slice_(Texture *t, int x, int y, int size, float start = 0, float end = 1, const char *text = NULL)
    {
        float scale = float(size)/max(t->xs, t->ys), xs = t->xs*scale, ys = t->ys*scale, fade = 1;
        if(start == end) { end = 1; fade = 0.5f; }
        glBindTexture(GL_TEXTURE_2D, t->id);
        gle::colorf(1, 1, 1, fade);
        int s = max(xs,ys)/2;
        drawslice(start, end, x+s/2, y+s/2, s);
        if(text && *text)
        {
            int w = text_width(text);
            text_(text, x+s/2-w/2, y+s/2-FONTH/2, 0xFFFFFF, guitextblend);
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

    int line_(int size, int space = 0, int colour = -1, int border = -1)
    {
        space = max(max(space, FONTW), size);
        if(visible())
        {
            int colour1 = colour >= 0 ? colour : (guibgcolour >= 0 ? guibgcolour : (guibordercolour >= 0 ? guibordercolour : 0x000000)),
                colour2 = border >= 0 ? border : (guibordercolour >= 0 ? guibordercolour : 0x808080),
                x1 = ishorizontal() ? curx+space/2-size/2 : curx, x2 = x1+(ishorizontal() ? size : xsize),
                y1 = ishorizontal() ? cury : cury+space/2-size/2, y2 = y1+(ishorizontal() ? ysize : size);
            if(colour1 >= 0)
            {
                hudnotextureshader->set();
                gle::color(vec::hexcolor(colour1), guibgblend);
                gle::defvertex(2);
                gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(x1, y1);
                gle::attribf(x2, y1);
                gle::attribf(x1, y2);
                gle::attribf(x2, y2);
                xtraverts += gle::end();
                hudshader->set();
            }
            if(colour2 >= 0)
            {
                hudnotextureshader->set();
                gle::color(vec::hexcolor(colour2), guiborderblend);
                gle::defvertex(2);
                gle::begin(GL_LINE_LOOP);
                gle::attribf(x1, y1);
                gle::attribf(x2, y1);
                gle::attribf(x2, y2);
                gle::attribf(x1, y2);
                xtraverts += gle::end();
                hudshader->set();
            }
        }
        layout(ishorizontal() ? space : 0, ishorizontal() ? 0 : space);
        return space;
    }

    int button_(const char *text, int color, const char *icon, int icolor, bool clickable, int wrap = -1, bool faded = true, const char *oicon = NULL, int ocolor = 0xFFFFFF)
    {
        int w = 0, h = 0;
        if((icon && *icon) || (oicon && *oicon))
        {
            w += FONTH;
            if(text && *text) w += 8;
        }
        if(text && *text)
        {
            int tw = 0, th = 0;
            text_bounds(text, tw, th, 0, 0, wrap > 0 ? wrap : -1);
            w += tw;
            h += th;
        }
        else h = FONTH;

        if(visible())
        {
            bool hit = ishit(w, h);
            int x = curx;
            if((icon && *icon) || (oicon && *oicon))
            {
                Texture *ttex = icon && *icon ? textureload(strstr(icon, "textures/") ? icon : makerelpath("textures", icon), 3, true, false) : NULL,
                    *otex = oicon && *oicon ? textureload(strstr(oicon, "textures/") ? oicon : makerelpath("textures", oicon), 3, true, false) : NULL;
                icon_(ttex, false, x, cury, FONTH, clickable && hit, icolor, otex, ocolor);
                x += FONTH;
                if(text && *text) x += 8;
            }
            if(text && *text) text_(text, x, cury, color, (hit && hitfx) || !faded || !clickable ? guitextblend : guitextfade, hit && clickable, wrap > 0 ? wrap : -1);
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
        int w = xsize+FONTW*8, h = ysize+FONTH*8;
        float aspect = forceaspect ? 1.0f/forceaspect : float(screenh)/float(screenw), fit = 1.0f;
        if(w*aspect*basescale>1.0f) fit = 1.0f/(w*aspect*basescale);
        if(h*basescale*fit>maxscale) fit *= maxscale/(h*basescale*fit);
        uiscale = vec(aspect*uiscale.x*fit, uiscale.y*fit, 1);
        uiorigin = vec(0.5f - ((w-xsize)/2 - (FONTW*4))*uiscale.x, 0.5f + (0.5f*h-(FONTH*4))*uiscale.y, 0);
    }

    void start(int starttime, int *tab, bool allowinput, bool wantstitle, bool wantsbgfx)
    {
        fontdepth = 0;
        gui::pushfont("default");
        basescale = guiscale;
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

            hudmatrix.ortho(0, 1, 1, 0, -1, 1);
            hudmatrix.translate(uiorigin);
            hudmatrix.scale(uiscale);

            resethudmatrix();
            hudshader->set();

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
                gui::pushfont("reduced");
                int width = 0, height = 0, tw = min(statuswidth ? statuswidth : (guistatuswidth ? guistatuswidth : -1), int(screenw*(1/uiscale.y))-FONTH*4);
                text_bounds(statusstr, width, height, 0, 0, tw, TEXT_CENTERED|TEXT_NO_INDENT);
                int w = width+FONTW*2, h = height+FONTH/2, x1 = -w/2, y1 = guispacesize, x2 = x1+w, y2 = y1+h;
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
                    int width, height, tw = min(tooltipwidth ? tooltipwidth : (guitooltipwidth ? guitooltipwidth : -1), int(screenw*(1/uiscale.y))-FONTH*4);
                    text_bounds(tooltipstr, width, height, 0, 0, tw, TEXT_NO_INDENT);
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
bool gui::passthrough, gui::hitfx = true, gui::skinfx = true;
int gui::curdepth, gui::fontdepth, gui::curlist, gui::xsize, gui::ysize, gui::curx, gui::cury, gui::mergelist, gui::mergedepth;
int gui::ty, gui::tx, gui::tpos, *gui::tcurrent, gui::tcolor;
static vector<gui> guis;

namespace UI
{
    bool isopen = false;

    bool textinput(const char *str, int len)
    {
        editor *e = currentfocus();
        if(fieldmode == FIELDKEY || fieldmode == FIELDSHOW || !e || e->mode == EDITORREADONLY) return false;

        e->input(str, len);
        return true;
    }

    bool keypress(int code, bool isdown)
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

        if(code < 0) switch(code)
        { // fall-through-o-rama
            case -5: mouseaction[1] |= GUI_ALT;
            case -4: mouseaction[1] |= isdown ? GUI_DOWN : GUI_UP;
                if(active()) return true;
                break;
            case -3: mouseaction[0] |= GUI_ALT;
            case -1: mouseaction[0] |= (guiactionon = isdown) ? GUI_DOWN : GUI_UP;
                if(isdown)
                {
                    firstx = gui::hitx;
                    firsty = gui::hity;
                }
                if(active()) return true;
                break;
            case -2:
                if(isdown) cleargui(1);
                if(active()) return true;
                break;
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
                return e->mode != EDITORFOREVER;
            case SDLK_RETURN:
            case SDLK_TAB:
                if(e->maxy != 1) break;
            case SDLK_KP_ENTER:
                if(isdown) fieldmode = FIELDCOMMIT; //signal field commit (handled when drawing field)
                return true;
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
        }
        if(isdown) e->key(code);
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
        float oldtextscale = curtextscale;
        curtextscale = guitextscale;
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

            //loopvrev(guis) guis[i].cb->gui(guis[i], false);
            guis.last().cb->gui(guis.last(), false);

            glDisable(GL_BLEND);
        }

        flusheditors();
        if(!fieldsactive) fieldmode = FIELDSHOW; //didn't draw any fields, so lose focus - mainly for menu closed
        if((fieldmode!=FIELDSHOW) != wasfocused)
        {
            textinput(fieldmode!=FIELDSHOW, TI_GUI);
            keyrepeat(fieldmode!=FIELDSHOW, KR_GUI);
        }
        loopi(2) mouseaction[i] = 0;
        curtextscale = oldtextscale;
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
        fieldmode = FIELDCOMMIT;
        e->unfocus = true;
    }
};
