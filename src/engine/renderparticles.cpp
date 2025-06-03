// renderparticles.cpp

#include "engine.h"

Shader *particleshader = NULL, *particlenotextureshader = NULL, *particletextshader = NULL, *particleenvshader = NULL, *particlesoftshader = NULL,
       *particlehazeshader = NULL, *particlehazemixshader = NULL;

VAR(IDF_PERSIST, particlelayers, 0, 1, 1);
VAR(IDF_PERSIST, particletext, 0, 1, 1);
VAR(IDF_PERSIST, particleicon, 0, 1, 1);
FVAR(IDF_PERSIST, particlebright, 0, 2, 100);
VAR(IDF_PERSIST, particlesize, 20, 100, 500);
VAR(IDF_PERSIST, particlewind, 0, 1, 1);

VARF(IDF_PERSIST, maxparticles, 10, 5000, 10000, initparticles());
VARF(IDF_PERSIST, fewparticles, 10, 100, 10000, initparticles());
VAR(IDF_PERSIST, maxparticledistance, 256, 1536, 4096);
VAR(IDF_PERSIST, maxparticletrail, 256, 512, VAR_MAX);
VAR(IDF_PERSIST, maxparticletextdistance, 0, 512, 10000);
VAR(IDF_PERSIST, maxparticleicondistance, 0, 512, 10000);

VAR(IDF_PERSIST, softparticles, 0, 1, 1);
VAR(IDF_PERSIST, softparticleblend, 1, 8, 64);

Texture *particletex = NULL;
VAR(IDF_PERSIST, particlehaze, 0, 1, 1);
FVAR(IDF_PERSIST, particlehazeblend, 0, 1, 1);
SVARF(IDF_PERSIST, particlehazetex, "textures/watern", particletex = textureload(particlehazetex, 0, true, false));
FVAR(IDF_PERSIST, particlehazedist, 0, 64, FVAR_MAX);
FVAR(IDF_PERSIST, particlehazemargin, 0, 8, FVAR_MAX);
FVAR(IDF_PERSIST, particlehazescalex, FVAR_NONZERO, 0.5f, FVAR_MAX);
FVAR(IDF_PERSIST, particlehazescaley, FVAR_NONZERO, 1, FVAR_MAX);
FVAR(IDF_PERSIST, particlehazerefract, FVAR_NONZERO, 2, 10);
FVAR(IDF_PERSIST, particlehazerefract2, FVAR_NONZERO, 4, 10);
FVAR(IDF_PERSIST, particlehazerefract3, FVAR_NONZERO, 8, 10);
FVAR(IDF_PERSIST, particlehazescrollx, FVAR_MIN, 0, FVAR_MAX);
FVAR(IDF_PERSIST, particlehazescrolly, FVAR_MIN, -0.5f, FVAR_MAX);

VARR(numenvparts, 0);

// Check canemitparticles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VAR(IDF_PERSIST, emitmillis, 1, 17, VAR_MAX);
static int lastemitframe = 0, emitoffset = 0;
static bool canemit = false, regenemitters = false, canstep = false, enviroparts = false;

static bool canemitparticles()
{
    return canemit || emitoffset;
}

VARN(0, debugparticles, dbgparts, 0, 0, 1);
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
        if(dbgpseed) conoutf(colourwhite, "radius: %f, maxfade: %d", radius, maxfade);
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
        if(e.type != ET_PARTICLES || e.flags&EF_VIRTUAL) continue;
        emitters.add(particleemitter(&e));
    }
    regenemitters = false;
}

const char *partnames[] = { "part", "tape", "trail", "text", "explosion", "lightning", "flare", "portal", "icon", "line", "triangle", "ellipse", "cone" };

struct partvert
{
    vec pos;
    bvec4 color;
    bvec hintcolor;
    vec2 tc, hintblend;
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
    virtual particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) = 0;
    virtual void update() { }
    virtual void render() = 0;
    virtual bool haswork() = 0;
    virtual int count() = 0; // for debug
    virtual void cleanup() {}

    virtual void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, float gravity)
    {
    }

    virtual void preload()
    {
        if(texname && !tex) tex = textureload(texname, texclamp);
    }

    // blend = 0 => remove it
    void calc(particle *p, int &blend, int &ts, float &size, bool step = true)
    {
        if(step)
        {
            if(p->enviro) numenvparts++;
            p->prev = p->o;
        }

        if(p->fade <= 5)
        {
            ts = 1;
            blend = 255;
            size = p->size;
        }
        else
        {
            ts = lastmillis - p->millis;
            blend = max(255 - ((ts<<8) / p->fade), step ? 0 : 1);
            size = p->size + (p->sizechange * ts);

            if(step)
            {
                float secs = curtime/1000.f;
                int part = type&0xFF;
                vec v = (part == PT_PART || part == PT_TRAIL) ? vec(p->d).mul(secs) : vec(0, 0, 0);
                bool istape = (type&PT_TAPE) != 0;

                if(p->gravity != 0)
                {
                    if(ts > p->fade) ts = p->fade;
                    static struct particleent : physent
                    {
                        particleent()
                        {
                            physent::reset();
                            type = ENT_DUMMY;
                        }
                    } d;
                    d.weight = p->gravity;
                    vec gravity = physics::gravityvel(&d, p->o, secs);
                    if(istape)
                    { // Cheat a bit and use the gravity magnitude to travel along the tape direction
                        float mag = gravity.magnitude();
                        gravity = vec(p->o).sub(p->d).normalize().mul(mag);
                    }
                    v.add(gravity);
                }

                p->o.add(v);
                if(istape) p->d.add(v); // Also add to the destination so the tape follows along
                if(particlewind && type&PT_WIND) p->o.add(p->wind.probe(p->prev).mul(secs * 10.0f));

                if(blend > 0 && p->inmaterial >= 0)
                {
                    int curmat = lookupmaterial(p->o);
                    if((p->inmaterial&MATF_VOLUME) != (curmat&MATF_VOLUME))
                        blend = 0;
                }

                vec move = vec(p->o).sub(p->prev);
                if(blend > 0 && p->collide)
                {
                    vec dir = move, hitpos;
                    float mag = dir.magnitude();
                    dir.mul(1/mag);
                    bool hit = false;
                    if(!p->precollide)
                    {
                        float dist = raycube(p->prev, dir);
                        if(dist <= mag)
                        {
                            hit = true;
                            hitpos = vec(dir).mul(dist).add(p->prev);
                        }
                    }
                    else
                    {
                        if(dir.z <= 0 ? p->o.z < p->val : p->o.z > p->val)
                        {
                            hit = true;
                            hitpos = vec(p->o.x, p->o.y, p->val);
                        }
                    }

                    if(hit)
                    {
                        p->o = hitpos;
                        if(p->collide > 0) addstain(p->collide - 1, hitpos, vec(dir).neg(), 2*p->size, p->color, type&PT_RND4 || type&PT_RND16 ? (p->flags>>5)&3 : 0, p->envcolor, p->envblend);
                        blend = 0;
                    }
                }

                if(blend > 0) p->m.add(move);
            }
        }

        game::particletrack(p, type, ts, step);
    }

    void debuginfo()
    {
        formatstring(info, "%d\t%s(", count(), partnames[type&0xFF]);
        if(type&PT_LERP) concatstring(info, "l,");
        if(type&PT_MOD) concatstring(info, "m,");
        if(type&PT_RND4) concatstring(info, "r4,");
        if(type&PT_RND16) concatstring(info, "r16,");
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

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f)
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
        p->color = bvec::hexcolor(color);
        p->envcolor = bvec::hexcolor(envcolor);
        p->envblend = envblend;
        p->size = size;
        p->blend = clamp(blend, 0.f, 1.f);
        p->hintcolor = bvec::hexcolor(hintcolor);
        p->hintblend = clamp(hintblend, 0.f, 1.f);
        p->gravity = gravity;
        p->collide = collide;
        p->val = val;
        p->enviro = enviroparts;
        p->inmaterial = type&PT_MATERIAL ? lookupmaterial(p->o) : -1;
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
        if(tex) settexture(tex);
        if(canstep)
        {
            for(T **prev = &list, *p = list; p; p = *prev)
            {
                if(renderpart(p) || drawtex) prev = &p->next;
                else
                { // remove
                    *prev = p->next;
                    p->next = parempty;
                    killpart(p);
                    parempty = p;
                }
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

        pushfont();
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
        if(*font) pushfont(font);
        float scale = p->size/80.0f, xoff = -text_widthf(text)*0.5f;

        matrix4x3 m(camright, vec(camup).neg(), vec(camdir).neg(), p->o);
        m.scale(scale);
        m.translate(xoff, 0, 50);

        textmatrix = &m;
        draw_text(text, 0, 0, p->color.r, p->color.g, p->color.b, int(p->blend*blend));
        textmatrix = NULL;
        if(*font) popfont();
    }
};
static textrenderer texts, textontop(PT_ONTOP);

