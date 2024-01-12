#include "game.h"
namespace defend
{
    defendstate st;

    VAR(IDF_PERSIST, defendhalos, 0, 1, 1);

    bool insideaffinity(defendstate::flag &b, gameent *d, bool lasthad = false)
    {
        bool hasflag = st.insideaffinity(b, d->feetpos());
        if(lasthad && b.hasflag != hasflag) { b.hasflag = hasflag; b.lasthad = lastmillis-max(1000-(lastmillis-b.lasthad), 0); }
        return hasflag;
    }

    ICOMMAND(0, getdefendnum, "", (), intret(st.flags.length()));
    ICOMMAND(0, getdefendowner, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].owner : -1));
    ICOMMAND(0, getdefendowners, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].owners : 0));
    ICOMMAND(0, getdefendenemy, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].enemy : -1));
    ICOMMAND(0, getdefendenemies, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].enemies : 0));
    ICOMMAND(0, getdefendconverted, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].converted : 0));
    ICOMMAND(0, getdefendpoints, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].points : 0));
    ICOMMAND(0, getdefendoccupied, "i", (int *n), floatret(st.flags.inrange(*n) ? (st.flags[*n].enemy ? clamp(st.flags[*n].converted/float(defendcount), 0.f, 1.f) : (st.flags[*n].owner ? 1.f : 0.f)) : 0));

    ICOMMAND(0, getdefendkinship, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].kinship : -1));
    ICOMMAND(0, getdefendname, "i", (int *n), result(st.flags.inrange(*n) ? st.flags[*n].name : ""));
    ICOMMAND(0, getdefendinfo, "i", (int *n), result(st.flags.inrange(*n) ? st.flags[*n].info : ""));
    ICOMMAND(0, getdefendinside, "isi", (int *n, const char *who, int *h), gameent *d = game::getclient(client::parseplayer(who)); intret(d && st.flags.inrange(*n) && insideaffinity(st.flags[*n], d, *h!=0) ? 1 : 0));

    bool radarallow(const vec &o, int id, int render, vec &dir, float &dist, bool justtest = false)
    {
        if(!st.flags.inrange(id) || (m_hard(game::gamemode, game::mutators) && !G(radarhardaffinity))) return false;
        if(justtest) return true;
        dir = vec(render > 0 ? st.flags[id].render : st.flags[id].o).sub(o);
        dist = dir.magnitude();
        if(hud::radarlimited(dist)) return false;
        return true;
    }

    bool haloallow(const vec &o, int id, int render, bool justtest, bool check)
    {
        if(check && drawtex != DRAWTEX_HALO) return true;
        if(!defendhalos) return false;
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(o, id, render, dir, dist, justtest)) return false;
        if(dist > halodist) return false;
        return true;
    }

    ICOMMAND(0, getdefendradarallow, "ibi", (int *n, int *v, int *q),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        intret(radarallow(camera1->o, *n, *v, dir, dist, *q != 0) ? 1 : 0);
    });
    ICOMMAND(0, getdefendradardist, "ib", (int *n, int *v),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(camera1->o, *n, *v, dir, dist)) return;
        floatret(dist);
    });
    ICOMMAND(0, getdefendradardir, "ib", (int *n, int *v),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(camera1->o, *n, *v, dir, dist)) return;
        dir.rotate_around_z(-camera1->yaw*RAD).normalize();
        floatret(-atan2(dir.x, dir.y)/RAD);
    });

    #define LOOPDEFEND(name,op) \
        ICOMMAND(0, loopdefend##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            if(!m_defend(game::gamemode) || st.flags.empty()) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            }); \
            loopend(id, stack); \
        });
    LOOPDEFEND(,loopcsv);
    LOOPDEFEND(rev,loopcsvrev);

    #define LOOPDEFENDIF(name,op) \
        ICOMMAND(0, loopdefend##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
        { \
            if(!m_defend(game::gamemode) || st.flags.empty()) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                if(executebool(cond)) execute(body); \
            }); \
            loopend(id, stack); \
        });
    LOOPDEFENDIF(,loopcsv);
    LOOPDEFENDIF(rev,loopcsvrev);

    void preload()
    {
        preloadmodel("props/point");
    }

    static vec skewcolour(int owner, int enemy, float occupy)
    {
        vec colour = vec::fromcolor(TEAM(owner, colour));
        if(enemy)
        {
            int team = owner && enemy && !m_dac_quick(game::gamemode, game::mutators) ? T_NEUTRAL : enemy;
            int timestep = totalmillis%1000;
            float amt = clamp((timestep <= 500 ? timestep/500.f : (1000-timestep)/500.f)*occupy, 0.f, 1.f);
            colour.lerp(vec::fromcolor(TEAM(team, colour)), amt);
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
                if(!st.flags.inrange(c->id)) break;
                defendstate::flag &f = st.flags[c->id];
                c->o = f.o;
                c->o.z += enttype[AFFINITY].radius*2/3;
                break;
            }
            default: break;
        }
    }

    void render()
    {
        loopv(st.flags) if(haloallow(camera1->o, i))
        {
            defendstate::flag &b = st.flags[i];
            modelstate mdl;
            float occupy = b.occupied(m_dac_quick(game::gamemode, game::mutators), defendcount);
            vec effect = skewcolour(b.owner, b.enemy, occupy);
            int colour = effect.tohexcolor();
            mdl.material[0] = bvec::fromcolor(effect);
            mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
            mdl.flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_HALO_TOP;
            mdl.yaw = b.yaw;
            mdl.o = b.render;

            if(drawtex == DRAWTEX_HALO)
            {
                mdl.material[0].mul(mdl.color.a);
                mdl.color.a = hud::radardepth(mdl.o, halodist, halotolerance, haloaddz);
            }
            rendermodel("props/point", mdl);
            if(drawtex != DRAWTEX_HALO)
            {
                if(b.enemy && b.owner)
                {
                    defformatstring(bowner, "%s", game::colourteam(b.owner, NULL));
                    formatstring(b.info, "%s v %s", bowner, game::colourteam(b.enemy, NULL));
                }
                else formatstring(b.info, "%s", TEAM(b.owner ? b.owner : b.enemy, name));
                float blend = camera1->o.distrange(b.o, game::affinityfadeat, game::affinityfadecut);
                part_explosion(b.o, 3, PART_GLIMMERY, 1, colour, 1, blend);
                part_create(PART_HINT_SOFT, 1, b.o, colour, 6, blend);
            }
        }
    }


    void adddynlights()
    {
        loopv(st.flags)
        {
            defendstate::flag &f = st.flags[i];
            float occupy = f.occupied(m_dac_quick(game::gamemode, game::mutators), defendcount);
            adddynlight(vec(f.o).addz(2), enttype[AFFINITY].radius, skewcolour(f.owner, f.enemy, occupy), 0, 0, L_NOSHADOW|L_NODYNSHADOW);
        }
    }

    void reset()
    {
        st.reset();
    }

    void setup()
    {
        int df = m_dac_king(game::gamemode, game::mutators) ? 0 : defendflags;
        loopv(entities::ents) if(entities::ents[i]->type == AFFINITY)
        {
            gameentity &e = *(gameentity *)entities::ents[i];
            if(!entities::isallowed(e)) continue;
            int team = e.attrs[0];
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
            defformatstring(alias, "point_%d", e.attrs[5]);
            const char *name = getalias(alias);
            if(!name || !*name)
            {
                formatstring(alias, "Point #%d", st.flags.length()+1);
                name = alias;
            }
            st.addaffinity(i, e.o, team, e.attrs[1], e.attrs[2], name);
        }
        if(!st.flags.length()) return; // map doesn't seem to support this mode at all..
        if(df != 0)
        {
            int bases[T_COUNT] = {0};
            loopv(st.flags) bases[st.flags[i].kinship]++;
            loopi(numteams(game::gamemode, game::mutators)-1) if(!bases[i+1] || (bases[i+1] != bases[i+2]))
            {
                loopvk(st.flags) st.flags[k].kinship = T_NEUTRAL;
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
                copystring(st.flags[smallest].name, "Top of the Hill");
                st.flags[smallest].kinship = T_NEUTRAL;
                loopi(smallest) st.flags.remove(0);
                while(st.flags.length() > 1) st.flags.remove(1);
            }
        }
        loopv(entities::ents) ((gameentity *)entities::ents[i])->affinity = -1;
        loopv(st.flags)
        {
            defendstate::flag &f = st.flags[i];
            if(!entities::ents.inrange(f.ent)) continue;
            ((gameentity *)entities::ents[f.ent])->affinity = i;
        }
    }

    void sendaffinity(packetbuf &p)
    {
        putint(p, N_SETUPAFFIN);
        putint(p, st.flags.length());
        loopv(st.flags)
        {
            defendstate::flag &b = st.flags[i];
            putint(p, b.ent);
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
        if(numflags < 0) return;
        while(st.flags.length() > numflags) st.flags.pop();
        loopi(numflags)
        {
            int ent = getint(p), kin = getint(p), yaw = getint(p), pitch = getint(p), converted = getint(p), owner = getint(p), enemy = getint(p);
            vec o;
            loopj(3) o[j] = getint(p)/DMF;
            string name;
            getstring(name, p);
            if(p.overread()) break;
            if(i >= MAXPARAMS) continue;
            while(!st.flags.inrange(i)) st.flags.add();
            st.initaffinity(i, ent, kin, yaw, pitch, o, owner, enemy, converted, name);
        }
    }

    void buildevent(defendstate::flag &b, int i, const char *action, int sndidx, int owner, int enemy, int converted)
    {
        gamelog *log = new gamelog(GAMELOG_EVENT);
        log->addlist("args", "type", "defend");
        log->addlist("args", "action", action);
        log->addlist("args", "sound", sndidx);
        log->addlist("args", "flags", GAMELOG_F_BROADCAST);
        log->addlist("args", "affinity", i);
        log->addlist("args", "oldowner", b.owner);
        log->addlist("args", "owner", owner);
        log->addlist("args", "oldenemy", b.enemy);
        log->addlist("args", "enemy", enemy);
        log->addlist("args", "oldconverted", b.converted);
        log->addlist("args", "converted", converted);
        log->addlist("args", "colour", colourgrey);
        log->addlistf("args", "console", "Team %s secured \fw\f($pointtex)%s", game::colourteam(owner), b.name);

        gameent *d = NULL;
        int numdyns = game::numdynents();
        loopi(numdyns) if((d = (gameent *)game::iterdynents(i)) && d->actortype < A_ENEMY && insideaffinity(b, d))
            log->addclient("client", d);

        log->push();
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
                    if(game::dynlighteffects) adddynlight(b.o, enttype[AFFINITY].radius*2, vec::fromcolor(TEAM(owner, colour)).mul(2.f), 500, 250, L_NOSHADOW|L_NODYNSHADOW);
                    buildevent(b, i, "secure", S_V_FLAGSECURED, owner, enemy, converted);
                }
            }
            else if(b.owner) buildevent(b, i, "overthrow", S_V_FLAGOVERTHROWN, owner, enemy, converted);
            b.converted = converted;
        }
        b.owner = owner;
        b.enemy = enemy;
    }

    void setscore(int team, int total)
    {
        hud::teamscore(team).total = total;
    }

    bool getpos(int idx, vec &o)
    {
        if(!st.flags.inrange(idx)) return false;
        o = st.flags[idx].o;
        return true;
    }

    bool aicheck(gameent *d, ai::aistate &b)
    {
        return false;
    }

    void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests)
    {
        if(d->actortype != A_BOT) return;
        vec pos = d->feetpos();
        loopvj(st.flags)
        {
            defendstate::flag &f = st.flags[j];
            static vector<int> targets; // build a list of others who are interested in this
            targets.setsize(0);
            ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, j, true);
            gameent *e = NULL;
            bool regen = !m_regen(game::gamemode, game::mutators) || d->health >= d->gethealth(game::gamemode, game::mutators);
            int numdyns = game::numdynents();
            loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
            {
                if(targets.find(e->clientnum) < 0 && e->feetpos().squaredist(f.o) <= (enttype[AFFINITY].radius*enttype[AFFINITY].radius))
                    targets.add(e->clientnum);
            }
            if((!regen && f.owner == d->team) || (targets.empty() && (f.owner != d->team || f.enemy)))
            {
                ai::interest &n = interests.add();
                n.state = ai::AI_S_DEFEND;
                n.node = ai::closestwaypoint(f.o, ai::CLOSEDIST, false);
                n.target = j;
                n.targtype = ai::AI_T_AFFINITY;
                n.score = pos.squaredist(f.o)/(!regen ? 100.f : 10.f);
                n.tolerance = 0.5f;
                n.team = true;
                n.acttype = ai::AI_A_PROTECT;
            }
        }
    }

    bool aidefense(gameent *d, ai::aistate &b)
    {
        if(!st.flags.inrange(b.target)) return false;
        defendstate::flag &f = st.flags[b.target];
        bool regen = d->actortype != A_BOT || !m_regen(game::gamemode, game::mutators) || d->health >= d->gethealth(game::gamemode, game::mutators);
        int walk = regen && f.owner == d->team && !f.enemy ? 1 : 0;
        if(walk)
        {
            int teammembers = 1;
            static vector<int> targets; // build a list of others who are interested in this
            targets.setsize(0);
            ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, b.target, true);
            if(d->actortype == A_BOT)
            {
                gameent *e = NULL;
                int numdyns = game::numdynents();
                float mindist = enttype[AFFINITY].radius*2; mindist *= mindist;
                loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && d->team == e->team)
                {
                    teammembers++;
                    if(e->state != CS_ALIVE || e->ai || targets.find(e->clientnum) < 0) continue;
                    if(e->feetpos().squaredist(f.o) <= mindist) targets.add(e->clientnum);
                }
            }
            if(targets.length() >= teammembers*0.5f)
            {
                if(lastmillis-b.millis >= (201-d->skill)*33)
                {
                    d->ai->tryreset = true; // re-evaluate so as not to herd
                    return true;
                }
                else walk = 2;
            }
        }
        return ai::defense(d, b, f.o, enttype[AFFINITY].radius, enttype[AFFINITY].radius*(walk+2), m_dac_king(game::gamemode, game::mutators) ? 0 : walk);
    }

    bool aicheckpos(gameent *d, ai::aistate &b)
    {
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
