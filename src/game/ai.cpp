#include "game.h"
namespace ai
{
    avoidset obstacles, wpavoid;
    int avoidmillis = 0, iteration = -1, itermillis = 0, itercount = 0;
    vec aitarget(0, 0, 0);

    VAR(0, aidebug, 0, 0, 7);
    VAR(0, aidebugfocus, 0, 1, 2);
    VAR(0, aitimeout, 1000, 1000, VAR_MAX);
    VAR(0, aiavoiddelay, 0, 100, 1000);

    VARF(0, showwaypoints, 0, 0, 1, if(showwaypoints) getwaypoints());

    VAR(IDF_PERSIST, autosavewaypoints, 0, 1, 1);
    VAR(IDF_PERSIST, showwaypointsdrop, 0, 1, 1);
    VAR(IDF_PERSIST, showwaypointsradius, 0, 256, VAR_MAX);
    VAR(IDF_PERSIST|IDF_HEX, showwaypointscolour, 0, 0xFF00FF, 0xFFFFFF);
    VAR(IDF_PERSIST, showaiinfo, 0, 0, 2); // 0/1 = shows/hides bot join/parts, 2 = show more verbose info

    bool dbgfocus(gameent *d)  { return d->ai && (!aidebugfocus || d == game::focus || (aidebugfocus != 2 && !game::focus->ai)); }

    void savemap(bool force, const char *mname)
    {
        if(autosavewaypoints || force) savewaypoints(true, mname);
    }

    void startmap(bool empty)    // called just after a map load
    {
        if(autosavewaypoints) savewaypoints();
        clearwaypoints(true);
        waypointversion = -1;
        showwaypoints = dropwaypoints = 0;
        if(!empty) getwaypoints();
    }

    float viewdist(int x) { return x <= 100 ? clamp((SIGHTMIN+(SIGHTMAX-SIGHTMIN))/100.f*float(x), float(SIGHTMIN), max(float(getfog()), SIGHTMIN)) : max(float(getfog()), SIGHTMIN); }
    float viewfieldx(int x) { return x <= 100 ? clamp((VIEWMIN+(VIEWMAX-VIEWMIN))/100.f*float(x), float(VIEWMIN), float(VIEWMAX)) : float(VIEWMAX); }
    float viewfieldy(int x) { return viewfieldx(x)*3.f/4.f; }

    float weapmindist(int weap, bool alt)
    {
        if(WF(false, weap, collide, alt)&COLLIDE_LENGTH) return 0.f;
        if(WX(false, weap, radial, alt, game::gamemode, game::mutators, 1.f) > 0) return WX(false, weap, radial, alt, game::gamemode, game::mutators, 1.f);
        return 1.f;
    }

    float weapmaxdist(int weap, bool alt)
    {
        if(W2(weap, aidist, alt) > 0) return W2(weap, aidist, alt);
        return SIGHTMAX;
    }

    bool weaprange(gameent *d, int weap, bool alt, float dist)
    {
        if(!isweap(weap) || (W2(weap, extinguish, alt) && isliquid(d->inmaterial&MATF_VOLUME))) return false;
        float mindist = weapmindist(weap, alt), maxdist = weapmaxdist(weap, alt);
        return dist >= mindist*mindist && dist <= maxdist*maxdist;
    }

    bool targetable(gameent *d, gameent *e, bool solid)
    {
        if(!d || !e || d == e || !e->isalive() || !(A(d->actortype, abilities)&A_A_ATTACK)) return false;

        bool hacked = false;
        if(d->actortype == A_JANITOR)
        {
            if(d->ai)
            {
                aistate &b = d->ai->getstate();
                if(b.overridetype == AI_O_HACKED)
                {
                    if(b.type == AI_S_DEFEND && b.targtype == AI_T_ACTOR)
                    {
                        if(b.target == e->clientnum) return false; // don't attack their hacker
                        if(m_team(game::gamemode, game::mutators))
                        {
                            gameent *hacker = game::getclient(b.target);
                            if(hacker && hacker->team == e->team) return false; // don't attack their hacker's team
                        }
                    }
                    
                    hacked = true;
                }
            }

            if(!hacked && d->hasprize <= 0 && !game::hasdamagemerge(d, e)) return false;
        }

        if(!solid || physics::issolid(e, d))
        {
            if(e->actortype >= A_ENVIRONMENT)
            {
                if(d->actortype >= A_ENEMY) return false; // don't let actors just attack each other
                if(e->hasprize <= 0) return false; // and don't have bots chase down janitors all the time, etc
            }
            if(hacked || !m_team(game::gamemode, game::mutators) || d->team != e->team) return true;
        }

        return false;
    }

    bool cansee(gameent *d, vec &x, vec &y, bool force, vec &targ)
    {
        if(force) return raycubelos(x, y, targ);
        return getsight(x, d->yaw, d->pitch, y, targ, d->ai->views[2], d->ai->views[0], d->ai->views[1]);
    }

    bool badhealth(gameent *d)
    {
        int hp = d->gethealth(game::gamemode, game::mutators);
        if(d->skill < 100 && d->health < hp) return d->health <= (111-d->skill)*max(hp*0.01f, 1.f);
        return false;
    }

    bool canshoot(gameent *d, gameent *e, bool alt = true)
    {
        if(isweap(d->weapselect) && weaprange(d, d->weapselect, alt, e->o.squaredist(d->o)))
        {
            int prot = m_protect(game::gamemode, game::mutators);
            if((d->actortype >= A_ENEMY || !d->protect(lastmillis, prot)) && targetable(d, e, true))
                return d->canshoot(d->weapselect, alt ? HIT_ALT : 0, m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis, (1<<W_S_RELOAD));
        }
        return false;
    }

    bool altfire(gameent *d, gameent *e)
    {
        if(e && !(W2(d->weapselect, cooked, true)&W_C_ZOOM) && canshoot(d, e, true))
        {
            if(d->weapstate[d->weapselect] == W_S_POWER || d->weapstate[d->weapselect] == W_S_ZOOM)
            {
                if(d->action[AC_SECONDARY] && (!d->action[AC_PRIMARY] || d->actiontime[AC_SECONDARY] > d->actiontime[AC_PRIMARY]))
                    return true;
            }
            switch(d->weapselect)
            {
                case W_PISTOL: return true; break;
                case W_ROCKET: default: return false; break;
                case W_CLAW: case W_SWORD: case W_SHOTGUN: case W_SMG: case W_FLAMER: case W_PLASMA: case W_ZAPPER:
                case W_GRENADE: case W_MINE:
                case W_MINIGUN: case W_JETSAW: case W_ECLIPSE:
                    if(rnd(d->skill*2) <= d->skill) return false;
                    break;
                case W_RIFLE: case W_CORRODER: if(weaprange(d, d->weapselect, false, e->o.squaredist(d->o))) return false; break;
            }
            return true;
        }
        return false;
    }

    bool hastarget(gameent *d, aistate &b, gameent *e, bool alt, bool insight, float yaw, float pitch)
    { // add margins of error
        if(insight && weaprange(d, d->weapselect, alt, e->o.squaredist(d->o)))
        {
            if(W2(d->weapselect, aidist, alt) < CLOSEDIST) return true;
            float skew = clamp(float(lastmillis-d->ai->enemymillis)/float((d->skill*W(d->weapselect, delayreload)/5000.f)+(d->skill*W2(d->weapselect, delayattack, alt)/500.f)), 0.f, weaptype[d->weapselect].thrown ? 0.25f : 1e16f),
                offy = yaw-d->yaw, offp = pitch-d->pitch;
            if(offy > 180) offy -= 360;
            else if(offy < -180) offy += 360;
            if(fabs(offy) <= d->ai->views[0]*skew && fabs(offp) <= d->ai->views[1]*skew) return true;
        }
        return false;
    }

    vec &getaimpos(gameent *d, gameent *e, bool alt)
    {
        d->ai->aimpos = e->o;

        if(!(A(d->actortype, abilities)&(1<<A_A_KAMIKAZE)) && d->skill < 100 && lastmillis >= d->ai->lastaimpos)
        {
            int radius = ceilf(e->radius * W2(d->weapselect, aiskew, alt));
            float scale = (101 - d->skill) / 100.f;
            loopk(3) d->ai->aimpos[k] += (rnd((radius * 2) + 1) - radius) * scale;
            int dur = max(d->skill * 2, 30) * 5;
            d->ai->lastaimpos = lastmillis + dur + rnd(dur);
        }

        d->ai->aimpos.add(vec(e->vel).mul(W2(d->weapselect, aileadvel, alt)).mul(clamp(d->o.dist(e->o) / W2(d->weapselect, aileaddist, alt), 0.f, 1.f)));

        return d->ai->aimpos;
    }

    int weappref(gameent *d)
    {
        if(d->loadweap.length()) return d->loadweap[0];
        return m_weapon(d->actortype, game::gamemode, game::mutators);
    }

    bool hasweap(gameent *d, int weap)
    {
        if(!isweap(weap) || !d->hasweap(weap, m_weapon(d->actortype, game::gamemode, game::mutators), 2)) return false;
        return true;
    }

    bool wantsweap(gameent *d, int weap, bool noitems = true)
    {
        if(!isweap(weap) || !m_maxcarry(d->actortype, game::gamemode, game::mutators)) return false;
        if(itemweap(weap)) { if(noitems) return false; }
        else
        {
            int weapnum = min(m_maxcarry(d->actortype, game::gamemode, game::mutators), d->loadweap.length()),
                weaphas = d->loadweap.find(weap);
            if(weaphas < 0 || weaphas >= m_maxcarry(d->actortype, game::gamemode, game::mutators))
            {
                bool force = false;
                if(m_classic(game::gamemode, game::mutators))
                { // we can pick up non-preferential weapons when we're empty
                    loopi(weapnum) if(d->getammo(d->loadweap[i], 0, true) < 0)
                    {
                        force = true;
                        break;
                    }
                }
                if(!force) return false;
            }
        }
        return d->canuseweap(game::gamemode, game::mutators, weap, m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis);
    }

    void create(gameent *d)
    {
        if(!d->ai && !(d->ai = new aiinfo()))
        {
            fatal("Could not create ai");
            return;
        }
        d->respawned = -1;
    }

    void destroy(gameent *d)
    {
        if(d->ai) DELETEP(d->ai);
    }

    void checkinfo(gameent *d)
    {
        gameent *o = game::getclient(d->ownernum);
        if(o)
        {
            copystring(d->hostip, o->hostip);
            d->version.grab(o->version);
        }
    }

    void init(gameent *d, int at, int et, int on, int sk, int bn, char *name, int tm, int c1, int c2, int md, const char *vn, const char *mx, vector<int> &lweaps)
    {
        getwaypoints();

        gameent *o = game::newclient(on);
        if(!o) return;
        bool resetthisguy = false;
        string m;
        copystring(m, game::colourname(o));
        if(!d->name[0])
        {
            if(at == A_BOT && showaiinfo && client::showpresence >= (client::waiting(false) ? 2 : 1))
            {
                if(showaiinfo > 1) conoutf(colourgreen, "%s assigned to %s at skill %d", game::colourname(d, name), m, sk);
                else conoutf(colourgreen, "%s was added to the game", game::colourname(d, name));
            }
            game::specreset(d);
            resetthisguy = true;
        }
        else
        {
            if(d->ownernum != on)
            {
                if(at == A_BOT && showaiinfo && client::showpresence >= (client::waiting(false) ? 2 : 1))
                    conoutf(colourgreen, "%s reassigned to %s", game::colourname(d, name), m);
                resetthisguy = true;
            }
            if(at == A_BOT && d->skill != sk && showaiinfo > 1 && client::showpresence >= (client::waiting(false) ? 2 : 1))
                conoutf(colourgreen, "%s changed skill to %d", game::colourname(d, name), sk);
        }

        if((d->actortype = at) >= A_ENEMY) d->type = ENT_AI;
        else
        {
            d->loadweap.shrink(0);
            loopv(lweaps) d->loadweap.add(lweaps[i]);
        }
        d->setname(name);
        d->spawnpoint = et;
        d->ownernum = on;
        d->skill = sk;
        d->team = tm;
        d->colours[0] = c1;
        d->colours[1] = c2;
        d->model = md;
        d->setvanity(vn);
        d->setmixer(mx);

        if(resetthisguy) projs::removeplayer(d);
        if(d->ownernum >= 0 && game::player1->clientnum == d->ownernum)
        {
            create(d);
            if(d->ai)
            {
                d->ai->views[0] = viewfieldx(d->skill);
                d->ai->views[1] = viewfieldy(d->skill);
                d->ai->views[2] = viewdist(d->skill);
            }
        }
        else if(d->ai) destroy(d);
        checkinfo(d);
    }

