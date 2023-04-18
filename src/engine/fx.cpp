#include "cube.h"
#include "engine.h"

namespace fx
{
    VAR(0, fxdebug, 0, 0, 2);
    VAR(0, fxstatinterval, 1, 1000, 10000);

    static instance *instances, *freeinstances;
    static emitter *emitters, *freeemitters, *activeemitters;
    static int numinstances, numemitters;

    static emitter dummyemitter;

    enum
    {
        FX_STAT_EMITTER_INIT = 0,
        FX_STAT_EMITTER_PROLONG,
        FX_STAT_EMITTER_FREE,
        FX_STAT_INSTANCE_PEAK,

        FX_STATS
    };

    static int statmillis;
    static int fxstats[FX_STATS];

    static inline void clearstats() { loopi(FX_STATS) fxstats[i] = 0; }
    static inline void bumpstat(int stat) { fxstats[stat]++; }
    static inline void maxstat(int stat, int val) { if(fxstats[stat] < val) fxstats[stat] = val; }

    static inline void printstats()
    {
        bool shouldprint = false;
        loopi(FX_STATS) if(fxstats[i])
        {
            shouldprint = true;
            break;
        }

        if(!shouldprint) return;

        conoutf("T %d EI %d EP %d EF %d NI %d NE %d PI %d",
            lastmillis,
            fxstats[FX_STAT_EMITTER_INIT],
            fxstats[FX_STAT_EMITTER_PROLONG],
            fxstats[FX_STAT_EMITTER_FREE],
            numinstances,
            numemitters,
            fxstats[FX_STAT_INSTANCE_PEAK]);
    }

    static instance *getinstance()
    {
        instance *result = NULL;

        listpopfront(result, freeinstances, prev, next);
        if(result) numinstances++;

        if(fxdebug == 2) conoutf("fx instance get: %p (num %d)", result, numinstances);
        maxstat(FX_STAT_INSTANCE_PEAK, numinstances);

        return result;
    }

    static void putinstance(instance *inst)
    {
        inst->reset();

        listdremove(inst, inst->e->firstfx, inst->e->lastfx, prev, next);
        listpushfront(inst, freeinstances, prev, next);
        numinstances--;

        if(fxdebug == 2) conoutf("fx instance put: %p (num %d)", inst, numinstances);
    }

    static emitter *getemitter()
    {
        emitter *result = NULL;

        listpopfront(result, freeemitters, prev, next);

        if(result)
        {
            listpushfront(result, activeemitters, prev, next);
            numemitters++;
        }

        if(fxdebug == 2) conoutf("fx emitter get: %p (num %d)", result, numemitters);

        return result;
    }

    static void putemitter(emitter *e)
    {
        e->unhook();
        while(e->firstfx) putinstance(e->firstfx);

        listremove(e, activeemitters, prev, next);
        listpushfront(e, freeemitters, prev, next);

        numemitters--;
        if(fxdebug == 2) conoutf("fx emitter put: %p (num %d)", e, numemitters);
        bumpstat(FX_STAT_EMITTER_FREE);
    }

    VARF(IDF_PERSIST, maxfxinstances, 1, 2000, 10000, setup());
    VARF(IDF_PERSIST, maxfxemitters, 1, 500, 10000, setup());

    void instance::reset(bool initialize)
    {
        fxdef &def = fxhandle.get();

        switch(def.type)
        {
            case FX_TYPE_SOUND:
                if(!initialize && issound(soundhook))
                {
                    if(soundsources[soundhook].flags & SND_LOOP) soundsources[soundhook].clear();
                    else soundsources[soundhook].hook = NULL; // let the sound finish if not looped
                }
                soundhook = -1;
                break;

            case FX_TYPE_WIND: windhook = NULL; break;
        }
    }

    void instance::init(emitter *em, FxHandle newhandle, instance *prnt)
    {
        e = em;
        fxhandle = newhandle;
        parent = prnt;
        sync = true;
        emitted = false;
        reset(true);
        calcactiveend();
        beginmillis = endmillis = lastmillis;
    }

    void instance::calcactiveend()
    {
        int interval = getprop<int>(FX_PROP_EMIT_INTERVAL);
        int length = interval > 1 ?
            getprop<int>(FX_PROP_ACTIVE_LENGTH) :
            getprop<int>(FX_PROP_EMIT_LENGTH) + getprop<int>(FX_PROP_EMIT_DELAY);

        activeendmillis = lastmillis + length;
        e->updateend(activeendmillis);
    }

