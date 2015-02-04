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
            defformatstring(ext)("_%s", side);
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

void draw_envbox_face(float s0, float t0, int x0, int y0, int z0,
                      float s1, float t1, int x1, int y1, int z1,
                      float s2, float t2, int x2, int y2, int z2,
                      float s3, float t3, int x3, int y3, int z3,
                      GLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
    glTexCoord2f(s0, t0); glVertex3f(x0, y0, z0);
    glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
    glEnd();
    xtraverts += 4;
}

void draw_envbox_quad(int x0, int y0, int z0,
                      int x1, int y1, int z1,
                      int x2, int y2, int z2,
                      int x3, int y3, int z3)
{
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(x3, y3, z3);
    glVertex3f(x2, y2, z2);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y1, z1);
    glEnd();
    xtraverts += 4;
}

void draw_envbox(int w, float z1clip = 0.0f, float z2clip = 1.0f, int faces = 0x3F, Texture **sky = NULL)
{
    if(z1clip >= z2clip) return;

    float v1 = 1-z1clip, v2 = 1-z2clip;
    int z1 = int(ceil(2*w*(z1clip-0.5f))), z2 = int(ceil(2*w*(z2clip-0.5f)));

    if(faces&0x01)
        draw_envbox_face(0.0f, v2,  -w, -w, z2,
                         1.0f, v2,  -w,  w, z2,
                         1.0f, v1,  -w,  w, z1,
                         0.0f, v1,  -w, -w, z1, sky && sky[0] ? sky[0]->id : notexture->id);

    if(faces&0x02)
        draw_envbox_face(1.0f, v1, w, -w, z1,
                         0.0f, v1, w,  w, z1,
                         0.0f, v2, w,  w, z2,
                         1.0f, v2, w, -w, z2, sky && sky[1] ? sky[1]->id : notexture->id);

    if(faces&0x04)
        draw_envbox_face(1.0f, v1, -w, -w, z1,
                         0.0f, v1,  w, -w, z1,
                         0.0f, v2,  w, -w, z2,
                         1.0f, v2, -w, -w, z2, sky && sky[2] ? sky[2]->id : notexture->id);

    if(faces&0x08)
        draw_envbox_face(1.0f, v1,  w,  w, z1,
                         0.0f, v1, -w,  w, z1,
                         0.0f, v2, -w,  w, z2,
                         1.0f, v2,  w,  w, z2, sky && sky[3] ? sky[3]->id : notexture->id);

    if(z1clip <= 0 && faces&0x10)
        draw_envbox_face(0.0f, 1.0f, -w,  w,  -w,
                         0.0f, 0.0f,  w,  w,  -w,
                         1.0f, 0.0f,  w, -w,  -w,
                         1.0f, 1.0f, -w, -w,  -w, sky && sky[4] ? sky[4]->id : notexture->id);

    if(z2clip >= 1 && faces&0x20)
        draw_envbox_face(0.0f, 1.0f,  w,  w, w,
                         0.0f, 0.0f, -w,  w, w,
                         1.0f, 0.0f, -w, -w, w,
                         1.0f, 1.0f,  w, -w, w, sky && sky[5] ? sky[5]->id : notexture->id);
}

void draw_envbox_bg(int w, float z1clip = 0.0f, float z2clip = 1.0f, int faces = 0x3F)
{
    if(z1clip >= z2clip) return;

    int z1 = int(ceil(2*w*(z1clip-0.5f))), z2 = int(ceil(2*w*(z2clip-0.5f)));
    if(faces&0x01) draw_envbox_quad(-w, -w, z2, -w,  w, z2, -w,  w, z1, -w, -w, z1);
    if(faces&0x02) draw_envbox_quad( w, -w, z1,  w,  w, z1,  w,  w, z2,  w, -w, z2);
    if(faces&0x04) draw_envbox_quad(-w, -w, z1,  w, -w, z1,  w, -w, z2, -w, -w, z2);
    if(faces&0x08) draw_envbox_quad( w,  w, z1, -w,  w, z1, -w,  w, z2,  w,  w, z2);
    if(z1clip <= 0 && faces&0x10) draw_envbox_quad(-w,  w, -w,  w,  w, -w,  w, -w, -w, -w, -w, -w);
    if(z2clip >= 1 && faces&0x20) draw_envbox_quad( w,  w,  w, -w,  w,  w, -w, -w,  w,  w, -w,  w);
}

