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

CVAR(IDF_WORLD, skybgcolour, 0x000000);

SVARF(IDF_WORLD, skybox, "", { if(skybox[0]) loadsky(skybox, sky); });
CVAR(IDF_WORLD, skycolour, 0xFFFFFF);
FVAR(IDF_WORLD, skyblend, 0, 1.0f, 1);
FVAR(IDF_WORLD, skyoverbright, 1, 2, 16);
FVAR(IDF_WORLD, skyoverbrightmin, 0, 1, 16);
FVAR(IDF_WORLD, skyoverbrightthreshold, 0, 0.7f, 1);
FVAR(IDF_WORLD, spinsky, -720, 0, 720);
VAR(IDF_WORLD, yawsky, 0, 0, 360);

SVARF(IDF_WORLD, cloudbox, "", { if(cloudbox[0]) loadsky(cloudbox, clouds); });
CVAR(IDF_WORLD, cloudcolour, 0xFFFFFF);
FVAR(IDF_WORLD, cloudblend, 0, 1.0f, 1);
FVAR(IDF_WORLD, spinclouds, -720, 0, 720);
VAR(IDF_WORLD, yawclouds, 0, 0, 360);

FVAR(IDF_WORLD, cloudclip, 0, 0.5f, 1);
SVARF(IDF_WORLD, cloudlayer, "", { if(cloudlayer[0]) cloudoverlay = loadskyoverlay(cloudlayer); });
CVAR(IDF_WORLD, cloudlayercolour, 0xFFFFFF);
FVAR(IDF_WORLD, cloudlayerblend, 0, 1.0f, 1);
FVAR(IDF_WORLD, cloudoffsetx, 0, 0, 1);
FVAR(IDF_WORLD, cloudoffsety, 0, 0, 1);
FVAR(IDF_WORLD, cloudscrollx, -16, 0, 16);
FVAR(IDF_WORLD, cloudscrolly, -16, 0, 16);
FVAR(IDF_WORLD, cloudscale, FVAR_NONZERO, 1, 64);
FVAR(IDF_WORLD, spincloudlayer, -720, 0, 720);
VAR(IDF_WORLD, yawcloudlayer, 0, 0, 360);
FVAR(IDF_WORLD, cloudheight, -1, 0.2f, 1);
FVAR(IDF_WORLD, cloudfade, 0, 0.2f, 1);
VAR(IDF_WORLD, cloudsubdiv, 4, 16, 64);

SVARF(IDF_WORLD, envlayer, "", { if(envlayer[0]) envoverlay = loadskyoverlay(envlayer); });
CVAR(IDF_WORLD, envlayercolour, 0xFFFFFF);
FVAR(IDF_WORLD, envlayerblend, 0, 1.0f, 1);
FVAR(IDF_WORLD, envoffsetx, 0, 0, 1);
FVAR(IDF_WORLD, envoffsety, 0, 0, 1);
FVAR(IDF_WORLD, envscrollx, -16, 0, 16);
FVAR(IDF_WORLD, envscrolly, -16, 0, 16);
FVAR(IDF_WORLD, envscale, FVAR_NONZERO, 1, 64);
FVAR(IDF_WORLD, spinenvlayer, -720, 0, 720);
VAR(IDF_WORLD, yawenvlayer, 0, 0, 360);
FVAR(IDF_WORLD, envheight, -1, 0.2f, 1);
FVAR(IDF_WORLD, envfade, 0, 0.2f, 1);
VAR(IDF_WORLD, envsubdiv, 4, 16, 64);

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

