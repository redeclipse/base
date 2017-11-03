// renderparticles.cpp

#include "engine.h"

Shader *particleshader = NULL, *particlenotextureshader = NULL, *particlesoftshader = NULL, *particletextshader = NULL;

VAR(IDF_PERSIST, particlelayers, 0, 1, 1);
FVAR(IDF_PERSIST, particlebright, 0, 2, 100);
VAR(IDF_PERSIST, particlesize, 20, 100, 500);

VAR(IDF_PERSIST, softparticles, 0, 1, 1);
VAR(IDF_PERSIST, softparticleblend, 1, 8, 64);

// Check canemitparticles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VAR(IDF_PERSIST, emitmillis, 1, 17, VAR_MAX);
static int lastemitframe = 0, emitoffset = 0;
static bool canemit = false, regenemitters = false, canstep = false;

static bool canemitparticles()
{
    return canemit || emitoffset;
}

VAR(IDF_PERSIST, showparticles, 0, 1, 1);
VAR(0, cullparticles, 0, 1, 1);
VAR(0, replayparticles, 0, 1, 1);
VARN(0, seedparticles, seedmillis, 0, 3000, 10000);
VAR(0, dbgpcull, 0, 0, 1);
VAR(0, dbgpseed, 0, 0, 1);

struct particleemitter
{
    extentity *ent;
    vec bbmin, bbmax;
    vec center;
    float radius;
    ivec cullmin, cullmax;
    int maxfade, lastemit, lastcull;

    particleemitter(extentity *ent)
        : ent(ent), bbmin(ent->o), bbmax(ent->o), maxfade(-1), lastemit(0), lastcull(0)
    {}

    void finalize()
    {
        center = vec(bbmin).add(bbmax).mul(0.5f);
        radius = bbmin.dist(bbmax)/2;
        cullmin = ivec::floor(bbmin);
        cullmax = ivec::ceil(bbmax);
        if(dbgpseed) conoutf("radius: %f, maxfade: %d", radius, maxfade);
    }

    void extendbb(const vec &o, float size = 0)
    {
        bbmin.x = min(bbmin.x, o.x - size);
        bbmin.y = min(bbmin.y, o.y - size);
        bbmin.z = min(bbmin.z, o.z - size);
        bbmax.x = max(bbmax.x, o.x + size);
        bbmax.y = max(bbmax.y, o.y + size);
        bbmax.z = max(bbmax.z, o.z + size);
    }

    void extendbb(float z, float size = 0)
    {
        bbmin.z = min(bbmin.z, z - size);
        bbmax.z = max(bbmax.z, z + size);
    }
};

static vector<particleemitter> emitters;
static particleemitter *seedemitter = NULL;

void clearparticleemitters()
{
    emitters.setsize(0);
    regenemitters = true;
}

void addparticleemitters()
{
    emitters.setsize(0);
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_PARTICLES) continue;
        emitters.add(particleemitter(&e));
    }
    regenemitters = false;
}

const char *partnames[] = { "part", "tape", "trail", "text", "explosion", "lightning", "flare", "portal", "icon", "line", "triangle", "ellipse", "cone" };

struct partvert
{
    vec pos;
    bvec4 color;
    vec2 tc;
};

#define COLLIDERADIUS 8.0f
#define COLLIDEERROR 1.0f

struct partrenderer
{
    Texture *tex;
    const char *texname;
    int texclamp;
    uint type;
    string info;

    partrenderer(const char *texname, int texclamp, int type)
        : tex(NULL), texname(texname), texclamp(texclamp), type(type) { }
    partrenderer(int type)
        : tex(NULL), texname(NULL), texclamp(0), type(type) {}

    virtual ~partrenderer() { }

    virtual void init(int n) { }
    virtual void reset() = 0;
    virtual void resettracked(physent *owner) { }
    virtual particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) = 0;
    virtual void update() { }
    virtual void render() = 0;
    virtual bool haswork() = 0;
    virtual int count() = 0; //for debug
    virtual void cleanup() {}

    virtual void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
    }

    virtual void preload()
    {
        if(texname && !tex) tex = textureload(texname, texclamp);
    }

    //blend = 0 => remove it
    void calc(particle *p, int &blend, int &ts, float &size, bool step = true)
    {
        vec o = p->o;
        if(p->fade <= 5)
        {
            ts = 1;
            blend = 255;
            size = p->size;
        }
        else
        {
            ts = lastmillis-p->millis;
            blend = max(255-((ts<<8)/p->fade), 0);
            int weight = p->grav;
            if((type&PT_SHRINK || type&PT_GROW) && p->fade >= 50)
            {
                float amt = clamp(ts/float(p->fade), 0.f, 1.f);
                if(type&PT_SHRINK)
                {
                    if(type&PT_GROW) { if((amt *= 2) > 1) amt = 2-amt; amt *= amt; }
                    else amt = 1-(amt*amt);
                }
                else amt *= amt;
                size = p->size*amt;
                if(weight) weight += weight*(p->size-size);
            }
            else size = p->size;
            if(weight)
            {
                if(ts > p->fade) ts = p->fade;
                float secs = curtime/1000.f;
                vec v = vec(p->d).mul(secs);
                static struct particleent : physent
                {
                    particleent()
                    {
                        physent::reset();
                        type = ENT_DUMMY;
                    }
                } d;
                d.weight = weight;
                v.z -= physics::gravityvel(&d)*secs;
                p->o.add(v);
            }
            if(step && p->collide && p->o.z < p->val)
            {
                if(p->collide >= 0)
                {
                    vec surface;
                    float floorz = rayfloor(vec(p->o.x, p->o.y, p->val), surface, RAY_CLIPMAT, COLLIDERADIUS);
                    float collidez = floorz<0 ? o.z-COLLIDERADIUS : p->val - floorz;
                    if(p->o.z >= collidez+COLLIDEERROR) p->val = collidez+COLLIDEERROR;
                    else
                    {
                        addstain(p->collide, vec(p->o.x, p->o.y, collidez), vec(p->o).sub(o).normalize(), 2*p->size, p->color, type&PT_RND4 ? (p->flags>>5)&3 : 0);
                        blend = 0;
                    }
                }
                else blend = 0;
            }
            else p->m.add(vec(p->o).sub(o));
        }
        game::particletrack(p, type, ts, step);
    }

    void debuginfo()
    {
        formatstring(info, "%d\t%s(", count(), partnames[type&0xFF]);
        if(type&PT_LERP) concatstring(info, "l,");
        if(type&PT_MOD) concatstring(info, "m,");
        if(type&PT_RND4) concatstring(info, "r,");
        if(type&PT_FLIP) concatstring(info, "f,");
        int len = strlen(info);
        info[len-1] = info[len-1] == ',' ? ')' : '\0';
        if(texname)
        {
            const char *title = strrchr(texname, '/');
            if(title) concformatstring(info, ": %s", title+1);
        }
    }
};

template<class T>
struct listparticle : particle
{
    T *next;
};

struct sharedlistparticle : listparticle<sharedlistparticle> {};

template<class T>
struct listrenderer : partrenderer
{
    static T *parempty;
    T *list;

    listrenderer(const char *texname, int texclamp, int type)
        : partrenderer(texname, texclamp, type), list(NULL)
    {
    }
    listrenderer(int type)
        : partrenderer(type), list(NULL)
    {
    }

    virtual ~listrenderer()
    {
    }

    virtual void killpart(T *p)
    {
    }

    void reset()
    {
        if(!list) return;
        T *p = list;
        for(;;)
        {
            killpart(p);
            if(p->next) p = p->next;
            else break;
        }
        p->next = parempty;
        parempty = list;
        list = NULL;
    }

