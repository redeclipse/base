// server-side ai manager
namespace aiman
{
    int dorefresh = 0, oldbotskillmin = -1, oldbotskillmax = -1, oldcoopskillmin = -1, oldcoopskillmax = -1, oldenemyskillmin = -1, oldenemyskillmax = -1,
        oldbotbalance = -2, oldnumplayers = -1, oldbotlimit = -1, oldbotoffset = 0, oldenemylimit = -1;
    float oldbotbalancescale = -1;

    int botrnd(clientinfo *ci, int t, int m)
    {
        if(G(botcolourseed)&t) return ci->colour%m;
        return rnd(m);
    }

    float clientbotscore(clientinfo *ci)
    {
        return (ci->bots.length() * G(aihostnum)) + (ci->ping * G(aihostping));
    }

    clientinfo *findaiclient(clientinfo *exclude = NULL)
    {
        clientinfo *least = NULL;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->actortype > A_PLAYER || !ci->online || !ci->isready() || ci == exclude) continue;
            if(!least || clientbotscore(ci) < clientbotscore(least)) least = ci;
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
                    if(deaths != 0 && G(coopskilldeaths) != 0)
                    {
                        int amt = G(coopskilldeaths)*deaths;
                        m += amt;
                        n += amt;
                    }
                    if(frags != 0 && G(coopskillfrags) != 0)
                    {
                        int amt = G(coopskillfrags)*frags;
                        m += amt;
                        n += amt;
                    }
                }
                else
                {
                    m = max(G(botskillmax), G(botskillmin));
                    n = min(G(botskillmin), m);
                    if(deaths != 0 && G(botskilldeaths) != 0)
                    {
                        int amt = G(botskilldeaths)*deaths;
                        m += amt;
                        n += amt;
                    }
                    if(frags != 0 && G(botskillfrags) != 0)
                    {
                        int amt = G(botskillfrags)*frags;
                        m += amt;
                        n += amt;
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
        getskillrange(ci->actortype, n, m, ci->frags, ci->deaths);
        if(ci->skill > m || ci->skill < n)
        { // needs re-skilling
            ci->skill = (m != n ? botrnd(ci, 2, m-n) + n + 1 : m);
            if(!ci->aireinit) ci->aireinit = 1;
        }
    }

    bool addai(int type, int ent, int skill)
    {
        int count = 0, limit = getlimit(type);
        if(!limit) return false;
        loopv(clients) if(clients[i]->actortype == type)
        {
            clientinfo *ci = clients[i];
            if(ci->ownernum < 0)
            { // reuse a slot that was going to removed
                clientinfo *owner = findaiclient();
                if(!owner) return false;
                ci->ownernum = owner->clientnum;
                owner->bots.add(ci);
                ci->aireinit = 1;
                ci->actortype = type;
                ci->spawnpoint = ent;
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
                ci->clientnum = cn;
                clientinfo *owner = findaiclient();
                ci->ownernum = owner ? owner->clientnum : -1;
                if(owner) owner->bots.add(ci);
                ci->aireinit = 2;
                ci->actortype = type;
                ci->spawnpoint = ent;
                clients.add(ci);
                ci->lasttimeplayed = totalmillis;
                ci->colour = rnd(0xFFFFFF);
                ci->model = botrnd(ci, 4, PLAYERTYPES);
                ci->pattern = botrnd(ci, 4, PLAYERPATTERNS);
                int s = skill, n = 1, m = 100;
                getskillrange(type, n, m);
                if(skill > m || skill < n) s = (m != n ? botrnd(ci, 2, m-n) + n + 1 : m);
                ci->skill = clamp(s, 1, 101);
                copystring(ci->name, AA(ci->actortype, vname), MAXNAMELEN);
                ci->loadweap.shrink(0);
                if(ci->actortype == A_BOT)
                {
                    const char *list = ci->model ? G(botfemalenames) : G(botmalenames);
                    int len = listlen(list);
                    if(len > 0)
                    {
                        int r = botrnd(ci, 1, len);
                        char *name = indexlist(list, r);
                        if(name)
                        {
                            if(*name)
                            {
                                copystring(ci->name, name, MAXNAMELEN);
                                if(G(botrandomcase) && rnd(G(botrandomcase)))
                                    ci->name[0] = iscubeupper(ci->name[0]) ? cubelower(ci->name[0]) : cubeupper(ci->name[0]);
                            }
                            delete[] name;
                        }
                    }
                    ci->setvanity(ci->model ? G(botfemalevanities) : G(botmalevanities));
                    static vector<int> weaplist;
                    weaplist.shrink(0);
                    loopi(W_LOADOUT) weaplist.add(W_OFFSET+i);
                    while(!weaplist.empty())
                    {
                        int iter = botrnd(ci, 8, weaplist.length());
                        ci->loadweap.add(weaplist[iter]);
                        weaplist.remove(iter);
                    }
                }
                ci->state = CS_DEAD;
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
        if(ci->actortype == A_PLAYER) return;
        int cn = ci->clientnum;
        loopv(clients) if(clients[i] != ci)
        {
            loopvk(clients[i]->fraglog) if(clients[i]->fraglog[k] == ci->clientnum)
                clients[i]->fraglog.remove(k--);
        }
        if(smode) smode->leavegame(ci, true);
        mutate(smuts, mut->leavegame(ci, true));
        savescore(ci);
        sendf(-1, 1, "ri3", N_DISCONNECT, cn, DISC_NONE);
        clientinfo *owner = (clientinfo *)getinfo(ci->ownernum);
        if(owner) owner->bots.removeobj(ci);
        clients.removeobj(ci);
        delclient(cn);
        dorefresh = max(dorefresh, 1);
    }

    bool delai(int type, bool skip)
    {
        bool retry = false;
        loopvrev(clients) if(clients[i]->actortype == type && clients[i]->ownernum >= 0)
        {
            if(!skip || clients[i]->state == CS_DEAD || clients[i]->state == CS_WAITING)
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
        if(ci->actortype == A_PLAYER) return;
        if(ci->ownernum < 0) deleteai(ci);
        else if(ci->aireinit >= 1)
        {
            if(ci->aireinit == 2) loopk(W_MAX) loopj(2) ci->weapshots[k][j].reset();
            sendf(-1, 1, "ri6si4siv", N_INITAI, ci->clientnum, ci->ownernum, ci->actortype, ci->spawnpoint, ci->skill, ci->name, ci->team, ci->colour, ci->model, ci->pattern, ci->vanity, ci->loadweap.length(), ci->loadweap.length(), ci->loadweap.getbuf());
            if(ci->aireinit == 2)
            {
                waiting(ci, DROP_RESET);
                if(smode) smode->entergame(ci);
                mutate(smuts, mut->entergame(ci));
            }
            ci->aireinit = 0;
        }
    }

    void shiftai(clientinfo *ci, clientinfo *owner = NULL)
    {
        clientinfo *prevowner = (clientinfo *)getinfo(ci->ownernum);
        if(prevowner) prevowner->bots.removeobj(ci);
        if(!owner) { ci->aireinit = 0; ci->ownernum = -1; }
        else if(ci->ownernum != owner->clientnum) { ci->aireinit = 1; ci->ownernum = owner->clientnum; owner->bots.add(ci); }
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
            if(ci->clientnum < 0 || ci->actortype > A_PLAYER || !ci->isready() || ci == exclude)
                continue;
            if(!lo || clientbotscore(ci) < clientbotscore(lo)) lo = ci;
            if(!hi || clientbotscore(hi) > clientbotscore(hi)) hi = ci;
        }
        if(hi && lo && clientbotscore(hi) - clientbotscore(lo) > G(aihostshift))
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
        loopv(clients) if(clients[i]->actortype > A_PLAYER && clients[i]->ownernum >= 0)
        {
            clientinfo *ci = clients[i];
            if(ci->actortype == A_BOT && ++numbots >= blimit) { shiftai(ci, NULL); continue; }
            if(ci->actortype >= A_ENEMY && ++numenemies >= elimit) { shiftai(ci, NULL); continue; }
            setskill(ci);
        }

        int people = numclients(-1, true, -1), balance = people, numt = numteams(gamemode, mutators);
        if(m_coop(gamemode, mutators))
        {
            numt--; // filter out the human team
            balance += int(ceilf(people*numt*(m_multi(gamemode, mutators) ? G(coopmultibalance) : G(coopbalance))));
            balance += G(botoffset)*numt;
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
            balance += G(botoffset)*numt;
            if(!m_duke(gamemode, mutators) && G(botbalancescale) != 1) balance = int(ceilf(balance*G(botbalancescale)));
            if(balance > 0 && m_team(gamemode, mutators))
            { // skew this if teams are unbalanced
                int plrs[T_TOTAL] = {0}, highest = -1, bots = 0, offset = balance%numt; // we do this because humans can unbalance in odd ways
                if(offset) balance += numt-offset;
                loopv(clients) if(clients[i]->team >= T_FIRST && isteam(gamemode, mutators, clients[i]->team, T_FIRST))
                {
                    if(clients[i]->actortype == A_BOT)
                    {
                        bots++;
                        continue;
                    }
                    int team = clients[i]->team-T_FIRST;
                    plrs[team]++;
                    if(highest < 0 || plrs[team] > plrs[highest]) highest = team;
                }
                if(highest >= 0) loopi(numt) if(i != highest && plrs[i] < plrs[highest]) loopj(plrs[highest]-plrs[i])
                {
                    if(bots > 0) bots--;
                    else balance++;
                }
            }
        }
        else balance += G(botoffset)*numt;
        int bots = balance-people;
        if(bots > blimit) balance -= bots-blimit;
        if(numt > 1 && (balance%numt) != 0) balance -= balance%numt;
        if(balance > 0)
        {
            while(numclients(-1, true, A_BOT) < balance) if(!addai(A_BOT)) break;
            while(numclients(-1, true, A_BOT) > balance) if(!delai(A_BOT)) break;
            if(m_team(gamemode, mutators)) loopvrev(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->actortype == A_BOT && ci->ownernum >= 0)
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
            loopvj(sents) if(sents[j].type == ACTOR)
            {
                if(!checkmapvariant(sents[j].attrs[enttype[sents[j].type].mvattr])) continue;
                if(sents[j].attrs[0] < 0 || sents[j].attrs[0] >= A_TOTAL || gamemillis < sents[j].millis) continue;
                if(sents[j].attrs[5] && sents[j].attrs[5] != triggerid) continue;
                if(!m_check(sents[j].attrs[3], sents[j].attrs[4], gamemode, mutators)) continue;
                if(sents[j].attrs[0]+A_ENEMY == A_TURRET && m_insta(gamemode, mutators)) continue;
                int count = 0, numenemies = 0;
                loopvrev(clients) if(clients[i]->actortype >= A_ENEMY)
                {
                    if(clients[i]->spawnpoint == j)
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
        loopvrev(clients) if(!type || (type == 2 ? clients[i]->actortype >= A_ENEMY : clients[i]->actortype == A_BOT))
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
                loopvrev(clients) if(clients[i]->actortype > A_PLAYER) reinitai(clients[i]);
                while(true) if(!reassignai()) break;
            }
        }
        else clearai();
    }
}
