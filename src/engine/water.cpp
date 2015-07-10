#include "engine.h"

VARF(IDF_PERSIST, waterreflect, 0, 1, 1, { cleanreflections(); preloadwatershaders(); });
VARF(IDF_PERSIST, waterrefract, 0, 1, 1, { cleanreflections(); preloadwatershaders(); });
VARF(IDF_PERSIST, waterenvmap, 0, 1, 1, { cleanreflections(); preloadwatershaders(); });
VARF(IDF_PERSIST, waterfallrefract, 0, 0, 1, { cleanreflections(); preloadwatershaders(); });

VAR(IDF_WORLD, watersubdiv, 0, 2, 3);
VAR(IDF_WORLD, waterlod, 0, 2, 3);

static int wx1, wy1, wx2, wy2, wsize;
static float whscale, whoffset;

#define VERTW(vertw, defbody, body) \
    static inline void def##vertw() \
    { \
        varray::defattrib(varray::ATTRIB_VERTEX, 3, GL_FLOAT); \
        defbody; \
    } \
    static inline void vertw(float v1, float v2, float v3) \
    { \
        float angle = (v1-wx1)*(v2-wy1)*(v1-wx2)*(v2-wy2)*whscale+whoffset; \
        float s = angle - int(angle) - 0.5f; \
        s *= 8 - fabs(s)*16; \
        float h = WATER_AMPLITUDE*s-WATER_OFFSET; \
        varray::attrib<float>(v1, v2, v3+h); \
        body; \
    }
#define VERTWN(vertw, defbody, body) \
    static inline void def##vertw() \
    { \
        varray::defattrib(varray::ATTRIB_VERTEX, 3, GL_FLOAT); \
        defbody; \
    } \
    static inline void vertw(float v1, float v2, float v3) \
    { \
        float h = -WATER_OFFSET; \
        varray::attrib<float>(v1, v2, v3+h); \
        body; \
    }
#define VERTWT(vertwt, defbody, body) \
    VERTW(vertwt, defbody, { \
        float v = angle - int(angle+0.25) - 0.25; \
        v *= 8 - fabs(v)*16; \
        float duv = 0.5f*v; \
        body; \
    })

VERTW(vertwt, {
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
}, {
    varray::attrib<float>(v1/8.0f, v2/8.0f);
})
VERTWN(vertwtn, {
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
}, {
    varray::attrib<float>(v1/8.0f, v2/8.0f);
})

static float lavaxk = 1.0f, lavayk = 1.0f, lavascroll = 0.0f;

VERTW(vertl, {
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
}, {
    varray::attrib<float>(lavaxk*(v1+lavascroll), lavayk*(v2+lavascroll));
})
VERTWN(vertln, {
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
}, {
    varray::attrib<float>(lavaxk*(v1+lavascroll), lavayk*(v2+lavascroll));
})

#define renderwaterstrips(vertw, z) { \
    def##vertw(); \
    for(int x = wx1; x<wx2; x += subdiv) \
    { \
        varray::begin(GL_TRIANGLE_STRIP); \
        vertw(x,        wy1, z); \
        vertw(x+subdiv, wy1, z); \
        for(int y = wy1; y<wy2; y += subdiv) \
        { \
            vertw(x,        y+subdiv, z); \
            vertw(x+subdiv, y+subdiv, z); \
        } \
        xtraverts += varray::end(); \
    } \
}

void rendervertwater(int subdiv, int xo, int yo, int z, int size, int mat)
{
    wx1 = xo;
    wy1 = yo;
    wx2 = wx1 + size,
    wy2 = wy1 + size;
    wsize = size;
    whscale = 59.0f/(23.0f*wsize*wsize)/(2*M_PI);

    ASSERT((wx1 & (subdiv - 1)) == 0);
    ASSERT((wy1 & (subdiv - 1)) == 0);

    switch(mat)
    {
        case MAT_WATER:
        {
            whoffset = fmod(float(lastmillis/600.0f/(2*M_PI)), 1.0f);
            renderwaterstrips(vertwt, z);
            break;
        }

        case MAT_LAVA:
        {
            whoffset = fmod(float(lastmillis/2000.0f/(2*M_PI)), 1.0f);
            renderwaterstrips(vertl, z);
            break;
        }
    }
}

int calcwatersubdiv(int x, int y, int z, int size)
{
    float dist;
    if(camera1->o.x >= x && camera1->o.x < x + size &&
       camera1->o.y >= y && camera1->o.y < y + size)
        dist = fabs(camera1->o.z - float(z));
    else
        dist = vec(x + size/2, y + size/2, z + size/2).dist(camera1->o) - size*1.42f/2;
    int subdiv = watersubdiv + int(dist) / (32 << waterlod);
    return subdiv >= 31 ? INT_MAX : 1<<subdiv;
}

