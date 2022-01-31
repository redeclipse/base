#include "engine.h"

#define NUMCAUSTICS 32

static Texture *caustictex[NUMCAUSTICS] = { NULL };

void loadcaustics(bool force)
{
    static bool needcaustics = false;
    if(force) needcaustics = true;
    if(!caustics || !needcaustics) return;
    useshaderbyname("caustics");
    if(caustictex[0]) return;
    loopi(NUMCAUSTICS)
    {
        defformatstring(name, "<grey><noswizzle>caustics/caust%.2d.png", i);
        caustictex[i] = textureload(name);
    }
}

void cleanupcaustics()
{
    loopi(NUMCAUSTICS) caustictex[i] = NULL;
}

VARF(IDF_WORLD, causticscale, 0, 50, 10000, preloadwatershaders());
VARF(IDF_WORLD, causticmillis, 0, 75, 1000, preloadwatershaders());
FVAR(IDF_WORLD, causticcontrast, 0, 0.6f, 2);
FVAR(IDF_WORLD, causticoffset, 0, 0.7f, 1);
VARF(IDF_PERSIST, caustics, 0, 1, 1, { loadcaustics(); preloadwatershaders(); });

void setupcaustics(int tmu, float surface = -1e16f)
{
    if(!caustictex[0]) loadcaustics(true);

    vec s = vec(0.011f, 0, 0.0066f).mul(100.0f/causticscale), t = vec(0, 0.011f, 0.0066f).mul(100.0f/causticscale);
    int tex = (lastmillis/causticmillis)%NUMCAUSTICS;
    float frac = float(lastmillis%causticmillis)/causticmillis;
    loopi(2)
    {
        glActiveTexture_(GL_TEXTURE0+tmu+i);
        glBindTexture(GL_TEXTURE_2D, caustictex[(tex+i)%NUMCAUSTICS]->id);
    }
    glActiveTexture_(GL_TEXTURE0);
    float blendscale = causticcontrast, blendoffset = 1;
    if(surface > -1e15f)
    {
        float bz = surface + camera1->o.z + (vertwater ? VOLUME_AMPLITUDE : 0);
        matrix4 m(vec4(s.x, t.x,  0, 0),
                  vec4(s.y, t.y,  0, 0),
                  vec4(s.z, t.z, -1, 0),
                  vec4(  0,   0, bz, 1));
        m.mul(worldmatrix);
        GLOBALPARAM(causticsmatrix, m);
        blendscale *= 0.5f;
        blendoffset = 0;
    }
    else
    {
        GLOBALPARAM(causticsS, s);
        GLOBALPARAM(causticsT, t);
    }
    GLOBALPARAMF(causticsblend, blendscale*(1-frac), blendscale*frac, blendoffset - causticoffset*blendscale);
}

void rendercaustics(float surface, float syl, float syr)
{
    if(!caustics || !causticscale || !causticmillis) return;
    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    setupcaustics(0, surface);
    SETSHADER(caustics);
    gle::defvertex(2);
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(1, -1);
    gle::attribf(-1, -1);
    gle::attribf(1, syr);
    gle::attribf(-1, syl);
    gle::end();
}