    void instance::calcend(int from)
    {
        int length = getprop<int>(FX_PROP_EMIT_LENGTH);
        endmillis = from + length;
    }

    void instance::prolong()
    {
        if(getprop<int>(FX_PROP_EMIT_SINGLE)) return;

        bool needsync = getprop<int>(FX_PROP_EMIT_PARENT) && parent;
        int interval = getprop<int>(FX_PROP_EMIT_INTERVAL);

        if(!needsync && interval < 2)
        {
            if(getprop<int>(FX_PROP_EMIT_RESTART)) beginmillis = lastmillis;
            calcend(lastmillis);
        }
        calcactiveend();
    }

    void instance::schedule(bool resync)
    {
        int delay = getprop<int>(FX_PROP_EMIT_DELAY);

        if(resync)
        {
            ASSERT(parent);

            beginmillis = parent->beginmillis + delay;
        }
        else
        {
            int interval = beginmillis == lastmillis ?
                0 : // emit immediatelly after init, TODO: property to enable/disable this?
                getprop<int>(FX_PROP_EMIT_INTERVAL);

            beginmillis = beginmillis + interval + delay;
        }

        calcend(beginmillis);

        if(getprop<int>(FX_PROP_EMIT_TIMELINESS) && endmillis > activeendmillis)
            beginmillis = endmillis = 0; // won't be able to finish in time, skip

        emitted = false;
        if(!resync) sync = true;
    }

    bool instance::checkconditions()
    {
        bool canemit = true;

        float movethreshold = getprop<float>(FX_PROP_EMIT_MOVE);
        if(movethreshold > 0 && e->from.dist(e->prevfrom) < movethreshold) canemit = false;

        return canemit;
    }

    void instance::update()
    {
        bool needsync = getprop<int>(FX_PROP_EMIT_PARENT) && parent;
        bool slip = false;

        if(endmillis) // set to 0 when skipping emission
        {
            // guarantee emit in case of frame slip ups
            if(beginmillis != endmillis && !emitted && lastmillis >= beginmillis) slip = true;

            if(needsync && parent->sync) schedule(true);
            else if(!needsync && lastmillis >= endmillis) schedule(false);
        }

        if(slip || (lastmillis >= beginmillis && lastmillis < endmillis))
        {
            if(checkconditions())
            {
                loopi(getprop<int>(FX_PROP_ITER)) emitfx(i);
                emitted = true;
            }
            else reset();
        }

        if(lastmillis >= activeendmillis && endmillis) stop();

        if(next) next->update();
        sync = false;
    }

    void instance::stop()
    {
        fxdef &def = fxhandle.get();

        reset();

        if(def.endfx.isvalid() && endmillis)
        {
            createfx(def.endfx)
                .setfrom(e->from)
                .setto(e->to)
                .setblend(e->blend)
                .setscale(e->scale)
                .setcolor(e->color)
                .setentity(e->pl);
        }

        beginmillis = endmillis = 0;
    }

    void emitter::calcrandom() { rand = vec(rndscale(1), rndscale(1), rndscale(1)); }

    void emitter::unhook()
    {
        if(!hook) return;

        *hook = NULL;
        hook = NULL;
    }

    void emitter::updateend(int end) { endmillis = max(endmillis, end); }

    void emitter::init(emitter **newhook)
    {
        firstfx = lastfx = NULL;
        beginmillis = lastmillis;
        endmillis = 0; // will be calculated by instances
        pl = NULL;
        hook = newhook;
        if(hook) *hook = this;
        calcrandom();
        bumpstat(FX_STAT_EMITTER_INIT);
    }

    bool emitter::instantiate(FxHandle handle, instance *parent)
    {
        if(!handle.isvalid())
        {
            conoutf("\frError: cannot instantiate fx, invalid handle");
            return false;
        }

        instance *inst = getinstance();

        if(!inst)
        {
            if(fxdebug == 2) conoutf("\fyWarning: cannot instantiate fx, no free instances");
            return false;
        }

        inst->init(this, handle, parent);

        listdpushback(inst, firstfx, lastfx, prev, next);

        fxdef &def = handle.get();
        loopv(def.children) instantiate(def.children[i], inst);

        return true;
    }

    void emitter::prolong()
    {
        instance *inst = firstfx;
        while(inst)
        {
            inst->prolong();
            inst = inst->next;
        }
        bumpstat(FX_STAT_EMITTER_PROLONG);
    }

    bool emitter::done() { return lastmillis >= endmillis; }

    void emitter::update()
    {
        calcrandom();
        firstfx->update();
        if(done()) putemitter(this);
    }