    void resettracked(physent *owner)
    {
        for(T **prev = &list, *cur = list; cur; cur = *prev)
        {
            if(!owner || cur->owner==owner)
            {
                *prev = cur->next;
                cur->next = parempty;
                parempty = cur;
            }
            else prev = &cur->next;
        }
    }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL)
    {
        if(!parempty)
        {
            T *ps = new T[256];
            loopi(255) ps[i].next = &ps[i+1];
            ps[255].next = parempty;
            parempty = ps;
        }
        T *p = parempty;
        parempty = p->next;
        p->next = list;
        list = p;
        p->o = o;
        p->m = vec(0, 0, 0);
        p->d = d;
        p->fade = fade;
        p->millis = lastmillis;
        p->color = bvec(color>>16, (color>>8)&0xFF, color&0xFF);
        p->size = size;
        p->blend = blend;
        p->grav = grav;
        p->collide = collide;
        if((p->owner = pl) != NULL && (p->owner->type == ENT_PLAYER || p->owner->type == ENT_AI)) switch(type&PT_TYPE)
        {
            case PT_TEXT: case PT_ICON: p->m.add(vec(p->o).sub(p->owner->abovehead())); break;
            default: break;
        }
        p->flags = 0;
        return p;
    }

    int count()
    {
        int num = 0;
        T *lp;
        for(lp = list; lp; lp = lp->next) num++;
        return num;
    }

    bool haswork()
    {
        return (list != NULL);
    }

    virtual void startrender() = 0;
    virtual void endrender() = 0;
    virtual void renderpart(T *p, int blend, int ts, float size) = 0;

    bool renderpart(T *p)
    {
        int blend, ts;
        float size;
        calc(p, blend, ts, size, canstep);
        if(blend <= 0) return false;
        renderpart(p, blend, ts, size);
        return p->fade > 5;
    }

    void render()
    {
        startrender();
        if(tex) glBindTexture(GL_TEXTURE_2D, tex->id);
        if(canstep) for(T **prev = &list, *p = list; p; p = *prev)
        {
            if(renderpart(p)) prev = &p->next;
            else
            { // remove
                *prev = p->next;
                p->next = parempty;
                killpart(p);
                parempty = p;
            }
        }
        else for(T *p = list; p; p = p->next) renderpart(p);
        endrender();
    }
};

template<class T> T *listrenderer<T>::parempty = NULL;

typedef listrenderer<sharedlistparticle> sharedlistrenderer;

struct textrenderer : sharedlistrenderer
{
    textrenderer(int type = 0)
        : sharedlistrenderer(type|PT_TEXT|PT_LERP|PT_SHADER|PT_NOLAYER)
    {}

    void startrender()
    {
        textshader = particletextshader;

        pushfont("default");
    }

    void endrender()
    {
        textshader = NULL;

        popfont();
    }

    void killpart(sharedlistparticle *p)
    {
        if(p->text && p->flags&1) delete[] p->text;
    }

    void renderpart(sharedlistparticle *p, int blend, int ts, float size)
    {
        const char *text = p->text;
        stringz(font);
        if(*text == '<')
        {
            const char *start = text;
            while(*text && *text != '>') text++;
            if(*text) { int len = text-(start+1); memcpy(font, start+1, len); font[len] = '\0'; text++; }
            else text = start;
        }
        pushfont(*font ? font : "default");
        float scale = p->size/80.0f, xoff = -text_widthf(text)*0.5f;

        matrix4x3 m(camright, vec(camup).neg(), vec(camdir).neg(), p->o);
        m.scale(scale);
        m.translate(xoff, 0, 50);

        textmatrix = &m;
        draw_text(text, 0, 0, p->color.r, p->color.g, p->color.b, int(p->blend*blend));
        textmatrix = NULL;
        popfont();
    }
};
static textrenderer texts, textontop(PT_ONTOP);

struct portal : listparticle<portal>
{
    float yaw, pitch;
};

struct portalrenderer : listrenderer<portal>
{
    portalrenderer(const char *texname)
        : listrenderer<portal>(texname, 3, PT_PORTAL|PT_LERP)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        gle::defvertex();
        gle::deftexcoord0();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_QUADS);
    }

    void endrender()
    {
        gle::end();
        glEnable(GL_CULL_FACE);
    }

    void renderpart(portal *p, int blend, int ts, float size)
    {
        matrix4x3 m(vec(size, 0, 0), vec(0, size, 0), vec(0, 0, size), p->o);
        m.rotate_around_z(p->yaw*RAD);
        m.rotate_around_x(p->pitch*RAD);

        bvec4 color(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        gle::attrib(m.transform(vec(-1, 0,  1))); gle::attribf(1, 0); gle::color(color);
        gle::attrib(m.transform(vec( 1, 0,  1))); gle::attribf(0, 0); gle::color(color);
        gle::attrib(m.transform(vec( 1, 0, -1))); gle::attribf(0, 1); gle::color(color);
        gle::attrib(m.transform(vec(-1, 0, -1))); gle::attribf(1, 1); gle::color(color);
    }

    portal *addportal(const vec &o, int fade, int color, float size, float blend, float yaw, float pitch)
    {
        portal *p = (portal *)listrenderer<portal>::addpart(o, vec(0, 0, 0), fade, color, size, blend);
        p->yaw = yaw;
        p->pitch = pitch;
        return p;
    }

    // use addportal() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};

struct icon : listparticle<icon>
{
    Texture *tex;
    float start, length, end;
};

struct iconrenderer : listrenderer<icon>
{
    Texture *lasttex;

    iconrenderer(int type = 0)
        : listrenderer<icon>(type|PT_ICON|PT_LERP)
    {}

    void startrender()
    {
        lasttex = NULL;
        gle::defvertex();
        gle::deftexcoord0();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_QUADS);
    }

    void endrender()
    {
        gle::end();
    }

    void renderpart(icon *p, int blend, int ts, float size)
    {
        if(p->tex != lasttex)
        {
            gle::end();
            glBindTexture(GL_TEXTURE_2D, p->tex->id);
            lasttex = p->tex;
        }

        matrix4x3 m(camright, vec(camup).neg(), vec(camdir).neg(), p->o);
        float aspect = p->tex->w/float(p->tex->h);
        m.scale(size*aspect, size, 1);

        #define iconvert(vx,vy) do { \
            gle::attrib(m.transform(vec2(vx, vy))); \
            gle::attribf(0.5f*(vx) + 0.5f, 0.5f*(vy) + 0.5f); \
            gle::attrib(color); \
        } while(0)

        bvec4 color(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        if(p->start > 0 || p->length < 1)
        {
            float sx = cosf((p->start + 0.25f)*2*M_PI), sy = -sinf((p->start + 0.25f)*2*M_PI),
                  ex = cosf((p->end + 0.25f)*2*M_PI), ey = -sinf((p->end + 0.25f)*2*M_PI);
            gle::end(); gle::begin(GL_TRIANGLE_FAN);
            iconvert(0, 0);

            if(p->start < 0.125f || p->start >= 0.875f) iconvert(sx/sy, -1);
            else if(p->start < 0.375f) iconvert(1, -sy/sx);
            else if(p->start < 0.625f) iconvert(-sx/sy, 1);
            else iconvert(-1, sy/sx);

            if(p->start <= 0.125f && p->end >= 0.125f) iconvert(1, -1);
            if(p->start <= 0.375f && p->end >= 0.375f) iconvert(1, 1);
            if(p->start <= 0.625f && p->end >= 0.625f) iconvert(-1, 1);
            if(p->start <= 0.875f && p->end >= 0.875f) iconvert(-1, -1);

            if(p->end < 0.125f || p->end >= 0.875f) iconvert(ex/ey, -1);
            else if(p->end < 0.375f) iconvert(1, -ey/ex);
            else if(p->end < 0.625f) iconvert(-ex/ey, 1);
            else iconvert(-1, ey/ex);
            gle::end(); gle::begin(GL_QUADS);
        }
        else
        {
            iconvert( 1,  1);
            iconvert(-1,  1);
            iconvert(-1, -1);
            iconvert( 1, -1);
        }
    }

    icon *addicon(const vec &o, Texture *tex, int fade, int color, float size, float blend, int grav, int collide, float start, float length, physent *pl = NULL)
    {
        icon *p = (icon *)listrenderer<icon>::addpart(o, vec(0, 0, 0), fade, color, size, blend, grav, collide);
        p->tex = tex;
        p->start = start;
        p->length = length;
        p->end = p->start + p->length;
        p->owner = pl;
        return p;
    }

    // use addicon() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};
static iconrenderer icons;

template<int T>
static inline void modifyblend(const vec &o, int &blend)
{
    blend = min(blend<<2, 255);
}

template<>
inline void modifyblend<PT_TAPE>(const vec &o, int &blend)
{
}

template<int T>
static inline void genpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs)
{
    vec udir = vec(camup).sub(camright).mul(size);
    vec vdir = vec(camup).add(camright).mul(size);
    vs[0].pos = vec(o.x + udir.x, o.y + udir.y, o.z + udir.z);
    vs[1].pos = vec(o.x + vdir.x, o.y + vdir.y, o.z + vdir.z);
    vs[2].pos = vec(o.x - udir.x, o.y - udir.y, o.z - udir.z);
    vs[3].pos = vec(o.x - vdir.x, o.y - vdir.y, o.z - vdir.z);
}

