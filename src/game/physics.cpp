#include "game.h"
namespace physics
{
    FVAR(IDF_WORLD, stairheight, 0, 4.1f, 1000);
    FVAR(IDF_WORLD, floorz, 0, 0.867f, 1);
    FVAR(IDF_WORLD, slopez, 0, 0.5f, 1);
    FVAR(IDF_WORLD, wallz, 0, 0.2f, 1);
    FVAR(IDF_WORLD, stepspeed, 1e-4f, 1.f, 1000);

    FVAR(IDF_PERSIST, floatspeed, FVAR_NONZERO, 200, FVAR_MAX);
    FVAR(IDF_PERSIST, floatcoast, 0, 3.f, FVAR_MAX);

    VAR(IDF_PERSIST, physframetime, 5, 5, 20);
    VAR(IDF_PERSIST, physinterp, 0, 1, 1);

    FVAR(IDF_PERSIST, impulsekick, 0, 150, 180); // determines the minimum yaw angle to switch between wall kick and run
    VAR(IDF_PERSIST, impulsemethod, 0, 3, 3); // determines which impulse method to use, 0 = none, 1 = power jump, 2 = power slide, 3 = both
    VAR(IDF_PERSIST, impulseaction, 0, 3, 3); // determines how impulse action works, 0 = off, 1 = impulse jump, 2 = impulse dash, 3 = both
    FVAR(IDF_PERSIST, impulseroll, 0, 15, 89);

    VAR(IDF_PERSIST, jumpstyle, 0, 1, 1); // 0 = unpressed on action, 1 = remains pressed
    VAR(IDF_PERSIST, dashstyle, 0, 1, 1); // 0 = only with impulse, 1 = double tap
    VAR(IDF_PERSIST, crouchstyle, 0, 0, 2); // 0 = press and hold, 1 = double-tap toggle, 2 = toggle
    VAR(IDF_PERSIST, walkstyle, 0, 0, 2); // 0 = press and hold, 1 = double-tap toggle, 2 = toggle
    VAR(IDF_PERSIST, kickoffstyle, 0, 1, 1); // 0 = old method 1 = new method
    FVAR(IDF_PERSIST, kickoffangle, 0, 60, 89);
    VAR(IDF_PERSIST, kickupstyle, 0, 1, 1); // 0 = old method 1 = new method
    FVAR(IDF_PERSIST, kickupangle, 0, 89, 89);

    int physsteps = 0, lastphysframe = 0, lastmove = 0, lastdirmove = 0, laststrafe = 0, lastdirstrafe = 0, lastcrouch = 0, lastwalk = 0;

    bool allowimpulse(physent *d, int type)
    {
        if(d && gameent::is(d))
            return (type ? impulseallowed&type : impulseallowed != 0) && (impulsestyle || PHYS(gravity) == 0);
        return false;
    }

    bool canimpulse(physent *d, int type, bool kick)
    {
        if(gameent::is(d) && allowimpulse(d, type))
        {
            gameent *e = (gameent *)d;
            if(!kick && impulsestyle == 1 && e->impulse[IM_TYPE] > IM_T_NONE && e->impulse[IM_TYPE] < IM_T_WALL) return false;
            int time = e->impulse[IM_TIME], delay = e->impulse[IM_TYPE] == IM_T_KICK || e->impulse[IM_TYPE] == IM_T_VAULT ? impulsekickdelay : impulseboostdelay;
            if(!time)
            {
                time = e->impulse[IM_JUMP];
                delay = impulsejumpdelay;
            }
            if(time && delay && lastmillis-time <= delay) return false;
            if(!m_freestyle(game::gamemode, game::mutators) && impulsestyle <= 2 && e->impulse[IM_COUNT] >= impulsecount) return false;
            return true;
        }
        return false;
    }

