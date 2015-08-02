// WARNING: Before modifying this file, please read our Guidelines
// This file can be found in the distribution under: ./docs/guidelines.txt
// Or at: http://redeclipse.net/wiki/Multiplayer_Guidelines
//
// The Red Eclipse Team provides the play.redeclipse.net master server for the
// benefit of the Red Eclipse Community. We impose a general set of guidelines
// for any server/user which connects to the play.redeclipse.net master server.
// The team reserves the right to block any attempt to connect to the master
// server at their discretion. Access to services provided by the project are
// considered to be a privilege, not a right.

// These guidelines are imposed to ensure the integrity of both the Red Eclipse
// game, and its community. If you do not agree to these terms, you should not
// connect to the play.redeclipse.net master server, or any servers which are
// connected to it. These guidelines are not designed to limit your opinion or
// freedoms granted to you by the open source licenses employed by the project,
// nor do they cover usage of the game in either offline play or on servers
// which are not connected to the Red Eclipse master.

// If you have questions or comments regarding these guidelines please contact
// the Red Eclipse Team. Any person seeking to modify their game or server for
// use on the master server should first seek permission from the Red Eclipse
// Team, each modification must be approved and will be done on a case-by-case
// basis.

#define GAMESERVER 1
#include "game.h"
#include "errno.h"

namespace server
{
    struct srventity
    {
        vec o;
        int type;
        bool spawned;
        int millis, last;
        attrvector attrs, kin;

        srventity() :  o(0, 0, 0), type(NOTUSED), spawned(false), millis(0), last(0) { reset(); }
        ~srventity() { reset(); }

        void reset()
        {
            o = vec(0, 0, 0);
            attrs.shrink(0);
            kin.shrink(0);
        }
    };

    static const int DEATHMILLIS = 300;

    struct clientinfo;

    struct gameevent
    {
        virtual ~gameevent() {}
        virtual bool flush(clientinfo *ci, int fmillis);
        virtual void process(clientinfo *ci) {}
        virtual bool keepable() const { return false; }
    };

    struct timedevent : gameevent
    {
        int millis;
        bool flush(clientinfo *ci, int fmillis);
    };

    struct shotevent : timedevent
    {
        int id, weap, flags, scale, num;
        ivec from;
        vector<shotmsg> shots;
        void process(clientinfo *ci);
    };

    struct switchevent : timedevent
    {
        int id, weap;
        void process(clientinfo *ci);
    };

    struct dropevent : timedevent
    {
        int id, weap;
        void process(clientinfo *ci);
    };

    struct reloadevent : timedevent
    {
        int id, weap;
        void process(clientinfo *ci);
    };

    struct hitset
    {
        int flags, proj, target;
        union
        {
            int rays;
            int dist;
        };
        ivec dir, vel;
    };

    struct destroyevent : timedevent
    {
        int id, weap, flags, radial, scale;
        vector<hitset> hits;
        bool keepable() const { return true; }
        void process(clientinfo *ci);
    };

    struct suicideevent : gameevent
    {
        int flags, material;
        void process(clientinfo *ci);
    };

    struct useevent : timedevent
    {
        int id, ent;
        void process(clientinfo *ci);
    };

    struct stickyevent : timedevent
    {
        int id, weap, flags, target;
        ivec norm, pos;
        bool keepable() const { return true; }
        void process(clientinfo *ci);
    };

    struct projectile
    {
        int id, ammo;
        projectile(int n, int a) : id(n), ammo(a) {}
        ~projectile() {}
    };
    struct projectilestate
    {
        vector<projectile> projs;
        projectilestate() { reset(); }
        void reset() { projs.shrink(0); }
        void add(int id, int ammo = -1)
        {
            projs.add(projectile(id, ammo));
        }
        bool remove(int id)
        {
            loopv(projs) if(projs[i].id==id)
            {
                projs.remove(i);
                return true;
            }
            return false;
        }
        int removeall(int id)
        {
            int count = 0;
            loopvrev(projs) if(projs[i].id==id)
            {
                projs.remove(i);
                count++;
            }
            return count;
        }
        bool find(int id)
        {
            loopv(projs) if(projs[i].id==id) return true;
            return false;
        }
        void values(int id, int &a)
        {
            loopv(projs) if(projs[i].id==id) { a = projs[i].ammo; return; }
            a = -1;
        }
    };

    struct dmghist
    {
        int clientnum, millis;
        dmghist() {}
        dmghist(int c, int m) : clientnum(c), millis(m) {}
        ~dmghist() {}
    };

    struct teamkill
    {
        int millis, team, points;
        teamkill() {}
        teamkill(int m, int t, int p) : millis(m), team(t), points(p) {}
        ~teamkill() {}
    };

    struct weaponstats
    {
        int timewielded, timeloadout;
        int hits1, hits2, flakhits1, flakhits2;
        int shots1, shots2, flakshots1, flakshots2;
        int frags1, frags2, damage1, damage2;

        weaponstats() { reset(); }
        ~weaponstats() {}

        void reset()
        {
            timewielded = timeloadout = 0;
            hits1 = hits2 = flakhits1 = flakhits2 = 0;
            shots1 = shots2 = flakshots1 = flakshots2 = 0;
            frags1 = frags2 = damage1 = damage2 = 0;
        }
    };

    extern int gamemode, mutators;

    enum { WARN_CHAT = 0, WARN_TEAMKILL, WARN_MAX };

    struct servstate : clientstate
    {
        vec o, vel, falling;
        float yaw, pitch, roll;
        int state, score, spree, rewards[2], shotdamage, damage;
        int lasttimewielded, lasttimeloadout[W_MAX];
        int lasttimeplayed, timeplayed, aireinit, lastboost, lastresowner[WR_MAX], lasttimealive, timealive, lastresweapon[WR_MAX];
        bool lastresalt[W_MAX];
        projectilestate dropped, weapshots[W_MAX][2];
        vector<int> fraglog, fragmillis, cpnodes, chatmillis;
        vector<dmghist> damagelog;
        vector<teamkill> teamkills;
        weaponstats weapstats[W_MAX];
        int warnings[WARN_MAX][2];

        servstate() : state(CS_SPECTATOR), aireinit(0)
        {
            loopi(WARN_MAX) loopj(2) warnings[i][j] = 0;
            resetresidualowner();
        }

        bool isalive(int millis)
        {
            return state == CS_ALIVE || ((state == CS_DEAD || state == CS_WAITING) && lastdeath && millis-lastdeath <= DEATHMILLIS);
        }

        void reset(bool change = false)
        {
            if(state != CS_SPECTATOR) state = CS_DEAD;
            dropped.reset();
            loopi(W_MAX) loopj(2) weapshots[i][j].reset();
            if(!change) score = timeplayed;
            else clientstate::mapchange();
            frags = spree = rewards[0] = rewards[1] = deaths = shotdamage = damage = timealive = 0;
            fraglog.shrink(0);
            fragmillis.shrink(0);
            cpnodes.shrink(0);
            damagelog.shrink(0);
            teamkills.shrink(0);
            loopi(W_MAX) weapstats[i].reset();
            respawn(0);
        }

        void resetresidualowner(int n = -1)
        {
            if(n >= 0 && n < WR_MAX) lastresowner[n] = -1;
            else loopi(WR_MAX) lastresowner[i] = -1;
        }

        void respawn(int millis)
        {
            lastboost = rewards[1] = 0;
            resetresidualowner();
            clientstate::respawn(millis);
            o = vec(-1e10f, -1e10f, -1e10f);
            vel = falling = vec(0, 0, 0);
            yaw = pitch = roll = 0;
        }

        void updateweaptime()
        {
            extern int gamemillis;
            if(isalive(gamemillis))
            {
                weapstats[weapselect].timewielded += totalmillis-lasttimewielded;
                loopi(W_MAX) if(holdweap(i, m_weapon(gamemode, mutators), lastmillis))
                    weapstats[i].timeloadout += totalmillis-lasttimeloadout[i];
            }
            lasttimewielded = totalmillis;
            loopi(W_MAX) lasttimeloadout[i] = totalmillis;
        }

        void updatetimeplayed(bool last = true)
        {
            timeplayed += totalmillis-lasttimeplayed;
            if(last) lasttimeplayed = totalmillis;
            extern int gamemillis;
            if(isalive(gamemillis))
                timealive += totalmillis-lasttimealive;
            lasttimealive = totalmillis;
            updateweaptime();
        }

        float scoretime(bool update = true)
        {
            if(update) updatetimeplayed();
            return score/float(max(timeplayed, 1));
        }

        vec feetpos(float offset = 0) const { return vec(o).add(vec(0, 0, offset)); }
        vec headpos(float offset = 0) const { return vec(o).add(vec(0, 0, offset+actor[actortype].height)); }
        vec center() const { return vec(o).add(vec(0, 0, actor[actortype].height*0.5f)); }
    };

    struct savedscore
    {
        uint ip;
        string name, handle;
        int points, score, frags, spree, rewards, timeplayed, timealive, deaths, shotdamage, damage, cptime, actortype;
        int warnings[WARN_MAX][2];
        vector<teamkill> teamkills;
        weaponstats weapstats[W_MAX];
        bool quarantine;

        void save(servstate &gs)
        {
            points = gs.points;
            score = gs.score;
            frags = gs.frags;
            spree = gs.spree;
            rewards = gs.rewards[0];
            timeplayed = gs.timeplayed;
            timealive = gs.timealive;
            deaths = gs.deaths;
            teamkills = gs.teamkills;
            shotdamage = gs.shotdamage;
            damage = gs.damage;
            cptime = gs.cptime;
            actortype = gs.actortype;
            loopi(W_MAX) weapstats[i] = gs.weapstats[i];
            loopi(WARN_MAX) loopj(2) warnings[i][j] = gs.warnings[i][j];
            quarantine = gs.quarantine;
        }

        void restore(servstate &gs)
        {
            gs.points = points;
            gs.score = score;
            gs.frags = frags;
            gs.spree = spree;
            gs.rewards[0] = rewards;
            gs.timeplayed = timeplayed;
            gs.timealive = timealive;
            gs.deaths = deaths;
            gs.teamkills = teamkills;
            gs.shotdamage = shotdamage;
            gs.damage = damage;
            gs.cptime = cptime;
            loopi(W_MAX) gs.weapstats[i] = weapstats[i];
            loopi(WARN_MAX) loopj(2) gs.warnings[i][j] = warnings[i][j];
            gs.quarantine = quarantine;
        }

        void mapchange()
        {
            points = frags = spree = rewards = deaths = shotdamage = damage = cptime = 0;
            actortype = A_MAX;
            teamkills.shrink(0);
        }
    };

    struct votecount
    {
        char *map;
        int mode, muts, count;
        votecount() {}
        votecount(char *s, int n, int m) : map(s), mode(n), muts(m), count(0) {}
    };

    struct clientinfo
    {
        servstate state;
        string name, handle, mapvote, authname, clientmap;
        int clientnum, connectmillis, sessionid, overflow, ping, team, lastteam, lastplayerinfo,
            modevote, mutsvote, lastvote, privilege, gameoffset, lastevent, wslen, swapteam;
        bool connected, ready, local, timesync, online, wantsmap, gettingmap, connectauth, kicked;
        uint mapcrc;
        vector<gameevent *> events;
        vector<uchar> position, messages;
        uchar *wsdata;
        vector<clientinfo *> bots;
        uint authreq;
        ENetPacket *clipboard;
        int lastclipboard, needclipboard;

        clientinfo() : clipboard(NULL) { reset(); }
        ~clientinfo() { events.deletecontents(); cleanclipboard(); }

        void addevent(gameevent *e)
        {
            if(state.state==CS_SPECTATOR || events.length()>250) delete e;
            else events.add(e);
        }

        void mapchange(bool change = true)
        {
            mapvote[0] = 0;
            state.reset(change);
            events.deletecontents();
            overflow = 0;
            ready = timesync = wantsmap = gettingmap = false;
            lastevent = gameoffset = lastvote = 0;
            if(!change) lastteam = T_NEUTRAL;
            team = swapteam = T_NEUTRAL;
            clientmap[0] = '\0';
            mapcrc = 0;
        }

        void cleanclipboard(bool fullclean = true)
        {
            if(clipboard) { if(--clipboard->referenceCount <= 0) enet_packet_destroy(clipboard); clipboard = NULL; }
            if(fullclean) lastclipboard = 0;
        }

        void reset()
        {
            ping = lastplayerinfo = 0;
            name[0] = handle[0] = 0;
            privilege = PRIV_NONE;
            connected = ready = local = online = wantsmap = gettingmap = connectauth = kicked = false;
            authreq = 0;
            position.setsize(0);
            messages.setsize(0);
            needclipboard = 0;
            cleanclipboard();
            mapchange(false);
        }

        int getmillis(int millis, int id)
        {
            if(!timesync)
            {
                timesync = true;
                gameoffset = millis-id;
                return millis;
            }
            return gameoffset+id;
        }

        bool isready()
        {
            return ready && !wantsmap;
        }
    };

    namespace aiman {
        extern void setskill(clientinfo *ci);
        extern bool addai(int type, int ent = -1, int skill = -1);
        extern void deleteai(clientinfo *ci);
        extern bool delai(int type, bool skip = true);
        extern void removeai(clientinfo *ci, bool complete = false);
        extern bool reassignai(clientinfo *exclude = NULL);
        extern void checkskills();
        extern void clearai(int type = 0);
        extern void checkai();
        extern void poke();
    }

    string smapname;
    int gamestate = G_S_WAITING, gamemode = G_EDITMODE, mutators = 0, gamemillis = 0, gamelimit = 0, mastermode = MM_OPEN;
    int timeremaining = -1, oldtimelimit = -1, gamewaittime = 0, gamewaitstate = 0, gamewaitstart = 0, lastteambalance = 0, nextteambalance = 0, lastrotatecycle = 0, mapsending = -1;
    bool hasgameinfo = false, updatecontrols = false, shouldcheckvotes = false, firstblood = false;
    enet_uint32 lastsend = 0;
    stream *mapdata[SENDMAP_MAX] = { NULL };
    uint mapcrc = 0;
    vector<clientinfo *> clients, connects;

    struct demofile
    {
        string info;
        uchar *data;
        int ctime, len;
    };

    vector<demofile> demos;

    bool demonextmatch = false;
    stream *demotmp = NULL, *demorecord = NULL, *demoplayback = NULL;
    int nextplayback = 0, triggerid = 0;
    struct triggergrp
    {
        int id;
        vector<int> ents;
        triggergrp() { reset(); }
        void reset(int n = 0) { id = n; ents.shrink(0); }
    } triggers[TRIGGERIDS+1];

    bool canplay(bool chk = true)
    {
        if(!demoplayback && !m_demo(gamemode))
            if(!chk || (m_play(gamemode) && !hasgameinfo) || !gs_playing(gamestate)) return false;
        return true;
    }

    struct servmode
    {
        servmode() {}
        virtual ~servmode() {}

        virtual void entergame(clientinfo *ci) {}
        virtual void leavegame(clientinfo *ci, bool disconnecting = false) {}

