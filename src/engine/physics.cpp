// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "engine.h"
#include "mpr.h"

const int MAXCLIPPLANES = 1024;
static clipplanes clipcache[MAXCLIPPLANES];
static int clipcacheversion = -2;

static inline clipplanes &getclipplanes(const cube &c, const ivec &o, int size, bool collide = true, int offset = 0)
{
    clipplanes &p = clipcache[int(&c - worldroot)&(MAXCLIPPLANES-1)];
    if(p.owner != &c || p.version != clipcacheversion+offset)
    {
        p.owner = &c;
        p.version = clipcacheversion+offset;
        genclipplanes(c, o, size, p, collide);
    }
    return p;
}

void resetclipplanes()
{
    clipcacheversion += 2;
    if(!clipcacheversion)
    {
        memset(clipcache, 0, sizeof(clipcache));
        clipcacheversion = 2;
    }
}

/////////////////////////  ray - cube collision ///////////////////////////////////////////////

static inline bool pointinbox(const vec &v, const vec &bo, const vec &br)
{
    return v.x <= bo.x+br.x &&
            v.x >= bo.x-br.x &&
            v.y <= bo.y+br.y &&
            v.y >= bo.y-br.y &&
            v.z <= bo.z+br.z &&
            v.z >= bo.z-br.z;
}

bool pointincube(const clipplanes &p, const vec &v)
{
    if(!pointinbox(v, p.o, p.r)) return false;
    loopi(p.size) if(p.p[i].dist(v)>1e-3f) return false;
    return true;
}

#define INTERSECTPLANES(setentry, exit) \
    float enterdist = -1e16f, exitdist = 1e16f; \
    loopi(p.size) \
    { \
        float pdist = p.p[i].dist(v), facing = ray.dot(p.p[i]); \
        if(facing < 0) \
        { \
            pdist /= -facing; \
            if(pdist > enterdist) \
            { \
                if(pdist > exitdist) exit; \
                enterdist = pdist; \
                setentry; \
            } \
        } \
        else if(facing > 0) \
        { \
            pdist /= -facing; \
            if(pdist < exitdist) \
            { \
                if(pdist < enterdist) exit; \
                exitdist = pdist; \
            } \
        } \
        else if(pdist > 0) exit; \
    }

#define INTERSECTBOX(setentry, exit) \
    loop(i, 3) \
    { \
        if(ray[i]) \
        { \
            float prad = fabs(p.r[i] * invray[i]), pdist = (p.o[i] - v[i]) * invray[i], pmin = pdist - prad, pmax = pdist + prad; \
            if(pmin > enterdist) \
            { \
                if(pmin > exitdist) exit; \
                enterdist = pmin; \
                setentry; \
            } \
            if(pmax < exitdist) \
            { \
                if(pmax < enterdist) exit; \
                exitdist = pmax; \
            } \
         } \
         else if(v[i] < p.o[i]-p.r[i] || v[i] > p.o[i]+p.r[i]) exit; \
    }

vec hitsurface;

static inline bool raycubeintersect(const clipplanes &p, const cube &c, const vec &v, const vec &ray, const vec &invray, float maxdist, float &dist)
{
    int entry = -1, bbentry = -1;
    INTERSECTPLANES(entry = i, return false);
    INTERSECTBOX(bbentry = i, return false);
    if(exitdist < 0) return false;
    dist = max(enterdist+0.1f, 0.0f);
    if(dist < maxdist)
    {
        if(bbentry>=0) { hitsurface = vec(0, 0, 0); hitsurface[bbentry] = ray[bbentry]>0 ? -1 : 1; }
        else hitsurface = p.p[entry];
    }
    return true;
}

extern void entselectionbox(const extentity &e, vec &eo, vec &es);
float hitentdist;
int hitent, hitorient;

#define mapmodelskip \
    { \
            if(e.attrs[6]&MMT_NOCLIP) continue; \
            if(e.lastemit) \
            { \
                if(e.attrs[6]&MMT_HIDE) \
                { \
                    if(e.spawned()) continue; \
                } \
                else if(e.lastemit > 0) \
                { \
                    int millis = lastmillis-e.lastemit, delay = entities::triggertime(e); \
                    if(!e.spawned() && millis < delay/2) continue; \
                    if(e.spawned() && millis > delay/2) continue; \
                } \
                else if(e.spawned()) continue; \
            } \
    }


static float disttoent(octaentities *oc, const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    vec eo, es;
    int orient = -1;
    float dist = radius, f = 0.0f;
    const vector<extentity *> &ents = entities::getents();

    #define entintersect(mask, type, func) {\
        if((mode&(mask))==(mask)) loopv(oc->type) \
        { \
            int n = oc->type[i]; \
            extentity &e = *ents[n]; \
            if(!(e.flags&EF_OCTA) || &e==t) continue; \
            func; \
            if(f<dist && f>0 && vec(ray).mul(f).add(o).insidebb(oc->o, oc->size)) \
            { \
                hitentdist = dist = f; \
                hitent = n; \
                hitorient = orient; \
            } \
        } \
    }

    entintersect(RAY_POLY, mapmodels, {
        if((mode&RAY_ENTS)!=RAY_ENTS) mapmodelskip
        else if(!entities::cansee(n)) continue;
        if(!mmintersect(e, o, ray, radius, mode, f)) continue;
    });

    entintersect(RAY_ENTS, other,
        if((mode&RAY_ENTS)==RAY_ENTS && !entities::cansee(n)) continue;
        entselectionbox(e, eo, es);
        if(!rayboxintersect(eo, es, o, ray, f, orient)) continue;
    );

    entintersect(RAY_ENTS, mapmodels,
        if((mode&RAY_ENTS)==RAY_ENTS && !entities::cansee(n)) continue;
        entselectionbox(e, eo, es);
        if(!rayboxintersect(eo, es, o, ray, f, orient)) continue;
    );

    return dist;
}

static float disttooutsideent(const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    vec eo, es;
    int orient;
    float dist = radius, f = 0.0f;
    const vector<extentity *> &ents = entities::getents();
    loopv(outsideents)
    {
        extentity &e = *ents[outsideents[i]];
        if(!(e.flags&EF_OCTA) || &e == t) continue;
        entselectionbox(e, eo, es);
        if(!rayboxintersect(eo, es, o, ray, f, orient)) continue;
        if(f<dist && f>0)
        {
            hitentdist = dist = f;
            hitent = outsideents[i];
            hitorient = orient;
        }
    }
    return dist;
}

