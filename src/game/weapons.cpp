#include "game.h"
namespace weapons
{
    VAR(IDF_PERSIST, weapautoreload, 0, 2, 4); // 0 = never, 1 = when empty, 2 = weapons that don't add a full clip, 3 = always (+1 zooming weaps too)
    VAR(IDF_PERSIST, weapautoreloaddelay, 0, 0, VAR_MAX);
    VAR(IDF_PERSIST, weapautoswitch, 0, 1, 1); // 0 = never, 1 = when empty

    VAR(IDF_PERSIST, weapskippickup, 0, 0, 1);
    VAR(IDF_PERSIST, weapskipempty, 0, 1, 1);

    int lastweapselect = 0;
    VAR(IDF_PERSIST, weapselectdelay, 0, 200, VAR_MAX);

    vector<int> weaplist;
    void buildweaplist(const char *str)
    {
        vector<char *> list;
        explodelist(str, list);
        weaplist.shrink(0);
        loopv(list)
        {
            int weap = -1;
            if(isnumeric(list[i][0])) weap = atoi(list[i]);
            else loopj(W_ALL) if(!strcasecmp(weaptype[j].name, list[i]))
            {
                weap = j;
                break;
            }
            if(isweap(weap) && weaplist.find(weap) < 0)
                weaplist.add(weap);
        }
        list.deletearrays();
        loopi(W_ALL) if(weaplist.find(i) < 0) weaplist.add(i); // make sure all weapons have a slot
        changedkeys = lastmillis;
    }
    SVARF(IDF_PERSIST, weapselectlist, "", buildweaplist(weapselectlist));
    VARF(IDF_PERSIST, weapselectslot, 0, 1, 2, buildweaplist(weapselectlist)); // 0 = by id, 1 = by slot, 2 = by list

    int slot(gameent *d, int n, bool back)
    {
        if(!d || !weapselectslot) return n;
        if(weapselectslot == 2 && weaplist.empty()) buildweaplist(weapselectlist);
        int p = m_weapon(d->actortype, game::gamemode, game::mutators), w = 0;
        loopi(W_ALL)
        {
            int weap = weapselectslot == 2 ? weaplist[i] : i;
            if(d->holdweap(weap, p, lastmillis))
            {
                if(n == (back ? w : weap)) return back ? weap : w;
                w++;
            }
        }
        return -1;
    }

    ICOMMAND(0, weapslot, "i", (int *o), intret(slot(game::player1, *o >= 0 ? *o : game::player1->weapselect))); // -1 = weapselect slot
    ICOMMAND(0, weapselect, "", (), intret(game::player1->weapselect));
    ICOMMAND(0, ammo, "i", (int *n, int *m), intret(isweap(*n) ? game::player1->weapammo[*n][clamp(*m, 0, W_A_MAX-1)] : -1));
    ICOMMAND(0, ammoclip, "i", (int *n), intret(isweap(*n) ? game::player1->weapammo[*n][W_A_CLIP] : -1));
    ICOMMAND(0, ammostore, "i", (int *n), intret(isweap(*n) ? game::player1->weapammo[*n][W_A_STORE] : -1));
    ICOMMAND(0, reloadweap, "i", (int *n), intret(w_reload(*n) ? 1 : 0));
    ICOMMAND(0, hasweap, "ii", (int *n, int *o), intret(isweap(*n) && game::player1->hasweap(*n, *o) ? 1 : 0));
    ICOMMAND(0, getweap, "ii", (int *n, int *o),
    {
        if(isweap(*n)) switch(*o)
        {
            case -1: result(weaptype[*n].name); break;
            case 0: result(W(*n, name)); break;
            case 1: result(hud::itemtex(WEAPON, *n)); break;
            default: break;
        }
    });

