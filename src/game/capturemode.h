struct captureservmode : capturestate, servmode
{
    bool hasflaginfo;

    captureservmode() : hasflaginfo(false) {}

    void reset()
    {
        capturestate::reset();
        hasflaginfo = false;
    }

    void dropaffinity(clientinfo *ci, const vec &o, const vec &inertia = vec(0, 0, 0), int offset = -1)
    {
        if(!canplay() || !hasflaginfo || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        int numflags = 0, iterflags = 0;
        loopv(flags) if(flags[i].owner == ci->clientnum) numflags++;
        vec dir = inertia, olddir = dir;
        if(numflags > 1 && dir.iszero())
        {
            dir.x = -sinf(RAD*ci->yaw);
            dir.y = cosf(RAD*ci->yaw);
            dir.z = 0;
            olddir = dir.normalize().mul(max(dir.magnitude(), 1.f));
        }
        loopv(flags) if(flags[i].owner == ci->clientnum)
        {
            if(numflags > 1)
            {
                float yaw = -45.f+(90/float(numflags+1)*(iterflags+1));
                dir = vec(olddir).rotate_around_z(yaw*RAD);
                iterflags++;
            }
            ivec p(vec(o).mul(DMF)), q(vec(dir).mul(DMF));
            capturestate::dropaffinity(i, o, dir, gamemillis, offset);
            sendf(-1, 1, "ri3i7", N_DROPAFFIN, ci->clientnum, flags[i].dropoffset, i, p.x, p.y, p.z, q.x, q.y, q.z);
        }
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(!canplay() || !hasflaginfo) return;
        dropaffinity(ci, ci->feetpos(G(capturedropheight)), vec(ci->vel).add(ci->falling));
    }

    void dodamage(clientinfo *m, clientinfo *v, int &damage, int &hurt, int &weap, int &flags, int &material, const ivec &hitpush, const ivec &hitvel, float dist)
    {
    }

    void died(clientinfo *ci, clientinfo *v)
    {
        if(!canplay() || !hasflaginfo) return;
        dropaffinity(ci, ci->feetpos(G(capturedropheight)), vec(ci->vel).add(ci->falling));
    }

    int addscore(int team, int points = 1)
    {
        score &cs = teamscore(team);
        cs.total += points;
        return cs.total;
    }

    void moved(clientinfo *ci, const vec &oldpos, const vec &newpos)
    {
        if(!canplay() || !hasflaginfo || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY)) || ci->state != CS_ALIVE) return;
        if(ci->floorpos != vec(-1, -1, -1))
            loopv(flags) if(flags[i].owner == ci->clientnum)
                (flags[i].floorpos = ci->floorpos).z += (enttype[AFFINITY].radius/4)+1;
        if(G(capturethreshold) > 0 && oldpos.dist(newpos) >= G(capturethreshold))
            dropaffinity(ci, oldpos, vec(ci->vel).add(ci->falling));
        if(!m_ctf_protect(gamemode, mutators)) loopv(flags) if(flags[i].owner == ci->clientnum)
        {
            flag &r = flags[i]; // held flag
            loopvk(flags)
            {
                flag &f = flags[k]; // goal flag
                if(f.team == ci->team && (f.owner < 0 || (f.owner == ci->clientnum && (i != k || gamemillis-f.taketime >= G(capturepickupdelay)))) && !f.droptime && newpos.dist(f.spawnloc) <= enttype[AFFINITY].radius*2/3)
                {
                    capturestate::returnaffinity(i, gamemillis);
                    if(r.team != ci->team)
                    {
                        capturestats cstats;
                        cstats.capturing = ci->team;
                        cstats.captured = r.team;
                        ci->captures.add(cstats);
                        int score = addscore(ci->team);
                        sendf(-1, 1, "ri5", N_SCOREAFFIN, ci->clientnum, i, k, score);
                        mutate(smuts, mut->scoreaffinity(ci));
                        givepoints(ci, G(capturepoints), m_points(gamemode, mutators), false);
                    }
                    else sendf(-1, 1, "ri3", N_RETURNAFFIN, ci->clientnum, i);
                }
            }
        }
    }

    void takeaffinity(clientinfo *ci, int i)
    {
        if(!canplay() || !hasflaginfo || !flags.inrange(i) || ci->state != CS_ALIVE || !ci->team || !(AA(ci->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        flag &f = flags[i];
        if(f.owner >= 0 || (f.team == ci->team && (m_ctf_defend(gamemode, mutators) || (m_ctf_quick(gamemode, mutators) && !f.droptime)))) return;
        if(f.lastowner == ci->clientnum && f.droptime && gamemillis-f.droptime <= G(capturepickupdelay)) return;
        if(!m_ctf_protect(gamemode, mutators) && f.team != ci->team && f.lastowner == ci->clientnum && f.returntime + G(captureretakedelay) >= gamemillis) return;
        if(m_ctf_quick(gamemode, mutators) && f.team == ci->team)
        {
            capturestate::returnaffinity(i, gamemillis);
            givepoints(ci, G(capturepoints), m_points(gamemode, mutators), false);
            sendf(-1, 1, "ri3", N_RETURNAFFIN, ci->clientnum, i);
        }
        else
        {
            capturestate::takeaffinity(i, ci->clientnum, gamemillis);
            if(ci->floorpos != vec(-1, -1, -1)) f.floorpos = ci->floorpos;
            else f.floorpos = f.droploc;
            f.floorpos.z += (enttype[AFFINITY].radius/4)+1;
            if((f.team != ci->team && !f.droptime) || f.lastowner != ci->clientnum) givepoints(ci, G(capturepickuppoints), m_points(gamemode, mutators), false);
            sendf(-1, 1, "ri3", N_TAKEAFFIN, ci->clientnum, i);
        }
    }

    void resetaffinity(clientinfo *ci, int i)
    {
        if(!canplay() || !hasflaginfo || !flags.inrange(i) || ci->ownernum >= 0) return;
        flag &f = flags[i];
        if(f.owner >= 0 || !f.droptime || f.votes.find(ci->clientnum) >= 0) return;
        f.votes.add(ci->clientnum);
        if(f.votes.length() >= int(floorf(numclients()*0.5f)))
        {
            if(G(captureresetfloor))
            {
                if(f.floorpos != vec(-1, -1, -1))
                {
                    ivec pos = ivec(vec(f.floorpos).mul(DMF));
                    capturestate::dropaffinity(i, f.floorpos, vec(0, 0, 1), gamemillis);
                    f.floorpos = vec(-1, -1, -1);
                    sendf(-1, 1, "ri3i3", N_RESETAFFIN, i, 2, pos.x, pos.y, pos.z);
                }
            }
            else
            {
                capturestate::returnaffinity(i, gamemillis);
                sendf(-1, 1, "ri3", N_RESETAFFIN, i, 1);
            }
        }
    }

    void layout()
    {
        if(!canplay() || !hasflaginfo) return;
        loopv(flags) if(flags[i].owner >= 0 || flags[i].droptime)
        {
            capturestate::returnaffinity(i, gamemillis);
            sendf(-1, 1, "ri3", N_RESETAFFIN, i, 1);
        }
    }

    void update()
    {
        if(!canplay() || !hasflaginfo) return;
        loopv(flags)
        {
            flag &f = flags[i];
            if(m_ctf_protect(gamemode, mutators) && f.owner >= 0)
            {
                clientinfo *ci = (clientinfo *)getinfo(f.owner);
                if(f.team != ci->team && f.taketime && gamemillis-f.taketime >= G(captureprotectdelay))
                {
                    capturestate::returnaffinity(i, gamemillis);
                    givepoints(ci, G(capturepoints), m_points(gamemode, mutators), false);
                    int score = addscore(ci->team);
                    sendf(-1, 1, "ri5", N_SCOREAFFIN, ci->clientnum, i, -1, score);
                    mutate(smuts, mut->scoreaffinity(ci));
                }
            }
            else if(f.owner < 0 && f.droptime && f.dropleft(gamemillis, capturestore) >= capturedelay)
            {
                capturestate::returnaffinity(i, gamemillis);
                sendf(-1, 1, "ri3", N_RESETAFFIN, i, 1);
            }
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
            putint(p, f.team);
            putint(p, f.yaw);
            putint(p, f.pitch);
            putint(p, f.owner);
            loopj(3) putint(p, int(f.spawnloc[j]*DMF));
            if(f.owner<0)
            {
                putint(p, f.droptime);
                if(f.droptime)
                {
                    putint(p, f.dropoffset);
                    loopj(3) putint(p, int(f.droploc[j]*DMF));
                    loopj(3) putint(p, int(f.inertia[j]*DMF));
                }
            }
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

    void regen(clientinfo *ci, int &total, int &amt, int &delay)
    {
        if(!canplay() || !hasflaginfo || !G(captureregenbuff) || !ci->lastbuff) return;
        if(G(maxhealth)) total = max(m_maxhealth(gamemode, mutators, ci->actortype), total);
        if(ci->lastregen && G(captureregendelay)) delay = G(captureregendelay);
        if(G(captureregenextra)) amt += G(captureregenextra);
    }

    void checkclient(clientinfo *ci)
    {
        if(!canplay() || !hasflaginfo || ci->state != CS_ALIVE || m_insta(gamemode, mutators)) return;
        bool buff = false;
        if(G(capturebuffing)) loopv(flags)
        {
            flag &f = flags[i];
            clientinfo *owner = f.owner >= 0 ? (clientinfo *)getinfo(f.owner) : NULL;
            if(f.team == ci->team)
            {
                if((G(capturebuffing)&1 || G(capturebuffing)&2) && !owner && (!f.droptime || m_ctf_defend(gamemode, mutators) || G(capturebuffing)&2) && ci->o.dist(f.droptime ? f.droploc : f.spawnloc) <= G(capturebuffarea)) { buff = true; break; }
                if(G(capturebuffing)&4 && owner && ci == owner) { buff = true; break; }
                if(G(capturebuffing)&8 && owner && ci != owner && owner->team == ci->team && (G(capturebuffarea) > 0 ? ci->o.dist(owner->o) <= G(capturebuffarea) : true)) { buff = true; break; }
            }
            else
            {
                if(G(capturebuffing)&16 && ci == owner) { buff = true; break; }
                if(G(capturebuffing)&32 && owner && ci != owner && owner->team == ci->team && (G(capturebuffarea) > 0 ? ci->o.dist(owner->o) <= G(capturebuffarea) : true)) { buff = true; break; }
            }
        }
        if(buff)
        {
            if(!ci->lastbuff) sendf(-1, 1, "ri4", N_SPHY, ci->clientnum, SPHY_BUFF, 1);
            ci->lastbuff = gamemillis;
        }
        else if(ci->lastbuff && (!G(capturebuffing) || gamemillis-ci->lastbuff >= G(capturebuffdelay)))
        {
            ci->lastbuff = 0;
            sendf(-1, 1, "ri4", N_SPHY, ci->clientnum, SPHY_BUFF, 0);
        }
    }

    void moveaffinity(clientinfo *ci, int cn, int id, const vec &o, const vec &inertia = vec(0, 0, 0))
    {
        if(!canplay() || !hasflaginfo || !flags.inrange(id)) return;
        flag &f = flags[id];
        if(!f.droptime || f.owner >= 0 || f.lastowner != ci->clientnum) return;
        f.droploc = o;
        f.inertia = inertia;
        //sendf(-1, 1, "ri9", N_MOVEAFFIN, ci->clientnum, id, int(f.droploc.x*DMF), int(f.droploc.y*DMF), int(f.droploc.z*DMF), int(f.inertia.x*DMF), int(f.inertia.y*DMF), int(f.inertia.z*DMF));
    }

    void parseaffinity(ucharbuf &p)
    {
        int numflags = getint(p);
        if(numflags)
        {
            loopi(numflags)
            {
                int team = getint(p), yaw = getint(p), pitch = getint(p);
                vec o;
                loopj(3) o[j] = getint(p)/DMF;
                if(p.overread()) break;
                if(!hasflaginfo && i < MAXPARAMS) addaffinity(o, team, yaw, pitch);
            }
            if(!hasflaginfo)
            {
                hasflaginfo = true;
                sendaffinity();
                loopv(clients) if(clients[i]->state == CS_ALIVE) entergame(clients[i]);
            }
        }
    }

    int points(clientinfo *m, clientinfo *v)
    {
        bool isteam = m==v || m->team == v->team;
        int p = isteam ? -1 : 1, q = p;
        loopv(flags) if(flags[i].owner == m->clientnum) p += q;
        return p;
    }
} capturemode;
