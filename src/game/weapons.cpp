#include "game.h"
namespace weapons
{
    VAR(IDF_PERSIST, autoreloading, 0, 2, 4); // 0 = never, 1 = when empty, 2 = weapons that don't add a full clip, 3 = always (+1 zooming weaps too)
    VAR(IDF_PERSIST, autodelayreload, 0, 0, VAR_MAX);

    VAR(IDF_PERSIST, skipspawnweapon, 0, 0, 6); // skip spawnweapon; 0 = never, 1 = if numweaps > 1 (+1), 3 = if carry > 0 (+2), 6 = always
    VAR(IDF_PERSIST, skipclaw, 0, 0, 10); // skip claw; 0 = never, 1 = if numweaps > 1 (+2), 4 = if carry > 0 (+2), 7 = if carry > 0 and is offset (+2), 10 = always
    VAR(IDF_PERSIST, skippistol, 0, 0, 10); // skip pistol; 0 = never, 1 = if numweaps > 1 (+2), 4 = if carry > 0 (+2), 7 = if carry > 0 and is offset (+2), 10 = always
    VAR(IDF_PERSIST, skipgrenade, 0, 0, 10); // skip grenade; 0 = never, 1 = if numweaps > 1 (+2), 4 = if carry > 0 (+2), 7 = if carry > 0 and is offset (+2), 10 = always
    VAR(IDF_PERSIST, skipmine, 0, 0, 10); // skip mine; 0 = never, 1 = if numweaps > 1 (+2), 4 = if carry > 0 (+2), 7 = if carry > 0 and is offset (+2), 10 = always
    VAR(IDF_PERSIST, skiprocket, 0, 0, 10); // skip mine; 0 = never, 1 = if numweaps > 1 (+2), 4 = if carry > 0 (+2), 7 = if carry > 0 and is offset (+2), 10 = always

    VAR(IDF_PERSIST, skippickup, 0, 0, 1);

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
        if(d && weapselectslot)
        {
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
        return n;
    }