template<>
inline void genpos<PT_TAPE>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec dir1 = vec(d).sub(o), dir2 = vec(d).sub(camera1->o), c;
    c.cross(dir2, dir1).normalize().mul(size);
    vs[0].pos = vec(d.x-c.x, d.y-c.y, d.z-c.z);
    vs[1].pos = vec(o.x-c.x, o.y-c.y, o.z-c.z);
    vs[2].pos = vec(o.x+c.x, o.y+c.y, o.z+c.z);
    vs[3].pos = vec(d.x+c.x, d.y+c.y, d.z+c.z);
}

template<>
inline void genpos<PT_TRAIL>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec e = d;
    if(grav) e.z -= float(ts)/grav;
    e.div(-75.0f).add(o);
    genpos<PT_TAPE>(o, e, size, ts, grav, vs);
}

template<int T>
static inline void genrotpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    genpos<T>(o, d, size, grav, ts, vs);
}

#define ROTCOEFFS(n) { \
    vec2(-1,  1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2( 1,  1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2( 1, -1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2(-1, -1).rotate_around_z(n*2*M_PI/32.0f) \
}
static const vec2 rotcoeffs[32][4] =
{
    ROTCOEFFS(0),  ROTCOEFFS(1),  ROTCOEFFS(2),  ROTCOEFFS(3),  ROTCOEFFS(4),  ROTCOEFFS(5),  ROTCOEFFS(6),  ROTCOEFFS(7),
    ROTCOEFFS(8),  ROTCOEFFS(9),  ROTCOEFFS(10), ROTCOEFFS(11), ROTCOEFFS(12), ROTCOEFFS(13), ROTCOEFFS(14), ROTCOEFFS(15),
    ROTCOEFFS(16), ROTCOEFFS(17), ROTCOEFFS(18), ROTCOEFFS(19), ROTCOEFFS(20), ROTCOEFFS(21), ROTCOEFFS(22), ROTCOEFFS(7),
    ROTCOEFFS(24), ROTCOEFFS(25), ROTCOEFFS(26), ROTCOEFFS(27), ROTCOEFFS(28), ROTCOEFFS(29), ROTCOEFFS(30), ROTCOEFFS(31),
};

template<>
inline void genrotpos<PT_PART>(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    const vec2 *coeffs = rotcoeffs[rot];
    vs[0].pos = vec(o).madd(camright, coeffs[0].x*size).madd(camup, coeffs[0].y*size);
    vs[1].pos = vec(o).madd(camright, coeffs[1].x*size).madd(camup, coeffs[1].y*size);
    vs[2].pos = vec(o).madd(camright, coeffs[2].x*size).madd(camup, coeffs[2].y*size);
    vs[3].pos = vec(o).madd(camright, coeffs[3].x*size).madd(camup, coeffs[3].y*size);
}

template<int T>
static inline void seedpos(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    if(grav)
    {
        float t = fade;
        vec end = vec(o).madd(d, t/5000.0f);
        end.z -= t*t/(2.0f * 5000.0f * grav);
        pe.extendbb(end, size);

        float tpeak = d.z*grav;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }
}

template<>
inline void seedpos<PT_TAPE>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    pe.extendbb(d, size);
}

template<>
inline void seedpos<PT_TRAIL>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    vec e = d;
    if(grav) e.z -= float(fade)/grav;
    e.div(-75.0f).add(o);
    pe.extendbb(e, size);
}

template<int T>
struct varenderer : partrenderer
{
    partvert *verts;
    particle *parts;
    int maxparts, numparts, lastupdate, rndmask;
    GLuint vbo;

    varenderer(const char *texname, int type, int texclamp = 3)
        : partrenderer(texname, texclamp, type),
          verts(NULL), parts(NULL), maxparts(0), numparts(0), lastupdate(-1), rndmask(0), vbo(0)
    {
        if(type & PT_HFLIP) rndmask |= 0x01;
        if(type & PT_VFLIP) rndmask |= 0x02;
        if(type & PT_ROT) rndmask |= 0x1F<<2;
        if(type & PT_RND4) rndmask |= 0x03<<5;
    }

    void cleanup()
    {
        if(vbo) { glDeleteBuffers_(1, &vbo); vbo = 0; }
    }

    void init(int n)
    {
        DELETEA(parts);
        DELETEA(verts);
        parts = new particle[n];
        verts = new partvert[n*4];
        maxparts = n;
        numparts = 0;
        lastupdate = -1;
    }

    void reset()
    {
        numparts = 0;
        lastupdate = -1;
    }

    void resettracked(physent *owner)
    {
        loopi(numparts)
        {
            particle *p = parts+i;
            if(!owner || (p->owner == owner)) p->fade = -1;
        }
        lastupdate = -1;
    }

    int count()
    {
        return numparts;
    }

    bool haswork()
    {
        return (numparts > 0);
    }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL)
    {
        particle *p = parts + (numparts < maxparts ? numparts++ : rnd(maxparts)); //next free slot, or kill a random kitten
        p->o = o;
        p->d = d;
        p->m = vec(0, 0, 0);
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec(color>>16, (color>>8)&0xFF, color&0xFF);
        p->blend = blend;
        p->size = size;
        p->grav = grav;
        p->collide = collide;
        p->owner = pl;
        p->flags = 0x80 | (rndmask ? rnd(0x80) & rndmask : 0);
        lastupdate = -1;
        return p;
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
        pe.maxfade = max(pe.maxfade, fade);
        size *= SQRT2;
        pe.extendbb(o, size);

        seedpos<T>(pe, o, d, fade, size, gravity);
        if(!gravity) return;

        vec end(o);
        float t = fade;
        end.add(vec(d).mul(t/5000.0f));
        end.z -= t*t/(2.0f * 5000.0f * gravity);
        pe.extendbb(end, size);

        float tpeak = d.z*gravity;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }

    void genverts(particle *p, partvert *vs, bool regen)
    {
        int blend = 255, ts = 1;
        float size = 1;
        calc(p, blend, ts, size);
        if(blend <= 1 || p->fade <= 5) p->fade = -1; //mark to remove on next pass (i.e. after render)

        modifyblend<T>(p->o, blend);

        if(regen)
        {
            p->flags &= ~0x80;

            #define SETTEXCOORDS(u1c, u2c, v1c, v2c, body) \
            { \
                float u1 = u1c, u2 = u2c, v1 = v1c, v2 = v2c; \
                body; \
                vs[0].tc = vec2(u1, v1); \
                vs[1].tc = vec2(u2, v1); \
                vs[2].tc = vec2(u2, v2); \
                vs[3].tc = vec2(u1, v2); \
            }
            if(type&PT_RND4)
            {
                float tx = 0.5f*((p->flags>>5)&1), ty = 0.5f*((p->flags>>6)&1);
                SETTEXCOORDS(tx, tx + 0.5f, ty, ty + 0.5f,
                {
                    if(p->flags&0x01) swap(u1, u2);
                    if(p->flags&0x02) swap(v1, v2);
                });

            }
            else if(type&PT_ICON)
            {
                float tx = 0.25f*(p->flags&3), ty = 0.25f*((p->flags>>2)&3);
                SETTEXCOORDS(tx, tx + 0.25f, ty, ty + 0.25f, {});
            }
            else SETTEXCOORDS(0, 1, 0, 1, {});

            #define SETCOLOR(r, g, b, a) \
            do { \
                bvec4 col(r, g, b, a); \
                loopi(4) vs[i].color = col; \
            } while(0)
            #define SETMODCOLOR SETCOLOR((p->color.r*blend)>>8, (p->color.g*blend)>>8, (p->color.b*blend)>>8, uchar(p->blend*255))
            if(type&PT_MOD) SETMODCOLOR;
            else SETCOLOR(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        }
        else if(type&PT_MOD) SETMODCOLOR;
        else loopi(4) vs[i].color.a = uchar(p->blend*blend);

        if(type&PT_ROT) genrotpos<T>(p->o, p->d, size, ts, p->grav, vs, (p->flags>>2)&0x1F);
        else genpos<T>(p->o, p->d, size, ts, p->grav, vs);
    }

    void genverts()
    {
        loopi(numparts)
        {
            particle *p = &parts[i];
            partvert *vs = &verts[i*4];
            if(p->fade < 0)
            {
                do
                {
                    --numparts;
                    if(numparts <= i) return;
                }
                while(parts[numparts].fade < 0);
                *p = parts[numparts];
                genverts(p, vs, true);
            }
            else genverts(p, vs, (p->flags&0x80)!=0);
        }
    }

    void genvbo()
    {
        if(lastmillis == lastupdate && vbo) return;
        lastupdate = lastmillis;

        genverts();

        if(!vbo) glGenBuffers_(1, &vbo);
        gle::bindvbo(vbo);
        glBufferData_(GL_ARRAY_BUFFER, maxparts*4*sizeof(partvert), NULL, GL_STREAM_DRAW);
        glBufferSubData_(GL_ARRAY_BUFFER, 0, numparts*4*sizeof(partvert), verts);
        gle::clearvbo();
    }

    void render()
    {
        genvbo();

        glBindTexture(GL_TEXTURE_2D, tex->id);

        gle::bindvbo(vbo);
        const partvert *ptr = 0;
        gle::vertexpointer(sizeof(partvert), ptr->pos.v);
        gle::texcoord0pointer(sizeof(partvert), ptr->tc.v);
        gle::colorpointer(sizeof(partvert), ptr->color.v);
        gle::enablevertex();
        gle::enabletexcoord0();
        gle::enablecolor();
        gle::enablequads();

        gle::drawquads(0, numparts);

        gle::disablequads();
        gle::disablevertex();
        gle::disabletexcoord0();
        gle::disablecolor();
        gle::clearvbo();
    }
};