int renderwaterlod(int x, int y, int z, int size, int mat)
{
    if(size <= (32 << waterlod))
    {
        int subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv < size * 2) rendervertwater(min(subdiv, size), x, y, z, size, mat);
        return subdiv;
    }
    else
    {
        int subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv >= size)
        {
            if(subdiv < size * 2) rendervertwater(size, x, y, z, size, mat);
            return subdiv;
        }
        int childsize = size / 2,
            subdiv1 = renderwaterlod(x, y, z, childsize, mat),
            subdiv2 = renderwaterlod(x + childsize, y, z, childsize, mat),
            subdiv3 = renderwaterlod(x + childsize, y + childsize, z, childsize, mat),
            subdiv4 = renderwaterlod(x, y + childsize, z, childsize, mat),
            minsubdiv = subdiv1;
        minsubdiv = min(minsubdiv, subdiv2);
        minsubdiv = min(minsubdiv, subdiv3);
        minsubdiv = min(minsubdiv, subdiv4);
        if(minsubdiv < size * 2)
        {
            if(minsubdiv >= size) rendervertwater(size, x, y, z, size, mat);
            else
            {
                if(subdiv1 >= size) rendervertwater(childsize, x, y, z, childsize, mat);
                if(subdiv2 >= size) rendervertwater(childsize, x + childsize, y, z, childsize, mat);
                if(subdiv3 >= size) rendervertwater(childsize, x + childsize, y + childsize, z, childsize, mat);
                if(subdiv4 >= size) rendervertwater(childsize, x, y + childsize, z, childsize, mat);
            }
        }
        return minsubdiv;
    }
}