// optimized shadow version
static float shadowent(octaentities *oc, const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    float dist = radius, f = 0.0f;
    const vector<extentity *> &ents = entities::getents();
    loopv(oc->mapmodels)
    {
        extentity &e = *ents[oc->mapmodels[i]];
        if(!(e.flags&EF_OCTA) || &e==t) continue;
        if(e.attrs[6]&MMT_NOSHADOW) continue;
        if(!mmintersect(e, o, ray, radius, mode, f)) continue;
        if(f>0 && f<dist) dist = f;
    }
    return dist;
}

#define INITRAYCUBE \
    float dist = 0, dent = radius > 0 ? radius : 1e16f; \
    vec v(o), invray(ray.x ? 1/ray.x : 1e16f, ray.y ? 1/ray.y : 1e16f, ray.z ? 1/ray.z : 1e16f); \
    cube *levels[20]; \
    levels[worldscale] = worldroot; \
    int lshift = worldscale, elvl = mode&RAY_BB ? worldscale : 0; \
    ivec lsizemask(invray.x>0 ? 1 : 0, invray.y>0 ? 1 : 0, invray.z>0 ? 1 : 0); \

#define CHECKINSIDEWORLD \
    if(!insideworld(o)) \
    { \
        float disttoworld = 0, exitworld = 1e16f; \
        loopi(3) \
        { \
            float c = v[i]; \
            if(c<0 || c>=hdr.worldsize) \
            { \
                float d = ((invray[i]>0?0:hdr.worldsize)-c)*invray[i]; \
                if(d<0) return (radius>0?radius:-1); \
                disttoworld = max(disttoworld, 0.1f + d); \
            } \
            float e = ((invray[i]>0?hdr.worldsize:0)-c)*invray[i]; \
            exitworld = min(exitworld, e); \
        } \
        if(disttoworld > exitworld) return (radius>0?radius:-1); \
        v.add(vec(ray).mul(disttoworld)); \
        dist += disttoworld; \
    }

#define DOWNOCTREE(disttoent, earlyexit) \
        cube *lc = levels[lshift]; \
        for(;;) \
        { \
            lshift--; \
            lc += octastep(x, y, z, lshift); \
            if(lc->ext && lc->ext->ents && lshift < elvl) \
            { \
                float edist = disttoent(lc->ext->ents, o, ray, dent, mode, t); \
                if(edist < dent) \
                { \
                    earlyexit return min(edist, dist); \
                    elvl = lshift; \
                    dent = min(dent, edist); \
                } \
            } \
            if(lc->children==NULL) break; \
            lc = lc->children; \
            levels[lshift] = lc; \
        }

#define FINDCLOSEST(xclosest, yclosest, zclosest) \
        float dx = (lo.x+(lsizemask.x<<lshift)-v.x)*invray.x, \
              dy = (lo.y+(lsizemask.y<<lshift)-v.y)*invray.y, \
              dz = (lo.z+(lsizemask.z<<lshift)-v.z)*invray.z; \
        float disttonext = dx; \
        xclosest; \
        if(dy < disttonext) { disttonext = dy; yclosest; } \
        if(dz < disttonext) { disttonext = dz; zclosest; } \
        disttonext += 0.1f; \
        v.add(vec(ray).mul(disttonext)); \
        dist += disttonext;

#define UPOCTREE(exitworld) \
        x = int(v.x); \
        y = int(v.y); \
        z = int(v.z); \
        uint diff = uint(lo.x^x)|uint(lo.y^y)|uint(lo.z^z); \
        if(diff >= uint(hdr.worldsize)) exitworld; \
        diff >>= lshift; \
        if(!diff) exitworld; \
        do \
        { \
            lshift++; \
            diff >>= 1; \
        } while(diff);

float raycube(const vec &o, const vec &ray, float radius, int mode, int size, extentity *t)
{
    if(ray.iszero()) return 0;

    INITRAYCUBE;
    CHECKINSIDEWORLD;

    int closest = -1, x = int(v.x), y = int(v.y), z = int(v.z);
    for(;;)
    {
        DOWNOCTREE(disttoent, if(mode&RAY_SHADOW));

        int lsize = 1<<lshift;

        cube &c = *lc;
        if((dist>0 || !(mode&RAY_SKIPFIRST)) &&
           (((mode&RAY_CLIPMAT) && isclipped(c.material&MATF_VOLUME)) ||
            ((mode&RAY_EDITMAT) && c.material != MAT_AIR) ||
            (!(mode&RAY_PASS) && lsize==size && !isempty(c)) ||
            isentirelysolid(c) ||
            dent < dist) &&
            (!(mode&RAY_CLIPMAT) || (c.material&MATF_CLIP)!=MAT_NOCLIP))
        {
            if(dist < dent)
            {
                if(closest < 0)
                {
                    float dx = ((x&(~0<<lshift))+(invray.x>0 ? 0 : 1<<lshift)-v.x)*invray.x,
                          dy = ((y&(~0<<lshift))+(invray.y>0 ? 0 : 1<<lshift)-v.y)*invray.y,
                          dz = ((z&(~0<<lshift))+(invray.z>0 ? 0 : 1<<lshift)-v.z)*invray.z;
                    closest = dx > dy ? (dx > dz ? 0 : 2) : (dy > dz ? 1 : 2);
                }
                hitsurface = vec(0, 0, 0);
                hitsurface[closest] = ray[closest]>0 ? -1 : 1;
            }
            return min(dent, dist);
        }

        ivec lo(x&(~0<<lshift), y&(~0<<lshift), z&(~0<<lshift));

        if(!isempty(c))
        {
            const clipplanes &p = getclipplanes(c, lo, lsize, false, 1);
            float f = 0;
            if(raycubeintersect(p, c, v, ray, invray, dent-dist, f) && (dist+f>0 || !(mode&RAY_SKIPFIRST)) && (!(mode&RAY_CLIPMAT) || (c.material&MATF_CLIP)!=MAT_NOCLIP))
                return min(dent, dist+f);
        }

        FINDCLOSEST(closest = 0, closest = 1, closest = 2);

        if(radius>0 && dist>=radius) return min(dent, dist);

        UPOCTREE(return min(dent, radius>0 ? radius : dist));
    }
}