    void update()
    {
        if(!gs_playing(game::gamestate))
        {
            loopv(game::players) if(game::players[i] && game::players[i]->ai) game::players[i]->stopmoving(true);
        }
        else // fixed rate logic done out-of-sequence at 1 frame per second for each ai
        {
            if(totalmillis-avoidmillis >= aiavoiddelay)
            {
                avoid();
                avoidmillis = totalmillis;
            }
            if(totalmillis-itermillis >= 1000)
            {
                iteration = itercount = 0;
                loopv(game::players) if(game::players[i] && game::players[i]->ai) itercount++;
                itermillis = totalmillis;
            }
            if(itercount > 0)
            {
                int x = 999/itercount, y = totalmillis-itermillis, c = 0;
                loopv(game::players) if(game::players[i] && game::players[i]->ai)
                {
                    gameent *d = game::players[i];
                    bool iterate = c == iteration && y >= c*x;
                    if(iterate)
                    {
                        iteration++;
                        switch(d->state)
                        {
                            case CS_DEAD:
                            {
                                if(d->respawned >= 0 || (d->lastdeath && lastmillis-d->lastdeath <= DEATHMILLIS*2)) break;
                                client::addmsg(N_TRYSPAWN, "ri", d->clientnum);
                                d->respawned = lastmillis;
                                break;
                            }
                            case CS_SPECTATOR:
                            {
                                if(d->respawned >= 0) break;
                                client::addmsg(N_SPECTATOR, "ri2", d->clientnum, 0);
                                d->respawned = lastmillis;
                                break;
                            }
                            default: break;
                        }
                    }
                    think(d, iterate);
                    c++;
                }
            }
        }
    }

    int checkothers(vector<int> &targets, gameent *d, int state, int targtype, int target, bool teams, int *members)
    { // checks the states of other ai for a match
        targets.setsize(0);
        gameent *e = NULL;
        int numdyns = game::numdynents();
        loopi(numdyns) if((e = (gameent *)game::iterdynents(i)))
        {
            if(targets.find(e->clientnum) >= 0) continue;
            if(teams && m_team(game::gamemode, game::mutators) && d->team != e->team) continue;
            if(members) (*members)++;
            if(e == d || !e->ai || e->state != CS_ALIVE || e->actortype != d->actortype) continue;
            aistate &b = e->ai->getstate();
            if(state >= 0 && b.type != state) continue;
            if(target >= 0 && b.target != target) continue;
            if(targtype >=0 && b.targtype != targtype) continue;
            targets.add(e->clientnum);
        }
        return targets.length();
    }

    bool makeroute(gameent *d, aistate &b, int node, bool changed, int retries, float dist)
    {
        if(changed && !d->ai->route.empty() && d->ai->route[0] == node) return true;
        if(route(d, d->lastnode, node, d->ai->route, obstacles, retries))
        {
            b.override = false;
            return true;
        }
        // retry fails: 0 = first attempt, 1 = try ignoring obstacles, 2 = try ignoring prevnodes too
        if(retries <= 1) return makeroute(d, b, node, false, retries+1, dist);
        return false;
    }

    bool makeroute(gameent *d, aistate &b, const vec &pos, bool changed, int retries, float dist)
    {
        int node = closestwaypoint(pos, dist, true);
        return makeroute(d, b, node, changed, retries, dist);
    }

    bool randomnode(gameent *d, aistate &b, const vec &pos, float guard, float wander)
    {
        static vector<int> candidates;
        candidates.setsize(0);
        findwaypointswithin(pos, guard, wander, candidates);
        while(!candidates.empty())
        {
            int w = rnd(candidates.length()), n = candidates.removeunordered(w);
            if(n != d->lastnode && !d->ai->hasprevnode(n) && !obstacles.find(n, d) && makeroute(d, b, n)) return true;
        }
        return false;
    }

    int closenode(gameent *d, int retries = 0)
    {
        vec pos = getbottom(d);
        int node1 = -1, node2 = -1, node3 = -1;
        float mindist1 = retries >= 0 ? CLOSEDIST*CLOSEDIST : MINWPDIST*MINWPDIST, mindist2 = retries > 0 ? RETRYDIST*RETRYDIST : mindist1, mindist3 = retries > 0 ? FARDIST*FARDIST : mindist2;
        loopv(d->ai->route) if(iswaypoint(d->ai->route[i]))
        {
            vec epos = waypoints[d->ai->route[i]].o;
            float dist = epos.squaredist(pos);
            if(dist > mindist3) continue;
            if(dist < mindist1) { node1 = i; mindist1 = dist; }
            else if(retries >= 0)
            {
                int entid = obstacles.remap(d, d->ai->route[i], epos);
                if(entid >= 0)
                {
                    if(entid != i) dist = epos.squaredist(pos);
                    if(dist < mindist2) { node2 = i; mindist2 = dist; }
                }
                else if(dist < mindist3) { node3 = i; mindist3 = dist; }
            }
        }
        int n = node1 >= 0 ? node1 : (node2 >= 0 ? node2 : node3);
        return n >= 0 || retries ? n : closenode(d, retries + 1);
    }

    vec getbottom(gameent *d)
    {
        if(!d->ai) return d->feetpos();
        if(totalmillis == d->ai->lastbottom) return d->ai->bottom;

        d->ai->bottom = d->feetpos();
        d->ai->lastbottom = totalmillis;

        if(physics::movepitch(d, true))
        {
            int n = closenode(d, -1);
            if(d->ai->route.inrange(n)) d->ai->bottom.z = waypoints[d->ai->route[n]].o.z;
            else
            {
                physics::droptofloor(d->ai->bottom, 3, d->radius, d->height);
                n = closenode(d, -1);
                if(d->ai->route.inrange(n)) d->ai->bottom.z = waypoints[d->ai->route[n]].o.z;
            }
        }

        return d->ai->bottom;
    }

    bool randomnode(gameent *d, aistate &b, float guard, float wander)
    {
        return randomnode(d, b, getbottom(d), guard, wander);
    }

    bool enemy(gameent *d, aistate &b, const vec &pos, float guard, int pursue, bool force, bool retry = false)
    {
        if(d->ai->enemy >= 0 && lastmillis - d->ai->enemymillis >= (111 - d->skill) * 50) return false;

        gameent *t = NULL, *e = NULL;
        float mindist = guard * guard, bestdist = 1e16f;
        int numdyns = game::numdynents();
        loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && targetable(d, e))
        {
            float dist = d->o.squaredist(e->o);
            if(dist < bestdist && (force || dist <= mindist || cansee(d, d->o, e->o, d->actortype >= A_ENEMY)))
            {
                t = e;
                bestdist = dist;
            }
        }

        if(t && violence(d, b, t, pursue)) return true;
        if(retry && !force) return enemy(d, b, pos, guard, pursue, true, false);

