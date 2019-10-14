#include "engine.h"
#include "game.h"

#define WIND_IMPULSE_DECAY_MUL 4.0f
#define WIND_DYNENT_MOVE_SCALE 0.01f
#define WIND_ATTEN_SCALE 0.01f
#define WIND_MAX_SPEED 2.0f

static vector<windemitter> windemitters;
static int numwindemitters;
static int windcost;

VAR(0, winddebug, 0, 0, 1);
VAR(IDF_PERSIST, windanimdist, 0, 1200, 10000);
VAR(IDF_PERSIST, windmaxemitters, 1, 100, 1000);
VAR(IDF_PERSIST, windcostdiv, 1, 2000, 10000);

VAR(IDF_WORLD, windyaw, 0, 45, 360);
VAR(IDF_WORLD, windyawalt, 0, 45, 360);
FVAR(IDF_WORLD, windspeed, 0, 1, 10);
FVAR(IDF_WORLD, windspeedalt, 0, 1, 10);
VAR(IDF_WORLD, windinterval, 0, 50000, VAR_MAX);
VAR(IDF_WORLD, windintervalalt, 0, 50000, VAR_MAX);

static vec2 globalwind;

// creates smooth periodic interpolation
static float interpwindspeed(int interval)
{
    return interval <= 0 ? 1.0f : (smoothwave(lastmillis / float(interval)) + 1.0f) / 2.0f;
}

// returns a free wind emitter
static windemitter *getemitter(extentity *e = NULL)
{
    if(numwindemitters >= windmaxemitters) return NULL;

    windemitter *we = NULL;

    // find a free emitter
    loopv(windemitters)
    {
        we = &windemitters[i];
        if(we->unused) break;
        else we = NULL;
    }

    // create a new one if all are busy
    if(!we) we = &windemitters.add();

    we->unused = false;
    we->hook = NULL;
    we->ent = e;
    we->lastimpulse = lastmillis;

    if(e)
    {
        // find the index of the supplied entity
        const vector<extentity *> &ents = entities::getents();
        int firstent = entities::firstent(WIND);
        int lastent = entities::lastent(WIND);
        for(int i = firstent; i < lastent; i++) if(ents[i] == e)
        {
            we->entindex = i;
            break;
        }
    }

    numwindemitters++;

    if(winddebug) conoutf("windemitter get (ent %d), total %d, allocated %d", we->entindex,
        numwindemitters, windemitters.length());

    return we;
}

static void putemitter(windemitter *we)
{
    if(we->unused) return;

    // invalidate the external handle
    if(we->hook) *we->hook = NULL;

    // mark this emitter as unused
    we->unused = true;

    numwindemitters--;

    if(winddebug) conoutf("windemitter put (ent %d), total %d, allocated %d, hook %p",
        we->entindex, numwindemitters, windemitters.length(), we->hook);
}

windemitter::windemitter(extentity *e) : ent(e), hook(NULL), curspeed(0), lastimpulse(lastmillis),
    unused(true) {}

windemitter::~windemitter() {}

void windemitter::updateimpulse()
{
    // check if the impulse has completed interpolating
    if(lastmillis >= lastimpulse + (attrs.length * WIND_IMPULSE_DECAY_MUL))
    {
        if(!attrs.interval) // was a one-time impulse (e.g. proj explosion)
        {
            putemitter(this);
            return;
        }
        else // period impulses, keep the emitter and wait for the next period
        {
            curspeed = 0;
            return;
        }
    }

    float t = (lastmillis - lastimpulse) / (float)attrs.length;
    curspeed = impulse(t);
}

// update smooth periodic wind
void windemitter::updatewind() { curspeed = interpwindspeed(attrs.interval); }

void windemitter::update()
{
    if(unused) return;

    if(attrs.mode & WIND_EMIT_IMPULSE) // wind impulses
    {
        // should we start a new impulse?
        if(attrs.interval > 0 && lastmillis >= lastimpulse + attrs.interval)
        {
            lastimpulse = lastmillis;
            if(entindex >= 0) entities::execlink(NULL, entindex, true); // trigger linked ents
        }
        updateimpulse();
    }
    else if(attrs.interval > 0) updatewind(); // smooth periodic wind
    else curspeed = 1.0f; // constant wind
}