#define TEXVERT(vx,vy) do { \
    gle::attrib(m.transform(vec2(vx, vy))); \
    gle::attribf(0.5f*(vx) + 0.5f, 0.5f*(vy) + 0.5f); \
    gle::attrib(color); \
    gle::attrib(hintcolor); \
    gle::attrib(hintblend); \
} while(0)

struct portal : listparticle<portal>
{
    GLuint envmap;
    float yaw, pitch, envmapblend, destyaw, destpitch;
};

struct portalrenderer : listrenderer<portal>
{
    portalrenderer(const char *texname, int type = 0)
        : listrenderer<portal>(texname, 3, type|PT_PORTAL|PT_LERP)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        gle::defvertex();
        gle::deftexcoord0();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::defhintcolor(3, GL_UNSIGNED_BYTE);
        gle::defhintblend(2, GL_FLOAT);
        if(type&PT_ENVMAP)
        {
            if(!particletex) particletex = textureload(particlehazetex, 0, true, false);

            glActiveTexture_(GL_TEXTURE2);
            settexture(particletex);
            glActiveTexture_(GL_TEXTURE0);
        }
        else gle::begin(GL_QUADS);
    }

    void endrender()
    {
        if(!(type&PT_ENVMAP)) gle::end();
        glEnable(GL_CULL_FACE);
    }

    void renderpart(portal *p, int blend, int ts, float size)
    {
        if(type&PT_ENVMAP)
        {
            matrix3 e;
            e.identity();
            e.rotate_around_y(-p->destpitch*RAD);
            e.rotate_around_z(-(p->destyaw+180.f)*RAD);
            LOCALPARAM(envmatrix, e);
            LOCALPARAMF(envblend, p->envmapblend);
            glActiveTexture_(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, p->envmap);
            glActiveTexture_(GL_TEXTURE0);
            gle::begin(GL_QUADS);
        }

        matrix4x3 m(vec(size, 0, 0), vec(0, size, 0), vec(0, 0, size), p->o);
        m.rotate_around_z(p->yaw*RAD);
        m.rotate_around_x((p->pitch-90.f)*RAD);

        bvec4 color(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        bvec hintcolor(p->hintcolor);
        vec2 hintblend(p->hintblend, p->hintblend > 0 ? 1.f/p->hintblend : 0.f);
        TEXVERT( 1,  1);
        TEXVERT(-1,  1);
        TEXVERT(-1, -1);
        TEXVERT( 1, -1);
        if(type&PT_ENVMAP) gle::end();
    }

    portal *addportal(const vec &o, int fade, int color, float size, float blend, float yaw, float pitch, GLuint envmap, float envmapblend, float destyaw, float destpitch, int hintcolor = 0, float hintblend = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f)
    {
        portal *p = (portal *)listrenderer<portal>::addpart(o, vec(0, 0, 0), fade, color, size, blend, hintcolor, hintblend, 0, 0, 0, NULL, envcolor, envblend);
        p->yaw = yaw;
        p->pitch = pitch;
        p->envmap = envmap;
        p->envmapblend = clamp(envmapblend, 0.f, 1.f);
        p->destyaw = destyaw;
        p->destpitch = destpitch;
        return p;
    }

    // use addportal() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) { return NULL; }
};

struct icon : listparticle<icon>
{
    Texture *tex;
    float start, length, end;
};

struct iconrenderer : listrenderer<icon>
{
    Texture *lasttex;
    bool inrender;

    iconrenderer(int type = 0)
        : listrenderer<icon>(type|PT_ICON|PT_LERP), inrender(false)
    {}

    void startrender()
    {
        lasttex = NULL;
        gle::defvertex();
        gle::deftexcoord0();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::defhintcolor(3, GL_UNSIGNED_BYTE);
        gle::defhintblend(2, GL_FLOAT);
    }

    void endrender()
    {
        if(inrender) gle::end();
    }

    void renderpart(icon *p, int blend, int ts, float size)
    {
        if(p->tex != lasttex)
        {
            if(inrender) gle::end();
            inrender = false;
            settexture(p->tex);
            lasttex = p->tex;
        }

        matrix4x3 m(camright, vec(camup).neg(), vec(camdir).neg(), p->o);
        float aspect = p->tex->w/float(p->tex->h);
        m.scale(size*aspect, size, 1);

        bvec4 color(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));
        bvec hintcolor(p->hintcolor);
        vec2 hintblend(p->hintblend, p->hintblend > 0 ? 1.f/p->hintblend : 0.f);
        if(p->start > 0 || p->length < 1)
        {
            float sx = cosf((p->start + 0.25f)*2*M_PI), sy = -sinf((p->start + 0.25f)*2*M_PI),
                  ex = cosf((p->end + 0.25f)*2*M_PI), ey = -sinf((p->end + 0.25f)*2*M_PI);
            if(inrender) gle::end();
            gle::begin(GL_TRIANGLE_FAN);
            TEXVERT(0, 0);

            if(p->start < 0.125f || p->start >= 0.875f) TEXVERT(sx/sy, -1);
            else if(p->start < 0.375f) TEXVERT(1, -sy/sx);
            else if(p->start < 0.625f) TEXVERT(-sx/sy, 1);
            else TEXVERT(-1, sy/sx);

            if(p->start <= 0.125f && p->end >= 0.125f) TEXVERT(1, -1);
            if(p->start <= 0.375f && p->end >= 0.375f) TEXVERT(1, 1);
            if(p->start <= 0.625f && p->end >= 0.625f) TEXVERT(-1, 1);
            if(p->start <= 0.875f && p->end >= 0.875f) TEXVERT(-1, -1);

            if(p->end < 0.125f || p->end >= 0.875f) TEXVERT(ex/ey, -1);
            else if(p->end < 0.375f) TEXVERT(1, -ey/ex);
            else if(p->end < 0.625f) TEXVERT(-ex/ey, 1);
            else TEXVERT(-1, ey/ex);
            gle::end();
            inrender = false;
        }
        else
        {
            if(!inrender) gle::begin(GL_QUADS);
            inrender = true;
            TEXVERT( 1,  1);
            TEXVERT(-1,  1);
            TEXVERT(-1, -1);
            TEXVERT( 1, -1);
        }
    }

    icon *addicon(const vec &o, Texture *tex, int fade, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float start, float length, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f)
    {
        icon *p = (icon *)listrenderer<icon>::addpart(o, vec(0, 0, 0), fade, color, size, blend, hintcolor, hintblend, gravity, collide, 0, NULL, envcolor, envblend);
        p->tex = tex;
        p->start = start;
        p->length = length;
        p->end = p->start + p->length;
        p->owner = pl;
        return p;
    }

    // use addicon() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) { return NULL; }
};
static iconrenderer icons, iconsontop(PT_ONTOP);

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
static inline void genpos(const vec &o, const vec &d, float size, float gravity, int ts, partvert *vs, particle *p)
{
    vec udir = vec(camup).sub(camright).mul(size);
    vec vdir = vec(camup).add(camright).mul(size);
    vs[0].pos = vec(o.x + udir.x, o.y + udir.y, o.z + udir.z);
    vs[1].pos = vec(o.x + vdir.x, o.y + vdir.y, o.z + vdir.z);
    vs[2].pos = vec(o.x - udir.x, o.y - udir.y, o.z - udir.z);
    vs[3].pos = vec(o.x - vdir.x, o.y - vdir.y, o.z - vdir.z);
}

template<>
inline void genpos<PT_TAPE>(const vec &o, const vec &d, float size, float gravity, int ts, partvert *vs, particle *p)
{
    vec dir1 = vec(d).sub(o), dir2 = vec(d).sub(camera1->o), c;
    c.cross(dir2, dir1).normalize().mul(size);
    vs[0].pos = vec(d.x-c.x, d.y-c.y, d.z-c.z);
    vs[1].pos = vec(o.x-c.x, o.y-c.y, o.z-c.z);
    vs[2].pos = vec(o.x+c.x, o.y+c.y, o.z+c.z);
    vs[3].pos = vec(d.x+c.x, d.y+c.y, d.z+c.z);
}

