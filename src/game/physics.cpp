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

    FVAR(IDF_PERSIST, impulseparkouryaw, 0, 150, 180); // determines the minimum yaw angle to switch between parkour climb and run
    VAR(IDF_PERSIST, impulsemethod, 0, 3, 3); // determines which impulse method to use, 0 = none, 1 = launch, 2 = slide, 3 = both
    VAR(IDF_PERSIST, impulseaction, 0, 3, 3); // determines how impulse action works, 0 = off, 1 = impulse jump, 2 = impulse boost, 3 = both

    VAR(IDF_PERSIST, crouchstyle, 0, 0, 2); // 0 = press and hold, 1 = double-tap toggle, 2 = toggle
    VAR(IDF_PERSIST, walkstyle, 0, 0, 2); // 0 = press and hold, 1 = double-tap toggle, 2 = toggle
    VAR(IDF_PERSIST, grabstyle, 0, 2, 2); // 0 = up=up down=down, 1 = up=down down=up, 2 = up=up, down=up
    VAR(IDF_PERSIST, grabplayerstyle, 0, 3, 3); // 0 = up=up down=down, 1 = up=down down=up, 2 = up=up, down=up, 3 = directly toward player

    int physsteps = 0, lastphysframe = 0, lastmove = 0, lastdirmove = 0, laststrafe = 0, lastdirstrafe = 0, lastcrouch = 0, lastwalk = 0;

    bool allowimpulse(physent *d, int type)
    {
        if(d && gameent::is(d))
            return (!type || AA(((gameent *)d)->actortype, abilities)&(1<<type)) && (impulsestyle || PHYS(gravity) == 0);
        return false;
    }

    bool canimpulse(physent *d, int type, bool touch)
    {
        if(!gameent::is(d) || !allowimpulse(d, type)) return false;
        gameent *e = (gameent *)d;
        if(e->impulse[IM_TYPE] == IM_T_PUSHER && e->impulsetime[IM_T_PUSHER] > lastmillis) return false;
        if(!touch && impulsestyle == 1 && e->impulse[IM_TYPE] > IM_T_JUMP && e->impulse[IM_TYPE] < IM_T_TOUCH) return false;
        if(impulsestyle <= 2 && e->impulse[IM_COUNT] >= impulsecount) return false;
        int time = 0, delay = 0;
        switch(type)
        {
            case A_A_PARKOUR:
                time = max(max(e->impulsetime[IM_T_PARKOUR], e->impulsetime[IM_T_MELEE]), e->impulsetime[IM_T_GRAB]);
                delay = impulseparkourdelay;
                break;
            case A_A_SLIDE:
                time = e->impulsetime[IM_T_SLIDE];
                delay = impulseslidedelay;
                break;
            case A_A_POUND:
                time = e->impulsetime[IM_T_POUND];
                delay = impulsepounddelay;
                break;
            case A_A_BOOST: default:
                time = max(e->impulsetime[IM_T_JUMP], max(e->impulsetime[IM_T_BOOST], e->impulsetime[IM_T_KICK]));
                delay = e->impulse[IM_TYPE] == IM_T_JUMP ? impulsejumpdelay : impulseboostdelay;
                break;
        }
        if(time && delay && lastmillis-time <= delay) return false;
        return true;
    }

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
        else if(type == AC_CROUCH || type == AC_JUMP) game::player1->actiontime[type] = -lastmillis;
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

    int carryaffinity(gameent *d)
    {
        if(m_capture(game::gamemode)) return capture::carryaffinity(d);
        if(m_bomber(game::gamemode)) return bomber::carryaffinity(d);
        return 0;
    }
    CLCOMMAND(carryaffinity, intret(carryaffinity(d)));

    bool secondaryweap(gameent *d)
    {
        if(!isweap(d->weapselect)) return false;
        if(d->action[AC_SECONDARY] && (!d->action[AC_PRIMARY] || d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY])) return true;
        if(d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY] && d->weapstate[d->weapselect] == W_S_POWER) return true;
        return false;
    }

    bool isghost(gameent *d, gameent *e, bool proj)
    { // d is target, e is from
        if(!e || (d == e && !proj)) return false;
        if(d->actortype < A_ENEMY && e->actortype < A_ENEMY && m_ghost(game::gamemode, game::mutators)) return true;
        switch(d->actortype)
        {
            case A_PLAYER: if(!(AA(e->actortype, collide)&(1<<A_C_PLAYERS))) return true; break;
            case A_BOT: if(!(AA(e->actortype, collide)&(1<<A_C_BOTS))) return true; break;
            default: if(!(AA(e->actortype, collide)&(1<<A_C_ENEMIES))) return true; break;
        }
        if(m_team(game::gamemode, game::mutators) && d->team == e->team && (proj || AA(e->actortype, teamdamage)&(1<<A_T_GHOST))) switch(d->actortype)
        {
            case A_PLAYER: if(!(AA(e->actortype, teamdamage)&(1<<A_T_PLAYERS))) return true; break;
            case A_BOT: if(!(AA(e->actortype, teamdamage)&(1<<A_T_BOTS))) return true; break;
            default: if(!(AA(e->actortype, teamdamage)&(1<<A_T_ENEMIES))) return true; break;
        }
        return false;
    }

    bool issolid(physent *d, physent *e, bool esc, bool impact, bool reverse)
    { // d is target, e is from
        if(!e || d == e) return false; // don't collide with themself
        if(inanimate::is(d))
        {
            inanimate *m = (inanimate *)d;
            if(m->coltarget && e == m->coltarget) return false;
            return true;
        }
        if(inanimate::is(e))
        {
            inanimate *m = (inanimate *)e;
            if(m->coltarget && d != m->coltarget) return false;
            return true;
        }
        if(projent::is(e))
        {
            projent *p = (projent *)e;
            if(gameent::is(d))
            {
                gameent *g = (gameent *)d;
                if(g->state != CS_ALIVE) return false;
                if(g->protect(lastmillis, m_protect(game::gamemode, game::mutators))) return false;
                if(p->stick == d || isghost(g, p->owner, true)) return false;
                if(impact && (p->hit == g || !(p->projcollide&COLLIDE_PLAYER))) return false;
                if(p->owner == d && (!(p->projcollide&COLLIDE_OWNER) || (esc && !p->escaped))) return false;
            }
            else if(projent::is(d))
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
            gameent *g = (gameent *)d;
            if(g->state != CS_ALIVE) return false;
            if(gameent::is(e) && isghost(g, (gameent *)e)) return false;
            return true;
        }
        else if(projent::is(d) && !reverse) return issolid(e, d, esc, impact, true);
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

    float jumpvel(physent *d, bool liquid = true)
    {
        float vel = d->jumpspeed;
        if(liquid && d->inliquid) vel *= liquidmerge(d, 1.f, PHYS(liquidspeed));
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            vel *= e->stunscale;
        }
        return vel;
    }

    float gravityvel(physent *d)
    {
        float vel = PHYS(gravity)*(d->weight/100.f);
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            if(e->vel.z+e->falling.z <= gravitycutoff) vel *= e->crouching(true) ? gravityfallcrouch : gravityfall;
            else if(e->actiontime[AC_JUMP] >= 0) vel *= e->crouching(true) ? gravityjumpcrouch : gravityjump;
            else if(e->crouching(true)) vel *= gravitycrouch;
            vel *= e->stungravity;
        }
        return vel;
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
        if(gameent::is(d) && ((gameent *)d)->impulse[IM_TYPE] == IM_T_PARKOUR) return true;
        return false;
    }

    bool isfloating(physent *d)
    {
        return d->type == ENT_CAMERA || (d->type == ENT_PLAYER && d->state == CS_EDITING);
    }

    float movevelocity(physent *d, bool floating)
    {
        physent *pl = d->type == ENT_CAMERA ? game::player1 : d;
        float vel = pl->speed*movespeed;
        if(floating) vel *= floatspeed/100.0f;
        else if(gameent::is(pl))
        {
            gameent *e = (gameent *)pl;
            vel *= e->stunscale;
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
        }
        return vel;
    }

    float impulsevelocity(physent *d, float amt, int &cost, int type, float redir, vec &keep)
    {
        float scale = 1.f;
        if(gameent::is(d))
        {
            gameent *e = (gameent *)d;
            scale *= e->stunscale;
            if(impulsemeter && cost)
            {
                if(impulsecostscale > 0) cost = int(cost*scale);
                int diff = impulsemeter-e->impulse[IM_METER];
                if(cost > diff)
                {
                    if(type >= A_A_IMFIRST && impulsecostrelax&(1<<(type-A_A_IMFIRST)))
                    {
                        scale *= float(diff)/float(cost);
                        cost = diff;
                    }
                    else return 0.f;
                }
            }
            else cost = 0;
        }
        float speed = (d->impulsespeed*amt*scale)+(keep.magnitude()*redir);
        keep.mul(1-min(redir, 1.f));
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
        if(!sticktofloor(d)) return false;
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
        if(sticktospecial(d))
        {
            d->airmillis = 0;
            d->physstate = PHYS_FLOOR;
            d->floor = vec(0, 0, 1);
            return;
        }
        if(floor.z > 0.0f && floor.z < slopez)
        {
            if(floor.z >= wallz) switchfloor(d, dir, floor);
            d->airmillis = 0;
            d->physstate = PHYS_SLIDE;
            d->floor = floor;
        }
        else if(d->physstate < PHYS_SLOPE || dir.dot(d->floor) > 0.01f*dir.magnitude() || (floor.z != 0.0f && floor.z != 1.0f) || !trystepdown(d, dir, true))
            d->physstate = PHYS_FALL;
    }

    void landing(physent *d, vec &dir, const vec &floor, bool collided)
    {
        switchfloor(d, dir, floor);
        d->airmillis = 0;
        if((d->physstate != PHYS_STEP_UP && d->physstate != PHYS_STEP_DOWN) || !collided)
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
        vec old(d->o), obstacle;
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
            else if(!sticktospecial(d) && d->physstate == PHYS_STEP_DOWN && dir.dot(d->floor) <= 1e-6f)
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
        bool launch = !melee && !slide && onfloor && impulsemethod&1 && d->sliding(true) && d->action[AC_JUMP],
             mchk = !melee || onfloor, action = mchk && (d->actortype >= A_BOT || melee || impulseaction&2);
        int move = action ? d->move : 0, strafe = action ? d->strafe : 0;
        bool moving = mchk && (move || strafe), pound = !melee && !launch && !slide && !onfloor && (impulsepoundstyle || !moving) && d->action[AC_CROUCH];
        if(d->actortype < A_BOT && !launch && !melee && !slide && !impulseaction) return false;
        int type = melee ? A_A_PARKOUR : (slide ? A_A_SLIDE : (pound ? A_A_POUND : A_A_BOOST));
        bool pulse = melee ? !onfloor : (!launch && !onfloor && (d->actortype >= A_BOT || impulseaction&1) && d->action[AC_JUMP]);
        if((!launch && !melee && !slide && !pulse) || !canimpulse(d, type, melee || slide)) return false;
        vec keepvel = inertia;
        int cost = int(impulsecost*(melee ? impulsecostmelee : (pound ? impulsecostpound : impulsecostboost)));
        float skew = melee ? impulsemelee : (slide ? impulseslide : (launch ? impulselaunch : (pound ? impulsepound : (moving ? impulseboost : impulsejump)))),
              redir = melee ? impulsemeleeredir : (slide ? impulseslideredir : (launch ? impulselaunchredir : (pound ? impulsepoundredir : (moving ? impulseboostredir : impulsejumpredir)))),
              force = impulsevelocity(d, skew, cost, type, redir, keepvel);
        if(force <= 0) return false;
        vec dir(0, 0, pound ? -1 : 1);
        if(!pound && (launch || slide || moving || onfloor))
        {
            float yaw = d->yaw, pitch = moving && (launch || pulse) ? d->pitch : 0;
            if(launch) pitch = clamp(pitch, impulselaunchpitchmin, impulselaunchpitchmax);
            else if(moving && pulse) pitch = clamp(pitch, impulseboostpitchmin, impulseboostpitchmax);
            vecfromyawpitch(yaw, pitch, moving ? move : 1, strafe, dir);
            if(!launch && slide && !d->floor.iszero() && !dir.iszero())
            {
                dir.project(d->floor).normalize();
                if(dir.z < 0) force += -dir.z*force;
            }
        }
        d->vel = vec(dir).mul(force).add(keepvel);
        if(launch) d->vel.z += jumpvel(d);
        d->doimpulse(melee ? IM_T_MELEE : (slide ? IM_T_SLIDE : (pound ? IM_T_POUND : IM_T_BOOST)), lastmillis, cost);
        d->action[AC_JUMP] = false;
        client::addmsg(N_SPHY, "ri2", d->clientnum, melee ? SPHY_MELEE : (slide ? SPHY_SLIDE : (pound ? SPHY_POUND: SPHY_BOOST)));
        game::impulseeffect(d);
        return true;
    }

    void modifyinput(gameent *d, vec &m, bool wantsmove, int millis)
    {
        bool onfloor = d->physstate >= PHYS_SLOPE || d->onladder || liquidcheck(d);
        if(d->impulse[IM_TYPE] == IM_T_PARKOUR && (!allowimpulse(d, A_A_PARKOUR) || (impulseparkourlen && lastmillis-d->impulsetime[IM_T_PARKOUR] > impulseparkourlen) || d->vel.iszero()))
        {
            d->doimpulse(IM_T_AFTER, lastmillis);
            client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_AFTER);
            d->resetphys(true);
            onfloor = false;
        }
        if(d->impulse[IM_TYPE] == IM_T_PARKOUR)
        {
            if(d->action[AC_JUMP] && canimpulse(d, A_A_BOOST, true))
            {
                int cost = int(impulsecost*impulsecostkick);
                vec keepvel = vec(d->vel).add(d->falling);
                float mag = impulsevelocity(d, impulsekick, cost, A_A_BOOST, impulsekickredir, keepvel);
                if(mag > 0)
                {
                    vec rft;
                    float pitch = clamp(d->pitch, impulsekickpitchmin, impulsekickpitchmax);
                    vecfromyawpitch(d->yaw, pitch, d->move || d->strafe ? d->move : 1, d->strafe, rft);
                    d->vel = vec(rft).mul(mag).add(keepvel);
                    d->doimpulse(IM_T_KICK, lastmillis, cost);
                    d->action[AC_JUMP] = onfloor = false;
                    client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_KICK);
                    game::impulseeffect(d);
                    game::footstep(d);
                }
            }
        }
        else if(!impulseplayer(d, onfloor, vec(d->vel).add(d->falling)) && onfloor && d->action[AC_JUMP] && AA(d->actortype, abilities)&(1<<A_A_JUMP))
        {
            float force = jumpvel(d);
            if(force > 0)
            {
                d->vel.z += force;
                if(d->inliquid)
                {
                    float scale = liquidmerge(d, 1.f, PHYS(liquidspeed));
                    d->vel.x *= scale;
                    d->vel.y *= scale;
                }
                d->doimpulse(IM_T_JUMP, lastmillis);
                d->action[AC_JUMP] = onfloor = false;
                client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_JUMP);
                playsound(S_JUMP, d->o, d);
                createshape(PART_SMOKE, int(d->radius), 0x222222, 21, 20, 250, d->feetpos(), 1, 1, -10, 0, 10.f);
            }
        }
        bool found = false;
        if(d->impulse[IM_TYPE] == IM_T_PARKOUR || d->action[AC_SPECIAL])
        {
            vec oldpos = d->o, dir;
            const int movements[8][2] = { { 2, 2 }, { 1, 2 }, { 1, 0 }, { 1, -1 }, { 1, 1 }, { 0, 1 }, { 0, -1 }, { -1, 0 } };
            loopi(d->impulse[IM_TYPE] == IM_T_PARKOUR ? 8 : 2) // we do these insane checks so that running along walls works at all times
            {
                int move = movements[i][0], strafe = movements[i][1];
                if(move == 2) move = d->move > 0 ? d->move : 0;
                if(strafe == 2) strafe = d->turnside ? d->turnside : d->strafe;
                if(!move && !strafe) continue;
                vecfromyawpitch(d->yaw, 0, move, strafe, dir);
                bool foundwall = false;
                loopk(d->impulse[IM_TYPE] == IM_T_PARKOUR ? 4 : 1)
                {
                    d->o.add(dir);
                    bool collided = collide(d);
                    if(!collided || (collideplayer && !inanimate::is(collideplayer)) || collidewall.iszero()) continue;
                    if(collidematerial && (collidematerial&MATF_CLIP) == MAT_CLIP && (collidematerial&MATF_FLAGS)&MAT_LADDER) continue;
                    foundwall = true;
                    break;
                }
                d->o = oldpos;
                if(!foundwall) continue;
                vec face = vec(collidewall).normalize();
                if(fabs(face.z) <= impulseparkournorm)
                {
                    bool canspec = d->action[AC_SPECIAL] && canimpulse(d, A_A_PARKOUR, true), parkour = canspec && !onfloor && !d->onladder;
                    float yaw = 0, pitch = 0;
                    vectoyawpitch(face, yaw, pitch);
                    float off = yaw-d->yaw;
                    if(off > 180) off -= 360;
                    else if(off < -180) off += 360;
                    bool isclimb = fabs(off) >= impulseparkouryaw, vault = false;
                    if(impulseclimbstyle && canspec && isclimb && !parkour && d->impulse[IM_TYPE] != IM_T_PARKOUR)
                    {
                        float space = d->height+d->aboveeye, m = min(impulseclimbvaultmin, impulseclimbvaultmax), n = max(impulseclimbvaultmin, impulseclimbvaultmax);
                        d->o.add(dir);
                        if(onfloor)
                        {
                            d->o.z += space*m;
                            if(collide(d))
                            {
                                d->o.z += space*n-space*m;
                                if(!collide(d) || (collideplayer && !inanimate::is(collideplayer))) vault = true;
                            }
                        }
                        else
                        {
                            d->o.z += space*n;
                            if(!collide(d) || (collideplayer && !inanimate::is(collideplayer))) vault = true;
                        }
                        d->o = oldpos;
                    }
                    if(d->impulse[IM_TYPE] == IM_T_PARKOUR || parkour || vault)
                    {
                        int side = isclimb ? 0 : (off < 0 ? -1 : 1);
                        if(isclimb)
                        {
                            if(pitch > 0)
                            {
                                yaw += 180;
                                pitch = 90-pitch;
                            }
                            else pitch += 90;
                        }
                        else
                        {
                            if(off < 0) yaw += 90;
                            else yaw -= 90;
                            pitch = 0;
                        }
                        while(yaw >= 360) yaw -= 360;
                        while(yaw < 0) yaw += 360;
                        vec rft;
                        vecfromyawpitch(yaw, pitch, 1, isclimb ? -d->strafe : 0, rft);
                        d->o.add(rft);
                        bool collided = collide(d, rft);
                        d->o = oldpos;
                        if(collided || (collideplayer && !inanimate::is(collideplayer))) continue; // we might find a better vector
                        if(d->impulse[IM_TYPE] != IM_T_PARKOUR)
                        {
                            int cost = int(impulsecost*(isclimb ? impulsecostclimb : impulsecostparkour));
                            vec keepvel = vec(d->vel).add(d->falling);
                            float mag = impulsevelocity(d, isclimb ? impulseclimb : impulseparkour, cost, A_A_PARKOUR, isclimb ? impulseclimbredir : impulseparkourredir, keepvel);
                            if(mag > 0)
                            {
                                d->vel = vec(rft).mul(mag).add(keepvel);
                                d->doimpulse(IM_T_PARKOUR, lastmillis, cost, side);
                                client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_PARKOUR);
                                game::impulseeffect(d);
                                game::footstep(d);
                                m = rft; // re-project and override
                                found = true;
                            }
                        }
                        else
                        {
                            d->turnside = side;
                            m = rft; // re-project and override
                            found = true;
                        }
                        break;
                    }
                }
            }
        }
        if(!found && d->impulse[IM_TYPE] == IM_T_PARKOUR)
        {
            if(!d->turnside && impulseclimbendstyle)
            {
                float mag = vec(d->vel).magnitude();
                if(mag > 0)
                {
                    vec rft;
                    vecfromyawpitch(d->yaw, impulseclimbendstyle == 2 ? max(d->pitch, impulseclimbendmin) : impulseclimbendmin, 1, 0, rft);
                    d->vel = rft.mul(mag);
                }
            }
            d->doimpulse(IM_T_AFTER, lastmillis);
            client::addmsg(N_SPHY, "ri2", d->clientnum, SPHY_AFTER);
        }
        bool sliding = d->sliding(true), pounding = !sliding && !onfloor && d->impulse[IM_TYPE] == IM_T_POUND && d->impulsetime[IM_T_POUND] != 0, kicking = !sliding && !pounding && !onfloor && d->action[AC_SPECIAL];
        if((sliding || pounding || kicking) && d->canmelee(m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis, sliding))
        {
            vec oldpos = d->o, dir(d->yaw*RAD, 0.f);
            loopi(2)
            {
                d->o.add(dir);
                bool collided = collide(d, dir, 0, true, true);
                d->o = oldpos;
                if(collided && collideplayer && gameent::is(collideplayer))
                {
                    vec pos = collideplayer->headpos();
                    if(weapons::doshot(d, pos, W_MELEE, true, sliding, 0, (gameent *)collideplayer))
                        impulseplayer(d, onfloor, vec(d->vel).add(d->falling), true);
                    break;
                }
                if(sliding) break;
                dir = vec(0, 0, -1); // try straight down
            }
        }
    }

    void modifymovement(gameent *d, vec &m, bool local, bool wantsmove, int millis)
    {
        if(local && game::allowmove(d)) modifyinput(d, m, wantsmove, millis);
        if(wantsmove && !sticktospecial(d) && d->physstate >= PHYS_SLOPE)
        { // move up or down slopes in air but only move up slopes in liquid
            float dz = -(m.x*d->floor.x + m.y*d->floor.y)/d->floor.z;
            m.z = liquidcheck(d) ? max(m.z, dz) : dz;
            if(!m.iszero()) m.normalize();
        }
        if(d->physstate == PHYS_FALL && !d->onladder && d->impulse[IM_TYPE] != IM_T_PARKOUR)
        {
            if(!d->airmillis) d->airmillis = lastmillis ? lastmillis : 1;
            d->floormillis = 0;
        }
        else
        {
            d->airmillis = 0;
            if(!d->floormillis) d->floormillis = lastmillis ? lastmillis : 1;
        }
        if(d->impulse[IM_TYPE] != IM_T_PARKOUR && d->onladder && !m.iszero()) m.add(vec(0, 0, m.z >= 0 ? 1 : -1)).normalize();
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
            float c = sticktospecial(pl) || pl->physstate >= PHYS_SLOPE || pl->onladder ? (slide ? PHYS(slidecoast) : PHYS(floorcoast))*coastscale(pl->feetpos(-1)) : PHYS(aircoast);
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
                      c = liquid ? 1.0f : clamp((pl->floor.z-slopez)/(floorz-slopez), 0.0f, 1.0f);
                pl->falling.mul(pow(max(1.0f - c/coast, 0.0f), curtime/20.0f));
            }
        }
        else pl->falling = vec(0, 0, 0);
    }

    void updatematerial(physent *pl, const vec &center, const vec &bottom, bool local)
    {
        float radius = center.z-bottom.z, height = radius*2, submerged = pl->submerged;
        int matid = 0, oldmatid = pl->inmaterial, oldmat = oldmatid&MATF_VOLUME, iters = max(int(ceilf(height)), 1), liquid = 0;
        float frac = height/float(iters); // guard against rounding errors
        vec tmp = bottom;
        pl->inliquid = pl->onladder = false;
        loopi(iters+1)
        {
            int chkmat = lookupmaterial(tmp);
            if(!i)
            {
                matid = chkmat;
                int curmat = matid&MATF_VOLUME;
                if(curmat != oldmat)
                {
                    #define mattrig(mo,mcol,ms,mt,mz,mq,mp,mw) \
                    { \
                        int col = (int(mcol[2]*(mq)) + (int(mcol[1]*(mq)) << 8) + (int(mcol[0]*(mq)) << 16)); \
                        regularshape(mp, mt, col, 21, 20, mz, mo, ms, 1, 10, 0, 20); \
                        if((mw) >= 0) playsound(mw, mo, pl);                    \
                    }
                    if(curmat == MAT_WATER || oldmat == MAT_WATER)
                    {
                        const bvec &watercol = getwatercolour((curmat == MAT_WATER ? matid : oldmatid)&MATF_INDEX);
                        mattrig(bottom, watercol, 0.5f, int(radius), PHYSMILLIS, 0.25f, PART_SPARK, curmat != MAT_WATER ? S_SPLASH2 : S_SPLASH1);
                    }
                    if(curmat == MAT_LAVA)
                    {
                        const bvec &lavacol = getlavacolour(matid&MATF_INDEX);
                        mattrig(center, lavacol, 2.f, int(radius), PHYSMILLIS*2, 1.f, PART_FIREBALL, S_BURNLAVA);
                    }
                }
            }
            else matid |= chkmat&MATF_FLAGS;

            if(isliquid(chkmat&MATF_VOLUME))
            {
                pl->inliquid = true;
                if(i) liquid++;
            }
            if((chkmat&MATF_FLAGS)&MAT_LADDER) pl->onladder = true;
            tmp.z += frac;
        }
        pl->inmaterial = matid;
        pl->submerged = liquid ? liquid/float(iters) : 0.f;
        if(pl->onladder && pl->physstate < PHYS_SLIDE) pl->floor = vec(0, 0, 1);
        if(local && gameent::is(pl))
        {
            if(pl->physstate < PHYS_SLIDE && submerged >= 0.5f && pl->submerged < 0.5f && pl->vel.z > 1e-3f) pl->vel.z = max(pl->vel.z, max(jumpvel(pl, false), gravityvel(pl)));
            if(pl->inmaterial != oldmatid || pl->submerged != submerged) client::addmsg(N_SPHY, "ri3f", ((gameent *)pl)->clientnum, SPHY_MATERIAL, pl->inmaterial, pl->submerged);
        }
    }

    // main physics routine, moves an actor for a time step
    // moveres indicates the physics precision (which is lower for monsters and multiplayer prediction)
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
            vec prevel = vec(pl->vel).add(pl->falling);
            float mag = prevel.magnitude();
            int collisions = 0, timeinair = pl->airtime(lastmillis);
            vel.mul(1.0f/moveres);
            loopi(moveres) if(!move(pl, vel)) { if(++collisions < 5) i--; } // discrete steps collision detection & sliding
            if(player && timeinair)
            {
                gameent *d = (gameent *)pl;
                if(!d->airmillis && !sticktospecial(d))
                {
                    if(local && impulsemethod&2 && timeinair >= impulseslideinair && (d->move == 1 || d->strafe) && d->action[AC_CROUCH] && allowimpulse(d, A_A_SLIDE))
                        impulseplayer(d, true, prevel, false, true);
                    if(timeinair >= PHYSMILLIS)
                    {
                        if(mag >= 20)
                        {
                            int vol = min(int(mag*1.25f), 255);
                            if(d->inliquid) vol *= 0.5f;
                            playsound(S_LAND, d->o, d, 0, vol);
                        }
                        else game::footstep(d);
                    }
                    d->resetjump();
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
                        game::suicide(d, HIT(LOST));
                        return false;
                    }
                    if(d->roll != 0) adjustscaled(d->roll, PHYSMILLIS);
                }
                else d->roll = 0;
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

    bool hitzonecollide(gameent *e, const vec &o, const vec &ray, float &dist)
    {
        modelstate mdl;
        modelattach mdlattach[ATTACHMENTMAX];
        const char *mdlname = game::getplayerstate(e, mdl, 1, e->curscale, 0, mdlattach);
        dist = 1e16f;
        int zone = intersectmodel(mdlname, mdl, o, ray, dist, 0, e);
        switch(zone)
        {
            case -1: return false;
            case 0: collidezones = CLZ_HEAD; break;
            case 1: collidezones = CLZ_TORSO; break;
            default: collidezones = CLZ_LIMB; break;
        }
        dist *= ray.magnitude();
        return true;
    }

    bool checkcollide(physent *d, const vec &dir, physent *o)
    {
        collidezones = CLZ_HEAD;
        if(!d || !projent::shot(d) || !gameent::is(o)) return true;
        gameent *e = (gameent *)o;
        if(!actors[e->actortype].hitboxes) return true;
        collidezones = CLZ_NONE;
        if(!d->o.reject(e->limbstag(), d->radius+max(e->limbsbox().x, e->limbsbox().y)) && ellipsecollide(d, dir, e->limbstag(), vec(0, 0, 0), e->yaw, e->limbsbox().x, e->limbsbox().y, e->limbsbox().z, e->limbsbox().z))
            collidezones |= CLZ_LIMB;
        if(!d->o.reject(e->torsotag(), d->radius+max(e->torsobox().x, e->torsobox().y)) && ellipsecollide(d, dir, e->torsotag(), vec(0, 0, 0), e->yaw, e->torsobox().x, e->torsobox().y, e->torsobox().z, e->torsobox().z))
            collidezones |= CLZ_TORSO;
        if(!d->o.reject(e->headtag(), d->radius+max(e->headbox().x, e->headbox().y)) && ellipsecollide(d, dir, e->headtag(), vec(0, 0, 0), e->yaw, e->headbox().x, e->headbox().y, e->headbox().z, e->headbox().z))
            collidezones |= CLZ_HEAD;
        return collidezones != CLZ_NONE;
    }

    bool checktracecollide(physent *d, const vec &from, const vec &to, float &dist, physent *o, float x1, float x2, float y1, float y2)
    {
        collidezones = CLZ_HEAD;
        if(!d || !projent::shot(d) || !gameent::is(o)) return true;
        gameent *e = (gameent *)o;
        if(!actors[e->actortype].hitboxes) return true;
        collidezones = CLZ_NONE;
        float bestdist = 1e16f;
        if(e->limbstag().x+e->limbsbox().x >= x1 && e->limbstag().y+e->limbsbox().y >= y1 && e->limbstag().x-e->limbsbox().x <= x2 && e->limbstag().y-e->limbsbox().y <= y2)
        {
            vec bottom(e->limbstag()), top(e->limbstag());
            bottom.z -= e->limbsbox().z;
            top.z += e->limbsbox().z;
            float t = 1e16f;
            if(linecylinderintersect(from, to, bottom, top, max(e->limbsbox().x, e->limbsbox().y), t))
            {
                collidezones |= CLZ_LIMB;
                bestdist = min(bestdist, t);
            }
        }
        if(e->torsotag().x+e->torsobox().x >= x1 && e->torsotag().y+e->torsobox().y >= y1 && e->torsotag().x-e->torsobox().x <= x2 && e->torsotag().y-e->torsobox().y <= y2)
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
        if(e->headtag().x+e->headbox().x >= x1 && e->headtag().y+e->headbox().y >= y1 && e->headtag().x-e->headbox().x <= x2 && e->headtag().y-e->headbox().y <= y2)
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

        if(m->coltype&(1<<INANIMATE_C_KILL))
        {
            game::suicide(e, HIT(TOUCH));
            return;
        }
        if(!inside && collidewall.z > 0) m->addpassenger(d);
    }

    void renderboundboxes(physent *d, const vec &rad, const vec &o)
    {
        if(!gameent::is(d)) return;
        vec pos = vec(o).sub(GUARDRADIUS), radius = vec(rad).add(GUARDRADIUS*2);
        loopj(6) boxs(j, pos, radius);
        gameent *e = (gameent *)d;
        if(!actors[e->actortype].hitboxes || (e == game::focus && !game::thirdpersonview())) return;
        vec headpos = vec(e->headtag()).sub(e->headbox()), headbox = vec(e->headbox()).mul(2),
            torsopos = vec(e->torsotag()).sub(e->torsobox()), torsobox = vec(e->torsobox()).mul(2),
            limbspos = vec(e->limbstag()).sub(e->limbsbox()), limbsbox = vec(e->limbsbox()).mul(2);
        loopj(6)
        {
            boxs(j, headpos, headbox);
            boxs(j, torsopos, torsobox);
            boxs(j, limbspos, limbsbox);
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