#define renderwaterquad(vertwn, z) \
    { \
        if(varray::data.empty()) { def##vertwn(); varray::begin(GL_QUADS); } \
        vertwn(x, y, z); \
        vertwn(x+rsize, y, z); \
        vertwn(x+rsize, y+csize, z); \
        vertwn(x, y+csize, z); \
        xtraverts += 4; \
    }

void renderflatwater(int x, int y, int z, int rsize, int csize, int mat)
{
    switch(mat)
    {
        case MAT_WATER:
            renderwaterquad(vertwtn, z);
            break;

        case MAT_LAVA:
            renderwaterquad(vertln, z);
            break;
    }
}

VARF(IDF_WORLD, vertwater, 0, 1, 1, if(!(identflags&IDF_WORLD)) allchanged());

static inline void renderwater(const materialsurface &m, int mat = MAT_WATER)
{
    if(!vertwater || minimapping) renderflatwater(m.o.x, m.o.y, m.o.z, m.rsize, m.csize, mat);
    else if(renderwaterlod(m.o.x, m.o.y, m.o.z, m.csize, mat) >= int(m.csize) * 2)
        rendervertwater(m.csize, m.o.x, m.o.y, m.o.z, m.csize, mat);
}

void renderlava(const materialsurface &m, Texture *tex, float scale)
{
    lavaxk = 8.0f/(tex->xs*scale);
    lavayk = 8.0f/(tex->ys*scale);
    lavascroll = lastmillis/1000.0f;
    renderwater(m, MAT_LAVA);
}

/* reflective/refractive water */

#define MAXREFLECTIONS 16

struct Reflection
{
    GLuint tex, refracttex;
    int material, height, depth, age;
    bool init;
    glmatrixf projmat;
    occludequery *query, *prevquery;
    vector<materialsurface *> matsurfs;

    Reflection() : tex(0), refracttex(0), material(-1), height(-1), depth(0), age(0), init(false), query(NULL), prevquery(NULL)
    {}
};

VAR(IDF_PERSIST, reflectdist, 0, 2000, 10000);

#define WATERVARS(name) \
    VAR(IDF_WORLD, name##fog, 0, 150, 10000); \
    bvec name##col(0x10, 0x30, 0x60), name##fallcol(0, 0, 0); \
    VARF(IDF_HEX|IDF_WORLD, name##colour, 0, 0x103060, 0xFFFFFF, \
    { \
        name##col = bvec((name##colour>>16)&0xFF, (name##colour>>8)&0xFF, name##colour&0xFF); \
    }); \
    VARF(IDF_HEX|IDF_WORLD, name##fallcolour, 0, 0, 0xFFFFFF, \
    { \
        name##fallcol = bvec((name##fallcolour>>16)&0xFF, (name##fallcolour>>8)&0xFF, name##fallcolour&0xFF); \
    }); \
    VAR(IDF_WORLD, name##spec, 0, 150, 1000);

WATERVARS(water)
WATERVARS(water2)
WATERVARS(water3)
WATERVARS(water4)

GETMATIDXVAR(water, colour, int)
GETMATIDXVAR(water, col, const bvec &)
GETMATIDXVAR(water, fallcolour, int)
GETMATIDXVAR(water, fallcol, const bvec &)
GETMATIDXVAR(water, fog, int)
GETMATIDXVAR(water, spec, int)

#define LAVAVARS(name) \
    VAR(IDF_WORLD, name##fog, 0, 50, 10000); \
    bvec name##col(0xFF, 0x44, 0x00); \
    VARF(IDF_HEX|IDF_WORLD, name##colour, 0, 0xFF4400, 0xFFFFFF, \
    { \
        name##col = bvec((name##colour>>16)&0xFF, (name##colour>>8)&0xFF, name##colour&0xFF); \
    });

LAVAVARS(lava)
LAVAVARS(lava2)
LAVAVARS(lava3)
LAVAVARS(lava4)

GETMATIDXVAR(lava, colour, int)
GETMATIDXVAR(lava, col, const bvec &)
GETMATIDXVAR(lava, fog, int)

void setprojtexmatrix(Reflection &ref)
{
    if(ref.init)
    {
        ref.init = false;
        (ref.projmat = mvpmatrix).projective();
    }

    glLoadMatrixf(ref.projmat.v);
}

Reflection reflections[MAXREFLECTIONS];
Reflection waterfallrefraction;
GLuint reflectionfb = 0, reflectiondb = 0;

GLuint getwaterfalltex() { return waterfallrefraction.refracttex ? waterfallrefraction.refracttex : notexture->id; }

VAR(0, oqwater, 0, 2, 2);
VARF(IDF_PERSIST, waterfade, 0, 1, 1, { cleanreflections(); preloadwatershaders(); });

void preloadwatershaders(bool force)
{
    static bool needwater = false;
    if(force) needwater = true;
    if(!needwater) return;

    useshaderbyname("waterglare");

    if(waterenvmap && !waterreflect)
        useshaderbyname(waterrefract ? (waterfade && hasFBO ? "waterenvfade" : "waterenvrefract") : "waterenv");
    else useshaderbyname(waterrefract ? (waterfade && hasFBO ? "waterfade" : "waterrefract") : (waterreflect ? "waterreflect" : "water"));

    useshaderbyname(waterrefract ? (waterfade && hasFBO ? "underwaterfade" : "underwaterrefract") : "underwater");

    extern int waterfallenv;
    if(waterfallenv) useshaderbyname("waterfallenv");
    if(waterfallrefract) useshaderbyname(waterfallenv ? "waterfallenvrefract" : "waterfallrefract");
}

void renderwater()
{
    if(editmode && showmat && !envmapping) return;
    if(!rplanes) return;

    glDisable(GL_CULL_FACE);

    glActiveTexture_(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture_(GL_TEXTURE2);
    glEnable(GL_TEXTURE_2D);

    if(!glaring && !minimapping)
    {
        if(waterrefract)
        {
            glActiveTexture_(GL_TEXTURE3);
            glEnable(GL_TEXTURE_2D);
            if(waterfade && hasFBO)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
        }
        else
        {
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_SRC_ALPHA);
        }
    }
    glActiveTexture_(GL_TEXTURE0);

    if(!glaring && waterenvmap && !waterreflect && !minimapping)
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_CUBE_MAP);
    }

    setenvparamf("camera", SHPARAM_VERTEX, 0, camera1->o.x, camera1->o.y, camera1->o.z);
    setenvparamf("millis", SHPARAM_VERTEX, 1, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f);

    #define SETWATERSHADER(which, name) \
    do { \
        static Shader *name##shader = NULL; \
        if(!name##shader) name##shader = lookupshaderbyname(#name); \
        which##shader = name##shader; \
    } while(0)

    Shader *aboveshader = NULL;
    if(glaring) SETWATERSHADER(above, waterglare);
    else if(minimapping) aboveshader = notextureshader;
    else if(waterenvmap && !waterreflect)
    {
        if(waterrefract)
        {
            if(waterfade && hasFBO) SETWATERSHADER(above, waterenvfade);
            else SETWATERSHADER(above, waterenvrefract);
        }
        else SETWATERSHADER(above, waterenv);
    }
    else if(waterrefract)
    {
        if(waterfade && hasFBO) SETWATERSHADER(above, waterfade);
        else SETWATERSHADER(above, waterrefract);
    }
    else if(waterreflect) SETWATERSHADER(above, waterreflect);
    else SETWATERSHADER(above, water);

    Shader *belowshader = NULL;
    if(!glaring && !minimapping)
    {
        if(waterrefract)
        {
            if(waterfade && hasFBO) SETWATERSHADER(below, underwaterfade);
            else SETWATERSHADER(below, underwaterrefract);
        }
        else SETWATERSHADER(below, underwater);

        if(waterreflect || waterrefract) glMatrixMode(GL_TEXTURE);
    }

    varray::enable();

    vec amb(max(skylightcolor[0], ambientcolor[0]), max(skylightcolor[1], ambientcolor[1]), max(skylightcolor[2], ambientcolor[2]));
    float offset = -WATER_OFFSET;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || ref.age || ref.matsurfs.empty()) continue;
        if(!glaring && oqfrags && oqwater && ref.query && ref.query->owner==&ref)
        {
            if(!ref.prevquery || ref.prevquery->owner!=&ref || checkquery(ref.prevquery))
            {
                if(checkquery(ref.query)) continue;
            }
        }

        bool below = camera1->o.z < ref.height+offset;
        if(below)
        {
            if(!belowshader) continue;
            belowshader->set();
        }
        else aboveshader->set();

        if(!glaring && !minimapping)
        {
            if(waterreflect || waterrefract)
            {
                if(waterreflect || !waterenvmap) glBindTexture(GL_TEXTURE_2D, waterreflect ? ref.tex : ref.refracttex);
                setprojtexmatrix(ref);
            }

            if(waterrefract)
            {
                glActiveTexture_(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, ref.refracttex);
                if(waterfade)
                {
                    float fadeheight = ref.height+offset+(below ? -2 : 2);
                    setlocalparamf("waterheight", SHPARAM_VERTEX, 7, fadeheight, fadeheight, fadeheight);
                }
            }
        }

        MSlot &mslot = lookupmaterialslot(ref.material);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mslot.sts.inrange(2) ? mslot.sts[2].t->id : notexture->id);
        glActiveTexture_(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mslot.sts.inrange(3) ? mslot.sts[3].t->id : notexture->id);
        glActiveTexture_(GL_TEXTURE0);
        if(!glaring && waterenvmap && !waterreflect && !minimapping)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, lookupenvmap(mslot));
        }

        glColor3ubv(getwatercol(ref.material).v);
        int wfog = getwaterfog(ref.material), wspec = getwaterspec(ref.material);

        const extentity *lastlight = (const extentity *)-1;
        int lastdepth = -1;
        loopvj(ref.matsurfs)
        {
            materialsurface &m = *ref.matsurfs[j];

            const extentity *light = (m.light && m.light->type==ET_LIGHT ? m.light : NULL);
            if(light!=lastlight)
            {
                if(varray::data.length()) varray::end();
                const vec &lightpos = light ? light->o : vec(hdr.worldsize/2, hdr.worldsize/2, hdr.worldsize);
                float lightrad = light && light->type == ET_LIGHT && light->attrs[0] ? light->attrs[0] : hdr.worldsize*8.0f;
                const vec &lightcol = (light ? (light->type == ET_LIGHT ? vec(light->attrs[1], light->attrs[2], light->attrs[3]) : vec(light->attrs[2], light->attrs[3], light->attrs[4])) : vec(amb)).div(255.0f).mul(wspec/100.0f);
                setlocalparamf("lightpos", SHPARAM_VERTEX, 2, lightpos.x, lightpos.y, lightpos.z);
                setlocalparamf("lightcolor", SHPARAM_PIXEL, 3, lightcol.x, lightcol.y, lightcol.z);
                setlocalparamf("lightradius", SHPARAM_PIXEL, 4, lightrad, lightrad, lightrad);
                lastlight = light;
            }

            if(!glaring && !waterrefract && m.depth!=lastdepth)
            {
                if(varray::data.length()) varray::end();
                float depth = !wfog ? 1.0f : min(0.75f*m.depth/wfog, 0.95f);
                depth = max(depth, !below && (waterreflect || waterenvmap) ? 0.3f : 0.6f);
                setlocalparamf("depth", SHPARAM_PIXEL, 5, depth, 1.0f-depth);
                lastdepth = m.depth;
            }

            renderwater(m);
        }
        if(varray::data.length()) varray::end();
    }

    varray::disable();

    if(!glaring && !minimapping)
    {
        if(waterreflect || waterrefract)
        {
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
        }
        if(waterrefract)
        {
            glActiveTexture_(GL_TEXTURE3);
            glDisable(GL_TEXTURE_2D);
            if(hasFBO && waterfade) glDisable(GL_BLEND);
        }
        else
        {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }

    loopi(2)
    {
        glActiveTexture_(GL_TEXTURE1+i);
        glDisable(GL_TEXTURE_2D);
    }
    glActiveTexture_(GL_TEXTURE0);

    if(!glaring && waterenvmap && !waterreflect && !minimapping)
    {
        glDisable(GL_TEXTURE_CUBE_MAP);
        glEnable(GL_TEXTURE_2D);
    }

    glEnable(GL_CULL_FACE);
}