typedef varenderer<PT_PART> quadrenderer;
typedef varenderer<PT_TAPE> taperenderer;
typedef varenderer<PT_TRAIL> trailrenderer;

#include "explosion.h"
#include "lensflare.h"
#include "lightning.h"

struct softquadrenderer : quadrenderer
{
    softquadrenderer(const char *texname, int type)
        : quadrenderer(texname, type|PT_SOFT)
    {
    }
};

struct lineprimitive : listparticle<lineprimitive>
{
    vec value;
};

struct lineprimitiverenderer : listrenderer<lineprimitive>
{
    lineprimitiverenderer(int type = 0)
        : listrenderer<lineprimitive>(type|PT_LINE|PT_LERP)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        gle::defvertex();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_LINES);
    }

    void endrender()
    {
        gle::end();
        glEnable(GL_CULL_FACE);
    }

    void renderpart(lineprimitive *p, int blend, int ts, float size)
    {
        glBindTexture(GL_TEXTURE_2D, blanktexture->id);
        bvec4 color(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        gle::attrib(p->o);
            gle::attrib(color);
        gle::attrib(vec(p->value).mul(size).add(p->o));
            gle::attrib(color);
    }

    lineprimitive *addline(const vec &o, const vec &v, int fade, int color, float size, float blend)
    {
        lineprimitive *p = (lineprimitive *)listrenderer<lineprimitive>::addpart(o, vec(0, 0, 0), fade, color, size, blend);
        p->value = vec(v).sub(o).div(size);
        return p;
    }

    // use addline() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};
static lineprimitiverenderer lineprimitives, lineontopprimitives(PT_ONTOP);

struct trisprimitive : listparticle<trisprimitive>
{
    vec value[2];
    bool fill;
};

struct trisprimitiverenderer : listrenderer<trisprimitive>
{
    trisprimitiverenderer(int type = 0)
        : listrenderer<trisprimitive>(type|PT_TRIANGLE|PT_LERP)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        gle::defvertex();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_TRIANGLES);
    }

    void endrender()
    {
        gle::end();
        glEnable(GL_CULL_FACE);
    }

    void renderpart(trisprimitive *p, int blend, int ts, float size)
    {
        glBindTexture(GL_TEXTURE_2D, blanktexture->id);
        bvec4 color(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        if(!p->fill) { gle::end(); gle::begin(GL_LINE_LOOP); }
        gle::attrib(p->o);
            gle::attrib(color);
        gle::attrib(vec(p->value[0]).mul(size).add(p->o));
            gle::attrib(color);
        gle::attrib(vec(p->value[1]).mul(size).add(p->o));
            gle::attrib(color);
        if(!p->fill) { gle::end(); gle::begin(GL_TRIANGLES); }
    }

    trisprimitive *addtriangle(const vec &o, float yaw, float pitch, int fade, int color, float size, float blend, bool fill)
    {
        vec dir[3];
        vecfromyawpitch(yaw, pitch, 1, 0, dir[0]);
        vecfromyawpitch(yaw, pitch, -1, 1, dir[1]);
        vecfromyawpitch(yaw, pitch, -1, -1, dir[2]);

        trisprimitive *p = (trisprimitive *)listrenderer<trisprimitive>::addpart(dir[0].mul(size*2).add(o), vec(0, 0, 0), fade, color, size, blend);
        p->value[0] = dir[1];
        p->value[1] = dir[2];
        p->fill = fill;
        return p;
    }

    // use addtriangle() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};
static trisprimitiverenderer trisprimitives, trisontopprimitives(PT_ONTOP);

struct loopprimitive : listparticle<loopprimitive>
{
    vec value;
    int axis;
    bool fill;
};

struct loopprimitiverenderer : listrenderer<loopprimitive>
{
    loopprimitiverenderer(int type = 0)
        : listrenderer<loopprimitive>(type|PT_ELLIPSE|PT_LERP)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        gle::defvertex();
    }

    void endrender()
    {
        glEnable(GL_CULL_FACE);
    }

    void renderpart(loopprimitive *p, int blend, int ts, float size)
    {
        glBindTexture(GL_TEXTURE_2D, blanktexture->id);
        gle::colorub(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        gle::begin(p->fill ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
        loopi(15 + (p->fill ? 1 : 0))
        {
            const vec2 &sc = sincos360[i*(360/15)];
            vec v;
            switch(p->axis)
            {
                case 0:
                    v = vec(p->value.x*sc.x, 0, p->value.z*sc.y);
                    break;
                case 1:
                    v = vec(0, p->value.y*sc.y, p->value.z*sc.x);
                    break;
                case 2: default:
                    v = vec(-p->value.y*sc.y, p->value.x*sc.x, 0);
                    break;
            }
            gle::attrib(v.mul(size).add(p->o));
        }
        gle::end();
    }

    loopprimitive *addellipse(const vec &o, const vec &v, int fade, int color, float size, float blend, int axis, bool fill)
    {
        loopprimitive *p = (loopprimitive *)listrenderer<loopprimitive>::addpart(o, vec(0, 0, 0), fade, color, size, blend);
        p->value = vec(v).div(size);
        p->axis = axis;
        p->fill = fill;
        return p;
    }

    // use addellipse() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};
static loopprimitiverenderer loopprimitives, loopontopprimitives(PT_ONTOP);

struct coneprimitive : listparticle<coneprimitive>
{
    vec dir, spot, spoke;
    float radius, angle;
    bool fill;
};

struct coneprimitiverenderer : listrenderer<coneprimitive>
{
    coneprimitiverenderer(int type = 0)
        : listrenderer<coneprimitive>(type|PT_CONE|PT_LERP)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        gle::defvertex();
    }

    void endrender()
    {
        glEnable(GL_CULL_FACE);
    }

    void renderpart(coneprimitive *p, int blend, int ts, float size)
    {
        glBindTexture(GL_TEXTURE_2D, blanktexture->id);
        gle::colorub(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));

        gle::begin(GL_LINES);
        loopi(15)
        {
            gle::attrib(p->o);
            gle::attrib(vec(p->spoke).rotate(sincos360[i*(360/15)], p->dir).add(p->spot).mul(size).add(p->o));
        }
        gle::end();

        gle::begin(GL_LINE_LOOP);
        loopi(15)
        {
            gle::attrib(vec(p->spoke).rotate(sincos360[i*(360/15)], p->dir).add(p->spot).mul(size).add(p->o));
        }
        gle::end();
    }

    coneprimitive *addcone(const vec &o, const vec &dir, float radius, float angle, int fade, int color, float size, float blend, bool fill)
    {
        coneprimitive *p = (coneprimitive *)listrenderer<coneprimitive>::addpart(o, vec(0, 0, 0), fade, color, size, blend);
        p->dir = dir;
        p->radius = radius;
        p->angle = angle;
        p->fill = fill;
        p->spot = vec(p->dir).mul(p->radius*cosf(p->angle*RAD));
        p->spoke.orthogonal(p->dir);
        p->spoke.normalize().mul(p->radius*sinf(p->angle*RAD));
        return p;
    }

    // use addcone() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};
static coneprimitiverenderer coneprimitives, coneontopprimitives(PT_ONTOP);

static partrenderer *parts[] =
{
    new portalrenderer("<grey>particles/teleport"), &icons,
    &lineprimitives, &lineontopprimitives, &trisprimitives, &trisontopprimitives,
    &loopprimitives, &loopontopprimitives, &coneprimitives, &coneontopprimitives,
    new softquadrenderer("<grey>particles/fire", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new softquadrenderer("<grey>particles/plasma", PT_PART|PT_BRIGHT|PT_FLIP|PT_SHRINK),
    new taperenderer("<grey>particles/sflare", PT_TAPE|PT_BRIGHT|PT_FEW),
    new taperenderer("<grey>particles/mflare", PT_TAPE|PT_BRIGHT|PT_RND4|PT_VFLIP|PT_FEW),
    new softquadrenderer("<grey>particles/smoke", PT_PART|PT_LERP|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/smoke", PT_PART|PT_LERP|PT_FLIP|PT_SHRINK),
    new softquadrenderer("<grey>particles/hint", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/hint", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey>particles/hint_bold", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/hint_bold", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey>particles/hint_vert", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey><rotate:1>particles/hint_vert", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey><rotate:1>particles/hint_vert", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/hint_vert", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey>particles/smoke", PT_PART|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/smoke", PT_PART|PT_FLIP|PT_SHRINK),
    new softquadrenderer("<grey>particles/hint", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/hint", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey>particles/hint_bold", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/hint_bold", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey>particles/hint_vert", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/hint_vert", PT_PART|PT_BRIGHT),
    new softquadrenderer("<grey><rotate:1>particles/hint_vert", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey><rotate:1>particles/hint_vert", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/blood", PT_PART|PT_MOD|PT_RND4|PT_FLIP),
    new quadrenderer("<grey>particles/entity", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/entity", PT_PART|PT_BRIGHT|PT_ONTOP),
    new quadrenderer("<grey>particles/spark", PT_PART|PT_BRIGHT|PT_FLIP|PT_SHRINK|PT_GROW),
    new softquadrenderer("<grey>particles/fire", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/fire", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new softquadrenderer("<grey>particles/plasma", PT_PART|PT_BRIGHT|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/plasma", PT_PART|PT_BRIGHT|PT_FLIP|PT_SHRINK),
    new softquadrenderer("<grey>particles/electric", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/electric", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new softquadrenderer("<grey>particles/eleczap", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/eleczap", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_SHRINK),
    new quadrenderer("<grey>particles/fire", PT_PART|PT_BRIGHT|PT_FLIP|PT_RND4|PT_BRIGHT|PT_SHRINK),
    new taperenderer("<grey>particles/sflare", PT_TAPE|PT_BRIGHT),
    new taperenderer("<grey>particles/mflare", PT_TAPE|PT_BRIGHT|PT_RND4|PT_VFLIP|PT_BRIGHT),
    new taperenderer("<grey>particles/lightning", PT_TAPE|PT_BRIGHT|PT_HFLIP|PT_VFLIP, 2), // uses same clamp setting as normal lightning to avoid conflict
    new taperenderer("<grey>particles/lightzap", PT_TAPE|PT_BRIGHT|PT_HFLIP|PT_VFLIP, 2),
    new quadrenderer("<grey>particles/muzzle", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP),
    new quadrenderer("<grey>particles/snow", PT_PART|PT_BRIGHT|PT_FLIP),
    &texts, &textontop,
    &explosions, &shockwaves, &shockballs, &lightnings, &lightzaps,
    &flares // must be done last!
};

VARF(IDF_PERSIST, maxparticles, 10, 4000, 10000, initparticles());
VARF(IDF_PERSIST, fewparticles, 10, 100, 10000, initparticles());

int partsorder[sizeof(parts)/sizeof(parts[0])];

void orderparts()
{
    int j = 0, n = sizeof(parts)/sizeof(parts[0]);
    loopi(n) if(!(parts[i]->type&PT_LERP)) partsorder[j++] = i;
    loopi(n) if(parts[i]->type&PT_LERP) partsorder[j++] = i;
}

void initparticles()
{
    if(initing) return;
    if(!particleshader) particleshader = lookupshaderbyname("particle");
    if(!particlenotextureshader) particlenotextureshader = lookupshaderbyname("particlenotexture");
    if(!particlesoftshader) particlesoftshader = lookupshaderbyname("particlesoft");
    if(!particletextshader) particletextshader = lookupshaderbyname("particletext");
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->init(parts[i]->type&PT_FEW ? min(fewparticles, maxparticles) : maxparticles);
    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        loadprogress = float(i+1)/(sizeof(parts)/sizeof(parts[0]));
        parts[i]->preload();
    }
    loadprogress = 0;
    orderparts();
}

void clearparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->reset();
    clearparticleemitters();
}

void cleanupparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->cleanup();
}

void removetrackedparticles(physent *owner)
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->resettracked(owner);
}

VARN(0, debugparticles, dbgparts, 0, 0, 1);

void debugparticles()
{
    if(!dbgparts) return;
    int n = sizeof(parts)/sizeof(parts[0]);
    pushhudmatrix();
    hudmatrix.ortho(0, FONTH*n*2*vieww/float(viewh), FONTH*n*2, 0, -1, 1); // squeeze into top-left corner
    flushhudmatrix();
    loopi(n) draw_text(parts[i]->info, FONTH, (i+n/2)*FONTH);
    pophudmatrix();
}

void renderparticles(int layer)
{
    canstep = layer != PL_UNDER;

    //want to debug BEFORE the lastpass render (that would delete particles)
    if(dbgparts && (layer == PL_ALL || layer == PL_UNDER)) loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->debuginfo();

    bool rendered = false;
    uint lastflags = PT_LERP|PT_SHADER,
         flagmask = PT_LERP|PT_MOD|PT_ONTOP|PT_BRIGHT|PT_NOTEX|PT_SOFT|PT_SHADER,
         excludemask = layer == PL_ALL ? ~0 : (layer != PL_NOLAYER ? PT_NOLAYER : 0);

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        partrenderer *p = parts[partsorder[i]];
        if((p->type&PT_NOLAYER) == excludemask || !p->haswork()) continue;

        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture_(GL_TEXTURE2);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
            glActiveTexture_(GL_TEXTURE0);
        }

        uint flags = p->type & flagmask, changedbits = (flags ^ lastflags);
        if(changedbits)
        {
            if(changedbits&PT_LERP) { if(flags&PT_LERP) resetfogcolor(); else zerofogcolor(); }
            if(changedbits&(PT_LERP|PT_MOD))
            {
                if(flags&PT_LERP) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                else if(flags&PT_MOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            if(!(flags&PT_SHADER))
            {
                if(changedbits&(PT_LERP|PT_SOFT|PT_NOTEX|PT_SHADER))
                {
                    if(flags&PT_SOFT && softparticles)
                    {
                        particlesoftshader->set();
                        LOCALPARAMF(softparams, -1.0f/softparticleblend, 0, 0);
                    }
                    else if(flags&PT_NOTEX) particlenotextureshader->set();
                    else particleshader->set();
                }
                if(changedbits&(PT_MOD|PT_BRIGHT|PT_SOFT|PT_NOTEX|PT_SHADER))
                {
                    float colorscale = flags&PT_MOD ? 1 : ldrscale;
                    if(flags&PT_BRIGHT) colorscale *= particlebright;
                    LOCALPARAMF(colorscale, colorscale, colorscale, colorscale, 1);
                }
            }
            if(changedbits&PT_ONTOP)
            {
                if(flags&PT_ONTOP) glDisable(GL_DEPTH_TEST);
                else glEnable(GL_DEPTH_TEST);
            }
            lastflags = flags;
        }
        p->render();
    }

    if(rendered)
    {
        if(lastflags&(PT_LERP|PT_MOD)) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        if(!(lastflags&PT_LERP)) resetfogcolor();
        if(lastflags&PT_ONTOP) glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

static int addedparticles = 0;

particle *newparticle(const vec &o, const vec &d, int fade, int type, int color, float size, float blend, int grav, int collide, physent *pl)
{
    static particle dummy;
    if(seedemitter)
    {
        parts[type]->seedemitter(*seedemitter, o, d, fade, size, grav);
        return &dummy;
    }
    if(fade + emitoffset < 0) return &dummy;
    addedparticles++;
    return parts[type]->addpart(o, d, fade, color, size, blend, grav, collide, pl);
}

void create(int type, int color, int fade, const vec &p, float size, float blend, int grav, int collide, physent *pl)
{
    if(camera1->o.dist(p) > maxparticledistance) return;
    float collidez = collide ? p.z - raycube(p, vec(0, 0, -1), collide >= 0 ? COLLIDERADIUS : max(p.z, 0.0f), RAY_CLIPMAT) + (collide >= 0 ? COLLIDEERROR : 0) : -1;
    int fmin = 1;
    int fmax = fade*3;
    int f = fmin + rnd(fmax); //help deallocater by using fade distribution rather than random
    newparticle(p, vec(0, 0, 0), f, type, color, size, blend, grav, collide, pl)->val = collidez;
}

void regularcreate(int type, int color, int fade, const vec &p, float size, float blend, int grav, int collide, physent *pl, int delay)
{
    if(!canemitparticles() || (delay > 0 && rnd(delay) != 0)) return;
    create(type, color, fade, p, size, blend, grav, collide, pl);
}

VAR(IDF_PERSIST, maxparticledistance, 256, 1024, 4096);

void splash(int type, int color, float radius, int num, int fade, const vec &p, float size, float blend, int grav, int collide, float vel)
{
    if(camera1->o.dist(p) > maxparticledistance && !seedemitter) return;
#if 0
    float collidez = collide ? p.z - raycube(p, vec(0, 0, -1), COLLIDERADIUS, RAY_CLIPMAT) + (collide >= 0 ? COLLIDEERROR : 0) : -1;
    int fmin = 1;
    int fmax = fade*3;
    loopi(num)
    {
        vec tmp(rnd(max(int(ceilf(radius*2)),1))-radius, rnd(max(int(ceilf(radius*2)),1))-radius, rnd(max(int(ceilf(radius*2)),1))-radius);
        int f = (num < 10) ? (fmin + rnd(fmax)) : (fmax - (i*(fmax-fmin))/(num-1)); //help deallocater by using fade distribution rather than random
        newparticle(p, tmp, f, type, color, size, blend, grav, collide)->val = collidez;
    }
#endif
    regularshape(type, radius, color, 21, num, fade, p, size, blend, grav, collide, vel);
}

void regularsplash(int type, int color, float radius, int num, int fade, const vec &p, float size, float blend, int grav, int collide, float vel, int delay)
{
    if(!canemitparticles() || (delay > 0 && rnd(delay) != 0)) return;
    splash(type, color, radius, num, fade, p, size, blend, grav, collide, vel);
}

bool canaddparticles()
{
    return !minimized;
}

void regular_part_create(int type, int fade, const vec &p, int color, float size, float blend, int grav, int collide, physent *pl, int delay)
{
    if(!canaddparticles()) return;
    regularcreate(type, color, fade, p, size, blend, grav, collide, pl, delay);
}

void part_create(int type, int fade, const vec &p, int color, float size, float blend, int grav, int collide, physent *pl)
{
    if(!canaddparticles()) return;
    create(type, color, fade, p, size, blend, grav, collide, pl);
}

void regular_part_splash(int type, int num, int fade, const vec &p, int color, float size, float blend, int grav, int collide, float radius, float vel, int delay)
{
    if(!canaddparticles()) return;
    regularsplash(type, color, radius, num, fade, p, size, blend, grav, collide, vel, delay);
}

void part_splash(int type, int num, int fade, const vec &p, int color, float size, float blend, int grav, int collide, float radius, float vel)
{
    if(!canaddparticles()) return;
    splash(type, color, radius, num, fade, p, size, blend, grav, collide, vel);
}

VAR(IDF_PERSIST, maxparticletrail, 256, 512, VAR_MAX);

void part_trail(int type, int fade, const vec &s, const vec &e, int color, float size, float blend, int grav, int collide)
{
    if(!canaddparticles()) return;
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*2), 1, maxparticletrail);
    v.div(steps);
    vec p = s;
    loopi(steps)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, type, color, size, blend, grav, collide);
    }
}

VAR(IDF_PERSIST, particletext, 0, 1, 1);
VAR(IDF_PERSIST, maxparticletextdistance, 0, 512, 10000);

void part_text(const vec &s, const char *t, int type, int fade, int color, float size, float blend, int grav, int collide, physent *pl)
{
    if(!canaddparticles() || !t[0]) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, blend, grav, collide, pl);
    p->text = t;
}

