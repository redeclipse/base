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

    static bool cancreatefx = false;
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

        conoutf(colourwhite, "T %d EI %d EP %d EF %d NI %d NE %d PI %d",
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

        if(fxdebug == 2) conoutf(colourwhite, "fx instance get: %p (num %d)", result, numinstances);
        maxstat(FX_STAT_INSTANCE_PEAK, numinstances);

        return result;
    }

    static void putinstance(instance *inst)
    {
        inst->reset();

        listdremove(inst, inst->e->firstfx, inst->e->lastfx, prev, next);
        listpushfront(inst, freeinstances, prev, next);
        numinstances--;

        if(fxdebug == 2) conoutf(colourwhite, "fx instance put: %p (num %d)", inst, numinstances);
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

        if(fxdebug == 2) conoutf(colourwhite, "fx emitter get: %p (num %d)", result, numemitters);

        return result;
    }

    static void putemitter(emitter *e)
    {
        e->unhook();
        while(e->firstfx) putinstance(e->firstfx);

        listremove(e, activeemitters, prev, next);
        listpushfront(e, freeemitters, prev, next);

        numemitters--;
        if(fxdebug == 2) conoutf(colourwhite, "fx emitter put: %p (num %d)", e, numemitters);
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

        emitted = false;

        loopi(FX_ITER_MAX) prevfrom[i] = vec(0, 0, 0);
    }

    void instance::setflags()
    {
        float cullradius = getprop<float>(FX_PROP_EMIT_CULL);

        if(getprop<float>(FX_PROP_EMIT_MOVE) != 0.0f) e->setflag(emitter::CALC_MOVED);
        if(getprop<float>(FX_PROP_EMIT_DIST) != 0.0f) e->setflag(emitter::CALC_CAMDIST);
        if(cullradius != 0.0f)
        {
            e->setflag(emitter::CALC_CULL);
            e->updatecullradius(cullradius);
        }
    }

    void instance::init(emitter *em, FxHandle newhandle, instance *prnt)
    {
        e = em;
        fxhandle = newhandle;
        parent = prnt;
        sync = true;
        canparttrack = true;
        reset(true);
        calcactiveend();
        beginmillis = endmillis = lastmillis;
        loopi(FX_ITER_MAX) prevfrom[i] = vec(0, 0, 0);

        setflags();
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

    static void calcdir(const vec &from, const vec &to, vec &dir, vec &up, vec &right)
    {
        dir = vec(to).sub(from).normalize();
        if(fabsf(dir.z) == 1.0f)
        {
            up = vec(0, dir.z, 0);
            right = vec(dir.z, 0, 0);
        }
        else
        {
            up = vec(0, 0, 1);
            right.cross(up, dir).normalize();
            up.cross(right, dir).normalize();
        }
    }

    static inline void offsetpos(vec &pos, const vec &offset, bool rel = false,
        const vec &dir = vec(0), const vec &up = vec(0), const vec &right = vec(0))
    {
        if(rel)
        {
            pos.madd(right, offset.x);
            pos.madd(dir, offset.y);
            pos.madd(up, offset.z);
        }
        else pos.add(offset);
    }

    float instance::getscale() { return getprop<float>(FX_PROP_SCALE) * e->scale; }

    void instance::calcpos()
    {
        float scale = getscale();

        canparttrack = true;

        vec dir(0), up(0), right(0);
        vec iteroffset = getprop<vec>(FX_PROP_ITER_OFFSET);
        vec fromoffset = getprop<vec>(FX_PROP_POS_OFFSET);
        vec tooffset = getprop<vec>(FX_PROP_END_OFFSET);
        vec endfrompos = getprop<vec>(FX_PROP_END_FROM_POS);
        vec posfromend = getprop<vec>(FX_PROP_POS_FROM_END);
        bool reloffset = getprop<int>(FX_RPOP_REL_OFFSET);
        bool endfromprev = getprop<int>(FX_PROP_END_FROM_PREV);

        int posfromtag = getprop<int>(FX_RPOP_POS_FROM_ENTTAG);
        int posfroment = getprop<int>(FX_PROP_POS_FROM_ENTPOS);
        int endfromtag = getprop<int>(FX_PROP_END_FROM_ENTTAG);
        int endfroment = getprop<int>(FX_PROP_END_FROM_ENTPOS);

        if(reloffset) calcdir(e->from, e->to, dir, up, right);

        if(e->pl && posfromtag >= 0)
        {
            game::fxtrack(from, e->pl, game::ENT_POS_TAG, posfromtag);
            canparttrack = false;
        }
        else if(e->pl && posfroment != game::ENT_POS_NONE)
        {
            from = to; // Base when calculating from ent direction
            game::fxtrack(from, e->pl, posfroment);
            canparttrack = false;
        }
        else if(!posfromend.iszero())
        {
            from = to;
            fromoffset.add(posfromend);
            canparttrack = false;
        }
        else from = e->from;

        if(endfromprev)
        {
            to = prevfrom[curiter].iszero() ? from : prevfrom[curiter];
            canparttrack = false;
        }
        else if(e->pl && endfromtag >= 0)
        {
            game::fxtrack(to, e->pl, game::ENT_POS_TAG, endfromtag);
            canparttrack = false;
        }
        else if(e->pl && endfroment != game::ENT_POS_NONE)
        {
            to = from; // Base when calculating from ent direction
            game::fxtrack(to, e->pl, endfroment);
            canparttrack = false;
        }
        else if(!endfrompos.iszero())
        {
            to = from;
            tooffset.add(endfrompos);
            canparttrack = false;
        }
        else to = e->to;

        if(!iteroffset.iszero() && curiter > 0)
        {
            fromoffset.add(iteroffset.mul(scale).mul(curiter));
            tooffset.add(iteroffset.mul(scale).mul(curiter));
        }

        if(!fromoffset.iszero())
        {
            offsetpos(from, fromoffset.mul(scale), reloffset, dir, up, right);
            canparttrack = false;
        }

        if(!tooffset.iszero())
        {
            offsetpos(to, tooffset.mul(scale), reloffset, dir, up, right);
            canparttrack = false;
        }

        if(getprop<int>(FX_PROP_POS_FLIP))
        {
            swap(from, to);
            canparttrack = false;
        }

        prevfrom[curiter] = from;
    }

    bool instance::checkconditions()
    {
        bool canemit = true;

        float movethreshold = getprop<float>(FX_PROP_EMIT_MOVE);
        if(movethreshold > 0 && e->moved < movethreshold) canemit = false;

        int paramtrigger = getprop<int>(FX_PROP_EMIT_PARAM);
        if(paramtrigger >= 0 && e->params[paramtrigger] == 0.0f) canemit = false;

        float maxdist = getprop<float>(FX_PROP_EMIT_DIST);
        if(maxdist > 0 && e->camdist > maxdist) canemit = false;

        if(getprop<int>(FX_PROP_EMIT_CULL) && e->cull) canemit = false;

        return canemit;
    }

    void instance::update()
    {
        bool needsync = getprop<int>(FX_PROP_EMIT_PARENT) && parent;
        bool slip = false;

        if(endmillis) // set to 0 when skipping emission
        {
            bool prevemitted = emitted;
            int slipmillis = lastmillis - beginmillis;

            if(needsync && parent->sync) schedule(true);
            else if(!needsync && lastmillis >= endmillis) schedule(false);

            // guarantee emit in case of frame slip ups
            slipmillis = max(slipmillis, lastmillis - beginmillis);
            if(beginmillis != endmillis && !prevemitted && slipmillis >= 0) slip = true;
        }

        if(slip || (lastmillis >= beginmillis && lastmillis < endmillis))
        {
            if(checkconditions())
            {
                iters = getprop<int>(FX_PROP_ITER);
                loopi(iters)
                {
                    curiter = i;
                    calcpos();
                    emitfx();
                }

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
        moved = camdist = cullradius = 0.0f;
        loopi(FX_PARAMS) params[i] = 0.0f;
        flags = 0;
        calcrandom();
        bumpstat(FX_STAT_EMITTER_INIT);
    }

    bool emitter::instantiate(FxHandle handle, instance *parent)
    {
        if(!handle.isvalid())
        {
            conoutf(colourred, "Error: cannot instantiate fx, invalid handle");
            return false;
        }

        instance *inst = getinstance();

        if(!inst)
        {
            if(fxdebug == 2) conoutf(colouryellow, "Warning: cannot instantiate fx, no free instances");
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
        if(flags&CALC_MOVED)   moved   = from.dist(prevfrom);
        if(flags&CALC_CAMDIST) camdist = camera1->o.dist(from);
        if(flags&CALC_CULL)    cull    = isfoggedsphere(cullradius, from);

        calcrandom();
        firstfx->update();
        prevfrom = from;
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

        physent *player = (physent *)game::focusedent(), *posent = pl == player && pl->isnophys() ? camera1 : pl;
        // If "from" is not set, use entity position
        if(from.iszero()) from = posent->o;

        // If "to" is not set, use entity direction
        if(to.iszero()) to = vec(from).add(vec(posent->yaw*RAD, posent->pitch*RAD));

        return *this;
    }

    bool emitter::isvalid() { return this != &dummyemitter; }

    void emitter::setflag(int flag) { flags |= flag; }

    void emitter::updatecullradius(float radius) { cullradius = max(radius, cullradius); }

    static emitter *testemitter = NULL;
    static int testmillis;

    void startframe() { cancreatefx = true; }

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

        // Prevent creation of new emitters past this point, timing gets wonky otherwise
        cancreatefx = false;
    }

    void stopfx(emitter *e)
    {
        e->stop();
        putemitter(e);
    }

    emitter &createfx(FxHandle fxhandle, emitter **hook)
    {
        ASSERT(cancreatefx);

        if(!fxhandle.isvalid()) return dummyemitter;

        // stop hooked FX if we want to make a different one under the same hook,
        // old hook is invalidated automatically
        if(hook && *hook && (*hook)->firstfx->fxhandle != fxhandle) stopfx(*hook);

        emitter *e = hook && *hook ? *hook : getemitter();

        if(!e)
        {
            if(fxdebug == 2) conoutf(colouryellow, "Warning: cannot create fx, no free emitters");
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
        e->color     = bvec(0, 0, 0);
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

    void testfx(char *name, int *sameinstance, int *color, int *numargs)
    {
        if(!hasfx(name)) return;

        vec dir;
        vec from;
        vec to;

        vecfromyawpitch(cursoryaw, cursorpitch, 1, 0, dir);
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
    }
    COMMAND(0, testfx, "siiN");
}
