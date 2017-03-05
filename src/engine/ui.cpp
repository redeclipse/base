#include "engine.h"
#include "textedit.h"

namespace UI
{
    int cursortype = CURSOR_DEFAULT;

    FVAR(0, uitextscale, 1, 0, 0);
    FVAR(0, uiscale, 0, 1, 100);

    SVAR(0, uiopencmd, "showui");
    SVAR(0, uiclosecmd, "hideui");

    VAR(IDF_PERSIST, uitextrows, 1, 48, VAR_MAX);

    VAR(IDF_PERSIST, uiscrollsteptime, 0, 50, VAR_MAX);
    VAR(IDF_PERSIST, uislidersteptime, 0, 50, VAR_MAX);
    VAR(IDF_PERSIST, uislotviewtime, 0, 25, VAR_MAX);

    FVAR(IDF_PERSIST, uitipoffset, 0, 0.005f, 1);

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

    struct Object;

    static Object *buildparent = NULL;
    static int buildchild = -1;

    #define BUILD(type, o, setup, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            setup; \
            o->buildchildren(contents); \
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

    struct Object
    {
        Object *parent;
        float x, y, w, h, ox, oy;
        bool overridepos;
        uchar adjust;
        ushort state, childstate;
        vector<Object *> children;

        Object() : ox(0), oy(0), overridepos(false), adjust(0), state(0), childstate(0) {}
        virtual ~Object()
        {
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
            adjust = ALIGN_HCENTER | ALIGN_VCENTER;
        }

        virtual uchar childalign() const { return ALIGN_HCENTER | ALIGN_VCENTER; }