FVAR(IDF_WORLD, fogdomeheight, -1, -0.5f, 1);
FVAR(IDF_WORLD, fogdomemin, 0, 0, 1);
FVAR(IDF_WORLD, fogdomemax, 0, 0, 1);
VAR(IDF_WORLD, fogdomecap, 0, 1, 1);
FVAR(IDF_WORLD, fogdomeclip, 0, 1, 1);
CVAR(IDF_WORLD, fogdomecolour, 0xFFFFFF);
VAR(IDF_WORLD, fogdomeclouds, 0, 1, 1);

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
        float capsize = fogdomecap && fogdomeheight < 1 ? (1 + fogdomeheight) / (1 - fogdomeheight) : -1;
        bvec color = !fogdomecolour.iszero() ? fogdomecolour : fogcolour;
        if(!numverts || lastcolor != color || lastminalpha != fogdomemin || lastmaxalpha != fogdomemax || lastcapsize != capsize || lastclipz != fogdomeclip)
        {
            init(color, min(fogdomemin, fogdomemax), fogdomemax, capsize, fogdomeclip);
            lastcolor = color;
            lastminalpha = fogdomemin;
            lastmaxalpha = fogdomemax;
            lastcapsize = capsize;
            lastclipz = fogdomeclip;
        }

        gle::bindvbo(vbuf);
        gle::bindebo(ebuf);

        gle::vertexpointer(sizeof(vert), &verts->pos);
        gle::colorpointer(sizeof(vert), &verts->color);
        gle::enablevertex();
        gle::enablecolor();

        glDrawRangeElements_(GL_TRIANGLES, 0, numverts-1, numindices + fogdomecap*capindices, GL_UNSIGNED_SHORT, indices);
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
    skymatrix.settranslation(vec(cammatrix.c).mul(farplane*fogdomeheight*0.5f));
    skymatrix.scale(farplane/2, farplane/2, farplane*(0.5f - fogdomeheight*0.5f));
    skyprojmatrix.mul(projmatrix, skymatrix);
    LOCALPARAM(skymatrix, skyprojmatrix);

    fogdome::draw();

    glDisable(GL_BLEND);
}

void cleanupsky()
{
    fogdome::cleanup();
}

VAR(IDF_WORLD, atmo, 0, 0, 1);
FVAR(IDF_WORLD, atmoplanetsize, 1e-3f, 8, 1e3f);
FVAR(IDF_WORLD, atmoheight, 1e-3f, 1, 1e3f);
FVAR(IDF_WORLD, atmobright, 0, 4, 16);
CVAR1(IDF_WORLD, atmosunlight, 0);
FVAR(IDF_WORLD, atmosunlightscale, 0, 1, 16);
FVAR(IDF_WORLD, atmosundisksize, 0, 1, 10);
FVAR(IDF_WORLD, atmosundiskbright, 0, 1, 16);
FVAR(IDF_WORLD, atmohaze, 0, 0.03f, 1);
CVAR0(IDF_WORLD, atmohazefade, 0xAEACA9);
FVAR(IDF_WORLD, atmohazefadescale, 0, 1, 1);
FVAR(IDF_WORLD, atmoclarity, 0, 0.2f, 10);
FVAR(IDF_WORLD, atmodensity, 1e-3f, 0.99f, 10);
FVAR(IDF_WORLD, atmoblend, 0, 1, 1);

static void drawatmosphere()
{
    SETSHADER(atmosphere);

    matrix4 sunmatrix = invcammatrix;
    sunmatrix.settranslation(0, 0, 0);
    sunmatrix.mul(invprojmatrix);
    LOCALPARAM(sunmatrix, sunmatrix);

    LOCALPARAM(sunlight, (!atmosunlight.iszero() ? atmosunlight.tocolor().mul(atmosunlightscale) : sunlight.tocolor().mul(sunlightscale)).mul(atmobright*ldrscale));
    LOCALPARAM(sundir, sunlightdir);

    vec sundiskparams;
    sundiskparams.y = -(1 - 0.0075f * atmosundisksize);
    sundiskparams.x = 1/(1 + sundiskparams.y);
    sundiskparams.y *= sundiskparams.x;
    sundiskparams.z = atmosundiskbright;
    LOCALPARAM(sundiskparams, sundiskparams);

    const float earthradius = 6.371e6f, earthatmoheight = 0.1e6f;
    float planetradius = earthradius*atmoplanetsize, atmoradius = planetradius + earthatmoheight*atmoheight;
    LOCALPARAMF(atmoradius, planetradius, atmoradius*atmoradius, atmoradius*atmoradius - planetradius*planetradius);

    float gm = (1 - atmohaze)*0.2f + 0.75f;
    LOCALPARAMF(gm, gm);

    vec lambda(680e-9f, 550e-9f, 450e-9f),
        betar = vec(lambda).square().square().recip().mul(1.86e-31f / atmodensity),
        betam = vec(lambda).recip().mul(2*M_PI).square().mul(atmohazefade.tocolor().mul(atmohazefadescale)).mul(1.36e-19f * max(atmohaze, 1e-3f)),
        betarm = vec(betar).div(1+atmoclarity).add(betam);
    betar.div(betarm).mul(3/(16*M_PI));
    betam.div(betarm).mul((1-gm)*(1-gm)/(4*M_PI));
    LOCALPARAM(betar, betar);
    LOCALPARAM(betam, betam);
    LOCALPARAM(betarm, betarm.div(M_LN2));

    LOCALPARAMF(atmoblend, atmoblend);

    gle::defvertex();
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(-1, 1, 1);
    gle::attribf(1, 1, 1);
    gle::attribf(-1, -1, 1);
    gle::attribf(1, -1, 1);
    xtraverts += gle::end();
}

