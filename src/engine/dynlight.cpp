#include "engine.h"

VARN(IDF_PERSIST, dynlights, usedynlights, 0, 1, 1);
VAR(IDF_PERSIST, dynlightdist, 0, 1024, 10000);

struct dynlight
{
    vec o, hud;
    float radius, initradius, curradius, dist;
    vec color, initcolor, curcolor;
    int fade, peak, expire, flags;
    physent *owner;
    vec dir;
    int spot;

    void calcradius()
    {
        if(fade + peak > 0)
        {
            int remaining = expire - lastmillis;
            if(flags&DL_EXPAND)
                curradius = initradius + (radius - initradius) * (1.0f - remaining/float(fade + peak));
            else if(!(flags&DL_FLASH) && remaining > fade)
                curradius = initradius + (radius - initradius) * (1.0f - float(remaining - fade)/peak);
            else if(flags&DL_SHRINK)
                curradius = (radius*remaining)/fade;
            else curradius = radius;
        }
        else curradius = radius;
    }

    void calccolor()
    {
        if(flags&DL_FLASH || peak <= 0) curcolor = color;
        else
        {
            int peaking = expire - lastmillis - fade;
            if(peaking <= 0) curcolor = color;
            else curcolor.lerp(initcolor, color, 1.0f - float(peaking)/peak);
        }

        float intensity = 1.0f;
        if(fade > 0)
        {
            int fading = expire - lastmillis;
            if(fading < fade) intensity = float(fading)/fade;
        }
        curcolor.mul(intensity);
    }
};

vector<dynlight> dynlights;
vector<dynlight *> closedynlights;

void adddynlight(const vec &o, float radius, const vec &color, int fade, int peak, int flags, float initradius, const vec &initcolor, physent *owner, const vec &dir, int spot)
{
    if(!usedynlights) return;
    if(o.dist(camera1->o) > dynlightdist || radius <= 0) return;

    int insert = 0, expire = fade + peak + lastmillis;
    loopvrev(dynlights) if(expire>=dynlights[i].expire) { insert = i+1; break; }
    dynlight d;
    d.o = d.hud = o;
    d.radius = radius;
    d.initradius = initradius;
    d.color = color;
    d.initcolor = initcolor;
    d.fade = fade;
    d.peak = peak;
    d.expire = expire;
    d.flags = flags;
    d.owner = owner;
    d.dir = dir;
    d.spot = spot;
    dynlights.insert(insert, d);
}

void cleardynlights()
{
    int faded = -1;
    loopv(dynlights) if(lastmillis<dynlights[i].expire) { faded = i; break; }
    if(faded<0) dynlights.setsize(0);
    else if(faded>0) dynlights.remove(0, faded);
}

void removetrackeddynlights(physent *owner)
{
    loopvrev(dynlights) if(owner ? dynlights[i].owner == owner : dynlights[i].owner != NULL) dynlights.remove(i);
}

void updatedynlights()
{
    game::adddynlights();

    loopv(dynlights)
    {
        dynlight &d = dynlights[i];
        if(d.owner) game::dynlighttrack(d.owner, d.o, d.hud);
        d.calcradius();
        d.calccolor();
    }
}

int finddynlights()
{
    closedynlights.setsize(0);
    if(!usedynlights) return 0;
    physent e;
    e.type = ENT_CAMERA;
    loopvj(dynlights)
    {
        dynlight &d = dynlights[j];
        if(d.curradius <= 0) continue;
        d.dist = camera1->o.dist(d.o) - d.curradius;
        if(d.dist > dynlightdist || isfoggedsphere(d.curradius, d.o) || pvsoccludedsphere(d.o, d.curradius))
            continue;
        e.o = d.o;
        e.radius = e.xradius = e.yradius = e.height = e.aboveeye = d.curradius;
        if(!collide(&e, vec(0, 0, 0), 0, false)) continue;

        int insert = 0;
        loopvrev(closedynlights) if(d.dist >= closedynlights[i]->dist) { insert = i+1; break; }
        closedynlights.insert(insert, &d);
    }
    return closedynlights.length();
}

bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags)
{
    if(!closedynlights.inrange(n)) return false;
    dynlight &d = *closedynlights[n];
    if(drawtex && !(d.flags&DL_ENVIRO)) return false;
    o = d.o;
    radius = d.curradius;
    color = d.curcolor;
    spot = d.spot;
    dir = d.dir;
    flags = d.flags & 0xFF;
    return true;
}

