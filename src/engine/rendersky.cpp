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
            conoutf(colourred, "Could not load side %s of sky texture %s", side, basename);
        }
    }
}

Texture *cloudoverlay = NULL, *envoverlay = NULL, *cloudcylinderoverlay = NULL, *envcylinderoverlay = NULL;

Texture *loadskyoverlay(const char *basename)
{
    Texture *t = textureload(basename, 0, true, false);
    if(t == notexture) conoutf(colourred, "Could not load sky overlay texture %s", basename);
    return t;
}

#define MPVLAYER(prefix, name, type) \
    SVARF(IDF_MAP, prefix##layer##name, "", { if(prefix##layer##name[0] && checkmapvariant(type)) prefix##overlay = loadskyoverlay(prefix##layer##name); }); \
    CVAR(IDF_MAP, prefix##layercolour##name, 0xFFFFFF); \
    FVAR(IDF_MAP, prefix##layerblend##name, 0, 1.0f, 1); \
    FVAR(IDF_MAP, prefix##offsetx##name, 0, 0, 1); \
    FVAR(IDF_MAP, prefix##offsety##name, 0, 0, 1); \
    FVAR(IDF_MAP, prefix##scrollx##name, -16, 0, 16); \
    FVAR(IDF_MAP, prefix##scrolly##name, -16, 0, 16); \
    FVAR(IDF_MAP, prefix##scale##name, FVAR_NONZERO, 1, 64); \
    FVAR(IDF_MAP, spin##prefix##layer##name, -720, 0, 720); \
    VAR(IDF_MAP, yaw##prefix##layer##name, 0, 0, 360); \
    FVAR(IDF_MAP, prefix##height##name, -2, 0.2f, 2); \
    FVAR(IDF_MAP, prefix##fade##name, 0, 0.2f, 1); \
    VAR(IDF_MAP, prefix##subdiv##name, 4, 16, 64); \
    VAR(IDF_MAP, prefix##farplane##name, 0, 1, 1); \
    VAR(IDF_MAP, prefix##shadow##name, 0, 0, 1); \
    FVAR(IDF_MAP, prefix##shadowblend##name, 0, 0.66f, 1); \

#define MPVCYLINDER(prefix, name, type) \
    MPVLAYER(prefix, name, type); \
    FVAR(IDF_MAP, prefix##dist##name, FVAR_NONZERO, 1, 1); \
    VAR(IDF_MAP, prefix##repeat##name, -64, 2, 64);

#define MPVVARS(name, type) \
    CVAR(IDF_MAP, ambient##name, 0x191919); \
    FVAR(IDF_MAP, ambientscale##name, 0, 1, 16); \
    CVAR(IDF_MAP, skylight##name, 0); \
    FVAR(IDF_MAP, skylightscale##name, 0, 1, 16); \
    VAR(IDF_MAP, fog##name, 16, 4000, 1000024); \
    CVAR(IDF_MAP, fogcolour##name, 0x8099B3); \
    CVAR(IDF_MAP, skybgcolour##name, 0x000000); \
    SVARF(IDF_MAP, skybox##name, "", { if(skybox##name[0] && checkmapvariant(type)) loadsky(skybox##name, sky); }); \
    CVAR(IDF_MAP, skycolour##name, 0xFFFFFF); \
    FVAR(IDF_MAP, skyblend##name, 0, 1.0f, 1); \
    FVAR(IDF_MAP, skyoverbright##name, 1, 2, 16); \
    FVAR(IDF_MAP, skyoverbrightmin##name, 0, 1, 16); \
    FVAR(IDF_MAP, skyoverbrightthreshold##name, 0, 0.7f, 1); \
    FVAR(IDF_MAP, spinsky##name, -720, 0, 720); \
    FVAR(IDF_MAP, spinskypitch##name, -720, 0, 720); \
    FVAR(IDF_MAP, spinskyroll##name, -720, 0, 720); \
    VAR(IDF_MAP, yawsky##name, 0, 0, 360); \
    VAR(IDF_MAP, pitchsky##name, 0, 0, 360); \
    VAR(IDF_MAP, rollsky##name, 0, 0, 360); \
    SVARF(IDF_MAP, cloudbox##name, "", { if(cloudbox##name[0] && checkmapvariant(type)) loadsky(cloudbox##name, clouds); }); \
    CVAR(IDF_MAP, cloudcolour##name, 0xFFFFFF); \
    FVAR(IDF_MAP, cloudblend##name, 0, 1.0f, 1); \
    FVAR(IDF_MAP, spinclouds##name, -720, 0, 720); \
    FVAR(IDF_MAP, spincloudspitch##name, -720, 0, 720); \
    FVAR(IDF_MAP, spincloudsroll##name, -720, 0, 720); \
    VAR(IDF_MAP, yawclouds##name, 0, 0, 360); \
    VAR(IDF_MAP, pitchclouds##name, 0, 0, 360); \
    VAR(IDF_MAP, rollclouds##name, 0, 0, 360); \
    FVAR(IDF_MAP, cloudclip##name, 0, 0.5f, 1); \
    VAR(IDF_MAP, atmo##name, 0, 1, 2); \
    VAR(IDF_MAP, atmostyle##name, 0, 0, 1); \
    FVAR(IDF_MAP, atmoplanetsize##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_MAP, atmoheight##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_MAP, atmobright##name, 0, 1, 16); \
    FVAR(IDF_MAP|IDF_READONLY, atmoclarity##name, 0, 0, 10); /* old map compat for fixatmo, don't use */ \
    CVAR(IDF_MAP, atmolight##name, 0); \
    FVAR(IDF_MAP, atmolightscale##name, 0, 1, 16); \
    FVAR(IDF_MAP, atmohaze##name, 0, 0.1f, 100); \
    FVAR(IDF_MAP, atmodensity##name, 0, 1, 100); \
    FVAR(IDF_MAP, atmoozone##name, 0, 1, 100); \
    FVAR(IDF_MAP, atmoblend##name, 0, 1, 1); \
    CVAR(IDF_MAP, atmodisk##name, 0); \
    FVAR(IDF_MAP, atmodisksize##name, 0, 12, 90); \
    FVAR(IDF_MAP, atmodiskcorona##name, 0, 0.4f, 1); \
    FVAR(IDF_MAP, atmodiskbright##name, 0, 1, 16); \
    FVAR(IDF_MAP, fogdomeheight##name, -1, -0.5f, 1); \
    FVAR(IDF_MAP, fogdomemin##name, 0, 0, 1); \
    FVAR(IDF_MAP, fogdomemax##name, 0, 0, 1); \
    VAR(IDF_MAP, fogdomecap##name, 0, 1, 1); \
    FVAR(IDF_MAP, fogdomeclip##name, 0, 1, 1); \
    CVAR(IDF_MAP, fogdomecolour##name, 0xFFFFFF); \
    VAR(IDF_MAP, fogdomeclouds##name, 0, 1, 1); \
    VAR(IDF_MAP, fogdomesquare##name, 0, 0, 1); \
    VAR(IDF_MAP, skytexture##name, 0, 0, 1); \
    VARF(IDF_MAP, skyshadow##name, 0, 0, 1, if(checkmapvariant(type)) clearshadowcache()); \
    MPVLAYER(cloud, name, type) MPVCYLINDER(cloudcylinder, name, type) \
    MPVLAYER(env, name, type) MPVCYLINDER(envcylinder, name, type)

MPVVARS(, MPV_DEFAULT);
MPVVARS(alt, MPV_ALTERNATE);

#define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
        return name; \
    }

#define GETMPVDARK(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt * game::darkness(DARK_ENV); \
        return name * game::darkness(DARK_ENV); \
    }

#define GETMPVDARKSUN(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt * game::darkness(DARK_SUN); \
        return name * game::darkness(DARK_SUN); \
    }

#define GETMPVDARKCOL(name, type) \
    type get##name() \
    { \
        static bvec res; \
        res = bvec(checkmapvariant(MPV_ALTERNATE) ? name##alt : name).mul(game::darkness(DARK_ENV)); \
        return res; \
    }

#define GETMPVDARKSUNCOL(name, type) \
    type get##name() \
    { \
        static bvec res; \
        res = bvec(checkmapvariant(MPV_ALTERNATE) ? name##alt : name).mul(game::darkness(DARK_SUN)); \
        return res; \
    }

GETMPV(ambient, const bvec &);
GETMPVDARK(ambientscale, float);
GETMPV(skylight, const bvec &);
GETMPVDARK(skylightscale, float);

GETMPV(fog, int);
GETMPVDARKCOL(fogcolour, const bvec &);

GETMPVDARKCOL(skybgcolour, const bvec &);
GETMPV(skybox, const char *);
GETMPVDARKCOL(skycolour, const bvec &);
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
GETMPVDARKCOL(cloudcolour, const bvec &);
GETMPV(cloudblend, float);
GETMPV(spinclouds, float);
GETMPV(spincloudspitch, float);
GETMPV(spincloudsroll, float);
GETMPV(yawclouds, int);
GETMPV(pitchclouds, int);
GETMPV(rollclouds, int);
GETMPV(cloudclip, float);
GETMPV(atmo, int);
GETMPV(atmostyle, int);
GETMPV(atmoplanetsize, float);
GETMPV(atmoheight, float);
GETMPV(atmobright, float);
GETMPV(atmolight, const bvec &);
GETMPVDARKSUN(atmolightscale, float);
GETMPV(atmohaze, float);
GETMPV(atmoclarity, float);
GETMPV(atmodensity, float);
GETMPV(atmoozone, float);
GETMPV(atmoblend, float);
GETMPVDARKSUNCOL(atmodisk, const bvec &);
GETMPV(atmodisksize, float);
GETMPV(atmodiskcorona, float);
GETMPV(atmodiskbright, float);
GETMPV(fogdomeheight, float);
GETMPV(fogdomemin, float);
GETMPV(fogdomemax, float);
GETMPV(fogdomecap, int);
GETMPV(fogdomeclip, float);
GETMPVDARKCOL(fogdomecolour, const bvec &);
GETMPV(fogdomeclouds, int);
GETMPV(skytexture, int);
GETMPV(skyshadow, int);

#define GETLAYER(prefix) \
    GETMPV(prefix##layer, const char *); \
    GETMPVDARKCOL(prefix##layercolour, const bvec &); \
    GETMPV(prefix##layerblend, float); \
    GETMPV(prefix##offsetx, float); \
    GETMPV(prefix##offsety, float); \
    GETMPV(prefix##scrollx, float); \
    GETMPV(prefix##scrolly, float); \
    GETMPV(prefix##scale, float); \
    GETMPV(spin##prefix##layer, float); \
    GETMPV(yaw##prefix##layer, int); \
    GETMPV(prefix##height, float); \
    GETMPV(prefix##fade, float); \
    GETMPV(prefix##subdiv, int); \
    GETMPV(prefix##farplane, int); \
    GETMPV(prefix##shadow, int); \
    GETMPV(prefix##shadowblend, float); \

#define GETCYLINDER(prefix) \
    GETLAYER(prefix) \
    GETMPV(prefix##dist, float); \
    GETMPV(prefix##repeat, int);

GETLAYER(cloud);
GETCYLINDER(cloudcylinder);
GETLAYER(env);
GETCYLINDER(envcylinder);

void drawenvboxface(float s0, float t0, int x0, int y0, int z0,
                    float s1, float t1, int x1, int y1, int z1,
                    float s2, float t2, int x2, int y2, int z2,
                    float s3, float t3, int x3, int y3, int z3,
                    Texture *tex)
{
    settexture(tex);
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

void drawenvoverlay(Texture *overlay, float height, int subdiv, float fade, float scale, const bvec &colour, float blend, float tx = 0, float ty = 0)
{
    int w = farplane/2;
    float z = w*height, tsz = 0.5f*(1-fade)/scale, psz = w*(1-fade);
    settexture(overlay);
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

void drawenvcylinder(Texture *overlay, float height, int subdiv, int repeat, float fade, float scale, float dist, const bvec &colour, float blend, float tx = 0, float ty = 0)
{
    bool invertx = repeat < 0, inverty = height < 0.f;
    int reps = clamp(abs(repeat), 1, 64), divisor = subdiv * reps, w = farplane / 2;
    float section = 1.0f / subdiv, z = w * fabs(height), xy = w * dist,
          tsy1 = 0.5f * (1 - fade) / scale, tsy2 = 0.5f * fade / scale,
          psz1 = z * (1 - fade), psz2 = z * fade;
    settexture(overlay);
    gle::defvertex();
    gle::deftexcoord0();
    gle::defcolor(4);
    loopk(3)
    {
        vec color = colour.tocolor();
        gle::begin(GL_TRIANGLE_STRIP);
        float zpos = 0, zsize = 0, typos = ty, tysize = 0;
        switch(k)
        {
            case 0: default:
                zpos = 0;
                zsize = psz1 * 0.5f;
                tysize = tsy1 * 0.5f;
                break;
            case 1:
                zpos = psz1 * 0.5f + psz2 * 0.5f;
                zsize = psz2 * 0.5f;
                if(inverty) typos -= tsy1 * 0.5f - tsy2 * 0.5f;
                else typos += tsy1 * 0.5f + tsy2 * 0.5f;
                tysize = tsy2 * 0.5f;
                break;
            case 2:
                zpos = 0 - psz1 * 0.5f - psz2 * 0.5f;
                zsize = psz2 * 0.5f;
                if(inverty) typos += tsy1 * 0.5f + tsy2 * 0.5f;
                else typos -= tsy1 * 0.5f - tsy2 * 0.5f;
                tysize = tsy2 * 0.5f;
                break;
        }
        if(zsize <= 0 || tysize <= 0) continue;
        loopi(divisor+1)
        {
            vec p(1, 1, 0);
            p.rotate_around_z(((invertx ? -2.0f : 2.0f) * M_PI * i) / divisor).mul(xy);
            float zpos1 = zpos + zsize, zpos2 = zpos - zsize,
                  txpos = tx + section * i, typos1 = inverty ? typos + tysize : typos - tysize, typos2 = inverty ? typos - tysize : typos + tysize;
            loopj(2)
            {
                gle::attribf(p.x, p.y, zpos1);
                    gle::attribf(txpos, typos1);
                    gle::attrib(color, k == 1 ? 0.0f : blend);
                gle::attribf(p.x, p.y, zpos2);
                    gle::attribf(txpos, typos2);
                    gle::attrib(color, k == 2 ? 0.0f : blend);
            }
        }
        xtraverts += gle::end();
    }
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
        verts = new vert[tris+1 + (capsize >= 0 ? 1 : 0)];
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
        bvec color = !getfogdomecolour().iszero() ? getfogdomecolour() : getfogcolour();
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
    SETVARIANT(skyfog, fogdomesquare ? 0 : -1, 0);

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

FVAR(IDF_PERSIST, atmodither, 0, 0.008f, 1.0f);

static void drawatmosphere()
{
    bool diskonly = getatmostyle() == 1, blended = diskonly || getatmoblend() < 1;
    if(blended)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    vec sundir = getpielightdir();
    if(diskonly) SETSHADER(atmospheredisk);
    else SETSHADER(atmosphere);

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

    LOCALPARAMF(atmodither, atmodither);

    gle::defvertex();
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(-1, 1, 1);
    gle::attribf(1, 1, 1);
    gle::attribf(-1, -1, 1);
    gle::attribf(1, -1, 1);
    xtraverts += gle::end();

    if(blended) glDisable(GL_BLEND);
}

VAR(0, showsky, 0, 1, 1);
VAR(0, clampsky, 0, 1, 1);
VAR(0, cloudshadowclamp, 0, 1, 1);

int explicitsky = 0;

bool limitsky()
{
    return explicitsky && (getskytexture() || (editmode && showsky));
}

bool hasenvshadow()
{
    return getcloudshadow() || getenvshadow();
}

void drawenvlayer(Texture *tex, float height, const bvec &colour, float blend, float subdiv, float fade, float scale, float offsetx, float offsety, float shadowblend, float zrot, bool skyplane, bool shadowpass, int cylinder, float dist)
{
    if(shadowpass) SETSHADER(skyboxshadow);
    else SETSHADER(skybox);
    glDisable(GL_CULL_FACE);
    if(shadowpass)
    {
        if(skyplane) glDisable(GL_DEPTH_TEST);
        if(hasDC && cloudshadowclamp) glEnable(GL_DEPTH_CLAMP);
        matrix4 skymatrix = shadowmatrix;
        if(!skyplane) skymatrix.translate(worldsize*0.5f, worldsize*0.5f, 0);
        skymatrix.rotate_around_z(zrot);
        LOCALPARAM(skymatrix, skymatrix);
        LOCALPARAMF(shadowstrength, shadowblend);
    }
    else
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        matrix4 skymatrix = cammatrix, skyprojmatrix;
        if(skyplane) skymatrix.settranslation(0, 0, 0);
        else
        {
            skymatrix.translate(worldsize*0.5f, worldsize*0.5f, 0);
            // if(renderfbo) blend *= 0.125f; // hack to prevent overbrightening in FBO mode
        }
        skymatrix.rotate_around_z(zrot);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);
    }
    if(cylinder) drawenvcylinder(tex, height, subdiv, cylinder, fade, scale, dist, colour, blend, offsetx, offsety);
    else drawenvoverlay(tex, height, subdiv, fade, scale, colour, blend, offsetx, offsety);
    if(shadowpass)
    {
        if(hasDC && cloudshadowclamp) glDisable(GL_DEPTH_CLAMP);
        if(skyplane) glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    glEnable(GL_CULL_FACE);
}

#define ENVLAYER(name) \
    const char *cur##name##layer = get##name##layer(); \
    if(cur##name##layer[0] && get##name##height() && (!shadowpass || get##name##shadow()) && get##name##farplane() == (skyplane ? 1 : 0)) \
        drawenvlayer(name##overlay, get##name##height(), get##name##layercolour(), get##name##layerblend(), get##name##subdiv(), get##name##fade(), get##name##scale(), \
            get##name##offsetx() + get##name##scrollx() * lastmillis/1000.0f, get##name##offsety() + get##name##scrolly() * lastmillis/1000.0f, \
            get##name##shadowblend(), (getspin##name##layer()*lastmillis/1000.0f+getyaw##name##layer())*-RAD, skyplane, shadowpass);

#define ENVCYLINDER(name) \
    const char *cur##name##layer = get##name##layer(); \
    if(cur##name##layer[0] && get##name##height() && (!shadowpass || get##name##shadow()) && get##name##farplane() == (skyplane ? 1 : 0)) \
        drawenvlayer(name##overlay, get##name##height(), get##name##layercolour(), get##name##layerblend(), get##name##subdiv(), get##name##fade(), get##name##scale(), \
            get##name##offsetx() + get##name##scrollx() * lastmillis/1000.0f, get##name##offsety() + get##name##scrolly() * lastmillis/1000.0f, \
            get##name##shadowblend(), (getspin##name##layer()*lastmillis/1000.0f+getyaw##name##layer())*-RAD, skyplane, shadowpass, get##name##repeat(), get##name##dist());

void drawenvlayers(bool skyplane, bool shadowpass)
{
    ENVLAYER(cloud);
    ENVCYLINDER(cloudcylinder);
    ENVLAYER(env);
    ENVCYLINDER(envcylinder);
    physics::drawenvlayers(skyplane, shadowpass);
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
    else if(blendsky && (!getatmo() || getatmostyle() == 1 || getatmoblend() < 1))
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

    drawenvlayers(true);

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
    const char *curcloudcylinderlayer = getcloudcylinderlayer();
    if(curcloudcylinderlayer[0]) cloudcylinderoverlay = loadskyoverlay(curcloudcylinderlayer);

    const char *curenvlayer = getenvlayer();
    if(curenvlayer[0]) envoverlay = loadskyoverlay(curenvlayer);
    const char *curenvcylinderlayer = getenvcylinderlayer();
    if(curenvcylinderlayer[0]) envcylinderoverlay = loadskyoverlay(curenvcylinderlayer);
}