// optimized version for lightmap shadowing... every cycle here counts!!!
float shadowray(const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    INITRAYCUBE;
    CHECKINSIDEWORLD;

    int side = O_BOTTOM, x = int(v.x), y = int(v.y), z = int(v.z);
    for(;;)
    {
        DOWNOCTREE(shadowent, );

        cube &c = *lc;
        ivec lo(x&(~0<<lshift), y&(~0<<lshift), z&(~0<<lshift));

        if(!isempty(c) && !(c.material&MAT_ALPHA))
        {
            if(isentirelysolid(c)) return c.texture[side]==DEFAULT_SKY && mode&RAY_SKIPSKY ? radius : dist;
            const clipplanes &p = getclipplanes(c, lo, 1<<lshift, false, 1);
            INTERSECTPLANES(side = p.side[i], goto nextcube);
            INTERSECTBOX(side = (i<<1) + 1 - lsizemask[i], goto nextcube);
            if(exitdist >= 0) return c.texture[side]==DEFAULT_SKY && mode&RAY_SKIPSKY ? radius : dist+max(enterdist+0.1f, 0.0f);
        }

    nextcube:
        FINDCLOSEST(side = O_RIGHT - lsizemask.x, side = O_FRONT - lsizemask.y, side = O_TOP - lsizemask.z);

        if(dist>=radius) return dist;

        UPOCTREE(return radius);
    }
}

// thread safe version

struct ShadowRayCache
{
    clipplanes clipcache[MAXCLIPPLANES];
    int version;

    ShadowRayCache() : version(-1) {}
};

ShadowRayCache *newshadowraycache() { return new ShadowRayCache; }

void freeshadowraycache(ShadowRayCache *&cache) { delete cache; cache = NULL; }

void resetshadowraycache(ShadowRayCache *cache)
{
    cache->version++;
    if(!cache->version)
    {
        memset(cache->clipcache, 0, sizeof(cache->clipcache));
        cache->version = 1;
    }
}

float shadowray(ShadowRayCache *cache, const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    INITRAYCUBE;
    CHECKINSIDEWORLD;

    int side = O_BOTTOM, x = int(v.x), y = int(v.y), z = int(v.z);
    for(;;)
    {
        DOWNOCTREE(shadowent, );

        cube &c = *lc;
        ivec lo(x&(~0<<lshift), y&(~0<<lshift), z&(~0<<lshift));

        if(!isempty(c) && !(c.material&MAT_ALPHA))
        {
            if(isentirelysolid(c)) return c.texture[side]==DEFAULT_SKY && mode&RAY_SKIPSKY ? radius : dist;
            clipplanes &p = cache->clipcache[int(&c - worldroot)&(MAXCLIPPLANES-1)];
            if(p.owner != &c || p.version != cache->version) { p.owner = &c; p.version = cache->version; genclipplanes(c, lo, 1<<lshift, p, false); }
            INTERSECTPLANES(side = p.side[i], goto nextcube);
            INTERSECTBOX(side = (i<<1) + 1 - lsizemask[i], goto nextcube);
            if(exitdist >= 0) return c.texture[side]==DEFAULT_SKY && mode&RAY_SKIPSKY ? radius : dist+max(enterdist+0.1f, 0.0f);
        }

    nextcube:
        FINDCLOSEST(side = O_RIGHT - lsizemask.x, side = O_FRONT - lsizemask.y, side = O_TOP - lsizemask.z);

        if(dist>=radius) return dist;

        UPOCTREE(return radius);
    }
}

float rayent(const vec &o, const vec &ray, float radius, int mode, int size, int &orient, int &ent)
{
    hitent = -1;
    hitentdist = radius;
    hitorient = -1;
    float dist = raycube(o, ray, radius, mode, size);
    if((mode&RAY_ENTS) == RAY_ENTS)
    {
        float dent = disttooutsideent(o, ray, dist < 0 ? 1e16f : dist, mode, NULL);
        if(dent < 1e15f && (dist < 0 || dent < dist)) dist = dent;
    }
    orient = hitorient;
    ent = hitentdist == dist ? hitent : -1;
    return dist;
}

float raycubepos(const vec &o, const vec &ray, vec &hitpos, float radius, int mode, int size)
{
    hitpos = ray;
    float dist = raycube(o, ray, radius, mode, size);
    if(radius>0 && dist>=radius) dist = radius;
    hitpos.mul(dist).add(o);
    return dist;
}

bool raycubelos(const vec &o, const vec &dest, vec &hitpos)
{
    vec ray(dest);
    ray.sub(o);
    float mag = ray.magnitude();
    ray.mul(1/mag);
    float distance = raycubepos(o, ray, hitpos, mag, RAY_CLIPMAT|RAY_POLY);
    return distance >= mag;
}

float rayfloor(const vec &o, vec &floor, int mode, float radius)
{
    if(o.z<=0) return -1;
    hitsurface = vec(0, 0, 1);
    float dist = raycube(o, vec(0, 0, -1), radius, mode);
    if(dist<0 || (radius>0 && dist>=radius)) return dist;
    floor = hitsurface;
    return dist;
}

/////////////////////////  entity collision  ///////////////////////////////////////////////

// info about collisions
bool collideinside; // whether an internal collision happened
physent *hitplayer; // whether the collection hit a player
int hitflags = HITFLAG_NONE;
vec collidewall; // just the normal vectors.

