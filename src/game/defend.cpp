#include "game.h"
namespace defend
{
    defendstate st;

    bool insideaffinity(const defendstate::flag &b, gameent *d)
    {
        return st.insideaffinity(b, d->feetpos());
    }

    void preload()
    {
        preloadmodel("props/point");
    }

    static vec skewcolour(int owner, int enemy, float occupy)
    {
        vec colour = vec::hexcolor(TEAM(owner, colour));
        if(enemy)
        {
            int team = owner && enemy && !m_dac_quick(game::gamemode, game::mutators) ? T_NEUTRAL : enemy;
            int timestep = totalmillis%1000;
            float amt = clamp((timestep <= 500 ? timestep/500.f : (1000-timestep)/500.f)*occupy, 0.f, 1.f);
            colour.lerp(vec::hexcolor(TEAM(team, colour)), amt);
        }
        return colour;
    }

    void checkcams(vector<cament *> &cameras)
    {
        loopv(st.flags) // flags/bases
        {
            defendstate::flag &f = st.flags[i];
            cament *c = cameras.add(new cament(cameras.length(), cament::AFFINITY, i));
            c->o = f.o;
            c->o.z += enttype[AFFINITY].radius*2/3;
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
                    defendstate::flag &f = st.flags[c->id];
                    c->o = f.o;
                    c->o.z += enttype[AFFINITY].radius*2/3;
                }
                break;
            }
        }
    }

    void render()
    {
        loopv(st.flags)
        {
            defendstate::flag &b = st.flags[i];
            float occupy = b.occupied(m_dac_quick(game::gamemode, game::mutators), defendcount);
            vec effect = skewcolour(b.owner, b.enemy, occupy);
            int colour = effect.tohexcolor();
            b.baselight.material[0] = bvec::fromcolor(effect);
            rendermodel(&b.baselight, "props/point", ANIM_MAPMODEL|ANIM_LOOP, b.render, b.yaw, 0, 0, MDL_DYNSHADOW|MDL_CULL_VFC|MDL_CULL_OCCLUDED, NULL, NULL, 0, 0, 1);
            if(b.enemy && b.owner)
            {
                defformatstring(bowner, "%s", game::colourteam(b.owner));
                formatstring(b.info, "%s - %s v %s", b.name, bowner, game::colourteam(b.enemy));
            }
            else
            {
                int defend = b.owner ? b.owner : b.enemy;
                formatstring(b.info, "%s - %s", b.name, game::colourteam(defend));
            }
            vec above = b.above;
            float blend = camera1->o.distrange(above, enttype[AFFINITY].radius, enttype[AFFINITY].radius/8);
            part_explosion(above, 3, PART_SHOCKBALL, 1, colour, 1, 0.5f*blend);
            above.z += 4;
            part_text(above, b.info, PART_TEXT, 1, 0xFFFFFF, 2, blend);
            above.z += 3;
            if(b.enemy)
            {
                part_icon(above, textureload(hud::progringtex, 3), 4, blend, 0, 0, 1, colour, (lastmillis%1000)/1000.f, 0.1f);
                part_icon(above, textureload(hud::progresstex, 3), 4, blend, 0, 0, 1, TEAM(b.enemy, colour), 0, occupy);
                part_icon(above, textureload(hud::progresstex, 3), 4, 0.25f*blend, 0, 0, 1, TEAM(b.owner, colour), occupy, 1-occupy);
            }
            else part_icon(above, textureload(hud::teamtexname(b.owner), 3), 3, blend, 0, 0, 1, TEAM(b.owner, colour));
        }
    }


    void adddynlights()
    {
        loopv(st.flags)
        {
            defendstate::flag &f = st.flags[i];
            float occupy = f.occupied(m_dac_quick(game::gamemode, game::mutators), defendcount);
            adddynlight(vec(f.o).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, skewcolour(f.owner, f.enemy, occupy), 0, 0, DL_KEEP);
        }
    }

    void drawblips(int w, int h, float blend)
    {
        loopv(st.flags)
        {
            defendstate::flag &f = st.flags[i];
            float occupy = f.occupied(m_dac_quick(game::gamemode, game::mutators), defendcount);
            vec colour = skewcolour(f.owner, f.enemy, occupy);
            bool attack = f.owner == game::focus->team && f.enemy;
            const char *tex = f.hasflag ? hud::arrowtex : (attack ? hud::attacktex : hud::pointtex);
            float size = hud::radaraffinitysize*(f.hasflag ? 1.5f : (attack ? 0.65f : 1.f));
            if(hud::radaraffinitynames >= (f.hasflag ? 1 : 2))
            {
                bool overthrow = f.owner && f.enemy == game::focus->team;
                if(occupy < 1.f) hud::drawblip(tex, f.hasflag ? 3 : 2, w, h, size, blend*hud::radaraffinityblend, f.hasflag ? 0 : -1, f.o, colour, "little", "\f[%d]%d%%", f.hasflag ? (overthrow ? 0xFF8800 : (occupy < 1.f ? 0xFFFF00 : 0x00FF00)) : TEAM(f.owner, colour), int(occupy*100.f));
                else hud::drawblip(tex, f.hasflag ? 3 : 2, w, h, size, blend*hud::radaraffinityblend, f.hasflag ? 0 : -1, f.o, colour, "little", "\f[%d]%s", f.hasflag ? (overthrow ? 0xFF8800 : (occupy < 1.f ? 0xFFFF00 : 0x00FF00)) : TEAM(f.owner, colour), TEAM(f.owner, name));
            }
            else hud::drawblip(tex, f.hasflag ? 3 : 2, w, h, size, blend*hud::radaraffinityblend, f.hasflag ? 0 : -1, f.o, colour);
        }
    }

    void drawnotices(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend)
    {
        if(game::focus->state == CS_ALIVE && hud::shownotices >= 3 && game::focus->lastbuff)
        {
            pushfont("reduced");
            if(m_regen(game::gamemode, game::mutators) && defendregenbuff && defendregenextra)
                ty += draw_textf("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield, +\fs\fy%d\fS regen", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, int(defendbuffdamage*100), int(defendbuffshield*100), defendregenextra);
            else ty += draw_textf("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, int(defendbuffdamage*100), int(defendbuffshield*100));
            popfont();
        }
    }

    void drawevents(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend)
    {
        if(game::focus->state == CS_ALIVE && hud::showevents >= 2)
        {
            pushfont("super");
            loopv(st.flags) if(insideaffinity(st.flags[i], game::focus) && (st.flags[i].owner == game::focus->team || st.flags[i].enemy == game::focus->team))
            {
                defendstate::flag &f = st.flags[i];
                float occupy = !f.owner || f.enemy ? clamp(f.converted/float(defendcount), 0.f, 1.f) : 1.f;
                bool overthrow = f.owner && f.enemy == game::focus->team;
                ty -= draw_textf("You are %s: %s \fs\f[%d]\f(%s)\f(%s)\fS \fs%s%d%%\fS", tx, ty, int(FONTW*hud::eventpadx), int(FONTH*hud::eventpady), tr, tg, tb, int(255*blend), TEXT_SKIN|TEXT_CENTERED, -1, -1, 1, overthrow ? "overthrowing" : "securing", f.name, TEAM(f.owner, colour), hud::teamtexname(f.owner), hud::pointtex, overthrow ? "\fy" : (occupy < 1.f ? "\fc" : "\fg"), int(occupy*100.f))+FONTH/4;
                break;
            }
            popfont();
        }
    }

    int drawinventory(int x, int y, int s, int m, float blend)
    {
        int sy = 0, numflags = st.flags.length(), estsize = ((numflags-1)*s*hud::inventoryskew)+s, fitsize = y-m, size = s;
        if(estsize > fitsize) size = int((fitsize/float(estsize))*s);
        loopv(st.flags)
        {
            if(y-sy-size < m) break;
            defendstate::flag &f = st.flags[i];
            bool hasflag = game::focus->state == CS_ALIVE && insideaffinity(f, game::focus);
            if(f.hasflag != hasflag) { f.hasflag = hasflag; f.lasthad = lastmillis-max(1000-(lastmillis-f.lasthad), 0); }
            int millis = lastmillis-f.lasthad;
            bool headsup = hud::chkcond(hud::inventorygame, game::player1->state == CS_SPECTATOR || f.owner == game::focus->team || st.flags.length() == 1);
            if(headsup || f.hasflag || millis <= 1000)
            {
                float skew = headsup ? hud::inventoryskew : 0.f,
                    occupy = f.enemy ? clamp(f.converted/float(defendcount), 0.f, 1.f) : (f.owner ? 1.f : 0.f);
                vec c = vec::hexcolor(TEAM(f.owner, colour)), c1 = c;
                if(f.enemy)
                {
                    float amt = float(lastmillis%1000)/500.f;
                    if(amt > 1.f) amt = 2.f-amt;
                    c.lerp(vec::hexcolor(TEAM(f.enemy, colour)), amt);
                }
                if(f.hasflag) skew += (millis <= 1000 ? clamp(float(millis)/1000.f, 0.f, 1.f)*(1.f-skew) : 1.f-skew);
                else if(millis <= 1000) skew += (1.f-skew)-(clamp(float(millis)/1000.f, 0.f, 1.f)*(1.f-skew));
                int oldy = y-sy;
                if(hasflag || f.enemy)
                    sy += hud::drawitem(hud::pointtex, x, oldy, size, 0, true, false, c.r, c.g, c.b, blend, skew, "super", "%s%d%%", hasflag ? (f.owner && f.enemy == game::focus->team ? "\fy" : (occupy < 1.f ? "\fc" : "\fg")) : "\fw", int(occupy*100.f));
                else sy += hud::drawitem(hud::pointtex, x, oldy, size, 0, true, false, c.r, c.g, c.b, blend, skew);
                if(f.enemy)
                {
                    vec c2 = vec::hexcolor(TEAM(f.enemy, colour));
                    hud::drawitem(hud::attacktex, x, oldy, size, 0.5f, true, false, c2.r, c2.g, c2.b, blend, skew);
                    hud::drawitembar(x, oldy, size, false, c.r, c.g, c.b, blend, skew, occupy);
                }
                else if(f.owner)
                    hud::drawitem(hud::teamtexname(f.owner), x, oldy, size, 0.5f, true, false, c1.r, c1.g, c1.b, blend, skew);
                hud::drawitemtext(x, oldy, size, false, skew, "default", blend, "%s", f.name);
            }
        }
        return sy;
    }

    void reset()
    {
        st.reset();
    }

    void setup()
    {
        int df = m_dac_king(game::gamemode, game::mutators) ? 0 : defendflags;
        loopv(entities::ents)
        {
            extentity *e = entities::ents[i];
            if(e->type != AFFINITY || !m_check(e->attrs[3], e->attrs[4], game::gamemode, game::mutators)) continue;
            int team = e->attrs[0];
            switch(df)
            {
                case 3:
                    if(team && !isteam(game::gamemode, game::mutators, team, T_NEUTRAL)) team = T_NEUTRAL;
                    break;
                case 2:
                    if(!isteam(game::gamemode, game::mutators, team, T_FIRST)) continue;
                    break;
                case 1:
                    if(team && !isteam(game::gamemode, game::mutators, team, T_NEUTRAL)) continue;
                    break;
                case 0: team = T_NEUTRAL; break;
            }
            defformatstring(alias, "point_%d", e->attrs[5]);
            const char *name = getalias(alias);
            if(!name || !*name)
            {
                formatstring(alias, "point #%d", st.flags.length()+1);
                name = alias;
            }
            st.addaffinity(e->o, team, e->attrs[1], e->attrs[2], name);
        }
        if(!st.flags.length()) return; // map doesn't seem to support this mode at all..
        bool hasteams = df != 0;
        if(hasteams)
        {
            int bases[T_ALL] = {0};
            loopv(st.flags) bases[st.flags[i].kinship]++;
            loopi(numteams(game::gamemode, game::mutators)-1) if(!bases[i+1] || (bases[i+1] != bases[i+2]))
            {
                loopvk(st.flags) st.flags[k].kinship = T_NEUTRAL;
                hasteams = false;
                break;
            }
        }
        if(m_dac_king(game::gamemode, game::mutators))
        {
            vec average(0, 0, 0);
            int count = 0;
            loopv(st.flags)
            {
                average.add(st.flags[i].o);
                count++;
            }
            int smallest = rnd(st.flags.length());
            if(count)
            {
                average.div(count);
                float dist = 1e16f, tdist = 1e16f;
                loopv(st.flags) if(!st.flags.inrange(smallest) || (tdist = st.flags[i].o.dist(average)) < dist)
                {
                    smallest = i;
                    dist = tdist;
                }
            }
            if(st.flags.inrange(smallest))
            {
                copystring(st.flags[smallest].name, "center");
                st.flags[smallest].kinship = T_NEUTRAL;
                loopi(smallest) st.flags.remove(0);
                while(st.flags.length() > 1) st.flags.remove(1);
            }
        }
    }

    void sendaffinity(packetbuf &p)
    {
        putint(p, N_SETUPAFFIN);
        putint(p, st.flags.length());
        loopv(st.flags)
        {
            defendstate::flag &b = st.flags[i];
            putint(p, b.kinship);
            putint(p, b.yaw);
            putint(p, b.pitch);
            loopj(3) putint(p, int(b.o[j]*DMF));
            sendstring(b.name, p);
        }
    }

    void parseaffinity(ucharbuf &p)
    {
        int numflags = getint(p);
        while(st.flags.length() > numflags) st.flags.pop();
        loopi(numflags)
        {
            int kin = getint(p), yaw = getint(p), pitch = getint(p), converted = getint(p), owner = getint(p), enemy = getint(p);
            vec o;
            loopj(3) o[j] = getint(p)/DMF;
            string name;
            getstring(name, p);
            if(p.overread()) break;
            if(i >= MAXPARAMS) continue;
            while(!st.flags.inrange(i)) st.flags.add();
            st.initaffinity(i, kin, yaw, pitch, o, owner, enemy, converted, name);
        }
    }

    void updateaffinity(int i, int owner, int enemy, int converted)
    {
        if(!st.flags.inrange(i)) return;
        defendstate::flag &b = st.flags[i];
        if(converted >= 0)
        {
            if(owner)
            {
                if(b.owner != owner)
                {
                    gameent *d = NULL, *e = NULL;
                    int numdyns = game::numdynents();
                    loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && e->actortype < A_ENEMY && insideaffinity(b, e))
                        if((d = e) == game::focus) break;
                    game::announcef(S_V_FLAGSECURED, CON_SELF, d, true, "\fateam %s secured \fw%s", game::colourteam(owner), b.name);
                    part_textcopy(vec(b.o).add(vec(0, 0, enttype[AFFINITY].radius)), "<super>\fzZeSECURED", PART_TEXT, game::eventiconfade, TEAM(owner, colour), 3, 1, -10);
                    if(game::dynlighteffects) adddynlight(b.o, enttype[AFFINITY].radius*2, vec::hexcolor(TEAM(owner, colour)).mul(2.f), 500, 250);
                }
            }
            else if(b.owner)
            {
                gameent *d = NULL, *e = NULL;
                int numdyns = game::numdynents();
                loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && e->actortype < A_ENEMY && insideaffinity(b, e))
                    if((d = e) == game::focus) break;
                game::announcef(S_V_FLAGOVERTHROWN, CON_SELF, d, true, "\fateam %s overthrew \fw%s", game::colourteam(enemy), b.name);
                part_textcopy(vec(b.o).add(vec(0, 0, enttype[AFFINITY].radius)), "<super>\fzZeOVERTHROWN", PART_TEXT, game::eventiconfade, TEAM(enemy, colour), 3, 1, -10);
                if(game::dynlighteffects) adddynlight(b.o, enttype[AFFINITY].radius*2, vec::hexcolor(TEAM(enemy, colour)).mul(2.f), 500, 250);
            }
            b.converted = converted;
        }
        b.owner = owner;
        b.enemy = enemy;
    }

    void setscore(int team, int total)
    {
        hud::teamscore(team).total = total;
    }

    bool aicheck(gameent *d, ai::aistate &b)
    {
        return false;
    }

    void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests)
    {
        if(d->actortype == A_BOT)
        {
            vec pos = d->feetpos();
            loopvj(st.flags)
            {
                defendstate::flag &f = st.flags[j];
                static vector<int> targets; // build a list of others who are interested in this
                targets.setsize(0);
                ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, j, true);
                gameent *e = NULL;
                bool regen = !m_regen(game::gamemode, game::mutators) || d->health >= m_health(game::gamemode, game::mutators, d->actortype);
                int numdyns = game::numdynents();
                loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
                {
                    vec ep = e->feetpos();
                    if(targets.find(e->clientnum) < 0 && ep.squaredist(f.o) <= (enttype[AFFINITY].radius*enttype[AFFINITY].radius))
                        targets.add(e->clientnum);
                }
                if((!regen && f.owner == d->team) || (targets.empty() && (f.owner != d->team || f.enemy)))
                {
                    ai::interest &n = interests.add();
                    n.state = ai::AI_S_DEFEND;
                    n.node = ai::closestwaypoint(f.o, ai::CLOSEDIST, false);
                    n.target = j;
                    n.targtype = ai::AI_T_AFFINITY;
                    n.score = pos.squaredist(f.o)/(!regen ? 100.f : 1.f);
                    n.tolerance = 0.25f;
                    n.team = true;
                    n.acttype = ai::AI_A_PROTECT;
                }
            }
        }
    }

    bool aidefense(gameent *d, ai::aistate &b)
    {
        if(st.flags.inrange(b.target))
        {
            defendstate::flag &f = st.flags[b.target];
            bool regen = d->actortype != A_BOT || !m_regen(game::gamemode, game::mutators) || d->health >= m_health(game::gamemode, game::mutators, d->actortype);
            int walk = regen && f.owner == d->team && !f.enemy ? 1 : 0;
            if(walk)
            {
                static vector<int> targets; // build a list of others who are interested in this
                targets.setsize(0);
                ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, b.target, true);
                if(d->actortype == A_BOT)
                {
                    gameent *e = NULL;
                    int numdyns = game::numdynents();
                    float mindist = enttype[AFFINITY].radius*4; mindist *= mindist;
                    loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
                    {
                        vec ep = e->feetpos();
                        if(targets.find(e->clientnum) < 0 && ep.squaredist(f.o) <= mindist)
                            targets.add(e->clientnum);
                    }
                }
                if(!targets.empty())
                {
                    if(lastmillis-b.millis >= (201-d->skill)*33)
                    {
                        d->ai->tryreset = true; // re-evaluate so as not to herd
                        return true;
                    }
                    else walk = 2;
                }
            }
            return ai::defense(d, b, f.o, enttype[AFFINITY].radius, enttype[AFFINITY].radius*(walk+1), m_dac_king(game::gamemode, game::mutators) ? 0 : walk);
        }
        return false;
    }

    bool aipursue(gameent *d, ai::aistate &b)
    {
        b.type = ai::AI_S_DEFEND;
        return aidefense(d, b);
    }

    void removeplayer(gameent *d)
    {
    }
}