template<>
inline void genpos<PT_TRAIL>(const vec &o, const vec &d, float size, float gravity, int ts, partvert *vs, particle *p)
{
    vec dir = vec(p->o).sub(p->prev).normalize();
    vec e = vec(o).sub(vec(dir).mul(8));
    genpos<PT_TAPE>(o, e, size, gravity, ts, vs, p);
}

template<int T>
static inline void genrotpos(const vec &o, const vec &d, float size, float gravity, int ts, partvert *vs, int rot, particle *p)
{
    genpos<T>(o, d, size, gravity, ts, vs, p);
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
inline void genrotpos<PT_PART>(const vec &o, const vec &d, float size, float gravity, int ts, partvert *vs, int rot, particle *p)
{
    const vec2 *coeffs = rotcoeffs[rot];
    vs[0].pos = vec(o).madd(camright, coeffs[0].x*size).madd(camup, coeffs[0].y*size);
    vs[1].pos = vec(o).madd(camright, coeffs[1].x*size).madd(camup, coeffs[1].y*size);
    vs[2].pos = vec(o).madd(camright, coeffs[2].x*size).madd(camup, coeffs[2].y*size);
    vs[3].pos = vec(o).madd(camright, coeffs[3].x*size).madd(camup, coeffs[3].y*size);
}

template<int T>
static inline void seedpos(particleemitter &pe, const vec &o, const vec &d, int fade, float size, float gravity)
{
    if(gravity)
    {
        float t = fade;
        vec end = vec(o).madd(d, t/5000.0f);
        end.z -= t*t/(2.0f * 5000.0f * gravity);
        pe.extendbb(end, size);

        float tpeak = d.z*gravity;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }
}

template<>
inline void seedpos<PT_TAPE>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, float gravity)
{
    pe.extendbb(d, size);
}

template<>
inline void seedpos<PT_TRAIL>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, float gravity)
{
    vec e = d;
    if(gravity) e.z -= float(fade)/gravity;
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
        if(type & PT_RND4 || type & PT_RND16) rndmask |= 0x03<<5;
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

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f)
    {
        particle *p = parts + (numparts < maxparts ? numparts++ : rnd(maxparts)); // next free slot, or kill a random kitten
        p->o = o;
        p->d = d;
        p->m = vec(0, 0, 0);
        p->prev = o;
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec::hexcolor(color);
        p->envcolor = bvec::hexcolor(envcolor);
        p->envblend = envblend;
        p->hintcolor = bvec::hexcolor(hintcolor);
        p->hintblend = clamp(hintblend, 0.f, 1.f);
        p->blend = clamp(blend, 0.f, 1.f);
        p->size = size;
        p->gravity = gravity;
        p->collide = collide;
        p->val = val;
        p->owner = pl;
        p->flags = 0x80 | (rndmask ? rnd(0x80) & rndmask : 0);
        p->enviro = enviroparts;
        p->inmaterial = type&PT_MATERIAL ? lookupmaterial(p->o) : -1;
        p->wind.reset();
        lastupdate = -1;
        return p;
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, float gravity)
    {
        pe.maxfade = max(pe.maxfade, fade);
        size *= SQRT2;
        pe.extendbb(o, size);

        seedpos<T>(pe, o, d, fade, size, gravity);
        #if 0
        vec end(o);
        float secs = fade/5000.0f;
        vec v = vec(d).mul(secs);
        if(gravity)
        {
            static struct particleent : physent
            {
                particleent()
                {
                    physent::reset();
                    type = ENT_DUMMY;
                }
            } p;
            p.weight = gravity;
            v.add(v).add(physics::gravityvel(&d, p->o, secs));
        }
        end.add(v);
        pe.extendbb(end, size);
        if(!gravity) return;
        #endif
        float t = fade;
        vec end = vec(o).madd(d, t/5000.0f);
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
        if(blend <= 1 || p->fade <= 5) p->fade = -1; // mark to remove on next pass (i.e. after render)

        modifyblend<T>(p->o, blend);

        #define SETTEXCOORDS(u1c, u2c, v1c, v2c, body) \
        { \
            float u1 = u1c, u2 = u2c, v1 = v1c, v2 = v2c; \
            body; \
            vs[0].tc = vec2(u1, v1); \
            vs[1].tc = vec2(u2, v1); \
            vs[2].tc = vec2(u2, v2); \
            vs[3].tc = vec2(u1, v2); \
        }

        #define SETCOLS(r, g, b, a) \
            do { \
                bvec4 col(r, g, b, a); \
                loopi(4) vs[i].color = col; \
            } while(0)

        #define SETHINT(r, g, b, s) \
            do { \
                bvec col(r, g, b); \
                loopi(4) vs[i].hintcolor = col; \
                loopi(4) vs[i].hintblend = vec2(s, s > 0 ? 1.f / s : 0.f); \
            } while(0)

        #define SETCOLOR \
        { \
            SETCOLS(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend)); \
            SETHINT(p->hintcolor.r, p->hintcolor.g, p->hintcolor.b, p->hintblend); \
        }

        #define SETMODCOLOR \
        { \
            SETCOLS((p->color.r*blend)>>8, (p->color.g*blend)>>8, (p->color.b*blend)>>8, uchar(p->blend*255)); \
            SETHINT((p->hintcolor.r*blend)>>8, (p->hintcolor.g*blend)>>8, (p->hintcolor.b*blend)>>8, p->hintblend); \
        }

        if(regen)
        {
            p->flags &= ~0x80;

            if(type&PT_RND4)
            {
                float tx = 0.5f*((p->flags>>5)&1), ty = 0.5f*((p->flags>>6)&1);
                SETTEXCOORDS(tx, tx + 0.5f, ty, ty + 0.5f,
                {
                    if(p->flags&0x01) swap(u1, u2);
                    if(p->flags&0x02) swap(v1, v2);
                });

            }
            else if(type&PT_RND16)
            {
                float tx = 0.25f*((p->flags>>5)&3), ty = 0.25f*((p->flags>>6)&3);
                SETTEXCOORDS(tx, tx + 0.25f, ty, ty + 0.25f,
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

            if(type&PT_MOD) SETMODCOLOR
            else SETCOLOR
        }
        else if(type&PT_MOD) SETMODCOLOR
        else loopi(4) vs[i].color.a = uchar(p->blend*blend);

        if(type&PT_ROT) genrotpos<T>(p->o, p->d, size, ts, p->gravity, vs, (p->flags>>2)&0x1F, p);
        else genpos<T>(p->o, p->d, size, p->gravity, ts, vs, p);
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

        settexture(tex);

        gle::bindvbo(vbo);
        const partvert *ptr = 0;
        gle::vertexpointer(sizeof(partvert), ptr->pos.v);
        gle::texcoord0pointer(sizeof(partvert), ptr->tc.v);
        gle::colorpointer(sizeof(partvert), ptr->color.v);
        gle::hintcolorpointer(sizeof(partvert), ptr->hintcolor.v);
        gle::hintblendpointer(sizeof(partvert), ptr->hintblend.v);
        gle::enablevertex();
        gle::enabletexcoord0();
        gle::enablecolor();
        gle::enablehintcolor();
        gle::enablehintblend();
        gle::enablequads();

        gle::drawquads(0, numparts);

        gle::disablequads();
        gle::disablevertex();
        gle::disabletexcoord0();
        gle::disablecolor();
        gle::disablehintcolor();
        gle::disablehintblend();
        gle::clearvbo();
    }
};

typedef varenderer<PT_PART> quadrenderer;
typedef varenderer<PT_TAPE> taperenderer;
typedef varenderer<PT_TRAIL> trailrenderer;

#include "explosion.h"
#include "lensflare.h"
#include "lightning.h"

struct lineprimitive : listparticle<lineprimitive>
{
    vec value;
};

struct lineprimitiverenderer : listrenderer<lineprimitive>
{
    lineprimitiverenderer(int type = 0)
        : listrenderer<lineprimitive>(type|PT_LINE|PT_LERP|PT_NOTEX)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        gle::defvertex();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_LINES);
    }

    void endrender()
    {
        gle::end();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void renderpart(lineprimitive *p, int blend, int ts, float size)
    {
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
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) { return NULL; }
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
        : listrenderer<trisprimitive>(type|PT_TRIANGLE|PT_LERP|PT_NOTEX)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        gle::defvertex();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_TRIANGLES);
    }

    void endrender()
    {
        gle::end();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void renderpart(trisprimitive *p, int blend, int ts, float size)
    {
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
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) { return NULL; }
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
        : listrenderer<loopprimitive>(type|PT_ELLIPSE|PT_LERP|PT_NOTEX)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        gle::defvertex();
    }

    void endrender()
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void renderpart(loopprimitive *p, int blend, int ts, float size)
    {
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
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) { return NULL; }
};
static loopprimitiverenderer loopprimitives, loopontopprimitives(PT_ONTOP);