        virtual void moved(clientinfo *ci, const vec &oldpos, const vec &newpos) {}
        virtual bool canspawn(clientinfo *ci, bool tryspawn = false) { return true; }
        virtual void spawned(clientinfo *ci) {}
        virtual int points(clientinfo *m, clientinfo *v)
        {
            if(m==v || m->team == v->team) return -1;
            return 1;
        }
        virtual void died(clientinfo *m, clientinfo *v = NULL) {}
        virtual void changeteam(clientinfo *ci, int oldteam, int newteam) {}
        virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting) {}
        virtual void update() {}
        virtual void reset() {}
        virtual void layout() {}
        virtual void balance(int oldbalance) {}
        virtual void intermission() {}
        virtual bool wantsovertime() { return false; }
        virtual bool damage(clientinfo *m, clientinfo *v, int damage, int weap, int flags, int material, const ivec &hitpush = ivec(0, 0, 0), const ivec &hitvel = ivec(0, 0, 0), float dist = 0) { return true; }
        virtual void dodamage(clientinfo *m, clientinfo *v, int &damage, int &hurt, int &weap, int &flags, int &material, const ivec &hitpush = ivec(0, 0, 0), const ivec &hitvel = ivec(0, 0, 0), float dist = 0) { }
        virtual void regen(clientinfo *ci, int &total, int &amt, int &delay) {}
        virtual void checkclient(clientinfo *ci) {}
        virtual void scoreaffinity(clientinfo *ci, bool win = true) {}
        virtual bool canbalance() { return true; }
    };

    vector<srventity> sents;
    vector<savedscore> savedscores;
    servmode *smode;
    vector<servmode *> smuts;
    #define mutate(a,b) { loopvk(a) { servmode *mut = a[k]; { b; } } }
    int curbalance = 0, nextbalance = 0, totalspawns = 0;
    bool teamspawns = false;

    vector<score> scores;
    score &teamscore(int team)
    {
        loopv(scores)
        {
            score &cs = scores[i];
            if(cs.team == team) return cs;
        }
        score &cs = scores.add();
        cs.team = team;
        cs.total = 0;
        return cs;
    }

    bool chkloadweap(clientinfo *ci, bool request = true)
    {
        if(ci->state.actortype == A_PLAYER && ci->state.loadweap.empty())
        {
            if(request)
            {
                ci->lastplayerinfo = 0;
                sendf(ci->clientnum, 1, "ri", N_LOADW);
            }
            return false;
        }
        return true;
    }

    void setspawn(int ent, bool spawned, bool clear = false, bool msg = false)
    {
        if(sents.inrange(ent))
        {
            if(clear) loopvk(clients) clients[k]->state.dropped.removeall(ent);
            sents[ent].spawned = spawned;
            sents[ent].millis = sents[ent].last = gamemillis;
            if(sents[ent].type == WEAPON && !(sents[ent].attrs[1]&W_F_FORCED))
                sents[ent].millis += w_spawn(w_attr(gamemode, mutators, sents[ent].type, sents[ent].attrs[0], m_weapon(gamemode, mutators)));
            else sents[ent].millis += G(itemspawntime);
            if(msg) sendf(-1, 1, "ri3", N_ITEMSPAWN, ent, sents[ent].spawned ? 1 : 0);
        }
    }

    void takeammo(clientinfo *ci, int weap, int amt = 1) { ci->state.ammo[weap] = max(ci->state.ammo[weap]-amt, 0); }

    struct droplist { int weap, ent, ammo; };
    enum
    {
        DROP_NONE = 0, DROP_WEAPONS = 1<<0, DROP_WCLR = 1<<1, DROP_KAMIKAZE = 1<<2, DROP_EXPLODE = 1<<3,
        DROP_DEATH = DROP_WEAPONS|DROP_KAMIKAZE, DROP_EXPIRE = DROP_WEAPONS|DROP_EXPLODE, DROP_RESET = DROP_WEAPONS|DROP_WCLR
    };

    void dropweapon(clientinfo *ci, int flags, int weap, vector<droplist> &drop)
    {
        if(isweap(weap) && weap != m_weapon(gamemode, mutators) && ci->state.hasweap(weap, m_weapon(gamemode, mutators)) && sents.inrange(ci->state.entid[weap]))
        {
            setspawn(ci->state.entid[weap], false);
            droplist &d = drop.add();
            d.weap = weap;
            d.ent = ci->state.entid[weap];
            d.ammo = ci->state.ammo[weap];
            ci->state.dropped.add(d.ent, d.ammo);
            ci->state.entid[weap] = -1;
            if(flags&DROP_WCLR) ci->state.ammo[weap] = -1;
        }
    }

    bool dropitems(clientinfo *ci, int flags = DROP_RESET)
    {
        bool kamikaze = false;
        vector<droplist> drop;
        if(flags&DROP_EXPLODE || (flags&DROP_KAMIKAZE && G(kamikaze) && (G(kamikaze) > 2 || (ci->state.hasweap(W_GRENADE, m_weapon(gamemode, mutators)) && (G(kamikaze) > 1 || ci->state.weapselect == W_GRENADE)))))
        {
            ci->state.weapshots[W_GRENADE][0].add(1);
            droplist &d = drop.add();
            d.weap = W_GRENADE;
            d.ent = d.ammo = -1;
            if(!(flags&DROP_EXPLODE)) takeammo(ci, W_GRENADE, W2(W_GRENADE, ammosub, false));
            kamikaze = true;
        }
        if(flags&DROP_WEAPONS) loopi(W_MAX) dropweapon(ci, flags, i, drop);
        if(!drop.empty())
            sendf(-1, 1, "ri3iv", N_DROP, ci->clientnum, -1, drop.length(), drop.length()*sizeof(droplist)/sizeof(int), drop.getbuf());
        return kamikaze;
    }

    struct vampireservmode : servmode
    {
        vampireservmode() {}
        void dodamage(clientinfo *m, clientinfo *v, int &damage, int &hurt, int &weap, int &flags, int &material, const ivec &hitpush, const ivec &hitvel, float dist)
        {
            if(v != m && (!m_team(gamemode, mutators) || v->team != m->team) && v->state.state == CS_ALIVE && hurt > 0)
            {
                int real = int(ceilf(hurt*G(vampirescale))), heal = v->state.health+real;
                if(v->state.actortype < A_ENEMY) heal = min(heal, m_maxhealth(gamemode, mutators, v->state.model));
                int eff = heal-v->state.health;
                if(eff > 0)
                {
                    v->state.health = heal;
                    v->state.lastregen = gamemillis;
                    sendf(-1, 1, "ri4", N_REGEN, v->clientnum, v->state.health, eff);
                }
            }
        }
    } vampiremutator;

    extern bool canbalancenow();

    struct spawnservmode : servmode // pseudo-mutator to regulate spawning clients
    {
        vector<clientinfo *> spawnq, playing;

        spawnservmode() {}

        bool spawnqueue(bool all = false, bool needinfo = true)
        {
            return m_play(gamemode) && !m_race(gamemode) && !m_duke(gamemode, mutators) && G(maxalive) > 0 && (!needinfo || canplay()) && (!all || G(maxalivequeue)) && numclients() > 1;
        }

        void queue(clientinfo *ci, bool msg = true, bool wait = true, bool top = false)
        {
            if(spawnqueue(true) && ci->online && ci->state.actortype < A_ENEMY && ci->state.state != CS_SPECTATOR && ci->state.state != CS_EDITING)
            {
                int n = spawnq.find(ci);
                playing.removeobj(ci);
                if(top)
                {
                    if(n >= 0) spawnq.remove(n);
                    spawnq.insert(0, ci);
                }
                else if(n < 0) spawnq.add(ci);
                if(wait && ci->state.state != CS_WAITING) waiting(ci, DROP_RESET);
                if(msg && allowbroadcast(ci->clientnum) && !top)
                {
                    int x = max(int(G(maxalive)*G(maxplayers)), max(int(numclients()*G(maxalivethreshold)), G(maxaliveminimum)));
                    if(m_team(gamemode, mutators))
                    {
                        if(x%2) x++;
                        x = x/2;
                        if(m_coop(gamemode, mutators) && ci->state.actortype == A_BOT)
                            x = int(x*G(coopbalance));
                    }
                    int slots = x;
                    loopv(playing) if(playing[i] && ci->team == playing[i]->team) slots--;
                    if(!slots)
                    {
                        int qn = 0;
                        loopv(spawnq) if(spawnq[i] && spawnq[i]->team == ci->team && spawnq[i]->state.actortype == A_PLAYER)
                        {
                            qn++;
                            if(spawnq[i] == ci)
                            {
                                if(qn > 1) srvmsgft(spawnq[i]->clientnum, CON_EVENT, "\fyyou are \fs\fzcg#%d\fS in the \fs\fgrespawn queue\fS", qn);
                                else srvmsgft(spawnq[i]->clientnum, CON_EVENT, "\fyyou are \fs\fzcrNEXT\fS in the \fs\fgrespawn queue\fS");
                                break;
                            }
                        }
                    }
                }
            }
        }

        void entergame(clientinfo *ci)
        {
            spawnq.removeobj(ci);
            playing.removeobj(ci);
            queue(ci);
        }

        void leavegame(clientinfo *ci, bool disconnecting = false)
        {
            spawnq.removeobj(ci);
            playing.removeobj(ci);
        }

        bool canspawn(clientinfo *ci, bool tryspawn = false)
        {
            if(ci->state.actortype >= A_ENEMY || !m_play(gamemode)) return true;
            else if(tryspawn)
            {
                if(m_loadout(gamemode, mutators) && !chkloadweap(ci)) return false;
                if(spawnqueue(true) && spawnq.find(ci) < 0 && playing.find(ci) < 0) queue(ci);
                return true;
            }
            if(m_balance(gamemode, mutators, teamspawns) && G(balancenospawn) && nextbalance && m_balreset(gamemode, mutators) && canbalancenow()) return false;
            int delay = ci->state.actortype >= A_ENEMY && ci->state.lastdeath ? G(enemyspawntime) : m_delay(gamemode, mutators, ci->team);
            if(delay && ci->state.respawnwait(gamemillis, delay)) return false;
            if(spawnqueue() && playing.find(ci) < 0)
            {
                if(!canplay()) return false;
                if(G(maxalivequeue) && spawnq.find(ci) < 0) queue(ci);
                int x = max(int(G(maxalive)*G(maxplayers)), max(int(numclients()*G(maxalivethreshold)), G(maxaliveminimum)));
                if(m_team(gamemode, mutators))
                {
                    if(x%2) x++;
                    x = x/2;
                    if(m_coop(gamemode, mutators) && ci->state.actortype == A_BOT)
                        x = int(x*G(coopbalance));
                }
                int alive = 0;
                loopv(playing)
                {
                    if(playing[i]->state.state != CS_DEAD && playing[i]->state.state != CS_ALIVE)
                    {
                        if(playing[i]->state.state != CS_WAITING || !G(maxalivequeue))
                        {
                            playing.removeobj(playing[i--]);
                            continue;
                        }
                    }
                    if(spawnq.find(playing[i]) >= 0) spawnq.removeobj(playing[i]);
                    if(ci->team == playing[i]->team) alive++;
                }
                if(alive >= x)
                {
                    if(ci->state.actortype == A_PLAYER) loopv(playing)
                    { // kill off bots for the human
                        if(playing[i]->state.actortype != A_BOT || ci->team != playing[i]->team)
                            continue;
                        queue(playing[i--]);
                        if(--alive < x) break;
                    }
                    if(alive >= x) return false;
                }
                if(G(maxalivequeue))
                {
                    if(ci->state.actortype == A_BOT) loopv(spawnq) if(spawnq[i]->team == ci->team)
                    {
                        if(spawnq[i] != ci && spawnq[i]->state.actortype == A_PLAYER) return false;
                        break;
                    }
                    // at this point is where it decides this player is spawning, so tell everyone else their position
                    if(x-alive == 1)
                    {
                        int qn = 0;
                        loopv(spawnq) if(spawnq[i] != ci && spawnq[i]->team == ci->team && spawnq[i]->state.actortype == A_PLAYER)
                        {
                            qn++;
                            if(allowbroadcast(spawnq[i]->clientnum))
                            {
                                if(qn > 1) srvmsgft(spawnq[i]->clientnum, CON_EVENT, "\fyyou are \fs\fzcg#%d\fS in the \fs\fgrespawn queue\fS", qn);
                                else srvmsgft(spawnq[i]->clientnum, CON_EVENT, "\fyyou are \fs\fzcrNEXT\fS in the \fs\fgrespawn queue\fS");
                            }
                        }
                    }
                }
                spawnq.removeobj(ci);
                if(playing.find(ci) < 0) playing.add(ci);
            }
            return true;
        }

        void spawned(clientinfo *ci)
        {
            spawnq.removeobj(ci);
            if(playing.find(ci) < 0) queue(ci);
        }

        void died(clientinfo *ci, clientinfo *at)
        {
            spawnq.removeobj(ci);
            if(G(maxalivequeue)) playing.removeobj(ci);
        }

        void reset()
        {
            spawnq.shrink(0);
            playing.shrink(0);
        }
    } spawnmutator;

    bool canbalancenow()
    {
        bool ret = true;
        if(smode) if(!smode->canbalance()) ret = false;
        if(ret) mutate(smuts, if(!mut->canbalance()) { ret = false; break; });
        return ret;
    }

    SVAR(0, serverpass, "");
    SVAR(0, adminpass, "");

    int sversion[2] = {0};
    ICOMMAND(0, setversion, "ii", (int *a, int *b), sversion[0] = *a; sversion[1] = *b);

    int mastermask()
    {
        switch(G(serveropen))
        {
            case 0: default: return MM_FREESERV; break;
            case 1: return MM_OPENSERV; break;
            case 2: return MM_COOPSERV; break;
            case 3: return MM_VETOSERV; break;
        }
        return 0;
    }

    #define setmod(a,b) \
    { \
        if(a != b) \
        { \
            ident *id = getident(#a); \
            if(id && id->type == ID_VAR && id->flags&IDF_SERVER) \
            { \
                *id->storage.i = clamp(b, id->minval, id->maxval); \
                id->changed(); \
                const char *sval = intstr(id); \
                sendf(-1, 1, "ri2sis", N_COMMAND, -1, &id->name[3], strlen(sval), sval); \
            } \
        } \
    }
    #define setmodf(a,b) \
    { \
        if(a != b) \
        { \
            ident *id = getident(#a); \
            if(id && id->type == ID_FVAR && id->flags&IDF_SERVER) \
            { \
                *id->storage.f = clamp(b, id->minvalf, id->maxvalf); \
                id->changed(); \
                const char *sval = floatstr(id); \
                if(sval) sendf(-1, 1, "ri2sis", N_COMMAND, -1, &id->name[3], strlen(sval), sval); \
            } \
        } \
    }
    #define setmods(a,b) \
    { \
        if(strcmp(a, b)) \
        { \
            ident *id = getident(#a); \
            if(id && id->type == ID_SVAR && id->flags&IDF_SERVER) \
            { \
                delete[] *id->storage.s; \
                *id->storage.s = newstring(b); \
                sendf(-1, 1, "ri2sis", N_COMMAND, -1, &id->name[3], strlen(*id->storage.s), *id->storage.s); \
            } \
        } \
    }

    int numgamevars = 0, numgamemods = 0;
    void resetgamevars(bool flush, bool all)
    {
        numgamevars = numgamemods = 0;
        enumerate(idents, ident, id, {
            if(id.flags&IDF_SERVER && !(id.flags&IDF_READONLY) && (all || !(id.flags&IDF_WORLD))) // reset vars
            {
                bool check = !(id.flags&IDF_ADMIN) && !(id.flags&IDF_WORLD);
                const char *val = NULL;
                if(check) numgamevars++;
                switch(id.type)
                {
                    case ID_VAR:
                    {
                        setvar(id.name, id.def.i, true);
                        if(check && *id.storage.i != id.bin.i) numgamemods++;
                        if(flush) val = intstr(&id);
                        break;
                    }
                    case ID_FVAR:
                    {
                        setfvar(id.name, id.def.f, true);
                        if(check && *id.storage.f != id.bin.f) numgamemods++;
                        if(flush) val = floatstr(*id.storage.f);
                        break;
                    }
                    case ID_SVAR:
                    {
                        setsvar(id.name, id.def.s && *id.def.s ? id.def.s : "", true);
                        if(check && strcmp(*id.storage.s, id.bin.s)) numgamemods++;
                        if(flush) val = *id.storage.s;
                        break;
                    }
                    default: break;
                }
                if(flush && val) sendf(-1, 1, "ri2sis", N_COMMAND, -1, &id.name[3], strlen(val), val);
            }
        });
    }

    void savegamevars()
    {
        enumerate(idents, ident, id, {
            if(id.flags&IDF_SERVER && !(id.flags&IDF_READONLY) && !(id.flags&IDF_WORLD)) switch(id.type)
            {
                case ID_VAR: id.def.i = *id.storage.i; break;
                case ID_FVAR: id.def.f = *id.storage.f; break;
                case ID_SVAR:
                {
                    delete[] id.def.s;
                    id.def.s = newstring(*id.storage.s);
                    break;
                }
                default: break;
            }
        });
    }

    const char *pickmap(const char *suggest, int mode, int muts, bool notry)
    {
        const char *map = G(defaultmap);
        if(!notry)
        {
            if(!map || !*map) map = choosemap(suggest, mode, muts, G(rotatemaps), true);
            else if(strchr(map, ' '))
            {
                static string defaultmap;
                defaultmap[0] = 0;
                vector<char *> maps;
                explodelist(map, maps);
                if(*sv_previousmaps)
                {
                    vector<char *> prev;
                    explodelist(sv_previousmaps, prev);
                    loopvj(prev) loopvrev(maps) if(strcmp(prev[j], maps[i]))
                    {
                        delete[] maps[i];
                        maps.remove(i);
                        if(maps.length() <= 1) break;
                    }
                    prev.deletearrays();
                }
                if(!maps.empty())
                {
                    int r = rnd(maps.length());
                    copystring(defaultmap, maps[r]);
                }
                maps.deletearrays();
                map = *defaultmap ? defaultmap : choosemap(suggest, mode, muts, G(rotatemaps), true);
            }
        }
        return map && *map ? map : "maps/untitled";
    }

    void setpause(bool on = false)
    {
        if(on) { setmod(sv_gamepaused, 1); }
        else { setmod(sv_gamepaused, 0); }
    }

    void setdemorecord(bool value, bool msg = false)
    {
        demonextmatch = value;
        if(msg) srvoutf(-3, "\fydemo recording is \fs\fc%s\fS for next match", demonextmatch ? "enabled" : "disabled");
    }

    void enddemorecord(bool full);
    void checkdemorecord(bool full)
    {
        if(demorecord) enddemorecord(full);
        if(G(demoautorec) && !demonextmatch) setdemorecord(true);
    }

    void resetbans()
    {
        loopvrev(control) if(control[i].type == ipinfo::BAN && control[i].flag <= ipinfo::INTERNAL) control.remove(i);
    }

    void resetallows()
    {
        loopvrev(control) if(control[i].type == ipinfo::ALLOW && control[i].flag <= ipinfo::INTERNAL) control.remove(i);
    }

    void resetmutes()
    {
        loopvrev(control) if(control[i].type == ipinfo::MUTE && control[i].flag <= ipinfo::INTERNAL) control.remove(i);
    }

    void resetlimits()
    {
        loopvrev(control) if(control[i].type == ipinfo::LIMIT && control[i].flag <= ipinfo::INTERNAL) control.remove(i);
    }

    void resetexcepts()
    {
        loopvrev(control) if(control[i].type == ipinfo::EXCEPT && control[i].flag <= ipinfo::INTERNAL) control.remove(i);
    }

    void cleanup(bool init = false)
    {
        setpause(false);
        setmod(sv_botoffset, 0);
        if(G(resetmmonend)) { mastermode = MM_OPEN; resetallows(); }
        if(G(resetbansonend)) resetbans();
        if(G(resetmutesonend)) resetmutes();
        if(G(resetlimitsonend)) resetlimits();
        if(G(resetexceptsonend)) resetexcepts();
        if(G(resetvarsonend) || init) resetgamevars(true, true);
        changemap();
        lastrotatecycle = clocktime;
    }

    void start()
    {
        cleanup(true);
    }

    void reload()
    {
        extern void localopreset();
        localopreset();
    }

    void shutdown()
    {
        srvoutf(-3, "\fyserver shutdown in progress..");
        aiman::clearai();
        loopv(clients) if(getinfo(i)) disconnect_client(i, DISC_SHUTDOWN);
    }

    void *newinfo() { return new clientinfo; }
    void deleteinfo(void *ci) { delete (clientinfo *)ci; }

    int numchannels() { return 3; }
    int reserveclients() { return G(serverclients)+4; }
    int dupclients() { return G(serverdupclients); }

    bool hasclient(clientinfo *ci, clientinfo *cp = NULL)
    {
        if(!ci || (ci != cp && ci->clientnum != cp->clientnum && ci->state.ownernum != cp->clientnum)) return false;
        return true;
    }

    int peerowner(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(ci && ci->state.actortype > A_PLAYER) return ci->state.ownernum;
        return n;
    }

    bool allowbroadcast(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        return ci && ci->connected && ci->state.actortype == A_PLAYER;
    }

    const char *mastermodename(int type)
    {
        switch(type)
        {
            case MM_OPEN: return "open";
            case MM_VETO: return "veto";
            case MM_LOCKED: return "locked";
            case MM_PRIVATE: return "private";
            case MM_PASSWORD: return "password";
            default: return "unknown";
        }
    }

    int numclients(int exclude, bool nospec, int actortype)
    {
        int n = 0;
        loopv(clients)
        {
            if(clients[i]->clientnum >= 0 && clients[i]->name[0] && clients[i]->clientnum != exclude &&
                (!nospec || clients[i]->state.state != CS_SPECTATOR) &&
                    (clients[i]->state.actortype == A_PLAYER || (actortype > A_PLAYER && clients[i]->state.actortype <= actortype && clients[i]->state.ownernum >= 0)))
                        n++;
        }
        return n;
    }

    bool duplicatename(clientinfo *ci, char *name)
    {
        if(!name) name = ci->name;
        loopv(clients) if(clients[i]!=ci && !strcmp(name, clients[i]->name)) return true;
        return false;
    }

    int findcolour(clientinfo *ci, bool tone = true)
    {
        if(tone)
        {
            int col = ci->state.actortype < A_ENEMY ? ci->state.colour : 0x060606;
            if(!col && isweap(ci->state.weapselect)) col = W(ci->state.weapselect, colour);
            if(col) return col;
        }
        return TEAM(ci->team, colour);
    }

    const char *privname(int priv, int actortype)
    {
        if(actortype != A_PLAYER) return "bot";
        const char *privnames[2][PRIV_MAX] = {
            { "none", "player account", "global supporter", "global moderator", "global operator", "global administrator", "project developer", "project founder" },
            { "none", "player account", "local supporter", "local moderator", "local operator", "local administrator", "none", "none" }
        };
        return privnames[priv&PRIV_LOCAL ? 1 : 0][clamp(priv&PRIV_TYPE, 0, int(priv&PRIV_LOCAL ? PRIV_ADMINISTRATOR : PRIV_LAST))];
    }

    const char *privnamex(int priv, int actortype, bool local)
    {
        if(actortype != A_PLAYER) return "bot";
        const char *privnames[2][PRIV_MAX] = {
            { "none", "player", "supporter", "moderator", "operator", "administrator", "developer", "founder" },
            { "none", "player", "localsupporter", "localmoderator", "localoperator", "localadministrator", "developer", "founder" }
        };
        return privnames[local && priv&PRIV_LOCAL ? 1 : 0][clamp(priv&PRIV_TYPE, 0, int(priv&PRIV_LOCAL ? PRIV_ADMINISTRATOR : PRIV_LAST))];
    }

    const char *colourname(clientinfo *ci, char *name = NULL, bool icon = true, bool dupname = true, int colour = 3)
    {
        if(!name) name = ci->name;
        static string colored; colored[0] = 0; string colortmp;
        if(colour) concatstring(colored, "\fs");
        if(icon)
        {
            if(colour&1)
            {
                formatstring(colortmp, "\f[%d]", findcolour(ci));
                concatstring(colored, colortmp);
            }
            formatstring(colortmp, "\f($priv%stex)", privnamex(ci->privilege, ci->state.actortype, true));
            concatstring(colored, colortmp);
        }
        if(colour&2)
        {
            formatstring(colortmp, "\f[%d]", TEAM(ci->team, colour));
            concatstring(colored, colortmp);
        }
        concatstring(colored, name);
        if(!name[0] || (ci->state.actortype < A_ENEMY && dupname && duplicatename(ci, name)))
        {
            formatstring(colortmp, "%s[%d]", name[0] ? " " : "", ci->clientnum);
            concatstring(colored, colortmp);
        }
        if(colour) concatstring(colored, "\fS");
        return colored;
    }

    const char *teamtexnamex(int team)
    {
        const char *teamtexs[T_MAX] = { "teamtex", "teamalphatex", "teamomegatex", "teamkappatex", "teamsigmatex", "teamtex" };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *colourteam(int team, const char *icon = "")
    {
        if(team < 0 || team > T_MAX) team = T_NEUTRAL;
        static string teamed; teamed[0] = 0; string teamtmp;
        concatstring(teamed, "\fs");
        formatstring(teamtmp, "\f[%d]", TEAM(team, colour));
        concatstring(teamed, teamtmp);
        if(icon != NULL)
        {
            formatstring(teamtmp, "\f($%s)", *icon ? icon : teamtexnamex(team));
            concatstring(teamed, teamtmp);
        }
        concatstring(teamed, TEAM(team, name));
        concatstring(teamed, "\fS");
        return teamed;
    }

    bool haspriv(clientinfo *ci, int flag, const char *msg = NULL)
    {
        if((ci->local && flag <= PRIV_MAX) || (ci->privilege&PRIV_TYPE) >= flag) return true;
        else if(mastermask()&MM_AUTOAPPROVE && flag <= PRIV_ELEVATED && !numclients(ci->clientnum)) return true;
        else if(msg && *msg)
            srvmsgft(ci->clientnum, CON_CHAT, "\fraccess denied, you need to be \fs\fc%s\fS to \fs\fc%s\fS", privnamex(flag), msg);
        return false;
    }

    bool cmppriv(clientinfo *ci, clientinfo *cp, const char *msg = NULL)
    {
        string str = "";
        if(msg && *msg) formatstring(str, "%s %s", msg, colourname(cp));
        if(haspriv(ci, cp->local ? PRIV_ADMINISTRATOR : cp->privilege&PRIV_TYPE, str)) return true;
        return false;
    }

    const char *gameid() { return VERSION_GAMEID; }
    ICOMMAND(0, gameid, "", (), result(gameid()));

    int getver(int n)
    {
        switch(n)
        {
            case 0: return CUR_VERSION;
            case 1: return VERSION_GAME;
            case 2: case 3: return sversion[n%2];
            case 4: return CUR_ARCH;
            default: break;
        }
        return 0;
    }
    ICOMMAND(0, getversion, "i", (int *a), intret(getver(*a)));

    const char *gamename(int mode, int muts, int compact, int limit, const char *separator)
    {
        if(!m_game(mode)) mode = G_DEATHMATCH;
        if(gametype[mode].implied) muts |= gametype[mode].implied;
        static string gname; gname[0] = 0;
        int start = clamp(compact, 0, 3), lps = clamp(4-start, 1, 4);
        loopk(lps)
        {
            int iter = start+k;
            if(muts)
            {
                int implied = gametype[mode].implied;
                loopi(G_M_NUM) if(muts&(1<<mutstype[i].type)) implied |= mutstype[i].implied&~(1<<mutstype[i].type);
                loopi(G_M_NUM) if(muts&(1<<mutstype[i].type) && (!implied || !(implied&(1<<mutstype[i].type))))
                {
                    const char *mut = i < G_M_GSP ? mutstype[i].name : gametype[mode].gsp[i-G_M_GSP];
                    if(mut && *mut)
                    {
                        string name;
                        switch(iter)
                        {
                            case 2: case 3: formatstring(name, "%s%s%c", *gname ? gname : "", *gname ? "-" : "", mut[0]); break;
                            case 1: formatstring(name, "%s%s%c%c", *gname ? gname : "", *gname ? "-" : "", mut[0], mut[1]); break;
                            case 0: default: formatstring(name, "%s%s%s", *gname ? gname : "", *gname ? "-" : "", mut); break;
                        }
                        copystring(gname, name);
                    }
                }
            }
            defformatstring(mname, "%s%s%s", *gname ? gname : "", *gname ? separator : "", k < 3 ? gametype[mode].name : gametype[mode].sname);
            if(k < 3 && limit > 0 && int(strlen(mname)) >= limit)
            {
                gname[0] = 0;
                continue; // let's try again
            }
            copystring(gname, mname);
            break;
        }
        return gname;
    }
    ICOMMAND(0, gamename, "iiii", (int *g, int *m, int *c, int *t), result(gamename(*g, *m, *c, *t)));

    const char *modedesc(int mode, int muts, int type)
    {
        if(!m_game(mode)) mode = G_DEATHMATCH;
        if(gametype[mode].implied) muts |= gametype[mode].implied;
        static string mdname; mdname[0] = 0;
        if(type == 1 || type == 3 || type == 4) concatstring(mdname, gametype[mode].name);
        if(type == 3 || type == 4) concatstring(mdname, ": ");
        if(type == 2 || type == 3 || type == 4 || type == 5)
        {
            if((type == 4 || type == 5) && m_capture(mode) && m_gsp3(mode, muts)) concatstring(mdname, gametype[mode].gsd[2]);
            else if((type == 4 || type == 5) && m_defend(mode) && m_gsp2(mode, muts)) concatstring(mdname, gametype[mode].gsd[1]);
            else if((type == 4 || type == 5) && m_bomber(mode) && m_gsp1(mode, muts)) concatstring(mdname, gametype[mode].gsd[0]);
            else if((type == 4 || type == 5) && m_bomber(mode) && m_gsp3(mode, muts)) concatstring(mdname, gametype[mode].gsd[2]);
            else if((type == 4 || type == 5) && m_race(mode) && m_gsp1(mode, muts)) concatstring(mdname, gametype[mode].gsd[0]);
            else if((type == 4 || type == 5) && m_race(mode) && m_gsp3(mode, muts)) concatstring(mdname, gametype[mode].gsd[2]);
            else concatstring(mdname, gametype[mode].desc);
        }
        return mdname;
    }
    ICOMMAND(0, modedesc, "iii", (int *g, int *m, int *c), result(modedesc(*g, *m, *c)));

    const char *mutsdesc(int mode, int muts, int type)
    {
        if(!m_game(mode)) mode = G_DEATHMATCH;
        static string mtname; mtname[0] = 0;
        int mutid = -1;
        loopi(G_M_NUM) if(muts == (1<<mutstype[i].type)) mutid = i;
        if(mutid < 0) return "";
        if(type == 4 || type == 5)
        {
            if(m_capture(mode) && m_gsp3(mode, muts)) return "";
            else if(m_defend(mode) && m_gsp2(mode, muts)) return "";
            else if(m_bomber(mode) && (m_gsp1(mode, muts) || m_gsp3(mode, muts))) return "";
            else if(m_race(mode) && (m_gsp1(mode, muts) || m_gsp3(mode, muts))) return "";
        }
        if(type == 1 || type == 3 || type == 4)
        {
            const char *n = mutid >= G_M_GSP ? gametype[mode].gsp[mutid-G_M_GSP] : mutstype[mutid].name;
            if(!n || !*n) return "";
            concatstring(mtname, n);
        }
        if(type == 3 || type == 4) concatstring(mtname, ": ");
        if(type == 2 || type == 3 || type == 4 || type == 5)
        {
            const char *n = mutid >= G_M_GSP ? gametype[mode].gsd[mutid-G_M_GSP] : mutstype[mutid].desc;
            if(!n || !*n) return "";
            concatstring(mtname, n);
        }
        return mtname;
    }
    ICOMMAND(0, mutsdesc, "iii", (int *g, int *m, int *c), result(mutsdesc(*g, *m, *c)));

    void changemode(int &mode, int &muts)
    {
        if(mode < 0)
        {
            mode = G(defaultmode);
            if(G(rotatemode))
            {
                int num = 0;
                loopi(G_MAX) if(G(rotatemodefilter)&(1<<i)) num++;
                if(!num) mode = rnd(G_RAND)+G_PLAY;
                else
                {
                    int r = rnd(num), n = 0;
                    loopi(G_MAX) if(G(rotatemodefilter)&(1<<i))
                    {
                        if(n != r) n++;
                        else { mode = i; break; }
                    }
                }
                if(!mode || !(G(rotatemodefilter)&(1<<mode))) mode = rnd(G_RAND)+G_PLAY;
            }
        }
        if(muts < 0)
        {
            muts = G(defaultmuts);
            if(G(rotatemuts))
            {
                int num = rnd(G_M_NUM+1);
                if(num) loopi(num) if(G(rotatemuts) == 1 || !rnd(G(rotatemuts)))
                {
                    int rmut = 1<<rnd(G_M_NUM);
                    if(G(rotatemutsfilter) && !(G(rotatemutsfilter)&rmut)) continue;
                    muts |= rmut;
                    modecheck(mode, muts, rmut);
                }
            }
        }
        modecheck(mode, muts);
    }

    const char *choosemap(const char *suggest, int mode, int muts, int force, bool notry)
    {
        static string chosen;
        if(suggest && *suggest)
        {
            if(!strncasecmp(suggest, "maps/", 5) || !strncasecmp(suggest, "maps\\", 5))
                copystring(chosen, suggest+5);
            else copystring(chosen, suggest);
        }
        else *chosen = 0;
        int rotate = force ? force : G(rotatemaps);
        if(rotate) loopj(2)
        {
            char *list = NULL;
            maplist(list, mode, muts, numclients(), G(rotatemapsfilter), j!=0);
            if(list)
            {
                bool found = false;
                int n = listlen(list), c = n ? rnd(n) : 0;
                if(c >= 0)
                {
                    int len = 0;
                    const char *elem = indexlist(list, c, len);
                    if(len > 0)
                    {
                        copystring(chosen, elem, len+1);
                        found = true;
                    }
                }
                DELETEA(list);
                if(found) break;
            }
        }
        return *chosen ? chosen : pickmap(suggest, mode, muts, notry);
    }

    bool canload(const char *type)
    {
        if(!strcmp(type, gameid())) return true;
        if(!strcmp(type, "bfa")) return true;
        if(!strcmp(type, "bfg")) return true;
        return false;
    }

    int timeleft()
    {
        switch(gamestate)
        {
            case G_S_PLAYING: case G_S_OVERTIME: return timeremaining;
            default: return max(gamewaittime-totalmillis, 0)/1000;
        }
        return 0;
    }

    void sendtick()
    {
        sendf(-1, 1, "ri3", N_TICK, gamestate, timeleft());
    }

    bool checkvotes(bool force = false);
    void sendstats();
    void startintermission(bool req = false)
    {
        if(gs_playing(gamestate))
        {
            sendstats();
            setpause(false);
            timeremaining = 0;
            gamelimit = min(gamelimit, gamemillis);
            if(smode) smode->intermission();
            mutate(smuts, mut->intermission());
        }
        if(req || !G(intermlimit))
        {
            checkdemorecord(true);
            if(gamestate != G_S_VOTING && G(votelimit))
            {
                gamestate = G_S_VOTING;
                gamewaittime = totalmillis+G(votelimit);
                sendtick();
            }
            else checkvotes(true);
        }
        else
        {
            gamestate = G_S_INTERMISSION;
            gamewaittime = totalmillis+G(intermlimit);
            sendtick();
        }
    }

    bool wantsovertime()
    {
        if(smode && smode->wantsovertime()) return true;
        mutate(smuts, if(mut->wantsovertime()) return true);
        if(!G(overtimeallow) || m_balance(gamemode, mutators, teamspawns)) return false;
        bool result = false;
        if(m_team(gamemode, mutators))
        {
            int best = -1;
            loopi(numteams(gamemode, mutators))
            {
                score &cs = teamscore(i+T_FIRST);
                if(best < 0 || cs.total > teamscore(best).total)
                {
                    best = i+T_FIRST;
                    result = false;
                }
                else if(cs.total == teamscore(best).total) result = true;
            }
        }
        else
        {
            int best = -1;
            loopv(clients) if(clients[i]->state.actortype < A_ENEMY && clients[i]->state.state != CS_SPECTATOR)
            {
                if(best < 0 || (m_laptime(gamemode, mutators) ? (clients[best]->state.cptime <= 0 || (clients[i]->state.cptime > 0 && clients[i]->state.cptime < clients[best]->state.cptime)) : clients[i]->state.points > clients[best]->state.points))
                {
                    best = i;
                    result = false;
                }
                else if(m_laptime(gamemode, mutators) ? clients[i]->state.cptime == clients[best]->state.cptime : clients[i]->state.points == clients[best]->state.points) result = true;
            }
        }
        return result;
    }

    void doteambalance(bool init)
    {
        vector<clientinfo *> tc[T_TOTAL];
        int numplaying = 0;
        loopv(clients)
        {
            clientinfo *cp = clients[i];
            if(!cp->team || cp->state.state == CS_SPECTATOR || cp->state.actortype > A_PLAYER) continue;
            cp->state.updatetimeplayed();
            tc[cp->team-T_FIRST].add(cp);
            numplaying++;
        }
        if(numplaying >= G(teambalanceplaying))
        {
            int nt = numteams(gamemode, mutators), mid = numplaying/nt, pmax = -1, pmin = -1;
            loopi(nt)
            {
                int cl = tc[i].length();
                if(pmax < 0 || cl > pmax) pmax = cl;
                if(pmin < 0 || cl < pmin) pmin = cl;
            }
            int offset = pmax-pmin;
            if(offset >= G(teambalanceamt))
            {
                if(!init && !nextteambalance)
                {
                    int secs = G(teambalancedelay)/1000;
                    nextteambalance = gamemillis+G(teambalancedelay);
                    ancmsgft(-1, S_V_BALWARN, CON_EVENT, "\fy\fs\fzoyWARNING:\fS \fs\fcteams\fS will be \fs\fcbalanced\fS in \fs\fc%d\fS %s", secs, secs != 1 ? "seconds" : "second");
                }
                else if(init || canbalancenow())
                {
                    int moved = 0;
                    loopi(nt) for(int team = i+T_FIRST, iters = tc[i].length(); iters > 0 && tc[i].length() > mid; iters--)
                    {
                        int id = -1;
                        loopvj(tc[i])
                        {
                            clientinfo *cp = tc[i][j];
                            if(m_swapteam(gamemode, mutators) && cp->swapteam && cp->swapteam == team) { id = j; break; }
                            switch(G(teambalancestyle))
                            {
                                case 1: if(id < 0 || tc[i][id]->state.timeplayed > cp->state.timeplayed) id = j; break;
                                case 2: if(id < 0 || tc[i][id]->state.points > cp->state.points) id = j; break;
                                case 3: if(id < 0 || tc[i][id]->state.frags > cp->state.frags) id = j; break;
                                case 4: if(id < 0 || tc[i][id]->state.scoretime(false) > cp->state.scoretime(false)) id = j; break;
                                case 0: default: if(id < 0) id = j; break;
                            }
                            if(!G(teambalancestyle) && !m_swapteam(gamemode, mutators)) break;
                        }
                        if(id >= 0)
                        {
                            clientinfo *cp = tc[i][id];
                            cp->swapteam = T_NEUTRAL; // make them rechoose if necessary
                            int t = chooseteam(cp, -1, true);
                            if(t != cp->team)
                            {
                                setteam(cp, t, (m_balreset(gamemode, mutators) ? TT_RESET : 0)|TT_INFOSM, false);
                                cp->state.lastdeath = 0;
                                tc[i].removeobj(cp);
                                tc[t-T_FIRST].add(cp);
                                moved++;
                            }
                        }
                        else break; // won't get any more
                    }
                    if(!init)
                    {
                        if(moved) ancmsgft(-1, S_V_BALALERT, CON_EVENT, "\fy\fs\fzoyALERT:\fS \fs\fcteams\fS have now been \fs\fcbalanced\fS");
                        else ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fy\fs\fzoyALERT:\fS \fs\fcteams\fS failed to be \fs\fcbalanced\fS");
                    }
                    lastteambalance = gamemillis+G(teambalancewait);
                    nextteambalance = 0;
                }
            }
            else
            {
                if(!init && nextteambalance) ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fy\fs\fzoyALERT:\fS \fs\fcteams\fS no longer need to be \fs\fcbalanced\fS");
                lastteambalance = gamemillis+(nextteambalance ? G(teambalancewait) : G(teambalancedelay));
                nextteambalance = 0;
            }
        }
        else
        {
            if(!init && nextteambalance) ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fy\fs\fzoyALERT:\fS \fs\fcteams\fS are no longer able to be \fs\fcbalanced\fS");
            lastteambalance = gamemillis+(nextteambalance ? G(teambalancewait) : G(teambalancedelay));
            nextteambalance = 0;
        }
    }

    void checklimits()
    {
        if(!m_play(gamemode)) return;
        bool wasinovertime = gamestate == G_S_OVERTIME;
        int limit = wasinovertime ? G(overtimelimit) : G(timelimit), numt = numteams(gamemode, mutators);
        bool newlimit = limit != oldtimelimit, newtimer = gamemillis-curtime>0 && gamemillis/1000!=(gamemillis-curtime)/1000,
             iterate = newlimit || newtimer;
        if(iterate)
        {
            if(newlimit)
            {
                if(limit && oldtimelimit) gamelimit += (limit-oldtimelimit)*60000;
                else if(limit) gamelimit = max(gamemillis, limit*60000);
                oldtimelimit = limit;
            }
            if(timeremaining)
            {
                if(limit)
                {
                    if(gamemillis >= gamelimit) timeremaining = 0;
                    else timeremaining = (gamelimit-gamemillis+999)/1000;
                }
                else timeremaining = -1;
                bool wantsoneminute = true;
                if(!timeremaining)
                {
                    if(gamestate != G_S_OVERTIME && wantsovertime())
                    {
                        limit = oldtimelimit = G(overtimelimit);
                        if(limit)
                        {
                            timeremaining = limit*60;
                            gamelimit += timeremaining*1000;
                            ancmsgft(-1, S_V_OVERTIME, CON_EVENT, "\fyovertime, match extended by \fs\fc%d\fS %s", limit, limit > 1 ? "minutes" : "minute");
                        }
                        else
                        {
                            timeremaining = -1;
                            gamelimit = 0;
                            ancmsgft(-1, S_V_OVERTIME, CON_EVENT, "\fyovertime, match extended until someone wins");
                        }
                        gamestate = G_S_OVERTIME;
                        wantsoneminute = false;
                    }
                    else
                    {
                        ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fytime limit has been reached");
                        startintermission();
                        return; // bail
                    }
                }
                if(gs_playing(gamestate) && timeremaining != 0)
                {
                    if(wantsoneminute && timeremaining == 60) ancmsgft(-1, S_V_ONEMINUTE, CON_EVENT, "\fzygone minute remains");
                    sendtick();
                }
            }
        }
        if(wasinovertime && !wantsovertime())
        {
            ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyovertime has ended, a winner has been chosen");
            startintermission();
            return; // bail
        }
        if(!m_balance(gamemode, mutators, teamspawns) && G(pointlimit) && m_dm(gamemode))
        {
            if(m_team(gamemode, mutators))
            {
                int best = -1;
                loopi(numt) if(best < 0 || teamscore(i+T_FIRST).total > teamscore(best).total)
                    best = i+T_FIRST;
                if(best >= 0 && teamscore(best).total >= G(pointlimit))
                {
                    ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyscore limit has been reached");
                    startintermission();
                    return; // bail
                }
            }
            else
            {
                int best = -1;
                loopv(clients) if(clients[i]->state.actortype < A_ENEMY && (best < 0 || clients[i]->state.points > clients[best]->state.points))
                    best = i;
                if(best >= 0 && clients[best]->state.points >= G(pointlimit))
                {
                    ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyscore limit has been reached");
                    startintermission();
                    return; // bail
                }
            }
        }
        if(m_balance(gamemode, mutators, teamspawns) && gamelimit > 0 && curbalance < (numt-1))
        {
            int delpart = min(gamelimit/(numt*2), G(balancedelay)), balpart = (gamelimit/numt*(curbalance+1))-delpart;
            if(gamemillis >= balpart)
            {
                if(!nextbalance)
                {
                    nextbalance = gamemillis+delpart;
                    if(delpart >= 1000)
                    {
                        int secs = delpart/1000;
                        ancmsgft(-1, S_V_BALWARN, CON_EVENT, "\fy\fs\fzoyWARNING:\fS \fs\fcteams\fS will be \fs\fcreassigned\fS in \fs\fc%d\fS %s %s", secs, secs != 1 ? "seconds" : "second", m_forcebal(gamemode, mutators) ? "to switch roles" : "for map symmetry");
                    }
                }
                if(gamemillis >= nextbalance && canbalancenow())
                {
                    int oldbalance = curbalance;
                    if(++curbalance >= numt) curbalance = 0; // safety first
                    static vector<clientinfo *> assign[T_TOTAL];
                    loopk(T_TOTAL) assign[k].setsize(0);
                    loopv(clients) if(isteam(gamemode, mutators, clients[i]->team, T_FIRST))
                        assign[clients[i]->team-T_FIRST].add(clients[i]);
                    int scores[T_TOTAL] = {0};
                    loopk(numt) scores[k] = teamscore(k+T_FIRST).total;
                    loopk(numt)
                    {
                        int from = mapbals[oldbalance][k], fromt = from-T_FIRST,
                            to = mapbals[curbalance][k], tot = to-T_FIRST;
                        loopv(assign[fromt])
                        {
                            clientinfo *cp = assign[fromt][i];
                            if(cp->swapteam)
                            {
                                loopj(numt) if(mapbals[oldbalance][j] == cp->swapteam)
                                {
                                    cp->swapteam = mapbals[curbalance][j];
                                    break;
                                }
                            }
                            if(m_race(gamemode))
                            {
                                cp->state.cpmillis = 0;
                                cp->state.cpnodes.shrink(0);
                                sendf(-1, 1, "ri3", N_CHECKPOINT, cp->clientnum, -1);
                            }
                            setteam(cp, to, (m_balreset(gamemode, mutators) ? TT_RESET : 0)|TT_INFO, false);
                            cp->state.lastdeath = 0;
                        }
                        score &cs = teamscore(from);
                        cs.total = scores[tot];
                        sendf(-1, 1, "ri3", N_SCORE, cs.team, cs.total);
                    }
                    ancmsgft(-1, S_V_BALALERT, CON_EVENT, "\fy\fs\fzoyALERT:\fS \fs\fcteams\fS have %sbeen \fs\fcreassigned\fS %s", delpart > 0 ? "now " : "", m_forcebal(gamemode, mutators) ? "to switch roles" : "for map symmetry");
                    if(smode) smode->balance(oldbalance);
                    mutate(smuts, mut->balance(oldbalance));
                    if(smode) smode->layout();
                    mutate(smuts, mut->layout());
                    nextbalance = 0;
                }
            }
        }
        if(m_balteam(gamemode, mutators, 4) && gamestate != G_S_OVERTIME && gamemillis >= G(teambalancewait) &&
           (!lastteambalance || gamemillis >= lastteambalance) && (!nextteambalance || gamemillis >= nextteambalance))
                doteambalance(false);
    }

    bool hasitem(int i)
    {
        if((m_race(gamemode) && !m_gsp3(gamemode, mutators)) || m_basic(gamemode, mutators) || !sents.inrange(i) || sents[i].type != WEAPON) return false;
        int sweap = m_weapon(gamemode, mutators), attr = w_attr(gamemode, mutators, sents[i].type, sents[i].attrs[0], sweap);
        if(!isweap(attr) || !w_item(attr, sweap) || !m_check(W(attr, modes), W(attr, muts), gamemode, mutators) || W(attr, disabled)) return false;
        if((sents[i].attrs[4] && sents[i].attrs[4] != triggerid) || !m_check(sents[i].attrs[2], sents[i].attrs[3], gamemode, mutators)) return false;
        return true;
    }

    bool finditem(int i, bool spawned = false, bool carry = false)
    {
        if(sents[i].spawned) return true;
        if(sents[i].type == WEAPON && !(sents[i].attrs[1]&W_F_FORCED)) loopvk(clients)
        {
            clientinfo *ci = clients[k];
            if(ci->state.dropped.find(i) && (!spawned || gamemillis < sents[i].millis)) return true;
            else if(carry) loopj(W_MAX)
                if(ci->online && ci->state.state == CS_ALIVE && ci->state.entid[j] == i && ci->state.hasweap(j, m_weapon(gamemode, mutators)))
                    return spawned;
        }
        if(spawned && gamemillis < sents[i].millis) return true;
        return false;
    }

    template<class T>
    void sortrandomly(vector<T> &src)
    {
        vector<T> dst;
        dst.reserve(src.length());
        while(src.length()) dst.add(src.removeunordered(rnd(src.length())));
        src.move(dst);
    }

    void setupitems(bool update)
    {
        vector<int> items, enemies;
        int sweap = m_weapon(gamemode, mutators);
        loopv(sents)
        {
            if(sents[i].type == ACTOR && sents[i].attrs[0] >= 0 && sents[i].attrs[0] < A_TOTAL && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
            {
                sents[i].millis = gamemillis+G(enemyspawndelay);
                switch(G(enemyspawnstyle) == 3 ? rnd(2)+1 : G(enemyspawnstyle))
                {
                    case 1: enemies.add(i); break;
                    case 2: sents[i].millis += (G(enemyspawntime)+rnd(G(enemyspawntime)))/2; break;
                    default: break;
                }
            }
            else if(m_play(gamemode) && enttype[sents[i].type].usetype == EU_ITEM && hasitem(i))
            {
                sents[i].millis = gamemillis+G(itemspawndelay);
                switch(G(itemspawnstyle) == 3 ? rnd(2)+1 : G(itemspawnstyle))
                {
                    case 1: items.add(i); break;
                    case 2:
                    {
                        int delay = sents[i].type == WEAPON ? w_spawn(w_attr(gamemode, mutators, sents[i].type, sents[i].attrs[0], sweap)) : G(itemspawntime);
                        if(delay > 1) sents[i].millis += (delay+rnd(delay))/2;
                        break;
                    }
                    default: break;
                }
            }
        }
        if(!items.empty())
        {
            sortrandomly(items);
            loopv(items) sents[items[i]].millis += G(itemspawndelay)*i;
        }
        if(!enemies.empty())
        {
            sortrandomly(enemies);
            loopv(enemies) sents[enemies[i]].millis += G(enemyspawndelay)*i;
        }
    }

    void setuptriggers(bool update)
    {
        triggerid = 0;
        loopi(TRIGGERIDS+1) triggers[i].reset(i);
        if(!update) return;

        loopv(sents) if(enttype[sents[i].type].idattr >= 0 && sents[i].attrs[enttype[sents[i].type].idattr] >= 0 && sents[i].attrs[enttype[sents[i].type].idattr] <= TRIGGERIDS)
        {
            if(enttype[sents[i].type].modesattr >= 0 && !m_check(sents[i].attrs[enttype[sents[i].type].modesattr], sents[i].attrs[enttype[sents[i].type].modesattr+1], gamemode, mutators)) continue;
            triggers[sents[i].attrs[enttype[sents[i].type].idattr]].ents.add(i);
        }

        vector<int> valid;
        loopi(TRIGGERIDS) if(!triggers[i+1].ents.empty()) valid.add(triggers[i+1].id);
        if(!valid.empty()) triggerid = valid[rnd(valid.length())];

        loopi(TRIGGERIDS) if(triggers[i+1].id != triggerid) loopvk(triggers[i+1].ents)
        {
            if(sents[triggers[i+1].ents[k]].type != TRIGGER) continue;
            bool spawn = sents[triggers[i+1].ents[k]].attrs[4]%2;
            if(spawn != sents[triggers[i+1].ents[k]].spawned)
            {
                sents[triggers[i+1].ents[k]].spawned = spawn;
                sents[triggers[i+1].ents[k]].millis = gamemillis;
            }
            sendf(-1, 1, "ri3", N_TRIGGER, triggers[i+1].ents[k], 1+(spawn ? 2 : 1));
            loopvj(sents[triggers[i+1].ents[k]].kin) if(sents.inrange(sents[triggers[i+1].ents[k]].kin[j]))
            {
                sents[sents[triggers[i+1].ents[k]].kin[j]].spawned = sents[triggers[i+1].ents[k]].spawned;
                sents[sents[triggers[i+1].ents[k]].kin[j]].millis = sents[triggers[i+1].ents[k]].millis;
            }
        }
    }

    struct spawn
    {
        int current, iteration;
        vector<int> ents;
        vector<int> cycle;

        spawn() { reset(); }
        ~spawn() {}

        void reset()
        {
            ents.shrink(0);
            cycle.shrink(0);
            iteration = 0;
            current = -1;
        }
        void add(int n)
        {
            ents.add(n);
            cycle.add(0);
        }
    } spawns[T_ALL];

    void setupspawns(bool update)
    {
        totalspawns = 0;
        teamspawns = m_team(gamemode, mutators);
        loopi(T_ALL) spawns[i].reset();
        if(update)
        {
            int numt = numteams(gamemode, mutators), cplayers = 0;
            if(m_race(gamemode))
            {
                loopv(sents) if(sents[i].type == PLAYERSTART && sents[i].attrs[0] == T_NEUTRAL && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                {
                    spawns[m_gsp3(gamemode, mutators) ? T_ALPHA : T_NEUTRAL].add(i);
                    totalspawns++;
                }
                if(!totalspawns) loopv(sents) if(sents[i].type == CHECKPOINT && sents[i].attrs[6] == CP_START && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                {
                    spawns[m_gsp3(gamemode, mutators) ? T_ALPHA : T_NEUTRAL].add(i);
                    totalspawns++;
                }
                if(m_gsp3(gamemode, mutators))
                {
                    int enemyspawns = 0;
                    loopv(sents) if(sents[i].type == PLAYERSTART && sents[i].attrs[0] >= T_OMEGA && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                    {
                        loopk(numt-1) spawns[T_OMEGA+k].add(i);
                        totalspawns++;
                        enemyspawns++;
                    }
                    if(!enemyspawns) loopv(sents) if(sents[i].type == CHECKPOINT && sents[i].attrs[6] == CP_RESPAWN && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                    {
                        loopk(numt-1) spawns[T_OMEGA+k].add(i);
                        totalspawns++;
                        enemyspawns++;
                    }
                }
                setmod(sv_numplayers, 0);
                setmod(sv_maxplayers, 0);
                return;
            }
            if(!teamspawns && m_duel(gamemode, mutators))
            { // iterate through teams so players spawn on opposite sides in duel
                teamspawns = true;
                numt = 2;
            }
            if(m_play(gamemode) && teamspawns)
            {
                loopk(3)
                {
                    loopv(sents) if(sents[i].type == PLAYERSTART && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                    {
                        if(!k && (m_team(gamemode, mutators) ? !isteam(gamemode, mutators, sents[i].attrs[0], T_FIRST) : (sents[i].attrs[0] == T_ALPHA || sents[i].attrs[0] == T_OMEGA)))
                            continue;
                        else if(k == 1 && sents[i].attrs[0] == T_NEUTRAL) continue;
                        else if(k == 2 && sents[i].attrs[0] != T_NEUTRAL) continue;
                        spawns[k ? T_NEUTRAL : sents[i].attrs[0]].add(i);
                        totalspawns++;
                    }
                    if(totalspawns && m_team(gamemode, mutators))
                    {
                        loopi(numt) if(spawns[i+T_FIRST].ents.empty())
                        {
                            loopj(T_ALL) spawns[j].reset();
                            totalspawns = 0;
                            break;
                        }
                    }
                    if(totalspawns) break;
                    teamspawns = false;
                }
                if(totalspawns && teamspawns)
                {
                    int actt = numteams(gamemode, mutators), off = numt-actt;
                    if(off > 0) loopk(off)
                    {
                        int t = T_ALPHA+k*2, v = t+2;
                        if(isteam(gamemode, mutators, t, T_FIRST) && isteam(gamemode, mutators, v, T_FIRST))
                            loopv(spawns[t].ents) spawns[v].add(spawns[t].ents[i]);
                    }
                }
            }
            if(!totalspawns)
            { // use all neutral spawns
                teamspawns = false;
                loopv(sents) if(sents[i].type == PLAYERSTART && sents[i].attrs[0] == T_NEUTRAL && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                {
                    spawns[T_NEUTRAL].add(i);
                    totalspawns++;
                }
            }
            if(!totalspawns)
            { // use all spawns
                teamspawns = false;
                loopk(2)
                {
                    loopv(sents) if(sents[i].type == PLAYERSTART && (k || ((sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))))
                    {
                        spawns[T_NEUTRAL].add(i);
                        totalspawns++;
                    }
                    if(totalspawns) break;
                }
            }

            if(totalspawns) cplayers = totalspawns/2;
            else
            { // we can cheat and use weapons for spawns
                teamspawns = false;
                loopv(sents) if(sents[i].type == WEAPON)
                {
                    spawns[T_NEUTRAL].add(i);
                    totalspawns++;
                }
                cplayers = totalspawns/3;
            }
            if(!m_edit(gamemode))
            {
                if(!cplayers) cplayers = totalspawns ? totalspawns : 1;
                int np = G(numplayers) ? G(numplayers) : cplayers, mp = G(maxplayers) ? G(maxplayers) : np*3;
                if(m_play(gamemode) && m_team(gamemode, mutators))
                {
                    int offt = np%numt, offq = mp%numt;
                    if(offt) np += numt-offt;
                    if(offq) mp += numt-offq;
                }
                if(mp < np) mp = np;
                setmod(sv_numplayers, np);
                setmod(sv_maxplayers, mp);
            }
        }
    }

    int pickspawn(clientinfo *ci)
    {
        if(ci->state.actortype >= A_ENEMY) return ci->state.spawnpoint;
        else
        {
            if(m_race(gamemode) && !ci->state.cpnodes.empty() && (!m_gsp3(gamemode, mutators) || ci->team == T_ALPHA))
            {
                int checkpoint = ci->state.cpnodes.last();
                if(sents.inrange(checkpoint)) return checkpoint;
            }
            if(totalspawns)
            {
                int team = T_NEUTRAL, rotate = G(spawnrotate);
                if(m_duel(gamemode, mutators) && !m_team(gamemode, mutators))
                {
                    if(!spawns[T_ALPHA].ents.empty() && !spawns[T_OMEGA].ents.empty())
                        team = spawns[T_ALPHA].iteration <= spawns[T_OMEGA].iteration ? T_ALPHA : T_OMEGA;
                    if(!rotate) rotate = 2;
                }
                else if(m_play(gamemode) && m_team(gamemode, mutators) && (!m_race(gamemode) || m_gsp3(gamemode, mutators)) && !spawns[ci->team].ents.empty()) team = ci->team;
                else switch(rotate)
                {
                    case 2:
                    { // random
                        static vector<int> lowest;
                        lowest.setsize(0);
                        loopv(spawns[team].cycle) if(lowest.empty() || spawns[team].cycle[i] <= spawns[team].cycle[lowest[0]])
                        {
                            if(spawns[team].cycle.length() >= 2 && spawns[team].current == i) continue; // avoid using this one again straight away
                            if(!lowest.empty() && spawns[team].cycle[i] < spawns[team].cycle[lowest[0]]) lowest.setsize(0);
                            lowest.add(i);
                        }
                        if(!lowest.empty())
                        {
                            spawns[team].current = lowest[lowest.length() >= 2 ? rnd(lowest.length()) : 0];
                            break;
                        }
                        // fall through if this fails..
                    }
                    case 1:
                    { // sequential
                        if(++spawns[team].current >= spawns[team].ents.length()) spawns[team].current = 0;
                        break;
                    }
                    case 0: default: spawns[team].current = -1; break;
                }
                if(spawns[team].ents.inrange(spawns[team].current))
                {
                    spawns[team].iteration++;
                    spawns[team].cycle[spawns[team].current]++;
                    return spawns[team].ents[spawns[team].current];
                }
            }
        }
        return -1;
    }

    void setupgameinfo()
    {
        setuptriggers(true);
        setupitems(true);
        setupspawns(true);
        hasgameinfo = true;
        aiman::poke();
    }

    void sendspawn(clientinfo *ci)
    {
        servstate &gs = ci->state;
        int weap = -1, health = 0;
        if(ci->state.actortype >= A_ENEMY)
        {
            bool hasent = sents.inrange(ci->state.spawnpoint) && sents[ci->state.spawnpoint].type == ACTOR;
            if(m_sweaps(gamemode, mutators)) weap = m_weapon(gamemode, mutators);
            else weap = hasent && sents[ci->state.spawnpoint].attrs[6] > 0 ? sents[ci->state.spawnpoint].attrs[6]-1 : actor[ci->state.actortype].weap;
            if(!m_insta(gamemode, mutators)) health = max(int((hasent && sents[ci->state.spawnpoint].attrs[7] > 0 ? sents[ci->state.spawnpoint].attrs[7] : actor[ci->state.actortype].health)*G(enemystrength)), 1);
            if(!isweap(weap) || (!actor[ci->state.actortype].canmove && weaptype[weap].melee)) weap = -1; // let spawnstate figure it out
        }
        else health = m_health(gamemode, mutators, ci->state.model);
        int spawn = pickspawn(ci);
        gs.spawnstate(gamemode, mutators, weap, health);
        sendf(ci->clientnum, 1, "ri9iv", N_SPAWNSTATE, ci->clientnum, spawn, gs.state, gs.points, gs.frags, gs.deaths, gs.health, gs.cptime, gs.weapselect, W_MAX, &gs.ammo[0]);
        gs.lastrespawn = gs.lastspawn = gamemillis;
        gs.updatetimeplayed();
    }

    template<class T>
    void sendstate(servstate &gs, T &p)
    {
        putint(p, gs.state);
        putint(p, gs.points);
        putint(p, gs.frags);
        putint(p, gs.deaths);
        putint(p, gs.health);
        putint(p, gs.cptime);
        putint(p, gs.weapselect);
        loopi(W_MAX) putint(p, gs.ammo[i]);
    }

    void relayf(int r, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        ircoutf(r, "%s", str);
#ifdef STANDALONE
        bigstring ft;
        filterstring(ft, str);
        logoutf("%s", ft);
#endif
    }

    void ancmsgft(int cn, int snd, int conlevel, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        if(cn < 0 || allowbroadcast(cn)) sendf(cn, 1, "ri3s", N_ANNOUNCE, snd, conlevel, str);
    }

    void srvmsgft(int cn, int conlevel, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        if(cn < 0 || allowbroadcast(cn)) sendf(cn, 1, "ri2s", N_SERVMSG, conlevel, str);
    }

    void srvmsgftforce(int cn, int conlevel, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        if(cn < 0 || allowbroadcast(cn)) sendf(cn, 1, "ri2s", N_SERVMSG, conlevel, str);
        if(cn >= 0 && !allowbroadcast(cn)) sendf(cn, 1, "ri2s", N_SERVMSG, conlevel, str);
    }

    void srvmsgf(int cn, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        if(cn < 0 || allowbroadcast(cn))
        {
            int conlevel = CON_MESG;
            switch(cn)
            {
                case -3: conlevel = CON_CHAT; cn = -1; break;
                case -2: conlevel = CON_EVENT; cn = -1; break;
                default: break;
            }
            sendf(cn, 1, "ri2s", N_SERVMSG, conlevel, str);
        }
    }

    void srvoutf(int r, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        srvmsgf(r >= 0 ? -1 : -2, "%s", str);
        relayf(abs(r), "%s", str);
    }

    void srvoutforce(clientinfo *ci, int r, const char *s, ...)
    {
        defvformatbigstring(str, s, s);
        srvmsgf(r >= 0 ? -1 : -2, "%s", str);
        if(!allowbroadcast(ci->clientnum))
            sendf(ci->clientnum, 1, "ri2s", N_SERVMSG, r >= 0 ? int(CON_MESG) : int(CON_EVENT), str);
        relayf(abs(r), "%s", str);
    }

    void listdemos(int cn)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SENDDEMOLIST);
        putint(p, demos.length());
        loopv(demos)
        {
            sendstring(demos[i].info, p);
            putint(p, demos[i].len);
            putint(p, demos[i].ctime);
        }
        sendpacket(cn, 1, p.finalize());
    }

    void cleardemos(int n)
    {
        if(!n)
        {
            loopv(demos) delete[] demos[i].data;
            demos.shrink(0);
            srvoutf(4, "\fycleared all demos");
        }
        else if(demos.inrange(n-1))
        {
            delete[] demos[n-1].data;
            demos.remove(n-1);
            srvoutf(4, "\fycleared demo \fs\fc%d\fS", n);
        }
    }

    void senddemo(int cn, int num)
    {
        if(!num) num = demos.length();
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
        sendf(cn, 2, "ri2m", N_SENDDEMO, d.ctime, d.len, d.data);
    }

    void sendwelcome(clientinfo *ci);
    int welcomepacket(packetbuf &p, clientinfo *ci);

    void enddemoplayback()
    {
        if(!demoplayback) return;
        DELETEP(demoplayback);
        loopv(clients) sendf(clients[i]->clientnum, 1, "ri3", N_DEMOPLAYBACK, 0, clients[i]->clientnum);
        srvoutf(4, "\fydemo playback finished");
        loopv(clients) sendwelcome(clients[i]);
        startintermission(true);
        resetgamevars(true, true);
    }

    void setupdemoplayback()
    {
        demoheader hdr;
        string msg = "";
        defformatstring(file, strstr(smapname, "maps/")==smapname || strstr(smapname, "maps\\")==smapname ? "%s.dmo" : "demos/%s.dmo", smapname);
        demoplayback = opengzfile(file, "rb");
        if(!demoplayback) formatstring(msg, "\frcould not read demo \fs\fc%s\fS", file);
        else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, VERSION_DEMOMAGIC, sizeof(hdr.magic)))
            formatstring(msg, "\frsorry, \fs\fc%s\fS is not a demo file", file);
        else
        {
            lilswap(&hdr.gamever, 4);
            if(hdr.gamever!=VERSION_GAME)
                formatstring(msg, "\frdemo \fs\fc%s\fS requires %s version of %s", file, hdr.gamever<VERSION_GAME ? "an older" : "a newer", VERSION_NAME);
        }
        if(msg[0])
        {
            DELETEP(demoplayback);
            srvoutf(4, "%s", msg);
            return;
        }

        srvoutf(4, "\fyplaying demo \fs\fc%s\fS", file);
        sendf(-1, 1, "ri3", N_DEMOPLAYBACK, 1, -1);

        if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        lilswap(&nextplayback, 1);
    }

    void readdemo()
    {
        if(!demoplayback || paused) return;
        while(gamemillis>=nextplayback)
        {
            int chan, len;
            if(demoplayback->read(&chan, sizeof(chan))!=sizeof(chan) ||
                demoplayback->read(&len, sizeof(len))!=sizeof(len))
            {
                enddemoplayback();
                return;
            }
            lilswap(&chan, 1);
            lilswap(&len, 1);
            ENetPacket *packet = enet_packet_create(NULL, len, 0);
            if(!packet || demoplayback->read(packet->data, len)!=size_t(len))
            {
                if(packet) enet_packet_destroy(packet);
                enddemoplayback();
                return;
            }
            sendpacket(-1, chan, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            if(!demoplayback) break;
            if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
            {
                enddemoplayback();
                return;
            }
            lilswap(&nextplayback, 1);
        }
    }

    void prunedemos(int extra = 0)
    {
        int n = clamp(demos.length()+extra-G(democount), 0, demos.length());
        if(n <= 0) return;
        loopi(n) delete[] demos[i].data;
        demos.remove(0, n);
    }

    struct demoinfo
    {
        demoheader hdr;
        string file;
    };
    vector<demoinfo> demoinfos;
    vector<char *> faildemos;

    int scandemo(const char *name)
    {
        if(!name || !*name) return -1;
        loopv(demoinfos) if(!strcmp(demoinfos[i].file, name)) return i;
        loopv(faildemos) if(!strcmp(faildemos[i], name)) return -1;
        stream *f = opengzfile(name, "rb");
        if(!f)
        {
            faildemos.add(newstring(name));
            return -1;
        }
        int num = demoinfos.length();
        demoinfo &d = demoinfos.add();
        copystring(d.file, name);
        string msg = "";
        if(f->read(&d.hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(d.hdr.magic, VERSION_DEMOMAGIC, sizeof(d.hdr.magic)))
            formatstring(msg, "\fs\fc%s\fS is not a demo file", name);
        else
        {
            lilswap(&d.hdr.gamever, 4);
            if(d.hdr.gamever!=VERSION_GAME)
                formatstring(msg, "\frdemo \fs\fc%s\fS requires \fs\fc%s\fS version of %s", name, d.hdr.gamever<VERSION_GAME ? "an older" : "a newer", VERSION_NAME);
        }
        delete f;
        if(msg[0])
        {
            conoutf("%s", msg);
            demoinfos.pop();
            faildemos.add(newstring(name));
            return -1;
        }
        return num;
    }

    void adddemo()
    {
        if(!demotmp) return;
        int len = (int)min(demotmp->size(), stream::offset((G(demomaxsize)<<20) + 0x10000));
        demofile &d = demos.add();
        d.ctime = clocktime;
        d.data = new uchar[len];
        d.len = len;
        formatstring(d.info, "%s on %s", gamename(gamemode, mutators, 0, 32), smapname);
        srvoutf(4, "\fydemo \fs\fc%s\fS recorded \fs\fc%s UTC\fS [\fs\fw%.2f%s\fS]", d.info, gettime(d.ctime, "%Y-%m-%d %H:%M.%S"), d.len > 1024*1024 ? d.len/(1024*1024.f) : d.len/1024.0f, d.len > 1024*1024 ? "MB" : "kB");
        demotmp->seek(0, SEEK_SET);
        demotmp->read(d.data, len);
        DELETEP(demotmp);
        if(G(demoautoserversave))
        {
            string dafilepath = "";
            if(*filetimeformat) formatstring(dafilepath, "demos/sv_%s_%s-%s.dmo", gettime(d.ctime, filetimeformat), gamename(gamemode, mutators, 1, 32, "_"), smapname);
            else formatstring(dafilepath, "demos/sv_%u_%s-%s.dmo", uint(d.ctime), gamename(gamemode, mutators, 1, 32, "_"), smapname);
            stream *dafile = openrawfile(dafilepath, "w");
            dafile->write(d.data, d.len);
            dafile->close();
            DELETEP(dafile);
        }
        if(G(demoserverkeeptime))
        {
            vector<char *> files;
            listfiles("demos", "dmo", files);
            loopvrev(files) if(!strncmp(files[i], "sv_", 3))
            {
                defformatstring(dirfile, "demos/%s.dmo", files[i]);
                int q = scandemo(dirfile);
                if(q >= 0 && (clocktime-demoinfos[q].hdr.starttime) >= G(demoserverkeeptime))
                {
                    const char *fullfile = findfile(dirfile, "r");
                    if(fullfile && *fullfile && !unlink(fullfile))
                    {
                        conoutf("deleted old demo: %s", files[i]);
                        demoinfos.remove(q);
                    }
                }
            }
        }
    }

    void enddemorecord(bool full)
    {
        if(!demorecord) return;
        DELETEP(demorecord);
        if(!demotmp) return;
        if(!full && !G(demokeep)) { DELETEP(demotmp); }
        else
        {
            prunedemos(1);
            adddemo();
        }
    }

    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord) return;
        int stamp[3] = { gamemillis, chan, len };
        lilswap(stamp, 3);
        demorecord->write(stamp, sizeof(stamp));
        demorecord->write(data, len);
        if(demorecord->rawtell() >= (G(demomaxsize)<<20)) enddemorecord(!gs_playing(gamestate));
    }

    void recordpacket(int chan, void *data, int len)
    {
        writedemo(chan, data, len);
    }

    void setupdemorecord()
    {
        if(demorecord) enddemorecord(false);
        if(m_demo(gamemode) || m_edit(gamemode)) return;
        demonextmatch = false;

        demotmp = opentempfile("demorecord", "w+b");
        stream *f = opengzfile(NULL, "wb", demotmp);
        if(!f) { DELETEP(demotmp); return; }

        demorecord = f;

        demoheader hdr;
        memcpy(hdr.magic, VERSION_DEMOMAGIC, sizeof(hdr.magic));
        hdr.gamever = VERSION_GAME;
        hdr.gamemode = gamemode;
        hdr.mutators = mutators;
        hdr.starttime = clocktime;
        lilswap(&hdr.gamever, 4);
        copystring(hdr.mapname, smapname);
        demorecord->write(&hdr, sizeof(demoheader));

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        welcomepacket(p, NULL);
        writedemo(1, p.buf, p.len);
    }

    void endmatch()
    {
        setpause(false);
        checkdemorecord(true);
        setmod(sv_botoffset, 0);
        if(G(resetmmonend) >= 2) { mastermode = MM_OPEN; resetallows(); }
        if(G(resetvarsonend) >= 2) resetgamevars(true, false);
        if(G(resetbansonend) >= 2) resetbans();
        if(G(resetmutesonend) >= 2) resetmutes();
        if(G(resetlimitsonend) >= 2) resetlimits();
        if(G(resetexceptsonend) >= 2) resetexcepts();
    }

    bool checkvotes(bool force)
    {
        shouldcheckvotes = false;
        int style = gamestate == G_S_VOTING ? G(voteinterm) : G(votestyle);
        if(style == 3 && !force) return false;
        vector<votecount> votes;
        int maxvotes = 0;
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->state.actortype > A_PLAYER) continue;
            maxvotes++;
            if(!oi->mapvote[0]) continue;
            if(style == 3) votes.add(votecount(oi->mapvote, oi->modevote, oi->mutsvote));
            else
            {
                votecount *vc = NULL;
                loopvj(votes) if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote == votes[j].mode && oi->mutsvote == votes[j].muts)
                {
                    vc = &votes[j];
                    break;
                }
                if(!vc) vc = &votes.add(votecount(oi->mapvote, oi->modevote, oi->mutsvote));
                vc->count++;
            }
        }

        votecount *best = NULL;
        bool passed = force;
        if(style == 3) best = !votes.empty() ? &votes[rnd(votes.length())] : NULL;
        else
        {
            int morethanone = 0;
            loopv(votes) if(!best || votes[i].count >= best->count)
            {
                if(best && votes[i].count == best->count) morethanone++;
                else morethanone = 0;
                best = &votes[i];
            }
            if(force && morethanone)
            {
                int r = rnd(morethanone+1), n = 0;
                loopv(votes) if(votes[i].count == best->count)
                {
                    if(n != r) n++;
                    else { best = &votes[i]; break; }
                }
            }
            if(!passed && best) switch(style)
            {
                case 2: passed = best->count >= maxvotes; break;
                case 1: passed = best->count >= maxvotes*G(votethreshold); break;
                case 0: default: break;
            }
        }
        if(passed)
        {
            endmatch();
            if(best)
            {
                srvoutf(-3, "vote passed: \fs\fy%s\fS on \fs\fo%s\fS", gamename(best->mode, best->muts), best->map);
                sendf(-1, 1, "risi3", N_MAPCHANGE, best->map, 0, best->mode, best->muts);
                changemap(best->map, best->mode, best->muts);
            }
            else
            {
                int mode = G(rotatemode) ? -1 : gamemode, muts = G(rotatemuts) ? -1 : mutators;
                changemode(mode, muts);
                const char *map = choosemap(smapname, mode, muts);
                srvoutf(-3, "server chooses: \fs\fy%s\fS on \fs\fo%s\fS", gamename(mode, muts), map);
                sendf(-1, 1, "risi3", N_MAPCHANGE, map, 0, mode, muts);
                changemap(map, mode, muts);
            }
            return true;
        }
        return false;
    }

    bool mutscmp(int req, int limit)
    {
        if(req)
        {
            if(!limit) return false;
            loopi(G_M_NUM) if(req&(1<<i) && !(limit&(1<<i))) return false;
        }
        return true;
    }

    void vote(const char *reqmap, int &reqmode, int &reqmuts, int sender)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        reqmuts |= G(mutslockforce);
        modecheck(reqmode, reqmuts);
        if(!ci || !m_game(reqmode) || !reqmap || !*reqmap) return;
        if(m_local(reqmode) && !ci->local)
        {
            srvmsgft(ci->clientnum, CON_EVENT, "\fraccess denied, you must be a local client to start a %s game", gametype[reqmode].name);
            return;
        }
        bool hasvote = false, hasveto = (mastermode == MM_VETO && haspriv(ci, G(vetolock))) || !numclients(ci->clientnum);
        if(!hasveto)
        {
            if(ci->lastvote && totalmillis-ci->lastvote <= G(votewait)) return;
            if(ci->modevote == reqmode && ci->mutsvote == reqmuts && !strcmp(ci->mapvote, reqmap)) return;
        }
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->state.actortype > A_PLAYER || !oi->mapvote[0] || ci == oi) continue;
            if(!strcmp(oi->mapvote, reqmap) && oi->modevote == reqmode && oi->mutsvote == reqmuts)
            {
                hasvote = true;
                break;
            }
        }
        if(!hasvote)
        {
            if(G(votelock)) switch(G(votelocktype))
            {
                case 1: if(!haspriv(ci, G(votelock), "vote for a new game")) return; break;
                case 2:
                    if(!m_edit(reqmode))
                    {
                        int n = listincludes(sv_previousmaps, reqmap, strlen(reqmap));
                        if(n >= 0 && n < G(maphistory) && !haspriv(ci, G(votelock), "vote for a recently played map")) return;
                    }
                    break;
                case 0: default: break;
            }
            if(G(modelock)) switch(G(modelocktype))
            {
                case 1: if(!haspriv(ci, G(modelock), "change game modes")) return; break;
                case 2: if((!((1<<reqmode)&G(modelockfilter)) || !mutscmp(reqmuts, G(mutslockfilter))) && !haspriv(ci, G(modelock), "change to a locked game mode")) return; break;
                case 0: default: break;
            }
            if(reqmode != G_EDITMODE && G(mapslock))
            {
                char *list = NULL;
                switch(G(mapslocktype))
                {
                    case 1:
                    {
                        list = newstring(G(allowmaps));
                        mapcull(list, reqmode, reqmuts, numclients(), G(mapsfilter), true);
                        break;
                    }
                    case 2:
                    {
                        maplist(list, reqmode, reqmuts, numclients(), G(mapsfilter), true);
                        break;
                    }
                    case 0: default: break;
                }
                if(list)
                {
                    if(listincludes(list, reqmap, strlen(reqmap)) < 0 && !haspriv(ci, G(modelock), "select maps not in the rotation"))
                    {
                        DELETEA(list);
                        return;
                    }
                    DELETEA(list);
                }
            }
        }
        copystring(ci->mapvote, reqmap);
        ci->modevote = reqmode;
        ci->mutsvote = reqmuts;
        ci->lastvote = totalmillis;
        if(hasveto)
        {
            endmatch();
            srvoutf(-3, "%s forced: \fs\fy%s\fS on \fs\fo%s\fS", colourname(ci), gamename(ci->modevote, ci->mutsvote), ci->mapvote);
            sendf(-1, 1, "risi3", N_MAPCHANGE, ci->mapvote, 0, ci->modevote, ci->mutsvote);
            changemap(ci->mapvote, ci->modevote, ci->mutsvote);
            return;
        }
        sendf(-1, 1, "ri2si2", N_MAPVOTE, ci->clientnum, ci->mapvote, ci->modevote, ci->mutsvote);
        relayf(3, "%s suggests: \fs\fy%s\fS on \fs\fo%s\fS", colourname(ci), gamename(ci->modevote, ci->mutsvote), ci->mapvote);
        checkvotes();
    }

    bool scorecmp(clientinfo *ci, uint ip, const char *name, const char *handle, uint clientip)
    {
        if(ci->handle[0] && !strcmp(handle, ci->handle)) return true;
        if(ip && clientip == ip && !strcmp(name, ci->name)) return true;
        return false;
    }

    savedscore *findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip && !ci->handle[0]) return NULL;
        if(!insert) loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->clientnum != ci->clientnum && scorecmp(ci, ip, oi->name, oi->handle, getclientip(oi->clientnum)))
            {
                oi->state.updatetimeplayed();
                static savedscore curscore;
                curscore.save(oi->state);
                return &curscore;
            }
        }
        loopv(savedscores)
        {
            savedscore &sc = savedscores[i];
            if(scorecmp(ci, ip, sc.name, sc.handle, sc.ip)) return &sc;
        }
        if(!insert) return NULL;
        savedscore &sc = savedscores.add();
        copystring(sc.name, ci->name);
        copystring(sc.handle, ci->handle);
        sc.ip = ip;
        return &sc;
    }

    void givepoints(clientinfo *ci, int points, bool team = true)
    {
        ci->state.score += points;
        ci->state.points += points;
        sendf(-1, 1, "ri4", N_POINTS, ci->clientnum, points, ci->state.points);
        if(team && m_team(gamemode, mutators) && m_dm(gamemode))
        {
            score &ts = teamscore(ci->team);
            ts.total += points;
            sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
        }
    }

    void savescore(clientinfo *ci)
    {
        ci->state.updatetimeplayed(false);
        savedscore *sc = findscore(ci, true);
        if(sc)
        {
            if(ci->state.actortype == A_PLAYER && m_dm(gamemode) && m_team(gamemode, mutators) && !m_nopoints(gamemode, mutators) && G(teamkillrestore) && canplay())
            {
                int restorepoints[T_MAX] = {0};
                loopv(ci->state.teamkills) restorepoints[ci->state.teamkills[i].team] += ci->state.teamkills[i].points;
                loopi(T_MAX) if(restorepoints[i] >= G(teamkillrestore))
                {
                    score &ts = teamscore(i);
                    ts.total += restorepoints[i];
                    sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                }
            }
            sc->save(ci->state);
        }
    }

    void swapteam(clientinfo *ci, int oldteam, int newteam = T_NEUTRAL, bool swaps = true)
    {
        if(ci->swapteam && (!newteam || ci->swapteam == newteam)) ci->swapteam = T_NEUTRAL;
        if(!swaps || ci->state.actortype != A_PLAYER || !oldteam || oldteam == newteam || !m_swapteam(gamemode, mutators)) return;
        loopv(clients) if(clients[i] && clients[i] != ci)
        {
            clientinfo *cp = clients[i];
            if(cp->state.actortype != A_PLAYER || (newteam && cp->team != newteam) || !cp->swapteam || cp->swapteam != oldteam) continue;
            setteam(cp, oldteam, TT_RESET|TT_INFOSM, false);
            cp->state.lastdeath = 0;
            ancmsgft(cp->clientnum, S_V_BALALERT, CON_EVENT, "\fyyou have been moved to %s as previously requested", colourteam(oldteam));
            return;
        }
        if(haspriv(ci, G(teambalancelock)))
        {
            int worst = -1;
            float csk = ci->state.scoretime(), wsk = 0;
            loopv(clients) if(clients[i] && clients[i] != ci)
            {
                clientinfo *cp = clients[i];
                if(cp->state.actortype != A_PLAYER || (newteam && cp->team != newteam)) continue;
                float psk = cp->state.scoretime();
                if(psk > csk || worst < 0 || psk > wsk) continue;
                worst = i;
                wsk = psk;
            }
            if(worst >= 0)
            {
                clientinfo *cp = clients[worst];
                setteam(cp, oldteam, TT_RESET|TT_INFOSM, false);
                cp->state.lastdeath = 0;
                ancmsgft(cp->clientnum, S_V_BALALERT, CON_EVENT, "\fyyou have been moved to %s by higher skilled player %s", colourteam(oldteam), colourname(ci));
                return;
            }
        }
    }

    void setteam(clientinfo *ci, int team, int flags, bool swaps)
    {
        swapteam(ci, ci->team, team, swaps);
        if(ci->team != team)
        {
            bool reenter = false;
            if(flags&TT_RESET) waiting(ci, DROP_WEAPONS, false);
            else if(flags&TT_SMODE && ci->state.state == CS_ALIVE)
            {
                if(smode) smode->leavegame(ci);
                mutate(smuts, mut->leavegame(ci));
                reenter = true;
            }
            ci->lastteam = ci->team;
            ci->team = team;
            if(reenter)
            {
                if(smode) smode->entergame(ci);
                mutate(smuts, mut->entergame(ci));
            }
            if(ci->state.actortype == A_PLAYER && ci->isready()) aiman::poke();
        }
        if(flags&TT_INFO) sendf(-1, 1, "ri3", N_SETTEAM, ci->clientnum, ci->team);
    }

    struct teamcheck
    {
        int team;
        float score;
        int clients;

        teamcheck() : team(T_NEUTRAL), score(0.f), clients(0) {}
        teamcheck(int n) : team(n), score(0.f), clients(0) {}
        teamcheck(int n, float r) : team(n), score(r), clients(0) {}
        teamcheck(int n, int s) : team(n), score(s), clients(0) {}

        ~teamcheck() {}
    };

    bool allowteam(clientinfo *ci, int team, int first = T_FIRST, bool check = true)
    {
        if(isteam(gamemode, mutators, team, first))
        {
            if(!m_coop(gamemode, mutators))
            {
                if(check && m_balteam(gamemode, mutators, 3) && team != chooseteam(ci, team)) return false;
                return true;
            }
            else if(ci->state.actortype >= A_BOT) return team != mapbals[curbalance][0];
            else return team == mapbals[curbalance][0];
        }
        return false;
    }

    int chooseteam(clientinfo *ci, int suggest, bool wantbal)
    {
        if(ci->state.actortype >= A_ENEMY) return T_ENEMY;
        else if(m_play(gamemode) && m_team(gamemode, mutators) && ci->state.state != CS_SPECTATOR && ci->state.state != CS_EDITING)
        {
            bool human = ci->state.actortype == A_PLAYER;
            int team = -1, bal = human && !wantbal && (G(teambalance) != 6 || !gs_playing(gamestate)) ? G(teambalance) : 1;
            if(human)
            {
                if(m_coop(gamemode, mutators)) return mapbals[curbalance][0];
                int teams[3][3] = {
                    { suggest, ci->team, -1 },
                    { suggest, ci->team, ci->lastteam },
                    { suggest, ci->lastteam, ci->team }
                };
                loopi(3) if(allowteam(ci, teams[G(teampersist)][i], T_FIRST, false))
                {
                    team = teams[G(teampersist)][i];
                    if(bal <= 2 && G(teampersist) == 2) return team;
                    break;
                }
            }
            teamcheck teamchecks[T_TOTAL];
            loopk(T_TOTAL) teamchecks[k].team = T_FIRST+k;
            loopv(clients) if(clients[i] != ci)
            {
                clientinfo *cp = clients[i];
                if(!cp->team || cp->state.state == CS_SPECTATOR) continue;
                if((cp->state.actortype > A_PLAYER && cp->state.ownernum < 0) || cp->state.actortype >= A_ENEMY) continue;
                teamcheck &ts = teamchecks[cp->team-T_FIRST];
                if(team > 0 && m_swapteam(gamemode, mutators) && ci->state.actortype == A_PLAYER && cp->state.actortype == A_PLAYER && cp->swapteam && ci->team == cp->swapteam && cp->team == team)
                    return team; // swapteam
                if(ci->state.actortype > A_PLAYER || (ci->state.actortype == A_PLAYER && cp->state.actortype == A_PLAYER))
                { // remember: ai just balance teams
                    ts.score += cp->state.scoretime();
                    ts.clients++;
                }
            }
            if(bal || team <= 0) loopj(team > 0 ? 2 : 1)
            {
                teamcheck *worst = NULL;
                loopi(numteams(gamemode, mutators)) if(allowteam(ci, teamchecks[i].team, T_FIRST, false))
                {
                    teamcheck &ts = teamchecks[i];
                    switch(bal)
                    {
                        case 2: case 5: case 6:
                        {
                            if(!worst || (team > 0 && ts.team == team && ts.score <= worst->score) || ts.score < worst->score || ((team <= 0 || worst->team != team) && ts.score == worst->score && ts.clients < worst->clients))
                                worst = &ts;
                            break;
                        }
                        case 1: case 3: case 4: default:
                        {
                            if(!worst || (team > 0 && ts.team == team && ts.clients <= worst->clients) || ts.clients < worst->clients || ((team <= 0 || worst->team != team) && ts.clients == worst->clients && ts.score < worst->score))
                                worst = &ts;
                            break;
                        }
                    }
                }
                if(worst)
                {
                    team = worst->team;
                    break;
                }
                team = -1;
            }
            return allowteam(ci, team, T_FIRST, false) ? team : T_ALPHA;
        }
        return T_NEUTRAL;
    }

    void stopdemo()
    {
        if(m_demo(gamemode)) enddemoplayback();
        else checkdemorecord(!gs_playing(gamestate));
    }

    void connected(clientinfo *ci);

    #include "auth.h"

    enum { ALST_TRY = 0, ALST_SPAWN, ALST_SPEC, ALST_EDIT, ALST_WALK, ALST_MAX };

    extern bool getmap(clientinfo *ci = NULL, bool force = false);

    bool crclocked(clientinfo *ci, bool msg = false)
    {
        if(m_play(gamemode) && G(crclock) && ci->state.actortype == A_PLAYER && (mapcrc ? ci->mapcrc != mapcrc : !ci->mapcrc) && !haspriv(ci, G(crclock)))
        {
            if(msg) srvmsgft(ci->clientnum, CON_EVENT, "\fyyou are \fs\fccrc locked\fS, you need the correct map version..");
            return true;
        }
        return false;
    }

    void spectator(clientinfo *ci, bool quarantine = false, int sender = -1)
    {
        if(!ci || ci->state.actortype > A_PLAYER) return;
        ci->state.state = CS_SPECTATOR;
        ci->state.quarantine = quarantine;
        sendf(sender, 1, "ri3", N_SPECTATOR, ci->clientnum, quarantine ? 2 : 1);
        setteam(ci, T_NEUTRAL, TT_INFOSM);
    }

    bool spectate(clientinfo *ci, bool val, bool quarantine = false)
    {
        if(ci->state.state != CS_SPECTATOR && val)
        {
            if(ci->state.state == CS_ALIVE)
            {
                suicideevent ev;
                ev.flags = HIT_SPEC;
                ev.process(ci);
            }
            if(smode) smode->leavegame(ci);
            mutate(smuts, mut->leavegame(ci));
            sendf(-1, 1, "ri3", N_SPECTATOR, ci->clientnum, quarantine ? 2 : 1);
            ci->state.state = CS_SPECTATOR;
            ci->state.quarantine = quarantine;
            ci->state.updatetimeplayed(false);
            setteam(ci, T_NEUTRAL, TT_INFO);
            if(ci->isready()) aiman::poke();
        }
        else if(ci->state.state == CS_SPECTATOR && !val)
        {
            if(crclocked(ci, true))
            {
                getmap(ci);
                return false;
            }
            int nospawn = 0;
            if(smode && !smode->canspawn(ci, true)) { nospawn++; }
            mutate(smuts, if(!mut->canspawn(ci, true)) { nospawn++; });
            ci->state.state = CS_DEAD;
            if(nospawn)
            {
                spectate(ci, true);
                return false;
            }
            ci->state.lasttimeplayed = totalmillis;
            ci->state.lasttimealive = totalmillis;
            ci->state.lasttimewielded = totalmillis;
            loopi(W_MAX) ci->state.lasttimeloadout[i] = totalmillis;
            ci->state.quarantine = false;
            waiting(ci, DROP_RESET);
            if(smode) smode->entergame(ci);
            mutate(smuts, mut->entergame(ci));
            if(ci->isready()) aiman::poke();
            ci->state.updatetimeplayed();
        }
        return true;
    }

    struct mapcrcs
    {
        uint id;
        vector<clientinfo *> clients;
        mapcrcs() {}
        mapcrcs(uint n, clientinfo *m) { id = n; clients.add(m); }
        ~mapcrcs() { clients.setsize(0); }
    };

    void resetmapdata()
    {
        mapcrc = 0;
        mapsending = -1;
        loopi(SENDMAP_MAX) if(mapdata[i]) DELETEP(mapdata[i]);
    }

    bool hasmapdata()
    {
        if(!mapcrc) return false;
        loopi(SENDMAP_HAS) if(!mapdata[i]) return false;
        return true;
    }

    bool getmap(clientinfo *ci, bool force)
    {
        if(gamestate == G_S_INTERMISSION || gamestate == G_S_VOTING || (m_edit(gamemode) && numclients() <= 1)) return false; // pointless
        if(ci)
        {
            if(mapsending == ci->clientnum) resetmapdata();
            ci->mapcrc = 0;
            ci->wantsmap = true;
            if(hasmapdata())
            {
                if(ci->gettingmap)
                {
                    srvmsgft(ci->clientnum, CON_EVENT, "\fyalready in the process of sending you the map..");
                    return true;
                }
                ci->gettingmap = true;
                srvmsgft(ci->clientnum, CON_EVENT, "\fysending you the map, please wait..");
                loopi(SENDMAP_MAX) if(mapdata[i]) sendfile(ci->clientnum, 2, mapdata[i], "ri2", N_SENDMAPFILE, i);
                sendwelcome(ci);
                ci->needclipboard = totalmillis ? totalmillis : 1;
                return true;
            }
            if(mapsending >= 0)
            {
                srvmsgft(ci->clientnum, CON_EVENT, "\fythe map is being uploaded, please wait..");
                return true;
            }
        }
        if((!force && gamestate == G_S_WAITING) || mapsending >= 0 || hasmapdata() || numclients() <= 1) return false;
        clientinfo *best = NULL;
        if(!m_edit(gamemode))
        {
            vector<mapcrcs> crcs;
            loopv(clients)
            {
                clientinfo *cs = clients[i];
                if(cs->state.actortype > A_PLAYER || !cs->name[0] || !cs->online || cs->wantsmap || !cs->mapcrc || !cs->ready) continue;
                bool found = false;
                loopvj(crcs) if(crcs[j].id == cs->mapcrc)
                {
                    crcs[j].clients.add(cs);
                    found = true;
                    break;
                }
                if(!found) crcs.add(mapcrcs(cs->mapcrc, cs));
            }
            int n = -1;
            loopv(crcs) if(n < 0 || crcs[n].clients.length() < crcs[i].clients.length()) n = i;
            if(n > 0) loopv(crcs[n].clients)
            {
                clientinfo *cs = crcs[n].clients[i];
                cs->state.updatetimeplayed();
                if(!best || cs->state.timeplayed > best->state.timeplayed) best = cs;
            }
        }
        if(!best) loopv(clients)
        {
            clientinfo *cs = clients[i];
            if(cs->state.actortype > A_PLAYER || !cs->name[0] || !cs->online || cs->wantsmap || !cs->ready) continue;
            cs->state.updatetimeplayed();
            if(!best || cs->state.timeplayed > best->state.timeplayed) best = cs;
        }
        if(best)
        {
            mapsending = best->clientnum;
            mapcrc = best->mapcrc;
            srvoutf(4, "\fythe map crc \fs\fc0x%.8x\fS is being requested from %s..", mapcrc, colourname(best));
            sendf(best->clientnum, 1, "ri", N_GETMAP);
            loopv(clients)
            {
                clientinfo *cs = clients[i];
                if(cs->state.actortype > A_PLAYER || !cs->name[0] || !cs->online || !cs->ready) continue;
                if(cs->wantsmap || crclocked(cs, true))
                {
                    cs->mapcrc = 0;
                    cs->wantsmap = true;
                    spectate(cs, true);
                }
            }
            return true;
        }
        if(ci) srvmsgft(ci->clientnum, CON_EVENT, "\fysorry, unable to get a valid map..");
        sendf(-1, 1, "ri", N_FAILMAP);
        return false;
    }

    bool allowstate(clientinfo *ci, int n, int lock = -1)
    {
        if(!ci) return false;
        uint ip = getclientip(ci->clientnum);
        switch(n)
        {
            case ALST_TRY: // try spawn
            {
                if(ci->state.quarantine) return false;
                if(ci->state.actortype == A_PLAYER)
                    if(mastermode >= MM_LOCKED && ip && !checkipinfo(control, ipinfo::ALLOW, ip) && !haspriv(ci, lock, "spawn"))
                        return false;
                if(ci->state.state == CS_ALIVE || ci->state.state == CS_WAITING) return false;
                if(ci->state.lastdeath && gamemillis-ci->state.lastdeath <= DEATHMILLIS) return false;
                if(crclocked(ci, true))
                {
                    getmap(ci);
                    return false;
                }
                break;
            }
            case ALST_SPAWN: // spawn
            {
                if(ci->state.quarantine) return false;
                if(ci->state.state != CS_DEAD && ci->state.state != CS_WAITING) return false;
                if(ci->state.lastdeath && gamemillis-ci->state.lastdeath <= DEATHMILLIS) return false;
                if(crclocked(ci, true))
                {
                    getmap(ci);
                    return false;
                }
                break;
            }
            case ALST_SPEC: return ci->state.actortype == A_PLAYER; // spec
            case ALST_WALK: if(ci->state.state != CS_EDITING) return false;
            case ALST_EDIT: // edit on/off
            {
                if(ci->state.quarantine || ci->state.actortype != A_PLAYER || !m_edit(gamemode)) return false;
                if(mastermode >= MM_LOCKED && ip && !checkipinfo(control, ipinfo::ALLOW, ip) && !haspriv(ci, lock, "edit")) return false;
                break;
            }
            default: break;
        }
        return true;
    }

    void sendstats()
    {
        if(G(serverstats) && auth::hasstats)
        {
            requestmasterf("stats begin\n");
            requestmasterf("stats game \"%s\" %d %d %d\n", escapestring(smapname), gamemode, mutators, gamemillis);
            requestmasterf("stats server \"%s\" %s %d\n", escapestring(G(serverdesc)), versionstring, serverport);
            flushmasteroutput();
            loopi(numteams(gamemode, mutators))
            {
                int tp = m_team(gamemode, mutators) ? T_FIRST : T_NEUTRAL;
                requestmasterf("stats team %d %d \"%s\"\n", i + tp, teamscore(i + tp).total, escapestring(TEAM(i + tp, name)));
            }
            flushmasteroutput();
            loopv(clients) if(clients[i]->state.actortype == A_PLAYER) savescore(clients[i]);
            loopv(savedscores) if(savedscores[i].actortype == A_PLAYER)
            {
                requestmasterf("stats player \"%s\" \"%s\" %d %d %d %d %d\n",
                    escapestring(savedscores[i].name), escapestring(savedscores[i].handle),
                    m_laptime(gamemode, mutators) ? savedscores[i].cptime : savedscores[i].score,
                    savedscores[i].timealive, savedscores[i].frags, savedscores[i].deaths, i
                );
                flushmasteroutput();
                loopj(W_MAX)
                {
                    weaponstats w = savedscores[i].weapstats[j];
                    requestmasterf("stats weapon %d \"%s\" %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                        i, escapestring(savedscores[i].handle), weaptype[j].name, w.timewielded, w.timeloadout,
                        w.damage1, w.frags1, w.hits1, w.flakhits1, w.shots1, w.flakshots1,
                        w.damage2, w.frags2, w.hits2, w.flakhits2, w.shots2, w.flakshots2
                    );
                    flushmasteroutput();
                }
            }
            requestmasterf("stats end\n");
            flushmasteroutput();
        }
    }

    #include "capturemode.h"
    #include "defendmode.h"
    #include "bombermode.h"
    #include "duelmut.h"
    #include "aiman.h"

    void changemap(const char *name, int mode, int muts)
    {
        hasgameinfo = shouldcheckvotes = firstblood = false;
        stopdemo();
        resetmapdata();
        changemode(gamemode = mode, mutators = muts);
        curbalance = nextbalance = lastteambalance = nextteambalance = gamemillis = gamewaitstate = gamewaitstart = 0;
        gamestate = m_play(gamemode) ? G_S_WAITING : G_S_PLAYING;
        gamewaittime = m_play(gamemode) ? totalmillis+G(waitforplayerload) : 0;
        oldtimelimit = m_play(gamemode) && G(timelimit) ? G(timelimit) : -1;
        timeremaining = m_play(gamemode) && G(timelimit) ? G(timelimit)*60 : -1;
        gamelimit = m_play(gamemode) && G(timelimit) ? timeremaining*1000 : 0;
        sents.shrink(0);
        scores.shrink(0);
        loopv(savedscores) savedscores[i].mapchange();
        setuptriggers(false);
        setupspawns(false);
        if(smode) smode->reset();
        mutate(smuts, mut->reset());
        aiman::clearai();
        aiman::poke();
        const char *reqmap = name && *name ? name : pickmap(NULL, gamemode, mutators);
        ifserver(reqmap && *reqmap)
        {
            loopi(SENDMAP_MAX)
            {
                defformatstring(reqfile, strstr(reqmap, "maps/")==reqmap || strstr(reqmap, "maps\\")==reqmap ? "%s.%s" : "maps/%s.%s", reqmap, sendmaptypes[i]);
                if(i == SENDMAP_MPZ)
                {
                    stream *f = opengzfile(reqfile, "rb");
                    if(f)
                    {
                        f->seek(-1, SEEK_END);
                        mapcrc = f->getcrc();
                        DELETEP(f);
                    }
                }
                mapdata[i] = openfile(reqfile, "rb");
            }
            if(!hasmapdata()) resetmapdata();
        }
        copystring(smapname, reqmap);

        // server modes
        if(m_capture(gamemode)) smode = &capturemode;
        else if(m_defend(gamemode)) smode = &defendmode;
        else if(m_bomber(gamemode)) smode = &bombermode;
        else smode = NULL;
        smuts.shrink(0);
        smuts.add(&spawnmutator);
        if(m_duke(gamemode, mutators)) smuts.add(&duelmutator);
        if(m_vampire(gamemode, mutators)) smuts.add(&vampiremutator);
        if(smode) smode->reset();
        mutate(smuts, mut->reset());

        if(m_local(gamemode)) kicknonlocalclients(DISC_PRIVATE);

        loopv(clients)
        {
            clients[i]->mapchange(true);
            spectator(clients[i]);
        }

        if(!demoplayback && m_play(gamemode) && numclients())
        {
            vector<char> buf;
            buf.put(smapname, strlen(smapname));
            if(*sv_previousmaps && G(maphistory))
            {
                vector<char *> prev;
                explodelist(sv_previousmaps, prev);
                loopvrev(prev) if(!strcmp(prev[i], smapname))
                {
                    delete[] prev[i];
                    prev.remove(i);
                }
                while(prev.length() >= G(maphistory))
                {
                    int last = prev.length()-1;
                    delete[] prev[last];
                    prev.remove(last);
                }
                loopv(prev)
                {
                    buf.add(' ');
                    buf.put(prev[i], strlen(prev[i]));
                }
                prev.deletearrays();
            }
            buf.add(0);
            const char *str = buf.getbuf();
            if(*str) setmods(sv_previousmaps, str);
        }
        else setmods(sv_previousmaps, "");

        if(numclients())
        {
            sendtick();
            if(m_demo(gamemode)) setupdemoplayback();
            else if(demonextmatch) setupdemorecord();
        }
    }

    void checkvar(ident *id, const char *arg)
    {
        if(id && id->flags&IDF_SERVER && !(id->flags&IDF_READONLY) && !(id->flags&IDF_ADMIN) && !(id->flags&IDF_WORLD)) switch(id->type)
        {
            case ID_VAR:
            {
                int ret = parseint(arg);
                if(*id->storage.i == id->bin.i) { if(ret != id->bin.i) numgamemods++; }
                else if(ret == id->bin.i) numgamemods--;
                break;
            }
            case ID_FVAR:
            {
                int ret = parsefloat(arg);
                if(*id->storage.f == id->bin.f) { if(ret != id->bin.f) numgamemods++; }
                else if(ret == id->bin.f) numgamemods--;
                break;
            }
            case ID_SVAR:
            {
                if(!strcmp(*id->storage.s, id->bin.s)) { if(strcmp(arg, id->bin.s)) numgamemods++; }
                else if(!strcmp(arg, id->bin.s)) numgamemods--;
                break;
            }
            default: break;
        }
    }

    bool servcmd(int nargs, const char *cmd, const char *arg)
    { // incoming commands
#ifndef STANDALONE
        if(::connected(false, false)) return false;
#endif
        ident *id = idents.access(cmd);
        if(id && id->flags&IDF_SERVER)
        {
            const char *val = NULL;
            switch(id->type)
            {
                case ID_COMMAND:
                {
                    int slen = strlen(id->name);
                    if(arg && nargs > 1) slen += strlen(arg)+1;
                    char *s = newstring(slen);
                    if(nargs <= 1 || !arg) nformatstring(s, slen+1, "%s", id->name);
                    else nformatstring(s, slen+1, "%s %s", id->name, arg);
                    char *ret = executestr(s);
                    conoutft(CON_MESG, "\fy\fs\fc%s\fS returned \fs\fc%s\fS", id->name, ret && *ret ? ret : "failed");
                    delete[] s;
                    delete[] ret;
                    return true;
                }
                case ID_VAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        conoutft(CON_MESG, id->flags&IDF_HEX && *id->storage.i >= 0 ? (id->maxval==0xFFFFFF ? "\fy%s = 0x%.6X" : "\fy%s = 0x%X") : "\fy%s = %d", id->name, *id->storage.i);
                        return true;
                    }
                    if(id->maxval < id->minval || id->flags&IDF_READONLY)
                    {
                        conoutft(CON_MESG, "\frcannot override variable: %s", id->name);
                        return true;
                    }
                    int ret = parseint(arg);
                    if(ret < id->minval || ret > id->maxval)
                    {
                        conoutft(CON_MESG,
                            id->flags&IDF_HEX ?
                                    (id->minval <= 255 ? "\frvalid range for %s is %d..0x%X" : "\frvalid range for %s is 0x%X..0x%X") :
                                    "\frvalid range for %s is %d..%d", id->name, id->minval, id->maxval);
                        return true;
                    }
                    if(versioning)
                    {
                        id->def.i = ret;
                        if(versioning == 2) id->bin.i = ret;
                    }
                    checkvar(id, arg);
                    *id->storage.i = ret;
                    id->changed();
#ifndef STANDALONE
                    if(versioning) setvar(&id->name[3], ret, true);
#endif
                    val = intstr(id);
                    break;
                }
                case ID_FVAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        conoutft(CON_MESG, "\fy%s = %s", id->name, floatstr(*id->storage.f));
                        return true;
                    }
                    if(id->maxvalf < id->minvalf || id->flags&IDF_READONLY)
                    {
                        conoutft(CON_MESG, "\frcannot override variable: %s", id->name);
                        return true;
                    }
                    float ret = parsefloat(arg);
                    if(ret < id->minvalf || ret > id->maxvalf)
                    {
                        conoutft(CON_MESG, "\frvalid range for %s is %s..%s", id->name, floatstr(id->minvalf), floatstr(id->maxvalf));
                        return true;
                    }
                    if(versioning)
                    {
                        id->def.f = ret;
                        if(versioning == 2) id->bin.f = ret;
                    }
                    checkvar(id, arg);
                    *id->storage.f = ret;
                    id->changed();