void part_textcopy(const vec &s, const char *t, int type, int fade, int color, float size, float blend, int grav, int collide, physent *pl)
{
    if(!canaddparticles() || !t[0]) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, blend, grav, collide, pl);
    p->text = newstring(t);
    p->flags = 1;
}

void part_flare(const vec &p, const vec &dest, int fade, int type, int color, float size, float blend, int grav, int collide, physent *pl)
{
    if(!canaddparticles()) return;
    newparticle(p, dest, fade, type, color, size, blend, grav, collide, pl);
}

void part_explosion(const vec &dest, float maxsize, int type, int fade, int color, float size, float blend, int grav, int collide)
{
    if(!canaddparticles()) return;
    float growth = maxsize - size;
    if(fade < 0) fade = int(growth*25);
    newparticle(dest, vec(0, 0, 1), fade, type, color, size, blend, grav, collide)->val = growth;
}

void regular_part_explosion(const vec &dest, float maxsize, int type, int fade, int color, float size, float blend, int grav, int collide)
{
    if(!canaddparticles() || !canemitparticles()) return;
    part_explosion(dest, maxsize, type, fade, color, size, blend, grav, collide);
}

void part_spawn(const vec &o, const vec &v, float z, uchar type, int amt, int fade, int color, float size, float blend, int grav, int collide)
{
    if(!canaddparticles()) return;
    loopi(amt)
    {
        vec w(rnd(int(v.x*2))-int(v.x), rnd(int(v.y*2))-int(v.y), rnd(int(v.z*2))-int(v.z)+z);
        w.add(o);
        part_splash(type, 1, fade, w, color, size, blend, grav, collide, 1, 1);
    }
}