    bool weapselect(gameent *d, int weap, int filter, bool local)
    {
        if(!gs_playing(game::gamestate)) return false;
        bool newoff = false;
        int oldweap = d->weapselect;
        if(local)
        {
            int interrupts = filter;
            interrupts &= ~(1<<W_S_RELOAD);
            if(!d->canswitch(weap, m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis, interrupts))
            {
                if(!d->canswitch(weap, m_weapon(d->actortype, game::gamemode, game::mutators), lastmillis, filter)) return false;
                else if(!isweap(oldweap) || d->weapload[oldweap][W_A_CLIP] <= 0) return false;
                else newoff = true;
            }
        }
        if(!d->weapswitch(weap, lastmillis, W(weap, delayswitch))) return false;
        if(local)
        {
            if(newoff)
            {
                int offset = d->weapload[oldweap][W_A_CLIP];
                d->weapammo[oldweap][W_A_CLIP] = max(d->weapammo[oldweap][W_A_CLIP]-offset, 0);
                if(W(oldweap, ammostore)) d->weapammo[oldweap][W_A_STORE] = clamp(d->weapammo[oldweap][W_A_STORE]+offset, 0, W(oldweap, ammostore));
                d->weapload[oldweap][W_A_CLIP] = -d->weapload[oldweap][W_A_CLIP];
            }
            client::addmsg(N_WEAPSELECT, "ri3", d->clientnum, lastmillis-game::maptime, weap);
        }
        playsound(WSND(weap, S_W_SWITCH), d->o, d, 0, -1, -1, -1, &d->wschan);
        return true;
    }

    bool weapreload(gameent *d, int weap, int load, int ammo, int store, bool local)
    {
        if(!gs_playing(game::gamestate) || (!local && (d == game::player1 || d->ai))) return false; // this can't be fixed until 1.5
        if(local)
        {
            if(!d->canreload(weap, m_weapon(d->actortype, game::gamemode, game::mutators), true, lastmillis))
            {
                if(d->weapstate[weap] == W_S_POWER) d->setweapstate(weap, W_S_WAIT, PHYSMILLIS, lastmillis);
                return false;
            }
            client::addmsg(N_RELOAD, "ri3", d->clientnum, lastmillis-game::maptime, weap);
            int oldammo = max(d->weapammo[weap][W_A_CLIP], 0), ammoadd = W(weap, ammoadd);
            if(d->actortype < A_ENEMY && W(weap, ammostore))
            {
                store = d->weapammo[weap][W_A_STORE];
                if(!w_reload(weap)) ammoadd = min(store, ammoadd);
            }
            ammo = min(oldammo+ammoadd, W(weap, ammoclip));
            int diff = ammo-oldammo;
            if(W(weap, ammostore)) store = clamp(store-diff, 0, W(weap, ammostore));
            load = diff;
        }
        d->weapload[weap][W_A_CLIP] = load;
        d->weapammo[weap][W_A_CLIP] = min(ammo, W(weap, ammoclip));
        if(W(weap, ammostore)) d->weapammo[weap][W_A_STORE] = clamp(store, 0, W(weap, ammostore));
        playsound(WSND(weap, S_W_RELOAD), d->o, d, 0, -1, -1, -1, &d->wschan);
        d->setweapstate(weap, W_S_RELOAD, W(weap, delayreload), lastmillis);
        return true;
    }