void setupwaterfallrefract(GLenum tmu1, GLenum tmu2)
{
    glActiveTexture_(tmu1);
    glBindTexture(GL_TEXTURE_2D, waterfallrefraction.refracttex ? waterfallrefraction.refracttex : notexture->id);
    glActiveTexture_(tmu2);
    glMatrixMode(GL_TEXTURE);
    setprojtexmatrix(waterfallrefraction);
    glMatrixMode(GL_MODELVIEW);
}

void cleanreflection(Reflection &ref)
{
    ref.material = -1;
    ref.height = -1;
    ref.init = false;
    ref.query = ref.prevquery = NULL;
    ref.matsurfs.setsize(0);
    if(ref.tex)
    {
        glDeleteTextures(1, &ref.tex);
        ref.tex = 0;
    }
    if(ref.refracttex)
    {
        glDeleteTextures(1, &ref.refracttex);
        ref.refracttex = 0;
    }
}

void cleanreflections()
{
    loopi(MAXREFLECTIONS) cleanreflection(reflections[i]);
    cleanreflection(waterfallrefraction);
    if(reflectionfb)
    {
        glDeleteFramebuffers_(1, &reflectionfb);
        reflectionfb = 0;
    }
    if(reflectiondb)
    {
        glDeleteRenderbuffers_(1, &reflectiondb);
        reflectiondb = 0;
    }
}

