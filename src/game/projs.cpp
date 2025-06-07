#include "game.h"
namespace projs
{
    #define FWCOL(n,c,p) ((p).flags&HIT_FLAK ? W##n##COL(&p, (p).weap, flak##c, WS((p).flags)) : W##n##COL(&p, (p).weap, c, WS((p).flags)))

    vector<hitmsg> hits;
    vector<projent *> projs, collideprojs, junkprojs, typeprojs[PROJ_MAX];

    struct toolent
    {
        int ent, type;
        float radius;
        vec o;

        vec pos() const { return entities::ents.inrange(ent) ? ((gameentity *)entities::ents[ent])->pos() : o; }
    };
    vector<toolent> toolents;

    VAR(IDF_PERSIST, maxprojectiles, 1, 512, VAR_MAX);

    VAR(IDF_PERSIST, ejectspin, 0, 1, 1);
    VAR(IDF_PERSIST, ejecthint, 0, 0, 1);

    VAR(IDF_PERSIST, projburntime, 0, 5500, VAR_MAX);
    VAR(IDF_PERSIST, projburndelay, 0, 1000, VAR_MAX);

    VAR(0, projdebug, 0, 0, 1);

    VAR(IDF_PERSIST, muzzleflash, 0, 3, 3); // 0 = off, 1 = other players, 2 = focused players, 3 = all
    FVAR(IDF_PERSIST, muzzleblend, 0, 1, 1);
    FVAR(IDF_PERSIST, muzzlefade, 0, 0.5f, 1);

    enum
    {
        PROJ_FX_HIT = 0,
        PROJ_FX_BOUNCE,
        PROJ_FX_DESTROY,
        PROJ_FX_LIFE,
        PROJ_FX_TRIPWIRE,

        PROJ_NUM_FX_SUBTYPES
    };

    static fx::FxHandle projfx[FX_P_TYPES * PROJ_NUM_FX_SUBTYPES];
    #define PROJFXINDEX(type, subtype) (((type) * PROJ_NUM_FX_SUBTYPES) + (subtype))

    void mapprojfx()
    {
        static const char *typeprefixes[FX_P_TYPES] =
        {
            "FX_P_BULLET_", "FX_P_PELLET_", "FX_P_ACID_", "FX_P_FLAK_", "FX_P_SHRAPNEL_", "FX_P_BLOB_", "FX_P_FLAME_",
            "FX_P_AIRBLAST_", "FX_P_PLASMA_", "FX_P_VORTEX_", "FX_P_ENERGY_", "FX_P_BEAM_",
            "FX_P_GRENADE_", "FX_P_MINE_", "FX_P_ROCKET_", "FX_P_CASING_", "FX_P_GIB_",
            "FX_P_DEBRIS_"
        };

        static const char *subtypes[PROJ_NUM_FX_SUBTYPES] =
        {
            "HIT", "BOUNCE", "DESTROY", "LIFE", "TRIPWIRE"
        };

        loopi(FX_P_TYPES) loopj(PROJ_NUM_FX_SUBTYPES)
        {
            string slotname;
            formatstring(slotname, "%s%s", typeprefixes[i], subtypes[j]);
            projfx[PROJFXINDEX(i, j)] = fx::getfxhandle(slotname);
        }
    }

    static inline fx::FxHandle getprojfx(int fxtype, int subtype)
    {
        return fxtype >= 0 && fxtype < FX_P_TYPES &&
            subtype >= 0 && subtype < PROJ_NUM_FX_SUBTYPES ?
                projfx[PROJFXINDEX(fxtype, subtype)] : fx::FxHandle();
    }

    static void doprojfx(projent &proj, int subtype)
    {
        fx::FxHandle fxhandle = getprojfx(proj.fxtype, subtype);

        // Update the hook
        if(proj.effect)
        {
            switch(subtype)
            {
                case PROJ_FX_DESTROY:
                    proj.effect->setparam(P_FX_LIFETIME_PARAM, 1.0f);
                    break;
            }
        }

        if(!fxhandle.isvalid()) return;

        bvec color(255, 255, 255);
        float scale = 1.0f, blend = 1.0f;
        fx::emitter **hook = NULL;
        vec from = proj.o, to = vec(proj.o).add(proj.norm);

        if(proj.projtype == PROJ_SHOT && proj.weap >= 0)
        {
            color = bvec(FWCOL(H, fxcol, proj));
            scale = WF(WK(proj.flags), proj.weap, fxscale, WS(proj.flags))*proj.curscale*proj.lifesize;
            blend = WF(WK(proj.flags), proj.weap, fxblend, WS(proj.flags))*proj.curscale;
        }

        switch(subtype)
        {
            case PROJ_FX_LIFE:
            {
                hook = &proj.effect;
                if(proj.projtype != PROJ_SHOT) break;
                from = proj.trailpos;
                to = proj.next ? proj.next->trailpos : proj.o;
                break;
            }
            case PROJ_FX_TRIPWIRE:
            {
                if(proj.projtype != PROJ_SHOT) break;
                to = proj.trailpos;
            }
            default: break;
        }

        physent *pe = NULL;

        // allow parttrack for hit-scan projectiles
        if(subtype == PROJ_FX_LIFE && proj.projcollide&COLLIDE_SCAN)
            pe = proj.owner;

        fx::emitter &e = fx::createfx(fxhandle, hook)
            .setfrom(from)
            .setto(to)
            .setblend(blend)
            .setscale(scale)
            .setcolor(color)
            .setentity(pe);

        float param;

        switch(subtype)
        {
            case PROJ_FX_LIFE:
                param = clamp(proj.lifespan, 0.0f, 1.0f);
                e.setparam(P_FX_LIFETIME_PARAM, param);
                break;

            case PROJ_FX_BOUNCE:
                param = clamp(vec(proj.vel).add(proj.falling).magnitude()*proj.curscale*0.005f, 0.0f, 1.0f);
                e.setparam(P_FX_BOUNCE_VEL_PARAM, param);
                break;

            case PROJ_FX_HIT:
            {
                bool living = false;
                if(gameent::is(proj.hit))
                {
                    gameent *d = (gameent *)proj.hit;
                    living = A(d->actortype, abilities)&(1<<A_A_LIVING);
                }
                e.setparam(P_FX_HIT_ALIVE_PARAM, living ? 1.0f : 0.0f);
            }
        }
    }

    static void testproj(int type, int weap)
    {
        if(connected(false, false)) return;

        vec from = camera1->o;
        vec to = from;
        vec dir;

        vecfromyawpitch(cursoryaw, cursorpitch, 0, 1, dir);
        to.add(dir.mul(32.0f));

        create(from, to, true, NULL, type, weap, 0, 10000, 10000, 0, 50.0f);
    }

    ICOMMAND(0, testproj, "ii", (int *type, int *weap), testproj(*type, *weap));

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

        float skew = clamp(scale, 0.f, 1.f) * damagescale * A(v->actortype, damage);

        if(flags&HIT_WHIPLASH) skew *= WF(WK(flags), weap, damagewhiplash, WS(flags));
        else if(flags&HIT_HEAD || flags&HIT_FULL) skew *= WF(WK(flags), weap, damagehead, WS(flags));
        else if(flags&HIT_TORSO) skew *= WF(WK(flags), weap, damagetorso, WS(flags));
        else if(flags&HIT_LIMB) skew *= WF(WK(flags), weap, damagelimb, WS(flags));
        else return 0;

        if(radial > 0) skew *= clamp(1.f-dist/size, FVAR_NONZERO, 1.f);
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
        if(isweap(proj.weap) && !(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH) && flags&HIT_PROJ && proj.weight != 0 && d->weight != 0)
            vel = vec(proj.vel).add(proj.falling).mul(proj.weight).div(d->weight);
        if(proj.owner && proj.local)
        {
            int hflags = proj.flags|flags;
            float size = hflags&HIT_WAVE ? radial*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) : radial;
            int damage = calcdamage(proj.owner, d, proj.weap, hflags, radial, size, dist, scale);
            if(damage) game::hiteffect(proj.weap, hflags, proj.fromweap, proj.fromflags, damage, d, proj.owner, dir, vel, dist, false);
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

    void projpush(projent *p, float dist = 0)
    {
        if(p->projtype != PROJ_SHOT || !p->owner) return;
        if(p->local) p->state = CS_DEAD;
        else
        {
            hitmsg &h = hits.add();
            h.flags = HIT_PROJ|HIT_FULL;
            h.proj = p->id;
            h.target = p->owner->clientnum;
            h.dist = int(dist*DNF);
            h.dir = h.vel = ivec(0, 0, 0);
        }
    }

    bool radialeffect(dynent *d, projent &proj, int flags, float radius)
    {
        bool push = WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) != 0, radiated = false;

        #define radialpush(rs,xx,yx,yy,yz1,yz2,zz) \
        { \
            if(!proj.o.rejectxyz(xx, yx, yy, yz1, yz2)) zz = 0; \
            else if(!proj.o.reject(xx, rs  +max(yx, yy))) \
            { \
                vec bottom(xx), top(xx); \
                bottom.z -= yz1; \
                top.z += yz2; \
                zz = closestpointcylinder(proj.o, bottom, top, max(yx, yy)).dist(proj.o); \
            } \
        }

        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            float maxdist = push ? radius*WF(WK(proj.flags), proj.weap, wavepush, WS(proj.flags)) : radius;
            if(actors[e->actortype].collidezones)
            {
                float rdist[3] = { -1, -1, -1 };
                if(actors[e->actortype].collidezones&CLZ_LIMBS)
                    radialpush(maxdist, e->limbstag(), e->limbsbox().x, e->limbsbox().y, e->limbsbox().z, e->limbsbox().z, rdist[0]);

                if(actors[e->actortype].collidezones&CLZ_TORSO)
                    radialpush(maxdist, e->torsotag(), e->torsobox().x, e->torsobox().y, e->torsobox().z, e->torsobox().z, rdist[1]);

                if(actors[e->actortype].collidezones&CLZ_HEAD)
                    radialpush(maxdist, e->headtag(), e->headbox().x, e->headbox().y, e->headbox().z, e->headbox().z, rdist[2]);

                int closest = -1;
                loopi(3) if(rdist[i] >= 0 && (closest < 0 || rdist[i] <= rdist[closest])) closest = i;

                loopi(3) if(rdist[i] >= 0)
                {
                    int flag = 0;
                    switch(i)
                    {
                        case 2: flag = closest != i || rdist[i] > WF(WK(proj.flags), proj.weap, headmin, WS(proj.flags)) ? HIT_WHIPLASH : HIT_HEAD; break;
                        case 1: flag = HIT_TORSO; break;
                        case 0: default: flag = HIT_LIMB; break;
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
                radialpush(maxdist, e->o, e->xradius, e->yradius, e->height, e->aboveeye, dist);

                if(dist >= 0)
                {
                    if(dist <= radius)
                    {
                        hitpush(e, proj, HIT_FULL|flags, radius, dist, proj.curscale);
                        radiated = true;
                    }
                    else if(push && dist <= maxdist)
                    {
                        hitpush(e, proj, HIT_FULL|HIT_WAVE, maxdist, dist, proj.curscale);
                        radiated = true;
                    }
                }
            }
        }
        else if(d->type == ENT_PROJ && flags&HIT_EXPLODE)
        {
            projent *e = (projent *)d;
            float dist = -1;
            radialpush(radius, e->o, e->xradius, e->yradius, e->height, e->aboveeye, dist);

            if(dist >= 0 && dist <= radius) projpush(e, dist);
        }

        return radiated;
    }


    bool hiteffect(projent &proj, physent *d, int flags, const vec &norm)
    {
        if(!d || !dynent::is(d)) return false;

        if(proj.projtype == PROJ_SHOT && physics::issolid(d, &proj))
        {
            bool drill = proj.projcollide&(d->type == ENT_PROJ ? DRILL_SHOTS : DRILL_PLAYER);
            proj.hit = d;
            proj.collidezones = flags;

            float expl = WX(WK(proj.flags), proj.weap, radial, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
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
                    if(proj.collidezones&CLZ_LIMBS) flags |= HIT_LIMB;
                    if(proj.collidezones&CLZ_TORSO) flags |= HIT_TORSO;
                    if(proj.collidezones&CLZ_HEAD) flags |= HIT_HEAD;
                    if(proj.collidezones&CLZ_FULL) flags |= HIT_FULL;
                    if(flags) hitpush((gameent *)d, proj, flags|HIT_PROJ, 0, proj.lifesize, proj.curscale);
                }
                else if(d->type == ENT_PROJ) projpush((projent *)d);
            }

            doprojfx(proj, PROJ_FX_HIT);

            return !drill;
        }

