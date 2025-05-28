#include "game.h"
namespace physics
{
    FVAR(IDF_MAP, stairheight, 0, 4.1f, 1000);
    FVAR(IDF_MAP, floorz, 0, 0.867f, 1);
    FVAR(IDF_MAP, slopez, 0, 0.5f, 1);
    FVAR(IDF_MAP, wallz, 0, 0.2f, 1);
    FVAR(IDF_MAP, stepspeed, 1e-4f, 1.f, 1000);

    #define MPVVARS(type) \
        FVAR(IDF_MAP, deathplane##type, 0, 0, 1);
    MPVVARS();
    MPVVARS(alt);

    #define GETMPV(name, type) \
        type get##name() \
        { \
            if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
            return name; \
        }
    GETMPV(deathplane, float);

    VAR(IDF_PERSIST, showphyslayers, 0, 1, 1);
    FVAR(IDF_PERSIST, physlayerscale, 0, 0.125f, 1);
    FVAR(IDF_PERSIST, physlayerblend, 0, 0.75f, 1);
    VAR(IDF_PERSIST, physlayersubdiv, 4, 4, 64);
    FVAR(IDF_PERSIST, physlayerfade, 0, 0.01f, 1);

    FVAR(IDF_PERSIST, floatspeed, FVAR_NONZERO, 200, FVAR_MAX);
    FVAR(IDF_PERSIST, floatcoast, 0, 3.f, FVAR_MAX);

    VAR(IDF_PERSIST, physframetime, 5, 5, 20);
    VAR(IDF_PERSIST, physinterp, 0, 1, 1);

    VAR(IDF_PERSIST, ragdollaccuracy, 0, 1, 1);
    FVAR(IDF_PERSIST, ragdollaccuracydist, 0, 1024, FVAR_MAX);
    FVAR(IDF_PERSIST, ragdollgravity, FVAR_MIN, 1.5f, FVAR_MAX);

    FVAR(IDF_PERSIST, impulsewallrunyaw, 0, 150, 180); // determines the minimum yaw angle to switch between parkour climb and run
    VAR(IDF_PERSIST, impulsemethod, 0, 3, 3); // determines which impulse method to use, 0 = none, 1 = launch, 2 = slide, 3 = both
    VAR(IDF_PERSIST, impulseaction, 0, 3, 3); // determines how impulse action works, 0 = off, 1 = impulse jump, 2 = impulse boost, 3 = both
    FVAR(IDF_PERSIST, impulseturnroll, 0, 15, 89);

    VAR(IDF_PERSIST, crouchstyle, 0, 0, 2); // 0 = press and hold, 1 = double-tap toggle, 2 = toggle
    VAR(IDF_PERSIST, walkstyle, 0, 0, 2); // 0 = press and hold, 1 = double-tap toggle, 2 = toggle
    VAR(IDF_PERSIST, grabstyle, 0, 2, 2); // 0 = up=up down=down, 1 = up=down down=up, 2 = up=up, down=up
    VAR(IDF_PERSIST, grabplayerstyle, 0, 3, 3); // 0 = up=up down=down, 1 = up=down down=up, 2 = up=up, down=up, 3 = directly toward player

    #define GETLIQUIDVARS(name) \
        GETMATIDXVAR(name, buoyancy, float) \
        GETMATIDXVAR(name, buoyancyscale, float) \
        GETMATIDXVAR(name, falldist, float) \
        GETMATIDXVAR(name, fallspeed, float) \
        GETMATIDXVAR(name, fallpush, float) \
        GETMATIDXVAR(name, speed, float) \
        GETMATIDXVAR(name, speedscale, float) \
        GETMATIDXVAR(name, coast, float) \
        GETMATIDXVAR(name, coastscale, float) \
        GETMATIDXVAR(name, boost, float) \
        GETMATIDXVAR(name, boostscale, float) \
        GETMATIDXVAR(name, submerge, float) \
        GETMATIDXVAR(name, submergescale, float)

    GETLIQUIDVARS(water)
    GETLIQUIDVARS(lava)
    GETMATIDXVAR(water, extinguish, float) \
    GETMATIDXVAR(water, extinguishscale, float)

    int physsteps = 0, lastphysframe = 0, lastmove = 0, lastdirmove = 0, laststrafe = 0, lastdirstrafe = 0, lastcrouch = 0, lastwalk = 0;

    #define imov(name,v,u,d,s,os) \
        void do##name(bool down) \
        { \
            game::player1->s = down; \
            int dir = game::player1->s ? d : (game::player1->os ? -(d) : 0); \
            game::player1->v = dir; \
            if(down) \
            { \
                last##v = lastmillis; \
                lastdir##v = dir; \
                last##u = lastdir##u = 0; \
            } \
        } \
        ICOMMAND(0, name, "D", (int *down), { do##name(*down!=0); });

    imov(backward, move,   strafe,  -1, k_down,  k_up);
    imov(forward,  move,   strafe,   1, k_up,    k_down);
    imov(left,     strafe, move,     1, k_left,  k_right);
    imov(right,    strafe, move,    -1, k_right, k_left);

    // inputs
    void doaction(int type, bool down)
    {
        if(type >= AC_MAX || type < 0) return;
        if(!game::allowmove(game::player1))
        {
            game::player1->action[type] = false;
            game::player1->actiontime[type] = 0;
            if((type == AC_PRIMARY || type == AC_JUMP) && down) game::respawn(game::player1);
            return;
        }

        int style = 0, *last = NULL;
        switch(type)
        {
            case AC_CROUCH: style = crouchstyle; last = &lastcrouch; break;
            case AC_WALK: style = walkstyle; last = &lastwalk; break;
            default: break;
        }
        if(last != NULL) switch(style)
        {
            case 1:
            {
                if(!down && game::player1->action[type])
                {
                    if(*last && lastmillis-*last < PHYSMILLIS) return;
                    *last = lastmillis;
                }
                break;
            }
            case 2:
            {
                if(!down)
                {
                    if(*last)
                    {
                        *last = 0;
                        return;
                    }
                    *last = lastmillis;
                }
                break;
            }
            default: break;
        }
        if(down) game::player1->actiontime[type] = lastmillis;
        else if(type == AC_CROUCH || type == AC_JUMP || type == AC_AFFINITY)
            game::player1->actiontime[type] = -lastmillis;
        game::player1->action[type] = down;
    }

    ICOMMAND(0, primary, "D", (int *n), doaction(AC_PRIMARY, *n!=0));
    ICOMMAND(0, secondary, "D", (int *n), doaction(AC_SECONDARY, *n!=0));
    ICOMMAND(0, reload, "D", (int *n), doaction(AC_RELOAD, *n!=0));
    ICOMMAND(0, use, "D", (int *n), doaction(AC_USE, *n!=0));
    ICOMMAND(0, jump, "D", (int *n), doaction(AC_JUMP, *n!=0));
    ICOMMAND(0, walk, "D", (int *n), doaction(AC_WALK, *n!=0));
    ICOMMAND(0, crouch, "D", (int *n), doaction(AC_CROUCH, *n!=0));
    ICOMMAND(0, special, "D", (int *n), doaction(AC_SPECIAL, *n!=0));
    ICOMMAND(0, drop, "D", (int *n), doaction(AC_DROP, *n!=0));
    ICOMMAND(0, affinity, "D", (int *n), doaction(AC_AFFINITY, *n!=0));

    int hasaffinity(gameent *d)
    {
        if(m_capture(game::gamemode)) return capture::hasaffinity(d);
        if(m_bomber(game::gamemode)) return bomber::hasaffinity(d);
        if(m_defend(game::gamemode)) return defend::hasaffinity(d);
        return 0;
    }
    CLCOMMAND(hasaffinity, intret(hasaffinity(d)));

    bool secondaryweap(gameent *d)
    {
        if(!isweap(d->weapselect)) return false;
        if(d->action[AC_SECONDARY] && (!d->action[AC_PRIMARY] || d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY])) return true;
        if(d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY] && d->weapstate[d->weapselect] == W_S_POWER) return true;
        return false;
    }

    bool isghost(gameent *d, gameent *e, bool proj)
    { // d is target, e is from
        if(!e || (!proj && d == e)) return false;
        if(!(A(d->actortype, abilities)&(1<<A_A_DAMAGE))) return true;
        if(d->actortype < A_ENEMY && e->actortype < A_ENEMY && m_ghost(game::gamemode, game::mutators)) return true;

        switch(d->actortype)
        {
            case A_JANITOR: if(janitorghost && !d->isprize(e)) return true; break;
            case A_PLAYER: if(!(A(e->actortype, collide)&(1<<A_C_PLAYERS))) return true; break;
            case A_BOT: if(!(A(e->actortype, collide)&(1<<A_C_BOTS))) return true; break;
            default: if(!(A(e->actortype, collide)&(1<<A_C_ENEMIES))) return true; break;
        }

        if(m_team(game::gamemode, game::mutators) && d->team == e->team && (proj || A(e->actortype, teamdamage)&(1<<A_T_GHOST)))
        {
            switch(d->actortype)
            {
                case A_PLAYER: if(!(A(e->actortype, teamdamage)&(1<<A_T_PLAYERS))) return true; break;
                case A_BOT: if(!(A(e->actortype, teamdamage)&(1<<A_T_BOTS))) return true; break;
                default: if(!(A(e->actortype, teamdamage)&(1<<A_T_ENEMIES))) return true; break;
            }
        }

        return false;
    }

    bool issolid(physent *d, physent *e, bool esc, bool impact, bool reverse)
    { // d is target, e is from
        if(!d || d == e) return false; // don't collide with themself
        
        if(inanimate::is(d))
        {
            inanimate *m = (inanimate *)d;
            
            if(e)
            {
                if(inanimate::is(e)) return false;
                if(m->coltarget && e == m->coltarget) return false;
            }
            
            return true;
        }
        
        if(e && inanimate::is(e))
        {
            inanimate *m = (inanimate *)e;
            
            if(inanimate::is(d)) return false;
            if(m->coltarget && d != m->coltarget) return false;
            
            return true;
        }
        
        if(e && projent::is(e))
        {
            projent *p = (projent *)e;
            
            if(gameent::is(d))
            {
                gameent *g = (gameent *)d;
                
                if(!g->isalive()) return false;
                if(g->protect(lastmillis, m_protect(game::gamemode, game::mutators))) return false;
                if(p->stick == d || isghost(g, p->owner, true)) return false;
                if(impact && (p->hit == g || !(p->projcollide&COLLIDE_PLAYER))) return false;
                if(p->owner == d && (!(p->projcollide&COLLIDE_OWNER) || (esc && !p->escaped))) return false;
            }
            else if(projent::is(d))
            {
                projent *q = (projent *)d;
                
                if(p->projtype == PROJ_SHOT && q->projtype == PROJ_SHOT)
                {
                    if((p->projcollide&IMPACT_SHOTS || p->projcollide&BOUNCE_SHOTS) && q->projcollide&COLLIDE_PROJ) return true;
                }
                
                return false;
            }
            else return false;
        }
        
        if(gameent::is(d))
        {
            gameent *g = (gameent *)d;
            
            if(!g->isalive()) return false;
            if(e && gameent::is(e) && isghost(g, (gameent *)e)) return false;
            
            return true;
        }
        else if(projent::is(d) && !reverse) return e ? issolid(e, d, esc, impact, true) : true;
        
        return false;
    }

    bool laddercheck(physent *d)
    {
        if(!gameent::is(d) || d->pitch < 0) return false;

        if(!isladder(d->inmaterial)) return false;

        int clipmat = d->inmaterial&MATF_CLIP;
        if(clipmat == MAT_CLIP || clipmat == MAT_NOCLIP) return false;

        return true;
    }

    bool liquidcheck(physent *d, float val)
    {
        return !laddercheck(d) && isliquid(d->inmaterial&MATF_VOLUME) && d->submerged >= (val >= 0.0f ? clamp(val, 0.0f, 1.0f) : LIQUIDPHYS(submerge, d->inmaterial));
    }

    float liquidmerge(physent *d, float from, float to)
    {
        if(isliquid(d->inmaterial&MATF_VOLUME))
        {
            if(d->physstate >= PHYS_SLIDE && d->submerged < 1.f)
                return from - ((from - to) * d->submerged);
            else return to;
        }

        return from;
    }

    vec gravityvel(physent *d, const vec &center, float secs, float radius, float height, int matid, float submerged)
    {
        bool floating = movepitch(d, true);
        float vel = floating ? 0.f : PHYS(gravity) * (d->weight / 100.f) * d->gravityscale, buoy = 0.f;
        bool liquid = isliquid(matid&MATF_VOLUME), inliquid = liquid && submerged >= LIQUIDPHYS(submerge, matid);

        if(inliquid) buoy = LIQUIDPHYS(buoyancy, matid) * (d->buoyancy / 100.f) * submerged;

        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(vel != 0)
            {
                if(d->state == CS_ALIVE && !inliquid)
                {
                    if(e->impulsetimer()) switch(e->impulse[IM_TYPE])
                    {
                        case IM_T_JUMP: vel *= impulsejumpgravity; break;
                        case IM_T_BOOST: vel *= impulseboostgravity; break;
                        case IM_T_DASH: vel *= impulsedashgravity; break;
                        case IM_T_SLIDE: vel *= impulseslidegravity; break;
                        case IM_T_LAUNCH: vel *= impulselaunchgravity; break;
                        case IM_T_MELEE: vel *= impulsemeleegravity; break;
                        case IM_T_KICK: vel *= impulsekickgravity; break;
                        case IM_T_GRAB: vel *= impulsegrabgravity; break;
                        case IM_T_WALLRUN: vel *= e->turnside ? impulsewallrungravity : impulseclimbgravity; break;
                        case IM_T_VAULT: vel *= impulsevaultgravity; break;
                        case IM_T_POUND: vel *= impulsepoundgravity; break;
                    }

                    if(e->crouching()) vel *= gravitycrouch;
                }
                vel *= e->stungravity;
            }

            if(buoy != 0)
            {
                if(d->state == CS_ALIVE && e->crouching()) buoy *= buoyancycrouch;
                buoy *= e->stungravity;
            }
        }

        vec g = vec(0, 0, -1).mul(vel).add(vec(0, 0, 1).mul(buoy));
        if(liquid)
        {
            vec f(0, 0, 0);
            int fall = 0;
            bool lava = (matid&MATF_VOLUME) == MAT_LAVA;
            float dist = LIQUIDVAR(falldist, matid);
            ivec bbrad = ivec(radius + dist, radius + dist, height + (dist * 0.5f)), bbmin = ivec(center).sub(bbrad), bbmax = ivec(center).add(bbrad);
            loopk(4)
            {
                vector<materialsurface> &surfs = lava ? lavafallsurfs[k] : waterfallsurfs[k];
                loopv(surfs)
                {
                    materialsurface &m = surfs[i];
                    int dim = dimension(m.orient);
                    int c = C[dim], r = R[dim];
                    if(m.o[dim] >= bbmin[dim] && m.o[dim] <= bbmax[dim] && m.o[c] + m.csize >= bbmin[c] && m.o[c] <= bbmax[c] && m.o[r] + m.rsize >= bbmin[r] && m.o[r] <= bbmax[r])
                    {
                        f[dim] -= 1 - dimcoord(m.orient) * 2;
                        fall++;
                    }
                }
            }

            if(fall) g.add(f.div(fall).mul(LIQUIDVAR(fallpush, matid)).subz(1).mul(LIQUIDVAR(fallspeed, matid)));
        }

        g.mul(secs);
        return g;
    }

