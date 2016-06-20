#include "engine.h"
#include "rendertarget.h"

VAR(IDF_PERSIST, shadowmap, 0, 0, 1);

extern void cleanshadowmap();
VARF(IDF_PERSIST, shadowmapsize, 7, 9, 11, cleanshadowmap());
VAR(IDF_PERSIST, shadowmapradius, 64, 96, 256);
VAR(0, shadowmapheight, 0, 32, 128);
VAR(IDF_PERSIST, shadowmapdist, 128, 256, 512);
VARF(IDF_PERSIST, fpshadowmap, 0, 0, 1, cleanshadowmap());
VARF(IDF_PERSIST, shadowmapprecision, 0, 0, 1, cleanshadowmap());
bvec shadowmapambientcolor(0, 0, 0);
VARF(IDF_HEX|IDF_WORLD, shadowmapambient, 0, 0, 0xFFFFFF,
{
    if(shadowmapambient <= 255) shadowmapambient |= (shadowmapambient<<8) | (shadowmapambient<<16);
    shadowmapambientcolor = bvec((shadowmapambient>>16)&0xFF, (shadowmapambient>>8)&0xFF, shadowmapambient&0xFF);
});
VAR(IDF_PERSIST, shadowmapintensity, 0, 40, 100);

VAR(IDF_PERSIST, blurshadowmap, 0, 1, 3);
VAR(IDF_PERSIST, blursmsigma, 1, 100, 200);

#define SHADOWSKEW 0.7071068f

vec shadowoffset(0, 0, 0), shadowfocus(0, 0, 0), shadowdir(0, SHADOWSKEW, 1);
VAR(0, shadowmapcasters, 1, 0, 0);
float shadowmapmaxz = 0;

void guessshadowdir()
{
    extern int shadowmapangle;
    if(shadowmapangle) return;
    vec lightpos(0, 0, 0);
    int numlights = 0;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        switch(e.type)
        {
            case ET_LIGHT:
            {
                if(!e.attrs[0])
                {
                    lightpos.add(e.o);
                    numlights++;
                }
                break;
            }
            case ET_SUNLIGHT:
            {
                lightpos.add(vec(1, 1, 1).mul(hdr.worldsize/2).add(vec(e.attrs[0]*RAD, (e.attrs[1]+90)*RAD).mul(hdr.worldsize*2)));
                numlights++;
                break;
            }
        }
    }
    if(!numlights) return;
    lightpos.div(numlights);
    vec dir(lightpos);
    dir.sub(vec(1, 1, 1).mul(hdr.worldsize/2));
    dir.z = 0;
    if(dir.iszero()) return;
    dir.normalize();
    dir.mul(SHADOWSKEW);
    dir.z = 1;
    shadowdir = dir;
}

void setshadowdir(int angle)
{
    if(angle)
    {
        shadowdir = vec(0, SHADOWSKEW, 1);
        shadowdir.rotate_around_z(angle*RAD);
    }
    else guessshadowdir();
}

VARF(IDF_WORLD, shadowmapangle, 0, 0, 360, setshadowdir(shadowmapangle));

bool shadowmapping = false;
matrix4 shadowmatrix;

VAR(IDF_PERSIST, shadowmapbias, 0, 5, 1024);
VAR(IDF_PERSIST, shadowmappeelbias, 0, 20, 1024);
VAR(0, smdepthpeel, 0, 1, 1);
VAR(0, smoothshadowmappeel, 1, 0, 0);

static struct shadowmaptexture : rendertarget
{
    const GLenum *colorformats() const
    {
        static const GLenum rgbafmts[] = { GL_RGBA16F, GL_RGBA16, GL_RGBA, GL_RGBA8, GL_FALSE };
        return &rgbafmts[fpshadowmap && hasTF ? 0 : (shadowmapprecision ? 1 : 2)];
    }

    bool swaptexs() const { return true; }

    bool scissorblur(int &x, int &y, int &w, int &h)
    {
        x = max(int(floor((scissorx1+1)/2*vieww)) - 2*blursize, 2);
        y = max(int(floor((scissory1+1)/2*viewh)) - 2*blursize, 2);
        w = min(int(ceil((scissorx2+1)/2*vieww)) + 2*blursize, vieww-2) - x;
        h = min(int(ceil((scissory2+1)/2*viewh)) + 2*blursize, viewh-2) - y;
        return true;
    }

    bool scissorrender(int &x, int &y, int &w, int &h)
    {
        x = y = 2;
        w = vieww - 2*2;
        h = viewh - 2*2;
        return true;
    }

