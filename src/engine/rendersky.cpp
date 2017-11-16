#include "engine.h"

Texture *sky[6] = { 0, 0, 0, 0, 0, 0 }, *clouds[6] = { 0, 0, 0, 0, 0, 0 };

void loadsky(const char *basename, Texture *texs[6])
{
    const char *wildcard = strchr(basename, '*');
    loopi(6)
    {
        const char *side = cubemapsides[i].name;
        string name;
        copystring(name, basename);
        if(wildcard)
        {
            char *chop = strchr(name, '*');
            if(chop) { *chop = '\0'; concatstring(name, side); concatstring(name, wildcard+1); }
        }
        else
        {
            defformatstring(ext, "_%s", side);
            concatstring(name, ext);
        }
        if((texs[i] = textureload(name, 3, true, false)) == notexture)
        {
            conoutf("\frCould not load side %s of sky texture %s", side, basename);
        }
    }
}

Texture *cloudoverlay = NULL, *envoverlay = NULL;

Texture *loadskyoverlay(const char *basename)
{
    Texture *t = textureload(basename, 0, true, false);
    if(t == notexture) conoutf("\frCould not load sky overlay texture %s", basename);
    return t;
}

#define DAYNIGHTVARS(name, type) \
    CVAR1(IDF_WORLD, ambient##name, 0x191919); \
    FVAR(IDF_WORLD, ambientscale##name, 0, 1, 16); \
    CVAR1(IDF_WORLD, skylight##name, 0); \
    FVAR(IDF_WORLD, skylightscale##name, 0, 1, 16); \
    VAR(IDF_WORLD, fog##name, 16, 4000, 1000024); \
    CVAR0(IDF_WORLD, fogcolour##name, 0x8099B3); \
    CVAR(IDF_WORLD, skybgcolour##name, 0x000000); \
    SVARF(IDF_WORLD, skybox##name, "", { if(skybox##name[0] && checkmapvariant(type)) loadsky(skybox##name, sky); }); \
    CVAR(IDF_WORLD, skycolour##name, 0xFFFFFF); \
    FVAR(IDF_WORLD, skyblend##name, 0, 1.0f, 1); \
    FVAR(IDF_WORLD, skyoverbright##name, 1, 2, 16); \
    FVAR(IDF_WORLD, skyoverbrightmin##name, 0, 1, 16); \
    FVAR(IDF_WORLD, skyoverbrightthreshold##name, 0, 0.7f, 1); \
    FVAR(IDF_WORLD, spinsky##name, -720, 0, 720); \
    VAR(IDF_WORLD, yawsky##name, 0, 0, 360); \
    SVARF(IDF_WORLD, cloudbox##name, "", { if(cloudbox##name[0] && checkmapvariant(type)) loadsky(cloudbox##name, clouds); }); \
    CVAR(IDF_WORLD, cloudcolour##name, 0xFFFFFF); \
    FVAR(IDF_WORLD, cloudblend##name, 0, 1.0f, 1); \
    FVAR(IDF_WORLD, spinclouds##name, -720, 0, 720); \
    VAR(IDF_WORLD, yawclouds##name, 0, 0, 360); \
    FVAR(IDF_WORLD, cloudclip##name, 0, 0.5f, 1); \
    SVARF(IDF_WORLD, cloudlayer##name, "", { if(cloudlayer##name[0] && checkmapvariant(type)) cloudoverlay = loadskyoverlay(cloudlayer##name); }); \
    CVAR(IDF_WORLD, cloudlayercolour##name, 0xFFFFFF); \
    FVAR(IDF_WORLD, cloudlayerblend##name, 0, 1.0f, 1); \
    FVAR(IDF_WORLD, cloudoffsetx##name, 0, 0, 1); \
    FVAR(IDF_WORLD, cloudoffsety##name, 0, 0, 1); \
    FVAR(IDF_WORLD, cloudscrollx##name, -16, 0, 16); \
    FVAR(IDF_WORLD, cloudscrolly##name, -16, 0, 16); \
    FVAR(IDF_WORLD, cloudscale##name, FVAR_NONZERO, 1, 64); \
    FVAR(IDF_WORLD, spincloudlayer##name, -720, 0, 720); \
    VAR(IDF_WORLD, yawcloudlayer##name, 0, 0, 360); \
    FVAR(IDF_WORLD, cloudheight##name, -1, 0.2f, 1); \
    FVAR(IDF_WORLD, cloudfade##name, 0, 0.2f, 1); \
    VAR(IDF_WORLD, cloudsubdiv##name, 4, 16, 64); \
    SVARF(IDF_WORLD, envlayer##name, "", { if(envlayer##name[0] && checkmapvariant(type)) envoverlay = loadskyoverlay(envlayer##name); }); \
    CVAR(IDF_WORLD, envlayercolour##name, 0xFFFFFF); \
    FVAR(IDF_WORLD, envlayerblend##name, 0, 1.0f, 1); \
    FVAR(IDF_WORLD, envoffsetx##name, 0, 0, 1); \
    FVAR(IDF_WORLD, envoffsety##name, 0, 0, 1); \
    FVAR(IDF_WORLD, envscrollx##name, -16, 0, 16); \
    FVAR(IDF_WORLD, envscrolly##name, -16, 0, 16); \
    FVAR(IDF_WORLD, envscale##name, FVAR_NONZERO, 1, 64); \
    FVAR(IDF_WORLD, spinenvlayer##name, -720, 0, 720); \
    VAR(IDF_WORLD, yawenvlayer##name, 0, 0, 360); \
    FVAR(IDF_WORLD, envheight##name, -1, 0.2f, 1); \
    FVAR(IDF_WORLD, envfade##name, 0, 0.2f, 1); \
    VAR(IDF_WORLD, envsubdiv##name, 4, 16, 64); \
    VAR(IDF_WORLD, atmo##name, 0, 0, 2); \
    FVAR(IDF_WORLD, atmoplanetsize##name, FVAR_NONZERO, 8, FVAR_MAX); \
    FVAR(IDF_WORLD, atmoheight##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_WORLD, atmobright##name, 0, 4, 16); \
    CVAR1(IDF_WORLD, atmolight##name, 0); \
    FVAR(IDF_WORLD, atmolightscale##name, 0, 1, 16); \
    FVAR(IDF_WORLD, atmodisksize##name, 0, 1, 1000); \
    FVAR(IDF_WORLD, atmodiskbright##name, 0, 1, 16); \
    FVAR(IDF_WORLD, atmohaze##name, 0, 0.03f, 1); \
    CVAR0(IDF_WORLD, atmohazefade##name, 0xAEACA9); \
    FVAR(IDF_WORLD, atmohazefadescale##name, 0, 1, 1); \
    FVAR(IDF_WORLD, atmoclarity##name, 0, 0.2f, 10); \
    FVAR(IDF_WORLD, atmodensity##name, FVAR_NONZERO, 0.99f, 100); \
    FVAR(IDF_WORLD, atmoblend##name, 0, 1, 1); \
    FVAR(IDF_WORLD, fogdomeheight##name, -1, -0.5f, 1); \
    FVAR(IDF_WORLD, fogdomemin##name, 0, 0, 1); \
    FVAR(IDF_WORLD, fogdomemax##name, 0, 0, 1); \
    VAR(IDF_WORLD, fogdomecap##name, 0, 1, 1); \
    FVAR(IDF_WORLD, fogdomeclip##name, 0, 1, 1); \
    CVAR(IDF_WORLD, fogdomecolour##name, 0xFFFFFF); \
    VAR(IDF_WORLD, fogdomeclouds##name, 0, 1, 1); \
    VAR(IDF_WORLD, skytexture##name, 0, 0, 1); \
    VARF(IDF_WORLD, skyshadow##name, 0, 0, 1, if(checkmapvariant(type)) clearshadowcache());

DAYNIGHTVARS(, MPV_DAY);
DAYNIGHTVARS(night, MPV_NIGHT);

#define GETDAYNIGHT(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_NIGHT)) return name##night; \
        return name; \
    }

GETDAYNIGHT(ambient, bvec &);
GETDAYNIGHT(ambientscale, float);
GETDAYNIGHT(skylight, bvec &);
GETDAYNIGHT(skylightscale, float);
GETDAYNIGHT(fog, int);
GETDAYNIGHT(fogcolour, bvec &);
GETDAYNIGHT(skybgcolour, bvec &);
GETDAYNIGHT(skybox, const char *);
GETDAYNIGHT(skycolour, bvec &);
GETDAYNIGHT(skyblend, float);
GETDAYNIGHT(skyoverbright, float);
GETDAYNIGHT(skyoverbrightmin, float);
GETDAYNIGHT(skyoverbrightthreshold, float);
GETDAYNIGHT(spinsky, float);
GETDAYNIGHT(yawsky, int);
GETDAYNIGHT(cloudbox, const char *);
GETDAYNIGHT(cloudcolour, bvec &);
GETDAYNIGHT(cloudblend, float);
GETDAYNIGHT(spinclouds, float);
GETDAYNIGHT(yawclouds, int);
GETDAYNIGHT(cloudclip, float);
GETDAYNIGHT(cloudlayer, const char *);
GETDAYNIGHT(cloudlayercolour, bvec &);
GETDAYNIGHT(cloudlayerblend, float);
GETDAYNIGHT(cloudoffsetx, float);
GETDAYNIGHT(cloudoffsety, float);
GETDAYNIGHT(cloudscrollx, float);
GETDAYNIGHT(cloudscrolly, float);
GETDAYNIGHT(cloudscale, float);
GETDAYNIGHT(spincloudlayer, float);
GETDAYNIGHT(yawcloudlayer, int);
GETDAYNIGHT(cloudheight, float);
GETDAYNIGHT(cloudfade, float);
GETDAYNIGHT(cloudsubdiv, int);
GETDAYNIGHT(envlayer, const char *);
GETDAYNIGHT(envlayercolour, bvec &);
GETDAYNIGHT(envlayerblend, float);
GETDAYNIGHT(envoffsetx, float);
GETDAYNIGHT(envoffsety, float);
GETDAYNIGHT(envscrollx, float);
GETDAYNIGHT(envscrolly, float);
GETDAYNIGHT(envscale, float);
GETDAYNIGHT(spinenvlayer, float);
GETDAYNIGHT(yawenvlayer, int);
GETDAYNIGHT(envheight, float);
GETDAYNIGHT(envfade, float);
GETDAYNIGHT(envsubdiv, int);
GETDAYNIGHT(atmo, int);
GETDAYNIGHT(atmoplanetsize, float);
GETDAYNIGHT(atmoheight, float);
GETDAYNIGHT(atmobright, float);
GETDAYNIGHT(atmolight, bvec &);
GETDAYNIGHT(atmolightscale, float);
GETDAYNIGHT(atmodisksize, float);
GETDAYNIGHT(atmodiskbright, float);
GETDAYNIGHT(atmohaze, float);
GETDAYNIGHT(atmohazefade, bvec &);
GETDAYNIGHT(atmohazefadescale, float);
GETDAYNIGHT(atmoclarity, float);
GETDAYNIGHT(atmodensity, float);
GETDAYNIGHT(atmoblend, float);
GETDAYNIGHT(fogdomeheight, float);
GETDAYNIGHT(fogdomemin, float);
GETDAYNIGHT(fogdomemax, float);
GETDAYNIGHT(fogdomecap, int);
GETDAYNIGHT(fogdomeclip, float);
GETDAYNIGHT(fogdomecolour, bvec &);
GETDAYNIGHT(fogdomeclouds, int);
GETDAYNIGHT(skytexture, int);
GETDAYNIGHT(skyshadow, int);

void drawenvboxface(float s0, float t0, int x0, int y0, int z0,
                    float s1, float t1, int x1, int y1, int z1,
                    float s2, float t2, int x2, int y2, int z2,
                    float s3, float t3, int x3, int y3, int z3,
                    Texture *tex)
{
    glBindTexture(GL_TEXTURE_2D, (tex ? tex : notexture)->id);
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x3, y3, z3); gle::attribf(s3, t3);
    gle::attribf(x2, y2, z2); gle::attribf(s2, t2);
    gle::attribf(x0, y0, z0); gle::attribf(s0, t0);
    gle::attribf(x1, y1, z1); gle::attribf(s1, t1);
    xtraverts += gle::end();
}

void drawenvboxbgface(int x0, int y0, int z0,
                      int x1, int y1, int z1,
                      int x2, int y2, int z2,
                      int x3, int y3, int z3)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x3, y3, z3);
    gle::attribf(x2, y2, z2);
    gle::attribf(x0, y0, z0);
    gle::attribf(x1, y1, z1);
    xtraverts += gle::end();
}

void drawenvbox(Texture **sky = NULL, float z1clip = 0.0f, float z2clip = 1.0f, int faces = 0x3F)
{
    if(z1clip >= z2clip) return;

    float v1 = 1-z1clip, v2 = 1-z2clip;
    int w = farplane/2, z1 = int(ceil(2*w*(z1clip-0.5f))), z2 = int(ceil(2*w*(z2clip-0.5f)));

    gle::defvertex();
    gle::deftexcoord0();

    if(faces&0x01)
        drawenvboxface(1.0f, v2,  -w, -w, z2,
                       0.0f, v2,  -w,  w, z2,
                       0.0f, v1,  -w,  w, z1,
                       1.0f, v1,  -w, -w, z1, sky[0]);

    if(faces&0x02)
        drawenvboxface(0.0f, v1, w, -w, z1,
                       1.0f, v1, w,  w, z1,
                       1.0f, v2, w,  w, z2,
                       0.0f, v2, w, -w, z2, sky[1]);

    if(faces&0x04)
        drawenvboxface(0.0f, v1, -w, -w, z1,
                       1.0f, v1,  w, -w, z1,
                       1.0f, v2,  w, -w, z2,
                       0.0f, v2, -w, -w, z2, sky[2]);

    if(faces&0x08)
        drawenvboxface(0.0f, v1,  w,  w, z1,
                       1.0f, v1, -w,  w, z1,
                       1.0f, v2, -w,  w, z2,
                       0.0f, v2,  w,  w, z2, sky[3]);

    if(z1clip <= 0 && faces&0x10)
        drawenvboxface(1.0f, 1.0f, -w,  w,  -w,
                       1.0f, 0.0f,  w,  w,  -w,
                       0.0f, 0.0f,  w, -w,  -w,
                       0.0f, 1.0f, -w, -w,  -w, sky[4]);

    if(z2clip >= 1 && faces&0x20)
        drawenvboxface(1.0f, 1.0f,  w,  w, w,
                       1.0f, 0.0f, -w,  w, w,
                       0.0f, 0.0f, -w, -w, w,
                       0.0f, 1.0f,  w, -w, w, sky[5]);
}

void drawenvboxbg(float z1clip = 0.0f, float z2clip = 1.0f)
{
    if(z1clip >= z2clip) return;

    int w = farplane/2, z1 = int(ceil(2*w*(z1clip-0.5f))), z2 = int(ceil(2*w*(z2clip-0.5f)));

    gle::defvertex();

    drawenvboxbgface(-w, -w, z2, -w,  w, z2, -w,  w, z1, -w, -w, z1);
    drawenvboxbgface( w, -w, z1,  w,  w, z1,  w,  w, z2,  w, -w, z2);
    drawenvboxbgface(-w, -w, z1,  w, -w, z1,  w, -w, z2, -w, -w, z2);
    drawenvboxbgface( w,  w, z1, -w,  w, z1, -w,  w, z2,  w,  w, z2);
    if(z1clip <= 0) drawenvboxbgface(-w,  w, -w,  w,  w, -w,  w, -w, -w, -w, -w, -w);
    if(z2clip >= 1) drawenvboxbgface( w,  w,  w, -w,  w,  w, -w, -w,  w,  w, -w,  w);
}

void drawenvoverlay(Texture *overlay, float height, int subdiv, float fade, float scale, bvec &colour, float blend, float tx = 0, float ty = 0)
{
    int w = farplane/2;
    float z = w*height, tsz = 0.5f*(1-fade)/scale, psz = w*(1-fade);
    glBindTexture(GL_TEXTURE_2D, (overlay ? overlay : notexture)->id);
    vec color = colour.tocolor();
    gle::color(color, blend);
    gle::defvertex();
    gle::deftexcoord0();
    gle::begin(GL_TRIANGLE_FAN);
    loopi(subdiv+1)
    {
        vec p(1, 1, 0);
        p.rotate_around_z((-2.0f*M_PI*i)/subdiv);
        gle::attribf(p.x*psz, p.y*psz, z);
            gle::attribf(tx - p.x*tsz, ty + p.y*tsz);
    }
    xtraverts += gle::end();
    float tsz2 = 0.5f/scale;
    gle::defvertex();
    gle::deftexcoord0();
    gle::defcolor(4);
    gle::begin(GL_TRIANGLE_STRIP);
    loopi(subdiv+1)
    {
        vec p(1, 1, 0);
        p.rotate_around_z((-2.0f*M_PI*i)/subdiv);
        gle::attribf(p.x*psz, p.y*psz, z);
            gle::attribf(tx - p.x*tsz, ty + p.y*tsz);
            gle::attrib(color, blend);
        gle::attribf(p.x*w, p.y*w, z);
            gle::attribf(tx - p.x*tsz2, ty + p.y*tsz2);
            gle::attrib(color, 0.0f);
    }
    xtraverts += gle::end();
}

namespace fogdome
{
    struct vert
    {
        vec pos;
        bvec4 color;

        vert() {}
        vert(const vec &pos, const bvec &fcolor, float alpha) : pos(pos), color(fcolor, uchar(alpha*255))
        {
        }
        vert(const vert &v0, const vert &v1) : pos(vec(v0.pos).add(v1.pos).normalize()), color(v0.color)
        {
            if(v0.pos.z != v1.pos.z) color.a += uchar((v1.color.a - v0.color.a) * (pos.z - v0.pos.z) / (v1.pos.z - v0.pos.z));
        }
    } *verts = NULL;
    GLushort *indices = NULL;
    int numverts = 0, numindices = 0, capindices = 0;
    GLuint vbuf = 0, ebuf = 0;
    bvec lastcolor(0, 0, 0);
    float lastminalpha = 0, lastmaxalpha = 0, lastcapsize = -1, lastclipz = 1;

    void subdivide(int depth, int face);

    void genface(int depth, int i1, int i2, int i3)
    {
        int face = numindices; numindices += 3;
        indices[face]   = i3;
        indices[face+1] = i2;
        indices[face+2] = i1;
        subdivide(depth, face);
    }

    void subdivide(int depth, int face)
    {
        if(depth-- <= 0) return;
        int idx[6];
        loopi(3) idx[i] = indices[face+2-i];
        loopi(3)
        {
            int curvert = numverts++;
            verts[curvert] = vert(verts[idx[i]], verts[idx[(i+1)%3]]); //push on to unit sphere
            idx[3+i] = curvert;
            indices[face+2-i] = curvert;
        }
        subdivide(depth, face);
        loopi(3) genface(depth, idx[i], idx[3+i], idx[3+(i+2)%3]);
    }

    static inline int sortcap(GLushort x, GLushort y)
    {
        const vec &xv = verts[x].pos, &yv = verts[y].pos;
        return xv.y < 0 ? yv.y >= 0 || xv.x < yv.x : yv.y >= 0 && xv.x > yv.x;
    }

    static void init(const bvec &color, float minalpha = 0.0f, float maxalpha = 1.0f, float capsize = -1, float clipz = 1, int hres = 16, int depth = 2)
    {
        const int tris = hres << (2*depth);
        numverts = numindices = capindices = 0;
        verts = new vert[tris+1 + (capsize >= 0 ? 1 : 0)];
        indices = new GLushort[(tris + (capsize >= 0 ? hres<<depth : 0))*3];
        if(clipz >= 1)
        {
            verts[numverts++] = vert(vec(0.0f, 0.0f, 1.0f), color, minalpha); //build initial 'hres' sided pyramid
            loopi(hres) verts[numverts++] = vert(vec(sincos360[(360*i)/hres], 0.0f), color, maxalpha);
            loopi(hres) genface(depth, 0, i+1, 1+(i+1)%hres);
        }
        else if(clipz <= 0)
        {
            loopi(hres<<depth) verts[numverts++] = vert(vec(sincos360[(360*i)/(hres<<depth)], 0.0f), color, maxalpha);
        }
        else
        {
            float clipxy = sqrtf(1 - clipz*clipz);
            const vec2 &scm = sincos360[180/hres];
            loopi(hres)
            {
                const vec2 &sc = sincos360[(360*i)/hres];
                verts[numverts++] = vert(vec(sc.x*clipxy, sc.y*clipxy, clipz), color, minalpha);
                verts[numverts++] = vert(vec(sc.x, sc.y, 0.0f), color, maxalpha);
                verts[numverts++] = vert(vec(sc.x*scm.x - sc.y*scm.y, sc.y*scm.x + sc.x*scm.y, 0.0f), color, maxalpha);
            }
            loopi(hres)
            {
                genface(depth-1, 3*i, 3*i+1, 3*i+2);
                genface(depth-1, 3*i, 3*i+2, 3*((i+1)%hres));
                genface(depth-1, 3*i+2, 3*((i+1)%hres)+1, 3*((i+1)%hres));
            }
        }

        if(capsize >= 0)
        {
            GLushort *cap = &indices[numindices];
            int capverts = 0;
            loopi(numverts) if(!verts[i].pos.z) cap[capverts++] = i;
            verts[numverts++] = vert(vec(0.0f, 0.0f, -capsize), color, maxalpha);
            quicksort(cap, capverts, sortcap);
            loopi(capverts)
            {
                int n = capverts-1-i;
                cap[n*3] = cap[n];
                cap[n*3+1] = cap[(n+1)%capverts];
                cap[n*3+2] = numverts-1;
                capindices += 3;
            }
        }

        if(!vbuf) glGenBuffers_(1, &vbuf);
        gle::bindvbo(vbuf);
        glBufferData_(GL_ARRAY_BUFFER, numverts*sizeof(vert), verts, GL_STATIC_DRAW);
        DELETEA(verts);

        if(!ebuf) glGenBuffers_(1, &ebuf);
        gle::bindebo(ebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, (numindices + capindices)*sizeof(GLushort), indices, GL_STATIC_DRAW);
        DELETEA(indices);
    }

    void cleanup()
    {
        numverts = numindices = 0;
        if(vbuf) { glDeleteBuffers_(1, &vbuf); vbuf = 0; }
        if(ebuf) { glDeleteBuffers_(1, &ebuf); ebuf = 0; }
    }

    void draw()
    {
        float capsize = getfogdomecap() && getfogdomeheight() < 1 ? (1 + getfogdomeheight()) / (1 - getfogdomeheight()) : -1;
        bvec color = !getfogdomecolour().iszero() ? getfogdomecolour() : fogcolour;
        if(!numverts || lastcolor != color || lastminalpha != getfogdomemin() || lastmaxalpha != getfogdomemax() || lastcapsize != capsize || lastclipz != getfogdomeclip())
        {
            init(color, min(getfogdomemin(), getfogdomemax()), getfogdomemax(), capsize, getfogdomeclip());
            lastcolor = color;
            lastminalpha = getfogdomemin();
            lastmaxalpha = getfogdomemax();
            lastcapsize = capsize;
            lastclipz = getfogdomeclip();
        }

        gle::bindvbo(vbuf);
        gle::bindebo(ebuf);

        gle::vertexpointer(sizeof(vert), &verts->pos);
        gle::colorpointer(sizeof(vert), &verts->color);
        gle::enablevertex();
        gle::enablecolor();

        glDrawRangeElements_(GL_TRIANGLES, 0, numverts-1, numindices + getfogdomecap()*capindices, GL_UNSIGNED_SHORT, indices);
        xtraverts += numverts;
        glde++;

        gle::disablevertex();
        gle::disablecolor();

        gle::clearvbo();
        gle::clearebo();
    }
}

static void drawfogdome()
{
    SETSHADER(skyfog);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    matrix4 skymatrix = cammatrix, skyprojmatrix;
    skymatrix.settranslation(vec(cammatrix.c).mul(farplane*getfogdomeheight()*0.5f));
    skymatrix.scale(farplane/2, farplane/2, farplane*(0.5f - getfogdomeheight()*0.5f));
    skyprojmatrix.mul(projmatrix, skymatrix);
    LOCALPARAM(skymatrix, skyprojmatrix);

    fogdome::draw();

    glDisable(GL_BLEND);
}

void cleanupsky()
{
    fogdome::cleanup();
}

static void drawatmosphere()
{
    if(getatmoblend() < 1)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    SETSHADER(atmosphere);

    matrix4 sunmatrix = invcammatrix;
    sunmatrix.settranslation(0, 0, 0);
    sunmatrix.mul(invprojmatrix);
    LOCALPARAM(sunmatrix, sunmatrix);

    bvec curatmolight = getatmolight();
    LOCALPARAM(sunlight, (!curatmolight.iszero() ? curatmolight.tocolor().mul(getatmolightscale()) : getpielight().tocolor().mul(getpielightscale())).mul(getatmobright()*ldrscale));
    LOCALPARAM(sundir, getpielightdir());

    vec sundiskparams;
    sundiskparams.y = -(1 - 0.0075f * getatmodisksize());
    sundiskparams.x = 1/(1 + sundiskparams.y);
    sundiskparams.y *= sundiskparams.x;
    sundiskparams.z = getatmodiskbright();
    LOCALPARAM(sundiskparams, sundiskparams);

    const float earthradius = 6.371e6f, earthatmoheight = 0.1e6f;
    float planetradius = earthradius*getatmoplanetsize(), atmoradius = planetradius + earthatmoheight*getatmoheight();
    LOCALPARAMF(atmoradius, planetradius, atmoradius*atmoradius, atmoradius*atmoradius - planetradius*planetradius);

    float gm = (1 - getatmohaze())*0.2f + 0.75f;
    LOCALPARAMF(gm, gm);

    vec lambda(680e-9f, 550e-9f, 450e-9f),
        betar = vec(lambda).square().square().recip().mul(1.86e-31f / getatmodensity()),
        betam = vec(lambda).recip().mul(2*M_PI).square().mul(getatmohazefade().tocolor().mul(getatmohazefadescale())).mul(1.36e-19f * max(getatmohaze(), FVAR_NONZERO)),
        betarm = vec(betar).div(1+getatmoclarity()).add(betam);
    betar.div(betarm).mul(3/(16*M_PI));
    betam.div(betarm).mul((1-gm)*(1-gm)/(4*M_PI));
    LOCALPARAM(betar, betar);
    LOCALPARAM(betam, betam);
    LOCALPARAM(betarm, betarm.div(M_LN2));

    LOCALPARAMF(atmoblend, getatmoblend());

    gle::defvertex();
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(-1, 1, 1);
    gle::attribf(1, 1, 1);
    gle::attribf(-1, -1, 1);
    gle::attribf(1, -1, 1);
    xtraverts += gle::end();

    if(getatmoblend() < 1) glDisable(GL_BLEND);
}

VAR(0, showsky, 0, 1, 1);
VAR(0, clampsky, 0, 1, 1);

int explicitsky = 0;

bool limitsky()
{
    return explicitsky && (getskytexture() || editmode);
}

void drawskybox(bool clear)
{
    bool limited = false;
    if(limitsky()) for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->sky && va->occluded < OCCLUDE_BB &&
           ((va->skymax.x >= 0 && isvisiblebb(va->skymin, ivec(va->skymax).sub(va->skymin)) != VFC_NOT_VISIBLE) ||
            !insideworld(camera1->o)))
        {
            limited = true;
            break;
        }
    }
    if(limited)
    {
        glDisable(GL_DEPTH_TEST);
    }
    else
    {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
    }

    if(clampsky) glDepthRange(1, 1);

    const char *curskybox = getskybox();
    bool blendsky = !curskybox[0] || !sky[0] || sky[0]->type&Texture::ALPHA || getskyblend() < 1;
    if(clear)
    {
        vec color = getskybgcolour().tocolor().mul(ldrscale);
        glClearColor(color.x, color.y, color.z, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else if(blendsky && (!getatmo() || getatmoblend() < 1))
    {
        SETSHADER(skyfog);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        gle::color(getskybgcolour());
        drawenvboxbg();
    }

    if(getatmo() == 2) drawatmosphere();

    if(curskybox[0])
    {
        if(blendsky)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        float overbright = getskyoverbright(), overbrightmin = getskyoverbrightmin(), overbrightthreshold = getskyoverbrightthreshold();
        if(ldrscale < 1 && (overbrightmin != 1 || (overbright > 1 && overbrightthreshold < 1)))
        {
            SETSHADER(skyboxoverbright);
            LOCALPARAMF(overbrightparams, overbrightmin, max(overbright, overbrightmin), overbrightthreshold);
        }
        else SETSHADER(skybox);

        gle::color(getskycolour().tocolor(), getskyblend());

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((getspinsky()*lastmillis/1000.0f+getyawsky())*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvbox(sky);

        if(blendsky) glDisable(GL_BLEND);
    }

    if(getatmo() == 1) drawatmosphere();

    if(getfogdomemax() && !getfogdomeclouds()) drawfogdome();

    const char *curcloudbox = getcloudbox();
    if(curcloudbox[0])
    {
        SETSHADER(skybox);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        gle::color(getcloudcolour().tocolor(), getcloudblend());

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((getspinclouds()*lastmillis/1000.0f+getyawclouds())*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvbox(clouds, getcloudclip());

        glDisable(GL_BLEND);
    }

    const char *curcloudlayer = getcloudlayer();
    if(curcloudlayer[0] && getcloudheight())
    {
        SETSHADER(skybox);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((getspincloudlayer()*lastmillis/1000.0f+getyawcloudlayer())*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(cloudoverlay, getcloudheight(), getcloudsubdiv(), getcloudfade(), getcloudscale(), getcloudlayercolour(), getcloudlayerblend(), getcloudoffsetx() + getcloudscrollx() * lastmillis/1000.0f, getcloudoffsety() + getcloudscrolly() * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    const char *curenvlayer = getenvlayer();
    if(curenvlayer[0] && getenvheight())
    {
        SETSHADER(skybox);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((getspinenvlayer()*lastmillis/1000.0f+getyawenvlayer())*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(envoverlay, getenvheight(), getenvsubdiv(), getenvfade(), getenvscale(), getenvlayercolour(), getenvlayerblend(), getenvoffsetx() + getenvscrollx() * lastmillis/1000.0f, getenvoffsety() + getenvscrolly() * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    if(getfogdomemax() && getfogdomeclouds()) drawfogdome();

    if(clampsky) glDepthRange(0, 1);

    if(limited)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }
}

bool hasskybox()
{
    return getskybox()[0] || getatmo() || getfogdomemax() || getcloudbox()[0] || getcloudlayer()[0] || getenvlayer()[0];
}

void initskybox()
{
    const char *curskybox = getskybox();
    if(curskybox[0]) loadsky(curskybox, sky);
    const char *curcloudbox = getcloudbox();
    if(curcloudbox[0]) loadsky(curskybox, sky);
    const char *curcloudlayer = getcloudlayer();
    if(curcloudlayer[0]) cloudoverlay = loadskyoverlay(curcloudlayer);
    const char *curenvlayer = getenvlayer();
    if(curenvlayer[0]) envoverlay = loadskyoverlay(curenvlayer);
}