void clearwindemitters() { loopv(windemitters) putemitter(&windemitters[i]); }

// updates global wind and emitters
void updatewind()
{
    loopv(windemitters) windemitters[i].update();

    // map settings
    float speed = checkmapvariant(MPV_ALT) ? windspeedalt : windspeed;
    float interval = checkmapvariant(MPV_ALT) ? windintervalalt : windinterval;

    // interpolate global wind speed
    speed *= interpwindspeed(interval);

    // apply the direction
    vecfromyaw(checkmapvariant(MPV_ALT) ? windyawalt : windyaw, 1, 0, globalwind);
    globalwind.mul(speed);

    windcost = 0;
}

// gets the wind for dynamic (moving) entities
static vec getentwindvec(const dynent *d)
{
    vec v = vec(0);

    if(d)
    {
        v = vec(d->vel).add(d->falling).mul(-WIND_DYNENT_MOVE_SCALE);

        // limit the magnitude to WIND_MAX_SPEED
        v.mul(min(1.0f, WIND_MAX_SPEED / v.magnitude()));
    }

    return v;
}

// returns the wind vector at a given position
vec getwind(const vec &o, const dynent *d)
{
    vec wind = vec(0);

    // global map wind and dynent wind
    wind.add(globalwind);
    wind.add(getentwindvec(d));

    // go through all active windemitters
    loopv(windemitters)
    {
        windemitter *we = &windemitters[i];
        if(we->unused) continue;

        const vec &eo = we->attrs.o;

        // outside the radius
        if(we->attrs.radius > 0 && o.dist(eo) > we->attrs.radius) continue;

        // attenuate the strength depending on the distance
        int atten = we->attrs.atten;
        float div = max(1.0f, o.dist2(eo) * atten * WIND_ATTEN_SCALE);
        float speed = (we->curspeed * we->attrs.speed) / div;

        // vectored mode, so we're getting the direction from the yaw parameter
        if(we->attrs.mode & WIND_EMIT_VECTORED)
        {
            vec2 windvec;
            vecfromyaw(we->attrs.yaw, 1, 0, windvec);
            wind.add(windvec.mul(speed));
        }
        else wind.add(vec(o).sub(eo).normalize().mul(speed)); // local mode, relative to emitter
    }

    windcost++;

    return wind;
}

void windprobe::reset()
{
    nextprobe = 0;
    lastwind = vec(0, 0, 0);
}

vec windprobe::probe(const vec &o, const dynent *d)
{
    if(lastmillis >= nextprobe)
    {
        int interval = (windcost * numwindemitters) / windcostdiv;
        nextprobe = lastmillis + interval;
        lastwind = getwind(o, d);
    }

    return lastwind;
}

void addwind(const vec &o, int mode, float speed, windemitter **hook, int yaw, int interval,
    int length, int radius, int atten)
{
    windemitter *we;

    if(hook && *hook) we = *hook; // already have an active emitter, reuse
    else we = getemitter(); // get a new emitter

    if(we)
    {
        we->attrs.o = o;
        we->attrs.mode = mode;
        we->attrs.yaw = yaw;
        we->attrs.speed = min(1.0f, speed) * WIND_MAX_SPEED;
        we->attrs.radius = radius;
        we->attrs.atten = atten;
        we->attrs.interval = interval;
        we->attrs.length = length;
        we->hook = hook;
    }

    if(hook) *hook = we; // update external handle
}

void remwind(windemitter **we)
{
    putemitter(*we);
    *we = NULL;
}

void addwind(extentity *e)
{
    windemitter *we = getemitter(e);

    if(we)
    {
        we->attrs.o = e->o;
        we->attrs.mode = e->attrs[0];
        we->attrs.yaw = e->attrs[1];
        we->attrs.speed = min(1.0f, e->attrs[2] / 255.0f) * WIND_MAX_SPEED;
        we->attrs.radius = e->attrs[3];
        we->attrs.atten = e->attrs[4];
        we->attrs.interval = e->attrs[5];
        we->attrs.length = e->attrs[6];
    }
}

void remwind(extentity *e)
{
    loopv(windemitters)
    {
        windemitter *we = &windemitters[i];
        if(we->ent == e) putemitter(we);
    }
}