        return false;
    }

    bool patrol(gameent *d, aistate &b, const vec &pos, float guard, float wander, int walk, bool retry)
    {
        if(A(d->actortype, abilities)&(1<<A_A_MOVE))
        {
            float dist = getbottom(d).squaredist(pos);
            if(walk == 2 || b.override || (walk && dist <= guard*guard) || !makeroute(d, b, pos))
            { // run away and back to keep ourselves busy
                if(!b.override && wander > 0 && randomnode(d, b, pos, guard, wander))
                {
                    b.override = true;
                    return true;
                }
                if(d->ai->route.empty())
                {
                    b.override = false;
                    if(!retry) return patrol(d, b, pos, guard, wander, walk, true);
                }
                if(wander <= 0)
                {
                    b.acttype = AI_A_IDLE;
                    return true;
                }
                b.override = false;
                return false;
            }
        }
        b.override = false;
        return true;
    }

    bool defense(gameent *d, aistate &b, const vec &pos, float guard, float wander, int walk, int actoverride)
    {
        bool canmove = A(d->actortype, abilities)&(1<<A_A_MOVE);
        if(!canmove || (!walk && getbottom(d).squaredist(pos) <= guard*guard))
        {
            if(actoverride >= 0) b.acttype = actoverride;
            else b.acttype = enemy(d, b, pos, wander >= 0 ? wander : guard*2, canmove && W2(d->weapselect, aidist, false) < CLOSEDIST ? 1 : 0, false, !canmove) ? AI_A_PROTECT : AI_A_IDLE;

            return true;
        }

        return patrol(d, b, pos, guard, wander, walk);
    }

    bool violence(gameent *d, aistate &b, gameent *e, int pursue)
    {
        if(!targetable(d, e)) return false;

        if(d->ai->enemy != e->clientnum)
        {
            gameent *f = game::getclient(d->ai->enemy);
            if(f && (d->o.squaredist(e->o) < d->o.squaredist(f->o) || (d->ai->enemyseen && lastmillis - d->ai->enemyseen <= (d->skill * 10) + 1000))) return false;
        }

        if(pursue && d->actortype < A_ENVIRONMENT)
        {
            if((b.targtype != AI_T_AFFINITY || (pursue && !(pursue%2))) && makeroute(d, b, e->lastnode))
                d->ai->switchstate(b, AI_S_PURSUE, AI_T_ACTOR, e->clientnum, b.targtype != AI_T_AFFINITY ? AI_A_NORMAL : AI_A_HASTE);
            else if(pursue >= 3) return false; // can't pursue
        }

        if(d->ai->enemy != e->clientnum)
        {
            d->ai->enemyseen = d->ai->enemymillis = lastmillis;
            d->ai->enemy = e->clientnum;
        }

        return true;
    }


    struct targcache
    {
        gameent *d;
        bool dominated, visible;
        float dist;

        targcache() : d(NULL), dominated(false), visible(false), dist(0) {}
        ~targcache() {}

        static bool tcsort(targcache &a,  targcache &b)
        {
            if(a.dominated && !b.dominated) return true;
            if(!a.dominated && b.dominated) return false;
            if(a.visible && !b.visible) return true;
            if(!a.visible && b.visible) return false;
            if(a.dist < b.dist) return true;
            if(a.dist > b.dist) return false;
            return true;
        }
    };

    bool target(gameent *d, aistate &b, int pursue = 0, bool force = false)
    {
        static vector<targcache> targets;
        targets.setsize(0);

        gameent *e = NULL;
        int numdyns = game::numdynents();
        loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && targetable(d, e))
        {
            targcache &c = targets.add();
            c.d = e;
            c.dist = d->o.squaredist(e->o);
            if(c.d->dominator.find(d) >= 0) c.dominated = true;
            c.visible = force || cansee(d, d->o, e->o, d->actortype >= A_ENEMY);
        }

        if(targets.empty()) return false;

        targets.sort(targcache::tcsort);
        d->ai->enemy = -1;
        d->ai->enemymillis = d->ai->enemyseen = 0;
        loopv(targets) if(violence(d, b, targets[i].d, pursue || targets[i].dominated ? 1 : 0)) return true;

        return false;
    }

    void assist(gameent *d, aistate &b, vector<interest> &interests, bool all = false, bool force = false)
    {
        gameent *e = NULL;
        int numdyns = game::numdynents();
        loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && e != d && (all || e->actortype == A_PLAYER) && (b.overridetype == AI_O_HACKED || d->team == e->team))
        {
            interest &n = interests.add();
            n.state = AI_S_DEFEND;
            n.node = e->lastnode;
            n.target = e->clientnum;
            n.targtype = AI_T_ACTOR;
            n.score = e->o.squaredist(d->o)/(force ? 1e8f : (hasweap(d, weappref(d)) ? 1.f : 0.5f));
            n.tolerance = 0.25f;
            n.team = true;
            n.acttype = AI_A_PROTECT;
        }
    }

    void items(gameent *d, aistate &b, vector<interest> &interests, bool force = false)
    {
        vec pos = getbottom(d);
        loopusej(EU_ITEM)
        {
            gameentity &e = *(gameentity *)entities::ents[j];
            if(enttype[e.type].usetype != EU_ITEM || e.type != WEAPON || !entities::isallowed(e)) continue;
            int attr = m_attr(e.type, e.attrs[0]);
            if(e.spawned() && isweap(attr) && wantsweap(d, attr, !force))
            { // go get a weapon upgrade
                interest &n = interests.add();
                n.state = AI_S_INTEREST;
                n.node = closestwaypoint(e.pos(), CLOSEDIST, true);
                n.target = j;
                n.targtype = AI_T_ENTITY;
                n.score =  pos.squaredist(e.pos())/(wantsweap(d, attr) ? 1e8f : (force ? 1e4f : 1.f));
                n.tolerance = 0;
            }
        }

        loopvj(projs::typeprojs[PROJ_ENTITY]) if(projs::typeprojs[PROJ_ENTITY][j]->projtype == PROJ_ENTITY && projs::typeprojs[PROJ_ENTITY][j]->ready())
        {
            projent &proj = *projs::typeprojs[PROJ_ENTITY][j];
            if(!entities::ents.inrange(proj.id)) continue;
            gameentity &e = *(gameentity *)entities::ents[proj.id];
            if(enttype[e.type].usetype != EU_ITEM || e.type != WEAPON || !entities::isallowed(e)) continue;
            int attr = m_attr(e.type, e.attrs[0]);
            if(isweap(attr) && wantsweap(d, attr, !force) && proj.owner != d)
            { // go get a weapon upgrade
                interest &n = interests.add();
                n.state = AI_S_INTEREST;
                n.node = closestwaypoint(proj.o, CLOSEDIST, true);
                n.target = proj.id;
                n.targtype = AI_T_DROP;
                n.score = pos.squaredist(proj.o)/(wantsweap(d, attr) ? 1e8f : (force ? 1e4f : 1.f));
                n.tolerance = 0;
            }
        }
    }

    void janitor(gameent *d, aistate &b, vector<interest> &interests)
    {
        if(janitorcollect && d->collects.length() >= janitorcollect)
        {
            if(entities::ents.inrange(d->spawnpoint))
            {
                gameentity &e = *(gameentity *)entities::ents[d->spawnpoint];

                interest &n = interests.add();
                n.state = AI_S_INTEREST;
                n.node = n.target = closestwaypoint(e.pos(), janitorsuckdist, true);
                n.targtype = AI_T_HOME; // go home
                n.score = d->o.squaredist(e.pos());
                n.tolerance = 0;

                return;
            }

            game::suicide(d, HIT_JANITOR);

            return; // fail
        }

        loopv(projs::junkprojs)
        {
            projent &proj = *projs::junkprojs[i];

            if(!proj.isjunk(m_messy(game::gamemode, game::mutators))) continue;

            int v = closestwaypoint(proj.o, janitorsuckdist, true);
            bool found = false;

            loopvj(interests)
            {
                interest &n = interests[j];
                if(n.state != AI_S_INTEREST || n.targtype != AI_T_JUNK || n.node != v) continue;
                n.score *= 0.9f; // don't scale too much to prioritise nearby junk
                found = true;
                break;
            }

            if(found) continue;

            interest &n = interests.add();
            n.state = AI_S_INTEREST;
            n.node = n.target = v;
            n.targtype = AI_T_JUNK;
            n.score = d->o.squaredist(proj.o);
            n.tolerance = 0;
        }

        int numdyns = game::numdynents(1);
        loopi(numdyns)
        {   // reject junk with nearby janitors
            dynent *e = game::iterdynents(i);
            if(!e || !gameent::is(e)) continue;

            gameent *f = (gameent *)e;
            if(f == d || f->actortype != A_JANITOR || !f->isalive()) continue;

            loopvkrev(interests)
            {
                interest &n = interests[k];

                if(n.state != AI_S_INTEREST || n.targtype != AI_T_JUNK || !iswaypoint(interests[k].node)) continue;
                if(f->o.squaredist(waypoints[interests[k].node].o) > janitorreject*janitorreject) continue;

                interests.remove(k);
            }
        }
    }

    bool find(gameent *d, aistate &b)
    {
        static vector<interest> interests; interests.setsize(0);
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        if(A(d->actortype, abilities)&(1<<A_A_MOVE))
        {
            if(d->actortype == A_JANITOR) janitor(d, b, interests);
            else
            {
                if(d->actortype < A_ENEMY && (A(d->actortype, abilities)&(1<<A_A_PRIMARY) || A(d->actortype, abilities)&(1<<A_A_SECONDARY)) && (!hasweap(d, weappref(d)) || d->carry(sweap) == 0))
                    items(d, b, interests, d->carry(sweap) == 0);
                if(m_team(game::gamemode, game::mutators) && !m_duke(game::gamemode, game::mutators))
                    assist(d, b, interests, false, false);
            }
        }
        else if(entities::ents.inrange(d->spawnpoint)) loopv(entities::ents[d->spawnpoint]->links)
        {
            int t = entities::ents[d->spawnpoint]->links[i];
            if(!entities::ents.inrange(t)) continue;
            interest &n = interests.add();
            n.state = AI_S_DEFEND;
            n.target = t;
            n.node = closestwaypoint(((gameentity *)entities::ents[t])->pos(), CLOSEDIST, true);
            n.targtype = AI_T_ENTITY;
            n.score = -1;
            n.tolerance = 1;
        }

        if(m_play(game::gamemode) && A(d->actortype, abilities)&(1<<A_A_AFFINITY))
        {
            if(m_capture(game::gamemode)) capture::aifind(d, b, interests);
            else if(m_defend(game::gamemode)) defend::aifind(d, b, interests);
            else if(m_bomber(game::gamemode)) bomber::aifind(d, b, interests);
        }

        bool canretry = d->actortype < A_ENEMY && A(d->actortype, abilities)&(1<<A_A_MOVE) && A(d->actortype, abilities)&A_A_ATTACK;
        loopk(canretry ? 2 : 1)
        {
            while(!interests.empty())
            {
                int q = interests.length()-1;
                loopi(interests.length()-1) if(interests[i].score < interests[q].score) q = i;
                interest n = interests.removeunordered(q);
                if(d->actortype == A_BOT && m_team(game::gamemode, game::mutators))
                {
                    int members = 0;
                    static vector<int> targets; targets.setsize(0);
                    int others = checkothers(targets, d, n.state, n.targtype, n.target, n.team, &members);
                    if(d->actortype == A_BOT && n.state == AI_S_DEFEND && members == 1) continue;
                    if(others >= int(ceilf(members*n.tolerance))) continue;
                }
                if(!(A(d->actortype, abilities)&(1<<A_A_MOVE)) || makeroute(d, b, n.node))
                {
                    d->ai->switchstate(b, n.state, n.targtype, n.target, n.acttype);
                    return true;
                }
            }
            if(!k && canretry) items(d, b, interests, true);
        }

        return false;
    }

    void damaged(gameent *d, gameent *e, int weap, int flags, int damage)
    {
        if(!d || !e || d == e) return; // shrug it off

        if(d->ai && A(d->actortype, aihackchance) > 0 && weap == A(d->actortype, aihackweap))
        {
            bool allow = false;
            if(A(d->actortype, aihacktype)&1 && !(flags&HIT_ALT) && !(flags&HIT_FLAK)) allow = true;
            else if(A(d->actortype, aihacktype)&2 && (flags&HIT_ALT) && !(flags&HIT_FLAK)) allow = true;
            else if(A(d->actortype, aihacktype)&4 && !(flags&HIT_ALT) && (flags&HIT_FLAK)) allow = true;
            else if(A(d->actortype, aihacktype)&8 && (flags&HIT_ALT) && (flags&HIT_FLAK)) allow = true;

            if(allow && (A(d->actortype, aihackchance) == 1 || !rnd(A(d->actortype, aihackchance))))
            {
                d->ai->clear();
                d->ai->switchstate(d->ai->getstate(), AI_S_DEFEND, AI_T_ACTOR, e->clientnum, AI_A_PROTECT, e->clientnum, AI_O_HACKED);
                return; // hack the ai
            }
        }

        if(d->actortype == A_JANITOR && d->hasprize <= 0)
        {
            if(!d->ai) return; // janitors don't care about damage unless they have a prize
            aistate &b = d->ai->getstate();
            if(b.type != AI_S_DEFEND || b.targtype != AI_T_ACTOR || b.target == e->clientnum) return; // don't override an existing hack
        }

        if(d->ai && (d->team == T_ENEMY || (hitdealt(flags) && damage > 0) || d->ai->enemy < 0 || d->dominator.find(e) >= 0)) // see if this ai is interested in a grudge
        {
            aistate &b = d->ai->getstate();
            violence(d, b, e, d->actortype != A_BOT || W2(d->weapselect, aidist, false) < CLOSEDIST ? 1 : 0);
        }

        static vector<int> targets; // check if one of our ai is defending them
        targets.setsize(0);
        if(checkothers(targets, d, AI_S_DEFEND, AI_T_ACTOR, d->clientnum, d->actortype < A_ENEMY))
        {
            gameent *t;
            loopv(targets) if((t = game::getclient(targets[i])) && t->ai && ((hitdealt(flags) && damage > 0) || t->ai->enemy < 0 || t->dominator.find(e) >= 0))
            {
                aistate &c = t->ai->getstate();
                violence(t, c, e, W2(d->weapselect, aidist, false) < CLOSEDIST ? 1 : 0);
            }
        }
    }

    void setup(gameent *d, int ent = -1)
    {
        d->spawnpoint = ent;
        if(d->ai)
        {
            d->ai->clean();
            d->ai->reset(true);
            d->ai->lastrun = lastmillis;
            safefindorientation(d->o, d->yaw, d->pitch, d->ai->target);
        }
    }

    void respawned(gameent *d, bool local, int ent)
    {
        if(d->ai) setup(d, ent);
    }

    void killed(gameent *d, gameent *e)
    {
        if(d->ai) d->ai->reset(true);
    }

    void itemspawned(int ent, int spawned)
    {
        if(!m_play(game::gamemode) || !entities::ents.inrange(ent) || entities::ents[ent]->type != WEAPON || spawned <= 0 || !entities::isallowed(ent)) return;
        gameentity &e = *(gameentity *)entities::ents[ent];

        loopv(game::players) if(game::players[i] && game::players[i]->ai && game::players[i]->actortype == A_BOT && game::players[i]->state == CS_ALIVE && iswaypoint(game::players[i]->lastnode))
        {
            gameent *d = game::players[i];
            if(d->actortype >= A_ENEMY) continue;
            aistate &b = d->ai->getstate();
            int sweap = m_weapon(d->actortype, game::gamemode, game::mutators), attr = m_attr(e.type, e.attrs[0]);
            if(!isweap(attr) || b.targtype == AI_T_AFFINITY) continue; // don't override any affinity states
            if((A(d->actortype, abilities)&(1<<A_A_PRIMARY) || A(d->actortype, abilities)&(1<<A_A_SECONDARY)) && !hasweap(d, attr) && (!hasweap(d, weappref(d)) || d->carry(sweap) == 0) && wantsweap(d, attr))
            {
                if(b.type == AI_S_INTEREST && (b.targtype == AI_T_ENTITY || b.targtype == AI_T_DROP))
                {
                    if(entities::ents.inrange(b.target))
                    {
                        gameentity &f = *(gameentity *)entities::ents[b.target];
                        int weap = m_attr(e.type, f.attrs[0]);
                        if(isweap(attr) && ((attr == weappref(d) && weap != weappref(d)) || d->o.squaredist(e.pos()) < d->o.squaredist(f.pos())))
                            d->ai->switchstate(b, AI_S_INTEREST, AI_T_ENTITY, ent);
                    }
                    continue;
                }
                d->ai->switchstate(b, AI_S_INTEREST, AI_T_ENTITY, ent);
            }
        }
    }

    bool check(gameent *d, aistate &b)
    {
        if(d->actortype == A_BOT)
        {
            if(m_capture(game::gamemode) && capture::aicheck(d, b)) return true;
            else if(m_defend(game::gamemode) && defend::aicheck(d, b)) return true;
            else if(m_bomber(game::gamemode) && bomber::aicheck(d, b)) return true;
        }
        return false;
    }

    bool dowait(gameent *d, aistate &b)
    {
        if(check(d, b) || find(d, b)) return true;
        if(target(d, b, 4, false)) return true;
        if(target(d, b, 4, true)) return true;
        if(A(d->actortype, abilities)&(1<<A_A_MOVE) && randomnode(d, b, FARDIST, 1e16f))
        {
            d->ai->switchstate(b, AI_S_INTEREST, AI_T_NODE, d->ai->route[0]);
            return true;
        }
        return false;
    }

    bool dodefense(gameent *d, aistate &b)
    {
        if(d->state != CS_ALIVE) return false;
        switch(b.targtype)
        {
            case AI_T_ENTITY:
            {
                if(check(d, b)) return true;
                if(entities::ents.inrange(b.target) && entities::isallowed(b.target))
                    return defense(d, b, ((gameentity *)entities::ents[b.target])->pos());
                break;
            }
            case AI_T_AFFINITY:
            {
                if(m_capture(game::gamemode)) return capture::aidefense(d, b);
                else if(m_defend(game::gamemode)) return defend::aidefense(d, b);
                else if(m_bomber(game::gamemode)) return bomber::aidefense(d, b);
                break;
            }
            case AI_T_ACTOR:
            {
                if(check(d, b)) return true;
                gameent *e = game::getclient(b.target);
                if(e && (b.overridetype == AI_O_HACKED || d->team == e->team))
                {
                    if(e->state == CS_ALIVE) return defense(d, b, getbottom(e));
                    if(b.owner >= 0) return patrol(d, b, getbottom(d));
                }
                break;
            }
            default:
            {
                if(check(d, b)) return true;
                if(iswaypoint(b.target)) return defense(d, b, waypoints[b.target].o, FARDIST, 0, 0);
                break;
            }
        }
        return false;
    }

    bool dojanitorjunk(gameent *d, aistate &b)
    {
        if(d->collects.length() >= janitorcollect) return false; // go home

        int count = 0, numdyns = game::numdynents(1);
        loopi(numdyns)
        {
            dynent *e = game::iterdynents(i);
            if(!e || !gameent::is(e)) continue;

            gameent *f = (gameent *)e;
            if(f == d || f->actortype != A_JANITOR || !f->isalive()) continue;
            if(f->o.squaredist(waypoints[b.target].o) <= janitorreject*janitorreject) return false;
        }

        loopv(projs::junkprojs)
        {
            projent &proj = *projs::junkprojs[i];

            if(!proj.isjunk(m_messy(game::gamemode, game::mutators))) continue;
            if(proj.o.squaredist(waypoints[b.target].o) > RETRYDIST*RETRYDIST) continue;
            count++;
        }

        if(!count) return false;

        return makeroute(d, b, waypoints[b.target].o, true, 0, janitorsuckdist);
    }

    bool dojanitorhome(gameent *d, aistate &b)
    {
        if(d->collects.empty()) return false;
        if(d->o.dist(waypoints[b.target].o) <= janitorsuckdist)
        {
            game::suicide(d, HIT_JANITOR);
            return defense(d, b, waypoints[b.target].o, MINWPDIST, janitorsuckdist, 0, AI_A_IDLE);
        }
        return makeroute(d, b, waypoints[b.target].o, true, 0, janitorsuckdist);
    }

    bool dointerest(gameent *d, aistate &b)
    {
        if(d->state != CS_ALIVE || !(A(d->actortype, abilities)&(1<<A_A_MOVE))) return false;

        switch(b.targtype)
        {
            case AI_T_ENTITY:
            {
                if(entities::ents.inrange(b.target))
                {
                    gameentity &e = *(gameentity *)entities::ents[b.target];
                    if(enttype[e.type].usetype != EU_ITEM || e.type != WEAPON || !entities::isallowed(e)) return false;
                    int attr = m_attr(e.type, e.attrs[0]);
                    if(!isweap(attr) || !e.spawned() || !wantsweap(d, attr, false)) return false;
                    return makeroute(d, b, e.pos());
                }
                break;
            }
            case AI_T_DROP:
            {
                loopvj(projs::typeprojs[PROJ_ENTITY]) if(projs::typeprojs[PROJ_ENTITY][j]->projtype == PROJ_ENTITY && projs::typeprojs[PROJ_ENTITY][j]->ready() && projs::typeprojs[PROJ_ENTITY][j]->id == b.target)
                {
                    projent &proj = *projs::typeprojs[PROJ_ENTITY][j];
                    if(!entities::ents.inrange(proj.id) || proj.owner == d) return false;
                    gameentity &e = *(gameentity *)entities::ents[proj.id];
                    if(enttype[entities::ents[proj.id]->type].usetype != EU_ITEM || e.type != WEAPON || !entities::isallowed(e)) return false;
                    int attr = m_attr(e.type, e.attrs[0]);
                    if(!isweap(attr) || !wantsweap(d, attr, false)) return false;
                    return makeroute(d, b, proj.o);
                }
                break;
            }
            case AI_T_JUNK:
            {
                return dojanitorjunk(d, b);
                break;
            }
            case AI_T_HOME:
            {
                return dojanitorhome(d, b);
                break;
            }
            default: // this is like a wait state without sitting still..
            {
                if(check(d, b) || (b.owner < 0 && find(d, b))) return true;
                if(target(d, b, b.owner < 0 ? 0 : 4, true)) return true;
                if(iswaypoint(b.target) && (b.owner >= 0 || d->lastnode != b.target))
                    return defense(d, b, waypoints[b.target].o, CLOSEDIST, b.owner >= 0 ? 0.f : FARDIST, b.owner >= 0 ? 0 : 2);
                break;
            }
        }

        return false;
    }

    bool dopursue(gameent *d, aistate &b)
    {
        if(d->state != CS_ALIVE) return false;
        switch(b.targtype)
        {
            case AI_T_AFFINITY:
            {
                if(m_capture(game::gamemode)) return capture::aipursue(d, b);
                else if(m_defend(game::gamemode)) return defend::aipursue(d, b);
                else if(m_bomber(game::gamemode)) return bomber::aipursue(d, b);
                break;
            }

            case AI_T_ACTOR:
            {
                //if(check(d, b)) return true;
                gameent *e = game::getclient(b.target);
                if(e && targetable(d, e))
                {
                    if(e->state == CS_ALIVE)
                    {
                        bool alt = altfire(d, e);
                        if(!(A(d->actortype, abilities)&(1<<A_A_MOVE)))
                        {
                            if(cansee(d, d->o, e->o, d->actortype >= A_ENEMY) || (e->clientnum == d->ai->enemy && d->ai->enemyseen && lastmillis - d->ai->enemyseen <= (d->skill * 30) + 1000))
                                return true;
                            return false;
                        }
                        return patrol(d, b, getbottom(e), weapmindist(d->weapselect, alt), weapmaxdist(d->weapselect, alt));
                    }
                    if(b.owner >= 0) return patrol(d, b, getbottom(d));
                }
                break;
            }
            default:
            {
                if(check(d, b)) return true;
                if(iswaypoint(b.target)) return defense(d, b, waypoints[b.target].o);
                break;
            }
        }
        return false;
    }

    bool dooverride(gameent *d, aistate &b)
    {
        if(d->state != CS_ALIVE) return false;
        switch(b.targtype)
        {
            case AI_T_NODE:
            {
                if(iswaypoint(b.target)) switch(b.overridetype)
                {
                    case AI_O_DANCE:
                        return defense(d, b, waypoints[b.target].o, MINWPDIST, FARDIST, 2, AI_A_HASTE); break;
                    case AI_O_STAND: case AI_O_CROUCH: default:
                        return defense(d, b, waypoints[b.target].o, MINWPDIST, 0, 0, AI_A_IDLE);
                }
                break;
            }
            case AI_T_ACTOR:
            {
                gameent *e = game::getclient(b.target);
                if(e && e->state == CS_ALIVE && iswaypoint(e->lastnode) && waypoints[e->lastnode].haslinks())
                {
                    switch(b.overridetype)
                    {
                        case AI_O_DANCE:
                            return defense(d, b, waypoints[e->lastnode].o, MINWPDIST, FARDIST, 2, AI_A_HASTE);
                        case AI_O_STAND: case AI_O_CROUCH: default:
                        {
                            int closest = -1;
                            float closedist = 1e16f;
                            loopi(MAXWAYPOINTLINKS)
                            {
                                int n = waypoints[e->lastnode].links[i];
                                if(!n || obstacles.find(n, d)) break;
                                float dist = waypoints[n].o.dist(getbottom(d));
                                if(closest < 0 || dist < closedist)
                                {
                                    closest = n;
                                    closedist = dist;
                                }
                            }
                            return defense(d, b, waypoints[closest].o, MINWPDIST, 0, 0, AI_A_IDLE);
                        }
                    }
                }
                break;
            }
            default:
            {
                if(iswaypoint(b.target)) return defense(d, b, waypoints[b.target].o, MINWPDIST, 0, 0, AI_A_IDLE);
                break;
            }
        }

        if(b.owner >= 0) return defense(d, b, getbottom(d), CLOSEDIST, FARDIST, 0, AI_A_IDLE);

        return false;
    }

    void setspot(gameent *d, const vec &pos, int targ = -1)
    {
        if(!d || !d->ai) return;

        // vec lastspot = d->ai->spot;

        d->ai->spot = pos;
        d->ai->targnode = targ;

        if(physics::movepitch(d, true))
        {
            if(!d->blocked)
            {
                d->ai->spot.z += A(d->actortype, aifloatheight);

                int blockmillis = d->ai->blocklast ? lastmillis - d->ai->blocklast : 3000;

                if(blockmillis < 3000)
                {
                    if(blockmillis < 500) d->ai->spot.z -= A(d->actortype, aifloatduck) * (blockmillis / 500.f);
                    else if(blockmillis > 1500) d->ai->spot.z -= A(d->actortype, aifloatduck) * (1.0f - ((blockmillis - 1500) / 1500.f));
                    else d->ai->spot.z -= A(d->actortype, aifloatduck);
                }
            }

            #if 0 // conflicts with ducking
            if(d->ai->spot != lastspot)
            {
                vec dir = vec(d->ai->spot).sub(d->o).normalize();
                if(!dir.iszero())
                {
                    bool reroute = false, found = false;
                    vec pos = vec(dir).mul(d->radius + GUARDRADIUS * 2).add(d->o);
                    loopk(3)
                    {
                        if(clipped(pos))
                        {
                            if(k == 2) pos = d->o;
                            pos.z -= GUARDRADIUS * (k + 1);
                            reroute = true;
                        }
                        else
                        {
                            if(reroute) found = true;
                            break;
                        }
                    }

                    if(found) d->ai->spot = pos;
                }
            }
            #endif
        }
    }

    int wpspot(gameent *d, int n, bool check = false)
    {
        if(iswaypoint(n)) loopk(2)
        {
            vec epos = waypoints[n].o;
            int entid = obstacles.remap(d, n, epos, k!=0);
            if(iswaypoint(entid))
            {
                vec feet = getbottom(d);

                if(!physics::movepitch(d))
                {
                    float zoff = epos.z - feet.z;
                    if(!d->canimpulse(IM_T_JUMP) && zoff >= JUMPMIN) epos.z = feet.z;
                    else if(d->canimpulse(IM_T_JUMP) && d->airtime(lastmillis) >= 25 && zoff <= -JUMPMIN) epos.z = feet.z;
                }

                setspot(d, epos, entid);

                return !check || feet.squaredist(epos) > MINWPDIST*MINWPDIST ? 1 : 2;
            }
        }
        return 0;
    }

    int randomlink(gameent *d, int n)
    {
        if(iswaypoint(n) && waypoints[n].haslinks())
        {
            waypoint &w = waypoints[n];
            static vector<int> linkmap; linkmap.setsize(0);
            loopi(MAXWAYPOINTLINKS)
            {
                if(!w.links[i]) break;
                if(iswaypoint(w.links[i]) && !d->ai->hasprevnode(w.links[i]) && d->ai->route.find(w.links[i]) < 0)
                    linkmap.add(w.links[i]);
            }
            if(!linkmap.empty()) return linkmap[rnd(linkmap.length())];
        }
        return -1;
    }

    bool anynode(gameent *d, aistate &b, int len = NUMPREVNODES)
    {
        if(iswaypoint(d->lastnode)) loopk(2)
        {
            int n = randomlink(d, d->lastnode);
            if(wpspot(d, n))
            {
                d->ai->route.add(n);
                d->ai->route.add(d->lastnode);
                loopi(len)
                {
                    n = randomlink(d, n);
                    if(iswaypoint(n)) d->ai->route.insert(0, n);
                    else break;
                }
                return true;
            }
        }
        return false;
    }

    bool checkroute(gameent *d, int n)
    {
        if(d->ai->route.empty() || !d->ai->route.inrange(n)) return false;

        int len = d->ai->route.length();
        if(len <= 2 || (d->ai->lastcheck && lastmillis - d->ai->lastcheck <= 1000)) return false;

        int w = d->ai->route[n], c = min(len, NUMPREVNODES);
        if(c >= 3) loopj(c)
        {   // check ahead to see if we need to go around something
            int m = len-j-1;
            if(m <= 1) return false; // route length is too short from this point

            int v = d->ai->route[j];
            if(d->ai->hasprevnode(v) || obstacles.find(v, d))
            {   // something is in the way, try to remap around it
                d->ai->lastcheck = lastmillis;
                loopi(m)
                {
                    int q = j+i+1, t = d->ai->route[q];
                    if(!d->ai->hasprevnode(t) && !obstacles.find(t, d))
                    {
                        static vector<int> remap; remap.setsize(0);
                        if(route(d, w, t, remap, obstacles))
                        {   // kill what we don't want and put the remap in
                            while(d->ai->route.length() > i) d->ai->route.pop();
                            loopvk(remap) d->ai->route.add(remap[k]);
                            return true;
                        }
                        return false; // we failed
                    }
                }
                return false;
            }
        }
        return false;
    }

    bool hunt(gameent *d, aistate &b, bool allowrnd)
    {
        if(!d->ai->route.empty())
        {
            int n = closenode(d);
            if(d->ai->route.inrange(n) && checkroute(d, n)) n = closenode(d);
            if(d->ai->route.inrange(n))
            {
                if(!n)
                {
                    switch(wpspot(d, d->ai->route[n], true))
                    {
                        case 2: d->ai->clear(false);
                        case 1: return true; // not close enough to pop it yet
                        case 0: default: break;
                    }
                }
                else if(n >= 0)
                {
                    while(d->ai->route.length() > n+1) d->ai->route.pop(); // waka-waka-waka-waka
                    int m = n-1; // next, please!
                    if(d->ai->route.inrange(m) && wpspot(d, d->ai->route[m])) return true;
                }
            }
        }

        if(b.type == AI_S_PURSUE && b.targtype == AI_T_AFFINITY)
        {
            if(m_capture(game::gamemode)) { if(capture::aicheckpos(d, b)) return true; }
            else if(m_defend(game::gamemode)) { if(defend::aicheckpos(d, b)) return true; }
            else if(m_bomber(game::gamemode)) { if(bomber::aicheckpos(d, b)) return true; }
        }
        else if(b.type == AI_S_INTEREST && b.targtype == AI_T_ENTITY)
        {
            if(entities::ents.inrange(b.target))
            {
                gameentity &e = *(gameentity *)entities::ents[b.target];
                if(e.pos().dist(getbottom(d)) <= WAYPOINTRADIUS*2 && entities::isallowed(b.target))
                    setspot(d, e.pos());
            }
        }

        b.override = false;

        return allowrnd ? anynode(d, b) : false;
    }

    void jumpto(gameent *d, aistate &b, const vec &pos)
    {
        vec off = vec(pos).sub(getbottom(d));
        int airtime = d->airtime(lastmillis);
        bool sequenced = d->ai->blockseq > 1 || d->ai->targseq > 1, offground = airtime && !physics::liquidcheck(d) && !physics::laddercheck(d),
             impulse = d->canimpulse(IM_T_BOOST) && airtime > (b.acttype >= AI_A_LOCKON ? 100 : 250) && d->hasparkour() && (b.acttype >= AI_A_LOCKON || off.z >= JUMPMIN),
             jumper = d->canimpulse(IM_T_JUMP) && !offground && (b.acttype == AI_A_LOCKON || sequenced || off.z >= JUMPMIN),
             jump = (impulse || jumper) && lastmillis >= d->ai->jumpseed, allowspecial = !sequenced && !physics::laddercheck(d) && airtime;

        if(jump)
        {
            vec old = d->o;
            d->o = vec(pos).add(vec(0, 0, d->height));
            if(collide(d, vec(0, 0, 1))) jump = false;
            d->o = old;

            if(jump) loopenti(PUSHER)
            {
                gameentity &e = *(gameentity *)entities::ents[i];
                if(e.type != PUSHER || !entities::isallowed(e)) continue;
                int radius = (e.attrs[3] ? e.attrs[3] : enttype[e.type].radius)*2;
                radius *= radius;
                if(e.pos().squaredist(pos) <= radius)
                {
                    jump = allowspecial = false;
                    break;
                }
            }

            if(jump) loopvrev(d->used) if(entities::ents.inrange(d->used[i].ent))
            {
                gameentity &e = *(gameentity *)entities::ents[i];
                if(e.type != PUSHER || !entities::isallowed(e)) continue;
                int fmillis = d->lastused(i);
                if(fmillis && lastmillis-fmillis < entities::triggertime(e))
                {
                    jump = allowspecial = false;
                    break;
                }
            }

        }

        if(d->action[AC_JUMP] != jump)
        {
            d->action[AC_JUMP] = jump;
            d->actiontime[AC_JUMP] = jump ? lastmillis : -lastmillis;
        }

        if(jumper && d->action[AC_JUMP])
        {
            int seed = (111-d->skill)*(b.acttype == AI_A_LOCKON ? 2 : 10);
            d->ai->jumpseed = lastmillis+seed+rnd(seed);
        }

        if(allowspecial)
        {
            if(!d->hasparkour() && (d->skill >= 100 || !rnd(101-d->skill)) && d->canimpulse(airtime > impulsejumpdelay ? IM_T_DASH : IM_T_WALLRUN))
                d->action[AC_SPECIAL] = true;
            else if(lastmillis-d->ai->lastmelee >= (201-d->skill)*35 && d->canmelee(m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis))
            {
                d->action[AC_SPECIAL] = true;
                d->ai->lastmelee = lastmillis;
            }
        }
    }

    bool lockon(gameent *d, gameent *e, float maxdist)
    {
        if(d->blocked) return false;
        vec dir = vec(e->o).sub(d->o);
        float xydist = dir.x*dir.x+dir.y*dir.y, zdist = dir.z*dir.z, mdist = maxdist*maxdist, ddist = d->radius*d->radius+e->radius*e->radius;
        if(zdist <= ddist && xydist >= ddist+4 && xydist <= mdist+ddist) return true;
        return false;
    }

    bool updatemovement(gameent *d, bool occupied)
    {
        bool ret = true;

        if(d->ai->dontmove || !(A(d->actortype, abilities)&(1<<A_A_MOVE)) || (A(d->actortype, hurtstop) && lastmillis-d->lastpain <= A(d->actortype, hurtstop)))
        {
            d->move = d->strafe = 0;
            ret = false;
        }
        else if(actors[d->actortype].onlyfwd)
        {
            d->move = 1;
            d->strafe = 0;
            ret = true;
        }
        else
        { // our guys move one way.. but turn another?! :)
            const struct aimdir { int move, strafe, offset; } aimdirs[8] =
            {
                {  1,  0,   0 },
                {  1,  -1,  45 },
                {  0,  -1,  90 },
                { -1,  -1,  135 },
                { -1,  0,   180 },
                { -1,  1,   225 },
                {  0,  1,   270 },
                {  1,  1,   315 }
            };

            if(physics::movepitch(d))
            {
                bool wantjump = d->ai->targpitch > A(d->actortype, aipitchangle),
                     wantcrouch = d->ai->targpitch < -A(d->actortype, aipitchangle);

                if(d->action[AC_JUMP] != wantjump)
                {
                    d->action[AC_JUMP] = wantjump;
                    d->actiontime[AC_JUMP] = wantjump ? lastmillis : -lastmillis;
                    d->ai->targpitch -= 90;
                }

                if(d->action[AC_CROUCH] != wantcrouch)
                {
                    d->action[AC_CROUCH] = wantcrouch;
                    d->actiontime[AC_CROUCH] = wantcrouch ? lastmillis : -lastmillis;
                    d->ai->targpitch += 90;
                }

                ret = false;
            }

            float yaw = d->ai->targyaw - d->yaw;
            if(yaw < 0.0f) yaw = 360.0f - fmodf(-yaw, 360.0f);
            else if(yaw >= 360.0f) yaw = fmodf(yaw, 360.0f);

            const aimdir &ad = aimdirs[clamp(((int)floor((yaw + 22.5f) / 45.0f))&7, 0, 7)];
            d->move = ad.move;
            d->strafe = ad.strafe;
        }

        return ret;
    }

    void runhazard(gameent *d, aistate &b, bool &occupied, bool &firing, bool &wantitem)
    {
        d->move = d->strafe = 0;
        d->ai->dontmove = true;
        setspot(d, getbottom(d));

        int weap = m_weapon(d->actortype, game::gamemode, game::mutators);
        bool wantsfire = false, hastrigger = false, hasspawned = false, alt = false;;

        if(entities::ents.inrange(d->spawnpoint))
        {
            gameentity &e = *(gameentity *)entities::ents[d->spawnpoint];
            loopv(e.links) if(entities::ents.inrange(e.links[i]))
            {
                gameentity &f = *(gameentity *)entities::ents[e.links[i]];
                if(f.type != TRIGGER) continue;
                hastrigger = true;
                if(!f.spawned()) continue;
                hasspawned = true;
                break;
            }

            weap = clamp(e.attrs[6] - 1, int(W_CLAW), int(W_MAX)-1);
            switch(e.attrs[9])
            {
                case 0: case 1: alt = false; break;
                case 2: alt = true; break;
                case 3: alt = abs(d->actiontime[AC_PRIMARY]) > abs(d->actiontime[AC_SECONDARY]); break;
                default: alt = rnd(2) != 0; break;
            }

            if((!hastrigger || hasspawned) && (!e.attrs[8] || !d->lastshoot || lastmillis - d->lastshoot >= e.attrs[8]))
                wantsfire = true;
        }
        else wantsfire = true;

        if(isweap(weap) && weap != d->weapselect)
        {
            wantsfire = false;
            occupied = true;
            weapons::weapselect(d, weap, (1<<W_S_SWITCH)|(1<<W_S_RELOAD));
        }

        if(wantsfire && (!W2(weap, cooktime, alt) || !W2(weap, cooked, alt) || !d->action[alt ? AC_SECONDARY : AC_PRIMARY]))
        {
            if(!d->action[alt ? AC_SECONDARY : AC_PRIMARY])
            {
                d->action[alt ? AC_SECONDARY : AC_PRIMARY] = true;
                d->actiontime[alt ? AC_SECONDARY : AC_PRIMARY] = lastmillis;
                d->ai->lastaction = lastmillis;
            }
            if(d->action[alt ? AC_PRIMARY : AC_SECONDARY])
            { // disengage the other firing mode
                d->action[alt ? AC_PRIMARY : AC_SECONDARY] = true;
                d->actiontime[alt ? AC_PRIMARY : AC_SECONDARY] = -lastmillis;
            }
            firing = occupied = true;
        }
        else loopk(2) if(d->action[k ? AC_SECONDARY : AC_PRIMARY])
        {
            d->action[k ? AC_SECONDARY : AC_PRIMARY] = false;
            d->actiontime[k ? AC_SECONDARY : AC_PRIMARY] = -lastmillis;
        }

        float oldyaw = d->yaw, oldpitch = d->pitch;
        if(!entities::getdynamic(d->spawnpoint, d->o, &d->yaw, &d->pitch))
        {
            if(entities::ents.inrange(d->spawnpoint))
            {
                d->yaw = entities::ents[d->spawnpoint]->attrs[1];
                d->pitch = entities::ents[d->spawnpoint]->attrs[2];
            }
            else
            {
                d->yaw = oldyaw;
                d->pitch = oldpitch;
            }
        }

        fixrange(d->yaw, d->pitch);
        safefindorientation(d->o, d->yaw, d->pitch, d->ai->target);
        d->resetinterp();
    }

    void process(gameent *d, aistate &b, bool &occupied, bool &firing, bool &wantitem)
    {
        if(d->actortype == A_HAZARD)
        {
            runhazard(d, b, occupied, firing, wantitem);
            return; // override
        }

        float frame = d->skill <= 100 ? (lastmillis - d->ai->lastrun) / float(max(101 - d->skill, 1) * 10) : 1;

        if(d->dominator.length()) frame *= 1 + d->dominator.length(); // berserker mode
        if(d->hasprize > 0) frame *= A(d->actortype, speedprize); // adjust for increased speed

        bool dancing = b.type == AI_S_OVERRIDE && b.overridetype == AI_O_DANCE,
            allowrnd = dancing || b.type == AI_S_WAIT || b.type == AI_S_PURSUE || b.type == AI_S_INTEREST;

        d->action[AC_SPECIAL] = d->ai->dontmove = false;

        if(b.acttype == AI_A_IDLE || !(A(d->actortype, abilities)&(1<<A_A_MOVE)))
        {
            d->ai->lastturn = 0;
            d->ai->dontmove = true;
            setspot(d, getbottom(d));
        }
        else if(hunt(d, b, allowrnd))
        {
            d->ai->lastturn = 0;
            game::getyawpitch(d->o, d->ai->spot, d->ai->targyaw, d->ai->targpitch);
            if(d->ai->route.length() <= 1 && d->ai->spot.squaredist(getbottom(d)) <= MINWPDIST*MINWPDIST)
                d->ai->dontmove = true;
        }
        else if(!allowrnd)
        {
            d->ai->lastturn = 0;
            d->ai->dontmove = true;
        }
        else if(d->blocked || (!d->ai->lastturn || lastmillis - d->ai->lastturn >= 10000))
        {
            int n = closestwaypoint(getbottom(d), SIGHTMAX, true);
            if(iswaypoint(n))
            {
                if(!d->blocked && d->ai->lastturn) game::suicide(d, HIT_LOST);
                else setspot(d, waypoints[n].o);
            }
            else setspot(d, vec((d->ai->targyaw + 90 + rnd(180)) * RAD, (physics::movepitch(d) ? rnd(51) - 25 : 0) * RAD).mul(CLOSEDIST).add(getbottom(d)));

            d->ai->lastturn = lastmillis;
            game::getyawpitch(d->o, d->ai->spot, d->ai->targyaw, d->ai->targpitch);
        }

        if(dancing)
        {
            if(!d->ai->lastturn || lastmillis-d->ai->lastturn >= 1000)
            {
                d->ai->targyaw = rnd(360);
                d->ai->targpitch = rnd(91) - 45;
                d->ai->lastturn = lastmillis;
                if(rnd(d->skill*2) >= d->skill) d->ai->spot.z += rnd(int(d->height * 3 / 2));
            }
        }
        else if(A(d->actortype, abilities)&(1<<A_A_PRIMARY) || A(d->actortype, abilities)&(1<<A_A_SECONDARY))
        {
            gameent *e = game::getclient(d->ai->enemy);
            bool shootable = false;

            if(!wantitem)
            {   // allow a random intersection to cause us to get aggressive
                shootable = e && targetable(d, e, true);

                if(!shootable || d->skill >= rnd(101) || d->ai->dontmove)
                {
                    gameent *f = game::intersectclosest(d->o, d->ai->target, d);
                    if(f)
                    {
                        if(targetable(d, f, true))
                        {
                            if(!shootable)
                                violence(d, b, f, !d->ai->dontmove && (b.type != AI_S_DEFEND || b.targtype != AI_T_AFFINITY) && W2(d->weapselect, aidist, altfire(d, f)) < CLOSEDIST ? 1 : 0);
                            shootable = true;
                            e = f;
                        }
                        else shootable = false; // would hit non-targetable person
                    }
                    else if((!shootable || d->ai->dontmove) && target(d, b, 0, d->ai->dontmove && (b.type != AI_S_DEFEND || b.targtype != AI_T_AFFINITY)))
                        shootable = (e = game::getclient(d->ai->enemy)) != NULL;
                }
            }

            if(e)
            {
                bool alt = altfire(d, e);
                float yaw, pitch;
                game::getyawpitch(d->o, getaimpos(d, e, alt), yaw, pitch);

                bool insight = cansee(d, d->o, e->o), hasseen = d->ai->enemyseen && lastmillis - d->ai->enemyseen <= (d->skill * 10) + 1000;
                if(insight) d->ai->enemyseen = lastmillis;

                if(d->ai->dontmove || insight || hasseen)
                {
                    bool kamikaze = A(d->actortype, abilities)&(1<<A_A_KAMIKAZE);
                    frame *= insight || d->skill > 100 ? 1.5f : (hasseen ? 1.25f : 1.f);

                    if(kamikaze || d->o.dist(e->o) < CLOSEDIST)
                    {
                        frame *= 2.f;
                        d->ai->dontmove = false;

                        if((kamikaze || W2(d->weapselect, aidist, alt) < CLOSEDIST) && lockon(d, e, CLOSEDIST))
                        {
                            b.acttype = AI_A_LOCKON;
                            d->ai->targyaw = yaw;
                            d->ai->targpitch = pitch;
                            setspot(d, getbottom(e));
                        }
                    }

                    game::scaleyawpitch(d->yaw, d->pitch, yaw, pitch, frame * A(d->actortype, aiyawscale), frame * A(d->actortype, aipitchscale));

                    if(shootable)
                    {
                        if(kamikaze)
                        {
                            if(d->suicided >= 0) occupied = true;
                            else if(d->feetpos().dist(e->feetpos()) <= d->radius + e->radius + W2(W_GRENADE, radial, false) * 0.25f)
                            {
                                game::suicide(d);
                                occupied = true;
                            }
                        }

                        if(!occupied)
                        {
                            bool shoot = canshoot(d, e, alt);
                            if(d->action[alt ? AC_SECONDARY : AC_PRIMARY] && W2(d->weapselect, cooktime, alt) && W2(d->weapselect, cooked, alt))
                            { // TODO: make AI more aware of what they're shooting
                                int cooked = W2(d->weapselect, cooked, alt);
                                if(cooked&W_C_LIFEN) shoot = false; // inverted life
                            }
                            if(shoot && hastarget(d, b, e, alt, insight, yaw, pitch))
                            {
                                d->action[alt ? AC_SECONDARY : AC_PRIMARY] = true;
                                d->actiontime[alt ? AC_SECONDARY : AC_PRIMARY] = lastmillis;
                                firing = true;
                            }
                        }
                    }

                    occupied = true;
                }
            }
        }
        else occupied = false;

        if(!firing) d->action[AC_PRIMARY] = d->action[AC_SECONDARY] = false;

        if(d->actortype == A_JANITOR && !occupied)
        {
            float closedist = 1e16f;
            int closest = -1;
            loopv(projs::junkprojs)
            {
                projent &p = *projs::junkprojs[i];
                if(!p.isjunk(m_messy(game::gamemode, game::mutators), true)) continue;

                float dist = p.o.squaredist(d->muzzletag());
                if(dist > janitorsuckdist*janitorsuckdist || (closest >= 0 && dist >= closedist))
                    continue;

                closedist = dist;
                closest = i;
            }

            if(closest >= 0)
            {
                projent &p = *projs::junkprojs[closest];
                float yaw, pitch;
                game::getyawpitch(d->o, p.o, yaw, pitch);
                game::scaleyawpitch(d->yaw, d->pitch, yaw, pitch, frame * A(d->actortype, aiyawscale), frame * A(d->actortype, aipitchscale));
                occupied = true; // make janitors look at junk
            }
        }

        if(updatemovement(d, occupied))
        {
            if(d->canimpulse(IM_T_JUMP)) jumpto(d, b, d->ai->spot);

            bool crouch = d->actortype == A_TURRET || (d->ai->dontmove && (b.type != AI_S_OVERRIDE || b.overridetype == AI_O_CROUCH));
            if(d->action[AC_CROUCH] != crouch)
            {
                d->action[AC_CROUCH] = crouch;
                d->actiontime[AC_CROUCH] = crouch ? lastmillis : -lastmillis;
            }
        }

        if(dancing || !occupied || d->actortype == A_HAZARD)
        {
            if(dancing || actors[d->actortype].onlyfwd) frame *= 2;
            else if(!m_insta(game::gamemode, game::mutators))
            {
                int hp = max(d->gethealth(game::gamemode, game::mutators) / 3, 1);
                if(b.acttype == AI_A_NORMAL && (d->health <= hp || (iswaypoint(d->ai->targnode) && obstacles.find(d->ai->targnode, d))))
                    b.acttype = AI_A_HASTE;
                if(b.acttype == AI_A_HASTE) frame *= 1 + (hp / float(max(d->health, 1)));
            }

            game::scaleyawpitch(d->yaw, d->pitch, d->ai->targyaw, d->ai->targpitch, frame * A(d->actortype, aiyawscale), frame * A(d->actortype, aipitchscale));
        }
        else fixrange(d->yaw, d->pitch);

        safefindorientation(d->o, d->yaw, d->pitch, d->ai->target);
    }

    bool hasrange(gameent *d, gameent *e, int weap)
    {
        if(!targetable(d, e)) return false;
        loopk(2)
        {
            float dist = e->o.squaredist(d->o);
            if(weaprange(d, weap, k!=0, dist)) return true;
        }
        return false;
    }

    bool request(gameent *d, aistate &b)
    {
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        bool occupied = false, firing = false,
             haswaited = d->weapwaited(d->weapselect, lastmillis, (1<<W_S_RELOAD));

        bool wantitem = false;
        if(m_maxcarry(d->actortype, game::gamemode, game::mutators) && d->actortype < A_ENEMY && d->hasitems() && entities::ents.inrange(d->actitems[0].ent))
        {
            extentity &e = *entities::ents[d->actitems[0].ent];
            if(enttype[e.type].usetype == EU_ITEM && e.type == WEAPON && entities::isallowed(e))
            {
                int attr = m_attr(e.type, e.attrs[0]);
                if(isweap(attr) && d->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)) && wantsweap(d, attr, false))
                    wantitem = true;
            }
        }

        process(d, b, occupied, firing, wantitem);

        if(haswaited && !firing && !d->action[AC_USE] && wantitem)
        {
            d->action[AC_USE] = true;
            d->ai->lastaction = d->actiontime[AC_USE] = lastmillis;
        }

        bool timepassed = d->weapstate[d->weapselect] == W_S_IDLE && (d->weapammo[d->weapselect][W_A_CLIP] <= 0 || lastmillis-d->weaptime[d->weapselect] >= max(6000-(d->skill*50), W(d->weapselect, delayswitch)));

        if(!firing && (!occupied || d->weapammo[d->weapselect][W_A_CLIP] <= 0) && timepassed && d->hasweap(d->weapselect, sweap) && weapons::weapreload(d, d->weapselect))
        {
            d->ai->lastaction = lastmillis;
            return true;
        }

        if(d->actortype != A_HAZARD && !firing && timepassed)
        {
            int weap = weappref(d);
            gameent *e = game::getclient(d->ai->enemy);
            if(!isweap(weap) || !hasweap(d, weap) || (e && !hasrange(d, e, weap)))
            {
                weap = -1;
                loopj(2)
                {
                    loopv(d->loadweap) if(hasweap(d, d->loadweap[i]) && (j || !e || hasrange(d, e, d->loadweap[i])))
                    {
                        weap = d->loadweap[i];
                        break;
                    }
                    if(isweap(weap)) break;
                }
                if(!isweap(weap)) weap = d->bestweap(sweap, true, true);
            }

            if(isweap(weap) && weap != d->weapselect && weapons::weapselect(d, weap, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                d->ai->lastaction = lastmillis;
                return true;
            }
        }

        return occupied;
    }

    bool transport(gameent *d, int find = 0)
    {
        vec pos = getbottom(d);
        static vector<int> candidates; candidates.setsize(0);
        if(find) findwaypointswithin(pos, WAYPOINTRADIUS, RETRYDIST*find, candidates);
        if(find ? !candidates.empty() : !d->ai->route.empty()) loopk(2)
        {
            int best = -1;
            float dist = 1e16f;
            loopv(find ? candidates : d->ai->route)
            {
                int n = find ? candidates[i] : d->ai->route[i];
                if((k || (!d->ai->hasprevnode(n) && n != d->lastnode)) && !obstacles.find(n, d))
                {
                    float v = waypoints[n].o.squaredist(pos);
                    if(!iswaypoint(best) || v < dist)
                    {
                        best = n;
                        dist = v;
                    }
                }
            }
            if(iswaypoint(best))
            {
                d->o = waypoints[best].o;
                d->o.z += d->height;
                d->resetinterp();
                return true;
            }
        }
        if(find <= 1) return transport(d, find+1);
        return false;
    }

    void timeouts(gameent *d, aistate &b)
    {
        if(d->blocked)
        {
            d->ai->blocklast = lastmillis;
            d->ai->blocktime += lastmillis - d->ai->lastrun;
            if(d->ai->blocktime > (d->ai->blockseq + 1) * aitimeout)
            {
                d->ai->blockseq++;
                switch(d->ai->blockseq)
                {
                    case 1: break;
                    case 2:
                        d->ai->clear(d->ai->blockseq != 1);
                        if(d->ai->blockseq != 1 && iswaypoint(d->ai->targnode))
                        {
                            if(!d->ai->hasprevnode(d->ai->targnode)) d->ai->addprevnode(d->ai->targnode);
                            wpavoid.avoidnear(NULL, waypoints[d->ai->targnode].o.z + WAYPOINTRADIUS, waypoints[d->ai->targnode].o, WAYPOINTRADIUS);
                        }
                        break;
                    case 3: if(!transport(d)) d->ai->reset(false); break;
                    case 4: default:
                        if(b.type != AI_S_WAIT) { game::suicide(d, HIT_LOST); return; } // this is our last resort..
                        else d->ai->blockseq = 0; // waiting, so just try again..
                        break;
                }
                if(aidebug >= 7 && dbgfocus(d))
                    conoutf(colourwhite, "%s blocked %dms sequence %d", game::colourname(d), d->ai->blocktime, d->ai->blockseq);
            }
        }
        else d->ai->blocktime = d->ai->blockseq = 0;

        if(iswaypoint(d->ai->targnode) && (d->ai->targnode == d->ai->targlast || d->ai->hasprevnode(d->ai->targnode)))
        {
            d->ai->targtime += lastmillis - d->ai->lastrun;
            if(d->ai->targtime > (d->ai->targseq + 1) * aitimeout)
            {
                d->ai->targseq++;
                switch(d->ai->targseq)
                {
                    case 1: break;
                    case 2:
                        d->ai->clear(d->ai->targseq != 1);
                        if(iswaypoint(d->ai->targnode) && !d->ai->hasprevnode(d->ai->targnode))
                            d->ai->addprevnode(d->ai->targnode);
                        break;
                    case 3: if(!transport(d)) d->ai->reset(false); break;
                    case 4: default:
                        if(b.type != AI_S_WAIT) { game::suicide(d, HIT_LOST); return; } // this is our last resort..
                        else d->ai->blockseq = 0; // waiting, so just try again..
                        break;
                }
                if(aidebug >= 7 && dbgfocus(d))
                    conoutf(colourwhite, "%s targeted %d too long %dms sequence %d", game::colourname(d), d->ai->targnode, d->ai->targtime, d->ai->targseq);
            }
        }
        else
        {
            d->ai->targtime = d->ai->targseq = 0;
            d->ai->targlast = d->ai->targnode;
        }
    }

    void logic(gameent *d, aistate &b)
    {
        if(d->state != CS_ALIVE || !game::allowmove(d)) d->stopmoving(true);
        else
        {
            if(!request(d, b)) target(d, b, W2(d->weapselect, aidist, false) < CLOSEDIST ? 1 : 0);
            weapons::shoot(d, d->ai->target);
        }

        if(d->actortype == A_HAZARD) return;

        if(d->state == CS_DEAD || d->state == CS_WAITING)
        {
            if(d->ragdoll) moveragdoll(d);
            else if(lastmillis-d->lastpain < 5000)
                physics::move(d, 1, false);
        }
        else
        {
            if(d->ragdoll) cleanragdoll(d);
            if(d->state == CS_ALIVE && gs_playing(game::gamestate))
            {
                physics::move(d, 1, true);
                if(A(d->actortype, abilities)&(1<<A_A_MOVE) && !d->ai->dontmove) timeouts(d, b);
            }
        }
        if(gs_playing(game::gamestate) && (d->state == CS_ALIVE || d->state == CS_DEAD || d->state == CS_WAITING))
            entities::checkitems(d);
    }

    void avoid()
    {
        obstacles.clear();
        obstacles.add(wpavoid);

        int numdyns = game::numdynents(1);
        loopi(numdyns)
        {
            dynent *d = game::iterdynents(i, 1);
            if(!d || !d->isalive() || !physics::issolid(d)) continue;
            if(projent::is(d))
            {
                projent *p = (projent *)d;
                if(p->projtype != PROJ_SHOT) continue;
                float expl = WX(WK(p->flags), p->weap, radial, WS(p->flags), game::gamemode, game::mutators, p->curscale);
                if(expl > 0) obstacles.avoidnear(p, p->o.z + expl + 1, p->o, WAYPOINTRADIUS + expl + 1);
            }
            else if(gameent::is(d))
            {
                gameent *e = (gameent *)d;
                obstacles.avoidnear(d, d->o.z + d->height + d->aboveeye + 1, getbottom(e), WAYPOINTRADIUS + d->radius + 1);
            }
        }

        #if 0
        loopenti(MAPMODEL) if(entities::ents[i]->type == MAPMODEL)
        {
            gameentity &e = *(gameentity *)entities::ents[i];
            if(e.attrs[6]&(1<<MDLF_NOCLIP) || !mapmodelvisible(e, i, 2)) continue;
            mapmodelinfo *mmi = getmminfo(e.attrs[0]);
            if(!mmi || !mmi->m || mmi->m->collide == COLLIDE_NONE|| mmi->m->collide == COLLIDE_TRI) continue;
            vec center, radius;
            mmi->m->collisionbox(center, radius);
            if(e.attrs[5])
            {
                float scale = e.attrs[5]/100.f;
                center.mul(scale);
                radius.mul(scale);
            }
            rotatebb(center, radius, int(e.attrs[1]), int(e.attrs[2]), int(e.attrs[3]));
            float xy = max(radius.x, radius.y), z = radius.z;
            xy += WAYPOINTRADIUS;
            z += WAYPOINTRADIUS;
            obstacles.avoidnear(NULL, e.o.z + z + 1, e.o, xy + 1);
        }
        #endif
    }

    void think(gameent *d, bool run)
    {
        // the state stack works like a chain of commands, certain commands simply replace each other
        // others spawn new commands to the stack the ai reads the top command from the stack and executes
        // it or pops the stack and goes back along the history until it finds a suitable command to execute
        bool cleannext = false;
        if(d->ai->state.empty()) d->ai->addstate(AI_S_WAIT);
        loopvrev(d->ai->state)
        {
            aistate &c = d->ai->state[i];
            if(c.owner >= 0)
            {
                gameent *e = game::getclient(c.owner);
                if(!e || e->team != d->team) c.owner = -1;
            }
            if(cleannext)
            {
                c.millis = lastmillis;
                c.override = false;
                cleannext = false;
            }
            if(d->state == CS_ALIVE && run)
            {
                bool result = false;
                c.acttype = m_insta(game::gamemode, game::mutators) ? AI_A_HASTE : AI_A_NORMAL;
                switch(c.type)
                {
                    case AI_S_WAIT: result = dowait(d, c); break;
                    case AI_S_DEFEND: result = dodefense(d, c); break;
                    case AI_S_PURSUE: result = dopursue(d, c); break;
                    case AI_S_INTEREST: result = dointerest(d, c); break;
                    case AI_S_OVERRIDE: result = dooverride(d, c); break;
                    default: result = false; break;
                }
                if(!result && c.type != AI_S_WAIT && c.owner < 0)
                {
                    d->ai->removestate(i);
                    cleannext = true;
                    continue; // logic is run on working states
                }
            }
            logic(d, c);
            break;
        }
        if(d->ai->tryreset) d->ai->reset();
        d->ai->lastrun = lastmillis;
    }

    void drawroute(gameent *d, float amt)
    {
        int colour = game::getcolour(d, game::playerdisplaytone, game::playerdisplaytonelevel, game::playerdisplaytonemix), last = -1;
        loopvrev(d->ai->route)
        {
            if(d->ai->route.inrange(last))
            {
                int index = d->ai->route[i], prev = d->ai->route[last];
                if(iswaypoint(index) && iswaypoint(prev))
                {
                    waypoint &w = waypoints[index], &v = waypoints[prev];
                    vec fr = v.o, dr = w.o;
                    fr.z += amt; dr.z += amt;
                    part_trace(fr, dr, 1, 1, 1, colour);
                }
            }
            last = i;
        }
        if(aidebug >= 5)
        {
            vec pos = getbottom(d);
            if(!d->ai->spot.iszero()) part_trace(pos, d->ai->spot, 1, 1, 1, colourcyan);
            if(iswaypoint(d->ai->targnode)) part_trace(pos, waypoints[d->ai->targnode].o, 1, 1, 1, colourpurple);
            if(iswaypoint(d->lastnode)) part_trace(pos, waypoints[d->lastnode].o, 1, 1, 1, colouryellow);
            loopi(NUMPREVNODES) if(iswaypoint(d->ai->prevnodes[i]))
            {
                part_trace(pos, waypoints[d->ai->prevnodes[i]].o, 1, 1, 1, colourbrown);
                pos = waypoints[d->ai->prevnodes[i]].o;
            }
        }
    }

    const char *stnames[AI_S_MAX] = {
        "wait", "defend", "pursue", "interest", "override"
    }, *sttypes[AI_T_MAX+1] = {
        "none", "node", "actor", "affinity", "entity", "drop"
    }, *attypes[AI_A_MAX] = {
        "normal", "idle", "lockon", "protect", "haste"
    };
    void render()
    {
        if(drawtex) return;
        if(aidebug >= 2)
        {
            int total = 0, alive = 0;
            loopv(game::players) if(game::players[i] && dbgfocus(game::players[i])) total++;
            loopv(game::players) if(game::players[i] && game::players[i]->state == CS_ALIVE && dbgfocus(game::players[i]))
            {
                gameent *d = game::players[i];
                vec pos = d->abovehead();
                pos.z += 3;
                alive++;
                if(aidebug >= 4 && A(d->actortype, abilities)&(1<<A_A_MOVE)) drawroute(d, 4.f*(alive/float(total)));
                if(aidebug >= 3)
                {
                    defformatstring(q, "node: %d route: %d (%d)",
                        d->lastnode,
                        !d->ai->route.empty() ? d->ai->route[0] : -1,
                        d->ai->route.length()
                    );
                    part_textcopy(pos, q);
                    pos.z += 2;
                }
                bool top = true;
                loopvrev(d->ai->state)
                {
                    aistate &b = d->ai->state[i];
                    gameent *e = b.owner >= 0 ? game::getclient(b.owner) : NULL;
                    defformatstring(s, "%s%s (%s) %s:%d (\fs%s%s\fS%s%s%s)",
                        top ? "<default>\fg" : "<sub>\fa",
                        stnames[b.type],
                        timestr(lastmillis-b.millis),
                        sttypes[b.targtype+1], b.target,
                        top ? "\fc" : "", attypes[b.acttype],
                        e ? " [" : "", e ? game::colourname(e) : "", e ? "]" : ""
                    );
                    part_textcopy(pos, s);
                    pos.z += 2;
                    if(top)
                    {
                        if(aidebug >= 3) top = false;
                        else break;
                    }
                }
                if(aidebug >= 3)
                {
                    if(isweap(weappref(d)))
                    {
                        part_textcopy(pos, W(weappref(d), name));
                        pos.z += 2;
                    }
                    gameent *e = game::getclient(d->ai->enemy);
                    if(e)
                    {
                        part_textcopy(pos, game::colourname(e));
                        pos.z += 2;
                    }
                }
            }
            if(aidebug >= 4)
            {
                int cur = 0;
                loopv(obstacles.obstacles)
                {
                    const avoidset::obstacle &ob = obstacles.obstacles[i];
                    int next = cur + ob.numwaypoints;
                    for(; cur < next; cur++)
                    {
                        int ent = obstacles.waypoints[cur];
                        if(iswaypoint(ent))
                            part_create(PART_ENTITY, 1, waypoints[ent].o, colourpurple, 0.5f);
                    }
                    cur = next;
                }
            }
        }
        if(showwaypoints || (dropwaypoints && showwaypointsdrop) || aidebug >= 7)
        {
            vector<int> close;
            int len = waypoints.length(), col = vec::fromcolor(showwaypointscolour).mul(0.5f).tohexcolor();
            if(showwaypointsradius)
            {
                findwaypointswithin(camera1->o, 0, showwaypointsradius, close);
                len = close.length();
            }
            loopi(len)
            {
                int idx = showwaypointsradius ? close[i] : i;
                waypoint &w = waypoints[idx];
                if(!w.haslinks()) part_create(PART_ENTITY, 1, w.o, showwaypointscolour, 1.f);
                else loopj(MAXWAYPOINTLINKS)
                {
                     int link = w.links[j];
                     if(!link) break;
                     waypoint &v = waypoints[link];
                     bool both = false;
                     loopk(MAXWAYPOINTLINKS) if(v.links[k] == idx) { both = true; break; }
                     part_trace(w.o, v.o, 1, 1, 1, both ? showwaypointscolour : col);
                }
            }
            if(game::player1->state == CS_ALIVE && iswaypoint(game::player1->lastnode))
                part_trace(game::player1->feetpos(), waypoints[game::player1->lastnode].o, 1, 1, 1, colouryellow);
        }
    }

    void preload()
    {
    }

    void botsay(gameent *d, gameent *t, const char *fmt, ...)
    {
        if(!d || !t) return;
        defvformatbigstring(msg, fmt, fmt);
        client::addmsg(N_TEXT, "ri3s", d->clientnum, t->clientnum, SAY_WHISPER, msg);
    }

    static const int AFFIRMNUM = 5;
    static const char *affirm[AFFIRMNUM] = { "Roger", "Okay", "OK", "Will do", "I'm on it" };
    static const int QUIPFORGETNUM = 4;
    static const char *quip_forget[QUIPFORGETNUM] = { "Back to what I was doing then", "Resuming previous operations", "I am no longer your slave", "Jolly good show then" };
    static const int QUIPRESETNUM = 4;
    static const char *quip_reset[QUIPRESETNUM] = { "What was I doing again?", "Duh.. off I go..", "Who were you again?", "Ummmm.. wtf do I do now?" };

    void scanchat(gameent *d, gameent *t, int flags, const char *text)
    {
        if(flags&SAY_ACTION || d->actortype != A_PLAYER) return;
        bigstring msg;
        filterstring(msg, text, true, true, true, true);

        const int MAXWORDS = 8;
        int numargs = MAXWORDS;
        char *w[MAXWORDS];
        const char *p = (const char *)msg;
        loopi(MAXWORDS)
        {
            w[i] = (char *)"";
            if(i > numargs) continue;
            char *s = parsetext(p);
            if(s) w[i] = s;
            else numargs = i;
        }

        if(*w[0]) loopvj(game::players) if(game::players[j] && game::players[j]->actortype == A_BOT && game::players[j]->ai)
        {
            gameent *e = game::players[j];
            int pos = 0;
            bool aimed = false;
            if(!(flags&SAY_WHISPER))
            {
                if(!strncmp(w[0], "bots", 4)) pos = 1;
                else
                {
                    size_t len = strlen(e->name);
                    if(!len || strncasecmp(w[0], e->name, len)) continue;
                    switch(w[0][len])
                    {
                        case 0: break;
                        case ':': case ',': case ';': len++; break;
                        default: continue;
                    }
                    if(w[0][len] != 0) continue;
                    pos = 1;
                    aimed = true;
                }
            }
            else aimed = true;

            if(aimed && t != e) continue;
            else if(!m_edit(game::gamemode) && d->team != e->team && (!aimed || !client::haspriv(d, botoverridelock)))
            {
                if(aimed) botsay(e, d, "Sorry, I can't obey you due to commands being locked on this server");
                continue;
            }

            if(!strcasecmp(w[pos], "defend"))
            {
                pos++;
                if(!strcasecmp(w[pos], "me"))
                {
                    e->ai->clear();
                    e->ai->addstate(AI_S_DEFEND, AI_T_ACTOR, d->clientnum, AI_A_PROTECT, d->clientnum);
                    botsay(e, d, "%s, defending you", affirm[rnd(AFFIRMNUM)]);
                }
                else if(!strcasecmp(w[pos], "here"))
                {
                    e->ai->clear();
                    e->ai->addstate(AI_S_DEFEND, AI_T_NODE, e->lastnode, AI_A_PROTECT, d->clientnum);
                    botsay(e, d, "%s, defending your position", affirm[rnd(AFFIRMNUM)]);
                }
                else
                {
                    gameent *f = game::getclient(client::parseplayer(w[pos]));
                    if(f && f != e && f->team == e->team)
                    {
                        e->ai->clear();
                        e->ai->addstate(AI_S_DEFEND, AI_T_ACTOR, f->clientnum, AI_A_PROTECT, d->clientnum);
                        botsay(e, d, "%s, defending %s", affirm[rnd(AFFIRMNUM)], f->name);
                    }
                    else
                    {
                        if(!strcasecmp(w[pos], "the")) pos++;
                        switch(game::gamemode)
                        {
                            case G_CAPTURE:
                            {
                                if(!strcasecmp(w[pos], "flag"))
                                {
                                    loopv(capture::st.flags) if(capture::st.flags[i].team == e->team)
                                    {
                                        e->ai->clear();
                                        e->ai->addstate(AI_S_DEFEND, AI_T_AFFINITY, i, AI_A_PROTECT, d->clientnum);
                                        botsay(e, d, "%s, defending the flag", affirm[rnd(AFFIRMNUM)]);
                                        break;
                                    }
                                }
                                else botsay(e, d, "Usage: me, here, or flag");
                                break;
                            }
                            case G_BOMBER:
                            {
                                if(!strcasecmp(w[pos], "goal") || !strcasecmp(w[pos], "base"))
                                {
                                    loopv(bomber::st.flags) if(!isbomberaffinity(bomber::st.flags[i]) && bomber::st.flags[i].team == e->team)
                                    {
                                        e->ai->clear();
                                        e->ai->addstate(AI_S_DEFEND, AI_T_AFFINITY, i, AI_A_PROTECT, d->clientnum);
                                        botsay(e, d, "%s, defending the goal", affirm[rnd(AFFIRMNUM)]);
                                        break;
                                    }
                                }
                                else botsay(e, d, "Usage: me, here, or goal");
                                break;
                            }
                            default: botsay(e, d, "Usage: me, or here"); break;
                        }
                    }
                }
            }
            else if(!strcasecmp(w[pos], "attack"))
            {
                pos++;
                if(!strcasecmp(w[pos], "the")) pos++;
                switch(game::gamemode)
                {
                    case G_CAPTURE:
                    {
                        if(!strcasecmp(w[pos], "flag"))
                        {
                            loopv(capture::st.flags) if(capture::st.flags[i].team != e->team)
                            {
                                e->ai->clear();
                                e->ai->addstate(AI_S_PURSUE, AI_T_AFFINITY, i, AI_A_HASTE, d->clientnum);
                                botsay(e, d, "%s, attacking the flag", affirm[rnd(AFFIRMNUM)]);
                                break;
                            }
                        }
                        else botsay(e, d, "Sorry, flag is the only option");
                        break;
                    }
                    case G_BOMBER:
                    {
                        if(!strcasecmp(w[pos], "goal") || !strcasecmp(w[pos], "base"))
                        {
                            loopv(bomber::st.flags) if(!isbomberaffinity(bomber::st.flags[i]) && bomber::st.flags[i].team != e->team)
                            {
                                e->ai->clear();
                                e->ai->addstate(AI_S_PURSUE, AI_T_AFFINITY, i, AI_A_HASTE, d->clientnum);
                                botsay(e, d, "%s, attacking the goal", affirm[rnd(AFFIRMNUM)]);
                                break;
                            }
                        }
                        else if(!strcasecmp(w[pos], "ball") || !strcasecmp(w[pos], "bomb"))
                        {
                            loopv(bomber::st.flags) if(isbomberaffinity(bomber::st.flags[i]))
                            {
                                e->ai->clear();
                                e->ai->addstate(AI_S_PURSUE, AI_T_AFFINITY, i, AI_A_HASTE, d->clientnum);
                                botsay(e, d, "%s, attacking the ball", affirm[rnd(AFFIRMNUM)]);
                                break;
                            }
                        }
                        else botsay(e, d, "Usage: goal, or ball");
                        break;
                    }
                    default: botsay(e, d, "Sorry, no attack directions in this game type"); break;
                }
            }
            else if(!strcasecmp(w[pos], "stand") || !strcasecmp(w[pos], "crouch") || !strcasecmp(w[pos], "dance"))
            {
                int state = AI_O_STAND, act = AI_A_NORMAL;
                const char *cmd = "stand";
                if(!strcasecmp(w[pos], "crouch"))
                {
                    state = AI_O_CROUCH;
                    cmd = "crouch";
                }
                else if(!strcasecmp(w[pos], "dance"))
                {
                    state = AI_O_DANCE;
                    act = AI_A_HASTE;
                    cmd = "dance";
                }
                pos++;
                if(!strcasecmp(w[pos], "here"))
                {
                    e->ai->clear();
                    e->ai->addstate(AI_S_OVERRIDE, AI_T_NODE, d->lastnode, act, d->clientnum, state);
                    botsay(e, d, "%s, will %s at your position (%d)", affirm[rnd(AFFIRMNUM)], cmd, d->lastnode);
                }
                else if(!strcasecmp(w[pos], "with") || !strcasecmp(w[pos], "next"))
                {
                    pos++;
                    if(!strcasecmp(w[pos], "me"))
                    {
                        e->ai->clear();
                        e->ai->addstate(AI_S_OVERRIDE, AI_T_ACTOR, d->clientnum, act, d->clientnum, state);
                        botsay(e, d, "%s, will %s next to you", affirm[rnd(AFFIRMNUM)], cmd);
                    }
                    else
                    {
                        gameent *f = game::getclient(client::parseplayer(w[pos]));
                        if(f && f != e)
                        {
                            e->ai->clear();
                            e->ai->addstate(AI_S_OVERRIDE, AI_T_ACTOR, f->clientnum, act, d->clientnum, state);
                            botsay(e, d, "%s, will %s next to %s", affirm[rnd(AFFIRMNUM)], cmd, f->name);
                        }
                    }
                }
            }
            else if(!strcasecmp(w[pos], "forget"))
            {
                loopvrev(e->ai->state) if(e->ai->state[i].owner == d->clientnum) e->ai->state.remove(i);
                botsay(e, d, "%s, %s", affirm[rnd(AFFIRMNUM)], quip_forget[rnd(QUIPFORGETNUM)]);
            }
            else if(!strcasecmp(w[pos], "reset"))
            {
                e->ai->reset(true, false);
                botsay(e, d, "%s, %s", affirm[rnd(AFFIRMNUM)], quip_reset[rnd(QUIPRESETNUM)]);
            }
            else botsay(e, d, "Usage: defend, attack, stand, crouch, dance, forget, or reset");
        }
        loopi(numargs) DELETEA(w[i]);
    }

}