    void doclear()
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    bool dorender()
    {
        vec skewdir(shadowdir);
        skewdir.rotate_around_z(-camera1->yaw*RAD);

        vec dir;
        vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, dir);
        dir.z = 0;
        dir.mul(shadowmapradius);

        vec dirx, diry;
        vecfromyawpitch(camera1->yaw, 0, 0, 1, dirx);
        vecfromyawpitch(camera1->yaw, 0, 1, 0, diry);
        shadowoffset.x = -fmod(dirx.dot(camera1->o) - skewdir.x*camera1->o.z, 2.0f*shadowmapradius/vieww);
        shadowoffset.y = -fmod(diry.dot(camera1->o) - skewdir.y*camera1->o.z, 2.0f*shadowmapradius/viewh);

        shadowmatrix.ortho(-shadowmapradius, shadowmapradius, -shadowmapradius, shadowmapradius, -shadowmapdist, shadowmapdist);
        shadowmatrix.mul(matrix3(vec(1, 0, 0), vec(0, 1, 0), vec(skewdir.x, skewdir.y, 1)));
        shadowmatrix.translate(skewdir.x*shadowmapheight + shadowoffset.x, skewdir.y*shadowmapheight + shadowoffset.y + dir.magnitude(), -shadowmapheight);
        shadowmatrix.rotate_around_z((camera1->yaw+180)*-RAD);
        shadowmatrix.translate(vec(camera1->o).neg());
        GLOBALPARAM(shadowmatrix, shadowmatrix);

        shadowfocus = camera1->o;
        shadowfocus.add(dir);
        shadowfocus.add(vec(shadowdir).mul(shadowmapheight));
        shadowfocus.add(dirx.mul(shadowoffset.x));
        shadowfocus.add(diry.mul(shadowoffset.y));

        gle::colorf(0, 0, 0);

        GLOBALPARAMF(shadowmapbias, -shadowmapbias/float(shadowmapdist), 1 - (shadowmapbias + (smoothshadowmappeel ? 0 : shadowmappeelbias))/float(shadowmapdist));

        shadowmapcasters = 0;
        shadowmapmaxz = shadowfocus.z - shadowmapdist;
        shadowmapping = true;
        rendergame();
        renderavatar(false);
        shadowmapping = false;
        shadowmapmaxz = min(shadowmapmaxz, shadowfocus.z);

        if(shadowmapcasters && smdepthpeel)
        {
            int sx, sy, sw, sh;
            bool scissoring = rtscissor && scissorblur(sx, sy, sw, sh) && sw > 0 && sh > 0;
            if(scissoring) glScissor(sx, sy, sw, sh);
            if(!rtscissor || scissoring) rendershadowmapreceivers();
        }

        return shadowmapcasters>0;
    }

    bool flipdebug() const { return false; }

    void dodebug(int w, int h)
    {
        if(shadowmapcasters)
        {
            if(colormask[0])
            {
                glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
                debugscissor(w, h);
            }
            if(colormask[2])
            {
                glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
                debugblurtiles(w, h);
            }
            glColorMask(COLORMASK, GL_TRUE);
        }
    }
} shadowmaptex;

void cleanshadowmap()
{
    shadowmaptex.cleanup(true);
}

void calcshadowmapbb(const vec &o, float xyrad, float zrad, float &x1, float &y1, float &x2, float &y2)
{
    vec skewdir(shadowdir);
    skewdir.rotate_around_z(-camera1->yaw*RAD);

    vec ro(o);
    ro.sub(camera1->o);
    ro.rotate_around_z(-(camera1->yaw+180)*RAD);
    ro.x += ro.z * skewdir.x + shadowoffset.x;
    ro.y += ro.z * skewdir.y + shadowmapradius * cosf(camera1->pitch*RAD) + shadowoffset.y;

    vec high(ro), low(ro);
    high.x += zrad * skewdir.x;
    high.y += zrad * skewdir.y;
    low.x -= zrad * skewdir.x;
    low.y -= zrad * skewdir.y;

    x1 = (min(high.x, low.x) - xyrad) / shadowmapradius;
    y1 = (min(high.y, low.y) - xyrad) / shadowmapradius;
    x2 = (max(high.x, low.x) + xyrad) / shadowmapradius;
    y2 = (max(high.y, low.y) + xyrad) / shadowmapradius;
}

