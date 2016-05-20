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
            conoutf("\frcould not load side %s of sky texture %s", side, basename);
        }
    }
}

Texture *cloudoverlay = NULL, *envoverlay = NULL;

Texture *loadskyoverlay(const char *basename)
{
    Texture *t = textureload(basename, 0, true, false);
    if(t == notexture) conoutf("\frcould not load sky overlay texture %s", basename);
    return t;
}

VAR(IDF_HEX|IDF_WORLD, skybgcolour, 0, 0x000000, 0xFFFFFF);
VAR(IDF_WORLD, skybgglare, 0, 0, 1);

SVARF(IDF_WORLD, skybox, "", { if(skybox[0]) loadsky(skybox, sky); });
FVAR(IDF_WORLD, skyblend, 0, 1.0f, 1);
VAR(IDF_WORLD, skyglare, 0, 0, 1);
VAR(IDF_HEX|IDF_WORLD, skycolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_WORLD, spinsky, -720, 0, 720);
VAR(IDF_WORLD, yawsky, 0, 0, 360);

SVARF(IDF_WORLD, cloudbox, "", { if(cloudbox[0]) loadsky(cloudbox, clouds); });
FVAR(IDF_WORLD, cloudblend, 0, 1.0f, 1);
VAR(IDF_WORLD, cloudglare, 0, 0, 1);
VAR(IDF_HEX|IDF_WORLD, cloudcolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_WORLD, spinclouds, -720, 0, 720);
VAR(IDF_WORLD, yawclouds, 0, 0, 360);

SVARF(IDF_WORLD, cloudlayer, "", { if(cloudlayer[0]) cloudoverlay = loadskyoverlay(cloudlayer); });
FVAR(IDF_WORLD, cloudlayerblend, 0, 1.0f, 1);
VAR(IDF_WORLD, cloudlayerglare, 0, 0, 1);
VAR(IDF_HEX|IDF_WORLD, cloudlayercolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_WORLD, cloudoffsetx, 0, 0, 1);
FVAR(IDF_WORLD, cloudoffsety, 0, 0, 1);
FVAR(IDF_WORLD, cloudscrollx, -16, 0, 16);
FVAR(IDF_WORLD, cloudscrolly, -16, 0, 16);
FVAR(IDF_WORLD, cloudscale, 0, 1, 64);
FVAR(IDF_WORLD, spincloudlayer, -720, 0, 720);
VAR(IDF_WORLD, yawcloudlayer, 0, 0, 360);
FVAR(IDF_WORLD, cloudheight, -1, 0.2f, 1);
FVAR(IDF_WORLD, cloudfade, 0, 0.2f, 1);
VAR(IDF_WORLD, cloudsubdiv, 4, 16, 64);

SVARF(IDF_WORLD, envlayer, "", { if(envlayer[0]) envoverlay = loadskyoverlay(envlayer); });
FVAR(IDF_WORLD, envlayerblend, 0, 1.0f, 1);
VAR(IDF_WORLD, envlayerglare, 0, 0, 1);
VAR(IDF_HEX|IDF_WORLD, envlayercolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_WORLD, envoffsetx, 0, 0, 1);
FVAR(IDF_WORLD, envoffsety, 0, 0, 1);
FVAR(IDF_WORLD, envscrollx, -16, 0, 16);
FVAR(IDF_WORLD, envscrolly, -16, 0, 16);
FVAR(IDF_WORLD, envscale, 0, 1, 64);
FVAR(IDF_WORLD, spinenvlayer, -720, 0, 720);
VAR(IDF_WORLD, yawenvlayer, 0, 0, 360);
FVAR(IDF_WORLD, envheight, -1, 0.2f, 1);
FVAR(IDF_WORLD, envfade, 0, 0.2f, 1);
VAR(IDF_WORLD, envsubdiv, 4, 16, 64);

FVAR(IDF_WORLD, cloudclip, 0, 0.5f, 1);

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

