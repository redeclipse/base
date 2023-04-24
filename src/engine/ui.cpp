#include "engine.h"
#include "textedit.h"

namespace UI
{
    GLuint comptexfbo = 0;
    static bool texgc = false;
    static int lastthumbnail = 0;

    FVAR(0, uitextscale, 1, 0, 0);

    SVAR(0, uiopencmd, "showui");
    SVAR(0, uiclosecmd, "hideui");
    SVAR(0, uiprecmd, "");
    SVAR(0, uipostcmd, "");

    TVAR(IDF_PERSIST|IDF_PRELOAD, uiloadtex, "<anim:100,4,3>textures/loading", 3);

    VAR(0, uihidden, 0, 0, 1);
    FVAR(IDF_PERSIST, uiscale, FVAR_NONZERO, 1, 100);
    VAR(IDF_PERSIST, uitextrows, 1, 48, VAR_MAX);

    VAR(IDF_PERSIST, uiscrollsteptime, 0, 50, VAR_MAX);
    VAR(IDF_PERSIST, uislidersteptime, 0, 50, VAR_MAX);
    VAR(IDF_PERSIST, uislotviewtime, 0, 25, VAR_MAX);

    FVAR(IDF_PERSIST, uitipoffset, -1, 0.001f, 1);

    static Texture *loadthumbnail(const char *name, int tclamp)
    {
        Texture *t = textureloaded(name);

        if(!t)
        {
            if(totalmillis - lastthumbnail < uislotviewtime) t = textureload(uiloadtex);
            else
            {
                t = textureload(name, tclamp, true, false, texgc);
                lastthumbnail = totalmillis;
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

    static void pushclip(float x, float y, float w, float h)
    {
        if(clipstack.empty()) glEnable(GL_SCISSOR_TEST);
        ClipArea &c = clipstack.add(ClipArea(x, y, w, h));
        if(clipstack.length() >= 2) c.intersect(clipstack[clipstack.length()-2]);
        c.scissor();
    }

    static void popclip()
    {
        clipstack.pop();
        if(clipstack.empty()) glDisable(GL_SCISSOR_TEST);
        else clipstack.last().scissor();
    }

    static inline bool isfullyclipped(float x, float y, float w, float h)
    {
        if(clipstack.empty()) return false;
        return clipstack.last().isfullyclipped(x, y, w, h);
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
        ALIGN_DEFAULT = ALIGN_HCENTER | ALIGN_VCENTER
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

        STATE_HOLD_MASK = STATE_HOLD | STATE_ALT_HOLD | STATE_ESC_HOLD
    };

    enum
    {
        SETSTATE_ANY = 0,
        SETSTATE_INSIDE,
        SETSTATE_FOCUSED
    };

    struct Object;

    static Object *buildparent = NULL;
    static Object *inputsteal = NULL;
    static int buildchild = -1;

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
    static int changed = 0;

    static Object *drawing = NULL;

    enum { BLEND_ALPHA, BLEND_MOD };
    static int blendtype = BLEND_ALPHA;

    static inline void changeblend(int type, GLenum src, GLenum dst)
    {
        if(blendtype != type)
        {
            blendtype = type;
            glBlendFunc(src, dst);
        }
    }

    void resetblend() { changeblend(BLEND_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    void modblend() { changeblend(BLEND_MOD, GL_ZERO, GL_SRC_COLOR); }

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

    #define UICMD(uitype, uiname, vname, valtype, args, body) \
        ICOMMAND(0, ui##uiname##vname, valtype, args, { \
            if(buildparent && buildparent->istype<uitype>()) \
            { \
                uitype *o = (uitype *)buildparent; \
                body; \
            } \
            else conoutf("Warning: parent not a %s in ui%s%s", #uitype, #uiname, #vname); \
        });

    #define UIGETOBJ(uitype, uiname, vname, type) \
        ICOMMAND(0, uiget##uiname##vname, "", (), { \
            if(buildparent) \
            { \
                uitype *o = (uitype *)buildparent; \
                type##ret(o->vname); \
            } \
        });

    #define UIGETCMD(uitype, uiname, vname, type) \
        ICOMMAND(0, uiget##uiname##vname, "", (), { \
            if(buildparent && buildparent->istype<uitype>()) \
            { \
                uitype *o = (uitype *)buildparent; \
                type##ret(o->vname); \
            } \
            else conoutf("Warning: parent not a %s in ui%s%s", #uitype, #uiname, #vname); \
        });

    #define UIARGB(uitype, uiname, vname) \
        UICMD(uitype, uiname, vname, "i", (int *val), { \
            o->vname = *val!=0; \
            intret(o->vname ? 1 : 0); \
        }); \
        UIGETCMD(uitype, uiname, vname, int);

    #define UIARGK(uitype, uiname, vname, valtype, type, cmin, cmax, valdef) \
        UICMD(uitype, uiname, vname, valtype, (type *val), { \
            o->vname = type(clamp((valdef), cmin, cmax)); \
            type##ret(o->vname); \
        }); \
        UIGETCMD(uitype, uiname, vname, type);

    #define UIARGSCALED(uitype, uiname, vname, valtype, type, cmin, cmax) \
        UICMD(uitype, uiname, vname, valtype, (type *val), { \
            o->vname = type(clamp(*val, cmin, cmax)*uiscale); \
            type##ret(o->vname); \
        }); \
        UIGETCMD(uitype, uiname, vname, type);

    #define UIARG(uitype, uiname, vname, valtype, type, cmin, cmax) \
        UICMD(uitype, uiname, vname, valtype, (type *val), { \
            o->vname = type(clamp(*val, cmin, cmax)); \
            type##ret(o->vname); \
        }); \
        UIGETCMD(uitype, uiname, vname, type);

    #define UICMDT(uitype, uiname, vname, valtype, args, body) \
        ICOMMAND(0, ui##uiname##vname, valtype, args, { \
            if(buildparent && buildparent->is##uiname()) \
            { \
                uitype *o = (uitype *)buildparent; \
                body; \
            } \
            else conoutf("Warning: parent not a is%s in ui%s%s", #uiname, #uiname, #vname); \
        });

    #define UIGETCMDT(uitype, uiname, vname, type) \
        ICOMMAND(0, uiget##uiname##vname, "", (), { \
            if(buildparent && buildparent->is##uiname()) \
            { \
                uitype *o = (uitype *)buildparent; \
                type##ret(o->vname); \
            } \
        });

    #define UIARGTB(uitype, uiname, vname) \
        UICMDT(uitype, uiname, vname, "i", (int *val), { \
            o->vname = *val!=0; \
            intret(o->vname ? 1 : 0); \
        }); \
        UIGETCMDT(uitype, uiname, vname, int);

    #define UIARGTK(uitype, uiname, vname, valtype, type, cmin, cmax, valdef) \
        UICMDT(uitype, uiname, vname, valtype, (type *val), { \
            o->vname = type(clamp((valdef), cmin, cmax)); \
            type##ret(o->vname); \
        }); \
        UIGETCMDT(uitype, uiname, vname, type);

    #define UIARGSCALEDT(uitype, uiname, vname, valtype, type, cmin, cmax) \
        UICMDT(uitype, uiname, vname, valtype, (type *val), { \
            o->vname = type(clamp(*val, cmin, cmax)*uiscale); \
            type##ret(o->vname); \
        }); \
        UIGETCMDT(uitype, uiname, vname, type);

    #define UIARGT(uitype, uiname, vname, valtype, type, cmin, cmax) \
        UICMDT(uitype, uiname, vname, valtype, (type *val), { \
            o->vname = type(clamp(*val, cmin, cmax)); \
            type##ret(o->vname); \
        }); \
        UIGETCMDT(uitype, uiname, vname, type);

    struct Object
    {
        Object *parent;
        float x, y, w, h, ox, oy;
        float lastx, lasty, lastsx, lastsy, lastw, lasth;
        bool overridepos, drawn;
        uchar adjust;
        ushort state, childstate;
        vector<Object *> children;

        Object() : ox(0), oy(0), lastx(0), lasty(0), lastsx(0), lastsy(0), lastw(0), lasth(0),
            overridepos(false), drawn(false), adjust(0), state(0), childstate(0) {}

        virtual ~Object()
        {
            if(inputsteal == this) inputsteal = NULL;
            clearchildren();
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

        bool hasstate(int flags) const { return ((state & ~childstate) & flags) != 0; }
        bool haschildstate(int flags) const
        {
            bool cs = ((state | childstate) & flags) != 0;
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
            #define DOSTATE(flags, func) case flags: func##children(cx, cy, true, mask, mode, setflags | flags); return haschildstate(flags);
            DOSTATES
            #undef DOSTATE
            }
            return false;
        }

        void clearstate(int flags)
        {
            state &= ~flags;
            if(childstate & flags)
            {
                loopchildren(o, { if((o->state | o->childstate) & flags) o->clearstate(flags); });
                childstate &= ~flags;
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

        #define DOSTATE(flags, func) \
            virtual void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
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

        virtual bool isallowed() const { return true; }
        virtual bool isfill() const { return false; }
        virtual bool iscolour() const { return false; }
        virtual bool istext() const { return false; }
        virtual bool iscomp() const { return false; }
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
            T *t;
            if(children.inrange(buildchild))
            {
                Object *o = children[buildchild];
                if(o->istype<T>()) t = (T *)o;
                else
                {
                    delete o;
                    t = new T;
                    children[buildchild] = t;
                }
            }
            else
            {
                t = new T;
                children.add(t);
            }
            if(!t->isallowed())
            {
                delete t;
                children.remove(buildchild);
                return NULL;
            }
            t->reset(this);
            buildchild++;
            return t;
        }

        void buildchildren(uint *contents)
        {
            if((*contents&CODE_OP_MASK) == CODE_EXIT) children.deletecontents();
            else
            {
                Object *oldparent = buildparent;
                int oldchild = buildchild;
                buildparent = this;
                buildchild = 0;
                executeret(contents);
                while(children.length() > buildchild)
                    delete children.pop();
                buildparent = oldparent;
                buildchild = oldchild;
            }
            resetstate();
        }

        virtual int childcolumns() const { return children.length(); }
    };

    ICOMMAND(0, uigroup, "e", (uint *children),
        BUILD(Object, o, o->setup(), children));

    static inline void stopdrawing()
    {
        if(drawing)
        {
            drawing->enddraw(0);
            drawing = NULL;
        }
    }

    struct Surface;
    enum { SURFACE_MAIN = 0, SURFACE_PROGRESS, SURFACE_COMPOSITE, SURFACE_MAX };
    int cursurface = -1;
    Surface *surfaces[SURFACE_MAX] = { NULL, NULL, NULL };

    struct Window;

    enum { WINTYPE_NORMAL = 0, WINTYPE_COMPOSITE, WINTYPE_MAX };
    const char *windowtype[WINTYPE_MAX] = { "Normal", "Composite" };
    const char *windowaffix[WINTYPE_MAX] = { "", "comp" };
    static Window *window[WINTYPE_MAX] = { NULL, NULL };

    const char *getwinprefix(int type, const char *suffix = " ")
    {
        type = clamp(type, 0, int(WINTYPE_MAX)-1);
        if(type > 0)
        {
            static string winprefix;
            formatstring(winprefix, "%s%s", windowtype[type], suffix);
            return winprefix;
        }
        return "";
    }

    int getwindowtype()
    {
        if(cursurface == SURFACE_COMPOSITE) return WINTYPE_COMPOSITE;
        return WINTYPE_NORMAL;
    }

    enum
    {
        WINDOW_NONE = 0,
        WINDOW_MENU = 1<<0, WINDOW_PASS = 1<<1, WINDOW_TIP = 1<<2, WINDOW_POPUP = 1<<3, WINDOW_PERSIST = 1<<4, WINDOW_TOP = 1<<5,
        WINDOW_ALL = WINDOW_MENU|WINDOW_PASS|WINDOW_TIP|WINDOW_POPUP|WINDOW_PERSIST|WINDOW_TOP
    };

    struct Window : Object
    {
        char *name, *body;
        uint *contents, *onshow, *onhide;
        bool exclusive, mapdef;
        int allowinput, windowflags;
        float px, py, pw, ph;
        vec2 sscale, soffset;

        Window(const char *name, const char *contents, const char *onshow, const char *onhide, int windowflags_, bool mapdef_) :
            name(newstring(name)), body(NULL),
            contents(compilecode(contents)),
            onshow(!mapdef_ && onshow && onshow[0] ? compilecode(onshow) : NULL),
            onhide(!mapdef_ && onhide && onhide[0] ? compilecode(onhide) : NULL),
            exclusive(false), mapdef(mapdef_), allowinput(1),
            px(0), py(0), pw(0), ph(0),
            sscale(1, 1), soffset(0, 0)
        {
            if(mapdef) body = newstring(contents);
            windowflags = mapdef ? 0 : clamp(windowflags_, 0, int(WINDOW_ALL));
        }
        ~Window()
        {
            delete[] name;
            freecode(contents);
            freecode(onshow);
            freecode(onhide);
        }

        static const char *typestr() { return "#Window"; }
        const char *gettype() const { return typestr(); }
        const char *getname() const { return name; }

        void build();

        void hide()
        {
            overridepos = false;
            if(onhide) execute(onhide);
        }

        void show()
        {
            overridepos = false;
            state |= STATE_HIDDEN;
            clearstate(STATE_HOLD_MASK);
            if(onshow) execute(onshow);
        }

        void setup()
        {
            Object::setup();
            allowinput = 1;
            exclusive = false;
            px = py = pw = ph = 0;
        }

        void layout()
        {
            if(state&STATE_HIDDEN) { w = h = 0; return; }
            window[getwindowtype()] = this;
            Object::layout();
            window[getwindowtype()] = NULL;
        }

        void draw(float sx, float sy)
        {
            if(state&STATE_HIDDEN) return;
            window[getwindowtype()] = this;

            projection();

            glEnable(GL_BLEND);
            blendtype = BLEND_ALPHA;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            resethudshader();

            changed = 0;
            drawing = NULL;

            Object::draw(sx, sy);

            stopdrawing();

            glDisable(GL_BLEND);

            window[getwindowtype()] = NULL;
        }

        void draw()
        {
            draw(x, y);
        }

        void adjustchildren()
        {
            if(state&STATE_HIDDEN) return;
            window[getwindowtype()] = this;
            Object::adjustchildren();
            window[getwindowtype()] = NULL;
        }

        void adjustlayout()
        {
            float aspect = float(hudw)/hudh;
            ph = max(max(h, w/aspect), 1.0f);
            pw = aspect*ph;
            Object::adjustlayout(0, 0, pw, ph);
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                if(!allowinput || state&STATE_HIDDEN || pw <= 0 || ph <= 0) return; \
                cx = cx*pw + px-x; \
                cy = cy*ph + py-y; \
                bool inside = (cx >= 0 && cy >= 0 && cx < w && cy < h); \
                if(mode != SETSTATE_INSIDE || inside) \
                    Object::func##children(cx, cy, inside, mask, mode, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void projection()
        {
            if(cursurface == SURFACE_COMPOSITE) // composites have flipped Y axis
                hudmatrix.ortho(px, px + pw, py, py + ph, -1, 1);
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
        }
    };

    Window *uirootwindow(Object *o)
    {
        Object *p = o;
        while(p)
        {
            if(p->istype<Window>()) return (Window *)p;
            p = p->parent;
        }
        return NULL;
    }
    ICOMMAND(0, uirootname, "", (), { Window *o = uirootwindow(buildparent); result(o ? o->name : ""); });

    #define UIWINCMDC(func, types, argtypes, body) \
        ICOMMAND(0, ui##func##root, types, argtypes, \
        { \
            Object *o = uirootwindow(buildparent); \
            if(o) { body; } \
        }); \
        ICOMMAND(0, ui##func, types, argtypes, \
        { \
            Object *o = buildparent; \
            if(o) { body; } \
        });

    hashnameset<Window *> windows[WINTYPE_MAX];

    void ClipArea::scissor()
    {
        int sx1, sy1, sx2, sy2;
        window[getwindowtype()]->calcscissor(x1, y1, x2, y2, sx1, sy1, sx2, sy2);
        glScissor(sx1, sy1, sx2-sx1, sy2-sy1);
    }

    struct Surface : Object
    {
        int cursortype;
        bool cursorlocked, mousetracking, lockscroll;
        vec2 mousetrackvec;

        Surface() : cursortype(CURSOR_DEFAULT), cursorlocked(false), mousetracking(false), lockscroll(false), mousetrackvec(0, 0) {}
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

        void adjustchildren()
        {
            loopwindows(w, w->adjustlayout());
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                loopwindowsrev(w, \
                { \
                    if(((w->state | w->childstate) & mask) != mask) continue; \
                    w->func##children(cx, cy, cinside, mask, mode, setflags); \
                    int wflags = (w->state | w->childstate) & (setflags); \
                    if(wflags) { childstate |= wflags; break; } \
                }); \
            }
        DOSTATES
        #undef DOSTATE

        void build()
        {
            reset();
            setup();
            loopwindows(w,
            {
                w->build();
                if(!children.inrange(i)) break;
                if(children[i] != w) i--;
            });
            resetstate(); // IMPORTED
        }

        bool forcetop()
        {
            if(children.empty()) return false;

            Window *w = (Window *)children.last();
            return w->windowflags&WINDOW_TOP;
        }

        bool show(Window *w)
        {
            if(children.find(w) >= 0) return false;
            w->resetchildstate();
            if(forcetop()) children.insert(max(0, children.length() - 1), w);
            else children.add(w);
            w->show();
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
                if(w->state&STATE_HIDDEN || w->windowflags&WINDOW_PERSIST) continue;
                if(w->allowinput || w->windowflags&WINDOW_PASS) { hide(w, i); return true; }
            });
            return false;
        }

        int hideall(bool force = false)
        {
            int hidden = 0;
            loopwindowsrev(w,
            {
                if(!force && w->windowflags&WINDOW_PERSIST) continue;
                hide(w, i);
                hidden++;
            });
            return hidden;
        }

        bool hasexclusive() const
        {
            loopwindows(w, if(w->exclusive) return true);
            return false;
        }

        int allowinput() const
        {
            bool hasexcl = hasexclusive();
            int ret = 0;
            loopwindows(w,
            {
                if(hasexcl && !w->exclusive) continue;
                if(w->allowinput && !(w->state&STATE_HIDDEN) && ret != 1) ret = w->allowinput;
            });
            return ret;
        }

        bool hasmenu(bool pass = true) const
        {
            loopwindows(w,
            {
                if(w->windowflags&WINDOW_MENU && !(w->state&STATE_HIDDEN)) return !pass || !(w->windowflags&WINDOW_PASS);
            });
            return false;
        }

        const char *topname()
        {
            loopwindowsrev(w,
            {
                if((w->allowinput || w->windowflags&WINDOW_PASS) && !(w->state&STATE_HIDDEN)) { return w->name; }
            });
            return NULL;
        }

        void draw(float sx, float sy) {}

        void draw()
        {
            if(children.empty()) return;
            bool hasexcl = hasexclusive();
            loopwindows(w,
            {
                if(hasexcl && !w->exclusive) continue;
                if(w->windowflags&WINDOW_TIP) // follows cursor
                    w->setpos((cursorx*float(hudw)/float(hudh))-(w->w*cursorx), cursory >= 0.5f ? cursory-w->h-uitipoffset : cursory+hud::cursorsize+uitipoffset);
                else if(w->windowflags&WINDOW_POPUP && !w->overridepos)
                    w->setpos((cursorx*float(hudw)/float(hudh))-(w->w*cursorx), cursory-w->h*0.5f);
            });
            loopwindows(w,
            {
                if(hasexcl && !w->exclusive) continue;
                w->draw();
            });
        }
    };

    bool inputsurface()
    {
        return cursurface == SURFACE_MAIN;
    }

    void Window::build()
    {
        if(cursurface < 0 || !surfaces[cursurface]) return;
        reset(surfaces[cursurface]);
        setup();
        window[getwindowtype()] = this;
        buildchildren(contents);
        window[getwindowtype()] = NULL;
    }

    void newui(int type, char *name, char *contents, char *onshow, char *onhide, int windowflags, bool mapdef = false)
    {
        if(!name || !*name || !contents || !*contents) return;
        type = clamp(type, 0, int(WINTYPE_MAX)-1);
        if(mapdef && !(identflags&IDF_MAP) && !editmode)
        {
            conoutf("\frMap %sUI %s is only directly modifiable in editmode", getwinprefix(type), name);
            return;
        }

        bool found = false;
        Window *w = windows[type].find(name, NULL);
        if(w)
        {
            if(w == UI::window[type]) return;
            if(!w->mapdef && mapdef)
            {
                conoutf("\frCannot override builtin %sUI %s with a one from the map", getwinprefix(type), w->name);
                return;
            }
            loopi(3) if(surfaces[i]) surfaces[i]->hide(w);
            windows[type].remove(name);
            delete w;
            found = true;
        }

        windows[type][name] = new Window(name, contents, onshow, onhide, windowflags, mapdef);

        if(found && type == WINTYPE_COMPOSITE)
            enumerate(textures, Texture, t, if(t.type&Texture::COMPOSITE && t.comp && !strcmp(name, t.comp)) composite(&t.id, t.comp, t.args, t.w, t.h, t.tclamp, t.mipmap, true));
    }
    ICOMMAND(0, newui, "ssssi", (char *name, char *contents, char *onshow, char *onhide, int *windowflags), newui(WINTYPE_NORMAL, name, contents, onshow, onhide, *windowflags));
    ICOMMAND(0, mapui, "ssssi", (char *name, char *contents, char *onshow, char *onhide, int *windowflags), newui(WINTYPE_NORMAL, name, contents, onshow, onhide, *windowflags, true));
    ICOMMAND(0, newcompui, "ssssi", (char *name, char *contents, char *onshow, char *onhide, int *windowflags), newui(WINTYPE_COMPOSITE, name, contents, onshow, onhide, *windowflags));
    ICOMMAND(0, mapcompui, "ssssi", (char *name, char *contents, char *onshow, char *onhide, int *windowflags), newui(WINTYPE_COMPOSITE, name, contents, onshow, onhide, *windowflags, true));

    ICOMMAND(0, uiallowinput, "b", (int *val), { if(window[getwindowtype()]) { if(*val >= 0) window[getwindowtype()]->allowinput = clamp(*val, 0, 2); intret(window[getwindowtype()]->allowinput); } });
    ICOMMAND(0, uiexclusive, "b", (int *val), { if(window[getwindowtype()]) { if(*val >= 0) window[getwindowtype()]->exclusive = *val!=0; intret(window[getwindowtype()]->exclusive ? 1 : 0); } });
    ICOMMAND(0, uiwindowflags, "b", (int *val), { if(window[getwindowtype()]) { if(*val >= 0) window[getwindowtype()]->windowflags = clamp(*val, 0, int(WINDOW_ALL)); intret(window[getwindowtype()]->windowflags); } });

    ICOMMAND(0, uioverridepos, "", (), { if(window[getwindowtype()]) { intret(window[getwindowtype()]->overridepos ? 1 : 0); } });
    ICOMMAND(0, uisetpos, "ff", (float *xpos, float *ypos), { if(window[getwindowtype()]) { window[getwindowtype()]->setpos(*xpos, *ypos); } });
    ICOMMAND(0, uiresetpos, "", (), { if(window[getwindowtype()]) { window[getwindowtype()]->resetpos(); } });

    ICOMMAND(0, uicursorx, "", (), floatret(cursorx*float(hudw)/hudh));
    ICOMMAND(0, uicursory, "", (), floatret(cursory));
    ICOMMAND(0, uilockcursor, "", (), if(cursurface >= 0 && surfaces[cursurface]) surfaces[cursurface]->cursorlocked = true);
    ICOMMAND(0, uilockscroll, "", (), if(cursurface >= 0 && surfaces[cursurface]) surfaces[cursurface]->lockscroll = true);

    ICOMMAND(0, uiaspect, "", (), floatret(float(hudw)/hudh));

    ICOMMAND(0, uicursortype, "b", (int *val), if(cursurface >= 0 && surfaces[cursurface]) { if(*val >= 0) surfaces[cursurface]->cursortype = clamp(*val, 0, CURSOR_MAX-1); intret(surfaces[cursurface]->cursortype); });

    ICOMMAND(0, uimousetrackx, "", (), {
        if(cursurface >= 0 && surfaces[cursurface])
        {
            surfaces[cursurface]->mousetracking = true;
            floatret(surfaces[cursurface]->mousetrackvec.x);
        }
    });

    ICOMMAND(0, uimousetracky, "", (), {
        if(cursurface >= 0 && surfaces[cursurface])
        {
            surfaces[cursurface]->mousetracking = true;
            floatret(surfaces[cursurface]->mousetrackvec.y);
        }
    });

    bool showui(const char *name)
    {
        if(cursurface < 0 || !surfaces[cursurface]) return false;
        Window *w = windows[getwindowtype()].find(name, NULL);
        if(!w) return false;
        return surfaces[cursurface]->show(w);
    }

    bool hideui(const char *name)
    {
        if(cursurface < 0 || !surfaces[cursurface]) return false;
        if(!name || !*name) return surfaces[cursurface]->hideall() > 0;
        else
        {
            Window *w = windows[getwindowtype()].find(name, NULL);
            if(w) return surfaces[cursurface]->hide(w);
        }
        return false;
    }

    bool toggleui(const char *name)
    {
        if(showui(name)) return true;
        hideui(name);
        return false;
    }

    int openui(const char *name)
    {
        defformatstring(cmd, "%s \"%s\"", uiopencmd, name ? name : "");
        return execute(cmd);
    }

    int closeui(const char *name)
    {
        defformatstring(cmd, "%s \"%s\"", uiclosecmd, name ? name : "");
        return execute(cmd);
    }

    void holdui(const char *name, bool on)
    {
        if(on) showui(name);
        else hideui(name);
    }

    void pressui(const char *name, bool on)
    {
        if(on) { if(!uivisible(name)) openui(name); }
        else if(uivisible(name)) closeui(name);
    }

    bool uivisible(const char *name)
    {
        if(cursurface < 0 || !surfaces[cursurface]) return false;
        if(!name || !*name) return surfaces[cursurface]->children.length() > 0;
        Window *w = windows[getwindowtype()].find(name, NULL);
        return w && surfaces[cursurface]->children.find(w) >= 0;
    }

    ICOMMAND(0, showui, "s", (char *name), intret(showui(name) ? 1 : 0));
    ICOMMAND(0, hideui, "s", (char *name), intret(hideui(name) ? 1 : 0));
    ICOMMAND(0, hidetopui, "", (), intret(cursurface >= 0 && surfaces[cursurface] && surfaces[cursurface]->hidetop() ? 1 : 0));
    ICOMMAND(0, topui, "", (), result(cursurface >= 0 && surfaces[cursurface] ? surfaces[cursurface]->topname() : ""));
    ICOMMAND(0, hideallui, "i", (int *n), intret(cursurface >= 0 && surfaces[cursurface] ? surfaces[cursurface]->hideall(*n != 0) : 0));
    ICOMMAND(0, toggleui, "s", (char *name), intret(toggleui(name) ? 1 : 0));
    ICOMMAND(0, holdui, "sD", (char *name, int *down), holdui(name, *down!=0));
    ICOMMAND(0, pressui, "sD", (char *name, int *down), pressui(name, *down!=0));
    ICOMMAND(0, uivisible, "s", (char *name), intret(uivisible(name) ? 1 : 0));
    ICOMMAND(0, uiname, "", (), { if(window[getwindowtype()]) result(window[getwindowtype()]->name); });

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
            int oldchild = buildchild;
            buildparent = this;
            buildchild = 0;
            executeret(columndata);
            if(columns != buildchild) while(children.length() > buildchild) delete children.pop();
            columns = buildchild;
            if((*contents&CODE_OP_MASK) != CODE_EXIT) executeret(contents);
            while(children.length() > buildchild) delete children.pop();
            buildparent = oldparent;
            buildchild = oldchild;
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
        vector<float> widths;

        static const char *typestr() { return "#Table"; }
        const char *gettype() const { return typestr(); }

        void setup(float spacew_ = 0, float spaceh_ = 0)
        {
            Object::setup();
            spacew = spacew_;
            spaceh = spaceh_;
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
            loopv(widths) subw += widths[i];
            w = max(w, subw + spacew*max(widths.length() - 1, 0));
            h = subh + spaceh*max(children.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offsety = 0, sy = 0,
                  cspace = (w - subw) / max(widths.length() - 1, 1),
                  cstep = (w - subw) / widths.length(),
                  rspace = (h - subh) / max(children.length() - 1, 1),
                  rstep = (h - subh) / children.length();
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
                    offsetx += widths[j] + cspace;
                    float sw = widths[j] + cstep;
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
        union
        {
            struct { uchar r, g, b, a; };
            uint mask;
        };

        Color() : mask(0xFFFFFFFFU) {}
        Color(uint c) : r((c>>16)&0xFF), g((c>>8)&0xFF), b(c&0xFF), a(c>>24 ? c>>24 : 0xFF) {}
        Color(uint c, uchar a) : r((c>>16)&0xFF), g((c>>8)&0xFF), b(c&0xFF), a(a) {}
        Color(uchar r, uchar g, uchar b, uchar a = 255) : r(r), g(g), b(b), a(a) {}
        Color(const Color &c) : r(c.r), g(c.g), b(c.b), a(c.a) {}

        void init() { gle::colorub(r, g, b, a); }
        void attrib() { gle::attribub(r, g, b, a); }

        static void def() { gle::defcolor(4, GL_UNSIGNED_BYTE); }

        Color &scale(const Color &c)
        {
            r = int(r*c.r/255.f);
            g = int(g*c.g/255.f);
            b = int(b*c.b/255.f);
            a = int(a*c.a/255.f);
            return *this;
        }

        vec tocolor() const { return vec(r*(1.0f/255.0f), g*(1.0f/255.0f), b*(1.0f/255.0f)); }
        int tohexcolor() const { return (int(r)<<16)|(int(g)<<8)|int(b); }

        vec4 tocolor4() const { return vec4(r*(1.0f/255.0f), g*(1.0f/255.0f), b*(1.0f/255.0f), a*(1.0f/255.0f)); }
        int tohexcolor4() const { return (int(a)<<24)|(int(r)<<16)|(int(g)<<8)|int(b); }

        bool operator==(const Color &o) const { return mask == o.mask; }
        bool operator!=(const Color &o) const { return mask != o.mask; }
    };

    struct Colored : Object
    {
        enum { SOLID = 0, MODULATE, OUTLINED };
        enum { VERTICAL, HORIZONTAL };

        int type, dir;
        vector<Color> colors;

        static const char *typestr() { return "#Colored"; }
        const char *gettype() const { return typestr(); }
        bool iscolour() const { return true; }

        void setup(const Color &color_, int type_ = SOLID, int dir_ = VERTICAL)
        {
            Object::setup();
            colors.setsize(0);
            colors.add(color_);
            type = type_;
            dir = dir_;
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
                    Color(colors[m].r-int((colors[m].r-colors[n].r)*iter),
                          colors[m].g-int((colors[m].g-colors[n].g)*iter),
                          colors[m].b-int((colors[m].b-colors[n].b)*iter),
                          colors[m].a-int((colors[m].a-colors[n].a)*iter)
                         )
                    );
                if(!rev) index++;
            }
            colors.setsize(0);
            loopv(colorstack) colors.add(colorstack[i]);
        }
    };
    UIARGT(Colored, colour, type, "i", int, int(Colored::SOLID), int(Colored::OUTLINED));
    UIARGT(Colored, colour, dir, "i", int, int(Colored::VERTICAL), int(Colored::HORIZONTAL));

    UICMDT(Colored, colour, set, "ii", (int *c, int *pos),
    {
        if(*pos >= 0 && *pos < o->colors.length()) o->colors[*pos] = Color(*c);
    });
    UICMDT(Colored, colour, get, "i", (int *pos), intret(o->colors[clamp(*pos, 0, o->colors.length()-1)].mask));
    UICMDT(Colored, colour, add, "i", (int *c), o->colors.add(Color(*c)));
    UICMDT(Colored, colour, del, "i", (int *c),
    {
        loopvrev(o->colors) if(o->colors[i] == Color(*c)) o->colors.remove(i);
        if(o->colors.empty()) o->colors.add(Color(colourwhite));
    });
    UICMDT(Colored, colour, rotate, "fii", (float *amt, int *start, int *count), o->rotatecolors(*amt, *start, *count));

    static const float defcoords[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

    struct Filler : Colored
    {
        enum { FC_TL = 0, FC_TR, FC_BR, FC_BL, FC_MAX };

        float minw, minh;
        vec2 coords[FC_MAX];

        void setup(float minw_, float minh_, const Color &color_ = Color(colourwhite), int type_ = SOLID, int dir_ = VERTICAL)
        {
            Colored::setup(color_, type_, dir_);
            minw = minw_;
            minh = minh_;
            loopi(FC_MAX) loopj(2) coords[i][j] = defcoords[i][j];
        }

        void setup(const Color &color_, int type_ = SOLID, int dir_ = VERTICAL)
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
        void setup(float minw_, float minh_, const Color &color_ = Color(colourwhite), int type_ = SOLID, int dir_ = VERTICAL)
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
        void setup(const Color &color_, float minw_ = 0, float minh_ = 0, int type_ = SOLID, int dir_ = VERTICAL)
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
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            int cols = colors.length();
            gle::begin(GL_TRIANGLE_STRIP);
            if(cols >= 2)
            {
                float vr = 1/float(cols-1), vcx1 = 0, vcx2 = 0, vcy1 = 0, vcy2 = 0, ts = 0,
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
                    ts += vr;
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
        BUILD(FillColor, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, Colored::SOLID), children));

    ICOMMAND(0, uimodcolour, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, Colored::MODULATE), children));

    struct Gradient : FillColor
    {
        void setup(const Color &color_, const Color &color2_, float minw_ = 0, float minh_ = 0, int type_ = SOLID, int dir_ = VERTICAL)
        {
            FillColor::setup(color_, minw_, minh_, type_, dir_);
            colors.add(color2_);
        }

        static const char *typestr() { return "#Gradient"; }
        const char *gettype() const { return typestr(); }
    };

    ICOMMAND(0, uivgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::SOLID, Gradient::VERTICAL), children));

    ICOMMAND(0, uimodvgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::MODULATE, Gradient::VERTICAL), children));

    ICOMMAND(0, uihgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::SOLID, Gradient::HORIZONTAL), children));

    ICOMMAND(0, uimodhgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::MODULATE, Gradient::HORIZONTAL), children));

    struct Line : Target
    {
        static const char *typestr() { return "#Line"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            colors[0].init();
            gle::begin(GL_LINES);
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy+h);
            gle::end();

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(0, uiline, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(Line, o, o->setup(*minw*uiscale, *minh*uiscale, Color(*c)), children));

    struct Outline : Target
    {
        static const char *typestr() { return "#Outline"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            colors[0].init();
            gle::begin(GL_LINE_LOOP);
            gle::attribf(sx+(w*getcoord(FC_TL, 0)), sy+(h*getcoord(FC_TL, 1))); // 0
            gle::attribf(sx+(w*getcoord(FC_TR, 0)), sy+(h*getcoord(FC_TR, 1))); // 1
            gle::attribf(sx+(w*getcoord(FC_BR, 0)), sy+(h*getcoord(FC_BR, 1))); // 2
            gle::attribf(sx+(w*getcoord(FC_BL, 0)), sy+(h*getcoord(FC_BL, 1))); // 3
            gle::end();

            Object::draw(sx, sy);
        }
    };

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

    struct Composite : Target
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
            if(!shdr) shdr = hudshader;
            params.setsize(0);
            texs.setsize(0);
        }

        static const char *typestr() { return "#Composite"; }
        const char *gettype() const { return typestr(); }
        bool iscomp() const { return true; }

        void startdraw()
        {
            shdr->set();
            gle::defvertex(2);
            gle::deftexcoord0();
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            resetblend();

            LOCALPARAMF(millis, lastmillis/1000.0f);
            LOCALPARAMF(viewsize, hudw*w, hudh*h, 1.0f/(hudw*w), 1.0f/(hudh*h));
            LOCALPARAMF(composite, sx, sy, w, h);

            vector<LocalShaderParam> list;
            loopv(params)
            {
                LocalShaderParam param = list.add(LocalShaderParam(params[i].name));
                param.set(params[i].value); // automatically converts to correct type
            }
            loopv(texs)
            {
                glActiveTexture_(GL_TEXTURE0+i);
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
        }
    };

    ICOMMAND(0, uicomp, "sffe", (char *name, float *minw, float *minh, uint *children), \
        BUILD(Composite, o, o->setup(name, *minw*uiscale, *minh*uiscale), children));

    UICMDT(Composite, comp, param, "sffff", (char *name, float *x, float *y, float *z, float *w),
    {
        if(!name || !*name) return;
        Composite::param *p = NULL;
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

    UICMDT(Composite, comp, tex, "s", (char *name),
    {
        if(!name || !*name || o->texs.length() >= 10) return;
        o->texs.add(textureload(name, 3, true, false, texgc));
    });

    struct Image : Target
    {
        static Texture *lasttex;
        static Color lastcolor;
        static GLenum lastmode;

        enum { CO_TL = 0, CO_TR, CO_TC, CO_BL, CO_BR, CO_BC, CO_ML, CO_MR, CO_MC, CO_MAX };

        Texture *tex;
        bool alphatarget, outline;
        float shadowsize;
        Color shadowcolor;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, bool outline_ = false, float shadowsize_ = 0.f)
        {
            Target::setup(minw_, minh_, color_, SOLID, VERTICAL);
            tex = tex_;
            alphatarget = alphatarget_;
            outline = outline_;
            shadowsize = shadowsize_;
            shadowcolor = Color(color_).scale(Color(0, 0, 0, 255));
        }

        void setup(Texture *tex_, const Color &color_, const Color &color2_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, int dir_ = VERTICAL, bool outline_ = false, float shadowsize_ = 0.f)
        {
            Target::setup(minw_, minh_, color_, SOLID, dir_);
            colors.add(color2_); // gradient version
            tex = tex_;
            alphatarget = alphatarget_;
            outline = outline_;
            shadowsize = shadowsize_;
            shadowcolor = Color(color_).scale(Color(0, 0, 0, 255));
        }

        static const char *typestr() { return "#Image"; }
        const char *gettype() const { return typestr(); }
        bool isimage() const { return true; }

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

        void bindtex(GLenum mode = GL_QUADS, int colstart = 0, bool forced = false)
        {
            changedraw(CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();
            int col = clamp(colstart, -1, colors.length()-1);
            Color c = col >= 0 ? (colors.inrange(col) ? colors[col] : colors[0]) : Color(colors[0]).scale(shadowcolor);
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
            if(!shading && outline)
            {
                changedraw(CHANGE_SHADER);
                hudoutlineshader->set();
                LOCALPARAMF(textparams, 0.15f, 0.35f, 0.35f, 0.55f);
            }
            if(!shading && cols >= 2)
            {
                bindtex(GL_TRIANGLE_STRIP, colstart, forced);
                float vr = 1/float(cols-1), vs = 0, vcx1 = 0, vcx2 = 0, vcy1 = 0, vcy2 = 0,
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
                    int color1 = i+colstart,
                        color2 = cols>1 ? color1+1 : color1;

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
                    vs += vr;
                }
            }
            else
            {
                bindtex(GL_QUADS, colstart, forced);
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
            BUILD(Image, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), Color(*c2), *a!=0, *minw*uiscale, *minh*uiscale, Image::VERTICAL), children)); \
        ICOMMAND(0, uiimagehgradient##name, "siiiffe", (char *texname, int *c, int *c2, int *a, float *minw, float *minh, uint *children), \
            BUILD(Image, o, o->setup(textureload(texname, value, true, false, texgc), Color(*c), Color(*c2), *a!=0, *minw*uiscale, *minh*uiscale, Image::HORIZONTAL), children)); \
        UICMDT(Image, image, tex##name, "s", (char *texname), if(texname && *texname) o->tex = textureload(texname, value, true, false, texgc)); \
        UICMDT(Image, image, alttex##name, "s", (char *texname), if(texname && *texname && o->tex == notexture) o->tex = textureload(texname, value, true, false, texgc));

    UIIMGCMDS(, 3);
    UIIMGCMDS(clamped, 0x7000);
    UIARGTB(Image, image, alphatarget);
    UIARGTB(Image, image, outline);
    UICMDT(Image, image, shadow, "fi", (float *s, int *c),
    {
        o->shadowsize = clamp(*s, FVAR_MIN, FVAR_MAX);
        o->shadowcolor = Color(*c);
    });
    UIARGT(Image, image, shadowsize, "f", float, FVAR_MIN, FVAR_MAX);
    UICMDT(Image, image, shadowcolour, "i", (int *c), o->shadowcolor = Color(*c));

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
                if(!shading && outline)
                {
                    changedraw(CHANGE_SHADER);
                    hudoutlineshader->set();
                    LOCALPARAMF(textparams, 0.15f, 0.35f, 0.35f, 0.55f);
                }
                bindtex(GL_QUADS, shading ? -1 : 0);

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
                if(!shading && outline)
                {
                    changedraw(CHANGE_SHADER);
                    hudoutlineshader->set();
                    LOCALPARAMF(textparams, 0.15f, 0.35f, 0.35f, 0.55f);
                }
                bindtex(GL_QUADS, shading ? -1 : 0);
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

            changedraw(CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

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
        void setup(const Color &color_, int type_ = SOLID, float minw_ = 0, float minh_ = 0)
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

        void setup(const Color &color_, float w = 0, float h = 0, int angle = 0, int type_ = SOLID)
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

            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            colors[0].init();
            gle::begin(type == OUTLINED ? GL_LINE_LOOP : GL_TRIANGLES);
            gle::attrib(vec2(sx, sy).add(a));
            gle::attrib(vec2(sx, sy).add(b));
            gle::attrib(vec2(sx, sy).add(c));
            gle::end();
        }
    };

    ICOMMAND(0, uitriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::SOLID), children));

    ICOMMAND(0, uitriangleoutline, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::OUTLINED), children));

    ICOMMAND(0, uimodtriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::MODULATE), children));

    struct Circle : Shape
    {
        float radius;

        void setup(const Color &color_, float size, int type_ = SOLID)
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

            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

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
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::SOLID), children));

    ICOMMAND(0, uicircleoutline, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::OUTLINED), children));

    ICOMMAND(0, uimodcircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::MODULATE), children));

    struct Text : Colored
    {
        float scale, wrap, tw, th, wlen, limit, rescale, growth;
        int align, pos, rotate;

        void setup(float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = 0, int pos_ = -1, float growth_ = 1, int rotate_ = 0)
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
        }

        static const char *typestr() { return "#Text"; }
        const char *gettype() const { return typestr(); }
        bool istext() const { return true; }

        float drawscale(float ss = 1) const { return scale / FONTH * ss; }

        virtual const char *getstr() const { return ""; }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_COLOR|CHANGE_SHADER);

            float k = drawscale(rescale), left = sx/k, top = sy/k;
            int a = TEXT_MODCOL;
            switch(align)
            {
                case -2:    a |= TEXT_NO_INDENT|TEXT_LEFT_JUSTIFY; break;
                case -1:    a |= TEXT_LEFT_JUSTIFY; break;
                case 0:     a |= TEXT_CENTERED; left += tw*k*0.5f; break;
                case 1:     a |= TEXT_RIGHT_JUSTIFY; left += tw*k; break;
                case 2:     a |= TEXT_NO_INDENT|TEXT_RIGHT_JUSTIFY; left += tw*k; break;
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
            draw_text(getstr(), 0, 0, colors[0].r, colors[0].g, colors[0].b, colors[0].a, a, pos, wlen, 1);
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
                wlen = 0-wrap;
                for(Object *o = this->parent; o != NULL; o = o->parent)
                {
                    if(o->istype<Padder>())
                    {
                        wp += ((Padder *)o)->left+((Padder *)o)->right;
                        continue;
                    }
                    float ww = o->w;
                    if(o->isfill()) ww = max(ww, ((Filler *)o)->minw);
                    if(ww > 0)
                    {
                        wlen *= (ww-wp)/k;
                        break;
                    }
                    if(o->istype<Window>()) break;
                }
            }
            else wlen = 0;
            int a = TEXT_NO_INDENT|TEXT_MODCOL;
            switch(align)
            {
                case -2:    a |= TEXT_NO_INDENT|TEXT_LEFT_JUSTIFY; break;
                case -1:    a |= TEXT_LEFT_JUSTIFY; break;
                case 0:     a |= TEXT_CENTERED; break;
                case 1:     a |= TEXT_RIGHT_JUSTIFY; break;
                case 2:     a |= TEXT_NO_INDENT|TEXT_RIGHT_JUSTIFY; break;
            }
            text_boundsf(getstr(), tw, th, 0, 0, wlen, a);
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

    ICOMMAND(0, uitextfill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw*uiscale * uitextscale*0.5f, *minh*uiscale * uitextscale), children));

    ICOMMAND(0, uitext, "tfe", (tagval *text, float *scale, uint *children),
        buildtext(*text, *scale, uitextscale, Color(colourwhite), children));

    ICOMMAND(0, uicolourtext, "tife", (tagval *text, int *c, float *scale, uint *children),
        buildtext(*text, *scale, uitextscale, Color(*c), children));

    struct Font : Object
    {
        char *str;

        Font() : str(NULL) {}
        ~Font() { delete[] str; }

        void setup(const char *str_)
        {
            Object::setup();
            SETSTR(str, str_);
        }

        void layout()
        {
            pushfont(str);
            Object::layout();
            popfont();
        }

        void draw(float sx, float sy)
        {
            pushfont(str);
            Object::draw(sx, sy);
            popfont();
        }

        void buildchildren(uint *contents)
        {
            pushfont(str);
            Object::buildchildren(contents);
            popfont();
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                pushfont(str); \
                Object::func##children(cx, cy, cinside, mask, mode, setflags); \
                popfont(); \
            }
        DOSTATES
        #undef DOSTATE

        bool rawkey(int code, bool isdown)
        {
            pushfont(str);
            bool result = Object::rawkey(code, isdown);
            popfont();
            return result;
        }

        bool key(int code, bool isdown)
        {
            pushfont(str);
            bool result = Object::key(code, isdown);
            popfont();
            return result;
        }

        bool textinput(const char *str, int len)
        {
            pushfont(str);
            bool result = Object::textinput(str, len);
            popfont();
            return result;
        }
    };

    ICOMMAND(0, uifont, "se", (char *name, uint *children),
        BUILD(Font, o, o->setup(name), children));

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

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
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
        bool inverted;

        Clipper() : offsetx(0), offsety(0), inverted(false) {}

        void setup(float sizew_ = 0, float sizeh_ = 0, float offsetx_ = -1, float offsety_ = -1)
        {
            Object::setup();
            sizew = sizew_;
            sizeh = sizeh_;
            if(offsetx_ >= 0) offsetx = offsetx_;
            if(offsety_ >= 0) offsety = offsety_;
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
            if(sizew) w = min(w, sizew);
            if(sizeh) h = min(h, sizeh);
            offsetx = min(offsetx, hlimit());
            offsety = min(offsety, vlimit());
        }

        void adjustchildren()
        {
            adjustchildrento(0, 0, virtw, virth);
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, bool cinside, int mask, int mode, int setflags) \
            { \
                cx += offsetx; \
                cy += offsety; \
                if(cx < virtw && cy < virth) Object::func##children(cx, cy, true, mask, mode, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void draw(float sx, float sy)
        {
            if((sizew && virtw > sizew) || (sizeh && virth > sizeh))
            {
                float drawx, drawy;

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

        void addhscroll(float hscroll) { sethscroll(offsetx + (hscroll * (inverted ? -1 : 1))); }
        void addvscroll(float vscroll) { setvscroll(offsety + (vscroll * (inverted ? -1 : 1))); }
        void sethscroll(float hscroll) { offsetx = clamp(hscroll, 0.0f, hlimit()); }
        void setvscroll(float vscroll) { offsety = clamp(vscroll, 0.0f, vlimit()); }
    };

    ICOMMAND(0, uiclip, "ffgge", (float *sizew, float *sizeh, float *offsetx, float *offsety, uint *children),
        BUILD(Clipper, o, o->setup(*sizew*uiscale, *sizeh*uiscale, *offsetx*uiscale, *offsety*uiscale), children));

    UIARGSCALEDT(Clipper, clip, sizew, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, sizeh, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, virtw, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, virth, "f", float, 0.f, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, offsetx, "f", float, FVAR_MIN, FVAR_MAX);
    UIARGSCALEDT(Clipper, clip, offsety, "f", float, FVAR_MIN, FVAR_MAX);
    UIARGT(Clipper, clip, inverted, "i", int, 0, 1);

    struct Scroller : Clipper
    {
        bool scrolllock;

        void setup(float sizew_ = 0, float sizeh_ = 0)
        {
            Clipper::setup(sizew_, sizeh_);
            scrolllock = false;
        }

        static const char *typestr() { return "#Scroller"; }
        const char *gettype() const { return typestr(); }

        void scrollup(float cx, float cy, bool inside);
        void scrolldown(float cx, float cy, bool inside);
        bool canscroll() const { return !scrolllock && !surfaces[cursurface]->lockscroll; }
        void setscrolllock(bool scrolllock_) { scrolllock = scrolllock_; }
    };

    ICOMMAND(0, uiscroll, "ffe", (float *sizew, float *sizeh, uint *children),
        BUILD(Scroller, o, o->setup(*sizew*uiscale, *sizeh*uiscale), children));

    ICOMMANDNS(0, "uicanscroll-", uicanscroll_, "i", (int *canscroll),
    {
        if(buildparent && buildchild > 0)
        {
            Object *o = buildparent->children[buildchild-1];
            if(o->istype<Scroller>())
                ((Scroller*)(buildparent->children[buildchild-1]))->setscrolllock(*canscroll == 0);
        }
    });

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

        void arrowscroll(float dir) { addscroll(dir*curtime/1000.0f); }
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
            laststep = totalmillis + 2*uislidersteptime;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }

        void hold(float cx, float cy, bool inside)
        {
            if(totalmillis < laststep + uislidersteptime)
                return;
            laststep = totalmillis;

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
        static TextEditor *focus;

        float scale, offsetx, offsety;
        editor *edit;
        char *keyfilter;
        bool canfocus, allowlines;

        TextEditor() : edit(NULL), keyfilter(NULL), canfocus(true), allowlines(true) {}

        bool isallowed() const { return inputsurface(); }
        bool iseditor() const { return true; }

        void setup(const char *name, int length, int height, float scale_ = 1, const char *initval = NULL, int mode = EDITORUSED, const char *keyfilter_ = NULL, bool _allowlines = true, int limit = 0)
        {
            Colored::setup(Color(colourwhite));
            editor *edit_ = useeditor(name, mode, false, initval);
            if(edit_ != edit)
            {
                if(edit) clearfocus();
                edit = edit_;
            }
            if(initval && edit->mode == EDITORFOCUSED && !isfocus()) edit->clear(initval);
            edit->active = true;
            edit->linewrap = length < 0;
            edit->maxx = edit->linewrap ? -1 : length;
            edit->maxy = height <= 0 ? 1 : -1;
            edit->pixelwidth = abs(length)*FONTMW;
            edit->limit = limit;
            if(edit->linewrap && edit->maxy == 1) edit->updateheight();
            else edit->pixelheight = FONTH*max(height, 1);
            scale = scale_;
            if(keyfilter_ && *keyfilter_) SETSTR(keyfilter, keyfilter_);
            else DELETEA(keyfilter);
            allowlines = _allowlines;
        }
        ~TextEditor()
        {
            clearfocus();
            DELETEA(keyfilter);
        }

        static void setfocus(TextEditor *e)
        {
            if(focus == e) return;
            focus = e;
            if(e) inputsteal = e;
            bool allowtextinput = focus!=NULL && focus->allowtextinput();
            ::textinput(allowtextinput, TI_UI);
            ::keyrepeat(allowtextinput, KR_UI);
        }
        void setfocus() { setfocus(this); }
        void setfocusable(bool focusable) { canfocus = focusable; }
        void clearfocus() { if(focus == this) setfocus(NULL); }
        bool isfocus() const { return focus == this; }

        static const char *typestr() { return "#TextEditor"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        float drawscale() const { return scale / FONTH; }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_COLOR | CHANGE_SHADER);

            edit->rendered = true;

            float k = drawscale();
            pushhudtranslate(sx, sy, k);

            edit->draw(FONTW/2, 0, colors[0].tohexcolor(), colors[0].a, isfocus());

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

    TextEditor *TextEditor::focus = NULL;
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

        bool isallowed() const { return inputsurface(); }
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

        static void setfocus(KeyCatcher *kc)
        {
            if(focus == kc) return;
            focus = kc;
            if(kc)
            {
                inputsteal = kc;
                kc->focusmillis = lastmillis;
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
                keymillis = lastmillis;
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
            changedraw(CHANGE_SHADER | CHANGE_COLOR);

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
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);

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
        bool dragging, interactive;

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
            interactive = false;
            translate = vec(0, 0, 0);
        }

        bool ispreview() const { return true; }

        void startdraw()
        {
            glDisable(GL_BLEND);

            if(clipstack.length()) glDisable(GL_SCISSOR_TEST);
        }

        void enddraw()
        {
            glEnable(GL_BLEND);

            if(clipstack.length()) glEnable(GL_SCISSOR_TEST);
        }

        void hold(float cx, float cy, bool inside)
        {
            if(!interactive) return;
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
    UICMDT(Preview, preview, interactive, "i", (int *c), o->interactive = *c != 0);
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

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window[getwindowtype()]->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, pitch+offsetpitch, roll, fov, false, clipstack.length() > 0, translate);
            model *m = loadmodel(name);
            if(m)
            {
                //loopi(min(colors.length(), int(MAXMDLMATERIALS))) mdl.material[i] = bvec(colors[i].r, colors[i].g, colors[i].b);
                vec center, radius;
                m->boundbox(center, radius);
                if(yaw >= 0) mdl.yaw = yaw;
                mdl.o = calcmodelpreviewpos(radius, mdl.yaw).sub(center);
                mdl.yaw += offsetyaw;
                rendermodel(name, mdl);
            }
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end(skycol, suncol, sundir, excol, exdir);

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
    UICMDT(ModelPreview, modelpreview, interactive, "i", (int *c), o->interactive = *c != 0);
    UICMDT(ModelPreview, modelpreview, resetoffset, "", (void), o->resetoffset());
    UICMDT(ModelPreview, modelpreview, colour, "fffg", (float *r, float *g, float *b, float *a), o->mdl.color = vec4(*r, *g, *b, *a >= 0 ? *a : 1.f));
    UICMDT(ModelPreview, modelpreview, basetime, "bb", (int *b, int *c), o->mdl.basetime = *b >= 0 ? *b : lastmillis; o->mdl.basetime2 = *c >= 0 ? *c : 0);
    UICMDT(ModelPreview, modelpreview, material, "iiii", (int *mat, int *r, int *g, int *b), if(*mat >= 0 && *mat < MAXMDLMATERIALS) o->mdl.material[*mat] = bvec(*r, *g, *b));
    UICMDT(ModelPreview, modelpreview, mixercolour, "fffg", (float *r, float *g, float *b, float *a), o->mdl.mixercolor = vec4(*r, *g, *b, *a >= 0 ? *a : 1.f));
    UICMDT(ModelPreview, modelpreview, matbright, "ff", (float *x, float *y), o->mdl.matbright = vec2(*x, *y));
    UICMDT(ModelPreview, modelpreview, mixerglow, "ff", (float *x, float *y), o->mdl.mixerglow = vec2(*x, *y));
    UICMDT(ModelPreview, modelpreview, mixerscroll, "ff", (float *x, float *y), o->mdl.mixerscroll = vec2(*x, *y));
    UICMDT(ModelPreview, modelpreview, patternscale, "f", (float *n), o->mdl.patternscale = *n);
    UICMDT(ModelPreview, modelpreview, mixer, "s", (const char *texname), o->mdl.mixer = textureload(texname, 3, true, false));
    UICMDT(ModelPreview, modelpreview, pattern, "s", (const char *texname), o->mdl.pattern = textureload(texname, 3, true, false));

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

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window[getwindowtype()]->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);

            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, pitch+offsetpitch, roll, fov, false, clipstack.length() > 0, translate);

            colors[0].a = uchar(colors[0].a*blend);
            game::renderplayerpreview(scale, colors[0].tocolor4(), actions, yaw, offsetyaw);
            if(clipstack.length()) clipstack.last().scissor();
            // Steal the matrix for calculating positions on the model
            lastmatrix = camprojmatrix;

            modelpreview::end(skycol, suncol, sundir, excol, exdir);

            Object::draw(sx, sy);
        }

        vec2 vanityscreenpos(int vanity)
        {
            vec pos2d;
            lastmatrix.transform(game::playerpreviewvanitypos(vanity), pos2d);
            if(pos2d.z <= 0) return vec2(0, 0);

            pos2d.div(pos2d.z);

            int sx1, sy1, sx2, sy2;
            window[getwindowtype()]->calcscissor(lastsx, lastsy, lastsx+lastw, lastsy+lasth, sx1, sy1, sx2, sy2, false);

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

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window[getwindowtype()]->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, pitch, roll, fov, false, clipstack.length() > 0);
            previewprefab(name, colors[0].tocolor(), blend*(colors[0].a/255.f), yaw, offsetyaw);
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end(skycol, suncol, sundir, excol, exdir);
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
                    if(totalmillis - lastthumbnail < uislotviewtime) t = textureload(uiloadtex);
                    else
                    {
                        slot.loadthumbnail();
                        lastthumbnail = totalmillis;
                    }
                }
                if(slot.thumbnail && slot.thumbnail != notexture) t = slot.thumbnail;
            }

            if(!t || t == notexture) return;

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

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
            if(slot.loaded) gle::colorf(colorscale.x*colors[0].r/255.f, colorscale.y*colors[0].g/255.f, colorscale.z*colors[0].b/255.f, colors[0].a/255.f);
            else gle::colorf(1, 1, 1);
            quad(x, y, w, h, tc);
            if(detailtex)
            {
                settexture(detailtex);
                quad(x + w/2, y + h/2, w/2, h/2, tc);
            }
            if(glowtex)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                settexture(glowtex);
                vec glowcolor = vslot.getglowcolor();
                gle::colorf(glowcolor.x*colors[0].r/255.f, glowcolor.y*colors[0].g/255.f, glowcolor.z*colors[0].b/255.f, colors[0].a/255.f);
                quad(x, y, w, h, tc);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            if(layertex)
            {
                vec layerscale = layer->getcolorscale();
                settexture(layertex);
                gle::colorf(layerscale.x*colors[0].r/255.f, layerscale.y*colors[0].g/255.f, layerscale.z*colors[0].g/255.f, colors[0].a/255.f);
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
                    if(totalmillis - lastthumbnail < uislotviewtime) t = textureload(uiloadtex);
                    else
                    {
                        slot.loadthumbnail();
                        lastthumbnail = totalmillis;
                    }
                }
                if(slot.thumbnail && slot.thumbnail != notexture) t = slot.thumbnail;
            }

            if(!t || t == notexture) return;

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

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
            if(slot.loaded) gle::colorf(colorscale.x*colors[0].r/255.f, colorscale.y*colors[0].g/255.f, colorscale.z*colors[0].b/255.f, colors[0].a/255.f);
            else gle::colorf(1, 1, 1, 1);
            quad(x, y, w, h, tc);
            if(glowtex)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                settexture(glowtex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                vec glowcolor = vslot.getglowcolor();
                gle::colorf(glowcolor.x*colors[0].r/255.f, glowcolor.y*colors[0].g/255.f, glowcolor.z*colors[0].b/255.f, colors[0].a/255.f);
                quad(x, y, w, h, tc);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        Texture *tex;
        float dist, border;

        void setup(Texture *tex_, const Color &color_, float dist_ = 0, float border_ = 0.05f, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_, color_, SOLID, VERTICAL);
            colors.add(Color(colourwhite));
            tex = tex_;
            dist = dist_;
            border = border_;
        }

        void setup(Texture *tex_, const Color &color_, const Color &color2_, float dist_ = 0, float border_ = 0.05f, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_, color_, SOLID, VERTICAL);
            colors.add(color2_); // minimap version
            tex = tex_;
            dist = dist_;
            border = border_;
        }

        static const char *typestr() { return "#MiniMap"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_COLOR);
            while(colors.length() < 2) colors.add(Color(colourwhite));
            if(hud::needminimap())
            {
                vec pos = vec(camera1->o).sub(minimapcenter).mul(minimapscale).add(0.5f), dir(camera1->yaw*RAD, 0.f);
                int limit = hud::radarlimit();
                float scale = min(dist > 0 ? dist : float(worldsize), limit > 0 ? limit : float(worldsize)),
                      qw = w*0.5f*border, qh = h*0.5f*border, rw = w*0.5f-qw, rh = h*0.5f-qh;
                colors[1].init();
                gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_TRIANGLE_FAN);
                bindminimap();
                loopi(32)
                {
                    vec v = vec(0, -1, 0).rotate_around_z(i/32.0f*2*M_PI);
                    gle::attribf(sx + qw + rw*(1.0f + v.x), sy + qh + rh*(1.0f + v.y));
                    vec tc = vec(dir).rotate_around_z(i/32.0f*2*M_PI);
                    gle::attribf(1.0f - (pos.x + tc.x*scale*minimapscale.x), pos.y + tc.y*scale*minimapscale.y);
                }
                gle::end();
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

    struct Radar : Target
    {
        float dist, offset, border;

        void setup(float dist_ = 1, float offset_ = 0, float border_ = 0, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_);
            dist = dist_;
            offset = clamp(offset_, 0.f, 1.f);
            border = clamp(border_, 0.f, 1.f);
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

    struct RadarBlip : Image
    {
        float yaw, blipx, blipy, texx, texy, dist, blipyaw;
        uchar blipadjust;

        void setup(Texture *tex_, const Color &color_, float yaw_ = 0, float blipyaw_ = 0, float dist_ = 0, float minw_ = 0, float minh_ = 0)
        {
            Image::setup(tex_, color_, true, minw_, minh_);
            yaw = yaw_; // direction in which the blip is
            blipyaw = blipyaw_; // rotation of the blip itself
            dist = dist_; // how far away the blip is
            blipx = blipy = texx = texy = 0;
            blipadjust = ALIGN_DEFAULT;
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
            texx = sinf(RAD*blipyaw);
            texy = -cosf(RAD*blipyaw);
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
                float bw = w*0.5f, bh = h*0.5f, rw = r->w*0.5f, rh = r->h*0.5f,
                      rx = sx+(blipx*(rw-(rw*r->border))*clamp(dist/max(r->dist, 1.f), r->offset, 1.f)),
                      ry = sy+(blipy*(rh-(rh*r->border))*clamp(dist/max(r->dist, 1.f), r->offset, 1.f));

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
                    anrm.normalize().mul(vec2(bw, bh)).rotate_around_z(yaw*RAD);
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
                        vec2 norm;
                        float tx = 0, ty = 0;
                        switch(k)
                        {
                            case 0: vecfromyaw(blipyaw, -1, -1, norm);  tx = 0; ty = 0; break;
                            case 1: vecfromyaw(blipyaw, -1, 1, norm);   tx = 1; ty = 0; break;
                            case 2: vecfromyaw(blipyaw, 1, 1, norm);    tx = 1; ty = 1; break;
                            case 3: vecfromyaw(blipyaw, 1, -1, norm);   tx = 0; ty = 1; break;
                        }
                        norm.mul(vec2(bw, bh)).add(vec2(rx+bw, ry+bh));
                        gle::attrib(norm); gle::attribf(tx, ty);
                    }
                }
                Object::draw(rx+bbx*w*RAD, ry+bby*h*RAD); // don't descend unless we process the blip
            }
        }
    };

    ICOMMAND(0, uiradarblip, "sifffffe", (char *texname, int *c, float *yaw, float *blipyaw, float *dist, float *minw, float *minh, uint *children),
        BUILD(RadarBlip, o, o->setup(textureload(texname, 3, true, false, texgc), Color(*c), *yaw, *blipyaw, *dist, *minw*uiscale, *minh*uiscale), children));

    #define IFSTATEVAL(state,t,f) { if(state) { if(t->type == VAL_NULL) intret(1); else result(*t); } else if(f->type == VAL_NULL) intret(0); else result(*f); }
    #define DOSTATE(flags, func) \
        ICOMMANDNS(0, "ui!" #func, uinot##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->hasstate(flags) ? t : f)); \
        ICOMMANDNS(0, "ui" #func, ui##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->haschildstate(flags) ? t : f)); \
        ICOMMANDNS(0, "ui!" #func "?", uinot##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->hasstate(flags), t, f)); \
        ICOMMANDNS(0, "ui" #func "?", ui##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->haschildstate(flags), t, f)); \
        ICOMMANDNS(0, "ui!" #func "+", uinextnot##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->hasstate(flags) ? t : f)); \
        ICOMMANDNS(0, "ui" #func "+", uinext##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->haschildstate(flags) ? t : f)); \
        ICOMMANDNS(0, "ui!" #func "+?", uinextnot##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->hasstate(flags), t, f)); \
        ICOMMANDNS(0, "ui" #func "+?", uinext##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->haschildstate(flags), t, f));
    DOSTATES
    #undef DOSTATE

    ICOMMANDNS(0, "uianyfocus", uifocus_, "", (),
        intret(buildparent && TextEditor::focus != NULL));
    ICOMMANDNS(0, "uifocus", uifocus_, "ee", (uint *t, uint *f),
        executeret(buildparent && TextEditor::focus == buildparent ? t : f));
    ICOMMANDNS(0, "uifocus?", uifocus__, "tt", (tagval *t, tagval *f),
        IFSTATEVAL(buildparent && TextEditor::focus == buildparent, t, f));
    ICOMMANDNS(0, "uifocus+", uinextfocus_, "ee", (uint *t, uint *f),
        executeret(buildparent && buildparent->children.inrange(buildchild) && TextEditor::focus == buildparent->children[buildchild] ? t : f));
    ICOMMANDNS(0, "uifocus+?", uinextfocus__, "tt", (tagval *t, tagval *f),
        IFSTATEVAL(buildparent && buildparent->children.inrange(buildchild) && TextEditor::focus == buildparent->children[buildchild], t, f));
    ICOMMANDNS(0, "uifocus-", uinextfocus_, "ee", (uint *t, uint *f),
        executeret(buildparent && buildchild > 0 && buildparent->children.inrange(buildchild-1) && TextEditor::focus == buildparent->children[buildchild-1] ? t : f));
    ICOMMANDNS(0, "uifocus-?", uinextfocus__, "tt", (tagval *t, tagval *f),
        IFSTATEVAL(buildparent && buildchild > 0 && buildparent->children.inrange(buildchild-1) && TextEditor::focus == buildparent->children[buildchild-1], t, f));

    ICOMMAND(0, uidrawn, "", (),
        intret(buildparent && buildparent->drawn));

    ICOMMAND(0, uialign, "ii", (int *xalign, int *yalign),
    {
        if(buildparent) buildparent->setalign(*xalign, *yalign);
    });
    ICOMMANDNS(0, "uialign-", uialign_, "ii", (int *xalign, int *yalign),
    {
        if(buildparent && buildchild > 0) buildparent->children[buildchild-1]->setalign(*xalign, *yalign);
    });
    ICOMMANDNS(0, "uialign*", uialign__, "ii", (int *xalign, int *yalign),
    {
        if(buildparent) loopi(buildchild) buildparent->children[i]->setalign(*xalign, *yalign);
    });

    ICOMMAND(0, uiclamp, "iiii", (int *left, int *right, int *top, int *bottom),
    {
        if(buildparent) buildparent->setclamp(*left, *right, *top, *bottom);
    });
    ICOMMANDNS(0, "uiclamp-", uiclamp_, "iiii", (int *left, int *right, int *top, int *bottom),
    {
        if(buildparent && buildchild > 0) buildparent->children[buildchild-1]->setclamp(*left, *right, *top, *bottom);
    });
    ICOMMANDNS(0, "uiclamp*", uiclamp__, "iiii", (int *left, int *right, int *top, int *bottom),
    {
        if(buildparent) loopi(buildchild) buildparent->children[i]->setclamp(*left, *right, *top, *bottom);
    });

    ICOMMAND(0, uiposition, "ff", (float *x, float *y),
    {
        if(buildparent) buildparent->setpos(*x, *y);
    });

    ICOMMANDNS(0, "uiposition-", uiposition_, "ff", (float *x, float *y),
    {
        if(buildparent) loopi(buildchild) buildparent->children[i]->setpos(*x, *y);
    });

    ICOMMAND(0, uigetlastx,  "", (), if(buildparent) floatret(buildparent->lastx));
    ICOMMAND(0, uigetlasty,  "", (), if(buildparent) floatret(buildparent->lasty));
    ICOMMAND(0, uigetlastsx, "", (), if(buildparent) floatret(buildparent->lastsx));
    ICOMMAND(0, uigetlastsy, "", (), if(buildparent) floatret(buildparent->lastsy));
    ICOMMAND(0, uigetlastw,  "", (), if(buildparent) floatret(buildparent->lastw));
    ICOMMAND(0, uigetlasth,  "", (), if(buildparent) floatret(buildparent->lasth));

    ICOMMANDNS(0, "uigetlastx-",  uigetlastx_,  "", (),
        if(buildparent && buildchild > 0) floatret(buildparent->children[buildchild-1]->lastx));
    ICOMMANDNS(0, "uigetlasty-",  uigetlasty_,  "", (),
        if(buildparent && buildchild > 0) floatret(buildparent->children[buildchild-1]->lasty));
    ICOMMANDNS(0, "uigetlastsx-", uigetlastsx_, "", (),
        if(buildparent && buildchild > 0) floatret(buildparent->children[buildchild-1]->lastsx));
    ICOMMANDNS(0, "uigetlastsy-", uigetlastsy_, "", (),
        if(buildparent && buildchild > 0) floatret(buildparent->children[buildchild-1]->lastsy));
    ICOMMANDNS(0, "uigetlastw-",  uigetlastw_,  "", (),
        if(buildparent && buildchild > 0) floatret(buildparent->children[buildchild-1]->lastw));
    ICOMMANDNS(0, "uigetlasth-",  uigetlasth_,  "", (),
        if(buildparent && buildchild > 0) floatret(buildparent->children[buildchild-1]->lasth));

    #define UICOLOURCMDS(t) \
        if(o->iscolour()) \
        { \
            loopvk(((Colored *)o)->colors) ((Colored *)o)->colors[k] = Color(*c); \
            t; \
        } \

    UIREVCMDC(setcolour, "i", (int *c), UICOLOURCMDS(break));
    void setchildcolours(Object *o, int *c)
    {
        UICOLOURCMDS();
        loopv(o->children) setchildcolours(o->children[i], c);
    }
    UIWINCMDC(setcolours, "i", (int *c), setchildcolours(o, c));

    #define UIBLENDCMDS(t) \
        if(o->iscolour()) \
        { \
            loopvk(((Colored *)o)->colors) ((Colored *)o)->colors[k].a = clamp(int(*c * ((Colored *)o)->colors[k].a), 0, 255); \
            t; \
        } \

    UIREVCMDC(changeblend, "f", (float *c), UIBLENDCMDS(break));
    void changechildblends(Object *o, float *c)
    {
        UIBLENDCMDS();
        loopv(o->children) changechildblends(o->children[i], c);
    }
    UIWINCMDC(changeblends, "f", (float *c), changechildblends(o, c));

    #define UICHGCOLCMDS(t) \
        if(o->iscolour()) \
        { \
            loopvk(((Colored *)o)->colors) \
            { \
                ((Colored *)o)->colors[k].r = clamp(int(*c * ((Colored *)o)->colors[k].r), 0, 255); \
                ((Colored *)o)->colors[k].g = clamp(int(*c * ((Colored *)o)->colors[k].g), 0, 255); \
                ((Colored *)o)->colors[k].b = clamp(int(*c * ((Colored *)o)->colors[k].b), 0, 255); \
            } \
            t; \
        } \

    UIREVCMDC(changecolour, "f", (float *c), UICHGCOLCMDS(break));
    void changechildcolours(Object *o, float *c)
    {
        UICHGCOLCMDS();
        loopv(o->children) changechildcolours(o->children[i], c);
    }
    UIWINCMDC(changecolours, "f", (float *c), changechildcolours(o, c));

    #define UIROTCOLCMDS(t) \
        if(o->iscolour()) \
        { \
            ((Colored *)o)->rotatecolors(*amt, *start, *count); \
            t; \
        } \

    UIREVCMDC(rotatecolour, "fii", (float *amt, int *start, int *count), UIROTCOLCMDS(break));
    void rotchildcolours(Object *o, float *amt, int *start, int *count)
    {
        UIROTCOLCMDS();
        loopv(o->children) rotchildcolours(o->children[i], amt, start, count);
    }
    UIWINCMDC(rotatecolours, "fii", (float *amt, int *start, int *count), rotchildcolours(o, amt, start, count));

    int hasinput()
    {
        return cursurface >= 0 && surfaces[cursurface] ? surfaces[cursurface]->allowinput() : 0;
    }

    bool hasmenu(bool pass)
    {
        return cursurface >= 0 && surfaces[cursurface] && surfaces[cursurface]->hasmenu(pass);
    }

    bool keypress(int code, bool isdown)
    {
        if(cursurface < 0 || !surfaces[cursurface]) return false;
        if(surfaces[cursurface]->rawkey(code, isdown)) return true;
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
        int setmode = inputsteal && action != STATE_SCROLL_UP && action != STATE_SCROLL_DOWN ?
            SETSTATE_FOCUSED : SETSTATE_INSIDE;

        if(action)
        {
            if(isdown)
            {
                if(hold) surfaces[cursurface]->clearstate(hold);
                if(surfaces[cursurface]->setstate(action, cursorx, cursory, 0, setmode, action|hold)) return true;
            }
            else if(hold)
            {
                if(surfaces[cursurface]->setstate(action, cursorx, cursory, hold, setmode, action))
                {
                    surfaces[cursurface]->clearstate(hold);
                    return true;
                }
                surfaces[cursurface]->clearstate(hold);
            }
        }
        return surfaces[cursurface]->key(code, isdown);
    }

    bool textinput(const char *str, int len)
    {
        return cursurface >= 0 && surfaces[cursurface] && surfaces[cursurface]->textinput(str, len);
    }

    void setup()
    {
        loopi(SURFACE_MAX) surfaces[i] = new Surface;
        cursurface = SURFACE_MAIN;
        inputsteal = NULL;
    }

    void cleanup()
    {
        loopi(SURFACE_MAX) surfaces[i]->children.setsize(0);
        loopj(WINTYPE_MAX)
        {
            enumerate(windows[j], Window *, w, delete w);
            windows[j].clear();
        }
        cursurface = NULL;
        loopi(SURFACE_MAX) DELETEP(surfaces[i]);
        inputsteal = NULL;
    }

    void calctextscale()
    {
        uitextscale = 1.0f/uitextrows;
    }

    void update()
    {
        if(!progressing && uihidden) return;

        int oldsurface = cursurface;
        cursurface = progressing ? SURFACE_PROGRESS : SURFACE_MAIN;
        if(progressing) showui("progress");

        float oldtextscale = curtextscale;
        curtextscale = 1;
        surfaces[cursurface]->cursortype = CURSOR_DEFAULT;
        surfaces[cursurface]->cursorlocked = false;
        surfaces[cursurface]->mousetracking = false;
        surfaces[cursurface]->lockscroll = false;

        pushfont();
        readyeditors();

        surfaces[cursurface]->setstate(STATE_HOVER, cursorx, cursory, surfaces[cursurface]->childstate&STATE_HOLD_MASK);
        if(surfaces[cursurface]->childstate&STATE_HOLD) surfaces[cursurface]->setstate(STATE_HOLD, cursorx, cursory, STATE_HOLD, SETSTATE_ANY);
        if(surfaces[cursurface]->childstate&STATE_ALT_HOLD) surfaces[cursurface]->setstate(STATE_ALT_HOLD, cursorx, cursory, STATE_ALT_HOLD, SETSTATE_ANY);
        if(surfaces[cursurface]->childstate&STATE_ESC_HOLD) surfaces[cursurface]->setstate(STATE_ESC_HOLD, cursorx, cursory, STATE_ESC_HOLD, SETSTATE_ANY);

        calctextscale();

        if(*uiprecmd) execute(uiprecmd);
        surfaces[cursurface]->build();
        if(*uipostcmd) execute(uipostcmd);

        if(inputsteal && !inputsteal->isfocus())
            inputsteal = NULL;

        if(!surfaces[cursurface]->mousetracking) surfaces[cursurface]->mousetrackvec = vec2(0, 0);

        flusheditors();
        popfont();

        curtextscale = oldtextscale;
        cursurface = oldsurface;
    }

    void render()
    {
        if(!progressing && uihidden) return;

        int oldsurface = cursurface;
        cursurface = progressing ? SURFACE_PROGRESS : SURFACE_MAIN;

        float oldtextscale = curtextscale;
        curtextscale = 1;

        pushfont();
        surfaces[cursurface]->layout();
        surfaces[cursurface]->adjustchildren();
        surfaces[cursurface]->draw();
        popfont();

        curtextscale = oldtextscale;
        cursurface = oldsurface;
    }

    void cleancomposite()
    {
        if(comptexfbo) { glDeleteFramebuffers_(1, &comptexfbo); comptexfbo = 0; }
    }

    SVAR(0, uicompargs, "");
    bool composite(uint *tex, const char *name, const char *args, int w, int h, int tclamp, bool mipit, bool msg)
    {
        if(!name || !*name || !tex) return false; // need a name
        if(msg) progress(loadprogress, "Compositing texture: %s [%dx%d]", name, w, h);

        GLint oldfbo = 0;
        if(progressing || !inbetweenframes || drawtex) glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfbo);

        GLERROR;
        if(!comptexfbo) glGenFramebuffers_(1, &comptexfbo);
        glBindFramebuffer_(GL_FRAMEBUFFER, comptexfbo);

        bool hastex = *tex != 0;
        if(!hastex)
        {
            glGenTextures(1, tex);
            createtexture(*tex, w, h, NULL, tclamp, mipit ? 3 : 0, GL_RGBA, GL_TEXTURE_2D);
        }
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);

        GLERROR;
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            if(msg) conoutf("\frFailed allocating composite texture framebuffer");
            if(!hastex) glDeleteTextures(1, tex);
            glBindFramebuffer_(GL_FRAMEBUFFER, oldfbo);
            return false;
        }
        int olddrawtex = drawtex;
        drawtex = DRAWTEX_COMPOSITE;

        setsvar("uicompargs", args ? args : "");
        int oldsurface = cursurface;
        cursurface = SURFACE_COMPOSITE;
        showui(name);

        int oldhudw = hudw, oldhudh = hudh;

        hudw = w;
        hudh = h;
        glViewport(0, 0, hudw, hudh);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        float oldtextscale = curtextscale;
        curtextscale = 1;

        pushfont();
        calctextscale();
        surfaces[cursurface]->build();
        popfont();

        curtextscale = 1;
        pushfont();
        surfaces[cursurface]->layout();
        surfaces[cursurface]->adjustchildren();
        surfaces[cursurface]->draw();
        popfont();

        glActiveTexture_(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, *tex);
        glGenerateMipmap_(GL_TEXTURE_2D);

        curtextscale = oldtextscale;
        hudw = oldhudw;
        hudh = oldhudh;

        surfaces[cursurface]->hideall(true);
        cursurface = oldsurface;

        drawtex = olddrawtex;
        glBindFramebuffer_(GL_FRAMEBUFFER, oldfbo);
        glViewport(0, 0, hudw, hudh);

        return true;
    }

    void cleangl()
    {
        cleancomposite();
    }

    void mousetrack(float dx, float dy)
    {
        loopi(SURFACE_MAX) if(surfaces[i]) surfaces[i]->mousetrackvec.add(vec2(dx, dy));
    }

    bool cursorlock()
    {
        if(surfaces[SURFACE_MAIN]) return surfaces[SURFACE_MAIN]->cursorlocked;
        return false;
    }

    int cursortype()
    {
        if(surfaces[SURFACE_MAIN]) return surfaces[SURFACE_MAIN]->cursortype;
        return CURSOR_DEFAULT;
    }

    int savemapmenus(stream *h)
    {
        int mapmenus = 0;
        loopj(WINTYPE_MAX) enumerate(windows[j], Window *, w,
        {
            if(!w->mapdef || !w->body) continue;
            h->printf("map%sui %s [%s]\n", windowaffix[j], w->name, w->body);
            mapmenus++;
        });
        return mapmenus;
    }

    void resetmapmenus()
    {
        loopj(WINTYPE_MAX) enumerate(windows[j], Window *, w,
        {
            if(!w->mapdef || !w->body) continue;
            DELETEA(w->body);
        });
    }
}