    #define imov(name,v,u,d,s,os) \
        void do##name(bool down) \
        { \
            game::player1->s = down; \
            int dir = game::player1->s ? d : (game::player1->os ? -(d) : 0); \
            game::player1->v = dir; \
            if(down) \
            { \
                if(allowimpulse(game::player1, IM_A_DASH) && dashstyle && impulseaction&2 && last##v && lastdir##v && dir == lastdir##v && lastmillis-last##v < PHYSMILLIS) \
                { \
                    game::player1->action[AC_DASH] = true; \
                    game::player1->actiontime[AC_DASH] = lastmillis; \
                } \
                last##v = lastmillis; lastdir##v = dir; \
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
        if(type < AC_TOTAL && type > -1)
        {
            if(game::allowmove(game::player1))
            {
                int style = 0, *last = NULL;
                switch(type)
                {
                    case AC_CROUCH: style = crouchstyle; last = &lastcrouch; break;
                    case AC_WALK: style = walkstyle; last = &lastwalk; break;
                    default: break;
                }
                switch(style)
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
                if(type == AC_CROUCH)
                {
                    if(game::player1->action[type] != down)
                    {
                        if(game::player1->actiontime[type] >= 0) game::player1->actiontime[type] = lastmillis-max(PHYSMILLIS-(lastmillis-game::player1->actiontime[type]), 0);
                        else if(down) game::player1->actiontime[type] = -game::player1->actiontime[type];
                    }
                }
                else if(down) game::player1->actiontime[type] = lastmillis;
                game::player1->action[type] = down;
            }
            else
            {
                game::player1->action[type] = false;
                if((type == AC_PRIMARY || type == AC_JUMP) && down) game::respawn(game::player1);
            }
        }
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

    bool carryaffinity(gameent *d)
    {
        if(m_capture(game::gamemode)) return capture::carryaffinity(d);
        if(m_bomber(game::gamemode)) return bomber::carryaffinity(d);
        return false;
    }

    bool dropaffinity(gameent *d)
    {
        if(m_capture(game::gamemode)) return capture::dropaffinity(d);
        if(m_bomber(game::gamemode)) return bomber::dropaffinity(d);
        return false;
    }

    bool secondaryweap(gameent *d, bool zoom)
    {
        if(!isweap(d->weapselect)) return false;
        if(d->weapselect != W_MELEE || (d->physstate == PHYS_FALL && !d->onladder))
        {
            if(d->action[AC_SECONDARY] && (!d->action[AC_PRIMARY] || d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY])) return true;
            else if(d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY] && d->weapstate[d->weapselect] == W_S_POWER) return true;
        }
        return false;
    }

    bool isghost(gameent *d, gameent *e)
    {
        if(d != e && d->actortype < A_ENEMY && (!e || e->actortype < A_ENEMY))
        {
            if(m_ghost(game::gamemode, game::mutators)) return true;
            if(m_team(game::gamemode, game::mutators)) switch(G(damageteam))
            {
                case 1: if(d->actortype > A_PLAYER || (e && e->actortype == A_PLAYER)) break;
                case 0: if(e && d->team == e->team) return true; break;
                case 2: default: break;
            }
        }
        return false;
    }

    bool issolid(physent *d, physent *e, bool esc, bool impact, bool reverse)
    {
        if(d == e) return false; // don't collide with themself
        if(e && e->type == ENT_PROJ && d->state == CS_ALIVE)
        {
            projent *p = (projent *)e;
            if(gameent::is(d))
            {
                if(p->stick == d || isghost((gameent *)d, p->owner)) return false;
                if(impact && (p->hit == d || !(p->projcollide&COLLIDE_PLAYER))) return false;
                if(p->owner == d && (!(p->projcollide&COLLIDE_OWNER) || (esc && !p->escaped))) return false;
            }
            else if(d->type == ENT_PROJ)
            {
                projent *q = (projent *)d;
                if(p->projtype == PRJ_SHOT && q->projtype == PRJ_SHOT)
                {
                    if((p->projcollide&IMPACT_SHOTS || p->projcollide&BOUNCE_SHOTS) && q->projcollide&COLLIDE_PROJ) return true;
                }
                return false;
            }
            else return false;
        }
        if(gameent::is(d))
        {
            if(d->state != CS_ALIVE) return false;
            if(isghost((gameent *)d, (gameent *)(e && gameent::is(e) ? e : NULL))) return false;
            if(((gameent *)d)->protect(lastmillis, m_protect(game::gamemode, game::mutators))) return false;
            return true;
        }
        else if(d->type == ENT_PROJ && e && !reverse) return issolid(e, d, esc, impact, true);
        return false;
    }

    bool liquidcheck(physent *d) { return d->inliquid && !d->onladder && d->submerged >= PHYS(liquidsubmerge); }

    float liquidmerge(physent *d, float from, float to)
    {
        if(d->inliquid)
        {
            if(d->physstate >= PHYS_SLIDE && d->submerged < 1.f)
                return from-((from-to)*d->submerged);
            else return to;
        }
        return from;
    }

    float jumpvel(physent *d, bool liquid)
    {
        float vel = jumpspeed*(liquid ? liquidmerge(d, 1.f, PHYS(liquidspeed)) : 1.f)*d->speedscale;
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            vel *= 1.f-clamp(e->stunned(lastmillis), 0.f, 1.f);
        }
        return vel;
    }