    void weaponswitch(gameent *d, int a = -1, int b = -1)
    {
        if(!gs_playing(game::gamestate) || a < -1 || b < -1 || a >= W_ALL || b >= W_ALL) return;
        if(weapselectdelay && lastweapselect && totalmillis-lastweapselect < weapselectdelay) return;
        if(d->weapwaited(d->weapselect, lastmillis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
        {
            int s = slot(d, d->weapselect), w = m_weapon(d->actortype, game::gamemode, game::mutators);
            loopi(W_ALL) // only loop the amount of times we have weaps for
            {
                if(a >= 0) s = a;
                else s += b;
                while(s >= W_ALL) s -= W_ALL;
                while(s < 0) s += W_ALL;
                int n = slot(d, s, true);
                if(a < 0 && weapskipempty && !d->hasweap(n, w, 3)) continue; // skip empty when scrolling
                if(weapselect(d, n, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
                {
                    lastweapselect = totalmillis ? totalmillis : 1;
                    return;
                }
                else if(a >= 0) break;
            }
        }
        game::errorsnd(d);
    }
    ICOMMAND(0, weapon, "ss", (char *a, char *b), weaponswitch(game::player1, *a ? parseint(a) : -1, *b ? parseint(b) : -1));

    void weapdrop(gameent *d, int w)
    {
        if(!gs_playing(game::gamestate)) return;
        int weap = isweap(w) ? w : d->weapselect, sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        d->action[AC_DROP] = false;
        if(!d->candrop(weap, sweap, lastmillis, m_classic(game::gamemode, game::mutators), (1<<W_S_SWITCH)))
        {
            if(!d->candrop(weap, sweap, lastmillis, m_classic(game::gamemode, game::mutators), (1<<W_S_SWITCH)|(1<<W_S_RELOAD)) || !isweap(d->weapselect) || d->weapload[d->weapselect][W_A_CLIP] <= 0)
            {
                game::errorsnd(d);
                return;
            }
            else
            {
                int offset = d->weapload[d->weapselect][W_A_CLIP];
                d->weapammo[d->weapselect][W_A_CLIP] = max(d->weapammo[d->weapselect][W_A_CLIP]-offset, 0);
                if(W(d->weapselect, ammostore)) d->weapammo[d->weapselect][W_A_STORE] = clamp(d->weapammo[d->weapselect][W_A_STORE]+offset, 0, W(d->weapselect, ammostore));
                d->weapload[d->weapselect][W_A_CLIP] = -d->weapload[d->weapselect][W_A_CLIP];
            }
        }
        client::addmsg(N_WEAPDROP, "ri3", d->clientnum, lastmillis-game::maptime, weap);
        d->setweapstate(weap, W_S_WAIT, PHYSMILLIS, lastmillis);
    }

    bool autoreload(gameent *d, int flags = 0)
    {
        if(gs_playing(game::gamestate) && d == game::player1 && W2(d->weapselect, ammosub, WS(flags)) && d->canreload(d->weapselect, m_weapon(d->actortype, game::gamemode, game::mutators), true, lastmillis))
        {
            bool noammo = d->weapammo[d->weapselect][W_A_CLIP] < W2(d->weapselect, ammosub, WS(flags)),
                 noattack = !d->action[AC_PRIMARY] && !d->action[AC_SECONDARY];
            if((noammo || noattack) && !d->action[AC_USE] && d->weapstate[d->weapselect] == W_S_IDLE && (noammo || lastmillis-d->weaptime[d->weapselect] >= weapautoreloaddelay ))
                return weapautoreload >= (noammo ? 1 : (W(d->weapselect, ammoadd) < W(d->weapselect, ammoclip) ? 2 : (W2(d->weapselect, cooked, true)&W_C_ZOOM ? 4 : 3)));
        }
        return false;
    }

    void checkweapons(gameent *d)
    {
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        if(!d->hasweap(d->weapselect, sweap, weapautoswitch ? 3 : 0)) weapselect(d, d->bestweap(sweap, true, true, d->weapselect), 1<<W_S_RELOAD, true);
        else if(d->action[AC_RELOAD] || autoreload(d)) weapreload(d, d->weapselect);
        else if(d->action[AC_DROP]) weapdrop(d, d->weapselect);
    }

    void offsetray(vec &from, vec &to, float spread, float z, vec &dest)
    {
        float f = to.dist(from)*spread/10000.f;
        for(;;)
        {
            #define RNDD rnd(101)-50
            vec v(RNDD, RNDD, RNDD);
            if(v.magnitude() > 50) continue;
            v.mul(f);
            v.z = z > 0 ? v.z/z : 0;
            dest = to;
            dest.add(v);
            if(dest != from)
            {
                vec dir = vec(dest).sub(from).normalize();
                raycubepos(from, dir, dest, 0, RAY_CLIPMAT|RAY_ALPHAPOLY);
            }
            return;
        }
    }

    float accmodspread(gameent *d, int weap, bool secondary, bool zooming)
    {
        float r = 0;
        bool running = d->running(moveslow), moving = d->move || d->strafe;
        if(running || moving) r += running ? W2(weap, spreadrunning, secondary) : W2(weap, spreadmoving, secondary);
        else if(zooming) r += W2(weap, spreadzoom, true);
        else if(d->crouching()) r += W2(weap, spreadcrouch, secondary);
        else r += W2(weap, spreadstill, secondary);
        if(W2(weap, spreadinair, secondary) > 0 && d->airmillis && !d->onladder) r += W2(weap, spreadinair, secondary);
        return r;
    }

    void accmodjitter(gameent *d, int weap, bool secondary, bool zooming, int &jittertime, float &jitteryawmin, float &jitteryawmax, float &jitterpitchmin, float &jitterpitchmax)
    {
        bool running = d->running(moveslow), moving = d->move || d->strafe;
        #define MODSPREAD(name, value) \
            if(running || moving) name##value = name##value*(running ? W2(weap, name##running, secondary) : W2(weap, name##moving, secondary)); \
            else if(zooming) name##value = name##value*(W2(weap, name##zoom, true)); \
            else if(d->crouching()) name##value = name##value*(W2(weap, name##crouch, secondary)); \
            else name##value = name##value*(W2(weap, name##still, secondary)); \
            if(W2(weap, name##inair, secondary) > 0 && d->airmillis && !d->onladder) name##value += name##value*(W2(weap, name##inair, secondary));
        MODSPREAD(jittertime, );
        MODSPREAD(jitteryaw, min);
        MODSPREAD(jitteryaw, max);
        MODSPREAD(jitterpitch, min);
        MODSPREAD(jitterpitch, max);
    }

    bool doshot(gameent *d, vec &targ, int weap, bool pressed, bool secondary, int force, gameent *v)
    {
        bool hadcook = W2(weap, cooked, true)&W_C_KEEP && (d->prevstate[weap] == W_S_ZOOM || d->prevstate[weap] == W_S_POWER),
             zooming = W2(weap, cooked, true)&W_C_ZOOM && (d->weapstate[weap] == W_S_ZOOM || (pressed && secondary) || (hadcook && d->prevstate[weap] == W_S_ZOOM)), wassecond = secondary;
        if(hadcook || zooming)
        {
            if(!pressed)
            {
                client::addmsg(N_WEAPCOOK, "ri5", d->clientnum, lastmillis-game::maptime, weap, -1, 0);
                d->setweapstate(weap, W_S_IDLE, 0, lastmillis, 0, true);
                return false;
            }
            else secondary = zooming;
        }
        int offset = 0, sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        if(!d->canshoot(weap, secondary ? HIT(ALT) : 0, sweap, lastmillis))
        {
            if(!d->canshoot(weap, secondary ? HIT(ALT) : 0, sweap, lastmillis, (1<<W_S_RELOAD)))
            {
                // if the problem is not enough ammo, do the reload..
                if(autoreload(d, secondary ? HIT(ALT) : 0)) weapreload(d, weap);
                else if(pressed && d->getammo(weap, 0, true) <= 0) game::errorsnd(d);
                return false;
            }
            else if(d->weapload[weap][W_A_CLIP] <= 0 || weap != d->weapselect) return false;
            else offset = d->weapload[weap][W_A_CLIP];
        }
        float scale = 1;
        int sub = W2(weap, ammosub, secondary), cooked = force;
        if(W2(weap, cooktime, secondary) || zooming)
        {
            float maxscale = 1;
            if(sub > 1 && d->weapammo[weap][W_A_CLIP] < sub) maxscale = d->weapammo[weap][W_A_CLIP]/float(sub);
            int len = max(int(W2(weap, cooktime, secondary)*maxscale), 1), type = zooming ? W_S_ZOOM : W_S_POWER;
            if(!cooked)
            {
                if(d->weapstate[weap] != type)
                {
                    int curammo = d->weapammo[weap][W_A_CLIP]-offset;
                    if(pressed && curammo > 0)
                    {
                        if(offset > 0)
                        {
                            d->weapammo[weap][W_A_CLIP] = max(curammo, 0);
                            if(W(weap, ammostore)) d->weapammo[weap][W_A_STORE] = clamp(d->weapammo[weap][W_A_STORE]+offset, 0, W(weap, ammostore));
                            d->weapload[weap][W_A_CLIP] = -offset;
                        }
                        int offtime = hadcook && d->prevstate[weap] == type ? lastmillis-d->prevtime[weap] : 0;
                        client::addmsg(N_WEAPCOOK, "ri5", d->clientnum, lastmillis-game::maptime, weap, zooming ? 2 : (secondary ? 1 : 0), offtime);
                        d->setweapstate(weap, type, len, lastmillis, offtime);
                        d->lastcook = lastmillis;
                    }
                    else return false;
                }
                cooked = clamp(lastmillis-d->weaptime[weap], 1, len);
                if(zooming) { if(pressed && wassecond) return false; }
                else if(pressed && cooked < len) return false;
            }
            scale = cooked/float(W2(weap, cooktime, secondary));
            if(sub > 1 && scale < 1) sub = clamp(int(ceilf(sub*scale)), 1, W2(weap, ammosub, secondary));
        }
        else if(!pressed) return false;

        vec from, dest;
        vector<shotmsg> shots;
        #define addshot(p) \
        { \
            shotmsg &s = shots.add(); \
            s.id = d->getprojid(); \
            s.pos = ivec(int(p.x*DMF), int(p.y*DMF), int(p.z*DMF)); \
        }
        int rays = W2(weap, rays, secondary);
        if(rays > 1 && W2(weap, cooked, secondary)&W_C_RAYS && W2(weap, cooktime, secondary) && scale < 1)
            rays = max(1, int(ceilf(rays*scale)));
        if(weap == W_MELEE || WF(false, weap, collide, secondary)&COLLIDE_LENGTH)
        {
            if(weap == W_MELEE)
            {
                from = secondary ? d->foottag(0) : d->feetpos();
                dest = targ;
            }
            else
            {
                from = d->muzzletag(weap);
                dest = d->origintag(weap);
            }
            loopi(rays) addshot(dest);
        }
        else
        {
            from = d->muzzletag(weap);
            dest = targ;
            float m = accmodspread(d, weap, secondary, W2(weap, cooked, true)&W_C_ZOOM && secondary && scale >= 0.9f);
            float spread = WSP(weap, secondary, game::gamemode, game::mutators, m);
            loopi(rays)
            {
                vec r;
                if(spread > 0) offsetray(from, dest, spread, W2(weap, spreadz, secondary), r);
                else r = dest;
                if(W2(weap, znudge, secondary) != 0) r.z += from.dist(r)*W2(weap, znudge, secondary);
                addshot(r);
            }
        }
        if(W2(weap, jittertime, secondary))
        {
            int jittertime = W2(weap, jittertime, secondary);
            if(jittertime < 0)
            {
                int value = -jittertime;
                jittertime = W2(weap, delayattack, secondary)/value;
            }
            float jitteryawmin = W2(weap, jitteryawmin, secondary), jitteryawmax = W2(weap, jitteryawmax, secondary),
                  jitterpitchmin = W2(weap, jitterpitchmin, secondary), jitterpitchmax = W2(weap, jitterpitchmax, secondary);
            accmodjitter(d, weap, secondary, W2(weap, cooked, true)&W_C_ZOOM && secondary && scale >= 0.9f, jittertime, jitteryawmin, jitteryawmax, jitterpitchmin, jitterpitchmax);
            d->addjitter(weap, lastmillis, int(ceilf(jittertime*scale)), jitteryawmin*scale, jitteryawmax*scale, jitterpitchmin*scale, jitterpitchmax*scale, W2(weap, jitterpitchdir, secondary));
        }
        projs::shootv(weap, secondary ? HIT(ALT) : 0, sub, offset, scale, from, dest, shots, d, true, v);
        client::addmsg(N_SHOOT, "ri9i4v", d->clientnum, lastmillis-game::maptime, weap, secondary ? HIT(ALT) : 0, cooked, v ? v->clientnum : -1, int(from.x*DMF), int(from.y*DMF), int(from.z*DMF), int(dest.x*DMF), int(dest.y*DMF), int(dest.z*DMF), shots.length(), shots.length()*sizeof(shotmsg)/sizeof(int), shots.getbuf());

        return true;
    }

    void shoot(gameent *d, vec &targ, int force)
    {
        if(!game::allowmove(d)) return;
        bool secondary = physics::secondaryweap(d);
        if(doshot(d, targ, d->weapselect, d->action[secondary ? AC_SECONDARY : AC_PRIMARY], secondary, force))
            if(!W2(d->weapselect, fullauto, secondary)) d->action[secondary ? AC_SECONDARY : AC_PRIMARY] = false;
    }

    void preload()
    {
        loopi(W_ALL)
        {
            if(*weaptype[i].item) preloadmodel(weaptype[i].item);
            if(*weaptype[i].vwep) preloadmodel(weaptype[i].vwep);
            if(*weaptype[i].hwep) preloadmodel(weaptype[i].hwep);
        }
    }
}