void drawenvbox(int w, float z1clip = 0.0f, float z2clip = 1.0f, int faces = 0x3F, Texture **sky = NULL)
{
    if(z1clip >= z2clip) return;

    float v1 = 1-z1clip, v2 = 1-z2clip;
    int z1 = int(ceil(2*w*(z1clip-0.5f))), z2 = int(ceil(2*w*(z2clip-0.5f)));

    gle::defvertex();
    gle::deftexcoord0();

    if(faces&0x01)
        drawenvboxface(0.0f, v2,  -w, -w, z2,
                       1.0f, v2,  -w,  w, z2,
                       1.0f, v1,  -w,  w, z1,
                       0.0f, v1,  -w, -w, z1, sky[0]);

    if(faces&0x02)
        drawenvboxface(1.0f, v1, w, -w, z1,
                       0.0f, v1, w,  w, z1,
                       0.0f, v2, w,  w, z2,
                       1.0f, v2, w, -w, z2, sky[1]);

    if(faces&0x04)
        drawenvboxface(1.0f, v1, -w, -w, z1,
                       0.0f, v1,  w, -w, z1,
                       0.0f, v2,  w, -w, z2,
                       1.0f, v2, -w, -w, z2, sky[2]);

    if(faces&0x08)
        drawenvboxface(1.0f, v1,  w,  w, z1,
                       0.0f, v1, -w,  w, z1,
                       0.0f, v2, -w,  w, z2,
                       1.0f, v2,  w,  w, z2, sky[3]);

    if(z1clip <= 0 && faces&0x10)
        drawenvboxface(0.0f, 1.0f, -w,  w,  -w,
                       0.0f, 0.0f,  w,  w,  -w,
                       1.0f, 0.0f,  w, -w,  -w,
                       1.0f, 1.0f, -w, -w,  -w, sky[4]);

    if(z2clip >= 1 && faces&0x20)
        drawenvboxface(0.0f, 1.0f,  w,  w, w,
                       0.0f, 0.0f, -w,  w, w,
                       1.0f, 0.0f, -w, -w, w,
                       1.0f, 1.0f,  w, -w, w, sky[5]);
}

void drawenvboxbg(int w, float z1clip = 0.0f, float z2clip = 1.0f, int faces = 0x3F)
{
    if(z1clip >= z2clip) return;

    int z1 = int(ceil(2*w*(z1clip-0.5f))), z2 = int(ceil(2*w*(z2clip-0.5f)));

    gle::defvertex();

    if(faces&0x01) drawenvboxbgface(-w, -w, z2, -w,  w, z2, -w,  w, z1, -w, -w, z1);
    if(faces&0x02) drawenvboxbgface( w, -w, z1,  w,  w, z1,  w,  w, z2,  w, -w, z2);
    if(faces&0x04) drawenvboxbgface(-w, -w, z1,  w, -w, z1,  w, -w, z2, -w, -w, z2);
    if(faces&0x08) drawenvboxbgface( w,  w, z1, -w,  w, z1, -w,  w, z2,  w,  w, z2);
    if(z1clip <= 0 && faces&0x10) drawenvboxbgface(-w,  w, -w,  w,  w, -w,  w, -w, -w, -w, -w, -w);
    if(z2clip >= 1 && faces&0x20) drawenvboxbgface( w,  w,  w, -w,  w,  w, -w, -w,  w,  w, -w,  w);
}

void drawenvoverlay(int w, float height, int subdiv, float fade, float scale, Texture *overlay = NULL, int colour = 0xFFFFFF, float a = 1.f, float tx = 0, float ty = 0)
{
    float z = w*height, tsz = 0.5f*(1-fade)/scale, psz = w*(1-fade);
    glBindTexture(GL_TEXTURE_2D, overlay ? overlay->id : notexture->id);
    vec color = vec::hexcolor(colour);
    gle::color(color, a);
    gle::defvertex();
    gle::deftexcoord0();
    gle::begin(GL_TRIANGLE_FAN);
    loopi(subdiv+1)
    {
        vec p(1, 1, 0);
        p.rotate_around_z((-2.0f*M_PI*i)/subdiv);
        gle::attribf(p.x*psz, p.y*psz, z);
            gle::attribf(tx + p.x*tsz, ty + p.y*tsz);
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
            gle::attribf(tx + p.x*tsz, ty + p.y*tsz);
            gle::attrib(color, a);
        gle::attribf(p.x*w, p.y*w, z);
            gle::attribf(tx + p.x*tsz2, ty + p.y*tsz2);
            gle::attrib(color, 0.0f);
    }
    xtraverts += gle::end();
}