void part_flares(const vec &o, const vec &v, float z1, const vec &d, const vec &w, float z2, uchar type, int amt, int fade, int color, float size, float blend, int grav, int collide, physent *pl)
{
    if(!canaddparticles()) return;
    loopi(amt)
    {
        vec from(rnd(int(v.x*2))-int(v.x), rnd(int(v.y*2))-int(v.y), rnd(int(v.z*2))-int(v.z)+z1);
        from.add(o);

        vec to(rnd(int(w.x*2))-int(w.x), rnd(int(w.y*2))-int(w.y), rnd(int(w.z*2))-int(w.z)+z1);
        to.add(d);

        newparticle(from, to, fade, type, color, size, blend, grav, collide, pl);
    }
}

void part_portal(const vec &o, float size, float blend, float yaw, float pitch, int type, int fade, int color)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_PORTAL) return;
    portalrenderer *p = (portalrenderer *)parts[type];
    p->addportal(o, fade, color, size, blend, yaw, pitch);
}

VAR(IDF_PERSIST, particleicon, 0, 1, 1);
VAR(IDF_PERSIST, maxparticleicondistance, 0, 512, 10000);

void part_icon(const vec &o, Texture *tex, float size, float blend, int grav, int collide, int fade, int color, float start, float length, physent *pl)
{
    if(!canaddparticles() || (parts[PART_ICON]->type&PT_TYPE) != PT_ICON) return;
    if(!particleicon || camera1->o.dist(o) > maxparticleicondistance) return;
    iconrenderer *p = (iconrenderer *)parts[PART_ICON];
    p->addicon(o, tex, fade, color, size, blend, grav, collide, start, length, pl);
}

