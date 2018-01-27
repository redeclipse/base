struct defendservmode : defendstate, servmode
{
    int scoresec;
    bool hasflaginfo;

    defendservmode() : scoresec(0), hasflaginfo(false) {}

    void reset()
    {
        defendstate::reset();
        scoresec = 0;
        hasflaginfo = false;
    }

    void stealaffinity(int n, int team)
    {
        flag &b = flags[n];
        loopv(clients) if(AA(clients[i]->actortype, abilities)&(1<<A_A_AFFINITY))
        {
            server::clientinfo *ci = clients[i];
            if(ci->state == CS_ALIVE && ci->team && ci->team == team && insideaffinity(b, ci->o))
                b.enter(ci->team);
        }
        sendaffinity(n);
    }

    void moveaffinity(int team, const vec &oldpos, const vec &newpos)
    {
        if(!team) return;
        loopv(flags)
        {
            flag &b = flags[i];
            bool leave = insideaffinity(b, oldpos),
                 enter = insideaffinity(b, newpos);
            if(leave && !enter && b.leave(team)) sendaffinity(i);
            else if(enter && !leave && b.enter(team)) sendaffinity(i);
            else if(leave && enter && b.steal(team)) stealaffinity(i, team);
        }
    }

    void leaveaffinity(int team, const vec &o)
    {
        moveaffinity(team, o, vec(-1e10f, -1e10f, -1e10f));
    }

    void enteraffinity(int team, const vec &o)
    {
        moveaffinity(team, vec(-1e10f, -1e10f, -1e10f), o);
    }

    void addscore(int i, int team, int points)
    {
        if(!points) return;
        flag &b = flags[i];
        loopvk(clients) if(AA(clients[k]->actortype, abilities)&(1<<A_A_AFFINITY) && team == clients[k]->team && insideaffinity(b, clients[k]->o)) givepoints(clients[k], points, m_points(gamemode, mutators), false);
        score &cs = teamscore(team);
        cs.total += points;
        sendf(-1, 1, "ri3", N_SCORE, team, cs.total);
    }

    void update()
    {
        int t = (gamemillis/G(defendinterval))-((gamemillis-(curtime+scoresec))/G(defendinterval));
        if(t < 1) { scoresec += curtime; return; }
        else scoresec = 0;
        loopv(flags)
        {
            flag &b = flags[i];
            if(b.enemy)
            {
                if(!b.owners || !b.enemies)
                {
                    int pts = b.occupy(b.enemy, G(defendpoints)*(b.enemies ? b.enemies : -(1+b.owners))*t, defendcount, m_dac_quick(gamemode, mutators));
                    if(pts > 0) loopvk(clients) if(AA(clients[k]->actortype, abilities)&(1<<A_A_AFFINITY) && b.owner == clients[k]->team && insideaffinity(b, clients[k]->o)) givepoints(clients[k], G(defendpoints), m_points(gamemode, mutators), false);
                }
                sendaffinity(i);
            }
            else if(b.owner)
            {
                if(!m_dac_king(gamemode, mutators) || b.owners)
                {
                    b.points += G(defendpoints)*t;
                    int score = 0;
                    while(b.points >= G(defendhold))
                    {
                        b.points -= G(defendhold);
                        score++;
                    }
                    if(score) addscore(i, b.owner, score);
                }
                sendaffinity(i);
            }
        }
    }

    void sendaffinity(int i, bool interim = false)
    {
        flag &b = flags[i];
        sendf(-1, 1, "ri5", N_INFOAFFIN, i, interim ? -1 : (b.enemy ? b.converted : 0), b.owner, b.enemy);
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
        putint(p, N_SETUPAFFIN);
        putint(p, flags.length());
        loopv(flags)
        {
            flag &b = flags[i];
            putint(p, b.kinship);
            putint(p, b.yaw);
            putint(p, b.pitch);
            putint(p, b.converted);
            putint(p, b.owner);
            putint(p, b.enemy);
            loopj(3) putint(p, int(b.o[j]*DMF));
            sendstring(b.name, p);
        }
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(!oi || !oi->connected || (ci && oi->clientnum == ci->clientnum) || !oi->lastbuff) continue;
            putint(p, N_SPHY);
            putint(p, oi->clientnum);
            putint(p, SPHY_BUFF);
            putint(p, 1);
        }
    }

    void entergame(clientinfo *ci)
    {
        if(!canplay() || !hasflaginfo || ci->state != CS_ALIVE || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        enteraffinity(ci->team, ci->o);
    }

    void spawned(clientinfo *ci)
    {
        if(!canplay() || !hasflaginfo || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        enteraffinity(ci->team, ci->o);
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(!canplay() || !hasflaginfo || ci->state != CS_ALIVE || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        leaveaffinity(ci->team, ci->o);
    }

    void died(clientinfo *ci, clientinfo *v)
    {
        if(!canplay() || !hasflaginfo || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        leaveaffinity(ci->team, ci->o);
    }

    void moved(clientinfo *ci, const vec &oldpos, const vec &newpos)
    {
        if(!canplay() || !hasflaginfo || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY)) || ci->state != CS_ALIVE) return;
        moveaffinity(ci->team, oldpos, newpos);
    }

    void regen(clientinfo *ci, int &total, int &amt, int &delay)
    {
        if(!canplay() || !hasflaginfo || !G(defendregenbuff) || !ci->lastbuff) return;
        if(G(maxhealth)) total = max(m_maxhealth(gamemode, mutators, ci->actortype), total);
        if(ci->lastregen && G(defendregendelay)) delay = G(defendregendelay);
        if(G(defendregenextra)) amt += G(defendregenextra);
    }

    void checkclient(clientinfo *ci)
    {
        if(!canplay() || !hasflaginfo || ci->state != CS_ALIVE || m_insta(gamemode, mutators)) return;
        #define defendbuff4 (G(defendbuffing)&4 && b.occupied(m_dac_quick(gamemode, mutators), defendcount) >= G(defendbuffoccupy))
        #define defendbuff1 (G(defendbuffing)&1 && b.owner == ci->team && (!b.enemy || defendbuff4))
        #define defendbuff2 (G(defendbuffing)&2 && b.owner == T_NEUTRAL && (b.enemy == ci->team || defendbuff4))
        if(G(defendbuffing)) loopv(flags)
        {
            flag &b = flags[i];
            if((defendbuff1 || defendbuff2) && (G(defendbuffarea) ? insideaffinity(b, ci->o, G(defendbuffarea)) : true))
            {
                if(!ci->lastbuff) sendf(-1, 1, "ri4", N_SPHY, ci->clientnum, SPHY_BUFF, 1);
                ci->lastbuff = gamemillis;
                return;
            }
        }
        if(ci->lastbuff && (!G(defendbuffing) || gamemillis-ci->lastbuff >= G(defendbuffdelay)))
        {
            ci->lastbuff = 0;
            sendf(-1, 1, "ri4", N_SPHY, ci->clientnum, SPHY_BUFF, 0);
        }
    }

    void parseaffinity(ucharbuf &p)
    {
        int numflags = getint(p);
        if(numflags <= 0) return;
        loopi(numflags)
        {
            int kin = getint(p), yaw = getint(p), pitch = getint(p);
            vec o;
            loopj(3) o[j] = getint(p)/DMF;
            string name;
            getstring(name, p);
            if(p.overread()) break;
            if(!hasflaginfo && i < MAXPARAMS) addaffinity(o, kin, yaw, pitch, name);
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
        bool isteam = m == v || m->team == v->team;
        int p = isteam ? -1 : 1, q = p;
        loopv(flags) if(insideaffinity(flags[i], m->o)) p += q;
        return p;
    }

    void balance(int oldbalance)
    {
        static vector<int> owners[T_TOTAL], enemies[T_TOTAL], modified;
        loopk(T_TOTAL)
        {
            owners[k].setsize(0);
            enemies[k].setsize(0);
        }
        modified.setsize(0);
        loopv(flags)
        {
            if(isteam(gamemode, mutators, flags[i].owner, T_FIRST))
                owners[flags[i].owner-T_FIRST].add(i);
            if(isteam(gamemode, mutators, flags[i].enemy, T_FIRST))
                enemies[flags[i].enemy-T_FIRST].add(i);
        }
        loopk(numteams(gamemode, mutators))
        {
            int from = mapbals[oldbalance][k], fromt = from-T_FIRST, to = mapbals[curbalance][k];
            loopv(owners[fromt])
            {
                flags[owners[fromt][i]].owner = to;
                if(modified.find(owners[fromt][i]) < 0) modified.add(owners[fromt][i]);
            }
            loopv(enemies[fromt])
            {
                flags[enemies[fromt][i]].enemy = to;
                if(modified.find(enemies[fromt][i]) < 0) modified.add(enemies[fromt][i]);
            }
        }
        loopv(modified) sendaffinity(modified[i], true);
    }
} defendmode;