FVAR(IDF_WORLD, fogdomeheight, -1, -0.5f, 1);
FVAR(IDF_WORLD, fogdomemin, 0, 0, 1);
FVAR(IDF_WORLD, fogdomemax, 0, 0, 1);
VAR(IDF_WORLD, fogdomecap, 0, 1, 1);
FVAR(IDF_WORLD, fogdomeclip, 0, 1, 1);
bvec fogdomecolor(0, 0, 0);
VARF(IDF_HEX|IDF_WORLD, fogdomecolour, 0, 0, 0xFFFFFF,
{
    fogdomecolor = bvec((fogdomecolour>>16)&0xFF, (fogdomecolour>>8)&0xFF, fogdomecolour&0xFF);
});

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

    int sortcap(GLushort x, GLushort y)
    {
        const vec &xv = verts[x].pos, &yv = verts[y].pos;
        return xv.y < 0 ? yv.y >= 0 || xv.x < yv.x : yv.y >= 0 && xv.x > yv.x;
    }

    void init(const bvec &color, float minalpha = 0.0f, float maxalpha = 1.0f, float capsize = -1, float clipz = 1, int hres = 16, int depth = 2)
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
        bvec color = fogdomecolour ? fogdomecolor : fogcolor;
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

void cleanupsky()
{
    fogdome::cleanup();
}

VAR(IDF_PERSIST, sparklyfix, 0, 0, 1);
VAR(0, showsky, 0, 1, 1);
VAR(0, clipsky, 0, 1, 1);

bool drawskylimits(bool explicitonly)
{
    nocolorshader->set();

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    bool rendered = rendersky(explicitonly);
    glColorMask(COLORMASK, GL_TRUE);

    return rendered;
}

