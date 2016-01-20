#include "game.h"
namespace capture
{
    capturestate st;

    bool carryaffinity(gameent *d)
    {
        loopv(st.flags) if(st.flags[i].owner == d) return true;
        return false;
    }

    bool dropaffinity(gameent *d)
    {
        if(carryaffinity(d) && d->action[AC_AFFINITY])
        {
            vec o = d->feetpos(capturedropheight), inertia = vec(d->vel).add(d->falling);
            client::addmsg(N_DROPAFFIN, "ri8", d->clientnum, -1, int(o.x*DMF), int(o.y*DMF), int(o.z*DMF), int(inertia.x*DMF), int(inertia.y*DMF), int(inertia.z*DMF));
            d->action[AC_AFFINITY] = false;
            return true;
        }
        return false;
    }

    bool canpickup(gameent *d, int n, bool check = false)
    {
        if(!st.flags.inrange(n)) return false;
        capturestate::flag &f = st.flags[n];
        if(f.owner) return false;
        if(f.pickuptime && lastmillis-f.pickuptime <= 1000) return false;
        if(f.team == d->team)
        {
            if(m_gsp2(game::gamemode, game::mutators)) return false;
            if(!f.droptime)
            {
                if(m_gsp1(game::gamemode, game::mutators)) return false;
                if(!check && !d->action[AC_AFFINITY]) return false;
            }
        }
        if(f.lastowner == d && f.droptime && lastmillis-f.droptime <= capturepickupdelay)
            return false;
        if((f.pos()).dist(d->feetpos()) > enttype[AFFINITY].radius*2/3) return false;
        return true;
    }

    void preload()
    {
        preloadmodel("props/point");
        preloadmodel("props/flag");
    }