bool ellipseboxcollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo)
{
    float below = (o.z+center.z-lo) - (d->o.z+d->aboveeye),
          above = (d->o.z-d->height) - (o.z+center.z+hi);
    if(below>=0 || above>=0) return false;

    vec yo(d->o);
    yo.sub(o);
    yo.rotate_around_z(-yaw*RAD);
    yo.sub(center);

    float dx = clamp(yo.x, -xr, xr) - yo.x, dy = clamp(yo.y, -yr, yr) - yo.y,
          dist = sqrtf(dx*dx + dy*dy) - d->radius;
    if(dist < 0)
    {
        int sx = yo.x <= -xr ? -1 : (yo.x >= xr ? 1 : 0),
            sy = yo.y <= -yr ? -1 : (yo.y >= yr ? 1 : 0);
        if(dist > (yo.z < 0 ? below : above) && (sx || sy))
        {
            vec ydir(dir);
            ydir.rotate_around_z(-yaw*RAD);
            if(sx*yo.x - xr > sy*yo.y - yr)
            {
                if(dir.iszero() || sx*ydir.x < -1e-6f)
                {
                    collidewall = vec(sx, 0, 0);
                    collidewall.rotate_around_z(yaw*RAD);
                    return true;
                }
            }
            else if(dir.iszero() || sy*ydir.y < -1e-6f)
            {
                collidewall = vec(0, sy, 0);
                collidewall.rotate_around_z(yaw*RAD);
                return true;
            }
        }
        if(yo.z < 0)
        {
            if(dir.iszero() || (dir.z > 0 && (d->type>=ENT_INANIMATE || below >= d->zmargin-(d->height+d->aboveeye)/4.0f)))
            {
                collidewall = vec(0, 0, -1);
                return true;
            }
        }
        else if(dir.iszero() || (dir.z < 0 && (d->type>=ENT_INANIMATE || above >= d->zmargin-(d->height+d->aboveeye)/3.0f)))
        {
            collidewall = vec(0, 0, 1);
            return true;
        }
        collideinside = true;
    }
    return false;
}

bool ellipsecollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo)
{
    float below = (o.z+center.z-lo) - (d->o.z+d->aboveeye),
          above = (d->o.z-d->height) - (o.z+center.z+hi);
    if(below>=0 || above>=0) return false;
    vec yo(center);
    yo.rotate_around_z(yaw*RAD);
    yo.add(o);
    float x = yo.x - d->o.x, y = yo.y - d->o.y;
    float angle = atan2f(y, x), dangle = angle-(d->yaw+90)*RAD, eangle = angle-(yaw+90)*RAD;
    float dx = d->xradius*cosf(dangle), dy = d->yradius*sinf(dangle);
    float ex = xr*cosf(eangle), ey = yr*sinf(eangle);
    float dist = sqrtf(x*x + y*y) - sqrtf(dx*dx + dy*dy) - sqrtf(ex*ex + ey*ey);
    if(dist < 0)
    {
        if(dist > (d->o.z < yo.z ? below : above) && (dir.iszero() || x*dir.x + y*dir.y > 0))
        {
            collidewall = vec(-x, -y, 0);
            if(!collidewall.iszero()) collidewall.normalize();
            return true;
        }
        if(d->o.z < yo.z)
        {
            if(dir.iszero() || (dir.z > 0 && (d->type>=ENT_INANIMATE || below >= d->zmargin-(d->height+d->aboveeye)/4.0f)))
            {
                collidewall = vec(0, 0, -1);
                return true;
            }
        }
        else if(dir.iszero() || (dir.z < 0 && (d->type>=ENT_INANIMATE || above >= d->zmargin-(d->height+d->aboveeye)/3.0f)))
        {
            collidewall = vec(0, 0, 1);
            return true;
        }
        collideinside = true;
    }
    return false;
}

#define DYNENTCACHESIZE 1024

static uint dynentframe = 0;

static struct dynentcacheentry
{
    int x, y;
    uint frame;
    vector<physent *> dynents;
} dynentcache[DYNENTCACHESIZE];

void cleardynentcache()
{
    dynentframe++;
    if(!dynentframe || dynentframe == 1) loopi(DYNENTCACHESIZE) dynentcache[i].frame = 0;
    if(!dynentframe) dynentframe = 1;
}

VARF(0, dynentsize, 4, 7, 12, cleardynentcache());

#define DYNENTHASH(x, y) (((((x)^(y))<<5) + (((x)^(y))>>5)) & (DYNENTCACHESIZE - 1))

const vector<physent *> &checkdynentcache(int x, int y)
{
    dynentcacheentry &dec = dynentcache[DYNENTHASH(x, y)];
    if(dec.x == x && dec.y == y && dec.frame == dynentframe) return dec.dynents;
    dec.x = x;
    dec.y = y;
    dec.frame = dynentframe;
    dec.dynents.setsize(0);
    int numdyns = game::numdynents(true), dsize = 1<<dynentsize, dx = x<<dynentsize, dy = y<<dynentsize;
    loopi(numdyns)
    {
        dynent *d = game::iterdynents(i, true);
        if(!d || d->state != CS_ALIVE ||
            d->o.x+d->radius <= dx || d->o.x-d->radius >= dx+dsize ||
            d->o.y+d->radius <= dy || d->o.y-d->radius >= dy+dsize)
            continue;
        dec.dynents.add(d);
    }
    return dec.dynents;
}

#define loopdynentcachebb(curx, cury, x1, y1, x2, y2) \
    for(int curx = max(int(x1), 0)>>dynentsize, endx = min(int(x2), hdr.worldsize-1)>>dynentsize; curx <= endx; curx++) \
    for(int cury = max(int(y1), 0)>>dynentsize, endy = min(int(y2), hdr.worldsize-1)>>dynentsize; cury <= endy; cury++)

#define loopdynentcache(curx, cury, o, radius) \
    loopdynentcachebb(curx, cury, o.x-radius, o.y-radius, o.x+radius, o.y+radius)

void updatedynentcache(physent *d)
{
    loopdynentcache(x, y, d->o, d->radius)
    {
        dynentcacheentry &dec = dynentcache[DYNENTHASH(x, y)];
        if(dec.x != x || dec.y != y || dec.frame != dynentframe || dec.dynents.find(d) >= 0) continue;
        dec.dynents.add(d);
    }
}

bool overlapsdynent(const vec &o, float radius)
{
    loopdynentcache(x, y, o, radius)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        loopv(dynents)
        {
            physent *d = dynents[i];
            if(physics::issolid(d) && o.dist(d->o)-d->radius < radius)
                return true;
        }
    }
    return false;
}

template<class E, class O>
static inline bool plcollide(physent *d, const vec &dir, physent *o)
{
    E entvol(d);
    O obvol(o);
    vec cp;
    if(mpr::collide(entvol, obvol, NULL, NULL, &cp))
    {
        vec wn = vec(cp).sub(obvol.center());
        collidewall = obvol.contactface(wn, dir.iszero() ? vec(wn).neg() : dir);
        if(!collidewall.iszero()) return true;
        collideinside = true;
    }
    return false;
}