    bool sticktofloor(physent *d)
    {
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(entities::currentpassenger(d)) return true;
            if(PHYS(gravity) != 0 && d->weight != 0 && !(A(e->actortype, abilities)&(1<<A_A_FLOAT))) return true;
        }

        return false;
    }

    bool sticktospecial(physent *d, bool parkour)
    {
        if(laddercheck(d) || (!liquidcheck(d) && entities::currentpassenger(d))) return true;

        if(parkour && gameent::is(d) && d->isalive() && ((gameent *)d)->impulsetimer(IM_T_WALLRUN) != 0) return true;

        return false;
    }

    bool isfloating(physent *d)
    {
        return d->type == ENT_CAMERA || (d->type == ENT_PLAYER && d->state == CS_EDITING);
    }

    float movevelocity(physent *d, bool floating)
    {
        physent *p = d->type == ENT_CAMERA ? game::player1 : d;
        float vel = p->speed * p->movescale * movespeed;

        if(floating) vel *= floatspeed/100.0f;
        else if(gameent::is(p))
        {
            gameent *e = (gameent *)p;
            vel *= e->stunscale;

            if((d->physstate >= PHYS_SLOPE || sticktospecial(d, false)) && !e->hasslide() && e->crouching()) vel *= movecrawl;
            else if(isweap(e->weapselect) && e->weapstate[e->weapselect] == W_S_ZOOM) vel *= movecrawl;

            if(e->impulsetimer()) switch(e->impulse[IM_TYPE])
            {
                case IM_T_JUMP: vel *= impulsejumpmove; break;
                case IM_T_BOOST: vel *= impulseboostmove; break;
                case IM_T_DASH: vel *= impulsedashmove; break;
                case IM_T_SLIDE: vel *= impulseslidemove; break;
                case IM_T_LAUNCH: vel *= impulselaunchmove; break;
                case IM_T_MELEE: vel *= impulsemeleemove; break;
                case IM_T_KICK: vel *= impulsekickmove; break;
                case IM_T_GRAB: vel *= impulsegrabmove; break;
                case IM_T_WALLRUN: vel *= e->turnside ? impulsewallrunmove : impulseclimbmove; break;
                case IM_T_VAULT: vel *= impulsevaultmove; break;
                case IM_T_POUND: vel *= impulsepoundmove; break;
            }

            if(e->move >= 0) vel *= e->strafe ? movestrafe : movestraight;
            if(e->running()) vel *= e->sprinting(false) ? movesprint : moverun;

            switch(e->physstate)
            {
                case PHYS_FALL: if(PHYS(gravity) > 0) vel *= moveinair; break;
                case PHYS_STEP_DOWN: vel *= movestepdown; break;
                case PHYS_STEP_UP: vel *= movestepup; break;
                default: break;
            }
        }
        return vel;
    }

    float impulsevelocity(physent *d, float amt, int &cost, int type, float redir, vec &keep)
    {
        float scale = liquidmerge(d, 1.f, LIQUIDPHYS(speed, d->inmaterial));

        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            scale *= e->stunscale;

            if(impulsecostmeter && cost)
            {
                if(impulsecostscale > 0) cost = int(cost*scale);
                int diff = impulsecostmeter - e->impulse[IM_METER];
                if(cost > diff)
                {
                    if(impulsecostrelax&(1<<type))
                    {
                        scale *= float(diff)/float(cost);
                        cost = diff;
                    }
                    else return 0.f;
                }
            }
            else cost = 0;
        }

        float speed = (d->impulsespeed * amt * scale) + (keep.magnitude() * redir);
        keep.mul(1.f - min(redir, 1.f));
        return speed;
    }

    bool movepitch(physent *d, bool onlyfloat)
    {
        if(d->type == ENT_DUMMY) return false;
        if(d->type == ENT_CAMERA || d->isnophys() || isfloating(d) || PHYS(gravity) == 0 || d->weight == 0) return true;
        if(d->isalive())
        {
            if(!onlyfloat && (laddercheck(d) || liquidcheck(d, 1.0f))) return true;
            if(gameent::is(d) && A(((gameent *)d)->actortype, abilities)&(1<<A_A_FLOAT)) return true;
        }
        return false;
    }

    void recalcdir(physent *d, const vec &oldvel, vec &dir)
    {
        float speed = oldvel.magnitude();
        if(speed > FVAR_NONZERO)
        {
            float step = dir.magnitude();
            dir = d->vel;
            dir.add(d->falling);
            dir.mul(step/speed);
        }
    }

    void slideagainst(physent *d, vec &dir, const vec &obstacle, bool foundfloor, bool slidecollide)
    {
        vec wall(obstacle);
        if(foundfloor ? wall.z > 0 : slidecollide)
        {
            wall.z = 0;
            if(!wall.iszero()) wall.normalize();
        }
        vec oldvel(d->vel);
        oldvel.add(d->falling);
        d->vel.project(wall);
        d->falling.project(wall);
        recalcdir(d, oldvel, dir);
    }

    void switchfloor(physent *d, vec &dir, const vec &floor)
    {
        if(floor.z >= floorz) d->falling = vec(0, 0, 0);

        vec oldvel(d->vel);
        oldvel.add(d->falling);
        if(dir.dot(floor) >= 0)
        {
            if(d->physstate < PHYS_SLIDE || fabs(dir.dot(d->floor)) > 0.01f*dir.magnitude()) return;
            d->vel.projectxy(floor, 0.0f);
        }
        else d->vel.projectxy(floor);
        d->falling.project(floor);
        recalcdir(d, oldvel, dir);
    }

    bool trystepup(physent *d, vec &dir, const vec &obstacle, float maxstep, const vec &floor)
    {
        vec old(d->o), stairdir = (obstacle.z >= 0 && obstacle.z < slopez ? vec(-obstacle.x, -obstacle.y, 0) : vec(dir.x, dir.y, 0)).rescale(1);
        bool cansmooth = true;
        /* check if there is space atop the stair to move to */
        if(d->physstate != PHYS_STEP_UP)
        {
            vec checkdir = stairdir;
            checkdir.mul(0.1f);
            checkdir.z += maxstep + 0.1f;
            d->o.add(checkdir);
            if(collide(d))
            {
                d->o = old;
                if(!collide(d, vec(0, 0, -1), slopez)) return false;
                cansmooth = false;
            }
        }

        if(cansmooth)
        {
            vec checkdir = stairdir;
            checkdir.z += 1;
            checkdir.mul(maxstep);
            d->o = old;
            d->o.add(checkdir);
            int scale = 2;
            if(collide(d, checkdir))
            {
                if(!collide(d, vec(0, 0, -1), slopez))
                {
                    d->o = old;
                    return false;
                }
                d->o.add(checkdir);
                if(collide(d, vec(0, 0, -1), slopez)) scale = 1;
            }
            if(scale != 1)
            {
                d->o = old;
                d->o.add(checkdir.mul2(2));
                if(!collide(d, vec(0, 0, -1), slopez)) scale = 1;
            }

            d->o = old;
            vec smoothdir = vec(dir.x, dir.y, 0);
            float magxy = smoothdir.magnitude();
            if(magxy > 1e-9f)
            {
                if(magxy > scale*dir.z)
                {
                    smoothdir.mul(1/magxy);
                    smoothdir.z = 1.0f/scale;
                    smoothdir.mul(dir.magnitude()/smoothdir.magnitude());
                }
                else smoothdir.z = dir.z;
                d->o.add(smoothdir.mul(stepspeed));
                float margin = (maxstep + 0.1f)*ceil(stepspeed);
                d->o.z += margin;
                if(!collide(d, smoothdir))
                {
                    d->o.z -= margin;
                    if(d->physstate == PHYS_FALL || d->floor != floor)
                    {
                        d->airmillis = 0;
                        d->floor = floor;
                        switchfloor(d, dir, d->floor);
                    }
                    d->physstate = PHYS_STEP_UP;
                    return true;
                }
            }
        }

        /* try stepping up */
        d->o = old;
        d->o.z += dir.magnitude()*stepspeed;
        if(!collide(d, vec(0, 0, 1)))
        {
            if(d->physstate == PHYS_FALL || d->floor != floor)
            {
                d->airmillis = 0;
                d->floor = floor;
                switchfloor(d, dir, d->floor);
            }
            if(cansmooth) d->physstate = PHYS_STEP_UP;
            return true;
        }
        d->o = old;
        return false;
    }

    bool trystepdown(physent *d, vec &dir, float step, float xy, float z, bool init = false)
    {
        vec stepdir(dir.x, dir.y, 0);
        stepdir.z = -stepdir.magnitude2()*z/xy;
        stepdir.safenormalize();
        if(!stepdir.z) return false;

        vec old(d->o);
        d->o.add(vec(stepdir).mul(stairheight/fabs(stepdir.z))).z -= stairheight;
        d->zmargin = -stairheight;
        if(collide(d, vec(0, 0, -1), slopez))
        {
            d->o = old;
            d->o.add(vec(stepdir).mul(step));
            d->zmargin = 0;
            if(!collide(d, vec(0, 0, -1)))
            {
                vec stepfloor(stepdir);
                stepfloor.mul(-stepfloor.z).z += 1;
                stepfloor.safenormalize();
                if(d->physstate >= PHYS_SLOPE && d->floor != stepfloor)
                {
                    // prevent alternating step-down/step-up states if player would keep bumping into the same floor
                    vec stepped(d->o);
                    d->o.z -= 0.5f;
                    d->zmargin = -0.5f;
                    if(collide(d, stepdir) && collidewall == d->floor)
                    {
                        d->o = old;
                        if(!init)
                        {
                            d->o.x += dir.x;
                            d->o.y += dir.y;
                            if(dir.z <= 0 || collide(d, dir))
                                d->o.z += dir.z;
                        }
                        d->zmargin = 0;
                        d->physstate = PHYS_STEP_DOWN;
                        return true;
                    }
                    d->o = init ? old : stepped;
                    d->zmargin = 0;
                }
                else if(init) d->o = old;
                switchfloor(d, dir, stepfloor);
                d->floor = stepfloor;
                if(init)
                {
                    d->airmillis = 0;
                    d->physstate = PHYS_STEP_DOWN;
                }
                return true;
            }
        }
        d->o = old;
        d->zmargin = 0;
        return false;
    }

    bool trystepdown(physent *d, vec &dir, bool init = false)
    {
        if(laddercheck(d) || !sticktofloor(d)) return false;

        vec old(d->o);
        d->o.z -= 0.1f;
        if(collide(d, vec(0, 0, -1)) || collideinside)
        {
            d->o = old;
            return false;
        }
        d->o.z -= stairheight;
        d->zmargin = -stairheight;

        if(!collide(d, vec(0, 0, -1), slopez))
        {
            d->o = old;
            d->zmargin = 0;
            return false;
        }
        d->o = old;
        d->zmargin = 0;

        float step = dir.magnitude();
        if(trystepdown(d, dir, step, 2, 1, init)) return true;
        if(trystepdown(d, dir, step, 1, 1, init)) return true;
        if(trystepdown(d, dir, step, 1, 2, init)) return true;

        return false;
    }

    void falling(physent *d, vec &dir, const vec &floor)
    {
        if(entities::currentpassenger(d))
        {
            d->floor = vec(0, 0, 1);
            d->airmillis = 0;
            d->physstate = PHYS_FLOOR;
            return;
        }

        if(floor.z > 0.0f && floor.z < slopez)
        {
            if(floor.z >= wallz) switchfloor(d, dir, floor);
            d->physstate = PHYS_SLIDE;
            d->floor = floor;
        }
        else if(d->physstate < PHYS_SLOPE || dir.dot(d->floor) > 0.01f*dir.magnitude() || (floor.z != 0.0f && floor.z != 1.0f) || !trystepdown(d, dir, true))
            d->physstate = PHYS_FALL;
    }

    void landing(physent *d, vec &dir, const vec &floor, bool collided)
    {
        switchfloor(d, dir, floor);

        if((d->physstate != PHYS_STEP_UP && d->physstate != PHYS_STEP_DOWN) || !collided)
            d->physstate = floor.z >= floorz ? PHYS_FLOOR : PHYS_SLOPE;

        if(d->physstate != PHYS_SLOPE) d->airmillis = 0;
        d->floor = floor;
    }

    bool findfloor(physent *d, bool collided, const vec &obstacle, bool &slide, vec &floor)
    {
        bool found = false;
        vec moved(d->o);
        d->o.z -= 0.1f;

        if(d->physstate != PHYS_FALL && collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE || d->physstate == PHYS_STEP_DOWN ? slopez : floorz))
        {
            floor = collidewall;
            found = true;
        }
        else if(collided && obstacle.z >= slopez)
        {
            floor = obstacle;
            found = true;
            slide = false;
        }
        else if(d->physstate == PHYS_STEP_UP || d->physstate == PHYS_SLIDE)
        {
            if(collide(d, vec(0, 0, -1)) && collidewall.z > 0.0f)
            {
                floor = collidewall;
                if(floor.z >= slopez) found = true;
            }
        }
        else if(d->physstate >= PHYS_SLOPE && d->floor.z < 1.0f)
        {
            if(collide(d, vec(d->floor).neg(), 0.95f) || !collide(d, vec(0, 0, -1)))
            {
                floor = collidewall;
                if(floor.z >= slopez && floor.z < 1.0f) found = true;
            }
        }

        if(collided && (!found || obstacle.z > floor.z))
        {
            floor = obstacle;
            slide = !found && (floor.z < wallz || floor.z >= slopez);
        }

        d->o = moved;

        return found;
    }

    bool move(physent *d, vec &dir)
    {
        vec old(d->o), obstacle(0, 0, 0);
        d->o.add(dir);

        bool collided = false, slidecollide = false;
        if(gameent::is(d))
        {
            if(collide(d, dir))
            {
                obstacle = collidewall;
                /* check to see if there is an obstacle that would prevent this one from being used as a floor */
                if(((collidewall.z >= slopez && dir.z < 0) || (collidewall.z <= -slopez && dir.z > 0)) && (dir.x || dir.y) && collide(d, vec(dir.x, dir.y, 0)))
                {
                    if(collidewall.dot(dir) >= 0) slidecollide = true;
                    obstacle = collidewall;
                }

                d->o = old;
                d->o.z -= stairheight;
                d->zmargin = -stairheight;
                if(d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR  || (collide(d, vec(0, 0, -1), slopez) && (d->physstate == PHYS_STEP_UP || d->physstate == PHYS_STEP_DOWN || collidewall.z >= floorz)))
                {
                    d->o = old;
                    d->zmargin = 0;
                    if(trystepup(d, dir, obstacle, stairheight, d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR ? d->floor : vec(collidewall)))
                        return true;
                }
                else
                {
                    d->o = old;
                    d->zmargin = 0;
                }
                collided = true; // can't step over the obstacle, so just slide against it
            }
            else if(d->physstate == PHYS_STEP_UP)
            {
                if(collide(d, vec(0, 0, -1), slopez))
                {
                    d->o = old;
                    if(trystepup(d, dir, vec(0, 0, 1), stairheight, vec(collidewall))) return true;
                    d->o.add(dir);
                }
            }
            else if(d->physstate == PHYS_STEP_DOWN && dir.dot(d->floor) <= FVAR_NONZERO)
            {
                vec moved(d->o);
                d->o = old;
                if(trystepdown(d, dir)) return true;
                d->o = moved;
            }
        }
        else if(collide(d, dir))
        {
            obstacle = collidewall;
            d->o = old;
            collided = true;
        }

        vec floor(0, 0, 0);
        bool slide = collided, found = findfloor(d, collided, obstacle, slide, floor);
        if(slide || (!collided && floor.z > 0 && floor.z < wallz))
        {
            slideagainst(d, dir, slide ? obstacle : floor, found, slidecollide);
            d->blocked = true;
        }

        if(found) landing(d, dir, floor, collided);
        else falling(d, dir, floor);

        return !collided;
    }

    bool impulseplayer(gameent *d, bool onfloor, const vec &inertia, bool melee = false, bool slide = false)
    {
        bool floating = movepitch(d, true),
             launch = !melee && !slide && onfloor && d->action[AC_JUMP] && !floating && impulsemethod&1 && d->impulsetimer(IM_T_SLIDE) != 0,
             mchk = !melee || onfloor, action = mchk && (d->actortype >= A_BOT || melee || impulseaction&2),
             dash = d->action[AC_SPECIAL] && onfloor, sliding = slide || (dash && d->move == 1 && d->crouching());
        int move = action ? d->move : 0, strafe = action ? d->strafe : 0;
        bool moving = mchk && (move || strafe), pound = !melee && !launch && !sliding && !onfloor && (impulsepoundstyle || !moving) && d->action[AC_CROUCH] && !floating;
        if(d->actortype < A_BOT && !launch && !melee && !sliding && !impulseaction && !dash) return false;

        bool pulse = melee ? !onfloor : dash || (!launch && !onfloor && (d->actortype >= A_BOT || impulseaction&1) && d->action[AC_JUMP] && !floating);
        int type = melee ? IM_T_MELEE : (sliding ? IM_T_SLIDE : (launch ? IM_T_LAUNCH : (pound ? IM_T_POUND : (dash ? IM_T_DASH : IM_T_BOOST))));
        if((!launch && !melee && !sliding && !pulse) || !d->canimpulse(type)) return false;

        vec keepvel = inertia;
        int cost = melee ? impulsecostmelee : (sliding ? impulsecostslide : (launch ? impulsecostlaunch : (pound ? impulsecostpound : (dash ? impulsecostdash : impulsecostboost))));
        float skew = melee ? impulsemelee : (sliding ? impulseslide : (launch ? impulselaunch : (pound ? impulsepound : (dash ? (move < 0 ? impulsedashback : impulsedash) : (moving ? impulseboost : impulseboostup))))),
              redir = melee ? impulsemeleeredir : (sliding ? impulseslideredir : (launch ? impulselaunchredir : (pound ? impulsepoundredir : (dash ? impulsedashredir : (moving ? impulseboostredir : impulseboostupredir))))),
              force = impulsevelocity(d, skew, cost, type, redir, keepvel);
        if(force <= 0) return false;

        vec dir(0, 0, pound ? -1 : 1);
        if(!pound && (launch || sliding || moving || onfloor))
        {
            float yaw = d->yaw, pitch = moving && (launch || (pulse && !dash)) ? d->pitch : 0;
            if(launch) pitch = clamp(pitch, impulselaunchpitchmin, impulselaunchpitchmax);
            else if(moving && pulse && !dash) pitch = clamp(pitch, impulseboostpitchmin, impulseboostpitchmax);
            vecfromyawpitch(yaw, pitch, moving ? move : 1, strafe, dir);
            if(!launch && sliding && !d->floor.iszero() && !dir.iszero())
            {
                dir.project(d->floor).normalize();
                if(dir.z < 0) force += -dir.z*force;
            }
        }

        d->vel = vec(dir).mul(force).add(keepvel);
        if(launch && impulselaunchextra > 0) d->vel.z += force * impulselaunchextra;
        d->doimpulse(type, lastmillis, cost, 3);
        client::addmsg(N_SPHY, "ri2", d->clientnum, melee ? SPHY_MELEE : (sliding ? SPHY_SLIDE : (launch ? SPHY_LAUNCH : (pound ? SPHY_POUND: (dash ? SPHY_DASH : SPHY_BOOST)))));
        game::impulseeffect(d);

        return true;
    }

    bool checkvault(gameent *d)
    {
        bool found = false;
        if(impulsevaultstyle&(d->turnside ? 1 : 2))
        {
            vec dir(d->yaw * RAD, d->pitch * RAD);
            float mag = max(d->vel.magnitude() + d->falling.magnitude(), d->speed * impulsevaultminspeed);

            if(!d->turnside)
            {   // if we were climbing something then broke free, fling us off
                vec oldpos = d->o, olddir = dir;
                dir = vec(d->yaw * RAD, (impulsevaultpitchclamp && d->pitch >= impulsevaultpitch ? d->pitch : impulsevaultpitch) * RAD);

                float pitchgap = (89.9f - impulsevaultpitch) * 0.1f;
                loopj(11)
                {   // scan for obstacle and try to go up and over
                    if(!collide(d, dir) && !collideinside)
                    {
                        found = true;
                        break;
                    }
                    dir = vec(d->yaw * RAD, j * pitchgap * RAD);
                    d->o = oldpos;
                }
                if(!found) dir = olddir;
                d->o = oldpos;
            }

            d->vel = vec(dir).mul(mag);
        }

        d->doimpulse(IM_T_VAULT, lastmillis);
        client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_VAULT);

        return found;
    }

    void modifyinput(gameent *d, vec &m, bool wantsmove)
    {
        bool onfloor = !(A(d->actortype, abilities)&(1<<A_A_FLOAT)) && (d->physstate >= PHYS_SLOPE || sticktospecial(d, false) || liquidcheck(d)),
             canparkour = d->action[AC_SPECIAL] && d->canimpulse(IM_T_WALLRUN), haswallrun = d->impulse[IM_TYPE] == IM_T_WALLRUN,
             found = false;

        if(d->hasparkour() || canparkour)
        {   // are we doing some fancy foot work?
            vec oldpos = d->o, newpos = oldpos, dir;

            if(!d->turnside)
            {
                bool testcollide = collide(d);
                if(testcollide || collideinside)
                {   // test if we need to pop the player out first
                    dir = vec(d->vel).safenormalize();
                    if(!dir.iszero())
                    {
                        loopk(8)
                        {
                            d->o.sub(dir);
                            testcollide = collide(d);
                            if(testcollide || collideinside) continue;
                            d->o.sub(dir);
                            oldpos = d->o;
                            break;
                        }
                        d->o = oldpos;
                    }

                    if(testcollide || collideinside)
                    {
                        dir = vec(d->yaw * RAD, 0.0f).safenormalize();
                        loopk(8)
                        {
                            d->o.sub(dir);
                            testcollide = collide(d);
                            if(testcollide || collideinside) continue;
                            d->o.sub(dir);
                            oldpos = d->o;
                            break;
                        }
                        d->o = oldpos;
                    }
                }
            }

            const int movements[7][2] = { { 1, 0 }, { 1, 2 }, { 0, 2 }, { 1, -1 }, { 1, 1 }, { 0, -1 }, { 0, 1 } }; // fwd, fwd+strafe, strafe, fwd+left, fwd+right, left, right
            loopi(haswallrun ? 7 : 2) // we do these insane checks so that running along walls works regardless of direction faced
            {
                int move = movements[i][0], strafe = movements[i][1];
                if(strafe == 2) strafe = d->turnside ? d->turnside : d->strafe;
                if(!move && !strafe) continue;
                vecfromyawpitch(d->yaw, 0, move, strafe, dir);
                bool foundwall = false;

                loopk(4)
                {   // check ahead for collision with something else
                    bool collided = collide(d, dir);
                    if(collided && collideplayer && !inanimate::is(collideplayer)) break;

                    if(!collided || collidewall.iszero() || (collidematerial && (collidematerial&MATF_CLIP) == MAT_CLIP && collidematerial&MAT_LADDER))
                    {
                        d->o.add(dir);
                        continue;
                    }

                    foundwall = true;
                    break;
                }

                if(!foundwall)
                {
                    d->o = oldpos;
                    continue; // nothing, can't be running on it then
                }

                vec face = vec(collidewall).safenormalize();
                if(fabs(face.z) > impulsewallrunnorm) continue; // check for correct normal to initiate wall running

                if(!haswallrun)
                {   // check for minimum step height
                    d->o.add(vec(dir).mul(impulsewallrunstep * 0.5f).addz(impulsewallrunstep));

                    if(!collide(d, dir) && !collideinside)
                    {
                        d->o = oldpos;
                        continue;
                    }

                    d->o = oldpos;
                }

                float yaw = 0, pitch = 0;
                vectoyawpitch(face, yaw, pitch);

                float off = yaw - d->yaw;
                if(off > 180.0f) off -= 360.0f;
                else if(off < -180.0f) off += 360.0f;

                bool isclimb = fabs(off) >= impulsewallrunyaw;
                int side = isclimb ? 0 : (off < 0 ? -1 : 1); // something succeeded, let's do it

                switch(side)
                {
                    case 0: default:
                    {
                        pitch = min(90.0f - pitch, 89.9f); // go up
                        yaw += 180.0f; // and turn the vector around
                        break;
                    }
                    case 1:
                    {
                        yaw -= 90.0f; // keep the wall at our right side
                        pitch = 0.0f;
                        break;
                    }
                    case -1:
                    {
                        yaw += 90.0f; // keep the wall at our left side
                        pitch = 0.0f;
                        break;
                    }
                }

                if(yaw < 0.0f) yaw = 360.0f - fmodf(-yaw, 360.0f);
                else if(yaw >= 360.0f) yaw = fmodf(yaw, 360.0f);

                float offyaw = yaw - d->yaw, offpitch = pitch - d->pitch;
                if(offyaw > 180.0f) offyaw -= 360.0f;
                else if(offyaw < -180.0f) offyaw += 360.0f;

                vec rft = vec(yaw * RAD, pitch * RAD).safenormalize();

                if(!side)
                {   // work around conditions where up is slopey
                    bool testcollide = collide(d, rft);
                    if(testcollide || collideinside)
                    {   // we shouldn't be colliding with our reflection angle
                        loopk(8)
                        {
                            d->o.sub(dir);
                            testcollide = collide(d, rft);
                            if(testcollide || collideinside) continue;
                            if(!side) d->o.sub(dir);
                            oldpos = d->o;
                            break;
                        }
                        d->o = oldpos;

                        if(testcollide || collideinside)
                        {
                            vec back = vec(d->yaw * RAD, 0.0f);
                            loopk(8)
                            {
                                d->o.sub(back);
                                testcollide = collide(d, rft);
                                if(testcollide || collideinside) continue;
                                d->o.sub(back);
                                oldpos = d->o;
                                break;
                            }
                            d->o = oldpos;
                        }

                        if(!haswallrun && (testcollide || collideinside)) continue; // it'll just result in a wasted move otherwise
                    }

                    d->o.addz(impulsewallrunhead);
                    if(collide(d, vec(0, 0, 1)) || collideinside)
                    {   // check for minimum head room
                        d->o = oldpos;
                        continue;
                    }
                    d->o = oldpos;
                }

                int updateturn = 0;
                if(canparkour)
                {   // we're not already doing it, so let's do it
                    int cost = isclimb ? impulsecostclimb : impulsecostwallrun;
                    vec keepvel = vec(d->vel).add(d->falling);
                    float mag = impulsevelocity(d, isclimb ? impulseclimb : impulsewallrun, cost, IM_T_WALLRUN, isclimb ? impulseclimbredir : impulsewallrunredir, keepvel);

                    if(mag > 0) d->vel = vec(rft).mul(mag).add(keepvel);
                    else
                    {
                        d->vel = vec(rft).mul(d->vel.magnitude());
                        cost = 0;
                    }

                    d->doimpulse(IM_T_WALLRUN, lastmillis, cost, 3);
                    haswallrun = true;
                    updateturn = 7;

                    client::addmsg(N_SPHY, "ri3", d->clientnum, SPHY_WALLRUN, 1);
                    game::impulseeffect(d);
                }
                else if(d->turnside != side || !haswallrun)
                {
                    updateturn = 4;

                    if(!haswallrun)
                    {   // run-on from a dash
                        d->vel = vec(rft).mul(d->vel.magnitude());
                        client::addmsg(N_SPHY, "ri3", d->clientnum, SPHY_WALLRUN, 0);
                        d->doimpulse(IM_T_WALLRUN, d->impulsetimer(IM_T_DASH, false));
                        haswallrun = true;
                        updateturn |= 3;
                    }
                }

                if(updateturn)
                    d->updateturn(side,
                        (updateturn&1) != 0 ? (offyaw * impulseyawscale) : 0.0f, (updateturn&1) != 0 ? impulseyawtime : -1,
                        (updateturn&2) != 0 ? (offpitch * impulsepitchscale) : 0.0f, (updateturn&2) != 0 ? impulsepitchtime : -1,
                        (updateturn&4) != 0 ? (impulseturnroll * side * impulserollscale) - d->roll : 0.0f, (updateturn&4) != 0 ? impulserolltime : -1);

                m = rft; // re-project and override
                found = true;

                break;
            }

            d->o = found && oldpos != newpos ? oldpos : newpos;
        }

        if(haswallrun && (!found || d->impulsetimer(IM_T_WALLRUN) == 0))
        {
            checkvault(d);
            haswallrun = onfloor = false;
        }

        if(!haswallrun)
        {   // normal movement
            bool floortolerance = onfloor || d->airtime(lastmillis) <= A(d->actortype, airtolerance) ||
                (impulsedashtolerate && d->impulsetimer(IM_T_DASH, true, impulsedashtolerate == 2) != 0) || (impulsevaulttolerate && d->impulsetimer(IM_T_VAULT, true, impulsevaulttolerate == 2) != 0);
            if(!impulseplayer(d, floortolerance, vec(d->vel).add(d->falling)) && floortolerance && d->action[AC_JUMP] && d->canimpulse(IM_T_JUMP) && !movepitch(d, true))
            {   // can't impulse and on the 'floor', so jump

                int cost = impulsecostjump;
                vec keepvel = vec(d->vel).add(d->falling);
                float mag = impulsevelocity(d, impulsejump, cost, IM_T_JUMP, impulsejumpredir, keepvel);

                if(mag > 0) d->vel = vec(0, 0, 1).mul(mag).add(keepvel);
                else
                {
                    d->vel = vec(0, 0, 1).mul(d->vel.magnitude());
                    cost = 0;
                }

                d->doimpulse(IM_T_JUMP, lastmillis, cost, 1);
                client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_JUMP);
                onfloor = false;

                static fx::FxHandle fx = fx::getfxhandle("FX_PLAYER_JUMP");
                fx::createfx(fx).setentity(d).setcolor(bvec(game::getcolour(d)));
            }
        }
        else if(d->action[AC_JUMP] && d->canimpulse(IM_T_KICK))
        {   // check for kick off from a wall run
            int cost = impulsecostkick;
            vec keepvel = vec(d->vel).add(d->falling);
            float mag = impulsevelocity(d, impulsekick, cost, IM_T_KICK, impulsekickredir, keepvel);

            if(mag > 0)
            {
                vec rft;
                float pitch = clamp(d->pitch, impulsekickpitchmin, impulsekickpitchmax);
                vecfromyawpitch(d->yaw, pitch, d->move || d->strafe ? d->move : 1, d->strafe, rft);

                d->vel = vec(rft).mul(mag).add(keepvel);
                d->doimpulse(IM_T_KICK, lastmillis, cost, 3);

                client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_KICK);
                game::impulseeffect(d);
                onfloor = false;
            }
        }

        bool sliding = d->impulsetimer(IM_T_SLIDE) != 0, pounding = d->impulsetimer(IM_T_POUND) != 0, kicking = !onfloor && d->action[AC_SPECIAL];
        if((sliding || pounding || kicking) && d->canmelee(m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis, sliding))
        {   // we were doing something terrible with our feet, but now we're going to kick someone
            vec oldpos = d->o, dir(d->yaw * RAD, 0.f);
            loopi(2)
            {
                d->o.add(dir);
                bool collided = collide(d, dir, 0, true, true);
                d->o = oldpos;

                if(collided && collideplayer && gameent::is(collideplayer))
                {   // yeah someone got in the way of my foot
                    vec pos = collideplayer->headpos(); // aim for the head just for fun
                    if(weapons::doshot(d, pos, W_MELEE, true, sliding, 0, (gameent *)collideplayer))
                        impulseplayer(d, onfloor, vec(d->vel).add(d->falling), true); // nice hack bro
                    break;
                }

                if(sliding) break;

                dir = vec(0, 0, -1); // try straight down
            }
        }
    }

    float coastscale(const vec &o)
    {
        return lookupvslot(lookupcube(ivec(o)).texture[O_TOP], false).coastscale;
    }

    void modifyvelocity(physent *d, bool local, int millis)
    {
        vec m(0, 0, 0);
        bool wantsmove = game::allowmove(d) && (d->move || d->strafe), floating = isfloating(d), inliquid = liquidcheck(d),
             onfloor = !floating && !(gameent::is(d) && A(((gameent *)d)->actortype, abilities)&(1<<A_A_FLOAT)) && (sticktospecial(d, false) || d->physstate >= PHYS_SLOPE),
             slide = false, dopitch = movepitch(d);

        if(wantsmove) vecfromyawpitch(d->yaw, dopitch ? d->pitch : 0, d->move, d->strafe, m);

        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(dopitch)
            {
                if(e->action[AC_JUMP] || e->action[AC_CROUCH])
                    vecfromyawpitch(d->yaw, clamp(d->pitch + (e->action[AC_JUMP] ? 90.0f : -90.0f), -89.9f, 89.9f), 1, d->strafe, m);
            }
            else
            {
                if(local && game::allowmove(e)) modifyinput(e, m, wantsmove);

                if(wantsmove)
                {
                    if(!e->hasparkour())
                    {
                        if(laddercheck(d))
                        {
                            m.z = m.iszero() ? 1 : m.z + 1;
                            m.normalize();
                        }
                        else if(!sticktospecial(e) && e->physstate >= PHYS_SLOPE)
                        { // move up or down slopes in air but only move up slopes in liquid
                            float dz = -(m.x * e->floor.x + m.y * e->floor.y) / e->floor.z;
                            m.z = inliquid ? max(m.z, dz) : dz;
                            if(!m.iszero()) m.normalize();
                        }
                    }
                    slide = e->hasslide();
                }
            }
        }

        if(floating && d->physstate != PHYS_FLOAT)
        {
            d->physstate = PHYS_FLOAT;
            d->airmillis = d->floormillis = 0;
        }
        else if(onfloor)
        {
            d->airmillis = 0;
            if(!d->floormillis) d->floormillis = lastmillis ? lastmillis : 1;
        }
        else
        {
            if(!d->airmillis) d->airmillis = lastmillis ? lastmillis : 1;
            d->floormillis = 0;
        }

        m.mul(movevelocity(d, floating));

        float coast = PHYS(floorcoast);
        if(floating) coast = floatcoast;
        else
        {
            float c = onfloor ? (slide ? PHYS(slidecoast) : PHYS(floorcoast)) * coastscale(d->feetpos(-1)) : (slide ? PHYS(slidecoast) : PHYS(aircoast));
            coast = inliquid ? liquidmerge(d, c, LIQUIDPHYS(coast, d->inmaterial)) : c;
        }
        coast *= d->coastscale;

        d->vel.lerp(m, d->vel, pow(max(1.0f - 1.0f / coast, 0.0f), millis / 20.0f));

        bool floorchk = d->floor.z > 0 && d->floor.z < floorz;
        if(floating || (onfloor && !floorchk)) d->resetphys(false);
        else
        {
            vec g = gravityvel(d, d->center(), millis / 1000.f, d->getradius(), d->getheight(), d->inmaterial, d->submerged);
            if(d->physstate != PHYS_FALL && floorchk) g.project(d->floor);

            d->falling.add(g);

            if(inliquid || d->physstate >= PHYS_SLOPE || sticktospecial(d))
            {
                float scoast = (inliquid ? liquidmerge(d, PHYS(aircoast), LIQUIDPHYS(coast, d->inmaterial)) : PHYS(floorcoast) * coastscale(d->feetpos(-1))) * d->coastscale,
                        floordiff = floorz-slopez, c = inliquid || floordiff == 0 ? 1.0f : clamp((d->floor.z-slopez)/floordiff, 0.0f, 1.0f);

                d->falling.mul(pow(max(1.0f - c / scoast, 0.0f), millis / 20.0f));
            }
        }
    }

    int materialslice(const vec &start, float height, float &submerged)
    {
        int matid = MAT_AIR, iters = max(int(ceilf(height)), 4), liquid = 0;
        float frac = height/float(iters); // guard against rounding errors
        vec tmp = start;
        loopi(iters+1)
        {
            int chkmat = lookupmaterial(tmp), matvol = chkmat&MATF_VOLUME, matclip = chkmat&MATF_CLIP, matflags = chkmat&MATF_FLAGS;
            // give priority to the lowest of each the volume/clip materials as they can't be OR'd together
            if(matvol && !(matid&MATF_VOLUME)) matid |= (chkmat&MATF_INDEX)|matvol;
            if(matclip && !(matid&MATF_CLIP)) matid |= matclip;
            if(matflags) matid |= matflags;

            if(isliquid(matvol) && i) liquid++;
            tmp.z += frac;
        }
        submerged = liquid ? liquid/float(iters) : 0.f;
        return matid;
    }

    void updatematerial(physent *d, const vec &center, const vec &bottom, bool local)
    {
        float radius = center.z-bottom.z, submerged = d->submerged;
        int oldmatid = d->inmaterial, oldmat = oldmatid&MATF_VOLUME;
        d->inmaterial = materialslice(bottom, radius*2, d->submerged);

        int curmat = d->inmaterial&MATF_VOLUME;
        if(curmat != oldmat)
        {
            #define mattrig(mo,mcol,ms,mt,mz,mq,mp,mw) \
            { \
                int col = (int(mcol[2]*(mq)) + (int(mcol[1]*(mq)) << 8) + (int(mcol[0]*(mq)) << 16)); \
                regularshape(mp, mt, col, 21, 20, mz, mo, ms, 1, 10, 0, 20); \
                if((mw) >= 0) emitsoundpos(mw, mo);                    \
            }
            if(curmat == MAT_WATER || oldmat == MAT_WATER)
            {
                const bvec &watercol = getwatercolour((curmat == MAT_WATER ? d->inmaterial : oldmatid)&MATF_INDEX);
                mattrig(bottom, watercol, 0.5f, int(radius), PHYSMILLIS, 0.25f, PART_SPARK, curmat != MAT_WATER ? S_SPLASH2 : S_SPLASH1);
            }
            if(curmat == MAT_LAVA)
            {
                const bvec &lavacol = getlavacolour(d->inmaterial&MATF_INDEX);
                mattrig(bottom, lavacol, 2.f, int(radius), PHYSMILLIS*2, 1.f, PART_FIREBALL, S_BURNLAVA);
            }
        }

        if(gameent::is(d) && local && d->state == CS_ALIVE)
        {
            gameent *e = (gameent *)d;
            if(e->physstate < PHYS_SLIDE)
            {
                float boost = LIQUIDPHYS(boost, e->inmaterial);
                if(submerged >= boost && e->submerged < boost) e->vel.z = max(e->vel.z, A(e->actortype, liquidboost));
            }
            if(e->inmaterial != oldmatid || e->submerged != submerged) client::addmsg(N_SPHY, "ri3f", e->clientnum, SPHY_MATERIAL, e->inmaterial, e->submerged);
        }
    }

    void updateragdoll(dynent *d, int index, int count, const vec &pos, const vec &oldpos, float radius, bool collided, vec &dpos, int millis)
    {
        int matid = d->inmaterial;
        float submerged = d->submerged, secs = millis/1000.f;
        vec g(0, 0, 0);

        if(ragdollaccuracy && camera1->o.squaredist(d->center()) <= ragdollaccuracydist*ragdollaccuracydist)
        {
            float secs = millis/1000.f;
            matid = materialslice(vec(pos).subz(radius), radius*2, submerged);
            g = gravityvel(d, pos, secs*secs, radius, radius*2, matid, submerged);
        }
        else
        {
            if(!index) d->falling = gravityvel(d, d->center(), secs*secs, d->getradius(), d->getheight(), matid, submerged);
            g = d->falling;
        }
        if(ragdollgravity) g.mul(ragdollgravity);

        dpos = vec(pos).sub(oldpos).add(g);

        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(e->shocktime && e->shockfunc(lastmillis, e->shocktime)) dpos.add(vec(rnd(201)-100, rnd(201)-100, rnd(201)-100).normalize().mul(shocktwitchvel*secs));
        }

        float coast = (collided ? (
                isliquid(matid&MATF_VOLUME) ? PHYS(aircoast) - ((PHYS(aircoast) - LIQUIDPHYS(coast, matid)) * submerged) : PHYS(floorcoast) * coastscale(vec(pos).subz(radius + 1))
              ) : PHYS(aircoast)) * d->coastscale;

        dpos.mul(pow(max(1.0f - 1.0f/coast, 0.0f), millis/20.0f));
    }

    // main physics routine, moves an actor for a time step
    // moveres indicates the physics precision (which is lower for monsters and multiplayer prediction)
    // local is false for multiplayer prediction

    bool moveplayer(physent *d, int moveres, bool local, int millis)
    {
        bool floating = isfloating(d);
        float secs = millis/1000.f;

        d->blocked = false;
        if(!floating) updatematerial(d, d->center(), d->feetpos(), local); // no material in editing, camera updates itself
        modifyvelocity(d, local, millis);

        vec vel(d->vel);
        if(!floating && liquidcheck(d)) vel.mul(liquidmerge(d, 1.f, LIQUIDPHYS(speed, d->inmaterial)));
        vel.add(d->falling).mul(secs);

        if(floating) d->o.add(vel); // just apply velocity
        else // apply velocity with collision
        {
            vec prevel = vec(d->vel).add(d->falling);
            float mag = prevel.magnitude();
            int collisions = 0, timeinair = d->airtime(lastmillis);

            vel.mul(1.0f / moveres);

            loopi(moveres) if(!move(d, vel)) { if(++collisions < 5) i--; } // discrete steps collision detection & sliding

            if(gameent::is(d) && !d->airmillis)
            {
                gameent *e = (gameent *)d;
                if(timeinair && !sticktospecial(e))
                {
                    if(local && impulsemethod&2 && timeinair >= impulseslideinair && (e->move == 1 || e->strafe) && e->action[AC_CROUCH] && e->canimpulse(IM_T_SLIDE) && !movepitch(d, true))
                        impulseplayer(e, true, prevel, false, true);

                    if(timeinair >= PHYSMILLIS)
                    {
                        if(mag >= 20)
                        {
                            float gain = min(mag*1.25f, 1.f);
                            if(isliquid(e->inmaterial&MATF_VOLUME)) gain *= 0.5f;

                            emitsound(S_LAND, game::getplayersoundpos(e), e, NULL, 0, gain);
                        }
                        else game::footstep(e);
                    }
                    e->resetjump();
                }

                if(e->impulseready(IM_T_ALL, false, false) && e->impulseready(IM_T_ALL, false, true))
                {
                    if(e->impulse[IM_TYPE] == IM_T_WALLRUN) checkvault(e);
                    e->clearimpulse((1<<IM_TYPE), IM_T_ALL);
                }
            }
        }

        if(gameent::is(d))
        {
            if(d->state == CS_ALIVE) updatedynentcache(d);
            if(local)
            {
                gameent *e = (gameent *)d;
                if(e->state == CS_ALIVE && !floating)
                {
                    if(e->o.z <= getdeathplane()*worldsize)
                    {
                        game::suicide(e, HIT_LOST);
                        return false;
                    }

                    if(e->impulsetimer(IM_T_WALLRUN) != 0)
                    {
                        loopi(3)
                        {
                            if(e->turnmillis[i] <= 0 || e->turntime[i] <= 0) continue;

                            float amt = clamp(min(e->turnmillis[i], millis) / float(e->turntime[i]), 0.0f, 1.0f),
                                  inc = e->turnangle[i] * amt;

                            switch(i)
                            {
                                case 0: e->yaw += inc; break;
                                case 1: e->pitch += inc; break;
                                case 2: e->roll += inc; break;
                            }
                            if((e->turnmillis[i] -= millis) < 0) e->turnmillis[i] = 0;
                        }
                    }
                    else if(e->roll != 0.0f) adjustscaled(e->roll, impulseturnroll, impulserolltime * physsteps);
                }
                else e->roll = 0.0f;
            }
        }

        return true;
    }

    void interppos(physent *d)
    {
        d->o = d->newpos;
        d->o.z += d->height;

        int diff = lastphysframe - lastmillis;
        if(diff <= 0 || !physinterp) return;

        vec deltapos(d->deltapos);
        deltapos.mul(min(diff, physframetime)/float(physframetime));
        d->o.add(deltapos);
    }

    bool movecamera(physent *d, const vec &dir, float dist, float stepdist)
    {
        int steps = (int)ceil(dist/stepdist);
        if(steps <= 0) return true;

        vec m(dir);
        m.mul(dist/steps);
        loopi(steps)
        {
            vec oldpos(d->o);
            d->o.add(m);
            if(collide(d, vec(0, 0, 0), 0, false))
            {
                d->o = oldpos;
                return false;
            }
        }
        return true;
    }

    void move(physent *d, int moveres, bool local)
    {
        if(physsteps <= 0)
        {
            if(local) interppos(d);
            return;
        }

        if(local)
        {
            d->o = d->newpos;
            d->o.z += d->height;
        }
        loopi(physsteps-1) moveplayer(d, moveres, local, physframetime);
        if(local) d->deltapos = d->o;
        moveplayer(d, moveres, local, physframetime);
        if(local)
        {
            d->newpos = d->o;
            d->deltapos.sub(d->newpos);
            d->newpos.z -= d->height;
            interppos(d);
        }
    }

    void updatephysstate(physent *d)
    {
        if(d->physstate == PHYS_FALL && !laddercheck(d)) return;

        vec old(d->o);
        /* Attempt to reconstruct the floor state.
         * May be inaccurate since movement collisions are not considered.
         * If good floor is not found, just keep the old floor and hope it's correct enough.
         */
        bool foundfloor = false;
        switch(d->physstate)
        {
            case PHYS_SLOPE:
            case PHYS_FLOOR:
            case PHYS_STEP_DOWN:
                d->o.z -= 0.15f;
                if(collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE || d->physstate == PHYS_STEP_DOWN ? slopez : floorz))
                {
                    d->floor = collidewall;
                    foundfloor = true;
                }
                break;

            case PHYS_STEP_UP:
                d->o.z -= stairheight+0.15f;
                if(collide(d, vec(0, 0, -1), slopez))
                {
                    d->floor = collidewall;
                    foundfloor = true;
                }
                break;

            case PHYS_SLIDE:
                d->o.z -= 0.15f;
                if(collide(d, vec(0, 0, -1)) && collidewall.z < slopez)
                {
                    d->floor = collidewall;
                    foundfloor = true;
                }
                break;
            default: break;
        }

        if((d->physstate > PHYS_FALL && d->floor.z <= 0) || (laddercheck(d) && !foundfloor)) d->floor = vec(0, 0, 1);

        d->o = old;
    }

    bool hitzonecollide(gameent *e, const vec &o, const vec &ray, float &dist)
    {
        modelstate mdl;
        modelattach mdlattach[VANITYMAX + ATTACHMENTMAX];
        const char *mdlname = game::getplayerstate(e, mdl, 1, e->curscale, 0, mdlattach);
        if(!mdlname || !*mdlname) return false;
        dist = 1e16f;
        int zone = intersectmodel(mdlname, mdl, o, ray, dist, 0, e);
        switch(zone)
        {
            case -1: return false;
            case 0: collidezones = CLZ_HEAD; break;
            case 1: collidezones = CLZ_TORSO; break;
            default: collidezones = CLZ_LIMBS; break;
        }
        dist *= ray.magnitude();
        return true;
    }

    bool checkcollide(physent *d, const vec &dir, physent *o)
    {
        collidezones = CLZ_HEAD;
        if(!d || !projent::shot(d) || !gameent::is(o)) return true;
        gameent *e = (gameent *)o;
        if(!actors[e->actortype].collidezones) return true;
        collidezones = CLZ_NONE;
        if(actors[e->actortype].collidezones&CLZ_LIMBS && !d->o.reject(e->limbstag(), d->radius+max(e->limbsbox().x, e->limbsbox().y)) && ellipsecollide(d, dir, e->limbstag(), vec(0, 0, 0), e->yaw, e->limbsbox().x, e->limbsbox().y, e->limbsbox().z, e->limbsbox().z))
            collidezones |= CLZ_LIMBS;
        if(actors[e->actortype].collidezones&CLZ_TORSO && !d->o.reject(e->torsotag(), d->radius+max(e->torsobox().x, e->torsobox().y)) && ellipsecollide(d, dir, e->torsotag(), vec(0, 0, 0), e->yaw, e->torsobox().x, e->torsobox().y, e->torsobox().z, e->torsobox().z))
            collidezones |= CLZ_TORSO;
        if(actors[e->actortype].collidezones&CLZ_HEAD && !d->o.reject(e->headtag(), d->radius+max(e->headbox().x, e->headbox().y)) && ellipsecollide(d, dir, e->headtag(), vec(0, 0, 0), e->yaw, e->headbox().x, e->headbox().y, e->headbox().z, e->headbox().z))
            collidezones |= CLZ_HEAD;
        return collidezones != CLZ_NONE;
    }

    bool checktracecollide(physent *d, const vec &from, const vec &to, float &dist, physent *o, float x1, float x2, float y1, float y2)
    {
        collidezones = CLZ_HEAD;
        if(!d || !projent::shot(d) || !gameent::is(o)) return true;
        gameent *e = (gameent *)o;
        if(!actors[e->actortype].collidezones) return true;
        collidezones = CLZ_NONE;
        float bestdist = 1e16f;
        if(actors[e->actortype].collidezones&CLZ_LIMBS && e->limbstag().x+e->limbsbox().x >= x1 && e->limbstag().y+e->limbsbox().y >= y1 && e->limbstag().x-e->limbsbox().x <= x2 && e->limbstag().y-e->limbsbox().y <= y2)
        {
            vec bottom(e->limbstag()), top(e->limbstag());
            bottom.z -= e->limbsbox().z;
            top.z += e->limbsbox().z;
            float t = 1e16f;
            if(linecylinderintersect(from, to, bottom, top, max(e->limbsbox().x, e->limbsbox().y), t))
            {
                collidezones |= CLZ_LIMBS;
                bestdist = min(bestdist, t);
            }
        }
        if(actors[e->actortype].collidezones&CLZ_TORSO && e->torsotag().x+e->torsobox().x >= x1 && e->torsotag().y+e->torsobox().y >= y1 && e->torsotag().x-e->torsobox().x <= x2 && e->torsotag().y-e->torsobox().y <= y2)
        {
            vec bottom(e->torsotag()), top(e->torsotag());
            bottom.z -= e->torsobox().z;
            top.z += e->torsobox().z;
            float t = 1e16f;
            if(linecylinderintersect(from, to, bottom, top, max(e->torsobox().x, e->torsobox().y), t))
            {
                collidezones |= CLZ_TORSO;
                bestdist = min(bestdist, t);
            }
        }
        if(actors[e->actortype].collidezones&CLZ_HEAD && e->headtag().x+e->headbox().x >= x1 && e->headtag().y+e->headbox().y >= y1 && e->headtag().x-e->headbox().x <= x2 && e->headtag().y-e->headbox().y <= y2)
        {
            vec bottom(e->headtag()), top(e->headtag());
            bottom.z -= e->headbox().z;
            top.z += e->headbox().z;
            float t = 1e16f;
            if(linecylinderintersect(from, to, bottom, top, max(e->headbox().x, e->headbox().y), t))
            {
                collidezones |= CLZ_HEAD;
                bestdist = min(bestdist, t);
            }
        }
        if(collidezones == CLZ_NONE) return false;
        dist = bestdist*from.dist(to);
        return true;
    }

    void collided(physent *d, const vec &dir, physent *o, bool inside)
    {
        gameent *e = NULL;
        inanimate *m = NULL;

        if(gameent::is(d)) e = (gameent *)d;
        else if(inanimate::is(d)) m = (inanimate *)d;
        else return;

        if(gameent::is(o)) e = (gameent *)o;
        else if(inanimate::is(o)) m = (inanimate *)o;
        else return;

        if(!e || !m) return;

        if(m->coltype&(1<<INANIMATE_C_KILL)) game::suicide(e, HIT_TOUCH);
        if(m == o && !(m->coltype&(1<<INANIMATE_C_NOPASS)) && !inside && collidewall.z > 0) entities::localpassenger(m, e);
    }

    void renderboundboxes(physent *d, const vec &rad, const vec &o)
    {
        if(!gameent::is(d)) return;
        vec pos = vec(o).sub(GUARDRADIUS), radius = vec(rad).add(GUARDRADIUS*2);
        loopj(6) boxs(j, pos, radius);
        gameent *e = (gameent *)d;
        if(!actors[e->actortype].collidezones || (e == game::focus && !game::thirdpersonview())) return;
        vec headpos = vec(e->headtag()).sub(e->headbox()), headbox = vec(e->headbox()).mul(2),
            torsopos = vec(e->torsotag()).sub(e->torsobox()), torsobox = vec(e->torsobox()).mul(2),
            limbspos = vec(e->limbstag()).sub(e->limbsbox()), limbsbox = vec(e->limbsbox()).mul(2);
        loopj(6)
        {
            if(actors[e->actortype].collidezones&CLZ_LIMBS) boxs(j, limbspos, limbsbox);
            if(actors[e->actortype].collidezones&CLZ_TORSO) boxs(j, torsopos, torsobox);
            if(actors[e->actortype].collidezones&CLZ_HEAD) boxs(j, headpos, headbox);
        }
    }

    void drawenvlayers(bool skyplane, bool shadowpass)
    {
        if(skyplane || shadowpass || !showphyslayers || game::player1->state != CS_EDITING) return;
        drawenvlayer((blanktexture ? blanktexture : notexture), getdeathplane(), bvec::fromcolor(colourdarkgrey).mul(physlayerscale), physlayerblend, physlayersubdiv, physlayerfade);

    }

    bool entinmap(physent *d, bool avoidplayers)
    {
        if(d->state != CS_ALIVE)
        {
            d->resetinterp();
            return insideworld(d->o);
        }
        vec orig = d->o;
        float maxrad = max(d->radius, max(d->xradius, d->yradius))+1;
        #define doposchk \
            if(insideworld(d->o) && !collide(d, vec(0, 0, 0), 0, avoidplayers)) \
            { \
                d->resetinterp(); \
                return true; \
            } \
            else d->o = orig;
        #define inmapchk(x,y) \
            loopi(x) \
            { \
                int n = i+1; \
                y; \
                doposchk; \
            }
        doposchk;
        if(gameent::is(d)) loopk(18)
        {
            vec dir = vec(d->yaw*RAD, d->pitch*RAD).rotate_around_z(k*20.f*RAD);
            if(!dir.iszero()) inmapchk(100, d->o.add(vec(dir).mul(n/10.f+maxrad)));
        }
        if(!d->vel.iszero()) loopk(18)
        {
            vec dir = vec(d->vel).normalize().rotate_around_z(k*20.f*RAD);
            inmapchk(100, d->o.add(vec(dir).mul(n/10.f+maxrad)));
        }
        inmapchk(100, d->o.add(vec((rnd(21)-10)/10.f, (rnd(21)-10)/10.f, (rnd(21)-10)/10.f).normalize().mul(vec(n/10.f+maxrad, n/10.f+maxrad, n/25.f+maxrad))));
        inmapchk(20, d->o.z += (d->height+d->aboveeye)*n/10.f);
        d->o = orig;
        d->resetinterp();
        return false;
    }

    VAR(IDF_PERSIST, smoothmove, 0, 100, 200);
    VAR(IDF_PERSIST, smoothdist, 0, 64, 1024);

    void predictplayer(gameent *d, bool domove, int res = 0, bool local = false)
    {
        d->o = d->newpos;
        d->o.z += d->height;

        d->yaw = d->newyaw;
        d->pitch = d->newpitch;

        if(domove)
        {
            move(d, res, local);
            d->newpos = d->o;
            d->newpos.z -= d->height;
        }

        float k = 1.0f - float(lastmillis - d->smoothmillis)/float(smoothmove);
        if(k > 0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw < 0) d->yaw += 360;
            else if(d->yaw >= 360) d->yaw -= 360;
            d->pitch += d->deltapitch*k;
        }
    }

    void smoothplayer(gameent *d, int res, bool local)
    {
        if(d->state == CS_ALIVE || d->state == CS_EDITING)
        {
            if(smoothmove && d->smoothmillis > 0) predictplayer(d, true, res, local);
            else move(d, res, local);
        }
        else if(d->state == CS_DEAD || d->state == CS_WAITING)
        {
            if(d->ragdoll) moveragdoll(d);
            else if(lastmillis-d->lastpain < 2000) move(d, res, local);
        }
    }

    bool droptofloor(vec &o, int type, float radius, float height)
    {
        static struct dropent : physent
        {
            dropent()
            {
                physent::reset();
                radius = xradius = yradius = height = aboveeye = 1;
                type = ENT_DUMMY;
                vel = vec(0, 0, -1);
            }
        } d;
        d.o = o;
        d.type = type;
        if(!insideworld(d.o))
        {
            if(d.o.z < worldsize) return false;
            d.o.z = worldsize - 1e-3f;
            if(!insideworld(d.o)) return false;
        }
        vec v(0.0001f, 0.0001f, -1);
        v.normalize();
        if(raycube(d.o, v, worldsize) >= worldsize) return false;
        d.radius = d.xradius = d.yradius = radius;
        d.height = height;
        d.aboveeye = radius;
        if(!movecamera(&d, vec(0, 0, -1), worldsize, 1))
        {
            o = d.o;
            return true;
        }
        return false;
    }

    void reset()
    {
        lastphysframe = 0;
    }

    void update()
    {
        if(!lastphysframe) lastphysframe = lastmillis;
        int diff = lastmillis - lastphysframe;
        if(diff <= 0) physsteps = 0;
        else
        {
            physsteps = (diff + physframetime - 1)/physframetime;
            lastphysframe += physsteps * physframetime;
        }
        cleardynentcache();
    }
}