void draw_env_overlay(int w, float height, int subdiv, float fade, float scale, Texture *overlay = NULL, int colour = 0xFFFFFF, float a = 1.f, float tx = 0, float ty = 0)
{
    float z = w*height, tsz = 0.5f*(1-fade)/scale, psz = w*(1-fade);
    glBindTexture(GL_TEXTURE_2D, overlay ? overlay->id : notexture->id);
    float r = (colour>>16)/255.0f, g = ((colour>>8)&255)/255.0f, b = (colour&255)/255.0f;
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    loopi(subdiv+1)
    {
        vec p(1, 1, 0);
        p.rotate_around_z((-2.0f*M_PI*i)/subdiv);
        glTexCoord2f(tx + p.x*tsz, ty + p.y*tsz); glVertex3f(p.x*psz, p.y*psz, z);
    }
    glEnd();
    float tsz2 = 0.5f/scale;
    glBegin(GL_TRIANGLE_STRIP);
    loopi(subdiv+1)
    {
        vec p(1, 1, 0);
        p.rotate_around_z((-2.0f*M_PI*i)/subdiv);
        glColor4f(r, g, b, a);
        glTexCoord2f(tx + p.x*tsz, ty + p.y*tsz); glVertex3f(p.x*psz, p.y*psz, z);
        glColor4f(r, g, b, 0);
        glTexCoord2f(tx + p.x*tsz2, ty + p.y*tsz2); glVertex3f(p.x*w, p.y*w, z);
    }
    glEnd();
}

static struct domevert
{
    vec pos;
    bvec4 color;

    domevert() {}
    domevert(const vec &pos, const bvec &fcolor, float alpha) : pos(pos), color(fcolor, uchar(alpha*255))
    {
    }
    domevert(const domevert &v0, const domevert &v1) : pos(vec(v0.pos).add(v1.pos).normalize()), color(v0.color)
    {
        if(v0.pos.z != v1.pos.z) color.a += uchar((v1.color.a - v0.color.a) * (pos.z - v0.pos.z) / (v1.pos.z - v0.pos.z));
    }
} *domeverts = NULL;
static GLushort *domeindices = NULL;
static int domenumverts = 0, domenumindices = 0, domecapindices = 0;
static GLuint domevbuf = 0, domeebuf = 0;
static bvec domecolor(0, 0, 0);
static float domeminalpha = 0, domemaxalpha = 0, domecapsize = -1, domeclipz = 1;

static void subdivide(int depth, int face);

static void genface(int depth, int i1, int i2, int i3)
{
    int face = domenumindices; domenumindices += 3;
    domeindices[face]   = i3;
    domeindices[face+1] = i2;
    domeindices[face+2] = i1;
    subdivide(depth, face);
}

static void subdivide(int depth, int face)
{
    if(depth-- <= 0) return;
    int idx[6];
    loopi(3) idx[i] = domeindices[face+2-i];
    loopi(3)
    {
        int vert = domenumverts++;
        domeverts[vert] = domevert(domeverts[idx[i]], domeverts[idx[(i+1)%3]]); //push on to unit sphere
        idx[3+i] = vert;
        domeindices[face+2-i] = vert;
    }
    subdivide(depth, face);
    loopi(3) genface(depth, idx[i], idx[3+i], idx[3+(i+2)%3]);
}

static int sortdomecap(GLushort x, GLushort y)
{
    const vec &xv = domeverts[x].pos, &yv = domeverts[y].pos;
    return xv.y < 0 ? yv.y >= 0 || xv.x < yv.x : yv.y >= 0 && xv.x > yv.x;
}

static void initdome(const bvec &color, float minalpha = 0.0f, float maxalpha = 1.0f, float capsize = -1, float clipz = 1, int hres = 16, int depth = 2)
{
    const int tris = hres << (2*depth);
    domenumverts = domenumindices = domecapindices = 0;
    DELETEA(domeverts);
    DELETEA(domeindices);
    domeverts = new domevert[tris+1 + (capsize >= 0 ? 1 : 0)];
    domeindices = new GLushort[(tris + (capsize >= 0 ? hres<<depth : 0))*3];
    if(clipz >= 1)
    {
        domeverts[domenumverts++] = domevert(vec(0.0f, 0.0f, 1.0f), color, minalpha); //build initial 'hres' sided pyramid
        loopi(hres) domeverts[domenumverts++] = domevert(vec(sincos360[(360*i)/hres], 0.0f), color, maxalpha);
        loopi(hres) genface(depth, 0, i+1, 1+(i+1)%hres);
    }
    else if(clipz <= 0)
    {
        loopi(hres<<depth) domeverts[domenumverts++] = domevert(vec(sincos360[(360*i)/(hres<<depth)], 0.0f), color, maxalpha);
    }
    else
    {
        float clipxy = sqrtf(1 - clipz*clipz);
        const vec2 &scm = sincos360[180/hres];
        loopi(hres)
        {
            const vec2 &sc = sincos360[(360*i)/hres];
            domeverts[domenumverts++] = domevert(vec(sc.x*clipxy, sc.y*clipxy, clipz), color, minalpha);
            domeverts[domenumverts++] = domevert(vec(sc.x, sc.y, 0.0f), color, maxalpha);
            domeverts[domenumverts++] = domevert(vec(sc.x*scm.x - sc.y*scm.y, sc.y*scm.x + sc.x*scm.y, 0.0f), color, maxalpha);
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
        GLushort *domecap = &domeindices[domenumindices];
        int domecapverts = 0;
        loopi(domenumverts) if(!domeverts[i].pos.z) domecap[domecapverts++] = i;
        domeverts[domenumverts++] = domevert(vec(0.0f, 0.0f, -capsize), color, maxalpha);
        quicksort(domecap, domecapverts, sortdomecap);
        loopi(domecapverts)
        {
            int n = domecapverts-1-i;
            domecap[n*3] = domecap[n];
            domecap[n*3+1] = domecap[(n+1)%domecapverts];
            domecap[n*3+2] = domenumverts-1;
            domecapindices += 3;
        }
    }

    if(hasVBO)
    {
        if(!domevbuf) glGenBuffers_(1, &domevbuf);
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, domevbuf);
        glBufferData_(GL_ARRAY_BUFFER_ARB, domenumverts*sizeof(domevert), domeverts, GL_STATIC_DRAW_ARB);
        DELETEA(domeverts);

        if(!domeebuf) glGenBuffers_(1, &domeebuf);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, domeebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, (domenumindices + domecapindices)*sizeof(GLushort), domeindices, GL_STATIC_DRAW_ARB);
        DELETEA(domeindices);
    }
}