void drawskyoutline()
{
    notextureshader->set();

    glDepthMask(GL_FALSE);
    extern int wireframe;
    if(!wireframe)
    {
        enablepolygonoffset(GL_POLYGON_OFFSET_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    gle::colorf(0.5f, 0.0f, 0.5f);
    rendersky(true);
    if(!wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
    }
    glDepthMask(GL_TRUE);
}

VAR(0, clampsky, 0, 1, 1);

VAR(IDF_WORLD, fogdomeclouds, 0, 1, 1);

static void drawfogdome(int farplane)
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

static int yawskyfaces(int faces, int yaw = 0, float spin = 0)
{
    if(spin || yaw%90) return faces&0x0F ? faces | 0x0F : faces;
    static const int faceidxs[3][4] =
    {
        { 3, 2, 0, 1 },
        { 1, 0, 3, 2 },
        { 2, 3, 1, 0 }
    };
    yaw /= 90;
    if(yaw < 1 || yaw > 3) return faces;
    const int *idxs = faceidxs[yaw - 1];
    return (faces & ~0x0F) | (((faces>>idxs[0])&1)<<0) | (((faces>>idxs[1])&1)<<1) | (((faces>>idxs[2])&1)<<2) | (((faces>>idxs[3])&1)<<3);
}

void drawskybox(int farplane, bool limited)
{
    extern int renderedskyfaces, renderedskyclip; // , renderedsky, renderedexplicitsky;
    bool alwaysrender = editmode || !insideworld(camera1->o) || reflecting,
         explicitonly = false;
    if(limited)
    {
        explicitonly = alwaysrender || !sparklyfix || refracting;
        if(!drawskylimits(explicitonly) && !alwaysrender) return;
        extern int ati_skybox_bug;
        if(!alwaysrender && !renderedskyfaces && !ati_skybox_bug) explicitonly = false;
    }
    else if(!alwaysrender)
    {
        renderedskyfaces = 0;
        renderedskyclip = INT_MAX;
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->occluded >= OCCLUDE_BB && va->skyfaces&0x80) continue;
            renderedskyfaces |= va->skyfaces&0x3F;
            if(!(va->skyfaces&0x1F) || camera1->o.z < va->skyclip) renderedskyclip = min(renderedskyclip, va->skyclip);
            else renderedskyclip = 0;
        }
        if(!renderedskyfaces) return;
    }

    if(alwaysrender)
    {
        renderedskyfaces = 0x3F;
        renderedskyclip = 0;
    }

    float skyclip = clipsky ? max(renderedskyclip-1, 0) : 0, topclip = 1;
    if(reflectz<hdr.worldsize)
    {
        if(refracting<0) topclip = 0.5f + 0.5f*(reflectz-camera1->o.z)/float(hdr.worldsize);
        else if(reflectz>skyclip) skyclip = reflectz;
    }
    if(skyclip) skyclip = 0.5f + 0.5f*(skyclip-camera1->o.z)/float(hdr.worldsize);

    if(limited)
    {
        if(explicitonly) glDisable(GL_DEPTH_TEST);
        else glDepthFunc(GL_GEQUAL);
    }
    else glDepthFunc(GL_LEQUAL);

    glDepthMask(GL_FALSE);

    if(clampsky) glDepthRange(1, 1);

    bool blendsky = !skybox[0] || !sky[0] || sky[0]->type&Texture::ALPHA;

    if((!glaring || skybgglare) && blendsky)
    {
        SETSHADER(skyfog);
        notextureshader->set();

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        gle::color(vec::hexcolor(skybgcolour));
        drawenvboxbg(farplane/2, skyclip, topclip, yawskyfaces(renderedskyfaces, yawsky, spinsky));
    }

    if(glaring) SETSHADER(skyboxglare);
    else SETSHADER(skybox);

    if((!glaring || skyglare) && skybox[0])
    {
        if(blendsky)
        {
            if(fading) glColorMask(COLORMASK, GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spinsky*lastmillis/1000.0f+yawsky)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        gle::color(vec::hexcolor(skycolour), skyblend);
        drawenvbox(farplane/2, skyclip, topclip, yawskyfaces(renderedskyfaces, yawsky, spinsky), sky);

        if(blendsky) glDisable(GL_BLEND);
    }

    if(!glaring && fogdomemax && !fogdomeclouds)
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);
        drawfogdome(farplane);
        SETSHADER(skybox);
    }

    if((!glaring || cloudglare) && cloudbox[0])
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spinclouds*lastmillis/1000.0f+yawclouds)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        gle::color(vec::hexcolor(cloudcolour), cloudblend);
        drawenvbox(farplane/2, skyclip ? skyclip : cloudclip, topclip, yawskyfaces(renderedskyfaces, yawclouds, spinclouds), clouds);

        glDisable(GL_BLEND);
    }

    if((!glaring || cloudlayerglare) && cloudlayer[0] && cloudheight && renderedskyfaces&(cloudheight < 0 ? 0x1F : 0x2F))
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spincloudlayer*lastmillis/1000.0f+yawcloudlayer)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(farplane/2, cloudheight, cloudsubdiv, cloudfade, cloudscale, cloudoverlay, cloudlayercolour, cloudlayerblend, cloudoffsetx + cloudscrollx * lastmillis/1000.0f, cloudoffsety + cloudscrolly * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    if((!glaring || envlayerglare) && envlayer[0] && envheight && renderedskyfaces&(envheight < 0 ? 0x1F : 0x2F))
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix, skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((spinenvlayer*lastmillis/1000.0f+yawenvlayer)*-RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(farplane/2, envheight, envsubdiv, envfade, envscale, envoverlay, envlayercolour, envlayerblend, envoffsetx + envscrollx * lastmillis/1000.0f, envoffsety + envscrolly * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    if(!glaring && fogdomemax && fogdomeclouds)
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);
        drawfogdome(farplane);
    }

    if(clampsky) glDepthRange(0, 1);

    glDepthMask(GL_TRUE);

    if(limited)
    {
        if(explicitonly) glEnable(GL_DEPTH_TEST);
        else glDepthFunc(GL_LESS);
        if(!reflecting && !refracting && !drawtex && editmode && showsky) drawskyoutline();
    }
    else glDepthFunc(GL_LESS);
}

VARN(IDF_WORLD, skytexture, useskytexture, 0, 1, 1);

int explicitsky = 0;
double skyarea = 0;

bool limitsky()
{
    return (explicitsky && (useskytexture || editmode)) || (sparklyfix && skyarea / (double(hdr.worldsize)*double(hdr.worldsize)*6) < 0.9);
}