void part_line(const vec &o, const vec &v, float size, float blend, int fade, int color, int type)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_LINE) return;
    lineprimitiverenderer *p = (lineprimitiverenderer *)parts[type];
    p->addline(o, v, fade, color, size, blend);
}

void part_triangle(const vec &o, float yaw, float pitch, float size, float blend, int fade, int color, bool fill, int type)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_TRIANGLE) return;
    trisprimitiverenderer *p = (trisprimitiverenderer *)parts[type];
    p->addtriangle(o, yaw, pitch, fade, color, size, blend, fill);
}

void part_dir(const vec &o, float yaw, float pitch, float length, float size, float blend, int fade, int color, int interval, bool fill)
{
    if(!canaddparticles()) return;

    vec v(yaw*RAD, pitch*RAD);
    part_line(o, vec(v).mul(length).add(o), size, blend, fade, color);
    if(interval)
    {
        int count = int(length/float(interval));
        vec q = o;
        loopi(count)
        {
            q.add(vec(v).mul(interval));
            part_triangle(q, yaw, pitch, size, blend, fade, color, fill);
        }
    }
    part_triangle(vec(v).mul(length-size*2).add(o), yaw, pitch, size, blend, fade, color, fill);
}

void part_trace(const vec &o, const vec &v, float size, float blend, int fade, int color, int interval, bool fill)
{
    part_line(o, v, size, blend, fade, color);
    float yaw, pitch;
    vec dir = vec(v).sub(o).normalize();
    vectoyawpitch(dir, yaw, pitch);
    if(interval)
    {
        int count = int(v.dist(o)/float(interval));
        vec q = o;
        loopi(count)
        {
            q.add(vec(dir).mul(interval));
            part_triangle(q, yaw, pitch, size, blend, fade, color, fill);
        }
    }
    part_triangle(vec(v).sub(vec(dir).mul(size*2)), yaw, pitch, size, blend, fade, color, fill);
}

void part_ellipse(const vec &o, const vec &v, float size, float blend, int fade, int color, int axis, bool fill, int type)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_ELLIPSE) return;
    loopprimitiverenderer *p = (loopprimitiverenderer *)parts[type];
    p->addellipse(o, v, fade, color, size, blend, axis, fill);
}

void part_radius(const vec &o, const vec &v, float size, float blend, int fade, int color, bool fill)
{
    if(!canaddparticles() || (parts[PART_ELLIPSE]->type&PT_TYPE) != PT_ELLIPSE) return;
    loopprimitiverenderer *p = (loopprimitiverenderer *)parts[PART_ELLIPSE];
    p->addellipse(o, v, fade, color, size, blend, 0, fill);
    p->addellipse(o, v, fade, color, size, blend, 1, fill);
    p->addellipse(o, v, fade, color, size, blend, 2, fill);
}

void part_cone(const vec &o, const vec &dir, float radius, float angle, float size, float blend, int fade, int color, bool fill, int type)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_CONE) return;
    coneprimitiverenderer *p = (coneprimitiverenderer *)parts[type];
    p->addcone(o, dir, radius, angle, fade, color, size, blend, fill);
}

//dir = 0..6 where 0=up
static inline vec offsetvec(vec o, int dir, int dist)
{
    vec v = vec(o);
    v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

/* Experiments in shapes...
 * dir: (where dir%3 is similar to offsetvec with 0=up)
 * 0..2 circle
 * 3.. 5 cylinder shell
 * 6..11 cone shell
 * 12..14 plane volume
 * 15..20 line volume, i.e. wall
 * 21 sphere
 * 24..26 flat plane
 * +32 to inverse direction
 */
 void createshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size, float blend, int grav, int collide, float vel)
{
    if(!canemitparticles()) return;

    int basetype = parts[type]->type&PT_TYPE;
    bool flare = (basetype == PT_TAPE) || (basetype == PT_LIGHTNING),
         inv = (dir&0x20)!=0, taper = (dir&0x40)!=0 && !seedemitter;
    dir &= 0x1F;
    loopi(num)
    {
        vec to, from;
        if(dir < 12)
        {
            const vec2 &sc = sincos360[rnd(360)];
            to[dir%3] = sc.y*radius;
            to[(dir+1)%3] = sc.x*radius;
            to[(dir+2)%3] = 0.0;
            to.add(p);
            if(dir < 3) //circle
                from = p;
            else if(dir < 6) //cylinder
            {
                from = to;
                to[(dir+2)%3] += radius;
                from[(dir+2)%3] -= radius;
            }
            else //cone
            {
                from = p;
                to[(dir+2)%3] += (dir < 9)?radius:(-radius);
            }
        }
        else if(dir < 15) //plane
        {
            to[dir%3] = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to[(dir+1)%3] = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to[(dir+2)%3] = radius;
            to.add(p);
            from = to;
            from[(dir+2)%3] -= 2*radius;
        }
        else if(dir < 21) //line
        {
            if(dir < 18)
            {
                to[dir%3] = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
                to[(dir+1)%3] = 0.0;
            }
            else
            {
                to[dir%3] = 0.0;
                to[(dir+1)%3] = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            }
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
            to[(dir+2)%3] += radius;
        }
        else if(dir < 24) //sphere
        {
            to = vec(2*M_PI*float(rnd(1000))/1000.0, M_PI*float(rnd(1000)-500)/1000.0).mul(radius);
            to.add(p);
            from = p;
        }
        else if(dir < 27) // flat plane
        {
            to[dir%3] = float(rndscale(2*radius)-radius);
            to[(dir+1)%3] = float(rndscale(2*radius)-radius);
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
        }
        else from = to = p;

        if(inv) swap(from, to);

        if(taper)
        {
            float dist = clamp(from.dist2(camera1->o)/maxparticledistance, 0.0f, 1.0f);
            if(dist > 0.2f)
            {
                dist = 1 - (dist - 0.2f)/0.8f;
                if(rnd(0x10000) > dist*dist*0xFFFF) continue;
            }
        }

        if(flare)
            newparticle(from, to, rnd(fade*3)+1, type, color, size, blend, grav, collide);
        else
        {
            vec d = vec(to).sub(from).rescale(vel);
            particle *np = newparticle(from, d, rnd(fade*3)+1, type, color, size, blend, grav, collide);
            if(np->collide)
                np->val = from.z - raycube(from, vec(0, 0, -1), np->collide >= 0 ? COLLIDERADIUS : max(from.z, 0.0f), RAY_CLIPMAT) + (np->collide >= 0 ? COLLIDEERROR : 0);
        }
    }
}

void regularshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size, float blend, int grav, int collide, float vel)
{
    if(!canemitparticles()) return;
    createshape(type, radius, color, dir, num, fade, p, size, blend, grav, collide, vel);
}

void regularflame(int type, const vec &p, float radius, float height, int color, int density, int fade, float size, float blend, int grav, int collide, float vel)
{
    if(!canemitparticles()) return;

    float s = size*min(radius, height);
    vec v(0, 0, min(1.0f, height)*vel);
    float collidez = collide ? p.z - raycube(p, vec(0, 0, -1), collide >= 0 ? COLLIDERADIUS : max(p.z, 0.0f), RAY_CLIPMAT) + (collide >= 0 ? COLLIDEERROR : 0) : -1;
    loopi(density)
    {
        vec q = vec(p).add(vec(rndscale(radius*2.f)-radius, rndscale(radius*2.f)-radius, 0));
        newparticle(q, v, rnd(max(int(fade*height), 1))+1, type, color, s, blend, grav, collide)->val = collidez;
    }
}

static int partcolour(int c, int p, int x)
{
    if(p || x)
    {
        vec r(1, 1, 1);
        if(c > 0) r = vec::fromcolor(c);
        r.mul(game::getpalette(p, x));
        return (int(r.x*255)<<16)|(int(r.y*255)<<8)|(int(r.z*255));
    }
    return c;
}

