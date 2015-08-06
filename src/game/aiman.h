// server-side ai manager
namespace aiman
{
    int dorefresh = 0, oldbotskillmin = -1, oldbotskillmax = -1, oldcoopskillmin = -1, oldcoopskillmax = -1, oldenemyskillmin = -1, oldenemyskillmax = -1,
        oldbotbalance = -2, oldnumplayers = -1, oldbotlimit = -1, oldbotoffset = 0, oldenemylimit = -1;
    float oldbotbalancescale = -1;

    clientinfo *findaiclient(clientinfo *exclude = NULL)
    {
        clientinfo *least = NULL;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.actortype > A_PLAYER || !ci->online || !ci->isready() || ci == exclude) continue;
            if(!least || ci->bots.length() < least->bots.length()) least = ci;
        }
        return least;
    }

    int getlimit(int type = A_BOT)
    {
        if(type >= A_ENEMY) return G(enemylimit);
        if(m_coop(gamemode, mutators))
        {
            int people = numclients(-1, true, -1), numt = numteams(gamemode, mutators)-1;
            return min(int(ceilf(people*numt*(m_multi(gamemode, mutators) ? G(coopmultibalance) : G(coopbalance)))), MAXAI);
        }
        return G(botlimit);
    }

    void getskillrange(int type, int &n, int &m, int frags = -1, int deaths = -1)
    {
        switch(type)
        {
            case A_BOT:
                if(m_coop(gamemode, mutators))
                {
                    m = max(G(coopskillmax), G(coopskillmin));
                    n = min(G(coopskillmin), m);
                    if(deaths > 0 && G(coopskilldeaths) > 0)
                    {
                        int amt = G(coopskilldeaths)*deaths;
                        m += amt;
                        n += amt;
                    }
                    if(frags > 0 && G(coopskillfrags) > 0)
                    {
                        int amt = G(coopskillfrags)*frags;
                        m -= amt;
                        n -= amt;
                    }
                }
                else
                {
                    m = max(G(botskillmax), G(botskillmin));
                    n = min(G(botskillmin), m);
                    if(deaths > 0 && G(botskilldeaths) > 0)
                    {
                        int amt = G(botskilldeaths)*deaths;
                        m += amt;
                        n += amt;
                    }
                    if(frags > 0 && G(botskillfrags) > 0)
                    {
                        int amt = G(botskillfrags)*frags;
                        m -= amt;
                        n -= amt;
                    }
                }
                break;
            default:
                m = max(G(enemyskillmax), G(enemyskillmin));
                n = min(G(enemyskillmin), m);
                break;
        }
        m = min(m, 101);
        n = min(n, m);
    }

    void setskill(clientinfo *ci)
    {
        int n = 1, m = 100;
        getskillrange(ci->state.actortype, n, m, ci->state.frags, ci->state.deaths);
        if(ci->state.skill > m || ci->state.skill < n)
        { // needs re-skilling
            ci->state.skill = (m != n ? rnd(m-n) + n + 1 : m);
            if(!ci->state.aireinit) ci->state.aireinit = 1;
        }
    }

    bool addai(int type, int ent, int skill)
    {
        int count = 0, limit = getlimit(type);
        if(!limit) return false;
        loopv(clients) if(clients[i]->state.actortype == type)
        {
            clientinfo *ci = clients[i];
            if(ci->state.ownernum < 0)
            { // reuse a slot that was going to removed
                clientinfo *owner = findaiclient();
                if(!owner) return false;
                ci->state.ownernum = owner->clientnum;
                owner->bots.add(ci);
                ci->state.aireinit = 1;
                ci->state.actortype = type;
                ci->state.spawnpoint = ent;
                return true;
            }
            if(++count >= limit) return false;
        }
        int cn = addclient(ST_REMOTE);
        if(cn >= 0)
        {
            clientinfo *ci = (clientinfo *)getinfo(cn);
            if(ci)
            {
                int s = skill, n = 1, m = 100;
                getskillrange(type, n, m);
                if(skill > m || skill < n) s = (m != n ? rnd(m-n) + n + 1 : m);
                ci->clientnum = cn;
                clientinfo *owner = findaiclient();
                ci->state.ownernum = owner ? owner->clientnum : -1;
                if(owner) owner->bots.add(ci);
                ci->state.aireinit = 2;
                ci->state.actortype = type;
                ci->state.spawnpoint = ent;
                ci->state.skill = clamp(s, 1, 101);
                clients.add(ci);
                ci->state.lasttimeplayed = totalmillis;
                ci->state.colour = rnd(0xFFFFFF);
                ci->state.model = rnd(PLAYERTYPES);
                ci->state.setvanity(ci->state.model ? G(botfemalevanities) : G(botmalevanities)); // the first slot is special
                copystring(ci->name, actor[ci->state.actortype].name, MAXNAMELEN);
                ci->state.loadweap.shrink(0);
                if(ci->state.actortype == A_BOT)
                {
                    const char *list = ci->state.model ? G(botfemalenames) : G(botmalenames);
                    int len = listlen(list);
                    if(len > 0)
                    {
                        int r = rnd(len);
                        char *name = indexlist(list, r);
                        if(name)
                        {
                            if(*name) copystring(ci->name, name, MAXNAMELEN);
                            delete[] name;
                        }
                    }
                    ci->state.loadweap.add(rnd(W_LOADOUT)+W_OFFSET);
                }
                ci->state.state = CS_DEAD;
                ci->team = type == A_BOT ? T_NEUTRAL : T_ENEMY;
                ci->online = ci->connected = ci->ready = true;
                return true;
            }
            delclient(cn);
        }
        return false;
    }

    void deleteai(clientinfo *ci)
    {
        if(ci->state.actortype == A_PLAYER) return;
        int cn = ci->clientnum;
        loopv(clients) if(clients[i] != ci)
        {
            loopvk(clients[i]->state.fraglog) if(clients[i]->state.fraglog[k] == ci->clientnum)
                clients[i]->state.fraglog.remove(k--);
        }
        if(smode) smode->leavegame(ci, true);
        mutate(smuts, mut->leavegame(ci, true));
        savescore(ci);
        sendf(-1, 1, "ri3", N_DISCONNECT, cn, DISC_NONE);
        clientinfo *owner = (clientinfo *)getinfo(ci->state.ownernum);
        if(owner) owner->bots.removeobj(ci);
        clients.removeobj(ci);
        delclient(cn);
        dorefresh = max(dorefresh, 1);
    }

    bool delai(int type, bool skip)
    {
        bool retry = false;
        loopvrev(clients) if(clients[i]->state.actortype == type && clients[i]->state.ownernum >= 0)
        {
            if(!skip || clients[i]->state.state == CS_DEAD || clients[i]->state.state == CS_WAITING)
            {
                deleteai(clients[i]);
                return true;
            }
            else if(skip && !retry) retry = true;
        }
        if(skip && retry) delai(type, false);
        return false;
    }

    void reinitai(clientinfo *ci)
    {
        if(ci->state.actortype == A_PLAYER) return;
        if(ci->state.ownernum < 0) deleteai(ci);
        else if(ci->state.aireinit >= 1)
        {
            if(ci->state.aireinit == 2) loopk(W_MAX) loopj(2) ci->state.weapshots[k][j].reset();
            sendf(-1, 1, "ri6si3siv", N_INITAI, ci->clientnum, ci->state.ownernum, ci->state.actortype, ci->state.spawnpoint, ci->state.skill, ci->name, ci->team, ci->state.colour, ci->state.model, ci->state.vanity, ci->state.loadweap.length(), ci->state.loadweap.length(), ci->state.loadweap.getbuf());
            if(ci->state.aireinit == 2)
            {
                waiting(ci, DROP_RESET);
                if(smode) smode->entergame(ci);
                mutate(smuts, mut->entergame(ci));
            }
            ci->state.aireinit = 0;
        }
    }

    void shiftai(clientinfo *ci, clientinfo *owner = NULL)
    {
        clientinfo *prevowner = (clientinfo *)getinfo(ci->state.ownernum);
        if(prevowner) prevowner->bots.removeobj(ci);
        if(!owner) { ci->state.aireinit = 0; ci->state.ownernum = -1; }
        else if(ci->state.ownernum != owner->clientnum) { ci->state.aireinit = 1; ci->state.ownernum = owner->clientnum; owner->bots.add(ci); }
    }

    void removeai(clientinfo *ci, bool complete)
    { // either schedules a removal, or someone else to assign to
        loopvrev(ci->bots) shiftai(ci->bots[i], complete ? NULL : findaiclient(ci));
    }

    bool reassignai(clientinfo *exclude)
    {
        clientinfo *hi = NULL, *lo = NULL;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->clientnum < 0 || ci->state.actortype > A_PLAYER || !ci->isready() || ci == exclude)
                continue;
            if(!lo || ci->bots.length() < lo->bots.length()) lo = ci;
            if(!hi || hi->bots.length() > hi->bots.length()) hi = ci;
        }
        if(hi && lo && hi->bots.length() - lo->bots.length() > 1)
        {
            loopvrev(hi->bots)
            {
                shiftai(hi->bots[i], lo);
                return true;
            }
        }
        return false;
    }

    void checksetup()
    {
        int numbots = 0, numenemies = 0, blimit = getlimit(A_BOT), elimit = getlimit(A_ENEMY);
        loopv(clients) if(clients[i]->state.actortype > A_PLAYER && clients[i]->state.ownernum >= 0)
        {
            clientinfo *ci = clients[i];
            if(ci->state.actortype == A_BOT && ++numbots >= blimit) { shiftai(ci, NULL); continue; }
            if(ci->state.actortype >= A_ENEMY && ++numenemies >= elimit) { shiftai(ci, NULL); continue; }
            setskill(ci);
        }

        int balance = 0, people = numclients(-1, true, -1), numt = numteams(gamemode, mutators);
        if(m_coop(gamemode, mutators))
        {
            numt--; // filter out the human team
            balance = people+int(ceilf(people*numt*(m_multi(gamemode, mutators) ? G(coopmultibalance) : G(coopbalance))));
        }
        else if(m_bots(gamemode) && blimit > 0)
        {
            int bb = m_botbal(gamemode, mutators);
            switch(bb)
            {
                case -1: balance = max(people, G(numplayers)); break; // use distributed map players
                case  0: balance = 0; break; // no bots
                default: balance = max(people, bb); break; // balance to at least this
            }
            if(balance > 0)
            {
                if(!m_duke(gamemode, mutators) && !m_coop(gamemode, mutators) && G(botbalancescale) != 1) balance = int(ceilf(balance*G(botbalancescale)));
                if(m_team(gamemode, mutators))
                { // skew this if teams are unbalanced
                    int plrs[T_TOTAL] = {0}, highest = -1; // we do this because humans can unbalance in odd ways
                    loopv(clients) if(clients[i]->state.actortype == A_PLAYER && clients[i]->team >= T_FIRST && isteam(gamemode, mutators, clients[i]->team, T_FIRST))
                    {
                        int team = clients[i]->team-T_FIRST;
                        plrs[team]++;
                        if(highest < 0 || plrs[team] > plrs[highest]) highest = team;
                    }
                    if(highest >= 0)
                    {
                        int bots = balance-people;
                        loopi(numt) if(i != highest && plrs[i] < plrs[highest]) loopj(plrs[highest]-plrs[i])
                        {
                            if(bots > 0) bots--;
                            else balance++;
                        }
                    }
                }
            }
        }
        balance += G(botoffset)*numt;
        int bots = balance-people;
        if(bots > blimit) balance -= bots-blimit;
        if(balance > 0)
        {
            while(numclients(-1, true, A_BOT) < balance) if(!addai(A_BOT)) break;
            while(numclients(-1, true, A_BOT) > balance) if(!delai(A_BOT)) break;
            if(m_team(gamemode, mutators)) loopvrev(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->state.actortype == A_BOT && ci->state.ownernum >= 0)
                {
                    int teamb = chooseteam(ci, ci->team);
                    if(ci->team != teamb) setteam(ci, teamb, TT_RESETX);
                }
            }
        }
        else clearai(1);
    }

    void checkenemies()
    {
        if(m_onslaught(gamemode, mutators))
        {
            loopvj(sents) if(sents[j].type == ACTOR && sents[j].attrs[0] >= 0 && sents[j].attrs[0] < A_TOTAL && gamemillis >= sents[j].millis && (sents[j].attrs[5] == triggerid || !sents[j].attrs[5]) && m_check(sents[j].attrs[3], sents[j].attrs[4], gamemode, mutators) && (sents[j].attrs[0] != A_TURRET || !m_insta(gamemode, mutators)))
            {
                int count = 0, numenemies = 0;
                loopvrev(clients) if(clients[i]->state.actortype >= A_ENEMY)
                {
                    if(clients[i]->state.spawnpoint == j)
                    {
                        count++;
                        if(count > G(enemybalance))
                        {
                            deleteai(clients[i]);
                            count--;
                            continue;
                        }
                    }
                    numenemies++;
                }
                if(numenemies < G(enemylimit) && count < G(enemybalance))
                {
                    int amt = min(G(enemybalance)-count, G(enemylimit)-numenemies);
                    loopk(amt) addai(sents[j].attrs[0]+A_ENEMY, j);
                    sents[j].millis = gamemillis+G(enemyspawntime);
                }
            }
        }
        else clearai(2);
    }

    void clearai(int type)
    { // clear and remove all ai immediately
        loopvrev(clients) if(!type || (type == 2 ? clients[i]->state.actortype >= A_ENEMY : clients[i]->state.actortype == A_BOT))
            deleteai(clients[i]);
    }

    void poke()
    {
        dorefresh = max(dorefresh, G(airefreshdelay));
    }

    void checkai()
    {
        if(!m_demo(gamemode) && numclients())
        {
            if(canplay())
            {
                if(!dorefresh)
                {
                    #define checkold(n) if(old##n != G(n)) { dorefresh = -1; old##n = G(n); }
                    if(m_onslaught(gamemode, mutators))
                    {
                        checkold(enemyskillmin);
                        checkold(enemyskillmax);
                    }
                    if(m_coop(gamemode, mutators))
                    {
                        checkold(coopskillmin);
                        checkold(coopskillmax);
                    }
                    else
                    {
                        checkold(botskillmin);
                        checkold(botskillmax);
                        checkold(botbalancescale);
                    }
                    checkold(botlimit);
                    checkold(botoffset);
                    checkold(enemylimit);
                    checkold(numplayers);
                    int bb = m_botbal(gamemode, mutators);
                    if(oldbotbalance != bb) { dorefresh = 1; oldbotbalance = bb; }
                }
                if(dorefresh)
                {
                    if(dorefresh > 0) dorefresh -= curtime;
                    if(dorefresh <= 0)
                    {
                        if(canbalancenow())
                        {
                            dorefresh = 0;
                            checksetup();
                        }
                        else dorefresh = -1;
                    }
                }
                checkenemies();
                loopvrev(clients) if(clients[i]->state.actortype > A_PLAYER) reinitai(clients[i]);
                while(true) if(!reassignai()) break;
            }
        }
        else clearai();
    }
}