VAR(0, showsky, 0, 1, 1);
VAR(0, clampsky, 0, 1, 1);
VARN(IDF_WORLD, skytexture, useskytexture, 0, 0, 1);
VARF(IDF_WORLD, skyshadow, 0, 0, 1, clearshadowcache());

int explicitsky = 0;

bool limitsky()
{
    return explicitsky && (useskytexture || editmode);
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

    if(clear || (!skybox[0] && (!atmo || atmoblend < 1)))
    {
        vec color = skybgcolour.tocolor().mul(ldrscale);
        glClearColor(color.x, color.y, color.z, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    bool blendsky = !skybox[0] || !sky[0] || sky[0]->type&Texture::ALPHA;

    if(blendsky || skyblend < 1)
    {
        SETSHADER(skyfog);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        gle::color(skybgcolour);
        drawenvboxbg();
    }

    if(skybox[0])
    {
        if(blendsky)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        if(ldrscale < 1 && (skyoverbrightmin != 1 || (skyoverbright > 1 && skyoverbrightthreshold < 1)))
        {
            SETSHADER(skyboxoverbright);
            LOCALPARAMF(overbrightparams, skyoverbrightmin, max(skyoverbright, skyoverbrightmin), skyoverbrightthreshold);
        }
        else SETSHADER(skybox);

        gle::color(skycolour.tocolor(), skyblend);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spinsky*lastmillis/1000.0f+yawsky)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvbox(sky);

        if(blendsky) glDisable(GL_BLEND);
    }

    if(atmo && (!skybox[0] || atmoblend < 1))
    {
        if(atmoblend < 1)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        drawatmosphere();

        if(atmoblend < 1) glDisable(GL_BLEND);
    }

    if(fogdomemax && !fogdomeclouds)
    {
        drawfogdome();
    }

    if(cloudbox[0])
    {
        SETSHADER(skybox);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        gle::color(cloudcolour.tocolor(), cloudblend);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spinclouds*lastmillis/1000.0f+yawclouds)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvbox(clouds, cloudclip);

        glDisable(GL_BLEND);
    }

    if(cloudlayer[0] && cloudheight)
    {
        SETSHADER(skybox);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spincloudlayer*lastmillis/1000.0f+yawcloudlayer)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(cloudoverlay, cloudheight, cloudsubdiv, cloudfade, cloudscale, cloudlayercolour, cloudlayerblend, cloudoffsetx + cloudscrollx * lastmillis/1000.0f, cloudoffsety + cloudscrolly * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    if(envlayer[0] && envheight)
    {
        SETSHADER(skybox);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spinenvlayer*lastmillis/1000.0f+yawenvlayer)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(envoverlay, envheight, envsubdiv, envfade, envscale, envlayercolour, envlayerblend, envoffsetx + envscrollx * lastmillis/1000.0f, envoffsety + envscrolly * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    if(fogdomemax && fogdomeclouds)
    {
        drawfogdome();
    }

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
    return skybox[0] || atmo || fogdomemax || cloudbox[0] || cloudlayer[0] || envlayer[0];
}