static void deletedome()
{
    domenumverts = domenumindices = 0;
    if(domevbuf) { glDeleteBuffers_(1, &domevbuf); domevbuf = 0; }
    if(domeebuf) { glDeleteBuffers_(1, &domeebuf); domeebuf = 0; }
    DELETEA(domeverts);
    DELETEA(domeindices);
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

static void drawdome()
{
    float capsize = fogdomecap && fogdomeheight < 1 ? (1 + fogdomeheight) / (1 - fogdomeheight) : -1;
    bvec color = fogdomecolour ? fogdomecolor : fogcolor;
    if(!domenumverts || domecolor != color || domeminalpha != fogdomemin || domemaxalpha != fogdomemax || domecapsize != capsize || domeclipz != fogdomeclip)
    {
        initdome(color, min(fogdomemin, fogdomemax), fogdomemax, capsize, fogdomeclip);
        domecolor = color;
        domeminalpha = fogdomemin;
        domemaxalpha = fogdomemax;
        domecapsize = capsize;
        domeclipz = fogdomeclip;
    }

    if(hasVBO)
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, domevbuf);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, domeebuf);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(domevert), &domeverts->pos);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(domevert), &domeverts->color);

    if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, 0, domenumverts-1, domenumindices + fogdomecap*domecapindices, GL_UNSIGNED_SHORT, domeindices);
    else glDrawElements(GL_TRIANGLES, domenumindices + fogdomecap*domecapindices, GL_UNSIGNED_SHORT, domeindices);
    xtraverts += domenumverts;
    glde++;

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    if(hasVBO)
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    }
}

void cleanupsky()
{
    deletedome();
}

VAR(IDF_PERSIST, sparklyfix, 0, 0, 1);
VAR(0, showsky, 0, 1, 1);
VAR(0, clipsky, 0, 1, 1);

bool drawskylimits(bool explicitonly)
{
    nocolorshader->set();

    glDisable(GL_TEXTURE_2D);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    bool rendered = rendersky(explicitonly);
    glColorMask(COLORMASK, GL_TRUE);
    glEnable(GL_TEXTURE_2D);

    return rendered;
}