        return false;
    }

    void removeplayer(gameent *d)
    {
        loopv(projs)
        {
            if(projs[i]->target == d) projs[i]->target = NULL;

            if(projs[i]->stick == d)
            {
                projs[i]->stuck = 0;
                projs[i]->stick = NULL;
                projs[i]->lastbounce = lastmillis;
            }

            if(projs[i]->owner == d)
            {
                if(projs[i]->isjunk()) junkprojs.removeobj(projs[i]);
                typeprojs[projs[i]->projtype].removeobj(projs[i]);
                if(projs[i]->projtype == PROJ_SHOT)
                {
                    if(projs[i]->projcollide&COLLIDE_PROJ)
                    {
                        collideprojs.removeobj(projs[i]);
                        cleardynentcache();
                    }

                    delete projs[i];
                    projs.removeunordered(i--);
                }
                else projs[i]->owner = NULL;
            }
        }
    }

    void destruct(gameent *d, int targ, int id, bool all)
    {
        loopv(projs)
        {
            projent &proj = *projs[i];
            if(proj.owner != d || proj.projtype != targ || proj.id != id) continue;
            proj.state = CS_DEAD;
            if(!all) break;
        }
    }

    void updatenormal(projent &proj)
    {
        vectoyawpitch(proj.norm, proj.yaw, proj.pitch);
        proj.pitch -= 90;
        fixfullrange(proj.yaw, proj.pitch, proj.roll, true);
        proj.resetinterp();
    }

    bool updatesticky(projent &proj, bool init = false)
    {
        if(!proj.stuck)
        {
            if(proj.stick && proj.stick->isalive()) // maybe this will fix it
                proj.stuck = max(proj.lastbounce, lastmillis);
            else return false;
        }

        if(init)
        {
            int lifetime = WF(WK(proj.flags), proj.weap, timestick, WS(proj.flags));
            if(lifetime > 0)
            {
                proj.lifetime = lifetime;
                proj.lifemillis = lifetime;
            }
            proj.vel = proj.falling = vec(0, 0, 0);
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

        return proj.stuck != 0;
    }

    void updateattract(gameent *d, gameent *v, int weap, int flags)
    {
        if(!(WF(WK(flags), weap, collide, WS(flags))&ATTRACT_PLAYER)) return;

        loopv(projs)
        {
            projent &proj = *projs[i];

            if(proj.projtype != PROJ_SHOT || proj.weap != weap || !proj.stuck || !proj.lifetime) continue;
            if((proj.target && proj.vel.magnitude() > WF(WK(proj.flags), proj.weap, attractminvel, WS(proj.flags))) || proj.owner != v) continue;
            if(!(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&ATTRACT_PLAYER)) continue;

            vec pos = d->center(), ray = vec(pos).sub(proj.o);
            float dist = ray.magnitude();
            if(dist > WF(WK(proj.flags), proj.weap, attractdist, WS(proj.flags))) continue;
            ray.safenormalize();

            // float blocked = tracecollide(&proj, pos, ray, dist, RAY_CLIPMAT|RAY_ALPHAPOLY, true);
            // if(blocked >= 0) continue;

            proj.target = d;
            proj.stuck = 0;
            proj.stick = NULL;
            proj.lastbounce = lastmillis;

            proj.projcollide &= ~(IMPACT_GEOM|STICK_GEOM|BOUNCE_PLAYER|STICK_PLAYER);
            proj.projcollide |= BOUNCE_GEOM|IMPACT_PLAYER;

            proj.dest = pos;

            float speed = proj.speed * WF(WK(flags), weap, attractspeed, WS(flags));
            if(WF(WK(proj.flags), proj.weap, attractscaled, WS(proj.flags)) > 0 && WF(WK(proj.flags), proj.weap, attractdist, WS(proj.flags)) > 0)
                speed *= 1.f - clamp(dist / WF(WK(proj.flags), proj.weap, attractdist, WS(proj.flags)), 0.f, 1.f) * WF(WK(proj.flags), proj.weap, attractscaled, WS(proj.flags));

            proj.o.add(vec(ray).mul(proj.radius * 2));
            proj.vel = vec(ray).mul(speed);

            proj.resetinterp();
        }
    }

    void stick(projent &proj, const vec &dir, gameent *d = NULL)
    {
        if(proj.projtype != PROJ_SHOT || (proj.owner && proj.local))
        {
            proj.stick = d;
            proj.sticknrm = proj.norm;
            proj.stuck = proj.lastbounce = lastmillis ? lastmillis : 1;

            doprojfx(proj, PROJ_FX_BOUNCE);

            #if 0
            vec fwd = dir.iszero() ? vec(proj.vel).add(proj.falling).safenormalize() : dir;
            if(!fwd.iszero()) loopi(20)
            {
                proj.o.sub(fwd);
                if(!collide(&proj, vec(0, 0, 0), 0.f, proj.projcollide&COLLIDE_DYNENT, true, GUARDRADIUS) && (proj.stick ? collideplayer != proj.stick : !collideplayer))
                    break;
            }
            #endif

            proj.stickpos = proj.o;

            if(proj.stick)
            {
                proj.stickpos.sub(proj.stick->center());
                proj.stickpos.rotate_around_z(-proj.stick->yaw*RAD);
                proj.sticknrm.rotate_around_z(-proj.stick->yaw*RAD);
            }

            if(updatesticky(proj, true) && proj.projtype == PROJ_SHOT)
                client::addmsg(N_STICKY, "ri9i3",
                    proj.owner->clientnum, lastmillis-game::maptime, proj.weap, proj.flags, WK(proj.flags) ? -proj.id : proj.id, proj.stick ? proj.stick->clientnum : -1,
                        int(proj.sticknrm.x*DNF), int(proj.sticknrm.y*DNF), int(proj.sticknrm.z*DNF), int(proj.stickpos.x*DMF), int(proj.stickpos.y*DMF), int(proj.stickpos.z*DMF));
        }
    }

    void sticky(gameent *d, int id, vec &norm, vec &pos, gameent *f)
    {
        loopv(projs) if(projs[i]->owner == d && projs[i]->projtype == PROJ_SHOT && projs[i]->id == id)
        {
            projs[i]->stuck = projs[i]->lastbounce = lastmillis ? lastmillis : 1;
            projs[i]->sticknrm = norm;
            projs[i]->stickpos = pos;
            if(!(projs[i]->stick = f)) projs[i]->o = pos;
            updatesticky(*projs[i], true);
            break;
        }
    }

    int checkitems(projent &proj, const vec &o, const vec &dir, float maxdist)
    {
        int ret = 0;
        loopv(toolents)
        {
            toolent &t = toolents[i];
            if(!(proj.interacts&t.type) || !entities::ents.inrange(t.ent) || !entities::isallowed(t.ent)) continue;
            float dist = 1e16f;
            if(!raysphereintersect(t.pos(), t.radius, o, dir, dist) || dist > maxdist) continue;
            entities::execitem(t.ent, -1, &proj, dist, false);
            ret++;
        }
        return ret;
    }

    void reset()
    {
        collideprojs.setsize(0);
        junkprojs.setsize(0);
        loopi(PROJ_MAX) typeprojs[i].setsize(0);
        cleardynentcache();
        projs.deletecontents();
        projs.shrink(0);
        toolents.shrink(0);
        loopv(entities::ents) switch(entities::ents[i]->type)
        {
            case TELEPORT: case PUSHER:
            {
                bool teleport = entities::ents[i]->type == TELEPORT;
                toolent &t = toolents.add();
                t.ent = i;
                t.type = teleport ? 1 : 2;
                t.radius = entities::ents[i]->attrs[3] > 0 ? entities::ents[i]->attrs[3] : enttype[entities::ents[i]->type].radius;
                if(teleport) t.radius *= 1.5f;
                t.o = entities::ents[i]->o;
                break;
            }
            default: break;
        }
    }

    void preload()
    {
        loopi(W_ALL) loopj(2)
        {
            loopk(weaptype[i].proj[j].count) preloadmodel(weaptype[i].proj[j].name[k]);
            loopk(weaptype[i].eprj[j].count) preloadmodel(weaptype[i].eprj[j].name[k]);
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
        loopi(PLAYERPARTS) preloadmodel(playerparts[i].filename);
        loopi(JANITORPARTS) preloadmodel(janitorparts[i].filename);
    }

    void reflect(projent &proj, vec &pos)
    {
        vec dir = vec(proj.vel).add(proj.falling);
        float mag = dir.magnitude(), elasticity = mag > 0.01f ? proj.elasticity : 1.f;

        if(elasticity <= 0.f)
        {
            proj.vel = proj.falling = vec(0, 0, 0);
            return;
        }

        mag *= elasticity; // conservation of energy
        dir.safenormalize().reflect(pos);
        mag = max(mag, proj.speedmin);
        if(proj.speedmax > 0) mag = min(mag, proj.speedmax);
        proj.vel = vec(dir).mul(mag);
        proj.falling = vec(0, 0, 0);

        #define repel(_x,_r,_z) \
        { \
            if(overlapsbox(proj.o, _r, _r, _x, _r, _r)) \
            { \
                proj.vel.add(vec(proj.o).sub(_x).safenormalize().mul(_z)); \
                break; \
            } \
        }
        switch(proj.projtype)
        {
            case PROJ_ENTITY:
            {
                if(itemrepulsion <= 0 || lastmillis - proj.spawntime < itemrepeldelay) break;
                if(!entities::ents.inrange(proj.id) || enttype[entities::ents[proj.id]->type].usetype != EU_ITEM) break;

                loopv(projs)
                {
                    projent *p = projs[i];
                    if(p->projtype != PROJ_ENTITY || p == &proj) continue;
                    if(!entities::ents.inrange(p->id) || enttype[entities::ents[p->id]->type].usetype != EU_ITEM) continue;

                    repel(p->o, itemrepulsion, itemrepelspeed);
                }

                loopusei(EU_ITEM)
                {
                    if(!entities::ents.inrange(i)) continue;
                    gameentity &e = *(gameentity *)entities::ents[i];
                    if(enttype[e.type].usetype != EU_ITEM) continue;

                    repel(e.pos(), itemrepulsion, itemrepelspeed);
                }
                break;
            }
            case PROJ_AFFINITY:
            {
                if(!m_capture(game::gamemode) || capturerepulsion <= 0 || lastmillis - proj.spawntime < capturerepeldelay) break;

                loopv(projs)
                {
                    projent *p = projs[i];
                    if(p->projtype != PROJ_AFFINITY || p == &proj) continue;

                    repel(projs[i]->o, capturerepulsion, capturerepelspeed);
                }
                break;
            }
        }
    }

    void bounce(projent &proj, bool ricochet)
    {
        if(!proj.limited && (proj.movement > 0 || (proj.projtype == PROJ_SHOT && WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH)) && (!proj.lastbounce || lastmillis-proj.lastbounce >= 250))
            doprojfx(proj, PROJ_FX_BOUNCE);
    }

    float fadeweap(projent &proj)
    {
        float trans = WF(WK(proj.flags), proj.weap, blend, WS(proj.flags));
        if(WF(WK(proj.flags), proj.weap, fadein, WS(proj.flags)) != 0)
        {
            int millis = proj.lifemillis-proj.lifetime, len = min(WF(WK(proj.flags), proj.weap, fadein, WS(proj.flags)), proj.lifemillis);
            if(len > 0 && millis < len) trans *= millis/float(len);
        }
        if(WF(WK(proj.flags), proj.weap, fadeout, WS(proj.flags)) != 0)
        {
            int len = min(WF(WK(proj.flags), proj.weap, fadeout, WS(proj.flags)), proj.lifemillis);
            if(len > 0 && proj.lifetime < len) trans *= proj.lifetime/float(len);
        }
        if(WF(WK(proj.flags), proj.weap, fade, WS(proj.flags))&(proj.owner != game::focus ? 2 : 1) && WF(WK(proj.flags), proj.weap, fadeat, WS(proj.flags)) > 0)
            trans *= camera1->o.distrange(proj.o, WF(WK(proj.flags), proj.weap, fadeat, WS(proj.flags)), WF(WK(proj.flags), proj.weap, fadecut, WS(proj.flags)));
        if(proj.stuck && isweap(proj.weap) && WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags)) != 0)
        {
            int millis = lastmillis-proj.stuck;
            if(WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags)) != 0 && millis < WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags)))
                trans *= 1.f-(WF(WK(proj.flags), proj.weap, visfade, WS(proj.flags))*millis/float(WF(WK(proj.flags), proj.weap, vistime, WS(proj.flags))));
            else trans *= 1.f-WF(WK(proj.flags), proj.weap, visfade, WS(proj.flags));
            if(proj.beenused && WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags)) != 0 && proj.lifetime < WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags)))
                trans += (1.f-trans)*((WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags))-proj.lifetime)/float(WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags))));
        }
        return trans;
    }

    bool spherecheck(projent &proj, bool rev = false)
    {
        if(!insideworld(proj.o)) return false;
        if(proj.stuck) return false;
        vec dir = vec(proj.vel).add(proj.falling).safenormalize();
        if(collide(&proj, dir, FVAR_NONZERO, false) || collideinside)
        {
            vec orig = proj.o;
            if(!proj.lastgood.iszero())
            {
                proj.o = proj.lastgood;
                if(!collide(&proj, dir, FVAR_NONZERO, false) && !collideinside)
                {
                    if(rev)
                    {
                        float mag = max(max(vec(proj.vel).add(proj.falling).magnitude()*proj.elasticity, proj.speedmin), 1.f);
                        if(proj.speedmax > 0) mag = min(mag, proj.speedmax);
                        proj.vel = vec(proj.o).sub(orig).safenormalize().mul(mag);
                        proj.falling = vec(0, 0, 0);
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
                if(!collide(&proj, dir, FVAR_NONZERO, false) && !collideinside)
                {
                    if(rev)
                    {
                        float mag = max(max(vec(proj.vel).add(proj.falling).magnitude()*proj.elasticity, proj.speedmin), 1.f);
                        if(proj.speedmax > 0) mag = min(mag, proj.speedmax);
                        proj.vel = vec(proj.o).sub(orig).safenormalize().mul(mag);
                        proj.falling = vec(0, 0, 0);
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
            case PROJ_AFFINITY: break;
            case PROJ_GIB: case PROJ_DEBRIS: case PROJ_EJECT: case PROJ_VANITY: case PROJ_PIECE: size = proj.lifesize;
            case PROJ_ENTITY:
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
        if(proj.mdlname && *proj.mdlname && ((m = loadmodel(proj.mdlname)) != NULL))
        {
            bool fullrot = proj.projtype == PROJ_GIB || proj.projtype == PROJ_EJECT;
            vec center, radius;
            m->boundbox(center, radius);
            center.mul(size);
            radius.mul(size);
            if(proj.projtype == PROJ_ENTITY)
            {
                center.add(size*0.5f);
                radius.add(size);
            }
            rotatebb(center, radius, proj.projtype != PROJ_AFFINITY ? proj.yaw : 0.f, fullrot ? proj.pitch : 0.f, fullrot ? proj.roll : 0.f);
            proj.radius = max(radius.x, radius.y);
            if(proj.projtype == PROJ_AFFINITY) proj.xradius = proj.yradius = proj.radius;
            else
            {
                proj.xradius = radius.x;
                proj.yradius = radius.y;
            }
            proj.height = proj.zradius = proj.aboveeye = radius.z;
        }
        else switch(proj.projtype)
        {
            case PROJ_GIB: case PROJ_DEBRIS: case PROJ_VANITY: case PROJ_PIECE:
            {
                proj.height = proj.radius = proj.xradius = proj.yradius = proj.zradius = proj.aboveeye = 0.5f*size;
                break;
            }
            case PROJ_EJECT:
            {
                proj.height = proj.zradius = proj.aboveeye = 0.25f*size;
                proj.radius = proj.yradius = 0.5f*size;
                proj.xradius = 0.125f*size;
                break;
            }
            case PROJ_ENTITY:
            {
                if(entities::ents.inrange(proj.id))
                    proj.radius = proj.xradius = proj.yradius = proj.height = proj.zradius = proj.aboveeye = enttype[entities::ents[proj.id]->type].radius*0.25f*size;
                else proj.radius = proj.xradius = proj.yradius = proj.height = proj.zradius = proj.aboveeye = size;
                break;
            }
        }
        return true;
    }

    void updatetrail(projent &proj)
    {
        if(proj.projcollide&COLLIDE_SCAN)
        {
            proj.trailpos = proj.from;
            return;
        }
        if(proj.projtype == PROJ_SHOT && proj.stuck && !proj.beenused && WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) == 2)
        {
            float dist = WX(WK(proj.flags), proj.weap, proxdist, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
            int stucktime = lastmillis-proj.stuck, stuckdelay = WF(WK(proj.flags), proj.weap, proxdelay, WS(proj.flags));
            if(stuckdelay && stuckdelay > stucktime) dist *= stucktime/float(stuckdelay);
            float blocked = tracecollide(&proj, proj.o, proj.norm, dist, RAY_CLIPMAT|RAY_ALPHAPOLY, true);
            proj.trailpos = vec(proj.o).add(vec(proj.norm).mul(blocked >= 0 ? blocked : dist));
            return;
        }
        vec dir(0, 0, 0);
        float dist = proj.o.dist(proj.from);
        if(!proj.child && dist > 0) dir = vec(proj.from).sub(proj.o).safenormalize();
        else
        {
            vec vel = vec(proj.vel).add(proj.falling);
            if(dist > 0 && !vel.iszero()) dir = vel.safenormalize();
        }
        if(dist > 0)
        {
            float len = WF(WK(proj.flags), proj.weap, length, WS(proj.flags))*(1.1f-proj.lifespan)*proj.curscale,
                  minsize = WF(WK(proj.flags), proj.weap, radius, WS(proj.flags))*proj.curscale,
                  maxsize = min(WF(WK(proj.flags), proj.weap, length, WS(proj.flags)), dist),
                  cursize = clamp(len, min(minsize, maxsize), maxsize);
            if(cursize > 0)
            {
                proj.trailpos = vec(dir).mul(cursize).add(proj.o);
                return;
            }
        }
        proj.trailpos = proj.o;
    }

    void updatetargets(projent &proj, bool waited = false)
    {
        if(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH)
        {
            if(proj.weap == W_MELEE)
            {
                vec feet = proj.owner->feetpos();
                if(proj.target && proj.target->isalive()) proj.dest = proj.target->headpos();
                else proj.dest = vec(proj.dest).sub(proj.from).safenormalize().mul(proj.owner->radius).add(feet);
                proj.o = proj.dest;
                proj.trailpos = proj.from = feet;
            }
            else
            {
                proj.o = proj.dest = proj.owner->muzzletag();
                proj.trailpos = proj.from = proj.owner->origintag();
            }
        }
        else
        {
            if(!proj.child && (waited || (WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_SCAN && !proj.bounced)))
                proj.from = proj.owner->muzzletag();
            if(!proj.child && waited) proj.o = proj.from;
            updatetrail(proj);
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
            case PROJ_SHOT:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = WF(WK(proj.flags), proj.weap, radius, WS(proj.flags));
                proj.elasticity = WF(WK(proj.flags), proj.weap, elasticity, WS(proj.flags));
                proj.relativity = W2(proj.weap, relativity, WS(proj.flags));
                proj.liquidcoast = WF(WK(proj.flags), proj.weap, liquidcoast, WS(proj.flags));
                proj.weight = WF(WK(proj.flags), proj.weap, weight, WS(proj.flags));
                proj.buoyancy = WF(WK(proj.flags), proj.weap, buoyancy, WS(proj.flags));
                proj.projcollide = WF(WK(proj.flags), proj.weap, collide, WS(proj.flags));
                proj.speedmin = WF(WK(proj.flags), proj.weap, speedmin, WS(proj.flags));
                proj.speedmax = WF(WK(proj.flags), proj.weap, speedmax, WS(proj.flags));
                proj.extinguish = WF(WK(proj.flags), proj.weap, extinguish, WS(proj.flags))|4;
                proj.interacts = WF(WK(proj.flags), proj.weap, interacts, WS(proj.flags));
                proj.mdlname = weaptype[proj.weap].proj[WS(proj.flags) ? 1 : 0].count ? weaptype[proj.weap].proj[WS(proj.flags) ? 1 : 0].name[rnd(weaptype[proj.weap].proj[WS(proj.flags) ? 1 : 0].count)] : "";
                proj.fxtype = WF(WK(proj.flags), proj.weap, fxtypeproj, WS(proj.flags));
                proj.escaped = !proj.owner || proj.child || WK(proj.flags) || WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH || proj.weap == W_MELEE;
                updatetargets(proj, waited);
                if(proj.projcollide&COLLIDE_PROJ) collideprojs.add(&proj);
                break;
            }
            case PROJ_GIB:
            {
                if(!game::nogore && game::bloodscale > 0)
                {
                    proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f;
                    if(!proj.mdlname || !*proj.mdlname)
                    {
                        proj.lifesize = 1.5f-(rnd(100)/100.f);
                        switch(rnd(3))
                        {
                            case 2: proj.mdlname = "projectiles/gibs/gib03"; break;
                            case 1: proj.mdlname = "projectiles/gibs/gib02"; break;
                            case 0: default: proj.mdlname = "projectiles/gibs/gib01"; break;
                        }

                        if(proj.owner)
                        {
                            if(!proj.owner->isnotalive())
                            {
                                proj.lifemillis = proj.lifetime = 1;
                                proj.lifespan = 1.f;
                                proj.state = CS_DEAD;
                                proj.escaped = true;
                                return;
                            }
                            // proj.o = proj.owner->headtag();
                            // proj.o.z -= proj.owner->zradius*0.125f;
                            proj.lifesize *= proj.owner->curscale;
                        }
                        proj.vel.add(vec(rnd(101)-50, rnd(101)-50, rnd(101)-50).mul((proj.speed / 100.f) * (proj.owner && proj.owner->obliterated ? 8 : 1)));
                    }

                    float buoy = gibbuoyancymax;
                    if(gibbuoyancymax != gibbuoyancymin)
                    {
                        float bmin = min(gibbuoyancymax, gibbuoyancymin), boff = max(gibbuoyancymax, gibbuoyancymin)-bmin;
                        buoy = bmin+(rnd(1000)*boff/1000.f);
                    }
                    proj.elasticity = gibelasticity;
                    proj.relativity = gibrelativity;
                    proj.liquidcoast = gibliquidcoast;
                    proj.weight = gibweight*proj.lifesize;
                    proj.buoyancy = buoy*proj.lifesize;
                    proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
                    proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                    proj.fadetime = rnd(200) + 51;
                    proj.extinguish = 6;
                    proj.interacts = 3;
                    proj.fxtype = FX_P_GIB;
                    break;
                } // otherwise fall through
            }
            case PROJ_DEBRIS:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f;
                if(!proj.mdlname || !*proj.mdlname)
                {
                    proj.lifesize = 1.5f-(rnd(100)/100.f);
                    switch(rnd(4))
                    {
                        case 3: proj.mdlname = "projectiles/debris/debris04"; break;
                        case 2: proj.mdlname = "projectiles/debris/debris03"; break;
                        case 1: proj.mdlname = "projectiles/debris/debris02"; break;
                        case 0: default: proj.mdlname = "projectiles/debris/debris01"; break;
                    }
                    if(proj.owner) proj.lifesize *= proj.owner->curscale;
                    proj.vel.add(vec(rnd(101)-50, rnd(101)-50, rnd(101)-50).mul((proj.speed / 100.f) * (proj.owner && proj.owner->obliterated ? 8 : 1)));
                }
                proj.relativity = 0.f;
                proj.elasticity = debriselasticity;
                proj.liquidcoast = debrisliquidcoast;
                proj.weight = debrisweight*proj.lifesize;
                proj.buoyancy = debrisbuoyancy*proj.lifesize;
                proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER|COLLIDE_OWNER;
                proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                proj.fadetime = rnd(200) + 51;
                proj.extinguish = 1;
                proj.interacts = 3;
                proj.fxtype = FX_P_DEBRIS;
                break;
            }
            case PROJ_EJECT:
            {
                proj.height = proj.aboveeye = 0.5f;
                proj.radius = proj.yradius = 1;
                proj.xradius = 0.25f;
                if(!proj.mdlname || !*proj.mdlname)
                {
                    if(!isweap(proj.weap) && proj.owner) proj.weap = proj.owner->weapselect;
                    if(waited && proj.owner)
                    {
                        proj.o = proj.from = proj.owner->ejecttag(proj.weap, 0);
                        proj.dest = proj.owner->ejecttag(proj.weap, 1);
                    }
                    if(isweap(proj.weap))
                    {
                        proj.mdlname = weaptype[proj.weap].eprj[WS(proj.flags) ? 1 : 0].count ? weaptype[proj.weap].eprj[WS(proj.flags) ? 1 : 0].name[rnd(weaptype[proj.weap].eprj[WS(proj.flags) ? 1 : 0].count)] : "";
                        if(!proj.mdlname || !*proj.mdlname) proj.mdlname = "projectiles/catridge";
                        proj.lifesize = weaptype[proj.weap].esize;
                        proj.material = bvec::fromcolor(W(proj.weap, colour));
                    }
                    else
                    {
                        proj.mdlname = "projectiles/catridge";
                        proj.lifesize = 1;
                    }
                    if(proj.owner) proj.lifesize *= proj.owner->curscale;
                }
                proj.elasticity = ejectelasticity;
                proj.relativity = ejectrelativity;
                proj.liquidcoast = ejectliquidcoast;
                proj.weight = (ejectweight+(proj.speed*2))*proj.lifesize; // so they fall better in relation to their speed
                proj.buoyancy = ejectbuoyancy*proj.lifesize;
                proj.projcollide = BOUNCE_GEOM;
                proj.escaped = true;
                proj.fadetime = rnd(200) + 51;
                proj.extinguish = 6;
                proj.interacts = 3;
                proj.fxtype = FX_P_CASING;
                break;
            }
            case PROJ_ENTITY:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 4;
                proj.mdlname = entities::entmdlname(entities::ents[proj.id]->type, entities::ents[proj.id]->attrs);
                proj.elasticity = itemelasticity;
                proj.relativity = itemrelativity;
                proj.liquidcoast = itemliquidcoast;
                proj.weight = itemweight;
                proj.buoyancy = itembuoyancy;
                proj.projcollide = itemcollide;
                proj.speedmin = itemspeedmin;
                proj.speedmax = itemspeedmax;
                proj.escaped = true;
                float mag = proj.inertia.magnitude(), yaw = proj.yaw, pitch = proj.pitch;
                if(mag > 0) vectoyawpitch(vec(proj.inertia).safenormalize(), yaw, pitch);
                else mag = itemdropminspeed;
                if(proj.flags > 1 && (itemdropspreadxy > 0 || itemdropspreadz > 0))
                {
                    int m = proj.value % 2, n = (2 + proj.value) / 2;
                    if(itemdropspreadxy > 0) yaw += n * itemdropspreadxy * (m ? 1 : -1);
                    if(itemdropspreadz > 0) pitch += n * itemdropspreadz * (m ? 1 : -1);
                }
                proj.inertia = vec(yaw*RAD, pitch*RAD).mul(mag);
                proj.fadetime = itemfadetime;
                proj.extinguish = itemextinguish;
                proj.interacts = iteminteracts;
                break;
            }
            case PROJ_AFFINITY:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 4;
                vec dir = vec(proj.dest).sub(proj.from).safenormalize();
                vectoyawpitch(dir, proj.yaw, proj.pitch);
                proj.escaped = true;
                proj.fadetime = 1;
                switch(game::gamemode)
                {
                    case G_BOMBER:
                        proj.mdlname = "props/ball";
                        proj.projcollide = bombercollide;
                        proj.extinguish = bomberextinguish;
                        proj.interacts = bomberinteracts;
                        proj.elasticity = bomberelasticity;
                        proj.weight = bomberweight;
                        proj.buoyancy = bomberbuoyancy;
                        proj.relativity = bomberrelativity;
                        proj.liquidcoast = bomberliquidcoast;
                        proj.speedmin = bomberspeedmin;
                        proj.speedmax = bomberspeedmax;
                        break;
                    case G_CAPTURE: default:
                        proj.mdlname = "props/flag";
                        proj.projcollide = capturecollide;
                        proj.extinguish = captureextinguish;
                        proj.interacts = captureinteracts;
                        proj.elasticity = captureelasticity;
                        proj.weight = captureweight;
                        proj.buoyancy = capturebuoyancy;
                        proj.relativity = capturerelativity;
                        proj.liquidcoast = captureliquidcoast;
                        proj.speedmin = capturespeedmin;
                        proj.speedmax = capturespeedmax;
                        break;
                }
                break;
            }
            case PROJ_VANITY: case PROJ_PIECE:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 1;
                if(!proj.mdlname || !*proj.mdlname)
                {
                    switch(proj.projtype)
                    {
                        case PROJ_VANITY: proj.mdlname = game::vanityfname(proj.owner, proj.weap, proj.value, true); break;
                        case PROJ_PIECE:
                        {
                            if(proj.owner) switch(proj.owner->actortype)
                            {
                                case A_PLAYER: case A_BOT: proj.mdlname = playerparts[clamp(proj.weap, 0, PLAYERPARTS-1)].filename; break;
                                case A_JANITOR: proj.mdlname = janitorparts[clamp(proj.weap, 0, JANITORPARTS-1)].filename; break;
                                default: break;
                            }
                            if(proj.mdlname && *proj.mdlname) break;
                            // otherwise fall through
                        }
                        default:
                        {
                            switch(rnd(7))
                            {
                                case 6: proj.mdlname = "projectiles/debris/debris04"; break;
                                case 5: proj.mdlname = "projectiles/debris/debris03"; break;
                                case 4: proj.mdlname = "projectiles/debris/debris02"; break;
                                case 3: proj.mdlname = "projectiles/debris/debris01"; break;
                                case 2: proj.mdlname = "projectiles/gibs/gib03"; break;
                                case 1: proj.mdlname = "projectiles/gibs/gib02"; break;
                                case 0: default: proj.mdlname = "projectiles/gibs/gib01"; break;
                            }
                            break;
                        }
                    }
                    if(proj.owner) proj.lifesize = proj.owner->curscale;
                    proj.vel.add(vec(rnd(101)-50, rnd(101)-50, rnd(101)-50).mul((proj.speed / 100.f) * (proj.owner && proj.owner->obliterated ? 8 : 1)));
                }
                proj.elasticity = vanityelasticity;
                proj.relativity = vanityrelativity;
                proj.liquidcoast = vanityliquidcoast;
                proj.weight = vanityweight;
                proj.buoyancy = vanitybuoyancy;
                proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
                proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                proj.fadetime = rnd(200) + 51;
                proj.extinguish = 6;
                proj.interacts = 3;
                if(proj.projtype == PROJ_PIECE) proj.fxtype = FX_P_GIB;
                break;
            }
            default: break;
        }

        if(proj.isjunk()) junkprojs.add(&proj);
        typeprojs[proj.projtype].add(&proj);

        if(proj.projtype != PROJ_SHOT) updatebb(proj, true);
        proj.spawntime = lastmillis;
        proj.hit = NULL;
        proj.collidezones = CLZ_NONE;

        if(proj.owner && (proj.projtype != PROJ_SHOT || (!proj.child && !(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH))))
        {
            vec eyedir = vec(proj.o).sub(proj.owner->o);
            float eyedist = eyedir.magnitude();
            if(eyedist > 0)
            {
                eyedir.safenormalize();
                float blocked = tracecollide(&proj, proj.owner->o, eyedir, eyedist, RAY_CLIPMAT|RAY_ALPHAPOLY, false, GUARDRADIUS);
                if(blocked >= 0)
                {
                    proj.o = vec(eyedir).mul(blocked-max(proj.radius, 1e-3f)).add(proj.owner->o);
                    proj.dest = vec(eyedir).mul(blocked).add(proj.owner->o);
                }
            }
        }

        if(proj.projtype != PROJ_SHOT || !(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH))
        {
            vec dir = vec(proj.dest).sub(proj.o);
            float maxdist = dir.magnitude();
            if(maxdist > 1e-3f)
            {
                dir.mul(1/maxdist);
                if(proj.projtype != PROJ_EJECT) vectoyawpitch(dir, proj.yaw, proj.pitch);
            }
            else if(!proj.child && proj.owner)
            {
                if(proj.projtype != PROJ_EJECT)
                {
                    proj.yaw = proj.owner->yaw;
                    proj.pitch = proj.owner->pitch;
                }
                dir = vec(proj.yaw*RAD, proj.pitch*RAD);
            }
            vec rel = vec(proj.vel).add(proj.falling).add(proj.inertia.mul(proj.relativity));
            proj.vel = vec(rel).add(vec(dir).mul(physics::movevelocity(&proj)));
            proj.falling = vec(0, 0, 0);
        }
        if(proj.projtype != PROJ_SHOT) spherecheck(proj, proj.projcollide&BOUNCE_GEOM);
        proj.resetinterp();
    }

    static int sequenceid = 0;

    projent *findprojseq(int type, int id)
    {
        if(type < 0 || type >= PROJ_MAX)
        {
            loopv(projs) if(projs[i]->seqid == id) return projs[i];
            return NULL;
        }

        loopv(typeprojs[type]) if(typeprojs[type][i]->seqid == id) return typeprojs[type][i];

        return NULL;
    }

    projent *create(const vec &from, const vec &dest, bool local, gameent *d, int type, int fromweap, int fromflags, int lifetime, int lifemillis, int waittime, int speed, int id, int weap, int value, int flags, float scale, bool child, gameent *target)
    {
        projent &proj = *new projent;

        proj.seqid = sequenceid;
        if(++sequenceid < 0) sequenceid = 0;

        proj.o = proj.from = proj.trailpos = from;
        proj.dest = dest;
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
        proj.target = target;
        proj.movement = proj.distance = 0;
        proj.prev = proj.next = NULL;

        if(proj.projtype == PROJ_AFFINITY)
        {
            proj.vel = proj.inertia = proj.dest;
            proj.dest.add(proj.from);
        }
        else
        {
            proj.weap = weap;
            proj.fromweap = fromweap;
            proj.fromflags = fromflags;
            proj.value = value;
            if(child)
            {
                proj.child = true;
                proj.owner = d;
                proj.vel = vec(proj.dest).sub(proj.from);
                vectoyawpitch(vec(proj.vel).safenormalize(), proj.yaw, proj.pitch);
            }
            else if(d)
            {
                proj.owner = d;
                proj.yaw = d->yaw;
                proj.pitch = d->pitch;
                proj.inertia = vec(d->vel).add(d->falling);
                if(proj.projtype == PROJ_SHOT && isweap(proj.weap))
                {
                    if(issound(d->wschan[WS_POWER_CHAN]))
                    {
                        if(weaptype[proj.weap].thrown)
                        {
                            int index = d->wschan[WS_POWER_CHAN];
                            soundsources[index].owner = &proj;
                            soundsources[index].vpos = &proj.o;
                            soundsources[index].vel = proj.vel;
                            soundsources[index].hook = &proj.schan;
                            soundsources[index].flags |= SND_LOOP;
                        }
                        soundsources[d->wschan[WS_POWER_CHAN]].clear();
                        d->wschan[WS_POWER_CHAN] = -1;
                    }
                    else emitsound(WSND2(proj.weap, WS(proj.flags), S_W_TRANSIT), &proj.o, &proj, &proj.schan, SND_LOOP);
                }
            }
            else vectoyawpitch(vec(proj.dest).sub(proj.from).safenormalize(), proj.yaw, proj.pitch);
        }

        if(!proj.waittime) init(proj, false);
        projs.add(&proj);

        return &proj;
    }

    void drop(gameent *d, int weap, int ent, int ammo, bool local, int targ, int index, int count, gameent *target)
    {
        if(isweap(weap) && weap >= W_OFFSET && weap < W_ALL)
        {
            if(ammo >= 0)
            {
                if(ammo > 0 && entities::ents.inrange(ent))
                {
                    create(d->muzzletag(), d->muzzletag(), local, d, PROJ_ENTITY, -1, 0, W(weap, spawnstay), W(weap, spawnstay), 1, 1, ent, ammo, index, count, 1.0f, false, target);

                    gameentity &e = *(gameentity *)entities::ents[ent];
                    if(enttype[e.type].usetype == EU_ITEM)
                    {
                        int attr = m_attr(e.type, e.attrs[0]);

                        if(gs_playing(game::gamestate) && e.type == WEAPON && (itemannouncedrop&(1<<attr)) != 0)
                        {
                            gamelog *log = new gamelog(GAMELOG_EVENT);
                            log->addlist("args", "type", "item");
                            log->addlist("args", "action", "drop");
                            log->addlist("args", "entity", ent);
                            log->addlist("args", "attr", attr);
                            log->addlist("args", "colour", colourwhite);
                            log->addlistf("args", "console", "%s dropped a %s", game::colourname(d), e.type == WEAPON && isweap(attr) ? W(attr, longname) : enttype[e.type].name);
                            log->addclient("client", d);
                            if(!log->push()) DELETEP(log);
                        }
                    }
                }
                d->weapammo[weap][W_A_CLIP] = -1;
                d->weapammo[weap][W_A_STORE] = 0;
                if(targ >= 0) d->setweapstate(weap, W_S_SWITCH, W(weap, delayswitch), lastmillis);
            }
            else create(d->muzzletag(), d->muzzletag(), local, d, PROJ_SHOT, -1, 0, 1, W2(weap, time, false), 1, 1, 1, weap, -1, 0, 1.0f, false, target);
        }
    }

    void shootv(int weap, int flags, int sub, int offset, float scale, vec &from, vec &dest, vector<shotmsg> &shots, gameent *d, bool local, gameent *v)
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
            int slot = WSNDF(weap, WS(flags));
            if(slot >= 0 && skew > 0)
            {
                vec *sndpos = weapons::getweapsoundpos(d, d->getmuzzle());

                // quick hack to have additional audio feedback for zapper
                if(weap == W_ZAPPER && !(WS(flags)))
                    emitsound(WSND2(weap, WS(flags), S_W_TRANSIT), sndpos, d, &d->wschan[WS_OTHER_CHAN], 0, skew);

                if((weap == W_FLAMER || weap == W_ZAPPER || weap == W_CORRODER) && !(WS(flags)))
                {
                    int ends = lastmillis + delayattack + PHYSMILLIS;
                    if(issound(d->wschan[WS_MAIN_CHAN]) && soundsources[d->wschan[WS_MAIN_CHAN]].slotnum == getsoundslot(slot))
                        soundsources[d->wschan[WS_MAIN_CHAN]].ends = ends;
                    else emitsound(slot, sndpos, d, &d->wschan[WS_MAIN_CHAN], SND_LOOP, skew, 1, -1, -1, -1, ends);
                }
                else if(!W2(weap, time, WS(flags)) || life)
                {
                    if(issound(d->wschan[WS_MAIN_CHAN]) &&
                       (soundsources[d->wschan[WS_MAIN_CHAN]].slotnum == getsoundslot(WSNDF(weap, false)) ||
                        soundsources[d->wschan[WS_MAIN_CHAN]].slotnum == getsoundslot(WSNDF(weap, true))))
                            soundsources[d->wschan[WS_MAIN_CHAN]].unhook();
                    emitsound(slot, sndpos, d, &d->wschan[WS_MAIN_CHAN], 0, W2(weap, soundskew, WS(flags)) ? skew : 1.0f);
                }
            }
        }
        vec orig = d == game::player1 || d->ai ? from : d->muzzletag();
        if(delayattack >= 5 && weap < W_ALL && game::showweapfx && muzzleflash&(d == game::focus ? 2 : 1))
        {
            int color = WHCOL(d, weap, fxcol, WS(flags));
            float muz = muzzleblend*W2(weap, fxblend, WS(flags));
            if(d == game::focus) muz *= muzzlefade;
            int fxtype = WF(WK(flags), weap, fxtype, WS(flags));
            fx::FxHandle fxhandle = game::getweapfx(fxtype);
            if(fxhandle.isvalid())
            {
                float fxscale = WF(WK(flags), weap, fxscale, WS(flags));
                vec targ;
                safefindorientation(d->o, d->yaw, d->pitch, targ);
                targ.sub(from).safenormalize().add(from);
                fx::emitter &e = fx::createfx(fxhandle, &d->weaponfx)
                    .setfrom(from)
                    .setto(targ)
                    .setblend(muz)
                    .setscale(fxscale)
                    .setcolor(bvec(color))
                    .setentity(d);

                e.setparam(W_FX_POWER_PARAM, scale);
            }
        }
        loopv(shots)
        {
            projent *shotproj = create(orig, vec(shots[i].pos).div(DMF), local, d, PROJ_SHOT, -1, 0, max(life, 1), W2(weap, time, WS(flags)), delay+(iter*i), speed, shots[i].id, weap, -1, flags, skew, false, v);

            if(W2(weap, fxchain, WS(flags)))
            {
                if(d->wasfiring < 0) d->projchain = NULL;
                listpushfront(shotproj, d->projchain, prev, next);
            }
        }
        if(A(d->actortype, abilities)&(1<<A_A_AMMO) && W2(weap, ammosub, WS(flags)) && ejectfade && weaptype[weap].eprj[WS(flags) ? 1 : 0].count) loopi(W2(weap, ammosub, WS(flags)))
            create(d->ejecttag(weap, 0), d->ejecttag(weap, 1), local, d, PROJ_EJECT, -1, 0, rnd(ejectfade)+ejectfade, 0, delay, rnd(weaptype[weap].espeed)+weaptype[weap].espeed, 0, weap, -1, flags);

        d->setweapstate(weap, WS(flags) ? W_S_SECONDARY : W_S_PRIMARY, delayattack, lastmillis);
        d->weapammo[weap][W_A_CLIP] = max(d->weapammo[weap][W_A_CLIP]-sub-offset, 0);
        d->weapshot[weap] = sub;
        if(offset > 0)
        {
            if(W(weap, ammostore) > 0) d->weapammo[weap][W_A_STORE] = clamp(d->weapammo[weap][W_A_STORE]+offset, 0, W(weap, ammostore));
            d->weapload[weap][W_A_CLIP] = -offset;
        }
        d->lastshoot = lastmillis;
        if(A(d->actortype, abilities)&(1<<A_A_PUSHABLE))
        {
            vec kick = vec(d->yaw*RAD, d->pitch*RAD).mul(-W2(weap, kickpush, WS(flags))*skew);
            if(!kick.iszero())
            {
                if(d == game::focus) game::swaypush.add(vec(kick).mul(kickpushsway));
                float kickmod = kickpushscale;
                if(W2(weap, cooked, WS(flags))&W_C_ZOOM && WS(flags)) kickmod *= kickpushzoom;
                if(d->crouching() && !d->hasslide()) kickmod *= kickpushcrouch;
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
                    if(WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags)) > 0 && distance > WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags)))
                    {
                        proj.state = CS_DEAD;
                        proj.lifesize = WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags));
                        break;
                    }
                    else if(WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)) > 0 && distance > WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)))
                    {
                        if(type%2 || !proj.usestuck())
                        {
                            float dist = 1-((distance-WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)))/WF(WK(proj.flags), proj.weap, taperout, WS(proj.flags)));
                            proj.lifesize = clamp(dist, WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                        }
                        break;
                    }
                }
                if(WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)) > 0 && distance < WF(WK(proj.flags), proj.weap, taperin, WS(proj.flags)))
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
                if(proj.lifespan < spanin)
                {
                    proj.lifesize = clamp(proj.lifespan/spanin, WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                    break;
                }
                else if(proj.lifespan >= (1-spanout))
                {
                    if(type%2 || !proj.usestuck())
                        proj.lifesize = clamp(1-((proj.lifespan-(1-spanout))/spanout), WF(WK(proj.flags), proj.weap, tapermin, WS(proj.flags)), WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)));
                    break;
                }
                proj.lifesize = WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags));
                break;
            }
            default: proj.lifesize = WF(WK(proj.flags), proj.weap, tapermax, WS(proj.flags)); break;
        }
    }

    void iter(projent &proj)
    {
        // proj.movement = 0; possible for this to get reset right before a collision, why was it here?
        proj.lifespan = clamp((proj.lifemillis-proj.lifetime)/float(max(proj.lifemillis, 1)), 0.f, 1.f);

        if(proj.target && !proj.target->isalive()) proj.target = NULL;
        updatesticky(proj);

        if(proj.projtype == PROJ_SHOT)
        {
            updatetargets(proj);
            updatetaper(proj, proj.distance, true);
        }
        else if(updatebb(proj) && spherecheck(proj, proj.projcollide&BOUNCE_GEOM)) proj.resetinterp();
        else proj.lastgood = proj.o;
    }

    void effect(projent &proj)
    {
        if(projdebug)
        {
            float yaw, pitch, length;
            vec pos;
            if(proj.stuck)
            {
                vectoyawpitch(proj.norm, yaw, pitch);
                length = proj.radius;
                pos = proj.o;
            }
            else if(proj.projcollide&COLLIDE_FROMTO)
            {
                vectoyawpitch(vec(proj.o).sub(proj.from).safenormalize(), yaw, pitch);
                length = proj.radius;
                pos = proj.from;
            }
            else
            {
                vec vel = vec(proj.vel).add(proj.falling);
                vectoyawpitch(vel.safenormalize(), yaw, pitch);
                length = max(vel.magnitude(), proj.radius * 2);
                pos = proj.o;
            }

            part_radius(pos, vec(proj.radius, proj.radius, proj.radius), 2, 1, 1, 0x22FFFF);
            part_dir(pos, yaw, pitch, length, 2, 1, 1, 0xFF22FF);
        }

        if(proj.projtype == PROJ_SHOT) updatetaper(proj, proj.distance);
        
        doprojfx(proj, PROJ_FX_LIFE);
        if(proj.projtype == PROJ_SHOT && proj.stuck && !proj.beenused && WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) == 2)
            doprojfx(proj, PROJ_FX_TRIPWIRE);
    }

    void destroy(projent &proj)
    {
        proj.lifespan = clamp((proj.lifemillis-proj.lifetime)/float(max(proj.lifemillis, 1)), 0.f, 1.f);

        if(proj.projcollide&COLLIDE_PROJ)
        {
            collideprojs.removeobj(&proj);
            cleardynentcache();
        }
        if(proj.isjunk()) junkprojs.removeobj(&proj);
        typeprojs[proj.projtype].removeobj(&proj);

        switch(proj.projtype)
        {
            case PROJ_SHOT:
            {
                updatetargets(proj);

                if(proj.owner)
                {
                    if(!WK(proj.flags) && !m_insta(game::gamemode, game::mutators) && W2(proj.weap, fragweap, WS(proj.flags)) >= 0)
                    {
                        vec vel = vec(proj.vel).add(proj.falling);
                        bool cond = (W2(proj.weap, fragcond, WS(proj.flags))&1) != 0;
                        if(!cond && W2(proj.weap, fragcond, WS(proj.flags))&2 && proj.hit) cond = true;
                        if(!cond && W2(proj.weap, fragcond, WS(proj.flags))&4 && proj.stick) cond = true;
                        if(!cond && W2(proj.weap, fragcond, WS(proj.flags))&8 && proj.stuck) cond = true;

                        int f = W2(proj.weap, fragweap, WS(proj.flags)), w = f%W_MAX,
                            life = W2(proj.weap, fragtime, WS(proj.flags)), delay = W2(proj.weap, fragtimedelay, WS(proj.flags));
                        float mag = max(vel.magnitude(), W2(proj.weap, fragspeedmin, WS(proj.flags))),
                              scale = W2(proj.weap, fragscale, WS(proj.flags))*proj.curscale,
                              offset = cond ? W2(proj.weap, fragoffset, WS(proj.flags)) : FVAR_NONZERO,
                              skew = cond ? W2(proj.weap, fragskew, WS(proj.flags)) : W2(proj.weap, fragspread, WS(proj.flags));

                        vec dir = vec(proj.stuck ? proj.norm : vel).safenormalize(), pos = vec(proj.o).add(vec(dir).mul(offset));
                        if(W2(proj.weap, fragspeedmax, WS(proj.flags)) > 0) mag = min(mag, W2(proj.weap, fragspeedmax, WS(proj.flags)));
                        if(W2(proj.weap, fragjump, WS(proj.flags)) > 0) life -= int(ceilf(life*W2(proj.weap, fragjump, WS(proj.flags))));

                        loopi(W2(proj.weap, fragrays, WS(proj.flags)))
                        {
                            vec to = vec(pos).add(dir);
                            if(W2(proj.weap, fragspeed, WS(proj.flags)) > 0)
                                mag = rnd(W2(proj.weap, fragspeed, WS(proj.flags)))*0.5f+W2(proj.weap, fragspeed, WS(proj.flags))*0.5f;

                            if(skew > 0) to.add(vec(rnd(2001)-1000, rnd(2001)-1000, rnd(2001)-1000).safenormalize().mul(skew*mag));

                            if(W2(proj.weap, fragrel, WS(proj.flags)) != 0) to.add(vec(dir).mul(W2(proj.weap, fragrel, WS(proj.flags))*mag));

                            create(pos, to, proj.local, proj.owner, PROJ_SHOT, proj.weap, proj.flags, max(life, 1), W2(proj.weap, fragtime, WS(proj.flags)), delay, W2(proj.weap, fragspeed, WS(proj.flags)), proj.id, w, -1, (f >= W_MAX ? HIT_ALT : 0)|HIT_FLAK, scale, true, proj.target);

                            delay += W2(proj.weap, fragtimeiter, WS(proj.flags));
                        }
                    }

                    if(proj.local)
                        client::addmsg(N_DESTROY, "ri9i2", proj.owner->clientnum, lastmillis-game::maptime, proj.projtype, proj.weap, proj.flags, proj.fromweap, proj.fromflags, WK(proj.flags) ? -proj.id : proj.id, 0, int(proj.curscale*DNF), 0);
                }
                break;
            }
            case PROJ_ENTITY:
            {
                if(proj.beenused <= 1 && proj.local && proj.owner)
                    client::addmsg(N_DESTROY, "ri9i2", proj.owner->clientnum, lastmillis-game::maptime, proj.projtype, -1, 0, -1, 0, proj.id, 0, int(proj.curscale*DNF), 0);
                break;
            }
            case PROJ_AFFINITY:
            {
                if(proj.beenused <= 1) client::addmsg(N_RESETAFFIN, "ri", proj.id);
                if(m_capture(game::gamemode) && capture::st.flags.inrange(proj.id)) capture::st.flags[proj.id].proj = NULL;
                else if(m_bomber(game::gamemode) && bomber::st.flags.inrange(proj.id)) bomber::st.flags[proj.id].proj = NULL;
                break;
            }
            default: break;
        }

        doprojfx(proj, PROJ_FX_DESTROY);
    }

    int check(projent &proj, const vec &dir)
    {
        if(proj.projtype == PROJ_SHOT ? proj.o.z <= physics::getdeathplane()*worldsize : !insideworld(proj.o)) return 0; // remove, always..
        int chk = 0;
        if(proj.extinguish&1 || proj.extinguish&2)
        {
            if(proj.extinguish&1 && (proj.inmaterial&MATF_VOLUME) == MAT_WATER) chk |= 1;
            if(proj.extinguish&2 && (proj.inmaterial&MAT_DEATH)) chk |= 2;
        }
        if(chk)
        {
            if(chk&1 && !proj.limited && !WK(proj.flags) && proj.weap != W_MELEE)
            {
                int snd = S_EXTINGUISH;
                float gain = clamp(0.2f*proj.curscale, 0.f, 1.f), size = max(proj.radius, 1.f);
                if(proj.projtype == PROJ_SHOT && isweap(proj.weap))
                {
                    snd = WSND2(proj.weap, WS(proj.flags), S_W_EXTINGUISH);
                    gain = clamp(0.1f+(0.9f*proj.lifespan*proj.lifesize*proj.curscale), 0.f, 1.f);
                    float expl = WX(WK(proj.flags), proj.weap, radial, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                    if(expl > 0) size *= expl*1.5f;
                    else size *= 2.5f;
                }
                else size *= 2.5f;
                if(gain > 0) emitsound(snd, &proj.o, NULL, NULL, 0, gain);
                part_create(PART_SMOKE, 500, proj.o, 0xAAAAAA, max(size, 1.5f), 1, -10);
                proj.limited = true;
                if(proj.projtype == PROJ_DEBRIS) proj.material = bvec::fromcolor(colourwhite);
            }
            proj.norm = dir;
            if(proj.extinguish&4) return 0;
        }
        return 1;
    }

    int impact(projent &proj, const vec &dir, physent *d, int flags, const vec &norm, int inside = 0)
    {
        int collidemod = proj.projcollide;
        proj.norm = norm;

        if(inside)
        {
            vec oldpos = proj.o;
            if(proj.projtype == PROJ_SHOT)
            {
                vec rev = vec(dir.iszero() ? proj.vel : dir).safenormalize().mul(max(proj.radius * 0.125f, 0.25f));
                for(int iters = inside + 8; iters > 0; iters--)
                {
                    proj.o.sub(rev);
                    if(collide(&proj, dir, 0.f, collidemod&COLLIDE_DYNENT, true, GUARDRADIUS))
                        if(!collideinside && (!collideplayer || collidemod&(collideplayer->type == ENT_PROJ ? COLLIDE_SHOTS : COLLIDE_PLAYER)))
                        {
                            if(collideplayer) d = collideplayer;
                            proj.norm = collidewall;
                            inside = 0;
                            break;
                        }
                }
            }
            if(inside)
            {
                if(proj.projtype == PROJ_SHOT) proj.o = oldpos;

                if(collidemod&BOUNCE_PLAYER)
                {
                    collidemod &= ~STICK_GEOM;
                    collidemod |= IMPACT_GEOM|BOUNCE_GEOM;
                }
                else
                {
                    collidemod &= ~STICK_GEOM|BOUNCE_GEOM;
                    collidemod |= IMPACT_GEOM;
                }
            }
        }

        if((!d || dynent::is(d)) && (d ? collidemod&(d->type == ENT_PROJ ? COLLIDE_SHOTS : COLLIDE_PLAYER) : collidemod&COLLIDE_GEOM))
        {
            if(d)
            {
                if(inanimate::is(d)) return 0; // inanimates don't really work yet
                if(proj.norm.iszero()) proj.norm = vec(proj.o).sub(d->center()).safenormalize();
                if(proj.norm.iszero()) proj.norm = vec(proj.vel).add(proj.falling).safenormalize().neg();

                if(gameent::is(d) && collidemod&COLLIDE_PLAYER)
                {
                    gameent *f = (gameent *)d;

                    if(proj.projtype == PROJ_SHOT)
                    {
                        #define RESIDUAL(name, type, pulse) \
                            if(WF(WK(proj.flags), proj.weap, destroy##name, WS(proj.flags)) && f->name##func(lastmillis, f->name##time)) \
                                return 0;
                        RESIDUALS
                        #undef RESIDUAL
                    }

                    if(collidemod&STICK_PLAYER)
                    {
                        stick(proj, dir, f);
                        return 1; // keep living!
                    }
                }

                if(!hiteffect(proj, d, flags, proj.norm)) return 1;
            }
            else
            {
                if(proj.norm.iszero()) proj.norm = vec(proj.vel).add(proj.falling).safenormalize().neg();

                if(collidemod&IMPACT_GEOM && collidemod&STICK_GEOM)
                {
                    stick(proj, dir);
                    return 1;
                }

                if(collidemod&(IMPACT_GEOM|BOUNCE_GEOM) && collidemod&DRILL_GEOM)
                {
                    vec orig = proj.o;
                    loopi(WF(WK(proj.flags), proj.weap, drill, WS(proj.flags)))
                    {
                        proj.o.add(vec(dir).safenormalize());
                        if(!collide(&proj, dir, 0.f, collidemod&COLLIDE_DYNENT, false, GUARDRADIUS) && !collideinside && !collideplayer) return 1;
                    }
                    proj.o = orig; // continues below
                }
            }

            if(proj.projtype == PROJ_SHOT && (WF(WK(proj.flags), proj.weap, grab, WS(proj.flags))&(d ? 2 : 1)) && gameent::is(d)
                && (proj.owner == game::player1 || proj.owner->ai) && proj.owner->state == CS_ALIVE && (d || fabs(proj.norm.z) <= impulsewallrunnorm))
            {
                gameent *e = (gameent *)proj.owner;
                if(e->canimpulse(IM_T_GRAB))
                {
                    vec keepvel = vec(e->vel).add(e->falling);
                    int cost = d ? impulsecostgrabplayer : impulsecostgrab;
                    float mag = physics::impulsevelocity(e, d ? impulsegrabplayer : impulsegrab, cost, IM_T_GRAB, d ? impulsegrabplayerredir : impulsegrabredir, keepvel);
                    if(mag > 0)
                    {
                        float yaw = e->yaw, pitch = 89.9f;
                        switch(e == game::player1 ? (d ? physics::grabplayerstyle : physics::grabstyle) : (d ? 3 : 2))
                        {
                            case 0: pitch = e->pitch; break;
                            case 1: pitch = -e->pitch; break;
                            case 2: pitch = fabs(e->pitch); break;
                            case 3:
                                if(d)
                                {
                                    vec toward = vec(d->center()).sub(e->center()).safenormalize();
                                    vectoyawpitch(toward, yaw, pitch);
                                    break;
                                }
                            default: break;
                        }
                        e->vel = vec(yaw*RAD, pitch*RAD).mul(mag).add(keepvel);
                        e->doimpulse(IM_T_GRAB, lastmillis, cost);
                        client::addmsg(N_SPHY, "ri2", e->clientnum, SPHY_GRAB);
                        game::impulseeffect(e);
                    }
                }
            }

            bool ricochet = collidemod&(d && !inanimate::is(d) ? (d->type == ENT_PROJ ? BOUNCE_SHOTS : BOUNCE_PLAYER) : BOUNCE_GEOM);
            bounce(proj, ricochet);

            if(ricochet)
            {
                if(proj.projtype != PROJ_SHOT || proj.child || !(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH))
                {
                    reflect(proj, proj.norm);
                    proj.o.add(vec(proj.norm).mul(0.1f)); // offset from surface slightly to avoid initial collision
                    proj.from = proj.trailpos = proj.o;
                    proj.lastbounce = lastmillis;
                    proj.movement = 0;
                    proj.bounced = true;
                }
                return 2; // bounce
            }
            else if(collidemod&(d && !inanimate::is(d)  ? (d->type == ENT_PROJ ? IMPACT_SHOTS : IMPACT_PLAYER) : IMPACT_GEOM))
                return 0; // die on impact
        }

        return 1; // live!
    }

    int step(projent &proj, const vec &dir, const vec &pos, bool skip)
    {
        int ret = check(proj, dir);
        if(!ret) return 0;
        if(!skip && proj.interacts && checkitems(proj, pos, dir, proj.o.dist(pos))) return -1;
        if(proj.projtype == PROJ_SHOT) updatetaper(proj, proj.distance+proj.o.dist(pos));
        if(ret == 1 && (collide(&proj, dir, 0.f, proj.projcollide&COLLIDE_DYNENT, true, GUARDRADIUS) || collideinside))
            ret = impact(proj, dir, collideplayer, collidezones, collidewall, collideinside);
        return ret;
    }

    int trace(projent &proj, const vec &dir, const vec &pos, bool skip)
    {
        int ret = check(proj, dir);
        if(!ret) return 0;
        float total = 0, dist = -1;
        if(proj.projcollide&COLLIDE_SCAN)
        { // run a pre-scan
            if(!skip)
            {
                vec scan = vec(proj.o).sub(proj.from);
                float scandist = scan.magnitude();
                if(scandist > 0)
                {
                    scan.mul(1/scandist);
                    dist = tracecollide(&proj, proj.from, scan, scandist, RAY_CLIPMAT|RAY_ALPHAPOLY, proj.projcollide&COLLIDE_DYNENT, GUARDRADIUS);
                    if(dist >= 0)
                    {
                        proj.o = vec(proj.from).add(vec(scan).mul(dist));
                        total = 0-max(scandist-dist, 0.f);
                    }
                }
            }
            if(proj.stuck) return ret;
        }
        if(dist < 0)
        {
            vec ray = dir;
            float maxdist = ray.magnitude();
            if(maxdist <= 0) return ret;
            ray.mul(1/maxdist);
            if(!skip && proj.interacts && checkitems(proj, pos, ray, maxdist)) return -1;
            dist = tracecollide(&proj, pos, ray, maxdist, RAY_CLIPMAT|RAY_ALPHAPOLY, proj.projcollide&COLLIDE_DYNENT, GUARDRADIUS);
            total = dist >= 0 ? dist : maxdist;
            if(total > 0) proj.o.add(vec(ray).mul(total));
        }
        if(proj.projtype == PROJ_SHOT) updatetaper(proj, proj.distance+total);
        if(dist >= 0) ret = impact(proj, dir, collideplayer, collidezones, collidewall);
        return ret;
    }

    void escaped(projent &proj, const vec &pos, const vec &dir)
    {
        if(!proj.owner || !(proj.projcollide&COLLIDE_OWNER) || proj.lastbounce || !proj.spawntime) proj.escaped = true;
        else
        {
            int delay = proj.projtype == PROJ_SHOT ? W2(proj.weap, escapedelay, WS(proj.flags)) : PHYSMILLIS;
            if(lastmillis-proj.spawntime >= delay)
            {
                vec ray = vec(dir).safenormalize();
                if(proj.spawntime && lastmillis-proj.spawntime >= delay*2) proj.escaped = true;
                else if(proj.projcollide&COLLIDE_FROMTO)
                {
                    if(!pltracecollide(&proj, pos, ray, dir.magnitude(), GUARDRADIUS) || collideplayer != proj.owner) proj.escaped = true;
                }
                else if(!plcollide(&proj, ray, true, GUARDRADIUS) || collideplayer != proj.owner) proj.escaped = true;
            }
        }
    }

    bool moveproj(projent &proj, float secs, bool skip = false)
    {
        vec dir(proj.vel), pos(proj.o);
        if(isliquid(proj.inmaterial&MATF_VOLUME)) dir.mul(physics::liquidmerge(&proj, 1.f, LIQUIDPHYS(speed, proj.inmaterial)));
        if(WF(WK(proj.flags), proj.weap, guided, WS(proj.flags)) && lastmillis - proj.spawntime >= WF(WK(proj.flags), proj.weap, guideddelay, WS(proj.flags)))
            dir.mul(WF(WK(proj.flags), proj.weap, guidedspeed, WS(proj.flags)));
        dir.add(proj.falling);
        dir.mul(secs);

        if(!proj.escaped && proj.owner) escaped(proj, pos, dir);

        bool blocked = false;
        if(proj.projcollide&COLLIDE_FROMTO)
        {
            switch(trace(proj, dir, pos, skip))
            {
                case 2: blocked = true; break;
                case 1: break;
                case 0: return false;
                case -1: return moveproj(proj, secs, true);
            }
        }
        else
        {
            if(proj.projtype == PROJ_SHOT)
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
                        case 2:
                            proj.o = pos;
                            blocked = true;
                            break;
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
                    case 2:
                        proj.o = pos;
                        if(proj.projtype == PROJ_SHOT) blocked = true;
                        break;
                    case 1: default: break;
                    case 0:
                        proj.o = pos;
                        if(proj.projtype == PROJ_SHOT)
                        {
                            dir.rescale(max(dir.magnitude()-0.15f, 0.0f));
                            proj.o.add(dir);
                        }
                        return false;
                    case -1: return moveproj(proj, secs, true);
                }
            }
        }

        float dist = proj.o.dist(pos), diff = dist/float(4*RAD);
        vec vel = vec(proj.vel).add(proj.falling);
        if(!blocked) proj.movement += dist;
        proj.distance += dist;
        switch(proj.projtype)
        {
            case PROJ_SHOT:
            {
                if(proj.stuck) break;
                if(proj.weap == W_MINE)
                {
                    if(!proj.lastbounce || proj.movement > 0)
                    {
                        vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                        if(vel.dot2(axis) >= 0)
                        {
                            proj.pitch -= diff;
                            if(proj.pitch < -180) proj.pitch = 180 - fmod(180 - proj.pitch, 360);
                        }
                        else
                        {
                            proj.pitch += diff;
                            if(proj.pitch > 180) proj.pitch = fmod(proj.pitch + 180, 360) - 180;
                        }
                        break;
                    }
                    if(proj.pitch != 0)
                    {
                        if(proj.pitch < 0)
                        {
                            proj.pitch += max(diff, !proj.lastbounce || proj.movement > 0 ? 1.f : 5.f);
                            if(proj.pitch > 0) proj.pitch = 0;
                        }
                        else if(proj.pitch > 0)
                        {
                            proj.pitch -= max(diff, !proj.lastbounce || proj.movement > 0 ? 1.f : 5.f);
                            if(proj.pitch < 0) proj.pitch = 0;
                        }
                    }
                    break;
                }
                if(proj.weap != W_GRENADE)
                {
                    if(proj.mdlname && *proj.mdlname)
                        vectoyawpitch(vec(vel).safenormalize(), proj.yaw, proj.pitch);
                    break;
                }
            }
            case PROJ_DEBRIS: case PROJ_GIB: case PROJ_AFFINITY: case PROJ_VANITY: case PROJ_PIECE:
            {
                if(!proj.lastbounce || proj.movement > 0)
                {
                    float yaw = proj.yaw, pitch = proj.pitch, speed = diff*secs;
                    vectoyawpitch(vec(vel).safenormalize(), yaw, pitch);
                    yaw += 90; // rolling requires turning 90 degrees
                    game::scaleyawpitch(proj.yaw, proj.pitch, yaw, pitch, speed, speed);
                    proj.roll -= diff; // and the roll subtracts
                    if(proj.roll < -180) proj.roll = 180 - fmod(180 - proj.roll, 360);
                    break;
                }
            }
            case PROJ_EJECT:
                if(!proj.lastbounce || proj.movement > 0)
                {
                    vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                    if(vel.dot2(axis) >= 0)
                    {
                        proj.pitch -= diff;
                        if(proj.pitch < -180) proj.pitch = 180 - fmod(180 - proj.pitch, 360);
                    }
                    else
                    {
                        proj.pitch += diff;
                        if(proj.pitch > 180) proj.pitch = fmod(proj.pitch + 180, 360) - 180;
                    }
                    break;
                }
            case PROJ_ENTITY:
            {
                if(proj.pitch != 0)
                {
                    if(proj.pitch < 0)
                    {
                        proj.pitch += max(diff, !proj.lastbounce || proj.movement > 0 ? 1.f : 5.f);
                        if(proj.pitch > 0) proj.pitch = 0;
                    }
                    else if(proj.pitch > 0)
                    {
                        proj.pitch -= max(diff, !proj.lastbounce || proj.movement > 0 ? 1.f : 5.f);
                        if(proj.pitch < 0) proj.pitch = 0;
                    }
                }
                if(proj.roll != 0)
                {
                    if(proj.roll < 0)
                    {
                        proj.roll += max(diff, !proj.lastbounce || proj.movement > 0 ? 1.f : 5.f);
                        if(proj.roll > 0) proj.roll = 0;
                    }
                    else if(proj.roll > 0)
                    {
                        proj.roll -= max(diff, !proj.lastbounce || proj.movement > 0 ? 1.f : 5.f);
                        if(proj.roll < 0) proj.roll = 0;
                    }
                }
                break;
            }
            default: break;
        }
        return true;
    }

    bool move(projent &proj, int millis)
    {
        float secs = millis/1000.f;
        physics::updatematerial(&proj, proj.o, proj.feetpos(), true);

        if(proj.projtype == PROJ_ENTITY && proj.target && !proj.lastbounce)
        {
            vec targ = vec(proj.target->o).sub(proj.o).safenormalize();
            if(!targ.iszero())
            {
                vec dir = vec(proj.vel).safenormalize();
                float amt = clamp(itemspeeddelta*secs, FVAR_NONZERO, 1.f), mag = max(proj.vel.magnitude(), itemspeedmin);
                if(itemspeedmax > 0) mag = min(mag, itemspeedmax);
                dir.mul(1.f-amt).add(targ.mul(amt)).safenormalize();
                if(!dir.iszero()) (proj.vel = dir).mul(mag);
            }
        }
        else if(proj.projtype == PROJ_AFFINITY && m_bomber(game::gamemode) && proj.target && !proj.lastbounce)
        {
            vec targ = vec(proj.target->o).sub(proj.o).safenormalize();
            if(!targ.iszero())
            {
                vec dir = vec(proj.vel).safenormalize();
                float amt = clamp(bomberspeeddelta*secs, FVAR_NONZERO, 1.f), mag = max(proj.vel.magnitude(), bomberspeedmin);
                if(bomberspeedmax > 0) mag = min(mag, bomberspeedmax);
                dir.mul(1.f-amt).add(targ.mul(amt)).safenormalize();
                if(!dir.iszero()) (proj.vel = dir).mul(mag);
            }
        }
        else if(proj.projtype == PROJ_SHOT && proj.escaped)
        {
            if(WF(WK(proj.flags), proj.weap, guided, WS(proj.flags)) && lastmillis - proj.spawntime >= WF(WK(proj.flags), proj.weap, guideddelay, WS(proj.flags)))
            {
                vec dir = vec(proj.vel).safenormalize();
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
                                    safefindorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, dest);
                                    t = game::intersectclosest(proj.owner->o, dest, proj.owner);
                                    break;
                                } // otherwise..
                            }
                            case 4: case 5:
                            {
                                float yaw, pitch;
                                vectoyawpitch(dir, yaw, pitch);
                                vec dest;
                                safefindorientation(proj.o, yaw, pitch, dest);
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
                            safefindorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, proj.dest);
                        break;
                    }
                }

                if(!proj.dest.iszero())
                {
                    float amt = clamp(WF(WK(proj.flags), proj.weap, speeddelta, WS(proj.flags))*secs, FVAR_NONZERO, 1.f),
                        mag = max(proj.vel.magnitude(), physics::movevelocity(&proj));
                    dir.mul(1.f-amt).add(vec(proj.dest).sub(proj.o).safenormalize().mul(amt)).safenormalize();
                    if(!dir.iszero()) (proj.vel = dir).mul(mag);
                }
            }
        }

        if(proj.isjunk(m_messy(game::gamemode, game::mutators), true))
        {
            int numdyns = game::numdynents();
            loopj(numdyns)
            {
                dynent *f = game::iterdynents(j);
                if(!gameent::is(f) || !f->isalive()) continue;

                gameent *e = (gameent *)f;
                if(e->actortype != A_JANITOR) continue;

                vec ray = vec(e->muzzletag()).sub(proj.o);
                float dist = ray.magnitude();
                if(dist >= janitorsuckdist) continue;
                ray.safenormalize();

                if(dist <= f->radius + proj.radius + 1)
                {
                    if(e->collected(proj.projtype, proj.lifesize, proj.mdlname) && !e->hasprize)
                    {
                        e->hasprize = -1;
                        client::addmsg(N_SPHY, "ri2", e->clientnum, SPHY_PRIZE);
                    }
                    proj.beenused = 1;
                    proj.lifetime = min(proj.lifetime, proj.fadetime);
                }

                proj.dest = e->muzzletag();
                float amt = clamp(10*secs, FVAR_NONZERO, 1.f), mag = max(proj.vel.magnitude(), physics::movevelocity(&proj), janitorsuckspeed + dist * 2.f);
                vec dir = vec(proj.vel).safenormalize().mul(1.f-amt).add(vec(ray).mul(amt)).safenormalize();
                if(!dir.iszero()) (proj.vel = dir).mul(mag);
            }
        }

        bool inliquid = physics::liquidcheck(&proj);
        float coast = inliquid ? physics::liquidmerge(&proj, PHYS(aircoast), LIQUIDPHYS(coast, proj.inmaterial)) : PHYS(aircoast), speed = pow(max(1.0f - 1.0f/coast, 0.0f), millis/100.f);
        proj.vel.mul(speed);
        proj.falling.add(physics::gravityvel(&proj, proj.o, secs, proj.radius, proj.height, proj.inmaterial, proj.submerged));

        if(inliquid) proj.falling.mul(speed);

        return moveproj(proj, secs);
    }

    bool moveframe(projent &proj)
    {
        if(((proj.lifetime -= physics::physframetime) <= 0 && proj.lifemillis) || (!proj.usestuck() && !move(proj, physics::physframetime)))
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
        float scale = proj.radius;
        if(proj.owner) scale *= proj.owner->curscale;
        vec ray = vec(proj.dest).sub(proj.from).safenormalize().mul(scale);
        float maxdist = ray.magnitude();
        if(maxdist <= 0) return 1; // not moving anywhere, so assume still alive since it was already alive
        ray.mul(1/maxdist);
        float dist = tracecollide(&proj, proj.from, ray, maxdist, RAY_CLIPMAT|RAY_ALPHAPOLY, proj.projcollide&COLLIDE_DYNENT, GUARDRADIUS);
        if(dist >= 0)
        {
            vec dir = vec(proj.dest).sub(proj.from).safenormalize();
            proj.o = vec(proj.from).add(vec(dir).mul(dist));
            switch(impact(proj, dir, collideplayer, collidezones, collidewall))
            {
                case 1: case 2: return true;
                case 0: default: return false;
            }
        }
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
            if(a->p->projtype != PROJ_ENTITY && b->p->projtype == PROJ_ENTITY) return true;
            if(a->p->projtype == PROJ_ENTITY && b->p->projtype != PROJ_ENTITY) return false;
            if(a->p->addtime < b->p->addtime) return true;
            if(a->p->addtime > b->p->addtime) return false;
            if(a->p->lifesize < b->p->lifesize) return true;
            if(a->p->lifesize > b->p->lifesize) return false;
            if(a->dist > b->dist) return true;
            if(a->dist < b->dist) return false;
            return false;
        }
    };


    void update()
    {
        vector<canrem *> canremove;
        loopvrev(junkprojs) if(junkprojs[i]->isjunk()) canremove.add(new canrem(junkprojs[i], camera1->o.dist(junkprojs[i]->o)));

        int count = canremove.length() - maxprojectiles;
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

            entities::physents(&proj);

            if(proj.projtype == PROJ_SHOT && WF(WK(proj.flags), proj.weap, radialdelay, WS(proj.flags)))
            {
                proj.hit = NULL;
                proj.collidezones = CLZ_NONE;
            }
            hits.setsize(0);

            if((proj.projtype != PROJ_SHOT || proj.owner) && proj.state != CS_DEAD)
            {
                if(proj.projtype == PROJ_ENTITY && entities::ents.inrange(proj.id)) // in case spawnweapon changes
                    proj.mdlname = entities::entmdlname(entities::ents[proj.id]->type, entities::ents[proj.id]->attrs);

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

                if(proj.projtype == PROJ_SHOT || proj.projtype == PROJ_ENTITY || proj.projtype == PROJ_AFFINITY)
                {
                    if(proj.projtype == PROJ_SHOT && WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH ? !raymove(proj) : !move(proj)) switch(proj.projtype)
                    {
                        case PROJ_ENTITY: case PROJ_AFFINITY:
                        {
                            if(!proj.beenused)
                            {
                                proj.beenused = 1;
                                proj.lifetime = min(proj.lifetime, proj.fadetime);
                            }
                            if(proj.lifetime > 0) break;
                        }
                        default:
                        {
                            proj.state = CS_DEAD;
                            proj.escaped = true;
                            break;
                        }
                    }
                }
                else for(int rtime = curtime; proj.state != CS_DEAD && rtime > 0;)
                {
                    int qtime = min(rtime, 30);
                    rtime -= qtime;

                    if(((proj.lifetime -= qtime) <= 0 && proj.lifemillis) || (!proj.usestuck() && !move(proj, qtime)))
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

            if(proj.local && proj.owner && proj.projtype == PROJ_SHOT)
            {
                float expl = WX(WK(proj.flags), proj.weap, radial, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);

                if(WF(WK(proj.flags), proj.weap, collide, WS(proj.flags))&COLLIDE_LENGTH) proj.o = proj.dest;

                if(!proj.limited && proj.state != CS_DEAD)
                {
                    if(!(proj.projcollide&DRILL_PLAYER)) proj.hit = NULL;

                    bool radial = WF(WK(proj.flags), proj.weap, radialdelay, WS(proj.flags)) && expl > 0 && (!proj.lastradial || lastmillis-proj.lastradial >= WF(WK(proj.flags), proj.weap, radialdelay, WS(proj.flags)));
                    int proxim = proj.stuck && !proj.beenused ? WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) : 0;

                    if(radial || proxim)
                    {
                        float dist = WX(WK(proj.flags), proj.weap, proxdist, WS(proj.flags), game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        int stucktime = proj.stuck ? lastmillis - proj.stuck : 0, stuckdelay = WF(WK(proj.flags), proj.weap, proxdelay, WS(proj.flags));
                        if(stucktime && stuckdelay && stuckdelay > stucktime) dist *= stucktime/float(stuckdelay);

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

                        proj.stick = oldstick;

                        if(proxim == 2 && !proj.beenused)
                        {
                            float blocked = tracecollide(&proj, proj.o, proj.norm, dist, RAY_CLIPMAT|RAY_ALPHAPOLY, true, GUARDRADIUS);
                            if(blocked >= 0 && collideplayer && collideplayer->state == CS_ALIVE && physics::issolid(collideplayer, &proj, true, false))
                            {
                                proj.beenused = 1;
                                proj.lifetime = min(proj.lifetime, WF(WK(proj.flags), proj.weap, proxtime, WS(proj.flags)));
                            }
                        }
                    }
                }

                if(proj.state == CS_DEAD)
                {
                    if(!(proj.projcollide&DRILL_PLAYER)) proj.hit = NULL;

                    if(!proj.limited && expl > 0)
                    {
                        int numdyns = game::numdynents(1);
                        gameent *oldstick = proj.stick;
                        proj.stick = NULL;

                        loopj(numdyns)
                        {
                            dynent *f = game::iterdynents(j, 1);
                            if(!f || f->state != CS_ALIVE || !physics::issolid(f, &proj, false, false)) continue;
                            radialeffect(f, proj, HIT_EXPLODE, expl);
                        }

                        proj.stick = oldstick;
                    }
                }

                if(!hits.empty())
                    client::addmsg(N_DESTROY, "ri9i2v", proj.owner->clientnum, lastmillis-game::maptime, proj.projtype, proj.weap, proj.flags, proj.fromweap, proj.fromflags, WK(proj.flags) ? -proj.id : proj.id,
                            int(expl*DNF), int(proj.curscale*DNF), hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
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
        if(proj.projtype == PROJ_SHOT && proj.owner && physics::isghost(proj.owner, game::focus, true)) trans *= 0.5f;
        if(proj.fadetime && proj.lifemillis)
        {
            int interval = min(proj.lifemillis, proj.fadetime);
            if(proj.lifetime < interval)
            {
                float amt = float(proj.lifetime) / float(interval);
                size *= amt;
                trans *= amt;
            }
            else if(proj.projtype != PROJ_EJECT && proj.lifemillis > interval)
            {
                int offmillis = proj.lifemillis - proj.lifetime;
                interval = min(proj.lifemillis - interval, proj.fadetime);

                if(offmillis < interval)
                {
                    float amt = offmillis / float(interval);
                    size *= amt;
                    trans *= amt;
                }
            }
        }
    }

    void render()
    {
        int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators);
        loopv(projs) if(projs[i]->ready(false))
        {
            projent &proj = *projs[i];
            if(proj.projtype == PROJ_AFFINITY || (drawtex == DRAWTEX_HALO && proj.projtype != PROJ_ENTITY)) continue;
            if((proj.projtype == PROJ_ENTITY && !entities::ents.inrange(proj.id)) || !projs[i]->mdlname || !*projs[i]->mdlname) continue;

            const char *mdlname = proj.mdlname;
            modelstate mdl;

            mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
            mdl.flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_CULL_DIST;
            mdl.basetime = proj.spawntime;
            mdl.size = proj.curscale;
            mdl.yaw = proj.yaw;
            mdl.pitch = proj.pitch;
            mdl.roll = proj.roll;
            mdl.o = proj.o;
            mdl.material[0] = proj.material;

            switch(proj.projtype)
            {
                case PROJ_DEBRIS:
                {
                    mdl.size *= proj.lifesize;
                    fadeproj(proj, mdl.color.a, mdl.size);

                    if(mdl.color.a <= 0) continue;

                    break;
                }

                case PROJ_PIECE: case PROJ_VANITY:
                {
                    if(proj.owner) game::getplayermaterials(proj.owner, mdl);
                    // fall-through
                }

                case PROJ_GIB: case PROJ_EJECT:
                {
                    mdl.size *= proj.lifesize;
                    fadeproj(proj, mdl.color.a, mdl.size);

                    if(mdl.color.a <= 0) continue;

                    break;
                }

                case PROJ_SHOT:
                {
                    mdl.color.a *= fadeweap(proj);
                    if(mdl.color.a <= 0) continue;

                    mdl.material[0] = proj.owner ? bvec::fromcolor(game::getcolour(proj.owner, game::playerovertone, game::playerovertonelevel, game::playerovertonemix)) : bvec(128, 128, 128);
                    mdl.material[1] = proj.owner ? bvec::fromcolor(game::getcolour(proj.owner, game::playerundertone, game::playerundertonelevel, game::playerundertonemix)) : bvec(128, 128, 128);
                    mdl.material[2] = proj.owner ? bvec::fromcolor(game::getcolour(proj.owner, game::playerfxtone, game::playerfxtonelevel, game::playerfxtonemix)) : bvec(128, 128, 128);

                    if(!isweap(proj.weap) || (WF(WK(proj.flags), proj.weap, proxtype, WS(proj.flags)) && (!proj.stuck || proj.lifetime%500 >= 300))) mdl.material[3] = bvec(0, 0, 0);
                    else if(W2(proj.weap, colourproj, WS(proj.flags)) != 0)
                    {
                        float amt = clamp(proj.lifespan, 0.f, 1.f);
                        mdl.material[3] = bvec::fromcolor(vec::fromcolor(W(proj.weap, colour)).mul(1-amt).add(vec(WPCOL(&proj, proj.weap, colourproj, WS(proj.flags))).mul(amt)).clamp(0.f, 1.f));
                    }
                    else if(WF(WK(proj.flags), proj.weap, fxcol, WS(proj.flags))) mdl.material[3] = bvec::fromcolor(FWCOL(P, fxcol, proj));

                    break;
                }

                case PROJ_ENTITY:
                {
                    if(!entities::haloallow(camera1->o, proj.id)) continue;
                    if(!entities::ents.inrange(proj.id)) continue;

                    gameentity &e = *(gameentity *)entities::ents[proj.id];
                    mdlname = entities::entmdlname(e.type, e.attrs);
                    fadeproj(proj, mdl.color.a, mdl.size);

                    if(drawtex != DRAWTEX_HALO && entities::entityeffect && enttype[e.type].usetype == EU_ITEM)
                    {
                        int millis = proj.lifemillis - proj.lifetime, timeoffset = int(ceilf(entities::entityeffecttime * itemfadetime));
                        
                        if(millis < timeoffset)
                        {
                            int partoffset = timeoffset / 2;
                            float partamt = millis / float(partoffset);
                            
                            if(partamt >= 1.0f)
                            {
                                partamt = 2.0f - partamt;
                                mdl.effecttype = MDLFX_SHIMMER;
                            }
                            else mdl.effecttype = MDLFX_DISSOLVE;
                            
                            mdl.effectcolor = vec4(pulsehexcol(PULSE_HEALTH), entities::entityeffectblend);
                            mdl.effectparams = vec4(partamt, entities::entityeffectslice, entities::entityeffectfade / entities::entityeffectslice, entities::entityeffectbright);
                        }
                    }

                    int colour = 0;
                    if(e.type == WEAPON)
                    {
                        int attr = m_attr(e.type, e.attrs[0]);
                        if(isweap(attr))
                        {
                            colour = W(attr, colour);
                            mdl.effectcolor.mul(vec::fromcolor(colour));

                            if(game::focus->isobserver() || game::focus->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, W_S_ALL, !entities::showentfull))
                            {
                                if(drawtex == DRAWTEX_HALO && attr >= W_SUPER && attr < W_ALL)
                                    mdl.flags |= MDL_HALO_TOP;
                                mdl.color.a *= entities::showentavailable;
                            }
                            else mdl.color.a *= entities::showentunavailable;
                        }
                        else continue;
                    }
                    else continue;

                    loopk(MAXMDLMATERIALS) mdl.material[k] = bvec::fromcolor(colour);

                    break;
                }
                default: break;
            }

            game::haloadjust(mdl.o, mdl);
            rendermodel(mdlname, mdl, &proj);
        }
    }

    PRCOMMANDK(from, result(p->from), intret(-1));
    PRCOMMANDK(dest, result(p->dest), intret(-1));
    PRCOMMANDK(norm, result(p->norm), intret(-1));
    PRCOMMANDK(inertia, result(p->inertia), intret(-1));
    PRCOMMANDK(sticknrm, result(p->sticknrm), intret(-1));
    PRCOMMANDK(stickpos, result(p->stickpos), intret(-1));
    PRCOMMANDK(effectpos, result(p->effectpos), intret(-1));
    PRCOMMANDK(trailpos, result(p->trailpos), intret(-1));
    PRCOMMANDK(lastgood, result(p->lastgood), intret(-1));
    PRCOMMANDK(addtime, intret(p->addtime), intret(0));
    PRCOMMANDK(lifetime, intret(p->lifetime), intret(0));
    PRCOMMANDK(lifemillis, intret(p->lifemillis), intret(0));
    PRCOMMANDK(waittime, intret(p->waittime), intret(0));
    PRCOMMANDK(spawntime, intret(p->spawntime), intret(0));
    PRCOMMANDK(fadetime, intret(p->fadetime), intret(0));
    PRCOMMANDK(lastradial, intret(p->lastradial), intret(-1));
    PRCOMMANDK(lasteffect, intret(p->lasteffect), intret(-1));
    PRCOMMANDK(lastbounce, intret(p->lastbounce), intret(-1));
    PRCOMMANDK(beenused, intret(p->beenused), intret(0));
    PRCOMMANDK(extinguish, intret(p->extinguish), intret(0));
    PRCOMMANDK(stuck, intret(p->stuck), intret(-1));
    PRCOMMANDK(movement, floatret(p->movement), floatret(0));
    PRCOMMANDK(distance, floatret(p->distance), floatret(0));
    PRCOMMANDK(lifespan, floatret(p->lifespan), floatret(0));
    PRCOMMANDK(lifesize, floatret(p->lifesize), floatret(0));
    PRCOMMANDK(speedmin, floatret(p->speedmin), floatret(0));
    PRCOMMANDK(speedmax, floatret(p->speedmax), floatret(0));
    PRCOMMANDK(local, intret(p->local ? 1: 0), intret(0));
    PRCOMMANDK(limited, intret(p->limited ? 1: 0), intret(0));
    PRCOMMANDK(escaped, intret(p->escaped ? 1: 0), intret(0));
    PRCOMMANDK(child, intret(p->child ? 1: 0), intret(0));
    PRCOMMANDK(bounced, intret(p->bounced ? 1 : 0), intret(0));
    PRCOMMANDK(projtype, intret(p->projtype), intret(-1));
    PRCOMMANDK(projcollide, intret(p->projcollide), intret(0));
    PRCOMMANDK(interacts, intret(p->interacts), intret(0));
    PRCOMMANDK(elasticity, floatret(p->elasticity), intret(0));
    PRCOMMANDK(relativity, floatret(p->relativity), intret(0));
    PRCOMMANDK(liquidcoast, floatret(p->liquidcoast), intret(0));
    PRCOMMANDK(seqid, intret(p->seqid), intret(-1));
    PRCOMMANDK(schan, intret(p->schan), intret(-1));
    PRCOMMANDK(id, intret(p->id), intret(-1));
    PRCOMMANDK(weap, intret(p->weap), intret(-1));
    PRCOMMANDK(fromweap, intret(p->fromweap), intret(-1));
    PRCOMMANDK(fromflags, intret(p->fromflags), intret(0));
    PRCOMMANDK(value, intret(p->value), intret(-1));
    PRCOMMANDK(flags, intret(p->flags), intret(0));
    PRCOMMANDK(collidezones, intret(p->collidezones), intret(0));
    PRCOMMANDK(owner, intret(p->owner ? p->owner->clientnum : -1), intret(-1));
    PRCOMMANDK(target, intret(p->target ? p->target->clientnum : -1), intret(-1));
    PRCOMMANDK(stick, intret(p->stick ? p->stick->clientnum : -1), intret(-1));
    PRCOMMANDK(hit, intret(p->hit && gameent::is(p->hit) ? ((gameent *)p->hit)->clientnum : -1), intret(-1));
    PRCOMMAND(mdlname, result(p->mdlname));
    PRCOMMANDK(fxtype, intret(p->fxtype), intret(-1));
    PRCOMMANDMK(isjunk, "iii", (int *seqid, int *type, int *timespan), intret(p->isjunk(*timespan != 0)), intret(0));
    PRCOMMANDMK(ready, "iib", (int *seqid, int *type, int *used), intret(p->ready(*used != 0) ? 1 : 0), intret(0));
    PRCOMMANDMK(fade, "iibb", (int *seqid, int *type, int *used, int *test), floatret(p->getfade(*used != 0, *test != 0)), floatret(0));
    PRCOMMANDMK(fadedir, "iibb", (int *seqid, int *type, int *used, int *test), intret(p->getfadedir(*used != 0, *test != 0)), intret(0));
}