struct coneprimitive : listparticle<coneprimitive>
{
    vec dir, spot, spoke;
    float radius, angle;
    bool fill;
    int spokenum;
};

struct coneprimitiverenderer : listrenderer<coneprimitive>
{
    coneprimitiverenderer(int type = 0)
        : listrenderer<coneprimitive>(type|PT_CONE|PT_LERP|PT_NOTEX)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        gle::defvertex();
    }

    void endrender()
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void renderpart(coneprimitive *p, int blend, int ts, float size)
    {
        gle::colorub(p->color.r, p->color.g, p->color.b, uchar(p->blend*blend));

        gle::begin(GL_LINES);
        loopi(p->spokenum)
        {
            gle::attrib(p->o);
            gle::attrib(vec(p->spoke).rotate(sincos360[i*(360/p->spokenum)], p->dir).add(p->spot).mul(size).add(p->o));
        }
        gle::end();

        gle::begin(GL_LINE_LOOP);
        loopi(p->spokenum)
        {
            gle::attrib(vec(p->spoke).rotate(sincos360[i*(360/p->spokenum)], p->dir).add(p->spot).mul(size).add(p->o));
        }
        gle::end();
    }

    coneprimitive *addcone(const vec &o, const vec &dir, float radius, float angle, int fade, int color, float size, float blend, bool fill, int spokenum = 15)
    {
        coneprimitive *p = (coneprimitive *)listrenderer<coneprimitive>::addpart(o, vec(0, 0, 0), fade, color, size, blend);
        p->dir = dir;
        p->radius = radius;
        p->angle = angle;
        p->fill = fill;
        p->spot = vec(p->dir).mul(p->radius*cosf(p->angle*RAD));
        p->spoke.orthogonal(p->dir);
        p->spoke.normalize().mul(p->radius*sinf(p->angle*RAD));
        p->spokenum = clamp(spokenum, 3, 359);
        return p;
    }

    // use addcone() instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f) { return NULL; }
};
static coneprimitiverenderer coneprimitives, coneontopprimitives(PT_ONTOP);