void drawskyoutline()
{
    lineshader->set();

    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    extern int wireframe;
    if(!wireframe)
    {
        enablepolygonoffset(GL_POLYGON_OFFSET_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    glColor3f(0.5f, 0.0f, 0.5f);
    rendersky(true);
    if(!wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);

    if(!glaring) defaultshader->set();
}

VAR(0, clampsky, 0, 1, 1);

VAR(IDF_WORLD, fogdomeclouds, 0, 1, 1);

static void drawfogdome(int farplane)
{
    notextureshader->set();
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glLoadMatrixf(viewmatrix.v);
    glRotatef(camera1->roll, 0, 1, 0);
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw, 0, 0, -1);
    if(reflecting) glScalef(1, 1, -1);
    glTranslatef(0, 0, farplane*fogdomeheight*0.5f);
    glScalef(farplane/2, farplane/2, farplane*(0.5f - fogdomeheight*0.5f));
    drawdome();
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
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
        extern vtxarray *visibleva;
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

    glDisable(GL_FOG);

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
        glDisable(GL_TEXTURE_2D);
        notextureshader->set();

        glPushMatrix();
        glLoadMatrixf(viewmatrix.v);
        glRotatef(camera1->roll, 0, 1, 0);
        glRotatef(camera1->pitch, -1, 0, 0);
        glRotatef(camera1->yaw, 0, 0, -1);
        if(reflecting) glScalef(1, 1, -1);
        glColor3f((skybgcolour>>16)/255.0f, ((skybgcolour>>8)&255)/255.0f, (skybgcolour&255)/255.0f);
        draw_envbox_bg(farplane/2, skyclip, topclip, yawskyfaces(renderedskyfaces, yawsky, spinsky));
        glPopMatrix();

        glEnable(GL_TEXTURE_2D);
    }

    if(glaring) SETSHADER(skyboxglare);
    else defaultshader->set();

    if((!glaring || skyglare) && skybox[0])
    {
        if(blendsky)
        {
            if(fading) glColorMask(COLORMASK, GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glPushMatrix();
        glLoadMatrixf(viewmatrix.v);
        glRotatef(camera1->roll, 0, 1, 0);
        glRotatef(camera1->pitch, -1, 0, 0);
        glRotatef(camera1->yaw+spinsky*lastmillis/1000.0f+yawsky, 0, 0, -1);
        if(reflecting) glScalef(1, 1, -1);
        glColor4f((skycolour>>16)/255.0f, ((skycolour>>8)&255)/255.0f, (skycolour&255)/255.0f, skyblend);
        draw_envbox(farplane/2, skyclip, topclip, yawskyfaces(renderedskyfaces, yawsky, spinsky), sky);
        glPopMatrix();

        if(blendsky) glDisable(GL_BLEND);
    }

    if(!glaring && fogdomemax && !fogdomeclouds)
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);
        drawfogdome(farplane);
        defaultshader->set();
    }

    if((!glaring || cloudglare) && cloudbox[0])
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glLoadMatrixf(viewmatrix.v);
        glRotatef(camera1->roll, 0, 1, 0);
        glRotatef(camera1->pitch, -1, 0, 0);
        glRotatef(camera1->yaw+spinclouds*lastmillis/1000.0f+yawclouds, 0, 0, -1);
        if(reflecting) glScalef(1, 1, -1);
        glColor4f((cloudcolour>>16)/255.0f, ((cloudcolour>>8)&255)/255.0f, (cloudcolour&255)/255.0f, cloudblend);
        draw_envbox(farplane/2, skyclip ? skyclip : cloudclip, topclip, yawskyfaces(renderedskyfaces, yawclouds, spinclouds), clouds);
        glPopMatrix();

        glDisable(GL_BLEND);
    }

    if((!glaring || cloudlayerglare) && cloudlayer[0] && cloudheight && renderedskyfaces&(cloudheight < 0 ? 0x1F : 0x2F))
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glLoadMatrixf(viewmatrix.v);
        glRotatef(camera1->roll, 0, 1, 0);
        glRotatef(camera1->pitch, -1, 0, 0);
        glRotatef(camera1->yaw+spincloudlayer*lastmillis/1000.0f+yawcloudlayer, 0, 0, -1);
        if(reflecting) glScalef(1, 1, -1);
        draw_env_overlay(farplane/2, cloudheight, cloudsubdiv, cloudfade, cloudscale, cloudoverlay, cloudlayercolour, cloudlayerblend, cloudoffsetx + cloudscrollx * lastmillis/1000.0f, cloudoffsety + cloudscrolly * lastmillis/1000.0f);
        glPopMatrix();

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }

    if((!glaring || envlayerglare) && envlayer[0] && envheight && renderedskyfaces&(envheight < 0 ? 0x1F : 0x2F))
    {
        if(fading) glColorMask(COLORMASK, GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glLoadMatrixf(viewmatrix.v);
        glRotatef(camera1->roll, 0, 1, 0);
        glRotatef(camera1->pitch, -1, 0, 0);
        glRotatef(camera1->yaw+spinenvlayer*lastmillis/1000.0f+yawenvlayer, 0, 0, -1);
        if(reflecting) glScalef(1, 1, -1);
        draw_env_overlay(farplane/2, envheight, envsubdiv, envfade, envscale, envoverlay, envlayercolour, envlayerblend, envoffsetx + envscrollx * lastmillis/1000.0f, envoffsety + envscrolly * lastmillis/1000.0f);
        glPopMatrix();

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
        if(!reflecting && !refracting && !envmapping && editmode && showsky) drawskyoutline();
    }
    else glDepthFunc(GL_LESS);

    glEnable(GL_FOG);
}

VARN(IDF_WORLD, skytexture, useskytexture, 0, 1, 1);

int explicitsky = 0;
double skyarea = 0;

bool limitsky()
{
    return (explicitsky && (useskytexture || editmode)) || (sparklyfix && skyarea / (double(hdr.worldsize)*double(hdr.worldsize)*6) < 0.9);
}

