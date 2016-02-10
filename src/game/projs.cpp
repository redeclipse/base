#include "game.h"
namespace projs
{
    #define FWCOL(n,c,p) ((p).flags&HIT_FLAK ? W##n##COL(&p, (p).weap, flak##c, WS((p).flags)) : W##n##COL(&p, (p).weap, c, WS((p).flags)))

    vector<hitmsg> hits;
    vector<projent *> projs, collideprojs;

    struct toolent
    {
        int ent;
        float radius;
        vec o;
    };
    vector<toolent> teleports, pushers;

    VAR(IDF_PERSIST, shadowdebris, 0, 0, 1);
    VAR(IDF_PERSIST, shadowgibs, 0, 1, 1);
    VAR(IDF_PERSIST, shadoweject, 0, 0, 1);
    VAR(IDF_PERSIST, shadowents, 0, 1, 1);
    VAR(IDF_PERSIST, shadowvanity, 0, 1, 1);

    VAR(IDF_PERSIST, maxprojectiles, 1, 128, VAR_MAX);

    VAR(IDF_PERSIST, ejectfade, 0, 2500, VAR_MAX);
    VAR(IDF_PERSIST, ejectspin, 0, 1, 1);
    VAR(IDF_PERSIST, ejecthint, 0, 0, 1);

    FVAR(IDF_PERSIST, gibselasticity, -10000, 0.35f, 10000);
    FVAR(IDF_PERSIST, gibsrelativity, -10000, 0.95f, 10000);
    FVAR(IDF_PERSIST, gibsliquidcoast, 0, 2, 10000);
    FVAR(IDF_PERSIST, gibsweight, -10000, 150, 10000);

    FVAR(IDF_PERSIST, vanityelasticity, -10000, 0.5f, 10000);
    FVAR(IDF_PERSIST, vanityrelativity, -10000, 0.95f, 10000);
    FVAR(IDF_PERSIST, vanityliquidcoast, 0, 2, 10000);
    FVAR(IDF_PERSIST, vanityweight, -10000, 100, 10000);

    FVAR(IDF_PERSIST, debriselasticity, -10000, 0.6f, 10000);
    FVAR(IDF_PERSIST, debrisliquidcoast, 0, 1.7f, 10000);
    FVAR(IDF_PERSIST, debrisweight, -10000, 165, 10000);

    FVAR(IDF_PERSIST, ejectelasticity, -10000, 0.35f, 10000);
    FVAR(IDF_PERSIST, ejectrelativity, -10000, 1, 10000);
    FVAR(IDF_PERSIST, ejectliquidcoast, 0, 1.75f, 10000);
    FVAR(IDF_PERSIST, ejectweight, -10000, 180, 10000);

    VAR(IDF_PERSIST, projtrails, 0, 1, 1);
    VAR(IDF_PERSIST, projtraildelay, 2, 50, VAR_MAX);
    VAR(IDF_PERSIST, projtraillength, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, projhints, 0, 0, 6);
    VAR(IDF_PERSIST, projfirehint, 0, 0, 1);
    FVAR(IDF_PERSIST, projhintblend, 0, 0.75f, 1);
    FVAR(IDF_PERSIST, projhintsize, 0, 1.45f, FVAR_MAX);
    FVAR(IDF_PERSIST, projfirehintsize, 0, 1.85f, FVAR_MAX);

    VAR(0, projdebug, 0, 0, 1);

    #define projhint(a,b)   (projhints >= 2 ? game::getcolour(a, projhints-2) : b)

    VAR(IDF_PERSIST, muzzleflash, 0, 3, 3); // 0 = off, 1 = only other players, 2 = only thirdperson, 3 = all
    VAR(IDF_PERSIST, muzzleflare, 0, 3, 3); // 0 = off, 1 = only other players, 2 = only thirdperson, 3 = all
    FVAR(IDF_PERSIST, muzzleblend, 0, 1, 1);
    FVAR(IDF_PERSIST, muzzlefade, 0, 0.5f, 1);

    #define muzzlechk(a,b) (a == 3 || (a == 2 && game::thirdpersonview(true)) || (a == 1 && b != game::focus))
    int calcdamage(gameent *v, gameent *target, int weap, int &flags, float radial, float size, float dist, float scale)
    {
        int nodamage = 0;
        flags &= ~HIT_SFLAGS;
        if(v->actortype < A_ENEMY)
        {
            if(v == target && !damageself) nodamage++;
            else if(physics::isghost(target, v, true)) nodamage++;
        }

        if(nodamage || !hitdealt(flags))
        {
            flags &= ~HIT_CLEAR;
            flags |= HIT_WAVE;
        }

        float skew = clamp(scale, 0.f, 1.f)*damagescale;

        if(flags&HIT_WHIPLASH) skew *= WF(WK(flags), weap, damagewhiplash, WS(flags));
        else if(flags&HIT_HEAD) skew *= WF(WK(flags), weap, damagehead, WS(flags));
        else if(flags&HIT_TORSO) skew *= WF(WK(flags), weap, damagetorso, WS(flags));
        else if(flags&HIT_LEGS) skew *= WF(WK(flags), weap, damagelegs, WS(flags));
        else return 0;

        if(radial > 0) skew *= clamp(1.f-dist/size, 1e-6f, 1.f);
        else if(WF(WK(flags), weap, taper, WS(flags)))
            skew *= clamp(dist, WF(WK(flags), weap, tapermin, WS(flags)), WF(WK(flags), weap, tapermax, WS(flags)));

        if(!m_insta(game::gamemode, game::mutators))
        {
            if(m_capture(game::gamemode) && capturebuffdelay)
            {
                if(v->lastbuff) skew *= capturebuffdamage;
                if(target->lastbuff) skew /= capturebuffshield;
            }
            else if(m_defend(game::gamemode) && defendbuffdelay)
            {
                if(v->lastbuff) skew *= defendbuffdamage;
                if(target->lastbuff) skew /= defendbuffshield;
            }
            else if(m_bomber(game::gamemode) && bomberbuffdelay)
            {
                if(v->lastbuff) skew *= bomberbuffdamage;
                if(target->lastbuff) skew /= bomberbuffshield;
            }
        }

        if(v == target)
        {
            float modify = WF(WK(flags), weap, damageself, WS(flags))*G(damageselfscale);
            if(modify != 0) skew *= modify;
            else
            {
                flags &= ~HIT_CLEAR;
                flags |= HIT_WAVE;
            }
        }
        else if(m_team(game::gamemode, game::mutators) && v->team == target->team)
        {
            float modify = WF(WK(flags), weap, damageteam, WS(flags))*G(damageteamscale);
            if(modify != 0) skew *= modify;
            else
            {
                flags &= ~HIT_CLEAR;
                flags |= HIT_WAVE;
            }
        }

        return int(ceilf(WF(WK(flags), weap, damage, WS(flags))*skew));
    }