#ifndef STANDALONE
                    if(versioning) setfvar(&id->name[3], ret, true);
#endif
                    val = floatstr(*id->storage.f);
                    break;
                }
                case ID_SVAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        conoutft(CON_MESG, strchr(*id->storage.s, '"') ? "\fy%s = [%s]" : "\fy%s = \"%s\"", id->name, *id->storage.s);
                        return true;
                    }
                    if(id->flags&IDF_READONLY)
                    {
                        conoutft(CON_MESG, "\frcannot override variable: %s", id->name);
                        return true;
                    }
                    if(versioning)
                    {
                        delete[] id->def.s;
                        id->def.s = newstring(arg);
                        if(versioning == 2)
                        {
                            delete[] id->bin.s;
                            id->bin.s = newstring(arg);
                        }
                    }
                    checkvar(id, arg);
                    delete[] *id->storage.s;
                    *id->storage.s = newstring(arg);
                    id->changed();
#ifndef STANDALONE
                    if(versioning) setsvar(&id->name[3], arg, true);
#endif
                    val = *id->storage.s;
                    break;
                }
                default: return false;
            }
            if(val)
            {
                sendf(-1, 1, "ri2sis", N_COMMAND, -1, &id->name[3], strlen(val), val);
                arg = val;
            }
            return true;
        }
        return false; // parse will spit out "unknown command" in this case
    }

    void parsecommand(clientinfo *ci, int nargs, const char *cmd, const char *arg)
    { // incoming commands from clients
        defformatstring(cmdname, "sv_%s", cmd);
        ident *id = idents.access(cmdname);
        if(id && id->flags&IDF_SERVER)
        {
            const char *name = &id->name[3], *val = NULL, *oldval = NULL;
            bool needfreeoldval = false;
            int locked = min(max(id->flags&IDF_ADMIN ? int(PRIV_ADMINISTRATOR) : 0, G(varslock)), int(PRIV_CREATOR));
            if(!strcmp(id->name, "sv_gamespeed") && G(gamespeedlock) > locked) locked = min(G(gamespeedlock), int(PRIV_CREATOR));
            else if(id->type == ID_VAR)
            {
                int len = strlen(id->name);
                if(len > 4 && !strcmp(&id->name[len-4], "lock"))
                    locked = min(max(max(*id->storage.i, parseint(arg)), locked), int(PRIV_CREATOR));
            }
#ifndef STANDALONE
            if(servertype < 3 && (!strcmp(id->name, "sv_gamespeed") || !strcmp(id->name, "sv_gamepaused"))) locked = PRIV_MAX;
#endif
            switch(id->type)
            {
                case ID_COMMAND:
                {
                    if(locked && !haspriv(ci, locked, "execute that command")) return;
                    int slen = strlen(id->name);
                    if(arg && nargs > 1) slen += strlen(arg)+1;
                    char *s = newstring(slen);
                    if(nargs <= 1 || !arg) nformatstring(s, slen+1, "%s", id->name);
                    else nformatstring(s, slen+1, "%s %s", id->name, arg);
                    char *ret = executestr(s);
                    srvoutf(-3, "\fy%s executed \fs\fc%s\fS (returned: \fs\fc%s\fS)", colourname(ci), name, ret && * ret ? ret : "failed");
                    delete[] s;
                    delete[] ret;
                    return;
                }
                case ID_VAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        srvmsgf(ci->clientnum, id->flags&IDF_HEX && *id->storage.i >= 0 ? (id->maxval==0xFFFFFF ? "\fy%s = 0x%.6X" : "\fy%s = 0x%X") : "\fy%s = %d", name, *id->storage.i);
                        return;
                    }
                    else if(locked && !haspriv(ci, locked, "change that variable"))
                    {
                        val = intstr(id);
                        sendf(ci->clientnum, 1, "ri2sis", N_COMMAND, -1, name, strlen(val), val);
                        return;
                    }
                    if(id->maxval < id->minval || id->flags&IDF_READONLY)
                    {
                        srvmsgf(ci->clientnum, "\frcannot override variable: %s", name);
                        return;
                    }
                    int ret = parseint(arg);
                    if(ret < id->minval || ret > id->maxval)
                    {
                        srvmsgf(ci->clientnum,
                            id->flags&IDF_HEX ?
                                (id->minval <= 255 ? "\frvalid range for %s is %d..0x%X" : "\frvalid range for %s is 0x%X..0x%X") :
                                "\frvalid range for %s is %d..%d", name, id->minval, id->maxval);
                        return;
                    }
                    checkvar(id, arg);
                    oldval = intstr(id);
                    *id->storage.i = ret;
                    id->changed();
                    val = intstr(id);
                    break;
                }
                case ID_FVAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        srvmsgf(ci->clientnum, "\fy%s = %s", name, floatstr(*id->storage.f));
                        return;
                    }
                    else if(locked && !haspriv(ci, locked, "change that variable"))
                    {
                        val = floatstr(*id->storage.f);
                        sendf(ci->clientnum, 1, "ri2sis", N_COMMAND, -1, name, strlen(val), val);
                        return;
                    }
                    if(id->maxvalf < id->minvalf || id->flags&IDF_READONLY)
                    {
                        srvmsgf(ci->clientnum, "\frcannot override variable: %s", name);
                        return;
                    }
                    float ret = parsefloat(arg);
                    if(ret < id->minvalf || ret > id->maxvalf)
                    {
                        srvmsgf(ci->clientnum, "\frvalid range for %s is %s..%s", name, floatstr(id->minvalf), floatstr(id->maxvalf));
                        return;
                    }
                    checkvar(id, arg);
                    oldval = floatstr(*id->storage.f);
                    *id->storage.f = ret;
                    id->changed();
                    val = floatstr(*id->storage.f);
                    break;
                }
                case ID_SVAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        srvmsgf(ci->clientnum, strchr(*id->storage.s, '"') ? "\fy%s = [%s]" : "\fy%s = \"%s\"", name, *id->storage.s);
                        return;
                    }
                    else if(locked && !haspriv(ci, locked, "change that variable"))
                    {
                        val = *id->storage.s;
                        sendf(ci->clientnum, 1, "ri2sis", N_COMMAND, -1, name, strlen(val), val);
                        return;
                    }
                    if(id->flags&IDF_READONLY)
                    {
                        srvmsgf(ci->clientnum, "\frcannot override variable: %s", name);
                        return;
                    }
                    checkvar(id, arg);
                    oldval = newstring(*id->storage.s);
                    needfreeoldval = true;
                    delete[] *id->storage.s;
                    *id->storage.s = newstring(arg);
                    id->changed();
                    val = *id->storage.s;
                    break;
                }
                default: return;
            }
            if(val)
            {
                sendf(-1, 1, "ri2sis", N_COMMAND, ci->clientnum, name, strlen(val), val);
                if(oldval)
                {
                    relayf(3, "\fy%s set %s to %s (was: %s)", colourname(ci), name, val, oldval);
                    if(needfreeoldval) delete[] oldval;
                }
                else relayf(3, "\fy%s set %s to %s", colourname(ci), name, val);
            }
        }
        else srvmsgf(ci->clientnum, "\frunknown command: %s", cmd);
    }

    bool rewritecommand(ident *id, tagval *args, int numargs)
    {
        bool found = false;
        const char *argstr = numargs > 2 ? conc(&args[1], numargs-1, true) : (numargs > 1 ? args[1].getstr() : "");
        if(id && id->flags&IDF_WORLD && identflags&IDF_WORLD) found = true;
        else if(id && id->flags&IDF_SERVER && id->type != ID_COMMAND) found = servcmd(numargs, args[0].s, argstr);
#ifndef STANDALONE
        else if(!id || id->flags&IDF_CLIENT) found = client::sendcmd(numargs, args[0].s, argstr);
#endif
        if(numargs > 2) delete[] (char *)argstr;
        return found;
    }

    void sendservinit(clientinfo *ci)
    {
        sendf(ci->clientnum, 1, "ri3ssi", N_SERVERINIT, ci->clientnum, VERSION_GAME, gethostname(ci->clientnum), gethostip(ci->clientnum), ci->sessionid);
    }

    bool restorescore(clientinfo *ci)
    {
        savedscore *sc = findscore(ci, false);
        if(sc)
        {
            sc->restore(ci->state);
            if(ci->state.actortype == A_PLAYER && m_dm(gamemode) && m_team(gamemode, mutators) && !m_nopoints(gamemode, mutators) && G(teamkillrestore) && canplay())
            {
                int restorepoints[T_MAX] = {0};
                loopv(ci->state.teamkills) restorepoints[ci->state.teamkills[i].team] += ci->state.teamkills[i].points;
                loopi(T_MAX) if(restorepoints[i] >= G(teamkillrestore))
                {
                    score &ts = teamscore(i);
                    ts.total -= restorepoints[i];
                    sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                }
            }
            return true;
        }
        return false;
    }

    void sendresume(clientinfo *ci)
    {
        servstate &gs = ci->state;
        sendf(-1, 1, "ri9vi", N_RESUME, ci->clientnum, gs.state, gs.points, gs.frags, gs.deaths, gs.health, gs.cptime, gs.weapselect, W_MAX, &gs.ammo[0], -1);
    }

    void putinitclient(clientinfo *ci, packetbuf &p)
    {
        if(ci->state.actortype > A_PLAYER)
        {
            if(ci->state.ownernum >= 0)
            {
                putint(p, N_INITAI);
                putint(p, ci->clientnum);
                putint(p, ci->state.ownernum);
                putint(p, ci->state.actortype);
                putint(p, ci->state.spawnpoint);
                putint(p, ci->state.skill);
                sendstring(ci->name, p);
                putint(p, ci->team);
                putint(p, ci->state.colour);
                putint(p, ci->state.model);
                sendstring(ci->state.vanity, p);
                putint(p, ci->state.loadweap.length());
                loopv(ci->state.loadweap) putint(p, ci->state.loadweap[i]);
            }
        }
        else
        {
            putint(p, N_CLIENTINIT);
            putint(p, ci->clientnum);
            putint(p, ci->state.colour);
            putint(p, ci->state.model);
            putint(p, ci->team);
            putint(p, ci->privilege);
            sendstring(ci->name, p);
            sendstring(ci->state.vanity, p);
            putint(p, ci->state.loadweap.length());
            loopv(ci->state.loadweap) putint(p, ci->state.loadweap[i]);
            sendstring(ci->handle, p);
            sendstring(gethostname(ci->clientnum), p);
            sendstring(gethostip(ci->clientnum), p);
            ci->state.version.put(p);
        }
    }

    void sendinitclient(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p);
        sendpacket(-1, 1, p.finalize(), ci->clientnum);
    }

    void sendinitclientself(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p);
        sendpacket(ci->clientnum, 1, p.finalize(), ci->clientnum);
    }

    void welcomeinitclient(packetbuf &p, int exclude = -1)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->connected || ci->clientnum == exclude) continue;
            putinitclient(ci, p);
        }
    }

    void sendwelcome(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        int chan = welcomepacket(p, ci);
        sendpacket(ci->clientnum, chan, p.finalize());
    }

    int welcomepacket(packetbuf &p, clientinfo *ci)
    {
        putint(p, N_WELCOME);
        putint(p, N_MAPCHANGE);
        sendstring(smapname, p);
        if(!ci) putint(p, 0);
        else if(!ci->online && m_edit(gamemode) && numclients(ci->clientnum) >= 2)
        {
            if(mapsending < 0) resetmapdata();
            getmap(ci);
            putint(p, 1); // already in progress
        }
        else
        {
            if(ci->online) putint(p, 2); // we got a temp map eh?
            else putint(p, ci->local ? -1 : 0);
        }
        putint(p, gamemode);
        putint(p, mutators);

        enumerate(idents, ident, id, {
            if(id.flags&IDF_SERVER && !(id.flags&IDF_WORLD)) // reset vars
            {
                const char *val = NULL;
                switch(id.type)
                {
                    case ID_VAR:
                    {
                        val = intstr(&id);
                        break;
                    }
                    case ID_FVAR:
                    {
                        val = floatstr(*id.storage.f);
                        break;
                    }
                    case ID_SVAR:
                    {
                        val = *id.storage.s;
                        break;
                    }
                    default: break;
                }
                if(val)
                {
                    putint(p, N_COMMAND);
                    putint(p, -1);
                    sendstring(&id.name[3], p);
                    putint(p, strlen(val));
                    sendstring(val, p);
                }
            }
        });

        if(!ci || (m_play(gamemode) && numclients()))
        {
            putint(p, N_TICK);
            putint(p, gamestate);
            putint(p, timeleft());
        }

        if(hasgameinfo)
        {
            putint(p, N_GAMEINFO);
            loopv(sents) if(enttype[sents[i].type].resyncs)
            {
                putint(p, i);
                if(enttype[sents[i].type].usetype == EU_ITEM) putint(p, finditem(i) ? 1 : 0);
                else putint(p, sents[i].spawned ? 1 : 0);
            }
            putint(p, -1);
        }

        if(ci)
        {
            ci->state.state = CS_SPECTATOR;
            ci->team = T_NEUTRAL;
            putint(p, N_SPECTATOR);
            putint(p, ci->clientnum);
            putint(p, ci->state.quarantine ? 2 : 1);
            sendf(-1, 1, "ri3x", N_SPECTATOR, ci->clientnum, ci->state.quarantine ? 2 : 1, ci->clientnum);
            putint(p, N_SETTEAM);
            putint(p, ci->clientnum);
            putint(p, ci->team);
        }
        if(!ci || clients.length() > 1)
        {
            putint(p, N_RESUME);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(ci && oi->clientnum == ci->clientnum) continue;
                putint(p, oi->clientnum);
                sendstate(oi->state, p);
            }
            putint(p, -1);
            welcomeinitclient(p, ci ? ci->clientnum : -1);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->state.actortype > A_PLAYER || (ci && oi->clientnum == ci->clientnum)) continue;
                if(oi->mapvote[0])
                {
                    putint(p, N_MAPVOTE);
                    putint(p, oi->clientnum);
                    sendstring(oi->mapvote, p);
                    putint(p, oi->modevote);
                    putint(p, oi->mutsvote);
                }
            }
        }

        if(*G(servermotd))
        {
            putint(p, N_ANNOUNCE);
            putint(p, S_GUIACT);
            putint(p, CON_CHAT);
            sendstring(G(servermotd), p);
        }

        if(m_team(gamemode, mutators)) loopv(scores)
        {
            score &cs = scores[i];
            putint(p, N_SCORE);
            putint(p, cs.team);
            putint(p, cs.total);
        }

        if(smode) smode->initclient(ci, p, true);
        mutate(smuts, mut->initclient(ci, p, true));
        if(ci) ci->online = true;
        return 1;
    }

    void clearevent(clientinfo *ci) { delete ci->events.remove(0); }

    void addhistory(clientinfo *m, clientinfo *v, int millis)
    {
        bool found = false;
        loopv(m->state.damagelog) if (m->state.damagelog[i].clientnum == v->clientnum)
        {
            m->state.damagelog[i].millis = millis;
            found = true;
            break;
        }
        if(!found) m->state.damagelog.add(dmghist(v->clientnum, millis));
    }

    void gethistory(clientinfo *m, clientinfo *v, int millis, vector<int> &log, bool clear = false, int points = 0)
    {
        loopv(m->state.damagelog) if(m->state.damagelog[i].clientnum != v->clientnum && millis-m->state.damagelog[i].millis <= G(assistkilldelay))
        {
            clientinfo *assist = (clientinfo *)getinfo(m->state.damagelog[i].clientnum);
            if(assist)
            {
                log.add(assist->clientnum);
                if(points && !m_nopoints(gamemode, mutators)) givepoints(assist, points);
            }
        }
        if(clear) m->state.damagelog.shrink(0);
    }

    bool isghost(clientinfo *d, clientinfo *e)
    {
        if(d != e && d->state.actortype < A_ENEMY && (!e || e->state.actortype < A_ENEMY))
        {
            if(m_ghost(gamemode, mutators)) return true;
            if(m_team(gamemode, mutators)) switch(G(damageteam))
            {
                case 1: if(d->state.actortype > A_PLAYER || (e && e->state.actortype == A_PLAYER)) break;
                case 0: if(e && d->team == e->team) return true; break;
                case 2: default: break;
            }
        }
        return false;
    }

    void dodamage(clientinfo *m, clientinfo *v, int damage, int weap, int flags, int material, const ivec &hitpush = ivec(0, 0, 0), const ivec &hitvel = ivec(0, 0, 0), float dist = 0, bool first = true)
    {
        int realdamage = damage, realflags = flags, nodamage = 0, hurt = 0, statweap = weap, statalt = WS(flags);
        realflags &= ~HIT_SFLAGS;
        if(realflags&HIT_MATERIAL && (material&MATF_VOLUME) == MAT_LAVA) realflags |= HIT_BURN;

        if(smode && !smode->damage(m, v, realdamage, weap, realflags, material, hitpush, hitvel, dist)) { nodamage++; }
        mutate(smuts, if(!mut->damage(m, v, realdamage, weap, realflags, material, hitpush, hitvel, dist)) { nodamage++; });
        if(v->state.actortype < A_ENEMY)
        {
            if(v == m && !G(damageself)) nodamage++;
            else if(isghost(m, v)) nodamage++;
        }

        if(isweap(weap) && WF(WK(flags), weap, residualundo, WS(flags)))
        {
            if(WF(WK(flags), weap, residualundo, WS(flags))&WR(BURN) && m->state.burning(gamemillis, G(burntime)))
            {
                m->state.lastres[WR_BURN] = m->state.lastrestime[WR_BURN] = 0;
                sendf(-1, 1, "ri3", N_SPHY, m->clientnum, SPHY_EXTINGUISH);
            }
            if(WF(WK(flags), weap, residualundo, WS(flags))&WR(BLEED) && m->state.bleeding(gamemillis, G(bleedtime)))
                m->state.lastres[WR_BLEED] = m->state.lastrestime[WR_BLEED] = 0;
            if(WF(WK(flags), weap, residualundo, WS(flags))&WR(SHOCK) && m->state.shocking(gamemillis, G(shocktime)))
                m->state.lastres[WR_SHOCK] = m->state.lastrestime[WR_SHOCK] = 0;
        }

        if(nodamage || !hithurts(realflags))
        {
            realflags &= ~HIT_CLEAR;
            realflags |= HIT_WAVE;
        }
        else
        {
            m->state.health = m->state.health-realdamage;
            if(m->state.actortype < A_ENEMY) m->state.health = min(m->state.health, m_maxhealth(gamemode, mutators, m->state.model));
            if(realdamage > 0)
            {
                hurt = min(m->state.health, realdamage);
                m->state.lastregen = 0;
                m->state.lastpain = gamemillis;
                v->state.damage += realdamage;
                if(m != v && (!m_team(gamemode, mutators) || m->team != v->team))
                {
                    if(weap == -1)
                    {
                        if(flags&HIT_BURN)
                        {
                            statalt = m->state.lastresalt[WR_BURN];
                            if(statalt)
                                v->state.weapstats[m->state.lastresweapon[WR_BURN]].damage2 += realdamage;
                            else
                                v->state.weapstats[m->state.lastresweapon[WR_BURN]].damage1 += realdamage;
                            statweap = m->state.lastresweapon[WR_BURN];
                        }
                        else if(flags&HIT_BLEED)
                        {
                            statalt = m->state.lastresalt[WR_BLEED];
                            if(statalt)
                                v->state.weapstats[m->state.lastresweapon[WR_BLEED]].damage2 += realdamage;
                            else
                                v->state.weapstats[m->state.lastresweapon[WR_BLEED]].damage1 += realdamage;
                            statweap = m->state.lastresweapon[WR_BLEED];
                        }
                        else if(flags&HIT_SHOCK)
                        {
                            statalt = m->state.lastresalt[WR_SHOCK];
                            if(statalt)
                                v->state.weapstats[m->state.lastresweapon[WR_SHOCK]].damage2 += realdamage;
                            else
                                v->state.weapstats[m->state.lastresweapon[WR_SHOCK]].damage1 += realdamage;
                            statweap = m->state.lastresweapon[WR_SHOCK];
                        }
                    }
                    else
                    {
                        if(WS(flags))
                            v->state.weapstats[statweap].damage2 += realdamage;
                        else
                            v->state.weapstats[statweap].damage1 += realdamage;
                    }
                }
                if(m->state.health <= 0) realflags |= HIT_KILL;
                if(wr_burning(weap, flags))
                {
                    m->state.lastres[WR_BURN] = m->state.lastrestime[WR_BURN] = gamemillis;
                    m->state.lastresowner[WR_BURN] = v->clientnum;
                    m->state.lastresweapon[WR_BURN] = weap;
                    m->state.lastresalt[WR_BURN] = WS(flags);
                }
                if(wr_bleeding(weap, flags))
                {
                    m->state.lastres[WR_BLEED] = m->state.lastrestime[WR_BLEED] = gamemillis;
                    m->state.lastresowner[WR_BLEED] = v->clientnum;
                    m->state.lastresweapon[WR_BLEED] = weap;
                    m->state.lastresalt[WR_BLEED] = WS(flags);
                }
                if(wr_shocking(weap, flags))
                {
                    m->state.lastres[WR_SHOCK] = m->state.lastrestime[WR_SHOCK] = gamemillis;
                    m->state.lastresowner[WR_SHOCK] = v->clientnum;
                    m->state.lastresweapon[WR_SHOCK] = weap;
                    m->state.lastresalt[WR_SHOCK] = WS(flags);
                }
                if(isweap(weap) && m != v && (!m_team(gamemode, mutators) || m->team != v->team) && first)
                {
                    if(WK(flags))
                    {
                        if(WS(flags))
                            v->state.weapstats[statweap].flakhits2++;
                        else
                            v->state.weapstats[statweap].flakhits1++;
                    }
                    else
                    {
                        if(WS(flags))
                            v->state.weapstats[statweap].hits2++;
                        else
                            v->state.weapstats[statweap].hits1++;
                    }
                }
            }
        }
        if(smode) smode->dodamage(m, v, realdamage, hurt, weap, realflags, material, hitpush, hitvel, dist);
        mutate(smuts, mut->dodamage(m, v, realdamage, hurt, weap, realflags, material, hitpush, hitvel, dist));
        if(realdamage >= 0 && m != v && (!m_team(gamemode, mutators) || m->team != v->team))
            addhistory(m, v, gamemillis);
        sendf(-1, 1, "ri9i5", N_DAMAGE, m->clientnum, v->clientnum, weap, realflags, realdamage, m->state.health, hitpush.x, hitpush.y, hitpush.z, hitvel.x, hitvel.y, hitvel.z, int(dist*DNF));
        if(realflags&HIT_KILL)
        {
            int fragvalue = 1;
            if(m != v && (!m_team(gamemode, mutators) || m->team != v->team))
            {
                v->state.frags++;
                if(statalt)
                    v->state.weapstats[statweap].frags2++;
                else
                    v->state.weapstats[statweap].frags1++;
            }
            else fragvalue = -fragvalue;
            bool isai = m->state.actortype >= A_ENEMY, isteamkill = false;
            int pointvalue = (smode && !isai ? smode->points(m, v) : fragvalue), style = FRAG_NONE;
            pointvalue *= isai ? G(enemybonus) : G(fragbonus);
            if(realdamage >= (realflags&HIT_EXPLODE ? m_health(gamemode, mutators, m->state.model)/2 : m_health(gamemode, mutators, m->state.model)))
                style = FRAG_OBLITERATE;
            m->state.spree = 0;
            if(m_team(gamemode, mutators) && v->team == m->team)
            {
                v->state.spree = 0;
                if(isweap(weap) && (v == m || WF(WK(flags), weap, damagepenalty, WS(flags))))
                {
                    pointvalue *= G(teamkillpenalty);
                    if(v != m) isteamkill = true;
                }
                else pointvalue = 0; // no penalty
            }
            else if(v != m && v->state.actortype < A_ENEMY)
            {
                if(!firstblood && !m_duel(gamemode, mutators) && ((v->state.actortype == A_PLAYER && m->state.actortype < A_ENEMY) || (v->state.actortype < A_ENEMY && m->state.actortype == A_PLAYER)))
                {
                    firstblood = true;
                    style |= FRAG_FIRSTBLOOD;
                    pointvalue += G(firstbloodpoints);
                }
                if(flags&HIT_HEAD) // NOT HZONE
                {
                    style |= FRAG_HEADSHOT;
                    pointvalue += G(headshotpoints);
                }
                if(m_play(gamemode) && m->state.actortype < A_ENEMY)
                {
                    int logs = 0;
                    v->state.spree++;
                    v->state.fraglog.add(m->clientnum);
                    if(G(multikilldelay))
                    {
                        logs = 0;
                        loopv(v->state.fragmillis)
                        {
                            if(gamemillis-v->state.fragmillis[i] > G(multikilldelay)) v->state.fragmillis.remove(i--);
                            else logs++;
                        }
                        if(!logs) v->state.rewards[0] &= ~FRAG_MULTI;
                        v->state.fragmillis.add(gamemillis);
                        logs++;
                        if(logs >= 2)
                        {
                            int offset = clamp(logs-2, 0, 2), type = 1<<(FRAG_MKILL+offset); // double, triple, multi..
                            if(!(v->state.rewards[0]&type))
                            {
                                style |= type;
                                v->state.rewards[0] |= type;
                                pointvalue += (G(multikillbonus) ? offset+1 : 1)*G(multikillpoints);
                            }
                        }
                    }
                    loopj(FRAG_SPREES) if(m->state.rewards[1]&(1<<(FRAG_SPREE+j)))
                    {
                        style |= FRAG_BREAKER;
                        pointvalue += G(spreebreaker);
                        break;
                    }
                    if(v->state.spree <= G(spreecount)*FRAG_SPREES && !(v->state.spree%G(spreecount)))
                    {
                        int offset = clamp((v->state.spree/G(spreecount)), 1, int(FRAG_SPREES))-1, type = 1<<(FRAG_SPREE+offset);
                        if(!(v->state.rewards[0]&type))
                        {
                            style |= type;
                            loopj(2) v->state.rewards[j] |= type;
                            pointvalue += G(spreepoints);
                        }
                    }
                    logs = 0;
                    loopv(m->state.fraglog) if(m->state.fraglog[i] == v->clientnum) { logs++; m->state.fraglog.remove(i--); }
                    if(logs >= G(dominatecount))
                    {
                        style |= FRAG_REVENGE;
                        pointvalue += G(revengepoints);
                    }
                    logs = 0;
                    loopv(v->state.fraglog) if(v->state.fraglog[i] == m->clientnum) logs++;
                    if(logs == G(dominatecount))
                    {
                        style |= FRAG_DOMINATE;
                        pointvalue += G(dominatepoints);
                    }
                }
            }
            if(m_race(gamemode) && (!m_gsp3(gamemode, mutators) || m->team == T_ALPHA) && m->state.cpnodes.length() == 1)
            {  // reset if hasn't reached another checkpoint yet
                m->state.cpmillis = 0;
                m->state.cpnodes.shrink(0);
                sendf(-1, 1, "ri3", N_CHECKPOINT, m->clientnum, -1);
            }
            if(pointvalue && !m_nopoints(gamemode, mutators))
            {
                if(v != m && v->state.actortype >= A_ENEMY && m->state.actortype < A_ENEMY)
                {
                    pointvalue = -pointvalue;
                    givepoints(m, pointvalue);
                }
                else if(v->state.actortype < A_ENEMY) givepoints(v, pointvalue);
            }
            m->state.deaths++;
            m->state.rewards[1] = 0;
            dropitems(m, actor[m->state.actortype].living ? DROP_DEATH : DROP_EXPIRE);
            static vector<int> dmglog;
            dmglog.setsize(0);
            gethistory(m, v, gamemillis, dmglog, true, 1);
            sendf(-1, 1, "ri9i3v", N_DIED, m->clientnum, m->state.deaths, v->clientnum, v->state.frags, v->state.spree, style, weap, realflags, realdamage, material, dmglog.length(), dmglog.length(), dmglog.getbuf());
            m->position.setsize(0);
            if(smode) smode->died(m, v);
            mutate(smuts, mut->died(m, v));
            m->state.state = CS_DEAD; // don't issue respawn yet until DEATHMILLIS has elapsed
            m->state.lastdeath = gamemillis;
            m->state.updatetimeplayed();
            if(m->state.actortype == A_BOT) aiman::setskill(m);
            if(m != v && v->state.actortype == A_BOT) aiman::setskill(v);
            if(isteamkill && v->state.actortype == A_PLAYER) // don't punish the idiot bots
            {
                v->state.teamkills.add(teamkill(totalmillis, v->team, 0-pointvalue));
                if(G(teamkilllock) && !haspriv(v, G(teamkilllock)))
                {
                    int numkills = 0;
                    if(!G(teamkilltime)) numkills = v->state.teamkills.length();
                    else loopv(v->state.teamkills)
                        if(totalmillis-v->state.teamkills[i].millis <= G(teamkilltime)*1000*60) numkills++;
                    if(numkills >= G(teamkillwarn) && numkills%G(teamkillwarn) == 0)
                    {
                        uint ip = getclientip(v->clientnum);
                        v->state.warnings[WARN_TEAMKILL][0]++;
                        v->state.warnings[WARN_TEAMKILL][1] = totalmillis ? totalmillis : 1;
                        if(ip && G(teamkillban) && v->state.warnings[WARN_TEAMKILL][0] >= G(teamkillban) && !haspriv(v, PRIV_MODERATOR) && !checkipinfo(control, ipinfo::EXCEPT, ip))
                        {
                            ipinfo &c = control.add();
                            c.ip = ip;
                            c.mask = 0xFFFFFFFF;
                            c.type = ipinfo::BAN;
                            c.flag = ipinfo::INTERNAL;
                            c.time = totalmillis ? totalmillis : 1;
                            c.reason = newstring("team killing is not permitted");
                            srvoutf(-3, "\fs\fcbanned\fS %s (%s/%s): %s", colourname(v), gethostname(v->clientnum), gethostip(v->clientnum), c.reason);
                            updatecontrols = true;
                        }
                        else if(G(teamkillkick) && v->state.warnings[WARN_TEAMKILL][0] >= G(teamkillkick))
                        {
                            srvoutf(-3, "\fs\fckicked\fS %s: team killing is not permitted", colourname(v));
                            v->kicked = updatecontrols = true;
                        }
                        else srvmsgft(v->clientnum, CON_CHAT, "\fy\fs\fzoyWARNING:\fS team killing is not permitted, action will be taken if you continue");
                    }
                }
            }
        }
    }

    void suicideevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(gs.state != CS_ALIVE) return;
        if(flags&HIT_MATERIAL && (material&MATF_VOLUME) == MAT_LAVA) flags |= HIT_BURN;
        if(!(flags&HIT_MATERIAL) && !(flags&HIT_LOST))
        {
            if(smode && !smode->damage(ci, ci, ci->state.health, -1, flags, material)) { return; }
            mutate(smuts, if(!mut->damage(ci, ci, ci->state.health, -1, flags, material)) { return; });
        }
        ci->state.spree = 0;
        ci->state.deaths++;
        bool kamikaze = dropitems(ci, actor[ci->state.actortype].living ? DROP_DEATH : DROP_EXPIRE);
        if(m_race(gamemode) && (!m_gsp3(gamemode, mutators) || ci->team == T_ALPHA) && !(flags&HIT_SPEC) && (!flags || ci->state.cpnodes.length() == 1))
        { // reset if suicided, hasn't reached another checkpoint yet
            ci->state.cpmillis = 0;
            ci->state.cpnodes.shrink(0);
            sendf(-1, 1, "ri3", N_CHECKPOINT, ci->clientnum, -1);
        }
        else if(!m_nopoints(gamemode, mutators) && ci->state.actortype == A_PLAYER)
        {
            int pointvalue = (smode ? smode->points(ci, ci) : -1)*G(fragbonus);
            if(kamikaze) pointvalue *= G(teamkillpenalty);
            givepoints(ci, pointvalue);
        }
        if(G(burntime) && flags&HIT_BURN)
        {
            ci->state.lastres[WR_BURN] = ci->state.lastrestime[WR_BURN] = gamemillis;
            ci->state.lastresowner[WR_BURN] = ci->clientnum;
        }
        static vector<int> dmglog; dmglog.setsize(0);
        gethistory(ci, ci, gamemillis, dmglog, true, 1);
        sendf(-1, 1, "ri9i3v", N_DIED, ci->clientnum, ci->state.deaths, ci->clientnum, ci->state.frags, 0, 0, -1, flags, ci->state.health*2, material, dmglog.length(), dmglog.length(), dmglog.getbuf());
        ci->position.setsize(0);
        if(smode) smode->died(ci, NULL);
        mutate(smuts, mut->died(ci, NULL));
        gs.state = CS_DEAD;
        gs.lastdeath = gamemillis;
        gs.updatetimeplayed();
        if(ci->state.actortype == A_BOT) aiman::setskill(ci);
    }

    int calcdamage(clientinfo *v, clientinfo *m, int weap, int &flags, float radial, float size, float dist, float scale, bool self)
    {
        flags &= ~HIT_SFLAGS;
        if(!hithurts(flags))
        {
            flags &= ~HIT_CLEAR;
            flags |= HIT_WAVE;
        }

        float skew = clamp(scale, 0.f, 1.f)*G(damagescale);

        if(flags&HIT_WHIPLASH) skew *= WF(WK(flags), weap, damagewhiplash, WS(flags));
        else if(flags&HIT_HEAD) skew *= WF(WK(flags), weap, damagehead, WS(flags));
        else if(flags&HIT_TORSO) skew *= WF(WK(flags), weap, damagetorso, WS(flags));
        else if(flags&HIT_LEGS) skew *= WF(WK(flags), weap, damagelegs, WS(flags));
        else return 0;

        if(radial > 0) skew *= clamp(1.f-dist/size, 1e-6f, 1.f);
        else if(WF(WK(flags), weap, taper, WS(flags))) skew *= clamp(dist, 0.f, 1.f);

        if(!m_insta(gamemode, mutators))
        {
            if(m_capture(gamemode) && G(capturebuffdelay))
            {
                if(v->state.lastbuff) skew *= G(capturebuffdamage);
                if(m->state.lastbuff) skew /= G(capturebuffshield);
            }
            else if(m_defend(gamemode) && G(defendbuffdelay))
            {
                if(v->state.lastbuff) skew *= G(defendbuffdamage);
                if(m->state.lastbuff) skew /= G(defendbuffshield);
            }
            else if(m_bomber(gamemode) && G(bomberbuffdelay))
            {
                if(v->state.lastbuff) skew *= G(bomberbuffdamage);
                if(m->state.lastbuff) skew /= G(bomberbuffshield);
            }
        }

        if(self)
        {
            float modify = WF(WK(flags), weap, damageself, WS(flags))*G(damageselfscale);
            if(modify != 0) skew *= modify;
            else
            {
                flags &= ~HIT_CLEAR;
                flags |= HIT_WAVE;
            }
        }
        else if(m_team(gamemode, mutators) && v->team == m->team)
        {
            float modify = WF(WK(flags), weap, damageteam, WS(flags))*G(damageteamscale);
            if(modify != 0) skew *= modify;
            else
            {
                flags &= ~HIT_CLEAR;
                flags |= HIT_WAVE;
            }
        }

        return int(ceilf(WF(WK(flags), weap, damage, WS(flags))*skew));
    }

    void stickyevent::process(clientinfo *ci)
    {
        if(isweap(weap))
        {
            servstate &gs = ci->state;
            if(!gs.weapshots[weap][WS(flags) ? 1 : 0].find(id))
            {
                if(G(serverdebug) >= 2) srvmsgf(ci->clientnum, "sync error: sticky [%d (%d)] failed - not found", weap, id);
                return;
            }
            clientinfo *m = target >= 0 ? (clientinfo *)getinfo(target) : NULL;
            if(target < 0 || (m && m->state.state == CS_ALIVE && !m->state.protect(gamemillis, m_protect(gamemode, mutators))))
                sendf(-1, 1, "ri9ix", N_STICKY, ci->clientnum, target, id, norm.x, norm.y, norm.z, pos.x, pos.y, pos.z, ci->clientnum);
            else if(G(serverdebug) >= 2) srvmsgf(ci->clientnum, "sync error: sticky [%d (%d)] failed - state disallows it", weap, id);
        }
    }

    void destroyevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(weap == -1)
        {
            gs.dropped.remove(id);
            if(sents.inrange(id)) sents[id].millis = gamemillis;
        }
        else if(isweap(weap))
        {
            if(!gs.weapshots[weap][WS(flags) ? 1 : 0].find(id))
            {
                if(G(serverdebug) >= 2) srvmsgf(ci->clientnum, "sync error: destroy [%d:%d (%d)] failed - not found", weap, WS(flags) ? 1 : 0, id);
                return;
            }
            vector<clientinfo *> hitclients;
            if(hits.empty())
            {
                gs.weapshots[weap][WS(flags) ? 1 : 0].remove(id);
                if(id >= 0 && !m_insta(gamemode, mutators))
                {
                    int f = W2(weap, fragweap, WS(flags));
                    if(f >= 0)
                    {
                        int w = f%W_MAX, r = min(W2(weap, fragrays, WS(flags)), MAXPARAMS);
                        loopi(r) gs.weapshots[w][f >= W_MAX ? 1 : 0].add(-id);
                        if(WS(flags))
                            gs.weapstats[weap].flakshots2 += r;
                        else
                            gs.weapstats[weap].flakshots1 += r;
                    }
                }
                sendf(-1, 1, "ri4x", N_DESTROY, ci->clientnum, 1, id, ci->clientnum);
            }
            else loopv(hits)
            {
                bool first = true;
                hitset &h = hits[i];
                clientinfo *m = (clientinfo *)getinfo(h.target);
                loopvj(hitclients) if(hitclients[j] == m) first = false;
                hitclients.add(m);
                if(!m)
                {
                    if(G(serverdebug) >= 2) srvmsgf(ci->clientnum, "sync error: destroy [%d (%d)] failed - hit %d [%d] not found", weap, id, i, h.target);
                    continue;
                }
                if(h.proj)
                {
                    servstate &ts = m->state;
                    loopj(W_MAX) loopk(2) if(ts.weapshots[j][k].find(h.proj))
                    {
                        sendf(m->clientnum, 1, "ri4", N_DESTROY, m->clientnum, 1, h.proj);
                        break;
                    }
                }
                else
                {
                    int hflags = flags|h.flags;
                    float skew = float(scale)/DNF, rad = radial > 0 ? clamp(radial/DNF, 0.f, WX(WK(flags), weap, explode, WS(flags), gamemode, mutators, skew)) : 0.f,
                          size = rad > 0 ? (hflags&HIT_WAVE ? rad*WF(WK(flags), weap, wavepush, WS(flags)) : rad) : 0.f, dist = float(h.dist)/DNF;
                    if(m->state.state == CS_ALIVE && !m->state.protect(gamemillis, m_protect(gamemode, mutators)))
                    {
                        int damage = calcdamage(ci, m, weap, hflags, rad, size, dist, skew, ci == m);
                        if(damage) dodamage(m, ci, damage, weap, hflags, 0, h.dir, h.vel, dist, first);
                        else if(G(serverdebug) >= 2)
                            srvmsgf(ci->clientnum, "sync error: destroy [%d (%d)] failed - hit %d [%d] determined zero damage", weap, id, i, h.target);
                    }
                    else if(G(serverdebug) >= 2)
                        srvmsgf(ci->clientnum, "sync error: destroy [%d (%d)] failed - hit %d [%d] state disallows it", weap, id, i, h.target);
                }
            }
        }
    }

    void shotevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(!gs.isalive(gamemillis) || !isweap(weap))
        {
            if(G(serverdebug) >= 3) srvmsgf(ci->clientnum, "sync error: shoot [%d] failed - unexpected message", weap);
            return;
        }
        int sub = W2(weap, ammosub, WS(flags));
        if(sub > 1 && W2(weap, cooktime, WS(flags)))
        {
            if(ci->state.ammo[weap] < sub)
            {
                int maxscale = int(ci->state.ammo[weap]/float(sub)*W2(weap, cooktime, WS(flags)));
                if(scale > maxscale) scale = maxscale;
            }
            sub = int(ceilf(sub*scale/float(W2(weap, cooktime, WS(flags)))));
        }
        if(!gs.canshoot(weap, flags, m_weapon(gamemode, mutators), millis))
        {
            if(!gs.canshoot(weap, flags, m_weapon(gamemode, mutators), millis, (1<<W_S_RELOAD)))
            {
                if(sub && W(weap, ammomax)) ci->state.ammo[weap] = max(ci->state.ammo[weap]-sub, 0);
                if(!gs.hasweap(weap, m_weapon(gamemode, mutators))) gs.entid[weap] = -1; // its gone..
                if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: shoot [%d] failed - current state disallows it", weap);
                return;
            }
            if(gs.weapload[gs.weapselect] > 0)
            {
                takeammo(ci, gs.weapselect, gs.weapload[gs.weapselect]);
                gs.weapload[gs.weapselect] = -gs.weapload[gs.weapselect]; // the client should already do this for themself
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, gs.weapselect, gs.weapload[gs.weapselect], gs.ammo[gs.weapselect], ci->clientnum);
            }
        }
        takeammo(ci, weap, sub);
        gs.setweapstate(weap, WS(flags) ? W_S_SECONDARY : W_S_PRIMARY, W2(weap, delayattack, WS(flags)), millis);
        sendf(-1, 1, "ri8ivx", N_SHOTFX, ci->clientnum, weap, flags, scale, from.x, from.y, from.z, shots.length(), shots.length()*sizeof(shotmsg)/sizeof(int), shots.getbuf(), ci->clientnum);
        gs.weapshot[weap] = sub;
        gs.shotdamage += W2(weap, damage, WS(flags))*shots.length();
        loopv(shots) gs.weapshots[weap][WS(flags) ? 1 : 0].add(shots[i].id);
        if(WS(flags)) gs.weapstats[weap].shots2++;
        else gs.weapstats[weap].shots1++;
        if(!gs.hasweap(weap, m_weapon(gamemode, mutators)))
        {
            //if(sents.inrange(gs.entid[weap])) setspawn(gs.entid[weap], false);
            sendf(-1, 1, "ri7", N_DROP, ci->clientnum, -1, 1, weap, -1, 0);
            gs.ammo[weap] = gs.entid[weap] = -1; // its gone..
        }
    }

    void switchevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(!gs.isalive(gamemillis) || !isweap(weap))
        {
            if(G(serverdebug) >= 3) srvmsgf(ci->clientnum, "sync error: switch [%d] failed - unexpected message", weap);
            sendf(ci->clientnum, 1, "ri3", N_WSELECT, ci->clientnum, gs.weapselect);
            return;
        }
        if(!gs.canswitch(weap, m_weapon(gamemode, mutators), millis, (1<<W_S_SWITCH)))
        {
            if(!gs.canswitch(weap, m_weapon(gamemode, mutators), millis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: switch [%d] failed - current state disallows it", weap);
                sendf(-1, 1, "ri3", N_WSELECT, ci->clientnum, gs.weapselect);
                return;
            }
            if(gs.weapload[gs.weapselect] > 0)
            {
                takeammo(ci, gs.weapselect, gs.weapload[gs.weapselect]);
                gs.weapload[gs.weapselect] = -gs.weapload[gs.weapselect]; // the client should already do this for themself
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, gs.weapselect, gs.weapload[gs.weapselect], gs.ammo[gs.weapselect], ci->clientnum);
            }
        }
        gs.updateweaptime();
        gs.weapswitch(weap, millis, G(weaponswitchdelay));
        sendf(-1, 1, "ri3x", N_WSELECT, ci->clientnum, weap, ci->clientnum);
    }

    void dropevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(!gs.isalive(gamemillis) || !isweap(weap))
        {
            if(G(serverdebug) >= 3) srvmsgf(ci->clientnum, "sync error: drop [%d] failed - unexpected message", weap);
            return;
        }
        int sweap = m_weapon(gamemode, mutators);
        if(!gs.candrop(weap, sweap, millis, m_loadout(gamemode, mutators), (1<<W_S_SWITCH)))
        {
            if(!gs.candrop(weap, sweap, millis, m_loadout(gamemode, mutators), (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: drop [%d] failed - current state disallows it", weap);
                return;
            }
            if(gs.weapload[gs.weapselect] > 0)
            {
                takeammo(ci, gs.weapselect, gs.weapload[gs.weapselect]);
                gs.weapload[gs.weapselect] = -gs.weapload[gs.weapselect];
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, gs.weapselect, gs.weapload[gs.weapselect], gs.ammo[gs.weapselect], ci->clientnum);
            }
        }
        int dropped = -1, ammo = -1, nweap = gs.bestweap(sweap, true); // switch to best weapon
        if(sents.inrange(gs.entid[weap]))
        {
            dropped = gs.entid[weap];
            ammo = gs.ammo[weap] ? gs.ammo[weap] : W(weap, ammomax);
            setspawn(dropped, false);
            gs.dropped.add(dropped, ammo);
        }
        gs.ammo[weap] = gs.entid[weap] = -1;
        gs.weapswitch(nweap, millis, G(weaponswitchdelay));
        sendf(-1, 1, "ri7", N_DROP, ci->clientnum, nweap, 1, weap, dropped, ammo);
    }

    void reloadevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(!gs.isalive(gamemillis) || !isweap(weap))
        {
            if(G(serverdebug) >= 3) srvmsgf(ci->clientnum, "sync error: reload [%d] failed - unexpected message", weap);
            return;
        }
        if(!gs.canreload(weap, m_weapon(gamemode, mutators), false, millis))
        {
            if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: reload [%d] failed - current state disallows it", weap);
            return;
        }
        gs.setweapstate(weap, W_S_RELOAD, W(weap, delayreload), millis);
        int oldammo = gs.ammo[weap];
        gs.ammo[weap] = min(max(gs.ammo[weap], 0) + W(weap, ammoadd), W(weap, ammomax));
        gs.weapload[weap] = gs.ammo[weap]-oldammo;
        sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, weap, gs.weapload[weap], gs.ammo[weap], ci->clientnum);
    }

    void useevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(gs.state != CS_ALIVE || !sents.inrange(ent) || sents[ent].type != WEAPON)
        {
            if(G(serverdebug) >= 3) srvmsgf(ci->clientnum, "sync error: use [%d] failed - unexpected message", ent);
            return;
        }
        if(!finditem(ent))
        {
            if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: use [%d] failed - doesn't seem to be spawned anywhere", ent);
            return;
        }
        gs.updateweaptime();
        int sweap = m_weapon(gamemode, mutators), attr = w_attr(gamemode, mutators, sents[ent].type, sents[ent].attrs[0], sweap);
        if(!gs.canuse(sents[ent].type, attr, sents[ent].attrs, sweap, millis, (1<<W_S_SWITCH)))
        {
            if(!gs.canuse(sents[ent].type, attr, sents[ent].attrs, sweap, millis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: use [%d] failed - current state disallows it", ent);
                return;
            }
            if(gs.weapload[gs.weapselect] > 0)
            {
                takeammo(ci, gs.weapselect, gs.weapload[gs.weapselect]);
                gs.weapload[gs.weapselect] = -gs.weapload[gs.weapselect]; // the client should already do this for themself
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, gs.weapselect, gs.weapload[gs.weapselect], gs.ammo[gs.weapselect], ci->clientnum);
            }
        }
        int weap = -1, ammoamt = -1, dropped = -1, ammo = -1;
        if(m_classic(gamemode, mutators) && !gs.hasweap(attr, sweap) && w_carry(attr, sweap) && gs.carry(sweap) >= G(maxcarry)) weap = gs.drop(sweap);
        loopvk(clients) if(clients[k]->state.dropped.find(ent))
        {
            clients[k]->state.dropped.values(ent, ammoamt);
            break;
        }
        if(isweap(weap))
        {
            if(sents.inrange(gs.entid[weap]))
            {
                dropped = gs.entid[weap];
                ammo = gs.ammo[weap];
                setspawn(dropped, false);
                gs.setweapstate(weap, W_S_SWITCH, G(weaponswitchdelay), millis);
                gs.dropped.add(dropped, ammo);
            }
            gs.ammo[weap] = gs.entid[weap] = -1;
        }
        setspawn(ent, false, true);
        gs.useitem(ent, sents[ent].type, attr, ammoamt, sweap, millis, G(weaponswitchdelay));
        sendf(-1, 1, "ri8", N_ITEMACC, ci->clientnum, ent, ammoamt, sents[ent].spawned ? 1 : 0, weap, dropped, ammo);
    }

    bool gameevent::flush(clientinfo *ci, int fmillis)
    {
        process(ci);
        return true;
    }

    bool timedevent::flush(clientinfo *ci, int fmillis)
    {
        if(millis > fmillis) return false;
        else if(millis >= ci->lastevent)
        {
            ci->lastevent = millis;
            process(ci);
        }
        return true;
    }

    void flushevents(clientinfo *ci, int millis)
    {
        while(ci->events.length())
        {
            gameevent *ev = ci->events[0];
            if(ev->flush(ci, millis)) clearevent(ci);
            else break;
        }
    }

    void processevents()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            flushevents(ci, gamemillis);
        }
    }

    void cleartimedevents(clientinfo *ci)
    {
        int keep = 0;
        loopv(ci->events)
        {
            if(ci->events[i]->keepable())
            {
                if(keep < i)
                {
                    for(int j = keep; j < i; j++) delete ci->events[j];
                    ci->events.remove(keep, i - keep);
                    i = keep;
                }
                keep = i+1;
                continue;
            }
        }
        while(ci->events.length() > keep) delete ci->events.pop();
    }

    int requestswap(clientinfo *ci, int team)
    {
        if(!allowteam(ci, team, T_FIRST, numclients() > 1))
        {
            if(team && m_swapteam(gamemode, mutators) && ci->team != team && ci->state.actortype == A_PLAYER && ci->swapteam != team && canplay())
            {
                ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fy%s requests swap to team %s, change teams to accept", colourname(ci), colourteam(team));
                ci->swapteam = team;
            }
            team = chooseteam(ci);
        }
        return team;
    }

    void waiting(clientinfo *ci, int drop, bool doteam, bool exclude)
    {
        if(ci->state.state == CS_ALIVE)
        {
            if(drop) dropitems(ci, drop);
            if(smode) smode->died(ci);
            mutate(smuts, mut->died(ci));
            ci->state.lastdeath = gamemillis;
        }
        //else if(!ci->state.lastdeath) ci->state.lastdeath = gamemillis;
        if(exclude) sendf(-1, 1, "ri2x", N_WAITING, ci->clientnum, ci->clientnum);
        else sendf(-1, 1, "ri2", N_WAITING, ci->clientnum);
        ci->state.state = CS_WAITING;
        ci->state.weapreset(false);
        if(doteam && !allowteam(ci, ci->team, T_FIRST, false)) setteam(ci, chooseteam(ci), TT_INFO);
    }

    int triggertime(int i)
    {
        if(sents.inrange(i)) switch(sents[i].type)
        {
            case TRIGGER: case MAPMODEL: case PARTICLES: case MAPSOUND: case TELEPORT: case PUSHER: return 1000; break;
            default: break;
        }
        return 0;
    }

    void checkents()
    {
        loopv(sents) switch(sents[i].type)
        {
            case TRIGGER:
            {
                if(sents[i].attrs[1] == TR_LINK && sents[i].spawned && gamemillis >= sents[i].millis && (sents[i].attrs[4] == triggerid || !sents[i].attrs[4]) && m_check(sents[i].attrs[5], sents[i].attrs[6], gamemode, mutators))
                {
                    sents[i].spawned = false;
                    sents[i].millis = gamemillis+(triggertime(i)*2);
                    sendf(-1, 1, "ri3", N_TRIGGER, i, 0);
                    loopvj(sents[i].kin) if(sents.inrange(sents[i].kin[j]))
                    {
                        if(sents[sents[i].kin[j]].type == TRIGGER && !m_check(sents[sents[i].kin[j]].attrs[5], sents[sents[i].kin[j]].attrs[6], gamemode, mutators))
                            continue;
                        sents[sents[i].kin[j]].spawned = sents[i].spawned;
                        sents[sents[i].kin[j]].millis = sents[i].millis;
                    }
                }
                break;
            }
            default:
            {
                if(enttype[sents[i].type].usetype != EU_ITEM) break;
                bool allowed = hasitem(i);
                if((allowed && !sents[i].spawned && !finditem(i, true, true)) || (!allowed && sents[i].spawned))
                    setspawn(i, allowed, true, true);
                break;
            }
        }
    }

    void checkclients()
    {
        loopv(clients) if(clients[i]->name[0] && clients[i]->online)
        {
            clientinfo *ci = clients[i];
            if(smode) smode->checkclient(ci);
            mutate(smuts, mut->checkclient(ci));
            if(ci->state.state == CS_ALIVE)
            {
                if(ci->state.burning(gamemillis, G(burntime)))
                {
                    if(gamemillis-ci->state.lastrestime[WR_BURN] >= G(burndelay))
                    {
                        clientinfo *co = (clientinfo *)getinfo(ci->state.lastresowner[WR_BURN]);
                        dodamage(ci, co ? co : ci, G(burndamage), -1, HIT_BURN, 0);
                        ci->state.lastrestime[WR_BURN] += G(burndelay);
                        if(ci->state.state != CS_ALIVE) continue;
                    }
                }
                else if(ci->state.lastres[WR_BURN]) ci->state.lastres[WR_BURN] = ci->state.lastrestime[WR_BURN] = 0;
                if(ci->state.bleeding(gamemillis, G(bleedtime)))
                {
                    if(gamemillis-ci->state.lastrestime[WR_BLEED] >= G(bleeddelay))
                    {
                        clientinfo *co = (clientinfo *)getinfo(ci->state.lastresowner[WR_BLEED]);
                        dodamage(ci, co ? co : ci, G(bleeddamage), -1, HIT_BLEED, 0);
                        ci->state.lastrestime[WR_BLEED] += G(bleeddelay);
                        if(ci->state.state != CS_ALIVE) continue;
                    }
                }
                else if(ci->state.lastres[WR_BLEED]) ci->state.lastres[WR_BLEED] = ci->state.lastrestime[WR_BLEED] = 0;
                if(ci->state.shocking(gamemillis, G(shocktime)))
                {
                    if(gamemillis-ci->state.lastrestime[WR_SHOCK] >= G(shockdelay))
                    {
                        clientinfo *co = (clientinfo *)getinfo(ci->state.lastresowner[WR_SHOCK]);
                        dodamage(ci, co ? co : ci, G(shockdamage), -1, HIT_SHOCK, 0);
                        ci->state.lastrestime[WR_SHOCK] += G(shockdelay);
                        if(ci->state.state != CS_ALIVE) continue;
                    }
                }
                else if(ci->state.lastres[WR_SHOCK]) ci->state.lastres[WR_SHOCK] = ci->state.lastrestime[WR_SHOCK] = 0;
                if(m_regen(gamemode, mutators) && ci->state.actortype < A_ENEMY)
                {
                    int total = m_health(gamemode, mutators, ci->state.model), amt = G(regenhealth),
                        delay = ci->state.lastregen ? G(regentime) : G(regendelay);
                    if(smode) smode->regen(ci, total, amt, delay);
                    if(delay && ci->state.health != total)
                    {
                        int millis = gamemillis-(ci->state.lastregen ? ci->state.lastregen : ci->state.lastpain);
                        if(millis >= delay)
                        {
                            int low = 0;
                            if(ci->state.health > total)
                            {
                                amt = -G(regendecay);
                                total = ci->state.health;
                                low = m_health(gamemode, mutators, ci->state.model);
                            }
                            int heal = clamp(ci->state.health+amt, low, total), eff = heal-ci->state.health;
                            if(eff)
                            {
                                ci->state.health = heal;
                                ci->state.lastregen = gamemillis;
                                sendf(-1, 1, "ri4", N_REGEN, ci->clientnum, ci->state.health, eff);
                            }
                        }
                    }
                }
            }
            else if(ci->state.state == CS_WAITING)
            {
                int nospawn = 0;
                if(smode && !smode->canspawn(ci, false)) { nospawn++; }
                mutate(smuts, if(!mut->canspawn(ci, false)) { nospawn++; });
                if(!nospawn)
                {
                    if(ci->state.lastdeath) flushevents(ci, ci->state.lastdeath + DEATHMILLIS);
                    cleartimedevents(ci);
                    ci->state.state = CS_DEAD; // safety
                    ci->state.respawn(gamemillis);
                    sendspawn(ci);
                }
            }
            if(G(autospectate) && ci->state.state == CS_DEAD && ci->state.lastdeath && gamemillis-ci->state.lastdeath >= G(autospecdelay))
                spectate(ci, true);
        }
    }

    void serverupdate()
    {
        loopvrev(connects) if(totalmillis-connects[i]->connectmillis >= G(connecttimeout))
        {
            clientinfo *ci = connects[i];
            if(ci->connectauth)
            { // auth might have stalled
                ci->connectauth = false;
                ci->authreq = ci->authname[0] = ci->handle[0] = 0;
                srvmsgftforce(ci->clientnum, CON_EVENT, "\founable to verify, authority request timed out");
                int disc = auth::allowconnect(ci);
                if(disc) disconnect_client(ci->clientnum, disc);
                else
                {
                    ci->connectmillis = totalmillis; // in case it doesn't work
                    connected(ci);
                }
            }
            else disconnect_client(ci->clientnum, DISC_TIMEOUT);
        }
        loopvrev(control) if(control[i].flag <= ipinfo::INTERNAL)
        {
            int timeout = 0;
            switch(control[i].type)
            {
                case ipinfo::ALLOW: timeout = G(allowtimeout); break;
                case ipinfo::BAN: timeout = G(bantimeout); break;
                case ipinfo::MUTE: timeout = G(mutetimeout); break;
                case ipinfo::LIMIT: timeout = G(limittimeout); break;
                case ipinfo::EXCEPT: timeout = G(excepttimeout); break;
                default: break;
            }
            if(timeout && totalmillis-control[i].time >= timeout) control.remove(i);
        }
        if(updatecontrols)
        {
            loopvrev(clients)
            {
                uint ip = getclientip(clients[i]->clientnum);
                if(ip && !haspriv(clients[i], G(banlock)) && checkipinfo(control, ipinfo::BAN, ip) && !checkipinfo(control, ipinfo::EXCEPT, ip))
                {
                    disconnect_client(clients[i]->clientnum, DISC_IPBAN);
                    continue;
                }
                if(clients[i]->kicked)
                {
                    disconnect_client(clients[i]->clientnum, DISC_KICK);
                    continue;
                }
            }
            updatecontrols = false;
        }
        if(numclients())
        {
            ifserver(shutdownwait)
            {
                int waituntil = maxshutdownwait*(gs_playing(gamestate) ? 2000 : 1000);
                if(totalmillis >= shutdownwait+waituntil)
                {
                    srvoutf(4, "waited \fs\fc%s\fS to shutdown, overriding and exiting...", timestr(totalmillis-shutdownwait, 4));
                    exit(EXIT_SUCCESS);
                    return;
                }
            }
            if(gamestate == G_S_WAITING)
            {
                if(!gamewaitstart) gamewaitstart = totalmillis;
                int numwait = 0, numgetmap = 0, numnotready = 0;
                loopv(clients)
                {
                    clientinfo *cs = clients[i];
                    if(cs->state.actortype > A_PLAYER) continue;
                    if(!cs->ready || (G(waitforplayers) == 2 && cs->state.state == CS_SPECTATOR)) numwait++;
                    if(cs->wantsmap || cs->gettingmap) numgetmap++;
                    if(!cs->ready) numnotready++;
                }
                switch(gamewaitstate)
                {
                    case 0: // start check
                    {
                        if(!G(waitforplayermaps))
                        {
                            gamewaittime = totalmillis+G(waitforplayertime);
                            gamewaitstate = 3;
                            sendtick();
                            break;
                        }
                        if(numnotready && gamewaittime > totalmillis) break;
                        if(!hasmapdata())
                        {
                            if(mapsending < 0) getmap(NULL, true);
                            if(mapsending >= 0)
                            {
                                srvoutf(-3, "\fyplease wait while the server downloads the map..");
                                gamewaittime = totalmillis+G(waitforplayermaps);
                                gamewaitstate = 1;
                                sendtick();
                                break;
                            }
                            gamewaittime = totalmillis+G(waitforplayertime);
                            gamewaitstate = 3;
                            sendtick();
                            break;
                        }
                    }
                    case 1: // waiting for server
                    {
                        if(!hasmapdata() && mapsending >= 0 && gamewaittime > totalmillis) break;
                        if(numgetmap && hasmapdata())
                        {
                            srvoutf(-3, "\fyplease wait for \fs\fc%d\fS %s to download the map..", numgetmap, numgetmap != 1 ? "players" : "player");
                            gamewaittime = totalmillis+G(waitforplayermaps);
                            gamewaitstate = 2;
                            sendtick();
                            break;
                        }
                        gamewaittime = totalmillis+G(waitforplayertime);
                        gamewaitstate = 3;
                        sendtick();
                        break;
                    }
                    case 2: // waiting for players
                    {
                        if(numgetmap && gamewaittime > totalmillis && hasmapdata()) break;
                        gamewaittime = totalmillis+G(waitforplayertime);
                        gamewaitstate = 3;
                        sendtick();
                        break;
                    }
                    case 3:
                    {
                        if(numwait && gamewaittime > totalmillis) break;
                        if(totalmillis-gamewaitstart < G(waitforplayermin)) break;
                    }
                    default:
                    {
                        gamewaittime = gamewaitstate = 0;
                        gamestate = G_S_PLAYING;
                        if(m_team(gamemode, mutators)) doteambalance(true);
                        if(m_play(gamemode) && !m_bomber(gamemode) && !m_duke(gamemode, mutators)) // they do their own "fight"
                            sendf(-1, 1, "ri3s", N_ANNOUNCE, S_V_FIGHT, CON_SELF, "match start, fight!");
                        sendtick();
                        break;
                    }
                }
            }
            if(canplay(!paused)) gamemillis += curtime;
            if(m_demo(gamemode)) readdemo();
            else if(canplay(!paused))
            {
                processevents();
                checkents();
                checklimits();
                checkclients();
                if(smode) smode->update();
                mutate(smuts, mut->update());
            }
            if(gs_intermission(gamestate) && gamewaittime <= totalmillis) startintermission(true); // wait then call for next map
            if(shouldcheckvotes) checkvotes();
        }
        else ifserver(shutdownwait)
        {
            srvoutf(4, "server empty, shutting down as scheduled");
            exit(EXIT_SUCCESS);
            return;
        }
        else if(G(rotatecycle) && clocktime-lastrotatecycle >= G(rotatecycle)*60) cleanup();
        aiman::checkai();
        auth::update();
    }

    int lastquerysort = 0;
    bool querysort(const clientinfo *a, const clientinfo *b)
    {
        if(a->state.points > b->state.points) return true;
        if(a->state.points < b->state.points) return false;
        return strcmp(a->name, b->name) < 0;
    }
    vector<clientinfo *> queryplayers;

    int clientconnect(int n, uint ip, bool local)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = local;
        connects.add(ci);
        if(!local && (m_local(gamemode) || servertype <= 0)) return DISC_PRIVATE;
        sendservinit(ci);
        return DISC_NONE;
    }

    void clientdisconnect(int n, bool local, int reason)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        bool complete = !numclients(n);
        if(local)
        {
            if(m_demo(gamemode)) enddemoplayback();
        }
        if(ci->connected)
        {
            if(reason != DISC_SHUTDOWN)
            {
                aiman::removeai(ci, complete);
                if(!complete)
                {
                    aiman::poke();
                    swapteam(ci, ci->team);
                }
                loopv(clients) if(clients[i] != ci)
                {
                    loopvk(clients[i]->state.fraglog) if(clients[i]->state.fraglog[k] == ci->clientnum)
                        clients[i]->state.fraglog.remove(k--);
                }
                if(ci->privilege) auth::setprivilege(ci, -1);
                if(smode) smode->leavegame(ci, true);
                mutate(smuts, mut->leavegame(ci, true));
                savescore(ci);
            }
            sendf(-1, 1, "ri3", N_DISCONNECT, n, reason);
            ci->connected = false;
            if(ci->name[0])
            {
                int amt = numclients(ci->clientnum);
                relayf(2, "\fo%s (%s) has left the game (%s, %d %s)", colourname(ci), gethostname(n), reason >= 0 ? disc_reasons[reason] : "normal", amt, amt != 1 ? "players" : "player");
            }
            clients.removeobj(ci);
            queryplayers.removeobj(ci);
        }
        else connects.removeobj(ci);
        if(complete) cleanup();
        else shouldcheckvotes = true;
        if(n == mapsending)
        {
            if(hasmapdata()) mapsending = -1;
            else
            {
                resetmapdata();
                getmap();
            }
        }
    }

    void queryreply(ucharbuf &req, ucharbuf &p)
    {
        if(!getint(req)) return;
        if(!lastquerysort || totalmillis-lastquerysort >= G(queryinterval))
        {
            queryplayers.setsize(0);
            loopv(clients) if(clients[i]->clientnum >= 0 && clients[i]->name[0] && clients[i]->state.actortype == A_PLAYER) queryplayers.add(clients[i]);
            queryplayers.sort(querysort);
            lastquerysort = totalmillis;
        }
        putint(p, queryplayers.length());
        putint(p, 15); // number of attrs following
        putint(p, VERSION_GAME); // 1
        putint(p, gamemode); // 2
        putint(p, mutators); // 3
        putint(p, timeremaining); // 4
        putint(p, G(serverclients)); // 5
        putint(p, serverpass[0] || G(connectlock) ? MM_PASSWORD : (m_local(gamemode) ? MM_PRIVATE : mastermode)); // 6
        putint(p, numgamevars); // 7
        putint(p, numgamemods); // 8
        putint(p, VERSION_MAJOR); // 9
        putint(p, VERSION_MINOR); // 10
        putint(p, VERSION_PATCH); // 11
        putint(p, versionplatform); // 12
        putint(p, versionarch); // 13
        putint(p, gamestate); // 14
        putint(p, timeleft()); // 15
        sendstring(smapname, p);
        if(*G(serverdesc)) sendstring(G(serverdesc), p);
        else
        {
            #ifdef STANDALONE
            sendstring("", p);
            #else
            const char *cname = client::getname();
            if(!cname || !cname[0]) cname = "";
            sendstring(cname, p);
            #endif
        }
        if(!queryplayers.empty())
        {
            loopv(queryplayers) sendstring(colourname(queryplayers[i]), p);
            loopv(queryplayers) sendstring(queryplayers[i]->handle, p);
        }
        sendqueryreply(p);
    }

    const char *tempmapfile[SENDMAP_MAX] = { "mapmpz", "mappng", "mapcfg", "mapwpt", "maptxt" };
    bool receivefile(int sender, uchar *data, int len)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        ucharbuf p(data, len);
        int type = getint(p), n = getint(p);
        data += p.length();
        len -= p.length();
        if(type != N_SENDMAPFILE) return false;
        if(n < 0 || n >= SENDMAP_MAX)
        {
            srvmsgf(sender, "bad map file type %d");
            return false;
        }
        if(ci->clientnum != mapsending)
        {
            srvmsgf(sender, "sorry, the map isn't needed from you");
            return false;
        }
        if(!len)
        {
            srvmsgf(sender, "you sent a zero length packet for map data");
            return false;
        }
        if(mapdata[n]) DELETEP(mapdata[n]);
        mapdata[n] = opentempfile(tempmapfile[n], "w+b");
        if(!mapdata[n])
        {
            srvmsgf(sender, "failed to open temporary file for map");
            return false;
        }
        mapdata[n]->write(data, len);
        if(n == SENDMAP_ALL) mapsending = -1; // milestone v1.6.0
        return n == SENDMAP_MIN;
    }

    static struct msgfilter
    {
        uchar msgmask[NUMMSG];

        msgfilter(int msg, ...)
        {
            memset(msgmask, 0, sizeof(msgmask));
            va_list msgs;
            va_start(msgs, msg);
            for(uchar val = 1; msg < NUMMSG; msg = va_arg(msgs, int))
            {
                if(msg < 0) val = uchar(-msg);
                else msgmask[msg] = val;
            }
            va_end(msgs);
        }

        uchar operator[](int msg) const { return msg >= 0 && msg < NUMMSG ? msgmask[msg] : 0; }
    } msgfilter(-1, N_CONNECT, N_SERVERINIT, N_CLIENTINIT, N_WELCOME, N_MAPCHANGE, N_SERVMSG, N_DAMAGE, N_SHOTFX, N_LOADW, N_DIED, N_POINTS, N_SPAWNSTATE, N_ITEMACC, N_ITEMSPAWN, N_TICK, N_DISCONNECT, N_CURRENTPRIV, N_PONG, N_RESUME, N_SCOREAFFIN, N_SCORE, N_ANNOUNCE, N_SENDDEMOLIST, N_SENDDEMO, N_DEMOPLAYBACK, N_REGEN, N_CLIENT, N_AUTHCHAL, -2, N_REMIP, N_NEWMAP, N_CLIPBOARD, -3, N_EDITENT, N_EDITLINK, N_EDITVAR, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_EDITVSLOT, N_UNDO, N_REDO, -4, N_POS, N_SPAWN, N_DESTROY, NUMMSG),
      connectfilter(-1, N_CONNECT, -2, N_AUTHANS, -3, N_PING, NUMMSG);

    int checktype(int type, clientinfo *ci)
    {
        if(ci)
        {
            if(!ci->connected) switch(connectfilter[type])
            {
                // allow only before authconnect
                case 1: return !ci->connectauth ? type : -1;
                // allow only during authconnect
                case 2: return ci->connectauth ? type : -1;
                // always allow
                case 3: return type;
                // never allow
                default: return -1;
            }
            if(ci->local) return type;
        }
        switch(msgfilter[type])
        {
            // server-only messages
            case 1: return ci ? -1 : type;
            // only allowed in coop-edit
            case 2: if(m_edit(gamemode) && ci && ci->state.state == CS_EDITING) break; return -1;
            // only allowed in coop-edit, no overflow check
            case 3: return m_edit(gamemode) && ci && ci->state.state == CS_EDITING ? type : -1;
            // no overflow check
            case 4: return type;
        }
        if(ci && !haspriv(ci, G(overflowlock)) && ++ci->overflow >= G(overflowsize)) return -2;
        return type;
    }

    struct worldstate
    {
        int uses, len;
        uchar *data;

        worldstate() : uses(0), len(0), data(NULL) {}

        void setup(int n) { len = n; data = new uchar[n]; }
        void cleanup() { DELETEA(data); len = 0; }
        bool contains(const uchar *p) const { return p >= data && p < &data[len]; }
    };
    vector<worldstate> worldstates;
    bool reliablemessages = false;

    void cleanworldstate(ENetPacket *packet)
    {
        loopv(worldstates)
        {
            worldstate &ws = worldstates[i];
            if(!ws.contains(packet->data)) continue;
            ws.uses--;
            if(ws.uses <= 0)
            {
                ws.cleanup();
                worldstates.removeunordered(i);
            }
            break;
        }
    }

    static void sendpositions(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(0, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.actortype != A_PLAYER) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 0, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addposition(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.position.empty()) return;
        if(wsbuf.length() + bi.position.length() > mtu) sendpositions(ws, wsbuf);
        int offset = wsbuf.length();
        wsbuf.put(bi.position.getbuf(), bi.position.length());
        bi.position.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    static void sendmessages(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(1, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.actortype != A_PLAYER) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 1, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addmessages(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.messages.empty()) return;
        if(wsbuf.length() + 10 + bi.messages.length() > mtu) sendmessages(ws, wsbuf);
        int offset = wsbuf.length();
        putint(wsbuf, N_CLIENT);
        putint(wsbuf, bi.clientnum);
        putuint(wsbuf, bi.messages.length());
        wsbuf.put(bi.messages.getbuf(), bi.messages.length());
        bi.messages.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    bool buildworldstate()
    {
        int wsmax = 0;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            ci.overflow = 0;
            ci.wsdata = NULL;
            wsmax += ci.position.length();
            if(ci.messages.length()) wsmax += 10 + ci.messages.length();
        }
        if(wsmax <= 0)
        {
            reliablemessages = false;
            return false;
        }
        worldstate &ws = worldstates.add();
        ws.setup(2*wsmax);
        int mtu = getservermtu() - 100;
        if(mtu <= 0) mtu = ws.len;
        ucharbuf wsbuf(ws.data, ws.len);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.actortype != A_PLAYER) continue;
            addposition(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addposition(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendpositions(ws, wsbuf);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.actortype != A_PLAYER) continue;
            addmessages(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addmessages(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendmessages(ws, wsbuf);
        reliablemessages = false;
        if(ws.uses) return true;
        ws.cleanup();
        worldstates.drop();
        return false;
    }

    bool sendpackets(bool force)
    {
        if(clients.empty() || (!hasnonlocalclients() && !demorecord)) return false;
        enet_uint32 millis = enet_time_get()-lastsend;
        if(millis<40 && !force) return false;
        bool flush = buildworldstate();
        lastsend += millis - (millis%40);
        return flush;
    }

    void sendclipboard(clientinfo *ci)
    {
        if(!ci->lastclipboard || !ci->clipboard) return;
        bool flushed = false;
        loopv(clients)
        {
            clientinfo &e = *clients[i];
            if(e.clientnum != ci->clientnum && e.needclipboard - ci->lastclipboard >= 0)
            {
                if(!flushed) { flushserver(true); flushed = true; }
                sendpacket(e.clientnum, 1, ci->clipboard);
            }
        }
    }

    void connected(clientinfo *ci)
    {
        if(!m_demo(gamemode) && !numclients() && demonextmatch) setupdemorecord();

        connects.removeobj(ci);
        clients.add(ci);

        ci->connected = true;
        ci->needclipboard = totalmillis ? totalmillis : 1;
        ci->state.lasttimeplayed = totalmillis;
        ci->state.lasttimealive = totalmillis;
        ci->state.lasttimewielded = totalmillis;
        loopi(W_MAX) ci->state.lasttimeloadout[i] = totalmillis;

        if(ci->handle[0]) // kick old logins
        {
            loopvrev(clients) if(clients[i] != ci && clients[i]->handle[0] && !strcmp(clients[i]->handle, ci->handle))
                disconnect_client(clients[i]->clientnum, DISC_KICK);
        }
        sendwelcome(ci);
        if(restorescore(ci)) sendresume(ci);
        sendinitclient(ci);
        int amt = numclients();
        if((ci->privilege&PRIV_TYPE) > PRIV_NONE)
        {
            if(ci->handle[0]) relayf(2, "\fg%s (%s) has joined the game (\fs\fy%s\fS: \fs\fc%s\fS) [%d.%d.%d-%s%d] (%d %s)", colourname(ci), gethostname(ci->clientnum), privname(ci->privilege), ci->handle, ci->state.version.major, ci->state.version.minor, ci->state.version.patch, plat_name(ci->state.version.platform), ci->state.version.arch, amt, amt != 1 ? "players" : "player");
            else relayf(2, "\fg%s (%s) has joined the game (\fs\fy%s\fS) [%d.%d.%d-%s%d] (%d %s)", colourname(ci), gethostname(ci->clientnum), privname(ci->privilege), ci->state.version.major, ci->state.version.minor, ci->state.version.patch, plat_name(ci->state.version.platform), ci->state.version.arch, amt, amt != 1 ? "players" : "player");
        }
        else relayf(2, "\fg%s (%s) has joined the game [%d.%d.%d-%s%d] (%d %s)", colourname(ci), gethostname(ci->clientnum), ci->state.version.major, ci->state.version.minor, ci->state.version.patch, plat_name(ci->state.version.platform), ci->state.version.arch, amt, amt != 1 ? "players" : "player");

        if(hasmapdata()) srvmsgft(ci->clientnum, CON_SELF, "\fythe server map crc for \fs\fc%s\fS is: \fs\fc0x%.8x\fS", smapname, mapcrc);

        if(m_demo(gamemode)) setupdemoplayback();
        else if(m_edit(gamemode))
        {
            ci->ready = true;
            aiman::poke();
        }
    }

    void parsepacket(int sender, int chan, packetbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0 || p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED || chan > 2) return;
        char text[MAXTRANS];
        int type = -1, prevtype = -1;
        clientinfo *ci = sender>=0 ? (clientinfo *)getinfo(sender) : NULL;
        if(ci && !ci->connected)
        {
            if(chan == 0) return;
            else if(chan != 1)
            {
                conoutf("\fy[msg error] from: %d, chan: %d while connecting", sender, chan);
                disconnect_client(sender, DISC_MSGERR);
                return;
            }
            else while(p.length() < p.maxlen)
            {
                int curtype = getint(p);
                prevtype = type;
                switch(type = checktype(curtype, ci))
                {
                    case N_CONNECT:
                    {
                        getstring(text, p);
                        string namestr = "";
                        filterstring(namestr, text, true, true, true, true, MAXNAMELEN);
                        if(!*namestr) copystring(namestr, "unnamed");
                        copystring(ci->name, namestr, MAXNAMELEN+1);
                        ci->state.colour = max(getint(p), 0);
                        ci->state.model = max(getint(p), 0);
                        getstring(text, p);
                        ci->state.setvanity(text);
                        int lw = getint(p);
                        ci->state.loadweap.shrink(0);
                        loopk(lw)
                        {
                            if(k >= W_LOADOUT) getint(p);
                            else ci->state.loadweap.add(getint(p));
                        }

                        string password = "", authname = "";
                        getstring(password, p);
                        getstring(text, p);
                        filterstring(authname, text, true, true, true, true, 100);

                        ci->state.version.get(p);

                        int disc = auth::allowconnect(ci, authname, password);
                        if(disc)
                        {
                            disconnect_client(sender, disc);
                            return;
                        }

                        if(!ci->connectauth) connected(ci);

                        break;
                    }

                    case N_AUTHANS:
                    {
                        uint id = (uint)getint(p);
                        getstring(text, p);
                        if(!auth::answerchallenge(ci, id, text)) auth::authfailed(ci->authreq);
                        break;
                    }

                    case N_PING:
                        getint(p);
                        break;

                    default:
                        conoutf("\fy[msg error] from: %d, cur: %d, msg: %d, prev: %d", sender, curtype, type, prevtype);
                        disconnect_client(sender, DISC_MSGERR);
                        return;
                }
            }
            return;
        }
        else if(chan == 2)
        {
            if(receivefile(sender, p.buf, p.maxlen))
            {
                if(hasmapdata()) loopv(clients)
                {
                    clientinfo *cs = clients[i];
                    if(cs->state.actortype > A_PLAYER || !cs->online || !cs->name[0] || !cs->ready) continue;
                    if(cs->wantsmap || crclocked(cs, true)) getmap(cs);
                }
                // milestone v1.6.0 - sendf(-1, 1, "ri", N_SENDMAP);
            }
            return;
        }
        if(p.packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;
        #define QUEUE_MSG { if(ci && (!ci->local || demorecord || hasnonlocalclients())) while(curmsg<p.length()) ci->messages.add(p.buf[curmsg++]); }
        #define QUEUE_BUF(body) { \
            if(ci && (!ci->local || demorecord || hasnonlocalclients())) \
            { \
                curmsg = p.length(); \
                { body; } \
            } \
        }
        #define QUEUE_INT(n) QUEUE_BUF(putint(ci->messages, n))
        #define QUEUE_UINT(n) QUEUE_BUF(putuint(ci->messages, n))
        #define QUEUE_FLT(n) QUEUE_BUF(putfloat(ci->messages, n))
        #define QUEUE_STR(text) QUEUE_BUF(sendstring(text, ci->messages))

        int curmsg;
        while((curmsg = p.length()) < p.maxlen)
        {
            int curtype = getint(p);
            prevtype = type;
            switch(type = checktype(curtype, ci))
            {
                case N_POS:
                {
                    int lcn = getuint(p);
                    if(lcn<0)
                    {
                        disconnect_client(sender, DISC_CN);
                        return;
                    }

                    bool havecn = true;
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) havecn = false;

                    p.get();
                    getuint(p);
                    uint flags = getuint(p);
                    vec pos, vel, falling;
                    float yaw, pitch, roll;
                    loopk(3)
                    {
                        int n = p.get(); n |= p.get()<<8; if(flags&(1<<k)) { n |= p.get()<<16; if(n&0x800000) n |= -1<<24; }
                        pos[k] = n/DMF;
                    }
                    int dir = p.get(); dir |= p.get()<<8;
                    yaw = dir%360;
                    pitch = clamp(dir/360, 0, 180)-90;
                    roll = clamp(int(p.get()), 0, 180)-90;
                    int mag = p.get(); if(flags&(1<<3)) mag |= p.get()<<8;
                    dir = p.get(); dir |= p.get()<<8;
                    vecfromyawpitch(dir%360, clamp(dir/360, 0, 180)-90, 1, 0, vel);
                    vel.mul(mag/DVELF);
                    if(flags&(1<<4))
                    {
                        mag = p.get(); if(flags&(1<<5)) mag |= p.get()<<8;
                        if(flags&(1<<6))
                        {
                            dir = p.get(); dir |= p.get()<<8;
                            vecfromyawpitch(dir%360, clamp(dir/360, 0, 180)-90, 1, 0, falling);
                        }
                        else falling = vec(0, 0, -1);
                        falling.mul(mag/DVELF);
                    }
                    else falling = vec(0, 0, 0);
                    if(havecn)
                    {
                        vec oldpos = cp->state.o;
                        cp->state.o = pos;
                        cp->state.vel = vel;
                        cp->state.falling = falling;
                        cp->state.yaw = yaw;
                        cp->state.pitch = pitch;
                        cp->state.roll = roll;
                        if((!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                        {
                            cp->position.setsize(0);
                            while(curmsg<p.length()) cp->position.add(p.buf[curmsg++]);
                        }
                        if(cp->state.state==CS_ALIVE)
                        {
                            if(smode) smode->moved(cp, oldpos, cp->state.o);
                            mutate(smuts, mut->moved(cp, oldpos, cp->state.o));
                        }
                    }
                    break;
                }

                case N_SPHY:
                {
                    int lcn = getint(p), idx = getint(p);
                    if(idx >= SPHY_SERVER) break; // clients can't send this
                    if(idx == SPHY_COOK) loopj(2) getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    if(idx == SPHY_COOK)
                    {
                        if(!cp->state.isalive(gamemillis))
                        {
                            if(G(serverdebug)) srvmsgf(cp->clientnum, "sync error: power [%d] failed - unexpected message", cp->state.weapselect);
                            break;
                        }
                        if(cp->state.weapstate[cp->state.weapselect] == W_S_RELOAD && !cp->state.weapwaited(cp->state.weapselect, gamemillis))
                        {
                            if(!cp->state.weapwaited(cp->state.weapselect, gamemillis, (1<<W_S_RELOAD)))
                            {
                                if(!cp->state.hasweap(cp->state.weapselect, m_weapon(gamemode, mutators))) cp->state.entid[cp->state.weapselect] = -1; // its gone..
                                if(G(serverdebug)) srvmsgf(cp->clientnum, "sync error: power [%d] failed - current state disallows it", cp->state.weapselect);
                                break;
                            }
                            else if(cp->state.weapload[cp->state.weapselect] > 0)
                            {
                                takeammo(cp, cp->state.weapselect, cp->state.weapload[cp->state.weapselect]);
                                cp->state.weapload[cp->state.weapselect] = -cp->state.weapload[cp->state.weapselect];
                                sendf(-1, 1, "ri5x", N_RELOAD, cp->clientnum, cp->state.weapselect, cp->state.weapload[cp->state.weapselect], cp->state.ammo[cp->state.weapselect], cp->clientnum);
                            }
                            else break;
                        }
                    }
                    else if(idx == SPHY_EXTINGUISH)
                    {
                        if(!cp->state.burning(gamemillis, G(burntime))) break;
                        cp->state.lastres[WR_BURN] = cp->state.lastrestime[WR_BURN] = 0;
                    }
                    else if(idx == SPHY_BOOST || idx == SPHY_DASH)
                    {
                        if(!cp->state.isalive(gamemillis) || (cp->state.lastboost && gamemillis-cp->state.lastboost <= G(impulseboostdelay))) break;
                        cp->state.lastboost = gamemillis;
                    }
                    QUEUE_MSG;
                    break;
                }

                case N_EDITMODE:
                {
                    int val = getint(p);
                    if(!ci || ci->state.actortype > A_PLAYER) break;
                    if(!allowstate(ci, val ? ALST_EDIT : ALST_WALK, G(editlock)))
                    {
                        if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: unable to switch state %s - %d [%d, %d]", colourname(ci), ci->state.state, ci->state.lastdeath, gamemillis);
                        spectator(ci);
                        break;
                    }
                    ci->state.editspawn(gamemode, mutators);
                    if(val)
                    {
                        if(smode) smode->leavegame(ci);
                        mutate(smuts, mut->leavegame(ci));
                        ci->state.state = CS_EDITING;
                        ci->events.deletecontents();
                    }
                    else
                    {
                        ci->state.state = CS_ALIVE;
                        if(smode) smode->entergame(ci);
                        mutate(smuts, mut->entergame(ci));
                    }
                    QUEUE_MSG;
                    break;
                }

                case N_MAPCRC:
                {
                    getstring(text, p);
                    int crc = getint(p);
                    if(!ci) break;
                    copystring(ci->clientmap, text);
                    ci->mapcrc = text[0] ? crc : 0;
                    ci->ready = true;
                    ci->wantsmap = ci->gettingmap = false;
                    if(!m_edit(gamemode))
                    {
                        if(hasmapdata()) srvoutf(4, "\fy%s has map crc: \fs\fc0x%.8x\fS (server: \fs\fc0x%.8x\fS)", colourname(ci), ci->mapcrc, mapcrc); // milestone v1.6.0
                        else srvoutf(4, "\fy%s has map crc: \fs\fc0x%.8x\fS", colourname(ci), ci->mapcrc);
                    }
                    getmap(crclocked(ci, true) ? ci : NULL);
                    if(ci->isready()) aiman::poke();
                    break;
                }

                case N_CHECKMAPS:
                    break;

                case N_TRYSPAWN:
                {
                    int lcn = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    if(!allowstate(cp, ALST_TRY, m_edit(gamemode) ? G(spawneditlock) : G(spawnlock)))
                    {
                        if(G(serverdebug)) srvmsgf(cp->clientnum, "sync error: unable to spawn %s - %d [%d, %d]", colourname(cp), cp->state.state, cp->state.lastdeath, gamemillis);
                        //spectator(cp);
                        break;
                    }
                    int nospawn = 0;
                    if(smode && !smode->canspawn(cp, true)) { nospawn++; }
                    mutate(smuts, if(!mut->canspawn(cp, true)) { nospawn++; });
                    if(!nospawn)
                    {
                        cp->state.state = CS_DEAD;
                        waiting(cp, DROP_RESET);
                    }
                    break;
                }

                case N_WSELECT:
                {
                    int lcn = getint(p), id = getint(p), weap = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci) || !isweap(weap)) break;
                    switchevent *ev = new switchevent;
                    ev->id = id;
                    ev->weap = weap;
                    ev->millis = cp->getmillis(gamemillis, ev->id);
                    cp->addevent(ev);
                    break;
                }

                case N_SPAWN:
                {
                    int lcn = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    if(!allowstate(cp, ALST_SPAWN))
                    {
                        if(G(serverdebug)) srvmsgf(cp->clientnum, "sync error: unable to spawn %s - %d [%d, %d]", colourname(cp), cp->state.state, cp->state.lastdeath, gamemillis);
                        //spectator(cp);
                        break;
                    }
                    cp->state.lastrespawn = -1;
                    cp->state.state = CS_ALIVE;
                    if(smode) smode->spawned(cp);
                    mutate(smuts, mut->spawned(cp););
                    QUEUE_BUF({
                        putint(ci->messages, N_SPAWN);
                        putint(ci->messages, cp->clientnum);
                        sendstate(cp->state, ci->messages);
                    });
                    break;
                }

                case N_SUICIDE:
                {
                    int lcn = getint(p), flags = getint(p), material = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    suicideevent *ev = new suicideevent;
                    ev->flags = flags;
                    ev->material = material;
                    cp->addevent(ev);
                    break;
                }

                case N_SHOOT:
                {
                    int lcn = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    bool havecn = hasclient(cp, ci);
                    shotevent *ev = new shotevent;
                    ev->id = getint(p);
                    ev->weap = getint(p);
                    ev->flags = getint(p);
                    ev->scale = getint(p);
                    if(!isweap(ev->weap)) havecn = false;
                    else
                    {
                        ev->scale = clamp(ev->scale, 0, W2(ev->weap, cooktime, WS(ev->flags)));
                        if(havecn) ev->millis = cp->getmillis(gamemillis, ev->id);
                    }
                    loopk(3) ev->from[k] = getint(p);
                    ev->num = getint(p);
                    loopj(ev->num)
                    {
                        if(p.overread()) break;
                        if(j >= MAXPARAMS || !havecn)
                        {
                            loopk(4) getint(p);
                            continue;
                        }
                        shotmsg &s = ev->shots.add();
                        s.id = getint(p);
                        loopk(3) s.pos[k] = getint(p);
                    }
                    if(havecn)
                    {
                        int rays = min(W2(ev->weap, rays, WS(ev->flags)), MAXPARAMS);
                        if(rays > 1 && W2(ev->weap, cooktime, WS(ev->flags))) rays = int(ceilf(rays*ev->scale/float(W2(ev->weap, cooktime, WS(ev->flags)))));
                        while(ev->shots.length() > rays) ev->shots.remove(rnd(ev->shots.length()));
                        cp->addevent(ev);
                        cp->state.lastshoot = gamemillis;
                    }
                    else delete ev;
                    break;
                }

                case N_DROP:
                {
                    int lcn = getint(p), id = getint(p), weap = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    dropevent *ev = new dropevent;
                    ev->id = id;
                    ev->weap = weap;
                    ev->millis = cp->getmillis(gamemillis, ev->id);
                    cp->events.add(ev);
                    break;
                }

                case N_RELOAD:
                {
                    int lcn = getint(p), id = getint(p), weap = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    reloadevent *ev = new reloadevent;
                    ev->id = id;
                    ev->weap = weap;
                    ev->millis = cp->getmillis(gamemillis, ev->id);
                    cp->events.add(ev);
                    break;
                }

                case N_DESTROY:
                {
                    int lcn = getint(p), millis = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    bool havecn = hasclient(cp, ci);
                    destroyevent *ev = new destroyevent;
                    ev->weap = getint(p);
                    ev->flags = getint(p);
                    if(havecn) ev->millis = cp->getmillis(gamemillis, millis);
                    ev->id = getint(p);
                    ev->radial = getint(p);
                    ev->scale = getint(p);
                    int hits = getint(p);
                    loopj(hits)
                    {
                        if(p.overread()) break;
                        static hitset dummy;
                        hitset &hit = havecn && j < 100 ? ev->hits.add() : dummy;
                        hit.flags = getint(p);
                        hit.proj = getint(p);
                        hit.target = getint(p);
                        hit.dist = max(getint(p), 0);
                        loopk(3) hit.dir[k] = getint(p);
                        loopk(3) hit.vel[k] = getint(p);
                    }
                    if(havecn) cp->events.add(ev);
                    else delete ev;
                    break;
                }

                case N_STICKY:
                {
                    int lcn = getint(p), millis = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    bool havecn = hasclient(cp, ci);
                    stickyevent *ev = new stickyevent;
                    ev->weap = getint(p);
                    ev->flags = getint(p);
                    if(havecn) ev->millis = cp->getmillis(gamemillis, millis);
                    ev->id = getint(p);
                    ev->target = getint(p);
                    loopk(3) ev->norm[k] = getint(p);
                    loopk(3) ev->pos[k] = getint(p);
                    if(havecn) cp->events.add(ev);
                    else delete ev;
                    break;
                }

                case N_ITEMUSE:
                {
                    int lcn = getint(p), id = getint(p), ent = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    useevent *ev = new useevent;
                    ev->id = id;
                    ev->ent = ent;
                    ev->millis = cp->getmillis(gamemillis, ev->id);
                    cp->events.add(ev);
                    break;
                }

                case N_TRIGGER:
                {
                    int lcn = getint(p), ent = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci) || cp->state.state != CS_ALIVE) break;
                    if(sents.inrange(ent))
                    {
                        if(sents[ent].type == CHECKPOINT)
                        {
                            if(sents[ent].attrs[5] && sents[ent].attrs[5] != triggerid) break;
                            if(!m_check(sents[ent].attrs[3], sents[ent].attrs[4], gamemode, mutators)) break;
                            if(!m_race(gamemode) || (m_gsp3(gamemode, mutators) && cp->team != T_ALPHA)) break;
                            if(cp->state.cpnodes.find(ent) >= 0) break;
                            switch(sents[ent].attrs[6])
                            {
                                case CP_LAST: case CP_FINISH:
                                {
                                    if(cp->state.cpmillis)
                                    {
                                        int laptime = gamemillis-cp->state.cpmillis, total = 0;
                                        if(cp->state.cptime <= 0 || laptime < cp->state.cptime) cp->state.cptime = laptime;
                                        cp->state.points++;
                                        sendf(-1, 1, "ri6", N_CHECKPOINT, cp->clientnum, ent, laptime, cp->state.cptime, cp->state.points);
                                        if(m_team(gamemode, mutators))
                                        {
                                            if(m_laptime(gamemode, mutators))
                                            {
                                                score &ts = teamscore(cp->team);
                                                if(!ts.total || ts.total > cp->state.cptime)
                                                {
                                                    total = ts.total = cp->state.cptime;
                                                    sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                                                }
                                            }
                                            else
                                            {
                                                score &ts = teamscore(cp->team);
                                                total = ++ts.total;
                                                sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                                            }
                                            if(total && m_gsp3(gamemode, mutators) && G(racegauntletwinner))
                                            {
                                                int numt = numteams(gamemode, mutators);
                                                if(curbalance == numt-1)
                                                {
                                                    bool found = false;
                                                    loopi(numt)
                                                    {
                                                        int t = i+T_FIRST, s = teamscore(t).total;
                                                        if(t != T_OMEGA && (m_laptime(gamemode, mutators) ? s <= total : s >= total))
                                                        {
                                                            found = true;
                                                            break;
                                                        }
                                                    }
                                                    if(!found)
                                                    {
                                                        ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fybest score has been reached");
                                                        startintermission();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else waiting(cp);
                                    cp->state.cpmillis = 0;
                                    cp->state.cpnodes.shrink(0);
                                    if(sents[ent].attrs[6] == CP_FINISH) waiting(cp);
                                    break;
                                }
                                case CP_START: case CP_RESPAWN:
                                {
                                    if(cp->state.cpnodes.find(ent) >= 0) break;
                                    if(sents[ent].attrs[6] == CP_START)
                                    {
                                        if(cp->state.cpmillis) break;
                                        cp->state.cpmillis = gamemillis;
                                    }
                                    else if(!cp->state.cpmillis)
                                    {
                                        waiting(cp);
                                        break;
                                    }
                                    sendf(-1, 1, "ri4", N_CHECKPOINT, cp->clientnum, ent, -1);
                                    cp->state.cpnodes.add(ent);
                                }
                                default: break;
                            }
                        }
                        else if(sents[ent].type == TRIGGER)
                        {
                            if(sents[ent].attrs[4] && sents[ent].attrs[4] != triggerid) break;
                            if(!m_check(sents[ent].attrs[5], sents[ent].attrs[6], gamemode, mutators)) break;
                            bool commit = false, kin = false;
                            switch(sents[ent].attrs[1])
                            {
                                case TR_TOGGLE:
                                {
                                    sents[ent].millis = gamemillis+(triggertime(ent)*2);
                                    sents[ent].spawned = !sents[ent].spawned;
                                    commit = kin = true;
                                    break;
                                }
                                case TR_ONCE: if(sents[ent].spawned) break;
                                case TR_LINK:
                                {
                                    sents[ent].millis = gamemillis+(triggertime(ent)*2);
                                    kin = true;
                                    if(!sents[ent].spawned)
                                    {
                                        sents[ent].spawned = true;
                                        commit = true;
                                    }
                                    break;
                                }
                                case TR_EXIT:
                                {
                                    if(sents[ent].spawned) break;
                                    sents[ent].spawned = true;
                                }
                            }
                            if(commit) sendf(-1, 1, "ri3x", N_TRIGGER, ent, sents[ent].spawned ? 1 : 0, cp->clientnum);
                            if(kin) loopvj(sents[ent].kin) if(sents.inrange(sents[ent].kin[j]))
                            {
                                if(sents[sents[ent].kin[j]].type == TRIGGER && !m_check(sents[sents[ent].kin[j]].attrs[5], sents[sents[ent].kin[j]].attrs[6], gamemode, mutators))
                                    continue;
                                sents[sents[ent].kin[j]].spawned = sents[ent].spawned;
                                sents[sents[ent].kin[j]].millis = sents[ent].millis;
                            }
                        }
                    }
                    else if(G(serverdebug)) srvmsgf(cp->clientnum, "sync error: cannot trigger %d - not a trigger", ent);
                    break;
                }

                case N_TEXT:
                {
                    int fcn = getint(p), tcn = getint(p), flags = getint(p);
                    getstring(text, p);
                    clientinfo *fcp = (clientinfo *)getinfo(fcn);
                    clientinfo *tcp = (clientinfo *)getinfo(tcn);
                    if(!hasclient(fcp, ci)) break;
                    if(!haspriv(fcp, G(messagelock), "send messages on this server")) break;
                    uint ip = getclientip(fcp->clientnum);
                    if(ip && checkipinfo(control, ipinfo::MUTE, ip) && !checkipinfo(control, ipinfo::EXCEPT, ip) && !haspriv(fcp, G(mutelock), "send messages while muted")) break;
                    if(G(floodlock))
                    {
                        int numlines = 0;
                        loopvrev(fcp->state.chatmillis)
                        {
                            if(totalmillis-fcp->state.chatmillis[i] <= G(floodtime)) numlines++;
                            else fcp->state.chatmillis.remove(i);
                        }
                        if(numlines >= G(floodlines))
                        {
                            if((!fcp->state.warnings[WARN_CHAT][1] || totalmillis-fcp->state.warnings[WARN_CHAT][1] >= 1000) && !haspriv(fcp, G(floodlock), "send too many messages consecutively"))
                            {
                                fcp->state.warnings[WARN_CHAT][0]++;
                                fcp->state.warnings[WARN_CHAT][1] = totalmillis ? totalmillis : 1;
                                if(ip && G(floodmute) && fcp->state.warnings[WARN_CHAT][0] >= G(floodmute) && !checkipinfo(control, ipinfo::EXCEPT, ip) && !haspriv(fcp, G(mutelock)))
                                {
                                    ipinfo &c = control.add();
                                    c.ip = ip;
                                    c.mask = 0xFFFFFFFF;
                                    c.type = ipinfo::MUTE;
                                    c.flag = ipinfo::INTERNAL;
                                    c.time = totalmillis ? totalmillis : 1;
                                    c.reason = newstring("exceeded the number of allowed flood warnings");
                                    srvoutf(-3, "\fs\fcmute\fS added on %s: %s", colourname(fcp), c.reason);
                                }
                            }
                            break;
                        }
                        fcp->state.chatmillis.add(totalmillis ? totalmillis : 1);
                    }
                    bigstring output;
                    copystring(output, text, G(messagelength));
                    filterstring(text, text, true, true, true, true, G(messagelength));
                    if(*(G(censorwords))) filterword(output, G(censorwords));
                    if(flags&SAY_TEAM && !m_team(gamemode, mutators)) flags &= ~SAY_TEAM;
                    sendf(-1, -1, "ri4s", N_TEXT, fcp->clientnum, tcp ? tcp->clientnum : -1, flags, output); // sent to negative chan for recordpacket
                    if(flags&SAY_WHISPER && tcp)
                    {
                        int scn = allowbroadcast(tcp->clientnum) ? tcp->clientnum : tcp->state.ownernum;
                        if(allowbroadcast(scn)) sendf(scn, 1, "ri4s", N_TEXT, fcp->clientnum, tcp->clientnum, flags, output);
                        if(allowbroadcast(fcp->clientnum) && scn != fcp->clientnum)
                            sendf(fcp->clientnum, 1, "ri4s", N_TEXT, fcp->clientnum, tcp->clientnum, flags, output);
                    }
                    else
                    {
                        static vector<int> sentto;
                        sentto.setsize(0);
                        loopv(clients)
                        {
                            clientinfo *t = clients[i];
                            if(flags&SAY_TEAM && fcp->team != t->team) continue;
                            int scn = t->clientnum;
                            if(!allowbroadcast(scn) && t->state.ownernum >= 0)
                            {
                                if(strncmp(text, "bots", 4))
                                {
                                    size_t len = strlen(t->name);
                                    if(!len || strncasecmp(text, t->name, len)) continue;
                                    switch(text[len])
                                    {
                                        case 0: break;
                                        case ':': case ',': case ';': len++; break;
                                        default: continue;
                                    }
                                    if(text[len] != 0) continue;
                                }
                                scn = t->state.ownernum;
                            }
                            if(!allowbroadcast(scn) || sentto.find(scn) >= 0) continue;
                            sendf(scn, 1, "ri4s", N_TEXT, fcp->clientnum, tcp ? tcp->clientnum : -1, flags, output);
                            sentto.add(scn);
                        }
                        defformatstring(m, "%s", colourname(fcp));
                        if(flags&SAY_TEAM)
                        {
                            defformatstring(t, " (to team %s)", colourteam(fcp->team));
                            concatstring(m, t);
                        }
                        if(flags&SAY_ACTION) relayf(0, "\fv* %s %s", m, output);
                        else relayf(0, "\fw<%s> %s", m, output);
                    }
                    break;
                }

                case N_COMMAND:
                {
                    int lcn = getint(p), nargs = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    getstring(text, p);
                    int alen = getint(p);
                    if(alen < 0 || alen > p.remaining()) break;
                    char *arg = newstring(alen);
                    getstring(arg, p, alen+1);
                    if(hasclient(cp, ci)) parsecommand(cp, nargs, text, arg);
                    delete[] arg;
                    break;
                }

                case N_SETPLAYERINFO:
                {
                    uint ip = getclientip(ci->clientnum);
                    if(ci->lastplayerinfo)
                    {
                        bool allow = true;
                        if(!haspriv(ci, G(setinfolock), "change player info on this server")) allow = false;
                        else if(ip && checkipinfo(control, ipinfo::MUTE, ip) && !checkipinfo(control, ipinfo::EXCEPT, ip) && !haspriv(ci, G(mutelock), "change player info while muted")) allow = false;
                        else if(totalmillis-ci->lastplayerinfo < G(setinfowait)) allow = false;
                        if(!allow)
                        {
                            getstring(text, p);
                            loopk(2) getint(p);
                            getstring(text, p);
                            int w = getint(p);
                            loopk(w) getint(p);
                            sendinitclientself(ci);
                            break;
                        }
                    }
                    QUEUE_MSG;
                    defformatstring(oldname, "%s", colourname(ci));
                    getstring(text, p);
                    string namestr = "";
                    filterstring(namestr, text, true, true, true, true, MAXNAMELEN);
                    if(!*namestr) copystring(namestr, "unnamed");
                    if(strcmp(ci->name, namestr))
                    {
                        copystring(ci->name, namestr, MAXNAMELEN+1);
                        relayf(2, "\fm* %s is now known as %s", oldname, colourname(ci));
                    }
                    ci->state.colour = max(getint(p), 0);
                    ci->state.model = max(getint(p), 0);
                    getstring(text, p);
                    ci->state.setvanity(text);
                    ci->state.loadweap.shrink(0);
                    int lw = getint(p);
                    vector<int> lweaps;
                    loopk(lw)
                    {
                        if(k >= W_LOADOUT) getint(p);
                        else ci->state.loadweap.add(getint(p));
                    }
                    ci->lastplayerinfo = totalmillis;
                    QUEUE_STR(ci->name);
                    QUEUE_INT(ci->state.colour);
                    QUEUE_INT(ci->state.model);
                    QUEUE_STR(ci->state.vanity);
                    QUEUE_INT(ci->state.loadweap.length());
                    loopvk(ci->state.loadweap) QUEUE_INT(ci->state.loadweap[k]);
                    break;
                }

                case N_SWITCHTEAM:
                {
                    int team = getint(p);
                    if(!m_team(gamemode, mutators) || ci->state.actortype >= A_ENEMY || !isteam(gamemode, mutators, team, T_FIRST)) break;
                    if(team == ci->team)
                    {
                        if(ci->swapteam)
                        {
                            if(m_swapteam(gamemode, mutators))
                                srvoutf(4, "\fy%s no longer wishes to swap to team %s", colourname(ci), colourteam(ci->swapteam));
                            ci->swapteam = T_NEUTRAL;
                        }
                        break;
                    }
                    uint ip = getclientip(ci->clientnum);
                    if(ip && checkipinfo(control, ipinfo::LIMIT, ip) && !checkipinfo(control, ipinfo::EXCEPT, ip) && !haspriv(ci, G(limitlock), "change teams while limited")) break;
                    int newteam = requestswap(ci, team);
                    if(newteam != team || newteam == ci->team) break;
                    bool reset = true;
                    if(ci->state.state == CS_SPECTATOR)
                    {
                        if(!allowstate(ci, ALST_TRY, m_edit(gamemode) ? G(spawneditlock) : G(spawnlock)))
                        {
                            if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: unable to spawn %s - %d [%d, %d]", colourname(ci), ci->state.state, ci->state.lastdeath, gamemillis);
                            spectator(ci);
                            break;
                        }
                        if(!spectate(ci, false)) break;
                        reset = false;
                    }
                    setteam(ci, newteam, (reset ? TT_RESET : 0)|TT_INFOSM);
                    break;
                }

                case N_MAPVOTE:
                {
                    getstring(text, p);
                    filterstring(text, text);
                    const char *s = text;
                    if(!strncasecmp(s, "maps/", 5) || !strncasecmp(s, "maps\\", 5)) s += 5;
                    int reqmode = getint(p), reqmuts = getint(p);
                    vote(s, reqmode, reqmuts, sender);
                    break;
                }

                case N_CLEARVOTE:
                {
                    if(ci->mapvote[0])
                    {
                        ci->mapvote[0] = 0;
                        ci->modevote = ci->mutsvote = -1;
                        sendf(-1, 1, "ri2", N_CLEARVOTE, ci->clientnum);
                    }
                    break;
                }

                case N_GAMEINFO:
                {
                    bool skip = hasgameinfo || crclocked(ci);
                    int n;
                    while((n = getint(p)) != -1)
                    {
                        if(p.overread()) break;
                        getstring(text, p);
                        defformatstring(cmdname, "sv_%s", text);
                        ident *id = idents.access(cmdname);
                        if(!skip && id && id->flags&IDF_SERVER && id->flags&IDF_WORLD && n == id->type)
                        {
                            switch(id->type)
                            {
                                case ID_VAR:
                                {
                                    int ret = getint(p);
                                    if(ret < id->minval || ret > id->maxval) break;
                                    *id->storage.i = ret;
                                    id->changed();
                                    break;
                                }
                                case ID_FVAR:
                                {
                                    float ret = getfloat(p);
                                    if(ret < id->minvalf || ret > id->maxvalf) break;
                                    *id->storage.f = ret;
                                    id->changed();
                                    break;
                                }
                                case ID_SVAR:
                                {
                                    getstring(text, p);
                                    delete[] *id->storage.s;
                                    *id->storage.s = newstring(text);
                                    id->changed();
                                    break;
                                }
                                default: break;
                            }
                        }
                        else switch(n)
                        {
                            case ID_VAR: getint(p); break;
                            case ID_FVAR: getfloat(p); break;
                            case ID_SVAR: getstring(text, p); break;
                            default: break;
                        }
                    }
                    while((n = getint(p)) != -1)
                    {
                        int type = getint(p), numattr = getint(p);
                        if(p.overread() || type < 0 || type >= MAXENTTYPES || n < 0 || n >= MAXENTS) break;
                        if(!skip && enttype[type].syncs)
                        {
                            while(sents.length() <= n) sents.add();
                            sents[n].reset();
                            sents[n].type = type;
                            sents[n].spawned = false; // wait a bit then load 'em up
                            sents[n].millis = gamemillis;
                            sents[n].attrs.add(0, clamp(numattr, type >= 0 && type < MAXENTTYPES ? enttype[type].numattrs : 0, MAXENTATTRS));
                            loopk(numattr)
                            {
                                if(p.overread()) break;
                                int attr = getint(p);
                                if(sents[n].attrs.inrange(k)) sents[n].attrs[k] = attr;
                            }
                            if(enttype[type].syncpos) loopj(3)
                            {
                                if(p.overread()) break;
                                sents[n].o[j] = getint(p)/DMF;
                            }
                            if(enttype[type].synckin)
                            {
                                int numkin = getint(p);
                                sents[n].kin.add(0, clamp(numkin, 0, MAXENTKIN));
                                loopk(numkin)
                                {
                                    if(p.overread()) break;
                                    int kin = getint(p);
                                    if(k < MAXENTKIN && sents[n].kin.inrange(k)) sents[n].kin[k] = kin;
                                }
                            }
                        }
                        else
                        {
                            loopk(numattr) { if(p.overread()) break; getint(p); }
                            if(enttype[type].syncpos) loopj(3) { if(p.overread()) break; getint(p); }
                            if(enttype[type].synckin)
                            {
                                int numkin = getint(p);
                                loopk(numkin) { if(p.overread()) break; getint(p); }
                            }
                        }
                    }
                    if(!skip) setupgameinfo();
                    break;
                }

                case N_SCORE:
                    getint(p);
                    getint(p);
                    QUEUE_MSG;
                    break;

                case N_INFOAFFIN:
                    getint(p);
                    getint(p);
                    getint(p);
                    getint(p);
                    QUEUE_MSG;
                    break;

                case N_SETUPAFFIN:
                    if(smode==&defendmode) defendmode.parseaffinity(p);
                    break;

                case N_MOVEAFFIN:
                {
                    int cn = getint(p), id = getint(p);
                    vec o, inertia;
                    loopi(3) o[i] = getint(p)/DMF;
                    loopi(3) inertia[i] = getint(p)/DMF;
                    clientinfo *cp = (clientinfo *)getinfo(cn);
                    if(!cp || !hasclient(cp, ci)) break;
                    if(smode==&capturemode) capturemode.moveaffinity(cp, cn, id, o, inertia);
                    else if(smode==&bombermode) bombermode.moveaffinity(cp, cn, id, o, inertia);
                    break;
                }

                case N_TAKEAFFIN:
                {
                    int lcn = getint(p), flag = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci) || cp->state.state == CS_SPECTATOR) break;
                    if(smode==&capturemode) capturemode.takeaffinity(cp, flag);
                    else if(smode==&bombermode) bombermode.takeaffinity(cp, flag);
                    break;
                }

                case N_RESETAFFIN:
                {
                    int flag = getint(p);
                    if(!ci) break;
                    if(smode==&capturemode) capturemode.resetaffinity(ci, flag);
                    else if(smode==&bombermode) bombermode.resetaffinity(ci, flag);
                    break;
                }

                case N_DROPAFFIN:
                {
                    int lcn = getint(p), tcn = getint(p);
                    vec droploc, inertia;
                    loopk(3) droploc[k] = getint(p)/DMF;
                    loopk(3) inertia[k] = getint(p)/DMF;
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci) || cp->state.state == CS_SPECTATOR) break;
                    if(smode==&capturemode) capturemode.dropaffinity(cp, droploc, inertia, tcn);
                    else if(smode==&bombermode) bombermode.dropaffinity(cp, droploc, inertia, tcn);
                    break;
                }

                case N_INITAFFIN:
                {
                    if(smode==&capturemode) capturemode.parseaffinity(p);
                    else if(smode==&bombermode) bombermode.parseaffinity(p);
                    break;
                }

                case N_PING:
                    sendf(sender, 1, "i2", N_PONG, getint(p));
                    break;

                case N_CLIENTPING:
                {
                    int ping = getint(p);
                    if(ci)
                    {
                        ci->ping = ping;
                        loopv(clients) if(clients[i]->state.ownernum == ci->clientnum) clients[i]->ping = ping;
                    }
                    QUEUE_MSG;
                    break;
                }

                case N_MASTERMODE:
                {
                    int mm = getint(p);
                    if(haspriv(ci, G(masterlock), "change mastermode") && mm >= MM_OPEN && mm <= MM_PRIVATE)
                    {
                        if(haspriv(ci, PRIV_ADMINISTRATOR) || (mastermask()&(1<<mm)))
                        {
                            mastermode = mm;
                            resetallows();
                            if(mastermode >= MM_PRIVATE)
                            {
                                loopv(clients)
                                {
                                    ipinfo &allow = control.add();
                                    allow.ip = getclientip(clients[i]->clientnum);
                                    allow.mask = 0xFFFFFFFF;
                                    allow.type = ipinfo::ALLOW;
                                    allow.time = totalmillis ? totalmillis : 1;
                                    allow.reason = newstring("mastermode set private");
                                }
                            }
                            srvoutf(-3, "\fymastermode is now \fs\fc%d\fS (\fs\fc%s\fS)", mastermode, mastermodename(mastermode));
                        }
                        else srvmsgft(sender, CON_EVENT, "\frmastermode %d (%s) is disabled on this server", mm, mastermodename(mm));
                    }
                    break;
                }

                case N_CLRCONTROL:
                {
                    int value = getint(p);
                    #define CONTROLSWITCH(x,y) \
                        case x: \
                        { \
                            if(haspriv(ci, G(y##lock), "clear " #y "s")) \
                            { \
                                reset##y##s(); \
                                srvoutf(-3, "%s cleared existing \fs\fc" #y "s\fS", colourname(ci)); \
                            } \
                            break; \
                        }

                    switch(value)
                    {
                        CONTROLSWITCH(ipinfo::ALLOW, allow);
                        CONTROLSWITCH(ipinfo::BAN, ban);
                        CONTROLSWITCH(ipinfo::MUTE, mute);
                        CONTROLSWITCH(ipinfo::LIMIT, limit);
                        CONTROLSWITCH(ipinfo::EXCEPT, except);
                        default: break;
                    }
                    #undef CONTROLSWITCH
                    break;
                }

                case N_ADDCONTROL:
                {
                    int m = getint(p), value = getint(p);
                    getstring(text, p);
                    #define CONTROLSWITCH(x,y) \
                        case x: \
                        { \
                            if(haspriv(ci, G(y##lock), #y " players") && m >= 0) \
                            { \
                                clientinfo *cp = (clientinfo *)getinfo(m); \
                                if(!cp || cp->state.ownernum >= 0 || (value != ipinfo::EXCEPT && !cmppriv(ci, cp, #y))) break; \
                                uint ip = getclientip(cp->clientnum); \
                                if(!ip) break; \
                                if(checkipinfo(control, ipinfo::EXCEPT, ip)) \
                                { \
                                    if(!haspriv(ci, PRIV_ADMINISTRATOR, #y " protected players")) break; \
                                    else if(value >= ipinfo::BAN) loopvrev(control) \
                                        if(control[i].type == ipinfo::EXCEPT && (ip & control[i].mask) == control[i].ip) \
                                            control.remove(i); \
                                } \
                                string name; \
                                copystring(name, colourname(ci)); \
                                if(value >= 0) \
                                { \
                                    ipinfo &c = control.add(); \
                                    c.ip = ip; \
                                    c.mask = 0xFFFFFFFF; \
                                    c.type = value; \
                                    c.time = totalmillis ? totalmillis : 1; \
                                    c.reason = newstring(text); \
                                    if(text[0]) srvoutf(-3, "%s added \fs\fc" #y "\fS on %s (%s/%s): %s", name, colourname(cp), gethostname(cp->clientnum), gethostip(cp->clientnum), text); \
                                    else srvoutf(-3, "%s added \fs\fc" #y "\fS on %s", name, colourname(cp)); \
                                    if(value == ipinfo::BAN) updatecontrols = true; \
                                    else if(value == ipinfo::LIMIT) cp->swapteam = 0; \
                                } \
                                else \
                                { \
                                    if(text[0]) srvoutf(-3, "%s \fs\fckicked\fS %s: %s", name, colourname(cp), text); \
                                    else srvoutf(-3, "%s \fs\fckicked\fS %s", name, colourname(cp)); \
                                    cp->kicked = updatecontrols = true; \
                                } \
                            } \
                            break; \
                        }
                    switch(value)
                    {
                        CONTROLSWITCH(-1, kick);
                        CONTROLSWITCH(ipinfo::ALLOW, allow);
                        CONTROLSWITCH(ipinfo::BAN, ban);
                        CONTROLSWITCH(ipinfo::MUTE, mute);
                        CONTROLSWITCH(ipinfo::LIMIT, limit);
                        CONTROLSWITCH(ipinfo::EXCEPT, except);
                        default: break;
                    }
                    #undef CONTROLSWITCH
                    break;
                }

                case N_SPECTATOR:
                {
                    int sn = getint(p), val = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(sn);
                    if(!cp || cp->state.actortype > A_PLAYER || (val ? cp->state.state == CS_SPECTATOR : cp->state.state != CS_SPECTATOR))
                    {
                        if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: unable to modify spectator %s - %d [%d, %d] - invalid", colourname(cp), cp->state.state, cp->state.lastdeath, gamemillis);
                        break;
                    }
                    if(sn != sender ? !haspriv(ci, max(m_edit(gamemode) ? G(spawneditlock) : G(spawnlock), G(speclock)), "control other players") : !allowstate(cp, val ? ALST_SPEC : ALST_TRY, m_edit(gamemode) ? G(spawneditlock) : G(spawnlock)))
                    {
                        if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: unable to modify spectator %s - %d [%d, %d] - restricted", colourname(cp), cp->state.state, cp->state.lastdeath, gamemillis);
                        break;
                    }
                    bool spec = val != 0, quarantine = cp != ci && val == 2, wasq = cp->state.quarantine;
                    if(!spectate(cp, spec, quarantine))
                    {
                        if(G(serverdebug)) srvmsgf(ci->clientnum, "sync error: unable to modify spectator %s - %d [%d, %d] - failed", colourname(cp), cp->state.state, cp->state.lastdeath, gamemillis);
                        break;
                    }
                    if(quarantine && cp->state.quarantine)
                    {
                        defformatstring(name, "%s", colourname(ci));
                        srvoutf(-3, "%s \fs\fcquarantined\fS %s", name, colourname(cp));
                    }
                    else if(wasq && !cp->state.quarantine)
                    {
                        defformatstring(name, "%s", colourname(ci));
                        srvoutf(-3, "%s \fs\fcreleased\fS %s from \fs\fcquarantine\fS", name, colourname(cp));
                    }
                    break;
                }

                case N_SETTEAM:
                {
                    int who = getint(p), team = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(who);
                    if(!cp || !m_team(gamemode, mutators) || m_local(gamemode) || cp->state.actortype >= A_ENEMY) break;
                    if(who < 0 || who >= getnumclients() || !haspriv(ci, G(teamlock), "change the team of others")) break;
                    if(cp->state.state == CS_SPECTATOR || !allowteam(cp, team, T_FIRST, false)) break;
                    setteam(cp, team, TT_RESETX);
                    break;
                }

                case N_RECORDDEMO:
                {
                    int val = getint(p);
                    if(!haspriv(ci, G(demolock), "record demos")) break;
                    setdemorecord(val != 0, true);
                    break;
                }

                case N_STOPDEMO:
                {
                    if(!haspriv(ci, G(demolock), "stop demos")) break;
                    if(m_demo(gamemode)) enddemoplayback();
                    else checkdemorecord(!gs_playing(gamestate));
                    break;
                }

                case N_CLEARDEMOS:
                {
                    int demo = getint(p);
                    if(!haspriv(ci, G(demolock), "clear demos")) break;
                    cleardemos(demo);
                    break;
                }

                case N_LISTDEMOS:
                    listdemos(sender);
                    break;

                case N_GETDEMO:
                {
                    int n = getint(p);
                    senddemo(sender, n);
                    break;
                }

                case N_EDITENT:
                {
                    int n = getint(p), oldtype = NOTUSED, newtype = NOTUSED;
                    bool tweaked = false, inrange = n < MAXENTS;
                    loopk(3) getint(p);
                    if(p.overread()) break;
                    if(sents.inrange(n)) oldtype = sents[n].type;
                    else if(inrange) while(sents.length() <= n) sents.add();
                    if((newtype = getint(p)) != oldtype && inrange)
                    {
                        sents[n].type = newtype;
                        tweaked = true;
                    }
                    int numattrs = getint(p), realattrs =  min(max(5, numattrs), MAXENTATTRS);
                    if(inrange) while(sents[n].attrs.length() < realattrs) sents[n].attrs.add(0);
                    loopk(numattrs)
                    {
                        int attr = getint(p);
                        if(p.overread()) break;
                        if(inrange && k < MAXENTATTRS) sents[n].attrs[k] = attr;
                    }
                    if(inrange)
                    {
                        if(oldtype == PLAYERSTART || sents[n].type == PLAYERSTART) setupspawns(true);
                        hasgameinfo = true;
                        QUEUE_MSG;
                        if(tweaked && enttype[sents[n].type].usetype != EU_NONE)
                        {
                            if(enttype[sents[n].type].usetype == EU_ITEM) setspawn(n, true, true, true);
                            if(sents[n].type == TRIGGER) setuptriggers(true);
                        }
                    }
                    break;
                }

                case N_EDITVAR:
                {
                    int t = getint(p);
                    getstring(text, p);
                    if(!ci || ci->state.state != CS_EDITING)
                    {
                        switch(t)
                        {
                            case ID_VAR: getint(p); break;
                            case ID_FVAR: getfloat(p); break;
                            case ID_SVAR: case ID_ALIAS:
                            {
                                int vlen = getint(p);
                                if(vlen < 0 || vlen > p.remaining()) break;
                                getstring(text, p, vlen+1);
                                break;
                            }
                            default: break;
                        }
                        break;
                    }
                    QUEUE_INT(N_EDITVAR);
                    QUEUE_INT(t);
                    QUEUE_STR(text);
                    switch(t)
                    {
                        case ID_VAR:
                        {
                            int val = getint(p);
                            relayf(3, "\fy%s set worldvar %s to %d", colourname(ci), text, val);
                            QUEUE_INT(val);
                            break;
                        }
                        case ID_FVAR:
                        {
                            float val = getfloat(p);
                            relayf(3, "\fy%s set worldvar %s to %s", colourname(ci), text, floatstr(val));
                            QUEUE_FLT(val);
                            break;
                        }
                        case ID_SVAR:
                        case ID_ALIAS:
                        {
                            int vlen = getint(p);
                            if(vlen < 0 || vlen > p.remaining()) break;
                            char *val = newstring(vlen);
                            getstring(val, p, vlen+1);
                            relayf(3, "\fy%s set world%s %s to %s", colourname(ci), t == ID_ALIAS ? "alias" : "var", text, val);
                            QUEUE_INT(vlen);
                            QUEUE_STR(val);
                            delete[] val;
                            break;
                        }
                        default: break;
                    }
                    break;
                }

                case N_GETMAP:
                {
                    ci->ready = true;
                    if(!getmap(ci) && numclients() <= 1) sendf(ci->clientnum, 1, "ri", N_FAILMAP);  // milestone v1.6.0
                    break;
                }

                case N_NEWMAP:
                {
                    int size = getint(p);
                    if(ci->state.state != CS_EDITING) break;
                    if(size >= 0)
                    {
                        copystring(smapname, "maps/untitled");
                        sents.shrink(0);
                        hasgameinfo = true;
                        if(smode) smode->reset();
                        mutate(smuts, mut->reset());
                    }
                    QUEUE_MSG;
                    break;
                }

                case N_SETPRIV:
                {
                    int val = getint(p);
                    getstring(text, p);
                    if(val != 0)
                    {
                        if(text[0])
                        {
                            if(!adminpass[0]) srvmsgft(ci->clientnum, CON_EVENT, "\fraccess denied, no administrator password set");
                            else if(!checkpassword(ci, adminpass, text)) srvmsgft(ci->clientnum, CON_EVENT, "\fraccess denied, invalid administrator password");
                            else auth::setprivilege(ci, 1, PRIV_ADMINISTRATOR|PRIV_LOCAL);
                        }
                        else if((ci->privilege&PRIV_TYPE) < PRIV_ELEVATED)
                        {
                            bool fail = false;
                            if(!(mastermask()&MM_AUTOAPPROVE))
                            {
                                srvmsgft(ci->clientnum, CON_EVENT, "\fraccess denied, you need a \fs\fcpassword/account\fS to \fs\fcelevate privileges\fS");
                                fail = true;
                            }
                            else loopv(clients) if(ci != clients[i] && (clients[i]->privilege&PRIV_TYPE) >= PRIV_ELEVATED)
                            {
                                srvmsgft(ci->clientnum, CON_EVENT, "\fraccess denied, there is already another player with elevated privileges");
                                fail = true;
                                break;
                            }
                            if(!fail) auth::setprivilege(ci, 1, PRIV_ELEVATED|PRIV_LOCAL);
                        }
                    }
                    else auth::setprivilege(ci, 0);
                    break; // don't broadcast the password
                }

                case N_AUTHTRY:
                {
                    getstring(text, p);
                    string authname = "";
                    filterstring(authname, text, true, true, true, true, 100);
                    auth::tryauth(ci, authname);
                    break;
                }

                case N_AUTHANS:
                {
                    uint id = (uint)getint(p);
                    getstring(text, p);
                    auth::answerchallenge(ci, id, text);
                    break;
                }

                case N_COPY:
                    ci->cleanclipboard();
                    ci->lastclipboard = totalmillis ? totalmillis : 1;
                    goto genericmsg;

                case N_PASTE:
                    if(ci->state.state == CS_EDITING) sendclipboard(ci);
                    goto genericmsg;

                case N_CLIPBOARD:
                {
                    int unpacklen = getint(p), packlen = getint(p);
                    ci->cleanclipboard(false);
                    if(ci->state.state != CS_EDITING)
                    {
                        if(packlen > 0) p.subbuf(packlen);
                        break;
                    }
                    if(packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                    {
                        if(packlen > 0) p.subbuf(packlen);
                        packlen = unpacklen = 0;
                    }
                    packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                    putint(q, N_CLIPBOARD);
                    putint(q, ci->clientnum);
                    putint(q, unpacklen);
                    putint(q, packlen);
                    if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                    ci->clipboard = q.finalize();
                    ci->clipboard->referenceCount++;
                    break;
                }

                case N_EDITT:
                case N_REPLACE:
                case N_EDITVSLOT:
                {
                    int size = msgsizelookup(type);
                    if(size<=0) { disconnect_client(sender, DISC_MSGERR); return; }
                    loopi(size-1) getint(p);
                    if(p.remaining() < 2) { disconnect_client(sender, DISC_MSGERR); return; }
                    int extra = lilswap(*(const ushort *)p.pad(2));
                    if(p.remaining() < extra) { disconnect_client(sender, DISC_MSGERR); return; }
                    p.pad(extra);
                    if(ci && ci->state.state!=CS_SPECTATOR) QUEUE_MSG;
                    break;
                }

                case N_UNDO:
                case N_REDO:
                {
                    int unpacklen = getint(p), packlen = getint(p);
                    if(!ci || ci->state.state==CS_SPECTATOR || packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                    {
                        if(packlen > 0) p.subbuf(packlen);
                        break;
                    }
                    if(p.remaining() < packlen) { disconnect_client(sender, DISC_MSGERR); return; }
                    packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                    putint(q, type);
                    putint(q, ci->clientnum);
                    putint(q, unpacklen);
                    putint(q, packlen);
                    if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                    sendpacket(-1, 1, q.finalize(), ci->clientnum);
                    break;
                }

                case -1:
                    conoutf("\fy[msg error] from: %d, cur: %d, msg: %d, prev: %d", sender, curtype, type, prevtype);
                    disconnect_client(sender, DISC_MSGERR);
                    return;

                case -2:
                    disconnect_client(sender, DISC_OVERFLOW);
                    return;

                default: genericmsg:
                {
                    int size = msgsizelookup(type);
                    if(size<=0)
                    {
                        conoutf("\fy[msg error] from: %d, cur: %d, msg: %d, prev: %d", sender, curtype, type, prevtype);
                        disconnect_client(sender, DISC_MSGERR);
                        return;
                    }
                    loopi(size-1) getint(p);
                    if(ci) QUEUE_MSG;
                    break;
                }
            }
            if(verbose > 5) conoutf("\fy[server] from: %d, cur: %d, msg: %d, prev: %d", sender, curtype, type, prevtype);
        }
    }

    bool serveroption(char *arg)
    {
        if(arg[0]=='-' && arg[1]=='s') switch(arg[2])
        {
            case 'P': setsvar("adminpass", &arg[3]); return true;
            case 'k': setsvar("serverpass", &arg[3]); return true;
            default: break;
        }
        return false;
    }
};
#undef GAMESERVER