VARF(IDF_PERSIST, reflectsize, 6, 8, 10, cleanreflections());

void genwatertex(GLuint &tex, GLuint &fb, GLuint &db, bool refract = false)
{
    static const GLenum colorfmts[] = { GL_RGBA, GL_RGBA8, GL_RGB, GL_RGB8, GL_FALSE },
                        depthfmts[] = { GL_DEPTH_STENCIL_EXT, GL_DEPTH24_STENCIL8_EXT, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT32, GL_FALSE };
    const int stencilfmts = 2;
    static GLenum reflectfmt = GL_FALSE, refractfmt = GL_FALSE, depthfmt = GL_FALSE, stencilfmt = GL_FALSE;
    static bool usingalpha = false;
    bool needsalpha = refract && hasFBO && waterrefract && waterfade;
    if(refract && usingalpha!=needsalpha)
    {
        usingalpha = needsalpha;
        refractfmt = GL_FALSE;
    }
    int size = 1<<reflectsize;
    if(!hasFBO) while(size>screen->w || size>screen->h) size /= 2;
    while(size>hwtexsize) size /= 2;

    glGenTextures(1, &tex);
    char *buf = new char[size*size*4];
    memset(buf, 0, size*size*4);

    GLenum &colorfmt = refract ? refractfmt : reflectfmt;
    if(colorfmt && (!hasFBO || (fb && db)))
    {
        createtexture(tex, size, size, buf, 3, 1, colorfmt);
        delete[] buf;
        return;
    }

    if(hasFBO)
    {
        if(!fb) glGenFramebuffers_(1, &fb);
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, fb);
    }

    int find = needsalpha ? 0 : 2;
    do
    {
        createtexture(tex, size, size, buf, 3, 1, colorfmt ? colorfmt : colorfmts[find]);
        if(!hasFBO) break;
        else
        {
            glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex, 0);
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT) break;
        }
    }
    while(!colorfmt && colorfmts[++find]);
    if(!colorfmt) colorfmt = colorfmts[find];

    delete[] buf;

    if(hasFBO)
    {
        if(!db) { glGenRenderbuffers_(1, &db); depthfmt = stencilfmt = GL_FALSE; }
        if(!depthfmt) glBindRenderbuffer_(GL_RENDERBUFFER_EXT, db);
        find = hasstencil && hasDS ? 0 : stencilfmts;
        do
        {
            if(!depthfmt) glRenderbufferStorage_(GL_RENDERBUFFER_EXT, depthfmts[find], size, size);
            glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, db);
            if(depthfmt ? stencilfmt : find<stencilfmts) glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, db);
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT) break;
        }
        while(!depthfmt && depthfmts[++find]);
        if(!depthfmt)
        {
            glBindRenderbuffer_(GL_RENDERBUFFER_EXT, 0);
            depthfmt = depthfmts[find];
            stencilfmt = find<stencilfmts ? depthfmt : GL_FALSE;
        }

        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
    }
}

void addwaterfallrefraction(materialsurface &m)
{
    Reflection &ref = waterfallrefraction;
    if(ref.age>=0)
    {
        ref.age = -1;
        ref.init = false;
        ref.matsurfs.setsize(0);
        ref.material = MAT_WATER;
        ref.height = INT_MAX;
    }
    ref.matsurfs.add(&m);

    if(!ref.refracttex) genwatertex(ref.refracttex, reflectionfb, reflectiondb);
}

void addreflection(materialsurface &m)
{
    int mat = m.material, height = m.o.z;
    Reflection *ref = NULL, *oldest = NULL;
    loopi(MAXREFLECTIONS)
    {
        Reflection &r = reflections[i];
        if(r.height<0)
        {
            if(!ref) ref = &r;
        }
        else if(r.height==height && r.material==mat)
        {
            r.matsurfs.add(&m);
            r.depth = max(r.depth, int(m.depth));
            if(r.age<0) return;
            ref = &r;
            break;
        }
        else if(!oldest || r.age>oldest->age) oldest = &r;
    }
    if(!ref)
    {
        if(!oldest || oldest->age<0) return;
        ref = oldest;
    }
    if(ref->height!=height || ref->material!=mat)
    {
        ref->material = mat;
        ref->height = height;
        ref->prevquery = NULL;
    }
    rplanes++;
    ref->age = -1;
    ref->init = false;
    ref->matsurfs.setsize(0);
    ref->matsurfs.add(&m);
    ref->depth = m.depth;
    if(minimapping) return;

    if(waterreflect && !ref->tex) genwatertex(ref->tex, reflectionfb, reflectiondb);
    if(waterrefract && !ref->refracttex) genwatertex(ref->refracttex, reflectionfb, reflectiondb, true);
}