void renderdepthfog(int mat, float surface)
{
    glDepthFunc(GL_NOTEQUAL);
    glDepthMask(GL_FALSE);
    glDepthRange(1, 1);

    glEnable(GL_BLEND);

    glActiveTexture_(GL_TEXTURE9);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    glActiveTexture_(GL_TEXTURE0);

    vec p[4] =
    {
        invcamprojmatrix.perspectivetransform(vec(-1, -1, -1)),
        invcamprojmatrix.perspectivetransform(vec(-1, 1, -1)),
        invcamprojmatrix.perspectivetransform(vec(1, -1, -1)),
        invcamprojmatrix.perspectivetransform(vec(1, 1, -1))
    };
    float bz = surface + camera1->o.z + (isliquid(mat&MATF_VOLUME) ? VOLUME_OFFSET : 0.f),
          syl = p[1].z > p[0].z ? 2*(bz - p[0].z)/(p[1].z - p[0].z) - 1 : 1,
          syr = p[3].z > p[2].z ? 2*(bz - p[2].z)/(p[3].z - p[2].z) - 1 : 1;

    if((mat&MATF_VOLUME) == MAT_WATER)
    {
        const bvec &deepcolor = getwaterdeepcolour(mat);
        int deep = getwaterdeep(mat);
        GLOBALPARAMF(fogdeepcolor, deepcolor.x*ldrscaleb, deepcolor.y*ldrscaleb, deepcolor.z*ldrscaleb);
        vec deepfade = getwaterdeepfade(mat).tocolor().mul(deep);
        GLOBALPARAMF(fogdeepfade,
            deepfade.x ? calcfogdensity(deepfade.x) : -1e4f,
            deepfade.y ? calcfogdensity(deepfade.y) : -1e4f,
            deepfade.z ? calcfogdensity(deepfade.z) : -1e4f,
            deep ? calcfogdensity(deep) : -1e4f);

        rendercaustics(surface, syl, syr);
    }
    else if((mat&MATF_VOLUME) == MAT_VOLFOG)
    {
        const bvec &deepcolor = getvolfogdeepcolour(mat);
        int deep = getvolfogdeep(mat);
        GLOBALPARAMF(fogdeepcolor, deepcolor.x*ldrscaleb, deepcolor.y*ldrscaleb, deepcolor.z*ldrscaleb);
        vec deepfade = getvolfogdeepfade(mat).tocolor().mul(deep);
        GLOBALPARAMF(fogdeepfade,
            deepfade.x ? calcfogdensity(deepfade.x) : -1e4f,
            deepfade.y ? calcfogdensity(deepfade.y) : -1e4f,
            deepfade.z ? calcfogdensity(deepfade.z) : -1e4f,
            deep ? calcfogdensity(deep) : -1e4f);
    }
    else
    {
        GLOBALPARAMF(fogdeepcolor, 0, 0, 0);
        GLOBALPARAMF(fogdeepfade, -1e4f, -1e4f, -1e4f, -1e4f);
    }

    GLOBALPARAMF(fogheight, bz);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SETSHADER(depthfog);
    gle::defvertex(3);
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(1, -1, 1);
    gle::attribf(-1, -1, 1);
    gle::attribf(1, syr, 1);
    gle::attribf(-1, syl, 1);
    gle::end();

    glDisable(GL_BLEND);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDepthRange(0, 1);
}

/* vertex water */
VAR(IDF_WORLD, watersubdiv, 0, 2, 3);
VAR(IDF_WORLD, waterlod, 0, 1, 3);

static int wx1, wy1, wx2, wy2, wz, wsize, wsubdiv;
static float whoffset, whphase;

static inline float vertwangle(int v1, int v2)
{
    static const float whscale = 59.0f/23.0f/(2*M_PI);
    v1 &= wsize-1;
    v2 &= wsize-1;
    return v1*v2*whscale+whoffset;
}

static inline float vertwphase(float angle)
{
    float s = angle - int(angle) - 0.5f;
    s *= 8 - fabs(s)*16;
    return VOLUME_AMPLITUDE*s-VOLUME_OFFSET;
}

static inline void vertw(int v1, int v2, int v3)
{
    float h = vertwphase(vertwangle(v1, v2));
    gle::attribf(v1, v2, v3+h);
}

static inline void vertwq(float v1, float v2, float v3)
{
    gle::attribf(v1, v2, v3+whphase);
}

static inline void vertwn(float v1, float v2, float v3, float offset)
{
    gle::attribf(v1, v2, v3-offset);
}

struct waterstrip
{
    int x1, y1, x2, y2, z;
    ushort size, subdiv;

    int numverts() const { return 2*((y2-y1)/subdiv + 1)*((x2-x1)/subdiv); }

    void save()
    {
        x1 = wx1;
        y1 = wy1;
        x2 = wx2;
        y2 = wy2;
        z = wz;
        size = wsize;
        subdiv = wsubdiv;
    }

    void restore()
    {
        wx1 = x1;
        wy1 = y1;
        wx2 = x2;
        wy2 = y2;
        wz = z;
        wsize = size;
        wsubdiv = subdiv;
    }
};
vector<waterstrip> waterstrips;

void flushwaterstrips()
{
    if(gle::attribbuf.length()) xtraverts += gle::end();
    gle::defvertex();
    int numverts = 0;
    loopv(waterstrips) numverts += waterstrips[i].numverts();
    gle::begin(GL_TRIANGLE_STRIP, numverts);
    loopv(waterstrips)
    {
        waterstrips[i].restore();
        for(int x = wx1; x < wx2; x += wsubdiv)
        {
            for(int y = wy1; y <= wy2; y += wsubdiv)
            {
                vertw(x,         y, wz);
                vertw(x+wsubdiv, y, wz);
            }
            x += wsubdiv;
            if(x >= wx2) break;
            for(int y = wy2; y >= wy1; y -= wsubdiv)
            {
                vertw(x,         y, wz);
                vertw(x+wsubdiv, y, wz);
            }
        }
        gle::multidraw();
    }
    waterstrips.setsize(0);
    wsize = 0;
    xtraverts += gle::end();
}