bool plcollide(physent *d, const vec &dir, physent *o)
{
    if(d->o.reject(o->o, d->radius + o->radius)) return false;
    switch(d->collidetype)
    {
        case COLLIDE_ELLIPSE:
        case COLLIDE_ELLIPSE_PRECISE:
            if(o->collidetype == COLLIDE_OBB) return ellipseboxcollide(d, dir, o->o, vec(0, 0, 0), o->yaw, o->xradius, o->yradius, o->aboveeye, o->height);
            else return ellipsecollide(d, dir, o->o, vec(0, 0, 0), o->yaw, o->xradius, o->yradius, o->aboveeye, o->height);
        case COLLIDE_OBB:
            if(o->collidetype == COLLIDE_OBB) return plcollide<mpr::EntOBB, mpr::EntOBB>(d, dir, o);
            else return plcollide<mpr::EntOBB, mpr::EntCylinder>(d, dir, o);
        default: return false;
    }
}

bool plcollide(physent *d, const vec &dir)  // collide with player or monster
{
    if(d->type==ENT_CAMERA || d->state!=CS_ALIVE) return false;
    loopdynentcache(x, y, d->o, d->radius)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        loopv(dynents)
        {
            physent *o = dynents[i];
            if(o==d || !physics::issolid(o, d)) continue;
            if(physics::xcollide(d, dir, o))
            {
                hitplayer = o;
                return true;
            }
        }
    }
    return false;
}

void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll)
{
    matrix3 orient;
    orient.identity();
    if(yaw) orient.rotate_around_z(sincosmod360(yaw));
    if(roll) orient.rotate_around_x(sincosmod360(-roll));
    if(pitch) orient.rotate_around_y(sincosmod360(-pitch));
    center = orient.transform(center);
    radius = orient.abstransform(radius);
}

template<class E, class M>
static inline bool mmcollide(physent *d, const vec &dir, const extentity &e, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    E entvol(d);
    M mdlvol(e.o, center, radius, yaw, pitch, roll);
    vec cp(0, 0, 0);
    if(mpr::collide(entvol, mdlvol, NULL, NULL, &cp))
    {
        vec wn = vec(cp).sub(mdlvol.center());
        collidewall = mdlvol.contactface(wn, dir.iszero() ? vec(wn).neg() : dir);
        if(!collidewall.iszero()) return true;
        collideinside = true;
    }
    return false;
}

template<class E>
static bool fuzzycollidebox(physent *d, const vec &dir, float cutoff, const vec &o, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    mpr::ModelOBB mdlvol(o, center, radius, yaw, pitch, roll);
    vec bbradius = mdlvol.orient.abstransposedtransform(radius);

    if(fabs(d->o.x - mdlvol.o.x) > bbradius.x + d->radius || fabs(d->o.y - mdlvol.o.y) > bbradius.y + d->radius ||
       d->o.z + d->aboveeye < mdlvol.o.z - bbradius.z || d->o.z - d->height > mdlvol.o.z + bbradius.z)
        return false;

    E entvol(d);
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    loopi(6)
    {
        vec w;
        float dist;
        switch(i)
        {
            default:
            case 0: w = mdlvol.orient.rowx().neg(); dist = -radius.x; break;
            case 1: w = mdlvol.orient.rowx(); dist = -radius.x; break;
            case 2: w = mdlvol.orient.rowy().neg(); dist = -radius.y; break;
            case 3: w = mdlvol.orient.rowy(); dist = -radius.y; break;
            case 4: w = mdlvol.orient.rowz().neg(); dist = -radius.z; break;
            case 5: w = mdlvol.orient.rowz(); dist = -radius.z; break;
        }
        vec pw = entvol.supportpoint(vec(w).neg());
        dist += w.dot(vec(pw).sub(mdlvol.o));
        if(dist >= 0) return false;
        if(dist <= bestdist) continue;
        collidewall = vec(0, 0, 0);
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude()) continue;
            if(d->type<ENT_CAMERA &&
                dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->height+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
                continue;
        }
        collidewall = w;
    }
    if(collidewall.iszero())
    {
        collideinside = true;
        return false;
    }
    return true;
}

template<class E>
static bool fuzzycollideellipse(physent *d, const vec &dir, float cutoff, const vec &o, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    mpr::ModelEllipse mdlvol(o, center, radius, yaw, pitch, roll);
    vec bbradius = mdlvol.orient.abstransposedtransform(radius);

    if(fabs(d->o.x - mdlvol.o.x) > bbradius.x + d->radius || fabs(d->o.y - mdlvol.o.y) > bbradius.y + d->radius ||
       d->o.z + d->aboveeye < mdlvol.o.z - bbradius.z || d->o.z - d->height > mdlvol.o.z + bbradius.z)
        return false;

    E entvol(d);
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    loopi(3)
    {
        vec w;
        float dist;
        switch(i)
        {
            default:
            case 0: w = mdlvol.orient.rowz(); dist = -radius.z; break;
            case 1: w = mdlvol.orient.rowz().neg(); dist = -radius.z; break;
            case 2:
            {
                vec2 ln(mdlvol.orient.transform(entvol.center().sub(mdlvol.o)));
                float r = ln.magnitude();
                if(r < 1e-6f) continue;
                vec2 lw = vec2(ln.x*radius.y, ln.y*radius.x).normalize();
                w = mdlvol.orient.transposedtransform(lw);
                dist = -vec2(ln.x*radius.x, ln.y*radius.y).dot(lw)/r;
                break;
            }
        }
        vec pw = entvol.supportpoint(vec(w).neg());
        dist += w.dot(vec(pw).sub(mdlvol.o));
        if(dist >= 0) return false;
        if(dist <= bestdist) continue;
        collidewall = vec(0, 0, 0);
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude()) continue;
            if(d->type<ENT_CAMERA &&
                dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->height+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    (dir.x*w.x < 0 || dir.y*w.y < 0 ? -d->radius : 0)))
                continue;
        }
        collidewall = w;
    }
    if(collidewall.iszero())
    {
        collideinside = true;
        return false;
    }
    return true;
}