static void drawmaterialquery(const materialsurface &m, float offset, float border = 0)
{
    if(varray::data.empty())
    {
        varray::defattrib(varray::ATTRIB_VERTEX, 3, GL_FLOAT);
        varray::begin(GL_QUADS);
    }
    float x = m.o.x, y = m.o.y, z = m.o.z, csize = m.csize + border, rsize = m.rsize + border;
    switch(m.orient)
    {
#define GENFACEORIENT(orient, v0, v1, v2, v3) \
        case orient: v0 v1 v2 v3 break;
#define GENFACEVERT(orient, vert, mx,my,mz, sx,sy,sz) \
            varray::attrib<float>(mx sx, my sy, mz sz);
        GENFACEVERTS(x, x, y, y, z, z, - border, + csize, - border, + rsize, + offset, - offset)
#undef GENFACEORIENT
#undef GENFACEVERT
    }
}

extern vtxarray *visibleva;
extern void drawreflection(float z, bool refract, int fogdepth = -1, const bvec &col = bvec(0, 0, 0));

int rplanes = 0;

void queryreflection(Reflection &ref, bool init)
{
    if(init)
    {
        nocolorshader->set();
        glDepthMask(GL_FALSE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDisable(GL_CULL_FACE);
    }
    startquery(ref.query);
    loopvj(ref.matsurfs)
    {
        materialsurface &m = *ref.matsurfs[j];
        float offset = 0.1f;
        if(m.orient==O_TOP)
        {
            offset = WATER_OFFSET +
                (vertwater ? WATER_AMPLITUDE*(camera1->pitch > 0 || m.depth < WATER_AMPLITUDE+0.5f ? -1 : 1) : 0);
            if(fabs(m.o.z-offset - camera1->o.z) < 0.5f && m.depth > WATER_AMPLITUDE+1.5f)
                offset += camera1->pitch > 0 ? -1 : 1;
        }
        drawmaterialquery(m, offset);
    }
    xtraverts += varray::end();
    endquery(ref.query);
}

void queryreflections()
{
    rplanes = 0;

    static int lastsize = 0;
    int size = 1<<reflectsize;
    if(!hasFBO) while(size>screen->w || size>screen->h) size /= 2;
    while(size>hwtexsize) size /= 2;
    if(size!=lastsize) { if(lastsize) cleanreflections(); lastsize = size; }

    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->matsurfs || va->occluded >= OCCLUDE_BB || va->curvfc >= VFC_FOGGED) continue;
        int lastmat = -1;
        loopi(va->matsurfs)
        {
            materialsurface &m = va->matbuf[i];
            if(m.material != lastmat)
            {
                if((m.material&MATF_VOLUME) != MAT_WATER || m.orient == O_BOTTOM) { i += m.skip; continue; }
                if(m.orient != O_TOP)
                {
                    if(!waterfallrefract || !getwaterfog(m.material)) { i += m.skip; continue; }
                }
                lastmat = m.material;
            }
            if(m.orient==O_TOP) addreflection(m);
            else addwaterfallrefraction(m);
        }
    }

    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        ++ref.age;
        if(ref.height>=0 && !ref.age && ref.matsurfs.length())
        {
            if(waterpvsoccluded(ref.height)) ref.matsurfs.setsize(0);
        }
    }
    if(waterfallrefract)
    {
        Reflection &ref = waterfallrefraction;
        ++ref.age;
        if(ref.height>=0 && !ref.age && ref.matsurfs.length())
        {
            if(waterpvsoccluded(-1)) ref.matsurfs.setsize(0);
        }
    }

    if((editmode && showmat && !envmapping) || !oqfrags || !oqwater || minimapping) return;

    varray::enable();

    int refs = 0;
    if(waterreflect || waterrefract) loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        ref.prevquery = oqwater > 1 ? ref.query : NULL;
        ref.query = ref.height>=0 && !ref.age && ref.matsurfs.length() ? newquery(&ref) : NULL;
        if(ref.query) queryreflection(ref, !refs++);
    }
    if(waterfallrefract)
    {
        Reflection &ref = waterfallrefraction;
        ref.prevquery = oqwater > 1 ? ref.query : NULL;
        ref.query = ref.height>=0 && !ref.age && ref.matsurfs.length() ? newquery(&ref) : NULL;
        if(ref.query) queryreflection(ref, !refs++);
    }

    varray::disable();

    if(refs)
    {
        defaultshader->set();
        glDepthMask(GL_TRUE);
        glColorMask(COLORMASK, GL_TRUE);
        glEnable(GL_CULL_FACE);
    }

    glFlush();
}

VAR(IDF_PERSIST, maxreflect, 1, 2, 8);

int refracting = 0, refractfog = 0;
bvec refractcolor(0, 0, 0);
bool reflecting = false, fading = false, fogging = false;
float reflectz = 1e16f;

VAR(0, maskreflect, 0, 2, 16);