    void emitter::stop()
    {
        instance *inst = firstfx;
        while(inst)
        {
            inst->stop();
            inst = inst->next;
        }
    }

    emitter &emitter::setfrom(const vec &newfrom)
    {
        from = newfrom;

        // Not emitted previously, initialize prevfrom
        if(!firstfx) prevfrom = from;

        return *this;
    }

    emitter &emitter::setto(const vec &newto)
    {
        to = newto;
        return *this;
    }

    emitter &emitter::setcolor(const bvec &newcolor)
    {
        color = newcolor;
        return *this;
    }

    emitter &emitter::setblend(float newblend)
    {
        blend = newblend;
        return *this;
    }

    emitter &emitter::setscale(float newscale)
    {
        scale = newscale;
        return *this;
    }

    emitter &emitter::setparam(int index, float param)
    {
        ASSERT(index >= 0 && index < FX_PARAMS);

        params[index] = param;
        return *this;
    }

    emitter &emitter::setentity(physent *newpl)
    {
        if(!newpl) return *this;

        pl = newpl;

        // If "from" is not set, use entity position
        if(from.iszero()) from = pl->o;

        // If "to" is not set, use entity direction
        if(to.iszero()) to = vec(from).add(vec(pl->yaw*RAD, pl->pitch*RAD));

        return *this;
    }

    bool emitter::isvalid() { return this != &dummyemitter; }

    static emitter *testemitter = NULL;
    static int testmillis;

    void update()
    {
        if(testemitter && lastmillis - testmillis < 10000) testemitter->prolong();

        emitter *e = activeemitters;
        while(e)
        {
            // store next now, current emitter might get reassigned during update
            emitter *next = e->next;
            e->update();
            e = next;
        }

        if(lastmillis - statmillis >= fxstatinterval)
        {
            if(fxdebug == 1) printstats();
            clearstats();
            statmillis = lastmillis;
        }
    }

    void stopfx(emitter *e)
    {
        e->stop();
        putemitter(e);
    }

    emitter &createfx(FxHandle fxhandle, emitter **hook)
    {
        if(!fxhandle.isvalid()) return dummyemitter;

        // stop hooked FX if we want to make a different one under the same hook,
        // old hook is invalidated automatically
        if(hook && *hook && (*hook)->firstfx->fxhandle != fxhandle) stopfx(*hook);

        emitter *e = hook && *hook ? *hook : getemitter();

        if(!e)
        {
            if(fxdebug == 2) conoutf("\fyWarning: cannot create fx, no free emitters");
            return dummyemitter;
        }

        if(!e->hook)
        {
            e->init(hook);
            if(!e->instantiate(fxhandle))
            {
                // no free instances, emitter has nothing to do
                putemitter(e);
                return dummyemitter;
            }

            e->prevfrom = vec(0, 0, 0);
        }
        else
        {
            e->prolong();
            e->prevfrom = e->from;
        }

        e->from      = vec(0, 0, 0);
        e->to        = vec(0, 0, 0);
        e->blend     = 1.0f;
        e->scale     = 1.0f;
        e->color     = bvec(255, 255, 255);
        e->pl        = NULL;

        return *e;
    }

    void clear() { while(activeemitters) putemitter(activeemitters); }

    ICOMMAND(0, clearfx, "", (), clear());

    void cleanup()
    {
        clear();
        DELETEA(instances);
        DELETEA(emitters);
    }

    void setup()
    {
        cleanup();
        instances = new instance[maxfxinstances];
        emitters = new emitter[maxfxemitters];

        listinit(instances, maxfxinstances, freeinstances, prev, next);
        listinit(emitters, maxfxemitters, freeemitters, prev, next);
    }

    void removetracked(physent *pl)
    {
        emitter *e = activeemitters;
        while(e)
        {
            emitter *next = e->next;
            if(e->pl == pl) stopfx(e);
            e = next;
        }
    }

    ICOMMAND(0, testfx, "siiN", (char *name, int *sameinstance, int *color, int *numargs),
    {
        if(!hasfx(name)) return;

        vec dir;
        vec from;
        vec to;

        vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, dir);
        raycubepos(camera1->o, dir, from);
        from.sub(dir.mul(16));
        to = vec(from).add(vec(0, 0, 32));

        bvec col = *numargs > 2 ? bvec(*color) : bvec(255, 255, 255);

        createfx(getfxhandle(name), *sameinstance ? &testemitter : NULL)
            .setfrom(from)
            .setto(to)
            .setblend(1)
            .setscale(1)
            .setcolor(col);

        if(*sameinstance) testmillis = lastmillis;
    });
}