    ICOMMAND(0, weapslot, "i", (int *o), intret(slot(game::player1, *o >= 0 ? *o : game::player1->weapselect))); // -1 = weapselect slot
    ICOMMAND(0, weapselect, "", (), intret(game::player1->weapselect));
    ICOMMAND(0, ammo, "i", (int *n), intret(isweap(*n) ? game::player1->ammo[*n] : -1));
    ICOMMAND(0, reloadweap, "i", (int *n), intret(isweap(*n) && w_reload(*n, m_weapon(game::player1->actortype, game::gamemode, game::mutators)) ? 1 : 0));
    ICOMMAND(0, hasweap, "ii", (int *n, int *o), intret(isweap(*n) && game::player1->hasweap(*n, *o) ? 1 : 0));
    ICOMMAND(0, getweap, "ii", (int *n, int *o), {
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
                else if(!isweap(oldweap) || d->weapload[oldweap] <= 0) return false;
                else newoff = true;
            }
        }
        if(d->weapswitch(weap, lastmillis, weaponswitchdelay))
        {
            if(local)
            {
                if(newoff)
                {
                    int offset = d->weapload[oldweap];
                    d->ammo[oldweap] = max(d->ammo[oldweap]-offset, 0);
                    d->weapload[oldweap] = -d->weapload[oldweap];
                }
                client::addmsg(N_WSELECT, "ri3", d->clientnum, lastmillis-game::maptime, weap);
            }
            playsound(WSND(weap, S_W_SWITCH), d->o, d, 0, -1, -1, -1, &d->wschan);
            return true;
        }
        return false;
    }

    bool weapreload(gameent *d, int weap, int load, int ammo, bool local)
    {
        if(!gs_playing(game::gamestate) || (!local && (d == game::player1 || d->ai))) return false; // this can't be fixed until 1.5
        if(local)
        {
            if(!d->canreload(weap, m_weapon(d->actortype, game::gamemode, game::mutators), false, lastmillis))
            {
                if(d->weapstate[weap] == W_S_POWER) d->setweapstate(weap, W_S_WAIT, 100, lastmillis);
                return false;
            }
            client::addmsg(N_RELOAD, "ri3", d->clientnum, lastmillis-game::maptime, weap);
            int oldammo = d->ammo[weap];
            ammo = min(max(d->ammo[weap], 0) + W(weap, ammoadd), W(weap, ammomax));
            load = ammo-oldammo;
        }
        d->weapload[weap] = load;
        d->ammo[weap] = min(ammo, W(weap, ammomax));
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
            int s = slot(d, d->weapselect);
            loopi(W_ALL) // only loop the amount of times we have weaps for
            {
                if(a >= 0) s = a;
                else s += b;
                while(s > W_ALL-1) s -= W_ALL;
                while(s < 0) s += W_ALL;

                int n = slot(d, s, true);
                if(a < 0)
                { // weapon skipping when scrolling
                    int p = m_weapon(d->actortype, game::gamemode, game::mutators);
                    #define skipweap(q,w) \
                    { \
                        if(q && n == w) switch(q) \
                        { \
                            case 10: continue; break; \
                            case 7: case 8: case 9: if(d->carry(p, 5, w) > (q-7)) continue; break; \
                            case 4: case 5: case 6: if(d->carry(p, 1, w) > (q-3)) continue; break; \
                            case 1: case 2: case 3: if(d->carry(p, 0, w) > q) continue; break; \
                            case 0: default: break; \
                        } \
                    }
                    skipweap(skipspawnweapon, p);
                    skipweap(skipclaw, W_CLAW);
                    skipweap(skippistol, W_PISTOL);
                    if(!m_kaboom(game::gamemode, game::mutators))
                    {
                        skipweap(skipgrenade, W_GRENADE);
                        skipweap(skipmine, W_MINE);
                        skipweap(skiprocket, W_ROCKET);
                    }
                }

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

    bool canuse(int weap)
    {
        if(!skippickup || m_kaboom(game::gamemode, game::mutators)) return true;
        #define canuseweap(q,w) if(q && weap == w) return false;
        canuseweap(skipgrenade, W_GRENADE);
        canuseweap(skipmine, W_MINE);
        canuseweap(skiprocket, W_ROCKET);
        return true;
    }

    void weapdrop(gameent *d, int w)
    {
        if(!gs_playing(game::gamestate)) return;
        int weap = isweap(w) ? w : d->weapselect, sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        d->action[AC_DROP] = false;
        if(!d->candrop(weap, sweap, lastmillis, m_loadout(game::gamemode, game::mutators), (1<<W_S_SWITCH)))
        {
            if(!d->candrop(weap, sweap, lastmillis, m_loadout(game::gamemode, game::mutators), (1<<W_S_SWITCH)|(1<<W_S_RELOAD)) || !isweap(d->weapselect) || d->weapload[d->weapselect] <= 0)
            {
                game::errorsnd(d);
                return;
            }
            else
            {
                int offset = d->weapload[d->weapselect];
                d->ammo[d->weapselect] = max(d->ammo[d->weapselect]-offset, 0);
                d->weapload[d->weapselect] = -d->weapload[d->weapselect];
            }
        }
        client::addmsg(N_DROP, "ri3", d->clientnum, lastmillis-game::maptime, weap);
        d->setweapstate(weap, W_S_WAIT, weaponswitchdelay, lastmillis);
    }

    bool autoreload(gameent *d, int flags = 0)
    {
        if(gs_playing(game::gamestate) && d == game::player1 && W2(d->weapselect, ammosub, WS(flags)) && d->canreload(d->weapselect, m_weapon(d->actortype, game::gamemode, game::mutators), false, lastmillis))
        {
            bool noammo = d->ammo[d->weapselect] < W2(d->weapselect, ammosub, WS(flags)),
                 noattack = !d->action[AC_PRIMARY] && !d->action[AC_SECONDARY];
            if((noammo || noattack) && !d->action[AC_USE] && d->weapstate[d->weapselect] == W_S_IDLE && (noammo || lastmillis-d->weaptime[d->weapselect] >= autodelayreload))
                return autoreloading >= (noammo ? 1 : (W(d->weapselect, ammoadd) < W(d->weapselect, ammomax) ? 2 : (W2(d->weapselect, cooked, true)&W_C_ZOOM ? 4 : 3)));
        }
        return false;
    }

    void checkweapons(gameent *d)
    {
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        if(!d->hasweap(d->weapselect, sweap)) weapselect(d, d->bestweap(sweap, true), 1<<W_S_RELOAD, true);
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
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, 0, RAY_CLIPMAT|RAY_ALPHAPOLY);
            return;
        }
    }

    float accmod(gameent *d, bool zooming)
    {
        float r = 0;
        bool running = d->running(moveslow), moving = d->move || d->strafe;
        if(running || moving) r += running ? spreadrunning : spreadmoving;
        else if(zooming) r += spreadzoom;
        else if(d->crouching()) r += spreadcrouch;
        else r += spreadstill;
        if(spreadinair > 0 && d->airmillis && !d->onladder) r += spreadinair;
        return r;
    }

    bool doshot(gameent *d, vec &targ, int weap, bool pressed, bool secondary, int force)
    {
        int offset = 0, sweap = m_weapon(d->actortype, game::gamemode, game::mutators);
        if(!d->canshoot(weap, secondary ? HIT_ALT : 0, sweap, lastmillis))
        {
            if(!d->canshoot(weap, secondary ? HIT_ALT : 0, sweap, lastmillis, (1<<W_S_RELOAD)))
            {
                // if the problem is not enough ammo, do the reload..
                if(autoreload(d, secondary ? HIT_ALT : 0)) weapreload(d, weap);
                return false;
            }
            else if(d->weapload[weap] <= 0 || weap != d->weapselect) return false;
            else offset = d->weapload[weap];
        }
        float scale = 1;
        bool hadcook = W2(weap, cooked, true)&W_C_KEEP && (d->prevstate[weap] == W_S_ZOOM || d->prevstate[weap] == W_S_POWER),
             zooming = W2(weap, cooked, true)&W_C_ZOOM && (d->weapstate[weap] == W_S_ZOOM || (pressed && secondary) || (hadcook && d->prevstate[weap] == W_S_ZOOM)), wassecond = secondary;
        if(hadcook || zooming)
        {
            if(!pressed)
            {
                client::addmsg(N_SPHY, "ri5", d->clientnum, SPHY_COOK, W_S_IDLE, 0, 0);
                d->setweapstate(weap, W_S_IDLE, 0, lastmillis, 0, true);
                return false;
            }
            else secondary = zooming;
        }
        int sub = W2(weap, ammosub, secondary), cooked = force;
        if(W2(weap, cooktime, secondary) || zooming)
        {
            float maxscale = 1;
            if(sub > 1 && d->ammo[weap] < sub) maxscale = d->ammo[weap]/float(sub);
            int len = int(W2(weap, cooktime, secondary)*maxscale), type = zooming ? W_S_ZOOM : W_S_POWER;
            if(!cooked)
            {
                if(d->weapstate[weap] != type)
                {
                    if(pressed)
                    {
                        if(offset > 0)
                        {
                            d->ammo[weap] = max(d->ammo[weap]-offset, 0);
                            d->weapload[weap] = -offset;
                        }
                        int offtime = hadcook && d->prevstate[weap] == type ? lastmillis-d->prevtime[weap] : 0;
                        client::addmsg(N_SPHY, "ri5", d->clientnum, SPHY_COOK, type, len, offtime);
                        d->setweapstate(weap, type, len, lastmillis, offtime);
                    }
                    else return false;
                }
                cooked = len ? clamp(lastmillis-d->weaptime[weap], 1, len) : 1;
                if(zooming)
                {
                    if(pressed && wassecond) return false;
                }
                else if(pressed && cooked < len) return false;
            }
            scale = len ? cooked/float(W2(weap, cooktime, secondary)) : 1;
            if(sub > 1 && scale < 1) sub = int(ceilf(sub*scale));
        }
        else if(!pressed) return false;

        vec to, from;
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
        if(weap == W_MELEE || WF(false, weap, collide, secondary)&COLLIDE_HITSCAN)
        {
            if(weap == W_MELEE)
            {
                from = d->center();
                to = vec(from).add(vec(d->yaw*RAD, d->pitch*RAD).mul(d->radius*2.f));
            }
            else
            {
                from = d->originpos(weap);
                to = d->muzzlepos(weap);
            }
            loopi(rays) addshot(to);
        }
        else
        {
            from = d->muzzlepos(weap);
            to = targ;
            float m = accmod(d, W2(d->weapselect, cooked, true)&W_C_ZOOM && secondary);
            float spread = WSP(weap, secondary, game::gamemode, game::mutators, m);
            loopi(rays)
            {
                vec dest;
                if(spread > 0) offsetray(from, to, spread, W2(weap, spreadz, secondary), dest);
                else dest = to;
                if(weaptype[weap].thrown[secondary ? 1 : 0] > 0)
                    dest.z += from.dist(dest)*weaptype[weap].thrown[secondary ? 1 : 0];
                addshot(dest);
            }
        }
        projs::shootv(weap, secondary ? HIT_ALT : 0, sub, offset, scale, from, shots, d, true);
        client::addmsg(N_SHOOT, "ri8iv", d->clientnum, lastmillis-game::maptime, weap, secondary ? HIT_ALT : 0, cooked, int(from.x*DMF), int(from.y*DMF), int(from.z*DMF), shots.length(), shots.length()*sizeof(shotmsg)/sizeof(int), shots.getbuf());

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
