#include "engine.h"
#include "textedit.h"

namespace UI
{
    static bool texgc = false;
    static int lastthumbnail = 0;
    vector<ident *> uiargs;

    FVAR(0, uitextscale, 1, 0, 0);

    SVAR(0, uiopencmd, "showui");
    SVAR(0, uiclosecmd, "hideui");

    TVAR(IDF_PERSIST|IDF_PRELOAD, uiloadtex, "<comp:1,1,2>loading", 3);

    VAR(0, uihidden, 0, 0, 1);
    FVAR(IDF_PERSIST, uiscale, FVAR_NONZERO, 1, 100);
    FVAR(IDF_PERSIST, uiworldscale, FVAR_NONZERO, 50, FVAR_MAX);

    FVAR(IDF_PERSIST, uimaxdist, 0, 2048, FVAR_MAX);
    FVAR(IDF_PERSIST, uimapmaxdist, 0, 2048, FVAR_MAX);

    VAR(IDF_PERSIST, uitextrows, 1, 48, VAR_MAX);
    void calctextscale() { uitextscale = 1.0f/uitextrows; }

    VAR(IDF_PERSIST, uiscrollsteptime, 0, 50, VAR_MAX);
    VAR(IDF_PERSIST, uislidersteptime, 0, 50, VAR_MAX);
    VAR(IDF_PERSIST, uislotviewtime, 0, 25, VAR_MAX);

    FVAR(IDF_PERSIST, uitipoffset, -1, 0.001f, 1);

    VAR(IDF_PERSIST, uihintintime, 0, 100, VAR_MAX);
    VAR(IDF_PERSIST, uihintholdtime, 0, 1000, VAR_MAX);
    VAR(IDF_PERSIST, uihintouttime, 0, 3000, VAR_MAX);
    FVAR(IDF_PERSIST, uihintoffset2d, -0.5f, 0.125f, 0.5f);
    FVAR(IDF_PERSIST, uihintoffset3d, -0.5f, 0.0625f, 0.5f);
    FVAR(IDF_PERSIST, uihintdist, 0.f, 56.f, FVAR_MAX);
    ICOMMANDV(0, uihinttime, uihintintime + uihintholdtime + uihintouttime);

    VARR(uilastmillis, 0);
    VARR(uitotalmillis, 0);
    VARR(uiclockmillis, 0);
    VARR(uiclockticks, 0);
    VARR(uicurtime, 0);

    static Texture *loadthumbnail(const char *name, int tclamp)
    {
        Texture *t = textureloaded(name);

        if(!t)
        {
            if(uiclockticks - lastthumbnail < uislotviewtime) t = textureload(uiloadtex, 0, true, false);
            else
            {
                t = textureload(name, tclamp, true, false, texgc);
                lastthumbnail = uiclockticks;
            }
        }

        return t;
    }

    #define SETSTR(dst, src) do { \
        if(dst) { if(dst != src && strcmp(dst, src)) { delete[] dst; dst = newstring(src); } } \
        else dst = newstring(src); \
    } while(0)

    static void quads(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
    {
        gle::attribf(x,   y);   gle::attribf(tx,    ty);
        gle::attribf(x+w, y);   gle::attribf(tx+tw, ty);
        gle::attribf(x+w, y+h); gle::attribf(tx+tw, ty+th);
        gle::attribf(x,   y+h); gle::attribf(tx,    ty+th);
    }

#if 0
    static void quad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);   gle::attribf(tx+tw, ty);
        gle::attribf(x,   y);   gle::attribf(tx,    ty);
        gle::attribf(x+w, y+h); gle::attribf(tx+tw, ty+th);
        gle::attribf(x,   y+h); gle::attribf(tx,    ty+th);
        gle::end();
    }