    void drawblips(int w, int h, float blend)
    {
        static vector<int> hasflags; hasflags.setsize(0);
        loopv(st.flags) if(st.flags[i].owner == game::focus) hasflags.add(i);
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            loopk(2)
            {
                vec pos, colour = vec::hexcolor(TEAM(f.team, colour));
                const char *tex = hud::flagtex;
                bool arrow = false;
                float fade = blend*hud::radaraffinityblend, size = hud::radaraffinitysize;
                int millis = lastmillis-f.displaytime;
                if(millis < 1000) size *= 1.f+(1-clamp(float(millis)/1000.f, 0.f, 1.f));
                if(f.owner) size *= 0.75f;
                if(k)
                {
                    if(f.owner == game::focus || (!f.owner && !f.droptime)) break;
                    pos = f.pos(true);
                    int interval = lastmillis%500;
                    if(interval >= 300 || interval <= 200)
                        fade *= clamp(interval >= 300 ? 1.f-((interval-300)/200.f) : interval/200.f, 0.f, 1.f);
                }
                else
                {
                    pos = f.spawnloc;
                    if(f.team == game::focus->team && !m_gsp3(game::gamemode, game::mutators) && !hasflags.empty())
                    {
                        size *= 1.25f;
                        tex = hud::arrowtex;
                        arrow = true;
                    }
                    else if(f.owner || f.droptime) tex = hud::alerttex;
                }
                if(hud::radaraffinitynames > (arrow ? 0 : 1)) hud::drawblip(tex, arrow ? 3 : 2, w, h, size, fade, arrow ? 0 : -1, pos, colour, "little", "\f[%d]%s", TEAM(f.team, colour), k ? "flag" : "base");
                else hud::drawblip(tex, arrow ? 3 : 2, w, h, hud::radaraffinitysize, fade, arrow ? 0 : -1, pos, colour);
            }
        }
    }

    char *buildflagstr(vector<int> &f, bool named = false)
    {
        static string s; s[0] = '\0';
        loopv(f)
        {
            defformatstring(d, "%s\f[%d]\f(%s)%s", i && named ? " " : "", TEAM(st.flags[f[i]].team, colour), hud::flagtex, named ? TEAM(st.flags[f[i]].team, name) : "");
            concatstring(s, d);
        }
        return s;
    }

    void drawnotices(int w, int h, int &tx, int &ty, float blend)
    {
        if(game::focus->state == CS_ALIVE && hud::shownotices >= 2)
        {
            if(game::focus->lastbuff && hud::shownotices >= 3)
            {
                pushfont("reduced");
                if(m_regen(game::gamemode, game::mutators) && captureregenbuff && captureregenextra)
                    ty += draw_textx("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield, +\fs\fy%d\fS regen", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, int(capturebuffdamage*100), int(capturebuffshield*100), captureregenextra)*hud::noticescale;
                else ty += draw_textx("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, int(capturebuffdamage*100), int(capturebuffshield*100))*hud::noticescale;
                popfont();
            }
            bool ownflag = false;
            static vector<int> pickup, hasflags, taken, droppedflags;
            pickup.setsize(0); hasflags.setsize(0); taken.setsize(0); droppedflags.setsize(0);
            loopv(st.flags)
            {
                capturestate::flag &f = st.flags[i];
                if(f.owner == game::focus)
                {
                    hasflags.add(i);
                    if(f.team == game::focus->team) ownflag = true;
                }
                if(canpickup(game::focus, i, true)) pickup.add(i);
                if(f.team == game::focus->team)
                {
                    if(f.owner && f.owner->team != game::focus->team) taken.add(i);
                    else if(f.droptime) droppedflags.add(i);
                }
            }
            if(!hasflags.empty())
            {
                if(capturebuffing&(ownflag ? 8 : 32))
                {
                    pushfont("reduced");
                    if(capturebuffarea > 0) ty += draw_textx("Buffing team-mates within \fs\fy%.2f\fom\fS", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, capturebuffarea/8.f)*hud::noticescale;
                    else ty += draw_textx("Buffing \fs\fyALL\fS team-mates", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1)*hud::noticescale;
                    popfont();
                }
            }
            if(!pickup.empty())
            {
                pushfont("emphasis");
                char *str = buildflagstr(pickup, pickup.length() <= 3);
                ty += draw_textx("Nearby: \fs%s\fS", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, str)*hud::noticescale;
                popfont();
            }
            if(game::focus == game::player1 && (!hasflags.empty() || !pickup.empty()))
            {
                SEARCHBINDCACHE(altkey)("affinity", 0, "\f{\fs\fzuy", "\fS}");
                pushfont("reduced");
                ty += draw_textx("Press %s to %s", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, altkey, !hasflags.empty() ? "drop flags" : "pick up flags")*hud::noticescale;
                popfont();
            }
            if(!taken.empty())
            {
                pushfont("default");
                char *str = buildflagstr(taken, taken.length() <= 3);
                ty += draw_textx("%s taken: \fs%s\fS", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, taken.length() == 1 ? "Flag" : "Flags", str)*hud::noticescale;
                popfont();
            }
            if(!droppedflags.empty())
            {
                pushfont("default");
                char *str = buildflagstr(droppedflags, droppedflags.length() <= 3);
                ty += draw_textx("%s dropped: \fs%s\fS", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, droppedflags.length() == 1 ? "Flag" : "Flags", str)*hud::noticescale;
                popfont();
            }
        }
    }

    void drawevents(int w, int h, int &tx, int &ty, float blend)
    {
        if(game::focus->state == CS_ALIVE && hud::showevents >= 2)
        {
            static vector<int> hasflags;
            hasflags.setsize(0);
            loopv(st.flags)
            {
                capturestate::flag &f = st.flags[i];
                if(f.owner == game::focus) hasflags.add(i);
            }
            if(!hasflags.empty())
            {
                pushfont("huge");
                char *str = buildflagstr(hasflags, hasflags.length() <= 3);
                ty += draw_textx("You are holding: \fs%s\fS", tx, ty, 255, 255, 255, int(255*blend), TEXT_CENTERED, -1, -1, str)*hud::eventscale;
                popfont();
            }
        }
    }

    int drawinventory(int x, int y, int s, int m, float blend)
    {
        int sy = 0, numflags = st.flags.length(), estsize = numflags*s, fitsize = y-m, size = s;
        if(estsize > fitsize) size = int((fitsize/float(estsize))*s);
        loopv(st.flags)
        {
            if(y-sy-size < m) break;
            capturestate::flag &f = st.flags[i];
            bool headsup = hud::chkcond(hud::inventorygame, game::player1->state == CS_SPECTATOR || f.team == T_NEUTRAL || f.team == game::focus->team);
            if(headsup || f.lastowner == game::focus)
            {
                int millis = lastmillis-f.displaytime, colour = TEAM(f.team, colour);
                float skew = headsup ? hud::inventoryskew : 0.f;
                vec c = vec::hexcolor(colour);
                if(f.owner || f.droptime)
                {
                    if(f.owner == game::focus)
                    {
                        if(millis <= 1000) skew += (1.f-skew)*clamp(float(millis)/1000.f, 0.f, 1.f);
                        else skew = 1; // override it
                    }
                    else if(millis <= 1000) skew += (1.f-skew)*clamp(float(millis)/1000.f, 0.f, 1.f);
                    else skew = 1;
                }
                else if(millis <= 1000) skew += (1.f-skew)-(clamp(float(millis)/1000.f, 0.f, 1.f)*(1.f-skew));
                int oldy = y-sy;
                sy += hud::drawitem(hud::flagtex, x, oldy, size, 0, true, false, c.r, c.g, c.b, blend, skew);
                if(f.owner)
                {
                    vec c2 = vec::hexcolor(TEAM(f.owner->team, colour));
                    hud::drawitem(hud::flagtakentex, x, oldy, size, 0.5f, true, false, c2.r, c2.g, c2.b, blend, skew);
                }
                else if(f.droptime) hud::drawitem(hud::flagdroptex, x, oldy, size, 0.5f, true, false, 0.25f, 1.f, 1.f, blend, skew);
                else hud::drawitem(hud::teamtexname(f.team), x, oldy, size, 0.5f, true, false, c.r, c.g, c.b, blend, skew);
                if(gs_playing(game::gamestate) && (f.droptime || (m_gsp3(game::gamemode, game::mutators) && f.taketime && f.owner && f.owner->team != f.team)))
                {
                    float wait = f.droptime ? clamp(f.dropleft(lastmillis, capturestore)/float(capturedelay), 0.f, 1.f) : clamp((lastmillis-f.taketime)/float(captureprotectdelay), 0.f, 1.f);
                    if(wait > 0.5f)
                    {
                        int delay = wait > 0.7f ? (wait > 0.85f ? 150 : 300) : 600, millis = lastmillis%(delay*2);
                        float amt = (millis <= delay ? millis/float(delay) : 1.f-((millis-delay)/float(delay)));
                        flashcolour(c.r, c.g, c.b, 0.65f, 0.65f, 0.65f, amt);
                    }
                    hud::drawitembar(x, oldy, size, false, c.r, c.g, c.b, blend, skew, wait);
                }
            }
        }
        return sy;
    }

    void checkcams(vector<cament *> &cameras)
    {
        loopv(st.flags) // flags/bases
        {
            capturestate::flag &f = st.flags[i];
            cament *c = cameras.add(new cament(cameras.length(), cament::AFFINITY, i));
            c->o = f.pos(true);
            c->o.z += enttype[AFFINITY].radius*2/3;
            c->player = f.owner;
        }
    }

    void updatecam(cament *c)
    {
        switch(c->type)
        {
            case cament::AFFINITY:
            {
                if(st.flags.inrange(c->id))
                {
                    capturestate::flag &f = st.flags[c->id];
                    c->o = f.pos(true);
                    c->o.z += enttype[AFFINITY].radius*2/3;
                    c->player = f.owner;
                }
                break;
            }
            default: break;
        }
    }

    FVAR(IDF_PERSIST, followflagblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, thirdflagblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, firstflagblend, 0, 1, 1);
    FVAR(IDF_PERSIST, freeflagblend, 0, 1, 1);

    void render()
    {
        static vector<int> numflags, iterflags; // dropped/owned
        loopv(numflags) numflags[i] = iterflags[i] = 0;
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            if(!f.owner) continue;
            while(numflags.length() <= f.owner->clientnum)
            {
                numflags.add(0);
                iterflags.add(0);
            }
            numflags[f.owner->clientnum]++;
        }
        loopv(st.flags) // flags/bases
        {
            capturestate::flag &f = st.flags[i];
            vec pos = f.pos(true);
            float wait = f.droptime ? clamp(f.dropleft(lastmillis, capturestore)/float(capturedelay), 0.f, 1.f) : ((m_gsp3(game::gamemode, game::mutators) && f.taketime && f.owner && f.owner->team != f.team) ? clamp((lastmillis-f.taketime)/float(captureprotectdelay), 0.f, 1.f) : 0.f),
                  blend = (!f.owner && (!f.droptime || m_gsp2(game::gamemode, game::mutators)) && f.team == game::focus->team ? camera1->o.distrange(pos, enttype[AFFINITY].radius, enttype[AFFINITY].radius/8) : 1.f)*(f.owner && f.owner == game::focus ? (game::thirdpersonview(true) ? (f.owner != game::player1 ? followflagblend : thirdflagblend) : firstflagblend) : freeflagblend);
            vec effect = vec::hexcolor(TEAM(f.team, colour));
            int colour = effect.tohexcolor();
            if(wait > 0.5f)
            {
                int delay = wait > 0.7f ? (wait > 0.85f ? 150 : 300) : 600, millis = lastmillis%(delay*2);
                float amt = (millis <= delay ? millis/float(delay) : 1.f-((millis-delay)/float(delay)));
                flashcolour(effect.r, effect.g, effect.b, 0.65f, 0.65f, 0.65f, amt);
            }
            f.baselight.material[0] = f.light.material[0] = bvec::fromcolor(effect);
            if(!f.owner && !f.droptime)
                rendermodel(&f.light, "props/flag", ANIM_MAPMODEL|ANIM_LOOP, pos, f.yaw, f.pitch, 0, MDL_DYNSHADOW|MDL_CULL_VFC|MDL_CULL_OCCLUDED, NULL, NULL, 0, 0, blend);
            else if(!f.owner || f.owner != game::focus || game::thirdpersonview(true) || !(rendernormally))
            {
                vec flagpos = pos;
                float yaw = 0, pitch = 0, roll = 0;
                if(f.owner) yaw = f.owner->yaw-45.f+(90/float(numflags[f.owner->clientnum]+1)*(iterflags[f.owner->clientnum]+1));
                else
                {
                    yaw = ((lastmillis/8)+(360/st.flags.length()*i))%360;
                    if(f.proj) flagpos.z -= f.proj->height;
                }
                while(yaw >= 360.f) yaw -= 360.f;
                rendermodel(&f.light, "props/flag", ANIM_MAPMODEL|ANIM_LOOP, flagpos, yaw, pitch, roll, MDL_DYNSHADOW|MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_LIGHTFX, NULL, NULL, 0, 0, blend);
                flagpos.z += enttype[AFFINITY].radius*2/3;
                if(f.owner)
                {
                    flagpos.z += iterflags[f.owner->clientnum]*2;
                    iterflags[f.owner->clientnum]++;
                }
                //defformatstring(info, "<super>%s flag", TEAM(f.team, name));
                //part_textcopy(flagpos, info, PART_TEXT, 1, TEAM(f.team, colour), 2, blend);
                //flagpos.z += 2.5f;
                if(gs_playing(game::gamestate) && (f.droptime || (m_gsp3(game::gamemode, game::mutators) && f.taketime && f.owner && f.owner->team != f.team)))
                {
                    float wait = f.droptime ? clamp(f.dropleft(lastmillis, capturestore)/float(capturedelay), 0.f, 1.f) : clamp((lastmillis-f.taketime)/float(captureprotectdelay), 0.f, 1.f);
                    part_icon(flagpos, textureload(hud::progringtex, 3), 4, blend, 0, 0, 1, colour, (lastmillis%1000)/1000.f, 0.1f);
                    part_icon(flagpos, textureload(hud::progresstex, 3), 4, 0.25f*blend, 0, 0, 1, colour);
                    part_icon(flagpos, textureload(hud::progresstex, 3), 4, blend, 0, 0, 1, colour, 0, wait);
                    //flagpos.z += 0.5f;
                    //defformatstring(str, "<huge>%d%%", int(wait*100.f)); part_textcopy(flagpos, str, PART_TEXT, 1, 0xFFFFFF, 2, 1);
                    //flagpos.z += 3;
                }
            }
            rendermodel(&f.baselight, "props/point", ANIM_MAPMODEL|ANIM_LOOP, f.render, f.yaw, 0, 0, MDL_DYNSHADOW|MDL_CULL_VFC|MDL_CULL_OCCLUDED, NULL, NULL, 0, 0, 1);
            vec above = f.above;
            above.z += !f.owner && !f.droptime ? enttype[AFFINITY].radius*2/3 : 3;
            blend = camera1->o.distrange(above, enttype[AFFINITY].radius, enttype[AFFINITY].radius/8);
            defformatstring(info, "<super>%s base", TEAM(f.team, name));
            part_textcopy(above, info, PART_TEXT, 1, TEAM(f.team, colour), 2, blend);
            above.z += 2.5f;
            if(gs_playing(game::gamestate) && (f.droptime || (m_gsp3(game::gamemode, game::mutators) && f.taketime && f.owner && f.owner->team != f.team)))
            {
                part_icon(above, textureload(hud::progringtex, 3), 4, blend, 0, 0, 1, colour, (lastmillis%1000)/1000.f, 0.1f);
                part_icon(above, textureload(hud::progresstex, 3), 4, 0.25f*blend, 0, 0, 1, colour);
                part_icon(above, textureload(hud::progresstex, 3), 4, blend, 0, 0, 1, colour, 0, wait);
                above.z += 3;
            }
            if(f.owner) part_icon(above, textureload(hud::flagtakentex, 3), 2, blend, 0, 0, 1, TEAM(f.owner->team, colour));
            else if(f.droptime) part_icon(above, textureload(hud::flagdroptex, 3), 2, blend, 0, 0, 1, 0x28FFFF);
            else part_icon(above, textureload(hud::teamtexname(f.team), 3), 2, blend, 0, 0, 1, TEAM(f.team, colour));
        }
    }

    void adddynlights()
    {
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            if(f.owner || f.droptime)
                adddynlight(vec(f.above).add(vec(0, 0, enttype[AFFINITY].radius/2)), enttype[AFFINITY].radius, vec::hexcolor(TEAM(f.team, colour)), 0, 0, DL_KEEP);
            adddynlight(vec(f.pos(true)).add(vec(0, 0, enttype[AFFINITY].radius/2)), enttype[AFFINITY].radius, vec::hexcolor(TEAM(f.team, colour)), 0, 0, DL_KEEP);
        }
    }

    void reset()
    {
        st.reset();
    }

    void setup()
    {
        loopv(entities::ents) if(entities::ents[i]->type == AFFINITY)
        {
            gameentity &e = *(gameentity *)entities::ents[i];
            if(!m_check(e.attrs[3], e.attrs[4], game::gamemode, game::mutators) || !isteam(game::gamemode, game::mutators, e.attrs[0], T_FIRST))
                continue;
            st.addaffinity(e.o, e.attrs[0], e.attrs[1], e.attrs[2]);
            if(st.flags.length() >= MAXPARAMS) break;
        }
    }

    void sendaffinity(packetbuf &p)
    {
        putint(p, N_INITAFFIN);
        putint(p, st.flags.length());
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            putint(p, f.team);
            putint(p, f.yaw);
            putint(p, f.pitch);
            loopj(3) putint(p, int(f.spawnloc[j]*DMF));
        }
    }

    void setscore(int team, int total)
    {
        hud::teamscore(team).total = total;
    }

    void parseaffinity(ucharbuf &p)
    {
        int numflags = getint(p);
        while(st.flags.length() > numflags) st.flags.pop();
        loopi(numflags)
        {
            int team = getint(p), yaw = getint(p), pitch = getint(p), owner = getint(p), dropped = 0, dropoffset = 0;
            vec spawnloc(0, 0, 0), droploc(0, 0, 0), inertia(0, 0, 0);
            loopj(3) spawnloc[j] = getint(p)/DMF;
            if(owner < 0)
            {
                dropped = getint(p);
                if(dropped)
                {
                    dropoffset = getint(p);
                    loopj(3) droploc[j] = getint(p)/DMF;
                    loopj(3) inertia[j] = getint(p)/DMF;
                }
            }
            if(p.overread()) break;
            if(i >= MAXPARAMS) continue;
            while(!st.flags.inrange(i)) st.flags.add();
            capturestate::flag &f = st.flags[i];
            f.reset();
            f.team = team;
            f.yaw = yaw;
            f.pitch = pitch;
            f.spawnloc = f.render = f.above = spawnloc;
            f.render.z += 2;
            physics::droptofloor(f.render);
            if(f.render.z >= f.above.z-1) f.above.z += f.render.z-(f.above.z-1);
            if(owner >= 0) st.takeaffinity(i, game::newclient(owner), lastmillis);
            else if(dropped) st.dropaffinity(i, droploc, inertia, lastmillis, dropoffset);
        }
    }

    void dropaffinity(gameent *d, int i, const vec &droploc, const vec &inertia, int target)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];
        game::announcef(S_V_FLAGDROP, CON_SELF, d, true, "\fa%s dropped the the %s flag", game::colourname(d), game::colourteam(f.team, "flagtex"));
        st.dropaffinity(i, droploc, inertia, lastmillis, target);
    }

    void removeplayer(gameent *d)
    {
        loopv(st.flags) if(st.flags[i].owner == d)
        {
            capturestate::flag &f = st.flags[i];
            st.dropaffinity(i, f.owner->feetpos(capturedropheight), f.owner->vel, lastmillis);
        }
    }

    void affinityeffect(int i, int team, const vec &from, const vec &to, int effect, const char *str)
    {
        if(from.x >= 0)
        {
            if(effect&1)
            {
                defformatstring(text, "<super>\fzZe%s", str);
                part_textcopy(vec(from).add(vec(0, 0, enttype[AFFINITY].radius)), text, PART_TEXT, game::eventiconfade, TEAM(team, colour), 3, 1, -10);
            }
            if(game::dynlighteffects) adddynlight(vec(from).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, vec::hexcolor(TEAM(team, colour)).mul(2.f), 500, 250);
        }
        if(to.x >= 0)
        {
            if(effect&2)
            {
                defformatstring(text, "<super>\fzZe%s",str);
                part_textcopy(vec(to).add(vec(0, 0, enttype[AFFINITY].radius)), text, PART_TEXT, game::eventiconfade, TEAM(team, colour), 3, 1, -10);
            }
            if(game::dynlighteffects) adddynlight(vec(to).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, vec::hexcolor(TEAM(team, colour)).mul(2.f), 500, 250);
        }
        if(from.x >= 0 && to.x >= 0) part_trail(PART_SPARK, 500, from, to, TEAM(team, colour), 1, 1, -10);
    }

    void returnaffinity(gameent *d, int i)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];
        affinityeffect(i, d->team, d->feetpos(), f.above, m_gsp(game::gamemode, game::mutators) ? 3 : 2, "RETURNED");
        game::announcef(S_V_FLAGRETURN, CON_SELF, d, true, "\fa%s returned the %s flag (time taken: \fs\fc%s\fS)", game::colourname(d), game::colourteam(f.team, "flagtex"), timestr(m_gsp1(game::gamemode, game::mutators) ? f.dropleft(lastmillis, capturestore) : lastmillis-f.taketime, 1));
        st.returnaffinity(i, lastmillis);
    }

    void resetaffinity(int i, int value)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];
        if(value > 0)
        {
            affinityeffect(i, T_NEUTRAL, f.droploc, f.above, 3, "RESET");
            game::announcef(S_V_FLAGRESET, CON_SELF, NULL, true, "\fathe %s flag has been reset", game::colourteam(f.team, "flagtex"));
        }
        st.returnaffinity(i, lastmillis);
    }

    void scoreaffinity(gameent *d, int relay, int goal, int score)
    {
        if(!st.flags.inrange(relay)) return;
        capturestate::flag &f = st.flags[relay];
        if(st.flags.inrange(goal))
        {
            capturestate::flag &g = st.flags[goal];
            affinityeffect(goal, d->team, g.above, f.above, 3, "CAPTURED");
        }
        else affinityeffect(goal, d->team, f.pos(true), f.above, 3, "CAPTURED");
        hud::teamscore(d->team).total = score;
        defformatstring(fteam, "%s", game::colourteam(f.team, "flagtex"));
        game::announcef(S_V_FLAGSCORE, CON_SELF, d, true, "\fa%s captured the %s flag for team %s (score: \fs\fc%d\fS, time taken: \fs\fc%s\fS)", game::colourname(d), fteam, game::colourteam(d->team), score, timestr(lastmillis-f.taketime, 1));
        st.returnaffinity(relay, lastmillis);
    }

    void takeaffinity(gameent *d, int i)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];
        playsound(S_CATCH, d->o, d);
        affinityeffect(i, d->team, d->feetpos(), f.pos(true), 1, f.team == d->team ? "SECURED" : "TAKEN");
        game::announcef(f.team == d->team ? S_V_FLAGSECURED : S_V_FLAGPICKUP, CON_SELF, d, true, "\fa%s %s the %s flag", game::colourname(d), f.team == d->team ? "secured" : (f.droptime ? "picked up" : "stole"), game::colourteam(f.team, "flagtex"));
        st.takeaffinity(i, d, lastmillis);
        if(d->ai) aihomerun(d, d->ai->state.last());
    }

    void checkaffinity(gameent *d, int i)
    {
        if(canpickup(d, i))
        {
            client::addmsg(N_TAKEAFFIN, "ri2", d->clientnum, i);
            st.flags[i].pickuptime = lastmillis;
            d->action[AC_AFFINITY] = false;
        }
    }

    void update()
    {
        gameent *d = NULL;
        int numdyn = game::numdynents();
        loopj(numdyn) if(((d = (gameent *)game::iterdynents(j))) && d->state == CS_ALIVE && (d == game::player1 || d->ai)) dropaffinity(d);
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            if(f.owner) continue;
            if(f.droptime)
            {
                f.droploc = f.pos();
                if(f.lastowner && (f.lastowner == game::player1 || f.lastowner->ai) && f.proj && (!f.movetime || totalmillis-f.movetime >= 40))
                {
                    f.inertia = f.proj->vel;
                    f.movetime = totalmillis-(totalmillis%40);
                    client::addmsg(N_MOVEAFFIN, "ri8", f.lastowner->clientnum, i, int(f.droploc.x*DMF), int(f.droploc.y*DMF), int(f.droploc.z*DMF), int(f.inertia.x*DMF), int(f.inertia.y*DMF), int(f.inertia.z*DMF));
                }
            }
            loopj(numdyn) if(((d = (gameent *)game::iterdynents(j))) && d->state == CS_ALIVE && (d == game::player1 || d->ai)) checkaffinity(d, i);
        }
    }

    vec &aiflagpos(gameent *d, capturestate::flag &f)
    {
        if(f.droptime || f.owner != d) return f.pos();
        return f.spawnloc;
    }

    bool aihomerun(gameent *d, ai::aistate &b)
    {
        if(!m_gsp3(game::gamemode, game::mutators))
        {
            vec pos = d->feetpos();
            loopk(2)
            {
                int closest = -1;
                float closedist = 1e16f;
                loopv(st.flags)
                {
                    capturestate::flag &f = st.flags[i];
                    if(f.team == d->team && (k || ((!f.owner || f.owner == d) && !f.droptime)))
                    {
                        float dist = aiflagpos(d, f).squaredist(pos);
                        if(closest < 0 || dist < closedist)
                        {
                            closest = i;
                            closedist = dist;
                        }
                    }
                }
                if(st.flags.inrange(closest) && ai::makeroute(d, b, aiflagpos(d, st.flags[closest])))
                {
                    d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_AFFINITY, closest, ai::AI_A_HASTE);
                    return true;
                }
            }
        }
        if(b.type == ai::AI_S_PURSUE && b.targtype == ai::AI_T_NODE) return true; // we already did this..
        if(ai::randomnode(d, b, ai::ALERTMIN, 1e16f))
        {
            d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_NODE, d->ai->route[0], ai::AI_A_HASTE);
            return true;
        }
        return false;
    }

    bool aicheck(gameent *d, ai::aistate &b)
    {
        if(d->actortype == A_BOT)
        {
            static vector<int> taken; taken.setsize(0);
            loopv(st.flags)
            {
                capturestate::flag &g = st.flags[i];
                if(g.owner == d)
                {
                    if(!m_gsp3(game::gamemode, game::mutators)) return aihomerun(d, b);
                }
                else if(g.team == d->team && (m_gsp3(game::gamemode, game::mutators) || (g.owner && g.owner->team != d->team) || g.droptime))
                    taken.add(i);
            }
            if(!ai::badhealth(d)) while(!taken.empty())
            {
                int flag = taken.length() > 2 ? rnd(taken.length()) : 0;
                if(ai::makeroute(d, b, aiflagpos(d, st.flags[taken[flag]])))
                {
                    d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_AFFINITY, taken[flag], ai::AI_A_HASTE);
                    return true;
                }
                else taken.remove(flag);
            }
        }
        return false;
    }

    void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests)
    {
        vec pos = d->feetpos();
        loopvj(st.flags)
        {
            capturestate::flag &f = st.flags[j];
            bool home = f.team == d->team;
            if(d->actortype == A_BOT && m_duke(game::gamemode, game::mutators) && home) continue;
            static vector<int> targets; // build a list of others who are interested in this
            targets.setsize(0);
            bool regen = d->actortype != A_BOT || f.team == T_NEUTRAL || m_gsp3(game::gamemode, game::mutators) || !m_regen(game::gamemode, game::mutators) || d->health >= m_health(game::gamemode, game::mutators, d->actortype);
            ai::checkothers(targets, d, home || d->actortype != A_BOT ? ai::AI_S_DEFEND : ai::AI_S_PURSUE, ai::AI_T_AFFINITY, j, true);
            if(d->actortype == A_BOT)
            {
                gameent *e = NULL;
                int numdyns = game::numdynents();
                float mindist = enttype[AFFINITY].radius*4; mindist *= mindist;
                loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
                {
                    if(targets.find(e->clientnum) < 0 && (f.owner == e || e->feetpos().squaredist(aiflagpos(d, f)) <= mindist))
                        targets.add(e->clientnum);
                }
            }
            if(home)
            {
                bool guard = false;
                if(f.owner || f.droptime || targets.empty()) guard = true;
                else if(d->hasweap(ai::weappref(d), m_weapon(d->actortype, game::gamemode, game::mutators)))
                { // see if we can relieve someone who only has a piece of crap
                    gameent *t;
                    loopvk(targets) if((t = game::getclient(targets[k])))
                    {
                        if((t->ai && !t->hasweap(ai::weappref(t), m_weapon(t->actortype, game::gamemode, game::mutators))) || (!t->ai && t->weapselect < W_OFFSET))
                        {
                            guard = true;
                            break;
                        }
                    }
                }
                if(guard)
                { // defend the flag
                    ai::interest &n = interests.add();
                    n.state = ai::AI_S_DEFEND;
                    n.node = ai::closestwaypoint(aiflagpos(d, f), ai::CLOSEDIST, true);
                    n.target = j;
                    n.targtype = ai::AI_T_AFFINITY;
                    n.score = pos.squaredist(aiflagpos(d, f))/(!regen ? 100.f : 1.f);
                    n.tolerance = 0.25f;
                    n.team = true;
                    n.acttype = ai::AI_A_PROTECT;
                }
            }
            else
            {
                if(targets.empty())
                { // attack the flag
                    ai::interest &n = interests.add();
                    n.state = d->actortype == A_BOT ? ai::AI_S_PURSUE : ai::AI_S_DEFEND;
                    n.node = ai::closestwaypoint(aiflagpos(d, f), ai::CLOSEDIST, true);
                    n.target = j;
                    n.targtype = ai::AI_T_AFFINITY;
                    n.score = pos.squaredist(aiflagpos(d, f));
                    n.tolerance = 0.25f;
                    n.team = true;
                }
                else
                { // help by defending the attacker
                    gameent *t;
                    loopvk(targets) if((t = game::getclient(targets[k])))
                    {
                        ai::interest &n = interests.add();
                        bool team = d->team == t->team;
                        n.state = team ? ai::AI_S_DEFEND : ai::AI_S_PURSUE;
                        n.node = t->lastnode;
                        n.target = t->clientnum;
                        n.targtype = ai::AI_T_ACTOR;
                        n.score = d->o.squaredist(t->o);
                        n.tolerance = 0.25f;
                        n.team = team;
                        if(team) n.acttype = ai::AI_A_PROTECT;
                    }
                }
            }
        }
    }

    bool aidefense(gameent *d, ai::aistate &b)
    {
        if(d->actortype == A_BOT)
        {
            if(!m_gsp3(game::gamemode, game::mutators)) loopv(st.flags) if(st.flags[i].owner == d) return aihomerun(d, b);
            if(d->actortype == A_BOT && m_duke(game::gamemode, game::mutators) && b.owner < 0) return false;
        }
        if(st.flags.inrange(b.target))
        {
            capturestate::flag &f = st.flags[b.target];
            if(f.team == d->team && f.owner && f.owner->team != d->team && ai::violence(d, b, f.owner, 4)) return true;
            int walk = f.owner && f.owner->team != d->team ? 1 : 0;
            if(d->actortype == A_BOT)
            {
                if((!m_regen(game::gamemode, game::mutators) || d->health >= m_health(game::gamemode, game::mutators, d->actortype)) && lastmillis-b.millis >= (201-d->skill)*33)
                {
                    if(b.owner < 0)
                    {
                        static vector<int> targets; // build a list of others who are interested in this
                        targets.setsize(0);
                        ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, b.target, true);
                        gameent *e = NULL;
                        int numdyns = game::numdynents();
                        float mindist = enttype[AFFINITY].radius*4; mindist *= mindist;
                        loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
                        {
                            if(targets.find(e->clientnum) < 0 && (f.owner == e || e->feetpos().squaredist(aiflagpos(d, f)) <= mindist))
                                targets.add(e->clientnum);
                        }
                        if(!targets.empty())
                        {
                            d->ai->tryreset = true; // re-evaluate so as not to herd
                            return true;
                        }
                        else
                        {
                            walk = 2;
                            b.millis = lastmillis;
                        }
                    }
                    else
                    {
                        walk = 2;
                        b.millis = lastmillis;
                    }
                }
                vec pos = d->feetpos();
                float mindist = enttype[AFFINITY].radius*8; mindist *= mindist;
                loopv(st.flags)
                { // get out of the way of the returnee!
                    capturestate::flag &g = st.flags[i];
                    if(pos.squaredist(aiflagpos(d, g)) <= mindist)
                    {
                        if(g.owner && g.owner->team == d->team && !walk) walk = 1;
                        if(g.droptime && ai::makeroute(d, b, aiflagpos(d, g))) return true;
                    }
                }
            }
            return ai::defense(d, b, aiflagpos(d, f), enttype[AFFINITY].radius, enttype[AFFINITY].radius*walk*8, walk);
        }
        return false;
    }

    bool aipursue(gameent *d, ai::aistate &b)
    {
        if(st.flags.inrange(b.target) && d->actortype == A_BOT)
        {
            capturestate::flag &f = st.flags[b.target];
            if(f.team != d->team)
            {
                if(f.owner)
                {
                    if(d == f.owner) return aihomerun(d, b);
                    else if(d->team != f.owner->team) return ai::violence(d, b, f.owner, 4);
                    else return ai::defense(d, b, aiflagpos(d, f));
                }
                return ai::makeroute(d, b, aiflagpos(d, f));
            }
            else loopv(st.flags) if(st.flags[i].owner == d && ai::makeroute(d, b, aiflagpos(d, f)))
            {
                b.acttype = ai::AI_A_HASTE;
                return true;
            }
            else if(b.owner >= 0) return ai::makeroute(d, b, aiflagpos(d, f));
        }
        return false;
    }
}