    float gravityvel(physent *d)
    {
        float vel = PHYS(gravity)*(d->weight/100.f);
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            vel *= 1.f-clamp(e->stunned(lastmillis, true), 0.f, 1.f);
        }
        return vel;
    }

    float stepvel(physent *d, bool up)
    {
        if(d->physstate > PHYS_FALL) return stepspeed;
        return 1.f;
    }

    bool sticktofloor(physent *d)
    {
        if(!d->onladder)
        {
            if(liquidcheck(d)) return false;
            if(gameent::is(d))
            {
                gameent *e = (gameent *)d;
                if(e->sliding()) return false;
            }
        }
        return true;
    }

    bool sticktospecial(physent *d)
    {
        if(gameent::is(d)) { if(((gameent *)d)->turnside) return true; }
        return false;
    }

    bool isfloating(physent *d)
    {
        return d->type == ENT_CAMERA || (d->type == ENT_PLAYER && d->state == CS_EDITING);
    }

    float movevelocity(physent *d, bool floating)
    {
        physent *pl = d->type == ENT_CAMERA ? game::player1 : d;
        float vel = pl->speed;
        if(floating) vel *= floatspeed/100.0f;
        else
        {
            vel *= pl->speedscale;
            if(gameent::is(pl))
            {
                gameent *e = (gameent *)pl;
                vel *= movespeed/100.f*(1.f-clamp(e->stunned(lastmillis), 0.f, 1.f));
                if((d->physstate >= PHYS_SLOPE || d->onladder) && !e->sliding(true) && e->crouching()) vel *= movecrawl;
                else if(isweap(e->weapselect) && e->weapstate[e->weapselect] == W_S_ZOOM) vel *= movecrawl;
                if(e->move >= 0) vel *= e->strafe ? movestrafe : movestraight;
                if(e->running()) vel *= moverun;
                switch(e->physstate)
                {
                    case PHYS_FALL: if(PHYS(gravity) > 0) vel *= moveinair; break;
                    case PHYS_STEP_DOWN: vel *= movestepdown; break;
                    case PHYS_STEP_UP: vel *= movestepup; break;
                    default: break;
                }
                if(carryaffinity(e))
                {
                    if(m_capture(game::gamemode)) vel *= capturecarryspeed;
                    else if(m_bomber(game::gamemode)) vel *= bombercarryspeed;
                }
            }
        }
        return vel;
    }

    float impulsevelocity(physent *d, float amt, int &cost, int type, float redir, vec &keep)
    {
        float scale = d->speedscale;
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            scale *= 1.f-clamp(e->stunned(lastmillis), 0.f, 1.f);
            if(carryaffinity(e))
            {
                if(m_capture(game::gamemode)) scale *= capturecarryspeed;
                else if(m_bomber(game::gamemode)) scale *= bombercarryspeed;
            }
            if(m_impulsemeter(game::gamemode, game::mutators))
            {
                if(impulsecostscale) cost = int(cost*scale);
                int diff = impulsemeter-e->impulse[IM_METER];
                if(cost > diff)
                {
                    if(impulsecostrelax&type)
                    {
                        scale *= float(diff)/float(cost);
                        cost = diff;
                    }
                    else return 0.f;
                }
            }
            else cost = 0;
        }
        float speed = (impulsespeed*amt*scale)+(keep.magnitude()*redir);
        keep.mul(1-min(redir, 1.f));
        if(impulselimit > 0) return min(speed, impulselimit*scale);
        return speed;
    }

    bool movepitch(physent *d)
    {
        if(d->type == ENT_CAMERA || d->state == CS_EDITING || d->state == CS_SPECTATOR) return true;
        if(d->onladder || (d->inliquid && (liquidcheck(d) || d->pitch < 0.f)) || PHYS(gravity) == 0) return true;
        return false;
    }

    void recalcdir(physent *d, const vec &oldvel, vec &dir)
    {
        float speed = oldvel.magnitude();
        if(speed > 1e-6f)
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
        float force = stepvel(d, true);
        bool cansmooth = true;
        d->o = old;
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
                d->o.sub(checkdir.mul(vec(2, 2, 1)));
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
                d->o.add(smoothdir.mul(force));
                float margin = (maxstep + 0.1f)*ceil(force);
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
        d->o.z += dir.magnitude()*force;
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
        if(!stepdir.z) return false;
        stepdir.normalize();

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
                stepfloor.normalize();
                if(d->physstate >= PHYS_SLOPE && d->floor != stepfloor)
                {
                    // prevent alternating step-down/step-up states if player would keep bumping into the same floor
                    vec stepped(d->o);
                    d->o.z -= 0.5f;
                    d->zmargin = -0.5f;
                    if(collide(d, stepdir) && collidewall == d->floor)
                    {
                        d->o = old;
                        if(!init) { d->o.x += dir.x; d->o.y += dir.y; if(dir.z <= 0 || collide(d, dir)) d->o.z += dir.z; }
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
        if(!sticktofloor(d)) return false;
        vec old(d->o);
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
        if(d->physstate >= PHYS_SLOPE && dir.dot(d->floor) <= 0.01f*dir.magnitude() && trystepdown(d, dir, true)) return;
        if(floor.z > 0.0f && floor.z < slopez)
        {
            if(floor.z >= wallz) switchfloor(d, dir, floor);
            d->airmillis = 0;
            d->physstate = PHYS_SLIDE;
            d->floor = floor;
        }
        else if(sticktospecial(d))
        {
            d->airmillis = 0;
            d->physstate = PHYS_FLOOR;
            d->floor = vec(0, 0, 1);
        }
        else d->physstate = PHYS_FALL;
    }

    void landing(physent *d, vec &dir, const vec &floor, bool collided)
    {
        switchfloor(d, dir, floor);
        d->airmillis = 0;
        if((d->physstate!=PHYS_STEP_UP && d->physstate!=PHYS_STEP_DOWN) || !collided)
            d->physstate = floor.z >= floorz ? PHYS_FLOOR : PHYS_SLOPE;
        d->floor = floor;
    }

    bool findfloor(physent *d, bool collided, const vec &obstacle, bool &slide, vec &floor)
    {
        bool found = false;
        vec moved(d->o);
        d->o.z -= 0.1f;
        if(sticktospecial(d))
        {
            floor = vec(0, 0, 1);
            found = true;
        }
        else if(d->physstate != PHYS_FALL && collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE || d->physstate == PHYS_STEP_DOWN ? slopez : floorz))
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
        vec old(d->o), obstacle; d->o.add(dir);
        bool collided = false, slidecollide = false;
        if(collide(d, dir))
        {
            obstacle = collidewall;
            /* check to see if there is an obstacle that would prevent this one from being used as a floor */
            if((gameent::is(d)) && ((collidewall.z>=slopez && dir.z<0) || (collidewall.z<=-slopez && dir.z>0)) && (dir.x || dir.y) && collide(d, vec(dir.x, dir.y, 0)))
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
        else if((gameent::is(d)) && d->physstate == PHYS_STEP_DOWN && dir.dot(d->floor) <= 1e-6f)
        {
            vec moved(d->o);
            d->o = old;
            if(trystepdown(d, dir)) return true;
            d->o = moved;
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

    bool impulseplayer(gameent *d, bool &onfloor, bool melee = false)
    {
        bool power = !melee && onfloor && impulsemethod&1 && d->sliding(true) && d->action[AC_JUMP];
        if(power || d->actortype >= A_BOT || impulseaction || melee)
        {
            bool dash = false, pulse = false;
            if(melee) { dash = onfloor; pulse = !onfloor; }
            else if(!power)
            {
                if(onfloor) dash = impulseaction&2 && d->action[AC_DASH] && (!d->impulse[IM_TIME] || lastmillis-d->impulse[IM_TIME] > impulsedashdelay);
                else pulse = ((d->actortype >= A_BOT || impulseaction&1) && d->action[AC_JUMP]) || ((d->actortype >= A_BOT || impulseaction&2) && d->action[AC_DASH]);
            }
            if(!canimpulse(d, dash ? IM_A_DASH : IM_A_BOOST, false)) return false;
            if(power || dash || pulse)
            {
                bool mchk = !melee || onfloor, action = mchk && (d->actortype >= A_BOT || melee || impulseaction&2);
                int move = action ? d->move : 0, strafe = action ? d->strafe : 0;
                bool moving = mchk && (move || strafe);
                float skew = power ? impulsepower : (moving ? impulseboost : impulsejump);
                if(onfloor)
                {
                    if(!power) skew = impulsedash;
                    if(!dash && !melee) d->impulse[IM_JUMP] = lastmillis;
                }
                int cost = impulsecost;
                vec keepvel = vec(d->vel).add(d->falling);
                float force = impulsevelocity(d, skew, cost, melee ? IM_A_PARKOUR : (dash ? IM_A_DASH : IM_A_BOOST), melee ? impulsemeleeredir : (dash ? impulsedashredir : (moving ? impulseboostredir : impulsejumpredir)), keepvel);
                if(force > 0)
                {
                    vec dir(0, 0, 1);
                    if(power || dash || moving || onfloor)
                    {
                        float yaw = d->yaw, pitch = moving && (power || pulse) ? d->pitch : 0;
                        vecfromyawpitch(yaw, pitch, moving ? move : 1, strafe, dir);
                        if(!power && dash && !d->floor.iszero() && !dir.iszero())
                        {
                            dir.project(d->floor).normalize();
                            if(dir.z < 0) force += -dir.z*force;
                        }
                    }
                    d->vel = vec(dir).mul(force).add(keepvel);
                    if(power) d->vel.z += jumpvel(d, true);
                    d->doimpulse(cost, melee ? IM_T_MELEE : (dash ? IM_T_DASH : IM_T_BOOST), lastmillis);
                    d->action[AC_JUMP] = false;
                    if(power || pulse) onfloor = false;
                    client::addmsg(N_SPHY, "ri2", d->clientnum, melee ? SPHY_MELEE : (dash ? SPHY_DASH : SPHY_BOOST));
                    game::impulseeffect(d);
                    return true;
                }
            }
        }
        return false;
    }

    void modifyinput(gameent *d, vec &m, bool wantsmove, int millis)
    {
        bool onfloor = d->physstate >= PHYS_SLOPE || d->onladder || liquidcheck(d);
        if(d->turnside && (!allowimpulse(d, IM_A_PARKOUR) || d->impulse[IM_TYPE] != IM_T_SKATE || (impulseskate && lastmillis-d->impulse[IM_TIME] > impulseskate) || d->vel.magnitude() <= 1))
        {
            d->turnside = 0;
            d->resetphys(true);
            onfloor = false;
        }
        if(d->turnside)
        {
            if(d->action[AC_JUMP] && canimpulse(d, IM_A_PARKOUR, true))
            {
                int cost = impulsecost;
                vec keepvel = vec(d->vel).add(d->falling);
                float mag = impulsevelocity(d, impulseparkourkick, cost, IM_A_PARKOUR, impulseparkourkickredir, keepvel);
                if(mag > 0)
                {
                    vec rft;
                    vecfromyawpitch(d->yaw, d->actortype >= A_BOT || !kickoffstyle ? kickoffangle : d->pitch, 1, 0, rft);
                    d->vel = vec(rft).mul(mag).add(keepvel);
                    d->doimpulse(cost, IM_T_KICK, lastmillis);
                    d->turnmillis = PHYSMILLIS;
                    d->turnside = 0; d->turnyaw = d->turnroll = 0;
                    d->action[AC_JUMP] = onfloor = false;
                    client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_KICK);
                    game::impulseeffect(d);
                    game::footstep(d);
                }
            }
        }
        else
        {
            impulseplayer(d, onfloor);
            if(onfloor && d->action[AC_JUMP])
            {
                float force = jumpvel(d, true);
                if(force > 0)
                {
                    d->vel.z += force;
                    if(d->inliquid)
                    {
                        float scale = liquidmerge(d, 1.f, PHYS(liquidspeed));
                        d->vel.x *= scale;
                        d->vel.y *= scale;
                    }
                    d->resetphys();
                    d->impulse[IM_JUMP] = lastmillis;
                    d->action[AC_JUMP] = onfloor = false;
                    client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_JUMP);
                    playsound(S_JUMP, d->o, d);
                    regularshape(PART_SMOKE, int(d->radius), 0x222222, 21, 20, 250, d->feetpos(), 1, 1, -10, 0, 10.f);
                }
            }
            if(d->hasmelee(lastmillis, true, d->sliding(true), onfloor))
            {
                vec oldpos = d->o, dir(d->yaw*RAD, 0.f);
                d->o.add(dir);
                bool collided = collide(d, dir);
                d->o = oldpos;
                if(collided && hitplayer && gameent::is(hitplayer))
                {
                    //d->action[AC_SPECIAL] = false;
                    d->resetjump();
                    impulseplayer(d, onfloor, true);
                    if(d->turnside)
                    {
                        d->turnmillis = PHYSMILLIS;
                        d->turnside = 0;
                        d->turnyaw = d->turnroll = 0;
                    }
                    loopv(projs::projs)
                    {
                        projent *p = projs::projs[i];
                        if(p->owner != d || !p->ready() || p->projtype != PRJ_SHOT || p->weap != W_MELEE || !WS(p->flags)) continue;
                        p->target = (gameent *)hitplayer;
                    }
                }
            }
        }
        bool found = false;
        if(d->turnside || d->action[AC_SPECIAL])
        {
            vec oldpos = d->o, dir;
            const int movements[6][2] = { { 2, 2 }, { 1, 2 }, { 1, -1 }, { 1, 1 }, { 0, 2 }, { -1, 2 } };
            loopi(d->turnside ? 6 : 4) // we do these insane checks so that running along walls works at all times
            {
                int move = movements[i][0], strafe = movements[i][1];
                if(move == 2) move = d->move > 0 ? d->move : 0;
                if(strafe == 2) strafe = d->turnside ? d->turnside : d->strafe;
                if(!move && !strafe) continue;
                vecfromyawpitch(d->yaw, 0, move, strafe, dir);
                d->o.add(dir);
                bool collided = collide(d);
                d->o = oldpos;
                if(!collided || hitplayer || collidewall.iszero()) continue;
                vec face = vec(collidewall).normalize();
                if(fabs(face.z) <= impulseparkournorm)
                {
                    bool cankick = d->action[AC_SPECIAL] && canimpulse(d, IM_A_PARKOUR, true), parkour = cankick && !onfloor && !d->onladder;
                    float yaw = 0, pitch = 0;
                    vectoyawpitch(face, yaw, pitch);
                    float off = yaw-d->yaw;
                    if(off > 180) off -= 360;
                    else if(off < -180) off += 360;
                    bool iskick = impulsekick > 0 && fabs(off) >= impulsekick, vault = false;
                    if(cankick && iskick)
                    {
                        float space = d->height+d->aboveeye, m = min(impulsevaultmin, impulsevaultmax), n = max(impulsevaultmin, impulsevaultmax);
                        d->o.add(dir);
                        if(onfloor)
                        {
                            d->o.z += space*m;
                            if(collide(d))
                            {
                                d->o.z += space*n-space*m;
                                if(!collide(d) || hitplayer) vault = true;
                            }
                        }
                        else
                        {
                            d->o.z += space*n;
                            if(!collide(d) || hitplayer) vault = true;
                        }
                        d->o = oldpos;
                    }
                    if(!d->turnside && (parkour || vault) && iskick)
                    {
                        int cost = impulsecost;
                        vec keepvel = vec(d->vel).add(d->falling);
                        float mag = impulsevelocity(d, vault ? impulseparkourvault : impulseparkourclimb, cost, IM_A_PARKOUR, vault ? impulseparkourvaultredir : impulseparkourclimbredir, keepvel);
                        if(mag > 0)
                        {
                            vec rft;
                            vecfromyawpitch(d->yaw, vault || d->actortype >= A_BOT || !kickupstyle ? kickupangle : fabs(d->pitch), 1, 0, rft);
                            rft.reflect(face);
                            d->vel = vec(rft).mul(mag).add(keepvel);
                            d->doimpulse(cost, vault ? IM_T_VAULT : IM_T_KICK, lastmillis);
                            d->turnmillis = PHYSMILLIS;
                            d->turnside = 0;
                            d->turnyaw = d->turnroll = 0;
                            client::addmsg(N_SPHY, "ri2", d->clientnum, vault ? SPHY_VAULT : SPHY_KICK);
                            game::impulseeffect(d);
                            game::footstep(d);
                        }
                        break;
                    }
                    else if(d->turnside || parkour)
                    {
                        int side = off < 0 ? -1 : 1;
                        if(off < 0) yaw += 90;
                        else yaw -= 90;
                        while(yaw >= 360) yaw -= 360;
                        while(yaw < 0) yaw += 360;
                        vec rft;
                        vecfromyawpitch(yaw, 0.f, 1, 0, rft);
                        if(!d->turnside)
                        {
                            int cost = impulsecost;
                            vec keepvel = vec(d->vel).add(d->falling);
                            float mag = impulsevelocity(d, impulseparkour, cost, IM_A_PARKOUR, impulseparkourredir, keepvel);
                            if(mag > 0)
                            {
                                d->vel = vec(rft).mul(mag).add(keepvel);
                                off = yaw-d->yaw;
                                if(off > 180) off -= 360;
                                else if(off < -180) off += 360;
                                d->doimpulse(cost, IM_T_SKATE, lastmillis);
                                d->turnmillis = PHYSMILLIS;
                                d->turnside = side;
                                d->turnyaw = off;
                                d->turnroll = (impulseroll*d->turnside)-d->roll;
                                client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_SKATE);
                                game::impulseeffect(d);
                                game::footstep(d);
                                found = true;
                            }
                            break;
                        }
                        else if(side == d->turnside)
                        {
                            (m = rft).normalize(); // re-project and override
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if(d->canmelee(m_weapon(game::gamemode, game::mutators), lastmillis, true, d->sliding(true), onfloor))
            weapons::doshot(d, d->o, W_MELEE, true, true);
        if(!found && d->turnside) d->turnside = 0;
        d->action[AC_DASH] = false;
    }

    void modifymovement(gameent *d, vec &m, bool local, bool wantsmove, int millis)
    {
        if(wantsmove && d->physstate >= PHYS_SLOPE)
        { // move up or down slopes in air but only move up slopes in liquid
            float dz = -(m.x*d->floor.x + m.y*d->floor.y)/d->floor.z;
            m.z = liquidcheck(d) ? max(m.z, dz) : dz;
            if(!m.iszero()) m.normalize();
        }
        if(!d->turnside && (d->physstate >= PHYS_SLOPE || d->onladder || liquidcheck(d))) d->resetjump();
        if(local && game::allowmove(d)) modifyinput(d, m, wantsmove, millis);
        if(d->physstate == PHYS_FALL && !d->onladder && !d->turnside)
        {
            if(!d->airmillis) d->airmillis = lastmillis ? lastmillis : 1;
            d->floormillis = 0;
        }
        else
        {
            d->airmillis = 0;
            if(!d->floormillis) d->floormillis = lastmillis ? lastmillis : 1;
        }
        if(!d->turnside && d->onladder && !m.iszero()) m.add(vec(0, 0, m.z >= 0 ? 1 : -1)).normalize();
    }

    float coastscale(const vec &o)
    {
        return lookupvslot(lookupcube(ivec(o)).texture[O_TOP], false).coastscale;
    }

    void modifyvelocity(physent *pl, bool local, bool floating, int millis)
    {
        vec m(0, 0, 0);
        bool wantsmove = game::allowmove(pl) && (pl->move || pl->strafe);
        if(wantsmove) vecfromyawpitch(pl->yaw, movepitch(pl) ? pl->pitch : 0, pl->move, pl->strafe, m);
        if(!floating && gameent::is(pl)) modifymovement((gameent *)pl, m, local, wantsmove, millis);
        else if(pl->physstate == PHYS_FALL && !pl->onladder)
        {
            if(!pl->airmillis) pl->airmillis = lastmillis ? lastmillis : 1;
            pl->floormillis = 0;
        }
        else
        {
            pl->airmillis = 0;
            if(!pl->floormillis) pl->floormillis = lastmillis ? lastmillis : 1;
        }

        m.mul(movevelocity(pl, floating));
        float coast = PHYS(floorcoast);
        if(floating || pl->type == ENT_CAMERA) coast = floatcoast;
        else
        {
            bool slide = gameent::is(pl) && ((gameent *)pl)->sliding();
            float c = pl->physstate >= PHYS_SLOPE || pl->onladder ? (slide ? PHYS(slidecoast) : PHYS(floorcoast))*coastscale(pl->feetpos(-1)) : PHYS(aircoast);
            coast = pl->inliquid ? liquidmerge(pl, c, PHYS(liquidcoast)) : c;
        }
        pl->vel.lerp(m, pl->vel, pow(max(1.0f - 1.0f/coast, 0.0f), millis/20.0f));
    }

    void modifygravity(physent *pl, int curtime)
    {
        if(PHYS(gravity) > 0)
        {
            vec g(0, 0, 0);
            float secs = curtime/1000.0f;
            if(pl->physstate == PHYS_FALL) g.z -= gravityvel(pl)*secs;
            else if(pl->floor.z > 0 && pl->floor.z < floorz)
            {
                g.z = -1;
                g.project(pl->floor);
                g.normalize();
                g.mul(gravityvel(pl)*secs);
            }
            bool liquid = liquidcheck(pl);
            if(!liquid || (!pl->move && !pl->strafe) || (gameent::is(pl) && ((gameent *)pl)->crouching()))
                pl->falling.add(g);
            if(liquid || pl->physstate >= PHYS_SLOPE)
            {
                float coast = liquid ? liquidmerge(pl, PHYS(aircoast), PHYS(liquidcoast)) : PHYS(floorcoast)*coastscale(pl->feetpos(-1)),
                      c = liquid ? 1.0f : clamp((pl->floor.z - slopez)/(floorz-slopez), 0.0f, 1.0f);
                pl->falling.mul(pow(max(1.0f - c/coast, 0.0f), curtime/20.0f));
            }
        }
        else pl->falling = vec(0, 0, 0);
    }

    void updatematerial(physent *pl, const vec &center, const vec &bottom, bool local)
    {
        int matid = lookupmaterial(bottom), curmat = matid&MATF_VOLUME, flagmat = matid&MATF_FLAGS,
            oldmat = pl->inmaterial&MATF_VOLUME;
        float radius = center.z-bottom.z;
        if(curmat != oldmat)
        {
            #define mattrig(mo,mcol,ms,mt,mz,mq,mp,mw) \
            { \
                int col = (int(mcol[2]*mq) + (int(mcol[1]*mq) << 8) + (int(mcol[0]*mq) << 16)); \
                regularshape(mp, mt, col, 21, 20, mz, mo, ms, 1, 10, 0, 20); \
                if(mw >= 0) playsound(mw, mo, pl); \
            }
            if(curmat == MAT_WATER || oldmat == MAT_WATER)
            {
                const bvec &watercol = getwatercol((curmat == MAT_WATER ? matid : pl->inmaterial) & MATF_INDEX);
                mattrig(bottom, watercol, 0.5f, int(radius), PHYSMILLIS, 0.25f, PART_SPARK, curmat != MAT_WATER ? S_SPLASH2 : S_SPLASH1);
            }
            if(curmat == MAT_LAVA)
            {
                const bvec &lavacol = getlavacol(matid & MATF_INDEX);
                mattrig(center, lavacol, 2.f, int(radius), PHYSMILLIS*2, 1.f, PART_FIREBALL, S_BURNLAVA);
            }
        }
        pl->inmaterial = matid;
        if(local && gameent::is(pl) && pl->state == CS_ALIVE && flagmat&MAT_DEATH) game::suicide((gameent *)pl, HIT_MATERIAL);
        if((pl->inliquid = isliquid(curmat)) != false)
        {
            float frac = radius/10.f, sub = pl->submerged;
            vec tmp = bottom;
            int found = 0;
            loopi(20)
            {
                tmp.z += frac;
                if(!isliquid(lookupmaterial(tmp)&MATF_VOLUME))
                {
                    found = i+1;
                    break;
                }
            }
            pl->submerged = found ? found/20.f : 1.f;
            if(local)
            {
                if(curmat == MAT_WATER && gameent::is(pl) && pl->submerged >= PHYS(liquidextinguish))
                {
                    gameent *d = (gameent *)pl;
                    if(d->burning(lastmillis, burntime) && lastmillis-d->lastres[WR_BURN] > PHYSMILLIS)
                    {
                        d->resetresidual(WR_BURN);
                        playsound(S_EXTINGUISH, d->o, d);
                        part_create(PART_SMOKE, 500, center, 0xAAAAAA, radius*4, 1, -10);
                        if(d->state == CS_ALIVE) client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_EXTINGUISH);
                    }
                }
                if(pl->physstate < PHYS_SLIDE && sub >= 0.5f && pl->submerged < 0.5f && pl->vel.z > 1e-3f)
                    pl->vel.z = max(pl->vel.z, max(jumpvel(pl, false), max(gravityvel(pl), 50.f)));
            }
        }
        else pl->submerged = 0;
        pl->onladder = flagmat&MAT_LADDER;
        if(pl->onladder && pl->physstate < PHYS_SLIDE) pl->floor = vec(0, 0, 1);
    }

    // main physics routine, moves a player/monster for a time step
    // moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
    // local is false for multiplayer prediction

    bool moveplayer(physent *pl, int moveres, bool local, int millis)
    {
        bool floating = isfloating(pl), player = !floating && gameent::is(pl);
        float secs = millis/1000.f;

        pl->blocked = false;
        if(player)
        {
            updatematerial(pl, pl->center(), pl->feetpos(), local);
            modifyvelocity(pl, local, false, millis);
            if(!sticktospecial(pl) && !pl->onladder) modifygravity(pl, millis); // apply gravity
            else pl->resetphys(false);
        }
        else
        {
            pl->inliquid = pl->onladder = false;
            pl->submerged = 0;
            modifyvelocity(pl, local, floating, millis);
        }

        vec vel(pl->vel);
        if(player && pl->inliquid) vel.mul(liquidmerge(pl, 1.f, PHYS(liquidspeed)));
        vel.add(pl->falling);
        vel.mul(secs);

        if(floating) // just apply velocity
        {
            if(pl->physstate != PHYS_FLOAT)
            {
                pl->physstate = PHYS_FLOAT;
                pl->airmillis = pl->floormillis = 0;
                pl->falling = vec(0, 0, 0);
            }
            pl->o.add(vel);
        }
        else // apply velocity with collision
        {
            float mag = vec(pl->vel).add(pl->falling).magnitude();
            int collisions = 0, timeinair = pl->airtime(lastmillis);
            vel.mul(1.0f/moveres);
            loopi(moveres) if(!move(pl, vel)) { if(++collisions<5) i--; } // discrete steps collision detection & sliding
            if(player)
            {
                gameent *d = (gameent *)pl;
                if(!d->airmillis)
                {
                    if(local && impulsemethod&2 && timeinair >= impulseboostdelay && d->move == 1 && allowimpulse(d, IM_A_DASH) && d->action[AC_CROUCH])
                    {
                        d->action[AC_DASH] = true;
                        d->actiontime[AC_DASH] = lastmillis;
                    }
                    if(timeinair >= PHYSMILLIS)
                    {
                        if(mag >= 20)
                        {
                            int vol = min(int(mag*1.25f), 255); if(d->inliquid) vol *= 0.5f;
                            playsound(S_LAND, d->o, d, 0, vol);
                        }
                        else game::footstep(d);
                    }
                }
            }
        }

        if(gameent::is(pl))
        {
            if(pl->state == CS_ALIVE) updatedynentcache(pl);
            if(local)
            {
                gameent *d = (gameent *)pl;
                if(d->state == CS_ALIVE && !floating)
                {
                    if(d->o.z < 0)
                    {
                        game::suicide(d, HIT_LOST);
                        return false;
                    }
                    if(d->turnmillis > 0)
                    {
                        float amt = float(millis)/float(PHYSMILLIS), yaw = d->turnyaw*amt, roll = d->turnroll*amt;
                        if(yaw != 0) d->yaw += yaw;
                        if(roll != 0) d->roll += roll;
                        d->turnmillis -= millis;
                    }
                    else
                    {
                        d->turnmillis = 0;
                        if(d->roll != 0 && !d->turnside) adjustscaled(d->roll, PHYSMILLIS);
                    }
                }
                else
                {
                    d->turnmillis = d->turnside = 0;
                    d->roll = 0;
                }
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

    bool movecamera(physent *pl, const vec &dir, float dist, float stepdist)
    {
        int steps = (int)ceil(dist/stepdist);
        if(steps <= 0) return true;

        vec d(dir);
        d.mul(dist/steps);
        loopi(steps)
        {
            vec oldpos(pl->o);
            pl->o.add(d);
            if(collide(pl, vec(0, 0, 0), 0, false))
            {
                pl->o = oldpos;
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

    void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space)
    {
        float rad = obstacle->radius+d->radius;
        vec bbmin(obstacle->o);
        bbmin.x -= rad;
        bbmin.y -= rad;
        bbmin.z -= obstacle->height+d->aboveeye;
        bbmin.sub(space);
        vec bbmax(obstacle->o);
        bbmax.x += rad;
        bbmax.y += rad;
        bbmax.z += obstacle->aboveeye+d->height;
        bbmax.add(space);

        loopi(3) if(d->o[i] <= bbmin[i] || d->o[i] >= bbmax[i]) return;

        float mindist = 1e16f;
        loopi(3) if(dir[i] != 0)
        {
            float dist = ((dir[i] > 0 ? bbmax[i] : bbmin[i]) - d->o[i]) / dir[i];
            mindist = min(mindist, dist);
        }
        if(mindist >= 0.0f && mindist < 1e15f) d->o.add(vec(dir).mul(mindist));
    }

    void updatephysstate(physent *d)
    {
        if(d->physstate == PHYS_FALL && !d->onladder) return;
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
        if((d->physstate > PHYS_FALL && d->floor.z <= 0) || (d->onladder && !foundfloor)) d->floor = vec(0, 0, 1);
        d->o = old;
    }

    bool xcollide(physent *d, const vec &dir, physent *o)
    {
        hitflags = HITFLAG_NONE;
        if(d->type == ENT_PROJ && gameent::is(o))
        {
            gameent *e = (gameent *)o;
            if(e->wantshitbox())
            {
                if(!d->o.reject(e->legs, d->radius+max(e->lrad.x, e->lrad.y)) && ellipsecollide(d, dir, e->legs, vec(0, 0, 0), e->yaw, e->lrad.x, e->lrad.y, e->lrad.z, e->lrad.z))
                    hitflags |= HITFLAG_LEGS;
                if(!d->o.reject(e->torso, d->radius+max(e->trad.x, e->trad.y)) && ellipsecollide(d, dir, e->torso, vec(0, 0, 0), e->yaw, e->trad.x, e->trad.y, e->trad.z, e->trad.z))
                    hitflags |= HITFLAG_TORSO;
                if(!d->o.reject(e->head, d->radius+max(e->hrad.x, e->hrad.y)) && ellipsecollide(d, dir, e->head, vec(0, 0, 0), e->yaw, e->hrad.x, e->hrad.y, e->hrad.z, e->hrad.z))
                    hitflags |= HITFLAG_HEAD;
                return hitflags != HITFLAG_NONE;
            }
        }
        if(plcollide(d, dir, o))
        {
            hitflags |= HITFLAG_TORSO;
            return true;
        }
        return false;
    }

    bool xtracecollide(physent *d, const vec &from, const vec &to, float x1, float x2, float y1, float y2, float maxdist, float &dist, physent *o)
    {
        hitflags = HITFLAG_NONE;
        if(d && d->type == ENT_PROJ && gameent::is(o))
        {
            gameent *e = (gameent *)o;
            if(e->wantshitbox())
            {
                float bestdist = 1e16f;
                if(e->legs.x+e->lrad.x >= x1 && e->legs.y+e->lrad.y >= y1 && e->legs.x-e->lrad.x <= x2 && e->legs.y-e->lrad.y <= y2)
                {
                    vec bottom(e->legs), top(e->legs); bottom.z -= e->lrad.z; top.z += e->lrad.z; float t = 1e16f;
                    if(linecylinderintersect(from, to, bottom, top, max(e->lrad.x, e->lrad.y), t)) { hitflags |= HITFLAG_LEGS; bestdist = min(bestdist, t); }
                }
                if(e->torso.x+e->trad.x >= x1 && e->torso.y+e->trad.y >= y1 && e->torso.x-e->trad.x <= x2 && e->torso.y-e->trad.y <= y2)
                {
                    vec bottom(e->torso), top(e->torso); bottom.z -= e->trad.z; top.z += e->trad.z; float t = 1e16f;
                    if(linecylinderintersect(from, to, bottom, top, max(e->trad.x, e->trad.y), t)) { hitflags |= HITFLAG_TORSO; bestdist = min(bestdist, t); }
                }
                if(e->head.x+e->hrad.x >= x1 && e->head.y+e->hrad.y >= y1 && e->head.x-e->hrad.x <= x2 && e->head.y-e->hrad.y <= y2)
                {
                    vec bottom(e->head), top(e->head); bottom.z -= e->hrad.z; top.z += e->hrad.z; float t = 1e16f;
                    if(linecylinderintersect(from, to, bottom, top, max(e->hrad.x, e->hrad.y), t)) { hitflags |= HITFLAG_HEAD; bestdist = min(bestdist, t); }
                }
                if(hitflags == HITFLAG_NONE) return false;
                dist = bestdist*from.dist(to);
                return true;
            }
        }
        if(o->o.x+o->radius >= x1 && o->o.y+o->radius >= y1 && o->o.x-o->radius <= x2 && o->o.y-o->radius <= y2 && intersect(o, from, to, dist))
        {
            hitflags |= HITFLAG_TORSO;
            return true;
        }
        return false;
    }

    void complexboundbox(physent *d)
    {
        render3dbox(d->o, d->height, d->aboveeye, d->xradius, d->yradius);
        renderellipse(d->o, d->xradius, d->yradius, d->yaw);
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(e->wantshitbox())
            {
                render3dbox(e->head, e->hrad.z, e->hrad.z, e->hrad.x, e->hrad.y);
                renderellipse(e->head, e->hrad.x, e->hrad.y, e->yaw);
                render3dbox(e->torso, e->trad.z, e->trad.z, e->trad.x, e->trad.y);
                renderellipse(e->torso, e->trad.x, e->trad.y, e->yaw);
                render3dbox(e->legs, e->lrad.z, e->lrad.z, e->lrad.x, e->lrad.y);
                renderellipse(e->legs, e->lrad.x, e->lrad.y, e->yaw);
            }
        }
    }

    bool entinmap(physent *d, bool avoidplayers)
    {
        if(d->state != CS_ALIVE)
        {
            d->resetinterp();
            return insideworld(d->o);
        }
        vec orig = d->o;
        float maxrad = max(d->radius, max(d->xradius, d->yradius));
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
        inmapchk(20, d->o.z += (d->height+d->aboveeye)*n/10.f);
        if(gameent::is(d))
        {
            vec dir = vec(d->yaw, d->pitch).mul(maxrad);
            if(!dir.iszero())
            {
                inmapchk(200, d->o.add(vec(dir).mul(n/10.f)));
                inmapchk(50, d->o.add(vec(dir).mul(-n/10.f)));
            }
            dir = vec(d->vel).normalize().mul(maxrad);
            if(!dir.iszero())
            {
                inmapchk(200, d->o.add(vec(dir).mul(n/10.f)));
                inmapchk(50, d->o.add(vec(dir).mul(-n/10.f)));
            }
            inmapchk(50, d->o.add(vec((rnd(21)-10)/10.f, (rnd(21)-10)/10.f, (rnd(21)-10)/10.f).normalize().mul(maxrad).mul(vec(n/10.f, n/10.f, n/25.f))));
        }
        else
        {
            vec dir = vec(d->vel).normalize().mul(maxrad);
            if(!dir.iszero())
            {
                inmapchk(100, d->o.add(vec(dir).mul(n/10.f)));
                inmapchk(20, d->o.add(vec(dir).mul(-n/10.f)));
            }
        }
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
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));

            d->yaw += d->deltayaw*k;
            if(d->yaw<0) d->yaw += 360;
            else if(d->yaw>=360) d->yaw -= 360;
            d->pitch += d->deltapitch*k;
        }
    }

    void smoothplayer(gameent *d, int res, bool local)
    {
        if(d->state == CS_ALIVE || d->state == CS_EDITING)
        {
            if(smoothmove && d->smoothmillis>0) predictplayer(d, true, res, local);
            else move(d, res, local);
        }
        else if(d->state==CS_DEAD || d->state == CS_WAITING)
        {
            if(d->ragdoll) moveragdoll(d, true);
            else if(lastmillis-d->lastpain<2000) move(d, res, local);
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
            if(d.o.z < hdr.worldsize) return false;
            d.o.z = hdr.worldsize - 1e-3f;
            if(!insideworld(d.o)) return false;
        }
        vec v(0.0001f, 0.0001f, -1);
        v.normalize();
        if(raycube(d.o, v, hdr.worldsize) >= hdr.worldsize) return false;
        d.radius = d.xradius = d.yradius = radius;
        d.height = height;
        d.aboveeye = radius;
        if(!movecamera(&d, vec(0, 0, -1), hdr.worldsize, 1))
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
