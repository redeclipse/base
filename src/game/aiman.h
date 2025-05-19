// server-side ai manager
namespace aiman
{
    int curbalance = 0, lastcheck = 0;
    bool oldcancheck = false;

    void intermission()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->actortype < A_ENVIRONMENT || ci->state != CS_ALIVE) continue;
            suicideevent ev;
            ev.flags = HIT_JANITOR;
            ev.process(ci); // process death immediately
        }
    }

    int botrnd(clientinfo *ci, int t, int m, int r = 0)
    {
        if(G(botcolourseed)&t) return ci->colours[r % 2] % m;
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
        if(type >= A_ENEMY) return type >= A_ENVIRONMENT ? MAXAI : G(enemylimit);
        if(m_coop(gamemode, mutators))
        {
            int people = numclients(-1, true, -1), numt = numteams(gamemode, mutators) - 1;
            return min(int(ceilf(people * numt * G(coopbalance))), MAXAI);
        }
        return G(botlimit);
    }

    void getskillrange(clientinfo *ci, int &n, int &m)
    {
        switch(ci->actortype)
        {
            case A_BOT:
            {
                #define BOTSKILL(a) \
                { \
                    m = max(G(a##skillmax), G(a##skillmin)); \
                    n = min(G(a##skillmin), m); \
                    if(ci->deaths != 0 && G(a##skilldeaths) != 0) \
                    { \
                        int amt = int(G(a##skilldeaths) * ci->deaths); \
                        m += amt; \
                        n += amt; \
                    } \
                    if(ci->frags != 0 && G(a##skillfrags) != 0) \
                    { \
                        int amt = int(G(a##skillfrags) * ci->frags); \
                        m += amt; \
                        n += amt; \
                    } \
                }
                if(m_coop(gamemode, mutators)) BOTSKILL(coop)
                else BOTSKILL(bot)
                break;
            }
            default:
                m = max(G(enemyskillmax), G(enemyskillmin));
                n = min(G(enemyskillmin), m);
                break;
        }
        m = clamp(m, 1, 101);
        n = clamp(n, 1, m);
    }

    void setskill(clientinfo *ci, bool init)
    {
        int n = 1, m = 100;
        getskillrange(ci, n, m);
        if(init || ci->skill > m || ci->skill < n)
        { // needs re-skilling
            ci->skill = (m != n ? botrnd(ci, 2, m-n) + n + 1 : m);
            if(!init && !ci->aireinit) ci->aireinit = 1;
        }
    }

    bool addai(int type, int ent)
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
                loopk(2) ci->colours[k] = rnd(0xFFFFFF);
                ci->model = botrnd(ci, 4, PLAYERTYPES);
                setskill(ci, true);
                copystring(ci->name, A(ci->actortype, vname), MAXNAMELEN);
                ci->loadweap.shrink(0);

                if(ci->actortype == A_BOT)
                {
                    vector<char *> list;

                    explodelist(ci->model ? G(botfemalenames) : G(botmalenames), list);

                    while(!list.empty())
                    {
                        int r = botrnd(ci, 1, list.length(), 1);
                        char *name = list[r];
                        loopv(clients) if(clients[i] != ci && !strcasecmp(name, clients[i]->name))
                        {
                            list.remove(r);
                            DELETEA(name);
                            break;
                        }
                        if(name)
                        {
                            copystring(ci->name, name, MAXNAMELEN);
                            if(G(botrandomcase) && rnd(G(botrandomcase)))
                                ci->name[0] = iscubeupper(ci->name[0]) ? cubelower(ci->name[0]) : cubeupper(ci->name[0]);
                            break;
                        }
                    }
                    list.deletearrays();

                    vector<int> weaplist;
                    loopi(W_LOADOUT) weaplist.add(W_OFFSET+i);
                    while(!weaplist.empty())
                    {
                        int iter = botrnd(ci, 8, weaplist.length(), rnd(2));
                        ci->loadweap.add(weaplist[iter]);
                        weaplist.remove(iter);
                    }
                }

                loopj(2)
                {
                    const char *itemlist = NULL;
                    if(ci->actortype == A_JANITOR) itemlist = j ? NULL : G(janitorvanities);
                    else if(ci->actortype == A_BOT)
                    {
                        if(j) itemlist = ci->model ? G(botfemalemixers) : G(botmalemixers);
                        else itemlist = ci->model ? G(botfemalevanities) : G(botmalevanities);
                    }

                    if(itemlist && *itemlist)
                    {
                        vector<char *> list;
                        explodelist(itemlist, list);
                        while(!list.empty())
                        {
                            int r = botrnd(ci, 1, list.length());
                            char *name = list[r];
                            if(!name || !*name)
                            {
                                list.remove(r);
                                DELETEA(name);
                                continue;
                            }
                            if(j) ci->setmixer(name);
                            else ci->setvanity(name);
                            break;
                        }
                        list.deletearrays();
                    }
                }

                ci->state = CS_DEAD;
                ci->team = type == A_BOT ? T_NEUTRAL : (type >= A_ENVIRONMENT ? T_ENVIRONMENT : T_ENEMY);
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

            sendf(-1, 1, "ri6si4ssiv", N_INITAI, ci->clientnum, ci->ownernum, ci->actortype, ci->spawnpoint, ci->skill, ci->name, ci->team, ci->colours[0], ci->colours[1], ci->model, ci->vanity, ci->mixer, ci->loadweap.length(), ci->loadweap.length(), ci->loadweap.getbuf());

            if(ci->aireinit == 2)
            {
                waiting(ci, DROP_RESET);
                if(smode) smode->entergame(ci);
                mutate(smuts, mut->entergame(ci));
            }
            else if(ci->state == CS_SPECTATOR) spectate(ci, false);

            ci->aireinit = 0;
        }
        else if(ci->state == CS_SPECTATOR) spectate(ci, false);
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
            if(ci->team == T_ENEMY && ++numenemies >= elimit) { shiftai(ci, NULL); continue; }
            setskill(ci);
        }

        int people = numclients(-1, true, -1), balance = people, numt = numteams(gamemode, mutators);
        if(m_coop(gamemode, mutators))
        {
            numt--; // filter out the human team
            balance += int(ceilf(people *numt * G(coopbalance)));
            balance += G(botoffset) * numt;
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
            balance += G(botoffset) * numt;

            if(!m_duke(gamemode, mutators) && G(botbalancescale) != 1) balance = int(ceilf(balance * G(botbalancescale)));

            if(balance > 0 && m_team(gamemode, mutators))
            { // skew this if teams are unbalanced
                int plrs[T_NUM] = {0}, highest = -1, bots = 0, offset = balance%numt; // we do this because humans can unbalance in odd ways
                if(offset) balance += numt-offset;

                loopv(clients) if(isteam(gamemode, mutators, clients[i]->team, T_FIRST))
                {
                    if(clients[i]->actortype == A_BOT)
                    {
                        bots++;
                        continue;
                    }
                    int team = clients[i]->team - T_FIRST;
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
        else balance += G(botoffset) * numt;

        int bots = balance - people;
        if(bots > blimit) balance -= bots - blimit;
        if(numt > 1 && (balance%numt) != 0) balance -= balance % numt;

        if(balance > 0)
        {
            while(numclients(-1, true, A_BOT) < balance) if(!addai(A_BOT)) break;
            while(numclients(-1, true, A_BOT) > balance) if(!delai(A_BOT)) break;
            loopvrev(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->actortype == A_BOT && ci->ownernum >= 0)
                {
                    int teamb = chooseteam(ci, ci->team);
                    if(ci->team != teamb) setteam(ci, teamb, TT_RESETX);
                }
            }
            curbalance = balance;
        }
        else clearai(1);
    }

    void checkjanitors()
    {
        int numjanitors = 0, maxjanitors = min(G(janitorlimit), int(ceilf(curbalance * G(janitorbalance))));
        loopvrev(clients) if(clients[i]->actortype == A_JANITOR) numjanitors++;

        if(numjanitors == maxjanitors) return;

        if(numjanitors > maxjanitors)
        {
            loopvrev(clients) if(clients[i]->actortype == A_JANITOR)
            {
                deleteai(clients[i]);
                if(--numjanitors <= maxjanitors) break;
            }

            return;
        }

        vector<int> candidates[2];
        int count = 0;
        loopvj(sents) if(sents[j].type == ACTOR)
        {
            if(!servermapvariant(sents[j].attrs[enttype[sents[j].type].mvattr])) continue;
            if(sents[j].attrs[0] < 0 || sents[j].attrs[0] >= A_TOTAL || gamemillis < sents[j].millis) continue;

            int atype = sents[j].attrs[0] + A_ENEMY;
            if(atype != A_JANITOR) continue;

            if(sents[j].attrs[5] && sents[j].attrs[5] != triggerid) continue;
            if(!m_check(sents[j].attrs[3], sents[j].attrs[4], gamemode, mutators)) continue;

            bool found = false;
            loopvrev(clients) if(clients[i]->actortype == A_JANITOR && clients[i]->spawnpoint == j) { found = true; break; }
            candidates[found ? 1 : 0].add(j);
            count++;
        }

        int amt = max(maxjanitors - numjanitors, 0);
        loopk(amt)
        {
            if(!count)
            {
                addai(A_JANITOR); // with no spawnpoint
                continue;
            }

            loopj(2)
            {
                if(candidates[j].empty()) continue;
                int r = rnd(candidates[j].length()), q = candidates[j][r];

                addai(A_JANITOR, q);
                sents[q].millis = gamemillis;

                candidates[j].remove(r);
            }
        }
    }

    void checkenemies()
    {
        loopvj(sents) if(sents[j].type == ACTOR)
        {
            if(!servermapvariant(sents[j].attrs[enttype[sents[j].type].mvattr])) continue;
            if(sents[j].attrs[0] < 0 || sents[j].attrs[0] >= A_TOTAL || gamemillis < sents[j].millis) continue;

            int atype = sents[j].attrs[0] + A_ENEMY;
            if(atype == A_JANITOR || (!m_onslaught(gamemode, mutators) && atype < A_ENVIRONMENT)) continue;
            if(atype == A_TURRET && m_insta(gamemode, mutators)) continue;

            if(sents[j].attrs[5] && sents[j].attrs[5] != triggerid) continue;
            if(!m_check(sents[j].attrs[3], sents[j].attrs[4], gamemode, mutators)) continue;

            if(atype >= A_ENVIRONMENT)
            {
                bool found = false;
                loopvrev(clients) if(clients[i]->actortype >= A_ENVIRONMENT && clients[i]->spawnpoint == j)
                {
                    if(found) deleteai(clients[i]);
                    found = true;
                }

                if(!found)
                {
                    addai(atype, j);
                    sents[j].millis = gamemillis;
                }

                continue;
            }

            int count = 0, numenemies = 0;
            loopvrev(clients) if(clients[i]->team == T_ENEMY)
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
                int amt = min(G(enemybalance) - count, G(enemylimit) - numenemies);
                loopk(amt) addai(atype, j);
                sents[j].millis = gamemillis + G(enemyspawndelay);
            }
        }
    }

    void clearai(int type)
    { // clear and remove all ai immediately
        curbalance = lastcheck = 0;
        oldcancheck = false;

        loopvrev(clients) if(!type || (type == 2 ? clients[i]->actortype >= A_ENEMY : clients[i]->actortype == A_BOT))
            deleteai(clients[i]);
    }

    void checkai()
    {
        if(demoplayback || m_demo(gamemode) || !numclients())
        {
            clearai();
            return;
        }

        bool cancheck = canplay();
        
        if(cancheck)
        {
            if(cancheck != oldcancheck || !lastcheck || (totalmillis - lastcheck >= G(airefreshdelay)))
            {
                if(canbalancenow()) checksetup();
                checkenemies();
                checkjanitors();
                lastcheck = totalmillis;
            }

            loopvrev(clients) if(clients[i]->actortype > A_PLAYER) reinitai(clients[i]);
            while(true) if(!reassignai()) break;
        }

        oldcancheck = cancheck;
    }
}
