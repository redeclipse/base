struct bomberservmode : bomberstate, servmode
{
    bool hasflaginfo, hasstarted;
    int bombertime, scoresec;

    bomberservmode() : hasflaginfo(false), hasstarted(false), bombertime(-1) {}

    bool spawnitems() const { return false; }

    void reset()
    {
        bomberstate::reset();
        hasflaginfo = hasstarted = false;
        bombertime = -1;
    }

    void dropaffinity(clientinfo *ci, const vec &o, const vec &inertia = vec(0, 0, 0), int target = -1)
    {
        if(!canplay() || !hasflaginfo || !(A(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;

        vec n = inertia.iszero() ? vec(0, 0, G(bomberspeed)/10.f) : inertia;
        loopv(flags) if(flags[i].owner == ci->clientnum)
        {
            ivec p(vec(o).mul(DMF)), q(vec(n).mul(DMF));
            sendf(-1, 1, "ri3i7", N_DROPAFFIN, ci->clientnum, target, i, p.x, p.y, p.z, q.x, q.y, q.z);
            bomberstate::dropaffinity(i, o, n, gamemillis, target);
        }
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(!canplay() || !hasflaginfo) return;

        dropaffinity(ci, ci->feetpos(G(bomberdropheight)), vec(ci->vel).add(ci->falling));
    }

    void spawned(clientinfo *ci)
    {
        if(bombertime >= 0) return;

        if(m_team(gamemode, mutators))
        {
            int alive[T_MAX] = {0}, numt = numteams(gamemode, mutators);
            loopv(clients) if(clients[i]->state == CS_ALIVE) alive[clients[i]->team]++;
            loopk(numt) if(!alive[k+1]) return;
        }
        else
        {
            int alive = 0;
            loopv(clients) if(clients[i]->state == CS_ALIVE) alive++;
            if(alive <= 1) return;
        }

        bombertime = gamemillis+(hasstarted ? G(bomberwait) : 0);

        loopvj(sents) if(enttype[sents[j].type].usetype == EU_ITEM) setspawn(j, hasitem(j), true, true);
    }

    void died(clientinfo *ci, clientinfo *v)
    {
        if(!canplay() || !hasflaginfo) return;

        dropaffinity(ci, ci->feetpos(G(bomberdropheight)), vec(ci->vel).add(ci->falling));

        if(v && m_bb_hold(gamemode, mutators) && (!m_team(gamemode, mutators) || ci->team != v->team))
        {
            loopv(flags) if(isbomberaffinity(flags[i]) && flags[i].owner == v->clientnum)
                flags[i].taketime = gamemillis;
        }
    }

    int addscore(int team, int points)
    {
        score &cs = teamscore(team);
        cs.total += points;
        return cs.total;
    }

    void scorebomb(clientinfo *ci, int relay, int goal)
    {
        if(!canplay() || !hasflaginfo) return;

        flag g = flags[goal];
        if(!g.enabled) return;

        int total = 0;
        if(g.team != ci->team)
        {
            givepoints(ci, G(bomberpoints), m_points(gamemode, mutators), false);
            int points = flags[relay].droptime ? G(bomberthrowinpoints) : G(bombertouchdownpoints);
            total = addscore(ci->team, points);
        }
        else
        {
            givepoints(ci, -G(bomberpenalty), m_points(gamemode, mutators), false);
            total = addscore(ci->team, -1);
        }

        bomberstate::returnaffinity(relay, gamemillis, false);

        sendf(-1, 1, "ri5", N_SCOREAFFIN, ci->clientnum, relay, goal, total);

        mutate(smuts, mut->scoreaffinity(ci, g.team != ci->team));
        bombertime = m_duke(gamemode, mutators) ? -1 : gamemillis+(hasstarted ? G(bomberwait) : 0);

        loopvj(flags) if(flags[j].enabled)
        {
            bomberstate::returnaffinity(j, gamemillis, false);
            sendf(-1, 1, "ri3", N_RESETAFFIN, j, 0);
        }

        if(m_bb_assault(gamemode, mutators) && G(bomberassaultwinner))
        {
            int numt = numteams(gamemode, mutators);
            if(curbalance == numt-1)
            {
                int teamid = -1, teamsc = 0;
                loopi(numt)
                {
                    int t = i+T_FIRST, s = teamscore(t).total;
                    if(t != ci->team && s >= total)
                    {
                        teamid = t;
                        teamsc = s;
                        break;
                    }
                }
                if(teamid >= 0)
                {
                    gamelog log(GAMELOG_EVENT);
                    log.addlist("args", "type", "match");
                    log.addlist("args", "action", "scorelimit");
                    log.addlist("args", "flags", GAMELOG_F_BROADCAST);
                    log.addlist("args", "team", teamid);
                    log.addlist("args", "score", teamsc);
                    log.addlist("args", "concol", colouryellow);
                    log.addlistf("args", "console", "Score limit has been reached");
                    log.addclient("client", ci);
                    log.push();
                    startintermission();
                }
            }
        }
    }

    void moved(clientinfo *ci, const vec &oldpos, const vec &newpos)
    {
        if(!canplay() || !hasflaginfo || !(A(ci->actortype, abilities)&(1<<A_A_AFFINITY)) || ci->state != CS_ALIVE) return;

        if(G(bomberthreshold) > 0 && oldpos.dist(newpos) >= G(bomberthreshold))
            dropaffinity(ci, oldpos, vec(ci->vel).add(ci->falling));

        if(m_bb_hold(gamemode, mutators) || (G(bomberbasketonly) && m_bb_basket(gamemode, mutators))) return;

        loopv(flags) if(isbomberaffinity(flags[i]) && flags[i].owner == ci->clientnum) loopvk(flags)
            if(isbombertarg(flags[k], ci->team) && newpos.dist(flags[k].spawnloc) <= enttype[AFFINITY].radius/2) scorebomb(ci, i, k);
    }

    void returnaffinity(int i, bool enabled)
    {
        flag &f = flags[i];
        bool wasenabled = isbomberaffinity(f) && f.enabled;
        bomberstate::returnaffinity(i, gamemillis, enabled);

        sendf(-1, 1, "ri3", N_RESETAFFIN, i, f.enabled ? 1 : 0);

        if(wasenabled && !f.enabled)
        {
            loopvj(flags) if(i != j && flags[j].enabled) returnaffinity(j, false);
            if(bombertime >= 0) bombertime = gamemillis+(hasstarted ? G(bomberwait) : 0);
        }
    }

    void takeaffinity(clientinfo *ci, int i)
    {
        if(!canplay() || !hasflaginfo || !flags.inrange(i) || ci->state != CS_ALIVE || !(A(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;

        flag &f = flags[i];
        if(!isbomberaffinity(f) || f.owner >= 0 || !f.enabled) return;
        if(f.lastowner == ci->clientnum && f.droptime && gamemillis-f.droptime <= G(bomberpickupdelay)) return;

        if(m_bb_assault(gamemode, mutators) && ci->team == T_ALPHA && G(bomberassaultreset))
        {
            if(!f.droptime) return;
            if(!f.droptime || f.lastowner != ci->clientnum) givepoints(ci, G(bomberpickuppoints), m_points(gamemode, mutators), false);
            returnaffinity(i, false);
        }
        else
        {
            bomberstate::takeaffinity(i, ci->clientnum, gamemillis);
            if(!f.droptime || f.lastowner != ci->clientnum) givepoints(ci, G(bomberpickuppoints), m_points(gamemode, mutators), false);
            sendf(-1, 1, "ri3", N_TAKEAFFIN, ci->clientnum, i);
        }
    }

    void resetaffinity(clientinfo *ci, int i)
    {
        if(!canplay() || !hasflaginfo || !flags.inrange(i) || ci->ownernum >= 0) return;
        flag &f = flags[i];
        if(!isbomberaffinity(f) || f.owner >= 0 || !f.droptime || f.votes.find(ci->clientnum) >= 0 || !f.enabled) return;
        f.votes.add(ci->clientnum);
        if(f.votes.length() >= int(floorf(numclients()*0.5f))) returnaffinity(i, false);
    }

    void layout()
    {
        if(!canplay() || !hasflaginfo) return;

        bombertime = -1;

        loopvj(flags) if(flags[j].enabled)
        {
            bomberstate::returnaffinity(j, gamemillis, false);
            sendf(-1, 1, "ri3", N_RESETAFFIN, j, 0);
        }

        bombertime = gamemillis+(hasstarted ? G(bomberwait) : 0);
    }

    void update()
    {
        if(!canplay() || !hasflaginfo || bombertime < 0) return;

        if(bombertime)
        {
            if(gamemillis < bombertime) return;

            int hasaffinity = 0, selected[T_MAX] = {-1};
            vector<int> candidates[T_MAX];
            loopv(flags)
            {
                if(!isteam(gamemode, mutators, flags[i].team, T_NEUTRAL)) continue;
                candidates[flags[i].team].add(i);
            }

            int wants = m_bb_hold(gamemode, mutators) ? 1 : (m_bb_assault(gamemode, mutators) ? 2 : teamcount(gamemode, mutators));
            loopi(wants) if(!candidates[i].empty())
            {
                int r = rnd(candidates[i].length());
                selected[i] = candidates[i].removeunordered(r);
                hasaffinity++;
            }

            if(hasaffinity < wants)
            {
                if(selected[T_NEUTRAL] >= 0 && !m_bb_hold(gamemode, mutators))
                {
                    int muts = mutators;
                    if(muts&(1<<G_M_GSP2)) muts &= ~(1<<G_M_GSP2);
                    if(muts&(1<<G_M_GSP3)) muts &= ~(1<<G_M_GSP3);
                    muts |= (1<<G_M_GSP1);

                    srvmsgf(-1, colourred, "This map does have enough goals, switching to hold mutator");
                    changemap(smapname, gamemode, muts);

                    return;
                }

                hasflaginfo = false;
                loopv(flags) sendf(-1, 1, "ri3", N_RESETAFFIN, i, 0);
                srvmsgf(-1, colourred, "This map is not playable in %s", gamename(gamemode, mutators));

                return;
            }

            loopi(wants)
            {
                bomberstate::returnaffinity(selected[i], gamemillis, true);
                sendf(-1, 1, "ri3", N_RESETAFFIN, selected[i], 1);
            }

            hasstarted = true;
            bombertime = 0;
        }

        int t = (gamemillis / G(bomberholdinterval)) - ((gamemillis - (curtime + scoresec)) / G(bomberholdinterval));
        if(t < 1) scoresec += curtime;
        else scoresec = 0;

        loopv(flags) if(isbomberaffinity(flags[i]))
        {
            flag &f = flags[i];
            if(f.owner >= 0)
            {
                clientinfo *ci = (clientinfo *)getinfo(f.owner);
                if(m_bb_hold(gamemode, mutators) && t > 0)
                {
                    int score = G(bomberholdpoints)*t;
                    if(score)
                    {
                        givepoints(ci, score, true, false);
                        if(m_team(gamemode, mutators))
                        {
                            int total = addscore(ci->team, score);
                            sendf(-1, 1, "ri3", N_SCORE, ci->team, total);
                        }
                    }
                }

                if(ci && carrytime && gamemillis-f.taketime >= carrytime)
                {
                    ci->weapshots[W_GRENADE][0].add(1);
                    sendf(-1, 1, "ri8", N_WEAPDROP, ci->clientnum, -1, 1, W_GRENADE, -1, -1, -1);
                    dropaffinity(ci, ci->feetpos(G(bomberdropheight)), vec(ci->vel).add(ci->falling));
                    if(m_bb_hold(gamemode, mutators) && G(bomberholdpenalty))
                    {
                        givepoints(ci, -G(bomberholdpenalty), true, false);
                        if(m_team(gamemode, mutators))
                        {
                            int total = addscore(ci->team, -G(bomberholdpenalty));
                            sendf(-1, 1, "ri3", N_SCORE, ci->team, total);
                        }
                    }
                }

                continue;
            }

            if(f.droptime && gamemillis - f.droptime >= G(bomberresetdelay)) returnaffinity(i, false);
        }
    }

    void sendaffinity()
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        initclient(NULL, p, false);
        sendpacket(-1, 1, p.finalize());
    }

    void initclient(clientinfo *ci, packetbuf &p, bool connecting)
    {
        if(!hasflaginfo) return;

        putint(p, N_INITAFFIN);
        putint(p, flags.length());

        loopv(flags)
        {
            flag &f = flags[i];
            putint(p, f.ent);
            putint(p, f.team);
            putint(p, f.yaw);
            putint(p, f.pitch);
            putint(p, f.enabled ? 1 : 0);
            putint(p, f.owner);

            loopj(3) putint(p, int(f.spawnloc[j]*DMF));

            if(f.owner < 0)
            {
                putint(p, f.droptime);
                if(f.droptime)
                {
                    putint(p, f.target);
                    loopj(3) putint(p, int(f.droploc[j]*DMF));
                    loopj(3) putint(p, int(f.inertia[j]*DMF));
                }
            }
        }

        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(!oi->connected || (ci && oi->clientnum == ci->clientnum) || !oi->lastbuff) continue;

            putint(p, N_SPHY);
            putint(p, oi->clientnum);
            putint(p, SPHY_BUFF);
            putint(p, 1);
        }
    }

    void regen(clientinfo *ci, int &total, int &amt, int &delay)
    {
        if(!canplay() || !hasflaginfo || !G(bomberregenbuff) || !ci->lastbuff) return;
        if(G(maxhealth)) total = max(ci->gethealth(gamemode, mutators, true), total);
        if(ci->lastregen && G(bomberregendelay)) delay = G(bomberregendelay);
        if(G(bomberregenextra)) amt += G(bomberregenextra);
    }

    void checkclient(clientinfo *ci)
    {
        if(!canplay() || !hasflaginfo || ci->state != CS_ALIVE || m_insta(gamemode, mutators)) return;

        #define bomberbuff1 (G(bomberbuffing)&1 && isbomberhome(f, ci->team) && (G(bomberbuffarea) > 0 ? ci->o.dist(f.spawnloc) <= G(bomberbuffarea) : true))
        #define bomberbuff2 ((G(bomberbuffing)&2 || (G(bomberbuffing)&4 && m_bb_assault(gamemode, mutators) && ci->team == T_ALPHA)) && isbomberaffinity(f) && f.owner == ci->clientnum)

        if(G(bomberbuffing)) loopv(flags)
        {
            flag &f = flags[i];
            if(f.enabled && (bomberbuff1 || bomberbuff2))
            {
                if(!ci->lastbuff) sendf(-1, 1, "ri4", N_SPHY, ci->clientnum, SPHY_BUFF, 1);
                ci->lastbuff = gamemillis;
                return;
            }
        }

        if(ci->lastbuff && (!G(bomberbuffing) || gamemillis-ci->lastbuff >= G(bomberbuffdelay)))
        {
            ci->lastbuff = 0;
            sendf(-1, 1, "ri4", N_SPHY, ci->clientnum, SPHY_BUFF, 0);
        }
    }

    void moveaffinity(clientinfo *ci, int cn, int id, const vec &o, const vec &inertia = vec(0, 0, 0))
    {
        if(!canplay() || !hasflaginfo || !flags.inrange(id)) return;

        flag &f = flags[id];
        if(!f.droptime || f.owner >= 0 || !isbomberaffinity(f) || f.lastowner != ci->clientnum) return;

        f.distance += f.droploc.dist(o);
        f.droploc = o;
        f.inertia = inertia;

        if(!m_bb_hold(gamemode, mutators) && m_bb_basket(gamemode, mutators)) loopv(flags)
        {
            if(isbomberaffinity(flags[i]) || f.droploc.dist(flags[i].spawnloc) > enttype[AFFINITY].radius/2) continue;
            if(G(bomberbasketmindist) > 0 && !flags[id].travel(flags[i].spawnloc, G(bomberbasketmindist))) continue;
            scorebomb(ci, id, i);
            break;
        }
        //sendf(-1, 1, "ri9", N_MOVEAFFIN, ci->clientnum, id, int(f.droploc.x*DMF), int(f.droploc.y*DMF), int(f.droploc.z*DMF), int(f.inertia.x*DMF), int(f.inertia.y*DMF), int(f.inertia.z*DMF));
    }

    void parseaffinity(ucharbuf &p)
    {
        int numflags = getint(p);
        if(numflags <= 0) return;

        loopi(numflags)
        {
            int ent = getint(p), team = getint(p), yaw = getint(p), pitch = getint(p);
            vec o;
            loopj(3) o[j] = getint(p)/DMF;
            if(p.overread()) break;
            if(!hasflaginfo && i < MAXPARAMS) addaffinity(ent, o, team, yaw, pitch);
        }

        if(!hasflaginfo)
        {
            hasflaginfo = true;
            sendaffinity();
            loopv(clients) if(clients[i]->state == CS_ALIVE) entergame(clients[i]);
        }
    }

    int points(clientinfo *m, clientinfo *v)
    {
        bool isteam = m == v || (m_team(gamemode, mutators) && m->team == v->team);
        int p = isteam ? -1 : (m_team(gamemode, mutators) ? 1 : 0), q = p;
        if(p) { loopv(flags) if(flags[i].owner == m->clientnum) p += q; }
        return p;
    }
} bombermode;