static partrenderer *parts[] =
{
    new portalrenderer("<comp:1,1,2>portal"),
    new portalrenderer("<comp:1,1,2>portal", PT_ENVMAP),
    &icons, &iconsontop, &lineprimitives, &lineontopprimitives, &trisprimitives, &trisontopprimitives,
    &loopprimitives, &loopontopprimitives, &coneprimitives, &coneontopprimitives,
    new quadrenderer("<comp:1,1,2>fire", PT_SOFT|PT_PART|PT_BRIGHT|PT_HFLIP|PT_RND4|PT_WIND),
    new quadrenderer("<grey>particles/plasma", PT_SOFT|PT_PART|PT_BRIGHT|PT_FLIP|PT_WIND),
    new taperenderer("<grey>particles/sflare", PT_TAPE|PT_BRIGHT|PT_FEW),
    new taperenderer("<grey>particles/mflare", PT_TAPE|PT_BRIGHT|PT_RND4|PT_VFLIP|PT_FEW),
    new quadrenderer("<comp:1,1,2>smoke", PT_SOFT|PT_PART|PT_LERP|PT_FLIP|PT_RND4|PT_WIND),
    new quadrenderer("<comp:1,1,2>smoke", PT_PART|PT_LERP|PT_FLIP|PT_RND4|PT_WIND),
    new quadrenderer("<comp:1,1,2>smoke", PT_SOFT|PT_PART|PT_FLIP|PT_RND4|PT_WIND),
    new quadrenderer("<comp:1,1,2>smoke", PT_PART|PT_FLIP|PT_RND4|PT_WIND),
    new quadrenderer("<comp:0,0.25,2>hint", PT_SOFT|PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hint", PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hintbold", PT_SOFT|PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hintbold", PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hintvert", PT_SOFT|PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hintvert", PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hinthorz", PT_SOFT|PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hinthorz", PT_PART|PT_BRIGHT),
    new quadrenderer("<grey>particles/blood", PT_PART|PT_MOD|PT_RND4|PT_FLIP),
    new quadrenderer("<comp:0,0.25,2>hintent", PT_PART|PT_BRIGHT),
    new quadrenderer("<comp:0,0.25,2>hintent", PT_PART|PT_BRIGHT|PT_ONTOP),
    new quadrenderer("<grey>particles/spark", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP),
    new quadrenderer("<comp:1,1,2>fire", PT_SOFT|PT_PART|PT_BRIGHT|PT_HFLIP|PT_RND4|PT_WIND),
    new quadrenderer("<comp:1,1,2>fire", PT_PART|PT_BRIGHT|PT_HFLIP|PT_RND4|PT_WIND),
    new quadrenderer("<grey>particles/plasma", PT_SOFT|PT_PART|PT_BRIGHT|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/plasma", PT_PART|PT_BRIGHT|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/electric", PT_SOFT|PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/electric", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/eleczap", PT_SOFT|PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/eleczap", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<comp:1,1,2>fire", PT_PART|PT_BRIGHT|PT_HFLIP|PT_RND4|PT_BRIGHT|PT_WIND),
    new taperenderer("<grey>particles/sflare", PT_TAPE|PT_BRIGHT),
    new taperenderer("<grey>particles/cleanflare", PT_TAPE|PT_BRIGHT),
    new taperenderer("<comp:1,1,2>distortflare", PT_TAPE|PT_BRIGHT),
    new taperenderer("<grey>particles/mflare", PT_TAPE|PT_BRIGHT|PT_RND4|PT_VFLIP|PT_BRIGHT),
    new taperenderer("<grey>particles/lightning", PT_TAPE|PT_BRIGHT|PT_HFLIP|PT_VFLIP, 2), // uses same clamp setting as normal lightning to avoid conflict
    new taperenderer("<grey>particles/lightzap", PT_TAPE|PT_BRIGHT|PT_HFLIP|PT_VFLIP, 2),
    new quadrenderer("<grey>particles/muzzle", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP),
    new quadrenderer("<grey>particles/snow", PT_PART|PT_BRIGHT|PT_FLIP|PT_WIND),
    new quadrenderer("<comp:0,0.25,2>hint", PT_HAZE|PT_PART),
    new quadrenderer("<comp:1,1,2>fire", PT_HAZE|PT_PART|PT_HFLIP|PT_RND4|PT_WIND),
    new taperenderer("<grey>particles/sflare", PT_HAZE|PT_TAPE),
    new trailrenderer("<comp:0,0.25,2>hint", PT_TRAIL|PT_LERP|PT_WIND),
    new quadrenderer("<comp:1,1,2>bubble", PT_SOFT|PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<comp:1,1,2>bubble", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/splash", PT_SOFT|PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<grey>particles/splash", PT_PART|PT_BRIGHT|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<comp:1,1,2>bubble", PT_HAZE|PT_PART|PT_RND4|PT_FLIP|PT_WIND),
    new quadrenderer("<comp:1,1,2>bubble", PT_SOFT|PT_PART|PT_RND4|PT_FLIP|PT_MATERIAL),
    &texts, &textontop,
    &explosions, &shockwaves, &shockballs, &glimmerballs, &lightnings, &lightzaps,
    &flares // must be done last!
};

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
    if(!particletextshader) particletextshader = lookupshaderbyname("particletext");
    if(!particleenvshader) particleenvshader = lookupshaderbyname("particleenv");
    if(!particlesoftshader) particlesoftshader = lookupshaderbyname("particlesoft");
    if(!particlehazeshader) particlehazeshader = lookupshaderbyname("particlehaze");
    if(!particlehazemixshader) particlehazemixshader = lookupshaderbyname("particlehazemix");
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

bool hazeparticles = false;

void renderparticles(int layer)
{
    timer *parttimer = drawtex ? NULL : begintimer("Particles", false);

    canstep = !drawtex && layer != PL_UNDER;

    // want to debug BEFORE the lastpass render (that would delete particles)
    if(dbgparts && (layer == PL_ALL || layer == PL_UNDER)) loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->debuginfo();

    bool rendered = false;
    uint lastflags = PT_LERP|PT_SHADER,
         flagmask = PT_LERP|PT_MOD|PT_ONTOP|PT_BRIGHT|PT_NOTEX|PT_SOFT|PT_SHADER|PT_ENVMAP,
         excludemask = layer == PL_ALL ? ~0 : (layer != PL_NOLAYER ? PT_NOLAYER : 0);

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        partrenderer *p = parts[partsorder[i]];
        if((p->type&PT_NOLAYER) == excludemask || !p->haswork()) continue;
        if(p->type&PT_HAZE)
        {
            if(particlehaze) hazeparticles = true;
            else p->reset();
            continue;
        }

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
                if(changedbits&(PT_LERP|PT_SOFT|PT_NOTEX|PT_SHADER|PT_ENVMAP))
                {
                    if(flags&PT_ENVMAP) particleenvshader->set();
                    else if(flags&PT_SOFT && softparticles)
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
    canstep = false;

    if(!drawtex) endtimer(parttimer);
}

void renderhazeparticles(GLuint hazertex, bool hazemix)
{
    if(!particlehaze)
    {
        hazeparticles = false;
        return;
    }
    timer *parttimer = drawtex ? NULL : begintimer("Particles", false);

    if(!particletex) particletex = textureload(particlehazetex, 0, true, false);
    canstep = !drawtex;

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture_(GL_TEXTURE0 + TEX_EARLY_DEPTH);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msearlydepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, earlydepthtex);

    glActiveTexture_(GL_TEXTURE4);
    settexture(particletex);

    glActiveTexture_(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_RECTANGLE, hazertex);
    glActiveTexture_(GL_TEXTURE0);

    float scroll = lastmillis/1000.0f;
    GLOBALPARAMF(hazerefract, particlehazerefract, particlehazerefract2, particlehazerefract3);
    GLOBALPARAMF(hazetexgen, particlehazescalex, particlehazescaley, particlehazescrollx*scroll, particlehazescrolly*scroll);
    GLOBALPARAMF(hazeparams, 1.0f/particlehazedist, 1.0f/particlehazemargin);

    if(hazemix)
    {
        const bvec &color = gethazecolour();
        float colormix = gethazecolourmix(), blend = gethazeblend();
        GLOBALPARAMF(worldhazecolor, color.x*ldrscaleb, color.y*ldrscaleb, color.z*ldrscaleb, colormix*blend);
        float margin = gethazemargin(), mindist = gethazemindist(), maxdist = max(max(mindist, gethazemaxdist())-mindist, margin);
        GLOBALPARAMF(worldhazeparams, mindist, 1.0f/maxdist, 1.0f/margin);
        particlehazemixshader->set();
    }
    else particlehazeshader->set();
    LOCALPARAMF(colorscale, 1, 1, 1, particlehazeblend);

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        partrenderer *p = parts[partsorder[i]];
        if(!(p->type&PT_HAZE) || !p->haswork()) continue;
        p->render();
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    canstep = hazeparticles = false;

    if(!drawtex) endtimer(parttimer);
}

static int addedparticles = 0;

float partcollide(const vec &p, int collide)
{
    return collide ? p.z - raycube(p, vec(0, 0, -1), collide > 0 ? COLLIDERADIUS : max(p.z, 0.0f)) + (collide > 0 ? COLLIDEERROR : 0) : 0;
}

particle *newparticle(const vec &o, const vec &d, int fade, int type, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float val, physent *pl, float sizechange, int envcolor, float envblend)
{
    static particle dummy;
    if(seedemitter)
    {
        parts[type]->seedemitter(*seedemitter, o, d, fade, size, gravity);
        return &dummy;
    }
    if(fade + emitoffset < 0) return &dummy;
    addedparticles++;

    particle *newpart = parts[type]->addpart(o, d, fade, color, size, blend, hintcolor, hintblend, gravity, collide, val, pl, envcolor, envblend);
    newpart->sizechange = sizechange;

    return newpart;
}

void create(int type, int color, int fade, const vec &p, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl, float sizechange, int envcolor, float envblend)
{
    if(camera1->o.dist(p) > maxparticledistance) return;
    newparticle(p, vec(0, 0, 0), fade, type, color, size, blend, hintcolor, hintblend, gravity, collide, partcollide(p, collide), pl, sizechange, envcolor, envblend);
}

void regularcreate(int type, int color, int fade, const vec &p, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl, int delay, float sizechange, int envcolor, float envblend)
{
    if(!canemitparticles() || (delay > 0 && rnd(delay) != 0)) return;
    create(type, color, fade, p, size, blend, hintcolor, hintblend, gravity, collide, pl, sizechange, envcolor, envblend);
}

void splash(int type, int color, float radius, int num, int fade, const vec &p, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float vel, float sizechange, int envcolor, float envblend)
{
    if(camera1->o.dist(p) > maxparticledistance && !seedemitter) return;
#if 0
    int fmin = 1;
    int fmax = fade*3;
    loopi(num)
    {
        vec tmp(rnd(max(int(ceilf(radius*2)),1))-radius, rnd(max(int(ceilf(radius*2)),1))-radius, rnd(max(int(ceilf(radius*2)),1))-radius);
        int f = (num < 10) ? (fmin + rnd(fmax)) : (fmax - (i*(fmax-fmin))/(num-1)); // help deallocater by using fade distribution rather than random
        newparticle(p, tmp, f, type, color, size, blend, hintcolor, hintblend, gravity, collide, partcollide(p, collide), NULL, sizechange, envcolor, envblend);
    }
#endif
    regularshape(type, radius, color, 21, num, fade, p, size, blend, hintcolor, hintblend, gravity, collide, vel, sizechange, envcolor, envblend);
}

void regularsplash(int type, int color, float radius, int num, int fade, const vec &p, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float vel, int delay, float sizechange, int envcolor, float envblend)
{
    if(!canemitparticles() || (delay > 0 && rnd(delay) != 0)) return;
    splash(type, color, radius, num, fade, p, size, blend, hintcolor, hintblend, gravity, collide, vel, sizechange, envcolor, envblend);
}

bool canaddparticles()
{
    return !minimized || renderunfocused;
}

void regular_part_create(int type, int fade, const vec &p, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl, int delay, float sizechange, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    regularcreate(type, color, fade, p, size, blend, hintcolor, hintblend, gravity, collide, pl, delay, sizechange, envcolor, envblend);
}

void part_create(int type, int fade, const vec &p, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl, float sizechange, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    create(type, color, fade, p, size, blend, hintcolor, hintblend, gravity, collide, pl, sizechange, envcolor, envblend);
}

void regular_part_splash(int type, int num, int fade, const vec &p, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float radius, float vel, int delay, float sizechange, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    regularsplash(type, color, radius, num, fade, p, size, blend, hintcolor, hintblend, gravity, collide, vel, delay, sizechange, envcolor, envblend);
}

void part_splash(int type, int num, int fade, const vec &p, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float radius, float vel, float sizechange, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    splash(type, color, radius, num, fade, p, size, blend, hintcolor, hintblend, gravity, collide, vel, sizechange, envcolor, envblend);
}

void part_trail(int type, int fade, const vec &s, const vec &e, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float sizechange, float vel, float stepscale, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*stepscale), 1, maxparticletrail);
    v.div(steps);
    vec p = s;
    loopi(steps)
    {
        p.add(v);
        vec tmp(rnd(max(int(ceilf(vel*2)),1))-vel, rnd(max(int(ceilf(vel*2)),1))-vel, rnd(max(int(ceilf(vel*2)),1))-vel);
        newparticle(p, tmp, rnd(fade)+fade, type, color, size, blend, hintcolor, hintblend, gravity, collide, 0, NULL, sizechange, envcolor, envblend);
    }
}

void part_text(const vec &s, const char *t, int type, int fade, int color, float size, float blend, float gravity, int collide, physent *pl, float sizechange, int envcolor, float envblend)
{
    if(!canaddparticles() || !t[0]) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 0), fade, type, color, size, blend, 0, 0, gravity, collide, 0, pl, sizechange, envcolor, envblend);
    p->text = t;
}

void part_textcopy(const vec &s, const char *t, int type, int fade, int color, float size, float blend, float gravity, int collide, physent *pl, int envcolor, float envblend)
{
    if(!canaddparticles() || !t[0]) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 0), fade, type, color, size, blend, 0, 0, gravity, collide, 0, pl, envcolor, envblend);
    p->text = newstring(t);
    p->flags = 1;
}