void flushwater(int mat = MAT_WATER, bool force = true)
{
    if(wsize)
    {
        if(wsubdiv >= wsize)
        {
            if(gle::attribbuf.empty()) { gle::defvertex(); gle::begin(GL_QUADS); }
            vertwq(wx1, wy1, wz);
            vertwq(wx2, wy1, wz);
            vertwq(wx2, wy2, wz);
            vertwq(wx1, wy2, wz);
        }
        else waterstrips.add().save();
        wsize = 0;
    }

    if(force)
    {
        if(gle::attribbuf.length()) xtraverts += gle::end();
        if(waterstrips.length()) flushwaterstrips();
    }
}

void rendervertwater(int subdiv, int xo, int yo, int z, int size, int mat)
{
    if(wsize == size && wsubdiv == subdiv && wz == z)
    {
        if(wx2 == xo)
        {
            if(wy1 == yo && wy2 == yo + size) { wx2 += size; return; }
        }
        else if(wy2 == yo && wx1 == xo && wx2 == xo + size) { wy2 += size; return; }
    }

    flushwater(mat, false);

    wx1 = xo;
    wy1 = yo;
    wx2 = xo + size,
    wy2 = yo + size;
    wz = z;
    wsize = size;
    wsubdiv = subdiv;

    ASSERT((wx1 & (subdiv - 1)) == 0);
    ASSERT((wy1 & (subdiv - 1)) == 0);
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

int rendervolumelod(int x, int y, int z, int size, int mat)
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
            subdiv1 = rendervolumelod(x, y, z, childsize, mat),
            subdiv2 = rendervolumelod(x + childsize, y, z, childsize, mat),
            subdiv3 = rendervolumelod(x + childsize, y + childsize, z, childsize, mat),
            subdiv4 = rendervolumelod(x, y + childsize, z, childsize, mat),
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

void renderflatvolume(int x, int y, int z, int rsize, int csize, float offset, int mat = MAT_AIR)
{
    if(gle::attribbuf.empty()) { gle::defvertex(); gle::begin(GL_QUADS); }
    vertwn(x,       y,       z, offset);
    vertwn(x+rsize, y,       z, offset);
    vertwn(x+rsize, y+csize, z, offset);
    vertwn(x,       y+csize, z, offset);
}

VARF(IDF_WORLD, vertwater, 0, 1, 1, if(!(identflags&IDF_WORLD)) allchanged());

static inline void rendervolume(const materialsurface &m, int mat = MAT_WATER)
{
    if(!vertwater || drawtex == DRAWTEX_MINIMAP) renderflatvolume(m.o.x, m.o.y, m.o.z, m.rsize, m.csize, VOLUME_OFFSET, mat);
    else if(rendervolumelod(m.o.x, m.o.y, m.o.z, m.csize, mat) >= int(m.csize) * 2)
        rendervertwater(m.csize, m.o.x, m.o.y, m.o.z, m.csize, mat);
}

#define WATERVARS(type, name) \
    CVAR0(IDF_WORLD, name##colour##type, 0x01212C); \
    CVAR0(IDF_WORLD, name##deepcolour##type, 0x010A10); \
    CVAR0(IDF_WORLD, name##deepfade##type, 0x60BFFF); \
    CVAR0(IDF_WORLD, name##refractcolour##type, 0xFFFFFF); \
    VAR(IDF_WORLD, name##fog##type, 0, 30, 10000); \
    VAR(IDF_WORLD, name##deep##type, 0, 50, 10000); \
    VAR(IDF_WORLD, name##spec##type, 0, 150, 200); \
    FVAR(IDF_WORLD, name##refract##type, 0, 0.1f, 1e3f); \
    CVAR(IDF_WORLD, name##fallcolour##type, 0); \
    CVAR(IDF_WORLD, name##fallrefractcolour##type, 0); \
    VAR(IDF_WORLD, name##fallspec##type, 0, 150, 200); \
    FVAR(IDF_WORLD, name##fallrefract##type, 0, 0.1f, 1e3f); \
    VAR(IDF_WORLD, name##reflectstep##type, 1, 32, 10000); \
    FVAR(IDF_WORLD, name##fallscrollx##type, FVAR_MIN, 0, FVAR_MAX); \
    FVAR(IDF_WORLD, name##fallscrolly##type, FVAR_MIN, -5, FVAR_MAX);

WATERVARS(, water)
WATERVARS(, water2)
WATERVARS(, water3)
WATERVARS(, water4)
WATERVARS(alt, water)
WATERVARS(alt, water2)
WATERVARS(alt, water3)
WATERVARS(alt, water4)

GETMATIDXVAR(water, colour, const bvec &)
GETMATIDXVAR(water, deepcolour, const bvec &)
GETMATIDXVAR(water, deepfade, const bvec &)
GETMATIDXVAR(water, refractcolour, const bvec &)
GETMATIDXVAR(water, fallcolour, const bvec &)
GETMATIDXVAR(water, fallrefractcolour, const bvec &)
GETMATIDXVAR(water, fog, int)
GETMATIDXVAR(water, deep, int)
GETMATIDXVAR(water, spec, int)
GETMATIDXVAR(water, refract, float)
GETMATIDXVAR(water, fallspec, int)
GETMATIDXVAR(water, fallrefract, float)
GETMATIDXVAR(water, reflectstep, int)
GETMATIDXVAR(water, fallscrollx, float)
GETMATIDXVAR(water, fallscrolly, float)

VARF(IDF_PERSIST, waterreflect, 0, 1, 1, { preloadwatershaders(); });
VARF(IDF_PERSIST, waterenvmap, 0, 1, 1, { preloadwatershaders(); });
VARF(IDF_PERSIST, waterfallenv, 0, 1, 1, preloadwatershaders());

#define LAVAVARS(type, name) \
    CVAR0(IDF_WORLD, name##colour##type, 0xFF4000); \
    VAR(IDF_WORLD, name##fog##type, 0, 50, 10000); \
    FVAR(IDF_WORLD, name##glowmin##type, 0, 0.25f, 2); \
    FVAR(IDF_WORLD, name##glowmax##type, 0, 1.0f, 2); \
    VAR(IDF_WORLD, name##spec##type, 0, 25, 200); \
    FVAR(IDF_WORLD, name##scrollx##type, FVAR_MIN, 1, FVAR_MAX); \
    FVAR(IDF_WORLD, name##scrolly##type, FVAR_MIN, 1, FVAR_MAX); \
    FVAR(IDF_WORLD, name##fallscrollx##type, FVAR_MIN, 0, FVAR_MAX); \
    FVAR(IDF_WORLD, name##fallscrolly##type, FVAR_MIN, -5, FVAR_MAX);

LAVAVARS(, lava)
LAVAVARS(, lava2)
LAVAVARS(, lava3)
LAVAVARS(, lava4)
LAVAVARS(alt, lava)
LAVAVARS(alt, lava2)
LAVAVARS(alt, lava3)
LAVAVARS(alt, lava4)

GETMATIDXVAR(lava, colour, const bvec &)
GETMATIDXVAR(lava, fog, int)
GETMATIDXVAR(lava, glowmin, float)
GETMATIDXVAR(lava, glowmax, float)
GETMATIDXVAR(lava, spec, int)
GETMATIDXVAR(lava, scrollx, float)
GETMATIDXVAR(lava, scrolly, float)
GETMATIDXVAR(lava, fallscrollx, float)
GETMATIDXVAR(lava, fallscrolly, float)

#define VOLFOGVARS(type, name) \
    CVAR(IDF_WORLD, name##colour##type, 0); \
    CVAR(IDF_WORLD, name##deepcolour##type, 0); \
    CVAR(IDF_WORLD, name##deepfade##type, 0); \
    VAR(IDF_WORLD, name##dist##type, 0, 50, 10000); \
    VAR(IDF_WORLD, name##deep##type, 0, 100, 10000); \
    VAR(IDF_WORLD, name##texture##type, 0, 1, 2); \
    CVAR(IDF_WORLD, name##texcolour##type, 0xFFFFFF); \
    FVAR(IDF_WORLD, name##texblend##type, 0, 1, 1); \
    FVAR(IDF_WORLD, name##scrollx##type, FVAR_MIN, 1, FVAR_MAX); \
    FVAR(IDF_WORLD, name##scrolly##type, FVAR_MIN, 1, FVAR_MAX);

VOLFOGVARS(, volfog)
VOLFOGVARS(, volfog2)
VOLFOGVARS(, volfog3)
VOLFOGVARS(, volfog4)
VOLFOGVARS(alt, volfog)
VOLFOGVARS(alt, volfog2)
VOLFOGVARS(alt, volfog3)
VOLFOGVARS(alt, volfog4)

#define GETVFIDXVAR(name, var, type) \
    type get##name##var(int mat) \
    { \
        switch(mat&MATF_INDEX) \
        { \
            default: case 0: \
                if(checkmapvariant(MPV_ALT)) return name##var##alt.iszero() ? getfogcolour() : name##var##alt; \
                return name##var.iszero() ? getfogcolour() : name##var; \
            case 1: \
                if(checkmapvariant(MPV_ALT)) return name##2##var##alt.iszero() ? getfogcolour() : name##2##var##alt; \
                return name##2##var.iszero() ? getfogcolour() : name##2##var; \
            case 2: \
                if(checkmapvariant(MPV_ALT)) return name##3##var##alt.iszero() ? getfogcolour() : name##3##var##alt; \
                return name##3##var.iszero() ? getfogcolour() : name##3##var; \
            case 3: \
                if(checkmapvariant(MPV_ALT)) return  name##4##var##alt.iszero() ? getfogcolour() : name##4##var##alt; \
                return  name##4##var.iszero() ? getfogcolour() : name##4##var; \
        } \
    }

GETVFIDXVAR(volfog, colour, const bvec &);
GETVFIDXVAR(volfog, deepcolour, const bvec &);
GETVFIDXVAR(volfog, deepfade, const bvec &);
GETVFIDXVAR(volfog, texcolour, const bvec &);

GETMATIDXVAR(volfog, dist, int);
GETMATIDXVAR(volfog, deep, int);
GETMATIDXVAR(volfog, texture, int);
GETMATIDXVAR(volfog, texblend, float);
GETMATIDXVAR(volfog, scrollx, float);
GETMATIDXVAR(volfog, scrolly, float);

void preloadwatershaders(bool force)
{
    static bool needwater = false;
    if(force) needwater = true;
    if(!needwater) return;

    if(caustics && causticscale && causticmillis)
    {
        if(waterreflect) useshaderbyname("waterreflectcaustics");
        else if(waterenvmap) useshaderbyname("waterenvcaustics");
        else useshaderbyname("watercaustics");
    }
    else
    {
        if(waterreflect) useshaderbyname("waterreflect");
        else if(waterenvmap) useshaderbyname("waterenv");
        else useshaderbyname("water");
    }

    useshaderbyname("underwater");

    if(waterfallenv) useshaderbyname("waterfallenv");
    useshaderbyname("waterfall");
}

static float wfwave = 0.0f;

static void rendervolumefall(const materialsurface &m, float offset)
{
    if(gle::attribbuf.empty())
    {
        gle::defvertex();
        gle::defnormal(4, GL_BYTE);
        gle::begin(GL_QUADS);
    }
    float x = m.o.x, y = m.o.y, zmin = m.o.z, zmax = zmin;
    if(m.ends&1) zmin += -VOLUME_OFFSET-VOLUME_AMPLITUDE;
    if(m.ends&2) zmax += wfwave;
    int csize = m.csize, rsize = m.rsize;
    switch(m.orient)
    {
    #define GENFACEORIENT(orient, v0, v1, v2, v3) \
        case orient: v0 v1 v2 v3 break;
    #define GENFACEVERT(orient, vert, mx,my,mz, sx,sy,sz) \
        { \
            gle::attribf(mx sx, my sy, mz sz); \
            gle::attrib(matnormals[orient]); \
        }
        GENFACEVERTSXY(x, x, y, y, zmin, zmax, /**/, + csize, /**/, + rsize, + offset, - offset)
    #undef GENFACEORIENT
    #undef GENFACEVERT
    }
}

void renderlava()
{
    loopk(4)
    {
        if(lavasurfs[k].empty() && (drawtex == DRAWTEX_MINIMAP || lavafallsurfs[k].empty())) continue;

        MatSlot &lslot = lookupmaterialslot(MAT_LAVA+k);

        SETSHADER(lava);
        float t = lastmillis/2000.0f;
        t -= floor(t);
        t = 1.0f - 2*fabs(t-0.5f);
        t = 0.5f + 0.5f*t;
        float glowmin = getlavaglowmin(k), glowmax = getlavaglowmax(k);
        int spec = getlavaspec(k);
        LOCALPARAMF(lavaglow, 0.5f*(glowmin + (glowmax-glowmin)*t));
        LOCALPARAMF(lavaspec, spec/100.0f);

        if(lavasurfs[k].length())
        {
            Texture *tex = lslot.sts.inrange(0) ? lslot.sts[0].t: notexture;
            float xscale = TEX_SCALE/(tex->xs*lslot.scale), yscale = TEX_SCALE/(tex->ys*lslot.scale),
                  scroll = lastmillis/1000.0f, xscroll = getlavascrollx(k)*scroll, yscroll = getlavascrolly(k)*scroll;
            LOCALPARAMF(lavatexgen, xscale, yscale, xscroll, yscroll);

            whoffset = fmod(float(lastmillis/2000.0f/(2*M_PI)), 1.0f);
            whphase = vertwphase(whoffset);

            glBindTexture(GL_TEXTURE_2D, tex->id);
            glActiveTexture_(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, lslot.sts.inrange(1) ? lslot.sts[1].t->id : notexture->id);
            glActiveTexture_(GL_TEXTURE0);

            gle::normal(vec(0, 0, 1));

            vector<materialsurface> &surfs = lavasurfs[k];
            loopv(surfs) rendervolume(surfs[i], MAT_LAVA);
            flushwater(MAT_LAVA);
        }

        if(drawtex != DRAWTEX_MINIMAP && lavafallsurfs[k].length())
        {
            Texture *tex = lslot.sts.inrange(2) ? lslot.sts[2].t : (lslot.sts.inrange(0) ? lslot.sts[0].t : notexture);
            float angle = fmod(float(lastmillis/2000.0f/(2*M_PI)), 1.0f), s = angle - int(angle) - 0.5f;
            s *= 8 - fabs(s)*16;
            wfwave = vertwater ? VOLUME_AMPLITUDE*s-VOLUME_OFFSET : -VOLUME_OFFSET;
            float xscale = TEX_SCALE/(tex->xs*lslot.scale), yscale = TEX_SCALE/(tex->ys*lslot.scale),
                  scroll = lastmillis/1000.0f, xscroll = getlavafallscrollx(k)*scroll, yscroll = getlavafallscrolly(k)*scroll;
            LOCALPARAMF(lavatexgen, xscale, yscale, xscroll, yscroll);

            glBindTexture(GL_TEXTURE_2D, tex->id);
            glActiveTexture_(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, lslot.sts.inrange(2) ? (lslot.sts.inrange(3) ? lslot.sts[3].t->id : notexture->id) : (lslot.sts.inrange(1) ? lslot.sts[1].t->id : notexture->id));
            glActiveTexture_(GL_TEXTURE0);

            vector<materialsurface> &surfs = lavafallsurfs[k];
            loopv(surfs)
            {
                materialsurface &m = surfs[i];
                rendervolumefall(m, VOLUME_INSET);
            }
            xtraverts += gle::end();
        }
    }
}

void rendervolumefalls()
{
    loopk(4)
    {
        vector<materialsurface> &surfs = waterfallsurfs[k];
        if(surfs.empty()) continue;

        MatSlot &wslot = lookupmaterialslot(MAT_WATER+k);

        Texture *tex = wslot.sts.inrange(2) ? wslot.sts[2].t : (wslot.sts.inrange(0) ? wslot.sts[0].t : notexture);
        float angle = fmod(float(lastmillis/600.0f/(2*M_PI)), 1.0f),
              s = angle - int(angle) - 0.5f;
        s *= 8 - fabs(s)*16;
        wfwave = vertwater ? VOLUME_AMPLITUDE*s-VOLUME_OFFSET : -VOLUME_OFFSET;

        float xscale = TEX_SCALE/(tex->xs*wslot.scale), yscale = TEX_SCALE/(tex->ys*wslot.scale),
              scroll = lastmillis/1000.0f, xscroll = getwaterfallscrollx(k)*scroll, yscroll = getwaterfallscrolly(k)*scroll;
        GLOBALPARAMF(waterfalltexgen, xscale, yscale, xscroll, yscroll);

        bvec color = getwaterfallcolour(k), refractcolor = getwaterfallrefractcolour(k);
        if(color.iszero()) color = getwatercolour(k);
        if(refractcolor.iszero()) refractcolor = getwaterrefractcolour(k);
        float colorscale = (0.5f/255), refractscale = colorscale/ldrscale;
        float refract = getwaterfallrefract(k);
        int spec = getwaterfallspec(k);
        GLOBALPARAMF(waterfallcolor, color.x*colorscale, color.y*colorscale, color.z*colorscale);
        GLOBALPARAMF(waterfallrefract, refractcolor.x*refractscale, refractcolor.y*refractscale, refractcolor.z*refractscale, refract*viewh);
        GLOBALPARAMF(waterfallspec, spec/100.0f);

        if(waterfallenv) SETSHADER(waterfallenv);
        else SETSHADER(waterfall);

        glBindTexture(GL_TEXTURE_2D, tex->id);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, wslot.sts.inrange(2) ? (wslot.sts.inrange(3) ? wslot.sts[3].t->id : notexture->id) : (wslot.sts.inrange(1) ? wslot.sts[1].t->id : notexture->id));
        if(waterfallenv)
        {
            glActiveTexture_(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_CUBE_MAP, lookupenvmap(wslot));
        }
        glActiveTexture_(GL_TEXTURE0);

        loopv(surfs)
        {
            materialsurface &m = surfs[i];
            rendervolumefall(m, VOLUME_INSET);
        }
        xtraverts += gle::end();
    }
}

void renderwater()
{
    loopk(4)
    {
        vector<materialsurface> &surfs = watersurfs[k];
        if(surfs.empty()) continue;

        MatSlot &wslot = lookupmaterialslot(MAT_WATER+k);

        Texture *tex = wslot.sts.inrange(0) ? wslot.sts[0].t: notexture;
        float xscale = TEX_SCALE/(tex->xs*wslot.scale), yscale = TEX_SCALE/(tex->ys*wslot.scale);
        GLOBALPARAMF(watertexgen, xscale, yscale);

        whoffset = fmod(float(lastmillis/600.0f/(2*M_PI)), 1.0f);
        whphase = vertwphase(whoffset);

        glBindTexture(GL_TEXTURE_2D, tex->id);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, wslot.sts.inrange(1) ? wslot.sts[1].t->id : notexture->id);
        if(caustics && causticscale && causticmillis) setupcaustics(2);
        if(waterenvmap && !waterreflect && drawtex != DRAWTEX_MINIMAP)
        {
            glActiveTexture_(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_CUBE_MAP, lookupenvmap(wslot));
        }
        glActiveTexture_(GL_TEXTURE0);

        float colorscale = 0.5f/255, refractscale = colorscale/ldrscale, reflectscale = 0.5f/ldrscale;
        const bvec &color = getwatercolour(k);
        const bvec &deepcolor = getwaterdeepcolour(k);
        const bvec &refractcolor = getwaterrefractcolour(k);
        int fog = getwaterfog(k), deep = getwaterdeep(k), spec = getwaterspec(k);
        float refract = getwaterrefract(k);
        GLOBALPARAMF(watercolor, color.x*colorscale, color.y*colorscale, color.z*colorscale);
        GLOBALPARAMF(waterdeepcolor, deepcolor.x*colorscale, deepcolor.y*colorscale, deepcolor.z*colorscale);
        float fogdensity = fog ? calcfogdensity(fog) : -1e4f;
        GLOBALPARAMF(waterfog, fogdensity);
        vec deepfade = getwaterdeepfade(k).tocolor().mul(deep);
        GLOBALPARAMF(waterdeepfade,
            deepfade.x ? calcfogdensity(deepfade.x) : -1e4f,
            deepfade.y ? calcfogdensity(deepfade.y) : -1e4f,
            deepfade.z ? calcfogdensity(deepfade.z) : -1e4f,
            deep ? calcfogdensity(deep) : -1e4f);
        GLOBALPARAMF(waterspec, spec/100.0f);
        GLOBALPARAMF(waterreflect, reflectscale, reflectscale, reflectscale, getwaterreflectstep(k));
        GLOBALPARAMF(waterrefract, refractcolor.x*refractscale, refractcolor.y*refractscale, refractcolor.z*refractscale, refract*viewh);

        #define SETWATERSHADER(which, name) \
        do { \
            static Shader *name##shader = NULL; \
            if(!name##shader) name##shader = lookupshaderbyname(#name); \
            which##shader = name##shader; \
        } while(0)

        Shader *aboveshader = NULL;
        if(drawtex == DRAWTEX_MINIMAP) SETWATERSHADER(above, minimapvol);
        else if(caustics && causticscale && causticmillis)
        {
            if(waterreflect) SETWATERSHADER(above, waterreflectcaustics);
            else if(waterenvmap) SETWATERSHADER(above, waterenvcaustics);
            else SETWATERSHADER(above, watercaustics);
        }
        else
        {
            if(waterreflect) SETWATERSHADER(above, waterreflect);
            else if(waterenvmap) SETWATERSHADER(above, waterenv);
            else SETWATERSHADER(above, water);
        }

        Shader *belowshader = NULL;
        if(drawtex != DRAWTEX_MINIMAP) SETWATERSHADER(below, underwater);

        if(aboveshader)
        {
            aboveshader->set();
            loopv(surfs)
            {
                materialsurface &m = surfs[i];
                if(camera1->o.z < m.o.z - VOLUME_OFFSET) continue;
                rendervolume(m);
            }
            flushwater();
        }

        if(belowshader)
        {
            belowshader->set();
            loopv(surfs)
            {
                materialsurface &m = surfs[i];
                if(camera1->o.z >= m.o.z - VOLUME_OFFSET) continue;
                rendervolume(m);
            }
            flushwater();
        }
    }
}

void rendervolfog()
{
    loopk(4)
    {
        vector<materialsurface> &surfs = volfogsurfs[k];
        if(surfs.empty()) continue;

        int textured = getvolfogtexture(k);
        if(textured)
        {
            MatSlot &fslot = lookupmaterialslot(MAT_VOLFOG+k);
            Texture *tex = fslot.sts.inrange(0) ? fslot.sts[0].t: notexture;
            float xscale = TEX_SCALE/(tex->xs*fslot.scale), yscale = TEX_SCALE/(tex->ys*fslot.scale),
                  scroll = lastmillis/1000.0f, xscroll = getvolfogscrollx(k)*scroll, yscroll = getvolfogscrolly(k)*scroll;
            GLOBALPARAMF(volfogtexgen, xscale, yscale, xscroll, yscroll);
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }

        float colorscale = 0.5f/255;
        const bvec &color = getvolfogcolour(k);
        const bvec &deepcolor = getvolfogdeepcolour(k);
        int fog = getvolfogdist(k), deep = getvolfogdeep(k);
        GLOBALPARAMF(volfogcolor, color.x*colorscale, color.y*colorscale, color.z*colorscale);
        GLOBALPARAMF(volfogdeepcolor, deepcolor.x*colorscale, deepcolor.y*colorscale, deepcolor.z*colorscale);
        if(textured)
        {
            const bvec &texcolor = getvolfogtexcolour(k);
            float texblend = getvolfogtexblend(k);
            GLOBALPARAMF(volfogtexcolor, texcolor.x*colorscale, texcolor.y*colorscale, texcolor.z*colorscale, texblend);
        }

        float fogdensity = fog ? calcfogdensity(fog) : -1e4f;
        GLOBALPARAMF(volfogdist, fogdensity);
        vec deepfade = getvolfogdeepfade(k).tocolor().mul(deep);
        GLOBALPARAMF(volfogdeepfade,
            deepfade.x ? calcfogdensity(deepfade.x) : -1e4f,
            deepfade.y ? calcfogdensity(deepfade.y) : -1e4f,
            deepfade.z ? calcfogdensity(deepfade.z) : -1e4f,
            deep ? calcfogdensity(deep) : -1e4f);

        #define SETVOLFOGSHADER(which, name) \
        do { \
            static Shader *name##shader = NULL; \
            if(!name##shader) name##shader = lookupshaderbyname(#name); \
            which##shader = name##shader; \
        } while(0)

        Shader *aboveshader = NULL;
        if(drawtex != DRAWTEX_MINIMAP) switch(textured)
        {
            case 2: SETVOLFOGSHADER(above, volfogtex); break;
            case 1: SETVOLFOGSHADER(above, volfogtexsmp); break;
            default: SETVOLFOGSHADER(above, volfog); break;
        }
        else SETVOLFOGSHADER(above, minimapvol);

        Shader *belowshader = NULL;
        if(drawtex != DRAWTEX_MINIMAP) switch(textured)
        {
            case 2: SETVOLFOGSHADER(below, undervolfogtex); break;
            case 1: SETVOLFOGSHADER(below, undervolfogtexsmp); break;
            default: SETVOLFOGSHADER(below, undervolfog); break;
        }

        if(aboveshader)
        {
            aboveshader->set();
            loopv(surfs)
            {
                materialsurface &m = surfs[i];
                if(camera1->o.z < m.o.z) continue;
                renderflatvolume(m.o.x, m.o.y, m.o.z, m.rsize, m.csize, 0.f);
            }
            if(gle::attribbuf.length()) xtraverts += gle::end();
        }

        if(belowshader)
        {
            belowshader->set();
            loopv(surfs)
            {
                materialsurface &m = surfs[i];
                if(camera1->o.z >= m.o.z) continue;
                renderflatvolume(m.o.x, m.o.y, m.o.z, m.rsize, m.csize, 0.f);
            }
            if(gle::attribbuf.length()) xtraverts += gle::end();
        }
    }
}