    void hitpush(gameent *d, projent &proj, int flags = 0, float radial = 0, float dist = 0, float scale = 1)
    {
        if(dist < 0) dist = 0.f;
        vec dir, vel(0, 0, 0), middle = d->center();
        dir = vec(middle).sub(proj.o);
        float dmag = dir.magnitude();
        if(dmag > 1e-3f) dir.div(dmag);
        else dir = vec(0, 0, 1);
        if(isweap(proj.weap) && !weaptype[proj.weap].traced && flags&HIT_PROJ && proj.weight != 0 && d->weight != 0)
            vel = vec(proj.vel).mul(proj.weight).div(d->weight);
        if(proj.owner && proj.local)
        {
            int hflags = proj.flags|flags;
            float size = hflags&HIT_WAVE ? radial*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) : radial;
            int damage = calcdamage(proj.owner, d, proj.weap, hflags, radial, size, dist, scale);
            if(damage) game::hiteffect(proj.weap, hflags, damage, d, proj.owner, dir, vel, dist, false);
            else return;
        }
        hitmsg &h = hits.add();
        h.flags = flags;
        h.proj = 0;
        h.target = d->clientnum;
        h.dist = int(dist*DNF);
        h.dir = ivec(int(dir.x*DNF), int(dir.y*DNF), int(dir.z*DNF));
        h.vel = ivec(int(vel.x*DNF), int(vel.y*DNF), int(vel.z*DNF));
    }

    void projpush(projent *p)
    {
        if(p->projtype == PRJ_SHOT && p->owner)
        {
            if(p->local) p->state = CS_DEAD;
            else
            {
                hitmsg &h = hits.add();
                h.flags = HIT_PROJ|HIT_TORSO;
                h.proj = p->id;
                h.target = p->owner->clientnum;
                h.dist = 0;
                h.dir = h.vel = ivec(0, 0, 0);
            }
        }
    }

    bool radialeffect(dynent *d, projent &proj, int flags, float radius)
    {
        bool push = WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) > 1, radiated = false;
        float maxdist = push ? radius*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) : radius;
        #define radialpush(xx,yx,yy,yz1,yz2,zz) \
            if(!proj.o.rejectxyz(xx, yx, yy, yz1, yz2)) zz = 0; \
            else if(!proj.o.reject(xx, maxdist+max(yx, yy))) \
            { \
                vec bottom(xx), top(xx); bottom.z -= yz1; top.z += yz2; \
                zz = closestpointcylinder(proj.o, bottom, top, max(yx, yy)).dist(proj.o); \
            }
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(e->wantshitbox())
            {
                float rdist[3] = { -1, -1, -1 };
                radialpush(e->legs, e->lrad.x, e->lrad.y, e->lrad.z, e->lrad.z, rdist[0]);
                radialpush(e->torso, e->trad.x, e->trad.y, e->trad.z, e->trad.z, rdist[1]);
                radialpush(e->head, e->hrad.x, e->hrad.y, e->hrad.z, e->hrad.z, rdist[2]);
                int closest = -1;
                loopi(3) if(rdist[i] >= 0 && (closest < 0 || rdist[i] <= rdist[closest])) closest = i;
                loopi(3) if(rdist[i] >= 0)
                {
                    int flag = 0;
                    switch(i)
                    {
                        case 2: flag = closest != i || rdist[i] > WF(WK(proj.flags), proj.weap, headmin, WS(proj.flags)) ? HIT_WHIPLASH : HIT_HEAD; break;
                        case 1: flag = HIT_TORSO; break;
                        case 0: default: flag = HIT_LEGS; break;
                    }
                    if(rdist[i] <= radius)
                    {
                        hitpush(e, proj, flag|flags, radius, rdist[i], proj.curscale);
                        radiated = true;
                    }
                    else if(push && rdist[i] <= maxdist)
                    {
                        hitpush(e, proj, flag|HIT_WAVE, maxdist, rdist[i], proj.curscale);
                        radiated = true;
                    }
                }
            }
            else
            {
                float dist = -1;
                radialpush(e->o, e->xradius, e->yradius, e->height, e->aboveeye, dist);
                if(dist >= 0)
                {
                    if(dist <= radius)
                    {
                        hitpush(e, proj, HIT_TORSO|flags, radius, dist, proj.curscale);
                        radiated = true;
                    }
                    else if(push && dist <= maxdist)
                    {
                        hitpush(e, proj, HIT_TORSO|HIT_WAVE, maxdist, dist, proj.curscale);
                        radiated = true;
                    }
                }
            }
        }
        else if(d->type == ENT_PROJ && flags&HIT_EXPLODE)
        {
            projent *e = (projent *)d;
            float dist = -1;
            radialpush(e->o, e->xradius, e->yradius, e->height, e->aboveeye, dist);
            if(dist >= 0 && dist <= radius) projpush(e);
        }
        return radiated;
    }


    bool hiteffect(projent &proj, physent *d, int flags, const vec &norm)
    {
        if(!d || !dynent::is(d)) return false;
        if(proj.projtype == PRJ_SHOT && physics::issolid(d, &proj))
        {
            bool drill = proj.projcollide&(d->type == ENT_PROJ ? DRILL_SHOTS : DRILL_PLAYER);
            proj.hit = d;
            proj.hitflags = flags;
            float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
            if(!proj.limited && proj.local)
            {
                if(expl > 0)
                {
                    if(drill)
                    {
                        gameent *oldstick = proj.stick;
                        radialeffect((dynent *)d, proj, HIT_EXPLODE, expl); // only if we're drilling
                        proj.stick = oldstick;
                    }
                }
                else if(gameent::is(d))
                {
                    int flags = 0;
                    if(proj.hitflags&HITFLAG_LEGS) flags |= HIT_LEGS;
                    if(proj.hitflags&HITFLAG_TORSO) flags |= HIT_TORSO;
                    if(proj.hitflags&HITFLAG_HEAD) flags |= HIT_HEAD;
                    if(flags) hitpush((gameent *)d, proj, flags|HIT_PROJ, 0, proj.lifesize, proj.curscale);
                }
                else if(d->type == ENT_PROJ) projpush((projent *)d);
            }
            int type = WF(WK(proj.flags), proj.weap, parttype, WS(proj.flags));
            switch(type)
            {
                case -1: break;
                case W_RIFLE:
                    part_splash(PART_SPARK, 10, 500, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale*0.125f, 1, 1, 0, 24, 20);
                    part_create(PART_PLASMA, 500, proj.o, FWCOL(H, partcol, proj), expl*0.5f, 0.5f, 0, 0);
                    adddynlight(proj.o, expl*1.1f, FWCOL(P, partcol, proj), 250, 10);
                    break;
                default:
                    if(weaptype[proj.weap].melee)
                    {
                        part_flare(proj.o, proj.from, type == W_SWORD ? 750 : 100, type == W_SWORD ? PART_LIGHTNING_FLARE : PART_MUZZLE_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, 0.5f);
                        if(type != W_SWORD) part_create(PART_PLASMA, 100, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, 0.25f);
                    }
                    break;
            }
            return !drill;
        }
        return false;
    }

    void remove(gameent *owner)
    {
        loopv(projs)
        {
            if(projs[i]->target == owner) projs[i]->target = NULL;
            if(projs[i]->stick == owner)
            {
                projs[i]->stuck = 0;
                projs[i]->stick = NULL;
                projs[i]->lastbounce = lastmillis;
            }
            if(projs[i]->owner == owner)
            {
                if(projs[i]->projtype == PRJ_SHOT)
                {
                    if(projs[i]->projcollide&COLLIDE_PROJ) collideprojs.removeobj(projs[i]);
                    delete projs[i];
                    projs.removeunordered(i--);
                }
                else projs[i]->owner = NULL;
            }
        }
    }

    void destruct(gameent *d, int id)
    {
        loopv(projs) if(projs[i]->owner == d && projs[i]->projtype == PRJ_SHOT && projs[i]->id == id)
        {
            projs[i]->state = CS_DEAD;
            break;
        }
    }

    void updatenormal(projent &proj)
    {
        vectoyawpitch(proj.norm, proj.yaw, proj.pitch);
        proj.pitch -= 90;
        game::fixfullrange(proj.yaw, proj.pitch, proj.roll, true);
        proj.resetinterp();
    }

    bool updatesticky(projent &proj, bool init = false)
    {
        if(proj.stuck)
        {
            if(init)
            {
                int lifetime = WF(WK(proj.flags), proj.weap, timestick, WS(proj.flags));
                if(lifetime > 0)
                {
                    proj.lifetime = lifetime;
                    proj.lifemillis = lifetime;
                }
                proj.vel = vec(0, 0, 0);
            }
            if(proj.stick)
            {
                if(proj.stick->state != CS_ALIVE)
                {
                    proj.stuck = 0;
                    proj.stick = NULL;
                    proj.lastbounce = lastmillis;
                    proj.resetinterp();
                }
                else
                {
                    proj.o = proj.stickpos;
                    proj.o.rotate_around_z(proj.stick->yaw*RAD);
                    proj.o.add(proj.stick->center());
                    proj.norm = proj.sticknrm;
                    proj.norm.rotate_around_z(proj.stick->yaw*RAD);
                    proj.vel = vec(proj.stick->vel).add(proj.stick->falling);
                    updatenormal(proj);
                    return true;
                }
                return false;
            }
            if(init)
            {
                proj.norm = proj.sticknrm;
                updatenormal(proj);
            }
        }
        return proj.stuck != 0;
    }

    void stick(projent &proj, const vec &dir, gameent *d = NULL)
    {
        if(proj.projtype != PRJ_SHOT || (proj.owner && proj.local))
        {
            proj.stick = d;
            proj.sticknrm = proj.norm;
            proj.stuck = proj.lastbounce = lastmillis ? lastmillis : 1;
            vec fwd = dir.iszero() ? vec(proj.vel).normalize() : dir;
            if(!fwd.iszero()) loopi(20)
            {
                proj.o.sub(fwd);
                if(!collide(&proj, vec(0, 0, 0), 0.f, proj.projcollide&COLLIDE_DYNENT) && !collideinside && (proj.stick ? hitplayer != proj.stick : !hitplayer))
                    break;
            }
            if(proj.stick)
            {
                proj.stickpos = vec(proj.o).sub(d->center());
                proj.stickpos.rotate_around_z(-proj.stick->yaw*RAD);
                proj.sticknrm.rotate_around_z(-proj.stick->yaw*RAD);
            }
            else proj.stickpos = proj.o;
            if(updatesticky(proj, true) && proj.projtype == PRJ_SHOT)
                client::addmsg(N_STICKY, "ri9i3",
                    proj.owner->clientnum, lastmillis-game::maptime, proj.weap, proj.flags, WK(proj.flags) ? -proj.id : proj.id, proj.stick ? proj.stick->clientnum : -1,
                        int(proj.sticknrm.x*DNF), int(proj.sticknrm.y*DNF), int(proj.sticknrm.z*DNF), int(proj.stickpos.x*DMF), int(proj.stickpos.y*DMF), int(proj.stickpos.z*DMF));
        }
    }

    void sticky(gameent *d, int id, vec &norm, vec &pos, gameent *f)
    {
        loopv(projs) if(projs[i]->owner == d && projs[i]->projtype == PRJ_SHOT && projs[i]->id == id)
        {
            projs[i]->stuck = projs[i]->lastbounce = lastmillis ? lastmillis : 1;
            projs[i]->sticknrm = norm;
            projs[i]->stickpos = pos;
            if(!(projs[i]->stick = f))
            {
                projs[i]->o = pos;
                projs[i]->stick = NULL;
            }
            updatesticky(*projs[i], true);
            break;
        }
    }

    bool checkitems(projent &proj, vector<toolent> &list, const vec &ray = vec(0, 0, 0), float dist = 0.f, bool teleport = false)
    {
        float closedist = 1e16f;
        int closeent = -1;
        loopv(list)
        {
            if(teleport)
            {
                if(proj.projtype == PRJ_AFFINITY && entities::ents[list[i].ent]->attrs[8]&(1<<TELE_NOAFFIN))
                    continue;
                int millis = proj.lastused(list[i].ent, true);
                if(millis && lastmillis-millis < 1000) continue;
            }
            float test = 1e16f;
            if(ray.iszero())
            {
                if(!raysphereintersect(list[i].o, list[i].radius, proj.o, ray, test) || test > dist) continue;
            }
            else
            {
                test = proj.o.dist(list[i].o);
                if(test > list[i].radius) continue;
            }

            if(closeent < 0 || test <= closedist)
            {
                closeent = list[i].ent;
                closedist = test;
            }
        }
        if(entities::ents.inrange(closeent))
        {
            entities::execitem(closeent, &proj, proj.o, closedist);
            return true;
        }
        return false;
    }

    bool checkitems(projent &proj, const vec &ray = vec(0, 0, 0), float dist = 0.f)
    {
        if(proj.interacts&1) if(checkitems(proj, teleports, ray, dist, true)) return true;
        if(proj.interacts&2) if(checkitems(proj, pushers, ray, dist)) return true;
        return false;
    }

    void reset()
    {
        collideprojs.setsize(0);
        projs.deletecontents();
        projs.shrink(0);
        teleports.shrink(0);
        pushers.shrink(0);
        loopv(entities::ents) switch(entities::ents[i]->type)
        {
            case TELEPORT: case PUSHER:
            {
                toolent &t = entities::ents[i]->type == TELEPORT ? teleports.add() : pushers.add();
                t.ent = i;
                t.radius = entities::ents[i]->attrs[3] > 0 ? entities::ents[i]->attrs[3] : enttype[entities::ents[i]->type].radius;
                t.o = entities::ents[i]->o;
                break;
            }
            default: break;
        }
    }

    void preload()
    {
        loopi(W_MAX)
        {
            if(*weaptype[i].proj) preloadmodel(weaptype[i].proj);
            if(*weaptype[i].eprj) preloadmodel(weaptype[i].eprj);
        }
        const char *mdls[] = {
            "projectiles/gibs/gib01",
            "projectiles/gibs/gib02",
            "projectiles/gibs/gib03",
            "projectiles/debris/debris01",
            "projectiles/debris/debris02",
            "projectiles/debris/debris03",
            "projectiles/debris/debris04",
            ""
        };
        for(int i = 0; *mdls[i]; i++) preloadmodel(mdls[i]);
    }

    void reflect(projent &proj, vec &pos)
    {
        bool speed = proj.vel.magnitude() > 0.01f;
        float elasticity = speed ? proj.elasticity : 1.f, reflectivity = speed ? proj.reflectivity : 0.f;
        if(elasticity > 0.f)
        {
            vec dir[2]; dir[0] = dir[1] = vec(proj.vel).normalize();
            float mag = proj.vel.magnitude()*elasticity; // conservation of energy
            dir[1].reflect(pos);
            if(!proj.lastbounce && reflectivity > 0.f)
            { // if projectile returns at 180 degrees [+/-]reflectivity, skew the reflection
                float aim[2][2] = { { 0.f, 0.f }, { 0.f, 0.f } };
                loopi(2) vectoyawpitch(dir[i], aim[0][i], aim[1][i]);
                loopi(2)
                {
                    float rmax = 180.f+reflectivity, rmin = 180.f-reflectivity,
                        off = aim[i][1]-aim[i][0];
                    if(fabs(off) <= rmax && fabs(off) >= rmin)
                    {
                        if(off > 0.f ? off > 180.f : off < -180.f)
                            aim[i][1] += rmax-off;
                        else aim[i][1] -= off-rmin;
                    }
                    while(aim[i][1] < 0.f) aim[i][1] += 360.f;
                    while(aim[i][1] >= 360.f) aim[i][1] -= 360.f;
                }
                vecfromyawpitch(aim[0][1], aim[1][1], 1, 0, dir[1]);
            }
            #define repel(x,r,z) \
            { \
                if(overlapsbox(proj.o, r, r, x, r, r)) \
                { \
                    vec nrm = vec(proj.o).sub(x).normalize(); \
                    dir[1].add(nrm).normalize(); \
                    break; \
                } \
            }
            switch(proj.projtype)
            {
                case PRJ_ENT:
                {
                    if(itemrepulsion > 0 && entities::ents.inrange(proj.id) && enttype[entities::ents[proj.id]->type].usetype == EU_ITEM)
                    {
                        loopv(projs) if(projs[i]->projtype == PRJ_ENT && projs[i] != &proj && entities::ents.inrange(projs[i]->id) && enttype[entities::ents[projs[i]->id]->type].usetype == EU_ITEM)
                            repel(projs[i]->o, itemrepulsion, itemrepelspeed);
                        loopi(entities::lastuse(EU_ITEM)) if(enttype[entities::ents[i]->type].usetype == EU_ITEM && entities::ents[i]->spawned())
                            repel(entities::ents[i]->o, itemrepulsion, itemrepelspeed);
                    }
                    break;
                }
                case PRJ_AFFINITY:
                {
                    if(m_capture(game::gamemode) && capturerepulsion > 0)
                    {
                        loopv(projs) if(projs[i]->projtype == PRJ_AFFINITY && projs[i] != &proj)
                            repel(projs[i]->o, capturerepulsion, capturerepelspeed);
                    }
                    break;
                }
            }
            if(!dir[1].iszero())
            {
                mag = max(mag, proj.speedmin);
                if(proj.speedmax > 0) mag = min(mag, proj.speedmax);
                proj.vel = vec(dir[1]).mul(mag);
            }
        }
        else proj.vel = vec(0, 0, 0);
    }

    void bounce(projent &proj, bool ricochet)
    {
        if(!proj.limited && (proj.movement >= 1 || (proj.projtype == PRJ_SHOT && weaptype[proj.weap].traced)) && (!proj.lastbounce || lastmillis-proj.lastbounce >= 250)) switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                int type = WF(WK(proj.flags), proj.weap, parttype, WS(proj.flags));
                switch(type)
                {
                    //case W_MELEE:
                    case W_SWORD:
                    {
                        part_splash(PART_SPARK, 10, 350, proj.o, FWCOL(H, partcol, proj), 0.35f, 1, 1, 0, 16, 15);
                        break;
                    }
                    case W_SHOTGUN: case W_SMG:
                    {
                        part_splash(PART_SPARK, 5, 350, proj.o, FWCOL(H, partcol, proj), 0.35f, 1, 1, 0, 16, 15);
                        adddecal(DECAL_BULLET, proj.o, proj.norm, max(WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.25f)*4*proj.curscale);
                        break;
                    }
                    case W_FLAMER:
                    {
                        adddecal(DECAL_SCORCH_SHORT, proj.o, proj.norm, WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize));
                        break;
                    }
                    default: break;
                }
                if(ricochet && !proj.limited && !WK(proj.flags))
                {
                    int vol = clamp(int(proj.vel.magnitude()*proj.curscale)*2, 0, 255);
                    if(vol > 0) playsound(WSND2(proj.weap, WS(proj.flags), S_W_BOUNCE), proj.o, NULL, 0, vol);
                }
                break;
            }
            case PRJ_GIBS:
            {
                if(game::nogore == 2) break;
                if(game::nogore || game::bloodscale > 0)
                {
                    adddecal(DECAL_BLOOD, proj.o, proj.norm, proj.radius*clamp(proj.vel.magnitude()/2, 1.f, 4.f), bvec(125, 255, 255));
                    int vol = clamp(int(proj.vel.magnitude()*proj.curscale)*2, 0, 255);
                    if(vol > 0) playsound(S_SPLOSH, proj.o, NULL, 0, vol);
                    break;
                } // otherwise fall through
            }
            case PRJ_DEBRIS: case PRJ_VANITY:
            {
                int vol = clamp(int(proj.vel.magnitude()*proj.curscale)*2, 0, 255);
                if(vol > 0)
                {
                    if(proj.projtype == PRJ_VANITY) vol /= 2;
                    playsound(S_DEBRIS, proj.o, NULL, 0, vol);
                }
                break;
            }
            case PRJ_EJECT: case PRJ_AFFINITY:
            {
                int vol = clamp(int(proj.vel.magnitude()*proj.curscale)*3, proj.projtype == PRJ_AFFINITY ? 32 : 0, 255);
                if(vol > 0) playsound(proj.projtype == PRJ_EJECT ? int(S_SHELL) : int(S_BOUNCE), proj.o, NULL, 0, vol);
                break;
            }
            default: break;
        }
    }

    float fadeweap(projent &proj)
    {
        float trans = WF(WK(proj.flags), proj.weap, blend, WS(proj.flags));
        if(WF(WK(proj.flags), proj.weap, fade, WS(proj.flags))&(proj.owner != game::focus ? 2 : 1))
        {
            int millis = lastmillis-proj.spawntime;
            if(millis < WF(WK(proj.flags), proj.weap, fadetime, WS(proj.flags)))
                trans *= millis/float(WF(WK(proj.flags), proj.weap, fadetime, WS(proj.flags)));
            if(!proj.escaped && proj.owner == game::focus && WF(WK(proj.flags), proj.weap, fadeat, WS(proj.flags)) > 0)
                trans *= camera1->o.distrange(proj.o, WF(WK(proj.flags), proj.weap, fadeat, WS(proj.flags)), WF(WK(proj.flags), proj.weap, fadecut, WS(proj.flags)));
        }
        if(proj.stuck && isweap(proj.weap) && WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags)))
        {
            int millis = lastmillis-proj.stuck;
            if(millis < WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags)))
                trans *= 1.f-(WF(WK(proj.flags), proj.weap, visfade, WS(proj.flags))*millis/float(WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags))));
            else trans *= 1.f-WF(WK(proj.flags), proj.weap, visfade, WS(proj.flags));
            if(proj.beenused && proj.lifetime < WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags)))
                trans += (1.f-trans)*((WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags))-proj.lifetime)/float(WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags))));
        }
        return trans;
    }

    bool spherecheck(projent &proj, bool rev = false)
    {
        if(!insideworld(proj.o)) return false;
        vec dir = vec(proj.vel).normalize();
        if(collide(&proj, dir, 0, false))
        {
            vec orig = proj.o;
            if(!proj.lastgood.iszero())
            {
                proj.o = proj.lastgood;
                if(!collide(&proj, dir, 0, false))
                {
                    if(rev)
                    {
                        float mag = max(max(proj.vel.magnitude()*proj.elasticity, proj.speedmin), 1.f);
                        if(proj.speedmax > 0) mag = min(mag, proj.speedmax);
                        proj.vel = vec(proj.o).sub(orig).normalize().mul(mag);
                    }
                    return true;
                }
                else proj.o = orig;
            }
            float yaw, pitch;
            vectoyawpitch(dir, yaw, pitch);
            if((yaw += 180) >= 360) yaw -= 360;
            static const int sphereyawchecks[8] = { 180, 135, 225, 90, 270, 45, 315 }, spherepitchchecks[5] = { 0, 45, -45, 89, -89 };
            loopi(2) loopj(5) loopk(8)
            {
                proj.o.add(vec((int(yaw+sphereyawchecks[k])%360)*RAD, spherepitchchecks[j]*RAD).mul(proj.radius*(i+1)*2));
                if(!collide(&proj, dir, 0, false))
                {
                    if(rev)
                    {
                        float mag = max(max(proj.vel.magnitude()*proj.elasticity, proj.speedmin), 1.f);
                        if(proj.speedmax > 0) mag = min(mag, proj.speedmax);
                        proj.vel = vec(proj.o).sub(orig).normalize().mul(mag);
                    }
                    return true;
                }
                else proj.o = orig;
            }
        }
        else proj.lastgood = proj.o;
        return false;
    }

    bool updatebb(projent &proj, bool init = false)
    {
        float size = 1;
        switch(proj.projtype)
        {
            case PRJ_AFFINITY: break;
            case PRJ_GIBS: case PRJ_DEBRIS: case PRJ_EJECT: case PRJ_VANITY: size = proj.lifesize;
            case PRJ_ENT:
                if(init) break;
                else if(proj.lifemillis && proj.fadetime)
                {
                    int interval = min(proj.lifemillis, proj.fadetime);
                    if(proj.lifetime < interval)
                    {
                        size *= float(proj.lifetime)/float(interval);
                        break;
                    }
                } // all falls through to ..
            default: return false;
        }
        size = clamp(size*proj.curscale, 0.1f, 1.f);
        model *m = NULL;
        if(proj.mdl && *proj.mdl && ((m = loadmodel(proj.mdl)) != NULL))
        {
            vec center, radius;
            m->boundbox(center, radius);
            center.mul(size);
            radius.mul(size);
            rotatebb(center, radius, proj.yaw, 0, 0);
            proj.xradius = radius.x;
            proj.yradius = radius.y;
            proj.radius = max(radius.x, radius.y);
            proj.height = proj.zradius = proj.aboveeye = radius.z;
        }
        else switch(proj.projtype)
        {
            case PRJ_GIBS: case PRJ_DEBRIS: case PRJ_VANITY: proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f*size; break;
            case PRJ_EJECT: proj.height = proj.aboveeye = 0.25f*size; proj.radius = proj.yradius = 0.5f*size; proj.xradius = 0.125f*size; break;
            case PRJ_ENT:
            {
                if(entities::ents.inrange(proj.id))
                    proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = enttype[entities::ents[proj.id]->type].radius*0.25f*size;
                else proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = size;
                break;
            }
        }
        return true;
    }

    void updatetargets(projent &proj, int style = 0)
    {
        if(proj.projtype == PRJ_SHOT && !WK(proj.flags) && proj.owner && proj.owner->state == CS_ALIVE)
        {
            if(weaptype[proj.weap].traced)
            {
                if(proj.weap == W_MELEE)
                {
                    proj.o = proj.to = proj.from = proj.dest = WS(proj.flags) ? proj.owner->center() : proj.owner->headpos();
                    if(proj.target && proj.target->state == CS_ALIVE)
                        proj.to.add(vec(proj.target->center()).sub(proj.from).normalize().mul(proj.owner->radius*2));
                    else proj.to.add(vec(proj.owner->yaw*RAD, proj.owner->pitch*RAD).mul(proj.owner->radius*2));
                }
                else
                {
                    proj.from = proj.owner->originpos(proj.weap == W_MELEE, WS(proj.flags));
                    proj.to = proj.dest = proj.owner->muzzlepos(proj.weap, WS(proj.flags));
                    if(style != 2) proj.o = proj.from;
                }
            }
            else
            {
                proj.from = proj.owner->muzzlepos(proj.weap, WS(proj.flags));
                if(style == 1) proj.o = proj.from;
            }
        }
    }

    void init(projent &proj, bool waited)
    {
        if(waited && !proj.child && proj.owner && proj.owner->state == CS_ALIVE)
        {
            proj.inertia = vec(proj.owner->vel).add(proj.owner->falling);
            proj.yaw = proj.owner->yaw;
            proj.pitch = proj.owner->pitch;
        }
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = WF(WK(proj.flags), proj.weap, radius, WS(proj.flags));
                proj.elasticity = WF(WK(proj.flags), proj.weap, elasticity, WS(proj.flags));
                proj.reflectivity = WF(WK(proj.flags), proj.weap, reflectivity, WS(proj.flags));
                proj.relativity = W2(proj.weap, relativity, WS(proj.flags));
                proj.liquidcoast = WF(WK(proj.flags), proj.weap, liquidcoast, WS(proj.flags));
                proj.weight = WF(WK(proj.flags), proj.weap, weight, WS(proj.flags));
                proj.projcollide = WF(WK(proj.flags), proj.weap, collide, WS(proj.flags));
                proj.speedmin = WF(WK(proj.flags), proj.weap, speedmin, WS(proj.flags));
                proj.speedmax = WF(WK(proj.flags), proj.weap, speedmax, WS(proj.flags));
                proj.extinguish = WF(WK(proj.flags), proj.weap, extinguish, WS(proj.flags))|4;
                proj.interacts = WF(WK(proj.flags), proj.weap, interacts, WS(proj.flags));
                proj.mdl = weaptype[proj.weap].proj;
                proj.escaped = !proj.owner || proj.child || WK(proj.flags) || weaptype[proj.weap].traced;
                updatetargets(proj, waited ? 1 : 0);
                if(WF(WK(proj.flags), proj.weap, guided, WS(proj.flags)) && proj.owner)
                    findorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, proj.dest);
                if(proj.projcollide&COLLIDE_PROJ) collideprojs.add(&proj);
                break;
            }
            case PRJ_GIBS:
            {
                if(game::nogore == 2)
                {
                    proj.lifemillis = proj.lifetime = 1;
                    proj.lifespan = 1.f;
                    proj.state = CS_DEAD;
                    proj.escaped = true;
                    return;
                }
                if(!game::nogore || game::bloodscale > 0)
                {
                    proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f;
                    proj.lifesize = 1.5f-(rnd(100)/100.f);
                    if(proj.owner)
                    {
                        if(proj.owner->state == CS_DEAD || proj.owner->state == CS_WAITING)
                        {
                            proj.o = proj.owner->wantshitbox() ? proj.owner->head : proj.owner->headpos();
                            proj.o.z -= proj.owner->zradius*0.125f;
                        }
                        else
                        {
                            proj.lifemillis = proj.lifetime = 1;
                            proj.lifespan = 1.f;
                            proj.state = CS_DEAD;
                            proj.escaped = true;
                            return;
                        }
                    }
                    switch(rnd(3))
                    {
                        case 2: proj.mdl = "projectiles/gibs/gib03"; break;
                        case 1: proj.mdl = "projectiles/gibs/gib02"; break;
                        case 0: default: proj.mdl = "projectiles/gibs/gib01"; break;
                    }
                    proj.reflectivity = 0.f;
                    proj.elasticity = gibselasticity;
                    proj.relativity = gibsrelativity;
                    proj.liquidcoast = gibsliquidcoast;
                    proj.weight = gibsweight*proj.lifesize;
                    proj.vel.add(vec(rnd(21)-10, rnd(21)-10, proj.owner && proj.owner->headless ? rnd(61)+10 : rnd(21)-10));
                    proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
                    proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                    proj.fadetime = rnd(50)+50;
                    proj.extinguish = 6;
                    proj.interacts = 3;
                    break;
                } // otherwise fall through
            }
            case PRJ_DEBRIS:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f;
                proj.lifesize = 1.5f-(rnd(100)/100.f);
                switch(rnd(4))
                {
                    case 3: proj.mdl = "projectiles/debris/debris04"; break;
                    case 2: proj.mdl = "projectiles/debris/debris03"; break;
                    case 1: proj.mdl = "projectiles/debris/debris02"; break;
                    case 0: default: proj.mdl = "projectiles/debris/debris01"; break;
                }
                proj.relativity = proj.reflectivity = 0.f;
                proj.elasticity = debriselasticity;
                proj.liquidcoast = debrisliquidcoast;
                proj.weight = debrisweight*proj.lifesize;
                proj.vel.add(vec(rnd(101)-50, rnd(101)-50, rnd(151)-50)).mul(2);
                proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER|COLLIDE_OWNER;
                proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                proj.fadetime = rnd(250)+250;
                proj.extinguish = 1;
                proj.interacts = 3;
                break;
            }
            case PRJ_EJECT:
            {
                proj.height = proj.aboveeye = 0.5f; proj.radius = proj.yradius = 1; proj.xradius = 0.25f;
                if(!isweap(proj.weap) && proj.owner) proj.weap = proj.owner->weapselect;
                if(proj.owner) proj.o = proj.from = proj.owner->ejectpos(proj.weap);
                if(isweap(proj.weap))
                {
                    proj.mdl = weaptype[proj.weap].eject && *weaptype[proj.weap].eprj ? weaptype[proj.weap].eprj : "projectiles/catridge";
                    proj.lifesize = weaptype[proj.weap].esize;
                    proj.light.material[0] = bvec(W(proj.weap, colour));
                }
                else
                {
                    proj.mdl = "projectiles/catridge";
                    proj.lifesize = 1;
                }
                proj.reflectivity = 0.f;
                proj.elasticity = ejectelasticity;
                proj.relativity = ejectrelativity;
                proj.liquidcoast = ejectliquidcoast;
                proj.weight = (ejectweight+(proj.speed*2))*proj.lifesize; // so they fall better in relation to their speed
                proj.projcollide = BOUNCE_GEOM;
                proj.escaped = true;
                proj.fadetime = rnd(300)+300;
                proj.extinguish = 6;
                proj.interacts = 3;
                if(proj.owner == game::focus && !game::thirdpersonview())
                    proj.o = proj.from.add(vec(proj.from).sub(camera1->o).normalize().mul(4));
                vecfromyawpitch(proj.yaw+40+rnd(41), proj.pitch+50-proj.speed+rnd(41), 1, 0, proj.to);
                proj.to.add(proj.from);
                break;
            }
            case PRJ_ENT:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 4;
                proj.mdl = entities::entmdlname(entities::ents[proj.id]->type, entities::ents[proj.id]->attrs);
                proj.reflectivity = 0.f;
                proj.elasticity = itemelasticity;
                proj.relativity = itemrelativity;
                proj.liquidcoast = itemliquidcoast;
                proj.weight = itemweight;
                proj.projcollide = itemcollide;
                proj.speedmin = itemspeedmin;
                proj.speedmax = itemspeedmax;
                proj.escaped = true;
                float mag = proj.inertia.magnitude();
                if(mag <= 50)
                {
                    if(mag <= 0) proj.inertia = vec(proj.yaw*RAD, proj.pitch*RAD);
                    proj.inertia.normalize().mul(50);
                }
                proj.to.add(proj.inertia);
                proj.to.z += 4;
                if(proj.flags) proj.inertia.div(proj.flags+1);
                proj.fadetime = 500;
                proj.extinguish = itemextinguish;
                proj.interacts = iteminteracts;
                break;
            }
            case PRJ_AFFINITY:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 4;
                vec dir = vec(proj.to).sub(proj.from).normalize();
                vectoyawpitch(dir, proj.yaw, proj.pitch);
                proj.reflectivity = 0.f;
                proj.escaped = true;
                proj.fadetime = 1;
                switch(game::gamemode)
                {
                    case G_BOMBER:
                        proj.mdl = "props/ball";
                        proj.projcollide = bombercollide;
                        proj.extinguish = bomberextinguish;
                        proj.interacts = bomberinteracts;
                        proj.elasticity = bomberelasticity;
                        proj.weight = bomberweight;
                        proj.relativity = bomberrelativity;
                        proj.liquidcoast = bomberliquidcoast;
                        proj.speedmin = bomberspeedmin;
                        proj.speedmax = bomberspeedmax;
                        break;
                    case G_CAPTURE: default:
                        proj.mdl = "props/flag";
                        proj.projcollide = capturecollide;
                        proj.extinguish = captureextinguish;
                        proj.interacts = captureinteracts;
                        proj.elasticity = captureelasticity;
                        proj.weight = captureweight;
                        proj.relativity = capturerelativity;
                        proj.liquidcoast = captureliquidcoast;
                        proj.speedmin = capturespeedmin;
                        proj.speedmax = capturespeedmax;
                        break;
                }
                break;
            }
            case PRJ_VANITY:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 1;
                if(proj.owner)
                {
                    if(proj.owner->state == CS_DEAD || proj.owner->state == CS_WAITING)
                    {
                        proj.o = proj.owner->headpos();
                        proj.lifesize = proj.owner->curscale;
                    }
                    else
                    {
                        proj.lifemillis = proj.lifetime = 1;
                        proj.lifespan = 1.f;
                        proj.state = CS_DEAD;
                        proj.escaped = true;
                        return;
                    }
                }
                proj.mdl = game::vanityfname(proj.owner, proj.weap, true);
                proj.reflectivity = 0.f;
                proj.elasticity = vanityelasticity;
                proj.relativity = vanityrelativity;
                proj.liquidcoast = vanityliquidcoast;
                proj.weight = vanityweight;
                proj.vel.add(vec(rnd(21)-10, rnd(21)-10, rnd(61)+10));
                proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
                proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                proj.fadetime = rnd(250)+250;
                proj.extinguish = 6;
                proj.interacts = 3;
                break;
            }
            default: break;
        }
        if(proj.projtype != PRJ_SHOT) updatebb(proj, true);
        proj.spawntime = lastmillis;
        proj.hit = NULL;
        proj.hitflags = HITFLAG_NONE;
        proj.movement = 1;
        if(proj.owner && (proj.projtype != PRJ_SHOT || (!proj.child && !WK(proj.flags) && !weaptype[proj.weap].traced)))
        {
            vec eyedir = vec(proj.o).sub(proj.owner->o);
            float eyedist = eyedir.magnitude();
            if(eyedist > 0)
            {
                eyedir.normalize();
                float blocked = tracecollide(&proj, proj.owner->o, eyedir, eyedist, RAY_CLIPMAT|RAY_ALPHAPOLY, false);
                if(blocked >= 0)
                {
                    proj.o = proj.from = vec(eyedir).mul(blocked-max(proj.radius, 1e-3f)).add(proj.owner->o);
                    proj.to = vec(eyedir).mul(blocked).add(proj.owner->o);
                }
            }
        }
        if(proj.projtype != PRJ_SHOT || !weaptype[proj.weap].traced)
        {
            vec dir = vec(proj.to).sub(proj.o);
            float maxdist = dir.magnitude();
            if(maxdist > 1e-3f)
            {
                dir.mul(1/maxdist);
                if(proj.projtype != PRJ_EJECT) vectoyawpitch(dir, proj.yaw, proj.pitch);
            }
            else if(!proj.child && proj.owner)
            {
                if(proj.projtype != PRJ_EJECT)
                {
                    proj.yaw = proj.owner->yaw;
                    proj.pitch = proj.owner->pitch;
                }
                dir = vec(proj.yaw*RAD, proj.pitch*RAD);
            }
            vec rel = vec(proj.vel).add(dir).add(proj.inertia.mul(proj.relativity));
            proj.vel = vec(rel).add(vec(dir).mul(physics::movevelocity(&proj)));
        }
        if(proj.projtype != PRJ_SHOT) spherecheck(proj, proj.projcollide&BOUNCE_GEOM);
        proj.resetinterp();
    }

    projent *create(const vec &from, const vec &to, bool local, gameent *d, int type, int lifetime, int lifemillis, int waittime, int speed, int id, int weap, int value, int flags, float scale, bool child, projent *parent)
    {
        projent &proj = *new projent;
        proj.o = proj.from = from;
        proj.to = proj.dest = to;
        proj.local = local;
        proj.projtype = type;
        proj.addtime = lastmillis;
        proj.lifetime = lifetime;
        proj.lifemillis = lifemillis ? lifemillis : proj.lifetime;
        proj.waittime = waittime;
        proj.speed = speed;
        proj.id = id;
        proj.flags = flags;
        proj.curscale = scale;
        proj.movement = proj.distance = 0;
        if(proj.projtype == PRJ_AFFINITY)
        {
            proj.vel = proj.inertia = proj.to;
            proj.to.add(proj.from);
            if(weap >= 0) proj.target = game::getclient(weap);
        }
        else
        {
            proj.weap = weap;
            proj.value = value;
            if(child)
            {
                proj.child = true;
                proj.owner = d;
                proj.vel = vec(proj.to).sub(proj.from);
                if(parent) proj.target = parent->target;
            }
            else if(d)
            {
                proj.owner = d;
                proj.yaw = d->yaw;
                proj.pitch = d->pitch;
                proj.inertia = vec(d->vel).add(d->falling);
                if(proj.projtype == PRJ_SHOT && isweap(proj.weap) && issound(d->pschan) && proj.weap != W_MELEE && !WK(proj.flags))
                    playsound(WSND2(proj.weap, WS(proj.flags), S_W_TRANSIT), proj.o, &proj, SND_LOOP, sounds[d->pschan].vol, -1, -1, &proj.schan, 0, &d->pschan);
            }
        }
        if(!proj.waittime) init(proj, false);
        projs.add(&proj);
        return &proj;
    }

    void drop(gameent *d, int weap, int ent, int ammo, bool local, int index, int targ)
    {
        if(weap >= W_OFFSET && isweap(weap))
        {
            if(ammo >= 0)
            {
                if(entities::ents.inrange(ent))
                    create(d->muzzlepos(), d->muzzlepos(), local, d, PRJ_ENT, w_spawn(weap), w_spawn(weap), 1, 1, ent, ammo, index);
                d->ammo[weap] = -1;
                if(targ >= 0) d->setweapstate(weap, W_S_SWITCH, weaponswitchdelay, lastmillis);
            }
            else if(weap == W_GRENADE || weap == W_MINE)
                create(d->muzzlepos(), d->muzzlepos(), local, d, PRJ_SHOT, 1, W2(weap, time, false), 1, 1, 1, weap);
            d->entid[weap] = -1;
        }
    }

    void shootv(int weap, int flags, int sub, int offset, float scale, vec &from, vector<shotmsg> &shots, gameent *d, bool local)
    {
        int delay = W2(weap, timedelay, WS(flags)), iter = W2(weap, timeiter, WS(flags)),
            delayattack = W2(weap, delayattack, WS(flags)),
            cook = W2(weap, cooktime, WS(flags)), cooked = W2(weap, cooked, WS(flags)),
            life = W2(weap, time, WS(flags)), speed = W2(weap, speed, WS(flags)),
            speedlimit = W2(weap, speedlimit, WS(flags));
        float skew = 1;
        if(cook && cooked)
        {
            if(cooked&W_C_SCALE)   skew = scale; // scaled
            if(cooked&W_C_SCALEN)  skew = 1-scale; // inverted scale
            if(cooked&W_C_LIFE)    life = int(ceilf(life*scale)); // life scale
            if(cooked&W_C_LIFEN)   life = int(ceilf(life*(1-scale))); // inverted life
            if(cooked&W_C_SPEED)   speed = speedlimit+int(ceilf(max(speed-speedlimit, 0)*scale)); // speed scale
            if(cooked&W_C_SPEEDN)  speed = speedlimit+int(ceilf(max(speed-speedlimit, 0)*(1-scale))); // inverted speed
        }

        if(weaptype[weap].sound >= 0 && (weap != W_MELEE || !(WS(flags))))
        {
            int slot = WSNDF(weap, WS(flags)), vol = clamp(int(ceilf(255*skew)), 0, 255);
            if(slot >= 0 && vol > 0)
            {
                if(weap == W_FLAMER && !(WS(flags)))
                {
                    int ends = lastmillis+delayattack+PHYSMILLIS;
                    if(issound(d->wschan) && sounds[d->wschan].slotnum == slot) sounds[d->wschan].ends = ends;
                    else playsound(slot, d->o, d, SND_LOOP, vol, -1, -1, &d->wschan, ends);
                }
                else if(!W2(weap, time, WS(flags)) || life)
                {
                    if(issound(d->wschan) && (sounds[d->wschan].slotnum == WSNDF(weap, false) || sounds[d->wschan].slotnum == WSNDF(weap, true)))
                    {
                        sounds[d->wschan].hook = NULL;
                        d->wschan = -1;
                    }
                    playsound(slot, d->o, d, 0, vol, -1, -1, &d->wschan);
                }
            }
        }
        if(delayattack >= 5 && weap != W_MELEE)
        {
            int colour = WHCOL(d, weap, partcol, WS(flags));
            float muz = muzzleblend*W2(weap, partblend, WS(flags));
            if(d == game::focus) muz *= muzzlefade;
            const struct weapfxs
            {
                int smoke, parttype, sparktime, sparknum, sparkrad;
                float partsize, flaresize, flarelen, sparksize;
            } weapfx[W_MAX] = {
                { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 200, PART_MUZZLE_FLASH, 200, 5, 4, 1, 1, 2, 0.0125f },
                { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 350, PART_MUZZLE_FLASH, 500, 20, 8, 3, 3, 6, 0.025f },
                { 50, PART_MUZZLE_FLASH, 350, 5, 6, 1.5f, 2, 4, 0.0125f },
                { 150, PART_MUZZLE_FLASH, 250, 5, 8, 1.5f, 0, 0, 0.025f },
                { 150, PART_PLASMA, 250, 10, 6, 1.5f, 0, 0, 0.0125f },
                { 150, PART_ELECZAP, 250, 10, 6, 1.5f, 0, 0, 0.0125f },
                { 150, PART_PLASMA, 250, 5, 6, 2, 3, 6, 0.0125f },
                { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 150, PART_MUZZLE_FLASH, 250, 10, 8, 3, 3, 6, 0.0125f },
            };
            if(weapfx[weap].smoke) part_create(PART_SMOKE_LERP, weapfx[weap].smoke, from, 0x888888, 1, 0.25f, -10);
            if(weapfx[weap].sparktime && weapfx[weap].sparknum)
                part_splash(weap == W_FLAMER ? PART_FIREBALL : PART_SPARK, weapfx[weap].sparknum, weapfx[weap].sparktime, from, colour, weapfx[weap].sparksize, muz, 1, 0, weapfx[weap].sparkrad, 15);
            if(muzzlechk(muzzleflash, d) && weapfx[weap].partsize > 0)
                part_create(weapfx[weap].parttype, delayattack/3, from, colour, weapfx[weap].partsize, muz, 0, 0, d);
            if(muzzlechk(muzzleflare, d) && weapfx[weap].flaresize > 0)
            {
                vec targ; findorientation(d->o, d->yaw, d->pitch, targ);
                targ.sub(from).normalize().mul(weapfx[weap].flarelen).add(from);
                part_flare(from, targ, delayattack/2, PART_MUZZLE_FLARE, colour, weapfx[weap].flaresize, muz, 0, 0, d);
            }
            int peak = delayattack/4, fade = min(peak/2, 75);
            adddynlight(from, 32, vec::hexcolor(colour).mul(0.5f), fade, peak - fade, DL_FLASH);
        }
        loopv(shots)
            create(from, vec(shots[i].pos).div(DMF), local, d, PRJ_SHOT, max(life, 1), W2(weap, time, WS(flags)), delay+(iter*i), speed, shots[i].id, weap, -1, flags, skew);
        if(ejectfade && weaptype[weap].eject && *weaptype[weap].eprj) loopi(clamp(sub, 1, W2(weap, ammosub, WS(flags))))
            create(from, from, local, d, PRJ_EJECT, rnd(ejectfade)+ejectfade, 0, delay, rnd(weaptype[weap].espeed)+weaptype[weap].espeed, 0, weap, -1, flags);

        d->setweapstate(weap, WS(flags) ? W_S_SECONDARY : W_S_PRIMARY, delayattack, lastmillis);
        d->ammo[weap] = max(d->ammo[weap]-sub-offset, 0);
        d->weapshot[weap] = sub;
        if(offset > 0) d->weapload[weap] = -offset;
        d->lastshoot = lastmillis;
        if(AA(d->actortype, abilities)&(1<<A_A_PUSHABLE))
        {
            vec kick = vec(d->yaw*RAD, d->pitch*RAD).mul(-W2(weap, kickpush, WS(flags))*skew);
            if(!kick.iszero())
            {
                if(d == game::focus) game::swaypush.add(vec(kick).mul(kickpushsway));
                float kickmod = kickpushscale;
                if(W2(weap, cooked, WS(flags))&W_C_ZOOM && WS(flags)) kickmod *= kickpushzoom;
                if(d->crouching() && !d->sliding()) kickmod *= kickpushcrouch;
                d->vel.add(vec(kick).mul(kickmod));
            }
        }
    }

    void updatetaper(projent &proj, float distance, bool firstpass = false)
    {
        int type = WF(WK(proj.flags), proj.weap, taper, WS(proj.flags));
        if(!firstpass && type <= 4) return; // only distance tapers need continuous updates
        switch(type)
        {
            case 5: case 6:
            {
                if(WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags)) > 0)
                {
                    if(distance > WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags)))
                    {
                        proj.state = CS_DEAD;
                        proj.lifesize = WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags));
                        break;
                    }
                    else if(distance > WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)))
                    {
                        if(type%2 || !proj.stuck)
                        {
                            float dist = 1-((distance-WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)))/WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags)));
                            proj.lifesize = clamp(dist, WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                        }
                        break;
                    }
                }
                if(distance < WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)))
                {
                    proj.lifesize = clamp(distance/WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                    break;
                }
                proj.lifesize = WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags));
                break;
            }
            case 1: case 2: case 3: case 4:
            {
                float spanin = WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)),
                      spanout = WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags));
                if(type >= 3)
                { // timer-to-span translation
                    spanin /= max(proj.lifemillis, 1);
                    spanout /= max(proj.lifemillis, 1);
                }
                if(spanin+spanout > 1)
                {
                    float off = (spanin+spanout)-1;
                    if(spanout > 0)
                    {
                        off *= 0.5f;
                        spanout -= off;
                        if(spanout < 0)
                        {
                            off += 0-spanout;
                            spanout = 0;
                        }
                    }
                    spanin = max(0.f, spanin-off);
                }
                if(spanin > 0)
                {
                    if(proj.lifespan < spanin)
                    {
                        proj.lifesize = clamp(proj.lifespan/spanin, WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                        break;
                    }
                    else if(spanout > 0 && proj.lifespan > (1-spanout))
                    {
                        if(type%2 || !proj.stuck)
                            proj.lifesize = clamp(1-((proj.lifespan-(1-spanout))/spanout), WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                        break;
                    }
                }
                proj.lifesize = WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags));
                break;
            }
            default: proj.lifesize = WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)); break;
        }
    }

    void iter(projent &proj)
    {
        proj.lifespan = clamp((proj.lifemillis-proj.lifetime)/float(max(proj.lifemillis, 1)), 0.f, 1.f);
        if(proj.target && proj.target->state != CS_ALIVE) proj.target = NULL;
        updatesticky(proj);
        if(proj.projtype == PRJ_SHOT)
        {
            updatetargets(proj);
            updatetaper(proj, proj.distance, true);
        }
        else if(updatebb(proj) && spherecheck(proj, proj.projcollide&BOUNCE_GEOM)) proj.resetinterp();
    }

    void effect(projent &proj)
    {
        if(projdebug)
        {
            float yaw, pitch;
            vectoyawpitch(vec(proj.vel).normalize(), yaw, pitch);
            part_radius(proj.o, vec(proj.radius, proj.radius, proj.radius), 2, 1, 1, 0x22FFFF);
            part_dir(proj.o, yaw, pitch, max(proj.vel.magnitude(), proj.radius+2), 2, 1, 1, 0xFF22FF);
        }
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                updatetaper(proj, proj.distance);
                float trans = fadeweap(proj)*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags));
                if(!proj.limited && !WK(proj.flags))// && proj.weap != W_MELEE)
                {
                    int vol = clamp(int(ceilf(255*proj.curscale)), 0, 255);
                    if(W2(proj.weap, cooktime, WS(proj.flags))) switch(W2(proj.weap, cooked, WS(proj.flags)))
                    {
                        case 4: case 5: vol = clamp(10+int(245*(1.f-proj.lifespan)*proj.lifesize*proj.curscale), 0, 255); break; // longer
                        case 1: case 2: case 3: default: vol = clamp(10+int(245*proj.lifespan*proj.lifesize*proj.curscale), 0, 255); break; // shorter
                    }
                    if(issound(proj.schan)) sounds[proj.schan].vol = vol;
                    else if(vol > 0) playsound(WSND2(proj.weap, WS(proj.flags), S_W_TRANSIT), proj.o, &proj, SND_LOOP, vol, -1, -1, &proj.schan);
                }
                int type = WF(WK(proj.flags), proj.weap, parttype, WS(proj.flags));
                switch(type)
                {
                    //case W_MELEE:
                    case W_SWORD:
                    {
                        part_flare(proj.from, proj.to, 1, PART_LIGHTNING_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, trans);
                        if(lastmillis-proj.lasteffect >= projtraildelay && proj.effectpos.dist(proj.to) >= 0.5f)
                        {
                            part_flare(proj.from, proj.to, 250, PART_LIGHTNING_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, 0.75f*trans);
                            proj.lasteffect = lastmillis - (lastmillis%projtraildelay);
                            proj.effectpos = proj.to;
                        }
                        break;
                    }
                    case W_PISTOL:
                    {
                        float size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.o.dist(proj.from)));
                        if(proj.lastbounce) size = min(size, max(proj.movement, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale));
                        if(size > 0)
                        {
                            proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            part_flare(proj.to, proj.o, 1, PART_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, clamp(1.25f-proj.lifespan, 0.35f, 0.85f)*trans);
                            if(projhints)
                            {
                                part_flare(proj.to, proj.o, 1, PART_FLARE, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*projhintsize*proj.curscale, clamp(1.25f-proj.lifespan, 0.35f, 0.85f)*projhintblend*trans);
                                part_create(PART_HINT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*projhintsize*proj.curscale, clamp(1.25f-proj.lifespan, 0.35f, 0.85f)*projhintblend*trans);
                            }
                        }
                        break;
                    }
                    case W_FLAMER:
                    {
                        float blend = clamp(1.25f-proj.lifespan, 0.35f, 0.85f)*(0.6f+(rnd(40)/100.f))*trans,
                            size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.o.dist(proj.from)));
                        if(proj.lastbounce) size = min(size, max(proj.movement, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale));
                        if(projfirehint) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, 0x120228), size*projfirehintsize, blend*projhintblend);
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay)
                        {
                            part_create(PART_FIREBALL_SOFT, max(int(projtraillength*0.5f*max(1.f-proj.lifespan, 0.1f)), 1), proj.o, FWCOL(H, partcol, proj), size, blend, -5);
                            proj.lasteffect = lastmillis - (lastmillis%projtraildelay);
                        }
                        else part_create(PART_FIREBALL_SOFT, 1, proj.o, FWCOL(H, partcol, proj), size, blend);
                        if(WS(proj.flags)) part_create(PART_FIREBALL_SOFT, 1, proj.o, FWCOL(H, partcol, proj), size*0.5f, blend);
                        break;
                    }
                    case W_GRENADE:
                    {
                        int interval = lastmillis%1000;
                        float fluc = 1.f+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                        part_create(PART_PLASMA_SOFT, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*max(proj.lifespan, 0.25f)+fluc, max(proj.lifespan, 0.25f)*trans);
                        if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*max(proj.lifespan, 0.25f)*projhintsize+fluc, max(proj.lifespan, 0.25f)*projhintblend*trans);
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay)
                        {
                            part_create(PART_SMOKE_LERP, projtraillength, proj.o, 0x888888, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.75f*trans, -5);
                            proj.lasteffect = lastmillis - (lastmillis%projtraildelay);
                        }
                        break;
                    }
                    case W_MINE:
                    {
                        int interval = lastmillis%1000;
                        float fluc = 1.f+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                        part_create(PART_PLASMA_SOFT, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*0.25f*max(proj.lifespan, 0.25f)+fluc, max(proj.lifespan, 0.25f)*trans);
                        if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*0.25f*max(proj.lifespan, 0.25f)*projhintsize+fluc, max(proj.lifespan, 0.25f)*projhintblend*trans);
                        break;
                    }
                    case W_ROCKET:
                    {
                        int interval = lastmillis%1000;
                        float fluc = 1.f+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                        part_create(PART_PLASMA_SOFT, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))+fluc, trans);
                        if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*projhintsize+fluc, projhintblend*trans);
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay/10)
                        {
                            part_create(PART_FIREBALL_SOFT, max(projtraillength, 2)/2, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.85f*trans, -1);
                            part_create(PART_SMOKE_LERP, max(projtraillength, 1), proj.o, 0x222222, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*1.5f, trans, -5);
                            proj.lasteffect = lastmillis - (lastmillis%(projtraildelay/10));
                        }
                        break;
                    }
                    case W_SHOTGUN:
                    {
                        if(proj.stuck || (!WK(proj.flags) && W2(proj.weap, fragweap, WS(proj.flags)) >= 0))
                        {
                            part_create(PART_PLASMA, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*5*proj.curscale, 0.75f*trans);
                            part_create(PART_PLASMA, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*10*proj.curscale, 0.75f*trans);
                            if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*10*projhintsize*proj.curscale, 0.75f*projhintblend*trans);
                        }
                        else
                        {
                            float size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.o.dist(proj.from)));
                            if(proj.lastbounce) size = min(size, max(proj.movement, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale));
                            if(size > 0)
                            {
                                proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                                part_flare(proj.to, proj.o, 1, PART_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, clamp(1.25f-proj.lifespan, 0.5f, 1.f)*trans);
                                if(projhints) part_flare(proj.to, proj.o, 1, PART_FLARE, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*(1.f-proj.lifespan)*projhintsize*proj.curscale, clamp(1.25f-proj.lifespan, 0.5f, 1.f)*projhintblend*trans);
                            }
                        }
                        break;
                    }
                    case W_SMG:
                    {
                        float size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.o.dist(proj.from)));
                        if(proj.lastbounce) size = min(size, max(proj.movement, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale));
                        if(size > 0)
                        {
                            proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            if(!proj.stuck)
                            {
                                part_flare(proj.to, proj.o, 1, PART_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, clamp(1.25f-proj.lifespan, 0.5f, 1.f)*trans);
                                if(projhints) part_flare(proj.to, proj.o, 1, PART_FLARE, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*(1.f-proj.lifespan)*projhintsize*proj.curscale, clamp(1.25f-proj.lifespan, 0.5f, 1.f)*projhintblend*trans);
                            }
                            if(proj.stuck || (!WK(proj.flags) && W2(proj.weap, fragweap, WS(proj.flags)) >= 0))
                            {
                                part_create(PART_PLASMA, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*4*proj.curscale, 0.75f*trans);
                                if(proj.stuck) part_create(PART_PLASMA, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*6*proj.curscale, 0.75f*trans);
                                if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*5.f*projhintsize*proj.curscale, 0.75f*projhintblend*trans);
                            }
                        }
                        break;
                    }
                    case W_PLASMA:
                    {
                        float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            part_explosion(proj.o, expl, PART_SHOCKBALL, 1, FWCOL(H, partcol, proj), 1.f, trans);
                            if(WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) >= 1)
                                part_explosion(proj.o, expl*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)), PART_SHOCKWAVE, 1, projhint(proj.owner, FWCOL(H, partcol, proj)), 1.f, 0.125f*projhintblend*trans);
                        }
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay)
                        {
                            part_create(PART_PLASMA_SOFT, projtraillength, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.lifesize*proj.curscale, 0.125f*trans, 20);
                            proj.lasteffect = lastmillis - (lastmillis%projtraildelay);
                        }
                        part_create(PART_PLASMA_SOFT, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.lifesize*proj.curscale, 0.5f*trans);
                        part_create(PART_ELECTRIC_SOFT, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*0.45f*proj.lifesize*proj.curscale, 0.5f*trans);
                        if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*projhintsize*proj.lifesize*proj.curscale, 0.5f*projhintblend*trans);
                        break;
                    }
                    case W_RIFLE: case W_ZAPPER:
                    {
                        vec from = type != W_ZAPPER || !proj.owner || proj.owner->weapselect != proj.weap || WK(proj.flags) ? proj.from : proj.owner->muzzlepos(proj.weap);
                        float size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.o.dist(from)));
                        if(proj.lastbounce) size = min(size, max(proj.movement, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale));
                        if(size > 0)
                        {
                            if(type != W_ZAPPER || !proj.owner || proj.owner->weapselect != proj.weap || WK(proj.flags))
                                proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            else proj.to = vec(proj.o).sub(vec(proj.o).sub(from).normalize().mul(size));
                            part_flare(proj.to, proj.o, 1, type != W_ZAPPER ? PART_FLARE : PART_LIGHTZAP_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, 0.85f*trans);
                            if(projhints) part_flare(proj.to, proj.o, 1, type != W_ZAPPER ? PART_FLARE : PART_LIGHTZAP_FLARE, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*projhintsize*proj.curscale, projhintblend*trans);
                            if(type != W_ZAPPER && !WK(proj.flags) && W2(proj.weap, fragweap, WS(proj.flags)) >= 0)
                            {
                                part_create(PART_PLASMA, 1, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale*5, 0.5f*trans);
                                if(projhints) part_create(PART_HINT_SOFT, 1, proj.o, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*5*projhintsize*proj.curscale, projhintblend*trans);
                            }
                        }
                        break;
                    }
                    default: break;
                }
                if(proj.stuck && !proj.beenused && WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) == 2)
                {
                    float dist = WX(WK(proj.flags), proj.weap, proxdist, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                    int stucktime = lastmillis-proj.stuck, stuckdelay = WF(WK(proj.flags), proj.weap, proxdelay, WS(proj.flags));
                    if(stuckdelay && stuckdelay > stucktime) dist *= stucktime/float(stuckdelay);
                    if(dist > 1e-6f)
                    {
                        vec from = vec(proj.o).add(vec(proj.norm).mul(proj.radius*2)), to = vec(from).add(vec(proj.norm).mul(dist)), dir = vec(to).sub(from);
                        float mag = dir.magnitude();
                        if(mag > 1e-6f)
                        {
                            dir.div(mag);
                            float blocked = tracecollide(&proj, from, dir, mag, RAY_CLIPMAT|RAY_ALPHAPOLY, true);
                            if(blocked >= 0) to = vec(from).add(vec(proj.norm).mul(blocked+proj.radius));
                            part_flare(proj.o, to, 1, PART_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*0.25f*proj.curscale*proj.lifesize, 0.75f*trans);
                        }
                    }
                }
                break;
            }
            case PRJ_GIBS: case PRJ_DEBRIS:
            {
                if(proj.projtype == PRJ_GIBS && !game::nogore && game::bloodscale > 0)
                {
                    if(proj.movement >= 1 && lastmillis-proj.lasteffect >= 1000 && proj.lifetime >= min(proj.lifemillis, proj.fadetime))
                    {
                        part_splash(PART_BLOOD, 1, game::bloodfade, proj.o, 0x229999, (rnd(game::bloodsize/2)+(game::bloodsize/2))/10.f, 1, 100, DECAL_BLOOD, int(proj.radius), 15);
                        proj.lasteffect = lastmillis - (lastmillis%1000);
                    }
                    if(!game::bloodsparks) break;
                }
                if(!proj.limited)
                {
                    bool effect = false;
                    float radius = (proj.radius+0.5f)*(clamp(1.f-proj.lifespan, 0.1f, 1.f)+0.25f), blend = clamp(1.25f-proj.lifespan, 0.25f, 1.f)*(0.75f+(rnd(25)/100.f)); // gets smaller as it gets older
                    if(projtrails && lastmillis-proj.lasteffect >= projtraildelay) { effect = true; proj.lasteffect = lastmillis - (lastmillis%projtraildelay); }
                    int len = effect ? max(int(projtraillength*0.5f*max(1.f-proj.lifespan, 0.1f)), 1) : 1,
                        colour = !proj.id && isweap(proj.weap) ? FWCOL(H, explcol, proj) : pulsecols[PULSE_FIRE][rnd(PULSECOLOURS)];
                    part_create(proj.projtype == PRJ_GIBS || (!proj.id && isweap(proj.weap) && WF(WK(proj.flags), proj.weap, explcol, WS(proj.flags)) <= PC(DISCO)) ? PART_SPARK : PART_FIREBALL, len, proj.o, colour, radius, blend, -5);
                }
                break;
            }
            case PRJ_EJECT:
            {
                if(isweap(proj.weap) && ejecthint)
                    part_create(PART_HINT, 1, proj.o, W(proj.weap, colour), max(proj.xradius, proj.yradius)*1.25f, clamp(1.f-proj.lifespan, 0.1f, 1.f)*0.35f);
                bool moving = proj.movement >= 1;
                if(moving && lastmillis-proj.lasteffect >= 100)
                {
                    part_create(PART_SMOKE, 75, proj.o, 0x222222, max(proj.xradius, proj.yradius), clamp(1.f-proj.lifespan, 0.1f, 1.f)*0.35f, -3);
                    proj.lasteffect = lastmillis - (lastmillis%100);
                }
            }
            case PRJ_AFFINITY:
            {
                bool moving = proj.movement >= 1;
                if(moving && lastmillis-proj.lasteffect >= 50)
                {
                    vec o(proj.o);
                    float size = max(proj.xradius, proj.yradius);
                    if(m_capture(game::gamemode)) o.z -= proj.zradius;
                    else size = max(proj.zradius, size);
                    part_create(PART_SMOKE, 150, o, 0xFFFFFF, size, 0.5f, -10);
                    proj.lasteffect = lastmillis - (lastmillis%50);
                }
            }
            default: break;
        }
    }

    void destroy(projent &proj)
    {
        proj.lifespan = clamp((proj.lifemillis-proj.lifetime)/float(max(proj.lifemillis, 1)), 0.f, 1.f);
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                updatetargets(proj, 2);
                if(proj.projcollide&COLLIDE_PROJ) collideprojs.removeobj(&proj);
                int vol = clamp(int(255*proj.curscale), 0, 255), type = WF(WK(proj.flags), proj.weap, parttype, WS(proj.flags)), len = W2(proj.weap, partfade, WS(proj.flags));
                if(!proj.limited) switch(type)
                {
                    case W_PISTOL:
                    {
                        vol = clamp(int(vol*(1.f-proj.lifespan)), 0, 255);
                        part_create(PART_SMOKE_LERP, len*2, proj.o, 0x999999, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), -20);
                        float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            part_explosion(proj.o, expl*0.5f, PART_EXPLOSION, len, FWCOL(H, explcol, proj), 1.f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                            part_splash(PART_SPARK, 5, 250, proj.o, FWCOL(H, partcol, proj), 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), 1, 1, 0, expl, 15);
                            if(WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) >= 1)
                                part_explosion(proj.o, expl*0.5f*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)), PART_SHOCKWAVE, len/2, projhint(proj.owner, FWCOL(H, explcol, proj)), 1.f, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))*projhintblend);
                            adddecal(DECAL_SCORCH_SHORT, proj.o, proj.norm, expl*0.5f);
                            adddynlight(proj.o, expl, FWCOL(P, explcol, proj), len, 10);
                        }
                        else adddecal(DECAL_BULLET, proj.o, proj.norm, max(WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.25f)*4);
                        break;
                    }
                    case W_FLAMER: case W_GRENADE: case W_MINE: case W_ROCKET:
                    { // all basically explosions
                        float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(type != W_FLAMER) part_create(PART_PLASMA_SOFT, len, proj.o, FWCOL(H, partcol, proj), max(expl*0.5f, 0.5f), 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))); // corona
                        if(expl > 0)
                        {
                            if(type != W_FLAMER) part_explosion(proj.o, expl, PART_EXPLOSION, len, FWCOL(H, explcol, proj), 1.f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                            part_splash(PART_SPARK, 20, len*2, proj.o, FWCOL(H, partcol, proj), 0.75f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), 1, 0, expl, 20);
                            if(WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) >= 1)
                                part_explosion(proj.o, expl*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)), PART_SHOCKWAVE, len/2, projhint(proj.owner, FWCOL(H, explcol, proj)), 1.f, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))*projhintblend);
                        }
                        else expl = max(WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.25f)*4;
                        part_create(PART_SMOKE_LERP_SOFT, len*3, proj.o, 0x444444, expl*1.5f, 0.75f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), type != W_FLAMER ? -15 : -10);
                        if(type != W_FLAMER && !m_kaboom(game::gamemode, game::mutators) && game::nogore != 2 && game::debrisscale > 0)
                        {
                            int debris = rnd(type != W_ROCKET ? 5 : 10)+5, amt = int((rnd(debris)+debris+1)*game::debrisscale);
                            loopi(amt) create(proj.o, vec(proj.o).add(proj.vel), true, NULL, PRJ_DEBRIS, rnd(game::debrisfade)+game::debrisfade, 0, rnd(501), rnd(101)+50, 0, proj.weap, 0, proj.flags);
                        }
                        adddecal(DECAL_ENERGY, proj.o, proj.norm, expl*0.75f, bvec::fromcolor(FWCOL(P, explcol, proj)));
                        if(type != W_FLAMER || WK(proj.flags) || W2(proj.weap, fragweap, WS(proj.flags))%W_MAX != W_FLAMER) loopi(type != W_ROCKET ? 5 : 10)
                        {
                            vec to(proj.o);
                            loopk(3) to.v[k] += expl*(rnd(201)-100)/200.f;
                            part_create(PART_FIREBALL_SOFT, len*2, to, FWCOL(H, explcol, proj), expl*1.25f, (0.5f+(rnd(50)/100.f))*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), -10);
                        }
                        adddecal(type == W_FLAMER ? DECAL_SCORCH_SHORT : DECAL_SCORCH, proj.o, proj.norm, expl*0.5f);
                        adddynlight(proj.o, expl, FWCOL(P, explcol, proj), len, 10);
                        break;
                    }
                    case W_SHOTGUN: case W_SMG:
                    {
                        vol = clamp(int(vol*(1.f-proj.lifespan)), 0, 255);
                        part_splash(PART_SPARK, type == W_SHOTGUN ? 5 : 3, len*2, proj.o, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale*0.5f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), 1, 0, 16, 15);
                        float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            part_explosion(proj.o, expl*0.5f, PART_EXPLOSION, len, FWCOL(H, explcol, proj), 1.f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                            if(WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) >= 1)
                                part_explosion(proj.o, expl*0.5f*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)), PART_SHOCKWAVE, len/2, projhint(proj.owner, FWCOL(H, explcol, proj)), 1.f, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))*projhintblend);
                            adddecal(DECAL_SCORCH_SHORT, proj.o, proj.norm, expl*0.5f);
                            adddynlight(proj.o, expl, FWCOL(P, explcol, proj), len, 10);
                        }
                        else adddecal(DECAL_BULLET, proj.o, proj.norm, max(WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.25f)*4);
                        break;
                    }
                    case W_PLASMA:
                    {
                        float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            part_explosion(proj.o, expl, PART_SHOCKBALL, len, FWCOL(H, explcol, proj), 1.f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                            if(WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) >= 1)
                                part_explosion(proj.o, expl*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)), PART_SHOCKWAVE, len/2, projhint(proj.owner, FWCOL(H, explcol, proj)), 1.f, 0.25f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))*projhintblend);
                        }
                        else expl = max(WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags)), 0.25f)*4;
                        part_splash(PART_SPARK, 20, len*2, proj.o, FWCOL(H, partcol, proj), 0.25f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), 1, 0, expl, 20);
                        part_create(PART_PLASMA_SOFT, len, proj.o, FWCOL(H, partcol, proj), expl*0.75f, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                        part_create(PART_ELECTRIC_SOFT, len/2, proj.o, FWCOL(H, partcol, proj), expl*0.375f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                        part_create(PART_SMOKE, len, proj.o, FWCOL(H, partcol, proj), expl*0.35f, 0.35f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), -30);
                        adddecal(DECAL_ENERGY, proj.o, proj.norm, expl*0.75f, bvec::fromcolor(FWCOL(P, explcol, proj)));
                        adddynlight(proj.o, 1.1f*expl, FWCOL(P, explcol, proj), len, 10);
                        break;
                    }
                    case W_RIFLE: case W_ZAPPER:
                    {
                        vec from = type != W_ZAPPER || !proj.owner || proj.owner->weapselect != proj.weap || WK(proj.flags) ? proj.from : proj.owner->muzzlepos(proj.weap);
                        float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize),
                              size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.o.dist(from)));
                        if(proj.lastbounce) size = min(size, max(proj.movement, WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale));
                        if(size > 0)
                        {
                            if(type != W_ZAPPER || !proj.owner || proj.owner->weapselect != proj.weap || WK(proj.flags))
                                proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            else proj.to = vec(proj.o).sub(vec(proj.o).sub(from).normalize().mul(size));
                        }
                        if(expl > 0)
                        {
                            part_create(type != W_ZAPPER ? PART_PLASMA_SOFT : PART_ELECZAP_SOFT, len, proj.o, FWCOL(H, partcol, proj), expl*0.5f, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))); // corona
                            part_splash(PART_SPARK, 10, len*2, proj.o, FWCOL(H, partcol, proj), 0.25f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)), 1, 0, expl, 15);
                            part_explosion(proj.o, expl, PART_SHOCKBALL, len, FWCOL(H, explcol, proj), 1.f, WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                            if(WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) >= 1)
                                part_explosion(proj.o, expl*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)), PART_SHOCKWAVE, len/2, projhint(proj.owner, FWCOL(H, explcol, proj)), 1.f, 0.5f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags))*projhintblend);
                        }
                        part_flare(proj.to, proj.o, len, type != W_ZAPPER ? PART_FLARE : PART_LIGHTZAP_FLARE, FWCOL(H, partcol, proj), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*proj.curscale, 0.85f*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                        if(projhints) part_flare(proj.to, proj.o, len, type != W_ZAPPER ? PART_FLARE : PART_LIGHTZAP_FLARE, projhint(proj.owner, FWCOL(H, partcol, proj)), WF(WK(proj.flags), proj.weap, partsize, WS(proj.flags))*projhintsize*proj.curscale, projhintblend*WF(WK(proj.flags), proj.weap, partblend, WS(proj.flags)));
                        adddecal(DECAL_SCORCH, proj.o, proj.norm, max(expl, 2.f));
                        adddecal(DECAL_ENERGY, proj.o, proj.norm, max(expl*0.5f, 1.f), bvec::fromcolor(FWCOL(P, explcol, proj)));
                        adddynlight(proj.o, 1.1f*expl, FWCOL(P, explcol, proj), len, 10);
                        break;
                    }
                    default: break;
                }
                if(vol > 0 && !proj.limited && !WK(proj.flags))
                {
                    int slot = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize) > 0 ? S_W_EXPLODE : S_W_DESTROY;
                    playsound(WSND2(proj.weap, WS(proj.flags), slot), proj.o, NULL, 0, vol);
                }
                if(proj.owner)
                {
                    if(!WK(proj.flags) && !m_insta(game::gamemode, game::mutators) && W2(proj.weap, fragweap, WS(proj.flags)) >= 0)
                    {
                        int f = W2(proj.weap, fragweap, WS(proj.flags)), w = f%W_MAX,
                            life = W2(proj.weap, fragtime, WS(proj.flags)), delay = W2(proj.weap, fragtimedelay, WS(proj.flags));
                        float mag = max(proj.vel.magnitude(), W2(proj.weap, fragspeedmin, WS(proj.flags))),
                              scale = W2(proj.weap, fragscale, WS(proj.flags))*proj.curscale,
                              offset = proj.hit || proj.stick ? W2(proj.weap, fragoffset, WS(proj.flags)) : 1e-6f,
                              skew = proj.hit || proj.stuck ? W2(proj.weap, fragskew, WS(proj.flags)) : W2(proj.weap, fragspread, WS(proj.flags));
                        vec dir = vec(proj.stuck ? proj.norm : proj.vel).normalize(),
                            pos = vec(proj.o).sub(vec(dir).mul(offset));
                        if(W2(proj.weap, fragspeedmax, WS(proj.flags)) > 0) mag = min(mag, W2(proj.weap, fragspeedmax, WS(proj.flags)));
                        if(W2(proj.weap, fragjump, WS(proj.flags)) > 0) life -= int(ceilf(life*W2(proj.weap, fragjump, WS(proj.flags))));
                        loopi(W2(proj.weap, fragrays, WS(proj.flags)))
                        {
                            vec to = vec(pos).add(dir);
                            if(W2(proj.weap, fragspeed, WS(proj.flags)) > 0)
                                mag = rnd(W2(proj.weap, fragspeed, WS(proj.flags)))*0.5f+W2(proj.weap, fragspeed, WS(proj.flags))*0.5f;
                            if(skew > 0) to.add(vec(rnd(2001)-1000, rnd(2001)-1000, rnd(2001)-1000).normalize().mul(skew*mag));
                            if(W2(proj.weap, fragrel, WS(proj.flags)) != 0) to.add(vec(dir).mul(W2(proj.weap, fragrel, WS(proj.flags))*mag));
                            create(pos, to, proj.local, proj.owner, PRJ_SHOT, max(life, 1), W2(proj.weap, fragtime, WS(proj.flags)), delay, W2(proj.weap, fragspeed, WS(proj.flags)), proj.id, w, -1, (f >= W_MAX ? HIT_ALT : 0)|HIT_FLAK, scale, true, &proj);
                            delay += W2(proj.weap, fragtimeiter, WS(proj.flags));
                        }
                    }
                    if(proj.local)
                        client::addmsg(N_DESTROY, "ri8", proj.owner->clientnum, lastmillis-game::maptime, proj.weap, proj.flags, WK(proj.flags) ? -proj.id : proj.id, 0, int(proj.curscale*DNF), 0);
                }
                break;
            }
            case PRJ_ENT:
            {
                if(proj.beenused <= 1 && proj.local && proj.owner)
                    client::addmsg(N_DESTROY, "ri8", proj.owner->clientnum, lastmillis-game::maptime, -1, 0, proj.id, 0, int(proj.curscale*DNF), 0);
                break;
            }
            case PRJ_AFFINITY:
            {
                if(proj.beenused <= 1) client::addmsg(N_RESETAFFIN, "ri", proj.id);
                if(m_capture(game::gamemode) && capture::st.flags.inrange(proj.id)) capture::st.flags[proj.id].proj = NULL;
                else if(m_bomber(game::gamemode) && bomber::st.flags.inrange(proj.id)) bomber::st.flags[proj.id].proj = NULL;
                break;
            }
            default: break;
        }
    }

    int check(projent &proj, const vec &dir, int mat = -1)
    {
        if(proj.projtype == PRJ_SHOT ? proj.o.z < 0 : !insideworld(proj.o, false)) return 0; // remove, always..
        int chk = 0;
        if(proj.extinguish&1 || proj.extinguish&2)
        {
            if(mat < 0) mat = lookupmaterial(proj.o);
            if(proj.extinguish&1 && (mat&MATF_VOLUME) == MAT_WATER) chk |= 1;
            if(proj.extinguish&2 && ((mat&MATF_VOLUME) == MAT_LAVA || mat&MAT_DEATH)) chk |= 2;
        }
        if(chk)
        {
            if(chk&1 && !proj.limited && !WK(proj.flags))// && proj.weap != W_MELEE)
            {
                int vol = clamp(int(ceilf(48*proj.curscale)), 0, 255), snd = S_EXTINGUISH;
                float size = max(proj.radius, 1.f);
                if(proj.projtype == PRJ_SHOT && isweap(proj.weap))
                {
                    snd = WSND2(proj.weap, WS(proj.flags), S_W_EXTINGUISH);
                    vol = clamp(10+int(245*proj.lifespan*proj.lifesize*proj.curscale), 0, 255);
                    float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                    if(expl > 0) size *= expl*1.5f;
                    else size *= 2.5f;
                }
                else size *= 2.5f;
                if(vol > 0) playsound(snd, proj.o, NULL, 0, vol);
                part_create(PART_SMOKE, 500, proj.o, 0xAAAAAA, max(size, 1.5f), 1, -10);
                proj.limited = true;
                if(proj.projtype == PRJ_DEBRIS) proj.light.material[0] = bvec(255, 255, 255);
            }
            proj.norm = dir;
            if(proj.extinguish&4) return 0;
        }
        return 1;
    }

    int impact(projent &proj, const vec &dir, physent *d, int flags, const vec &norm)
    {
        if((!d || dynent::is(d)) && (d ? proj.projcollide&(d->type == ENT_PROJ ? COLLIDE_SHOTS : COLLIDE_PLAYER) : proj.projcollide&COLLIDE_GEOM))
        {
            proj.norm = norm;
            if(d)
            {
                /*if(proj.norm.iszero())*/ proj.norm = vec(proj.o).sub(d->center()).normalize();
                if(proj.norm.iszero()) proj.norm = vec(proj.vel).normalize().neg();
                if(gameent::is(d) && proj.projcollide&IMPACT_PLAYER && proj.projcollide&STICK_PLAYER)
                {
                    stick(proj, dir, (gameent *)d);
                    return 1;
                }
                if(!hiteffect(proj, d, flags, proj.norm)) return 1;
            }
            else
            {
                if(proj.norm.iszero()) proj.norm = vec(proj.vel).normalize().neg();
                if(proj.projcollide&IMPACT_GEOM)
                {
                    if(proj.projcollide&STICK_GEOM)
                    {
                        stick(proj, dir);
                        return 1;
                    }
                    else if(proj.projtype == PRJ_SHOT && proj.weap == W_MELEE && !WS(proj.flags) && (proj.owner == game::player1 || proj.owner->ai) && proj.owner->state == CS_ALIVE && physics::canimpulse(proj.owner, A_A_PARKOUR, true))
                    {
                        gameent *d = (gameent *)proj.owner;
                        vec keepvel = vec(d->vel).add(d->falling);
                        int cost = int(impulsecost*impulsecostgrabscale);
                        float mag = physics::impulsevelocity(d, impulseparkourgrab, cost, A_A_PARKOUR, impulseparkourgrabredir, keepvel);
                        if(mag > 0)
                        {
                            d->vel = vec(d->yaw*RAD, (d != game::player1 || physics::grabstyle ? -d->pitch : d->pitch)*RAD).mul(mag).add(keepvel);
                            d->doimpulse(cost, IM_T_GRAB, lastmillis);
                            d->turnmillis = PHYSMILLIS;
                            d->turnside = 0;
                            d->turnyaw = d->turnroll = 0;
                            client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_GRAB);
                            game::impulseeffect(d);
                            game::footstep(d);
                        }
                    }
                }
                if(proj.projcollide&(IMPACT_GEOM|BOUNCE_GEOM) && proj.projcollide&DRILL_GEOM)
                {
                    vec orig = proj.o;
                    loopi(WF(WK(proj.flags), proj.weap, drill, WS(proj.flags)))
                    {
                        proj.o.add(vec(dir).normalize());
                        if(!collide(&proj, dir, 0.f, proj.projcollide&COLLIDE_DYNENT) && !collideinside && !hitplayer) return 1;
                    }
                    proj.o = orig; // continues below
                }
            }
            bool ricochet = proj.projcollide&(d ? (d->type == ENT_PROJ ? BOUNCE_SHOTS : BOUNCE_PLAYER) : BOUNCE_GEOM);
            bounce(proj, ricochet);
            if(ricochet)
            {
                if(proj.projtype != PRJ_SHOT || proj.child || WK(proj.flags) || !weaptype[proj.weap].traced)
                {
                    reflect(proj, proj.norm);
                    proj.o.add(vec(proj.norm).mul(0.1f)); // offset from surface slightly to avoid initial collision
                    proj.movement = 0;
                }
                proj.lastbounce = lastmillis;
                return 2; // bounce
            }
            else if(proj.projcollide&(d ? (d->type == ENT_PROJ ? IMPACT_SHOTS : IMPACT_PLAYER) : IMPACT_GEOM))
                return 0; // die on impact
        }
        return 1; // live!
    }

    int step(projent &proj, const vec &dir, const vec &oldpos, bool skip)
    {
        int ret = check(proj, dir);
        if(!ret) return 0;
        if(!skip && proj.interacts && checkitems(proj)) return -1;
        if(proj.projtype == PRJ_SHOT) updatetaper(proj, proj.distance+proj.o.dist(oldpos));
        if(ret == 1 && (collide(&proj, dir, 0.f, proj.projcollide&COLLIDE_DYNENT) || collideinside))
            ret = impact(proj, dir, hitplayer, hitflags, collidewall);
        return ret;
    }

    int trace(projent &proj, const vec &dir, const vec &oldpos, int mat, bool skip)
    {
        int ret = check(proj, dir, mat);
        if(!ret) return 0;
        vec to(proj.o), ray = dir;
        to.add(dir);
        float maxdist = ray.magnitude();
        if(maxdist > 0)
        {
            ray.mul(1/maxdist);
            float dist = tracecollide(&proj, proj.o, ray, maxdist, RAY_CLIPMAT|RAY_ALPHAPOLY, proj.projcollide&COLLIDE_DYNENT),
                  total = dist >= 0 ? dist : maxdist;
            if(!skip && proj.interacts && checkitems(proj, ray, total)) return -1;
            proj.o.add(vec(ray).mul(total));
            if(proj.projtype == PRJ_SHOT) updatetaper(proj, proj.distance+proj.o.dist(oldpos));
            if(dist >= 0) ret = impact(proj, dir, hitplayer, hitflags, hitsurface);
        }
        return ret;
    }

    void escaped(projent &proj, const vec &pos, const vec &dir)
    {
        if(!proj.owner || !(proj.projcollide&COLLIDE_OWNER) || proj.lastbounce || !proj.spawntime) proj.escaped = true;
        else
        {
            int delay = proj.projtype == PRJ_SHOT ? W2(proj.weap, escapedelay, WS(proj.flags)) : PHYSMILLIS;
            if(lastmillis-proj.spawntime >= delay)
            {
                if(proj.spawntime && lastmillis-proj.spawntime >= delay*2) proj.escaped = true;
                else if(proj.projcollide&COLLIDE_TRACE)
                {
                    vec to = vec(pos).add(dir);
                    float x1 = floor(min(pos.x, to.x)), y1 = floor(min(pos.y, to.y)),
                          x2 = ceil(max(pos.x, to.x)), y2 = ceil(max(pos.y, to.y)),
                          maxdist = dir.magnitude(), dist = 1e16f;
                    if(!physics::xtracecollide(&proj, pos, to, x1, x2, y1, y2, maxdist, dist, proj.owner) || dist > maxdist) proj.escaped = true;
                }
                else if(!physics::xcollide(&proj, dir, proj.owner)) proj.escaped = true;
            }
        }
    }

    bool moveproj(projent &proj, float secs, bool skip = false)
    {
        vec dir(proj.vel), pos(proj.o);
        int mat = lookupmaterial(pos);
        if(isliquid(mat&MATF_VOLUME) && proj.liquidcoast > 0) dir.div(proj.liquidcoast);
        dir.mul(secs);

        if(!proj.escaped && proj.owner) escaped(proj, pos, dir);

        bool blocked = false;
        if(proj.projcollide&COLLIDE_TRACE)
        {
            switch(trace(proj, dir, pos, mat, skip))
            {
                case 2: blocked = true; break;
                case 1: break;
                case 0: return false;
                case -1: return moveproj(proj, secs, true);
            }
        }
        else
        {
            if(proj.projtype == PRJ_SHOT)
            {
                float stepdist = dir.magnitude();
                vec ray(dir);
                ray.mul(1/stepdist);
                float barrier = raycube(proj.o, ray, stepdist, RAY_CLIPMAT|RAY_POLY);
                if(barrier < stepdist)
                {
                    proj.o.add(ray.mul(barrier-0.15f));
                    switch(step(proj, ray, pos, skip))
                    {
                        case 2: proj.o = pos; blocked = true; break;
                        case 1: proj.o = pos; break;
                        case 0: return false;
                        case -1: return moveproj(proj, secs, true);
                    }
                }
            }
            if(!blocked)
            {
                proj.o.add(dir);
                switch(step(proj, dir, pos, skip))
                {
                    case 2: proj.o = pos; if(proj.projtype == PRJ_SHOT) blocked = true; break;
                    case 1: default: break;
                    case 0: proj.o = pos; if(proj.projtype == PRJ_SHOT) { dir.rescale(max(dir.magnitude()-0.15f, 0.0f)); proj.o.add(dir); } return false;
                    case -1: return moveproj(proj, secs, true);
                }
            }
        }

        float dist = proj.o.dist(pos), diff = dist/float(4*RAD);
        if(!blocked) proj.movement += dist;
        proj.distance += dist;
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                if(proj.stuck) break;
                if(proj.weap == W_MINE)
                {
                    if(!proj.lastbounce || proj.movement >= 1)
                    {
                        vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                        if(proj.vel.dot2(axis) >= 0) { proj.pitch -= diff; if(proj.pitch < -180) proj.pitch = 180 - fmod(180 - proj.pitch, 360); }
                        else { proj.pitch += diff; if(proj.pitch > 180) proj.pitch = fmod(proj.pitch + 180, 360) - 180; }
                        break;
                    }
                    if(proj.pitch != 0)
                    {
                        if(proj.pitch < 0) { proj.pitch += max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.pitch > 0) proj.pitch = 0; }
                        else if(proj.pitch > 0) { proj.pitch -= max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.pitch < 0) proj.pitch = 0; }
                    }
                    break;
                }
                if(proj.weap == W_ROCKET)
                {
                    vectoyawpitch(vec(proj.vel).normalize(), proj.yaw, proj.pitch);
                    break;
                }
                if(proj.weap != W_GRENADE) break;
            }
            case PRJ_DEBRIS: case PRJ_GIBS: case PRJ_AFFINITY: case PRJ_VANITY:
            {
                if(!proj.lastbounce || proj.movement >= 1)
                {
                    float yaw = proj.yaw, pitch = proj.pitch, speed = diff*secs;
                    vectoyawpitch(vec(proj.vel).normalize(), yaw, pitch);
                    game::scaleyawpitch(proj.yaw, proj.pitch, yaw, pitch, speed, speed);
                    vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                    if(proj.vel.dot2(axis) >= 0) { proj.roll -= diff; if(proj.roll < -180) proj.roll = 180 - fmod(180 - proj.roll, 360); }
                    else { proj.roll += diff; if(proj.roll > 180) proj.roll = fmod(proj.roll + 180, 360) - 180; }
                }
                if(proj.projtype != PRJ_VANITY) break;
            }
            case PRJ_EJECT:
                if(!proj.lastbounce || proj.movement >= 1)
                {
                    vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                    if(proj.vel.dot2(axis) >= 0) { proj.pitch -= diff; if(proj.pitch < -180) proj.pitch = 180 - fmod(180 - proj.pitch, 360); }
                    else { proj.pitch += diff; if(proj.pitch > 180) proj.pitch = fmod(proj.pitch + 180, 360) - 180; }
                    break;
                }
            case PRJ_ENT:
            {
                if(proj.pitch != 0)
                {
                    if(proj.pitch < 0) { proj.pitch += max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.pitch > 0) proj.pitch = 0; }
                    else if(proj.pitch > 0) { proj.pitch -= max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.pitch < 0) proj.pitch = 0; }
                }
                if(proj.roll != 0)
                {
                    if(proj.roll < 0) { proj.roll += max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.roll > 0) proj.roll = 0; }
                    else if(proj.roll > 0) { proj.roll -= max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.roll < 0) proj.roll = 0; }
                }
                break;
            }
            default: break;
        }
        return true;
    }

    bool move(projent &proj, int qtime)
    {
        float secs = float(qtime)/1000.f;
        if(proj.projtype == PRJ_AFFINITY && m_bomber(game::gamemode) && proj.target && !proj.lastbounce)
        {
            vec targ = vec(proj.target->o).sub(proj.o).normalize();
            if(!targ.iszero())
            {
                vec dir = vec(proj.vel).normalize();
                float amt = clamp(bomberspeeddelta*secs, 1e-8f, 1.f), mag = max(proj.vel.magnitude(), bomberspeedmin);
                if(bomberspeedmax > 0) mag = min(mag, bomberspeedmax);
                dir.mul(1.f-amt).add(targ.mul(amt)).normalize();
                if(!dir.iszero()) (proj.vel = dir).mul(mag);
            }
        }
        else if(proj.projtype == PRJ_SHOT && proj.escaped && WF(WK(proj.flags), proj.weap, guided, WS(proj.flags)) && lastmillis-proj.spawntime >= WF(WK(proj.flags), proj.weap, guideddelay, WS(proj.flags)))
        {
            vec dir = vec(proj.vel).normalize();
            switch(WF(WK(proj.flags), proj.weap, guided, WS(proj.flags)))
            {
                case 6: default: break; // use original dest
                case 5: case 4: case 3: case 2:
                {
                    if(WF(WK(proj.flags), proj.weap, guided, WS(proj.flags))%2 && proj.target && proj.target->state == CS_ALIVE)
                        proj.dest = proj.target->center();
                    gameent *t = NULL;
                    switch(WF(WK(proj.flags), proj.weap, guided, WS(proj.flags)))
                    {
                        case 2: case 3: default:
                        {
                            if(proj.owner && proj.owner->state == CS_ALIVE)
                            {
                                vec dest;
                                findorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, dest);
                                t = game::intersectclosest(proj.owner->o, dest, proj.owner);
                                break;
                            } // otherwise..
                        }
                        case 4: case 5:
                        {
                            float yaw, pitch;
                            vectoyawpitch(dir, yaw, pitch);
                            vec dest; findorientation(proj.o, yaw, pitch, dest);
                            t = game::intersectclosest(proj.o, dest, proj.owner);
                            break;
                        }
                    }
                    if(t && (!m_team(game::gamemode, game::mutators) || (t->type != ENT_PLAYER && t->type != ENT_AI) || ((gameent *)t)->team != proj.owner->team))
                    {
                        if(proj.target && proj.o.dist(proj.target->o) < proj.o.dist(t->o)) break;
                        proj.target = t;
                        proj.dest = proj.target->center();
                    }
                    break;
                }
                case 1:
                {
                    if(proj.owner && proj.owner->state == CS_ALIVE)
                        findorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, proj.dest);
                    break;
                }
            }
            if(!proj.dest.iszero())
            {
                float amt = clamp(WF(WK(proj.flags), proj.weap, speeddelta, WS(proj.flags))*secs, 1e-8f, 1.f),
                      mag = max(proj.vel.magnitude(), physics::movevelocity(&proj));
                dir.mul(1.f-amt).add(vec(proj.dest).sub(proj.o).normalize().mul(amt)).normalize();
                if(!dir.iszero()) (proj.vel = dir).mul(mag);
            }
        }
        if(proj.weight != 0.f) proj.vel.z -= physics::gravityvel(&proj)*secs;
        return moveproj(proj, secs);
    }

    bool moveframe(projent &proj)
    {
        if(((proj.lifetime -= physics::physframetime) <= 0 && proj.lifemillis) || (!proj.stuck && !move(proj, physics::physframetime)))
        {
            if(proj.lifetime < 0) proj.lifetime = 0;
            return false;
        }
        return true;
    }

    bool move(projent &proj)
    {
        if(physics::physsteps <= 0)
        {
            physics::interppos(&proj);
            return true;
        }

        bool alive = true;
        proj.o = proj.newpos;
        proj.o.z += proj.height;
        loopi(physics::physsteps-1) if(!(alive = moveframe(proj))) break;
        proj.deltapos = proj.o;
        if(alive) alive = moveframe(proj);
        proj.newpos = proj.o;
        proj.deltapos.sub(proj.newpos);
        proj.newpos.z -= proj.height;
        if(alive) physics::interppos(&proj);
        return alive;
    }

    bool raymove(projent &proj)
    {
        if((proj.lifetime -= curtime) <= 0 && proj.lifemillis)
        {
            if(proj.lifetime < 0) proj.lifetime = 0;
            return false;
        }
        float scale = W2(proj.weap, trace, WS(proj.flags));
        if(proj.owner) scale *= 1.f/proj.owner->curscale;
        vec ray = vec(proj.to).sub(proj.from).mul(scale);
        float maxdist = ray.magnitude();
        if(maxdist <= 0) return 1; // not moving anywhere, so assume still alive since it was already alive
        ray.mul(1/maxdist);
        float dist = tracecollide(&proj, proj.from, ray, maxdist, RAY_CLIPMAT|RAY_ALPHAPOLY, proj.projcollide&COLLIDE_DYNENT);
        if(dist >= 0)
        {
            vec dir = vec(proj.to).sub(proj.from).normalize();
            proj.o = vec(proj.from).add(vec(dir).mul(dist));
            switch(impact(proj, dir, hitplayer, hitflags, hitsurface))
            {
                case 1: case 2: return true;
                case 0: default: return false;
            }
        }
        proj.o = proj.to;
        return true;
    }

    struct canrem
    {
        projent *p;
        float dist;

        canrem(projent *p, float dist = 0) : p(p), dist(dist) {}
        ~canrem() {}

        static bool cmsort(const canrem *a, const canrem *b)
        {
            if(a->dist > b->dist) return true;
            if(a->dist < b->dist) return false;
            if(a->p->addtime < b->p->addtime) return true;
            if(a->p->addtime > b->p->addtime) return false;
            return false;
        }
    };


    void update()
    {
        vector<canrem *> canremove;
        loopvrev(projs) if(projs[i]->projtype == PRJ_DEBRIS || projs[i]->projtype == PRJ_GIBS || projs[i]->projtype == PRJ_EJECT)
            canremove.add(new canrem(projs[i], camera1->o.dist(projs[i]->o)));
        int count = canremove.length()-maxprojectiles;
        if(count > 0)
        {
            canremove.sort(canrem::cmsort);
            loopi(count)
            {
                canremove[i]->p->state = CS_DEAD;
                canremove[i]->p->escaped = true;
            }
        }
        canremove.deletecontents();

        loopv(projs)
        {
            projent &proj = *projs[i];
            if(proj.projtype == PRJ_SHOT && WF(WK(proj.flags), proj.weap, radial, WS(proj.flags)))
            {
                proj.hit = NULL;
                proj.hitflags = HITFLAG_NONE;
            }
            hits.setsize(0);
            if((proj.projtype != PRJ_SHOT || proj.owner) && proj.state != CS_DEAD)
            {
                if(proj.projtype == PRJ_ENT && entities::ents.inrange(proj.id) && !entities::simpleitems) // in case spawnweapon changes
                    proj.mdl = entities::entmdlname(entities::ents[proj.id]->type, entities::ents[proj.id]->attrs);
                if(proj.waittime > 0)
                {
                    if((proj.waittime -= curtime) <= 0)
                    {
                        proj.waittime = 0;
                        init(proj, true);
                    }
                    else continue;
                }
                iter(proj);
                if(proj.projtype == PRJ_SHOT || proj.projtype == PRJ_ENT || proj.projtype == PRJ_AFFINITY)
                {
                    if(proj.projtype == PRJ_SHOT && !WK(proj.flags) && weaptype[proj.weap].traced ? !raymove(proj) : !move(proj)) switch(proj.projtype)
                    {
                        case PRJ_ENT: case PRJ_AFFINITY:
                        {
                            if(!proj.beenused)
                            {
                                proj.beenused = 1;
                                proj.lifetime = min(proj.lifetime, proj.fadetime);
                            }
                            if(proj.lifetime > 0) break;
                        }
                        default: proj.state = CS_DEAD; proj.escaped = true; break;
                    }
                }
                else for(int rtime = curtime; proj.state != CS_DEAD && rtime > 0;)
                {
                    int qtime = min(rtime, 30);
                    rtime -= qtime;

                    if(((proj.lifetime -= qtime) <= 0 && proj.lifemillis) || (!proj.stuck && !move(proj, qtime)))
                    {
                        if(proj.lifetime < 0) proj.lifetime = 0;
                        proj.state = CS_DEAD;
                        proj.escaped = true;
                        break;
                    }
                }
                effect(proj);
            }
            else
            {
                proj.state = CS_DEAD;
                proj.escaped = true;
            }
            if(proj.local && proj.owner && proj.projtype == PRJ_SHOT)
            {
                float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                if(!WK(proj.flags) && weaptype[proj.weap].traced) proj.o = proj.to;
                if(!proj.limited && proj.state != CS_DEAD)
                {
                    if(!(proj.projcollide&DRILL_PLAYER)) proj.hit = NULL;
                    bool radial = WF(WK(proj.flags), proj.weap, radial, WS(proj.flags)) && expl > 0 && (!proj.lastradial || lastmillis-proj.lastradial >= WF(WK(proj.flags), proj.weap, radial, WS(proj.flags)));
                    int proxim = proj.stuck && !proj.beenused ? WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) : 0;
                    if(radial || proxim)
                    {
                        float dist = WX(WK(proj.flags), proj.weap, proxdist, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        int stucktime = lastmillis-proj.stuck, stuckdelay = WF(WK(proj.flags), proj.weap, proxdelay, WS(proj.flags));
                        if(stuckdelay && stuckdelay > stucktime) dist *= stucktime/float(stuckdelay);
                        if(dist <= 1e-6f) proxim = 0;
                        int numdyns = game::numdynents();
                        gameent *oldstick = proj.stick;
                        proj.stick = NULL;
                        loopj(numdyns)
                        {
                            dynent *f = game::iterdynents(j);
                            if(!f || f->state != CS_ALIVE || !physics::issolid(f, &proj, true, false)) continue;
                            if(radial && radialeffect(f, proj, HIT_BURN, expl)) proj.lastradial = lastmillis;
                            if(proxim == 1 && !proj.beenused && f != oldstick && f->center().dist(proj.o) <= dist)
                            {
                                proj.beenused = 1;
                                proj.lifetime = min(proj.lifetime, WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags)));
                            }
                        }
                        if(proxim == 2 && !proj.beenused)
                        {
                            vec from = vec(proj.o).add(vec(proj.norm).mul(proj.radius*2)), to = vec(from).add(vec(proj.norm).mul(dist)), dir = vec(to).sub(from);
                            float mag = dir.magnitude();
                            if(mag > 1e-6f)
                            {
                                dir.div(mag);
                                float blocked = tracecollide(&proj, from, dir, mag, RAY_CLIPMAT|RAY_ALPHAPOLY, true);
                                if(blocked >= 0 && hitplayer && hitplayer->state == CS_ALIVE && physics::issolid(hitplayer, &proj, true, false))
                                {
                                    proj.beenused = 1;
                                    proj.lifetime = min(proj.lifetime, WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags)));
                                }
                            }
                        }
                        proj.stick = oldstick;
                    }
                }
                if(proj.state == CS_DEAD)
                {
                    if(!(proj.projcollide&DRILL_PLAYER)) proj.hit = NULL;
                    if(!proj.limited && expl > 0)
                    {
                        int numdyns = game::numdynents(true);
                        gameent *oldstick = proj.stick;
                        proj.stick = NULL;
                        loopj(numdyns)
                        {
                            dynent *f = game::iterdynents(j, true);
                            if(!f || f->state != CS_ALIVE || !physics::issolid(f, &proj, false, false)) continue;
                            radialeffect(f, proj, HIT_EXPLODE, expl);
                        }
                        proj.stick = oldstick;
                    }
                }
                if(!hits.empty())
                    client::addmsg(N_DESTROY, "ri7iv", proj.owner->clientnum, lastmillis-game::maptime, proj.weap, proj.flags, WK(proj.flags) ? -proj.id : proj.id,
                            int(expl*DNF), int(proj.curscale*DNF), hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                //if(proj.weap == W_MELEE && WS(proj.flags)) proj.target = NULL;
            }
            if(proj.state == CS_DEAD)
            {
                destroy(proj);
                delete &proj;
                projs.removeunordered(i--);
            }
        }
    }

    void fadeproj(projent &proj, float &trans, float &size)
    {
        if(proj.projtype == PRJ_SHOT && proj.owner && physics::isghost(proj.owner, game::focus, true)) trans *= 0.5f;
        if(proj.fadetime && proj.lifemillis)
        {
            int interval = min(proj.lifemillis, proj.fadetime);
            if(proj.lifetime < interval)
            {
                float amt = float(proj.lifetime)/float(interval);
                size *= amt;
                trans *= amt;
            }
            else if(proj.projtype != PRJ_EJECT && proj.lifemillis > interval)
            {
                interval = min(proj.lifemillis-interval, proj.fadetime);
                if(proj.lifemillis-proj.lifetime < interval)
                {
                    float amt = float(proj.lifemillis-proj.lifetime)/float(interval);
                    size *= amt;
                    trans *= amt;
                }
            }
        }
    }

    void render()
    {
        loopv(projs) if(projs[i]->ready(false) && projs[i]->projtype != PRJ_AFFINITY)
        {
            projent &proj = *projs[i];
            if((proj.projtype == PRJ_ENT && !entities::ents.inrange(proj.id)) || !projs[i]->mdl || !*projs[i]->mdl) continue;
            float trans = 1, size = projs[i]->curscale, yaw = proj.yaw, pitch = proj.pitch, roll = proj.roll;
            int flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_CULL_DIST;
            switch(proj.projtype)
            {
                case PRJ_DEBRIS:
                {
                    if(shadowdebris) flags |= MDL_DYNSHADOW;
                    size *= proj.lifesize;
                    fadeproj(proj, trans, size);
                    if(!proj.limited)
                    {
                        flags |= MDL_LIGHTFX;
                        vec burncol = !proj.id && isweap(proj.weap) ? FWCOL(P, explcol, proj) : game::rescolour(&proj, PULSE_BURN);
                        burncol.lerp(proj.light.effect, clamp((proj.lifespan - 0.3f)/0.5f, 0.0f, 1.0f));
                        proj.light.effect.max(burncol);
                    }
                    break;
                }
                case PRJ_GIBS: case PRJ_VANITY:
                {
                    if(proj.projtype == PRJ_GIBS ? shadowgibs : shadowvanity) flags |= MDL_DYNSHADOW;
                    size *= proj.lifesize;
                    flags |= MDL_LIGHT_FAST;
                    fadeproj(proj, trans, size);
                    if(proj.projtype == PRJ_VANITY && proj.owner)
                    {
                        loopi(3) proj.light.material[i] = proj.owner->light.material[i];
                        proj.light.effect = proj.owner->light.effect;
                    }
                    break;
                }
                case PRJ_EJECT:
                {
                    if(shadoweject) flags |= MDL_DYNSHADOW;
                    size *= proj.lifesize;
                    flags |= MDL_LIGHT_FAST;
                    fadeproj(proj, trans, size);
                    yaw += 90;
                    break;
                }
                case PRJ_SHOT:
                {
                    if(shadowents) flags |= MDL_DYNSHADOW;
                    trans *= fadeweap(proj);
                    if(WF(WK(proj.flags), proj.weap, partcol, WS(proj.flags)))
                    {
                        flags |= MDL_LIGHTFX;
                        proj.light.material[0] = bvec::fromcolor(FWCOL(P, partcol, proj));
                        if(WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) && (!proj.stuck || proj.lifetime%500 >= 300))
                            proj.light.material[0] = bvec(0, 0, 0);
                    }
                    yaw += 90;
                    break;
                }
                case PRJ_ENT:
                {
                    if(entities::simpleitems) continue;
                    if(shadowents) flags |= MDL_DYNSHADOW;
                    fadeproj(proj, trans, size);
                    if(entities::ents.inrange(proj.id))
                    {
                        gameentity &e = *(gameentity *)entities::ents[proj.id];
                        if(e.type == WEAPON)
                        {
                            flags |= MDL_LIGHTFX;
                            int col = W(w_attr(game::gamemode, game::mutators, e.type, e.attrs[0], m_weapon(game::focus->actortype, game::gamemode, game::mutators)), colour), interval = lastmillis%1000;
                            proj.light.effect = vec::hexcolor(col).mul(interval >= 500 ? (1000-interval)/500.f : interval/500.f);
                        }
                    }
                    break;
                }
                default: break;
            }
            rendermodel(&proj.light, proj.mdl, ANIM_MAPMODEL|ANIM_LOOP, proj.o, yaw, pitch, roll, flags, &proj, NULL, proj.spawntime, 0, trans, size);
        }
    }

    void adddynlights()
    {
        loopv(projs) if(projs[i]->ready() && projs[i]->projtype == PRJ_SHOT && !projs[i]->limited && !projs[i]->child)
        {
            projent &proj = *projs[i];
            float trans = fadeweap(proj);
            int type = WF(WK(proj.flags), proj.weap, parttype, WS(proj.flags));
            if(trans > 0) switch(type)
            {
                case W_SWORD: adddynlight(proj.o, 24*trans, FWCOL(P, partcol, proj)); break;
                case W_PISTOL: case W_SHOTGUN: case W_SMG: case W_ZAPPER: case W_RIFLE:
                {
                    float expl = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                    if(type != W_ZAPPER || expl <= 0)
                    {
                        float size = clamp(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*(1.f-proj.lifespan)*proj.curscale, proj.curscale, min(16.f, min(min(WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags)), proj.movement), proj.o.dist(proj.from))));
                        adddynlight(proj.o, 1.25f*size*trans, FWCOL(P, partcol, proj));
                        break;
                    }
                }
                case W_FLAMER: case W_PLASMA: case W_GRENADE: case W_ROCKET:
                {
                    float size = WX(WK(proj.flags), proj.weap, explode, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                    if(size <= 0) size = WF(WK(proj.flags), proj.weap, partlen, WS(proj.flags))*proj.lifesize*proj.curscale*0.5f;
                    adddynlight(proj.o, 1.5f*size*trans, FWCOL(P, partcol, proj));
                    break;
                }
                default: break;
            }
        }
    }
}
