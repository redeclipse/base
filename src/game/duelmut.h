#ifdef CPP_GAME_SERVER
struct duelservmode : servmode
{
    int duelround, dueltime, duelcheck, dueldeath, duelwinner, duelwins, duelaffin;
    bool waitforhumans;
    vector<clientinfo *> duelqueue, allowed, playing, restricted;

    duelservmode() {}

    #define DSGS(x) DSG(gamemode, mutators, x)

    bool spawnitems() const { return false; }

    void shrink()
    {
        allowed.shrink(0);
        playing.shrink(0);
    }

    void doreset(bool start)
    {
        dueltime = start ? 1 : DSGS(cooloff);
        duelcheck = dueldeath = -1;
        duelaffin = 0;
    }

    void reset()
    {
        waitforhumans = true;
        shrink();
        duelqueue.shrink(0);
        restricted.shrink(0);
        doreset(true);
        duelround = duelwins = 0;
        duelwinner = -1;
    }

    void position()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(m_survivor(gamemode, mutators)) ci->queuepos = -1;
            else
            {
                int n = duelqueue.find(ci);
                ci->queuepos = n > 0 && dueltime >= 0 ? n-1 : n;
            }
            sendf(-1, 1, "ri3", N_QUEUEPOS, ci->clientnum, ci->queuepos);
        }
    }

    bool remqueue(clientinfo *ci, bool pos = true)
    {
        int n = duelqueue.find(ci);
        if(n >= 0)
        {
            duelqueue.remove(n);
            if(pos) position();
            return true;
        }
        return false;
    }

    bool queue(clientinfo *ci, bool pos = true, bool top = false, bool wait = true)
    {
        if(ci->actortype >= A_ENEMY || ci->state == CS_SPECTATOR) return remqueue(ci, pos);
        if(gamestate == G_S_OVERTIME && !restricted.empty() && restricted.find(ci) < 0) return remqueue(ci, pos);
        if(DSGS(maxqueued) && duelqueue.find(ci) < 0 && playing.find(ci) < 0)
        {
            int count = 0;
            loopv(duelqueue) if(duelqueue[i]->actortype == A_PLAYER) count++;
            loopv(playing) if(playing[i]->actortype == A_PLAYER) count++;
            if(count >= DSGS(maxqueued))
            {
                spectator(ci);
                srvmsgft(ci->clientnum, CON_EVENT, "\fySorry, the \fs\fcqueue\fS is \fs\fzgcFULL\fS (max: \fs\fc%d\fS %s)", DSGS(maxqueued), DSGS(maxqueued) != 1 ? "players" : "player");
                return remqueue(ci, pos);
            }
        }
        if(ci->actortype == A_PLAYER && waitforhumans) waitforhumans = false;
        int n = duelqueue.find(ci);
        if(top)
        {
            if(n >= 0) duelqueue.remove(n);
            duelqueue.insert(0, ci);
            n = 0;
        }
        else if(n < 0)
        {
            n = duelqueue.length();
            duelqueue.add(ci);
        }
        if(wait && ci->state != CS_WAITING) waiting(ci, DROP_RESET);
        if(allowed.find(ci) >= 0) allowed.removeobj(ci);
        if(pos) position();
        return true;
    }

    void entergame(clientinfo *ci)
    {
        queue(ci);
        if(dueltime < 0 && dueldeath < 0 && m_affinity(gamemode) && (DSGS(affinity) || numclients() <= 1)) allowed.add(ci);
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(duelwinner == ci->clientnum)
        {
            duelwinner = -1;
            duelwins = 0;
        }
        allowed.removeobj(ci);
        playing.removeobj(ci);
        remqueue(ci);
    }

    bool damage(clientinfo *m, clientinfo *v, int damage, int weap, int flags, int material, const ivec &hitpush, const ivec &hitvel, float dist)
    {
        if(dueltime >= 0) return false;
        return true;
    }

    bool canspawn(clientinfo *ci, bool tryspawn = false)
    {
        if(allowed.find(ci) >= 0 || ci->actortype >= A_ENEMY) return true;
        if(gamestate == G_S_OVERTIME && !restricted.empty() && restricted.find(ci) < 0) return false;
        else if(tryspawn)
        {
            queue(ci);
            return true;
        }
        return false; // you spawn when we want you to buddy
    }

    void spawned(clientinfo *ci)
    {
        allowed.removeobj(ci);
    }

    void layout()
    {
        loopvj(clients)
        {
            vector<int> shots;
            loop(a, W_MAX) loop(b, 2)
            {
                loopv(clients[j]->weapshots[a][b].projs)
                    shots.add(clients[j]->weapshots[a][b].projs[i].id);
                clients[j]->weapshots[a][b].projs.shrink(0);
            }
            if(!shots.empty()) sendf(-1, 1, "ri4v", N_DESTROY, clients[j]->clientnum, PRJ_SHOT, shots.length(), shots.length(), shots.getbuf());

        }
        if(DSGS(clear))
        {
            loopv(sents) if(enttype[sents[i].type].usetype == EU_ITEM)
            {
                bool spawn = hasitem(i);
                int delay = 0;
                if(spawn && sents[i].type == WEAPON)
                {
                    int attr = m_attr(sents[i].type, sents[i].attrs[0]);
                    if(isweap(attr) && W(attr, spawnduke))
                    {
                        spawn = false;
                        delay = W(attr, spawnduke);
                    }
                }
                setspawn(i, spawn, true, true, delay);
            }
        }
    }

    void scoreaffinity(clientinfo *ci, bool win)
    {
        if(!m_affinity(gamemode) || dueltime >= 0 || duelround <= 0) return;
        #define scoredteam(x,y) (win ? x != y : x == y)
        int queued = 0;
        loopv(clients) if(clients[i]->actortype < A_ENEMY) switch(clients[i]->state)
        {
            case CS_ALIVE:
                if(playing.find(clients[i]) < 0 || scoredteam(clients[i]->team, ci->team)) if(queue(clients[i], false)) queued++;
                break;
            case CS_DEAD:
                if(playing.find(clients[i]) < 0 || scoredteam(clients[i]->team, ci->team))
                {
                    if(queue(clients[i], false)) queued++;
                    break;
                }
                if(allowed.find(clients[i]) < 0) allowed.add(clients[i]);
                waiting(clients[i], DROP_RESET);
                break;
            case CS_WAITING:
                if(playing.find(clients[i]) < 0 || scoredteam(clients[i]->team, ci->team))
                {
                    if(queue(clients[i], false)) queued++;
                    break;
                }
                if(allowed.find(clients[i]) < 0) allowed.add(clients[i]);
                break;
            default: break;
        }
        duelaffin = win ? ci->team : -ci->team;
        duelcheck = gamemillis+DSGS(delay);
        if(queued) position();
    }

    void clear()
    {
        doreset(false);
        bool reset = false;
        if(m_duel(gamemode, mutators) && G(duelcycle)&(m_team(gamemode, mutators) ? 2 : 1) && duelwinner >= 0 && duelwins > 0)
        {
            clientinfo *ci = (clientinfo *)getinfo(duelwinner);
            if(ci)
            {
                int numwins = G(duelcycles), numplrs = 0;
                loopv(clients)
                    if(clients[i]->actortype < A_ENEMY && clients[i]->state != CS_SPECTATOR && clients[i]->team == ci->team)
                        numplrs++;
                if(numplrs > (m_team(gamemode, mutators) ? 1 : 2))
                {
                    if(!numwins) numwins = numplrs;
                    if(duelwins >= numwins) reset = true;
                }
            }
            else
            {
                duelwinner = -1;
                duelwins = 0;
            }
        }
        int queued = 0;
        loopv(clients) if(queue(clients[i], false, !reset && clients[i]->state == CS_ALIVE, reset || DSGS(reset) || clients[i]->state != CS_ALIVE)) queued++;
        shrink();
        if(queued) position();
    }

    void endffaround(vector<clientinfo *> alive)
    {
        loopv(clients) if(playing.find(clients[i]) >= 0)
        {
            ffaroundstats rs;
            rs.round = duelround;
            rs.winner = !alive.empty() && clients[i] == alive[0];
            clients[i]->ffarounds.add(rs);
        }
    }

    void update()
    {
        if(!canplay() || waitforhumans) return;
        if(dueltime >= 0)
        {
            if(dueltime && ((dueltime -= curtime) <= 0)) dueltime = 0;
            if(!dueltime)
            {
                shrink();
                int wants = max(numteams(gamemode, mutators), 2);
                loopv(clients) if(clients[i]->state != CS_SPECTATOR && clients[i]->state != CS_ALIVE) queue(clients[i], false);
                if(gamestate == G_S_OVERTIME && DSGS(overtime) && m_ffa(gamemode, mutators) && restricted.empty())
                {
                    loopv(clients)
                    {
                        clientinfo *cs = clients[i];
                        if(cs->state != CS_ALIVE && cs->state != CS_WAITING)
                        {
                            remqueue(cs, false);
                            continue;
                        }
                        if(restricted.empty() || cs->points > restricted[0]->points)
                        {
                            restricted.shrink(0);
                            restricted.add(cs);
                        }
                        else if(cs->points == restricted[0]->points) restricted.add(cs);
                    }
                    loopv(clients)
                    {
                        clientinfo *cs = clients[i];
                        if(duelqueue.find(cs) >= 0 && restricted.find(cs) < 0)
                        {
                            remqueue(cs, false);
                        }
                    }
                }
                if(m_duel(gamemode, mutators) && !duelround && G(duelrandom))
                {
                    vector<clientinfo *> duelrand;
                    while(!duelqueue.empty())
                    {
                        int r = rnd(duelqueue.length());
                        duelrand.add(duelqueue[r]);
                        duelqueue.remove(r);
                    }
                    while(!duelrand.empty())
                    {
                        int r = rnd(duelrand.length());
                        duelqueue.add(duelrand[r]);
                        duelrand.remove(r);
                    }
                }
                loopv(duelqueue)
                {
                    if(m_duel(gamemode, mutators) && playing.length() >= wants) break;
                    if(duelqueue[i]->state != CS_ALIVE)
                    {
                        if(duelqueue[i]->state != CS_WAITING) waiting(duelqueue[i], DROP_RESET);
                        if(m_duel(gamemode, mutators) && m_team(gamemode, mutators))
                        {
                            bool skip = false;
                            loopvj(playing) if(duelqueue[i]->team == playing[j]->team) { skip = true; break; }
                            if(skip) continue;
                        }
                    }
                    playing.add(duelqueue[i]);
                }
                if(playing.length() >= wants)
                {
                    if(smode) smode->layout();
                    mutate(smuts, mut->layout());
                    duelround++;
                    stringz(fight);
                    if(m_duel(gamemode, mutators))
                    {
                        stringz(names);
                        loopv(playing)
                        {
                            concatstring(names, colourname(playing[i]));
                            if(i == wants-1) break;
                            else if(i == wants-2) concatstring(names, " and ");
                            else concatstring(names, ", ");
                        }
                        formatstring(fight, "Duel between %s, round \fs\fc#%d\fS", names, duelround);
                    }
                    else if(m_survivor(gamemode, mutators)) formatstring(fight, "Survivor, round \fs\fc#%d\fS", duelround);
                    loopv(playing)
                    {
                        if(playing[i]->state == CS_ALIVE)
                        {
                            playing[i]->lastregen = gamemillis;
                            playing[i]->lastregenamt = 0; // amt = 0 regens impulse
                            playing[i]->resetresidual();
                            playing[i]->health = playing[i]->gethealth(gamemode, mutators);
                            sendf(-1, 1, "ri4", N_REGEN, playing[i]->clientnum, playing[i]->health, playing[i]->lastregenamt);
                        }
                        else if(allowed.find(playing[i]) < 0) allowed.add(playing[i]);
                        duelqueue.removeobj(playing[i]);
                    }
                    if(gamestate == G_S_OVERTIME && !restricted.empty())
                        ancmsgft(-1, S_V_FIGHT, CON_EVENT, "\fy\fs\fzcgSudden Death\fS, %s", fight);
                    else ancmsgft(-1, S_V_FIGHT, CON_EVENT, "\fy%s", fight);
                    dueltime = dueldeath = -1;
                    duelcheck = gamemillis+5000;
                }
                else shrink();
                position();
            }
        }
        else if(duelround > 0)
        {
            bool cleanup = false;
            vector<clientinfo *> alive;
            int queued = 0;
            loopv(clients) if(clients[i]->actortype < A_ENEMY && clients[i]->state == CS_ALIVE)
            {
                if(playing.find(clients[i]) < 0) { if(queue(clients[i], false)) queued++; }
                else alive.add(clients[i]);
            }
            if(queued) position();
            if(!allowed.empty() && duelcheck >= 0 && gamemillis >= duelcheck) loopvrev(allowed)
            {
                if(alive.find(allowed[i]) < 0) spectator(allowed[i]);
                allowed.remove(i);
                cleanup = true;
            }
            if(!allowed.empty()) return;
            if(m_survivor(gamemode, mutators) && m_team(gamemode, mutators) && !alive.empty())
            {
                bool found = false;
                loopv(alive) if(i && alive[i]->team != alive[i-1]->team) { found = true; break; }
                if(!found)
                {
                    if(dueldeath < 0) dueldeath = gamemillis+DSGS(delay);
                    else if(gamemillis >= dueldeath)
                    {
                        if(!cleanup)
                        {
                            bool teampoints = true;
                            loopv(clients)
                            {
                                bool wasplay = playing.find(clients[i]) >= 0;
                                if(wasplay)
                                {
                                    if(clients[i]->team == alive[0]->team)
                                    {
                                        if(alive.find(clients[i]) >= 0)
                                        {
                                            if(!m_affinity(gamemode))
                                            {
                                                givepoints(clients[i], 1, !m_dm_oldschool(gamemode, mutators), teampoints);
                                                teampoints = false;
                                            }
                                            else if(!duelaffin && teampoints && !m_dm_oldschool(gamemode, mutators))
                                            {
                                                score &ts = teamscore(clients[i]->team);
                                                ts.total++;
                                                sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                                                teampoints = false;
                                            }
                                        }
                                    }
                                }
                                if(allowbroadcast(clients[i]->clientnum)) sendf(clients[i]->clientnum, 1, "ri3", N_DUELEND, alive[0]->team, wasplay ? 3 : 2);
                            }
                        }
                        clear();
                    }
                }
            }
            else switch(alive.length())
            {
                case 0:
                {
                    if(m_affinity(gamemode) && duelaffin) // reverse it!
                    {
                        score &ts = teamscore(abs(duelaffin));
                        if(duelaffin > 0) ts.total--;
                        else ts.total++;
                        sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                    }
                    if(!cleanup)
                    {
                        endffaround(alive);
                        sendf(-1, 1, "ri2", N_DUELEND, -1);
                        duelwinner = -1;
                        duelwins = 0;
                    }
                    clear();
                    break;
                }
                case 1:
                {
                    if(dueldeath < 0)
                    {
                        dueldeath = gamemillis+DSGS(delay);
                        break;
                    }
                    else if(gamemillis < dueldeath) break;
                    if(!cleanup)
                    {
                        endffaround(alive);
                        if(duelwinner != alive[0]->clientnum)
                        {
                            duelwinner = alive[0]->clientnum;
                            duelwins = 1;
                        }
                        else
                        {
                            duelwins++;
                        }
                        loopv(clients)
                        {
                            bool wasplay = playing.find(clients[i]) >= 0;
                            if(wasplay && clients[i] == alive[0] && !m_dm_oldschool(gamemode, mutators))
                            {
                                if(!m_affinity(gamemode)) givepoints(clients[i], 1, true, true);
                                else if(!duelaffin)
                                {
                                    score &ts = teamscore(clients[i]->team);
                                    ts.total++;
                                    sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                                }
                            }
                            if(allowbroadcast(clients[i]->clientnum)) sendf(clients[i]->clientnum, 1, "ri4", N_DUELEND, duelwinner, wasplay ? 1 : 0, duelwins);
                        }
                    }
                    clear();
                    break;
                }
                default: break;
            }
        }
    }

    bool wantsovertime()
    {
        if(dueltime < 0 && duelround > 0) return true;
        return false;
    }

    bool canbalance()
    {
        if(dueltime < 0 && duelround > 0) return false;
        return true;
    }

    void balance(int oldbalance)
    {
        doreset(true);
    }
} duelmutator;
#endif