bool mmcollide(physent *d, const vec &dir, float cutoff, octaentities &oc) // collide with a mapmodel
{
    if(d->type == ENT_CAMERA) return false;
    const vector<extentity *> &ents = entities::getents();
    loopv(oc.mapmodels)
    {
        extentity &e = *ents[oc.mapmodels[i]];
        mapmodelskip;
        model *m = loadmapmodel(e.attrs[0]);
        if(!m || !m->collide) continue;

        vec center, radius;
        float rejectradius = m->collisionbox(center, radius), scale = e.attrs[5]  ? max(e.attrs[5]/100.0f, 1e-3f) : 1;
        center.mul(scale);
        if(d->o.reject(vec(e.o).add(center), d->radius + rejectradius*scale)) continue;

        radius.mul(scale);
        int yaw = e.attrs[1], pitch = e.attrs[2], roll = e.attrs[3];
        switch(d->collidetype)
        {
            case COLLIDE_ELLIPSE:
            case COLLIDE_ELLIPSE_PRECISE:
                if(m->ellipsecollide)
                {
                    if(pitch || roll)
                    {
                        if(fuzzycollideellipse<mpr::EntCapsule>(d, dir, cutoff, e.o, center, radius, yaw, pitch, roll)) return true;
                    }
                    else if(ellipsecollide(d, dir, e.o, center, yaw, radius.x, radius.y, radius.z, radius.z)) return true;
                }
                else if(pitch || roll)
                {
                    if(fuzzycollidebox<mpr::EntCapsule>(d, dir, cutoff, e.o, center, radius, yaw, pitch, roll)) return true;
                }
                else if(ellipseboxcollide(d, dir, e.o, center, yaw, radius.x, radius.y, radius.z, radius.z)) return true;
                break;
            case COLLIDE_OBB:
                if(m->ellipsecollide)
                {
                    if(mmcollide<mpr::EntOBB, mpr::ModelEllipse>(d, dir, e, center, radius, yaw, pitch, roll)) return true;
                }
                else if(mmcollide<mpr::EntOBB, mpr::ModelOBB>(d, dir, e, center, radius, yaw, pitch, roll)) return true;
                break;
            default: continue;
        }
    }
    return false;
}