bool addshadowmapcaster(const vec &o, float xyrad, float zrad)
{
    if(o.z + zrad <= shadowfocus.z - shadowmapdist || o.z - zrad >= shadowfocus.z) return false;

    shadowmapmaxz = max(shadowmapmaxz, o.z + zrad);

    float x1, y1, x2, y2;
    calcshadowmapbb(o, xyrad, zrad, x1, y1, x2, y2);

    if(!shadowmaptex.addblurtiles(x1, y1, x2, y2, 2)) return false;

    shadowmapcasters++;
    return true;
}

bool isshadowmapreceiver(vtxarray *va)
{
    if(!shadowmap || !shadowmapcasters) return false;

    if(va->shadowmapmax.z <= shadowfocus.z - shadowmapdist || va->shadowmapmin.z >= shadowmapmaxz) return false;

    float xyrad = SQRT2*0.5f*max(va->shadowmapmax.x-va->shadowmapmin.x, va->shadowmapmax.y-va->shadowmapmin.y),
          zrad = 0.5f*(va->shadowmapmax.z-va->shadowmapmin.z),
          x1, y1, x2, y2;
    if(xyrad<0 || zrad<0) return false;

    vec center = vec(va->shadowmapmin).add(vec(va->shadowmapmax)).mul(0.5f);
    calcshadowmapbb(center, xyrad, zrad, x1, y1, x2, y2);

    return shadowmaptex.checkblurtiles(x1, y1, x2, y2, 2);

#if 0
    // cheaper inexact test
    float dz = va->o.z + va->size/2 - shadowfocus.z;
    float cx = shadowfocus.x + dz*shadowdir.x, cy = shadowfocus.y + dz*shadowdir.y;
    float skew = va->size/2*SHADOWSKEW;
    if(!shadowmap || !shadowmaptex ||
       va->o.z + va->size <= shadowfocus.z - shadowmapdist || va->o.z >= shadowmapmaxz ||
       va->o.x + va->size <= cx - shadowmapradius-skew || va->o.x >= cx + shadowmapradius+skew ||
       va->o.y + va->size <= cy - shadowmapradius-skew || va->o.y >= cy + shadowmapradius+skew)
        return false;
    return true;
#endif
}

bool isshadowmapcaster(const vec &o, float rad)
{
    // cheaper inexact test
    float dz = o.z - shadowfocus.z;
    float cx = shadowfocus.x + dz*shadowdir.x, cy = shadowfocus.y + dz*shadowdir.y;
    float skew = rad*SHADOWSKEW;
    if(!shadowmapping ||
       o.z + rad <= shadowfocus.z - shadowmapdist || o.z - rad >= shadowfocus.z ||
       o.x + rad <= cx - shadowmapradius-skew || o.x - rad >= cx + shadowmapradius+skew ||
       o.y + rad <= cy - shadowmapradius-skew || o.y - rad >= cy + shadowmapradius+skew)
        return false;
    return true;
}

void pushshadowmap()
{
    if(!shadowmap || !shadowmaptex.rendertex) return;

    glActiveTexture_(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, shadowmaptex.rendertex);

    matrix4 m = shadowmatrix;
    m.projective(-1, 1-shadowmapbias/float(shadowmapdist));
    GLOBALPARAM(shadowmapproject, m);

    glActiveTexture_(GL_TEXTURE0);

    float r, g, b;
    if(!shadowmapambient)
    {
        if(skylightcolor[0] || skylightcolor[1] || skylightcolor[2])
        {
            r = max(25.0f, 0.4f*ambientcolor[0] + 0.6f*max(ambientcolor[0], skylightcolor[0]));
            g = max(25.0f, 0.4f*ambientcolor[1] + 0.6f*max(ambientcolor[1], skylightcolor[1]));
            b = max(25.0f, 0.4f*ambientcolor[2] + 0.6f*max(ambientcolor[2], skylightcolor[2]));
        }
        else
        {
            r = max(25.0f, 2.0f*ambientcolor[0]);
            g = max(25.0f, 2.0f*ambientcolor[1]);
            b = max(25.0f, 2.0f*ambientcolor[2]);
        }
    }
    else { r = shadowmapambientcolor[0]; g = shadowmapambientcolor[1]; b = shadowmapambientcolor[2]; }
    GLOBALPARAMF(shadowmapambient, r/255.0f, g/255.0f, b/255.0f);
}

void popshadowmap()
{
    if(!shadowmap || !shadowmaptex.rendertex) return;
}

void rendershadowmap()
{
    if(!shadowmap) return;

    shadowmaptex.render(1<<shadowmapsize, 1<<shadowmapsize, blurshadowmap, blursmsigma/100.0f);
}

VAR(0, debugsm, 0, 0, 1);

void viewshadowmap()
{
    if(!shadowmap) return;
    shadowmaptex.debug();
}