        void reset(Object *parent_)
        {
            resetlayout();
            parent = parent_;
            adjust = parent->childalign();
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
            loopchildren(o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
        }

        void resetstate()
        {
            state &= STATE_HOLD_MASK;
            childstate &= STATE_HOLD_MASK;
        }
        void resetchildstate()
        {
            resetstate();
            loopchildren(o, o->resetchildstate());
        }

        bool hasstate(int flags) const { return ((state & ~childstate) & flags) != 0; }
        bool haschildstate(int flags) const { return ((state | childstate) & flags) != 0; }

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

        bool setstate(int state, float cx, float cy, int mask = 0, bool inside = true, int setflags = 0)
        {
            switch(state)
            {
            #define DOSTATE(flags, func) case flags: func##children(cx, cy, mask, inside, setflags | flags); return haschildstate(flags);
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

        #define propagatestate(o, cx, cy, mask, inside, body) \
            loopchildrenrev(o, \
            { \
                if(((o->state | o->childstate) & mask) != mask) continue; \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                if(!inside) \
                { \
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
            virtual void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                propagatestate(o, cx, cy, mask, inside, \
                { \
                    o->func##children(ox, oy, mask, inside, setflags); \
                    childstate |= (o->state | o->childstate) & (setflags); \
                }); \
                if(target(cx, cy)) state |= (setflags); \
                func(cx, cy); \
            } \
            virtual void func(float cx, float cy) {}
        DOSTATES
        #undef DOSTATE

        static const char *typestr() { return "#Object"; }
        virtual const char *gettype() const { return typestr(); }
        virtual const char *getname() const { return gettype(); }
        virtual const char *gettypename() const { return gettype(); }

        template<class T> bool istype() const { return T::typestr() == gettype(); }
        bool isnamed(const char *name) const { return name[0] == '#' ? name == gettypename() : !strcmp(name, getname()); }

        virtual bool iswindow() const { return false; }
        virtual bool isfiller() const { return false; }
        virtual bool isspacer() const { return false; }
        virtual bool iscolor() const { return false; }
        virtual bool istext() const { return false; }
        virtual bool isimage() const { return false; }
        virtual bool iseditor() const { return false; }
        virtual bool isclipper() const { return false; }
        virtual bool isradar() const { return false; }

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

    static inline void stopdrawing()
    {
        if(drawing)
        {
            drawing->enddraw(0);
            drawing = NULL;
        }
    }

    struct Window;

    static Window *window = NULL;

    enum
    {
        WINDOW_NONE = 0,
        WINDOW_MENU = 1<<0, WINDOW_PASS = 1<<1, WINDOW_TIP = 1<<2, WINDOW_POPUP = 1<<3, WINDOW_PERSIST = 1<<4,
        WINDOW_ALL = WINDOW_MENU|WINDOW_PASS|WINDOW_TIP|WINDOW_POPUP|WINDOW_PERSIST
    };

    struct Window : Object
    {
        char *name;
        uint *contents, *onshow, *onhide;
        bool allowinput, exclusive;
        int windowflags;
        float px, py, pw, ph;
        vec2 sscale, soffset;

        Window(const char *name, const char *contents, const char *onshow, const char *onhide, int windowflags_) :
            name(newstring(name)),
            contents(compilecode(contents)),
            onshow(onshow && onshow[0] ? compilecode(onshow) : NULL),
            onhide(onhide && onhide[0] ? compilecode(onhide) : NULL),
            allowinput(true), exclusive(false),
            px(0), py(0), pw(0), ph(0),
            sscale(1, 1), soffset(0, 0)
        {
            windowflags = clamp(windowflags_, 0, int(WINDOW_ALL));
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
        bool iswindow() const { return true; }

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
            allowinput = true;
            exclusive = false;
            px = py = pw = ph = 0;
        }

        void layout()
        {
            if(state&STATE_HIDDEN) { w = h = 0; return; }
            window = this;
            Object::layout();
            window = NULL;
        }

        void draw(float sx, float sy)
        {
            if(state&STATE_HIDDEN) return;
            window = this;

            projection();
            hudshader->set();

            glEnable(GL_BLEND);
            blendtype = BLEND_ALPHA;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gle::colorf(1, 1, 1);

            changed = 0;
            drawing = NULL;

            Object::draw(sx, sy);

            stopdrawing();

            glDisable(GL_BLEND);

            window = NULL;
        }

        void draw()
        {
            draw(x, y);
        }

        void adjustchildren()
        {
            if(state&STATE_HIDDEN) return;
            window = this;
            Object::adjustchildren();
            window = NULL;
        }

        void adjustlayout()
        {
            float aspect = float(screenw)/screenh;
            ph = max(max(h, w/aspect), 1.0f);
            pw = aspect*ph;
            Object::adjustlayout(0, 0, pw, ph);
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                if(!allowinput || state&STATE_HIDDEN || pw <= 0 || ph <= 0) return; \
                cx = cx*pw + px-x; \
                cy = cy*ph + py-y; \
                if(!inside || (cx >= 0 && cy >= 0 && cx < w && cy < h)) \
                    Object::func##children(cx, cy, mask, inside, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void projection()
        {
            hudmatrix.ortho(px, px + pw, py + ph, py, -1, 1);
            resethudmatrix();
            sscale = vec2(hudmatrix.a.x, hudmatrix.b.y).mul(0.5f);
            soffset = vec2(hudmatrix.d.x, hudmatrix.d.y).mul(0.5f).add(0.5f);
        }

        void calcscissor(float x1, float y1, float x2, float y2, int &sx1, int &sy1, int &sx2, int &sy2, bool clip = true)
        {
            vec2 s1 = vec2(x1, y2).mul(sscale).add(soffset),
                 s2 = vec2(x2, y1).mul(sscale).add(soffset);
            sx1 = int(floor(s1.x*screenw + 0.5f));
            sy1 = int(floor(s1.y*screenh + 0.5f));
            sx2 = int(floor(s2.x*screenw + 0.5f));
            sy2 = int(floor(s2.y*screenh + 0.5f));
            if(clip)
            {
                sx1 = clamp(sx1, 0, screenw);
                sy1 = clamp(sy1, 0, screenh);
                sx2 = clamp(sx2, 0, screenw);
                sy2 = clamp(sy2, 0, screenh);
            }
        }
    };

    static hashnameset<Window *> windows;

    void ClipArea::scissor()
    {
        int sx1, sy1, sx2, sy2;
        window->calcscissor(x1, y1, x2, y2, sx1, sy1, sx2, sy2);
        glScissor(sx1, sy1, sx2-sx1, sy2-sy1);
    }

    struct World : Object
    {
        static const char *typestr() { return "#World"; }
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
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                loopwindowsrev(w, \
                { \
                    if(((w->state | w->childstate) & mask) != mask) continue; \
                    w->func##children(cx, cy, mask, inside, setflags); \
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
        }

        bool show(Window *w)
        {
            if(children.find(w) >= 0) return false;
            w->resetchildstate();
            children.add(w);
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

        int hideall()
        {
            int hidden = 0;
            loopwindowsrev(w,
            {
                if(w->windowflags&WINDOW_PERSIST) continue;
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

        bool allowinput() const
        {
            bool hasexcl = hasexclusive();
            loopwindows(w,
            {
                if(hasexcl && !w->exclusive) continue;
                if(w->allowinput && !(w->state&STATE_HIDDEN)) return true;
            });
            return false;
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
                    w->setpos((cursorx*float(screenw)/float(screenh))-(w->w*cursorx), cursory-w->h-uitipoffset);
                else if(w->windowflags&WINDOW_POPUP && !w->overridepos)
                    w->setpos((cursorx*float(screenw)/float(screenh))-(w->w*cursorx), cursory-w->h*0.5f);
            });
            loopwindows(w,
            {
                if(hasexcl && !w->exclusive) continue;
                w->draw();
            });
        }
    };

    static World *world = NULL;

    void Window::build()
    {
        reset(world);
        setup();
        window = this;
        buildchildren(contents);
        window = NULL;
    }

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
            for(int i = 0; i < children.length(); i++)
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

    struct TableHeader : Object
    {
        int columns;

        TableHeader() : columns(-1) {}

        static const char *typestr() { return "#TableHeader"; }
        const char *gettype() const { return typestr(); }

        uchar childalign() const { return columns < 0 ? ALIGN_VCENTER : ALIGN_HCENTER | ALIGN_VCENTER; }

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

    struct TableRow : TableHeader
    {
        static const char *typestr() { return "#TableRow"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }
    };

    #define BUILDCOLUMNS(type, o, setup, columndata, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            setup; \
            o->buildchildren(columndata, contents); \
        } \
    } while(0)

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

    struct Spacer : Object
    {
        float spacew, spaceh;

        void setup(float spacew_, float spaceh_)
        {
            Object::setup();
            spacew = spacew_;
            spaceh = spaceh_;
        }

        static const char *typestr() { return "#Spacer"; }
        const char *gettype() const { return typestr(); }
        bool isspacer() const { return true; }

        void layout()
        {
            w = spacew;
            h = spaceh;
            loopchildren(o,
            {
                o->x = spacew;
                o->y = spaceh;
                o->layout();
                w = max(w, o->x + o->w);
                h = max(h, o->y + o->h);
            });
            w += spacew;
            h += spaceh;
        }

        void adjustchildren()
        {
            adjustchildrento(spacew, spaceh, w - 2*spacew, h - 2*spaceh);
        }
    };

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
        bool isspacer() const { return true; }

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

        vec tocolor() const { return vec(r*(1.0f/255.0f), g*(1.0f/255.0f), b*(1.0f/255.0f)); }
        int tohexcolor() const { return (int(r)<<16)|(int(g)<<8)|int(b); }

        vec4 tocolor4() const { return vec4(r*(1.0f/255.0f), g*(1.0f/255.0f), b*(1.0f/255.0f), a*(1.0f/255.0f)); }
        int tohexcolor4() const { return (int(a)<<24)|(int(r)<<16)|(int(g)<<8)|int(b); }

        bool operator==(const Color &o) const { return mask == o.mask; }
        bool operator!=(const Color &o) const { return mask != o.mask; }
    };

    struct Filler : Object
    {
        float minw, minh;

        void setup(float minw_, float minh_)
        {
            Object::setup();
            minw = minw_;
            minh = minh_;
        }

        static const char *typestr() { return "#Filler"; }
        const char *gettype() const { return typestr(); }
        bool isfiller() const { return true; }

        void layout()
        {
            Object::layout();

            w = max(w, minw);
            h = max(h, minh);
        }
    };

    struct Target : Filler
    {
        static const char *typestr() { return "#Target"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }
    };

    struct TargetColor : Target
    {
        enum { SOLID = 0, MODULATE, OUTLINE };
        enum { VERTICAL, HORIZONTAL };

        int type, dir;
        vector<Color> colors;

        void setup(const Color &color_, float minw_ = 0, float minh_ = 0, int type_ = SOLID, int dir_ = VERTICAL)
        {
            Target::setup(minw_, minh_);
            colors.setsize(0);
            colors.add(color_);
            type = type_;
            dir = dir_;
        }

        void rotatecolors(float amt)
        {
            if(amt == 0 || colors.length() <= 1) return;
            int cols = colors.length(), pieces = 0;
            float progress = clamp(fabs(amt), 0.f, 1.f), part = 1.f/cols;
            while(progress >= part)
            {
                progress -= part;
                pieces++;
            }
            float iter = progress/part;
            static vector<Color> colorstack;
            colorstack.setsize(0);
            bool rev = amt < 0;
            loopv(colors)
            {
                int p = rev ? i-pieces : (i+pieces)%cols, m = p >= 0 ? p : cols+p, q = rev ? m-1 : (m+1)%cols, n = q >= 0 ? q : cols+q;
                colorstack.add(
                    Color(colors[m].r-int((colors[m].r-colors[n].r)*iter),
                          colors[m].g-int((colors[m].g-colors[n].g)*iter),
                          colors[m].b-int((colors[m].b-colors[n].b)*iter),
                          colors[m].a-int((colors[m].a-colors[n].a)*iter)
                         )
                    );
            }
            colors.setsize(0);
            loopv(colorstack) colors.add(colorstack[i]);
        }

        static const char *typestr() { return "#TargetColor"; }
        const char *gettype() const { return typestr(); }
        bool iscolor() const { return true; }
    };

    struct FillColor : TargetColor
    {
        void setup(const Color &color_, float minw_ = 0, float minh_ = 0, int type_ = SOLID, int dir_ = VERTICAL)
        {
            TargetColor::setup(color_, minw_, minh_, type_, dir_);
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
                float gw = dir == HORIZONTAL ? w/float(cols-1) : w,
                      gh = dir == VERTICAL ? h/float(cols-1) : h,
                      vx = sx, vy = sy;
                loopi(cols-1) switch(dir)
                {
                    case HORIZONTAL:
                        gle::attribf(vx,    vy);    colors[i].attrib();
                        gle::attribf(vx+gw, vy);    colors[i+1].attrib();
                        gle::attribf(vx,    vy+gh); colors[i].attrib();
                        gle::attribf(vx+gw, vy+gh); colors[i+1].attrib();
                        vx += gw;
                        break;
                    case VERTICAL:
                        gle::attribf(vx+gw, vy);    colors[i].attrib();
                        gle::attribf(vx+gw, vy+gh); colors[i+1].attrib();
                        gle::attribf(vx,    vy);    colors[i].attrib();
                        gle::attribf(vx,    vy+gh); colors[i+1].attrib();
                        vy += gh;
                        break;
                }
            }
            else
            {
                gle::attribf(sx+w, sy);   colors[0].attrib();
                gle::attribf(sx,   sy);   colors[0].attrib();
                gle::attribf(sx+w, sy+h); colors[0].attrib();
                gle::attribf(sx,   sy+h); colors[0].attrib();
            }
            gle::end();

            Object::draw(sx, sy);
        }
    };

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

    struct Line : TargetColor
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

    struct Outline : TargetColor
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
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy);
            gle::attribf(sx+w, sy+h);
            gle::attribf(sx,   sy+h);
            gle::end();

            Object::draw(sx, sy);
        }
    };

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

    struct Image : TargetColor
    {
        static Texture *lasttex;
        static Color lastcolor;
        static GLenum lastmode;

        Texture *tex;
        bool alphatarget;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0)
        {
            TargetColor::setup(color_, minw_, minh_, SOLID, VERTICAL);
            tex = tex_;
            alphatarget = alphatarget_;
        }

        void setup(Texture *tex_, const Color &color_, const Color &color2_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, int dir_ = VERTICAL)
        {
            TargetColor::setup(color_, minw_, minh_, SOLID, dir_);
            colors.add(color2_); // gradient version
            tex = tex_;
            alphatarget = alphatarget_;
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

        void bindtex(GLenum mode = GL_QUADS)
        {
            changedraw(CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();
            if(lastmode != mode)
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
                glBindTexture(GL_TEXTURE_2D, tex->id);
                goto changecolor;
            }
            if(mode == GL_QUADS && lastcolor != colors[0])
            {
                gle::end();
            changecolor:
                switch(mode)
                {
                    case GL_TRIANGLE_STRIP:
                        lastcolor = Color(0, 0, 0, 0);
                    case GL_QUADS:
                        lastcolor = colors[0];
                        colors[0].init();
                        break;
                }
            }
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            int cols = colors.length();
            bindtex(cols >= 2 ? GL_TRIANGLE_STRIP : GL_QUADS);
            if(cols >= 2)
            {
                float gw = dir == HORIZONTAL ? w/float(cols-1) : w,
                      gh = dir == VERTICAL ? h/float(cols-1) : h,
                      part = 1/float(cols-1),
                      vx = sx, vy = sy, ts = 0;
                loopi(cols-1)
                {
                    float left = 1-(ts+part);
                    if(left < part) part += left;
                    switch(dir)
                    {
                        case HORIZONTAL:
                            gle::attribf(vx, vy);       gle::attribf(ts, 0.f);      colors[i].attrib();
                            gle::attribf(vx+gw, vy);    gle::attribf(ts+part, 0.f); colors[i+1].attrib();
                            gle::attribf(vx, vy+gh);    gle::attribf(ts, 1.f);      colors[i].attrib();
                            gle::attribf(vx+gw, vy+gh); gle::attribf(ts+part, 1.f); colors[i+1].attrib();
                            vx += gw;
                            break;
                        case VERTICAL:
                            gle::attribf(vx+gw, vy);    gle::attribf(1.f, ts);      colors[i].attrib();
                            gle::attribf(vx+gw, vy+gh); gle::attribf(1.f, ts+part); colors[i+1].attrib();
                            gle::attribf(vx, vy);       gle::attribf(0.f, ts);      colors[i].attrib();
                            gle::attribf(vx, vy+gh);    gle::attribf(0.f, ts+part); colors[i+1].attrib();
                            vy += gh;
                            break;
                    }
                    ts += part;
                }
            }
            else quads(sx, sy, w, h);

            Object::draw(sx, sy);
        }
    };

    Texture *Image::lasttex = NULL;
    Color Image::lastcolor(255, 255, 255);
    GLenum Image::lastmode = GL_POINTS; // something we don't use

    struct CroppedImage : Image
    {
        float cropx, cropy, cropw, croph;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, float cropx_ = 0, float cropy_ = 0, float cropw_ = 1, float croph_ = 1)
        {
            Image::setup(tex_, color_, alphatarget_, minw_, minh_);
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

            bindtex();
            quads(sx, sy, w, h, cropx, cropy, cropw, croph);

            Object::draw(sx, sy);
        }
    };

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

            bindtex();

            float splitw = (minw ? min(minw, w) : w) / 2,
                  splith = (minh ? min(minh, h) : h) / 2,
                  vy = sy, ty = 0;
            loopi(3)
            {
                float vh = 0, th = 0;
                switch(i)
                {
                    case 0: if(splith < h - splith) { vh = splith; th = 0.5f; } else { vh = h; th = 1; } break;
                    case 1: vh = h - 2*splith; th = 0; break;
                    case 2: vh = splith; th = 0.5f; break;
                }
                float vx = sx, tx = 0;
                loopj(3)
                {
                    float vw = 0, tw = 0;
                    switch(j)
                    {
                        case 0: if(splitw < w - splitw) { vw = splitw; tw = 0.5f; } else { vw = w; tw = 1; } break;
                        case 1: vw = w - 2*splitw; tw = 0; break;
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

            Object::draw(sx, sy);
        }
    };

    struct BorderedImage : Image
    {
        float texborder, screenborder;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float texborder_ = 0, float screenborder_ = 0, float minw_ = 0, float minh_ = 0)
        {
            Image::setup(tex_, color_, alphatarget_, minw_, minh_);
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

            bindtex();

            float vy = sy, ty = 0;
            loopi(3)
            {
                float vh = 0, th = 0;
                switch(i)
                {
                    case 0: vh = screenborder; th = texborder; break;
                    case 1: vh = h - 2*screenborder; th = 1 - 2*texborder; break;
                    case 2: vh = screenborder; th = texborder; break;
                }
                float vx = sx, tx = 0;
                loopj(3)
                {
                    float vw = 0, tw = 0;
                    switch(j)
                    {
                        case 0: vw = screenborder; tw = texborder; break;
                        case 1: vw = w - 2*screenborder; tw = 1 - 2*texborder; break;
                        case 2: vw = screenborder; tw = texborder; break;
                    }
                    quads(vx, vy, vw, vh, tx, ty, tw, th);
                    vx += vw;
                    tx += tw;
                }
                vy += vh;
                ty += th;
            }

            Object::draw(sx, sy);
        }
    };

    struct TiledImage : Image
    {
        float tilew, tileh;

        void setup(Texture *tex_, const Color &color_, bool alphatarget_ = false, float minw_ = 0, float minh_ = 0, float tilew_ = 0, float tileh_ = 0)
        {
            Image::setup(tex_, color_, alphatarget_, minw_, minh_);
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

            bindtex();

            if(tex->clamp)
            {
                for(float dy = 0; dy < h; dy += tileh)
                {
                    float dh = min(tileh, h - dy);
                    for(float dx = 0; dx < w; dx += tilew)
                    {
                        float dw = min(tilew, w - dx);
                        quads(sx + dx, sy + dy, dw, dh, 0, 0, dw / tilew, dh / tileh);
                    }
                }
            }
            else quads(sx, sy, w, h, 0, 0, w/tilew, h/tileh);

            Object::draw(sx, sy);
        }
    };

    struct Shape : TargetColor
    {
        void setup(const Color &color_, int type_ = SOLID, float minw_ = 0, float minh_ = 0)
        {
            TargetColor::setup(color_, minw_, minh_, type_);
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
            if(type == OUTLINE) return false;
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
            gle::begin(type == OUTLINE ? GL_LINE_LOOP : GL_TRIANGLES);
            gle::attrib(vec2(sx, sy).add(a));
            gle::attrib(vec2(sx, sy).add(b));
            gle::attrib(vec2(sx, sy).add(c));
            gle::end();
        }
    };

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
            if(type == OUTLINE) return false;
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
            if(type == OUTLINE)
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

    #define SETSTR(dst, src) do { \
        if(dst) { if(dst != src && strcmp(dst, src)) { delete[] dst; dst = newstring(src); } } \
        else dst = newstring(src); \
    } while(0)

    struct Text : Object
    {
        float scale, wrap, tw, th, wlen, limit, rescale, growth;
        int align, pos;
        Color color;

        void setup(float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = -1, int pos_ = -1, float growth_ = 1)
        {
            Object::setup();
            tw = th = wlen = 0;
            rescale = 1;
            scale = scale_;
            color = color_;
            wrap = wrap_;
            limit = limit_;
            align = align_;
            pos = pos_;
            growth = growth_;
        }

        static const char *typestr() { return "#Text"; }
        const char *gettype() const { return typestr(); }
        bool istext() const { return true; }

        float drawscale(float ss = 1) const { return scale / FONTH * ss; }

        virtual const char *getstr() const { return ""; }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_COLOR);

            float k = drawscale(rescale), left = sx/k, top = sy/k;
            int a = TEXT_MODCOL;
            switch(align)
            {
                case -2: a |= TEXT_NO_INDENT|TEXT_LEFT_JUSTIFY; break;
                case -1: a |= TEXT_LEFT_JUSTIFY; break;
                case 0: a |= TEXT_CENTERED; left += tw*k*0.5f; break;
                case 1: a |= TEXT_RIGHT_JUSTIFY; left += tw*k; break;
                case 2: a |= TEXT_NO_INDENT|TEXT_RIGHT_JUSTIFY; left += tw*k; break;
                default: break;
            }
            if(rescale != 1) top += (((th*drawscale())-(th*k))*0.5f)/k;
            if(growth < 0) top += th-(th/(0-growth));
            pushhudtranslate(0, 0, k);
            draw_text(getstr(), left, top, color.r, color.g, color.b, color.a, a, pos, wlen, 1);
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
                    if(o->isspacer())
                    {
                        wp += ((Spacer *)o)->spacew*2;
                        continue;
                    }
                    float ww = o->w > 0 || !o->isfiller() ? o->w : ((Filler *)o)->minw;
                    if(ww > 0)
                    {
                        wlen *= (ww-wp)/k;
                        break;
                    }
                    if(o->iswindow()) break;
                }
            }
            else wlen = 0;
            int a = TEXT_NO_INDENT|TEXT_MODCOL;
            switch(align)
            {
                case -1: a |= TEXT_LEFT_JUSTIFY; break;
                case 0: a |= TEXT_CENTERED; break;
                case 1: a |= TEXT_RIGHT_JUSTIFY; break;
                default: break;
            }
            text_boundsf(getstr(), tw, th, 0, 0, wlen, a);
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
                    if(o->isspacer())
                    {
                        lp += ((Spacer *)o)->spacew*2;
                        continue;
                    }
                    float ls = o->w > 0 || !o->isfiller() ? o->w : ((Filler *)o)->minw;
                    if(ls > 0)
                    {
                        float lo = (ls-lp)*lm;
                        if(lw > lo) rescale = lo/lw;
                        break;
                    }
                    if(o->iswindow()) break;
                }
            }
            if(growth != 1) th *= growth > 0 ? growth : 0-growth;
            w = max(w, tw*k*rescale);
            h = max(h, th*k);
        }
    };

    struct TextString : Text
    {
        char *str;

        TextString() : str(NULL) {}
        ~TextString() { delete[] str; }

        void setup(const char *str_, float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = -1, int pos_ = -1, float growth_ = 1)
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

        void setup(int val_, float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = -1, int pos_ = -1, float growth_ = 1)
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

        void setup(float val_, float scale_ = 1, const Color &color_ = Color(colourwhite), float wrap_ = 0, float limit_ = 0, int align_ = -1, int pos_ = -1, float growth_ = 1)
        {
            Text::setup(scale_, color_, wrap_, limit_, align_, pos_, growth_);

            if(val != val_) { val = val_; floatformat(str, val, sizeof(str)); }
        }

        static const char *typestr() { return "#TextFloat"; }
        const char *gettype() const { return typestr(); }

        const char *getstr() const { return str; }
    };

    struct Font : Object
    {
        ::font *font;

        Font() : font(NULL) {}

        void setup(const char *name)
        {
            Object::setup();

            if(!font || !strcmp(font->name, name)) font = findfont(name);
        }

        void layout()
        {
            pushfont(font);
            Object::layout();
            popfont();
        }

        void draw(float sx, float sy)
        {
            pushfont(font);
            Object::draw(sx, sy);
            popfont();
        }

        void buildchildren(uint *contents)
        {
            pushfont(font);
            Object::buildchildren(contents);
            popfont();
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                pushfont(font); \
                Object::func##children(cx, cy, mask, inside, setflags); \
                popfont(); \
            }
        DOSTATES
        #undef DOSTATE

        bool rawkey(int code, bool isdown)
        {
            pushfont(font);
            bool result = Object::rawkey(code, isdown);
            popfont();
            return result;
        }

        bool key(int code, bool isdown)
        {
            pushfont(font);
            bool result = Object::key(code, isdown);
            popfont();
            return result;
        }

        bool textinput(const char *str, int len)
        {
            pushfont(font);
            bool result = Object::textinput(str, len);
            popfont();
            return result;
        }
    };

    float uicontextscale = 0;
    ICOMMAND(0, uicontextscale, "", (), floatret(FONTH*uicontextscale));

    #if 0
    struct Console : Filler
    {
        void setup(float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);
        }

        static const char *typestr() { return "#Console"; }
        const char *gettype() const { return typestr(); }

        float drawscale() const { return uicontextscale; }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            float k = drawscale();
            pushhudtranslate(sx, sy, k);
            renderfullconsole(w/k, h/k);
            pophudmatrix();
        }
    };
    #endif

    struct Clipper : Object
    {
        float clipw, cliph, virtw, virth, offsetx, offsety;

        Clipper() : offsetx(0), offsety(0) {}

        void setup(float clipw_ = 0, float cliph_ = 0, float offsetx_ = -1, float offsety_ = -1)
        {
            Object::setup();
            clipw = clipw_;
            cliph = cliph_;
            if(offsetx_ >= 0) offsetx = offsetx_;
            if(offsety_ >= 0) offsety = offsety_;
            virtw = virth = 0;
        }

        static const char *typestr() { return "#Clipper"; }
        const char *gettype() const { return typestr(); }
        bool isclipper() const { return true; }

        void layout()
        {
            Object::layout();
            virtw = w;
            virth = h;
            if(clipw) w = min(w, clipw);
            if(cliph) h = min(h, cliph);
            offsetx = min(offsetx, hlimit());
            offsety = min(offsety, vlimit());
        }

        void adjustchildren()
        {
            adjustchildrento(0, 0, virtw, virth);
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                cx += offsetx; \
                cy += offsety; \
                if(cx < virtw && cy < virth) Object::func##children(cx, cy, mask, inside, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void draw(float sx, float sy)
        {
            if((clipw && virtw > clipw) || (cliph && virth > cliph))
            {
                stopdrawing();
                pushclip(sx, sy, w, h);
                Object::draw(sx - offsetx, sy - offsety);
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

        void addhscroll(float hscroll) { sethscroll(offsetx + hscroll); }
        void addvscroll(float vscroll) { setvscroll(offsety + vscroll); }
        void sethscroll(float hscroll) { offsetx = clamp(hscroll, 0.0f, hlimit()); }
        void setvscroll(float vscroll) { offsety = clamp(vscroll, 0.0f, vlimit()); }
    };

    struct Scroller : Clipper
    {
        void setup(float clipw_ = 0, float cliph_ = 0)
        {
            Clipper::setup(clipw_, cliph_);
        }

        static const char *typestr() { return "#Scroller"; }
        const char *gettype() const { return typestr(); }

        void scrollup(float cx, float cy);
        void scrolldown(float cx, float cy);
    };

    struct ScrollButton : Object
    {
        static const char *typestr() { return "#ScrollButton"; }
        const char *gettype() const { return typestr(); }
    };

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

        void hold(float cx, float cy)
        {
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(button && button->haschildstate(STATE_HOLD)) movebutton(button, offsetx, offsety, cx - button->x, cy - button->y);
        }

        void press(float cx, float cy)
        {
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(button && button->haschildstate(STATE_PRESS)) { offsetx = cx - button->x; offsety = cy - button->y; }
            else scrollto(cx, cy, true);
        }

        virtual void addscroll(Scroller *scroller, float dir) = 0;

        void addscroll(float dir)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(scroller) addscroll(scroller, dir);
        }

        void arrowscroll(float dir) { addscroll(dir*curtime/1000.0f); }
        void wheelscroll(float step);
        virtual int wheelscrolldirection() const { return 1; }

        void scrollup(float cx, float cy) { wheelscroll(-wheelscrolldirection()); }
        void scrolldown(float cx, float cy) { wheelscroll(wheelscrolldirection()); }

        virtual void movebutton(Object *o, float fromx, float fromy, float tox, float toy) = 0;
    };

    void Scroller::scrollup(float cx, float cy)
    {
        ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
        if(scrollbar) scrollbar->wheelscroll(-scrollbar->wheelscrolldirection());
    }

    void Scroller::scrolldown(float cx, float cy)
    {
        ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
        if(scrollbar) scrollbar->wheelscroll(scrollbar->wheelscrolldirection());
    }

    struct ScrollArrow : Object
    {
        float arrowspeed;

        void setup(float arrowspeed_ = 0)
        {
            Object::setup();
            arrowspeed = arrowspeed_;
        }

        static const char *typestr() { return "#ScrollArrow"; }
        const char *gettype() const { return typestr(); }

        void hold(float cx, float cy)
        {
            ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
            if(scrollbar) scrollbar->arrowscroll(arrowspeed);
        }
    };

    void ScrollBar::wheelscroll(float step)
    {
        ScrollArrow *arrow = (ScrollArrow *)findsibling(ScrollArrow::typestr());
        if(arrow) addscroll(arrow->arrowspeed*step*uiscrollsteptime/1000.0f);
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
            scroller->sethscroll(offset*scroller->virtw);
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
            button->x = scroller->hoffset()*bscale;
            button->adjust &= ~ALIGN_HMASK;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float fromy, float tox, float toy)
        {
            scrollto(o->x + tox - fromx, o->y + toy);
        }
    };

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
            scroller->setvscroll(offset*scroller->virth);
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
            button->y = scroller->voffset()*bscale;
            button->adjust &= ~ALIGN_VMASK;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float fromy, float tox, float toy)
        {
            scrollto(o->x + tox, o->y + toy - fromy);
        }

        int wheelscrolldirection() const { return -1; }
    };

    struct SliderButton : Object
    {
        static const char *typestr() { return "#SliderButton"; }
        const char *gettype() const { return typestr(); }
    };

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
                case ID_VAR: vmin_ = id_->minval; vmax_ = id_->maxval; break;
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

        void scrollup(float cx, float cy) { wheelscroll(-wheelscrolldirection()); }
        void scrolldown(float cx, float cy) { wheelscroll(wheelscrolldirection()); }

        virtual void scrollto(float cx, float cy) {}

        void hold(float cx, float cy)
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

        void press(float cx, float cy)
        {
            laststep = totalmillis + 2*uislidersteptime;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }

        void hold(float cx, float cy)
        {
            if(totalmillis < laststep + uislidersteptime)
                return;
            laststep = totalmillis;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }
    };

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

    struct TextEditor : Object
    {
        static TextEditor *focus;

        float scale, offsetx, offsety;
        editor *edit;
        char *keyfilter;
        Color color;

        TextEditor() : edit(NULL), keyfilter(NULL) {}

        bool iseditor() const { return true; }

        void setup(const char *name, int length, int height, float scale_ = 1, const char *initval = NULL, int mode = EDITORUSED, const char *keyfilter_ = NULL)
        {
            Object::setup();
            editor *edit_ = useeditor(name, mode, false, initval);
            if(edit_ != edit)
            {
                if(edit) clearfocus();
                edit = edit_;
            }
            else if(isfocus() && !hasstate(STATE_HOVER)) commit();
            if(initval && edit->mode == EDITORFOCUSED && !isfocus()) edit->clear(initval);
            edit->active = true;
            edit->linewrap = length < 0;
            edit->maxx = edit->linewrap ? -1 : length;
            edit->maxy = height <= 0 ? 1 : -1;
            edit->pixelwidth = abs(length)*FONTW;
            if(edit->linewrap && edit->maxy == 1) edit->updateheight();
            else edit->pixelheight = FONTH*max(height, 1);
            color = Color(colourwhite);
            scale = scale_;
            if(keyfilter_) SETSTR(keyfilter, keyfilter_);
            else DELETEA(keyfilter);
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
            bool allowtextinput = focus!=NULL && focus->allowtextinput();
            ::textinput(allowtextinput, TI_UI);
            ::keyrepeat(allowtextinput, KR_UI);
        }
        void setfocus() { setfocus(this); }
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
            changedraw(CHANGE_COLOR);

            edit->rendered = true;

            float k = drawscale();
            pushhudtranslate(sx, sy, k);

            edit->draw(FONTW/2, 0, color.tohexcolor(), color.a, isfocus());

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

        void press(float cx, float cy)
        {
            setfocus();
            resetmark(cx, cy);
        }

        void hold(float cx, float cy)
        {
            if(isfocus())
            {
                float k = drawscale();
                bool dragged = max(fabs(cx - offsetx), fabs(cy - offsety)) > (FONTH/8.0f)*k;
                edit->hit(int(floor(cx/k - FONTW/2)), int(floor(cy/k)), dragged);
            }
        }

        void scrollup(float cx, float cy)
        {
            edit->scrollup();
        }

        void scrolldown(float cx, float cy)
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

        bool key(int code, bool isdown)
        {
            if(Object::key(code, isdown)) return true;
            if(!isfocus()) return false;
            switch(code)
            {
                case SDLK_ESCAPE:
                    if(isdown) cancel();
                    return true;
                case SDLK_RETURN:
                case SDLK_TAB:
                    if(edit->maxy != 1) break;
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
    };

    TextEditor *TextEditor::focus = NULL;

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

        void setup(ident *id_, int length, uint *onchange, float scale = 1, const char *keyfilter_ = NULL, bool immediate = false)
        {
            if(isfocus() && !hasstate(STATE_HOVER)) commit();
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
            TextEditor::setup(id_->name, length, 0, scale, initval, EDITORFOCUSED, keyfilter_);
            if(shouldfree) delete[] initval;
            id = id_;
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

    struct KeyField : Field
    {
        static const char *typestr() { return "#KeyField"; }
        const char *gettype() const { return typestr(); }

        void resetmark(float cx, float cy)
        {
            edit->clear();
            Field::resetmark(cx, cy);
        }

        void insertkey(int code)
        {
            const char *keyname = getkeyname(code);
            if(keyname)
            {
                if(!edit->empty()) edit->insert(" ");
                edit->insert(keyname);
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

    struct Preview : TargetColor
    {
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
    };

    struct ModelPreview : Preview
    {
        char *name;
        int anim;
        float scale, blend;

        ModelPreview() : name(NULL) {}
        ~ModelPreview() { delete[] name; }

        void setup(const char *name_, const char *animspec, float scale_, float blend_, float minw_, float minh_)
        {
            Preview::setup(Color(colourwhite), minw_, minh_);
            SETSTR(name, name_);

            anim = ANIM_ALL;
            if(animspec[0])
            {
                if(isdigit(animspec[0]))
                {
                    anim = parseint(animspec);
                    if(anim >= 0) anim %= ANIM_INDEX;
                    else anim = ANIM_ALL;
                }
                else
                {
                    vector<int> anims;
                    game::findanims(animspec, anims);
                    if(anims.length()) anim = anims[0];
                }
            }
            anim |= ANIM_LOOP;
            scale = scale_;
            blend = blend_;
        }

        static const char *typestr() { return "#ModelPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.length() > 0);
            model *m = loadmodel(name);
            if(m)
            {
                entitylight light;
                light.color = colors[0].tocolor();
                light.dir = vec(0, -1, 2).normalize();
                vec center, radius;
                m->boundbox(center, radius);
                float yaw;
                vec o = calcmodelpreviewpos(radius, yaw).sub(center);
                rendermodel(&light, name, anim|ANIM_NOTRANS, o, yaw, 0, 0, 0, NULL, NULL, 0, 0, blend*(colors[0].a/255.f), scale);
            }
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end();
        }
    };

    struct PlayerPreview : Preview
    {
        int model, team, weapon;
        float scale, blend;
        char *vanity;
        Color pcol;

        PlayerPreview() : vanity(NULL) {}
        ~PlayerPreview() { DELETEA(vanity); }

        void setup(int model_, const Color &pcol_, int team_, int weapon_, char *vanity_, float scale_, float blend_, float minw_, float minh_)
        {
            Preview::setup(Color(colourwhite), minw_, minh_);
            model = model_;
            pcol = pcol_;
            team = team_;
            weapon = weapon_;
            scale = scale_;
            blend = blend_;
            if(vanity_) SETSTR(vanity, vanity_);
            else DELETEA(vanity);
        }

        static const char *typestr() { return "#PlayerPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.length() > 0);
            game::renderplayerpreview(model, pcol.tohexcolor(), team, weapon, vanity, scale, blend*(colors[0].a/255.f), colors[0].tocolor());
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end();
        }
    };

    struct PrefabPreview : Preview
    {
        char *name;
        float blend;

        PrefabPreview() : name(NULL) {}
        ~PrefabPreview() { delete[] name; }

        void setup(const char *name_, const Color &color_, float blend, float minw_, float minh_)
        {
            Preview::setup(color_, minw_, minh_);
            SETSTR(name, name_);
        }

        static const char *typestr() { return "#PrefabPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.length() > 0);
            previewprefab(name, colors[0].tocolor(), blend*(colors[0].a/255.f));
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end();
        }
    };

    static int lastthumbnail = 0;

    struct SlotViewer : TargetColor
    {
        int index;

        void setup(int index_, float minw_ = 0, float minh_ = 0)
        {
            TargetColor::setup(Color(colourwhite), minw_, minh_);
            index = index_;
        }

        static const char *typestr() { return "#SlotViewer"; }
        const char *gettype() const { return typestr(); }

        void previewslot(Slot &slot, VSlot &vslot, float x, float y)
        {
            if(slot.sts.empty()) return;
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
                    if(totalmillis - lastthumbnail < uislotviewtime) return;
                    loadthumbnail(slot);
                    lastthumbnail = totalmillis;
                }
                if(slot.thumbnail != notexture) t = slot.thumbnail;
                else return;
            }

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            SETSHADER(hudrgb);
            vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
            int xoff = vslot.offset.x, yoff = vslot.offset.y;
            if(vslot.rotation)
            {
                if((vslot.rotation&5) == 1) { swap(xoff, yoff); loopk(4) swap(tc[k].x, tc[k].y); }
                if(vslot.rotation >= 2 && vslot.rotation <= 4) { xoff *= -1; loopk(4) tc[k].x *= -1; }
                if(vslot.rotation <= 2 || vslot.rotation == 5) { yoff *= -1; loopk(4) tc[k].y *= -1; }
            }
            float xt = min(1.0f, t->xs/float(t->ys)), yt = min(1.0f, t->ys/float(t->xs));
            loopk(4) { tc[k].x = tc[k].x/xt - float(xoff)/t->xs; tc[k].y = tc[k].y/yt - float(yoff)/t->ys; }
            glBindTexture(GL_TEXTURE_2D, t->id);
            if(slot.loaded) gle::colorf(vslot.colorscale.x*colors[0].r/255.f, vslot.colorscale.y*colors[0].g/255.f, vslot.colorscale.z*colors[0].b/255.f, colors[0].a/255.f);
            quad(x, y, w, h, tc);
            if(detailtex)
            {
                glBindTexture(GL_TEXTURE_2D, detailtex->id);
                quad(x + w/2, y + h/2, w/2, h/2, tc);
            }
            if(glowtex)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glBindTexture(GL_TEXTURE_2D, glowtex->id);
                vec glowcolor = vslot.getglowcolor();
                gle::colorf(glowcolor.x*colors[0].r/255.f, glowcolor.y*colors[0].g/255.f, glowcolor.z*colors[0].b/255.f, colors[0].a/255.f);
                quad(x, y, w, h, tc);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            if(layertex)
            {
                glBindTexture(GL_TEXTURE_2D, layertex->id);
                gle::colorf(layer->colorscale.x*colors[0].r/255.f, layer->colorscale.y*colors[0].g/255.f, layer->colorscale.z*colors[0].g/255.f, colors[0].a/255.f);
                quad(x, y, w/2, h/2, tc);
            }
        }

        void draw(float sx, float sy)
        {
            Slot &slot = lookupslot(index, false);
            previewslot(slot, *slot.variants, sx, sy);

            Object::draw(sx, sy);
        }
    };

    struct VSlotViewer : SlotViewer
    {
        static const char *typestr() { return "#VSlotViewer"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            VSlot &vslot = lookupvslot(index, false);
            previewslot(*vslot.slot, vslot, sx, sy);

            Object::draw(sx, sy);
        }
    };

    struct MiniMap : TargetColor
    {
        Texture *tex;
        float dist, border;

        void setup(Texture *tex_, const Color &color_, float dist_ = 0, float border_ = 0.05f, float minw_ = 0, float minh_ = 0)
        {
            TargetColor::setup(color_, minw_, minh_, SOLID, VERTICAL);
            colors.add(Color(colourwhite));
            tex = tex_;
            dist = dist_;
            border = border_;
        }

        void setup(Texture *tex_, const Color &color_, const Color &color2_, float dist_ = 0, float border_ = 0.05f, float minw_ = 0, float minh_ = 0)
        {
            TargetColor::setup(color_, minw_, minh_, SOLID, VERTICAL);
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
                float scale = min(dist > 0 ? dist : float(getworldsize()), limit > 0 ? limit : float(getworldsize())),
                      qw = w*0.5f*border, qh = h*0.5f*border, rw = w*0.5f-qw, rh = h*0.5f-qh;
                colors[1].init();
                gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_TRIANGLE_FAN);
                bindminimap();
                loopi(36)
                {
                    vec v = vec(0, -1, 0).rotate_around_z(i/36.0f*2*M_PI);
                    gle::attribf(sx + qw + rw*(1.0f + v.x), sy + qh + rh*(1.0f + v.y));
                    vec tc = vec(dir).rotate_around_z(i/36.0f*2*M_PI);
                    gle::attribf(pos.x + tc.x*scale*minimapscale.x, pos.y + tc.y*scale*minimapscale.y);
                }
                gle::end();
            }
            if(tex != notexture)
            {
                colors[0].init();
                gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_QUADS);
                glBindTexture(GL_TEXTURE_2D, tex->id);
                quads(sx, sy, w, h);
                gle::end();
            }

            Object::draw(sx, sy);
        }
    };

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

    struct RadarBlip : Image
    {
        float yaw, blipx, blipy, dist, blipyaw;
        uchar blipadjust;

        void setup(Texture *tex_, const Color &color_, float yaw_ = 0, float blipyaw_ = 0, float dist_ = 0, float minw_ = 0, float minh_ = 0)
        {
            Image::setup(tex_, color_, true, minw_, minh_);
            yaw = yaw_;
            blipyaw = blipyaw_;
            blipx = sinf(RAD*yaw);
            blipy = -cosf(RAD*yaw);
            dist = dist_;
            blipadjust = ALIGN_HCENTER | ALIGN_VCENTER;
        }

        static const char *typestr() { return "#RadarBlip"; }
        const char *gettype() const { return typestr(); }

        Radar *getradar()
        {
            for(Object *o = parent; o != NULL; o = o->parent)
            {
                if(o->isradar()) return (Radar *)o;
                if(o->iswindow()) break;
            }
            return NULL;
        }

        void layout()
        {
            Image::layout();
            // children don't exceed the dimensions of a blip if specified
            if(minw > 0 && w > minw) w = minw;
            if(minh > 0 && h > minh) h = minh;
        }

        void setalign(int xalign, int yalign)
        {
            adjust = ALIGN_HCENTER | ALIGN_VCENTER; // always align center and use our own adjustment
            blipadjust &= ~ALIGN_MASK;
            blipadjust |= (clamp(xalign, -2, 1)+2)<<ALIGN_HSHIFT;
            blipadjust |= (clamp(yalign, -2, 1)+2)<<ALIGN_VSHIFT;
        }

        void setclamp(int left, int right, int top, int bottom)
        {
            adjust &= ~CLAMP_MASK;
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
                    case ALIGN_HCENTER: break;
                    case ALIGN_RIGHT:   anrm.x = 1; break;
                }
                switch(blipadjust&ALIGN_VMASK)
                {
                    case ALIGN_TOP:     anrm.y = 1; break;
                    case ALIGN_VCENTER: break;
                    case ALIGN_BOTTOM:  anrm.y = -1; break;
                }
                if(!anrm.iszero())
                {
                    anrm.normalize().mul(vec2(bw, bh)).rotate_around_z(yaw*RAD);
                    rx += anrm.x;
                    ry += anrm.y;
                }

                float bbx = blipx, bby = blipy;
                if(tex != notexture)
                {
                    bindtex();
                    bbx = sinf(RAD*blipyaw);
                    bby = -cosf(RAD*blipyaw);
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

    ICOMMAND(0, newui, "ssssi", (char *name, char *contents, char *onshow, char *onhide, int *windowflags),
    {
        Window *window = windows.find(name, NULL);
        if(window) { world->hide(window); windows.remove(name); delete window; }
        windows[name] = new Window(name, contents, onshow, onhide, *windowflags);
    });

    ICOMMAND(0, uiallowinput, "b", (int *val), { if(window) { if(*val >= 0) window->allowinput = *val!=0; intret(window->allowinput ? 1 : 0); } });
    ICOMMAND(0, uiexclusive, "b", (int *val), { if(window) { if(*val >= 0) window->exclusive = *val!=0; intret(window->exclusive ? 1 : 0); } });
    ICOMMAND(0, uiwindowflags, "b", (int *val), { if(window) { if(*val >= 0) window->windowflags = clamp(*val, 0, int(WINDOW_ALL)); intret(window->windowflags); } });

    ICOMMAND(0, uioverridepos, "", (), { if(window) { intret(window->overridepos ? 1 : 0); } });
    ICOMMAND(0, uisetpos, "ff", (float *xpos, float *ypos), { if(window) { window->setpos(*xpos, *ypos); } });
    ICOMMAND(0, uiresetpos, "", (), { if(window) { window->resetpos(); } });

    ICOMMAND(0, uicursorx, "", (), floatret(cursorx*float(screenw)/screenh));
    ICOMMAND(0, uicursory, "", (), floatret(cursory));

    ICOMMAND(0, uicursortype, "b", (int *val), { if(*val >= 0) cursortype = clamp(*val, 0, CURSOR_MAX-1); intret(cursortype); });

    bool showui(const char *name)
    {
        Window *window = windows.find(name, NULL);
        if(!window) return false;
        return world->show(window);
    }

    bool hideui(const char *name)
    {
        if(!name) return world->hideall() > 0;
        else
        {
            Window *window = windows.find(name, NULL);
            if(window) return world->hide(window);
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
        if(!name) return world->children.length() > 0;
        Window *window = windows.find(name, NULL);
        return window && world->children.find(window) >= 0;
    }

    ICOMMAND(0, showui, "s", (char *name), intret(showui(name) ? 1 : 0));
    ICOMMAND(0, hideui, "s", (char *name), intret(hideui(name) ? 1 : 0));
    ICOMMAND(0, hidetopui, "", (), intret(world->hidetop() ? 1 : 0));
    ICOMMAND(0, topui, "", (), result(world->topname()));
    ICOMMAND(0, hideallui, "", (), intret(world->hideall()));
    ICOMMAND(0, toggleui, "s", (char *name), intret(toggleui(name) ? 1 : 0));
    ICOMMAND(0, holdui, "sD", (char *name, int *down), holdui(name, *down!=0));
    ICOMMAND(0, pressui, "sD", (char *name, int *down), pressui(name, *down!=0));
    ICOMMAND(0, uivisible, "s", (char *name), intret(uivisible(name) ? 1 : 0));
    ICOMMAND(0, uiname, "", (), { if(window) result(window->name); });

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

    ICOMMAND(0, uigroup, "e", (uint *children),
        BUILD(Object, o, o->setup(), children));

    ICOMMAND(0, uihlist, "fe", (float *space, uint *children),
        BUILD(HorizontalList, o, o->setup(*space*uiscale), children));

    ICOMMAND(0, uivlist, "fe", (float *space, uint *children),
        BUILD(VerticalList, o, o->setup(*space*uiscale), children));

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

    ICOMMAND(0, uigrid, "iffe", (int *columns, float *spacew, float *spaceh, uint *children),
        BUILD(Grid, o, o->setup(*columns, *spacew*uiscale, *spaceh*uiscale), children));

    ICOMMAND(0, uitableheader, "ee", (uint *columndata, uint *children),
        BUILDCOLUMNS(TableHeader, o, o->setup(), columndata, children));
    ICOMMAND(0, uitablerow, "ee", (uint *columndata, uint *children),
        BUILDCOLUMNS(TableRow, o, o->setup(), columndata, children));
    ICOMMAND(0, uitable, "ffe", (float *spacew, float *spaceh, uint *children),
        BUILD(Table, o, o->setup(*spacew*uiscale, *spaceh*uiscale), children));

    ICOMMAND(0, uispace, "ffe", (float *spacew, float *spaceh, uint *children),
        BUILD(Spacer, o, o->setup(*spacew*uiscale, *spaceh*uiscale), children));

    ICOMMAND(0, uioffset, "ffe", (float *offsetx, float *offsety, uint *children),
        BUILD(Offsetter, o, o->setup(*offsetx*uiscale, *offsety*uiscale), children));

    ICOMMAND(0, uifill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uitarget, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Target, o, o->setup(*minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiclip, "ffgge", (float *clipw, float *cliph, float *offsetx, float *offsety, uint *children),
        BUILD(Clipper, o, o->setup(*clipw*uiscale, *cliph*uiscale, *offsetx*uiscale, *offsety*uiscale), children));

    ICOMMAND(0, uiscroll, "ffe", (float *clipw, float *cliph, uint *children),
        BUILD(Scroller, o, o->setup(*clipw*uiscale, *cliph*uiscale), children));

    ICOMMAND(0, uihscrolloffset, "", (),
    {
        if(buildparent && buildparent->isclipper())
        {
            Clipper *clipper = (Clipper *)buildparent;
            floatret(clipper->offsetx);
        }
    });

    ICOMMAND(0, uivscrolloffset, "", (),
    {
        if(buildparent && buildparent->isclipper())
        {
            Clipper *clipper = (Clipper *)buildparent;
            floatret(clipper->offsety);
        }
    });

    ICOMMAND(0, uihscrollbar, "e", (uint *children),
        BUILD(HorizontalScrollBar, o, o->setup(), children));

    ICOMMAND(0, uivscrollbar, "e", (uint *children),
        BUILD(VerticalScrollBar, o, o->setup(), children));

    ICOMMAND(0, uiscrollarrow, "fe", (float *dir, uint *children),
        BUILD(ScrollArrow, o, o->setup(*dir), children));

    ICOMMAND(0, uiscrollbutton, "e", (uint *children),
        BUILD(ScrollButton, o, o->setup(), children));

    ICOMMAND(0, uihslider, "rfffee", (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children),
        BUILD(HorizontalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children));

    ICOMMAND(0, uivslider, "rfffee", (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children),
        BUILD(VerticalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children));

    ICOMMAND(0, uisliderarrow, "fe", (float *dir, uint *children),
        BUILD(SliderArrow, o, o->setup(*dir), children));

    ICOMMAND(0, uisliderbutton, "e", (uint *children),
        BUILD(SliderButton, o, o->setup(), children));

    ICOMMAND(0, uicolour, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, TargetColor::SOLID), children));

    ICOMMAND(0, uimodcolour, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, TargetColor::MODULATE), children));

    Object *uirootwin(Object *root)
    {
        if(root)
        {
            if(root->iswindow()) return root;
            if(root && !root->iswindow()) while(root->parent)
            {
                root = root->parent;
                if(root->iswindow()) return root;
            }
        }
        return NULL;
    }

    #define UIWINCMDC(func, types, argtypes, body) \
        ICOMMAND(0, ui##func, types, argtypes, \
        { \
            Object *o = uirootwin(buildparent); \
            if(o) { body; } \
        });

    #define UIREVCMDC(func, types, argtypes, body) \
        ICOMMAND(0, ui##func, types, argtypes, \
        { \
            for(Object *o = buildparent; o != NULL; o = o->parent) \
            { \
                body; \
                if(o->iswindow()) break; \
            } \
        });

    #define UIREVCMDW(func, types, argtypes, body) \
        ICOMMAND(0, ui##func, types, argtypes, \
        { \
            for(Object *o = buildparent; o != NULL; o = o->parent) \
            { \
                if(o->iswindow()) break; \
                body; \
            } \
        });

    #define UICOLOURCMDS(t) \
        if(o->iscolor()) \
        { \
            loopvk(((TargetColor *)o)->colors) ((TargetColor *)o)->colors[k] = Color(*c); \
            t; \
        } \
        else if(o->istext()) { ((Text *)o)->color = Color(*c); t; } \
        else if(o->iseditor()) { ((TextEditor *)o)->color = Color(*c); t; }

    UIREVCMDC(setcolour, "i", (int *c), UICOLOURCMDS(break));
    void setchildcolours(Object *o, int *c)
    {
        UICOLOURCMDS();
        loopv(o->children) setchildcolours(o->children[i], c);
    }
    UIWINCMDC(setcolours, "i", (int *c), setchildcolours(o, c));

    #define UIBLENDCMDS(t) \
        if(o->iscolor()) \
        { \
            loopvk(((TargetColor *)o)->colors) ((TargetColor *)o)->colors[k].a = clamp(int(*c * ((TargetColor *)o)->colors[k].a), 0, 255); \
            t; \
        } \
        else if(o->istext()) { ((Text *)o)->color.a = clamp(int(*c * ((Text *)o)->color.a), 0, 255); t; } \
        else if(o->iseditor()) { ((TextEditor *)o)->color.a = clamp(int(*c * ((TextEditor *)o)->color.a), 0, 255); t; }

    UIREVCMDC(changeblend, "f", (float *c), UIBLENDCMDS(break));
    void changechildblends(Object *o, float *c)
    {
        UIBLENDCMDS();
        loopv(o->children) changechildblends(o->children[i], c);
    }
    UIWINCMDC(changeblends, "f", (float *c), changechildblends(o, c));


    #define UICHGCOLCMDS(t) \
        if(o->iscolor()) \
        { \
            loopvk(((TargetColor *)o)->colors) \
            { \
                ((TargetColor *)o)->colors[k].r = clamp(int(*c * ((TargetColor *)o)->colors[k].r), 0, 255); \
                ((TargetColor *)o)->colors[k].g = clamp(int(*c * ((TargetColor *)o)->colors[k].g), 0, 255); \
                ((TargetColor *)o)->colors[k].b = clamp(int(*c * ((TargetColor *)o)->colors[k].b), 0, 255); \
            } \
            t; \
        } \
        else if(o->istext()) \
        { \
            ((Text *)o)->color.r = clamp(int(*c * ((Text *)o)->color.r), 0, 255); \
            ((Text *)o)->color.g = clamp(int(*c * ((Text *)o)->color.g), 0, 255); \
            ((Text *)o)->color.b = clamp(int(*c * ((Text *)o)->color.b), 0, 255); \
            t; \
        } \
        else if(o->iseditor()) \
        { \
            ((TextEditor *)o)->color.r = clamp(int(*c * ((TextEditor *)o)->color.r), 0, 255); \
            ((TextEditor *)o)->color.g = clamp(int(*c * ((TextEditor *)o)->color.g), 0, 255); \
            ((TextEditor *)o)->color.b = clamp(int(*c * ((TextEditor *)o)->color.b), 0, 255); \
            t; \
        }

    UIREVCMDC(changecolour, "f", (float *c), UICHGCOLCMDS(break));
    void changechildcolours(Object *o, float *c)
    {
        UICHGCOLCMDS();
        loopv(o->children) changechildcolours(o->children[i], c);
    }
    UIWINCMDC(changecolours, "f", (float *c), changechildcolours(o, c));

    UIREVCMDW(addcolour, "i", (int *c),
    {
        if(!o->iscolor()) continue;
        ((TargetColor *)o)->colors.add(Color(*c));
        break;
    });

    UIREVCMDW(delcolour, "i", (int *c),
    {
        if(!o->iscolor()) continue;
        loopvrev(((TargetColor *)o)->colors) if(((TargetColor *)o)->colors[i] == Color(*c)) ((TargetColor *)o)->colors.remove(i);
        if(((TargetColor *)o)->colors.empty()) ((TargetColor *)o)->colors.add(Color(255, 255, 255));
        break;
    });

    UIREVCMDW(setgradient, "i", (int *c),
    {
        if(!o->iscolor()) continue;
        ((TargetColor *)o)->dir = clamp(*c, TargetColor::VERTICAL, TargetColor::HORIZONTAL);
        break;
    });

    UIREVCMDW(setgradmod, "i", (int *c),
    {
        if(!o->iscolor()) continue;
        ((TargetColor *)o)->type = clamp(*c, TargetColor::SOLID, TargetColor::MODULATE);
        break;
    });

    UIREVCMDW(rotategrad, "f", (float *amt),
    {
        if(!o->iscolor()) continue;
        ((TargetColor *)o)->rotatecolors(*amt);
        break;
    });

    ICOMMAND(0, uivgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::SOLID, Gradient::VERTICAL), children));

    ICOMMAND(0, uimodvgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::MODULATE, Gradient::VERTICAL), children));

    ICOMMAND(0, uihgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::SOLID, Gradient::HORIZONTAL), children));

    ICOMMAND(0, uimodhgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Color(*c), Color(*c2), *minw*uiscale, *minh*uiscale, Gradient::MODULATE, Gradient::HORIZONTAL), children));

    ICOMMAND(0, uioutline, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(Outline, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiline, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(Line, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uitriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::SOLID), children));

    ICOMMAND(0, uitriangleoutline, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::OUTLINE), children));

    ICOMMAND(0, uimodtriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw*uiscale, *minh*uiscale, *angle, Triangle::MODULATE), children));

    ICOMMAND(0, uicircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::SOLID), children));

    ICOMMAND(0, uicircleoutline, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::OUTLINE), children));

    ICOMMAND(0, uimodcircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size*uiscale, Circle::MODULATE), children));

    static inline void buildtext(tagval &t, float scale, float scalemod, const Color &color, float wrap, float limit, int align, int pos, float growth, uint *children)
    {
        if(scale <= 0) scale = uiscale;
        scale *= scalemod;
        switch(t.type)
        {
            case VAL_INT:
                BUILD(TextInt, o, o->setup(t.i, scale, color, wrap, limit, align, pos, growth), children);
                break;
            case VAL_FLOAT:
                BUILD(TextFloat, o, o->setup(t.f, scale, color, wrap, limit, align, pos, growth), children);
                break;
            case VAL_CSTR:
            case VAL_MACRO:
            case VAL_STR:
                if(t.s[0])
                {
                    BUILD(TextString, o, o->setup(t.s, scale, color, wrap, limit, align, pos, growth), children);
                    break;
                }
                // fall-through
            default:
                BUILD(TextString, o, o->setup("", scale, color, wrap, limit, align, pos, growth), children);
                break;
        }
    }

    ICOMMAND(0, uitextfill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw*uiscale * uitextscale*0.5f, *minh*uiscale * uitextscale), children));

    ICOMMAND(0, uitext, "tfie", (tagval *text, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(colourwhite), 0, 0, *align, -1, 1, children));

    ICOMMAND(0, uicolourtext, "tifie", (tagval *text, int *c, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), 0, 0, *align, -1, 1, children));

    ICOMMAND(0, uiwraptext, "tffie", (tagval *text, float *wrap, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(colourwhite), *wrap*uiscale, 0, *align, -1, 1, children));

    ICOMMAND(0, uiwrapcolourtext, "tfifie", (tagval *text, float *wrap, int *c, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), *wrap*uiscale, 0, *align, -1, 1, children));

    ICOMMAND(0, uilimittext, "tffie", (tagval *text, float *limit, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(colourwhite), 0, *limit, *align, -1, 1, children));

    ICOMMAND(0, uilimitcolourtext, "tfifie", (tagval *text, float *limit, int *c, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), 0, *limit, *align, -1, 1, children));

    ICOMMAND(0, uiwrapcolourtext, "tfifie", (tagval *text, float *wrap, int *c, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), *wrap*uiscale, 0, *align, -1, 1, children));

    ICOMMAND(0, uiwrapcolourcontext, "tfifie", (tagval *text, float *wrap, int *c, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, FONTH*uicontextscale, Color(*c), *wrap*uiscale, 0, *align, -1, 1, children));

    ICOMMAND(0, uiwrapcontext, "tffie", (tagval *text, float *wrap, float *scale, int *align, uint *children),
        buildtext(*text, *scale*uiscale, FONTH*uicontextscale, Color(colourwhite), *wrap*uiscale, 0, *align, -1, 1, children));

    ICOMMAND(0, uipostext, "tfibe", (tagval *text, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(colourwhite), 0, 0, *align, *pos, 1, children));

    ICOMMAND(0, uicolourpostext, "tifibe", (tagval *text, int *c, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), 0, 0, *align, *pos, 1, children));

    ICOMMAND(0, uiwrappostext, "tffibe", (tagval *text, float *wrap, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(colourwhite), *wrap*uiscale, 0, *align, *pos, 1, children));

    ICOMMAND(0, uiwrapcolourpostext, "tfifibe", (tagval *text, float *wrap, int *c, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), *wrap*uiscale, 0, *align, *pos, 1, children));

    ICOMMAND(0, uilimitpostext, "tffibe", (tagval *text, float *limit, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(colourwhite), 0, *limit, *align, *pos, 1, children));

    ICOMMAND(0, uilimitcolourpostext, "tfifibe", (tagval *text, float *limit, int *c, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), 0, *limit, *align, *pos, 1, children));

    ICOMMAND(0, uiwrapcolourpostext, "tfifibe", (tagval *text, float *wrap, int *c, float *scale, int *align, int *pos, uint *children),
        buildtext(*text, *scale*uiscale, uitextscale, Color(*c), *wrap*uiscale, 0, *align, *pos, 1, children));

    ICOMMAND(0, uitexteditor, "siifsie", (char *name, int *length, int *height, float *scale, char *initval, int *mode, uint *children),
        BUILD(TextEditor, o, o->setup(name, *length, *height, (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale, initval, *mode <= 0 ? EDITORFOREVER : *mode, NULL), children));

    ICOMMAND(0, uifont, "se", (char *name, uint *children),
        BUILD(Font, o, o->setup(name), children));

    #define UITEXTSET(name, valtype, type, cmin, cmax) \
        ICOMMAND(0, uisettext##name, valtype, (type *val), if(buildparent && buildparent->istext()) { ((Text *)buildparent)->name = clamp(*val, cmin, cmax); type##ret(((Text *)buildparent)->name); });
    UITEXTSET(scale, "f", float, 0.f, FVAR_MAX);
    UITEXTSET(wrap, "f", float, FVAR_MIN, FVAR_MAX);
    UITEXTSET(limit, "f", float, FVAR_MIN, FVAR_MAX);
    UITEXTSET(growth, "f", float, FVAR_MIN, FVAR_MAX);
    UITEXTSET(align, "i", int, -2, 2);
    UITEXTSET(pos, "i", int, -1, VAR_MAX);

    ICOMMAND(0, uifield, "riefie", (ident *var, int *length, uint *onchange, float *scale, int *immediate, uint *children),
        BUILD(Field, o, o->setup(var, *length, onchange, (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale, NULL, *immediate!=0), children));

    ICOMMAND(0, uikeyfield, "riefe", (ident *var, int *length, uint *onchange, float *scale, uint *children),
        BUILD(KeyField, o, o->setup(var, *length, onchange, (*scale <= 0 ? 1 : *scale)*uiscale * uitextscale), children));

    ICOMMAND(0, uiimage, "siiffse", (char *texname, int *c, int *a, float *minw, float *minh, char *alttex, uint *children),
        BUILD(Image, o, {
            Texture *tex = textureload(texname, 3, true, false);
            if(tex == notexture && *alttex) tex = textureload(alttex, 3, true, false);
            o->setup(tex, Color(*c), *a!=0, *minw*uiscale, *minh*uiscale);
        }, children));

    ICOMMAND(0, uiimagevgradient, "siiiffse", (char *texname, int *c, int *c2, int *a, float *minw, float *minh, char *alttex, uint *children),
        BUILD(Image, o, {
            Texture *tex = textureload(texname, 3, true, false);
            if(tex == notexture && *alttex) tex = textureload(alttex, 3, true, false);
            o->setup(tex, Color(*c), Color(*c2), *a!=0, *minw*uiscale, *minh*uiscale, Image::VERTICAL);
        }, children));

    ICOMMAND(0, uiimagehgradient, "siiiffse", (char *texname, int *c, int *c2, int *a, float *minw, float *minh, char *alttex, uint *children),
        BUILD(Image, o, {
            Texture *tex = textureload(texname, 3, true, false);
            if(tex == notexture && *alttex) tex = textureload(alttex, 3, true, false);
            o->setup(tex, Color(*c), Color(*c2), *a!=0, *minw*uiscale, *minh*uiscale, Image::HORIZONTAL);
        }, children));

    ICOMMAND(0, uistretchedimage, "siiffse", (char *texname, int *c, int *a, float *minw, float *minh, char *alttex, uint *children),
        BUILD(StretchedImage, o, {
            Texture *tex = textureload(texname, 3, true, false);
            if(tex == notexture && *alttex) tex = textureload(alttex, 3, true, false);
            o->setup(tex, Color(*c), *a!=0, *minw*uiscale, *minh*uiscale);
        }, children));

    static inline float parsepixeloffset(const tagval *t, int size)
    {
        switch(t->type)
        {
            case VAL_INT: return t->i*uiscale;
            case VAL_FLOAT: return t->f*uiscale;
            case VAL_NULL: return 0;
            default:
            {
                const char *s = t->getstr();
                char *end;
                float val = strtod(s, &end);
                return (*end == 'p' ? val/size : val)*uiscale;
            }
        }
    }

    ICOMMAND(0, uicroppedimage, "siifftttte", (char *texname, int *c, int *a, float *minw, float *minh, tagval *cropx, tagval *cropy, tagval *cropw, tagval *croph, uint *children),
        BUILD(CroppedImage, o, {
            Texture *tex = textureload(texname, 3, true, false);
            o->setup(tex, Color(*c), *a!=0, *minw*uiscale, *minh*uiscale,
                parsepixeloffset(cropx, tex->xs), parsepixeloffset(cropy, tex->ys),
                parsepixeloffset(cropw, tex->xs), parsepixeloffset(croph, tex->ys));
        }, children));

    ICOMMAND(0, uiborderedimage, "siitfffe", (char *texname, int *c, int *a, tagval *texborder, float *screenborder, float *minw, float *minh, uint *children),
        BUILD(BorderedImage, o, {
            Texture *tex = textureload(texname, 3, true, false);
            o->setup(tex, Color(*c), *a!=0,
                parsepixeloffset(texborder, tex->xs),
                *screenborder, *minw, *minh);
        }, children));

    ICOMMAND(0, uitiledimage, "siiffffse", (char *texname, int *c, int *a, float *tilew, float *tileh, float *minw, float *minh, char *alttex, uint *children),
        BUILD(TiledImage, o, {
            Texture *tex = textureload(texname, 3, true, false);
            if(tex == notexture && *alttex) tex = textureload(alttex, 3, true, false);
            o->setup(tex, Color(*c), *a!=0, *minw*uiscale, *minh*uiscale, *tilew <= 0 ? 1 : *tilew, *tileh <= 0 ? 1 : *tileh);
        }, children));

    ICOMMAND(0, uialtimage, "s", (char *texname),
    {
        if(!buildparent || !buildparent->isimage()) return;
        Image *o = (Image *)buildparent;
        if(o && o->tex == notexture) o->tex = textureload(texname, 3, true, false);
    });

    ICOMMAND(0, uimodelpreview, "ssffffe", (char *model, char *animspec, float *scale, float *blend, float *minw, float *minh, uint *children),
        BUILD(ModelPreview, o, o->setup(model, animspec, *scale, *blend, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiplayerpreview, "iiiisffffe", (int *model, int *colour, int *team, int *weapon, char *vanity, float *scale, float *blend, float *minw, float *minh, uint *children),
        BUILD(PlayerPreview, o, o->setup(*model, Color(*colour), *team, *weapon, vanity, *scale, *blend, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiprefabpreview, "sifffe", (char *prefab, int *colour, float *blend, float *minw, float *minh, uint *children),
        BUILD(PrefabPreview, o, o->setup(prefab, Color(*colour), *blend, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uislotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(SlotViewer, o, o->setup(*index, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uivslotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(VSlotViewer, o, o->setup(*index, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiminimap, "siffffe", (char *texname, int *c, float *dist, float *border, float *minw, float *minh, uint *children),
        BUILD(MiniMap, o, o->setup(textureload(texname, 3, true, false), Color(*c), *dist, *border, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiminimapcolour, "siiffffe", (char *texname, int *c, int *c2, float *dist, float *border, float *minw, float *minh, uint *children),
        BUILD(MiniMap, o, o->setup(textureload(texname, 3, true, false), Color(*c), Color(*c2), *dist, *border, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiradar, "fffffe", (float *dist, float *offset, float *border, float *minw, float *minh, uint *children),
        BUILD(Radar, o, o->setup(*dist, *offset, *border, *minw*uiscale, *minh*uiscale), children));

    ICOMMAND(0, uiradarblip, "sifffffe", (char *texname, int *c, float *yaw, float *blipyaw, float *dist, float *minw, float *minh, uint *children),
        BUILD(RadarBlip, o, o->setup(textureload(texname, 3, true, false), Color(*c), *yaw, *blipyaw, *dist, *minw*uiscale, *minh*uiscale), children));

    bool hasinput()
    {
        return world->allowinput();
    }

    bool hasmenu(bool pass)
    {
        return world->hasmenu(pass);
    }

    bool keypress(int code, bool isdown)
    {
        if(world->rawkey(code, isdown)) return true;
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
        if(action)
        {
            if(isdown)
            {
                if(hold) world->clearstate(hold);
                if(world->setstate(action, cursorx, cursory, 0, true, action|hold)) return true;
            }
            else if(hold)
            {
                if(world->setstate(action, cursorx, cursory, hold, true, action))
                {
                    world->clearstate(hold);
                    return true;
                }
                world->clearstate(hold);
            }
        }
        return world->key(code, isdown);
    }

    bool textinput(const char *str, int len)
    {
        return world->textinput(str, len);
    }

    void setup()
    {
        world = new World;
    }

    void cleanup()
    {
        world->children.setsize(0);
        enumerate(windows, Window *, w, delete w);
        windows.clear();
        DELETEP(world);
    }

    void calctextscale()
    {
        uitextscale = 1.0f/uitextrows;

        int tw = screenw, th = screenh;
        if(forceaspect) tw = int(ceil(th*forceaspect));
        gettextres(tw, th);
        uicontextscale = uitextscale/th;
    }

    void update()
    {
        float oldtextscale = curtextscale;
        curtextscale = 1;
        cursortype = CURSOR_DEFAULT;
        pushfont("default");
        readyeditors();

        world->setstate(STATE_HOVER, cursorx, cursory, world->childstate&STATE_HOLD_MASK);
        if(world->childstate&STATE_HOLD) world->setstate(STATE_HOLD, cursorx, cursory, STATE_HOLD, false);
        if(world->childstate&STATE_ALT_HOLD) world->setstate(STATE_ALT_HOLD, cursorx, cursory, STATE_ALT_HOLD, false);
        if(world->childstate&STATE_ESC_HOLD) world->setstate(STATE_ESC_HOLD, cursorx, cursory, STATE_ESC_HOLD, false);

        calctextscale();

        world->build();

        flusheditors();
        popfont();
        curtextscale = oldtextscale;
    }

    void render()
    {
        float oldtextscale = curtextscale;
        curtextscale = 1;
        pushfont("default");
        world->layout();
        world->adjustchildren();
        world->draw();
        popfont();
        curtextscale = oldtextscale;
    }

    editor *geteditor(const char *name, int mode, const char *init)
    {
        return useeditor(name, mode, false, init);
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

    void editoredit(editor *e, const char *init)
    {
        if(!e) return;
        useeditor(e->name, e->mode, true, init);
        e->clear(init);
        e->unfocus = true;
    }
}