void maskreflection(Reflection &ref, float offset, bool reflect, bool clear = false)
{
    const bvec &wcol = getwatercol(ref.material);
    float fogc[4] = { wcol[0]/255.0f, wcol[1]/255.0f, wcol[2]/255.0f, 1.0f };
    bool inside = ref.height < INT_MAX && !hasFBO && vertwater && fabs(ref.height + offset - camera1->o.z) <= WATER_AMPLITUDE;
    if(!maskreflect || inside)
    {
        if(clear || inside) glClearColor(fogc[0], fogc[1], fogc[2], fogc[3]);
        glClear(GL_DEPTH_BUFFER_BIT | (clear || inside ? GL_COLOR_BUFFER_BIT : 0) | (hasstencil && hasDS ? GL_STENCIL_BUFFER_BIT : 0));
        return;
    }
    glClearDepth(0);
    glClear(GL_DEPTH_BUFFER_BIT | (hasstencil && hasDS ? GL_STENCIL_BUFFER_BIT : 0));
    glClearDepth(1);
    glDepthRange(1, 1);
    glDepthFunc(GL_ALWAYS);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    if(clear)
    {
        notextureshader->set();
        glColor3f(fogc[0], fogc[1], fogc[2]);
    }
    else
    {
        nocolorshader->set();
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
    if(reflect)
    {
        glPushMatrix();
        glTranslatef(0, 0, 2*(ref.height+offset));
        glScalef(1, 1, -1);
    }
    varray::enable();
    loopv(ref.matsurfs)
    {
        materialsurface &m = *ref.matsurfs[i];
        drawmaterialquery(m, -offset, maskreflect);
    }
    xtraverts += varray::end();
    varray::disable();
    if(reflect) glPopMatrix();
    if(!clear) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    defaultshader->set();
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glDepthFunc(GL_LESS);
    glDepthRange(0, 1);
}

VAR(0, reflectscissor, 0, 1, 1);
VAR(0, reflectvfc, 0, 1, 1);

static bool calcscissorbox(Reflection &ref, int size, vec &clipmin, vec &clipmax, int &sx, int &sy, int &sw, int &sh)
{
    materialsurface &m0 = *ref.matsurfs[0];
    int dim = dimension(m0.orient), r = R[dim], c = C[dim];
    ivec bbmin = m0.o, bbmax = bbmin;
    bbmax[r] += m0.rsize;
    bbmax[c] += m0.csize;
    loopvj(ref.matsurfs)
    {
        materialsurface &m = *ref.matsurfs[j];
        bbmin[r] = min(bbmin[r], m.o[r]);
        bbmin[c] = min(bbmin[c], m.o[c]);
        bbmax[r] = max(bbmax[r], m.o[r] + m.rsize);
        bbmax[c] = max(bbmax[c], m.o[c] + m.csize);
        bbmin[dim] = min(bbmin[dim], m.o[dim]);
        bbmax[dim] = max(bbmax[dim], m.o[dim]);
    }

    vec4 v[8];
    float sx1 = 1, sy1 = 1, sx2 = -1, sy2 = -1;
    loopi(8)
    {
        vec4 &p = v[i];
        mvpmatrix.transform(vec(i&1 ? bbmax.x : bbmin.x, i&2 ? bbmax.y : bbmin.y, (i&4 ? bbmax.z + WATER_AMPLITUDE : bbmin.z - WATER_AMPLITUDE) - WATER_OFFSET), p);
        if(p.z >= -p.w)
        {
            float x = p.x / p.w, y = p.y / p.w;
            sx1 = min(sx1, x);
            sy1 = min(sy1, y);
            sx2 = max(sx2, x);
            sy2 = max(sy2, y);
        }
    }
    if(sx1 >= sx2 || sy1 >= sy2) return false;
    loopi(8)
    {
        const vec4 &p = v[i];
        if(p.z >= -p.w) continue;
        loopj(3)
        {
            const vec4 &o = v[i^(1<<j)];
            if(o.z <= -o.w) continue;
            float t = (p.z + p.w)/(p.z + p.w - o.z - o.w),
                  w = p.w + t*(o.w - p.w),
                  x = (p.x + t*(o.x - p.x))/w,
                  y = (p.y + t*(o.y - p.y))/w;
            sx1 = min(sx1, x);
            sy1 = min(sy1, y);
            sx2 = max(sx2, x);
            sy2 = max(sy2, y);
        }
    }
    if(sx1 <= -1 && sy1 <= -1 && sx2 >= 1 && sy2 >= 1) return false;
    sx1 = max(sx1, -1.0f);
    sy1 = max(sy1, -1.0f);
    sx2 = min(sx2, 1.0f);
    sy2 = min(sy2, 1.0f);
    if(reflectvfc)
    {
        clipmin.x = clamp(clipmin.x, sx1, sx2);
        clipmin.y = clamp(clipmin.y, sy1, sy2);
        clipmax.x = clamp(clipmax.x, sx1, sx2);
        clipmax.y = clamp(clipmax.y, sy1, sy2);
    }
    sx = int(floor((hasFBO ? 0 : screen->w-size) + (sx1+1)*0.5f*size));
    sy = int(floor((hasFBO ? 0 : screen->h-size) + (sy1+1)*0.5f*size));
    sw = max(int(ceil((hasFBO ? 0 : screen->w-size) + (sx2+1)*0.5f*size)) - sx, 0);
    sh = max(int(ceil((hasFBO ? 0 : screen->h-size) + (sy2+1)*0.5f*size)) - sy, 0);
    return true;
}

VAR(IDF_WORLD, refractclear, 0, 0, 1);

void drawreflections()
{
    if((editmode && showmat && !envmapping) || minimapping) return;

    static int lastdrawn = 0;
    int refs = 0, n = lastdrawn;
    float offset = -WATER_OFFSET;
    int size = 1<<reflectsize;
    if(!hasFBO) while(size>screen->w || size>screen->h) size /= 2;
    while(size>hwtexsize) size /= 2;

    if(waterreflect || waterrefract) loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[++n%MAXREFLECTIONS];
        if(ref.height<0 || ref.age || ref.matsurfs.empty()) continue;
        if(oqfrags && oqwater && ref.query && ref.query->owner==&ref)
        {
            if(!ref.prevquery || ref.prevquery->owner!=&ref || checkquery(ref.prevquery))
            {
                if(checkquery(ref.query)) continue;
            }
        }

        if(!refs)
        {
            glViewport(hasFBO ? 0 : screen->w-size, hasFBO ? 0 : screen->h-size, size, size);
            if(hasFBO) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, reflectionfb);
        }
        refs++;
        ref.init = true;
        lastdrawn = n;

        vec clipmin(-1, -1, -1), clipmax(1, 1, 1);
        int sx, sy, sw, sh;
        bool scissor = reflectscissor && calcscissorbox(ref, size, clipmin, clipmax, sx, sy, sw, sh);
        if(scissor) glScissor(sx, sy, sw, sh);
        else
        {
            sx = hasFBO ? 0 : screen->w-size;
            sy = hasFBO ? 0 : screen->h-size;
            sw = sh = size;
        }

        const bvec &wcol = getwatercol(ref.material);
        int wfog = getwaterfog(ref.material);

        if(waterreflect && ref.tex && camera1->o.z >= ref.height+offset)
        {
            if(hasFBO) glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref.tex, 0);
            if(scissor) glEnable(GL_SCISSOR_TEST);
            maskreflection(ref, offset, true);
            savevfcP();
            setvfcP(ref.height+offset, clipmin, clipmax);
            drawreflection(ref.height+offset, false);
            restorevfcP();
            if(scissor) glDisable(GL_SCISSOR_TEST);
            if(!hasFBO)
            {
                glBindTexture(GL_TEXTURE_2D, ref.tex);
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, sx-(screen->w-size), sy-(screen->h-size), sx, sy, sw, sh);
            }
        }

        if(waterrefract && ref.refracttex)
        {
            if(hasFBO) glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref.refracttex, 0);
            if(scissor) glEnable(GL_SCISSOR_TEST);
            maskreflection(ref, offset, false, refractclear || !wfog || (ref.depth>=10000 && camera1->o.z >= ref.height + offset));
            if(wfog || (waterfade && hasFBO))
            {
                savevfcP();
                setvfcP(-1, clipmin, clipmax);
                drawreflection(ref.height+offset, true, wfog, wcol);
                restorevfcP();
            }
            if(scissor) glDisable(GL_SCISSOR_TEST);
            if(!hasFBO)
            {
                glBindTexture(GL_TEXTURE_2D, ref.refracttex);
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, sx-(screen->w-size), sy-(screen->h-size), sx, sy, sw, sh);
            }
        }

        if(refs>=maxreflect) break;
    }

    if(waterfallrefract && waterfallrefraction.refracttex)
    {
        Reflection &ref = waterfallrefraction;

        if(ref.height<0 || ref.age || ref.matsurfs.empty()) goto nowaterfall;
        if(oqfrags && oqwater && ref.query && ref.query->owner==&ref)
        {
            if(!ref.prevquery || ref.prevquery->owner!=&ref || checkquery(ref.prevquery))
            {
                if(checkquery(ref.query)) goto nowaterfall;
            }
        }

        if(!refs)
        {
            glViewport(hasFBO ? 0 : screen->w-size, hasFBO ? 0 : screen->h-size, size, size);
            if(hasFBO) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, reflectionfb);
        }
        refs++;
        ref.init = true;

        vec clipmin(-1, -1, -1), clipmax(1, 1, 1);
        int sx, sy, sw, sh;
        bool scissor = reflectscissor && calcscissorbox(ref, size, clipmin, clipmax, sx, sy, sw, sh);
        if(scissor) glScissor(sx, sy, sw, sh);
        else
        {
            sx = hasFBO ? 0 : screen->w-size;
            sy = hasFBO ? 0 : screen->h-size;
            sw = sh = size;
        }

        if(hasFBO) glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref.refracttex, 0);
        if(scissor) glEnable(GL_SCISSOR_TEST);
        maskreflection(ref, -0.1f, false);
        savevfcP();
        setvfcP(-1, clipmin, clipmax);
        drawreflection(-1, true);
        restorevfcP();
        if(scissor) glDisable(GL_SCISSOR_TEST);
        if(!hasFBO)
        {
            glBindTexture(GL_TEXTURE_2D, ref.refracttex);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, sx-(screen->w-size), sy-(screen->h-size), sx, sy, sw, sh);
        }
    }
nowaterfall:

    if(!refs) return;
    glViewport(0, 0, screen->w, screen->h);
    if(hasFBO) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);

    defaultshader->set();
}

