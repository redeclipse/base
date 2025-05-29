#include "game.h"
namespace capture
{
    capturestate st;

    VAR(IDF_PERSIST, capturehalos, 0, 1, 1);

    ICOMMAND(0, getcapturedelay, "i", (int *n), intret(capturedelay));
    ICOMMAND(0, getcapturestore, "i", (int *n), intret(capturestore));

    ICOMMAND(0, getcapturenum, "b", (int *n), intret(*n >= 0 ? (st.flags.inrange(*n) ? 1 : 0) : st.flags.length()));
    ICOMMAND(0, getcaptureteam, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].team : -1));
    ICOMMAND(0, getcapturedroptime, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].droptime : -1));
    ICOMMAND(0, getcapturetaketime, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].taketime : -1));
    ICOMMAND(0, getcapturedisptime, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].displaytime : -1));
    ICOMMAND(0, getcaptureowner, "i", (int *n), intret(st.flags.inrange(*n) && st.flags[*n].owner ? st.flags[*n].owner->clientnum : -1));
    ICOMMAND(0, getcapturelastowner, "i", (int *n), intret(st.flags.inrange(*n) && st.flags[*n].lastowner ? st.flags[*n].lastowner->clientnum : -1));

    bool radarallow(const vec &o, int id, int render, vec &dir, float &dist, bool justtest = false)
    {
        if(!st.flags.inrange(id) || (m_hard(game::gamemode, game::mutators) && !G(radarhardaffinity))) return false;
        if(justtest) return true;
        dir = vec(render > 0 ? st.flags[id].spawnloc : st.flags[id].pos(render < 0)).sub(o);
        dist = dir.magnitude();
        if(st.flags[id].owner != game::focus && hud::radarlimited(dist)) return false;
        return true;
    }

    bool haloallow(const vec &o, int id, int render, bool justtest)
    {
        if(drawtex != DRAWTEX_HALO) return true;
        if(!capturehalos || !halosurf.check()) return false;

        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(o, id, render, dir, dist, justtest)) return false;
        if(dist > halodist) return false;

        return true;
    }

    ICOMMAND(0, getcaptureradarallow, "ibi", (int *n, int *v, int *q),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        intret(radarallow(camera1->o, *n, *v, dir, dist, *q != 0) ? 1 : 0);
    });
    ICOMMAND(0, getcaptureradardist, "ib", (int *n, int *v),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(camera1->o, *n, *v, dir, dist)) return;
        floatret(dist);
    });
    ICOMMAND(0, getcaptureradardir, "ib", (int *n, int *v),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(camera1->o, *n, *v, dir, dist)) return;
        dir.rotate_around_z(-camera1->yaw*RAD).normalize();
        floatret(-atan2(dir.x, dir.y)/RAD);
    });

    #define LOOPCAPTURE(name,op) \
        ICOMMAND(0, loopcapture##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            if(!m_capture(game::gamemode) || st.flags.empty()) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            }); \
            loopend(id, stack); \
        }); \
        ICOMMAND(0, loopcapture##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
        { \
            if(!m_capture(game::gamemode) || st.flags.empty()) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                if(executebool(cond)) execute(body); \
            }); \
            loopend(id, stack); \
        }); \
        ICOMMAND(0, loopcapture##name##while, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
        { \
            if(!m_capture(game::gamemode) || st.flags.empty()) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                if(!executebool(cond)) break; \
                execute(body); \
            }); \
            loopend(id, stack); \
        });
    LOOPCAPTURE(,loopcsv);
    LOOPCAPTURE(rev,loopcsvrev);

    int hasaffinity(gameent *d)
    {
        int n = 0;
        loopv(st.flags) if(st.flags[i].owner == d) n++;
        return n;
    }

    bool dropaffinity(gameent *d)
    {
        if(!hasaffinity(d) || !d->action[AC_AFFINITY]) return false;
        vec o = d->feetpos(capturedropheight), inertia = vec(d->vel).add(d->falling);
        client::addmsg(N_DROPAFFIN, "ri8", d->clientnum, -1, int(o.x*DMF), int(o.y*DMF), int(o.z*DMF), int(inertia.x*DMF), int(inertia.y*DMF), int(inertia.z*DMF));
        d->action[AC_AFFINITY] = false;
        return true;
    }

    bool canpickup(gameent *d, int n, bool check = false)
    {
        if(!st.flags.inrange(n) || !(A(d->actortype, abilities)&(1<<A_A_AFFINITY))) return false;
        capturestate::flag &f = st.flags[n];
        if(f.owner) return false;
        if(f.team == d->team)
        {
            if(m_ctf_defend(game::gamemode, game::mutators)) return false;
            if(!f.droptime)
            {
                if(m_ctf_quick(game::gamemode, game::mutators)) return false;
                if(!check && !d->action[AC_AFFINITY]) return false;
            }
        }
        if(f.lastowner == d && f.droptime && lastmillis-f.droptime <= capturepickupdelay) return false;
        if((f.pos()).dist(d->feetpos()) > enttype[AFFINITY].radius*3/4) return false;
        return true;
    }

    void preload()
    {
        preloadmodel("props/point");
        preloadmodel("props/flag");
    }

    void checkcams(vector<cament *> &cameras)
    {
        loopv(st.flags) // flags/bases
        {
            capturestate::flag &f = st.flags[i];
            cament *c = cameras.add(new cament(cameras.length(), cament::AFFINITY, i));
            c->o = f.pos(true);
            c->o.z += enttype[AFFINITY].radius / 2;
            c->player = f.owner;
        }
    }

    void updatecam(cament *c)
    {
        switch(c->type)
        {
            case cament::AFFINITY:
            {
                if(!st.flags.inrange(c->id)) break;
                capturestate::flag &f = st.flags[c->id];
                c->o = f.pos(true);
                c->o.z += enttype[AFFINITY].radius / 2;
                c->player = f.owner;
                break;
            }
            default: break;
        }
    }

    DEFUIVARS(capture, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 512.f, 0.f, 0.f);
    DEFUIVARS(captureflag, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 512.f, 0.f, 0.f);

    void checkui()
    {
        if(captureui < 0 && captureflagui < 0) return;

        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            if(captureui >= 0)
            {
                MAKEUI(capture, i, true, vec(f.render).addz(enttype[AFFINITY].radius * (f.owner || f.droptime ? 0.25f : 0.875f)));
            }

            if(captureflagui >= 0 && (f.owner != game::focus && (f.owner || f.droptime)))
            {
                MAKEUI(captureflag, i, true, vec(f.pos(true)).addz(enttype[AFFINITY].radius * (f.droptime ? 0.5f : 0.875f)));
            }
        }
    }

    void render()
    {
        static vector<int> numflags, iterflags; // dropped/owned
        loopv(numflags) numflags[i] = iterflags[i] = 0;
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            if(!f.owner) continue;
            while(f.owner->clientnum >= numflags.length())
            {
                numflags.add(0);
                iterflags.add(0);
            }
            numflags[f.owner->clientnum]++;
        }

        loopv(st.flags) if(haloallow(camera1->o, i)) // flags/bases
        {
            capturestate::flag &f = st.flags[i];
            modelstate mdl, basemdl;
            vec pos = f.pos(true);
            float blend = 1.f, wait = f.droptime ? clamp(f.dropleft(lastmillis, capturestore)/float(capturedelay), 0.f, 1.f) :
                  ((m_ctf_protect(game::gamemode, game::mutators) && f.taketime && f.owner && f.owner->team != f.team) ? clamp((lastmillis-f.taketime)/float(captureprotectdelay), 0.f, 1.f) : 0.f);
            vec effect = vec::fromcolor(TEAM(f.team, colour));

            if(drawtex != DRAWTEX_HALO && !f.owner && (!f.droptime || m_ctf_defend(game::gamemode, game::mutators)) && f.team == game::focus->team)
                blend *= camera1->o.distrange(pos, enttype[AFFINITY].radius, enttype[AFFINITY].radius/8);

            if(wait > 0.5f)
            {
                int delay = wait > 0.7f ? (wait > 0.85f ? 150 : 300) : 600, millis = lastmillis%(delay*2);
                float amt = (millis <= delay ? millis/float(delay) : 1.f-((millis-delay)/float(delay)));
                flashcolour(effect.r, effect.g, effect.b, 0.65f, 0.65f, 0.65f, amt);
            }

            loopk(MAXMDLMATERIALS) basemdl.material[k] = mdl.material[k] = bvec::fromcolor(effect);
            mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
            mdl.flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_HALO_TOP;

            if(!f.owner && !f.droptime)
            {
                vec flagpos = pos;
                mdl.o = flagpos;
                mdl.color = vec4(1, 1, 1, blend);

                game::haloadjust(mdl.o, mdl);
                rendermodel("props/flag", mdl);
            }
            else if(!f.owner || f.owner != game::focus || game::thirdpersonview(true))
            {
                vec flagpos = pos;
                if(f.owner)
                {
                    if(f.owner == game::focus && game::thirdpersonview(true))
                        blend *= f.owner != game::player1 ? game::affinityfollowblend : game::affinitythirdblend;
                    mdl.yaw = f.owner->yaw-45.f+(90/float(numflags[f.owner->clientnum]+1)*(iterflags[f.owner->clientnum]+1));
                }
                else
                {
                    mdl.yaw = ((lastmillis/8)+(360/st.flags.length()*i))%360;
                    if(f.proj) flagpos.z -= f.proj->height;
                }
                while(mdl.yaw >= 360.f) mdl.yaw -= 360.f;

                mdl.o = flagpos;
                mdl.color = vec4(1, 1, 1, blend);

                game::haloadjust(mdl.o, mdl);
                rendermodel("props/flag", mdl);

                if(f.owner) iterflags[f.owner->clientnum]++;
            }

            basemdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
            basemdl.flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_HALO_TOP;
            basemdl.o = f.render;

            game::haloadjust(basemdl.o, basemdl);
            rendermodel("props/point", basemdl);
        }
    }

    void adddynlights()
    {
        loopv(st.flags)
            adddynlight(st.flags[i].pos(true), enttype[AFFINITY].radius, vec::fromcolor(TEAM(st.flags[i].team, colour)), 0, 0, L_NOSHADOW|L_NODYNSHADOW);
    }

    void reset()
    {
        st.reset();
    }

    void setaffinity()
    {
        loopv(entities::ents) ((gameentity *)entities::ents[i])->affinity = -1;
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            if(!entities::ents.inrange(f.ent)) continue;
            ((gameentity *)entities::ents[f.ent])->affinity = i;
        }
    }

    void setup()
    {
        loopv(entities::ents) if(entities::ents[i]->type == AFFINITY && entities::isallowed(i))
        {
            gameentity &e = *(gameentity *)entities::ents[i];
            if(!isteam(game::gamemode, game::mutators, e.attrs[0], T_FIRST)) continue;
            st.addaffinity(i, e.o, e.attrs[0], e.attrs[1], e.attrs[2]);
            if(st.flags.length() >= MAXPARAMS) break;
        }
        setaffinity();
    }

    void sendaffinity(packetbuf &p)
    {
        putint(p, N_INITAFFIN);
        putint(p, st.flags.length());
        loopv(st.flags)
        {
            capturestate::flag &f = st.flags[i];
            putint(p, f.ent);
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
        if(numflags < 0) return;
        while(st.flags.length() > numflags) st.flags.pop();
        loopi(numflags)
        {
            int ent = getint(p), team = getint(p), yaw = getint(p), pitch = getint(p), owner = getint(p), dropped = 0, dropoffset = -1;
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
            f.ent = ent;
            f.team = team;
            f.yaw = yaw;
            f.pitch = pitch;
            f.setposition(spawnloc);
            if(owner >= 0) st.takeaffinity(i, game::newclient(owner), lastmillis);
            else if(dropped) st.dropaffinity(i, droploc, inertia, lastmillis, dropoffset);
        }
        setaffinity();
    }

    void dropaffinity(gameent *d, int i, const vec &droploc, const vec &inertia, int offset)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];

        gamelog *log = new gamelog(GAMELOG_EVENT);
        log->addlist("args", "type", "capture");
        log->addlist("args", "action", "drop");
        log->addlist("args", "flags", GAMELOG_F_BROADCAST);
        log->addlist("args", "affinity", i);
        log->addlist("args", "colour", colourgrey);
        log->addlistf("args", "console", "%s dropped the the %s flag", game::colourname(d), game::colourteam(f.team, "flagtex"));
        log->addclient("client", d);
        if(!log->push()) DELETEP(log);

        st.dropaffinity(i, droploc, inertia, lastmillis, offset);
    }

    void removeplayer(gameent *d)
    {
        loopv(st.flags) if(st.flags[i].owner == d)
        {
            capturestate::flag &f = st.flags[i];
            st.dropaffinity(i, f.owner->feetpos(capturedropheight), f.owner->vel, lastmillis);
        }
    }

    void affinityeffect(int i, int team, const vec &from, const vec &to)
    {
        if(game::dynlighteffects)
        {
            if(from.x >= 0) adddynlight(vec(from).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, vec::fromcolor(TEAM(team, colour)).mul(2.f), 500, 250, L_NOSHADOW|L_NODYNSHADOW);
            if(to.x >= 0) adddynlight(vec(to).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, vec::fromcolor(TEAM(team, colour)).mul(2.f), 500, 250, L_NOSHADOW|L_NODYNSHADOW);
        }
        if(from.x >= 0 && to.x >= 0 && from != to) part_trail(PART_SPARK, 500, from, to, TEAM(team, colour), 1, 1, colourwhite, 0.25f, -10);
    }

    void returnaffinity(gameent *d, int i)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];
        affinityeffect(i, d->team, d->feetpos(), f.spawnloc);
        game::spawneffect(PART_SPARK, vec(f.spawnloc).add(vec(0, 0, enttype[AFFINITY].radius*0.45f)), enttype[AFFINITY].radius*0.25f, TEAM(f.team, colour), 1);
        game::spawneffect(PART_SPARK, vec(f.spawnloc).add(vec(0, 0, enttype[AFFINITY].radius*0.45f)), enttype[AFFINITY].radius*0.25f, colourwhite, 1);
        int millis = m_ctf_quick(game::gamemode, game::mutators) ? f.dropleft(lastmillis, capturestore) : lastmillis-f.taketime;

        gamelog *log = new gamelog(GAMELOG_EVENT);
        log->addlist("args", "type", "capture");
        log->addlist("args", "action", "return");
        log->addlist("args", "flags", GAMELOG_F_BROADCAST);
        log->addlist("args", "affinity", i);
        log->addlist("args", "millis", millis);
        log->addlist("args", "taketime", f.taketime);
        log->addlist("args", "droptime", f.droptime);
        log->addlist("args", "dropoffset", f.dropoffset);
        log->addlist("args", "colour", colourgrey);
        log->addlistf("args", "console", "%s returned the %s flag (time taken: \fs\fc%s\fS)", game::colourname(d), game::colourteam(f.team, "flagtex"), timestr(millis, 1));
        log->addclient("client", d);
        if(!log->push()) DELETEP(log);

        st.returnaffinity(i, lastmillis);
    }

    void resetaffinity(int i, int value, const vec &pos)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];
        if(value > 0)
        {
            affinityeffect(i, T_NEUTRAL, f.droploc, value == 2 ? pos : f.spawnloc);
            game::spawneffect(PART_SPARK, vec(f.pos()).add(vec(0, 0, enttype[AFFINITY].radius*0.45f)), enttype[AFFINITY].radius*0.25f, TEAM(f.team, colour), 1);
            game::spawneffect(PART_SPARK, vec(f.pos()).add(vec(0, 0, enttype[AFFINITY].radius*0.45f)), enttype[AFFINITY].radius*0.25f, colourwhite, 1);
            game::spawneffect(PART_SPARK, value == 2 ? pos : vec(f.spawnloc).add(vec(0, 0, enttype[AFFINITY].radius*0.45f)), enttype[AFFINITY].radius*0.25f, TEAM(f.team, colour), 1);
            game::spawneffect(PART_SPARK, value == 2 ? pos : vec(f.spawnloc).add(vec(0, 0, enttype[AFFINITY].radius*0.45f)), enttype[AFFINITY].radius*0.25f, colourwhite, 1);

            gamelog *log = new gamelog(GAMELOG_EVENT);
            log->addlist("args", "type", "capture");
            log->addlist("args", "action", "reset");
            log->addlist("args", "flags", GAMELOG_F_BROADCAST);
            log->addlist("args", "affinity", i);
            log->addlist("args", "value", value);
            log->addlist("args", "value", value);
            log->addlist("args", "taketime", f.taketime);
            log->addlist("args", "droptime", f.droptime);
            log->addlist("args", "dropoffset", f.dropoffset);
            log->addlist("args", "colour", colourgrey);
            log->addlistf("args", "console", "The %s flag has been reset", game::colourteam(f.team, "flagtex"));
            if(!log->push()) DELETEP(log);
        }

        if(value == 2)
        {
            st.dropaffinity(i, pos, vec(0, 0, 1), lastmillis);
            f.proj->stuck = 1;
            f.proj->stick = NULL;
        }
        else st.returnaffinity(i, lastmillis);
    }

    void scoreaffinity(gameent *d, int relay, int goal, int score)
    {
        if(!st.flags.inrange(relay)) return;
        float radius = enttype[AFFINITY].radius;
        vec abovegoal, capturepos, returnpos;
        capturestate::flag &f = st.flags[relay];

        if(st.flags.inrange(goal))
        {
            capturestate::flag &g = st.flags[goal];
            abovegoal = capturepos = g.spawnloc;
        }
        else abovegoal = capturepos = f.pos(true); // m_ctf_protect

        returnpos = vec(f.spawnloc);
        returnpos.add(vec(0, 0, radius*0.45f));
        capturepos.add(vec(0, 0, radius*0.45f));
        affinityeffect(goal, d->team, abovegoal, f.spawnloc);
        game::spawneffect(PART_SPARK, capturepos, radius*0.25f, TEAM(d->team, colour), 1);
        game::spawneffect(PART_SPARK, capturepos, radius*0.25f, colourwhite, 1);
        game::spawneffect(PART_SPARK, returnpos, radius*0.25f, TEAM(f.team, colour), 1);
        game::spawneffect(PART_SPARK, returnpos, radius*0.25f, colourwhite, 1);
        hud::teamscore(d->team).total = score;
        int millis = lastmillis-f.taketime;

        gamelog *log = new gamelog(GAMELOG_EVENT);
        log->addlist("args", "type", "capture");
        log->addlist("args", "action", "score");
        log->addlist("args", "flags", GAMELOG_F_BROADCAST);
        log->addlist("args", "affinity", relay);
        log->addlist("args", "goal", goal);
        log->addlist("args", "score", score);
        log->addlist("args", "millis", millis);
        log->addlist("args", "taketime", f.taketime);
        log->addlist("args", "colour", colourgrey);
        log->addlistf("args", "console", "%s captured the flag for team %s (score: \fs\fc%d\fS, time taken: \fs\fc%s\fS)", game::colourname(d), game::colourteam(d->team), score, timestr(millis, 1));
        log->addclient("client", d);
        if(!log->push()) DELETEP(log);

        st.returnaffinity(relay, lastmillis);
    }

    void takeaffinity(gameent *d, int i)
    {
        if(!st.flags.inrange(i)) return;
        capturestate::flag &f = st.flags[i];

        emitsound(S_CATCH, game::getplayersoundpos(d), d);
        affinityeffect(i, d->team, d->feetpos(), f.pos(true));

        gamelog *log = new gamelog(GAMELOG_EVENT);
        log->addlist("args", "type", "capture");
        log->addlist("args", "action", "secure");
        log->addlist("args", "flags", GAMELOG_F_BROADCAST);
        log->addlist("args", "affinity", i);
        log->addlist("args", "colour", colourgrey);
        log->addlistf("args", "console", "%s %s the %s flag", game::colourname(d), f.team == d->team ? "secured" : (f.droptime ? "picked up" : "stole"), game::colourteam(f.team, "flagtex"));
        log->addclient("client", d);
        if(!log->push()) DELETEP(log);

        st.takeaffinity(i, d, lastmillis);
        if(d->ai) aihomerun(d, d->ai->state.last());
    }

    void checkaffinity(gameent *d, int i)
    {
        if(!canpickup(d, i)) return;
        client::addmsg(N_TAKEAFFIN, "ri2", d->clientnum, i);
        d->action[AC_AFFINITY] = false;
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

    bool getpos(int idx, vec &o)
    {
        if(!st.flags.inrange(idx)) return false;
        o = st.flags[idx].pos(true);
        return true;
    }

    vec &aiflagpos(gameent *d, capturestate::flag &f)
    {
        if(f.droptime || f.owner != d) return f.pos();
        return f.spawnloc;
    }

    bool aihomerun(gameent *d, ai::aistate &b)
    {
        if(d->actortype < A_ENEMY && !m_ctf_protect(game::gamemode, game::mutators))
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
        if(d->actortype != A_BOT) return false;
        static vector<int> taken; taken.setsize(0);
        loopv(st.flags)
        {
            capturestate::flag &g = st.flags[i];
            if(g.owner == d)
            {
                if(!m_ctf_protect(game::gamemode, game::mutators)) return aihomerun(d, b);
            }
            else if(g.team == d->team && (m_ctf_protect(game::gamemode, game::mutators) || (g.owner && g.owner->team != d->team) || g.droptime))
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
            bool regen = d->actortype != A_BOT || f.team == T_NEUTRAL || m_ctf_protect(game::gamemode, game::mutators) || !m_regen(game::gamemode, game::mutators) || d->health >= d->gethealth(game::gamemode, game::mutators);
            ai::checkothers(targets, d, home || d->actortype != A_BOT ? ai::AI_S_DEFEND : ai::AI_S_PURSUE, ai::AI_T_AFFINITY, j, d->actortype < A_ENEMY);
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
            if(!m_ctf_protect(game::gamemode, game::mutators)) loopv(st.flags) if(st.flags[i].owner == d) return aihomerun(d, b);
            if(d->actortype == A_BOT && m_duke(game::gamemode, game::mutators) && b.owner < 0) return false;
        }
        if(st.flags.inrange(b.target))
        {
            capturestate::flag &f = st.flags[b.target];
            if(f.team == d->team && f.owner && f.owner->team != d->team && ai::violence(d, b, f.owner, 4)) return true;
            int walk = f.owner && f.owner->team != d->team ? 1 : 0;
            if(d->actortype == A_BOT)
            {
                if((!m_regen(game::gamemode, game::mutators) || d->health >= d->gethealth(game::gamemode, game::mutators)) && lastmillis-b.millis >= (201-d->skill)*33)
                {
                    if(b.owner < 0)
                    {
                        static vector<int> targets; // build a list of others who are interested in this
                        targets.setsize(0);
                        ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, b.target, d->actortype < A_ENEMY);
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

    bool aicheckpos(gameent *d, ai::aistate &b)
    {
        return false;
    }

    bool aipursue(gameent *d, ai::aistate &b)
    {
        if(!st.flags.inrange(b.target) || d->actortype != A_BOT) return false;
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
        else
        {
            if(f.owner && d->team != f.owner->team) return ai::violence(d, b, f.owner, 4);
            loopv(st.flags) if(st.flags[i].owner == d && ai::makeroute(d, b, aiflagpos(d, f)))
            {
                b.acttype = ai::AI_A_HASTE;
                return true;
            }
        }
        if(b.owner >= 0) return ai::makeroute(d, b, aiflagpos(d, f));
        return false;
    }
}