template<class E>
static bool fuzzycollidesolid(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with solid cube geometry
{
    int crad = size/2;
    if(fabs(d->o.x - co.x - crad) > d->radius + crad || fabs(d->o.y - co.y - crad) > d->radius + crad ||
       d->o.z + d->aboveeye < co.z || d->o.z - d->height > co.z + size)
        return false;

    E entvol(d);
    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = isentirelysolid(c) ? c.visible : 0xFF;
    #define CHECKSIDE(side, distval, dotval, margin, normal) if(visible&(1<<side)) do \
    { \
        float dist = distval; \
        if(dist > 0) return false; \
        if(dist <= bestdist) continue; \
        if(!dir.iszero()) \
        { \
            if(dotval >= -cutoff*dir.magnitude()) continue; \
            if(d->type<ENT_CAMERA && dotval < 0 && dist < margin) continue; \
        } \
        collidewall = normal; \
        bestdist = dist; \
    } while(0)
    CHECKSIDE(O_LEFT, co.x - (d->o.x + d->radius), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(O_RIGHT, d->o.x - d->radius - (co.x + size), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(O_BACK, co.y - (d->o.y + d->radius), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(O_FRONT, d->o.y - d->radius - (co.y + size), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(O_BOTTOM, co.z - (d->o.z + d->aboveeye), -dir.z, d->zmargin-(d->height+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(O_TOP, d->o.z - d->height - (co.z + size), dir.z, d->zmargin-(d->height+d->aboveeye)/3.0f, vec(0, 0, 1));

    if(collidewall.iszero())
    {
        collideinside = true;
        return false;
    }
    return true;
}

template<class E>
static inline bool clampcollide(const clipplanes &p, const E &entvol, const plane &w, const vec &pw)
{
    if(w.x && (w.y || w.z) && fabs(pw.x - p.o.x) > p.r.x)
    {
        vec c = entvol.center();
        float fv = pw.x < p.o.x ? p.o.x-p.r.x : p.o.x+p.r.x, fdist = (w.x*fv + w.y*c.y + w.z*c.z + w.offset) / (w.y*w.y + w.z*w.z);
        vec fdir(fv - c.x, -w.y*fdist, -w.z*fdist);
        if((pw.y-c.y-fdir.y)*w.y + (pw.z-c.z-fdir.z)*w.z >= 0 && entvol.supportpoint(fdir).squaredist(c) < fdir.squaredlen()) return true;
    }
    if(w.y && (w.x || w.z) && fabs(pw.y - p.o.y) > p.r.y)
    {
        vec c = entvol.center();
        float fv = pw.y < p.o.y ? p.o.y-p.r.y : p.o.y+p.r.y, fdist = (w.x*c.x + w.y*fv + w.z*c.z + w.offset) / (w.x*w.x + w.z*w.z);
        vec fdir(-w.x*fdist, fv - c.y, -w.z*fdist);
        if((pw.x-c.x-fdir.x)*w.x + (pw.z-c.z-fdir.z)*w.z >= 0 && entvol.supportpoint(fdir).squaredist(c) < fdir.squaredlen()) return true;
    }
    if(w.z && (w.x || w.y) && fabs(pw.z - p.o.z) > p.r.z)
    {
        vec c = entvol.center();
        float fv = pw.z < p.o.z ? p.o.z-p.r.z : p.o.z+p.r.z, fdist = (w.x*c.x + w.y*c.y + w.z*fv + w.offset) / (w.x*w.x + w.y*w.y);
        vec fdir(-w.x*fdist, -w.y*fdist, fv - c.z);
        if((pw.x-c.x-fdir.x)*w.x + (pw.y-c.y-fdir.y)*w.y >= 0 && entvol.supportpoint(fdir).squaredist(c) < fdir.squaredlen()) return true;
    }
    return false;
}

template<class E>
static bool fuzzycollideplanes(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with deformed cube geometry
{
    const clipplanes &p = getclipplanes(c, co, size);

    if(fabs(d->o.x - p.o.x) > p.r.x + d->radius || fabs(d->o.y - p.o.y) > p.r.y + d->radius ||
       d->o.z + d->aboveeye < p.o.z - p.r.z || d->o.z - d->height > p.o.z + p.r.z)
        return false;

    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = p.visible;
    CHECKSIDE(O_LEFT, p.o.x - p.r.x - (d->o.x + d->radius), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(O_RIGHT, d->o.x - d->radius - (p.o.x + p.r.x), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(O_BACK, p.o.y - p.r.y - (d->o.y + d->radius), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(O_FRONT, d->o.y - d->radius - (p.o.y + p.r.y), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(O_BOTTOM, p.o.z - p.r.z - (d->o.z + d->aboveeye), -dir.z, d->zmargin-(d->height+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(O_TOP, d->o.z - d->height - (p.o.z + p.r.z), dir.z, d->zmargin-(d->height+d->aboveeye)/3.0f, vec(0, 0, 1));

    E entvol(d);
    int bestplane = -1;
    loopi(p.size)
    {
        const plane &w = p.p[i];
        vec pw = entvol.supportpoint(vec(w).neg());
        float dist = w.dist(pw);
        if(dist >= 0) return false;
        if(dist <= bestdist) continue;
        bestplane = -1;
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude()) continue;
            if(d->type<ENT_CAMERA &&
                dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->height+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    ((dir.x*w.x < 0 || dir.y*w.y < 0) ? -d->radius : 0)))
                continue;
        }
        if(clampcollide(p, entvol, w, pw)) continue;
        bestplane = i;
    }
    if(bestplane >= 0) collidewall = p.p[bestplane];
    else if(collidewall.iszero())
    {
        collideinside = true;
        return false;
    }
    return true;
}

template<class E>
static bool cubecollidesolid(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with solid cube geometry
{
    int crad = size/2;
    if(fabs(d->o.x - co.x - crad) > d->radius + crad || fabs(d->o.y - co.y - crad) > d->radius + crad ||
       d->o.z + d->aboveeye < co.z || d->o.z - d->height > co.z + size)
        return false;

    E entvol(d);
    bool collided = mpr::collide(mpr::SolidCube(co, size), entvol);
    if(!collided) return false;

    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = isentirelysolid(c) ? c.visible : 0xFF;
    CHECKSIDE(O_LEFT, co.x - entvol.right(), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(O_RIGHT, entvol.left() - (co.x + size), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(O_BACK, co.y - entvol.front(), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(O_FRONT, entvol.back() - (co.y + size), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(O_BOTTOM, co.z - entvol.top(), -dir.z, d->zmargin-(d->height+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(O_TOP, entvol.bottom() - (co.z + size), dir.z, d->zmargin-(d->height+d->aboveeye)/3.0f, vec(0, 0, 1));

    if(collidewall.iszero())
    {
        collideinside = true;
        return false;
    }
    return true;
}

template<class E>
static bool cubecollideplanes(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size) // collide with deformed cube geometry
{
    const clipplanes &p = getclipplanes(c, co, size);

    if(fabs(d->o.x - p.o.x) > p.r.x + d->radius || fabs(d->o.y - p.o.y) > p.r.y + d->radius ||
       d->o.z + d->aboveeye < p.o.z - p.r.z || d->o.z - d->height > p.o.z + p.r.z)
        return false;

    E entvol(d);
    bool collided = mpr::collide(mpr::CubePlanes(p), entvol);
    if(!collided) return false;

    collidewall = vec(0, 0, 0);
    float bestdist = -1e10f;
    int visible = p.visible;
    CHECKSIDE(O_LEFT, p.o.x - p.r.x - entvol.right(), -dir.x, -d->radius, vec(-1, 0, 0));
    CHECKSIDE(O_RIGHT, entvol.left() - (p.o.x + p.r.x), dir.x, -d->radius, vec(1, 0, 0));
    CHECKSIDE(O_BACK, p.o.y - p.r.y - entvol.front(), -dir.y, -d->radius, vec(0, -1, 0));
    CHECKSIDE(O_FRONT, entvol.back() - (p.o.y + p.r.y), dir.y, -d->radius, vec(0, 1, 0));
    CHECKSIDE(O_BOTTOM, p.o.z - p.r.z - entvol.top(), -dir.z, d->zmargin-(d->height+d->aboveeye)/4.0f, vec(0, 0, -1));
    CHECKSIDE(O_TOP, entvol.bottom() - (p.o.z + p.r.z), dir.z, d->zmargin-(d->height+d->aboveeye)/3.0f, vec(0, 0, 1));

    int bestplane = -1;
    loopi(p.size)
    {
        const plane &w = p.p[i];
        vec pw = entvol.supportpoint(vec(w).neg());
        float dist = w.dist(pw);
        if(dist <= bestdist) continue;
        bestplane = -1;
        bestdist = dist;
        if(!dir.iszero())
        {
            if(w.dot(dir) >= -cutoff*dir.magnitude()) continue;
            if(d->type<ENT_CAMERA &&
                dist < (dir.z*w.z < 0 ?
                    d->zmargin-(d->height+d->aboveeye)/(dir.z < 0 ? 3.0f : 4.0f) :
                    ((dir.x*w.x < 0 || dir.y*w.y < 0) ? -d->radius : 0)))
                continue;
        }
        if(clampcollide(p, entvol, w, pw)) continue;
        bestplane = i;
    }
    if(bestplane >= 0) collidewall = p.p[bestplane];
    else if(collidewall.iszero())
    {
        collideinside = true;
        return false;
    }
    return true;
}

static inline bool cubecollide(physent *d, const vec &dir, float cutoff, const cube &c, const ivec &co, int size, bool solid)
{
    switch(d->collidetype)
    {
    case COLLIDE_OBB:
        if(isentirelysolid(c) || solid) return cubecollidesolid<mpr::EntOBB>(d, dir, cutoff, c, co, size);
        else return cubecollideplanes<mpr::EntOBB>(d, dir, cutoff, c, co, size);
    case COLLIDE_ELLIPSE:
        if(isentirelysolid(c) || solid) return fuzzycollidesolid<mpr::EntCapsule>(d, dir, cutoff, c, co, size);
        else return fuzzycollideplanes<mpr::EntCapsule>(d, dir, cutoff, c, co, size);
    case COLLIDE_ELLIPSE_PRECISE:
        if(isentirelysolid(c) || solid) return cubecollidesolid<mpr::EntCapsule>(d, dir, cutoff, c, co, size);
        else return cubecollideplanes<mpr::EntCapsule>(d, dir, cutoff, c, co, size);
    default: return false;
    }
}

static inline bool octacollide(physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs, const cube *c, const ivec &cor, int size) // collide with octants
{
    loopoctabox(cor, size, bo, bs)
    {
        if(c[i].ext && c[i].ext->ents) if(mmcollide(d, dir, cutoff, *c[i].ext->ents)) return true;
        ivec o(i, cor, size);
        if(c[i].children)
        {
            if(octacollide(d, dir, cutoff, bo, bs, c[i].children, o, size>>1)) return true;
        }
        else
        {
            bool solid = false;
            switch(c[i].material&MATF_CLIP)
            {
                case MAT_NOCLIP: continue;
                case MAT_CLIP: if(isclipped(c[i].material&MATF_VOLUME) || d->type<ENT_CAMERA) solid = true; break;
            }
            if(!solid && isempty(c[i])) continue;
            if(cubecollide(d, dir, cutoff, c[i], o, size, solid)) return true;
        }
    }
    return false;
}

static inline bool octacollide(physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs)
{
    int diff = (bo.x^bs.x) | (bo.y^bs.y) | (bo.z^bs.z),
        scale = worldscale-1;
    if(diff&~((1<<scale)-1) || uint(bo.x|bo.y|bo.z|bs.x|bs.y|bs.z) >= uint(hdr.worldsize))
       return octacollide(d, dir, cutoff, bo, bs, worldroot, ivec(0, 0, 0), hdr.worldsize>>1);
    const cube *c = &worldroot[octastep(bo.x, bo.y, bo.z, scale)];
    if(c->ext && c->ext->ents && mmcollide(d, dir, cutoff, *c->ext->ents)) return true;
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &c->children[octastep(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->ents && mmcollide(d, dir, cutoff, *c->ext->ents)) return true;
        scale--;
    }
    if(c->children) return octacollide(d, dir, cutoff, bo, bs, c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale);
    bool solid = false;
    switch(c->material&MATF_CLIP)
    {
        case MAT_NOCLIP: return false;
        case MAT_CLIP: if(isclipped(c->material&MATF_VOLUME) || d->type<ENT_CAMERA) solid = true; break;
    }
    if(!solid && isempty(*c)) return false;
    int csize = 2<<scale, cmask = ~(csize-1);
    return cubecollide(d, dir, cutoff, *c, ivec(bo).mask(cmask), csize, solid);
}

// all collision happens here
bool collide(physent *d, const vec &dir, float cutoff, bool playercol)
{
    collideinside = false;
    hitplayer = NULL;
    hitflags = HITFLAG_NONE;
    collidewall = vec(0, 0, 0);
    ivec bo(int(d->o.x-d->radius), int(d->o.y-d->radius), int(d->o.z-d->height)),
         bs(int(d->o.x+d->radius), int(d->o.y+d->radius), int(d->o.z+d->aboveeye));
    bs.add(1);  // guard space for rounding errors
    return octacollide(d, dir, cutoff, bo, bs) || (playercol && plcollide(d, dir));
}

float pltracecollide(physent *d, const vec &from, const vec &ray, float maxdist)
{
    vec to = vec(ray).mul(maxdist).add(from);
    float x1 = floor(min(from.x, to.x)), y1 = floor(min(from.y, to.y)),
          x2 = ceil(max(from.x, to.x)), y2 = ceil(max(from.y, to.y));
    float bestdist = 1e16f; int bestflags = HITFLAG_NONE;
    loopdynentcachebb(x, y, x1, y1, x2, y2)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        loopv(dynents)
        {
            physent *o = dynents[i];
            if(!physics::issolid(o, d)) continue;
            float dist = 1e16f;
            if(physics::xtracecollide(d, from, to, x1, x2, y1, y2, maxdist, dist, o) && dist < bestdist)
            {
                bestdist = dist;
                if(dist <= maxdist) { hitplayer = o; bestflags = hitflags; }
            }
        }
    }
    hitflags = bestflags;
    return bestdist <= maxdist ? bestdist : -1;
}

float tracecollide(physent *d, const vec &o, const vec &ray, float maxdist, int mode, bool playercol)
{
    hitsurface = vec(0, 0, 0);
    hitplayer = NULL;
    hitflags = HITFLAG_NONE;
    float dist = raycube(o, ray, maxdist+1e-3f, mode);
    if(playercol)
    {
        float pldist = pltracecollide(d, o, ray, min(dist, maxdist));
        if(pldist >= 0 && pldist < dist) dist = pldist;
    }
    return dist <= maxdist ? dist : -1;
}

void phystest()
{
    static const char * const states[] = {"float", "fall", "slide", "slope", "floor", "step up", "step down", "bounce"};
    physent *player = (physent *)game::focusedent();
    conoutf("PHYS(pl): %d %s, air %d, mat: %d, ladder: %s, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)", player->state, states[player->physstate], lastmillis-player->airmillis, player->inmaterial, player->onladder ? "yes" : "no", player->floor.x, player->floor.y, player->floor.z, player->vel.x, player->vel.y, player->vel.z, player->falling.x, player->falling.y, player->falling.z);
}

COMMAND(0, phystest, "");

bool intersect(physent *d, const vec &from, const vec &to, float &dist)   // if lineseg hits entity bounding box
{
    vec bottom(d->o), top(d->o);
    bottom.z -= d->height;
    top.z += d->aboveeye;
    if(!linecylinderintersect(from, to, bottom, top, d->radius, dist)) return false;
    dist *= from.dist(to);
    return true;
}

bool overlapsbox(const vec &d, float h1, float r1, const vec &v, float h2, float r2)
{
    return d.x <= v.x+r2+r1 && d.x >= v.x-r2-r1 &&
    d.y <= v.y+r2+r1 && d.y >= v.y-r2-r1 &&
    d.z <= v.z+h2+h1 && d.z >= v.z-h2-h1;
}

bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy)
{
    float dist = o.dist(q);

    if(dist <= mdist)
    {
        float x = fmod(fabs(asin((q.z-o.z)/dist)/RAD-pitch), 360);
        float y = fmod(fabs(-atan2(q.x-o.x, q.y-o.y)/RAD-yaw), 360);
        if(min(x, 360-x) <= fovx && min(y, 360-y) <= fovy) return raycubelos(o, q, v);
    }
    return false;
}