void part_flare(const vec &p, const vec &dest, int fade, int type, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl, float sizechange, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    newparticle(p, dest, fade, type, color, size, blend, hintcolor, hintblend, gravity, collide, 0, pl, sizechange, envcolor, envblend);
}

void part_explosion(const vec &dest, float maxsize, int type, int fade, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl)
{
    if(!canaddparticles()) return;
    float growth = maxsize - size;
    if(fade < 0) fade = int(growth*25);
    newparticle(dest, vec(0, 0, 0), fade, type, color, size, blend, hintcolor, hintblend, gravity, collide, growth, pl);
}

void regular_part_explosion(const vec &dest, float maxsize, int type, int fade, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide)
{
    if(!canaddparticles() || !canemitparticles()) return;
    part_explosion(dest, maxsize, type, fade, color, size, blend, hintcolor, hintblend, gravity, collide, NULL);
}

void part_spawn(const vec &o, const vec &v, float z, uchar type, int amt, int fade, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    loopi(amt)
    {
        vec w(rnd(int(v.x*2))-int(v.x), rnd(int(v.y*2))-int(v.y), rnd(int(v.z*2))-int(v.z)+z);
        w.add(o);
        part_splash(type, 1, fade, w, color, size, blend, hintcolor, hintblend, gravity, collide, 1, 1, envcolor, envblend);
    }
}

void part_flares(const vec &o, const vec &v, float z1, const vec &d, const vec &w, float z2, uchar type, int amt, int fade, int color, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, physent *pl, int envcolor, float envblend)
{
    if(!canaddparticles()) return;
    loopi(amt)
    {
        vec from(rnd(int(v.x*2))-int(v.x), rnd(int(v.y*2))-int(v.y), rnd(int(v.z*2))-int(v.z)+z1);
        from.add(o);

        vec to(rnd(int(w.x*2))-int(w.x), rnd(int(w.y*2))-int(w.y), rnd(int(w.z*2))-int(w.z)+z1);
        to.add(d);

        newparticle(from, to, fade, type, color, size, blend, hintcolor, hintblend, gravity, collide, 0, pl, envcolor, envblend);
    }
}

void part_portal(const vec &o, float size, float blend, float yaw, float pitch, int type, int fade, int color, GLuint envmap, float envblend, float destyaw, float destpitch, int hintcolor, float hintblend)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_PORTAL) return;
    portalrenderer *p = (portalrenderer *)parts[type];
    p->addportal(o, fade, color, size, blend, yaw, pitch, envmap, envblend, destyaw, destpitch, hintcolor, hintblend);
}

void part_icons(const vec &o, Texture *tex, int type, float size, float blend, float gravity, int collide, int fade, int color, int hintcolor, float hintblend, float start, float length, physent *pl, int envcolor, float envblend)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_ICON) return;
    if(!particleicon || camera1->o.dist(o) > maxparticleicondistance) return;
    iconrenderer *p = (iconrenderer *)parts[type];
    p->addicon(o, tex, fade, color, size, blend, hintcolor, hintblend, gravity, collide, start, length, pl, envcolor, envblend);
}

void part_icon(const vec &o, Texture *tex, float size, float blend, float gravity, int collide, int fade, int color, int hintcolor, float hintblend, float start, float length, physent *pl, int envcolor, float envblend)
{
    part_icons(o, tex, PART_ICON, size, blend, gravity, collide, fade, color, hintcolor, hintblend, start, length, pl, envcolor, envblend);
}