#endif

    static void quad(float x, float y, float w, float h, const vec2 tc[4])
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);   gle::attrib(tc[1]);
        gle::attribf(x,   y);   gle::attrib(tc[0]);
        gle::attribf(x+w, y+h); gle::attrib(tc[2]);
        gle::attribf(x,   y+h); gle::attrib(tc[3]);
        gle::end();
    }

    enum
    {
        ALIGN_MASK    = 0xF,

        ALIGN_HMASK   = 0x3,
        ALIGN_HSHIFT  = 0,
        ALIGN_HNONE   = 0,
        ALIGN_LEFT    = 1,
        ALIGN_HCENTER = 2,
        ALIGN_RIGHT   = 3,

        ALIGN_VMASK   = 0xC,
        ALIGN_VSHIFT  = 2,
        ALIGN_VNONE   = 0<<2,
        ALIGN_TOP     = 1<<2,
        ALIGN_VCENTER = 2<<2,
        ALIGN_BOTTOM  = 3<<2,

        CLAMP_MASK    = 0xF0,
        CLAMP_LEFT    = 0x10,
        CLAMP_RIGHT   = 0x20,
        CLAMP_TOP     = 0x40,
        CLAMP_BOTTOM  = 0x80,

        NO_ADJUST     = ALIGN_HNONE | ALIGN_VNONE,
        ALIGN_DEFAULT = ALIGN_HCENTER | ALIGN_VCENTER,
        ALIGN_WORLD   = ALIGN_HCENTER | ALIGN_TOP
    };

    enum
    {
        STATE_HOVER       = 1<<0,
        STATE_PRESS       = 1<<1,
        STATE_HOLD        = 1<<2,
        STATE_RELEASE     = 1<<3,
        STATE_ALT_PRESS   = 1<<4,
        STATE_ALT_HOLD    = 1<<5,
        STATE_ALT_RELEASE = 1<<6,
        STATE_ESC_PRESS   = 1<<7,
        STATE_ESC_HOLD    = 1<<8,
        STATE_ESC_RELEASE = 1<<9,
        STATE_SCROLL_UP   = 1<<10,
        STATE_SCROLL_DOWN = 1<<11,
        STATE_HIDDEN      = 1<<12,

        STATE_HOLD_MASK = STATE_HOLD | STATE_ALT_HOLD | STATE_ESC_HOLD,

        STATE_ALL = STATE_HOVER|STATE_PRESS|STATE_HOLD|STATE_RELEASE|STATE_ALT_PRESS|STATE_ALT_HOLD|STATE_ALT_RELEASE|STATE_ESC_PRESS|STATE_ESC_HOLD|STATE_ESC_RELEASE|STATE_SCROLL_UP|STATE_SCROLL_DOWN|STATE_HIDDEN
    };

    enum
    {
        SETSTATE_ANY = 0,
        SETSTATE_INSIDE,
        SETSTATE_FOCUSED
    };

    struct Object;
    struct Window;
    struct Surface;

    static Object *buildparent = NULL, *inputsteal = NULL;

    #define BUILD(type, o, setup, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            if(o) \
            { \
                setup; \
                o->buildchildren(contents); \
            } \
        } \
    } while(0)

    enum
    {
        CHANGE_SHADER = 1<<0,
        CHANGE_COLOR  = 1<<1,
        CHANGE_BLEND  = 1<<2
    };
    static Object *drawing = NULL;
    static bool propagating = false;

    enum { BLEND_ALPHA, BLEND_MOD, BLEND_SRC, BLEND_BUFFER, BLEND_GLOW, BLEND_MAX };
    static int changed = 0, surfacetype = -1, surfaceinput = SURFACE_FOREGROUND, surfaceformat = 0, blendtype = BLEND_ALPHA, blendtypedef = BLEND_ALPHA;
    static bool blendsep = false, blendsepdef = false;
    static float *surfacehint2d = NULL, *surfacehint3d = NULL;

    void setblend(int type, bool sep, bool force = false)
    {
        if(type < 0 || type >= BLEND_MAX) type = BLEND_ALPHA;

        const GLenum mappings[BLEND_MAX][4] {
            { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE },
            { GL_ZERO, GL_SRC_COLOR, GL_ONE_MINUS_DST_ALPHA, GL_ONE },
            { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO },
            { GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA },
            { GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA }
        };

        if(force || blendtype != type || blendsep != sep)
        {
            blendtype = type;
            blendsep = sep;
            if(sep) glBlendFuncSeparate_(mappings[type][0], mappings[type][1], mappings[type][2], mappings[type][3]);
            else glBlendFunc(mappings[type][0], mappings[type][1]);
        }
    }

    void resetblend(bool force = false)
    {
        if(force)
        {
            blendtypedef = BLEND_ALPHA;
            blendsepdef = true; // all roads lead to visor now
        }
        setblend(blendtypedef, blendsepdef, force);
    }

    #define UIREVCMDC(func, types, argtypes, body) \
        ICOMMAND(0, ui##func, types, argtypes, \
        { \
            for(Object *o = buildparent; o != NULL; o = o->parent) \
            { \
                body; \
                if(o->istype<Window>()) break; \
            } \
        });

    #define UIREVCMDW(func, types, argtypes, body) \
        ICOMMAND(0, ui##func, types, argtypes, \
        { \
            for(Object *o = buildparent; o != NULL; o = o->parent) \
            { \
                if(o->istype<Window>()) break; \
                body; \
            } \
        });

    #define UICMD(utype, uname, vname, valtype, args, body) \
        ICOMMAND(0, ui##uname##vname, valtype, args, { \
            if(buildparent && buildparent->istype<utype>()) \
            { \
                utype *o = (utype *)buildparent; \
                body; \
            } \
            else if(!propagating) conoutf(colourorange, "Warning: parent %s not a %s class for ui%s%s", buildparent ? buildparent->gettype() : "<null>", #utype, #uname, #vname); \
        });

    #define UIARGB(utype, uname, vname) \
        UICMD(utype, uname, vname, "iN$", (int *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = *val!=0; \
            else if(*numargs < 0) intret(o->vname ? 1 : 0); \
            else printvar(id, o->vname ? 1 : 0); \
        });

    #define UIARGK(utype, uname, vname, valtype, type, cmin, cmax, valdef) \
        UICMD(utype, uname, vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp((valdef), cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UIARGSCALED(utype, uname, vname, valtype, type, cmin, cmax) \
        UICMD(utype, uname, vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax) * uiscale); \
            else if(*numargs < 0) type##ret(o->vname * uiscale); \
            else print##type##var(id, o->vname * uiscale); \
        });

    #define UIARG(utype, uname, vname, valtype, type, cmin, cmax) \
        UICMD(utype, uname, vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UICMDT(utype, uname, vname, valtype, args, body) \
        ICOMMAND(0, ui##uname##vname, valtype, args, { \
            if(buildparent && buildparent->is##uname()) \
            { \
                utype *o = (utype *)buildparent; \
                body; \
            } \
            else if(!propagating) conoutf(colourorange, "Warning: parent %s not a %s type for ui%s%s", buildparent ? buildparent->gettype() : "<null>", #uname, #uname, #vname); \
        });

    #define UIARGTB(utype, uname, vname) \
        UICMDT(utype, uname, vname, "iN$", (int *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = *val!=0; \
            else if(*numargs < 0) intret(o->vname ? 1 : 0); \
            else printvar(id, o->vname ? 1 : 0); \
        });

    #define UIARGTK(utype, uname, vname, valtype, type, cmin, cmax, valdef) \
        UICMDT(utype, uname, vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp((valdef), cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UIARGSCALEDT(utype, uname, vname, valtype, type, cmin, cmax) \
        UICMDT(utype, uname, vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax) * uiscale); \
            else if(*numargs < 0) type##ret(o->vname * uiscale); \
            else print##type##var(id, o->vname * uiscale); \
        });

    #define UIARGT(utype, uname, vname, valtype, type, cmin, cmax) \
        UICMDT(utype, uname, vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    struct ClipArea
    {
        float x1, y1, x2, y2;

        ClipArea(float x, float y, float w, float h) : x1(x), y1(y), x2(x+w), y2(y+h) {}

        void intersect(const ClipArea &c)
        {
            x1 = max(x1, c.x1);
            y1 = max(y1, c.y1);
            x2 = max(x1, min(x2, c.x2));
            y2 = max(y1, min(y2, c.y2));

        }

        bool isfullyclipped(float x, float y, float w, float h)
        {
            return x1 == x2 || y1 == y2 || x >= x2 || y >= y2 || x+w <= x1 || y+h <= y1;
        }

        void scissor();
    };

    static vector<ClipArea> clipstack;

    static inline bool isfullyclipped(float x, float y, float w, float h)
    {
        if(clipstack.empty()) return false;
        return clipstack.last().isfullyclipped(x, y, w, h);
    }

    struct Object
    {
        static const char *curtag;
        static int buildlevel, taglevel;

        Object *parent;
        int buildchild;
        float x, y, w, h, ox, oy;
        float lastx, lasty, lastsx, lastsy, lastw, lasth;
        bool overridepos, drawn;
        uchar adjust;
        ushort state, childstate, allowstate;
        vector<Object *> children, orphans;
        char *tag;

        Object() : buildchild(-1), ox(0), oy(0), lastx(0), lasty(0), lastsx(0), lastsy(0), lastw(0), lasth(0),
            overridepos(false), drawn(false), adjust(0), state(0), childstate(0), allowstate(STATE_ALL), tag(NULL) {}

        virtual ~Object()
        {
            if(inputsteal == this)
                inputsteal = NULL;
            clearchildren();
            DELETEA(tag);
        }

        void resetlayout()
        {
            x = y = w = h = 0;
            if(overridepos)
            {
                x = ox;
                y = oy;
            }
        }

        void reset()
        {
            resetlayout();
            parent = NULL;
            adjust = ALIGN_DEFAULT;
        }

        virtual uchar childalign() const { return ALIGN_DEFAULT; }

        void reset(Object *parent_)
        {
            resetlayout();
            parent = parent_;
            adjust = parent ? parent->childalign() : ALIGN_DEFAULT;
        }

        void setup()
        {
        }

        void clearchildren()
        {
            children.deletecontents();
            orphans.deletecontents();
        }

        #define loopchildren(o, body) do { \
            loopv(children) \
            { \
                Object *o = children[i]; \
                body; \
            } \
        } while(0)

        #define loopchildrenrev(o, body) do { \
            loopvrev(children) \
            { \
                Object *o = children[i]; \
                body; \
            } \
        } while(0)

        #define loopchildrange(start, end, o, body) do { \
            for(int i = start; i < end; i++) \
            { \
                Object *o = children[i]; \
                body; \
            } \
        } while(0)

        #define loopinchildren(o, cx, cy, inbody, outbody) \
            loopchildren(o, \
            { \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                if(o##x >= 0 && o##x < o->w && o##y >= 0 && o##y < o->h) \
                { \
                    inbody; \
                } \
                outbody; \
            })

        #define loopinchildrenrev(o, cx, cy, inbody, outbody) \
            loopchildrenrev(o, \
            { \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                if(o##x >= 0 && o##x < o->w && o##y >= 0 && o##y < o->h) \
                { \
                    inbody; \
                } \
                outbody; \
            })

        virtual void prepare()
        {
            loopchildren(o, o->prepare());
        }

        virtual void layout()
        {
            w = h = 0;
            loopchildren(o,
            {
                o->x = o->y = 0;
                o->layout();
                w = max(w, o->x + o->w);
                h = max(h, o->y + o->h);
            });
        }

        void adjustchildrento(float px, float py, float pw, float ph)
        {
            loopchildren(o, o->adjustlayout(px, py, pw, ph));
        }

        virtual void adjustchildren()
        {
            adjustchildrento(0, 0, w, h);
        }

        void adjustlayout(float px, float py, float pw, float ph)
        {
            if(overridepos)
            {
                x = ox;
                y = oy;
            }
            else
            {
                switch(adjust&ALIGN_HMASK)
                {
                    case ALIGN_LEFT:    x = px; break;
                    case ALIGN_HCENTER: x = px + (pw - w) / 2; break;
                    case ALIGN_RIGHT:   x = px + pw - w; break;
                }

                switch(adjust&ALIGN_VMASK)
                {
                    case ALIGN_TOP:     y = py; break;
                    case ALIGN_VCENTER: y = py + (ph - h) / 2; break;
                    case ALIGN_BOTTOM:  y = py + ph - h; break;
                }
            }

            if(adjust&CLAMP_MASK)
            {
                if(adjust&CLAMP_LEFT)   { w += x - px; x = px; }
                if(adjust&CLAMP_RIGHT)    w = px + pw - x;
                if(adjust&CLAMP_TOP)    { h += y - py; y = py; }
                if(adjust&CLAMP_BOTTOM)   h = py + ph - y;
            }

            adjustchildren();
            lastx = x; lasty = y;
            lastw = w; lasth = h;
        }

        virtual void setalign(int xalign, int yalign)
        {
            adjust &= ~ALIGN_MASK;
            adjust |= (clamp(xalign, -2, 1)+2)<<ALIGN_HSHIFT;
            adjust |= (clamp(yalign, -2, 1)+2)<<ALIGN_VSHIFT;
        }

        virtual void setclamp(int left, int right, int top, int bottom)
        {
            adjust &= ~CLAMP_MASK;
            if(left) adjust |= CLAMP_LEFT;
            if(right) adjust |= CLAMP_RIGHT;
            if(top) adjust |= CLAMP_TOP;
            if(bottom) adjust |= CLAMP_BOTTOM;
        }

        void setpos(float xpos, float ypos)
        {
            x = ox = xpos;
            y = oy = ypos;
            overridepos = true;
        }

        void resetpos()
        {
            x = ox = y = oy = 0;
            overridepos = false;
        }

        virtual bool isfocus() const
        {
            return false;
        }

        virtual bool target(float cx, float cy)
        {
            return false;
        }

        virtual bool rawkey(int code, bool isdown)
        {
            loopchildrenrev(o,
            {
                if(o->rawkey(code, isdown)) return true;
            });
            return false;
        }

        virtual bool key(int code, bool isdown)
        {
            loopchildrenrev(o,
            {
                if(o->key(code, isdown)) return true;
            });
            return false;
        }

        virtual bool textinput(const char *str, int len)
        {
            loopchildrenrev(o,
            {
                if(o->textinput(str, len)) return true;
            });
            return false;
        }

        virtual void init() {}

        virtual void startdraw() {}
        virtual void enddraw() {}

        void enddraw(int change)
        {
            enddraw();

            changed &= ~change;
            if(changed)
            {
                if(changed&CHANGE_SHADER) hudshader->set();
                if(changed&CHANGE_COLOR) gle::colorf(1, 1, 1);
                if(changed&CHANGE_BLEND) resetblend();
            }
        }

        void changedraw(int change = 0)
        {
            if(!drawing)
            {
                startdraw();
                changed = change;
            }
            else if(drawing->gettype() != gettype())
            {
                drawing->enddraw(change);
                startdraw();
                changed = change;
            }
            drawing = this;
        }

        virtual void draw(float sx, float sy)
        {
            lastsx = sx;
            lastsy = sy;
            drawn = true;

            loopchildren(o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
        }

        void resetstate()
        {
            drawn = false;
            state &= STATE_HOLD_MASK;
            childstate &= STATE_HOLD_MASK;
        }
        void resetchildstate()
        {
            resetstate();
            loopchildren(o, o->resetchildstate());
        }

        bool hasstate(int chkflags) const { return allowstate&chkflags && (state & ~childstate)&chkflags; }
        bool haschildstate(int chkflags) const
        {
            bool cs = allowstate&chkflags && (state | childstate)&chkflags;
            bool steal = (this == inputsteal || !inputsteal) || (state & (STATE_SCROLL_UP|STATE_SCROLL_DOWN));
            return cs && steal;
        }

        #define DOSTATES \
            DOSTATE(STATE_HOVER, hover) \
            DOSTATE(STATE_PRESS, press) \
            DOSTATE(STATE_HOLD, hold) \
            DOSTATE(STATE_RELEASE, release) \
            DOSTATE(STATE_ALT_HOLD, althold) \
            DOSTATE(STATE_ALT_PRESS, altpress) \
            DOSTATE(STATE_ALT_RELEASE, altrelease) \
            DOSTATE(STATE_ESC_HOLD, eschold) \
            DOSTATE(STATE_ESC_PRESS, escpress) \
            DOSTATE(STATE_ESC_RELEASE, escrelease) \
            DOSTATE(STATE_SCROLL_UP, scrollup) \
            DOSTATE(STATE_SCROLL_DOWN, scrolldown)

        bool setstate(int state, float cx, float cy, int mask = 0, int mode = SETSTATE_INSIDE, int setflags = 0)
        {
            switch(state)
            {
            #define DOSTATE(chkflags, func) case chkflags: if(allowstate&chkflags) { func##children(cx, cy, true, mask, mode, setflags | chkflags); return haschildstate(chkflags); } break;
            DOSTATES
            #undef DOSTATE
            }
            return false;
        }

        void clearstate(int chkflags)
        {
            state &= ~chkflags;
            if(childstate & chkflags)
            {
                loopchildren(o, { if((o->state | o->childstate) & chkflags) o->clearstate(chkflags); });
                childstate &= ~chkflags;
            }
        }

        #define propagatestate(o, cx, cy, mask, mode, body) \
            loopchildrenrev(o, \
            { \
                if(((o->state | o->childstate) & mask) != mask) continue; \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                bool oinside = true; \
                if(mode != SETSTATE_INSIDE) \
                { \
                    if(o##x < 0.0f || o##x >= o->w || \
                       o##y < 0.0f || o##y >= o->h) \
                        oinside = false; \
                    \
                    o##x = clamp(o##x, 0.0f, o->w); \
                    o##y = clamp(o##y, 0.0f, o->h); \
                    body; \
                } \
                else if(o##x >= 0 && o##x < o->w && o##y >= 0 && o##y < o->h) \
                { \
                    body; \
                } \
            })

        #define DOSTATE(chkflags, func) \
            virtual void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!(allowstate&chkflags)) return; \
                propagatestate(o, cx, cy, mask, mode, \
                { \
                    o->func##children(ox, oy, oinside, mask, mode, setflags); \
                    childstate |= (o->state | o->childstate) & (setflags); \
                }); \
                if(mode != SETSTATE_FOCUSED || isfocus()) \
                { \
                    if(target(cx, cy)) state |= (setflags); \
                    func(cx, cy, cinside); \
                } \
            } \
            virtual void func(float cx, float cy, bool inside) {}
        DOSTATES
        #undef DOSTATE

        static const char *typestr() { return "#Object"; }
        virtual const char *gettype() const { return typestr(); }
        virtual const char *getname() const { return gettype(); }
        virtual const char *gettypename() const { return gettype(); }

        template<class T> bool istype() const { return T::typestr() == gettype(); }
        bool isnamed(const char *name) const { return name[0] == '#' ? name == gettypename() : !strcmp(name, getname()); }

        virtual bool iswindow() const { return true; }
        virtual bool isfill() const { return false; }
        virtual bool iscolour() const { return false; }
        virtual bool istext() const { return false; }
        virtual bool isrender() const { return false; }
        virtual bool isviewport() const { return false; }
        virtual bool isimage() const { return false; }
        virtual bool iseditor() const { return false; }
        virtual bool isclip() const { return false; }
        virtual bool isradar() const { return false; }
        virtual bool ispreview() const { return false; }
        virtual bool ismodelpreview() const { return false; }
        virtual bool iskeycatcher() const { return false; }

        Object *find(const char *name, bool recurse = true, const Object *exclude = NULL) const
        {
            loopchildren(o,
            {
                if(o != exclude && o->isnamed(name)) return o;
            });
            if(recurse) loopchildren(o,
            {
                if(o != exclude)
                {
                    Object *found = o->find(name);
                    if(found) return found;
                }
            });
            return NULL;
        }

        Object *findsibling(const char *name) const
        {
            for(const Object *prev = this, *cur = parent; cur; prev = cur, cur = cur->parent)
            {
                Object *o = cur->find(name, true, prev);
                if(o) return o;
            }
            return NULL;
        }

        template<class T> T *buildtype()
        {
            T *t = NULL;

            if(curtag)
            { // tag items so we can move through the list in case something was removed
                for(int i = buildchild; i < children.length(); i++)
                { // search the siblings in case an item before it has moved
                    Object *o = children[i];
                    if(!o->istype<T>() || !o->tag || strcmp(o->tag, curtag)) continue;
                    t = (T *)o;
                    if(i > buildchild) for(int j = i - 1; j >= buildchild; j--)
                    { // orphan siblings up to this point so we can bring the object forward
                        orphans.add(children[j]);
                        children.remove(j);
                    }
                    break;
                }
                if(!t) loopv(orphans)
                { // search the orphans in case it moved
                    Object *o = orphans[i];
                    if(!o->istype<T>() || !o->tag || strcmp(o->tag, curtag)) continue;
                    children.insert(buildchild, o);
                    orphans.remove(i);
                    t = (T *)o;
                    i--;
                }
            }

            if(!t)
            {
                if(children.inrange(buildchild))
                {
                    Object *o = children[buildchild];
                    if(o->istype<T>() && !o->tag) t = (T *)o;
                    else
                    {
                        if(o->tag) orphans.add(o);
                        else delete o;
                        t = new T;
                        children[buildchild] = t;
                    }
                }
                else
                {
                    t = new T;
                    children.add(t);
                }
            }

            t->reset(this);

            if(t->tag && (!curtag || strcmp(t->tag, curtag)))
            {
                delete[] t->tag;
                t->tag = NULL;
            }
            if(!t->tag && curtag) t->tag = newstring(curtag);

            buildchild++;
            return t;
        }

        void buildchildren(uint *contents, bool mapdef = false)
        {
            if((*contents&CODE_OP_MASK) == CODE_EXIT) clearchildren();
            else
            {
                Object *oldparent = buildparent;
                buildparent = this;
                buildchild = 0;

                const char *oldttag = curtag;
                if(buildlevel != taglevel) curtag = NULL;
                buildlevel++;

                DOMAP(mapdef, executeret(contents));
                while(children.length() > buildchild)
                    delete children.pop();
                orphans.deletecontents();

                curtag = oldttag;
                buildlevel--;

                buildparent = oldparent;
            }
            resetstate();
        }

        virtual int childcolumns() const { return children.length(); }
    };

    const char *Object::curtag = NULL;
    int Object::buildlevel = -1, Object::taglevel = -1;

    ICOMMAND(0, uigroup, "e", (uint *children),
        BUILD(Object, o, o->setup(), children));

    #define UIOBJCMD(vname, valtype, args, body) \
        ICOMMAND(0, ui##vname, valtype, args, { \
            if(buildparent) \
            { \
                Object *o = buildparent; \
                body; \
            } \
            else if(!propagating) conoutf(colourorange, "Warning: No object available for ui%s", #vname); \
        });

    #define UIOBJARGB(vname) \
        UIOBJCMD(vname, "iN$", (int *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = *val!=0; \
            else if(*numargs < 0) intret(o->vname ? 1 : 0); \
            else printvar(id, o->vname ? 1 : 0); \
        });

    #define UIOBJARGK(vname, valtype, type, cmin, cmax, valdef) \
        UIOBJCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp((valdef), cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UIOBJARGSCALED(vname, valtype, type, cmin, cmax) \
        UIOBJCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax) * uiscale); \
            else if(*numargs < 0) type##ret(o->vname * uiscale); \
            else print##type##var(id, o->vname * uiscale); \
        });

    #define UIOBJARG(vname, valtype, type, cmin, cmax) \
        UIOBJCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    UIOBJARGB(overridepos);
    UIOBJCMD(setpos, "ff", (float *xpos, float *ypos), o->setpos(*xpos, *ypos));
    UIOBJCMD(resetpos, "", (), o->resetpos());
    UIOBJARG(allowstate, "b", int, 0, int(STATE_ALL));

    ICOMMANDVS(0, uitype, buildparent ? buildparent->gettype() : "");
    ICOMMANDV(0, uidrawn, buildparent && buildparent->drawn ? 1 : 0);
    ICOMMANDVS(0, uitagid, buildparent ? buildparent->tag : "");
    ICOMMANDVS(0, uicurtag, Object::curtag ? Object::curtag : "");
    ICOMMANDV(0, uibuildlevel, Object::buildlevel);
    ICOMMANDV(0, uitaglevel, Object::taglevel);

    static inline void stopdrawing()
    {
        if(drawing)
        {
            drawing->enddraw(0);
            drawing = NULL;
        }
    }

    Surface *surface = NULL, *surfaces[SURFACE_MAX] = { NULL, NULL, NULL, NULL, NULL };
    vector<Surface *> surfacestack;
    Window *window = NULL;

    struct Code
    {
        char *body;
        uint *code;

        Code() : body(NULL), code(NULL) {}
        Code(const char *str) : body(newstring(str)), code(compilecode(str)) {}
        ~Code()
        {
            DELETEA(body);
            freecode(code);
        }
    };

    struct Window : Object
    {
        char *name, *dyn;
        Code *contents, *onshow, *onhide, *vistest, *forcetest;
        bool exclusive, mapdef, saved, menu, passthrough, tooltip, popup, persist, ontop, attached, visible;
        int allowinput, lasthit, lastshow, lastpoke, zindex, numargs, initargs, hint;
        float px, py, pw, ph,
              maxdist, yaw, pitch, curyaw, curpitch, detentyaw, detentpitch,
              scale, dist, hitx, hity;
        vec2 sscale, soffset;
        vec origin, pos;
        tagval args[MAXARGS];

        Window(const char *name_, const char *contents_, const char *onshow_, const char *onhide_, const char *vistest_, const char *forcetest_, bool mapdef_, const char *dyn_ = NULL, tagval *args_ = NULL, int numargs_ = 0) :
            name(newstring(name_)), dyn(dyn_ && *dyn_ ? newstring(dyn_) : NULL),
            contents(NULL), onshow(NULL), onhide(NULL), vistest(NULL), forcetest(NULL),
            exclusive(false), mapdef(mapdef_),
            menu(false), passthrough(false), tooltip(false), popup(false), persist(false), ontop(false), attached(false), visible(false),
            allowinput(0), lasthit(0), lastshow(0), lastpoke(0), zindex(0), numargs(0), initargs(0), hint(0),
            px(0), py(0), pw(0), ph(0),
            maxdist(0), yaw(-1), pitch(0), curyaw(0), curpitch(0), detentyaw(0), detentpitch(0),
            scale(1), dist(0), hitx(-1), hity(-1),
            sscale(1, 1), soffset(0, 0),
            origin(0, 0, 0), pos(0, 0, 0)
        {
            if(contents_ && *contents_) contents = new Code(contents_);
            if(onshow_ && *onshow_) onshow = new Code(onshow_);
            if(onhide_ && *onhide_) onhide = new Code(onhide_);
            if(vistest_ && *vistest_) vistest = new Code(vistest_);
            if(forcetest_ && *forcetest_) forcetest = new Code(forcetest_);
            numargs = initargs = 0;
            loopi(MAXARGS) args[i].setnull();
            if(args_ && numargs_ > 0)
            {
                numargs = initargs = min(numargs_, int(MAXARGS));
                memcpy(args, args_, numargs*sizeof(tagval));
            }
        }
        ~Window()
        {
            delete[] name;
            if(contents) delete contents;
            if(onshow) delete onshow;
            if(onhide) delete onhide;
            if(vistest) delete vistest;
            if(forcetest) delete forcetest;
            resetargs(true);
        }

        static const char *typestr() { return "#Window"; }
        const char *gettype() const { return typestr(); }
        const char *getname() const { return name; }

        bool iswindow() const { return true; }

        void build();

        void resetargs(bool full = false)
        {
            if(full) initargs = 0;
            numargs = initargs;
            int n = MAXARGS - initargs;
            loopi(n) args[i + initargs].reset();
        }

        int allocarg(int n)
        {
            if(n > 0) // > 0 is uiarg1, uiarg2, ... uiargN
            {
                if(n > MAXARGS) return -1; // not zero-indexed!
                while(n > numargs) args[++numargs].reset(); // n at least equal numargs
                return n - 1; // return zero indexed
            }
            else if(n < 0) resetargs(n < -1); // < 0 is reset
            else if(numargs >= MAXARGS) return -1; // 0 is next arg
            numargs++;
            return numargs - 1;
        }

        void setarg(int val, int n = 0)
        {
            int arg = allocarg(n);
            if(arg < 0) return;
            args[arg].setint(val);
        }

        void setarg(bool val, int n = 0)
        {
            int arg = allocarg(n);
            if(arg < 0) return;
            args[arg].setint(val ? 1 : 0);
        }

        void setarg(float val, int n = 0)
        {
            int arg = allocarg(n);
            if(arg < 0) return;
            args[arg].setfloat(val);
        }

        void setarg(const char *val, int n = 0)
        {
            int arg = allocarg(n);
            if(arg < 0) return;
            args[arg].setstr(newstring(val));
        }

        void setarg(char *val, int n = 0)
        {
            int arg = allocarg(n);
            if(arg < 0) return;
            args[arg].setstr(newstring(val));
        }

        void setargs()
        {
            loopi(MAXARGS)
            {
                if(i >= numargs) uiargs[i]->reset();
                else uiargs[i]->copyval(args[i]);
            }
        }

        void resetworld(const vec &o = nullvec, float m = 0, float s = 1)
        {
            maxdist = m;
            yaw = -1;
            pitch = curyaw = curpitch = 0;
            scale = s;
            origin = pos = o;
        }

        void hide()
        {
            overridepos = false;
            if(onhide)
            {
                setargs();
                DOMAP(mapdef, executeret(onhide->code));
            }
            resetworld();
        }

        float getscale()
        {
            if(surfacetype == SURFACE_WORLD) return scale >= 0 ? scale * uiworldscale : -scale;
            return scale >= 0 ? scale : (0 - scale) / uiworldscale;
        }

        void show(const vec &o = nullvec, float m = 0, float y = 0, float p = 0, float s = 1, float dy = 0, float dp = 0)
        {
            overridepos = false;
            state |= STATE_HIDDEN;
            clearstate(STATE_HOLD_MASK);

            resetworld(o, m, s);

            if(surfacetype == SURFACE_WORLD)
            {
                yaw = y;
                pitch = p;
                detentyaw = dy > 0 ? clamp(dy, 0.f, 180.f) : 0.f;
                detentpitch = dp > 0 ? clamp(dp, 0.f, 90.f) : 0.f;
            }

            if(onshow)
            {
                setargs();
                DOMAP(mapdef, executeret(onshow->code));
            }

            lastshow = uitotalmillis;
        }

        void setup()
        {
            Object::setup();

            exclusive = passthrough = tooltip = popup = persist = ontop = attached = menu = false;
            allowinput = zindex = hint = 0;
            px = py = pw = ph = 0;

            if(surfacetype != SURFACE_WORLD) pos = origin;
        }

        void vposition()
        {
            float cx = 0.5f, cy = 0.5f, cz = 1;

            if(vectocursor(pos, cx, cy, cz))
            {
                if(rendervisor == VisorSurface::VISOR) visorsurf.coords(cx, cy, cx, cy, true);

                float aspect = hudw/float(hudh);
                cx *= aspect; // convert to UI coordinate system

                switch(adjust&ALIGN_HMASK)
                {
                    case ALIGN_LEFT:    cx -= w; break;
                    case ALIGN_HCENTER: cx -= w * 0.5f; break;
                    case ALIGN_RIGHT:   break;
                }

                switch(adjust&ALIGN_VMASK)
                {
                    case ALIGN_TOP:     cy -= h; break;
                    case ALIGN_VCENTER: cy -= h * 0.5f; break;
                    case ALIGN_BOTTOM:  break;
                }

                if(rendervisor != VisorSurface::VISOR || (cx > 0 && cx + w < aspect && cy > 0 && cy + h < 1))
                { // keep away from the edges of the visor to avoid smearing
                    setpos(cx, cy);
                    return;
                }
            }
            visible = false;
            state = STATE_HIDDEN;
        }

        void layout()
        {
            if(!visible || state&STATE_HIDDEN) { w = h = 0; return; }

            Window *oldwindow = window;
            window = this;

            Object::layout();

            bool haspos = pos != nullvec;
            if(hint > 0)
            {
                int offset = uilastmillis - hint;
                if(offset <= uihintintime + uihintholdtime + uihintouttime)
                {
                    float amt = smoothinterp(offset > (uihintholdtime + uihintintime) ?
                                    1.f - ((offset - uihintintime - uihintholdtime) / float(uihintouttime))
                                : (offset < uihintintime ? offset / float(uihintintime) : 1.f));

                    if(haspos)
                    {
                        vec campos = vec(camera1->o).add(vec(camera1->yaw * RAD, (camera1->pitch + fovy * (*surfacehint3d)) * RAD).mul(uihintdist));
                        origin = vec(origin).add(vec(campos).sub(origin).mul(amt));
                    }
                    else
                    {
                        vec2 campos(aspect * 0.5f, 0.5f - *surfacehint2d), curpos(x, y);

                        switch(adjust&ALIGN_HMASK)
                        {
                            case ALIGN_LEFT:    campos.x -= w; break;
                            case ALIGN_HCENTER: campos.x -= w * 0.5f; break;
                            case ALIGN_RIGHT:   break;
                        }

                        switch(adjust&ALIGN_VMASK)
                        {
                            case ALIGN_TOP:     campos.y -= h; break;
                            case ALIGN_VCENTER: campos.y -= h * 0.5f; break;
                            case ALIGN_BOTTOM:  break;
                        }

                        campos = vec2(curpos).add(vec2(campos).sub(curpos).mul(amt));
                        setpos(campos.x, campos.y);

                        *surfacehint2d = 0.5f - (campos.y < 0.5f ? campos.y + h : campos.y);
                    }
                }
            }

            if(surfacetype != SURFACE_WORLD && haspos) vposition();

            window = oldwindow;
        }

        void draw(float sx, float sy)
        {
            if(!visible || state&STATE_HIDDEN) return;

            Window *oldwindow = window;
            window = this;

            projection();
            resetblend(true);

            hudshader->set();
            gle::colorf(1, 1, 1);

            changed = 0;
            drawing = NULL;

            if(surfacetype == SURFACE_WORLD && ontop) glDisable(GL_DEPTH_TEST);

            Object::draw(sx, sy);
            stopdrawing();

            if(surfacetype == SURFACE_WORLD && ontop) glEnable(GL_DEPTH_TEST);

            window = oldwindow;
        }

        void draw()
        {
            draw(x, y);
        }

        void adjustchildren()
        {
            if(!visible || state&STATE_HIDDEN) return;
            Window *oldwindow = window;
            window = this;
            Object::adjustchildren();
            window = oldwindow;
        }

        void adjustlayout()
        {
            if(surfacetype == SURFACE_COMPOSITE) pw = ph = 1;
            else if(surfacetype == SURFACE_WORLD)
            {
                pw = w;
                ph = h;
            }
            else
            {
                float aspect = hudw/float(hudh);
                ph = max(max(h, w/aspect), 1.0f);
                pw = aspect*ph;
            }
            Object::adjustlayout(0, 0, pw, ph);
        }

        bool planeintersect(const vec &o, const vec &d, const vec &p, const vec &n, vec &v)
        {
            float denom = d.dot(n);
            if(fabs(denom) < FVAR_NONZERO) return false;
            float t = vec(p).sub(o).dot(n) / denom;
            if(t < 0) return false;
            v = vec(o).add(vec(d).mul(t));
            return true;
        }

        void hitintersect(const vec &o, const vec &p, const vec &n, float a, float &x, float &y)
        {
            vec r(1, 0, 0);

            vec v, u;
            v.cross(n, vec(1, 0, 0)).normalize();
            u.cross(v, n).normalize();

            x = vec(o).sub(p).dot(u);
            y = vec(o).sub(p).dot(v);

            if(a > 90 && a < 270) // hack
            {
                x = -x;
                y = -y;
            }
        }

        vec rotatedir(const vec &from, const vec &to, float angle)
        {
            // Calculate the axis of rotation
            vec axis, faxis;
            axis.cross(from, to);
            axis.normalize();
            faxis.cross(from, axis);
            faxis.normalize();

            return vec(from).mul(cosf(angle)).add(vec(axis).mul(axis.dot(from)).mul(1 - cosf(angle))).add(vec(faxis).mul(sinf(angle)));
        }

        float worldcalc()
        {
            pos = origin;
            curyaw = yaw;
            curpitch = pitch;

            vec ray = vec(camera1->o).sub(pos), dir = vec(ray).normalize();
            float offyaw, offpitch;
            vectoyawpitch(dir, offyaw, offpitch);

            if(curyaw < 0) curyaw = offyaw;
            if(curpitch < -180 || curpitch > 180) curpitch = offpitch;
            if(detentyaw > 0) curyaw = round(curyaw / detentyaw) * detentyaw;
            if(detentpitch > 0) curpitch = round(curpitch / detentpitch) * detentpitch;
            float curscale = getscale();

            vec q = vec(curyaw * RAD, curpitch * RAD), n = vec(q).normalize(), up(0, 0, 1), right;
            if(fabsf(n.z) < 1.f) right.cross(up, n).normalize();
            else right.cross(n, up).normalize();
            up.cross(n, right).normalize();

            switch(adjust&ALIGN_HMASK)
            {
                case ALIGN_LEFT:    break;
                case ALIGN_RIGHT:   pos.add(vec(right).mul(pw * curscale)); break;
                default:            pos.add(vec(right).mul(pw * curscale * 0.5f)); break;
            }
            switch(adjust&ALIGN_VMASK)
            {
                case ALIGN_BOTTOM:  break;
                case ALIGN_TOP:     pos.add(vec(up).mul(ph * curscale)); break;
                default:            pos.add(vec(up).mul(ph * curscale * 0.5f)); break;
            }

            return ray.magnitude();
        }

        void getcursor(float &cx, float &cy)
        {
            if(uitotalmillis == lasthit)
            {
                cx = hitx * pw + px - x;
                cy = hity * ph + py - y;
                return;
            }

            if(surfacetype != SURFACE_WORLD)
            {
                hitx = cx;
                hity = cy;
            }
            else
            {
                hitx = hity = -1;
                float mag = worldcalc();

                if(mag >= FVAR_NONZERO)
                {
                    vec n = vec(curyaw * RAD, curpitch * RAD), v;
                    if(planeintersect(camera1->o, cursordir, pos, n, v))
                    {
                        float qx = 0, qy = 0, pyaw = 0, ppitch = 0, curscale = getscale();
                        vectoyawpitch(n, pyaw, ppitch);
                        hitintersect(v, pos, n, pyaw, qx, qy);
                        hitx = qx / (pw * curscale);
                        hity = qy / (ph * curscale);
                    }
                }
            }
            cx = hitx * pw + px - x;
            cy = hity * ph + py - y;
            lasthit = uitotalmillis;
        }

        #define DOSTATE(chkflags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!visible || !allowinput || !(allowstate&chkflags) || state&STATE_HIDDEN || pw <= 0 || ph <= 0) return; \
                getcursor(cx, cy); \
                bool inside = (cx >= 0 && cy >= 0 && cx < w && cy < h); \
                if(mode != SETSTATE_INSIDE || inside) \
                    Object::func##children(cx, cy, inside, mask, mode, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void projection()
        {
            if(surfacetype == SURFACE_COMPOSITE) // composites have flipped Y axis
                hudmatrix.ortho(px, px + pw, py, py + ph, -1, 1);
            else if(surfacetype == SURFACE_WORLD)
            {
                worldcalc();

                hudmatrix.muld(nojittermatrix, cammatrix);
                // hudmatrix = camprojmatrix;
                hudmatrix.translate(pos);
                hudmatrix.rotate_around_z(curyaw*RAD);
                hudmatrix.rotate_around_x((curpitch - 90)*RAD);

                hudmatrix.scale(getscale());
            }
            else hudmatrix.ortho(px, px + pw, py + ph, py, -1, 1);

            resethudmatrix();
            sscale = vec2(hudmatrix.a.x, hudmatrix.b.y).mul(0.5f);
            soffset = vec2(hudmatrix.d.x, hudmatrix.d.y).mul(0.5f).add(0.5f);
        }

        void calcscissor(float x1, float y1, float x2, float y2, int &sx1, int &sy1, int &sx2, int &sy2, bool clip = true)
        {
            vec2 s1 = vec2(x1, y2).mul(sscale).add(soffset),
                 s2 = vec2(x2, y1).mul(sscale).add(soffset);
            sx1 = int(floor(s1.x*hudw + 0.5f));
            sy1 = int(floor(s1.y*hudh + 0.5f));
            sx2 = int(floor(s2.x*hudw + 0.5f));
            sy2 = int(floor(s2.y*hudh + 0.5f));
            if(clip)
            {
                sx1 = clamp(sx1, 0, hudw);
                sy1 = clamp(sy1, 0, hudh);
                sx2 = clamp(sx2, 0, hudw);
                sy2 = clamp(sy2, 0, hudh);
            }
            if(surfacetype == SURFACE_COMPOSITE) swap(sy1, sy2);
        }

        static bool compare(const Object *a, const Object *b)
        {
            Window *aa = (Window *)a, *bb = (Window *)b;

            // newest hint windows last
            if(aa->hint < bb->hint) return true;
            if(aa->hint > bb->hint) return false;

            // visible windows last
            if(!aa->visible && bb->visible) return true;
            if(aa->visible && !bb->visible) return false;

            // ontop windows last
            if(!aa->ontop && bb->ontop) return true;
            if(aa->ontop && !bb->ontop) return false;

            // zindex sort higher to last
            if(aa->zindex < bb->zindex) return true;
            if(aa->zindex > bb->zindex) return false;

            // reverse order so further gets drawn first
            if(aa->dist > bb->dist) return true;
            if(aa->dist < bb->dist) return false;

            if(aa->pos != nullvec && bb->pos != nullvec)
            {   // draw bottom-up
                if(aa->pos.z < bb->pos.z) return true;
                if(aa->pos.z > bb->pos.z) return false;
            }

            // newest last
            return aa->lastshow > bb->lastshow;
        }
    };

    void uitag(const char *tag, uint *contents)
    {
        if(!window) return;

        const char *oldtag = Object::curtag;
        int oldlevel = Object::taglevel;

        Object::curtag = tag;
        Object::taglevel = Object::buildlevel;

        DOMAP(window->mapdef, executeret(contents));

        Object::taglevel = oldlevel;
        Object::curtag = oldtag;
    }
    ICOMMAND(0, uitag, "se", (const char *tag, uint *contents), uitag(tag, contents));

    struct Blend : Object
    {
        int setting, sep;

        void setup(int type_, int sep_)
        {
            Object::setup();
            setting = type_;
            sep = sep_;
        }

        static const char *typestr() { return "#Blend"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            int oldblend = blendtypedef;
            bool oldsep = blendsepdef;
            if(setting >= 0) blendtypedef = setting;
            if(sep >= 0) blendsepdef = sep != 0;
            resetblend();
            Object::draw(sx, sy);
            blendtypedef = oldblend;
            blendsepdef = oldsep;
            resetblend();
        }
    };

    ICOMMAND(0, uiblend, "iie", (int *type, int *sep, uint *children), BUILD(Blend, o, o->setup(clamp(*type, -1, BLEND_MAX-1), *sep), children));

    struct Font : Object
    {
        char *name;

        Font() : name(NULL) {}
        ~Font() { delete[] name; }

        void setup(const char *name_)
        {
            Object::setup();
            SETSTR(name, name_);
        }

        static const char *typestr() { return "#Font"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            pushfont(name);
            Object::layout();
            popfont();
        }

        void draw(float sx, float sy)
        {
            pushfont(name);
            Object::draw(sx, sy);
            popfont();
        }

        void buildchildren(uint *contents)
        {
            pushfont(name);
            Object::buildchildren(contents);
            popfont();
        }

        #define DOSTATE(chkflags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!(allowstate&chkflags)) return; \
                pushfont(name); \
                Object::func##children(cx, cy, cinside, mask, mode, setflags); \
                popfont(); \
            }
        DOSTATES
        #undef DOSTATE

        bool rawkey(int code, bool isdown)
        {
            pushfont(name);
            bool result = Object::rawkey(code, isdown);
            popfont();
            return result;
        }

        bool key(int code, bool isdown)
        {
            pushfont(name);
            bool result = Object::key(code, isdown);
            popfont();
            return result;
        }

        bool textinput(const char *str, int len)
        {
            pushfont(name);
            bool result = Object::textinput(str, len);
            popfont();
            return result;
        }
    };

    ICOMMAND(0, uifont, "se", (char *name, uint *children), BUILD(Font, o, o->setup(name), children));

    #define UIWINCMD(vname, valtype, args, body) \
        ICOMMAND(0, ui##vname, valtype, args, { \
            if(window) \
            { \
                Window *o = window; \
                body; \
            } \
            else if(!propagating) conoutf(colourorange, "Warning: No window available for ui%s", #vname); \
        });

    #define UIWINARGB(vname) \
        UIWINCMD(vname, "iN$", (int *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = *val!=0; \
            else if(*numargs < 0) intret(o->vname ? 1 : 0); \
            else printvar(id, o->vname ? 1 : 0); \
        });

    #define UIWINARGK(vname, valtype, type, cmin, cmax, valdef) \
        UIWINCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp((valdef), cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UIWINARGSCALED(vname, valtype, type, cmin, cmax) \
        UIWINCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax) * uiscale); \
            else if(*numargs < 0) type##ret(o->vname * uiscale); \
            else print##type##var(id, o->vname * uiscale); \
        });

    #define UIWINARG(vname, valtype, type, cmin, cmax) \
        UIWINCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UIWINARGV(vname) \
        UIWINCMD(vname, "fffN$", (float *x, float *y, float *z, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = vec(*x, *y, *z); \
            else \
            { \
                if(*numargs < 0) result(o->vname); \
                else printsvar(id, o->vname); \
            } \
        }); \
        UIWINCMD(vname##x, "fN$", (float *v, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname.x = *v; \
            else if(*numargs < 0) floatret(o->vname.x); \
            else printfvar(id, o->vname.x); \
        }); \
        UIWINCMD(vname##y, "fN$", (float *v, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname.y = *v; \
            else if(*numargs < 0) floatret(o->vname.y); \
            else printfvar(id, o->vname.y); \
        }); \
        UIWINCMD(vname##z, "fN$", (float *v, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname.z = *v; \
            else if(*numargs < 0) floatret(o->vname.z); \
            else printfvar(id, o->vname.z); \
        });

    ICOMMANDVS(0, uiname, window ? window->name : "")

    UIWINARG(allowinput, "b", int, 0, 2);
    UIWINARGB(exclusive);
    UIWINARGB(menu);
    UIWINARGB(passthrough);
    UIWINARGB(tooltip);
    UIWINARGB(popup);
    UIWINARGB(persist);
    UIWINARGB(ontop);
    UIWINARGB(attached);
    UIWINARGB(visible);

    UIWINARGV(origin);
    UIWINARGV(pos);
    UIWINARG(maxdist, "f", float, 0.f, FVAR_MAX);
    UIWINARG(yaw, "f", float, -1, 360);
    UIWINARG(pitch, "f", float, -181, 181);
    UIWINARG(zindex, "i", int, VAR_MIN, VAR_MAX);
    UIWINARG(hint, "i", int, 0, VAR_MAX);

    float getuicursorx(bool aspect = true) { return visorsurf.getcursorx(surfacetype == SURFACE_VISOR ? 1 : -1) * (aspect ? hudw / float(hudh) : 1.0f); }
    float getuicursory() { return visorsurf.getcursory(surfacetype == SURFACE_VISOR ? 1 : -1); }

    ICOMMANDVF(0, uicuryaw, window ? window->curyaw : -1.f);
    ICOMMANDVF(0, uicurpitch, window ? window->curpitch : -1.f);
    ICOMMANDVF(0, uihitx, window ? window->hitx : -1.f);
    ICOMMANDVF(0, uihity, window ? window->hity : -1.f);
    ICOMMANDV(0, uiworld, surfacetype == SURFACE_WORLD ? 1 : 0);
    ICOMMANDVF(0, uicursorx, getuicursorx());
    ICOMMANDVF(0, uicursory, getuicursory());
    ICOMMANDVF(0, uiaspect, hudw / float(hudh));

    static void pushclip(float x, float y, float w, float h)
    {
        if(clipstack.empty())
        {
            if(surfacetype != SURFACE_WORLD) glEnable(GL_SCISSOR_TEST);
            else glEnable(GL_STENCIL_TEST);
        }

        ClipArea &c = clipstack.add(ClipArea(x, y, w, h));
        if(clipstack.length() >= 2) c.intersect(clipstack[clipstack.length()-2]);
        c.scissor();
    }

    static void popclip()
    {
        clipstack.pop();
        if(clipstack.empty())
        {
            if(surfacetype != SURFACE_WORLD) glDisable(GL_SCISSOR_TEST);
            else glDisable(GL_STENCIL_TEST);
        }
        else clipstack.last().scissor();
    }

    static void enableclip()
    {
        if(clipstack.empty()) return;

        if(surfacetype != SURFACE_WORLD) glEnable(GL_SCISSOR_TEST);
        else glEnable(GL_STENCIL_TEST);
    }

    static void disableclip()
    {
        if(clipstack.empty()) return;

        if(surfacetype != SURFACE_WORLD) glDisable(GL_SCISSOR_TEST);
        else glDisable(GL_STENCIL_TEST);
    }

    void ClipArea::scissor()
    {
        int sx1, sy1, sx2, sy2;
        window->calcscissor(x1, y1, x2, y2, sx1, sy1, sx2, sy2);
        if(surfacetype != SURFACE_WORLD) glScissor(sx1, sy1, sx2-sx1, sy2-sy1);
        else
        {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glStencilFunc(GL_ALWAYS, 0xFF, ~0);
            glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

            Shader *oldshader = Shader::lastshader;
            hudnotextureshader->set();
            gle::defvertex(2);
            gle::colorf(1, 1, 1, 1);
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(x2, y1);
            gle::attribf(x1, y1);
            gle::attribf(x2, y2);
            gle::attribf(x1, y2);
            gle::end();
            if(oldshader) oldshader->set();

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glStencilFunc(GL_EQUAL, 0xFF, ~0);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
       }
    }

    struct Surface : Object
    {
        int type, cursortype, exclcheck;
        bool lockcursor, mousetracking, lockscroll, hasexclusive;
        vec2 mousetrackvec;
        float hintoffset2d, hintoffset3d;

        hashnameset<Window *> windows;
        vector<Texture *> texs;

        Surface(int _type) : type(_type),
            cursortype(CURSOR_DEFAULT), exclcheck(0), lockcursor(false), mousetracking(false), lockscroll(false), hasexclusive(false), mousetrackvec(0, 0), hintoffset2d(0), hintoffset3d(0) {}
        ~Surface() {}

        static const char *typestr() { return "#Surface"; }
        const char *gettype() const { return typestr(); }

        #define loopwindows(o, body) do { \
            loopv(children) \
            { \
                Window *o = (Window *)children[i]; \
                body; \
            } \
        } while(0)

        #define loopwindowsrev(o, body) do { \
            loopvrev(children) \
            { \
                Window *o = (Window *)children[i]; \
                body; \
            } \
        } while(0)

        bool checkexclusive(Window *w)
        {
            if(exclcheck != uitotalmillis)
            {
                hasexclusive = false;
                exclcheck = uitotalmillis;
                loopwindows(w,
                {
                    if(surfacetype != SURFACE_WORLD && w->exclusive)
                    {
                        hasexclusive = true;
                        break;
                    }
                });
            }
            return !hasexclusive || (surfacetype != SURFACE_WORLD && w->exclusive);
        }

        void adjustchildren()
        {
            loopwindows(w, w->adjustlayout());
        }

        #define DOSTATE(chkflags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!(allowstate&chkflags)) return; \
                loopwindowsrev(w, \
                { \
                    if(((w->state | w->childstate) & mask) != mask || !checkexclusive(w)) continue; \
                    w->func##children(cx, cy, cinside, mask, mode, setflags); \
                    int wflags = (w->state | w->childstate) & (setflags); \
                    if(wflags) { childstate |= wflags; break; } \
                }); \
            }
        DOSTATES
        #undef DOSTATE

        void checkinputsteal()
        {
            if(surfaceinput != type) return;

            if(inputsteal && !inputsteal->isfocus())
                inputsteal = NULL;
        }

        void build()
        {
            float oldtextscale = curtextscale, olduiscale = uiscale;
            curtextscale = 1;
            uiscale = 1;
            pushfont();

            cursortype = CURSOR_DEFAULT;
            lockcursor = false;
            mousetracking = false;
            lockscroll = false;
            hintoffset2d = uihintoffset2d;
            hintoffset3d = uihintoffset3d;

            checkinputsteal();
            prepare();

            if(surfaceinput == type)
            {
                setstate(STATE_HOVER, getuicursorx(false), getuicursory(), childstate&STATE_HOLD_MASK);
                if(childstate&STATE_HOLD) setstate(STATE_HOLD, getuicursorx(false), getuicursory(), STATE_HOLD, SETSTATE_ANY);
                if(childstate&STATE_ALT_HOLD) setstate(STATE_ALT_HOLD, getuicursorx(false), getuicursory(), STATE_ALT_HOLD, SETSTATE_ANY);
                if(childstate&STATE_ESC_HOLD) setstate(STATE_ESC_HOLD, getuicursorx(false), getuicursory(), STATE_ESC_HOLD, SETSTATE_ANY);
            }

            calctextscale();

            reset();
            setup();
            loopwindows(w,
            {   // try to get rid of any windows that aren't visible early to avoid processing
                if(w->lastpoke && w->lastpoke != uitotalmillis)
                {   // poke windows are updated every frame or disregarded
                    w->visible = false;
                    continue;
                }

                w->dist = 0.0f;
                w->visible = true;

                if(w->vistest)
                {   // visibility test script provided
                    w->setargs();
                    if(!executebool(w->vistest->code))
                    {
                        w->visible = false;
                        continue;
                    }
                }

                if(w->pos != nullvec)
                {   // can only be used for things with an actual 3d position
                    if(w->visible)
                    {
                        w->dist = w->pos.dist(camera1->o);
                        float maxdist = w->maxdist > 0 ? min(w->maxdist, uimaxdist) : uimaxdist;
                        if(maxdist > 0 && w->dist > maxdist) w->visible = false;
                    }

                    // if(w->visible && !getvisible(camera1->o, camera1->yaw, camera1->pitch, w->pos, curfov, fovy, 0, -1))
                    //    w->visible = false;

                    if(!w->visible && w->forcetest)
                    {   // fall back to the force test script
                        w->setargs();
                        if(executebool(w->forcetest->code))
                        {
                            w->dist = w->pos.dist(camera1->o);
                            w->visible = true;
                        }
                    }

                    if(!w->visible) continue;
                }


                uiscale = 1;
                w->build();

                if(!children.inrange(i)) break;
                if(children[i] != w) i--;
            });

            loopwindowsrev(w, if(!w->visible && !w->persist) hide(w, i));

            children.sort(Window::compare);
            resetstate(); // IMPORTED

            if(surfaceinput == type) checkinputsteal();
            if(!mousetracking) mousetrackvec = vec2(0, 0);

            popfont();
            curtextscale = oldtextscale;
            uiscale = olduiscale;
        }

        void initchildren(Object *o)
        {
            if(!o) return;
            o->init();
            loopv(o->children) initchildren(o->children[i]);
        }

        void init()
        {
            loopwindows(w, if(w->visible) initchildren(w));
        }

        void render()
        {
            float oldtextscale = curtextscale, olduiscale = uiscale;
            curtextscale = 1;
            uiscale = 1;
            hintoffset2d = uihintoffset2d;
            hintoffset3d = uihintoffset3d;

            pushfont();
            layout();
            adjustchildren();
            draw();
            popfont();

            curtextscale = oldtextscale;
            uiscale = olduiscale;
        }

        bool show(Window *w, const vec &pos = nullvec, float m = 0, float y = 0, float p = 0, float s = 1, float dy = 0, float dp = 0)
        {
            if(children.find(w) >= 0) return false;
            w->resetchildstate();
            children.add(w);
            w->show(pos, m, y, p, s, dy, dp);
            return true;
        }

        void hide(Window *w, int index)
        {
            children.remove(index);
            childstate = 0;
            loopchildren(o, childstate |= o->state | o->childstate);
            w->hide();
        }

        bool hide(Window *w)
        {
            int index = children.find(w);
            if(index < 0) return false;
            hide(w, index);
            return true;
        }

        bool hidetop()
        {
            loopwindowsrev(w,
            {
                if(!w->visible || !checkexclusive(w)) continue;
                if(surfacetype == SURFACE_WORLD || w->state&STATE_HIDDEN || w->persist) continue;
                if(w->allowinput || w->passthrough) { hide(w, i); return true; }
            });
            return false;
        }

        int hideall(bool force = false)
        {
            int hidden = 0;
            loopwindowsrev(w,
            {
                if(!force && w->persist) continue;
                hide(w, i);
                hidden++;
            });
            return hidden;
        }

        int allowinput(bool cursor, bool force = false)
        {
            int ret = 0;
            if(force || surfaceinput == type) loopwindows(w,
            {
                if(!w->visible || !checkexclusive(w)) continue;
                if(surfacetype == SURFACE_WORLD && !w->attached && (!cursor || w->hitx < 0 || w->hitx > 1 || w->hity < 0 || w->hity > 1)) continue;
                if(w->allowinput && !(w->state&STATE_HIDDEN) && ret != 1) ret = max(w->allowinput, ret);
            });
            return ret;
        }

        bool hasmenu(bool pass = true)
        {
            if(surfaceinput == type) loopwindows(w,
            {
                if(!w->visible || !checkexclusive(w)) continue;
                if(surfacetype != SURFACE_WORLD && w->menu && !(w->state&STATE_HIDDEN)) return !pass || !w->passthrough;
            });
            return false;
        }

        const char *topname()
        {
            loopwindowsrev(w,
            {
                if(!w->visible || !checkexclusive(w)) continue;
                if(surfacetype != SURFACE_WORLD && (w->allowinput || w->passthrough) && !(w->state&STATE_HIDDEN)) { return w->name; }
            });
            return NULL;
        }

        void draw(float sx, float sy) {}

        void draw()
        {
            if(children.empty()) return;
            loopwindows(w,
            {
                if(!w->visible || !checkexclusive(w)) continue;
                uiscale = 1;
                if(w->tooltip) // follows cursor
                    w->setpos(getuicursorx() - (w->w * getuicursorx(false)), getuicursory() >= 0.5f ? getuicursory() - w->h-uitipoffset : getuicursory() + hud::cursorsize + uitipoffset);
                else if(w->popup && !w->overridepos) w->setpos(getuicursorx() - (w->w * getuicursorx(false)), getuicursory() - w->h * 0.5f);
                w->draw();
            });
        }
    };

    #define UISURFCMD(vname, valtype, args, body) \
        ICOMMAND(0, ui##vname, valtype, args, { \
            if(surface) \
            { \
                Surface *o = surface; \
                body; \
            } \
            else if(!propagating) conoutf(colourorange, "Warning: No surface available for ui%s", #vname); \
        });

    #define UISURFARGB(vname) \
        UISURFCMD(vname, "iN$", (int *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = *val!=0; \
            else if(*numargs < 0) intret(o->vname ? 1 : 0); \
            else printvar(id, o->vname ? 1 : 0); \
        });

    #define UISURFARGK(vname, valtype, type, cmin, cmax, valdef) \
        UISURFCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp((valdef), cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UISURFARGSCALED(vname, valtype, type, cmin, cmax) \
        UISURFCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax) * uiscale); \
            else if(*numargs < 0) type##ret(o->vname * uiscale); \
            else print##type##var(id, o->vname * uiscale); \
        });

    #define UISURFARG(vname, valtype, type, cmin, cmax) \
        UISURFCMD(vname, valtype "N$", (type *val, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = type(clamp(*val, cmin, cmax)); \
            else if(*numargs < 0) type##ret(o->vname); \
            else print##type##var(id, o->vname); \
        });

    #define UISURFARGV(vname) \
        UISURFCMD(vname, "fffN$", (float *x, float *y, float *z, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname = vec(*x, *y, *z); \
            else \
            { \
                if(*numargs < 0) result(o->vname); \
                else printsvar(id, o->vname); \
            } \
        }); \
        UISURFCMD(vname##x, "fN$", (float *v, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname.x = *v; \
            else if(*numargs < 0) floatret(o->vname.x); \
            else printfvar(id, o->vname.x); \
        }); \
        UISURFCMD(vname##y, "fN$", (float *v, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname.y = *v; \
            else if(*numargs < 0) floatret(o->vname.y); \
            else printfvar(id, o->vname.y); \
        }); \
        UISURFCMD(vname##z, "fN$", (float *v, int *numargs, ident *id), { \
            if(*numargs > 0) o->vname.z = *v; \
            else if(*numargs < 0) floatret(o->vname.z); \
            else printfvar(id, o->vname.z); \
        });

    UISURFARGB(lockcursor);
    UISURFARGB(lockscroll);
    UISURFARG(cursortype, "i", int, 0, int(CURSOR_MAX)-1);
    ICOMMANDV(0, uisurfacetype, surfacetype);
    ICOMMANDV(0, uisurfaceformat, surfaceformat);
    ICOMMANDV(0, uisurfaceinput, surfaceinput);

    ICOMMAND(0, uimousetrackx, "", (), {
        if(surface)
        {
            surface->mousetracking = true;
            floatret(surface->mousetrackvec.x);
        }
    });
    ICOMMAND(0, uimousetracky, "", (), {
        if(surface)
        {
            surface->mousetracking = true;
            floatret(surface->mousetrackvec.y);
        }
    });

    bool pushsurface(int surf)
    {
        if(surf < 0 || surf >= SURFACE_MAX || !surfaces[surf]) return false;
        surfacestack.add(surface);
        surface = surfaces[surf];
        surfacetype = surface->type;
        surfacehint2d = &surface->hintoffset2d;
        surfacehint3d = &surface->hintoffset3d;
        return true;
    }

    void popsurface()
    {
        surface = surfacestack.empty() ? NULL : surfacestack.pop();
        surfacetype = surface ? surface->type : -1;
        surfacehint2d = surface ? &surface->hintoffset2d : NULL;
        surfacehint3d = surface ? &surface->hintoffset3d : NULL;
    }

    #define DOSURFACE(surf, body) \
    { \
        if(pushsurface(surf))\
        { \
            body; \
            popsurface(); \
        } \
    }

    #define SWSURFACE(surf, body) \
    { \
        if(surf >= 0) DOSURFACE(surf, body) \
        else loopk(SURFACE_MAX) if(surfaces[k]) DOSURFACE(k, body) \
    }

    void Window::build()
    {
        if(!surface) return;
        reset(surface);
        setup();

        Window *oldwindow = window;
        window = this;
        setargs();
        buildlevel = taglevel = -1;
        if(contents) buildchildren(contents->code, mapdef);
        window = oldwindow;
    }

    bool newui(const char *name, int stype, const char *contents, const char *onshow, const char *onhide, const char *vistest, const char *forcetest, bool mapdef = false, const char *dyn = NULL, tagval *args = NULL, int numargs = 0)
    {
        if(!name || !*name || !contents || !*contents || stype < 0 || stype >= SURFACE_MAX) return false;

        if(mapdef && !(identflags&IDF_MAP) && !editmode)
        {
            conoutf(colourred, "Map %s UI %s is only directly modifiable in editmode", SURFACE_STR[stype], name);
            return false;
        }

        if(!pushsurface(stype))
        {
            conoutf(colourred, "Cannot create %s on Surface %s", name, SURFACE_STR[stype]);
            return false;
        }

        Window *w = surface->windows.find(name, NULL);
        if(w)
        {
            if(w == window)
            {
                conoutf(colourred, "Cannot redefine %s UI %s while it is currently active", SURFACE_STR[stype], w->name);
                popsurface();
                return false;
            }

            if(!w->mapdef && mapdef)
            {
                conoutf(colourred, "Cannot override builtin %s UI %s with a one from the map", SURFACE_STR[stype], w->name);
                popsurface();
                return false;
            }

            surface->hide(w);
            surface->windows.remove(name);
            delete w;
            loopv(surface->texs)
            {
                Texture *t = surface->texs[i];
                if(strcmp(name, t->comp)) continue;
                t->rendered = 0; // redraw
            }
        }

        surface->windows[name] = new Window(name, contents, onshow, onhide, vistest, forcetest, mapdef, dyn, args, numargs);
        popsurface();

        return true;
    }

    ICOMMAND(0, newui, "sisssss", (char *name, int *stype, char *contents, char *onshow, char *onhide, char *vistest, char *forcetest), if(!(identflags&IDF_MAP)) newui(name, *stype, contents, onshow, onhide, vistest, forcetest, false));
    ICOMMAND(0, mapui, "sisssss", (char *name, int *stype, char *contents, char *onshow, char *onhide, char *vistest, char *forcetest), newui(name, *stype, contents, onshow, onhide, vistest, forcetest, true));

    void closedynui(const char *name, int stype, bool mapdef)
    {
        SWSURFACE(stype, enumerate(surface->windows, Window *, w,
        {
            if(!w->dyn || !*w->dyn || w->mapdef != mapdef || (name && *name && strcmp(w->dyn, name))) continue;
            surface->hide(w);
        }));
    }

    ICOMMAND(0, closedynui, "si", (char *name, int *stype), closedynui(name, *stype, false));

    void cleardynui(const char *name, int stype, bool mapdef)
    {
        SWSURFACE(stype, enumerate(surface->windows, Window *, w,
        {
            if(!w->dyn || !*w->dyn || w->mapdef != mapdef || (name && *name && strcmp(w->dyn, name))) continue;
            surface->hide(w);
            surface->windows.remove(w->name);
            delete w;
        }));
    }

    ICOMMAND(0, cleardynui, "sb", (char *name, int *stype), cleardynui(name, *stype, false));
    ICOMMAND(0, clearmapdynui, "sb", (char *name, int *stype), cleardynui(name, *stype, true));

    struct DynUI
    {
        char *name, *contents, *onshow, *onhide, *vistest, *forcetest;
        bool mapdef;

        DynUI(const char *s) : name(newstring(s)), contents(NULL), onshow(NULL), onhide(NULL), vistest(NULL), forcetest(NULL), mapdef(false) {}
        ~DynUI()
        {
            DELETEA(name);
            cleanup();
        }

        void cleanup()
        {
            DELETEA(contents);
            DELETEA(onshow);
            DELETEA(onhide);
            DELETEA(vistest);
            DELETEA(forcetest);
        }
    };
    vector<DynUI *> dynuis;

    DynUI *finddynui(const char *name)
    {
        if(!name || !*name) return NULL;
        loopv(dynuis) if(!strcmp(dynuis[i]->name, name)) return dynuis[i];
        return NULL;
    }

    struct DynUIRef
    {
        char *name;
        int param;
        string ref;

        DynUIRef(const char *s, int n) : name(newstring(s)), param(n)
        {
            formatstring(ref, "%s_%d", name, param);
        }

        ~DynUIRef()
        {
            DELETEA(name);
        }

        bool match(const char *s, int n) { return !strcmp(name, s) && param == n; }
    };
    vector<DynUIRef *> dynuirefs;

    DynUIRef *finddynuiref(const char *name, int param = -1)
    {
        if(!name || !*name || param < 0) return NULL; // not dynui
        loopv(dynuirefs) if(dynuirefs[i]->match(name, param)) return dynuirefs[i];
        return NULL;
    }

    const char *dynuiref(const char *name, int param = -1, bool create = false)
    {
        if(!name || !*name || param < 0) return name; // not dynui

        DynUIRef *d = finddynuiref(name, param);
        if(!d)
        {
            if(!create) return NULL;
            d = new DynUIRef(name, param);
            dynuirefs.add(d);
        }

        return d ? d->ref : NULL;
    }

    Window *dynuirefwin(const char *name, int param = -1, bool create = false)
    {
        if(!name || !*name) return NULL;
        const char *ref = dynuiref(name, param, create);
        return ref && *ref ? surface->windows.find(ref, NULL) : NULL;
    }

    void dynui(const char *name, const char *contents, const char *onshow, const char *onhide, const char *vistest, const char *forcetest, bool mapdef)
    {
        if(!name || !*name || !contents || !*contents) return;

        DynUI *m = NULL;
        loopv(dynuis) if(!strcmp(dynuis[i]->name, name))
        {
            m = dynuis[i];
            break;
        }

        if(m && !m->mapdef && mapdef)
        {
            conoutf(colourred, "Cannot override builtin DynUI %s with one from the map", name);
            return;
        }
        if(mapdef && !(identflags&IDF_MAP) && !editmode)
        {
            conoutf(colourred, "Map DynUI %s is only directly modifiable in editmode", name);
            return;
        }

        if(!m) dynuis.add(m = new DynUI(name));
        else m->cleanup();

        m->contents = newstring(contents);
        m->onshow = onshow && *onshow ? newstring(onshow) : NULL;
        m->onhide = onhide && *onhide ? newstring(onhide) : NULL;
        m->vistest = vistest && *vistest ? newstring(vistest) : NULL;
        m->forcetest = forcetest && *forcetest ? newstring(forcetest) : NULL;
        m->mapdef = mapdef;
    }

    ICOMMAND(0, dynui, "ssssss", (char *name, char *contents, char *onshow, char *onhide, char *vistest, char *forcetest), dynui(name, contents, onshow, onhide, vistest, forcetest, (identflags&IDF_MAP) != 0));
    ICOMMAND(0, mapdynui, "ssssss", (char *name, char *contents, char *onshow, char *onhide, char *vistest, char *forcetest), dynui(name, contents, onshow, onhide, vistest, forcetest, true));

    const char *dynuiexec(const char *name, int param)
    {
        if(!name || !*name || param < 0) return NULL;

        DynUI *d = finddynui(name);
        if(d)
        {
            tagval t; t.setint(param);
            const char *refname = dynuiref(name, param, true); // should be the only one creating refs
            if(newui(refname, surfacetype, d->contents, d->onshow, d->onhide, d->vistest, d->forcetest, d->mapdef, d->name, &t, 1))
                return refname;
        }

        return NULL;
    }

    Window *dynuicreate(const char *name, int param)
    {
        if(!name || !*name || param < 0) return NULL;

        const char *refname = dynuiexec(name, param);
        if(refname) return surface->windows.find(refname, NULL);

        return NULL;
    }

    bool windowvisible(Window *w)
    {
        #if 0
        if(!w->vistest) return true;
        w->setargs();
        return executebool(w->vistest->code);
        #else
        return true;
        #endif
    }

    bool showui(const char *name, int stype, int param, const vec &origin, float maxdist, float yaw, float pitch, float scale, float detentyaw, float detentpitch)
    {
        if(!pushsurface(stype)) return false;

        Window *w = dynuirefwin(name, param);
        if(!w) w = dynuicreate(name, param);

        bool ret = w && windowvisible(w) && surface->show(w, origin, maxdist, yaw, pitch, scale, detentyaw, detentpitch);

        popsurface();

        return ret;
    }

    // poke UI's don't display unless they are poked every frame
    bool pokeui(const char *name, int stype, int param, const vec &origin, float maxdist, float yaw, float pitch, float scale, float detentyaw, float detentpitch)
    {
        if(!engineready || !pushsurface(stype)) return false;

        Window *w = dynuirefwin(name, param);

        if(w && surface->children.find(w) >= 0)
        {
            if(!windowvisible(w))
            {
                surface->hide(w);
                return false;
            }

            w->origin = origin;
            w->maxdist = maxdist;
            w->yaw = yaw;
            w->pitch = pitch;
            w->scale = scale;
            w->detentyaw = detentyaw;
            w->detentpitch = detentpitch;
            w->lastpoke = uitotalmillis;

            popsurface();
            return true;
        }

        if(!w) w = dynuicreate(name, param);

        bool ret = false;
        if(w && windowvisible(w) && surface->show(w, origin, maxdist, yaw, pitch, scale, detentyaw, detentpitch))
        {
            w->lastpoke = uitotalmillis;
            ret = true;
        }

        popsurface();

        return ret;
    }

    bool hideui(const char *name, int stype, int param)
    {
        if(!pushsurface(stype)) return false;

        if(!name || !*name)
        {
            bool ret = surface->hideall(false) > 0;
            popsurface();
            return ret;
        }

        Window *w = dynuirefwin(name, param);
        bool ret = w && surface->hide(w);

        popsurface();

        return ret;
    }

    bool toggleui(const char *name, int stype, int param, const vec &origin, float maxdist, float yaw, float pitch, float scale, float detentyaw, float detentpitch)
    {
        if(showui(name, stype, param, origin, maxdist, yaw, pitch, scale, detentyaw, detentpitch)) return true;
        hideui(name, stype, param);
        return false;
    }

    int openui(const char *name, int stype)
    {
        if(uitest(name, stype) || !pushsurface(stype)) return 0;

        defformatstring(cmd, "%s \"%s\" %d", uiopencmd, name ? name : "",  stype);
        int ret = execute(cmd);

        popsurface();

        return ret;
    }

    int closeui(const char *name, int stype)
    {
        if(!uitest(name, stype) || !pushsurface(stype)) return 0;

        defformatstring(cmd, "%s \"%s\" %d", uiclosecmd, name ? name : "", stype);
        int ret = execute(cmd);

        popsurface();

        return ret;
    }

    void hideall()
    {
        loopi(SURFACE_MAX) hideui(NULL, i);
    }

    void holdui(const char *name, bool on, int stype, int param, const vec &origin, float maxdist, float yaw, float pitch, float scale, float detentyaw, float detentpitch)
    {
        if(on) showui(name, stype, param, origin, maxdist, yaw, pitch, scale, detentyaw, detentpitch);
        else hideui(name, stype, param);
    }

    void pressui(const char *name, bool on, int stype, int param, const vec &origin, float maxdist, float yaw, float pitch, float scale, float detentyaw, float detentpitch)
    {
        if(on) { if(!uitest(name, stype, param)) showui(name, stype, param, origin, maxdist, yaw, pitch, scale, detentyaw, detentpitch); }
        else if(uitest(name, stype, param)) hideui(name, stype, param);
    }

    bool uitest(const char *name, int stype, int param)
    {
        if(!pushsurface(stype)) return false;

        if(!name || !*name)
        {
            bool ret = surface->children.length() > 0;
            popsurface();
            return ret;
        }

        Window *w = dynuirefwin(name, param);
        bool ret = w && surface->children.find(w) >= 0;

        popsurface();

        return ret;
    }

    ICOMMAND(IDF_NOECHO, showui, "sbbgggfgffff", (char *name, int *sf, int *param, float *x, float *y, float *z, float *maxdist, float *yaw, float *pitch, float *scale, float *detentyaw, float *detentpitch), intret(showui(name, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param, vec(*x, *y, *z), *maxdist, *yaw, *pitch, *scale, *detentyaw, *detentpitch) ? 1 : 0));
    ICOMMAND(IDF_NOECHO, pokeui, "sbbgggfgffff", (char *name, int *sf, int *param, float *x, float *y, float *z, float *maxdist, float *yaw, float *pitch, float *scale, float *detentyaw, float *detentpitch), intret(pokeui(name, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param, vec(*x, *y, *z), *maxdist, *yaw, *pitch, *scale, *detentyaw, *detentpitch) ? 1 : 0));
    ICOMMAND(IDF_NOECHO, hideui, "sbb", (char *name, int *sf, int *param), intret(hideui(name, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param) ? 1 : 0));
    ICOMMAND(IDF_NOECHO, toggleui, "sbbgggfgffff", (char *name, int *sf, int *param, float *x, float *y, float *z, float *maxdist, float *yaw, float *pitch, float *scale, float *detentyaw, float *detentpitch), intret(toggleui(name, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param, vec(*x, *y, *z), *maxdist, *yaw, *pitch, *scale, *detentyaw, *detentpitch) ? 1 : 0));
    ICOMMAND(IDF_NOECHO, holdui, "sbbgggfgffffD", (char *name, int *sf, int *param, float *x, float *y, float *z, float *maxdist, float *yaw, float *pitch, float *scale, float *detentyaw, float *detentpitch, int *down), holdui(name, *down!=0, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param, vec(*x, *y, *z), *maxdist, *yaw, *pitch, *scale, *detentyaw, *detentpitch));
    ICOMMAND(IDF_NOECHO, pressui, "sbbgggfgffffD", (char *name, int *sf, int *param, float *x, float *y, float *z, float *maxdist, float *yaw, float *pitch, float *scale, float *detentyaw, float *detentpitch, int *down), pressui(name, *down!=0, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param, vec(*x, *y, *z), *maxdist, *yaw, *pitch, *scale, *detentyaw, *detentpitch));
    ICOMMAND(IDF_NOECHO, uitest, "sbb", (char *name, int *sf, int *param), intret(uitest(name, *sf >= 0 && *sf < SURFACE_MAX ? *sf : int(SURFACE_FOREGROUND), *param) ? 1 : 0));

    #define SURFACEOP(idx)  Surface *s = (idx) >= 0 && (idx) < SURFACE_MAX ? surfaces[(idx)] : (surface ? surface : surfaces[SURFACE_FOREGROUND]);

    ICOMMAND(IDF_NOECHO, hidetopui, "b", (int *sf), SURFACEOP(*sf); intret(s && s->hidetop() ? 1 : 0));
    ICOMMAND(IDF_NOECHO, hideallui, "bii", (int *sf, int *n), SURFACEOP(*sf); intret(s ? s->hideall(*n != 0) : 0));
    ICOMMAND(IDF_NOECHO, uitopwindow, "b", (int *sf), SURFACEOP(*sf); result(s ? s->topname() : ""));
    ICOMMANDVS(0, uitopname, surface ? surface->topname() : (surfaces[SURFACE_FOREGROUND] ? surfaces[SURFACE_FOREGROUND]->topname() : ""));

    ICOMMANDVS(0, uiname, window ? window->name : "")

    struct HorizontalList : Object
    {
        float space, subw;

        static const char *typestr() { return "#HorizontalList"; }
        const char *gettype() const { return typestr(); }

        void setup(float space_ = 0)
        {
            Object::setup();
            space = space_;
        }

        uchar childalign() const { return ALIGN_VCENTER; }

        void layout()
        {
            subw = h = 0;
            loopchildren(o,
            {
                o->x = subw;
                o->y = 0;
                o->layout();
                subw += o->w;
                h = max(h, o->y + o->h);
            });
            w = subw + space*max(children.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offset = 0, sx = 0, cspace = (w - subw) / max(children.length() - 1, 1), cstep = (w - subw) / children.length();
            loopv(children)
            {
                Object *o = children[i];
                o->x = offset;
                offset += o->w + cspace;
                float sw = o->w + cstep;
                o->adjustlayout(sx, 0, sw, h);
                sx += sw;
            }
        }
    };

    ICOMMAND(0, uihlist, "fe", (float *space, uint *children),
        BUILD(HorizontalList, o, o->setup(*space*uiscale), children));

    UIARGSCALED(HorizontalList, hlist, space, "f", float, 0.f, FVAR_MAX);

    struct VerticalList : Object
    {
        float space, subh;

        static const char *typestr() { return "#VerticalList"; }
        const char *gettype() const { return typestr(); }

        void setup(float space_ = 0)
        {
            Object::setup();
            space = space_;
        }

        uchar childalign() const { return ALIGN_HCENTER; }

        void layout()
        {
            w = subh = 0;
            loopchildren(o,
            {
                o->x = 0;
                o->y = subh;
                o->layout();
                subh += o->h;
                w = max(w, o->x + o->w);
            });
            h = subh + space*max(children.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offset = 0, sy = 0, rspace = (h - subh) / max(children.length() - 1, 1), rstep = (h - subh) / children.length();
            loopchildren(o,
            {
                o->y = offset;
                offset += o->h + rspace;
                float sh = o->h + rstep;
                o->adjustlayout(0, sy, w, sh);
                sy += sh;
            });
        }
    };

    ICOMMAND(0, uivlist, "fe", (float *space, uint *children),
        BUILD(VerticalList, o, o->setup(*space*uiscale), children));

    UIARGSCALED(VerticalList, vlist, space, "f", float, 0.f, FVAR_MAX);

    ICOMMAND(0, uilist, "fe", (float *space, uint *children),
    {
        for(Object *parent = buildparent; parent && !parent->istype<VerticalList>(); parent = parent->parent)
        {
            if(parent->istype<HorizontalList>())
            {
                BUILD(VerticalList, o, o->setup(*space*uiscale), children);
                return;
            }
        }
        BUILD(HorizontalList, o, o->setup(*space*uiscale), children);
    });

    struct Grid : Object
    {
        int columns;
        float spacew, spaceh, subw, subh;
        vector<float> widths, heights;

        static const char *typestr() { return "#Grid"; }
        const char *gettype() const { return typestr(); }

        void setup(int columns_, float spacew_ = 0, float spaceh_ = 0)
        {
            Object::setup();
            columns = columns_;
            spacew = spacew_;
            spaceh = spaceh_;
        }

        uchar childalign() const { return 0; }

        void layout()
        {
            widths.setsize(0);
            heights.setsize(0);

            int column = 0, row = 0;
            loopchildren(o,
            {
                o->layout();
                if(column >= widths.length()) widths.add(o->w);
                else if(o->w > widths[column]) widths[column] = o->w;
                if(row >= heights.length()) heights.add(o->h);
                else if(o->h > heights[row]) heights[row] = o->h;
                column = (column + 1) % columns;
                if(!column) row++;
            });

            subw = subh = 0;
            loopv(widths) subw += widths[i];
            loopv(heights) subh += heights[i];
            w = subw + spacew*max(widths.length() - 1, 0);
            h = subh + spaceh*max(heights.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            int row = 0, column = 0;
            float offsety = 0, sy = 0, offsetx = 0, sx = 0,
                  cspace = (w - subw) / max(widths.length() - 1, 1),
                  cstep = (w - subw) / widths.length(),
                  rspace = (h - subh) / max(heights.length() - 1, 1),
                  rstep = (h - subh) / heights.length();
            loopchildren(o,
            {
                o->x = offsetx;
                o->y = offsety;
                o->adjustlayout(sx, sy, widths[column] + cstep, heights[row] + rstep);
                offsetx += widths[column] + cspace;
                sx += widths[column] + cstep;
                column = (column + 1) % columns;
                if(!column)
                {
                    offsetx = sx = 0;
                    offsety += heights[row] + rspace;
                    sy += heights[row] + rstep;
                    row++;
                }
            });
        }
    };

    ICOMMAND(0, uigrid, "iffe", (int *columns, float *spacew, float *spaceh, uint *children),
        BUILD(Grid, o, o->setup(*columns, *spacew*uiscale, *spaceh*uiscale), children));

    UIARGSCALED(Grid, grid, spacew, "f", float, 0.f, FVAR_MAX);
    UIARGSCALED(Grid, grid, spaceh, "f", float, 0.f, FVAR_MAX);
    UIARG(Grid, grid, columns, "i", int, 0, VAR_MAX);

    struct TableHeader : Object
    {
        int columns;

        TableHeader() : columns(-1) {}

        static const char *typestr() { return "#TableHeader"; }
        const char *gettype() const { return typestr(); }

        uchar childalign() const { return columns < 0 ? ALIGN_VCENTER : ALIGN_DEFAULT; }

        int childcolumns() const { return columns; }

        void buildchildren(uint *columndata, uint *contents)
        {
            Object *oldparent = buildparent;
            buildparent = this;
            buildchild = 0;

            const char *oldttag = curtag;
            if(buildlevel != taglevel) curtag = NULL;
            buildlevel++;

            executeret(columndata);
            if(columns != buildchild) while(children.length() > buildchild) delete children.pop();
            columns = buildchild;
            if((*contents&CODE_OP_MASK) != CODE_EXIT) executeret(contents);
            while(children.length() > buildchild) delete children.pop();

            curtag = oldttag;
            buildlevel--;

            buildparent = oldparent;
            resetstate();
        }

        void adjustchildren()
        {
            loopchildrange(columns, children.length(), o, o->adjustlayout(0, 0, w, h));
        }

        void draw(float sx, float sy)
        {
            drawn = true;

            loopchildrange(columns, children.length(), o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
            loopchildrange(0, columns, o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
        }
    };
    #define BUILDCOLUMNS(type, o, setup, columndata, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            if(o) \
            { \
                setup; \
                o->buildchildren(columndata, contents); \
            } \
        } \
    } while(0)

    ICOMMAND(0, uitableheader, "ee", (uint *columndata, uint *children),
        BUILDCOLUMNS(TableHeader, o, o->setup(), columndata, children));

    struct TableRow : TableHeader
    {
        static const char *typestr() { return "#TableRow"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }
    };

    ICOMMAND(0, uitablerow, "ee", (uint *columndata, uint *children),
        BUILDCOLUMNS(TableRow, o, o->setup(), columndata, children));

    struct Table : Object
    {
        float spacew, spaceh, subw, subh;
        int column;
        vector<float> widths;

        static const char *typestr() { return "#Table"; }
        const char *gettype() const { return typestr(); }

        void setup(float spacew_ = 0, float spaceh_ = 0, int column_ = 0)
        {
            Object::setup();
            spacew = spacew_;
            spaceh = spaceh_;
            column = column_;
        }

        uchar childalign() const { return 0; }

        void layout()
        {
            widths.setsize(0);

            w = subh = 0;
            loopchildren(o,
            {
                o->layout();
                int cols = o->childcolumns();
                while(widths.length() < cols) widths.add(0);
                loopj(cols)
                {
                    Object *c = o->children[j];
                    if(c->w > widths[j]) widths[j] = c->w;
                }
                w = max(w, o->w);
                subh += o->h;
            });

            subw = 0;
            if(column > 0)
            {
                float wadjust = spacew * max(widths.length() - 1, 0) / max(widths.length(), 1);
                loopv(widths)
                {
                    widths[i] += wadjust;
                    subw += widths[i];
                }
                w = max(w, subw);
            }
            else
            {
                loopv(widths) subw += widths[i];
                w = max(w, subw + spacew * max(widths.length() - 1, 0));
            }
            h = subh + spaceh * max(children.length() - 1, 0);
        }

        void adjustrow(Object *o, float &offsety, float &sy, float rspace, float rstep)
        {
            o->x = 0;
            o->y = offsety;
            o->w = w;
            offsety += o->h + rspace;
            float sh = o->h + rstep;
            o->adjustlayout(0, sy, w, sh);
            sy += sh;
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offsety = 0, sy = 0,
                  rspace = (h - subh) / max(children.length() - 1, 1),
                  rstep = (h - subh) / children.length(),
                  cspace = column > 0 ? 0.f : (w - subw) / max(widths.length() - 1, 1),
                  cstep = column > 0 ? 0.f : (w - subw) / widths.length(),
                  cwidth = w - subw;

            loopchildren(o,
            {
                o->x = 0;
                o->y = offsety;
                o->w = w;
                offsety += o->h + rspace;
                float sh = o->h + rstep;
                o->adjustlayout(0, sy, w, sh);
                sy += sh;

                float offsetx = 0;
                float sx = 0;
                int cols = o->childcolumns();
                loopj(cols)
                {
                    Object *c = o->children[j];

                    c->x = offsetx;
                    offsetx += widths[j];
                    if(column <= 0) offsetx += cspace;

                    float sw = widths[j];
                    if(column > 0)
                        sw += j + 1 == column ? cwidth : 0.f;
                    else sw += cstep;

                    c->adjustlayout(sx, 0, sw, o->h);

                    sx += sw;
                }
            });
        }
    };

    ICOMMAND(0, uitable, "ffe", (float *spacew, float *spaceh, uint *children),
        BUILD(Table, o, o->setup(*spacew*uiscale, *spaceh*uiscale), children));

    UIARGSCALED(Table, table, spacew, "f", float, 0.f, FVAR_MAX);
    UIARGSCALED(Table, table, spaceh, "f", float, 0.f, FVAR_MAX);
    UIARG(Table, table, column, "i", int, 0, VAR_MAX);

    struct Padder : Object
    {
        float left, right, top, bottom;

        void setup(float left_, float right_, float top_, float bottom_)
        {
            Object::setup();
            left = left_;
            right = right_;
            top = top_;
            bottom = bottom_;
        }

        static const char *typestr() { return "#Padder"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            w = left;
            h = top;
            loopchildren(o,
            {
                o->x = left;
                o->y = top;
                o->layout();
                w = max(w, o->x + o->w);
                h = max(h, o->y + o->h);
            });
            w += right;
            h += bottom;
        }

        void adjustchildren()
        {
            adjustchildrento(left, top, w - (left+right), h - (top+bottom));
        }
    };

    ICOMMAND(0, uipad, "ffffe", (float *left, float *right, float *top, float *bottom, uint *children),
        BUILD(Padder, o, o->setup(*left*uiscale, *right*uiscale, *top*uiscale, *bottom*uiscale), children));

    ICOMMAND(0, uispace, "ffe", (float *spacew, float *spaceh, uint *children),
        BUILD(Padder, o, o->setup(*spacew*uiscale, *spacew*uiscale, *spaceh*uiscale, *spaceh*uiscale), children));

    UIARGSCALED(Padder, pad, left, "f", float, 0.f, FVAR_MAX);
    UIARGSCALED(Padder, pad, right, "f", float, 0.f, FVAR_MAX);
    UIARGSCALED(Padder, pad, top, "f", float, 0.f, FVAR_MAX);
    UIARGSCALED(Padder, pad, bottom, "f", float, 0.f, FVAR_MAX);

    struct Offsetter : Object
    {
        float offsetx, offsety;

        void setup(float offsetx_, float offsety_)
        {
            Object::setup();
            offsetx = offsetx_;
            offsety = offsety_;
        }

        static const char *typestr() { return "#Offsetter"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            Object::layout();

            loopchildren(o,
            {
                o->x += offsetx;
                o->y += offsety;
            });

            w += offsetx;
            h += offsety;
        }

        void adjustchildren()
        {
            adjustchildrento(offsetx, offsety, w - offsetx, h - offsety);
        }
    };

    ICOMMAND(0, uioffset, "ffe", (float *offsetx, float *offsety, uint *children),
        BUILD(Offsetter, o, o->setup(*offsetx*uiscale, *offsety*uiscale), children));

    UIARGSCALED(Offsetter, offset, x, "f", float, FVAR_MIN, FVAR_MAX);
    UIARGSCALED(Offsetter, offset, y, "f", float, FVAR_MIN, FVAR_MAX);

    struct Color
    {
        bvec4 val;

        Color() { val.mask = 0xFFFFFFFFU; }
        Color(int c, bool force = false) { fillvalue(c, force); }
        Color(uint c, bool force = false) { fillvalue(c, force); }
        Color(uint c, uchar a) { fillvalue(c, a); }
        Color(uchar r, uchar g, uchar b, uchar a = 255) : val(r, g, b, a) {}
        Color(const Color &c) : val(c.val) {}

        void fillvalue(int c, bool force = false)
        {
            val.r = (uint(c)>>16)&0xFF;
            val.g = (uint(c)>>8)&0xFF;
            val.b = uint(c)&0xFF;
            val.a = uint(c)>>24 || force ? uint(c)>>24 : 0xFF;
        }

        void fillvalue(uint c, bool force = false)
        {
            val.r = (c>>16)&0xFF;
            val.g = (c>>8)&0xFF;
            val.b = c&0xFF;
            val.a = c>>24 || force ? uint(c)>>24 : 0xFF;
        }

        void fillvalue(uint c, uchar a)
        {
            val.r = (c>>16)&0xFF;
            val.g = (c>>8)&0xFF;
            val.b = c&0xFF;
            val.a = a;
        }

        void init(float scale = 1, float alpha = 1) { gle::colorub(val.r*scale, val.g*scale, val.b*scale, val.a*alpha); }
        void attrib(float scale = 1, float alpha = 1) { gle::attribub(val.r*scale, val.g*scale, val.b*scale, val.a*alpha); }
        void init(const Color &c) { gle::colorub(val.r*c.val.r, val.g*c.val.g, val.b*c.val.b, val.a*c.val.a); }
        void attrib(const Color &c) { gle::attribub(val.r*c.val.r, val.g*c.val.g, val.b*c.val.b, val.a*c.val.a); }

        static void def() { gle::defcolor(4, GL_UNSIGNED_BYTE); }

        Color &scale(const Color &c)
        {
            val.scale(c.val);
            return *this;
        }

        Color &combine(const Color &c)
        {
            val.combine(c.val);
            return *this;
        }

        bool operator==(const Color &o) const { return val.mask == o.val.mask; }
        bool operator!=(const Color &o) const { return val.mask != o.val.mask; }
    };

    struct Colored : Object
    {
        enum { SOLID = 0, MODULATE, OUTLINED, OVERWRITE, BUFFER, GLOW };
        enum { VERTICAL, HORIZONTAL };

        int type, dir, sep;
        vector<Color> colors;

        static const char *typestr() { return "#Colored"; }
        const char *gettype() const { return typestr(); }
        bool iscolour() const { return true; }

        void setupdraw(int drawflags = 0)
        {
            int outtype = -1;
            bool outsep = sep >= 0 ? sep != 0 : blendsepdef;
            switch(type)
            {
                case SOLID: outtype = BLEND_ALPHA; break;
                case MODULATE: outtype = BLEND_MOD; break;
                case OVERWRITE: outtype = BLEND_SRC; break;
                case BUFFER: outtype = BLEND_BUFFER; break;
                case GLOW: outtype = BLEND_GLOW; break;
                default: outtype = blendtypedef; break;
            }

            bool wantblend = outtype != blendtype || outsep != blendsep;
            if(wantblend) drawflags |= CHANGE_BLEND;
            drawflags |= CHANGE_COLOR;

            changedraw(drawflags);
            if(wantblend) setblend(outtype, outsep);
        }

        void setup(const Color &color_, int type_ = -1, int dir_ = -1, int sep_ = -1)
        {
            Object::setup();
            colors.setsize(0);
            colors.add(color_);
            type = type_ >= 0 ? type_ : -1;
            dir = dir_ >= 0 ? dir_ : VERTICAL;
            sep = sep_;
        }

        void rotatecolors(float amt, int colstart = 0, int colcount = 0)
        {
            if(amt == 0) return;
            int cols = clamp(colcount ? colcount : colors.length()-colstart, min(colstart, colors.length()-1), colors.length());
            if(cols <= 1) return;
            int pieces = 0;
            float progress = clamp(fabs(amt), 0.f, 1.f), part = 1.f/cols;
            while(progress >= part)
            {
                progress -= part;
                pieces++;
            }
            float iter = progress/part;
            static vector<Color> colorstack;
            colorstack.setsize(0);
            loopi(colstart) colorstack.add(colors[i]);
            loopirev(colstart) colors.remove(i);
            bool rev = amt < 0;
            int over = colors.length()-cols, index = rev ? 0 : colorstack.length();
            loopi(over) colorstack.add(colors[cols+i]);
            loopirev(over) colors.remove(cols+i);
            loopv(colors)
            {
                int p = rev ? i-pieces : (i+pieces)%cols, m = p >= 0 ? p : cols+p, q = rev ? m-1 : (m+1)%cols, n = q >= 0 ? q : cols+q;
                colorstack.insert(index,
                    Color(colors[m].val.r-int((colors[m].val.r-colors[n].val.r)*iter),
                          colors[m].val.g-int((colors[m].val.g-colors[n].val.g)*iter),
                          colors[m].val.b-int((colors[m].val.b-colors[n].val.b)*iter),
                          colors[m].val.a-int((colors[m].val.a-colors[n].val.a)*iter)
                         )
                    );
                if(!rev) index++;
            }
            colors.setsize(0);
            loopv(colorstack) colors.add(colorstack[i]);
        }
    };

    UIARGT(Colored, colour, type, "i", int, int(Colored::SOLID), int(Colored::OVERWRITE));
    UIARGT(Colored, colour, dir, "i", int, int(Colored::VERTICAL), int(Colored::HORIZONTAL));
    UIARGT(Colored, colour, sep, "i", int, -1, 1);

    UICMDT(Colored, colour, set, "iii", (int *c, int *pos, int *force),
    {
        if(o->colors.inrange(*pos)) o->colors[*pos] = Color(*c, *force != 0);
    });
    UICMDT(Colored, colour, get, "i", (int *pos), intret(o->colors[clamp(*pos, 0, o->colors.length()-1)].val.mask));
    UICMDT(Colored, colour, add, "ii", (int *c, int *force), o->colors.add(Color(*c, *force != 0)));
    UICMDT(Colored, colour, del, "i", (int *c),
    {
        loopvrev(o->colors) if(o->colors[i] == Color(*c)) o->colors.remove(i);
        if(o->colors.empty()) o->colors.add(Color(colourwhite));
    });
    UICMDT(Colored, colour, scale, "ibi", (int *c, int *pos, int *force),
    {
        if(*pos < 0) loopv(o->colors) o->colors[i].scale(Color(*c, *force != 0));
        else if(o->colors.inrange(*pos)) o->colors[*pos].scale(Color(*c, *force != 0));
    });
    UICMDT(Colored, colour, combine, "ibi", (int *c, int *pos, int *force),
    {
        if(*pos < 0) loopv(o->colors) o->colors[i].combine(Color(*c, *force != 0));
        else if(o->colors.inrange(*pos)) o->colors[*pos].combine(Color(*c, *force != 0));
    });
    UICMDT(Colored, colour, rotate, "fii", (float *amt, int *start, int *count), o->rotatecolors(*amt, *start, *count));
    UICMDT(Colored, colour, blend, "f", (float *blend), loopvk(o->colors) o->colors[k].val.a = clamp(uchar(*blend * o->colors[k].val.a), 0, 255));

    static const float defcoords[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    struct Filler : Colored
    {
        enum { FC_TL = 0, FC_TR, FC_BR, FC_BL, FC_MAX };

        float minw, minh;
        vec2 coords[FC_MAX];

        void setup(float minw_, float minh_, const Color &color_ = Color(colourwhite), int type_ = -1, int dir_ = -1)
        {
            Colored::setup(color_, type_, dir_);
            minw = minw_;
            minh = minh_;
            loopi(FC_MAX) loopj(2) coords[i][j] = defcoords[i][j];
        }

        void setup(const Color &color_, int type_ = -1, int dir_ = -1)
        {
            Colored::setup(color_, type_, dir_);
            minw = minh = 0;
            loopi(FC_MAX) loopj(2) coords[i][j] = defcoords[i][j];
        }

        static const char *typestr() { return "#Filler"; }
        const char *gettype() const { return typestr(); }
        bool isfill() const { return true; }

        void layout()
        {
            Object::layout();

            w = max(w, minw);
            h = max(h, minh);
        }

        float getcoord(int num, int axis)
        {
            if(num < 0 || num >= FC_MAX || axis < 0 || axis > 1) return 0.f;
            if(coords[num][axis] < 0)
            {
                float len = axis ? h : w;
                if(len == 0) return 0.f;
                float ret = clamp((0-coords[num][axis])/len, 0.f, 1.f);
                coords[num][axis] = (defcoords[num][axis] == 1 ? 1.f-ret : ret);
            }
            return coords[num][axis];
        }
    };

    ICOMMAND(0, uifill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw*uiscale, *minh*uiscale), children));

    UIARGSCALEDT(Filler, fill, minw, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Filler, fill, minh, "f", float, 0.f, FVAR_MAX);

    UICMDT(Filler, fill, coord, "iff", (int *pos, float *x, float *y),
    {
        if(*pos < 0 || *pos >= Filler::FC_MAX) return;
        o->coords[*pos][0] = min(*x, 1.f);
        o->coords[*pos][1] = min(*y, 1.f);
    });

    struct Target : Filler
    {
        void setup(float minw_, float minh_, const Color &color_ = Color(colourwhite), int type_ = -1, int dir_ = -1)
        {
            Filler::setup(minw_, minh_, color_, type_, dir_);
        }

        static const char *typestr() { return "#Target"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }
    };

    ICOMMAND(0, uitarget, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Target, o, o->setup(*minw*uiscale, *minh*uiscale), children));

    struct FillColor : Target
    {
        void setup(const Color &color_, float minw_ = 0, float minh_ = 0, int type_ = -1, int dir_ = -1)
        {
            Target::setup(minw_, minh_, color_, type_, dir_);
        }

        static const char *typestr() { return "#FillColor"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
            Color::def();
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            int cols = colors.length();
            gle::begin(GL_TRIANGLE_STRIP);
            if(cols >= 2)
            {
                float vr = 1/float(cols-1), vcx1 = 0, vcx2 = 0, vcy1 = 0, vcy2 = 0,
                      vw1 = w*(getcoord(FC_TR, 0)-getcoord(FC_TL, 0)), vx1 = w*getcoord(FC_TL, 0),
                      vw2 = w*(getcoord(FC_BR, 0)-getcoord(FC_BL, 0)), vx2 = w*getcoord(FC_BL, 0),
                      vdw1 = w*(getcoord(FC_BL, 0)-getcoord(FC_TL, 0)), vdw2 = w*(getcoord(FC_BR, 0)-getcoord(FC_TR, 0)),
                      vh1 = h*(getcoord(FC_BL, 1)-getcoord(FC_TL, 1)), vy1 = h*getcoord(FC_TL, 1),
                      vh2 = h*(getcoord(FC_BR, 1)-getcoord(FC_TR, 1)), vy2 = h*getcoord(FC_TR, 1),
                      vdh1 = h*(getcoord(FC_TR, 1)-getcoord(FC_TL, 1)), vdh2 = h*(getcoord(FC_BR, 1)-getcoord(FC_BL, 1));
                loopi(cols-1)
                {
                    switch(dir)
                    {
                        case HORIZONTAL:
                        {
                            gle::attribf(sx+vx1+vcx1, sy+vy1+vcy1); colors[i].attrib(); // 0
                            gle::attribf(sx+vx1+vcx1+(vw1*vr), sy+vy1+vcy1+(vdh1*vr)); colors[i+1].attrib(); // 1
                            gle::attribf(sx+vx2+vcx2, sy+vy1+vcy2+vh1); colors[i].attrib(); // 3
                            gle::attribf(sx+vx2+vcx2+(vw2*vr), sy+vy1+vcy2+vh1+(vdh2*vr)); colors[i+1].attrib(); // 2
                            vcx1 += vw1*vr;
                            vcx2 += vw2*vr;
                            vcy1 += vdh1*vr;
                            vcy2 += vdh2*vr;
                            break;
                        }
                        case VERTICAL:
                        {
                            gle::attribf(sx+vx1+vcx2+vw1, sy+vy2+vcy2); colors[i].attrib(); // 1
                            gle::attribf(sx+vx1+vcx2+vw1+(vdw2*vr), sy+vy2+vcy2+(vh2*vr)); colors[i+1].attrib(); // 2
                            gle::attribf(sx+vx1+vcx1, sy+vy1+vcy1); colors[i].attrib(); // 0
                            gle::attribf(sx+vx1+vcx1+(vdw1*vr), sy+vy1+vcy1+(vh1*vr)); colors[i+1].attrib(); // 3
                            vcy1 += vh1*vr;
                            vcy2 += vh2*vr;
                            vcx1 += vdw1*vr;
                            vcx2 += vdw2*vr;
                            break;
                        }
                    }
                }
            }
            else
            {
                gle::attribf(sx+(w*getcoord(FC_TR, 0)), sy+(h*getcoord(FC_TR, 1))); colors[0].attrib(); // 1
                gle::attribf(sx+(w*getcoord(FC_TL, 0)), sy+(h*getcoord(FC_TL, 1))); colors[0].attrib(); // 0
                gle::attribf(sx+(w*getcoord(FC_BR, 0)), sy+(h*getcoord(FC_BR, 1))); colors[0].attrib(); // 2
                gle::attribf(sx+(w*getcoord(FC_BL, 0)), sy+(h*getcoord(FC_BL, 1))); colors[0].attrib(); // 3
            }
            gle::end();

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uicolour, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uimodcolour, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, Colored::MODULATE), children));

    struct Gradient : FillColor
    {
        void setup(const Color &color_, const Color &color2_, float minw_ = 0, float minh_ = 0, int type_ = -1, int dir_ = -1)
        {
            FillColor::setup(color_, minw_, minh_, type_, dir_);
            colors.add(color2_);
        }

        static const char *typestr() { return "#Gradient"; }
        const char *gettype() const { return typestr(); }
    };

    ICOMMAND(0, uivgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uimodvgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::MODULATE), children));

    ICOMMAND(0, uihgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, -1, Gradient::HORIZONTAL), children));

    ICOMMAND(0, uimodhgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::MODULATE, Gradient::HORIZONTAL), children));

    struct Line : Target
    {
        float width;

        void setup(float minw_ = 0, float minh_ = 0, const Color &color_ = Color(colourwhite), float width_ = 1)
        {
            Target::setup(minw_, minh_, color_);
            width = width_;
        }

        static const char *typestr() { return "#Line"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            if(width != 1) glLineWidth(width);
            colors[0].init();
            gle::begin(GL_LINES);
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy+h);
            gle::end();
            if(width != 1) glLineWidth(1);

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiline, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(Line, o, o->setup(*minw*uiscale, *minh*uiscale, Color(*c)), children));

    UIARG(Line, line, width, "f", float, FVAR_NONZERO, FVAR_MAX);

    struct Outline : Target
    {
        float width;

        void setup(float minw_ = 0, float minh_ = 0, const Color &color_ = Color(colourwhite), float width_ = 1)
        {
            Target::setup(minw_, minh_, color_);
            width = width_;
        }

        static const char *typestr() { return "#Outline"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            if(width != 1) glLineWidth(width);
            colors[0].init();
            gle::begin(GL_LINE_LOOP);
            gle::attribf(sx+(w*getcoord(FC_TL, 0)), sy+(h*getcoord(FC_TL, 1))); // 0
            gle::attribf(sx+(w*getcoord(FC_TR, 0)), sy+(h*getcoord(FC_TR, 1))); // 1
            gle::attribf(sx+(w*getcoord(FC_BR, 0)), sy+(h*getcoord(FC_BR, 1))); // 2
            gle::attribf(sx+(w*getcoord(FC_BL, 0)), sy+(h*getcoord(FC_BL, 1))); // 3
            gle::end();
            if(width != 1) glLineWidth(1);

            Object::draw(sx, sy);
        }
    };
    UIARG(Outline, outline, width, "f", float, FVAR_NONZERO, FVAR_MAX);

    ICOMMAND(0, uioutline, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(Outline, o, o->setup(*minw*uiscale, *minh*uiscale, Color(*c)), children));

    static inline bool checkalphamask(Texture *tex, float x, float y)
    {
        if(!tex->alphamask)
        {
            loadalphamask(tex);
            if(!tex->alphamask) return true;
        }
        int tx = clamp(int(x*tex->xs), 0, tex->xs-1),
            ty = clamp(int(y*tex->ys), 0, tex->ys-1);
        if(tex->alphamask[ty*((tex->xs+7)/8) + tx/8] & (1<<(tx%8))) return true;
        return false;
    }

    struct Render : Target
    {
        Shader *shdr;
        struct param
        {
            char *name;
            vec4 value;

            param() : name(NULL), value(0, 0, 0, 0) {}
            ~param() { DELETEA(name); }
        };

        vector<param> params;
        vector<Texture *> texs;

        void setup(char *name_, float minw_ = 0, float minh_ = 0, const Color &color_ = Color(colourwhite))
        {
            Target::setup(minw_, minh_, color_);
            shdr = lookupshaderbyname(name_);
            if(!shdr) conoutf(colourred, "Cannot locate UI render shader: %s", name_);
            params.setsize(0);
            texs.setsize(0);
        }

        static const char *typestr() { return "#Render"; }
        const char *gettype() const { return typestr(); }
        bool isrender() const { return true; }

        void startdraw()
        {
            if(shdr) shdr->set();
            else
            {
                hudshader->set();
                params.setsize(0);
                texs.setsize(0);
                texs.add(notexture);
            }
            gle::defvertex(2);
            gle::deftexcoord0();
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            LOCALPARAMF(curmillis, uilastmillis / 1000.0f, uitotalmillis / 1000.0f, lastmillis / 1000.0f, totalmillis / 1000.0f);
            LOCALPARAMF(curticks, uiclockticks / 1000.0f, uiclockmillis / 1000.0f, getclockticks() / 1000.0f, getclockmillis() / 1000.0f);
            LOCALPARAMF(viewsize, hudw * w, hudh * h, 1.0f / (hudw * w), 1.0f / (hudh * h));
            
            LOCALPARAMF(rendersize, sx, sy, w, h);

            float waspect = w > h ? w / h : 1.0f, haspect = h > w ? h / w : 1.0f;
            LOCALPARAMF(renderaspect, waspect, haspect, 1.0f / waspect, 1.0f / haspect);

            vector<LocalShaderParam> list;
            loopv(params)
            {
                LocalShaderParam param = list.add(LocalShaderParam(params[i].name));
                param.set(params[i].value); // automatically converts to correct type
            }
            loopv(colors)
            {
                if(!i) continue; // becomes vcolor
                defformatstring(texparam, "objcolor%d", i);
                LocalShaderParam param = list.add(LocalShaderParam(texparam));
                param.set(colors[i].val.tocolor4()); // automatically converts to correct type
            }
            loopv(texs)
            {
                glActiveTexture_(GL_TEXTURE0 + i);
                settexture(texs[i]);
                defformatstring(texparam, "texsize%d", i);
                LocalShaderParam param = list.add(LocalShaderParam(texparam));
                param.setf(texs[i]->w, texs[i]->h, texs[i]->xs, texs[i]->ys);
            }
            glActiveTexture_(GL_TEXTURE0);

            colors[0].init();
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(sx+(w*getcoord(FC_TL, 0)), sy+(h*getcoord(FC_TL, 1))); gle::attribf(0, 0);
            gle::attribf(sx+(w*getcoord(FC_TR, 0)), sy+(h*getcoord(FC_TR, 1))); gle::attribf(1, 0);
            gle::attribf(sx+(w*getcoord(FC_BL, 0)), sy+(h*getcoord(FC_BL, 1))); gle::attribf(0, 1);
            gle::attribf(sx+(w*getcoord(FC_BR, 0)), sy+(h*getcoord(FC_BR, 1))); gle::attribf(1, 1);
            gle::end();

            Object::draw(sx, sy);
            stopdrawing();
        }
    };

    ICOMMAND(0, uirender, "sffe", (char *name, float *minw, float *minh, uint *children), \
        BUILD(Render, o, o->setup(name, *minw*uiscale, *minh*uiscale), children));

    UICMD(Render, render, param, "sffff", (char *name, float *x, float *y, float *z, float *w),
    {
        if(!name || !*name) return;
        Render::param *p = NULL;
        loopv(o->params) if(!strcmp(o->params[i].name, name))
        {
            p = &o->params[i];
            break;
        }
        if(!p)
        {
            p = &o->params.add();
            p->name = newstring(name);
        }
        p->value = vec4(*x, *y, *z, *w);
    });

    UICMD(Render, render, pmcol, "sig", (char *name, int *c, float *a),
    {
        if(!name || !*name) return;
        Render::param *p = NULL;
        loopv(o->params) if(!strcmp(o->params[i].name, name))
        {
            p = &o->params[i];
            break;
        }
        if(!p)
        {
            p = &o->params.add();
            p->name = newstring(name);
        }
        p->value = vec4::fromcolor(*c, *a >= 0.0f ? *a : 1.0f);
    });

    UICMD(Render, render, tex, "sbbb", (char *name, int *tclamp, int *mipit, int *tgc),
    {
        if(!name || !*name || o->texs.length() >= 10) return;
        o->texs.add(textureload(name, *tclamp >= 0 ? *tclamp : 3, *mipit != 0, false, *tgc >= 0 ? *tgc != 0 : texgc));
    });

    VAR(IDF_PERSIST, viewportsize, 0, 256, VAR_MAX); // limit size to this much
    VAR(IDF_PERSIST, viewportuprate, 0, 50, VAR_MAX); // limit updates to this ms
    VAR(IDF_PERSIST, viewportlimit, 0, 1, VAR_MAX); // limit updates to this count per cycle

    struct ViewPortEntry
    {
        char *refname = NULL;
        ViewSurface surf = ViewSurface(DRAWTEX_SCENE);
        int lastupdate = 0, lastrender = 0, width = 0, height = 0, uprate = 0;
        bool ready = false;

        ViewPortEntry(const char *n) : refname(newstring(n)) {}
        ~ViewPortEntry() { surf.destroy(); DELETEA(refname); }
    };

    vector<ViewPortEntry *> viewports;

    ViewPortEntry *findviewport(const char *refname)
    {
        loopv(viewports) if(!strcmp(viewports[i]->refname, refname)) return viewports[i];
        return NULL;
    }

    ViewPortEntry *getviewport(const char *refname)
    {
        ViewPortEntry *vp = findviewport(refname);
        if(vp) return vp;
        vp = new ViewPortEntry(refname);
        viewports.add(vp);
        return vp;
    }

    static inline bool vpsort(ViewPortEntry *a, ViewPortEntry *b)
    {
        if(a->lastrender < b->lastrender) return true;
        if(a->lastrender > b->lastrender) return false;
        return false;
    }

    int processviewports()
    {
        if(viewports.empty()) return 0;

        if(viewportlimit) viewports.sort(vpsort);

        static int lastframe = 0;
        int rendered = 0, processed = 0;

        loopv(viewports)
        {
            ViewPortEntry *vp = viewports[i];

            if(lastframe && vp->lastupdate < lastframe)
            {
                viewports.removeobj(vp);
                delete vp;
                i--;
                continue;
            }

            if(vp->ready)
            {
                if(!vp->uprate)
                {   // zero uprate signals not to render more than once
                    vp->lastrender = uiclockticks;
                    continue;
                }
                if(uiclockticks - vp->lastrender < max(vp->uprate, viewportuprate)) continue;
            }

            vp->ready = vp->surf.render(min(vp->width, viewportsize), min(vp->height, viewportsize));
            processed++;

            if(vp->ready)
            {
                vp->lastrender = uiclockticks;
                if(++rendered >= viewportlimit) break;
            }
        }

        lastframe = uiclockticks;

        return processed;
    }

    struct ViewPort : Target
    {
        char *refname = NULL;
        int uprate = 0, width = 0, height = 0;
        vec worldpos = vec(0, 0, 0);
        float yaw = 0.0f, pitch = 0.0f, roll = 0.0f, fov = 90.0f, ratio = 0.0f, nearpoint = 0.54f, farscale = 1.0f;
        ViewPortEntry *vp = NULL;

        ViewPort() : refname(NULL) {}
        ~ViewPort() { DELETEA(refname); }

        void setup(const char *_refname, float _x, float _y, float _z, float _yaw, float _pitch, float minw_ = 0, float minh_ = 0, const Color &color_ = Color(colourwhite))
        {
            Target::setup(minw_, minh_, color_);
            SETSTR(refname, _refname);
            worldpos = vec(_x, _y, _z);
            yaw = _yaw;
            pitch = _pitch;
            vp = NULL;
        }

        static const char *typestr() { return "#ViewPort"; }
        const char *gettype() const { return typestr(); }
        bool isviewport() const { return true; }

        void init()
        {
            if(!refname || !*refname)
            {
                vp = NULL;
                return;
            }

            vp = getviewport(refname);
            if(!vp) return;

            vp->uprate = uprate;
            vp->width = width > 0 ? width : viewportsize;
            vp->height = height > 0 ? height : viewportsize;
            vp->lastupdate = uiclockticks;
            vp->surf.worldpos = worldpos;
            vp->surf.yaw = yaw;
            vp->surf.pitch = pitch;
            vp->surf.roll = roll;
            vp->surf.fov = fov;
            vp->surf.ratio = ratio;
            vp->surf.nearpoint = nearpoint;
            vp->surf.farscale = farscale;
        }

        void startdraw()
        {
            (!vp || !vp->ready ? hudshader : hudrectshader)->set();
            gle::defvertex(2);
            gle::deftexcoord0();
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            colors[0].init();
            if(!vp || !vp->ready)
            {
                settexture(notexture);

                gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(sx+(w*getcoord(FC_TL, 0)), sy+(h*getcoord(FC_TL, 1))); gle::attribf(0, 0);
                gle::attribf(sx+(w*getcoord(FC_TR, 0)), sy+(h*getcoord(FC_TR, 1))); gle::attribf(1, 0);
                gle::attribf(sx+(w*getcoord(FC_BL, 0)), sy+(h*getcoord(FC_BL, 1))); gle::attribf(0, 1);
                gle::attribf(sx+(w*getcoord(FC_BR, 0)), sy+(h*getcoord(FC_BR, 1))); gle::attribf(1, 1);
                gle::end();
            }
            else
            {
                vp->surf.bindtex();

                gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(sx+(w*getcoord(FC_TL, 0)), sy+(h*getcoord(FC_TL, 1))); gle::attribf(0, vp->surf.buffers[0]->height);
                gle::attribf(sx+(w*getcoord(FC_TR, 0)), sy+(h*getcoord(FC_TR, 1))); gle::attribf(vp->surf.buffers[0]->width, vp->surf.buffers[0]->height);
                gle::attribf(sx+(w*getcoord(FC_BL, 0)), sy+(h*getcoord(FC_BL, 1))); gle::attribf(0, 0);
                gle::attribf(sx+(w*getcoord(FC_BR, 0)), sy+(h*getcoord(FC_BR, 1))); gle::attribf(vp->surf.buffers[0]->width, 0);
                gle::end();
            }

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiviewport, "sfffffffe", (char *s, float *x, float *y, float *z, float *yaw, float *pitch, float *minw, float *minh, uint *children), \
        BUILD(ViewPort, o, o->setup(s, *x, *y, *z, *yaw, *pitch, *minw*uiscale, *minh*uiscale), children));

    UICMD(ViewPort, viewport, pos, "fff", (float *x, float *y, float *z), o->worldpos = vec(*x, *y, *z));
    UIARGT(ViewPort, viewport, yaw, "f", float, 0.0f, 360.0f);
    UIARGT(ViewPort, viewport, pitch, "f", float, -89.9f, 89.9f);
    UIARGT(ViewPort, viewport, roll, "f", float, -180.0f, 180.0f);
    UIARGT(ViewPort, viewport, fov, "f", float, 10.0f, 180.0f);
    UIARGT(ViewPort, viewport, ratio, "f", float, FVAR_NONZERO, FVAR_MAX);
    UIARGT(ViewPort, viewport, nearpoint, "f", float, 0.0f, 1.0f);
    UIARGT(ViewPort, viewport, farscale, "f", float, FVAR_NONZERO, FVAR_MAX);
    UIARGT(ViewPort, viewport, uprate, "i", int, 0, VAR_MAX);
    UIARGT(ViewPort, viewport, width, "i", int, 2, VAR_MAX);
    UIARGT(ViewPort, viewport, height, "i", int, 2, VAR_MAX);

    struct Image : Target
    {
        static Texture *lasttex;
        static Color lastcolor;
        static GLenum lastmode;

        enum { CO_TL = 0, CO_TR, CO_TC, CO_BL, CO_BR, CO_BC, CO_ML, CO_MR, CO_MC, CO_MAX };

        Texture *tex;
        bool alphatarget, outline, aspect;
        int shadowtype;
        float shadowsize;
        Color shadowcolor;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, bool outline_ = false, float shadowsize_ = 0.f, bool aspect_ = false)
        {
            Target::setup(minw_, minh_, color_);
            tex = tex_;
            alphatarget = alphatarget_;
            outline = outline_;
            shadowsize = shadowsize_;
            shadowcolor = Color(0, 0, 0, 255);
            shadowtype = 0;
            aspect = aspect_;
        }

        void setup(Texture *tex_, const Color &color_, const Color &color2_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, int dir_ = -1, bool outline_ = false, float shadowsize_ = 0.f, bool aspect_ = false)
        {
            Target::setup(minw_, minh_, color_, -1, dir_);
            colors.add(color2_); // gradient version
            tex = tex_;
            alphatarget = alphatarget_;
            outline = outline_;
            shadowsize = shadowsize_;
            shadowcolor = Color(0, 0, 0, 255);
            shadowtype = 0;
            aspect = aspect_;
        }

        static const char *typestr() { return "#Image"; }
        const char *gettype() const { return typestr(); }
        bool isimage() const { return true; }

        void layout()
        {
            Object::layout();

            float mw = minw, mh = minh;
            if(aspect && tex && (mw > 0 || mh > 0))
            {
                if(mw <= 0) mw = mh * (tex->w / float(tex->h));
                else if(mh <= 0) mh = mw * (tex->h / float(tex->w));
            }

            w = max(w, mw);
            h = max(h, mh);
        }

        bool target(float cx, float cy)
        {
            return !alphatarget || !(tex->type&Texture::ALPHA) || checkalphamask(tex, cx/w, cy/h);
        }

        void startdraw()
        {
            lasttex = NULL;
            lastcolor = Color(0, 0, 0, 0);
            lastmode = GL_POINTS; // force mode change in bindtex
        }

        void enddraw()
        {
            gle::end();
        }

        void bindtex(GLenum mode = GL_QUADS, int colstart = 0, bool forced = false, bool oline = false)
        {
            int flags = 0;
            if(oline)
            {
                flags = CHANGE_SHADER;
                hudoutlineshader->set();
                LOCALPARAMF(textparams, 0.15f, 0.35f, 0.35f, 0.55f);
            }

            setupdraw(flags);

            int col = clamp(colstart, -1, colors.length()-1);
            Color c = colors.inrange(col) ? colors[col] : colors[0];
            if(col < 0) switch(shadowtype)
            {
                case 1: c.combine(shadowcolor); break;
                case 2: c.scale(shadowcolor); break;
                case 0: default: c = shadowcolor; break;
            }
            if(forced || lastmode != mode)
            {
                if(lastmode != GL_POINTS) gle::end();
                gle::defvertex(2);
                gle::deftexcoord0();
                if(mode == GL_TRIANGLE_STRIP) Color::def(); // gradient
                gle::begin(mode);
                lastmode = mode;
                goto changetex;
            }
            if(lasttex != tex)
            {
                gle::end();
            changetex:
                lasttex = tex;
                settexture(tex);
                goto changecolor;
            }
            if(lastcolor != c)
            {
                gle::end();
            changecolor:
                switch(mode)
                {
                    case GL_TRIANGLE_STRIP:
                        lastcolor = Color(0, 0, 0, 0);
                    case GL_QUADS:
                        lastcolor = c;
                        c.init();
                        break;
                }
            }
        }

        bool drawmapped(float sx, float sy, vec2 coordmap[FC_MAX], vec2 tcoordmap[FC_MAX], int colstart = 0, int colcount = 0, bool forced = false, bool shading = false)
        {
            int cols = clamp(colcount ? colcount : colors.length()-colstart, 0, colors.length());
            if(!shading && cols >= 2)
            {
                bindtex(GL_TRIANGLE_STRIP, colstart, forced, !shading && outline);
                float vr = 1/float(cols-1), vcx1 = 0, vcx2 = 0, vcy1 = 0, vcy2 = 0,
                    vw1 = coordmap[FC_TR][0]-coordmap[FC_TL][0], vx1 = coordmap[FC_TL][0],
                    vw2 = coordmap[FC_BR][0]-coordmap[FC_BL][0], vx2 = coordmap[FC_BL][0],
                    vdw1 = coordmap[FC_BL][0]-coordmap[FC_TL][0], vdw2 = coordmap[FC_BR][0]-coordmap[FC_TR][0],
                    vh1 = coordmap[FC_BL][1]-coordmap[FC_TL][1], vy1 = coordmap[FC_TL][1],
                    vh2 = coordmap[FC_BR][1]-coordmap[FC_TR][1], vy2 = coordmap[FC_TR][1],
                    vdh1 = coordmap[FC_TR][1]-coordmap[FC_TL][1], vdh2 = coordmap[FC_BR][1]-coordmap[FC_BL][1],
                    tcx1 = 0, tcx2 = 0, tcy1 = 0, tcy2 = 0,
                    tw1 = tcoordmap[FC_TR][0]-tcoordmap[FC_TL][0], tx1 = tcoordmap[FC_TL][0],
                    tw2 = tcoordmap[FC_BR][0]-tcoordmap[FC_BL][0], tx2 = tcoordmap[FC_BL][0],
                    tdw1 = tcoordmap[FC_BL][0]-tcoordmap[FC_TL][0], tdw2 = tcoordmap[FC_BR][0]-tcoordmap[FC_TR][0],
                    th1 = tcoordmap[FC_BL][1]-tcoordmap[FC_TL][1], ty1 = tcoordmap[FC_TL][1],
                    th2 = tcoordmap[FC_BR][1]-tcoordmap[FC_TR][1], ty2 = tcoordmap[FC_TR][1],
                    tdh1 = tcoordmap[FC_TR][1]-tcoordmap[FC_TL][1], tdh2 = tcoordmap[FC_BR][1]-tcoordmap[FC_BL][1];
                loopi(cols-1)
                {
                    int color1 = i + colstart, color2 = (color1 + 1) % cols; // wrap around
                    switch(dir)
                    {
                        case HORIZONTAL:
                        {
                            gle::attribf(sx+vx1+vcx1, sy+vy1+vcy1); gle::attribf(tx1+tcx1, ty1+tcy1); colors[color1].attrib(); // 0
                            gle::attribf(sx+vx1+vcx1+(vw1*vr), sy+vy1+vcy1+(vdh1*vr)); gle::attribf(tx1+tcx1+(tw1*vr), ty1+tcy1+(tdh1*vr)); colors[color2].attrib(); // 1
                            gle::attribf(sx+vx2+vcx2, sy+vy1+vcy2+vh1); gle::attribf(tx2+tcx2, ty1+tcy2+th1); colors[color1].attrib(); // 3
                            gle::attribf(sx+vx2+vcx2+(vw2*vr), sy+vy1+vcy2+vh1+(vdh2*vr)); gle::attribf(tx2+tcx2+(tw2*vr), ty1+tcy2+th1+(tdh2*vr)); colors[color2].attrib(); // 2
                            vcx1 += vw1*vr;
                            vcx2 += vw2*vr;
                            vcy1 += vdh1*vr;
                            vcy2 += vdh2*vr;
                            tcx1 += tw1*vr;
                            tcx2 += tw2*vr;
                            tcy1 += tdh1*vr;
                            tcy2 += tdh2*vr;
                            break;
                        }
                        case VERTICAL:
                        {
                            gle::attribf(sx+vx1+vcx2+vw1, sy+vy2+vcy2); gle::attribf(tx1+tcx2+tw1, ty2+tcy2); colors[color1].attrib(); // 1
                            gle::attribf(sx+vx1+vcx2+vw1+(vdw2*vr), sy+vy2+vcy2+(vh2*vr)); gle::attribf(tx1+tcx2+tw1+(tdw2*vr), ty2+tcy2+(th2*vr)); colors[color2].attrib(); // 2
                            gle::attribf(sx+vx1+vcx1, sy+vy1+vcy1); gle::attribf(tx1+tcx1, ty1+tcy1); colors[color1].attrib(); // 0
                            gle::attribf(sx+vx1+vcx1+(vdw1*vr), sy+vy1+vcy1+(vh1*vr)); gle::attribf(tx1+tcx1+(tdw1*vr), ty1+tcy1+(th1*vr)); colors[color2].attrib(); // 3
                            vcy1 += vh1*vr;
                            vcy2 += vh2*vr;
                            vcx1 += vdw1*vr;
                            vcx2 += vdw2*vr;
                            tcy1 += th1*vr;
                            tcy2 += th2*vr;
                            tcx1 += tdw1*vr;
                            tcx2 += tdw2*vr;
                            break;
                        }
                    }
                    lastcolor = colors[color2];
                }
            }
            else
            {
                bindtex(GL_QUADS, colstart, forced, !shading && outline);
                gle::attribf(sx+coordmap[FC_TL][0], sy+coordmap[FC_TL][1]); gle::attribf(tcoordmap[FC_TL][0], tcoordmap[FC_TL][1]); // 0
                gle::attribf(sx+coordmap[FC_TR][0], sy+coordmap[FC_TR][1]); gle::attribf(tcoordmap[FC_TR][0], tcoordmap[FC_TR][1]); // 1
                gle::attribf(sx+coordmap[FC_BR][0], sy+coordmap[FC_BR][1]); gle::attribf(tcoordmap[FC_BR][0], tcoordmap[FC_BR][1]); // 2
                gle::attribf(sx+coordmap[FC_BL][0], sy+coordmap[FC_BL][1]); gle::attribf(tcoordmap[FC_BL][0], tcoordmap[FC_BL][1]); // 3
            }
            return false;
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            float gs = fabs(shadowsize), gw = max(w-(shadowsize != 0 ? float(gs) : 0.f), 0.f), gh = max(h-(shadowsize != 0 ? float(gs) : 0.f), 0.f);
            loopk(shadowsize != 0 ? 2 : 1)
            {
                bool shading = shadowsize != 0 && !k;
                float gx = sx, gy = sy;
                if((shadowsize > 0 && !k) || (shadowsize < 0 && k))
                {
                    gx += gs;
                    gy += gs;
                }
                vec2 coordmap[FC_MAX], tcoordmap[FC_MAX];
                loopi(FC_MAX) loopj(2)
                {
                    coordmap[i][j] = getcoord(i, j)*(j ? gh : gw);
                    tcoordmap[i][j] = defcoords[i][j];
                }
                drawmapped(gx, gy, coordmap, tcoordmap, shading ? -1 : 0, 0, false, shading);
            }

            Object::draw(sx, sy);
        }
    };

    Texture *Image::lasttex = NULL;
    Color Image::lastcolor(255, 255, 255);
    GLenum Image::lastmode = GL_POINTS; // something we don't use

    #define UIIMGCMDS(name, value) \
        ICOMMAND(0, uiimage##name, "siiffe", (char *texname, int *c, int *a, float *minw, float *minh, uint *children), \
            BUILD(Image, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), *a!=0, *minw*uiscale, *minh*uiscale), children)); \
        ICOMMAND(0, uiimagevgradient##name, "siiiffe", (char *texname, int *c, int *c2, int *a, float *minw, float *minh, uint *children), \
            BUILD(Image, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), Color(*c2), *a!=0, *minw*uiscale, *minh*uiscale), children)); \
        ICOMMAND(0, uiimagehgradient##name, "siiiffe", (char *texname, int *c, int *c2, int *a, float *minw, float *minh, uint *children), \
            BUILD(Image, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), Color(*c2), *a!=0, *minw*uiscale, *minh*uiscale, Image::HORIZONTAL), children)); \
        UICMDT(Image, image, tex##name, "s", (char *texname), if(texname && *texname) o->tex = textureload(texname, value, true, false, texgc)); \
        UICMDT(Image, image, alttex##name, "s", (char *texname), if(texname && *texname && o->tex == notexture) o->tex = textureload(texname, value, true, false, texgc));

    UIIMGCMDS(, 3);
    UIIMGCMDS(clamped, 0x7000);
    UIARGTB(Image, image, alphatarget);
    UIARGTB(Image, image, outline);
    UIARGTB(Image, image, aspect);
    UICMDT(Image, image, shadow, "fii", (float *s, int *c, int *t),
    {
        o->shadowsize = clamp(*s, FVAR_MIN, FVAR_MAX);
        o->shadowcolor = Color(*c);
        o->shadowtype = *t;
    });
    UIARGT(Image, image, shadowsize, "f", float, FVAR_MIN, FVAR_MAX);
    UICMDT(Image, image, shadowcolour, "i", (int *c), o->shadowcolor = Color(*c));
    UICMDT(Image, image, shadowtype, "i", (int *t), o->shadowtype = *t);

    struct CroppedImage : Image
    {
        float cropx, cropy, cropw, croph;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, float cropx_ = 0, float cropy_ = 0, float cropw_ = 1, float croph_ = 1, bool outline_ = false, float shadowsize_ = 0.f)
        {
            Image::setup(tex_, color_, alphatarget_, minw_, minh_, outline_, shadowsize_);
            cropx = cropx_;
            cropy = cropy_;
            cropw = cropw_;
            croph = croph_;
        }

        static const char *typestr() { return "#CroppedImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return !alphatarget || !(tex->type&Texture::ALPHA) || checkalphamask(tex, cropx + cx/w*cropw, cropy + cy/h*croph);
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            float gs = fabs(shadowsize), gw = max(w-(shadowsize != 0 ? float(gs) : 0.f), 0.f), gh = max(h-(shadowsize != 0 ? float(gs) : 0.f), 0.f);
            loopk(shadowsize != 0 ? 2 : 1)
            {
                bool shading = shadowsize != 0 && !k;
                float gx = sx, gy = sy;
                if((shadowsize > 0 && !k) || (shadowsize < 0 && k))
                {
                    gx += gs;
                    gy += gs;
                }
                float texmap[FC_MAX][2] = { { cropx, cropy }, { cropx+cropw, cropy }, { cropx+cropw, cropy+croph }, { cropx, cropy+croph } };
                vec2 coordmap[FC_MAX], tcoordmap[FC_MAX];
                loopi(FC_MAX) loopj(2)
                {
                    coordmap[i][j] = getcoord(i, j)*(j ? gh : gw);
                    tcoordmap[i][j] = texmap[i][j];
                }
                drawmapped(gx, gy, coordmap, tcoordmap, shading ? -1 : 0, 0, false, shading);
            }

            Object::draw(sx, sy);
        }
    };

    static inline float parsepixeloffset(const tagval *t, int size)
    {
        switch(t->type)
        {
            case VAL_INT: return t->i;
            case VAL_FLOAT: return t->f;
            case VAL_NULL: return 0;
            default:
            {
                const char *s = t->getstr();
                char *end;
                float val = strtod(s, &end);
                return *end == 'p' ? val/size : val;
            }
        }
    }

    #define UICIMGCMDS(name, value) \
        ICOMMAND(0, uicroppedimage##name, "siifftttte", (char *texname, int *c, int *a, float *minw, float *minh, tagval *cropx, tagval *cropy, tagval *cropw, tagval *croph, uint *children), \
            BUILD(CroppedImage, o, { \
                Texture *tex = textureload(texname, value, true, false, texgc); \
                o->setup(tex, Color(*c), *a!=0, *minw*uiscale, *minh*uiscale, \
                    parsepixeloffset(cropx, tex->xs), parsepixeloffset(cropy, tex->ys), \
                    parsepixeloffset(cropw, tex->xs), parsepixeloffset(croph, tex->ys)); \
            }, children));

    UICIMGCMDS(, 3);
    UICIMGCMDS(clamped, 0x7000);
    UIARG(CroppedImage, image, cropx, "f", float, 0.f, 1.f);
    UIARG(CroppedImage, image, cropy, "f", float, 0.f, 1.f);
    UIARG(CroppedImage, image, cropw, "f", float, 0.f, 1.f);
    UIARG(CroppedImage, image, croph, "f", float, 0.f, 1.f);

    struct StretchedImage : Image
    {
        static const char *typestr() { return "#StretchedImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(!alphatarget || !(tex->type&Texture::ALPHA)) return true;

            float mx, my;
            if(w <= minw) mx = cx/w;
            else if(cx < minw/2) mx = cx/minw;
            else if(cx >= w - minw/2) mx = 1 - (w - cx) / minw;
            else mx = 0.5f;
            if(h <= minh) my = cy/h;
            else if(cy < minh/2) my = cy/minh;
            else if(cy >= h - minh/2) my = 1 - (h - cy) / minh;
            else my = 0.5f;

            return checkalphamask(tex, mx, my);
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            float gs = fabs(shadowsize), gw = max(w-(shadowsize != 0 ? float(gs) : 0.f), 0.f), gh = max(h-(shadowsize != 0 ? float(gs) : 0.f), 0.f);
            loopk(shadowsize != 0 ? 2 : 1)
            {
                bool shading = shadowsize != 0 && !k;
                float gx = sx, gy = sy;
                if((shadowsize > 0 && !k) || (shadowsize < 0 && k))
                {
                    gx += gs;
                    gy += gs;
                }
                bindtex(GL_QUADS, shading ? -1 : 0, false, !shading && outline);

                float splitw = (minw ? min(minw, gw) : gw) / 2,
                    splith = (minh ? min(minh, gh) : gh) / 2,
                    vy = gy, ty = 0;
                loopi(3)
                {
                    float vh = 0, th = 0;
                    switch(i)
                    {
                        case 0: if(splith < gh - splith) { vh = splith; th = 0.5f; } else { vh = gh; th = 1; } break;
                        case 1: vh = gh - 2*splith; th = 0; break;
                        case 2: vh = splith; th = 0.5f; break;
                    }
                    float vx = gx, tx = 0;
                    loopj(3)
                    {
                        float vw = 0, tw = 0;
                        switch(j)
                        {
                            case 0: if(splitw < gw - splitw) { vw = splitw; tw = 0.5f; } else { vw = gw; tw = 1; } break;
                            case 1: vw = gw - 2*splitw; tw = 0; break;
                            case 2: vw = splitw; tw = 0.5f; break;
                        }
                        quads(vx, vy, vw, vh, tx, ty, tw, th);
                        vx += vw;
                        tx += tw;
                        if(tx >= 1) break;
                    }
                    vy += vh;
                    ty += th;
                    if(ty >= 1) break;
                }
            }

            Object::draw(sx, sy);
        }
    };

    #define UISIMGCMDS(name, value) \
        ICOMMAND(0, uistretchedimage##name, "siiffe", (char *texname, int *c, int *a, float *minw, float *minh, uint *children), \
            BUILD(StretchedImage, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), *a!=0, *minw*uiscale, *minh*uiscale), children));

    UISIMGCMDS(, 3);
    UISIMGCMDS(clamped, 0x7000);

    struct BorderedImage : Image
    {
        float texborder, screenborder;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float texborder_ = 0, float screenborder_ = 0, float minw_ = 0, float minh_ = 0, bool outline_ = false, float shadowsize_ = 0.f)
        {
            Image::setup(tex_, color_, alphatarget_, minw_, minh_, outline_, shadowsize_);
            texborder = texborder_;
            screenborder = screenborder_;
        }

        static const char *typestr() { return "#BorderedImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(!alphatarget || !(tex->type&Texture::ALPHA)) return true;

            float mx, my;
            if(cx < screenborder) mx = cx/screenborder*texborder;
            else if(cx >= w - screenborder) mx = 1-texborder + (cx - (w - screenborder))/screenborder*texborder;
            else mx = texborder + (cx - screenborder)/(w - 2*screenborder)*(1 - 2*texborder);
            if(cy < screenborder) my = cy/screenborder*texborder;
            else if(cy >= h - screenborder) my = 1-texborder + (cy - (h - screenborder))/screenborder*texborder;
            else my = texborder + (cy - screenborder)/(h - 2*screenborder)*(1 - 2*texborder);

            return checkalphamask(tex, mx, my);
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            float gs = fabs(shadowsize), gw = max(w-(shadowsize != 0 ? float(gs) : 0.f), 0.f), gh = max(h-(shadowsize != 0 ? float(gs) : 0.f), 0.f);
            loopk(shadowsize != 0 ? 2 : 1)
            {
                bool shading = shadowsize != 0 && !k;
                float gx = sx, gy = sy;
                if((shadowsize > 0 && !k) || (shadowsize < 0 && k))
                {
                    gx += gs;
                    gy += gs;
                }
                vec2 outline[FC_MAX], projdir[2], coordmap[CO_MAX][FC_MAX], tcoordmap[CO_MAX][FC_MAX];
                loopi(FC_MAX) loopj(2) outline[i][j] = getcoord(i, j)*(j ? gh : gw);

                // top left
                projdir[0] = vec2(outline[1]).sub(outline[0]).normalize();
                projdir[1] = vec2(outline[3]).sub(outline[0]).normalize();
                coordmap[CO_TL][FC_TL] = outline[0];
                coordmap[CO_TL][FC_TR] = vec2(coordmap[CO_TL][FC_TL]).add(vec2(projdir[0]).mul(screenborder));
                coordmap[CO_TL][FC_BL] = vec2(coordmap[CO_TL][FC_TL]).add(vec2(projdir[1]).mul(screenborder));
                coordmap[CO_TL][FC_BR] = vec2(coordmap[CO_TL][FC_BL]).add(vec2(projdir[0]).mul(screenborder));
                tcoordmap[CO_TL][FC_TL] = vec2(0, 0);
                tcoordmap[CO_TL][FC_TR] = vec2(texborder, 0);
                tcoordmap[CO_TL][FC_BR] = vec2(texborder, texborder);
                tcoordmap[CO_TL][FC_BL] = vec2(0, texborder);

                // top right
                projdir[0] = vec2(outline[0]).sub(outline[1]).normalize();
                projdir[1] = vec2(outline[2]).sub(outline[1]).normalize();
                coordmap[CO_TR][FC_TR] = outline[1];
                coordmap[CO_TR][FC_TL] = vec2(coordmap[CO_TR][FC_TR]).add(vec2(projdir[0]).mul(screenborder));
                coordmap[CO_TR][FC_BR] = vec2(coordmap[CO_TR][FC_TR]).add(vec2(projdir[1]).mul(screenborder));
                coordmap[CO_TR][FC_BL] = vec2(coordmap[CO_TR][FC_BR]).add(vec2(projdir[0]).mul(screenborder));
                tcoordmap[CO_TR][FC_TL] = vec2(1-texborder, 0);
                tcoordmap[CO_TR][FC_TR] = vec2(1, 0);
                tcoordmap[CO_TR][FC_BR] = vec2(1, texborder);
                tcoordmap[CO_TR][FC_BL] = vec2(1-texborder, texborder);

                // top center
                coordmap[CO_TC][FC_TL] = coordmap[CO_TL][FC_TR];
                coordmap[CO_TC][FC_TR] = coordmap[CO_TR][FC_TL];
                coordmap[CO_TC][FC_BR] = coordmap[CO_TR][FC_BL];
                coordmap[CO_TC][FC_BL] = coordmap[CO_TL][FC_BR];
                tcoordmap[CO_TC][FC_TL] = vec2(texborder, 0);
                tcoordmap[CO_TC][FC_TR] = vec2(1-texborder, 0);
                tcoordmap[CO_TC][FC_BR] = vec2(1-texborder, texborder);
                tcoordmap[CO_TC][FC_BL] = vec2(texborder, texborder);

                // bottom left
                projdir[0] = vec2(outline[2]).sub(outline[3]).normalize();
                projdir[1] = vec2(outline[0]).sub(outline[3]).normalize();
                coordmap[CO_BL][FC_BL] = outline[3];
                coordmap[CO_BL][FC_TL] = vec2(coordmap[CO_BL][FC_BL]).add(vec2(projdir[1]).mul(screenborder));
                coordmap[CO_BL][FC_BR] = vec2(coordmap[CO_BL][FC_BL]).add(vec2(projdir[0]).mul(screenborder));
                coordmap[CO_BL][FC_TR] = vec2(coordmap[CO_BL][FC_BR]).add(vec2(projdir[1]).mul(screenborder));
                tcoordmap[CO_BL][FC_TL] = vec2(0, 1-texborder);
                tcoordmap[CO_BL][FC_TR] = vec2(texborder, 1-texborder);
                tcoordmap[CO_BL][FC_BR] = vec2(texborder, 1);
                tcoordmap[CO_BL][FC_BL] = vec2(0, 1);

                // bottom right
                projdir[0] = vec2(outline[3]).sub(outline[2]).normalize();
                projdir[1] = vec2(outline[1]).sub(outline[2]).normalize();
                coordmap[CO_BR][FC_BR] = outline[2];
                coordmap[CO_BR][FC_TR] = vec2(coordmap[CO_BR][FC_BR]).add(vec2(projdir[1]).mul(screenborder));
                coordmap[CO_BR][FC_BL] = vec2(coordmap[CO_BR][FC_BR]).add(vec2(projdir[0]).mul(screenborder));
                coordmap[CO_BR][FC_TL] = vec2(coordmap[CO_BR][FC_BL]).add(vec2(projdir[1]).mul(screenborder));
                tcoordmap[CO_BR][FC_TL] = vec2(1-texborder, 1-texborder);
                tcoordmap[CO_BR][FC_TR] = vec2(1, 1-texborder);
                tcoordmap[CO_BR][FC_BR] = vec2(1, 1);
                tcoordmap[CO_BR][FC_BL] = vec2(1-texborder, 1);

                // bottom center
                coordmap[CO_BC][FC_TL] = coordmap[CO_BL][FC_TR];
                coordmap[CO_BC][FC_TR] = coordmap[CO_BR][FC_TL];
                coordmap[CO_BC][FC_BR] = coordmap[CO_BR][FC_BL];
                coordmap[CO_BC][FC_BL] = coordmap[CO_BL][FC_BR];
                tcoordmap[CO_BC][FC_TL] = vec2(texborder, 1-texborder);
                tcoordmap[CO_BC][FC_TR] = vec2(1-texborder, 1-texborder);
                tcoordmap[CO_BC][FC_BR] = vec2(1-texborder, 1);
                tcoordmap[CO_BC][FC_BL] = vec2(texborder, 1);

                // middle left
                coordmap[CO_ML][FC_TL] = coordmap[CO_TL][FC_BL];
                coordmap[CO_ML][FC_TR] = coordmap[CO_TL][FC_BR];
                coordmap[CO_ML][FC_BR] = coordmap[CO_BL][FC_TR];
                coordmap[CO_ML][FC_BL] = coordmap[CO_BL][FC_TL];
                tcoordmap[CO_ML][FC_TL] = vec2(0, texborder);
                tcoordmap[CO_ML][FC_TR] = vec2(texborder, texborder);
                tcoordmap[CO_ML][FC_BR] = vec2(texborder, 1-texborder);
                tcoordmap[CO_ML][FC_BL] = vec2(0, 1-texborder);

                // middle right
                coordmap[CO_MR][FC_TL] = coordmap[CO_TR][FC_BL];
                coordmap[CO_MR][FC_TR] = coordmap[CO_TR][FC_BR];
                coordmap[CO_MR][FC_BR] = coordmap[CO_BR][FC_TR];
                coordmap[CO_MR][FC_BL] = coordmap[CO_BR][FC_TL];
                tcoordmap[CO_MR][FC_TL] = vec2(1-texborder, texborder);
                tcoordmap[CO_MR][FC_TR] = vec2(1, texborder);
                tcoordmap[CO_MR][FC_BR] = vec2(1, 1-texborder);
                tcoordmap[CO_MR][FC_BL] = vec2(1-texborder, 1-texborder);

                // middle center
                coordmap[CO_MC][FC_TL] = coordmap[CO_TL][FC_BR];
                coordmap[CO_MC][FC_TR] = coordmap[CO_TR][FC_BL];
                coordmap[CO_MC][FC_BR] = coordmap[CO_BR][FC_TL];
                coordmap[CO_MC][FC_BL] = coordmap[CO_BL][FC_TR];
                tcoordmap[CO_MC][FC_TL] = vec2(texborder, texborder);
                tcoordmap[CO_MC][FC_TR] = vec2(1-texborder, texborder);
                tcoordmap[CO_MC][FC_BR] = vec2(1-texborder, 1-texborder);
                tcoordmap[CO_MC][FC_BL] = vec2(1-texborder, texborder);

                if(!shading && colors.length() >= 2)
                {
                    static const int drawmap[2][CO_MAX] = {
                        { CO_TL, CO_ML, CO_BL, CO_TC, CO_MC, CO_BC, CO_TR, CO_MR, CO_BR }, // VERTICAL
                        { CO_TL, CO_TC, CO_TR, CO_ML, CO_MC, CO_MR, CO_BL, CO_BC, CO_BR }  // HORIZONTAL
                    };
                    loopi(3) loopj(3)
                    {
                        int index = (i*3)+j, target = drawmap[dir][index], colstart = 0, colcount = 0;
                        switch(j)
                        {
                            case 0: colstart = 0; colcount = 1; break;
                            case 1: colstart = 0; colcount = colors.length(); break;
                            case 2: colstart = colors.length()-1; colcount = 1; break;
                        }
                        drawmapped(gx, gy, coordmap[target], tcoordmap[target], colstart, colcount, j == 0);
                    }
                }
                else loopi(CO_MAX) drawmapped(gx, gy, coordmap[i], tcoordmap[i], shading ? -1 : 0, 0, false, shading);
            }
            Object::draw(sx, sy);
        }
    };

    #define UIBIMGCMDS(name, value) \
        ICOMMAND(0, uiborderedimage##name, "siitfffe", (char *texname, int *c, int *a, tagval *texborder, float *screenborder, float *minw, float *minh, uint *children), \
            BUILD(BorderedImage, o, { \
                Texture *tex = textureload(texname, value, true, false, texgc); \
                o->setup(tex, Color(*c), *a!=0, \
                    parsepixeloffset(texborder, tex->xs), \
                    *screenborder*uiscale, *minw*uiscale, *minh*uiscale); \
            }, children));

    UIBIMGCMDS(, 3);
    UIBIMGCMDS(clamped, 0x7000);
    UIARG(BorderedImage, image, texborder, "f", float, 0.f, 1.f);
    UIARGSCALED(BorderedImage, image, screenborder, "f", float, 0.f, FVAR_MAX);

    struct TiledImage : Image
    {
        float tilew, tileh;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, float tilew_ = 0, float tileh_ = 0, bool outline_ = false, float shadowsize_ = 0.f)
        {
            Image::setup(tex_, color_, alphatarget_, minw_, minh_, outline_, shadowsize_);
            tilew = tilew_;
            tileh = tileh_;
        }

        static const char *typestr() { return "#TiledImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(!alphatarget || !(tex->type&Texture::ALPHA)) return true;
            return checkalphamask(tex, fmod(cx/tilew, 1), fmod(cy/tileh, 1));
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            float gs = fabs(shadowsize), gw = max(w-(shadowsize != 0 ? float(gs) : 0.f), 0.f), gh = max(h-(shadowsize != 0 ? float(gs) : 0.f), 0.f);
            loopk(shadowsize != 0 ? 2 : 1)
            {
                bool shading = shadowsize != 0 && !k;
                float gx = sx, gy = sy;
                if((shadowsize > 0 && !k) || (shadowsize < 0 && k))
                {
                    gx += gs;
                    gy += gs;
                }
                bindtex(GL_QUADS, shading ? -1 : 0, false, !shading && outline);
                if(tex->tclamp)
                {
                    for(float dy = 0; dy < gh; dy += tileh)
                    {
                        float dh = min(tileh, gh - dy);
                        for(float dx = 0; dx < gw; dx += tilew)
                        {
                            float dw = min(tilew, gw - dx);
                            quads(gx + dx, gy + dy, dw, dh, 0, 0, dw / tilew, dh / tileh);
                        }
                    }
                }
                else quads(gx, gy, gw, gh, 0, 0, w/tilew, h/tileh);
            }

            Object::draw(sx, sy);
        }
    };

    #define UITIMGCMDS(name, value) \
        ICOMMAND(0, uitiledimage##name, "siiffffe", (char *texname, int *c, int *a, float *tilew, float *tileh, float *minw, float *minh, uint *children), \
            BUILD(TiledImage, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), *a!=0, *minw*uiscale, *minh*uiscale, *tilew <= 0 ? 1 : *tilew, *tileh <= 0 ? 1 : *tileh), children));

    UITIMGCMDS(, 3);
    UITIMGCMDS(clamped, 0x7000);
    UIARG(TiledImage, image, tilew, "f", float, FVAR_NONZERO, FVAR_MAX);
    UIARG(TiledImage, image, tileh, "f", float, FVAR_NONZERO, FVAR_MAX);

    struct Thumbnail : Target
    {
        static Color lastcolor;
        Texture *t;

        Thumbnail() : t(NULL) {}

        void setup(Texture *_t, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_, Color(colourwhite));
            t = _t;
        }

        static const char *typestr() { return "#Thumbnail"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            lastcolor = Color(0, 0, 0, 0);
        }

        void draw(float sx, float sy)
        {
            if(!t || t == notexture)
            {
                Object::draw(sx, sy);
                return;
            }

            setupdraw();

            Color c = colors[0];
            if(lastcolor != c)
            {
                lastcolor = c;
                c.init();
            }

            vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
            float xt = min(1.0f, t->xs/float(t->ys)), yt = min(1.0f, t->ys/float(t->xs));
            float xoff = (1.0f - xt) * 0.5f, yoff = (1.0f - yt) * 0.5f;
            loopk(4) { tc[k].x = (tc[k].x - xoff)/xt; tc[k].y = (tc[k].y - yoff)/yt; }

            settexture(t);
            quad(sx, sy, w, h, tc);

            Object::draw(sx, sy);
        }
    };

    Color Thumbnail::lastcolor(255, 255, 255);

    ICOMMAND(0, uithumbnailunclamped, "sffe", (char *texname, float *minw, float *minh, uint *children),
    {
        Texture *t = loadthumbnail(texname, 0);
        BUILD(Thumbnail, o, o->setup(t, *minw*uiscale, *minh*uiscale), children);
    });

    ICOMMAND(0, uithumbnail, "sffe", (char *texname, float *minw, float *minh, uint *children),
    {
        Texture *t = loadthumbnail(texname, 3);
        BUILD(Thumbnail, o, o->setup(t, *minw*uiscale, *minh*uiscale), children);
    });

    ICOMMAND(0, uithumbnailclamped, "sffe", (char *texname, float *minw, float *minh, uint *children),
    {
        Texture *t = loadthumbnail(texname, 0x7000);
        BUILD(Thumbnail, o, o->setup(t, *minw*uiscale, *minh*uiscale), children);
    });

    struct Shape : Target
    {
        void setup(const Color &color_, int type_ = -1, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_, color_, type_);
        }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }
    };

    struct Triangle : Shape
    {
        vec2 a, b, c;

        void setup(const Color &color_, float w = 0, float h = 0, int angle = 0, int type_ = -1)
        {
            a = vec2(0, -h*2.0f/3);
            b = vec2(-w/2, h/3);
            c = vec2(w/2, h/3);
            if(angle)
            {
                vec2 rot = sincosmod360(-angle);
                a.rotate_around_z(rot);
                b.rotate_around_z(rot);
                c.rotate_around_z(rot);
            }
            vec2 bbmin = vec2(a).min(b).min(c);
            a.sub(bbmin);
            b.sub(bbmin);
            c.sub(bbmin);
            vec2 bbmax = vec2(a).max(b).max(c);

            Shape::setup(color_, type_, bbmax.x, bbmax.y);
        }

        static const char *typestr() { return "#Triangle"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(type == OUTLINED) return false;
            bool side = vec2(cx, cy).sub(b).cross(vec2(a).sub(b)) < 0;
            return (vec2(cx, cy).sub(c).cross(vec2(b).sub(c)) < 0) == side &&
                   (vec2(cx, cy).sub(a).cross(vec2(c).sub(a)) < 0) == side;
        }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            setupdraw(CHANGE_SHADER);

            colors[0].init();
            gle::begin(type == OUTLINED ? GL_LINE_LOOP : GL_TRIANGLES);
            gle::attrib(vec2(sx, sy).add(a));
            gle::attrib(vec2(sx, sy).add(b));
            gle::attrib(vec2(sx, sy).add(c));
            gle::end();
        }
    };

    ICOMMAND(0, uitriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle), children));

    ICOMMAND(0, uitriangleoutline, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::OUTLINED), children));

    ICOMMAND(0, uimodtriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::MODULATE), children));

    struct Circle : Shape
    {
        float radius;

        void setup(const Color &color_, float size, int type_ = -1)
        {
            Shape::setup(color_, type_, size, size);

            radius = size/2;
        }

        static const char *typestr() { return "#Circle"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(type == OUTLINED) return false;
            float r = radius <= 0 ? min(w, h)/2 : radius;
            return vec2(cx, cy).sub(r).squaredlen() <= r*r;
        }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            setupdraw(CHANGE_SHADER);

            float r = radius <= 0 ? min(w, h)/2 : radius;
            colors[0].init();
            vec2 center(sx + r, sy + r);
            if(type == OUTLINED)
            {
                gle::begin(GL_LINE_LOOP);
                for(int angle = 0; angle < 360; angle += 360/15)
                    gle::attrib(vec2(sincos360[angle]).mul(r).add(center));
                gle::end();
            }
            else
            {
                gle::begin(GL_TRIANGLE_FAN);
                gle::attrib(center);
                gle::attribf(center.x + r, center.y);
                for(int angle = 360/15; angle < 360; angle += 360/15)
                {
                    vec2 p = vec2(sincos360[angle]).mul(r).add(center);
                    gle::attrib(p);
                    gle::attrib(p);
                }
                gle::attribf(center.x + r, center.y);
                gle::end();
            }
        }
    };

    ICOMMAND(0, uicircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale), children));

    ICOMMAND(0, uicircleoutline, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::OUTLINED), children));

    ICOMMAND(0, uimodcircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::MODULATE), children));

    struct Text : Colored
    {
        float scale, wrap, tw, th, wlen, limit, rescale, growth;
        int align, pos, rotate;
        bool modcol;

        void setup(float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = 0, int pos_ = -1, float growth_ = 1, int rotate_ = 0, bool modcol_ = false)
        {
            Colored::setup(color_);
            tw = th = wlen = 0;
            rescale = 1;
            scale = scale_;
            wrap = wrap_;
            limit = limit_;
            align = align_;
            pos = pos_;
            growth = growth_;
            rotate = rotate_;
            modcol = modcol_;
        }

        static const char *typestr() { return "#Text"; }
        const char *gettype() const { return typestr(); }
        bool istext() const { return true; }

        float drawscale(float ss = 1) const { return scale / FONTH * ss; }

        virtual const char *getstr() const { return ""; }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            float k = drawscale(rescale), left = sx/k, top = sy/k;
            int flags = modcol ? TEXT_MODCOL : 0;
            switch(align)
            {
                case -2:    flags |= TEXT_NO_INDENT|TEXT_LEFT_JUSTIFY; break;
                case -1:    flags |= TEXT_LEFT_JUSTIFY; break;
                case 0:     flags |= TEXT_CENTERED; left += tw*k*0.5f; break;
                case 1:     flags |= TEXT_RIGHT_JUSTIFY; left += tw*k; break;
                case 2:     flags |= TEXT_NO_INDENT|TEXT_RIGHT_JUSTIFY; left += tw*k; break;
            }
            if(rescale != 1) top += (((th*drawscale())-(th*k))*0.5f)/k;
            //if(rescale != 1) top += (th-(th*rescale))*0.5f;
            if(growth < 0) top += th-(th/(0-growth));
            if(rotate == 1 || rotate == 2) left += tw;
            if(rotate == 2 || rotate == 3) top += th;

            pushhudmatrix();
            hudmatrix.scale(k, k, 1);
            hudmatrix.translate(left, top, 0);
            if(rotate) hudmatrix.rotate_around_z(rotate*90*RAD);
            flushhudmatrix();
            textshader = hudtextshader;
            draw_text(getstr(), 0, 0, colors[0].val.r, colors[0].val.g, colors[0].val.b, colors[0].val.a, flags, pos, wlen, 1);
            textshader = NULL;
            pophudmatrix();

            Object::draw(sx, sy);
        }

        void layout()
        {
            Object::layout();

            float k = drawscale();
            if(wrap > 0) wlen = wrap/k;
            else if(wrap < 0)
            {
                float wp = 0.f;
                wlen = 0 - wrap;
                for(Object *o = this->parent; o != NULL; o = o->parent)
                {
                    if(o->istype<Padder>())
                    {
                        wp += ((Padder *)o)->left + ((Padder *)o)->right;
                        continue;
                    }
                    float ww = o->w;
                    if(o->isfill()) ww = max(ww, ((Filler *)o)->minw);
                    else if(o->istype<HorizontalList>())
                    {
                        HorizontalList *h = (HorizontalList *)o;
                        loopv(o->children)
                        {
                            Object *p = o->children[i];
                            if(p == this) break;
                            ww -= p->w + h->space;
                        }
                    }
                    if(ww > 0)
                    {
                        wlen *= (ww - wp)/k;
                        break;
                    }
                    if(o->istype<Window>()) break;
                }
            }
            else wlen = 0;
            int flags = modcol ? TEXT_MODCOL : 0;
            switch(align)
            {
                case -2:    flags |= TEXT_NO_INDENT|TEXT_LEFT_JUSTIFY; break;
                case -1:    flags |= TEXT_LEFT_JUSTIFY; break;
                case 0:     flags |= TEXT_CENTERED; break;
                case 1:     flags |= TEXT_RIGHT_JUSTIFY; break;
                case 2:     flags |= TEXT_NO_INDENT|TEXT_RIGHT_JUSTIFY; break;
            }
            text_boundsf(getstr(), tw, th, 0, 0, wlen, flags);
            if(rotate%2) { int rw = tw; tw = th; th = rw; }
            rescale = 1;
            if(limit < 0)
            {
                float lw = tw*k, lm = 0-limit;
                if(lw > lm) rescale = lm/lw;
            }
            else if(limit > 0)
            {
                float lw = tw*k, lm = limit, lp = 0;
                if(lw > 0) for(Object *o = this->parent; o != NULL; o = o->parent)
                {
                    if(o->istype<Padder>())
                    {
                        lp += ((Padder *)o)->left+((Padder *)o)->right;
                        continue;
                    }
                    float ls = o->w;
                    if(o->isfill()) ls = max(ls, ((Filler *)o)->minw);
                    if(ls > 0)
                    {
                        float lo = (ls-lp)*lm;
                        if(lw > lo) rescale = lo/lw;
                        break;
                    }
                    if(o->istype<Window>()) break;
                }
            }
            if(growth != 1) th *= growth > 0 ? growth : 0-growth;
            w = max(w, tw*k*min(rescale, 1.f));
            h = max(h, th*k);
        }
    };

    UIARGTK(Text, text, scale, "f", float, 0.f, FVAR_MAX, *val*uiscale*uitextscale);
    UIARGTK(Text, text, wrap, "f", float, FVAR_MIN, FVAR_MAX, *val >= 0 ? *val*uiscale : *val);
    UIARGTK(Text, text, limit, "f", float, FVAR_MIN, FVAR_MAX, *val >= 0 ? *val : *val*uiscale);
    UIARGT(Text, text, growth, "f", float, FVAR_MIN, FVAR_MAX);
    UIARGT(Text, text, align, "i", int, -2, 2);
    UIARGT(Text, text, pos, "i", int, -1, VAR_MAX);
    UIARGT(Text, text, rotate, "i", int, 0, 3);
    UIARGTB(Text, text, modcol);

    struct TextString : Text
    {
        char *str;

        TextString() : str(NULL) {}
        ~TextString() { delete[] str; }

        void setup(const char *str_, float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = 0, int pos_ = -1, float growth_ = 1)
        {
            Text::setup(scale_, color_, wrap_, limit_, align_, pos_, growth_);

            SETSTR(str, str_);
        }

        static const char *typestr() { return "#TextString"; }
        const char *gettype() const { return typestr(); }
        bool istext() const { return true; }

        const char *getstr() const { return str; }
    };

    struct TextInt : Text
    {
        int val;
        char str[20];

        TextInt() : val(0) { str[0] = '0'; str[1] = '\0'; }

        void setup(int val_, float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = 0, int pos_ = -1, float growth_ = 1)
        {
            Text::setup(scale_, color_, wrap_, limit_, align_, pos_, growth_);

            if(val != val_) { val = val_; intformat(str, val, sizeof(str)); }
        }

        static const char *typestr() { return "#TextInt"; }
        const char *gettype() const { return typestr(); }
        bool istext() const { return true; }

        const char *getstr() const { return str; }
    };

    struct TextFloat : Text
    {
        float val;
        char str[20];

        TextFloat() : val(0) { memcpy(str, "0.0", 4); }

        void setup(float val_, float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = 0, int pos_ = -1, float growth_ = 1)
        {
            Text::setup(scale_, color_, wrap_, limit_, align_, pos_, growth_);

            if(val != val_) { val = val_; floatformat(str, val, sizeof(str)); }
        }

        static const char *typestr() { return "#TextFloat"; }
        const char *gettype() const { return typestr(); }
        bool istext() const { return true; }

        const char *getstr() const { return str; }
    };

    static inline void buildtext(tagval &t, float scale, float scalemod, const Color &color, uint *children)
    {
        if(scale <= 0) scale = 1;
        scale *= scalemod;
        switch(t.type)
        {
            case VAL_INT:
                BUILD(TextInt, o, o->setup(t.i, scale*uiscale, color), children);
                break;
            case VAL_FLOAT:
                BUILD(TextFloat, o, o->setup(t.f, scale*uiscale, color), children);
                break;
            case VAL_CSTR:
            case VAL_MACRO:
            case VAL_STR:
                if(t.s[0])
                {
                    BUILD(TextString, o, o->setup(t.s, scale*uiscale, color), children);
                    break;
                }
                // fall-through
            default:
                BUILD(TextString, o, o->setup("", scale*uiscale, color), children);
                break;
        }
    }

    ICOMMAND(0, uitext, "tfe", (tagval *text, float *scale, uint *children),
        buildtext(*text, *scale, uitextscale, Color(colourwhite), children));

    ICOMMAND(0, uicolourtext, "tife", (tagval *text, int *c, float *scale, uint *children),
        buildtext(*text, *scale, uitextscale, Color(*c), children));

    struct TexGC : Object
    {
        void layout()
        {
            bool oldtexgx = texgc;
            texgc = true;

            Object::layout();

            texgc = oldtexgx;
        }

        void draw(float sx, float sy)
        {
            bool oldtexgx = texgc;
            texgc = true;

            Object::draw(sx, sy);

            texgc = oldtexgx;
        }

        void buildchildren(uint *contents)
        {
            bool oldtexgx = texgc;
            texgc = true;

            Object::buildchildren(contents);

            texgc = oldtexgx;
        }

        #define DOSTATE(chkflags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!(allowstate&chkflags)) return; \
                bool oldtexgx = texgc; \
                texgc = true; \
                Object::func##children(cx, cy, cinside, mask, mode, setflags); \
                texgc = oldtexgx; \
            }
        DOSTATES
        #undef DOSTATE

        bool rawkey(int code, bool isdown)
        {
            bool oldtexgx = texgc;
            texgc = true;

            bool result = Object::rawkey(code, isdown);

            texgc = oldtexgx;

            return result;
        }

        bool key(int code, bool isdown)
        {
            bool oldtexgx = texgc;
            texgc = true;

            bool result = Object::key(code, isdown);

            texgc = oldtexgx;

            return result;
        }

        bool textinput(const char *str, int len)
        {
            bool oldtexgx = texgc;
            texgc = true;

            bool result = Object::textinput(str, len);

            texgc = oldtexgx;

            return result;
        }
    };

    ICOMMAND(0, uitexgc, "e", (uint *children),
        BUILD(TexGC, o, o->setup(), children));

    struct Clipper : Object
    {
        float sizew, sizeh, virtw, virth, offsetx, offsety;
        bool inverted, invscroll, forced;

        Clipper() : offsetx(0), offsety(0), inverted(false), invscroll(false) {}

        void setup(float sizew_ = 0, float sizeh_ = 0, float offsetx_ = 0, float offsety_ = 0, bool offset_ = false)
        {
            Object::setup();
            sizew = sizew_;
            sizeh = sizeh_;
            //if(offsetx_ >= 0) offsetx = offsetx_;
            //if(offsety_ >= 0) offsety = offsety_;
            if(offset_)
            {
                offsetx = offsetx_;
                offsety = offsety_;
            }
            forced = false;
            //virtw = virth = 0;
        }

        static const char *typestr() { return "#Clipper"; }
        const char *gettype() const { return typestr(); }
        bool isclip() const { return true; }

        void layout()
        {
            Object::layout();
            virtw = w;
            virth = h;
            if(sizew || forced) w = min(w, sizew);
            if(sizeh || forced) h = min(h, sizeh);
            offsetx = min(offsetx, hlimit());
            offsety = min(offsety, vlimit());
        }

        void adjustchildren()
        {
            adjustchildrento(0, 0, virtw, virth);
        }

        #define DOSTATE(chkflags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!(allowstate&chkflags)) return; \
                cx += offsetx; \
                cy += offsety; \
                if(cx < virtw && cy < virth) Object::func##children(cx, cy, true, mask, mode, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void draw(float sx, float sy)
        {
            bool isdraw = (sizew && virtw > sizew) || (sizeh && virth > sizeh);
            if(forced || isdraw)
            {
                float drawx = sx, drawy = sy;

                if(isdraw)
                {
                    if(inverted)
                    {
                        drawx = sx - (hlimit() - offsetx);
                        drawy = sy - (vlimit() - offsety);
                    }
                    else
                    {
                        drawx = sx - offsetx;
                        drawy = sy - offsety;
                    }
                }
                stopdrawing();
                pushclip(sx, sy, w, h);
                Object::draw(drawx, drawy);
                stopdrawing();
                popclip();
            }
            else Object::draw(sx, sy);
        }

        float hlimit() const { return max(virtw - w, 0.0f); }
        float vlimit() const { return max(virth - h, 0.0f); }
        float hoffset() const { return offsetx / max(virtw, w); }
        float voffset() const { return offsety / max(virth, h); }
        float hscale() const { return w / max(virtw, w); }
        float vscale() const { return h / max(virth, h); }

        void addhscroll(float hscroll) { sethscroll(offsetx + (hscroll * (invscroll ? 1 : -1))); }
        void addvscroll(float vscroll) { setvscroll(offsety + (vscroll * (invscroll ? -1 : 1))); }
        void sethscroll(float hscroll) { offsetx = clamp(hscroll, 0.0f, hlimit()); }
        void setvscroll(float vscroll) { offsety = clamp(vscroll, 0.0f, vlimit()); }
    };

    ICOMMAND(0, uiclip, "ffffe", (float *sizew, float *sizeh, float *offsetx, float *offsety, uint *children),
        BUILD(Clipper, o, o->setup(*sizew*uiscale, *sizeh*uiscale, *offsetx*uiscale, *offsety*uiscale, true), children));

    UIARGSCALEDT(Clipper, clip, sizew, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, sizeh, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, virtw, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, virth, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, offsetx, "f", float, FVAR_MIN, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, offsety, "f", float, FVAR_MIN, FVAR_MAX);
    UIARGTB(Clipper, clip, inverted);
    UIARGTB(Clipper, clip, invscroll);
    UIARGTB(Clipper, clip, forced);

    struct Scroller : Clipper
    {
        bool disabled;

        void setup(float sizew_ = 0, float sizeh_ = 0)
        {
            Clipper::setup(sizew_, sizeh_);
            disabled = false;
        }

        static const char *typestr() { return "#Scroller"; }
        const char *gettype() const { return typestr(); }

        void scrollup(float cx, float cy, bool inside);
        void scrolldown(float cx, float cy, bool inside);
        bool canscroll() const { return !disabled && !surface->lockscroll; }
    };

    ICOMMAND(0, uiscroll, "ffe", (float *sizew, float *sizeh, uint *children),
        BUILD(Scroller, o, o->setup(*sizew*uiscale, *sizeh*uiscale), children));
    UIARGB(Scroller, scroll, disabled);

    struct ScrollButton : Object
    {
        static const char *typestr() { return "#ScrollButton"; }
        const char *gettype() const { return typestr(); }
    };

    ICOMMAND(0, uiscrollbutton, "e", (uint *children),
        BUILD(ScrollButton, o, o->setup(), children));

    struct ScrollBar : Object
    {
        float offsetx, offsety;

        ScrollBar() : offsetx(0), offsety(0) {}

        static const char *typestr() { return "#ScrollBar"; }
        const char *gettype() const { return typestr(); }
        const char *gettypename() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        virtual void scrollto(float cx, float cy, bool closest = false) {}

        void hold(float cx, float cy, bool inside)
        {
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(button && button->haschildstate(STATE_HOLD)) movebutton(button, offsetx, offsety, cx - button->x, cy - button->y);
        }

        void press(float cx, float cy, bool inside)
        {
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(button && button->haschildstate(STATE_PRESS)) { offsetx = cx - button->x; offsety = cy - button->y; }
            else scrollto(cx, cy, true);
        }

        virtual void addscroll(Scroller *scroller, float dir) = 0;

        void addscroll(float dir)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(scroller && scroller->canscroll()) addscroll(scroller, dir);
        }

        void arrowscroll(float dir) { addscroll(dir*uicurtime/1000.0f); }
        void wheelscroll(float step);
        virtual int wheelscrolldirection() const { return 1; }

        void scrollup(float cx, float cy, bool inside) { wheelscroll(-wheelscrolldirection()); }
        void scrolldown(float cx, float cy, bool inside) { wheelscroll(wheelscrolldirection()); }

        virtual void movebutton(Object *o, float fromx, float fromy, float tox, float toy) = 0;
    };

    void Scroller::scrollup(float cx, float cy, bool inside)
    {
        if(!canscroll()) return;
        ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
        if(scrollbar) scrollbar->wheelscroll(-scrollbar->wheelscrolldirection());
    }

    void Scroller::scrolldown(float cx, float cy, bool inside)
    {
        if(!canscroll()) return;
        ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
        if(scrollbar) scrollbar->wheelscroll(scrollbar->wheelscrolldirection());
    }

    struct ScrollArrow : Object
    {
        float speed;

        void setup(float speed_ = 0)
        {
            Object::setup();
            speed = speed_;
        }

        static const char *typestr() { return "#ScrollArrow"; }
        const char *gettype() const { return typestr(); }

        void hold(float cx, float cy, bool inside)
        {
            ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
            if(scrollbar) scrollbar->arrowscroll(speed);
        }
    };

    ICOMMAND(0, uiscrollarrow, "fe", (float *dir, uint *children),
        BUILD(ScrollArrow, o, o->setup(*dir), children));

    UIARG(ScrollArrow, scrollarrow, speed, "f", float, FVAR_MIN, FVAR_MAX);

    void ScrollBar::wheelscroll(float step)
    {
        ScrollArrow *arrow = (ScrollArrow *)findsibling(ScrollArrow::typestr());
        if(arrow) addscroll(arrow->speed*step*uiscrollsteptime/1000.0f);
    }

    struct HorizontalScrollBar : ScrollBar
    {
        static const char *typestr() { return "#HorizontalScrollBar"; }
        const char *gettype() const { return typestr(); }

        void addscroll(Scroller *scroller, float dir)
        {
            scroller->addhscroll(dir);
        }

        void scrollto(float cx, float cy, bool closest = false)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bscale = (w - button->w) / (1 - scroller->hscale()),
                  offset = bscale > 1e-3f ? (closest && cx >= button->x + button->w ? cx - button->w : cx)/bscale : 0;

            if(scroller->inverted) scroller->sethscroll(scroller->hlimit() - (offset*scroller->virtw));
            else scroller->sethscroll(offset*scroller->virtw);
        }

        void adjustchildren()
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bw = w*scroller->hscale();
            button->w = max(button->w, bw);
            float bscale = scroller->hscale() < 1 ? (w - button->w) / (1 - scroller->hscale()) : 1;

            if(scroller->inverted) button->x = ((1 - scroller->hoffset())*bscale) - button->w;
            else button->x = scroller->hoffset()*bscale;

            button->adjust &= ~ALIGN_HMASK;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float fromy, float tox, float toy)
        {
            scrollto(o->x + tox - fromx, o->y + toy);
        }
    };

    ICOMMAND(0, uihscrollbar, "e", (uint *children),
        BUILD(HorizontalScrollBar, o, o->setup(), children));

    struct VerticalScrollBar : ScrollBar
    {
        static const char *typestr() { return "#VerticalScrollBar"; }
        const char *gettype() const { return typestr(); }

        void addscroll(Scroller *scroller, float dir)
        {
            scroller->addvscroll(dir);
        }

        void scrollto(float cx, float cy, bool closest = false)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bscale = (h - button->h) / (1 - scroller->vscale()),
                  offset = bscale > 1e-3f ? (closest && cy >= button->y + button->h ? cy - button->h : cy)/bscale : 0;

            if(scroller->inverted) scroller->setvscroll(scroller->vlimit() - (offset*scroller->virth));
            else scroller->setvscroll(offset*scroller->virth);
        }

        void adjustchildren()
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bh = h*scroller->vscale();
            button->h = max(button->h, bh);
            float bscale = scroller->vscale() < 1 ? (h - button->h) / (1 - scroller->vscale()) : 1;

            if(scroller->inverted) button->y = ((1 - scroller->voffset())*bscale) - button->h;
            else button->y = scroller->voffset()*bscale;

            button->adjust &= ~ALIGN_VMASK;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float fromy, float tox, float toy)
        {
            scrollto(o->x + tox, o->y + toy - fromy);
        }

        int wheelscrolldirection() const { return -1; }
    };

    ICOMMAND(0, uivscrollbar, "e", (uint *children),
        BUILD(VerticalScrollBar, o, o->setup(), children));

    struct SliderButton : Object
    {
        static const char *typestr() { return "#SliderButton"; }
        const char *gettype() const { return typestr(); }
    };

    ICOMMAND(0, uisliderbutton, "e", (uint *children),
        BUILD(SliderButton, o, o->setup(), children));

    static double getfval(ident *id, double val = 0)
    {
        switch(id->type)
        {
            case ID_VAR: val = *id->storage.i; break;
            case ID_FVAR: val = *id->storage.f; break;
            case ID_SVAR: val = parsenumber(*id->storage.s); break;
            case ID_ALIAS: val = id->getnumber(); break;
            case ID_COMMAND:
            {
                tagval t;
                executeret(id, NULL, 0, true, t);
                val = t.getnumber();
                t.cleanup();
                break;
            }
        }
        return val;
    }

    static void setfval(ident *id, double val, uint *onchange = NULL)
    {
        switch(id->type)
        {
            case ID_VAR: setvarchecked(id, int(clamp(val, double(INT_MIN), double(INT_MAX)))); break;
            case ID_FVAR: setfvarchecked(id, val); break;
            case ID_SVAR: setsvarchecked(id, numberstr(val)); break;
            case ID_ALIAS: alias(id->name, numberstr(val)); break;
            case ID_COMMAND:
            {
                tagval t;
                t.setnumber(val);
                execute(id, &t, 1);
                break;
            }
        }
        if(onchange && (*onchange&CODE_OP_MASK) != CODE_EXIT) execute(onchange);
    }

    struct Slider : Object
    {
        ident *id;
        double val, vmin, vmax, vstep;
        bool changed;

        Slider() : id(NULL), val(0), vmin(0), vmax(0), vstep(0), changed(false) {}

        void setup(ident *id_, double vmin_ = 0, double vmax_ = 0, double vstep_ = 1, uint *onchange = NULL)
        {
            Object::setup();
            if(!vmin_ && !vmax_) switch(id_->type)
            {
                case ID_VAR:
                {
                    if(id->flags&IDF_HEX && uint(id_->maxval) == 0xFFFFFFFFU)
                    {
                        vmin_ = uint(id_->minval);
                        vmax_ = uint(id_->maxval);
                    }
                    else
                    {
                        vmin_ = id_->minval;
                        vmax_ = id_->maxval;
                    }
                    break;
                }
                case ID_FVAR: vmin_ = id_->minvalf; vmax_ = id_->maxvalf; break;
            }
            if(id != id_) changed = false;
            id = id_;
            vmin = vmin_;
            vmax = vmax_;
            vstep = vstep_ > 0 ? vstep_ : 1;
            if(changed)
            {
                setfval(id, val, onchange);
                changed = false;
            }
            else val = getfval(id, vmin);
        }

        static const char *typestr() { return "#Slider"; }
        const char *gettype() const { return typestr(); }
        const char *gettypename() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        void arrowscroll(double dir)
        {
            double newval = val + dir*vstep;
            newval += vstep * (newval < 0 ? -0.5 : 0.5);
            newval -= fmod(newval, vstep);
            newval = clamp(newval, min(vmin, vmax), max(vmin, vmax));
            if(val != newval) changeval(newval);
        }

        void wheelscroll(float step);
        virtual int wheelscrolldirection() const { return 1; }

        void scrollup(float cx, float cy, bool inside) { wheelscroll(-wheelscrolldirection()); }
        void scrolldown(float cx, float cy, bool inside) { wheelscroll(wheelscrolldirection()); }

        virtual void scrollto(float cx, float cy) {}

        void hold(float cx, float cy, bool inside)
        {
            scrollto(cx, cy);
        }

        void changeval(double newval)
        {
            val = newval;
            changed = true;
        }
    };

    struct SliderArrow : Object
    {
        double stepdir;
        int laststep;

        SliderArrow() : laststep(0) {}

        void setup(double dir_ = 0)
        {
            Object::setup();
            stepdir = dir_;
        }

        static const char *typestr() { return "#SliderArrow"; }
        const char *gettype() const { return typestr(); }

        void press(float cx, float cy, bool inside)
        {
            laststep = uitotalmillis + 2*uislidersteptime;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }

        void hold(float cx, float cy, bool inside)
        {
            if(uitotalmillis < laststep + uislidersteptime)
                return;
            laststep = uitotalmillis;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }
    };

    ICOMMAND(0, uisliderarrow, "fe", (float *dir, uint *children),
        BUILD(SliderArrow, o, o->setup(*dir), children));

    void Slider::wheelscroll(float step)
    {
        SliderArrow *arrow = (SliderArrow *)findsibling(SliderArrow::typestr());
        if(arrow) step *= arrow->stepdir;
        arrowscroll(step);
    }

    struct HorizontalSlider : Slider
    {
        static const char *typestr() { return "#HorizontalSlider"; }
        const char *gettype() const { return typestr(); }

        void scrollto(float cx, float cy)
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            float offset = w > button->w ? clamp((cx - button->w/2)/(w - button->w), 0.0f, 1.0f) : 0.0f;
            int step = int((val - vmin) / vstep),
                bstep = int(offset * (vmax - vmin) / vstep);
            if(step != bstep) changeval(bstep * vstep + vmin);
        }

        void adjustchildren()
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            int step = int((val - vmin) / vstep),
                bstep = int(button->x / (w - button->w) * (vmax - vmin) / vstep);
            if(step != bstep) button->x = (w - button->w) * step * vstep / (vmax - vmin);
            button->adjust &= ~ALIGN_HMASK;

            Slider::adjustchildren();
        }
    };

    ICOMMAND(0, uihslider, "rfffee", (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children),
        BUILD(HorizontalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children));

    struct VerticalSlider : Slider
    {
        static const char *typestr() { return "#VerticalSlider"; }
        const char *gettype() const { return typestr(); }

        void scrollto(float cx, float cy)
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            float offset = h > button->h ? clamp((cy - button->h/2)/(h - button->h), 0.0f, 1.0f) : 0.0f;
            int step = int((val - vmin) / vstep),
                bstep = int(offset * (vmax - vmin) / vstep);
            if(step != bstep) changeval(bstep * vstep + vmin);
        }

        void adjustchildren()
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            int step = int((val - vmin) / vstep),
                bstep = int(button->y / (h - button->h) * (vmax - vmin) / vstep);
            if(step != bstep) button->y = (h - button->h) * step * vstep / (vmax - vmin);
            button->adjust &= ~ALIGN_VMASK;

            Slider::adjustchildren();
        }

        int wheelscrolldirection() const { return -1; }
    };

    ICOMMAND(0, uivslider, "rfffee", (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children),
        BUILD(VerticalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children));

    struct TextEditor : Colored
    {
        float scale, offsetx, offsety;
        editor *edit;
        char *keyfilter;
        bool canfocus, allowlines, wasfocus;

        TextEditor() : edit(NULL), keyfilter(NULL), canfocus(true), allowlines(true) {}

        bool iseditor() const { return true; }

        void setup(const char *name, int length, int height, float scale_ = 1, const char *initval = NULL, int mode = EDITORUSED, const char *keyfilter_ = NULL, bool allowlines_ = true, int limit_ = 0)
        {
            Colored::setup(Color(colourwhite));
            edit = useeditor(name, mode, false, initval);
            if(initval && edit->mode == EDITORFOCUSED && !isfocus()) edit->clear(initval);
            edit->active = true;
            edit->linewrap = length < 0;
            edit->maxx = edit->linewrap ? -1 : length;
            edit->maxy = height <= 0 ? 1 : -1;
            edit->pixelwidth = abs(length)*FONTW;
            edit->limit = limit_;
            if(edit->linewrap && edit->maxy == 1) edit->updateheight();
            else edit->pixelheight = FONTH*max(height, 1);
            scale = scale_;
            if(keyfilter_ && *keyfilter_) SETSTR(keyfilter, keyfilter_);
            else DELETEA(keyfilter);
            allowlines = allowlines_;
            wasfocus = false;
        }

        ~TextEditor()
        {
            if(inputsteal == this) inputsteal = NULL;
            DELETEA(keyfilter);
        }

        void prepare()
        {
            if(!inputsteal && isfocus())
                inputsteal = this;
            else if(wasfocus)
            {
                wasfocus = false;
                if(!textfocus)
                {
                    ::textinput(false, TI_UI);
                    ::keyrepeat(false, KR_UI);
                }
            }

            Object::prepare();
        }

        void setfocus()
        {
            if(isfocus()) return;
            textfocus = edit;
            wasfocus = true;
            ::textinput(true, TI_UI);
            ::keyrepeat(true, KR_UI);
        }

        void setfocusable(bool focusable) { canfocus = focusable; }

        void clearfocus(bool force = false)
        {
            if(!isfocus()) return;
            textfocus = NULL;
            wasfocus = false;
            ::textinput(false, TI_UI);
            ::keyrepeat(false, KR_UI);
        }

        bool isfocus() const { return edit && textfocus == edit; }

        static const char *typestr() { return "#TextEditor"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        float drawscale() const { return scale / FONTH; }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            edit->rendered = true;

            float k = drawscale();
            pushhudtranslate(sx, sy, k);

            textshader = hudtextshader;
            edit->draw(FONTW/2, 0, colors[0].val.tohexcolor(), colors[0].val.a, isfocus());
            textshader = NULL;

            pophudmatrix();

            Object::draw(sx, sy);
        }

        void layout()
        {
            Object::layout();

            float k = drawscale();
            w = max(w, (edit->pixelwidth + FONTW)*k);
            h = max(h, edit->pixelheight*k);
        }

        virtual void resetmark(float cx, float cy)
        {
            edit->mark(false);
            offsetx = cx;
            offsety = cy;
        }

        void press(float cx, float cy, bool inside)
        {
            if(!canfocus) return;

            setfocus();
            resetmark(cx, cy);
        }

        void release(float cx, float cy, bool inside)
        {
            if(isfocus() && !inside) cancel();
        }

        void hold(float cx, float cy, bool inside)
        {
            if(isfocus())
            {
                float k = drawscale();
                bool dragged = max(fabs(cx - offsetx), fabs(cy - offsety)) > (FONTH/8.0f)*k;
                edit->hit(int(floor(cx/k - FONTW/2)), int(floor(cy/k)), dragged);
            }
        }

        void scrollup(float cx, float cy, bool inside)
        {
            edit->scrollup();
        }

        void scrolldown(float cx, float cy, bool inside)
        {
            edit->scrolldown();
        }

        virtual void cancel()
        {
            clearfocus();
        }

        virtual void commit()
        {
            clearfocus();
        }

        void escrelease(float cx, float cy, bool inside)
        {
            cancel();
        }

        bool key(int code, bool isdown)
        {
            if(Object::key(code, isdown)) return true;
            if(!isfocus()) return false;
            switch(code)
            {
                case SDLK_ESCAPE:
                    if(!isdown) cancel();
                    return true;
                case SDLK_RETURN:
                case SDLK_TAB:
                    if(edit->maxy != 1 && allowlines) break;
                    // fall-through
                case SDLK_KP_ENTER:
                    if(isdown) commit();
                    return true;
            }
            if(isdown) edit->key(code);
            return true;
        }

        virtual bool allowtextinput() const { return true; }

        bool textinput(const char *str, int len)
        {
            if(Object::textinput(str, len)) return true;
            if(!isfocus() || !allowtextinput()) return false;
            if(!keyfilter) edit->input(str, len);
            else while(len > 0)
            {
                int accept = min(len, (int)strspn(str, keyfilter));
                if(accept > 0) edit->input(str, accept);
                str += accept + 1;
                len -= accept + 1;
                if(len <= 0) break;
                int reject = (int)strcspn(str, keyfilter);
                str += reject;
                str -= reject;
            }
            return true;
        }

        int count() const { return isfocus() && edit ? edit->len : -1; }
    };

    ICOMMAND(0, uitexteditor, "siifsies", (char *name, int *length, int *height, float *scale, char *initval, int *mode, uint *children, char *keyfilter),
        BUILD(TextEditor, o, o->setup(name, *length, *height, (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale, initval, *mode <= 0 ? EDITORFOREVER : *mode, keyfilter), children));

    UICMDT(TextEditor, editor, isfocus, "", (), intret(o->isfocus()));
    UICMDT(TextEditor, editor, setfocus, "", (), o->setfocus());
    UICMDT(TextEditor, editor, setfocusable, "i", (int *focusable), o->setfocusable(*focusable));
    UICMDT(TextEditor, editor, getcount, "", (), intret(o->count()));

    static const char *getsval(ident *id, bool &shouldfree, const char *val = "")
    {
        switch(id->type)
        {
            case ID_VAR: val = intstr(*id->storage.i); break;
            case ID_FVAR: val = floatstr(*id->storage.f); break;
            case ID_SVAR: val = *id->storage.s; break;
            case ID_ALIAS: val = id->getstr(); break;
            case ID_COMMAND: val = executestr(id, NULL, 0, true); shouldfree = true; break;
        }
        return val;
    }

    static void setsval(ident *id, const char *val, uint *onchange = NULL)
    {
        switch(id->type)
        {
            case ID_VAR: setvarchecked(id, parseint(val)); break;
            case ID_FVAR: setfvarchecked(id, parsefloat(val)); break;
            case ID_SVAR: setsvarchecked(id, val); break;
            case ID_ALIAS: alias(id->name, val); break;
            case ID_COMMAND:
            {
                tagval t;
                t.setstr(newstring(val));
                execute(id, &t, 1);
                break;
            }
        }
        if(onchange && (*onchange&CODE_OP_MASK) != CODE_EXIT) execute(onchange);
    }

    struct Field : TextEditor
    {
        ident *id;
        bool changed;

        Field() : id(NULL), changed(false) {}

        void setup(ident *id_, int length, uint *onchange, float scale = 1, const char *keyfilter_ = NULL, bool immediate = false, int height = 0, int limit = 0)
        {
            if(isfocus() && immediate && edit && id == id_)
            {
                bool shouldfree = false;
                const char *curval = getsval(id, shouldfree);
                if(strcmp(edit->lines[0].text, curval)) changed = true;
                if(shouldfree) delete[] curval;
            }
            if(changed)
            {
                if(id == id_) setsval(id, edit->lines[0].text, onchange);
                changed = false;
            }
            bool shouldfree = false;
            const char *initval = id != id_ || !isfocus() ? getsval(id_, shouldfree) : NULL;
            TextEditor::setup(id_->name, length, height, scale, initval, EDITORFOCUSED, keyfilter_, false, limit);
            if(shouldfree) delete[] initval;
            id = id_;
            edit->linewrapmark = false;
        }

        static const char *typestr() { return "#Field"; }
        const char *gettype() const { return typestr(); }

        void commit()
        {
            TextEditor::commit();
            changed = true;
        }

        void cancel()
        {
            TextEditor::cancel();
            changed = false;
        }
    };

    ICOMMAND(0, uifield, "riefies", (ident *var, int *length, uint *onchange, float *scale, int *immediate, uint *children, char *keyfilter),
        BUILD(Field, o, o->setup(var, *length, onchange, (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale, keyfilter, *immediate!=0), children));

    ICOMMAND(0, uimlfield, "riiiefies", (ident *var, int *length, int *height, int *limit, uint *onchange, float *scale, int *immediate, uint *children, char *keyfilter),
        BUILD(Field, o, o->setup(var, *length, onchange, (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale, keyfilter, *immediate!=0, *height, *limit), children));

    struct KeyCatcher : Target
    {
        static KeyCatcher *focus;

        ident *id;
        int pressedkey;

        // Workaround to prevent the focusing event from being treated as a keypress
        int focusmillis, keymillis;

        KeyCatcher() : id(NULL), pressedkey(0) {}
        ~KeyCatcher() { if(inputsteal == this) inputsteal = NULL; }

        bool iskeycatcher() const { return true; }

        static const char *typestr() { return "#KeyCatcher"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy) { return true; }

        void setup(ident *id_, float minw, float minh, uint *onkey)
        {
            Target::setup(minw, minh);

            id = id_;
            if(isfocus() && pressedkey && keymillis > focusmillis)
            {
                setsval(id, getkeyname(pressedkey), onkey);
                pressedkey = 0;
                clearfocus();
            }
        }

        void prepare()
        {
            if(!inputsteal && isfocus()) inputsteal = this;

            Object::prepare();
        }

        static void setfocus(KeyCatcher *kc)
        {
            if(focus == kc) return;
            focus = kc;
            if(kc)
            {
                inputsteal = kc;
                kc->focusmillis = uitotalmillis;
            }
        }
        void setfocus() { setfocus(this); }
        void clearfocus() { if(focus == this) setfocus(NULL); }
        bool isfocus() const { return focus == this; }

        void press(float cx, float cy, bool inside)
        {
            setfocus();
        }

        bool key(int code, bool isdown)
        {
            if(Object::key(code, isdown)) return true;
            if(!isfocus()) return false;
            switch(code)
            {
                case SDLK_ESCAPE:
                    return true;
            }
            return true;
        }

        bool rawkey(int code, bool isdown)
        {
            if(Object::rawkey(code, isdown)) return true;
            if(!isfocus() || !isdown) return false;
            if(code == SDLK_ESCAPE) clearfocus();
            else
            {
                pressedkey = code;
                keymillis = uitotalmillis;
            }
            return true;
        }
    };

    KeyCatcher *KeyCatcher::focus = NULL;
    ICOMMAND(0, uikeycatcher, "rffee", (ident *var, float *minw, float *minh, uint *onkey,
        uint *children),
        BUILD(KeyCatcher, o, o->setup(var, *minw, *minh, onkey), children));

    UICMDT(KeyCatcher, keycatcher, isfocus, "", (), intret(o->isfocus()));
    UICMDT(KeyCatcher, keycatcher, setfocus, "", (), o->setfocus());

    struct KeyField : Field
    {
        enum { MODE_MULTI = 0, MODE_COMBO };

        static const char *typestr() { return "#KeyField"; }
        const char *gettype() const { return typestr(); }

        int fieldmode;

        void setup(ident *id_, int length, uint *onchange, float scale = 1,
            const char *keyfilter_ = NULL, bool immediate = false, int mode = MODE_MULTI)
        {
            Field::setup(id_, length, onchange, scale, keyfilter_, immediate);
            fieldmode = mode;
        }

        void resetmark(float cx, float cy)
        {
            edit->clear();
            Field::resetmark(cx, cy);
        }

        void insertkey(int code)
        {
            bool ctrl = false, shift = false, alt = false;

            ctrl = code == SDLK_LCTRL || code == SDLK_RCTRL;
            alt = code == SDLK_LALT || code == SDLK_RALT;
            shift = code == SDLK_LSHIFT || code == SDLK_RSHIFT;

            // Do not insert the modifiers themselves
            if(fieldmode == MODE_COMBO && (ctrl || shift || alt))
                return;

            const char *keyname = getkeyname(code);
            if(keyname)
            {
                if(!edit->empty())
                {
                    if(fieldmode == MODE_MULTI) edit->insert(" ");
                    else edit->clear();
                }

                if(fieldmode == MODE_COMBO)
                {
                    if(SDL_GetModState()&MOD_KEYS) edit->insert("CTRL+");
                    if(SDL_GetModState()&MOD_ALTS) edit->insert("ALT+");
                    if(SDL_GetModState()&MOD_SHIFTS) edit->insert("SHIFT+");
                }

                edit->insert(keyname);

                if(fieldmode == MODE_COMBO) commit();
            }
        }

        bool rawkey(int code, bool isdown)
        {
            if(Object::rawkey(code, isdown)) return true;
            if(!isfocus() || !isdown) return false;
            if(code == SDLK_ESCAPE) commit();
            else insertkey(code);
            return true;
        }

        bool allowtextinput() const { return false; }
    };

    ICOMMAND(0, uikeyfield, "riefe", (ident *var, int *length, uint *onchange, float *scale,
        uint *children),
    {
        float s = (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale;
        BUILD(KeyField, o, o->setup(var, *length, onchange, s, NULL, false), children);
    });

    ICOMMAND(0, uicombokeyfield, "riefe", (ident *var, int *length, uint *onchange, float *scale,
        uint *children),
    {
        float s = (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale;
        BUILD(KeyField, o, o->setup(var, *length, onchange, s, NULL, false, KeyField::MODE_COMBO),
            children);
    });

    struct AxisView : Filler
    {
        void startdraw()
        {
            hudnotextureshader->set();

            gle::defvertex();
            gle::defcolor();
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            pushhudmatrix();
            matrix4 axismatrix, axisprojmatrix;
            axismatrix.identity();

            axismatrix.translate(0, 0, -0.5f);
            axismatrix.rotate_around_y(camera1->roll*RAD);
            axismatrix.rotate_around_x((camera1->pitch+90)*-RAD);
            axismatrix.rotate_around_z(camera1->yaw*RAD);

            axisprojmatrix.perspective(25, w/h, 0.1f, 10.0f);
            hudmatrix.muld(axisprojmatrix, axismatrix);
            flushhudmatrix();

            float hudaspect = hudw/float(hudh);
            glViewport(hudw*sx*(1/hudaspect), hudh*(1.0f-(sy+h)), hudw*w*(1/hudaspect), hudh*h);

            glLineWidth(4);

            const float offset = 0.08;

            const vec vbo[12][2] = {
                { vec(0,       0,       0),       vec(0.25f, 0.25f, 0.5f ) }, // -Z
                { vec(0,       0,       -offset), vec(0.25f, 0.25f, 0.5f ) },

                { vec(0,       0,       0),       vec(0.5f,  0.25f, 0.25f) }, // -X
                { vec(offset,  0,       0),       vec(0.5f,  0.25f, 0.25f) },
                { vec(0,       0,       0),       vec(1,     0,     0    ) }, // X
                { vec(-offset, 0,       0),       vec(1,     0,     0    ) },

                { vec(0,       0,       0),       vec(0.25f, 0.5f,  0.25f) }, // -Y
                { vec(0,       -offset, 0),       vec(0.25f, 0.5f,  0.25f) },
                { vec(0,       0,       0),       vec(0,     1,     0    ) }, // Y
                { vec(0,       offset,  0),       vec(0,     1,     0    ) },

                { vec(0,       0,       0),       vec(0,     0,     1    ) }, // Z
                { vec(0,       0,       offset),  vec(0,     0,     1    ) }
            };

            gle::begin(GL_LINES);

            if(camera1->pitch > 0) loopirev(12) loopj(2) gle::attrib(vbo[i][j]);
            else loopi(12) loopj(2) gle::attrib(vbo[i][j]);

            gle::end();

            glLineWidth(1);
            glViewport(0, 0, hudw, hudh);

            pophudmatrix();

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiaxisview, "ffe", (float *minw, float *minh, uint *children),
        BUILD(AxisView, o, o->setup(*minw*uiscale, *minh*uiscale), children));

    struct AmmoClip : Target
    {
        int weap;

        void setup(float weap_, float minw_, float minh_)
        {
            Target::setup(minw_, minh_);
            weap = int(weap_);
        }

        void startdraw()
        {
            hudshader->set();

            gle::defvertex();
            gle::defcolor();

            gle::colorf(1, 1, 1);
        }

        void draw(float sx, float sy)
        {
            setupdraw(CHANGE_SHADER);

            pushhudmatrix();
            hudmatrix.ortho(0, hud::hudwidth, hud::hudheight, 0, -1, 1);
            flushhudmatrix();

            hud::drawclip(weap, (sx / (float(hud::hudwidth)/hud::hudheight)) * hud::hudwidth, sy * hud::hudheight, hud::hudsize, true);

            pophudmatrix();

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiammoclip, "iffe", (int *weap, float *minw, float *minh, uint *children),
        BUILD(AmmoClip, o, o->setup(*weap, *minw*uiscale, *minh*uiscale), children));

    struct Preview : Target
    {
        float yaw, pitch, roll, fov;
        vec skycol, suncol, sundir, excol, exdir, translate;
        float offsetx, offsety, offsetyaw, offsetpitch;
        bool dragging, interact;

        Preview() { resetoffset(); }

        void resetoffset() { offsetx = offsety = offsetyaw = offsetpitch = 0; dragging = false; }

        void setup(float minw_ = 0, float minh_ = 0, float yaw_ = -1, float pitch_ = -15, float roll_ = 0, float fov_ = 0, const vec &skycol_ = vec(0.1f, 0.1f, 0.1f), const vec &suncol_ = vec(0.6f, 0.6f, 0.6f), const vec &sundir_ = vec(0, -1, 2), const vec &excol_ = vec(0.f, 0.f, 0.f), const vec &exdir_ = vec(0, 0, 0), const Color &color_ = Color(colourwhite))
        {
            Target::setup(minw_, minh_, color_);
            yaw = yaw_;
            pitch = pitch_;
            roll = roll_;
            fov = fov_;
            skycol = skycol_;
            suncol = suncol_;
            sundir = sundir_;
            excol = excol_;
            exdir = exdir_;
            interact = false;
            translate = vec(0, 0, 0);
        }

        bool ispreview() const { return true; }

        void startdraw()
        {
            glDisable(GL_BLEND);
            disableclip();
        }

        void enddraw()
        {
            glEnable(GL_BLEND);
            enableclip();
        }

        void hold(float cx, float cy, bool inside)
        {
            if(!interact) return;
            float rx = (cx - x) / w, ry = (cy - y) / h;
            if(rx < 0 || rx > 1 || ry < 0 || ry > 1) return;
            if(!dragging || rx != offsetx || ry != offsety)
            {
                if(dragging)
                {
                    float qx = offsetx - rx, qy = offsety - ry;
                    offsetyaw += qx * 360.f;
                    offsetpitch += qy * 180.f;
                }
                offsetx = rx;
                offsety = ry;
                dragging = true;
            }
        }

        void release(float cx, float cy, bool inside)
        {
            offsetx = 0;
            offsety = 0;
            dragging = false;
        }
    };

    UICMDT(Preview, preview, skycol, "i", (int *c), o->skycol = vec::fromcolor(*c));
    UICMDT(Preview, preview, suncol, "i", (int *c), o->suncol = vec::fromcolor(*c));
    UICMDT(Preview, preview, sundir, "fff", (float *x, float *y, float *z), o->sundir = vec(*x, *y, *z));
    UICMDT(Preview, preview, excol, "i", (int *c), o->excol = vec::fromcolor(*c));
    UICMDT(Preview, preview, exdir, "fff", (float *x, float *y, float *z), o->exdir = vec(*x, *y, *z));
    UICMDT(Preview, preview, yaw, "f", (float *n), o->yaw = *n);
    UICMDT(Preview, preview, pitch, "f", (float *n), o->pitch = *n);
    UICMDT(Preview, preview, roll, "f", (float *n), o->roll = *n);
    UICMDT(Preview, preview, fov, "f", (float *n), o->fov = *n);
    UICMDT(Preview, preview, interact, "i", (int *c), o->interact = *c != 0);
    UICMDT(Preview, preview, resetoffset, "", (void), o->resetoffset());
    UICMDT(Preview, preview, translate, "fff", (float *x, float *y, float *z), o->translate = vec(*x, *y, *z));

    struct ModelPreview : Preview
    {
        char *name;
        modelstate mdl;

        ModelPreview() : name(NULL) { resetoffset(); }
        ~ModelPreview() { delete[] name; }

        void setup(const char *name_, const char *animspec, float scale_, float blend_, float minw_, float minh_, float yaw_ = -1, float pitch_ = -15, float roll_ = 0, float fov_ = 20, const vec &skycol_ = vec(0.1f, 0.1f, 0.1f), const vec &suncol_ = vec(0.6f, 0.6f, 0.6f), const vec &sundir_ = vec(0, -1, 2), const vec &excol_ = vec(0.f, 0.f, 0.f), const vec &exdir_ = vec(0, 0, 0))
        {
            mdl.reset();

            Preview::setup(minw_, minh_, yaw_, pitch_, roll_, fov_, skycol_, suncol_, sundir_, excol_, exdir_);
            SETSTR(name, name_);

            setanim(animspec);
            mdl.size = scale_;
            mdl.color = vec4(1, 1, 1, blend_);
        }

        static const char *typestr() { return "#ModelPreview"; }
        const char *gettype() const { return typestr(); }
        bool ismodelpreview() const { return true; }

        void setanim(const char *animspec)
        {
            mdl.anim = ANIM_ALL;
            if(animspec[0])
            {
                if(isdigit(animspec[0]))
                {
                    mdl.anim = parseint(animspec);
                    if(mdl.anim >= 0) mdl.anim %= ANIM_INDEX;
                    else mdl.anim = ANIM_ALL;
                }
                else
                {
                    vector<int> anims;
                    game::findanims(animspec, anims);
                    if(anims.length()) mdl.anim = anims[0];
                }
            }
            mdl.anim |= ANIM_LOOP;
        }

        void draw(float sx, float sy)
        {
            if(!loadedshaders) { Object::draw(sx, sy); return; }

            setupdraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            bool hasclipstack = clipstack.length() > 0;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, pitch+offsetpitch, roll, fov, false, hasclipstack, translate);
            model *m = loadmodel(name);
            if(m)
            {
                vec center, radius;
                m->boundbox(center, radius);
                if(yaw >= 0) mdl.yaw = yaw;
                mdl.o = calcmodelpreviewpos(radius, mdl.yaw).sub(center);
                mdl.yaw += offsetyaw;
                rendermodel(name, mdl);
            }
            if(hasclipstack) clipstack.last().scissor();
            modelpreview::end(renderfbo, skycol, suncol, sundir, excol, exdir);

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uimodelpreview, "ssffffe", (char *model, char *animspec, float *scale, float *blend, float *minw, float *minh, uint *children),
        BUILD(ModelPreview, o, o->setup(model, animspec, *scale, *blend, *minw*uiscale, *minh*uiscale), children));

    UICMDT(ModelPreview, modelpreview, file, "s", (const char *s), SETSTR(o->name, s));
    UICMDT(ModelPreview, modelpreview, anim, "s", (const char *s), o->setanim(s));
    UICMDT(ModelPreview, modelpreview, scale, "f", (float *n), o->mdl.size = *n);
    UICMDT(ModelPreview, modelpreview, skycol, "i", (int *c), o->skycol = vec::fromcolor(*c));
    UICMDT(ModelPreview, modelpreview, suncol, "i", (int *c), o->suncol = vec::fromcolor(*c));
    UICMDT(ModelPreview, modelpreview, sundir, "fff", (float *x, float *y, float *z), o->sundir = vec(*x, *y, *z));
    UICMDT(ModelPreview, modelpreview, excol, "i", (int *c), o->excol = vec::fromcolor(*c));
    UICMDT(ModelPreview, modelpreview, exdir, "fff", (float *x, float *y, float *z), o->exdir = vec(*x, *y, *z));
    UICMDT(ModelPreview, modelpreview, yaw, "f", (float *n), o->yaw = *n);
    UICMDT(ModelPreview, modelpreview, pitch, "f", (float *n), o->pitch = *n);
    UICMDT(ModelPreview, modelpreview, roll, "f", (float *n), o->roll = *n);
    UICMDT(ModelPreview, modelpreview, fov, "f", (float *n), o->fov = *n);
    UICMDT(ModelPreview, modelpreview, interact, "i", (int *c), o->interact = *c != 0);
    UICMDT(ModelPreview, modelpreview, resetoffset, "", (void), o->resetoffset());
    UICMDT(ModelPreview, modelpreview, colour, "fffg", (float *r, float *g, float *b, float *a), o->mdl.color = vec4(*r, *g, *b, *a >= 0 ? *a : 1.f));
    UICMDT(ModelPreview, modelpreview, basetime, "bb", (int *b, int *c), o->mdl.basetime = *b >= 0 ? *b : uilastmillis; o->mdl.basetime2 = *c >= 0 ? *c : 0);
    UICMDT(ModelPreview, modelpreview, material, "iiii", (int *mat, int *r, int *g, int *b), if(*mat >= 0 && *mat < MAXMDLMATERIALS) o->mdl.material[*mat] = bvec(*r, *g, *b));
    UICMDT(ModelPreview, modelpreview, materialcol, "ii", (int *mat, int *c), if(*mat >= 0 && *mat < MAXMDLMATERIALS) o->mdl.material[*mat] = bvec::fromcolor(*c));
    UICMDT(ModelPreview, modelpreview, effectparams, "ffff", (float *r, float *g, float *b, float *a), o->mdl.effectparams = vec4(*r, *g, *b, *a));
    UICMDT(ModelPreview, modelpreview, matbright, "ffff", (float *x, float *y, float *z, float *w), o->mdl.matbright = vec4(*x, *y, *z, *w));
    UICMDT(ModelPreview, modelpreview, matsplit, "f", (float *n), o->mdl.matsplit = clamp(*n, 0.0f, 0.5f));
    UICMDT(ModelPreview, modelpreview, mixer, "s", (const char *texname), o->mdl.mixer = textureload(texname, 3, true, false));
    UICMDT(ModelPreview, modelpreview, mixerscale, "f", (float *n), o->mdl.mixerscale = *n);

    struct PlayerPreview : Preview
    {
        float scale, blend;
        char *actions;
        matrix4 lastmatrix;

        PlayerPreview() : actions(NULL) { resetoffset(); }
        ~PlayerPreview() { delete[] actions; }

        void setup(float scale_, float blend_, float minw_, float minh_, const char *actions_, float yaw_ = -1, float pitch_ = -15, float roll_ = 0, float fov_ = 20, const vec &skycol_ = vec(0.1f, 0.1f, 0.1f), const vec &suncol_ = vec(0.6f, 0.6f, 0.6f), const vec &sundir_ = vec(0, -1, 2), const vec &excol_ = vec(0.f, 0.f, 0.f), const vec &exdir_ = vec(0, 0, 0))
        {
            Preview::setup(minw_, minh_, yaw_, pitch_, roll_,fov_, skycol_, suncol_, sundir_, excol_, exdir_);
            scale = scale_;
            blend = blend_;
            SETSTR(actions, actions_);
        }

        static const char *typestr() { return "#PlayerPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            if(!loadedshaders) { Object::draw(sx, sy); return; }

            setupdraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            bool hasclipstack = clipstack.length() > 0;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);

            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, pitch+offsetpitch, roll, fov, false, hasclipstack, translate);

            colors[0].val.a = uchar(colors[0].val.a*blend);
            game::renderplayerpreview(scale, colors[0].val.tocolor4(), actions, yaw, offsetyaw);
            if(hasclipstack) clipstack.last().scissor();
            // Steal the matrix for calculating positions on the model
            lastmatrix = camprojmatrix;

            modelpreview::end(renderfbo, skycol, suncol, sundir, excol, exdir);

            Object::draw(sx, sy);
        }

        vec2 vanityscreenpos(int vanity)
        {
            vec pos2d;
            lastmatrix.transform(game::playerpreviewvanitypos(vanity), pos2d);
            if(pos2d.z <= 0) return vec2(0, 0);

            pos2d.div(pos2d.z);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(lastsx, lastsy, lastsx+lastw, lastsy+lasth, sx1, sy1, sx2, sy2, false);

            pos2d.add(vec(1.0f, 1.0f, 0.0f)).mul(vec(0.5f, 0.5f, 0.0f));
            pos2d.y = 1.0f - pos2d.y;
            pos2d.mul(vec((sx2-sx1)/(float)hudw, (sy2-sy1)/(float)hudh, 0.0f));

            // Correct for uiaspect
            pos2d.x *= hudw/(float)hudh;

            return vec2(pos2d.x, pos2d.y);
        }

        vec vanitypos(int vanity) { return game::playerpreviewvanitypos(vanity, true); }
    };

    ICOMMAND(0, uiplayerpreview, "ffffse", (float *scale, float *blend, float *minw, float *minh, char *actions, uint *children),
        BUILD(PlayerPreview, o, o->setup(*scale, *blend, *minw*uiscale, *minh*uiscale, actions), children));

    UICMD(PlayerPreview, playerpreview, vanityscreenpos, "i", (int *vanity),
    {
        vec2 pos = o->vanityscreenpos(*vanity);
        defformatbigstring(str, "%f %f", pos.x, pos.y);
        result(str);
    });

    UICMD(PlayerPreview, playerpreview, vanitypos, "i", (int *vanity),
    {
        vec pos = o->vanitypos(*vanity);
        defformatbigstring(str, "%f %f %f", pos.x, pos.y, pos.z);
        result(str);
    });

    struct PrefabPreview : Preview
    {
        char *name;
        float blend;

        PrefabPreview() : name(NULL) { resetoffset(); }
        ~PrefabPreview() { delete[] name; }

        void setup(const char *name_, const Color &color_, float blend, float minw_, float minh_, float yaw_ = -1, float pitch_ = -15, float roll_ = 0, float fov_ = 20, const vec &skycol_ = vec(0.1f, 0.1f, 0.1f), const vec &suncol_ = vec(0.6f, 0.6f, 0.6f), const vec &sundir_ = vec(0, -1, 2), const vec &excol_ = vec(0.f, 0.f, 0.f), const vec &exdir_ = vec(0, 0, 0))
        {
            Preview::setup(minw_, minh_, yaw_, pitch_, roll_, fov_, skycol_, suncol_, sundir_, excol_, exdir_, color_);
            SETSTR(name, name_);
        }

        static const char *typestr() { return "#PrefabPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            if(!loadedshaders) return;

            setupdraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            bool hasclipstack = clipstack.length() > 0;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, pitch, roll, fov, false, hasclipstack);
            previewprefab(name, colors[0].val.tocolor(), blend*(colors[0].val.a/255.f), yaw, offsetyaw);
            if(hasclipstack) clipstack.last().scissor();
            modelpreview::end(renderfbo, skycol, suncol, sundir, excol, exdir);
        }
    };

    ICOMMAND(0, uiprefabpreview, "sifffe", (char *prefab, int *colour, float *blend, float *minw, float *minh, uint *children),
        BUILD(PrefabPreview, o, o->setup(prefab, Color(*colour), *blend, *minw*uiscale, *minh*uiscale), children));

    struct SlotViewer : Target
    {
        int index;

        void setup(int index_, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_, Color(colourwhite));
            index = index_;
        }

        static const char *typestr() { return "#SlotViewer"; }
        const char *gettype() const { return typestr(); }

        void previewslot(Slot &slot, VSlot &vslot, float x, float y)
        {
            if(!loadedshaders || slot.sts.empty()) return;
            VSlot *layer = NULL;
            Texture *t = NULL, *glowtex = NULL, *layertex = NULL, *detailtex = NULL;
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
            else
            {
                if(!slot.thumbnail)
                {
                    if(uiclockticks - lastthumbnail < uislotviewtime) t = textureload(uiloadtex);
                    else
                    {
                        slot.loadthumbnail();
                        lastthumbnail = uiclockticks;
                    }
                }
                if(slot.thumbnail && slot.thumbnail != notexture) t = slot.thumbnail;
            }

            if(!t || t == notexture) return;

            setupdraw(CHANGE_SHADER|(glowtex ? CHANGE_BLEND : 0));

            SETSHADER(hudrgb);
            vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
            int xoff = vslot.offset.x, yoff = vslot.offset.y;
            if(vslot.rotation)
            {
                const texrotation &r = texrotations[vslot.rotation];
                if(r.swapxy) { swap(xoff, yoff); loopk(4) swap(tc[k].x, tc[k].y); }
                if(r.flipx) { xoff *= -1; loopk(4) tc[k].x *= -1; }
                if(r.flipy) { yoff *= -1; loopk(4) tc[k].y *= -1; }
            }
            float xt = min(1.0f, t->xs/float(t->ys)), yt = min(1.0f, t->ys/float(t->xs));
            loopk(4) { tc[k].x = tc[k].x/xt - float(xoff)/t->xs; tc[k].y = tc[k].y/yt - float(yoff)/t->ys; }
            settexture(t);
            vec colorscale = vslot.getcolorscale();
            if(slot.loaded) gle::colorf(colorscale.x*colors[0].val.r/255.f, colorscale.y*colors[0].val.g/255.f, colorscale.z*colors[0].val.b/255.f, colors[0].val.a/255.f);
            else gle::colorf(1, 1, 1);
            quad(x, y, w, h, tc);
            if(detailtex)
            {
                settexture(detailtex);
                quad(x + w/2, y + h/2, w/2, h/2, tc);
            }
            if(glowtex)
            {
                int oldblendtype = blendtype;
                setblend(BLEND_GLOW, blendsep);
                settexture(glowtex);
                vec glowcolor = vslot.getglowcolor();
                gle::colorf(glowcolor.x*colors[0].val.r/255.f, glowcolor.y*colors[0].val.g/255.f, glowcolor.z*colors[0].val.b/255.f, colors[0].val.a/255.f);
                quad(x, y, w, h, tc);
                setblend(oldblendtype, blendsep);
            }
            if(layertex)
            {
                vec layerscale = layer->getcolorscale();
                settexture(layertex);
                gle::colorf(layerscale.x*colors[0].val.r/255.f, layerscale.y*colors[0].val.g/255.f, layerscale.z*colors[0].val.g/255.f, colors[0].val.a/255.f);
                quad(x, y, w/2, h/2, tc);
            }
        }

        void draw(float sx, float sy)
        {
            if(slots.inrange(index))
            {
                Slot &slot = lookupslot(index, false);
                previewslot(slot, *slot.variants, sx, sy);
            }

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uislotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(SlotViewer, o, o->setup(*index, *minw*uiscale, *minh*uiscale), children));

    struct VSlotViewer : SlotViewer
    {
        static const char *typestr() { return "#VSlotViewer"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            if(vslots.inrange(index))
            {
                VSlot &vslot = lookupvslot(index, false);
                previewslot(*vslot.slot, vslot, sx, sy);
            }

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uivslotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(VSlotViewer, o, o->setup(*index, *minw*uiscale, *minh*uiscale), children));

    struct DecalSlotViewer : SlotViewer
    {
        static const char *typestr() { return "#DecalSlotViewer"; }
        const char *gettype() const { return typestr(); }

        void previewslot(Slot &slot, VSlot &vslot, float x, float y)
        {
            if(!loadedshaders || slot.sts.empty()) return;
            Texture *t = NULL, *glowtex = NULL;
            if(slot.loaded)
            {
                t = slot.sts[0].t;
                if(t == notexture) return;
                Slot &slot = *vslot.slot;
                if(slot.texmask&(1<<TEX_GLOW)) { loopvj(slot.sts) if(slot.sts[j].type==TEX_GLOW) { glowtex = slot.sts[j].t; break; } }
            }
            else
            {
                if(!slot.thumbnail)
                {
                    if(uiclockticks - lastthumbnail < uislotviewtime) t = textureload(uiloadtex);
                    else
                    {
                        slot.loadthumbnail();
                        lastthumbnail = uiclockticks;
                    }
                }
                if(slot.thumbnail && slot.thumbnail != notexture) t = slot.thumbnail;
            }

            if(!t || t == notexture) return;

            setupdraw(CHANGE_SHADER|(glowtex ? CHANGE_BLEND : 0));

            SETSHADER(hudrgb);
            vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
            float xoff = float(vslot.offset.x)/t->xs, yoff = float(vslot.offset.y)/t->ys;
            float xt = min(1.0f, t->xs/float(t->ys)), yt = min(1.0f, t->ys/float(t->xs));

            xoff += (1.0f - xt) * 0.5f; yoff += (1.0f - yt) * 0.5f;

            if(vslot.rotation)
            {
                const texrotation &r = texrotations[vslot.rotation];
                if(r.swapxy) { swap(xoff, yoff); loopk(4) swap(tc[k].x, tc[k].y); }
                if(r.flipx) { xoff *= -1; loopk(4) tc[k].x *= -1; }
                if(r.flipy) { yoff *= -1; loopk(4) tc[k].y *= -1; }
            }
            loopk(4) { tc[k].x = (tc[k].x - xoff)/xt; tc[k].y = (tc[k].y - yoff)/yt; }
            settexture(t);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            vec colorscale = vslot.getcolorscale();
            if(slot.loaded) gle::colorf(colorscale.x*colors[0].val.r/255.f, colorscale.y*colors[0].val.g/255.f, colorscale.z*colors[0].val.b/255.f, colors[0].val.a/255.f);
            else gle::colorf(1, 1, 1, 1);
            quad(x, y, w, h, tc);
            if(glowtex)
            {
                setblend(BLEND_GLOW, blendsep);
                settexture(glowtex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                vec glowcolor = vslot.getglowcolor();
                gle::colorf(glowcolor.x*colors[0].val.r/255.f, glowcolor.y*colors[0].val.g/255.f, glowcolor.z*colors[0].val.b/255.f, colors[0].val.a/255.f);
                quad(x, y, w, h, tc);
            }
        }

        void draw(float sx, float sy)
        {
            if(decalslots.inrange(index))
            {
                DecalSlot &slot = lookupdecalslot(index, false);
                previewslot(slot, *slot.variants, sx, sy);
            }

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uidecalslotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(DecalSlotViewer, o, o->setup(*index, *minw*uiscale, *minh*uiscale), children));

    struct MiniMap : Target
    {
        enum { ELLIPSE = 0, SQUARE };

        Texture *tex;
        float dist, border;
        int shape;

        void setup(Texture *tex_, const Color &color_, float dist_ = 0, float border_ = 0.05f, float minw_ = 0, float minh_ = 0, int shape_ = ELLIPSE)
        {
            Target::setup(minw_, minh_, color_);
            colors.add(Color(colourwhite));
            tex = tex_;
            dist = dist_;
            border = border_;
            shape = shape_;
        }

        void setup(Texture *tex_, const Color &color_, const Color &color2_, float dist_ = 0, float border_ = 0.05f, float minw_ = 0, float minh_ = 0, int shape_ = ELLIPSE)
        {
            Target::setup(minw_, minh_, color_);
            colors.add(color2_); // minimap version
            tex = tex_;
            dist = dist_;
            border = border_;
            shape = shape_;
        }

        static const char *typestr() { return "#MiniMap"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        void draw(float sx, float sy)
        {
            setupdraw();
            while(colors.length() < 2) colors.add(Color(colourwhite));

            if(hud::needminimap())
            {
                Shader *oldshader = Shader::lastshader;
                SETSHADER(hudminimap);

                LOCALPARAMF(minimapparams, 2<<(minimapsize-1));

                vec pos = vec(camera1->o).sub(minimapcenter).mul(minimapscale).add(0.5f), dir(camera1->yaw * RAD, 0.f);
                int limit = hud::radarlimit();
                float scale = min(dist > 0 ? dist : float(worldsize), limit > 0 ? limit : float(worldsize)),
                      qw = w * 0.25f * border, qh = h * 0.25f * border, rw = w * 0.5f - qw, rh = h * 0.5f - qh;

                colors[1].init(game::darkness(DARK_UI));
                gle::defvertex(2);
                gle::deftexcoord0();

                if(shape == SQUARE)
                {
                    gle::begin(GL_QUADS);
                    bindminimap();

                    const int corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                    loopi(4)
                    {
                        float angle = fmod(camera1->yaw + 180, 360) * RAD,
                              rx = corners[i][0] * cos(angle) - corners[i][1] * sin(angle),
                              ry = corners[i][0] * sin(angle) + corners[i][1] * cos(angle);

                        gle::attribf(sx + qw + rw * (1.0f + corners[i][0]), sy + qh + rh * (1.0f + corners[i][1]));
                        gle::attribf(1.0f - (pos.x + rx * scale * minimapscale.x), pos.y + ry * scale * minimapscale.y);
                    }

                    gle::end();
                }
                else
                {
                    gle::begin(GL_TRIANGLE_FAN);
                    bindminimap();

                    loopi(32)
                    {
                        vec v = vec(0, -1, 0).rotate_around_z(i / 32.0f * 2 * M_PI);
                        gle::attribf(sx + qw + rw * (1.0f + v.x), sy + qh + rh * (1.0f + v.y));
                        vec tc = vec(dir).rotate_around_z(i / 32.0f * 2 * M_PI);
                        gle::attribf(1.0f - (pos.x + tc.x * scale * minimapscale.x), pos.y + tc.y * scale * minimapscale.y);
                    }

                    gle::end();
                }

                if(oldshader) oldshader->set();
            }

            if(tex != notexture)
            {
                colors[0].init();
                gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_QUADS);
                settexture(tex);
                quads(sx, sy, w, h);
                gle::end();
            }

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiminimap, "siffffe", (char *texname, int *c, float *dist, float *border, float *minw, float *minh, uint *children),
        BUILD(MiniMap, o, o->setup(textureload(texname, 0x7000, true, false, texgc), Color(*c), *dist, *border, *minw*uiscale, *minh*uiscale), children));
    ICOMMAND(0, uiminimapcolour, "siiffffe", (char *texname, int *c, int *c2, float *dist, float *border, float *minw, float *minh, uint *children),
        BUILD(MiniMap, o, o->setup(textureload(texname, 0x7000, true, false, texgc), Color(*c), Color(*c2), *dist, *border, *minw*uiscale, *minh*uiscale), children));

    UIARG(MiniMap, minimap, dist, "f", float, 0.0f, FVAR_MAX);
    UIARG(MiniMap, minimap, border, "f", float, 0.0f, 1.0f);
    UIARG(MiniMap, minimap, shape, "i", int, MiniMap::ELLIPSE, MiniMap::SQUARE);

    struct Radar : Target
    {
        enum { ELLIPSE = 0, SQUARE, BEARING };

        float dist, offset, border, maxdist;
        int shape;

        void setup(float dist_ = 1, float offset_ = 0, float border_ = 0, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_);
            dist = dist_;
            offset = clamp(offset_, 0.f, 1.f);
            border = clamp(border_, 0.f, 1.f);
            maxdist = 0.0f;
            shape = ELLIPSE;
        }
        bool isradar() const { return true; }

        static const char *typestr() { return "#Radar"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiradar, "fffffe", (float *dist, float *offset, float *border, float *minw, float *minh, uint *children),
        BUILD(Radar, o, o->setup(*dist, *offset, *border, *minw*uiscale, *minh*uiscale), children));

    UIARG(Radar, radar, dist, "f", float, 0.0f, FVAR_MAX);
    UIARG(Radar, radar, maxdist, "f", float, 0.0f, FVAR_MAX);
    UIARG(Radar, radar, border, "f", float, 0.0f, 1.0f);
    UIARG(Radar, radar, offset, "f", float, 0.0f, 1.0f);
    UIARG(Radar, radar, shape, "i", int, Radar::ELLIPSE, Radar::BEARING);

    void radarfade(Object *o, float alpha)
    {
        if(o->iscolour())
        {
            Colored *c = (Colored *)o;
            loopv(c->colors) c->colors[i].val.a = uchar(c->colors[i].val.a * alpha);
        }
        loopv(o->children) radarfade(o->children[i], alpha);
    }

    struct RadarBlip : Image
    {
        float yaw, blipx, blipy, texx, texy, dist, heading, maxdist;
        uchar blipadjust;
        int priority;

        void setup(Texture *tex_, const Color &color_, float yaw_ = 0, float heading_ = 0, float dist_ = 0, float minw_ = 0, float minh_ = 0)
        {
            Image::setup(tex_, color_, true, minw_, minh_);
            yaw = yaw_; // direction in which the blip is
            heading = heading_; // rotation of the blip itself
            dist = dist_; // how far away the blip is
            blipx = blipy = texx = texy = maxdist = 0;
            blipadjust = ALIGN_DEFAULT;
            priority = 0;
        }

        static const char *typestr() { return "#RadarBlip"; }
        const char *gettype() const { return typestr(); }

        Radar *getradar()
        {
            for(Object *o = parent; o != NULL; o = o->parent)
            {
                if(o->isradar()) return (Radar *)o;
                if(o->istype<Window>()) break;
            }
            return NULL;
        }

        void layout()
        {
            blipx = sinf(RAD*yaw);
            blipy = -cosf(RAD*yaw);

            texx = sinf(RAD*heading);
            texy = -cosf(RAD*heading);

            Image::layout();

            // children don't increase the dimensions of a blip if specified
            if(minw > 0 && w > minw) w = minw;
            if(minh > 0 && h > minh) h = minh;
        }

        void setalign(int xalign, int yalign)
        {
            adjust = ALIGN_DEFAULT; // always align center and use our own adjustment

            blipadjust &= ~ALIGN_MASK;
            blipadjust |= (clamp(xalign, -2, 1)+2)<<ALIGN_HSHIFT;
            blipadjust |= (clamp(yalign, -2, 1)+2)<<ALIGN_VSHIFT;
        }

        void setclamp(int left, int right, int top, int bottom)
        {
            adjust &= ~CLAMP_MASK; // clamping to the parent radar would be silly
        }

        bool target(float cx, float cy)
        {
            return true;
        }

        void draw(float sx, float sy)
        {
            Radar *r = getradar();
            if(r)
            {
                if(dist > r->dist && !(priority&1))
                {
                    float md = max(r->maxdist, maxdist);

                    if(md > r->dist)
                    {
                        if(dist < md)
                            radarfade(this, 1.0f - clamp((dist - r->dist) / (md - r->dist), 0.0f, 1.0f));
                        else return;
                    }
                    else if(md > 0.0f) return;
                }

                float bb = priority&8 ? 0.0f : r->border,
                      bw = w * 0.5f, bh = h * 0.5f, rw = r->w * 0.5f, rh = r->h * 0.5f, rd = max(r->dist, 1.f),
                      rx = 0, ry = 0, fw = rw * bb, fh = rh * bb, gw = rw - fw, gh = rh - fh;

                switch(r->shape)
                {
                    case Radar::BEARING:
                    {
                        if(blipy > 0.0f)
                        {
                            if(!(priority&2)) return;
                            if(!(priority&4)) radarfade(this, 0.5f);
                            rx = blipx > 0.0f ? gw : -gw;
                        }
                        else
                        {
                            if(!(priority&4))
                            {
                                float fade = fabs(blipx);
                                if(priority&2) fade *= 0.5f;
                                radarfade(this, 1.0f - fade);
                            }
                            rx = clamp(blipx * gw, -gw, gw);
                        }
                        break;
                    }

                    case Radar::SQUARE:
                    {
                        rx = clamp(blipx * gw * dist / rd, -gw, gw);
                        ry = clamp(blipy * gh * dist / rd, -gh, gh);
                        break;
                    }

                    default:
                    {
                        rx = blipx * gw * clamp(dist / rd, 0.f, 1.f);
                        ry = blipy * gh * clamp(dist / rd, 0.f, 1.f);
                        break;
                    }
                }

                if(r->shape != Radar::BEARING)
                {
                    float mx = rx, my = ry;
                    if(mx != 0 || my != 0)
                    {
                        float ds = sqrtf(mx*mx + my*my);
                        if(ds > 0)
                        {
                            mx /= ds;
                            my /= ds;
                        }
                    }

                    mx *= r->offset * gw;
                    my *= r->offset * gh;

                    rx += sx + mx;
                    ry += sy + my;
                }
                else
                {
                    rx += sx;
                    ry += sy;
                }

                vec2 anrm(0, 0);
                switch(blipadjust&ALIGN_HMASK)
                {
                    case ALIGN_LEFT:    anrm.x = -1; break;
                    case ALIGN_RIGHT:   anrm.x = 1; break;
                }

                switch(blipadjust&ALIGN_VMASK)
                {
                    case ALIGN_TOP:     anrm.y = 1; break;
                    case ALIGN_BOTTOM:  anrm.y = -1; break;
                }

                if(!anrm.iszero())
                { // adjust the alignment of the blip taking into account its rotation
                    anrm.normalize().mul(vec2(bw, r->shape != Radar::BEARING ? bh : gh - bh));

                    if(r->shape != Radar::BEARING) anrm.rotate_around_z(yaw * RAD);
                    else anrm.mul(-1);

                    rx += anrm.x;
                    ry += anrm.y;
                }

                float bbx = blipx, bby = blipy;
                if(tex != notexture)
                {
                    bindtex();

                    bbx = texx;
                    bby = texy;

                    loopk(4)
                    {
                        vec2 norm(0, 0);
                        float tx = 0, ty = 0;
                        switch(k)
                        {
                            case 0: vecfromyaw(heading, -1, -1, norm);  tx = 0; ty = 0; break;
                            case 1: vecfromyaw(heading, -1, 1, norm);   tx = 1; ty = 0; break;
                            case 2: vecfromyaw(heading, 1, 1, norm);    tx = 1; ty = 1; break;
                            case 3: vecfromyaw(heading, 1, -1, norm);   tx = 0; ty = 1; break;
                        }
                        norm.mul(vec2(bw, bh)).add(vec2(rx + bw, ry + bh));

                        gle::attrib(norm);
                        gle::attribf(tx, ty);
                    }
                }
                Object::draw(rx + bbx * w * RAD, ry + bby * h * RAD); // don't descend unless we process the blip
            }
        }
    };

    ICOMMAND(0, uiradarblip, "sifffffe", (char *texname, int *c, float *yaw, float *heading, float *dist, float *minw, float *minh, uint *children),
        BUILD(RadarBlip, o, o->setup(texname && texname[0] ? textureload(texname, 3, true, false, texgc) : NULL, Color(*c), *yaw, *heading, *dist, *minw*uiscale, *minh*uiscale), children));

    UIARG(RadarBlip, radarblip, priority, "i", int, 0, 15);
    UIARG(RadarBlip, radarblip, yaw, "f", float, FVAR_MIN, FVAR_MAX);
    UIARG(RadarBlip, radarblip, dist, "f", float, 0.0f, FVAR_MAX);
    UIARG(RadarBlip, radarblip, heading, "f", float, FVAR_MIN, FVAR_MAX);
    UIARG(RadarBlip, radarblip, maxdist, "f", float, 0.0f, FVAR_MAX);

    #define IFSTATEVAL(state,t,f) { if(state) { if(t->type == VAL_NULL) intret(1); else result(*t); } else if(f->type == VAL_NULL) intret(0); else result(*f); }
    #define DOSTATE(chkflags, func) \
        ICOMMANDNS(0, "ui!" #func, uinot##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->hasstate(chkflags) ? t : f)); \
        ICOMMANDNS(0, "ui" #func, ui##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->haschildstate(chkflags) ? t : f)); \
        ICOMMANDNS(0, "ui!" #func "?", uinot##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->hasstate(chkflags), t, f)); \
        ICOMMANDNS(0, "ui" #func "?", ui##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->haschildstate(chkflags), t, f)); \
        ICOMMANDNS(0, "ui!" #func "+", uinextnot##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->children.inrange(buildparent->buildchild) && buildparent->children[buildparent->buildchild]->hasstate(chkflags) ? t : f)); \
        ICOMMANDNS(0, "ui" #func "+", uinext##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->children.inrange(buildparent->buildchild) && buildparent->children[buildparent->buildchild]->haschildstate(chkflags) ? t : f)); \
        ICOMMANDNS(0, "ui!" #func "+?", uinextnot##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->children.inrange(buildparent->buildchild) && buildparent->children[buildparent->buildchild]->hasstate(chkflags), t, f)); \
        ICOMMANDNS(0, "ui" #func "+?", uinext##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->children.inrange(buildparent->buildchild) && buildparent->children[buildparent->buildchild]->haschildstate(chkflags), t, f));
    DOSTATES
    #undef DOSTATE

    ICOMMANDNS(0, "uianyfocus", uifocus_, "", (), intret(textfocus != NULL));

    #define TEXTEDITOR(chk, obj) TextEditor *e = chk && obj->iseditor() ? (TextEditor *)obj : NULL
    #define TEXTEDITTF e && e->edit && e->edit == textfocus

    ICOMMANDNS(0, "uifocus", uifocus_, "ee", (uint *t, uint *f),
    {
        TEXTEDITOR(buildparent, buildparent);
        executeret(TEXTEDITTF ? t : f);
    });
    ICOMMANDNS(0, "uifocus?", uifocus__, "tt", (tagval *t, tagval *f),
    {
        TEXTEDITOR(buildparent, buildparent);
        IFSTATEVAL(TEXTEDITTF, t, f)
    });
    ICOMMANDNS(0, "uifocus+", uinextfocus_, "ee", (uint *t, uint *f),
    {
        TEXTEDITOR(buildparent && buildparent->children.inrange(buildparent->buildchild), buildparent->children[buildparent->buildchild]);
        executeret(TEXTEDITTF ? t : f);
    });
    ICOMMANDNS(0, "uifocus+?", uinextfocus__, "tt", (tagval *t, tagval *f),
    {
        TEXTEDITOR(buildparent && buildparent->children.inrange(buildparent->buildchild), buildparent->children[buildparent->buildchild]);
        IFSTATEVAL(TEXTEDITTF, t, f)
    });
    ICOMMANDNS(0, "uifocus-", uinextfocus_, "ee", (uint *t, uint *f),
    {
        TEXTEDITOR(buildparent && buildparent->buildchild > 0 && buildparent->children.inrange(buildparent->buildchild-1), buildparent->children[buildparent->buildchild-1]);
        executeret(TEXTEDITTF ? t : f);
    });
    ICOMMANDNS(0, "uifocus-?", uinextfocus__, "tt", (tagval *t, tagval *f),
    {
        TEXTEDITOR(buildparent && buildparent->buildchild > 0 && buildparent->children.inrange(buildparent->buildchild-1), buildparent->children[buildparent->buildchild-1]);
        IFSTATEVAL(TEXTEDITTF, t, f);
    });

    ICOMMAND(0, uiprev, "e", (uint *code),
    {
        if(!buildparent || buildparent->buildchild <= 0) return;
        Object *oldparent = buildparent;
        buildparent = oldparent->children[oldparent->buildchild-1];
        executeret(code);
        buildparent = oldparent;
    });

    ICOMMAND(0, uipropchild, "e", (uint *code),
    {
        if(!buildparent) return;
        bool oldprop = propagating;
        propagating = true;
        Object *oldparent = buildparent;
        loopv(oldparent->children)
        {
            buildparent = oldparent->children[i];
            executeret(code);
        }
        propagating = oldprop;
        buildparent = oldparent;
    });

    void uichildren(uint *code)
    {
        if(!buildparent) return;
        bool oldprop = propagating;
        propagating = true;
        executeret(code);
        Object *oldparent = buildparent;
        loopv(oldparent->children)
        {
            buildparent = oldparent->children[i];
            uichildren(code);
        }
        propagating = oldprop;
        buildparent = oldparent;
    }
    ICOMMAND(0, uipropagate, "e", (uint *code), uichildren(code));

    ICOMMAND(0, uiproproot, "e", (uint *code),
    {
        if(!buildparent) return;
        Object *oldparent = buildparent;
        buildparent = window;
        uichildren(code);
        buildparent = oldparent;
    });

    ICOMMAND(0, uialign, "ii", (int *xalign, int *yalign), if(buildparent) buildparent->setalign(*xalign, *yalign));
    ICOMMAND(0, uiclamp, "iiii", (int *left, int *right, int *top, int *bottom), if(buildparent) buildparent->setclamp(*left, *right, *top, *bottom));
    ICOMMAND(0, uiposition, "ff", (float *x, float *y), if(buildparent) buildparent->setpos(*x, *y));

    #define UIGETFVAL(vname) \
        ICOMMAND(0, ui##vname, "N$", (int *numargs, ident *id), { \
            if(*numargs != 0) floatret(buildparent ? buildparent->vname : 0.f); \
            else printvar(id, buildparent ? buildparent->vname : 0.f); \
        }); \
        ICOMMAND(0, ui##vname##prev, "N$", (int *numargs, ident *id), { \
            if(*numargs != 0) floatret(buildparent && buildparent->buildchild > 0 ? buildparent->children[buildparent->buildchild-1]->vname : 0.f); \
            else printvar(id, buildparent && buildparent->buildchild > 0 ? buildparent->children[buildparent->buildchild-1]->vname : 0.f); \
        });

    UIGETFVAL(lastx);
    UIGETFVAL(lasty);
    UIGETFVAL(lastsx);
    UIGETFVAL(lastsy);
    UIGETFVAL(lastw);
    UIGETFVAL(lasth);

    int hasinput(bool cursor, int stype)
    {
        int ret = 0;
        SWSURFACE(stype,
        {
            int val = surface->allowinput(cursor);
            ret = max(val, ret);
        });
        return ret;
    }

    ICOMMANDV(0, uihasinput, hasinput());
    ICOMMAND(0, uigetinput, "ib", (int *cursor, int *stype), intret(hasinput(*cursor != 0, *stype >= 0 ? *stype : -1)));

    bool hasmenu(bool pass, int stype)
    {
        bool ret = false;
        SWSURFACE(stype, if(surface->hasmenu(pass)) ret = true);
        return ret;
    }

    ICOMMANDV(0, uihasmenu, hasmenu());
    ICOMMAND(0, uigetmenu, "ib", (int *pass, int *stype), intret(hasmenu(*pass != 0, *stype >= 0 ? *stype : -1)));

    bool keypress(int code, bool isdown)
    {
        if(!pushsurface(surfaceinput)) return false;

        if(surface->rawkey(code, isdown))
        {
            popsurface();
            return true;
        }

        int action = 0, hold = 0;
        switch(code)
        {
            case SDLK_ESCAPE: action = isdown ? STATE_ESC_PRESS : STATE_ESC_RELEASE; hold = STATE_ESC_HOLD; break;
            case -1: action = isdown ? STATE_PRESS : STATE_RELEASE; hold = STATE_HOLD; break;
            case -2: action = isdown ? STATE_ESC_PRESS : STATE_ESC_RELEASE; hold = STATE_ESC_HOLD; break;
            case -3: action = isdown ? STATE_ALT_PRESS : STATE_ALT_RELEASE; hold = STATE_ALT_HOLD; break;
            case -4: action = STATE_SCROLL_UP; break;
            case -5: action = STATE_SCROLL_DOWN; break;
        }

        int setmode = inputsteal && action != STATE_SCROLL_UP && action != STATE_SCROLL_DOWN ? SETSTATE_FOCUSED : SETSTATE_INSIDE;

        if(action)
        {
            if(isdown)
            {
                if(hold) surface->clearstate(hold);
                if(surface->setstate(action, getuicursorx(false), getuicursory(), 0, setmode, action|hold))
                {
                    popsurface();
                    return true;
                }
            }
            else if(hold)
            {
                if(surface->setstate(action, getuicursorx(false), getuicursory(), hold, setmode, action))
                {
                    surface->clearstate(hold);
                    popsurface();
                    return true;
                }
                else surface->clearstate(hold);
            }
        }

        bool ret = surface->key(code, isdown);
        popsurface();

        return ret;
    }

    bool textinput(const char *str, int len)
    {
        if(!pushsurface(surfaceinput)) return false;

        bool ret = surface->textinput(str, len);

        popsurface();

        return ret;
    }

    void closemapuis(int n, int stype)
    {
        if(n < 0) return;

        loopi(SURFACE_LOOP)
        {
            if(i == stype || !pushsurface(i)) continue;

            enumerate(surface->windows, Window *, w,
            {
                if(w->args[0].getint() != n || strncmp(w->name, "entity_", 7)) continue;
                surface->hide(w);
            });

            popsurface();
        }
    }

    void checkmapuis()
    {
        int oldflags = identflags;
        identflags |= IDF_MAP;

        vector<extentity *> &ents = entities::getents();
        loopenti(ET_MAPUI)
        {
            extentity &e = *ents[i];
            if(e.type != ET_MAPUI || !entities::isallowed(e)) continue;

            physent *player = (physent *)game::focusedent(true);
            if(!player) player = camera1;
            bool inside = e.attrs[4] > 0 && e.attrs[1]&MAPUI_PROXIMITY ? player->o.dist(e.o) <= e.attrs[4] : true;

            if(!inside && e.attrs[1]&MAPUI_SHOWPROX) continue;

            int stype = SURFACE_WORLD;
            if(e.attrs[1]&MAPUI_VISOR) stype = SURFACE_VISOR;
            else if(e.attrs[1]&MAPUI_BACKGROUND) stype = SURFACE_BACKGROUND;
            else if(e.attrs[1]&MAPUI_FOREGROUND) stype = SURFACE_FOREGROUND;

            vec pos = e.o;
            float yaw = 0, pitch = 0;
            if(entities::getdynamic(e, pos, &yaw, &pitch))
            {
                if(e.attrs[2] >= 0)
                {
                    yaw += e.attrs[2];
                    if(yaw < 0.0f) yaw = 360.0f - fmodf(-yaw, 360.0f);
                    else if(yaw >= 360.0f) yaw = fmodf(yaw, 360.0f);
                }

                if(e.attrs[3] <= 180 || e.attrs[3] >= -180)
                {
                    pitch += e.attrs[3];
                    if(pitch < -180.0f) pitch = 180.0f - fmodf(-180.0f - pitch, 360.0f);
                    else if(pitch >= 180.0f) pitch = fmodf(pitch + 180.0f, 360.0f) - 180.0f;
                }
            }
            else
            {
                yaw = e.attrs[2] >= 0 ? e.attrs[2] : 0;
                pitch = e.attrs[3] <= 180 && e.attrs[3] >= -180 ? e.attrs[3] : 0;
            }

            defformatstring(name, "entity_%s%d", e.attrs[0] < 0 ? "builtin_" : "", abs(e.attrs[0]));
            if(UI::pokeui(name, stype, i, pos, uimapmaxdist, yaw, pitch, e.attrs[5] != 0 ? e.attrs[5]/100.f : 1.f, e.attrs[6] > 0 ? e.attrs[6] : 0.f, e.attrs[7] > 0 ? e.attrs[7] : 0.f))
            {
                if(pushsurface(stype))
                {
                    Window *w = dynuirefwin(name, i);
                    if(w) w->allowinput = inside && (e.attrs[1]&MAPUI_INPUTPROX) != 0 ? 1 : 0;
                    popsurface();
                }
            }
        }

        identflags = oldflags;
    }

    #define COMPOSITESIZE (1<<9) // xs/ys scale
    extern void reloadcomp();
    VARF(IDF_PERSIST, compositesize, 1<<1, COMPOSITESIZE, 1<<12, reloadcomp());
    VAR(IDF_PERSIST, compositeuprate, 0, 16, VAR_MAX); // limit updates to this ms
    VAR(IDF_PERSIST, compositeruncount, 0, 2, VAR_MAX); // limit updates to this count per cycle
    VAR(IDF_PERSIST, compositerewind, 0, 1, 1); // rewind if over time limit

    GLenum compformat(int format = -1)
    {
        switch(format)
        {
            case 1: return GL_RED;
            case 2: return GL_RG;
            case 3: return GL_RGB;
            case 4: return GL_RGBA;
            default: break;
        }

        return GL_RGBA;
    }

    int formatcomp(GLenum format)
    {
        switch(format)
        {
            case GL_RED: return 1;
            case GL_RG: return 2;
            case GL_RGB: return 3;
            case GL_RGBA: return 4;
        }

        return 4;
    }

    Texture *composite(const char *name, int tclamp, bool mipit, bool msg, bool gc, Texture *tex, bool reload)
    {
        if(!name || !*name || !pushsurface(SURFACE_COMPOSITE))
        {
            if(msg) conoutf(colourred, "Cannot create null composite texture: %s", name);
            popsurface();
            return notexture; // need a name
        }

        Texture *t = tex ? tex : textures.access(name);
        if(t && !reload)
        {
            // Strip GC flag with gc=false
            if(!gc && t->type&Texture::GC) t->type &= ~Texture::GC;
            popsurface();
            return t;
        }

        const char *cmds = NULL, *file = name;
        if(name && name[0] == '<')
        {
            cmds = name;
            file = strchr(name, '>');
            if(file) file++;
            if(!file || !*file)
            {
                if(msg) conoutf(colourred, "Cannot create null composite texture: %s", name);
                popsurface();
                return notexture; // need a name
            }
        }

        bool iscomposite = false;
        float ssize = 0;
        int delay = 0, bpp = -1;
        if(cmds && *cmds == '<')
        {
            #define COMPNUMARGS 3
            const char *cmd = NULL, *end = NULL, *arg[COMPNUMARGS] = { NULL, NULL, NULL };
            cmd = &cmds[1];
            end = strchr(cmd, '>');
            if(end)
            {
                size_t len = strcspn(cmd, ":,><");
                loopi(COMPNUMARGS)
                {
                    arg[i] = strchr(i ? arg[i-1] : cmd, i ? ',' : ':');
                    if(!arg[i] || arg[i] >= end) arg[i] = "";
                    else arg[i]++;
                }
                if(matchstring(cmd, len, "comp"))
                {
                    if(*arg[0]) delay = max(atoi(arg[0]), 0);
                    if(*arg[1]) ssize = atof(arg[1]);
                    if(*arg[2]) bpp = clamp(atoi(arg[2]), 0, 4);
                    iscomposite = true;
                }
            }
            #undef COMPNUMARGS
        }

        if(!iscomposite)
        {
            if(msg) conoutf(colourred, "Not a composite texture: %s", name);
            popsurface();
            return notexture;
        }

        vector<char *> list;
        explodelist(file, list, 5); // name [delay] args [size] [bpp]

        if(list.empty()) // need at least the name
        {
            list.deletearrays();
            if(msg) conoutf(colourred, "Could not composite texture: %s", name);
            popsurface();
            return notexture;
        }

        int argidx = 1;
        if(list.length() >= 2 && isdigit(list[1][0]))
        { // Import from old style
            argidx = 2;
            delay = max(atoi(list[1]), 0);
            if(list.length() >= 4) ssize = atof(list[3]);
            if(list.length() >= 5) bpp = clamp(atoi(list[3]), 0, 4);
        }

        char *cname = list[0], *args = list.length() >= (argidx + 1) ? list[argidx] : NULL;
        int tsize = ssize >= 0 ? int(ssize * compositesize) : abs(int(ssize));
        if(tsize <= 0) tsize = compositesize;
        else if(tsize < 1<<1) tsize = 1<<1;

        if(msg) progress(0, "Compositing texture: %s (%s)", cname, args && *args ? args : "-");

        Window *w = surface->windows.find(cname, NULL);
        if(!w)
        {
            if(msg) conoutf(colourred, "Failed to locate composite UI: %s (%s)", cname, name);
            list.deletearrays();
            popsurface();
            return notexture;
        }

        GLint oldfbo = renderfbo; // necessary as a texture can load at pretty much any point in the frame
        poke(true);

        GLERROR;
        GLuint fbo = t ? t->fbo : 0;
        if(fbo)
        {
            glDeleteFramebuffers_(1, &fbo);
            fbo = 0;
        }
        glGenFramebuffers_(1, &fbo);
        glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
        renderfbo = fbo;

        GLenum format = compformat(bpp);
        GLuint id = t ? t->id : 0;
        if(id)
        {
            glDeleteTextures(1, &id);
            id = 0;
        }
        glGenTextures(1, &id);
        createtexture(id, tsize, tsize, NULL, tclamp, mipit ? 3 : 0, format, GL_TEXTURE_2D, 0, 0, 0, true, format, true);

        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);

        GLERROR;
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            if(msg) conoutf(colourred, "Failed allocating composite texture framebuffer: %s [%u / %u]", name, id, fbo);
            if(id) glDeleteTextures(1, &id);
            if(fbo) glDeleteFramebuffers_(1, &fbo);
            glBindFramebuffer_(GL_FRAMEBUFFER, oldfbo);
            renderfbo = oldfbo;

            list.deletearrays();
            popsurface();

            return notexture;
        }

        glViewport(0, 0, tsize, tsize);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        if(!t)
        {
            char *key = newstring(name);
            t = &textures[key];
            t->name = key;
            t->comp = newstring(cname);
            t->args = args ? newstring(args) : NULL;
        }
        t->type = Texture::IMAGE | Texture::COMPOSITE | Texture::ALPHA;
        t->tclamp = tclamp;
        t->mipmap = mipit;
        if(gc) t->type |= Texture::GC;
        if(t->tclamp&0x300) t->type |= Texture::MIRROR;
        t->w = t->h = tsize;
        t->xs = t->ys = ssize >= 0 ? int(t->w * (COMPOSITESIZE / float(t->w))) : t->w;
        t->bpp = formatsize(format);
        t->format = format;
        t->delay = delay;
        t->id = id;
        t->fbo = fbo;
        t->used = t->last = uiclockticks;

        bool hastex = false;
        loopv(surface->texs)
        {
            if(surface->texs[i] != t) continue;
            hastex = true;
            break;
        }
        if(!hastex) surface->texs.add(t);

        if(t->mipmap)
        {
            glActiveTexture_(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, t->id);
            glGenerateMipmap_(GL_TEXTURE_2D);
        }

        glBindFramebuffer_(GL_FRAMEBUFFER, oldfbo);
        renderfbo = oldfbo;
        glViewport(0, 0, hudw, hudh);

        if(msg) progress(1, "Compositing texture: %s (%s)", cname, args && *args ? args : "-");

        list.deletearrays();
        popsurface();

        return t;
    }

    void reloadcomp()
    {
        enumerate(textures, Texture, t,
        {
            if(!(t.type&Texture::COMPOSITE)) continue;
            composite(t.name, t.tclamp, t.mipmap, true, t.type&Texture::GC, &t, true);
            t.rendered = 0;
        });
    }

    void mousetrack(float dx, float dy)
    {
        loopi(SURFACE_MAX) if(surfaces[i]) surfaces[i]->mousetrackvec.add(vec2(dx, dy));
    }

    bool cursorlock()
    {
        if(surfaces[surfaceinput]) return surfaces[surfaceinput]->lockcursor;
        return false;
    }

    int cursortype()
    {
        if(surfaces[surfaceinput]) return surfaces[surfaceinput]->cursortype;
        return CURSOR_DEFAULT;
    }

    int savemap(stream *h)
    {
        int mapmenus = 0;
        loopj(SURFACE_MAX)
        {
            if(j == SURFACE_PROGRESS || !pushsurface(j)) continue;

            enumerate(surface->windows, Window *, w,
            {
                if(!w->mapdef || !w->contents || w->dyn) continue;

                h->printf("mapui %s %d [%s]", w->name, surfacetype, w->contents->body);

                if(w->onshow)
                    h->printf(" [%s]", w->onshow->body);
                else if(w->onhide || w->vistest || w->forcetest) h->printf(" []");

                if(w->onhide)
                    h->printf(" [%s]", w->onhide->body);
                else if(w->vistest || w->forcetest) h->printf(" []");

                if(w->vistest)
                    h->printf(" [%s]", w->vistest->body);
                else if(w->forcetest) h->printf(" []");

                if(w->forcetest)
                    h->printf(" [%s]", w->forcetest->body);

                h->printf("\n\n");

                mapmenus++;
            });

            popsurface();
        }

        loopv(dynuis)
        {
            DynUI *d = dynuis[i];
            if(!d->mapdef || !d->contents) continue;

            h->printf("mapdynui %s [%s]", d->name, d->contents);

            if(d->onshow)
                h->printf(" [%s]", d->onshow);
            else if(d->onhide || d->vistest || d->forcetest) h->printf(" []");

            if(d->onhide)
                h->printf(" [%s]", d->onhide);
            else if(d->vistest || d->forcetest) h->printf(" []");

            if(d->vistest)
                h->printf(" [%s]", d->vistest);
            else if(d->forcetest) h->printf(" []");

            if(d->forcetest)
                h->printf(" [%s]", d->forcetest);

            h->printf("\n\n");

            mapmenus++;
        }

        return mapmenus;
    }

    void resetmap()
    {
        loopj(SURFACE_MAX)
        {
            if(j == SURFACE_PROGRESS || !pushsurface(j)) continue;
            if(j == SURFACE_COMPOSITE) loopvrev(surface->texs)
            {
                Texture *t = surface->texs[i];
                if(!t->rendered) continue;
                Window *w = surface->windows.find(t->comp, NULL);
                if(!w || !w->mapdef) continue;

                surface->texs.remove(i);
                t->type |= Texture::GC;
                cleanuptexture(t);
            }
            enumerate(surface->windows, Window *, w,
            {
                if(!w->mapdef) continue;
                surface->hide(w);
                surface->windows.remove(w->name);
                delete w;
            });
            popsurface();
        }

        loopvrev(dynuis)
        {
            DynUI *d = dynuis[i];
            if(!d->mapdef) continue;
            dynuis.remove(i);
            delete d;
        }
    }

    void setup()
    {
        loopi(MAXARGS)
        {
            defformatstring(argname, "uiarg%d", i+1);
            ident *id = newident(argname, IDF_ARG);
            id->forcenull();
            uiargs.add(id);
        }

        surfacestack.setsize(0);
        loopi(SURFACE_MAX) surfaces[i] = new Surface(i);

        surface = NULL;
        inputsteal = NULL;
        viewports.deletecontents();
    }

    void cleanup()
    {
        loopvrev(dynuis)
        {
            DynUI *d = dynuis[i];
            dynuis.remove(i);
            delete d;
        }

        loopj(SURFACE_MAX)
        {
            if(!pushsurface(j)) continue;
            surface->hideall(true);
            surface->children.setsize(0);
            enumerate(surface->windows, Window *, w, delete w);
            surface->windows.clear();
            popsurface();
        }

        surface = NULL;
        inputsteal = NULL;
        surfacestack.setsize(0);
        loopi(SURFACE_MAX) DELETEP(surfaces[i]);
        viewports.deletecontents();
    }

    void cleangl()
    {
        Image::lasttex = NULL;

        loopj(SURFACE_MAX)
        {
            if(!pushsurface(j)) continue;
            surface->hideall(true);
            loopv(surface->texs) surface->texs[i]->cleanup();
            surface->texs.setsize(0);
            popsurface();
        }
        viewports.deletecontents();
    }

    static inline bool texsort(Texture *a, Texture *b)
    {
        if(!a->paused(uiclockticks) && b->paused(uiclockticks)) return true;
        if(a->paused(uiclockticks) && !b->paused(uiclockticks)) return false;
        if(a->last < b->last) return true;
        if(a->last > b->last) return false;
        return false;
    }

    void updatetextures()
    {
        if(!pushsurface(SURFACE_COMPOSITE)) return;

        bool found = false;
        int oldhudw = hudw, oldhudh = hudh, oldsf = surfaceformat;
        GLuint oldfbo = renderfbo;

        poke(true);

        int processed = 0;
        if(compositeruncount) surface->texs.sort(texsort);

        loopv(surface->texs)
        {
            Texture *t = surface->texs[i];

            if(t->rendered >= 2 && compositeruncount > 0 && processed >= compositeruncount) continue;

            int delay = 0, elapsed = t->update(delay, uiclockticks, compositeuprate);
            if(t->rendered >= 2 && elapsed < 0) continue;

            found = true;

            poke(false);

            if(delay >= 0 && compositerewind)
            {
                uicurtime = elapsed;

                int offset = delay > 1 ? elapsed % delay : delay;
                if(offset > 0)
                {
                    uilastmillis -= int(offset * timescale / 100.f);
                    uitotalmillis -= offset;
                }
            }

            GLERROR;
            if(!t->fbo) glGenFramebuffers_(1, &t->fbo);
            glBindFramebuffer_(GL_FRAMEBUFFER, t->fbo);
            renderfbo = t->fbo;

            if(!t->id)
            {
                if(!t->format) t->format = compformat();
                glGenTextures(1, &t->id);
                createtexture(t->id, t->w, t->w, NULL, t->tclamp, t->mipmap ? 3 : 0, t->format, GL_TEXTURE_2D, 0, 0, 0, true, t->format, true);
                glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->id, 0);
            }

            GLERROR;
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                conoutf(colourred, "Failed rendering composite texture framebuffer: %s [%u / %u]", t->name, t->id, t->fbo);
                continue;
            }

            hudw = hudh = t->w;
            glViewport(0, 0, hudw, hudh);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            surfaceformat = formatcomp(t->format);

            Window *w = surface->windows.find(t->comp, NULL);
            if(w)
            {
                w->setarg(t->args ? t->args : "", -1);

                surface->show(w);
                surface->build();
                surface->render();

                if(t->mipmap)
                {
                    glActiveTexture_(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, t->id);
                    glGenerateMipmap_(GL_TEXTURE_2D);
                }

                surface->hide(w);
            }

            t->last = uiclockticks;

            if(t->rendered < 2) t->rendered++;
            else if(delay < 0)
            { // don't need to keep stuff we're not going to continue using it
                if(t->fbo)
                {
                    glDeleteFramebuffers_(1, &t->fbo);
                    t->fbo = 0;
                }
                surface->texs.remove(i--);
            }

            processed++;
        }

        popsurface();
        poke(false);

        if(found)
        {
            hudw = oldhudw;
            hudh = oldhudh;
            surfaceformat = oldsf;
            glBindFramebuffer_(GL_FRAMEBUFFER, oldfbo);
            renderfbo = oldfbo;
            glViewport(0, 0, hudw, hudh);
        }
    }
    ICOMMANDV(0, compositecount, surfaces[SURFACE_COMPOSITE] ? surfaces[SURFACE_COMPOSITE]->texs.length() : 0);

    static const int surforder[SURFACE_ALL] = { SURFACE_FOREGROUND, SURFACE_VISOR, SURFACE_BACKGROUND, SURFACE_WORLD };

    void poke(bool ticks)
    {
        uilastmillis = lastmillis;
        uitotalmillis = totalmillis;
        if(ticks)
        {
            uiclockmillis = getclockmillis();
            uiclockticks = getclockticks();
        }
        uicurtime = curtime;

        int oldinput = surfaceinput;
        loopi(SURFACE_ALL)
        {
            Surface *s = surfaces[surforder[i]];
            if(!s || !s->allowinput(true, true)) continue;
            surfaceinput = s->type;
            break;
        }
        if(oldinput != surfaceinput) inputsteal = NULL;
    }

    void update()
    {
        checkmapuis();
    }

    void build(bool noview)
    {
        if(uihidden) return;

        readyeditors();

        loopi(SURFACE_ALL)
        {
            if((i == SURFACE_WORLD && noview) || !pushsurface(i))
                continue; // skip world UI's when in noview

            surface->build();
            surface->init();

            popsurface();
        }

        flusheditors();
    }

    void render(int stype)
    {
        if(uihidden || !pushsurface(stype)) return;

        if(surfacetype == SURFACE_PROGRESS)
        {
            if(!pokeui("default", SURFACE_PROGRESS))
            {
                popsurface();
                return;
            }
            surface->build();
        }
        surface->render();

        popsurface();
    }
}