void makeparticle(const vec &o, attrvector &attr)
{
    bool oldemit = canemit;
    if(attr[11]) canemit = true;
    switch(attr[0])
    {
        case 0: //fire
        {
            float radius = attr[1] ? float(attr[1])/100.0f : 1.5f,
                  height = attr[2] ? float(attr[2])/100.0f : radius*3,
                  size = attr[7] ? float(attr[7])/100.f : 2.f,
                  blend = attr[8] ? float(attr[8])/100.f : 1.f,
                  vel = attr[10] ? float(attr[10]) : 30.f;
            int fade = attr[4] > 0 ? attr[4] : 1000, grav = attr[9] ? attr[9] : -10;
            regularflame(PART_FLAME, o, radius, height, partcolour(attr[3] ? attr[3] : 0xF05010, attr[5], attr[6]), 3, fade/2, size, blend, grav/2, 0, vel);
            regularflame(PART_SMOKE, vec(o.x, o.y, o.z + 2.f*min(radius, height)), radius, height, 0x101008, 1, fade, size, blend, grav, 0, vel);
            break;
        }
        case 1: //smoke vent - <dir>
            regularsplash(PART_SMOKE, 0x897661, 2, 1, 200,  offsetvec(o, attr[1], rnd(10)), 2.4f, 1, -20);
            break;
        case 2: //water fountain - <dir>
        {
            int mat = MAT_WATER + clamp(-attr[2], 0, 3);
            const bvec &wfcol = getwaterfallcolour(mat);
            int color = (int(wfcol[0])<<16) | (int(wfcol[1])<<8) | int(wfcol[2]);
            if(!color)
            {
                const bvec &wcol = getwatercolour(mat);
                color = (int(wcol[0])<<16) | (int(wcol[1])<<8) | int(wcol[2]);
            }
            regularsplash(PART_SPARK, color, 10, 4, 200, offsetvec(o, attr[1], rnd(10)), 0.6f, 1, 20);
            break;
        }
        case 3: //fire ball - <size> <rgb> <type> <blend>
        {
            int types[3] = { PART_EXPLOSION, PART_SHOCKWAVE, PART_SHOCKBALL },
                type = types[attr[3] >= 0 && attr[3] <= 2 ? attr[3] : 0];
            float blend = attr[4] > 0 && attr[4] < 100 ? attr[4]/100.f : 1.f;
            newparticle(o, vec(0, 0, 1), 1, type, partcolour(attr[2], attr[3], attr[4]), 4.f, blend)->val = 1+attr[1];
            break;
        }
        case 4:  //tape - <dir> <length> <rgb>
        case 7:  //lightning
        case 8:  //fire
        case 9:  //smoke
        case 10: //water
        case 11: //plasma
        case 12: //snow
        case 13: //sparks
        {
            const int typemap[] = { PART_FLARE, -1, -1, PART_LIGHTNING, PART_FIREBALL, PART_SMOKE, PART_ELECTRIC, PART_PLASMA, PART_SNOW, PART_SPARK },
                      gravmap[] = { 0, 0, 0, 0, -5, -10, -10, 0, 10, 20 };
            const float sizemap[] = { 0.28f, 0.0f, 0.0f, 0.25f, 4.f, 2.f, 0.6f, 4.f, 0.5f, 0.2f }, velmap[] = { 0, 0, 0, 0, 30, 30, 50, 20, 10, 20 };
            int type = typemap[attr[0]-4], fade = attr[4] > 0 ? attr[4] : 250,
                grav = attr[0] > 7 && attr[7] != 0 ? attr[7] : gravmap[attr[0]-4],
                stain = attr[0] > 7 && attr[6] > 0 && attr[6] <= STAIN_MAX ? attr[6]-1 : -1,
                colour = attr[0] > 7 ? partcolour(attr[3], attr[9], attr[10]) : partcolour(attr[3], attr[6], attr[7]);
            float size = attr[5] != 0 ? attr[5]/100.f : sizemap[attr[0]-4],
                  vel = attr[0] > 7 && attr[8] != 0 ? attr[8] : velmap[attr[0]-4];
            if(attr[1] >= 256) regularshape(type, max(1+attr[2], 1), colour, attr[1]-256, 5, fade, o, size, 1, grav, stain, vel);
            else newparticle(o, offsetvec(o, attr[1], max(1+attr[2], 0)), fade, type, colour, size, 1, grav, stain);
            break;
        }
        case 14: // flames <radius> <height> <rgb>
        case 15: // smoke plume
        {
            const int typemap[] = { PART_FLAME, PART_SMOKE }, fademap[] = { 500, 1000 }, densitymap[] = { 3, 1 }, gravmap[] = { -5, -10 };
            const float sizemap[] = { 2, 2 }, velmap[] = { 25, 50 };
            int type = typemap[attr[0]-14], density = densitymap[attr[0]-14];
            regularflame(type, o, float(attr[1])/100.0f, float(attr[2])/100.0f, attr[3], density, attr[4] > 0 ? attr[4] : fademap[attr[0]-14], attr[5] != 0 ? attr[5]/100.f : sizemap[attr[0]-14], 1, attr[6] != 0 ? attr[6] : gravmap[attr[0]-14], 0, attr[7] != 0 ? attr[7] : velmap[attr[0]-14]);
            break;
        }
        case 6: //meter, metervs - <percent> <rgb> <rgb2>
        {
            float length = clamp(attr[1], 0, 100)/100.f;
            part_icon(o, textureload(hud::progresstex, 3), 2, 1, 0, 0, 1, partcolour(attr[3], attr[6], attr[7]), length, 1-length); // fall through
        }
        case 5:
        {
            float length = clamp(attr[1], 0, 100)/100.f;
            int colour = partcolour(attr[2], attr[4], attr[5]);
            part_icon(o, textureload(hud::progringtex, 3), 3, 1, 0, 0, 1, colour, (totalmillis%1000)/1000.f, 0.1f);
            part_icon(o, textureload(hud::progresstex, 3), 3, 1, 0, 0, 1, colour, 0, length);
            break;
        }
        case 32: //lens flares - plain/sparkle/sun/sparklesun <red> <green> <blue>
        case 33:
        case 34:
        case 35:
            flares.addflare(o, attr[1], attr[2], attr[3], (attr[0]&2)!=0, (attr[0]&1)!=0 ? ((attr[0]&2)!=0 ? 1 : 2) : 0);
            break;
        default:
            defformatstring(ds, "%d?", attr[0]);
            part_textcopy(o, ds);
            break;
    }
    canemit = oldemit;
}

static inline void makeparticles(extentity &e)
{
    makeparticle(e.o, e.attrs);
}

void seedparticles()
{
    progress(0, "seeding particles");
    addparticleemitters();
    canemit = true;
    loopv(emitters)
    {
        particleemitter &pe = emitters[i];
        extentity &e = *pe.ent;
        seedemitter = &pe;
        for(int millis = 0; millis < seedmillis; millis += min(emitmillis, seedmillis/10))
            if(entities::checkparticle(e))
                makeparticles(e);
        seedemitter = NULL;
        pe.lastemit = -seedmillis;
        pe.finalize();
    }
}

void updateparticles()
{
    if(regenemitters) addparticleemitters();

    if(minimized) { canemit = false; return; }

    if(lastmillis - lastemitframe >= emitmillis)
    {
        canemit = true;
        lastemitframe = lastmillis - (lastmillis%emitmillis);
    }
    else canemit = false;

    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->update();

    entities::drawparticles();
    flares.drawflares(); // do after drawparticles so that we can make flares for them too

    if(!editmode || showparticles)
    {
        int emitted = 0, replayed = 0;
        addedparticles = 0;
        loopv(emitters)
        {
            particleemitter &pe = emitters[i];
            extentity &e = *pe.ent;
            if(!entities::checkparticle(e) || e.o.dist(camera1->o) > maxparticledistance) { pe.lastemit = lastmillis; continue; }

            if(cullparticles && pe.maxfade >= 0)
            {
                if(isfoggedsphere(pe.radius, pe.center)) { pe.lastcull = lastmillis; continue; }
                if(pvsoccluded(pe.cullmin, pe.cullmax)) { pe.lastcull = lastmillis; continue; }
            }
            makeparticles(e);
            emitted++;
            if(replayparticles && pe.maxfade > 5 && pe.lastcull > pe.lastemit)
            {
                for(emitoffset = max(pe.lastemit + emitmillis - lastmillis, -pe.maxfade); emitoffset < 0; emitoffset += emitmillis)
                {
                    makeparticles(e);
                    replayed++;
                }
                emitoffset = 0;
            }
            pe.lastemit = lastmillis;
        }
        if(dbgpcull && (canemit || replayed) && addedparticles) conoutf("\fr%d emitters, %d particles", emitted, addedparticles);
    }
}