void part_icon_ontop(const vec &o, Texture *tex, float size, float blend, float gravity, int collide, int fade, int color, int hintcolor, float hintblend, float start, float length, physent *pl, int envcolor, float envblend)
{
    part_icons(o, tex, PART_ICON_ONTOP, size, blend, gravity, collide, fade, color, hintcolor, hintblend, start, length, pl, envcolor, envblend);
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

void part_cone(const vec &o, const vec &dir, float radius, float angle, float size, float blend, int fade, int color, bool fill, int spokenum, int type)
{
    if(!canaddparticles() || (parts[type]->type&PT_TYPE) != PT_CONE) return;
    coneprimitiverenderer *p = (coneprimitiverenderer *)parts[type];
    p->addcone(o, dir, radius, angle, fade, color, size, blend, fill, spokenum);
}

// dir = 0..6 where 0=up
static inline vec offsetvec(vec o, int dir, int dist)
{
    vec v = vec(o);
    v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

VAR(IDF_PERSIST, weatherdropdist, 0, 200, VAR_MAX);
VAR(IDF_PERSIST, weatherdropheight, 0, 200, VAR_MAX);
FVAR(IDF_PERSIST, weatherviewangle, 1, 160, 360);
FVAR(IDF_PERSIST, weatherdropnumscale, 0, 1.0f, FVAR_MAX);

#define MPVVARS(name) \
    FVAR(IDF_MAP, weatherdrops##name, 0, 0, FVAR_MAX); \
    VAR(IDF_MAP, weatherdroppart##name, PART_FIREBALL_LERP, PART_RAIN, PART_LAST); \
    VAR(IDF_MAP, weatherdropcollide##name, -2, -1, STAIN_MAX); \
    VAR(IDF_MAP, weatherdropfade##name, 1, 750, VAR_MAX); \
    VAR(IDF_MAP, weatherdropgravity##name, VAR_MIN, 300, VAR_MAX); \
    PCVAR(IDF_MAP, weatherdropcolour##name, 0xFFFFFF); \
    PCVAR(IDF_MAP, weatherdrophintcolour##name, 0xFFFFFF); \
    PCVAR(IDF_MAP, weatherdropenvcolour##name, 0xFFFFFF); \
    FVAR(IDF_MAP, weatherdrophintblend##name, 0, 0, 1); \
    FVAR(IDF_MAP, weatherdropblend##name, 0, 0.4f, 1); \
    FVAR(IDF_MAP, weatherdropenvblend##name, 0, 0.5f, 1); \
    FVAR(IDF_MAP, weatherdropsize##name, FVAR_NONZERO, 0.15f, FVAR_MAX); \
    FVAR(IDF_MAP, weatherdroplen##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_MAP, weatherdropvariance##name, 0, 0.05f, 1); \
    VAR(IDF_MAP, weatherdropmindist##name, 0, 0, VAR_MAX); \
    VAR(IDF_MAP, weatherdropmaxdist##name, 0, 0, VAR_MAX);
MPVVARS();
MPVVARS(alt);

#define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
        return name; \
    }

#define GETMPVDARK(name, type) \
    type get##name() \
    { \
        static int res; \
        res = int((checkmapvariant(MPV_ALTERNATE) ? name##alt : name)*game::darkness(DARK_PART)); \
        return res; \
    }

GETMPV(weatherdrops, float);
GETMPV(weatherdroppart, int);
GETMPV(weatherdropcollide, int);
GETMPV(weatherdropfade, int);
GETMPV(weatherdropgravity, int);
GETMPVDARK(weatherdropcolour, int);
GETMPVDARK(weatherdrophintcolour, int);
GETMPVDARK(weatherdropenvcolour, int);
GETMPV(weatherdrophintblend, float);
GETMPV(weatherdropblend, float);
GETMPV(weatherdropenvblend, float);
GETMPV(weatherdropsize, float);
GETMPV(weatherdroplen, float);
GETMPV(weatherdropvariance, float);

vec part_weather_pos()
{
    // Camera position
    vec o = vec(camera1->o);
    vec pos;

    float angle = rndscale(weatherviewangle) - weatherviewangle/2;

    vecfromyawpitch(camera1->yaw + angle, 0, 1, 0, pos);
    pos.mul(rndscale(weatherdropdist)).add(o);

    pos.z += (rnd(weatherdropheight * 2) - weatherdropheight);

    return pos;
}

void part_weather()
{
    if(!canaddparticles()) return;

    enviroparts = true;

    // Calculate the number of drops to spawn
    int drops = round(curtime * getweatherdrops() * weatherdropnumscale);
    if(!drops) return;

    int part = getweatherdroppart();
    if(part < PART_FIREBALL_LERP || part > PART_LAST) return;
    bool istape = (parts[part]->type&PT_TAPE) != 0;

    int collide = getweatherdropcollide(), fade = getweatherdropfade(),
        gravity = getweatherdropgravity(), color = getweatherdropcolour(),
        hintcolor = getweatherdrophintcolour(), envcolor = getweatherdropenvcolour();

    float variance = getweatherdropvariance(), size = getweatherdropsize(),
          length = istape ? getweatherdroplen() : abs(gravity), blend = getweatherdropblend(),
          hintblend = getweatherdrophintblend(), envblend = getweatherdropenvblend();;

    // Scale the number of drops if the distance changes
    if(weatherdropdist != 200) drops = round(drops * float(weatherdropdist) / 200.f);

    loopi(drops)
    {
        // Pick a random position within the cube
        vec o = part_weather_pos();

        // Add random jitter to the drop direction
        vec dir = vec(rndscale(variance * 2) - variance, rndscale(variance * 2) - variance, gravity >= 0 ? -1 : 1).normalize();

        // Follow the inverse direction to find the boundary position
        vec skypos = vec(o).sub(vec(dir).mul(worldsize));

        vec dest = vec(dir).mul(length);
        if(istape) dest = vec(o).sub(dest); // Tape particles use d = dest

        float zoff = 0;
        bool wantcollide = false;
        if(collide)
        {
            // Raycast from outside map to find obstructions, raycube is limited to world bounds
            float hitz = raycube(skypos, dir);

            // Project a bit further to see if the drop is just exiting the world bounds
            vec hitpos = vec(dir).mul(hitz).add(skypos), project = vec(hitpos).add(dir);

            // Only run the cull if the projection is inside the world
            if(insideworld(project))
            {
                // Check if we only want drops that progress through the entire world
                if(collide == -2) continue;

                zoff = hitpos.z;
                if(gravity >= 0) zoff = max(zoff, camera1->o.z - (weatherdropdist * 0.5f));
                else zoff = min(zoff, camera1->o.z + (weatherdropdist * 0.5f));

                // If collided before the destination, discard
                if(gravity >= 0 ? zoff >= o.z : zoff <= o.z) continue;
                wantcollide = true;
            }
        }

        particle *newpart = newparticle(o, dest, fade, part, getpulsehexcol(color, -1), size, blend, getpulsehexcol(hintcolor, -1), hintblend, istape ? gravity : 0, wantcollide ? collide : 0, zoff, NULL, 0, envcolor, envblend);
        if(wantcollide && newpart) newpart->precollide = true;
    }

    enviroparts = false;
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
 void createshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float vel, float sizechange, int envcolor, float envblend)
{
    int basetype = parts[type]->type&PT_TYPE;
    bool flare = (basetype == PT_TAPE) || (basetype == PT_LIGHTNING),
         inv = (dir&0x20)!=0, taper = (dir&0x40)!=0 && !seedemitter;
    dir &= 0x1F;
    loopi(num)
    {
        vec to, from = p;
        if(dir < 12)
        {
            const vec2 &sc = sincos360[rnd(360)];
            to[dir%3] = sc.y*radius;
            to[(dir+1)%3] = sc.x*radius;
            to[(dir+2)%3] = 0.0f;
            to.add(p);
            if(dir < 3) // circle
                from = p;
            else if(dir < 6) // cylinder
            {
                from = to;
                to = vec(p).sub(from).rescale(radius).add(from);
            }
            else // cone
            {
                from = p;
                to[(dir+2)%3] += (dir < 9)?radius:(-radius);
            }
        }
        else if(dir < 15) // plane
        {
            to[dir%3] = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to[(dir+1)%3] = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to[(dir+2)%3] = radius;
            to.add(p);
            from = to;
            from[(dir+2)%3] -= 2*radius;
        }
        else if(dir < 21) // line
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
        else if(dir < 24) // sphere
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
        else if(dir < 28) // field
        {
            from.x += float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            from.y += float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            from.z += float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to.x    = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to.y    = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to.z    = float(rnd(int(ceilf(radius))<<4)-(int(ceilf(radius))<<3))/8.0;
            to.add(from);
        }
        else to = p;

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
            newparticle(from, to, rnd(fade*3)+1, type, color, size, blend, hintcolor, hintblend, gravity, collide, 0, NULL, sizechange, envcolor, envblend);
        else
        {
            vec d = vec(from).sub(to).rescale(vel);
            newparticle(from, d, rnd(fade*3)+1, type, color, size, blend, hintcolor, hintblend, gravity, collide, partcollide(from, collide), NULL, sizechange, envcolor, envblend);
        }
    }
}

void regularshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float vel, float sizechange, int envcolor, float envblend)
{
    if(!canemitparticles()) return;
    createshape(type, radius, color, dir, num, fade, p, size, blend, hintcolor, hintblend, gravity, collide, vel, sizechange, envcolor, envblend);
}

void regularflame(int type, const vec &p, float radius, float height, int color, int density, int fade, float size, float blend, int hintcolor, float hintblend, float gravity, int collide, float vel, int envcolor, float envblend)
{
    if(!canemitparticles()) return;

    float s = size*min(radius, height);
    vec v(0, 0, min(1.0f, height)*vel);
    loopi(density)
    {
        vec q = vec(p).add(vec(rndscale(radius*2.f)-radius, rndscale(radius*2.f)-radius, 0));
        newparticle(q, v, rnd(max(int(fade*height), 1))+1, type, color, s, blend, hintcolor, hintblend, gravity, collide, partcollide(q, collide), NULL, 0, envcolor, envblend);
    }
}

void lensflare(const vec &o, const vec &color, bool sun, int sparkle, float scale)
{
    flares.addflare(o, uchar(color.r*255), uchar(color.g*255), uchar(color.b*255), sun, sparkle, scale);
}

static int partcolour(int c, int p, int x, bool hasdark = true)
{
    if(c <= 0) c = 0xFFFFFF;

    vec r = c > 0 ? vec::fromcolor(c) : vec(1, 1, 1);
    if(p || x) r.mul(game::getpalette(p, x));
    if(hasdark) r.mul(game::darkness(DARK_PART));

    return (int(r.x*255)<<16)|(int(r.y*255)<<8)|(int(r.z*255));
}

void makeparticle(const vec &o, attrvector &attr)
{
    bool oldemit = canemit;
    if(attr[12]) canemit = true;
    enviroparts = true;
    switch(attr[0])
    {
        case 0: // fire
        {
            float radius = attr[1] ? float(attr[1])/100.0f : 1.5f,
                  height = attr[2] ? float(attr[2])/100.0f : radius*3,
                  size = attr[7] ? float(attr[7])/100.f : 2.f,
                  blend = attr[8] ? float(attr[8])/100.f : 1.f,
                  vel = attr[10] ? float(attr[10]) : 30.f,
                  hintblend = attr[18] > 0 ? attr[18]/100.f : 0.f;
            int color = partcolour(attr[3] ? attr[3] : 0xF05010, attr[5], attr[6]),
                fade = attr[4] > 0 ? attr[4] : 1000, gravity = attr[9] ? attr[9] : -10,
                hintcolor = attr[17] > 0 ? attr[17] : vec::fromcolor(color).neg().tohexcolor();
            regularflame(PART_FLAME, o, radius, height, color, 3, fade/2, size, blend, hintcolor, hintblend, gravity/2, 0, vel);
            regularflame(PART_SMOKE_SOFT, vec(o).addz(2.f*min(radius, height)), radius, height, 0x101008, 1, fade, size, blend, 0x000000, 0.25f, gravity, 0, vel);
            break;
        }
        case 1: // smoke vent - <dir>
        {
            float hintblend = attr[18] > 0 ? attr[18]/100.f : 0.f;
            int hintcolor = attr[17] > 0 ? attr[17] : vec::fromcolor(0x897661).neg().tohexcolor();
            regularsplash(PART_SMOKE_SOFT, 0x897661, 2, 1, 200, offsetvec(o, attr[1], rnd(10)), 2.4f, 1.f, hintcolor, hintblend);
            break;
        }
        case 2: // water fountain - <dir>
        {
            int mat = MAT_WATER + clamp(-attr[2], 0, 3);
            const bvec &wfcol = getwaterfallcolour(mat);
            int color = (int(wfcol[0])<<16) | (int(wfcol[1])<<8) | int(wfcol[2]);
            if(!color)
            {
                const bvec &wcol = getwatercolour(mat);
                color = (int(wcol[0])<<16) | (int(wcol[1])<<8) | int(wcol[2]);
            }
            float hintblend = attr[18] > 0 ? attr[18]/100.f : 0.f;
            int hintcolor = attr[17] > 0 ? attr[17] : vec::fromcolor(color).neg().tohexcolor();
            regularsplash(PART_SPARK, color, 10, 4, 200, offsetvec(o, attr[1], rnd(10)), 0.6f, 1.f, hintcolor, hintblend);
            break;
        }
        case 3: // fire ball - <size> <rgb> <type> <blend>
        {
            int types[4] = { PART_EXPLOSION, PART_SHOCKWAVE, PART_SHOCKBALL, PART_GLIMMERY }, type = types[attr[3] >= 0 && attr[3] <= 3 ? attr[3] : 0];
            float blend = attr[4] > 0 && attr[4] < 100 ? attr[4]/100.f : 1.f;
            float hintblend = attr[18] > 0 ? attr[18]/100.f : 0.f;
            int color = partcolour(attr[2], attr[3], attr[4]), hintcolor = attr[17] > 0 ? attr[17] : vec::fromcolor(color).neg().tohexcolor();
            newparticle(o, vec(0, 0, 0), 1, type, color, 4.f, blend, hintcolor, hintblend, 0, 0, 1+attr[1]);
            break;
        }
        case 4:  // tape - <dir> <length> <rgb>
        case 7:  // lightning
        case 8:  // fire
        case 9:  // smoke
        case 10: // water
        case 11: // plasma
        case 12: // snow
        case 13: // sparks
        case 16: case 17: case 18: // haze
        case 19: // rain
        case 20: // clean flare
        case 21: // noisy flare
        case 22: // muzzle flare
        case 23: // bubble
        case 24: // splash
        case 25: // material bubble
        {
            const int typemap[] =        { PART_FLARE,   -1,     -1,     PART_LIGHTNING, PART_FIREBALL,  PART_SMOKE, PART_ELECTRIC,  PART_PLASMA,    PART_SNOW,  PART_SPARK,     -1,     -1,     PART_HAZE,  PART_FLAME_HAZE,    PART_TAPE_HAZE, PART_RAIN,     PART_CLEAN_FLARE,   PART_NOISY_FLARE,   PART_MUZZLE_FLARE,  PART_BUBBLE_SOFT,   PART_SPLASH_SOFT,   PART_BUBBLE_MATERIAL };
            const bool tapemap[] =       { true,         false,  false,  true,           false,          false,      false,          false,          false,      false,          false,  false,  false,      false,              true,           false,         true,               true,               true,               false,              false,              false };
            const float sizemap[] =      { 0.28f,        0.0f,   0.0f,   0.25f,          4.f,            2.f,        0.6f,           4.f,            0.5f,       0.2f,           0.0f,   0.0f,   8.0f,       8.0f,               1.0f,           1.0f,          0.25f,              0.25f,              0.25f,              0.25f,              0.25f,              0.25f };

            int mapped = attr[0] - 4, type = typemap[mapped];
            bool istape = tapemap[mapped], ishaze = type == PART_HAZE || type == PART_FLAME_HAZE || type == PART_TAPE_HAZE;

            int fade = attr[4] > 0 ? attr[4] : 250, gravity = !istape ? attr[7] : 0,
                stain = !istape ? (attr[6] >= 0 && attr[6] <= STAIN_MAX ? attr[6] : -1) : 0,
                color = !istape ? partcolour(attr[3], attr[9], attr[10], !ishaze) : partcolour(attr[3], attr[6], attr[7], !ishaze),
                hintcolor = attr[17] > 0 ? attr[17] : vec::fromcolor(color).neg().tohexcolor();

            float size = attr[5] != 0 ? attr[5]/100.f : sizemap[mapped], vel = !istape ? attr[8] : 1, blend = attr[11] > 0 ? attr[11]/100.f : 1.f, hintblend = attr[18] > 0 ? attr[18]/100.f : 0.f;
            if(attr[1] >= 256) regularshape(type, max(1+attr[2], 1), color, attr[1]-256, 5, fade, o, size, blend, hintcolor, hintblend, gravity, stain, vel);
            else newparticle(o, vec(offsetvec(!istape ? vec(0, 0, 0) : o, attr[1], max(1+attr[2], 0))).mul(vel), fade, type, color, size, blend, hintcolor, hintblend, gravity, stain);
            break;
        }
        case 14: // flames <radius> <height> <rgb>
        case 15: // smoke plume
        {
            const int typemap[] = { PART_FLAME, PART_SMOKE }, colormap[] = { 0xF05010, 0x101008 }, fademap[] = { 500, 1000 }, densitymap[] = { 3, 1 }, gravitymap[] = { -5, -10 };
            const float sizemap[] = { 2, 2 }, velmap[] = { 25, 50 };
            float hintblend = attr[18] > 0 ? attr[18]/100.f : 0.f;
            int color = attr[3] ? attr[3] : colormap[attr[0]-14], hintcolor = attr[17] > 0 ? attr[17] : vec::fromcolor(color).neg().tohexcolor();
            regularflame(typemap[attr[0]-14], o, float(attr[1])/100.0f, float(attr[2])/100.0f, partcolour(color, attr[8], attr[9]), densitymap[attr[0]-14], attr[4] > 0 ? attr[4] : fademap[attr[0]-14], attr[5] != 0 ? attr[5]/100.f : sizemap[attr[0]-14], attr[10] > 0 ? attr[10]/100.f : 1.f, hintcolor, hintblend, attr[6] != 0 ? attr[6] : gravitymap[attr[0]-14], 0, attr[7] != 0 ? attr[7] : velmap[attr[0]-14]);
            break;
        }
        case 6: // meter, metervs - <percent> <rgb> <rgb2>
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
        case 32: // lens flares - plain/sparkle/sun/sparklesun <red> <green> <blue>
        case 33:
        case 34:
        case 35:
        {
            float level = game::darkness(DARK_PART);
            flares.addflare(o, int(attr[1]*level), int(attr[2]*level), int(attr[3]*level), (attr[0]&2)!=0, (attr[0]&1)!=0 ? ((attr[0]&2)!=0 ? 1 : 2) : 0);
            break;
        }
        default:
            defformatstring(ds, "%d?", attr[0]);
            part_textcopy(o, ds);
            break;
    }
    canemit = oldemit;
    enviroparts = false;
}

void seedparticles()
{
    progress(0, "Seeding particles..");
    addparticleemitters();
    canemit = true;
    loopv(emitters)
    {
        particleemitter &pe = emitters[i];
        extentity &e = *pe.ent;
        seedemitter = &pe;
        for(int millis = 0; millis < seedmillis; millis += min(emitmillis, seedmillis/10))
            if(entities::checkparticle(e))
                makeparticle(e.o, e.attrs);
        seedemitter = NULL;
        pe.lastemit = -seedmillis;
        pe.finalize();
    }
}

void updateparticles()
{
    if(regenemitters) addparticleemitters();

    if(paused || (minimized && !renderunfocused))
    {
        canemit = false;
        return;
    }

    if(lastmillis - lastemitframe >= emitmillis)
    {
        canemit = true;
        lastemitframe = lastmillis - (lastmillis%emitmillis);
    }
    else canemit = false;

    numenvparts = 0;

    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->update();

    entities::drawparticles();
    flares.drawflares(); // do after drawparticles so that we can make flares for them too

    part_weather();

    if(!editmode || showparticles)
    {
        int emitted = 0, replayed = 0;
        addedparticles = 0;
        loopv(emitters)
        {
            particleemitter &pe = emitters[i];
            extentity &e = *pe.ent;
            if(e.dynamic() || !entities::checkparticle(e) || e.o.dist(camera1->o) > maxparticledistance) { pe.lastemit = lastmillis; continue; }
            if(cullparticles && pe.maxfade >= 0 && insideworld(e.o))
            {
                if(isfoggedsphere(pe.radius, pe.center)) { pe.lastcull = lastmillis; continue; }
                if(pvsoccluded(pe.cullmin, pe.cullmax)) { pe.lastcull = lastmillis; continue; }
            }
            makeparticle(e.o, e.attrs);
            emitted++;
            if(replayparticles && pe.maxfade > 5 && pe.lastcull > pe.lastemit)
            {
                for(emitoffset = max(pe.lastemit + emitmillis - lastmillis, -pe.maxfade); emitoffset < 0; emitoffset += emitmillis)
                {
                    makeparticle(e.o, e.attrs);
                    replayed++;
                }
                emitoffset = 0;
            }
            pe.lastemit = lastmillis;
        }
        if(dbgpcull && (canemit || replayed) && addedparticles) conoutf(colourred, "%d emitters, %d particles", emitted, addedparticles);
    }
}
