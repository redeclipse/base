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

#define MPVVARS(name, type) \
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
    FVAR(IDF_WORLD, spinskypitch##name, -720, 0, 720); \
    FVAR(IDF_WORLD, spinskyroll##name, -720, 0, 720); \
    VAR(IDF_WORLD, yawsky##name, 0, 0, 360); \
    VAR(IDF_WORLD, pitchsky##name, 0, 0, 360); \
    VAR(IDF_WORLD, rollsky##name, 0, 0, 360); \
    SVARF(IDF_WORLD, cloudbox##name, "", { if(cloudbox##name[0] && checkmapvariant(type)) loadsky(cloudbox##name, clouds); }); \
    CVAR(IDF_WORLD, cloudcolour##name, 0xFFFFFF); \
    FVAR(IDF_WORLD, cloudblend##name, 0, 1.0f, 1); \
    FVAR(IDF_WORLD, spinclouds##name, -720, 0, 720); \
    FVAR(IDF_WORLD, spincloudspitch##name, -720, 0, 720); \
    FVAR(IDF_WORLD, spincloudsroll##name, -720, 0, 720); \
    VAR(IDF_WORLD, yawclouds##name, 0, 0, 360); \
    VAR(IDF_WORLD, pitchclouds##name, 0, 0, 360); \
    VAR(IDF_WORLD, rollclouds##name, 0, 0, 360); \
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
    FVAR(IDF_WORLD, atmoplanetsize##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_WORLD, atmoheight##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_WORLD, atmobright##name, 0, 1, 16); \
    FVAR(IDF_WORLD|IDF_READONLY, atmoclarity##name, 0, 0, 10); /* old map compat for fixatmo, don't use */ \
    CVAR1(IDF_WORLD, atmolight##name, 0); \
    FVAR(IDF_WORLD, atmolightscale##name, 0, 1, 16); \
    CVAR1(IDF_WORLD, atmodisk##name, 0); \
    FVAR(IDF_WORLD, atmodisksize##name, 0, 12, 90); \
    FVAR(IDF_WORLD, atmodiskcorona##name, 0, 0.4f, 1); \
    FVAR(IDF_WORLD, atmodiskbright##name, 0, 1, 16); \
    FVAR(IDF_WORLD, atmohaze##name, 0, 0.1f, 100); \
    FVAR(IDF_WORLD, atmodensity##name, 0, 1, 100); \
    FVAR(IDF_WORLD, atmoozone##name, 0, 1, 100); \
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

MPVVARS(, MPV_DEF);
MPVVARS(alt, MPV_ALT);

#define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALT)) return name##alt; \
        return name; \
    }

GETMPV(ambient, bvec &);
GETMPV(ambientscale, float);
GETMPV(skylight, bvec &);
GETMPV(skylightscale, float);
GETMPV(fog, int);
GETMPV(fogcolour, bvec &);
GETMPV(skybgcolour, bvec &);
GETMPV(skybox, const char *);
GETMPV(skycolour, bvec &);
GETMPV(skyblend, float);
GETMPV(skyoverbright, float);
GETMPV(skyoverbrightmin, float);
GETMPV(skyoverbrightthreshold, float);
GETMPV(spinsky, float);
GETMPV(spinskypitch, float);
GETMPV(spinskyroll, float);
GETMPV(yawsky, int);
GETMPV(pitchsky, int);
GETMPV(rollsky, int);
GETMPV(cloudbox, const char *);
GETMPV(cloudcolour, bvec &);
GETMPV(cloudblend, float);
GETMPV(spinclouds, float);
GETMPV(spincloudspitch, float);
GETMPV(spincloudsroll, float);
GETMPV(yawclouds, int);
GETMPV(pitchclouds, int);
GETMPV(rollclouds, int);
GETMPV(cloudclip, float);
GETMPV(cloudlayer, const char *);
GETMPV(cloudlayercolour, bvec &);
GETMPV(cloudlayerblend, float);
GETMPV(cloudoffsetx, float);
GETMPV(cloudoffsety, float);
GETMPV(cloudscrollx, float);
GETMPV(cloudscrolly, float);
GETMPV(cloudscale, float);
GETMPV(spincloudlayer, float);
GETMPV(yawcloudlayer, int);
GETMPV(cloudheight, float);
GETMPV(cloudfade, float);
GETMPV(cloudsubdiv, int);
GETMPV(envlayer, const char *);
GETMPV(envlayercolour, bvec &);
GETMPV(envlayerblend, float);
GETMPV(envoffsetx, float);
GETMPV(envoffsety, float);
GETMPV(envscrollx, float);
GETMPV(envscrolly, float);
GETMPV(envscale, float);
GETMPV(spinenvlayer, float);
GETMPV(yawenvlayer, int);
GETMPV(envheight, float);
GETMPV(envfade, float);
GETMPV(envsubdiv, int);
GETMPV(atmo, int);
GETMPV(atmoplanetsize, float);
GETMPV(atmoheight, float);
GETMPV(atmobright, float);
GETMPV(atmolight, bvec &);
GETMPV(atmolightscale, float);
GETMPV(atmodisk, bvec &);
GETMPV(atmodisksize, float);
GETMPV(atmodiskcorona, float);
GETMPV(atmodiskbright, float);
GETMPV(atmohaze, float);
GETMPV(atmoclarity, float);
GETMPV(atmodensity, float);
GETMPV(atmoozone, float);
GETMPV(atmoblend, float);
GETMPV(fogdomeheight, float);
GETMPV(fogdomemin, float);
GETMPV(fogdomemax, float);
GETMPV(fogdomecap, int);
GETMPV(fogdomeclip, float);
GETMPV(fogdomecolour, bvec &);
GETMPV(fogdomeclouds, int);
GETMPV(skytexture, int);
GETMPV(skyshadow, int);

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
            verts[curvert] = vert(verts[idx[i]], verts[idx[(i+1)%3]]); // push on to unit sphere
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
        verts = new vert[tris+1 + (capsize >= 0)];
        indices = new GLushort[(tris + (capsize >= 0 ? hres<<depth : 0))*3];
        if(clipz >= 1)
        {
            verts[numverts++] = vert(vec(0.0f, 0.0f, 1.0f), color, minalpha); // build initial 'hres' sided pyramid
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

void fixatmo()
{
    if(noedit(true)) return;
    float betar = 1.86e-31 / (pow(550e-9f, 4) * atmodensity),
          betam = pow(2*M_PI/550e-9f, 2) * 1.36e-19f * 0.68f * atmohaze,
          ratio = (betar / (1 + atmoclarity) + betam) / (betar / 1.2f + betam);
    setfvar("atmohaze", atmohaze * (0.1f/0.03f));
    setfvar("atmobright", pow(atmobright / 4, 2) / ratio);
    setfvar("atmodisksize", max(12 + 5 * (atmodisksize - 1), 0.0f));
    setfvar("atmodiskcorona", 0.4f);
    setfvar("atmodensity", 0.99f / atmodensity);
    setfvar("atmoheight", atmoheight * ratio);
    setfvar("atmoplanetsize", atmoplanetsize / 8);
    float betaralt = 1.86e-31 / (pow(550e-9f, 4) * atmodensityalt),
          betamalt = pow((2*M_PI)/550e-9f, 2) * 1.36e-19f * 0.68f * atmohazealt,
          ratioalt = (betaralt / (1 + atmoclarityalt) + betamalt) / (betaralt / 1.2f + betamalt);
    setfvar("atmohazealt", atmohazealt * (0.1f/0.03f));
    setfvar("atmobrightalt", pow(atmobrightalt / 4, 2) / ratioalt);
    setfvar("atmodisksizealt", max(12 + 5 * (atmodisksizealt - 1), 0.0f));
    setfvar("atmodiskcoronaalt", 0.4f);
    setfvar("atmodensityalt", 0.99f / atmodensityalt);
    setfvar("atmoheightalt", atmoheightalt * ratioalt);
    setfvar("atmoplanetsizealt", atmoplanetsizealt / 8);
}
COMMAND(0, fixatmo, "");

static void drawatmosphere()
{
    if(getatmoblend() < 1)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    vec sundir = getpielightdir();
    SETSHADER(atmosphere);

    matrix4 sunmatrix = invcammatrix;
    sunmatrix.settranslation(0, 0, 0);
    sunmatrix.mul(invprojmatrix);
    LOCALPARAM(sunmatrix, sunmatrix);

    // optical depth scales for 3 different shells of atmosphere - air, haze, ozone
    const float earthradius = 6371e3f, earthairheight = 8.4e3f, earthhazeheight = 1.25e3f, earthozoneheight = 50e3f;
    float planetradius = earthradius*getatmoplanetsize();
    vec atmoshells = vec(earthairheight, earthhazeheight, earthozoneheight).mul(getatmoheight()).add(planetradius).square().sub(planetradius*planetradius);
    LOCALPARAM(opticaldepthparams, vec4(atmoshells, planetradius));

    // Henyey-Greenstein approximation, 1/(4pi) * (1 - g^2)/(1 + g^2 - 2gcos)]^1.5
    // Hoffman-Preetham variation uses (1-g)^2 instead of 1-g^2 which avoids excessive glare
    // clamp values near 0 angle to avoid spotlight artifact inside sundisk
    float gm = max(0.95f - 0.2f*getatmohaze(), 0.65f), miescale = pow((1-gm)*(1-gm)/(4*M_PI), -2.0f/3.0f);
    LOCALPARAMF(mieparams, miescale*(1 + gm*gm), miescale*-2*gm, 1 - (1 - cosf(0.5f*getatmodisksize()*(1 - getatmodiskcorona())*RAD)));

    static const vec lambda(680e-9f, 550e-9f, 450e-9f),
                     k(0.686f, 0.678f, 0.666f),
                     ozone(3.426f, 8.298f, 0.356f);
    vec betar = vec(lambda).square().square().recip().mul(1.241e-30f/M_LN2 * getatmodensity()),
        betam = vec(lambda).recip().square().mul(k).mul(9.072e-17f/M_LN2 * getatmohaze()),
        betao = vec(ozone).mul(1.5e-7f/M_LN2 * getatmoozone());
    LOCALPARAM(betarayleigh, betar);
    LOCALPARAM(betamie, betam);
    LOCALPARAM(betaozone, betao);

    // extinction in direction of sun
    float sunoffset = sundir.z*planetradius;
    vec sundepth = vec(atmoshells).add(sunoffset*sunoffset).sqrt().sub(sunoffset);
    vec sunweight = vec(betar).mul(sundepth.x).madd(betam, sundepth.y).madd(betao, sundepth.z - sundepth.x);
    vec sunextinction = vec(sunweight).neg().exp2();
    bvec curatmolight = getatmolight();
    vec suncolor = !curatmolight.iszero() ? curatmolight.tocolor().mul(getatmolightscale()) : getpielight().tocolor().mul(getpielightscale());
    // assume sunlight color is gamma encoded, so decode to linear light, then apply extinction
    extern float hdrgamma;
    vec sunscale = vec(suncolor).mul(ldrscale).pow(hdrgamma).mul(getatmobright() * 16).mul(sunextinction);
    float maxsunweight = max(max(sunweight.x, sunweight.y), sunweight.z);
    if(maxsunweight > 127) sunweight.mul(127/maxsunweight);
    sunweight.add(1e-4f);
    LOCALPARAM(sunweight, sunweight);
    LOCALPARAM(sunlight, vec4(sunscale, getatmoblend()));
    LOCALPARAM(sundir, sundir);

    // invert extinction at zenith to get an approximation of how bright the sun disk should be
    vec zenithdepth = vec(atmoshells).add(planetradius*planetradius).sqrt().sub(planetradius);
    vec zenithweight = vec(betar).mul(zenithdepth.x).madd(betam, zenithdepth.y).madd(betao, zenithdepth.z - zenithdepth.x);
    vec zenithextinction = vec(zenithweight).sub(sunweight).exp2();
    bvec curatmodisk = getatmodisk();
    vec diskcolor = (!curatmodisk.iszero() ? curatmodisk.tocolor() : suncolor).mul(ldrscale).pow(hdrgamma).mul(zenithextinction).mul(getatmodiskbright() * 4);
    LOCALPARAM(sundiskcolor, diskcolor);

    // convert from view cosine into mu^2 for limb darkening, where mu = sqrt(1 - sin^2) and sin^2 = 1 - cos^2, thus mu^2 = 1 - (1 - cos^2*scale)
    // convert corona offset into scale for mu^2, where sin = (1-corona) and thus mu^2 = 1 - (1-corona^2)
    float sundiskscale = sinf(0.5f*getatmodisksize()*RAD);
    float coronamu = 1 - (1-getatmodiskcorona())*(1-getatmodiskcorona());
    if(sundiskscale > 0) LOCALPARAMF(sundiskparams, 1.0f/(sundiskscale*sundiskscale), 1.0f/max(coronamu, 1e-3f));
    else LOCALPARAMF(sundiskparams, 0, 0);

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
        skymatrix.rotate_around_y((getspinskypitch()*lastmillis/1000.0f+getpitchsky())*-RAD);
        skymatrix.rotate_around_x((getspinskyroll()*lastmillis/1000.0f+getrollsky())*-RAD);
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
        skymatrix.rotate_around_y((getspincloudspitch()*lastmillis/1000.0f+getpitchclouds())*-RAD);
        skymatrix.rotate_around_x((getspincloudsroll()*lastmillis/1000.0f+getrollclouds())*-RAD);
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
    if(curcloudbox[0]) loadsky(curcloudbox, clouds);
    const char *curcloudlayer = getcloudlayer();
    if(curcloudlayer[0]) cloudoverlay = loadskyoverlay(curcloudlayer);
    const char *curenvlayer = getenvlayer();
    if(curenvlayer[0]) envoverlay = loadskyoverlay(curenvlayer);
}
