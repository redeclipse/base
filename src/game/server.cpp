// WARNING: Before modifying this file, please read our Guidelines
// This file can be found in the distribution under: ./docs/guidelines.txt
// Or at: https://redeclipse.net/wiki/Multiplayer_Guidelines
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
        int id, weap, fromweap, fromflags, flags, radial, scale;
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

    struct capturestats
    {
        int capturing;
        int captured;

        capturestats() { reset(); }
        ~capturestats() {}

        void reset()
        {
            capturing = captured = 0;
        }
    };

    struct bombstats
    {
        int bombing;
        int bombed;

        bombstats() { reset(); }
        ~bombstats() {}

        void reset()
        {
            bombing = bombed = 0;
        }
    };

    struct ffaroundstats
    {
        int round;
        bool winner;

        ffaroundstats() { reset(); }
        ~ffaroundstats() {}

        void reset()
        {
            round = 0;
            winner = false;
        }
    };

    extern int gamemode, mutators;

    enum { WARN_CHAT = 0, WARN_TEAMKILL, WARN_MAX };

    struct servstate : baseent, clientstate
    {
        int rewards[2], shotdamage, damage, lasttimewielded, lasttimeloadout[W_MAX], aireinit,
            lastresowner[WR_MAX], lasttimealive, timealive, lasttimeactive, timeactive, lastresweapon[WR_MAX], lasthurt,
            localtotalpoints, localtotalfrags, localtotaldeaths;
        bool lastresalt[W_MAX];
        projectilestate dropped, weapshots[W_MAX][2];
        vector<int> fraglog, fragmillis, cpnodes, chatmillis;
        vector<dmghist> damagelog;
        vector<teamkill> teamkills;

        weaponstats weapstats[W_MAX];
        vector<capturestats> captures;
        vector<bombstats> bombings;
        vector<ffaroundstats> ffarounds;

        int warnings[WARN_MAX][2];

        servstate() : lasttimewielded(0), aireinit(0), lasttimealive(0), timealive(0), lasttimeactive(0), timeactive(0), lasthurt(0), localtotalpoints(0), localtotalfrags(0), localtotaldeaths(0)
        {
            loopi(WARN_MAX) loopj(2) warnings[i][j] = 0;
            loopi(W_MAX) lasttimeloadout[i] = 0;
            resetresidualowner();
        }

        bool isalive(int millis)
        {
            return state == CS_ALIVE || ((state == CS_DEAD || state == CS_WAITING) && lastdeath && millis-lastdeath <= DEATHMILLIS);
        }

        void mapchange(bool change = false)
        {
            if(state != CS_SPECTATOR) state = CS_DEAD;
            dropped.reset();
            loopi(W_MAX) loopj(2) weapshots[i][j].reset();
            clientstate::mapchange(change);
            rewards[0] = rewards[1] = shotdamage = damage = timealive = timeactive = lasthurt = 0;
            fraglog.shrink(0);
            fragmillis.shrink(0);
            cpnodes.shrink(0);
            damagelog.shrink(0);
            teamkills.shrink(0);
            loopi(W_MAX) weapstats[i].reset();
            captures.shrink(0);
            bombings.shrink(0);
            ffarounds.shrink(0);
            respawn(0);
        }

        void resetresidualowner(int n = -1)
        {
            if(n >= 0 && n < WR_MAX) lastresowner[n] = -1;
            else loopi(WR_MAX) lastresowner[i] = -1;
        }

        void respawn(int millis)
        {
            baseent::reset();
            rewards[1] = lasthurt = 0;
            resetresidualowner();
            clientstate::respawn(millis);
        }

        void updateweaptime()
        {
            extern int gamemillis;
            if(lasttimewielded && isalive(gamemillis))
            {
                int millis = totalmillis-lasttimewielded, secs = millis/1000;
                weapstats[weapselect].timewielded += secs;
                lasttimewielded = totalmillis+(secs*1000)-millis;
                loopi(W_MAX)
                {
                    if(lasttimeloadout[i] && holdweap(i, m_weapon(actortype, gamemode, mutators), lastmillis))
                    {
                        int millis = totalmillis-lasttimeloadout[i], secs = millis/1000;
                        weapstats[i].timeloadout += secs;
                        lasttimeloadout[i] = totalmillis+(secs*1000)-millis;
                    }
                    else lasttimeloadout[i] = totalmillis ? totalmillis : 1;
                }
            }
            else
            {
                lasttimewielded = totalmillis ? totalmillis : 1;
                loopi(W_MAX) lasttimeloadout[i] = totalmillis ? totalmillis : 1;
            }
        }

        void updatetimeplayed()
        {
            clientstate::updatetimeplayed();
            extern int gamemillis;
            if(lasttimealive && isalive(gamemillis))
            {
                int millis = totalmillis-lasttimealive, secs = millis/1000;
                timealive += secs;
                lasttimealive = totalmillis+(secs*1000)-millis;
            }
            else lasttimealive = totalmillis ? totalmillis : 1;
            if(lasttimeactive && (state == CS_ALIVE || state == CS_DEAD || state == CS_WAITING))
            {
                int millis = totalmillis-lasttimeactive, secs = millis/1000;
                timeactive += secs;
                lasttimeactive = totalmillis+(secs*1000)-millis;
            }
            else lasttimeactive = totalmillis ? totalmillis : 1;
            updateweaptime();
        }

        vec feetpos(float offset = 0) const { return vec(o).add(vec(0, 0, offset)); }
        vec headpos(float offset = 0) const { return vec(o).add(vec(0, 0, offset+PLAYERHEIGHT)); }
        vec center() const { return vec(o).add(vec(0, 0, PLAYERHEIGHT*0.5f)); }
    };

    struct votecount
    {
        char *map;
        int mode, muts, count;
        votecount() {}
        votecount(char *s, int n, int m) : map(s), mode(n), muts(m), count(0) {}
    };

    struct clientinfo : servstate
    {
        string name, handle, mapvote, authname, clientmap;
        int clientnum, connectmillis, sessionid, overflow, ping, team, lastteam, lastplayerinfo,
            modevote, mutsvote, lastvote, privilege, oldprivilege, gameoffset, lastevent, wslen, swapteam, clientcrc;
        bool connected, ready, local, timesync, online, wantsmap, gettingmap, connectauth, kicked;
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
            if(state==CS_SPECTATOR || events.length()>250) delete e;
            else events.add(e);
        }

        void mapchange(bool change = true)
        {
            mapvote[0] = '\0';
            modevote = mutsvote = -1;
            servstate::mapchange(change);
            events.deletecontents();
            overflow = 0;
            ready = timesync = wantsmap = gettingmap = false;
            lastevent = gameoffset = lastvote = clientcrc = 0;
            if(!change) lastteam = T_NEUTRAL;
            team = swapteam = T_NEUTRAL;
            clientmap[0] = '\0';
            if(handle[0])
            {
                requestmasterf("reqauthstats \"%s\"\n", handle);
                flushmasteroutput();
            }
        }

        void cleanclipboard(bool fullclean = true)
        {
            if(clipboard) { if(--clipboard->referenceCount <= 0) enet_packet_destroy(clipboard); clipboard = NULL; }
            if(fullclean) lastclipboard = 0;
        }

        void reset()
        {
            ping = lastplayerinfo = 0;
            name[0] = handle[0] = '\0';
            privilege = PRIV_NONE;
            oldprivilege = -1;
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

    struct savedscore
    {
        uint ip;
        string name, handle;
        int points, frags, deaths, localtotalpoints, localtotalfrags, localtotaldeaths, spree, rewards, timeplayed, timealive, timeactive, shotdamage, damage, cptime, actortype;
        int warnings[WARN_MAX][2];
        bool quarantine;
        weaponstats weapstats[W_MAX];
        vector<teamkill> teamkills;
        vector<capturestats> captures;
        vector<bombstats> bombings;
        vector<ffaroundstats> ffarounds;

        void save(clientinfo *ci)
        {
            points = ci->points;
            frags = ci->frags;
            deaths = ci->deaths;
            localtotalpoints = ci->localtotalpoints;
            localtotalfrags = ci->localtotalfrags;
            localtotaldeaths = ci->localtotaldeaths;
            spree = ci->spree;
            rewards = ci->rewards[0];
            timeplayed = ci->timeplayed;
            timealive = ci->timealive;
            timeactive = ci->timeactive;
            shotdamage = ci->shotdamage;
            damage = ci->damage;
            cptime = ci->cptime;
            actortype = ci->actortype;
            loopi(W_MAX) weapstats[i] = ci->weapstats[i];
            loopi(WARN_MAX) loopj(2) warnings[i][j] = ci->warnings[i][j];
            quarantine = ci->quarantine;
            teamkills.shrink(0);
            loopv(ci->teamkills) teamkills.add(ci->teamkills[i]);
            captures.shrink(0);
            loopv(ci->captures) captures.add(ci->captures[i]);
            bombings.shrink(0);
            loopv(ci->bombings) bombings.add(ci->bombings[i]);
            ffarounds.shrink(0);
            loopv(ci->ffarounds) ffarounds.add(ci->ffarounds[i]);
        }

        void restore(clientinfo *ci)
        {
            ci->points = points;
            ci->frags = frags;
            ci->deaths = deaths;
            ci->localtotalpoints = localtotalpoints;
            ci->localtotalfrags = localtotalfrags;
            ci->localtotaldeaths = localtotaldeaths;
            ci->totalpoints = localtotalpoints;
            ci->totalfrags = localtotalfrags;
            ci->totaldeaths = localtotaldeaths;
            ci->spree = spree;
            ci->rewards[0] = rewards;
            ci->timeplayed = timeplayed;
            ci->timealive = timealive;
            ci->timeactive = timeactive;
            ci->shotdamage = shotdamage;
            ci->damage = damage;
            ci->cptime = cptime;
            loopi(W_MAX) ci->weapstats[i] = weapstats[i];
            loopi(WARN_MAX) loopj(2) ci->warnings[i][j] = warnings[i][j];
            ci->quarantine = quarantine;
            ci->teamkills.shrink(0);
            loopv(teamkills) ci->teamkills.add(teamkills[i]);
            ci->captures.shrink(0);
            loopv(captures) ci->captures.add(captures[i]);
            ci->bombings.shrink(0);
            loopv(bombings) ci->bombings.add(bombings[i]);
            ci->ffarounds.shrink(0);
            loopv(ffarounds) ci->ffarounds.add(ffarounds[i]);
        }

        void mapchange()
        {
            points = frags = spree = rewards = deaths = timeplayed = timealive = timeactive = shotdamage = damage = cptime = 0;
            actortype = A_MAX;
            teamkills.shrink(0);
            captures.shrink(0);
            bombings.shrink(0);
            ffarounds.shrink(0);
            loopi(W_MAX) weapstats[i].reset();
        }
    };

    namespace aiman {
        extern void setskill(clientinfo *ci);
        extern bool addai(int type, int ent = -1, int skill = -1);
        extern void deleteai(clientinfo *ci);
        extern bool delai(int type, bool skip = true);
        extern void removeai(clientinfo *ci, bool complete = false);
        extern bool reassignai(clientinfo *exclude = NULL);
        extern void clearai(int type = 0);
        extern void checkai();
        extern void poke();
    }

    string smapname = "";
    int smapcrc = 0, smapvariant = MPV_DEFAULT, mapsending = -1, mapgameinfo = -1, gamestate = G_S_WAITING, gamemode = G_EDITMODE, mutators = 0, gamemillis = 0, gamelimit = 0,
        mastermode = MM_OPEN, timeremaining = -1, oldtimelimit = -1, gamewaittime = 0, lastteambalance = 0, nextteambalance = 0, lastrotatecycle = 0;
    bool hasgameinfo = false, updatecontrols = false, shouldcheckvotes = false, firstblood = false, sentstats = false;
    enet_uint32 lastsend = 0;
    stream *mapdata[SENDMAP_MAX] = { NULL };
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

    bool canplay()
    {
        if(!demoplayback && !m_demo(gamemode))
            if((m_play(gamemode) && !hasgameinfo) || !gs_playing(gamestate)) return false;
        return true;
    }

    struct servmode
    {
        servmode() {}
        virtual ~servmode() {}
        virtual bool spawnitems() const { return true; }

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
    vector<savedscore> savedstatsscores;
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

    bool checkmapvariant(int variant)
    {
        if(variant > 0 && smapvariant > 0 && smapvariant != variant) return false;
        return true;
    }

    bool chkloadweap(clientinfo *ci, bool request = true)
    {
        if(ci->actortype == A_PLAYER && ci->loadweap.empty())
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
            if(clear) loopvk(clients) clients[k]->dropped.removeall(ent);
            sents[ent].spawned = spawned;
            sents[ent].millis = sents[ent].last = gamemillis;
            if(sents[ent].type == WEAPON && !(sents[ent].attrs[1]&W_F_FORCED))
            {
                int attr = w_attr(gamemode, mutators, sents[ent].type, sents[ent].attrs[0], m_weapon(A_PLAYER, gamemode, mutators));
                if(isweap(attr)) sents[ent].millis += w_spawn(attr);
            }
            else sents[ent].millis += G(itemspawntime);
            if(msg) sendf(-1, 1, "ri3", N_ITEMSPAWN, ent, sents[ent].spawned ? 1 : 0);
        }
    }

    void takeammo(clientinfo *ci, int weap, int amt = 1) { ci->ammo[weap] = max(ci->ammo[weap]-amt, 0); }

    struct droplist { int weap, ent, ammo; };
    enum
    {
        DROP_NONE = 0, DROP_WEAPONS = 1<<0, DROP_WCLR = 1<<1, DROP_KAMIKAZE = 1<<2, DROP_EXPLODE = 1<<3,
        DROP_DEATH = DROP_WEAPONS|DROP_KAMIKAZE, DROP_EXPIRE = DROP_WEAPONS|DROP_EXPLODE, DROP_RESET = DROP_WEAPONS|DROP_WCLR
    };

    void dropweapon(clientinfo *ci, int flags, int weap, vector<droplist> &drop)
    {
        if(isweap(weap) && weap != m_weapon(ci->actortype, gamemode, mutators) && ci->hasweap(weap, m_weapon(ci->actortype, gamemode, mutators)) && sents.inrange(ci->entid[weap]))
        {
            setspawn(ci->entid[weap], false);
            droplist &d = drop.add();
            d.weap = weap;
            d.ent = ci->entid[weap];
            d.ammo = ci->ammo[weap];
            ci->dropped.add(d.ent, d.ammo);
            if(flags&DROP_WCLR) ci->entid[weap] = ci->ammo[weap] = -1;
        }
    }

    bool dropitems(clientinfo *ci, int flags = DROP_RESET)
    {
        bool kamikaze = false;
        vector<droplist> drop;
        if(flags&DROP_EXPLODE || (flags&DROP_KAMIKAZE && G(kamikaze) && (G(kamikaze) > 2 || (ci->hasweap(W_GRENADE, m_weapon(ci->actortype, gamemode, mutators)) && (G(kamikaze) > 1 || ci->weapselect == W_GRENADE)))))
        {
            ci->weapshots[W_GRENADE][0].add(1);
            droplist &d = drop.add();
            d.weap = W_GRENADE;
            d.ent = d.ammo = -1;
            if(!(flags&DROP_EXPLODE)) takeammo(ci, W_GRENADE, W2(W_GRENADE, ammosub, false));
            kamikaze = true;
        }
        if(flags&DROP_WEAPONS) loopi(W_ALL) dropweapon(ci, flags, i, drop);
        if(!drop.empty())
            sendf(-1, 1, "ri3iv", N_DROP, ci->clientnum, -1, drop.length(), drop.length()*sizeof(droplist)/sizeof(int), drop.getbuf());
        return kamikaze;
    }

    struct vampireservmode : servmode
    {
        vampireservmode() {}
        void dodamage(clientinfo *m, clientinfo *v, int &damage, int &hurt, int &weap, int &flags, int &material, const ivec &hitpush, const ivec &hitvel, float dist)
        {
            if(v != m && (!m_team(gamemode, mutators) || v->team != m->team) && v->state == CS_ALIVE && hurt > 0)
            {
                int real = int(ceilf(hurt*G(vampirescale))), heal = v->health+real;
                if(AA(v->actortype, abilities)&(1<<A_A_REGEN)) heal = min(heal, m_maxhealth(gamemode, mutators, v->actortype));
                int eff = heal-v->health;
                if(eff > 0)
                {
                    v->health = heal;
                    v->lastregen = gamemillis;
                    v->lastregenamt = eff;
                    sendf(-1, 1, "ri4", N_REGEN, v->clientnum, v->health, v->lastregenamt);
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
            if(spawnqueue(true) && ci->online && ci->actortype < A_ENEMY && ci->state != CS_SPECTATOR && ci->state != CS_EDITING)
            {
                int n = spawnq.find(ci);
                playing.removeobj(ci);
                if(top)
                {
                    if(n >= 0) spawnq.remove(n);
                    spawnq.insert(0, ci);
                }
                else if(n < 0) spawnq.add(ci);
                if(wait && ci->state != CS_WAITING) waiting(ci, DROP_RESET);
                if(msg && allowbroadcast(ci->clientnum) && !top)
                {
                    int x = max(int(G(maxalive)*G(maxplayers)), max(int(numclients()*G(maxalivethreshold)), G(maxaliveminimum)));
                    if(m_team(gamemode, mutators))
                    {
                        if(x%2) x++;
                        x = x/2;
                        if(m_coop(gamemode, mutators) && ci->actortype == A_BOT)
                            x = int(x*G(coopbalance));
                    }
                    int slots = x;
                    loopv(playing) if(playing[i] && ci->team == playing[i]->team) slots--;
                    if(!slots)
                    {
                        int qn = 0;
                        loopv(spawnq) if(spawnq[i] && spawnq[i]->team == ci->team && spawnq[i]->actortype == A_PLAYER)
                        {
                            qn++;
                            if(spawnq[i] == ci)
                            {
                                spawnq[i]->queuepos = qn;
                                sendf(-1, 1, "ri3", N_QUEUEPOS, spawnq[i]->clientnum, spawnq[i]->queuepos);
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
            if(ci->actortype >= A_ENEMY || !m_play(gamemode)) return true;
            else if(tryspawn)
            {
                if(m_loadout(gamemode, mutators) && !chkloadweap(ci)) return false;
                if(spawnqueue(true) && spawnq.find(ci) < 0 && playing.find(ci) < 0) queue(ci);
                return true;
            }
            if(m_balance(gamemode, mutators, teamspawns) && G(balancenospawn) && nextbalance && m_balreset(gamemode, mutators) && canbalancenow()) return false;
            int delay = m_delay(ci->actortype, gamemode, mutators, ci->team);
            if(delay && ci->respawnwait(gamemillis, delay)) return false;
            if(spawnqueue() && playing.find(ci) < 0)
            {
                if(!canplay()) return false;
                if(G(maxalivequeue) && spawnq.find(ci) < 0) queue(ci);
                int x = max(int(G(maxalive)*G(maxplayers)), max(int(numclients()*G(maxalivethreshold)), G(maxaliveminimum)));
                if(m_team(gamemode, mutators))
                {
                    if(x%2) x++;
                    x = x/2;
                    if(m_coop(gamemode, mutators) && ci->actortype == A_BOT)
                        x = int(x*G(coopbalance));
                }
                int alive = 0;
                loopv(playing)
                {
                    if(playing[i]->state != CS_DEAD && playing[i]->state != CS_ALIVE)
                    {
                        if(playing[i]->state != CS_WAITING || !G(maxalivequeue))
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
                    if(ci->actortype == A_PLAYER) loopv(playing)
                    { // kill off bots for the human
                        if(playing[i]->actortype != A_BOT || ci->team != playing[i]->team)
                            continue;
                        queue(playing[i--]);
                        if(--alive < x) break;
                    }
                    if(alive >= x) return false;
                }
                if(G(maxalivequeue))
                {
                    if(ci->actortype == A_BOT) loopv(spawnq) if(spawnq[i]->team == ci->team)
                    {
                        if(spawnq[i] != ci && spawnq[i]->actortype == A_PLAYER) return false;
                        break;
                    }
                    // at this point is where it decides this player is spawning, so tell everyone else their position
                    if(x-alive == 1)
                    {
                        int qn = 0;
                        loopv(spawnq) if(spawnq[i] != ci && spawnq[i]->team == ci->team && spawnq[i]->actortype == A_PLAYER)
                        {
                            qn++;
                            if(allowbroadcast(spawnq[i]->clientnum))
                            {
                                spawnq[i]->queuepos = qn;
                                sendf(-1, 1, "ri3", N_QUEUEPOS, spawnq[i]->clientnum, spawnq[i]->queuepos);
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
                if(id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU) \
                    *id->storage.i = clamp(uint(b), uint(id->minval), uint(id->maxval)); \
                else *id->storage.i = clamp(b, id->minval, id->maxval); \
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
    void resetgamevars(bool all)
    {
        numgamevars = numgamemods = 0;
        enumerate(idents, ident, id, {
            if(id.flags&IDF_SERVER && !(id.flags&IDF_READONLY) && (all || !(id.flags&IDF_WORLD))) // reset vars
            {
                const char *val = NULL;
                if(id.flags&IDF_GAMEMOD) numgamevars++;
                switch(id.type)
                {
                    case ID_VAR:
                    {
                        if(*id.storage.i != id.def.i)
                        {
                            setvar(id.name, id.def.i, true);
                            val = intstr(&id);
                        }
                        if(id.flags&IDF_GAMEMOD && *id.storage.i != id.bin.i) numgamemods++;
                        break;
                    }
                    case ID_FVAR:
                    {
                        if(*id.storage.f != id.def.f)
                        {
                            setfvar(id.name, id.def.f, true);
                            val = floatstr(*id.storage.f);
                        }
                        if(id.flags&IDF_GAMEMOD && *id.storage.f != id.bin.f) numgamemods++;
                        break;
                    }
                    case ID_SVAR:
                    {
                        if(strcmp(*id.storage.s, id.bin.s))
                        {
                            setsvar(id.name, id.def.s && *id.def.s ? id.def.s : "", true);
                            val = *id.storage.s;
                        }
                        if(id.flags&IDF_GAMEMOD && strcmp(*id.storage.s, id.bin.s)) numgamemods++;
                        break;
                    }
                    default: break;
                }
                if(val) sendf(-1, 1, "ri2sis", N_COMMAND, -1, &id.name[3], strlen(val), val);
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
                defaultmap[0] = '\0';
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
        if(msg) srvoutf(-3, "\fyDemo recording is \fs\fc%s\fS for next match", demonextmatch ? "enabled" : "disabled");
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
        if(G(resetvarsonend) || init) resetgamevars(true);
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
        srvoutf(-3, "\fyServer shutdown in progress..");
        aiman::clearai();
        loopv(clients) if(getinfo(i)) disconnect_client(i, DISC_SHUTDOWN);
    }

    void *newinfo() { return new clientinfo; }
    void deleteinfo(void *ci) { delete (clientinfo *)ci; }

    int numchannels() { return 3; }
    int spectatorslots() { return clamp(G(serverspectators) > 0 ? G(serverspectators) : G(serverclients), 1, MAXCLIENTS); }
    int maxslots() { return clamp(G(serverclients)+spectatorslots(), 1, MAXCLIENTS); }
    int reserveclients() { return maxslots()+4; }
    int dupclients() { return G(serverdupclients); }

    bool hasclient(clientinfo *ci, clientinfo *cp = NULL)
    {
        if(!ci || (ci != cp && ci->clientnum != cp->clientnum && ci->ownernum != cp->clientnum)) return false;
        return true;
    }

    int peerowner(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(ci && ci->actortype > A_PLAYER) return ci->ownernum;
        return n;
    }

    bool allowbroadcast(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        return ci && ci->connected && ci->actortype == A_PLAYER;
    }

    int numclients(int exclude, bool nospec, int actortype)
    {
        int n = 0;
        loopv(clients)
        {
            if(clients[i]->clientnum >= 0 && clients[i]->name[0] && clients[i]->clientnum != exclude &&
                (!nospec || clients[i]->state != CS_SPECTATOR) &&
                    (clients[i]->actortype == A_PLAYER || (actortype > A_PLAYER && clients[i]->actortype <= actortype && clients[i]->ownernum >= 0)))
                        n++;
        }
        return n;
    }

    int numspectators(int exclude = -1)
    {
        int n = 0;
        loopv(clients)
            if(clients[i]->clientnum >= 0 && clients[i]->name[0] && clients[i]->clientnum != exclude && clients[i]->actortype == A_PLAYER && clients[i]->state == CS_SPECTATOR)
                n++;
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
            int col = ci->actortype < A_ENEMY ? ci->colour : TEAM(T_ENEMY, colour);
            if(!col && isweap(ci->weapselect)) col = W(ci->weapselect, colour);
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
        static string colored; colored[0] = '\0'; string colortmp;
        if(colour) concatstring(colored, "\fs");
        if(icon)
        {
            if(colour&1)
            {
                formatstring(colortmp, "\f[%d]", findcolour(ci));
                concatstring(colored, colortmp);
            }
            formatstring(colortmp, "\f($priv%stex)", privnamex(ci->privilege, ci->actortype, true));
            concatstring(colored, colortmp);
        }
        if(colour&2)
        {
            formatstring(colortmp, "\f[%d]", TEAM(ci->team, colour));
            concatstring(colored, colortmp);
        }
        concatstring(colored, name);
        if(!name[0] || (ci->actortype < A_ENEMY && dupname && duplicatename(ci, name)))
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
        static string teamed; teamed[0] = '\0'; string teamtmp;
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
            srvmsgft(ci->clientnum, CON_MESG, "\frAccess denied, you need to be \fs\fc%s\fS to \fs\fc%s\fS", privnamex(flag), msg);
        return false;
    }

    bool cmppriv(clientinfo *ci, clientinfo *cp, const char *msg = NULL)
    {
        stringz(str);
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

    const char *gamename(int mode, int muts, int compact, int limit, char separator)
    {
        if(!m_game(mode)) mode = G_DEATHMATCH;
        if(gametype[mode].implied) muts |= gametype[mode].implied;
        static string gname; gname[0] = '\0';
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
            defformatstring(mname, "%s%s%s", *gname ? gname : "", *gname ? "-" : "", k < 3 ? gametype[mode].name : gametype[mode].sname);
            if(k < 3 && limit > 0 && int(strlen(mname)) >= limit)
            {
                gname[0] = '\0';
                continue; // let's try again
            }
            copystring(gname, mname);
            if(separator != ' ') for(int n = strlen(mname); mname[n]; n++) if(mname[n] == ' ') mname[n] = separator;
            break;
        }
        return gname;
    }
    ICOMMAND(0, gamename, "iiiis", (int *g, int *m, int *c, int *t, char *s), result(gamename(*g, *m, *c, *t, *s)));

    const char *modedesc(int mode, int muts, int type)
    {
        if(!m_game(mode)) mode = G_DEATHMATCH;
        if(gametype[mode].implied) muts |= gametype[mode].implied;
        static string mdname; mdname[0] = '\0';
        if(type == 1 || type == 3 || type == 4) concatstring(mdname, gametype[mode].name);
        if(type == 3 || type == 4) concatstring(mdname, ": ");
        if(type == 2 || type == 3 || type == 4 || type == 5)
        {
            if((type == 4 || type == 5) && m_ctf_protect(mode, muts)) concatstring(mdname, gametype[mode].gsd[2]);
            else if((type == 4 || type == 5) && m_dac_king(mode, muts)) concatstring(mdname, gametype[mode].gsd[1]);
            else if((type == 4 || type == 5) && m_bb_hold(mode, muts)) concatstring(mdname, gametype[mode].gsd[0]);
            else if((type == 4 || type == 5) && m_bb_attack(mode, muts)) concatstring(mdname, gametype[mode].gsd[2]);
            else if((type == 4 || type == 5) && m_ra_timed(mode, muts)) concatstring(mdname, gametype[mode].gsd[0]);
            else if((type == 4 || type == 5) && m_ra_gauntlet(mode, muts)) concatstring(mdname, gametype[mode].gsd[2]);
            else concatstring(mdname, gametype[mode].desc);
        }
        return mdname;
    }
    ICOMMAND(0, modedesc, "iii", (int *g, int *m, int *c), result(modedesc(*g, *m, *c)));

    const char *mutsdesc(int mode, int muts, int type)
    {
        if(!m_game(mode)) mode = G_DEATHMATCH;
        static string mtname; mtname[0] = '\0';
        int mutid = -1;
        loopi(G_M_NUM) if(muts == (1<<mutstype[i].type)) mutid = i;
        if(mutid < 0) return "";
        if(type == 4 || type == 5)
        {
            if(m_ctf_protect(mode, muts)) return "";
            else if(m_dac_king(mode, muts)) return "";
            else if(m_bb_hold(mode, muts) || m_bb_attack(mode, muts)) return "";
            else if(m_ra_timed(mode, muts) || m_ra_gauntlet(mode, muts)) return "";
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
            default: return gamewaittime ? max(gamewaittime-totalmillis, 0)/1000 : 0;
        }
        return 0;
    }

    void sendtick()
    {
        sendf(-1, 1, "ri3", N_TICK, gamestate, timeleft());
    }

    bool checkvotes(bool force = false);
    void sendstats(bool fromintermission = false);
    void startintermission(bool req = false)
    {
        if(gs_playing(gamestate))
        {
            sendstats(true);
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
        if(!m_mmvar(gamemode, mutators, overtimeallow) || m_balance(gamemode, mutators, teamspawns)) return false;
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
            loopv(clients) if(clients[i]->actortype < A_ENEMY && clients[i]->state != CS_SPECTATOR)
            {
                if(best < 0 || (m_laptime(gamemode, mutators) ? (clients[best]->cptime <= 0 || (clients[i]->cptime > 0 && clients[i]->cptime < clients[best]->cptime)) : clients[i]->points > clients[best]->points))
                {
                    best = i;
                    result = false;
                }
                else if(m_laptime(gamemode, mutators) ? clients[i]->cptime == clients[best]->cptime : clients[i]->points == clients[best]->points) result = true;
            }
        }
        return result;
    }

    bool balancecmp(clientinfo *a, clientinfo *b)
    {
        return (a->balancescore() > b->balancescore());
    }

    void doteambalance(bool init)
    {
        vector<clientinfo *> tc[T_TOTAL];
        int numplaying = 0;
        loopv(clients)
        {
            clientinfo *cp = clients[i];
            if(!cp->team || cp->state == CS_SPECTATOR || cp->actortype > A_PLAYER) continue;
            cp->updatetimeplayed();
            tc[cp->team-T_FIRST].add(cp);
            numplaying++;
        }
        if((G(teambalancestyle) || m_swapteam(gamemode, mutators)) && numplaying >= G(teambalanceplaying))
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
                else if(init)
                {
                    vector <clientinfo *> pool;
                    loopvj(clients)
                    {
                        clientinfo *cp = clients[j];
                        if(!cp->team || cp->state == CS_SPECTATOR || cp->actortype > A_PLAYER) continue;
                        pool.add(cp);
                        setteam(cp, T_NEUTRAL, 0, false);
                    }
                    pool.sort(balancecmp);
                    loopvj(pool)
                    {
                        clientinfo *cp = pool[j];
                        cp->swapteam = T_NEUTRAL;
                        int t = chooseteam(cp, -1, true);
                        if(t != cp->team)
                        {
                            setteam(cp, t, (m_balreset(gamemode, mutators) ? TT_RESET : 0)|TT_INFOSM, false);
                            cp->lastdeath = 0;
                        }
                    }
                }
                else if(canbalancenow())
                {
                    int moved = 0;
                    loopi(nt) for(int team = i+T_FIRST, iters = tc[i].length(); iters > 0 && tc[i].length() > mid; iters--)
                    {
                        int id = -1;
                        loopvj(tc[i])
                        {
                            clientinfo *cp = tc[i][j];
                            if(m_swapteam(gamemode, mutators) && cp->swapteam && cp->swapteam == team) { id = j; break; }
                            if(G(teambalancestyle) == 0)
                            {
                                if(id < 0) id = j;
                            }
                            else if(G(teambalancehighest))
                            {
                                if(id < 0 || tc[i][id]->balancescore() < cp->balancescore()) id = j;
                            }
                            else
                            {
                                if(id < 0 || tc[i][id]->balancescore() > cp->balancescore()) id = j;
                            }
                        }
                        if(id >= 0)
                        {
                            clientinfo *cp = tc[i][id];
                            cp->swapteam = T_NEUTRAL; // make them rechoose if necessary
                            int t = chooseteam(cp, -1, true);
                            if(t != cp->team)
                            {
                                setteam(cp, t, (m_balreset(gamemode, mutators) ? TT_RESET : 0)|TT_INFOSM, false);
                                cp->lastdeath = 0;
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
        int limit = wasinovertime ? m_mmvar(gamemode, mutators, overtimelimit) : m_mmvar(gamemode, mutators, timelimit), numt = numteams(gamemode, mutators);
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
                        limit = oldtimelimit = m_mmvar(gamemode, mutators, overtimelimit);
                        if(limit)
                        {
                            timeremaining = limit*60;
                            gamelimit += timeremaining*1000;
                            ancmsgft(-1, S_V_OVERTIME, CON_EVENT, "\fyOvertime, match extended by \fs\fc%d\fS %s", limit, limit > 1 ? "minutes" : "minute");
                        }
                        else
                        {
                            timeremaining = -1;
                            gamelimit = 0;
                            ancmsgft(-1, S_V_OVERTIME, CON_EVENT, "\fyOvertime, match extended until someone wins");
                        }
                        gamestate = G_S_OVERTIME;
                        wantsoneminute = false;
                    }
                    else
                    {
                        ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyTime limit has been reached");
                        startintermission();
                        return; // bail
                    }
                }
                if(gs_playing(gamestate) && timeremaining != 0)
                {
                    if(wantsoneminute && timeremaining == 60) ancmsgft(-1, S_V_ONEMINUTE, CON_EVENT, "\fzYgone minute remains");
                    sendtick();
                }
            }
        }
        if(wasinovertime && !wantsovertime())
        {
            ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyOvertime has ended, a winner has been chosen");
            startintermission();
            return; // bail
        }
        if(!m_balance(gamemode, mutators, teamspawns))
        {
            int plimit = 0;
            if(m_dm(gamemode)) plimit = m_dm_oldschool(gamemode, mutators) ? G(fraglimit) : G(pointlimit);
            else if(m_capture(gamemode)) plimit = G(capturelimit);
            else if(m_defend(gamemode)) plimit = G(defendlimit) ? G(defendlimit) : INT_MAX-1;
            else if(m_bomber(gamemode)) plimit = m_bb_hold(gamemode, mutators) ? G(bomberholdlimit) : G(bomberlimit);
            else if(m_race(gamemode) && !m_ra_timed(gamemode, mutators) && !m_ra_gauntlet(gamemode, mutators)) plimit = G(racelimit);
            if(plimit)
            {
                if(m_team(gamemode, mutators))
                {
                    int best = -1;
                    loopi(numt) if(best < 0 || teamscore(i+T_FIRST).total > teamscore(best).total)
                        best = i+T_FIRST;
                    if(best >= 0 && teamscore(best).total >= plimit)
                    {
                        ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyScore limit has been reached");
                        startintermission();
                        return; // bail
                    }
                }
                else
                {
                    int best = -1;
                    loopv(clients) if(clients[i]->actortype < A_ENEMY && (best < 0 || clients[i]->points > clients[best]->points))
                        best = i;
                    if(best >= 0 && clients[best]->points >= plimit)
                    {
                        ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyScore limit has been reached");
                        startintermission();
                        return; // bail
                    }
                }
            }
        }
        else if(gamelimit > 0 && curbalance < (numt-1))
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
                                cp->cpmillis = 0;
                                cp->cpnodes.shrink(0);
                                sendf(-1, 1, "ri3", N_CHECKPOINT, cp->clientnum, -1);
                            }
                            setteam(cp, to, (m_balreset(gamemode, mutators) ? TT_RESET : 0)|TT_INFO, false);
                            cp->lastdeath = 0;
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
        if(m_balteam(gamemode, mutators, 4) && gamestate != G_S_OVERTIME && gamemillis >= G(teambalancewait) && (!lastteambalance || gamemillis >= lastteambalance) && (!nextteambalance || gamemillis >= nextteambalance))
            doteambalance(false);
    }

    bool hasitem(int i)
    {
        if((m_race(gamemode) && !m_ra_gauntlet(gamemode, mutators)) || m_basic(gamemode, mutators) || !sents.inrange(i) || sents[i].type != WEAPON) return false;
        int sweap = m_weapon(A_PLAYER, gamemode, mutators), attr = w_attr(gamemode, mutators, sents[i].type, sents[i].attrs[0], sweap);
        if(!isweap(attr) || !w_item(attr, sweap) || !m_check(W(attr, modes), W(attr, muts), gamemode, mutators) || W(attr, disabled)) return false;
        if(!checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) || (sents[i].attrs[4] && sents[i].attrs[4] != triggerid) || !m_check(sents[i].attrs[2], sents[i].attrs[3], gamemode, mutators)) return false;
        return true;
    }

    bool finditem(int i, bool spawned = false, bool carry = false)
    {
        if(sents[i].spawned) return true;
        if(sents[i].type == WEAPON && !(sents[i].attrs[1]&W_F_FORCED)) loopvk(clients)
        {
            clientinfo *ci = clients[k];
            if(ci->dropped.find(i) && (!spawned || gamemillis < sents[i].millis)) return true;
            else if(carry) loopj(W_ALL)
                if(ci->online && ci->state == CS_ALIVE && ci->entid[j] == i && ci->hasweap(j, m_weapon(A_PLAYER, gamemode, mutators)))
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
        int sweap = m_weapon(A_PLAYER, gamemode, mutators);
        bool dospawn = true;
        if(smode && !smode->spawnitems()) dospawn = false;
        mutate(smuts, if(!mut->spawnitems()) dospawn = false);
        loopv(sents)
        {
            if(sents[i].type == ACTOR)
            {
                if(!checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr])) continue;
                if(sents[i].attrs[0] < 0 || sents[i].attrs[0] >= A_TOTAL) continue;
                if(sents[i].attrs[5] && sents[i].attrs[5] != triggerid) continue;
                if(!m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators)) continue;
                if(sents[i].attrs[0] == A_TURRET && m_insta(gamemode, mutators)) continue;
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
                sents[i].millis = gamemillis;
                if(dospawn)
                {
                    sents[i].millis += G(itemspawndelay);
                    switch(G(itemspawnstyle) == 3 ? rnd(2)+1 : G(itemspawnstyle))
                    {
                        case 1: items.add(i); break;
                        case 2:
                        {
                            int attr = w_attr(gamemode, mutators, sents[i].type, sents[i].attrs[0], sweap), delay = sents[i].type == WEAPON && isweap(attr) ? w_spawn(attr) : G(itemspawntime);
                            if(delay > 1) sents[i].millis += (delay+rnd(delay))/2;
                            break;
                        }
                        default: break;
                    }
                }
                else sents[i].millis += 60000;
            }
        }
        if(dospawn && !items.empty())
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
            if(enttype[sents[i].type].mvattr >= 0 && !checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr])) continue;
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
                loopv(sents) if(sents[i].type == PLAYERSTART && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && sents[i].attrs[0] == T_NEUTRAL && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                {
                    spawns[m_ra_gauntlet(gamemode, mutators) ? T_ALPHA : T_NEUTRAL].add(i);
                    totalspawns++;
                }
                if(!totalspawns) loopv(sents) if(sents[i].type == CHECKPOINT && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && sents[i].attrs[6] == CP_START && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                {
                    spawns[m_ra_gauntlet(gamemode, mutators) ? T_ALPHA : T_NEUTRAL].add(i);
                    totalspawns++;
                }
                if(m_ra_gauntlet(gamemode, mutators))
                {
                    int enemyspawns = 0;
                    loopv(sents) if(sents[i].type == PLAYERSTART && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && sents[i].attrs[0] >= T_OMEGA && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
                    {
                        loopk(numt-1) spawns[T_OMEGA+k].add(i);
                        totalspawns++;
                        enemyspawns++;
                    }
                    if(!enemyspawns) loopv(sents) if(sents[i].type == CHECKPOINT && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && sents[i].attrs[6] == CP_RESPAWN && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
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
                    loopv(sents) if(sents[i].type == PLAYERSTART && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
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
                loopv(sents) if(sents[i].type == PLAYERSTART && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && sents[i].attrs[0] == T_NEUTRAL && (sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))
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
                    loopv(sents) if(sents[i].type == PLAYERSTART && checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr]) && (k || ((sents[i].attrs[5] == triggerid || !sents[i].attrs[5]) && m_check(sents[i].attrs[3], sents[i].attrs[4], gamemode, mutators))))
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
        if(ci->actortype >= A_ENEMY) return ci->spawnpoint;
        else
        {
            if(m_race(gamemode) && !ci->cpnodes.empty() && (!m_ra_gauntlet(gamemode, mutators) || ci->team == T_ALPHA))
            {
                int checkpoint = ci->cpnodes.last();
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
                else if(m_play(gamemode) && m_team(gamemode, mutators) && (!m_race(gamemode) || m_ra_gauntlet(gamemode, mutators)) && !spawns[ci->team].ents.empty()) team = ci->team;
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

    void setupgameinfo(bool update = false)
    {
        setuptriggers(true);
        setupitems(true);
        setupspawns(true);
        if(!update)
        {
            mapgameinfo = -1;
            hasgameinfo = true;
        }
        aiman::poke();
    }

    void changemapvariant(int variant)
    {
        if(variant != smapvariant)
        {
            smapvariant = variant;
            setupgameinfo(true);
        }
    }

    void sendspawn(clientinfo *ci)
    {
        int weap = -1, health = m_health(gamemode, mutators, ci->actortype);
        if(ci->actortype >= A_ENEMY)
        {
            bool hasent = sents.inrange(ci->spawnpoint) && sents[ci->spawnpoint].type == ACTOR;
            if(m_sweaps(gamemode, mutators)) weap = m_weapon(ci->actortype, gamemode, mutators);
            else weap = hasent && sents[ci->spawnpoint].attrs[6] > 0 ? sents[ci->spawnpoint].attrs[6]-1 : m_weapon(ci->actortype, gamemode, mutators);
            if(!m_insta(gamemode, mutators) && hasent && sents[ci->spawnpoint].attrs[7] > 0) health = max(sents[ci->spawnpoint].attrs[7], 1);
        }
        int spawn = pickspawn(ci);
        ci->spawnstate(gamemode, mutators, weap, health);
        ci->updatetimeplayed();
        sendf(ci->clientnum, 1, "ri9i5v", N_SPAWNSTATE, ci->clientnum, spawn, ci->state, ci->points, ci->frags, ci->deaths, ci->totalpoints, ci->totalfrags, ci->totaldeaths, ci->timeplayed, ci->health, ci->cptime, ci->weapselect, W_MAX, &ci->ammo[0]);
        ci->lastspawn = gamemillis;
    }

    template<class T>
    void sendstate(clientinfo *ci, T &p)
    {
        ci->updatetimeplayed();
        putint(p, ci->state);
        putint(p, ci->points);
        putint(p, ci->frags);
        putint(p, ci->deaths);
        putint(p, ci->totalpoints);
        putint(p, ci->totalfrags);
        putint(p, ci->totaldeaths);
        putint(p, ci->timeplayed);
        putint(p, ci->health);
        putint(p, ci->cptime);
        putint(p, ci->weapselect);
        loopi(W_MAX) putint(p, ci->ammo[i]);
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
            int conlevel = CON_EVENT;
            switch(cn)
            {
                case -3: conlevel = CON_DEBUG; cn = -1; break;
                case -2: conlevel = CON_MESG; cn = -1; break;
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
            srvoutf(4, "\fyCleared all demos");
        }
        else if(demos.inrange(n-1))
        {
            delete[] demos[n-1].data;
            demos.remove(n-1);
            srvoutf(4, "\fyCleared demo \fs\fc%d\fS", n);
        }
    }

    void senddemo(int cn, int num, int dni)
    {
        if(!num) num = demos.length();
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
        sendf(cn, 2, "ri3m", N_SENDDEMO, d.ctime, dni, d.len, d.data);
    }

    void sendwelcome(clientinfo *ci);
    int welcomepacket(packetbuf &p, clientinfo *ci);

    void enddemoplayback()
    {
        if(!demoplayback) return;
        DELETEP(demoplayback);
        loopv(clients) sendf(clients[i]->clientnum, 1, "ri3", N_DEMOPLAYBACK, 0, clients[i]->clientnum);
        srvoutf(4, "\fyDemo playback finished");
        loopv(clients) sendwelcome(clients[i]);
        startintermission(true);
        resetgamevars(true);
    }

    void setupdemoplayback()
    {
        demoheader hdr;
        stringz(msg);
        defformatstring(file, strstr(smapname, "maps/")==smapname || strstr(smapname, "maps\\")==smapname ? "%s.dmo" : "demos/%s.dmo", smapname);
        demoplayback = opengzfile(file, "rb");
        if(!demoplayback) formatstring(msg, "\frCould not read demo \fs\fc%s\fS", file);
        else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, VERSION_DEMOMAGIC, sizeof(hdr.magic)))
            formatstring(msg, "\frSorry, \fs\fc%s\fS is not a demo file", file);
        else
        {
            lilswap(&hdr.gamever, 4);
            if(hdr.gamever!=VERSION_GAME)
                formatstring(msg, "\frDemo \fs\fc%s\fS requires %s version of %s (with protocol version %d)", file, hdr.gamever<VERSION_GAME ? "an older" : "a newer", versionname, hdr.gamever);
        }
        if(msg[0])
        {
            DELETEP(demoplayback);
            srvoutf(4, "%s", msg);
            return;
        }

        srvoutf(4, "\fyPlaying demo \fs\fc%s\fS", file);
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
        stringz(msg);
        if(f->read(&d.hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(d.hdr.magic, VERSION_DEMOMAGIC, sizeof(d.hdr.magic)))
            formatstring(msg, "\fs\fc%s\fS is not a demo file", name);
        else
        {
            lilswap(&d.hdr.gamever, 4);
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
        int len = (int)min(demotmp->size(), stream::offset(G(demomaxsize) + 0x10000));
        demofile &d = demos.add();
        d.ctime = clocktime;
        d.data = new uchar[len];
        d.len = len;
        formatstring(d.info, "%s on %s", gamename(gamemode, mutators, 0, 32), smapname);
        relayf(4, "\fyDemo \fs\fc%s\fS recorded \fs\fc%s UTC\fS [\fs\fw%.2f%s\fS]", d.info, gettime(d.ctime, "%Y-%m-%d %H:%M.%S"), d.len > 1024*1024 ? d.len/(1024*1024.f) : d.len/1024.0f, d.len > 1024*1024 ? "MB" : "kB");
        sendf(-1, 1, "ri4s", N_DEMOREADY, demos.length(), d.ctime, d.len, d.info);
        demotmp->seek(0, SEEK_SET);
        demotmp->read(d.data, len);
        DELETEP(demotmp);
        if(G(demoautoserversave))
        {
            stringz(dafilepath);
            if(*filetimeformat) formatstring(dafilepath, "demos/sv_%s_%s-%s.dmo", gettime(d.ctime, filetimeformat), gamename(gamemode, mutators, 1, 32, '_'), smapname);
            else formatstring(dafilepath, "demos/sv_%u_%s-%s.dmo", uint(d.ctime), gamename(gamemode, mutators, 1, 32, '_'), smapname);
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
                        conoutf("Deleted old demo: %s", files[i]);
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
        if(demorecord->rawtell() >= G(demomaxsize)) enddemorecord(!gs_playing(gamestate));
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

        demotmp = opentempfile("backups/demorecord", "w+b");
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
        if(G(resetmmonend) >= 2) mastermode = MM_OPEN;
        if(G(resetvarsonend) >= 2) resetgamevars(false);
        if(G(resetallowsonend) >= 2) resetallows();
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
            if(oi->actortype > A_PLAYER) continue;
            if(G(votefilter) && !gs_waiting(gamestate) && oi->state == CS_SPECTATOR && !*oi->mapvote) continue; // filter out spectators who haven't voted
            maxvotes++;
            if(!*oi->mapvote) continue;
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
            sendstats();
            endmatch();
            if(best)
            {
                srvoutf(-3, "vote passed: \fs\fy%s\fS on \fs\fo%s\fS", gamename(best->mode, best->muts), best->map);
                changemap(best->map, best->mode, best->muts);
            }
            else
            {
                int mode = G(rotatemode) ? -1 : gamemode, muts = G(rotatemuts) ? -1 : mutators;
                changemode(mode, muts);
                const char *map = choosemap(smapname, mode, muts);
                srvoutf(-3, "server chooses: \fs\fy%s\fS on \fs\fo%s\fS", gamename(mode, muts), map);
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
        if(!ci) return;
        reqmuts |= G(mutslockforce);
        modecheck(reqmode, reqmuts);
        if(!m_game(reqmode)) return;
        if(!reqmap || !*reqmap) reqmap = "<random>";
        bool israndom = !strcmp(reqmap, "<random>");
        if(m_local(reqmode) && !ci->local)
        {
            srvmsgft(ci->clientnum, CON_EVENT, "\frAccess denied, you must be a local client to start a %s game", gametype[reqmode].name);
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
            if(oi->actortype > A_PLAYER || !*oi->mapvote || ci == oi) continue;
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
                    if(!israndom && !m_edit(reqmode))
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
            if(!m_edit(reqmode) && G(mapslock))
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
                    if(!israndom && listincludes(list, reqmap, strlen(reqmap)) < 0 && !haspriv(ci, G(modelock), "select maps not in the rotation"))
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
        ci->lastvote = totalmillis ? totalmillis : 1;
        if(hasveto)
        {
            sendstats();
            endmatch();
            srvoutf(-3, "%s forced: \fs\fy%s\fS on \fs\fo%s\fS", colourname(ci), gamename(ci->modevote, ci->mutsvote), ci->mapvote);
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
        if(!ci->handle[0] && ip && clientip == ip && !strcmp(name, ci->name)) return true;
        return false;
    }

    savedscore *findscore(vector<savedscore> &scores, clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip && !ci->handle[0]) return NULL;
        if(!insert) loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->clientnum != ci->clientnum && scorecmp(ci, ip, oi->name, oi->handle, getclientip(oi->clientnum)))
            {
                oi->updatetimeplayed();
                static savedscore curscore;
                curscore.save(oi);
                return &curscore;
            }
        }
        loopv(scores)
        {
            savedscore &sc = scores[i];
            if(scorecmp(ci, ip, sc.name, sc.handle, sc.ip)) return &sc;
        }
        if(!insert) return NULL;
        savedscore &sc = scores.add();
        copystring(sc.name, ci->name);
        copystring(sc.handle, ci->handle);
        sc.ip = ip;
        return &sc;
    }

    void givepoints(clientinfo *ci, int points, bool give, bool team = true)
    {
        ci->totalpoints += points;
        ci->localtotalpoints += points;
        if(give)
        {
            ci->points += points;
            sendf(-1, 1, "ri5", N_POINTS, ci->clientnum, points, ci->points, ci->totalpoints);
            if(team && m_team(gamemode, mutators) && m_dm(gamemode))
            {
                score &ts = teamscore(ci->team);
                ts.total += points;
                sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
            }
        }
        else if(points) sendf(-1, 1, "ri5", N_POINTS, ci->clientnum, points, ci->points, ci->totalpoints);
    }

    void savescore(clientinfo *ci)
    {
        ci->updatetimeplayed();
        savedscore *sc = findscore(savedscores, ci, true);
        if(sc)
        {
            if(ci->actortype == A_PLAYER && m_dm(gamemode) && m_team(gamemode, mutators) && !m_nopoints(gamemode, mutators) && G(teamkillrestore) && canplay())
            {
                int restorepoints[T_MAX] = {0};
                loopv(ci->teamkills) restorepoints[ci->teamkills[i].team] += ci->teamkills[i].points;
                loopi(T_MAX) if(restorepoints[i] >= G(teamkillrestore))
                {
                    score &ts = teamscore(i);
                    ts.total += restorepoints[i];
                    sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                }
            }
            sc->save(ci);
        }
    }

    void savestatsscore(clientinfo *ci)
    {
        ci->updatetimeplayed();
        savedscore *sc = findscore(savedstatsscores, ci, true);
        if(sc) sc->save(ci);
    }

    void swapteam(clientinfo *ci, int oldteam, int newteam = T_NEUTRAL, bool swaps = true)
    {
        if(ci->swapteam && (!newteam || ci->swapteam == newteam)) ci->swapteam = T_NEUTRAL;
        if(!swaps || ci->actortype != A_PLAYER || !oldteam || oldteam == newteam || !m_swapteam(gamemode, mutators)) return;
        loopv(clients) if(clients[i] && clients[i] != ci)
        {
            clientinfo *cp = clients[i];
            if(cp->actortype != A_PLAYER || (newteam && cp->team != newteam) || !cp->swapteam || cp->swapteam != oldteam) continue;
            setteam(cp, oldteam, TT_RESET|TT_INFOSM, false);
            cp->lastdeath = 0;
            ancmsgft(cp->clientnum, S_V_BALALERT, CON_EVENT, "\fyYou have been moved to %s as previously requested", colourteam(oldteam));
            return;
        }
        if(haspriv(ci, G(teambalancelock)))
        {
            int worst = -1;
            float csk = 0, wsk = 0;
            csk = ci->balancescore();
            loopv(clients) if(clients[i] && clients[i] != ci)
            {
                clientinfo *cp = clients[i];
                if(cp->actortype != A_PLAYER || (newteam && cp->team != newteam)) continue;
                float psk = 0;
                psk = cp->balancescore();
                if(psk > csk || psk > wsk) continue;
                worst = i;
                wsk = psk;
            }
            if(worst >= 0)
            {
                clientinfo *cp = clients[worst];
                setteam(cp, oldteam, TT_RESET|TT_INFOSM, false);
                cp->lastdeath = 0;
                ancmsgft(cp->clientnum, S_V_BALALERT, CON_EVENT, "\fyYou have been moved to %s by higher skilled %s %s", colourteam(oldteam), privname(G(teambalancelock)), colourname(ci));
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
            else if(flags&TT_SMODE && ci->state == CS_ALIVE)
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
            if(ci->isready()) aiman::poke();
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
            else if(ci->actortype >= A_BOT) return team != mapbals[curbalance][0];
            else return team == mapbals[curbalance][0];
        }
        return false;
    }

    int chooseteam(clientinfo *ci, int suggest, bool wantbal)
    {
        if(ci->actortype >= A_ENEMY) return T_ENEMY;
        else if(m_play(gamemode) && m_team(gamemode, mutators) && ci->state != CS_SPECTATOR && ci->state != CS_EDITING)
        {
            bool human = ci->actortype == A_PLAYER;
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
                if(!cp->team || cp->state == CS_SPECTATOR) continue;
                if((cp->actortype > A_PLAYER && cp->ownernum < 0) || cp->actortype >= A_ENEMY) continue;
                teamcheck &ts = teamchecks[cp->team-T_FIRST];
                if(team > 0 && m_swapteam(gamemode, mutators) && ci->actortype == A_PLAYER && cp->actortype == A_PLAYER && cp->swapteam && ci->team == cp->swapteam && cp->team == team)
                    return team; // swapteam
                if(ci->actortype > A_PLAYER || (ci->actortype == A_PLAYER && cp->actortype == A_PLAYER))
                { // remember: ai just balance teams
                    ts.score += cp->balancescore(1);
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
                    vector <int> possibleteams;
                    loopi(numteams(gamemode, mutators)) if(allowteam(ci, teamchecks[i].team, T_FIRST, false))
                    {
                        teamcheck &ts = teamchecks[i];
                        if(ts.score == worst->score && ts.clients == worst->clients)
                        {
                            possibleteams.add(ts.team);
                        }
                    }
                    team = possibleteams[rnd(possibleteams.length())];
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
    void welcomeinitclient(clientinfo *ci, packetbuf &p, int exclude = -1, bool nobots = false);

    #include "auth.h"

    enum { ALST_TRY = 0, ALST_SPAWN, ALST_SPEC, ALST_EDIT, ALST_WALK, ALST_MAX };

    bool getmap(clientinfo *ci = NULL, bool force = false);

    bool crclocked(clientinfo *ci, bool msg = false)
    {
        if(m_play(gamemode) && G(crclock) && ci->actortype == A_PLAYER && (smapcrc ? ci->clientcrc != smapcrc : !ci->clientcrc) && !haspriv(ci, G(crclock)))
        {
            if(msg) srvmsgft(ci->clientnum, CON_EVENT, "\fyYou are \fs\fccrc locked\fS, please wait for the correct map version..");
            return true;
        }
        return false;
    }

    void spectator(clientinfo *ci, bool quarantine = false, int sender = -1)
    {
        if(!ci || ci->actortype > A_PLAYER) return;
        ci->state = CS_SPECTATOR;
        ci->quarantine = quarantine;
        sendf(sender, 1, "ri3", N_SPECTATOR, ci->clientnum, quarantine ? 2 : 1);
        setteam(ci, T_NEUTRAL, TT_INFOSM);
    }

    bool spectate(clientinfo *ci, bool val, bool quarantine = false)
    {
        if(ci->state != CS_SPECTATOR && val)
        {
            if(ci->state == CS_ALIVE)
            {
                suicideevent ev;
                ev.flags = HIT_SPEC;
                ev.process(ci); // process death immediately
            }
            if(smode) smode->leavegame(ci);
            mutate(smuts, mut->leavegame(ci));
            sendf(-1, 1, "ri3", N_SPECTATOR, ci->clientnum, quarantine ? 2 : 1);
            ci->state = CS_SPECTATOR;
            ci->quarantine = quarantine;
            ci->updatetimeplayed();
            setteam(ci, T_NEUTRAL, TT_INFO);
            if(ci->isready()) aiman::poke();
        }
        else if(ci->state == CS_SPECTATOR && !val)
        {
            if(crclocked(ci, true))
            {
                getmap(ci);
                return false;
            }
            int nospawn = 0;
            if(numclients(ci->clientnum, true) >= G(serverclients)) nospawn++;
            if(smode && !smode->canspawn(ci, true)) { nospawn++; }
            mutate(smuts, if(!mut->canspawn(ci, true)) { nospawn++; });
            ci->state = CS_DEAD;
            if(nospawn)
            {
                spectate(ci, true);
                return false;
            }
            ci->lasttimeplayed = totalmillis ? totalmillis : 1;
            ci->lasttimealive = totalmillis ? totalmillis : 1;
            ci->lasttimeactive = totalmillis ? totalmillis : 1;
            ci->lasttimewielded = totalmillis ? totalmillis : 1;
            loopi(W_MAX) ci->lasttimeloadout[i] = totalmillis ? totalmillis : 1;
            ci->quarantine = false;
            waiting(ci, DROP_RESET);
            if(smode) smode->entergame(ci);
            mutate(smuts, mut->entergame(ci));
            if(ci->isready()) aiman::poke();
        }
        return true;
    }

    struct clientcrcs
    {
        int id;
        vector<clientinfo *> clients;
        clientcrcs() {}
        clientcrcs(int n, clientinfo *m) { id = n; clients.add(m); }
        ~clientcrcs() { clients.setsize(0); }
    };

    void resetmapdata(bool get = false)
    {
        smapcrc = 0;
        mapsending = -1;
        loopi(SENDMAP_MAX) if(mapdata[i]) DELETEP(mapdata[i]);
        if(get) getmap();
    }

    bool hasmapdata()
    {
        if(m_edit(gamemode))
        {
            loopi(SENDMAP_EDIT) if(!mapdata[i]) return false;
            return true;
        }
        if(!smapcrc) return false;
        loopi(SENDMAP_HAS) if(!mapdata[i]) return false;
        return true;
    }

    bool getmap(clientinfo *ci, bool force)
    {
        if(gs_intermission(gamestate)) return false; // pointless
        if(ci && !numclients(ci->clientnum) && !hasmapdata())
        {
            ci->wantsmap = false;
            sendf(ci->clientnum, 1, "ri", N_FAILMAP);
            return false;
        }
        if(ci)
        {
            ci->clientcrc = 0;
            ci->wantsmap = true;
            if(mapsending == ci->clientnum)
            {
                resetmapdata();
                return false;
            }
            if(mapsending >= 0)
            {
                srvmsgft(ci->clientnum, CON_EVENT, "\fyThe map is being uploaded, please wait..");
                return true;
            }
            if(hasmapdata())
            {
                if(ci->gettingmap) return true;
                ci->gettingmap = true;
                srvmsgft(ci->clientnum, CON_EVENT, "\fySending you the map, please wait..");
                loopi(SENDMAP_MAX) if(mapdata[i]) sendfile(ci->clientnum, 2, mapdata[i], "ri3s", N_SENDMAPFILE, i, smapcrc, smapname);
                sendwelcome(ci);
                ci->needclipboard = 0;
                return true;
            }
        }
        if((!force && gs_waiting(gamestate)) || mapsending >= 0 || hasmapdata()) return false;
        clientinfo *best = NULL;
        if(!m_edit(gamemode) || force)
        {
            vector<clientcrcs> crcs;
            loopv(clients)
            {
                clientinfo *cs = clients[i];
                if(cs->actortype > A_PLAYER || !cs->name[0] || !cs->online || cs->wantsmap || !cs->clientcrc || !cs->ready) continue;
                bool found = false;
                loopvj(crcs) if(crcs[j].id == cs->clientcrc)
                {
                    crcs[j].clients.add(cs);
                    found = true;
                    break;
                }
                if(!found) crcs.add(clientcrcs(cs->clientcrc, cs));
            }
            int n = -1;
            loopv(crcs) if(n < 0 || crcs[n].clients.length() < crcs[i].clients.length()) n = i;
            if(n > 0) loopv(crcs[n].clients)
            {
                clientinfo *cs = crcs[n].clients[i];
                cs->updatetimeplayed();
                if(!best || cs->timeplayed > best->timeplayed) best = cs;
            }
        }
        if(!best) loopv(clients)
        {
            clientinfo *cs = clients[i];
            if(cs->actortype > A_PLAYER || !cs->name[0] || !cs->online || cs->wantsmap || !cs->ready) continue;
            cs->updatetimeplayed();
            if(!best || cs->timeplayed > best->timeplayed) best = cs;
        }
        if(best)
        {
            mapsending = best->clientnum;
            if(m_edit(gamemode))
            {
                smapcrc = 0;
                srvoutf(4, "\fyThe map is being requested from %s..", colourname(best));
            }
            else
            {
                smapcrc = best->clientcrc;
                srvoutf(4, "\fyThe map crc \fs\fc0x%.8x\fS is being requested from %s..", smapcrc, colourname(best));
            }
            sendf(best->clientnum, 1, "ri", N_GETMAP);
            loopv(clients)
            {
                clientinfo *cs = clients[i];
                if(cs->actortype > A_PLAYER || !cs->name[0] || !cs->online || !cs->ready) continue;
                if(cs->wantsmap || crclocked(cs, true))
                {
                    cs->clientcrc = 0;
                    cs->wantsmap = true;
                    spectate(cs, true);
                }
            }
            return true;
        }
        if(ci) srvmsgft(ci->clientnum, CON_EVENT, "\fySorry, unable to get a map..");
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
                if(ci->quarantine || (ci->state == CS_SPECTATOR && numclients(ci->clientnum, true) >= G(serverclients))) return false;
                if(ci->actortype == A_PLAYER)
                    if(mastermode >= MM_LOCKED && ip && !checkipinfo(control, ipinfo::ALLOW, ip) && !haspriv(ci, lock, "spawn"))
                        return false;
                if(ci->state == CS_ALIVE || ci->state == CS_WAITING) return false;
                if(ci->lastdeath && gamemillis-ci->lastdeath <= DEATHMILLIS) return false;
                if(crclocked(ci, true))
                {
                    getmap(ci);
                    return false;
                }
                break;
            }
            case ALST_SPAWN: // spawn
            {
                if(ci->quarantine || (ci->state == CS_SPECTATOR && numclients(ci->clientnum, true) >= G(serverclients))) return false;
                if(ci->state != CS_DEAD && ci->state != CS_WAITING) return false;
                if(ci->lastdeath && gamemillis-ci->lastdeath <= DEATHMILLIS) return false;
                if(crclocked(ci, true))
                {
                    getmap(ci);
                    return false;
                }
                break;
            }
            case ALST_SPEC: return ci->actortype == A_PLAYER; // spec
            case ALST_WALK: if(ci->state != CS_EDITING) return false;
            case ALST_EDIT: // edit on/off
            {
                if(ci->quarantine || (ci->state == CS_SPECTATOR && numclients(ci->clientnum, true) >= G(serverclients)) || ci->actortype != A_PLAYER || !m_edit(gamemode)) return false;
                if(mastermode >= MM_LOCKED && ip && !checkipinfo(control, ipinfo::ALLOW, ip) && !haspriv(ci, lock, "edit")) return false;
                break;
            }
            default: break;
        }
        return true;
    }

    void sendstats(bool fromintermission)
    {
        if(G(serverstats) && auth::hasstats && !sentstats && gamemillis)
        {
            loopv(clients) if(clients[i]->actortype == A_PLAYER) savestatsscore(clients[i]);
            bool worthy = false;
            if(fromintermission) worthy = true;
            else if(m_laptime(gamemode, mutators))
            {
                loopv(savedstatsscores) if(savedstatsscores[i].actortype == A_PLAYER) if(savedstatsscores[i].cptime > 0)
                {
                    worthy = true;
                    break;
                }
            }
            if(!worthy) return;

            loopv(clients)
            {
                clients[i]->localtotalpoints -= clients[i]->points;
                clients[i]->localtotalfrags -= clients[i]->frags;
                clients[i]->localtotaldeaths -= clients[i]->deaths;
            }

            sentstats = true;
            requestmasterf("stats begin\n");
            int unique = 0;
            vector<uint> seen;
            loopv(savedstatsscores) if(savedstatsscores[i].actortype == A_PLAYER)
            {
                if((gamemillis / 1000 / 25) >= savedstatsscores[i].timeactive) continue;
                if(savedstatsscores[i].handle[0])
                {
                    seen.add(savedstatsscores[i].ip);
                    unique += 1;
                }
                else
                {
                    bool inseen = false;
                    loopvj(seen) if(seen[j] == savedstatsscores[i].ip) inseen = true;
                    if(!inseen)
                    {
                        seen.add(savedstatsscores[i].ip);
                        unique += 1;
                    }
                }
            }
            requestmasterf("stats game %s %d %d %d %d %d\n", escapestring(smapname), gamemode, mutators, gamemillis/1000, unique, m_normalweapons(gamemode, mutators) ? 1 : 0);
            flushmasteroutput();
            requestmasterf("stats server %s %s %d\n", escapestring(limitstring(G(serverdesc), MAXSDESCLEN+1)), versionstring, serverport);
            flushmasteroutput();
            loopi(numteams(gamemode, mutators))
            {
                int tp = m_team(gamemode, mutators) ? T_FIRST : T_NEUTRAL;
                requestmasterf("stats team %d %d %s\n", i + tp, teamscore(i + tp).total, escapestring(TEAM(i + tp, name)));
                flushmasteroutput();
            }
            loopv(savedstatsscores) if(savedstatsscores[i].actortype == A_PLAYER && (savedstatsscores[i].timealive > 0 || savedstatsscores[i].timeactive > 0))
            {
                requestmasterf("stats player %s %s %d %d %d %d %d %d\n",
                    escapestring(savedstatsscores[i].name), escapestring(savedstatsscores[i].handle),
                    m_laptime(gamemode, mutators) ? savedstatsscores[i].cptime : savedstatsscores[i].points,
                    savedstatsscores[i].timealive, savedstatsscores[i].frags, savedstatsscores[i].deaths, i,
                    savedstatsscores[i].timeactive
                );
                flushmasteroutput();
                loopj(W_MAX)
                {
                    weaponstats w = savedstatsscores[i].weapstats[j];
                    if (w.timewielded == 0 && w.timeloadout == 0) continue;
                    requestmasterf("stats weapon %d %s %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                        i, escapestring(savedstatsscores[i].handle), weaptype[j].name, w.timewielded, w.timeloadout,
                        w.damage1, w.frags1, w.hits1, w.flakhits1, w.shots1, w.flakshots1,
                        w.damage2, w.frags2, w.hits2, w.flakhits2, w.shots2, w.flakshots2
                    );
                    flushmasteroutput();
                }
                loopvj(savedstatsscores[i].captures)
                {
                    requestmasterf("stats capture %d %s %d %d\n",
                        i, escapestring(savedstatsscores[i].handle),
                        savedstatsscores[i].captures[j].capturing, savedstatsscores[i].captures[j].captured);
                    flushmasteroutput();
                }
                loopvj(savedstatsscores[i].bombings)
                {
                    requestmasterf("stats bombing %d %s %d %d\n",
                        i, escapestring(savedstatsscores[i].handle),
                        savedstatsscores[i].bombings[j].bombing, savedstatsscores[i].bombings[j].bombed);
                    flushmasteroutput();
                }
                loopvj(savedstatsscores[i].ffarounds)
                {
                    requestmasterf("stats ffaround %d %s %d %d\n",
                        i, escapestring(savedstatsscores[i].handle),
                        savedstatsscores[i].ffarounds[j].round, (int)savedstatsscores[i].ffarounds[j].winner);
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
        hasgameinfo = shouldcheckvotes = firstblood = sentstats = false;
        mapgameinfo = -1;
        smapvariant = G(forcemapvariant) ? G(forcemapvariant) : (m_edit(mode) ? MPV_DEFAULT : 1+rnd(MPV_MAX-1));
        stopdemo();
        resetmapdata();
        changemode(gamemode = mode, mutators = muts);
        curbalance = nextbalance = lastteambalance = nextteambalance = gamemillis = 0;
        gamestate = G_S_WAITING;
        gamewaittime = 0;
        bool hastime = m_play(gamemode) && m_mmvar(gamemode, mutators, timelimit);
        oldtimelimit = hastime ? m_mmvar(gamemode, mutators, timelimit) : -1;
        timeremaining = hastime ? m_mmvar(gamemode, mutators, timelimit)*60 : -1;
        gamelimit = hastime ? timeremaining*1000 : 0;
        loopv(savedscores) savedscores[i].mapchange();
        loopv(savedstatsscores) savedstatsscores[i].mapchange();
        setuptriggers(false);
        setupspawns(false);
        if(smode) smode->reset();
        mutate(smuts, mut->reset());
        smode = NULL;
        smuts.shrink(0);
        sents.shrink(0);
        scores.shrink(0);
        aiman::clearai();
        aiman::poke();
        const char *reqmap = name && *name && strcmp(name, "<random>") ? name : pickmap(NULL, gamemode, mutators);
        ifserver(reqmap && *reqmap)
        {
            loopi(SENDMAP_MAX)
            {
                defformatstring(reqfile, strstr(reqmap, "maps/")==reqmap || strstr(reqmap, "maps\\")==reqmap ? "%s.%s" : "maps/%s.%s", reqmap, sendmaptypes[i]);
                if(i == SENDMAP_MPZ) smapcrc = crcfile(reqfile);
                mapdata[i] = openfile(reqfile, "rb");
            }
            if(!hasmapdata()) resetmapdata();
        }
        copystring(smapname, reqmap);
        sendf(-1, 1, "risi4", N_MAPCHANGE, smapname, gamemode, mutators, hasmapdata() ? smapcrc : -1, smapvariant);

        // server modes
        if(m_capture(gamemode)) smode = &capturemode;
        else if(m_defend(gamemode)) smode = &defendmode;
        else if(m_bomber(gamemode)) smode = &bombermode;
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
        if(id && id->flags&IDF_SERVER && id->flags&IDF_GAMEMOD) switch(id->type)
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
                    conoutft(CON_DEBUG, "\fy\fs\fc%s\fS returned \fs\fc%s\fS", id->name, ret && *ret ? ret : "failed");
                    delete[] s;
                    delete[] ret;
                    return true;
                }
                case ID_VAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        conoutft(CON_DEBUG, id->flags&IDF_HEX && *id->storage.i >= 0 ? (id->maxval==0xFFFFFF ? "\fy%s = 0x%.6X" : (uint(id->maxval)==0xFFFFFFFFU ? "\fy%s = 0x%.8X" : "\fy%s = 0x%X")) : "\fy%s = %d", id->name, id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU ? uint(*id->storage.i) : *id->storage.i);
                        return true;
                    }
                    if(id->flags&IDF_READONLY)
                    {
                        conoutft(CON_DEBUG, "\frCannot override variable: %s", id->name);
                        return true;
                    }
                    int ret = parseint(arg);
                    if(id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU)
                    {
                        if(uint(ret) < uint(id->minval) || uint(ret) > uint(id->maxval))
                        {
                            conoutft(CON_DEBUG, "\frValid range for %s is 0x%X..0x%X", id->name, uint(id->minval), uint(id->maxval));
                            return true;
                        }
                    }
                    else if(ret < id->minval || ret > id->maxval)
                    {
                        conoutft(CON_DEBUG,
                            id->flags&IDF_HEX ?
                                    (id->minval <= 255 ? "\frValid range for %s is %d..0x%X" : "\frValid range for %s is 0x%X..0x%X") :
                                    "\frValid range for %s is %d..%d", id->name, id->minval, id->maxval);
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
                        conoutft(CON_DEBUG, "\fy%s = %s", id->name, floatstr(*id->storage.f));
                        return true;
                    }
                    if(id->maxvalf < id->minvalf || id->flags&IDF_READONLY)
                    {
                        conoutft(CON_DEBUG, "\frCannot override variable: %s", id->name);
                        return true;
                    }
                    float ret = parsefloat(arg);
                    if(ret < id->minvalf || ret > id->maxvalf)
                    {
                        conoutft(CON_DEBUG, "\frValid range for %s is %s..%s", id->name, floatstr(id->minvalf), floatstr(id->maxvalf));
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
                        conoutft(CON_DEBUG, strchr(*id->storage.s, '"') ? "\fy%s = [%s]" : "\fy%s = \"%s\"", id->name, *id->storage.s);
                        return true;
                    }
                    if(id->flags&IDF_READONLY)
                    {
                        conoutft(CON_DEBUG, "\frCannot override variable: %s", id->name);
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
            int locked = min(max(id->flags&IDF_ADMIN ? int(G(adminlock)) : (id->flags&IDF_MODERATOR ? int(G(moderatorlock)) : 0), G(varslock)), int(PRIV_CREATOR));
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
                    srvoutf(3, "\fy%s executed \fs\fc%s\fS (returned: \fs\fc%s\fS)", colourname(ci), name, ret && * ret ? ret : "failed");
                    delete[] s;
                    delete[] ret;
                    return;
                }
                case ID_VAR:
                {
                    if(nargs <= 1 || !arg)
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, id->flags&IDF_HEX && *id->storage.i >= 0 ? (id->maxval==0xFFFFFF ? "\fy%s = 0x%.6X" : (uint(id->maxval)==0xFFFFFFFFU ? "\fy%s = 0x%.8X" : "\fy%s = 0x%X")) : "\fy%s = %d", name, id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU ? uint(*id->storage.i) : *id->storage.i);
                        return;
                    }
                    else if(locked && !haspriv(ci, locked, "change that variable"))
                    {
                        val = intstr(id);
                        sendf(ci->clientnum, 1, "ri2sis", N_COMMAND, -1, name, strlen(val), val);
                        return;
                    }
                    if(id->flags&IDF_READONLY)
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, "\frCannot override variable: %s", name);
                        return;
                    }
                    int ret = parseint(arg);
                    if(id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU)
                    {
                        if(uint(ret) < uint(id->minval) || uint(ret) > uint(id->maxval))
                        {
                            srvmsgf(ci->clientnum, "\frValid range for %s is 0x%X..0x%X", id->name, uint(id->minval), uint(id->maxval));
                            return;
                        }
                    }
                    else if(ret < id->minval || ret > id->maxval)
                    {
                        srvmsgf(ci->clientnum,
                            id->flags&IDF_HEX ?
                                (id->minval <= 255 ? "\frValid range for %s is %d..0x%X" : "\frValid range for %s is 0x%X..0x%X") :
                                "\frValid range for %s is %d..%d", name, id->minval, id->maxval);
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
                        srvmsgft(ci->clientnum, CON_DEBUG, "\fy%s = %s", name, floatstr(*id->storage.f));
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
                        srvmsgft(ci->clientnum, CON_DEBUG, "\frCannot override variable: %s", name);
                        return;
                    }
                    float ret = parsefloat(arg);
                    if(ret < id->minvalf || ret > id->maxvalf)
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, "\frValid range for %s is %s..%s", name, floatstr(id->minvalf), floatstr(id->maxvalf));
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
                        srvmsgft(ci->clientnum, CON_DEBUG, strchr(*id->storage.s, '"') ? "\fy%s = [%s]" : "\fy%s = \"%s\"", name, *id->storage.s);
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
                        srvmsgft(ci->clientnum, CON_DEBUG, "\frCannot override variable: %s", name);
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
        else srvmsgft(ci->clientnum, CON_DEBUG, "\frUnknown command: %s", cmd);
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
        sendf(ci->clientnum, 1, "ri3si", N_SERVERINIT, ci->clientnum, VERSION_GAME, gethostip(ci->clientnum), ci->sessionid);
    }

    bool restorescore(clientinfo *ci)
    {
        savedscore *sc = findscore(savedscores, ci, false);
        if(sc)
        {
            sc->restore(ci);
            if(ci->actortype == A_PLAYER && m_dm(gamemode) && m_team(gamemode, mutators) && !m_nopoints(gamemode, mutators) && G(teamkillrestore) && canplay())
            {
                int restorepoints[T_MAX] = {0};
                loopv(ci->teamkills) restorepoints[ci->teamkills[i].team] += ci->teamkills[i].points;
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

    void sendresume(clientinfo *ci, bool reset = false)
    {
        ci->updatetimeplayed();
        if(reset) ci->weapreset(false);
        sendf(reset ? ci->clientnum : -1, 1, "ri9i4vi", N_RESUME, ci->clientnum, reset ? -1 : ci->state, ci->points, ci->frags, ci->deaths, ci->totalpoints, ci->totalfrags, ci->totaldeaths, ci->timeplayed, ci->health, ci->cptime, ci->weapselect, W_MAX, &ci->ammo[0], -1);
    }

    void putinitclient(clientinfo *ci, packetbuf &p, bool allow)
    {
        if(ci->actortype > A_PLAYER)
        {
            if(ci->ownernum >= 0)
            {
                putint(p, N_INITAI);
                putint(p, ci->clientnum);
                putint(p, ci->ownernum);
                putint(p, ci->actortype);
                putint(p, ci->spawnpoint);
                putint(p, ci->skill);
                sendstring(ci->name, p);
                putint(p, ci->team);
                putint(p, ci->colour);
                putint(p, ci->model);
                putint(p, ci->pattern);
                sendstring(ci->vanity, p);
                putint(p, ci->loadweap.length());
                loopv(ci->loadweap) putint(p, ci->loadweap[i]);
            }
        }
        else
        {
            putint(p, N_CLIENTINIT);
            putint(p, ci->clientnum);
            putint(p, ci->colour);
            putint(p, ci->model);
            putint(p, ci->pattern);
            putint(p, ci->checkpointspawn);
            putint(p, ci->team);
            putint(p, ci->privilege);
            sendstring(ci->name, p);
            sendstring(ci->vanity, p);
            putint(p, ci->loadweap.length());
            loopv(ci->loadweap) putint(p, ci->loadweap[i]);
            putint(p, ci->randweap.length());
            loopv(ci->randweap) putint(p, ci->randweap[i]);
            sendstring(ci->handle, p);
            sendstring(allow ? gethostip(ci->clientnum) : "*", p);
            ci->version.put(p);
        }
    }

    void sendinitclient(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE), q(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p, true);
        p.finalize();
        putinitclient(ci, q, false);
        q.finalize();
        loopv(clients) if(clients[i] != ci && allowbroadcast(clients[i]->clientnum))
            sendpacket(clients[i]->clientnum, 1, haspriv(clients[i], G(iphostlock)) ? p.packet : q.packet);
        sendpacket(-1, -1, q.packet); // anonymous packet just for recording
    }

    void sendinitclientself(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p, true);
        sendpacket(ci->clientnum, 1, p.finalize());
    }

    void welcomeinitclient(clientinfo *ci, packetbuf &p, int exclude, bool nobots)
    {
        bool iph = ci ? haspriv(ci, G(iphostlock)) : false;
        loopv(clients)
        {
            clientinfo *cp = clients[i];
            if(!cp->connected || cp->clientnum == exclude || (nobots && cp->actortype != A_PLAYER)) continue;
            putinitclient(cp, p, iph);
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
        putint(p, mastermode);
        putint(p, N_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, gamemode);
        putint(p, mutators);
        if(ci && !ci->online && m_edit(gamemode))
        {
            if(numclients(ci->clientnum))
            {
                if(mapsending < 0) resetmapdata();
                getmap(ci);
                putint(p, -2); // start with an empty map and wait for it
            }
            else putint(p, -1); // start with an empty map and use it
        }
        else putint(p, smapcrc);
        putint(p, smapvariant);

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

        if(!ci || numclients())
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
            ci->state = CS_SPECTATOR;
            ci->team = T_NEUTRAL;
            putint(p, N_SPECTATOR);
            putint(p, ci->clientnum);
            putint(p, ci->quarantine ? 2 : 1);
            sendf(-1, 1, "ri3x", N_SPECTATOR, ci->clientnum, ci->quarantine ? 2 : 1, ci->clientnum);
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
                sendstate(oi, p);
            }
            putint(p, -1);
            welcomeinitclient(ci, p, ci ? ci->clientnum : -1);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->actortype > A_PLAYER || (ci && oi->clientnum == ci->clientnum)) continue;
                if(*oi->mapvote)
                {
                    putint(p, N_MAPVOTE);
                    putint(p, oi->clientnum);
                    sendstring(oi->mapvote, p);
                    putint(p, oi->modevote);
                    putint(p, oi->mutsvote);
                }
            }
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

        if(ci && !ci->online && *G(servermotd))
        {
            putint(p, N_ANNOUNCE);
            putint(p, S_ACTION);
            putint(p, CON_MESG);
            sendstring(G(servermotd), p);
        }

        if(ci) ci->online = true;
        return 1;
    }

    void clearevent(clientinfo *ci) { delete ci->events.remove(0); }

    void addhistory(clientinfo *m, clientinfo *v, int millis)
    {
        bool found = false;
        loopv(m->damagelog) if (m->damagelog[i].clientnum == v->clientnum)
        {
            m->damagelog[i].millis = millis;
            found = true;
            break;
        }
        if(!found) m->damagelog.add(dmghist(v->clientnum, millis));
    }

    void gethistory(clientinfo *m, clientinfo *v, int millis, vector<int> &log, bool clear = false, int points = 0, int lastpoints = 0)
    {
        int last = -1;
        if(lastpoints)
        {
            loopv(m->damagelog) if(m->damagelog[i].clientnum != v->clientnum && millis-m->damagelog[i].millis <= G(lasthitdelay))
            {
                if(last < 0 || m->damagelog[i].millis > m->damagelog[last].millis) last = i;
            }
        }
        loopv(m->damagelog) if(m->damagelog[i].clientnum != v->clientnum)
        {
            clientinfo *assist = (clientinfo *)getinfo(m->damagelog[i].clientnum);
            if(assist)
            {
                if(millis-m->damagelog[i].millis <= G(assistkilldelay)) log.add(assist->clientnum);
                if(points)
                {
                    if(!lastpoints || i != last)
                    {
                        if(millis-m->damagelog[i].millis <= G(assistkilldelay)) givepoints(assist, points, m_points(gamemode, mutators), true);
                    }
                    else givepoints(assist, lastpoints, m_points(gamemode, mutators), true);
                }
            }
        }
        if(clear) m->damagelog.shrink(0);
    }

    bool isghost(clientinfo *d, clientinfo *e)
    {
        if(!e) return false;
        if(d->actortype < A_ENEMY && e->actortype < A_ENEMY && m_ghost(gamemode, mutators)) return true;
        switch(d->actortype)
        {
            case A_PLAYER: if(!(AA(e->actortype, collide)&(1<<A_C_PLAYERS))) return true; break;
            case A_BOT: if(!(AA(e->actortype, collide)&(1<<A_C_BOTS))) return true; break;
            default: if(!(AA(e->actortype, collide)&(1<<A_C_ENEMIES))) return true; break;
        }
        if(m_team(gamemode, mutators) && d->team == e->team) switch(d->actortype)
        {
            case A_PLAYER: if(!(AA(e->actortype, teamdamage)&(1<<A_T_PLAYERS))) return true; break;
            case A_BOT: if(!(AA(e->actortype, teamdamage)&(1<<A_T_BOTS))) return true; break;
            default: if(!(AA(e->actortype, teamdamage)&(1<<A_T_ENEMIES))) return true; break;
        }
        return false;
    }

    void dodamage(clientinfo *m, clientinfo *v, int damage, int weap, int fromweap, int fromflags, int flags, int material, const ivec &hitpush = ivec(0, 0, 0), const ivec &hitvel = ivec(0, 0, 0), float dist = 0, bool first = true)
    {
        int realdamage = damage, realflags = flags, nodamage = 0, hurt = 0, statweap = fromweap, statalt = WS(fromflags);
        realflags &= ~HIT_SFLAGS;
        if(realflags&HIT_MATERIAL && (material&MATF_VOLUME) == MAT_LAVA) realflags |= HIT_BURN;

        if(smode && !smode->damage(m, v, realdamage, weap, realflags, material, hitpush, hitvel, dist)) { nodamage++; }
        mutate(smuts, if(!mut->damage(m, v, realdamage, weap, realflags, material, hitpush, hitvel, dist)) { nodamage++; });
        if(!(realflags&HIT_MATERIAL) && v->actortype < A_ENEMY)
        {
            if(v == m && !G(damageself)) nodamage++;
            else if(isghost(m, v)) nodamage++;
        }

        if(isweap(weap) && WF(WK(flags), weap, residualundo, WS(flags)) != 0)
        {
            if(WF(WK(flags), weap, residualundo, WS(flags))&WR(BURN) && m->burning(gamemillis, G(burntime)))
            {
                m->lastres[WR_BURN] = m->lastrestime[WR_BURN] = 0;
                sendf(-1, 1, "ri3", N_SPHY, m->clientnum, SPHY_EXTINGUISH);
            }
            if(WF(WK(flags), weap, residualundo, WS(flags))&WR(BLEED) && m->bleeding(gamemillis, G(bleedtime)))
                m->lastres[WR_BLEED] = m->lastrestime[WR_BLEED] = 0;
            if(WF(WK(flags), weap, residualundo, WS(flags))&WR(SHOCK) && m->shocking(gamemillis, G(shocktime)))
                m->lastres[WR_SHOCK] = m->lastrestime[WR_SHOCK] = 0;
        }

        if(nodamage || !hitdealt(realflags))
        {
            realflags &= ~HIT_CLEAR;
            realflags |= HIT_WAVE;
        }
        else
        {
            m->health = min(m->health-realdamage, m_maxhealth(gamemode, mutators, m->actortype));
            if(realdamage > 0)
            {
                hurt = min(m->health, realdamage);
                m->lastregen = m->lastregenamt = 0;
                m->lastpain = gamemillis;
                v->damage += realdamage;
                if(m != v && (!m_team(gamemode, mutators) || m->team != v->team))
                {
                    if(weap == -1)
                    {
                        if(flags&HIT_BURN)
                        {
                            statalt = m->lastresalt[WR_BURN];
                            statweap = m->lastresweapon[WR_BURN];
                            if(isweap(statweap))
                            {
                                if(statalt) v->weapstats[statweap].damage2 += realdamage;
                                else v->weapstats[statweap].damage1 += realdamage;
                            }
                        }
                        if(flags&HIT_BLEED)
                        {
                            statalt = m->lastresalt[WR_BLEED];
                            statweap = m->lastresweapon[WR_BLEED];
                            if(isweap(statweap))
                            {
                                if(statalt) v->weapstats[statweap].damage2 += realdamage;
                                else v->weapstats[statweap].damage1 += realdamage;
                            }
                        }
                        if(flags&HIT_SHOCK)
                        {
                            statalt = m->lastresalt[WR_SHOCK];
                            statweap = m->lastresweapon[WR_SHOCK];
                            if(isweap(statweap))
                            {
                                if(statalt) v->weapstats[statweap].damage2 += realdamage;
                                else v->weapstats[statweap].damage1 += realdamage;
                            }
                        }
                    }
                    else if(isweap(statweap))
                    {
                        if(statalt) v->weapstats[statweap].damage2 += realdamage;
                        else v->weapstats[statweap].damage1 += realdamage;
                    }
                }
                if(m->health <= 0) realflags |= HIT_KILL;
                if(wr_burning(weap, flags) && (m->submerged < G(liquidextinguish) || (m->inmaterial&MATF_VOLUME) != MAT_WATER))
                {
                    m->lastres[WR_BURN] = m->lastrestime[WR_BURN] = gamemillis;
                    m->lastresowner[WR_BURN] = v->clientnum;
                    m->lastresweapon[WR_BURN] = fromweap;
                    m->lastresalt[WR_BURN] = statalt;
                }
                if(wr_bleeding(weap, flags))
                {
                    m->lastres[WR_BLEED] = m->lastrestime[WR_BLEED] = gamemillis;
                    m->lastresowner[WR_BLEED] = v->clientnum;
                    m->lastresweapon[WR_BLEED] = fromweap;
                    m->lastresalt[WR_BLEED] = statalt;
                }
                if(wr_shocking(weap, flags))
                {
                    m->lastres[WR_SHOCK] = m->lastrestime[WR_SHOCK] = gamemillis;
                    m->lastresowner[WR_SHOCK] = v->clientnum;
                    m->lastresweapon[WR_SHOCK] = fromweap;
                    m->lastresalt[WR_SHOCK] = statalt;
                }
                if(isweap(statweap) && m != v && (!m_team(gamemode, mutators) || m->team != v->team) && first)
                {
                    if(WK(flags))
                    {
                        if(statalt) v->weapstats[statweap].flakhits2++;
                        else v->weapstats[statweap].flakhits1++;
                    }
                    else
                    {
                        if(statalt) v->weapstats[statweap].hits2++;
                        else v->weapstats[statweap].hits1++;
                    }
                }
            }
        }
        if(smode) smode->dodamage(m, v, realdamage, hurt, weap, realflags, material, hitpush, hitvel, dist);
        mutate(smuts, mut->dodamage(m, v, realdamage, hurt, weap, realflags, material, hitpush, hitvel, dist));
        if(realdamage >= 0 && m != v && (!m_team(gamemode, mutators) || m->team != v->team))
            addhistory(m, v, gamemillis);
        sendf(-1, 1, "ri9i5", N_DAMAGE, m->clientnum, v->clientnum, weap, realflags, realdamage, m->health, hitpush.x, hitpush.y, hitpush.z, hitvel.x, hitvel.y, hitvel.z, int(dist*DNF));
        if(realflags&HIT_KILL)
        {
            int fragvalue = 1;
            if(m != v && (!m_team(gamemode, mutators) || m->team != v->team))
            {
                v->frags++;
                v->totalfrags++;
                v->localtotalfrags++;
                if(isweap(statweap))
                {
                    if(statalt) v->weapstats[statweap].frags2++;
                    else v->weapstats[statweap].frags1++;
                }
            }
            else fragvalue = -fragvalue;
            bool isai = m->actortype >= A_ENEMY, isteamkill = false;
            int pointvalue = fragvalue, style = FRAG_NONE;
            if(!m_dm_oldschool(gamemode, mutators))
                pointvalue = (smode && !isai ? smode->points(m, v) : fragvalue)*(isai ? G(enemybonus) : G(fragbonus));
            if(realdamage >= (realflags&HIT_EXPLODE ? m_health(gamemode, mutators, m->actortype)/2 : m_health(gamemode, mutators, m->actortype)))
                style = FRAG_OBLITERATE;
            m->spree = 0;
            if(m_team(gamemode, mutators) && v->team == m->team)
            {
                v->spree = 0;
                if(isweap(weap) && (v == m || WF(WK(flags), weap, damagepenalty, WS(flags)) != 0))
                {
                    if(!m_dm_oldschool(gamemode, mutators)) pointvalue *= G(teamkillpenalty);
                    if(v != m) isteamkill = true;
                }
                else pointvalue = 0; // no penalty
            }
            else if(v != m && v->actortype < A_ENEMY)
            {
                if(!firstblood && !m_duel(gamemode, mutators) && ((v->actortype == A_PLAYER && m->actortype < A_ENEMY) || (v->actortype < A_ENEMY && m->actortype == A_PLAYER)))
                {
                    firstblood = true;
                    style |= FRAG_FIRSTBLOOD;
                    if(!m_dm_oldschool(gamemode, mutators)) pointvalue += G(firstbloodpoints);
                }
                if(flags&HIT_HEAD) // NOT HZONE
                {
                    style |= FRAG_HEADSHOT;
                    if(!m_dm_oldschool(gamemode, mutators)) pointvalue += G(headshotpoints);
                }
                if(m_play(gamemode) && m->actortype < A_ENEMY)
                {
                    int logs = 0;
                    v->spree++;
                    v->fraglog.add(m->clientnum);
                    if(G(multikilldelay))
                    {
                        logs = 0;
                        loopv(v->fragmillis)
                        {
                            if(gamemillis-v->fragmillis[i] > G(multikilldelay)) v->fragmillis.remove(i--);
                            else logs++;
                        }
                        if(!logs) v->rewards[0] &= ~FRAG_MULTI;
                        v->fragmillis.add(gamemillis);
                        logs++;
                        if(logs >= 2)
                        {
                            int offset = clamp(logs-2, 0, 2), type = 1<<(FRAG_MKILL+offset); // double, triple, multi..
                            if(!(v->rewards[0]&type))
                            {
                                style |= type;
                                v->rewards[0] |= type;
                                if(!m_dm_oldschool(gamemode, mutators)) pointvalue += (G(multikillbonus) ? offset+1 : 1)*G(multikillpoints);
                            }
                        }
                    }
                    loopj(FRAG_SPREES) if(m->rewards[1]&(1<<(FRAG_SPREE+j)))
                    {
                        style |= FRAG_BREAKER;
                        if(!m_dm_oldschool(gamemode, mutators)) pointvalue += G(spreebreaker);
                        break;
                    }
                    if(v->spree <= G(spreecount)*FRAG_SPREES && !(v->spree%G(spreecount)))
                    {
                        int offset = clamp((v->spree/G(spreecount)), 1, int(FRAG_SPREES))-1, type = 1<<(FRAG_SPREE+offset);
                        if(!(v->rewards[0]&type))
                        {
                            style |= type;
                            loopj(2) v->rewards[j] |= type;
                            if(!m_dm_oldschool(gamemode, mutators)) pointvalue += G(spreepoints);
                        }
                    }
                    logs = 0;
                    loopv(m->fraglog) if(m->fraglog[i] == v->clientnum) { logs++; m->fraglog.remove(i--); }
                    if(logs >= G(dominatecount))
                    {
                        style |= FRAG_REVENGE;
                        if(!m_dm_oldschool(gamemode, mutators)) pointvalue += G(revengepoints);
                    }
                    logs = 0;
                    loopv(v->fraglog) if(v->fraglog[i] == m->clientnum) logs++;
                    if(logs == G(dominatecount))
                    {
                        style |= FRAG_DOMINATE;
                        if(!m_dm_oldschool(gamemode, mutators)) pointvalue += G(dominatepoints);
                    }
                }
            }
            if(m->actortype < A_ENEMY && m_race(gamemode) && (!m_ra_gauntlet(gamemode, mutators) || m->team == T_ALPHA) && m->cpnodes.length() == 1)
            {  // reset if hasn't reached another checkpoint yet
                m->cpmillis = 0;
                m->cpnodes.shrink(0);
                sendf(-1, 1, "ri3", N_CHECKPOINT, m->clientnum, -1);
            }
            if(pointvalue)
            {
                if(v != m && v->actortype >= A_ENEMY && m->actortype < A_ENEMY)
                {
                    pointvalue = -pointvalue;
                    givepoints(m, pointvalue, m_points(gamemode, mutators) || m_dm_oldschool(gamemode, mutators), true);
                }
                else if(v->actortype < A_ENEMY) givepoints(v, pointvalue, m_points(gamemode, mutators) || m_dm_oldschool(gamemode, mutators), true);
            }
            m->deaths++;
            m->totaldeaths++;
            m->localtotaldeaths++;
            m->rewards[1] = 0;
            dropitems(m, DROP_DEATH);
            static vector<int> dmglog;
            dmglog.setsize(0);
            gethistory(m, v, gamemillis, dmglog, true, m_dm_oldschool(gamemode, mutators) ? 0 : 1);
            sendf(-1, 1, "ri9i5v", N_DIED, m->clientnum, m->deaths, m->totaldeaths, v->clientnum, v->frags, v->totalfrags, v->spree, style, weap, realflags, realdamage, material, dmglog.length(), dmglog.length(), dmglog.getbuf());
            m->position.setsize(0);
            if(smode) smode->died(m, v);
            mutate(smuts, mut->died(m, v));
            m->updatetimeplayed();
            m->state = CS_DEAD; // don't issue respawn yet until DEATHMILLIS has elapsed
            m->lastdeath = gamemillis;
            if(m->actortype == A_BOT) aiman::setskill(m);
            if(m != v && v->actortype == A_BOT) aiman::setskill(v);
            if(isteamkill && v->actortype == A_PLAYER) // don't punish the idiot bots
            {
                v->teamkills.add(teamkill(totalmillis, v->team, 0-pointvalue));
                if(G(teamkilllock) && !haspriv(v, G(teamkilllock)))
                {
                    int numkills = 0;
                    if(!G(teamkilltime)) numkills = v->teamkills.length();
                    else loopv(v->teamkills)
                        if(totalmillis-v->teamkills[i].millis <= G(teamkilltime)*1000*60) numkills++;
                    if(numkills >= G(teamkillwarn) && numkills%G(teamkillwarn) == 0)
                    {
                        uint ip = getclientip(v->clientnum);
                        v->warnings[WARN_TEAMKILL][0]++;
                        v->warnings[WARN_TEAMKILL][1] = totalmillis ? totalmillis : 1;
                        if(ip && G(teamkillban) && v->warnings[WARN_TEAMKILL][0] >= G(teamkillban) && !haspriv(v, PRIV_MODERATOR) && !checkipinfo(control, ipinfo::EXCEPT, ip))
                        {
                            ipinfo &c = control.add();
                            c.ip = ip;
                            c.mask = 0xFFFFFFFFU;
                            c.type = ipinfo::BAN;
                            c.flag = ipinfo::INTERNAL;
                            c.time = totalmillis ? totalmillis : 1;
                            c.reason = newstring("team killing is not permitted");
                            srvoutf(3, "\fs\fcbanned\fS %s: %s", colourname(v), c.reason);
                            updatecontrols = true;
                        }
                        else if(G(teamkillkick) && v->warnings[WARN_TEAMKILL][0] >= G(teamkillkick))
                        {
                            srvoutf(3, "\fs\fckicked\fS %s: team killing is not permitted", colourname(v));
                            v->kicked = updatecontrols = true;
                        }
                        else srvmsgft(v->clientnum, CON_MESG, "\fy\fs\fzoyWARNING:\fS team killing is not permitted, action will be taken if you continue");
                    }
                }
            }
        }
    }

    void suicideevent::process(clientinfo *ci)
    {
        if(ci->state != CS_ALIVE) return;
        if(flags&HIT_MATERIAL && (material&MATF_VOLUME) == MAT_LAVA) flags |= HIT_BURN;
        if(!(flags&HIT_MATERIAL) && !(flags&HIT_LOST) && !(flags&HIT_SPEC))
        {
            if(smode && !smode->damage(ci, ci, ci->health, -1, flags, material)) { return; }
            mutate(smuts, if(!mut->damage(ci, ci, ci->health, -1, flags, material)) { return; });
        }
        ci->spree = 0;
        ci->deaths++;
        ci->totaldeaths++;
        bool kamikaze = dropitems(ci, DROP_DEATH);
        if(ci->actortype < A_ENEMY && m_race(gamemode) && (!m_ra_gauntlet(gamemode, mutators) || ci->team == T_ALPHA) && !(flags&HIT_SPEC) && (!flags || ci->cpnodes.length() == 1 || !ci->checkpointspawn))
        { // reset if suicided, hasn't reached another checkpoint yet
            ci->cpmillis = 0;
            ci->cpnodes.shrink(0);
            sendf(-1, 1, "ri3", N_CHECKPOINT, ci->clientnum, -1);
        }
        else if(!(flags&HIT_LOST) && !(flags&HIT_SPEC))
        {
            int pointvalue = -1;
            if(!m_dm_oldschool(gamemode, mutators))
            {
                pointvalue = (smode ? smode->points(ci, ci) : -1)*G(fragbonus);
                if(kamikaze) pointvalue *= G(teamkillpenalty);
            }
            givepoints(ci, pointvalue, m_points(gamemode, mutators) || m_dm_oldschool(gamemode, mutators), true);
        }
        if(G(burntime) && flags&HIT_BURN)
        {
            ci->lastres[WR_BURN] = ci->lastrestime[WR_BURN] = gamemillis;
            ci->lastresowner[WR_BURN] = ci->clientnum;
        }
        if(G(bleedtime) && flags&HIT_BLEED)
        {
            ci->lastres[WR_BLEED] = ci->lastrestime[WR_BLEED] = gamemillis;
            ci->lastresowner[WR_BLEED] = ci->clientnum;
        }
        if(G(shocktime) && flags&HIT_SHOCK)
        {
            ci->lastres[WR_SHOCK] = ci->lastrestime[WR_SHOCK] = gamemillis;
            ci->lastresowner[WR_SHOCK] = ci->clientnum;
        }
        static vector<int> dmglog; dmglog.setsize(0);
        gethistory(ci, ci, gamemillis, dmglog, true, m_dm_oldschool(gamemode, mutators) ? 0 : 1, m_lasthit(gamemode, mutators) ? G(lasthitbonus) : 0);
        sendf(-1, 1, "ri9i5v", N_DIED, ci->clientnum, ci->deaths, ci->totaldeaths, ci->clientnum, ci->frags, ci->totalfrags, 0, 0, -1, flags, ci->health*2, material, dmglog.length(), dmglog.length(), dmglog.getbuf());
        ci->position.setsize(0);
        if(smode) smode->died(ci, NULL);
        mutate(smuts, mut->died(ci, NULL));
        ci->updatetimeplayed();
        ci->state = CS_DEAD;
        ci->lastdeath = gamemillis;
        if(ci->actortype == A_BOT) aiman::setskill(ci);
    }

    int calcdamage(clientinfo *v, clientinfo *m, int weap, int &flags, float radial, float size, float dist, float scale, bool self)
    {
        flags &= ~HIT_SFLAGS;
        if(!hitdealt(flags))
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
        else if(WF(WK(flags), weap, taper, WS(flags)) != 0)
            skew *= clamp(dist, WF(WK(flags), weap, tapermin, WS(flags)), WF(WK(flags), weap, tapermax, WS(flags)));

        if(!m_insta(gamemode, mutators))
        {
            if(m_capture(gamemode) && G(capturebuffdelay))
            {
                if(v->lastbuff) skew *= G(capturebuffdamage);
                if(m->lastbuff) skew /= G(capturebuffshield);
            }
            else if(m_defend(gamemode) && G(defendbuffdelay))
            {
                if(v->lastbuff) skew *= G(defendbuffdamage);
                if(m->lastbuff) skew /= G(defendbuffshield);
            }
            else if(m_bomber(gamemode) && G(bomberbuffdelay))
            {
                if(v->lastbuff) skew *= G(bomberbuffdamage);
                if(m->lastbuff) skew /= G(bomberbuffshield);
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
            if(!ci->weapshots[weap][WS(flags) ? 1 : 0].find(id))
            {
                srvmsgft(ci->clientnum, CON_DEBUG, "sync error: sticky [%d (%d)] failed - not found", weap, id);
                return;
            }
            clientinfo *m = target >= 0 ? (clientinfo *)getinfo(target) : NULL;
            if(target < 0 || (m && m->state == CS_ALIVE && !m->protect(gamemillis, m_protect(gamemode, mutators))))
                sendf(-1, 1, "ri9ix", N_STICKY, ci->clientnum, target, id, norm.x, norm.y, norm.z, pos.x, pos.y, pos.z, ci->clientnum);
            //else srvmsgft(ci->clientnum, CON_DEBUG, "sync error: sticky [%d (%d)] failed - state disallows it", weap, id);
        }
    }

    void destroyevent::process(clientinfo *ci)
    {
        if(weap == -1)
        {
            ci->dropped.remove(id);
            if(sents.inrange(id)) sents[id].millis = gamemillis;
        }
        else if(isweap(weap))
        {
            if(!ci->weapshots[weap][WS(flags) ? 1 : 0].find(id))
            {
                srvmsgft(ci->clientnum, CON_DEBUG, "sync error: destroy [%d:%d (%d)] failed - not found", weap, WS(flags) ? 1 : 0, id);
                return;
            }
            vector<clientinfo *> hitclients;
            if(hits.empty())
            {
                ci->weapshots[weap][WS(flags) ? 1 : 0].remove(id);
                if(id >= 0 && !m_insta(gamemode, mutators))
                {
                    int f = W2(weap, fragweap, WS(flags));
                    if(f >= 0)
                    {
                        int w = f%W_MAX, r = min(W2(weap, fragrays, WS(flags)), MAXPARAMS);
                        loopi(r) ci->weapshots[w][f >= W_MAX ? 1 : 0].add(-id);
                        if(WS(flags)) ci->weapstats[weap].flakshots2 += r;
                        else ci->weapstats[weap].flakshots1 += r;
                    }
                }
                sendf(-1, 1, "ri4x", N_DESTROY, ci->clientnum, 1, id, ci->clientnum);
            }
            else loopv(hits)
            {
                hitset &h = hits[i];
                clientinfo *m = (clientinfo *)getinfo(h.target);
                if(!m) continue;
                bool first = true;
                loopvj(hitclients) if(hitclients[j] == m) first = false;
                hitclients.add(m);
                if(h.proj)
                {
                    loopj(W_MAX) loopk(2) if(m->weapshots[j][k].find(h.proj))
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
                    if(m->state == CS_ALIVE && !m->protect(gamemillis, m_protect(gamemode, mutators)))
                    {
                        int damage = calcdamage(ci, m, weap, hflags, rad, size, dist, skew, ci == m);
                        if(damage) dodamage(m, ci, damage, weap, fromweap, fromflags, hflags, 0, h.dir, h.vel, dist, first);
                        //else srvmsgft(ci->clientnum, CON_DEBUG, "sync error: destroy [%d (%d)] failed - hit %d [%d] determined zero damage", weap, id, i, h.target);
                    }
                    //else srvmsgft(ci->clientnum, CON_DEBUG, "sync error: destroy [%d (%d)] failed - hit %d [%d] state disallows it", weap, id, i, h.target);
                }
            }
        }
    }

    void shotevent::process(clientinfo *ci)
    {
        if(!ci->isalive(gamemillis) || !isweap(weap))
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: shoot [%d] failed - unexpected message", weap);
            return;
        }
        int sub = W2(weap, ammosub, WS(flags));
        if(sub > 1 && W2(weap, cooktime, WS(flags)))
        {
            if(ci->ammo[weap] < sub)
            {
                int maxscale = int(ci->ammo[weap]/float(sub)*W2(weap, cooktime, WS(flags)));
                if(scale > maxscale) scale = maxscale;
            }
            sub = int(ceilf(sub*scale/float(W2(weap, cooktime, WS(flags)))));
        }
        if(!ci->canshoot(weap, flags, m_weapon(ci->actortype, gamemode, mutators), millis))
        {
            if(!ci->canshoot(weap, flags, m_weapon(ci->actortype, gamemode, mutators), millis, (1<<W_S_RELOAD)))
            {
                if(sub && W(weap, ammomax)) ci->ammo[weap] = max(ci->ammo[weap]-sub, 0);
                if(!ci->hasweap(weap, m_weapon(ci->actortype, gamemode, mutators))) ci->entid[weap] = -1; // its gone..
                srvmsgft(ci->clientnum, CON_DEBUG, "sync error: shoot [%d] failed - current state disallows it", weap);
                sendresume(ci, true);
                return;
            }
            if(ci->weapload[ci->weapselect] > 0)
            {
                takeammo(ci, ci->weapselect, ci->weapload[ci->weapselect]);
                ci->weapload[ci->weapselect] = -ci->weapload[ci->weapselect]; // the client should already do this for themself
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, ci->weapselect, ci->weapload[ci->weapselect], ci->ammo[ci->weapselect], ci->clientnum);
            }
        }
        takeammo(ci, weap, sub);
        ci->setweapstate(weap, WS(flags) ? W_S_SECONDARY : W_S_PRIMARY, W2(weap, delayattack, WS(flags)), millis);
        sendf(-1, 1, "ri8ivx", N_SHOTFX, ci->clientnum, weap, flags, scale, from.x, from.y, from.z, shots.length(), shots.length()*sizeof(shotmsg)/sizeof(int), shots.getbuf(), ci->clientnum);
        ci->weapshot[weap] = sub;
        ci->shotdamage += W2(weap, damage, WS(flags))*shots.length();
        loopv(shots) ci->weapshots[weap][WS(flags) ? 1 : 0].add(shots[i].id);
        if(WS(flags)) ci->weapstats[weap].shots2++;
        else ci->weapstats[weap].shots1++;
        if(ci->state != CS_ALIVE)
        {
            if(sents.inrange(ci->entid[weap])) loopv(ci->dropped.projs)
            {
                if(ci->dropped.projs[i].id != ci->entid[weap]) continue;
                ci->dropped.projs[i].ammo -= sub;
                if(ci->dropped.projs[i].ammo <= 0) setspawn(ci->entid[weap], false, true, true);
                break;
            }
        }
        else if(!ci->hasweap(weap, m_weapon(ci->actortype, gamemode, mutators)))
        {
            sendf(-1, 1, "ri7", N_DROP, ci->clientnum, -1, 1, weap, -1, 0);
            ci->ammo[weap] = ci->entid[weap] = -1; // its gone..
        }
    }

    void switchevent::process(clientinfo *ci)
    {
        if(!ci->isalive(gamemillis) || !isweap(weap))
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: switch [%d] failed - unexpected message", weap);
            sendf(ci->clientnum, 1, "ri3", N_WSELECT, ci->clientnum, ci->weapselect);
            return;
        }
        if(!ci->canswitch(weap, m_weapon(ci->actortype, gamemode, mutators), millis, (1<<W_S_SWITCH)))
        {
            if(!ci->canswitch(weap, m_weapon(ci->actortype, gamemode, mutators), millis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                srvmsgft(ci->clientnum, CON_DEBUG, "sync error: switch [%d] failed - current state disallows it", weap);
                sendresume(ci, true);
                return;
            }
            if(ci->weapload[ci->weapselect] > 0)
            {
                takeammo(ci, ci->weapselect, ci->weapload[ci->weapselect]);
                ci->weapload[ci->weapselect] = -ci->weapload[ci->weapselect]; // the client should already do this for themself
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, ci->weapselect, ci->weapload[ci->weapselect], ci->ammo[ci->weapselect], ci->clientnum);
            }
        }
        ci->updateweaptime();
        ci->weapswitch(weap, millis, G(weaponswitchdelay));
        sendf(-1, 1, "ri3x", N_WSELECT, ci->clientnum, weap, ci->clientnum);
    }

    void dropevent::process(clientinfo *ci)
    {
        if(!ci->isalive(gamemillis) || !isweap(weap))
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: drop [%d] failed - unexpected message", weap);
            return;
        }
        int sweap = m_weapon(ci->actortype, gamemode, mutators);
        if(!ci->candrop(weap, sweap, millis, m_loadout(gamemode, mutators), (1<<W_S_SWITCH)))
        {
            if(!ci->candrop(weap, sweap, millis, m_loadout(gamemode, mutators), (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                srvmsgft(ci->clientnum, CON_DEBUG, "sync error: drop [%d] failed - current state disallows it", weap);
                sendresume(ci, true);
                return;
            }
            if(ci->weapload[ci->weapselect] > 0)
            {
                takeammo(ci, ci->weapselect, ci->weapload[ci->weapselect]);
                ci->weapload[ci->weapselect] = -ci->weapload[ci->weapselect];
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, ci->weapselect, ci->weapload[ci->weapselect], ci->ammo[ci->weapselect], ci->clientnum);
            }
        }
        int dropped = -1, ammo = -1, nweap = ci->bestweap(sweap, true); // switch to best weapon
        if(sents.inrange(ci->entid[weap]))
        {
            dropped = ci->entid[weap];
            ammo = ci->ammo[weap] ? ci->ammo[weap] : W(weap, ammomax);
            setspawn(dropped, false);
            ci->dropped.add(dropped, ammo);
        }
        ci->ammo[weap] = ci->entid[weap] = -1;
        ci->weapswitch(nweap, millis, G(weaponswitchdelay));
        sendf(-1, 1, "ri7", N_DROP, ci->clientnum, nweap, 1, weap, dropped, ammo);
    }

    void reloadevent::process(clientinfo *ci)
    {
        if(!ci->isalive(gamemillis) || !isweap(weap))
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: reload [%d] failed - unexpected message", weap);
            return;
        }
        if(!ci->canreload(weap, m_weapon(ci->actortype, gamemode, mutators), false, millis))
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: reload [%d] failed - current state disallows it", weap);
            sendresume(ci, true);
            return;
        }
        ci->setweapstate(weap, W_S_RELOAD, W(weap, delayreload), millis);
        int oldammo = ci->ammo[weap];
        ci->ammo[weap] = min(max(ci->ammo[weap], 0) + W(weap, ammoadd), W(weap, ammomax));
        ci->weapload[weap] = ci->ammo[weap]-oldammo;
        sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, weap, ci->weapload[weap], ci->ammo[weap], ci->clientnum);
    }

    void useevent::process(clientinfo *ci)
    {
        if(ci->state != CS_ALIVE || !sents.inrange(ent) || sents[ent].type != WEAPON)
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: use [%d] failed - unexpected message", ent);
            return;
        }
        if(!finditem(ent))
        {
            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: use [%d] failed - doesn't seem to be spawned anywhere", ent);
            return;
        }
        ci->updateweaptime();
        int sweap = m_weapon(ci->actortype, gamemode, mutators), attr = w_attr(gamemode, mutators, sents[ent].type, sents[ent].attrs[0], sweap);
        if(!isweap(attr)) return;
        if(!ci->canuse(sents[ent].type, attr, sents[ent].attrs, sweap, millis, (1<<W_S_SWITCH)))
        {
            if(!ci->canuse(sents[ent].type, attr, sents[ent].attrs, sweap, millis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)))
            {
                srvmsgft(ci->clientnum, CON_DEBUG, "sync error: use [%d] failed - current state disallows it", ent);
                sendresume(ci, true);
                return;
            }
            if(ci->weapload[ci->weapselect] > 0)
            {
                takeammo(ci, ci->weapselect, ci->weapload[ci->weapselect]);
                ci->weapload[ci->weapselect] = -ci->weapload[ci->weapselect]; // the client should already do this for themself
                sendf(-1, 1, "ri5x", N_RELOAD, ci->clientnum, ci->weapselect, ci->weapload[ci->weapselect], ci->ammo[ci->weapselect], ci->clientnum);
            }
        }
        int weap = -1, ammoamt = -1, dropped = -1, ammo = -1;
        if(m_classic(gamemode, mutators) && !ci->hasweap(attr, sweap) && w_carry(attr, sweap) && ci->carry(sweap) >= AA(ci->actortype, maxcarry)) weap = ci->drop(sweap);
        loopvk(clients) if(clients[k]->dropped.find(ent))
        {
            clients[k]->dropped.values(ent, ammoamt);
            break;
        }
        if(isweap(weap))
        {
            if(sents.inrange(ci->entid[weap]))
            {
                dropped = ci->entid[weap];
                ammo = ci->ammo[weap];
                setspawn(dropped, false);
                ci->setweapstate(weap, W_S_SWITCH, G(weaponswitchdelay), millis);
                ci->dropped.add(dropped, ammo);
            }
            ci->ammo[weap] = ci->entid[weap] = -1;
        }
        setspawn(ent, false, true);
        ci->useitem(ent, sents[ent].type, attr, ammoamt, sweap, millis, G(weaponswitchdelay));
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
            if(team && m_swapteam(gamemode, mutators) && ci->team != team && ci->actortype == A_PLAYER && ci->swapteam != team && canplay())
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
        ci->updatetimeplayed();
        if(ci->state == CS_ALIVE)
        {
            if(drop) dropitems(ci, drop);
            if(smode) smode->died(ci);
            mutate(smuts, mut->died(ci));
            ci->lastdeath = gamemillis;
        }
        //else if(!ci->lastdeath) ci->lastdeath = gamemillis;
        if(exclude) sendf(-1, 1, "ri2x", N_WAITING, ci->clientnum, ci->clientnum);
        else sendf(-1, 1, "ri2", N_WAITING, ci->clientnum);
        ci->state = CS_WAITING;
        ci->weapreset(true);
        if(doteam && !allowteam(ci, ci->team, T_FIRST, false)) setteam(ci, chooseteam(ci), TT_INFO);
    }

    int triggertime(int i, bool delay = false)
    {
        if(sents.inrange(i)) switch(sents[i].type)
        {
            case TRIGGER: case MAPMODEL: case PARTICLES: case MAPSOUND: case TELEPORT: case PUSHER: return delay ? G(triggerdelay) : G(triggermillis); break;
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
                if(!checkmapvariant(sents[i].attrs[enttype[sents[i].type].mvattr])) continue;
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
            if(ci->state == CS_ALIVE)
            {
                // hurt material
                if((ci->inmaterial&MATF_FLAGS)&MAT_HURT)
                {
                    if(!ci->lasthurt || gamemillis-ci->lasthurt >= G(hurtdelay))
                    {
                        int flags = HIT_MATERIAL;
                        if(G(hurtresidual)&WR(BURN)) flags |= HIT_BURN;
                        if(G(hurtresidual)&WR(BLEED)) flags |= HIT_BLEED;
                        if(G(hurtresidual)&WR(SHOCK)) flags |= HIT_SHOCK;
                        dodamage(ci, ci, G(hurtdamage), -1, -1, HIT_NONE, flags, ci->inmaterial);
                        if(!ci->lasthurt) ci->lasthurt = gamemillis;
                        else ci->lasthurt += G(hurtdelay);
                        if(ci->state != CS_ALIVE) continue;
                    }
                }
                else if(ci->lasthurt && gamemillis-ci->lasthurt >= G(hurtdelay)) ci->lasthurt = 0;
                // burning residual
                if(ci->burning(gamemillis, G(burntime)))
                {
                    if(gamemillis-ci->lastrestime[WR_BURN] >= G(burndelay))
                    {
                        clientinfo *co = (clientinfo *)getinfo(ci->lastresowner[WR_BURN]);
                        dodamage(ci, co ? co : ci, G(burndamage), -1, -1, HIT_NONE, HIT_BURN, 0);
                        ci->lastrestime[WR_BURN] += G(burndelay);
                        if(ci->state != CS_ALIVE) continue;
                    }
                }
                else if(ci->lastres[WR_BURN]) ci->lastres[WR_BURN] = ci->lastrestime[WR_BURN] = 0;
                // bleeding residual
                if(ci->bleeding(gamemillis, G(bleedtime)))
                {
                    if(gamemillis-ci->lastrestime[WR_BLEED] >= G(bleeddelay))
                    {
                        clientinfo *co = (clientinfo *)getinfo(ci->lastresowner[WR_BLEED]);
                        dodamage(ci, co ? co : ci, G(bleeddamage), -1, -1, HIT_NONE, HIT_BLEED, 0);
                        ci->lastrestime[WR_BLEED] += G(bleeddelay);
                        if(ci->state != CS_ALIVE) continue;
                    }
                }
                else if(ci->lastres[WR_BLEED]) ci->lastres[WR_BLEED] = ci->lastrestime[WR_BLEED] = 0;
                // shocking residual
                if(ci->shocking(gamemillis, G(shocktime)))
                {
                    if(gamemillis-ci->lastrestime[WR_SHOCK] >= G(shockdelay))
                    {
                        clientinfo *co = (clientinfo *)getinfo(ci->lastresowner[WR_SHOCK]);
                        dodamage(ci, co ? co : ci, G(shockdamage), -1, -1, HIT_NONE, HIT_SHOCK, 0);
                        ci->lastrestime[WR_SHOCK] += G(shockdelay);
                        if(ci->state != CS_ALIVE) continue;
                    }
                }
                else if(ci->lastres[WR_SHOCK]) ci->lastres[WR_SHOCK] = ci->lastrestime[WR_SHOCK] = 0;
                // regen wear-off
                if(m_regen(gamemode, mutators) && AA(ci->actortype, abilities)&(1<<A_A_REGEN))
                {
                    int total = m_health(gamemode, mutators, ci->actortype), amt = G(regenhealth),
                        delay = ci->lastregen ? G(regentime) : G(regendelay);
                    if(smode) smode->regen(ci, total, amt, delay);
                    if(delay && ci->health != total)
                    {
                        int millis = gamemillis-(ci->lastregen ? ci->lastregen : ci->lastpain);
                        if(millis >= delay)
                        {
                            int low = 0;
                            if(ci->health > total)
                            {
                                amt = -G(regendecay);
                                total = ci->health;
                                low = m_health(gamemode, mutators, ci->actortype);
                            }
                            int heal = clamp(ci->health+amt, low, total), eff = heal-ci->health;
                            if(eff)
                            {
                                ci->health = heal;
                                ci->lastregen = gamemillis;
                                ci->lastregenamt = eff;
                                sendf(-1, 1, "ri4", N_REGEN, ci->clientnum, ci->health, ci->lastregenamt);
                            }
                        }
                    }
                }
            }
            else if(ci->state == CS_WAITING)
            {
                int nospawn = 0;
                if(smode && !smode->canspawn(ci, false)) { nospawn++; }
                mutate(smuts, if(!mut->canspawn(ci, false)) { nospawn++; });
                if(!nospawn)
                {
                    if(ci->lastdeath) flushevents(ci, ci->lastdeath + DEATHMILLIS);
                    cleartimedevents(ci);
                    ci->state = CS_DEAD; // safety
                    ci->respawn(gamemillis);
                    sendspawn(ci);
                }
            }
            if(G(autospectate) && ci->state == CS_DEAD && ci->lastdeath && gamemillis-ci->lastdeath >= G(autospecdelay))
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
                ci->authreq = ci->authname[0] = ci->handle[0] = '\0';
                srvmsgftforce(ci->clientnum, CON_EVENT, "\foUnable to verify, authority request timed out");
                int disc = auth::allowconnect(ci);
                if(disc) disconnect_client(ci->clientnum, disc);
                else
                {
                    ci->connectmillis = totalmillis ? totalmillis : 1; // in case it doesn't work
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
                    srvoutf(3, "Waited \fs\fc%s\fS to shutdown, overriding and exiting...", timestr(totalmillis-shutdownwait, 4));
#ifdef STANDALONE
                    cleanupserver();
                    exit(EXIT_SUCCESS);
#else
                    quit();
#endif
                    return;
                }
            }
            if(gs_waiting(gamestate))
            {
                int numwait = 0, numgetmap = 0, numnotready = 0;
                loopv(clients)
                {
                    clientinfo *cs = clients[i];
                    if(cs->actortype > A_PLAYER) continue;
                    if(m_play(gamemode) && (!cs->ready || (G(waitforplayers) == 2 && cs->state == CS_SPECTATOR))) numwait++;
                    if(cs->wantsmap || cs->gettingmap) numgetmap++;
                    if(!cs->ready) numnotready++;
                }
                switch(gamestate)
                {
                    case G_S_WAITING: // start check
                    {
                        if(!G(waitforplayermaps))
                        {
                            gamewaittime = totalmillis+G(waitforplayertime);
                            gamestate = G_S_READYING;
                            sendtick();
                            break;
                        }
                        if(!gamewaittime)
                        {
                            gamewaittime = totalmillis+max(m_play(gamemode) ? G(waitforplayerload) : 1, 1);
                            sendtick();
                        }
                        if(numnotready && gamewaittime > totalmillis) break;
                        if(!hasmapdata())
                        {
                            if(mapsending < 0) getmap(NULL, true);
                            if(mapsending >= 0)
                            {
                                srvoutf(4, "\fyPlease wait while the server downloads the map..");
                                gamewaittime = totalmillis+G(waitforplayermaps);
                                gamestate = G_S_GETMAP;
                                sendtick();
                                break;
                            }
                            gamewaittime = totalmillis+G(waitforplayertime);
                            gamestate = G_S_READYING;
                            sendtick();
                            break;
                        }
                        // fall through
                    }
                    case G_S_GETMAP: // waiting for server
                    {
                        if(!gamewaittime)
                        {
                            gamewaittime = totalmillis+G(waitforplayermaps);
                            sendtick();
                        }
                        if(!hasmapdata() && mapsending >= 0 && gamewaittime > totalmillis) break;
                        if(numgetmap && hasmapdata())
                        {
                            srvoutf(4, "\fyPlease wait for \fs\fc%d\fS %s to download the map..", numgetmap, numgetmap != 1 ? "players" : "player");
                            gamewaittime = totalmillis+G(waitforplayermaps);
                            gamestate = G_S_SENDMAP;
                            sendtick();
                            break;
                        }
                        gamewaittime = totalmillis+G(waitforplayertime);
                        gamestate = G_S_READYING;
                        sendtick();
                        break;
                    }
                    case G_S_SENDMAP: // waiting for players
                    {
                        if(!gamewaittime)
                        {
                            gamewaittime = totalmillis+G(waitforplayermaps);
                            sendtick();
                        }
                        if(numgetmap && gamewaittime > totalmillis && hasmapdata()) break;
                        gamewaittime = totalmillis+G(waitforplayertime);
                        gamestate = G_S_READYING;
                        sendtick();
                        // fall through
                    }
                    case G_S_READYING: // waiting for ready
                    {
                        if(!gamewaittime)
                        {
                            gamewaittime = totalmillis+G(waitforplayertime);
                            sendtick();
                        }
                        if(numwait && gamewaittime > totalmillis) break;
                        if(!hasgameinfo)
                        {
                            clientinfo *best = NULL;
                            loopv(clients)
                            {
                                clientinfo *cs = clients[i];
                                if(cs->actortype > A_PLAYER || !cs->name[0] || !cs->online || cs->wantsmap || !cs->ready) continue;
                                cs->updatetimeplayed();
                                if(!best || cs->timeplayed > best->timeplayed) best = cs;
                            }
                            if(best)
                            {
                                mapgameinfo = best->clientnum;
                                srvoutf(4, "\fyRequesting game information from %s..", colourname(best));
                                sendf(best->clientnum, 1, "ri", N_GETGAMEINFO);
                                gamewaittime = totalmillis+G(waitforplayerinfo);
                                gamestate = G_S_GAMEINFO;
                                sendtick();
                                break;
                            }
                        }
                        gamestate = G_S_PLAYING;
                        break;
                    }
                    case G_S_GAMEINFO:
                    {
                        if(!gamewaittime)
                        {
                            gamewaittime = totalmillis+G(waitforplayerinfo);
                            sendtick();
                        }
                        if(!hasgameinfo && gamewaittime > totalmillis) break;
                        if(hasgameinfo) srvoutf(4, "\fyGame information received, starting..");
                        else
                        {
                            if(mapgameinfo != -2)
                            {
                                int asked = 0;
                                mapgameinfo = -2;
                                loopv(clients)
                                {
                                    clientinfo *cs = clients[i];
                                    if(cs->actortype > A_PLAYER || !cs->name[0] || !cs->online || cs->wantsmap || !cs->ready || cs->clientnum == mapgameinfo) continue;
                                    sendf(cs->clientnum, 1, "ri", N_GETGAMEINFO);
                                    asked++;
                                }
                                if(!asked) srvoutf(4, "\fyNo game information response, and nobody to ask, giving up..");
                                else
                                {
                                    srvoutf(4, "\fyNo game information response, broadcasting..");
                                    gamewaittime = totalmillis+G(waitforplayerinfo);
                                    sendtick();
                                    break;
                                }
                            }
                            else srvoutf(4, "\fyNo broadcast game information response, giving up..");
                        }
                        mapgameinfo = -1;
                    }
                    default: gamestate = G_S_PLAYING; break;
                }
                if(gamestate == G_S_PLAYING)
                {
                    gamewaittime = 0;
                    if(m_team(gamemode, mutators)) doteambalance(true);
                    if(m_play(gamemode) && !m_bomber(gamemode) && !m_duke(gamemode, mutators)) // they do their own "fight"
                        sendf(-1, 1, "ri3s", N_ANNOUNCE, S_V_FIGHT, CON_EVENT, "Match start, fight!");
                    sendtick();
                }
            }
            if(canplay() && !paused) gamemillis += curtime;
            if(m_demo(gamemode)) readdemo();
            else if(canplay() && !paused)
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
        else
        {
            ifserver(shutdownwait)
            {
                srvoutf(4, "Server empty, shutting down as scheduled");
                #ifdef STANDALONE
                cleanupserver();
                exit(EXIT_SUCCESS);
                #else
                quit();
                #endif
                return;
            }
            if(G(rotatecycle) && clocktime-lastrotatecycle >= G(rotatecycle)*60) cleanup();
        }
        aiman::checkai();
        auth::update();
    }

    int lastquerysort = 0;
    bool querysort(const clientinfo *a, const clientinfo *b)
    {
        if(a->points > b->points) return true;
        if(a->points < b->points) return false;
        return strcmp(a->name, b->name) < 0;
    }
    vector<clientinfo *> queryplayers;

    int clientconnect(int n, uint ip, bool local)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        ci->connectmillis = totalmillis ? totalmillis : 1;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = local;
        connects.add(ci);
        conoutf("%s peer connection attempt from %s [%d]", ci->local ? "Local" : "Remote", gethostip(ci->clientnum), ci->clientnum);
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
        if(complete && ci->connected) sendstats();
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
                savestatsscore(ci);
                loopv(clients) if(clients[i] != ci)
                {
                    loopvk(clients[i]->fraglog) if(clients[i]->fraglog[k] == ci->clientnum)
                        clients[i]->fraglog.remove(k--);
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
                relayf(2, "\fo%s has left the game (%s, %d %s)", colourname(ci), reason >= 0 ? disc_reasons[reason] : "normal", amt, amt != 1 ? "players" : "player");
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
            else resetmapdata(true);
        }
        if(n == mapgameinfo) mapgameinfo = -1;
    }

    void queryreply(ucharbuf &req, ucharbuf &p)
    {
        if(!getint(req)) return;
        if(!lastquerysort || totalmillis-lastquerysort >= G(queryinterval))
        {
            queryplayers.setsize(0);
            loopv(clients) if(clients[i]->clientnum >= 0 && clients[i]->name[0] && clients[i]->actortype == A_PLAYER) queryplayers.add(clients[i]);
            queryplayers.sort(querysort);
            lastquerysort = totalmillis ? totalmillis : 1;
        }
        putint(p, queryplayers.length());
        putint(p, 15); // number of attrs following
        putint(p, VERSION_GAME); // 1
        putint(p, gamemode); // 2
        putint(p, mutators); // 3
        putint(p, timeremaining); // 4
        putint(p, maxslots()); // 5
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
        if(*G(serverdesc)) sendstring(limitstring(G(serverdesc), MAXSDESCLEN+1), p);
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
        sendstring(versionbranch, p);
        if(!queryplayers.empty())
        {
            loopv(queryplayers) sendstring(colourname(queryplayers[i]), p);
            loopv(queryplayers) sendstring(queryplayers[i]->handle, p);
        }
        sendqueryreply(p);
    }

    int receivefile(int sender, uchar *data, int len)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        ucharbuf p(data, len);
        int type = getint(p), n = getint(p), crc = getint(p);
        data += p.length();
        len -= p.length();
        if(type != N_SENDMAPFILE) return -1;
        if(n < 0 || n >= SENDMAP_MAX) return -1;
        if(ci->clientnum != mapsending) return -1;
        if(mapdata[n]) DELETEP(mapdata[n]);
        if(!len) return n; // zero len is no file
        defformatstring(fname, "backups/tempfile.%s", sendmaptypes[n]);
        mapdata[n] = opentempfile(fname, "w+b");
        if(!mapdata[n])
        {
            srvmsgf(-1, "Failed to open temporary file for map");
            return n;
        }
        mapdata[n]->write(data, len);
        if(n == SENDMAP_MPZ)
        {
            smapcrc = crcstream(mapdata[n]);
            if(crc != smapcrc)
            {
                if(m_edit(gamemode)) ci->clientcrc = smapcrc;
                else srvmsgf(-1, "Warning: new crc 0x%.8x doesn't match client 0x%.8x [0x%.8x]", smapcrc, crc, ci->clientcrc);
            }
        }
        return n;
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
    } msgfilter(-1, N_CONNECT, N_SERVERINIT, N_CLIENTINIT, N_WELCOME, N_MAPCHANGE, N_SERVMSG, N_DAMAGE, N_SHOTFX, N_LOADW, N_DIED, N_POINTS, N_SPAWNSTATE, N_ITEMACC, N_ITEMSPAWN, N_TICK, N_DISCONNECT, N_CURRENTPRIV, N_PONG, N_RESUME, N_SCOREAFFIN, N_SCORE, N_ANNOUNCE, N_SENDDEMOLIST, N_SENDDEMO, N_DEMOPLAYBACK, N_SENDMAP, N_REGEN, N_CLIENT, N_AUTHCHAL, N_QUEUEPOS, -2, N_REMIP, N_NEWMAP, N_CLIPBOARD, -3, N_EDITENT, N_EDITLINK, N_EDITVAR, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_EDITVSLOT, N_UNDO, N_REDO, -4, N_POS, N_SPAWN, N_DESTROY, NUMMSG),
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
            case 2: if(m_edit(gamemode) && ci && ci->state == CS_EDITING) break; return -1;
            // only allowed in coop-edit, no overflow check
            case 3: return m_edit(gamemode) && ci && ci->state == CS_EDITING ? type : -1;
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
            if(ci.actortype != A_PLAYER) continue;
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
            if(ci.actortype != A_PLAYER) continue;
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
            if(ci.actortype != A_PLAYER) continue;
            addposition(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addposition(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendpositions(ws, wsbuf);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.actortype != A_PLAYER) continue;
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
            if(e.clientnum != ci->clientnum && e.needclipboard < ci->lastclipboard)
            {
                if(!flushed) { flushserver(true); flushed = true; }
                sendpacket(e.clientnum, 1, ci->clipboard);
                e.needclipboard = ci->lastclipboard;
            }
        }
    }

    void connected(clientinfo *ci)
    {
        if(!m_demo(gamemode) && !numclients() && demonextmatch) setupdemorecord();

        connects.removeobj(ci);
        clients.add(ci);

        ci->connected = true;
        ci->needclipboard = 0;
        ci->lasttimeplayed = totalmillis ? totalmillis : 1;
        ci->lasttimealive = totalmillis ? totalmillis : 1;
        ci->lasttimeactive = totalmillis ? totalmillis : 1;
        ci->lasttimewielded = totalmillis ? totalmillis : 1;
        loopi(W_MAX) ci->lasttimeloadout[i] = totalmillis ? totalmillis : 1;

        if(ci->handle[0]) // kick old logins
        {
            loopvrev(clients) if(clients[i] != ci && clients[i]->handle[0] && !strcmp(clients[i]->handle, ci->handle))
                disconnect_client(clients[i]->clientnum, DISC_AUTH);
        }
        sendwelcome(ci);
        if(restorescore(ci)) sendresume(ci);
        sendinitclient(ci);
        int amt = numclients();
        if((ci->privilege&PRIV_TYPE) > PRIV_NONE)
        {
            if(ci->handle[0]) relayf(2, "\fg%s has joined the game (\fs\fy%s\fS: \fs\fc%s\fS) [%d.%d.%d-%s%d-%s] (%d %s)", colourname(ci), privname(ci->privilege), ci->handle, ci->version.major, ci->version.minor, ci->version.patch, plat_name(ci->version.platform), ci->version.arch, ci->version.branch, amt, amt != 1 ? "players" : "player");
            else relayf(2, "\fg%s has joined the game (\fs\fy%s\fS) [%d.%d.%d-%s%d-%s] (%d %s)", colourname(ci), privname(ci->privilege), ci->version.major, ci->version.minor, ci->version.patch, plat_name(ci->version.platform), ci->version.arch, ci->version.branch, amt, amt != 1 ? "players" : "player");
        }
        else relayf(2, "\fg%s has joined the game [%d.%d.%d-%s%d-%s] (%d %s)", colourname(ci), ci->version.major, ci->version.minor, ci->version.patch, plat_name(ci->version.platform), ci->version.arch, ci->version.branch, amt, amt != 1 ? "players" : "player");

        if(m_demo(gamemode)) setupdemoplayback();
        else if(m_edit(gamemode))
        {
            ci->ready = true;
            aiman::poke();
        }
    }

    void parsepacket(int sender, int chan, packetbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender < 0 || p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED || chan > 2) return;
        char text[MAXTRANS];
        int type = -1, prevtype = -1;
        clientinfo *ci = sender >= 0 ? (clientinfo *)getinfo(sender) : NULL;
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
                        stringz(namestr);
                        filterstring(namestr, text, true, true, true, true, MAXNAMELEN);
                        if(!*namestr) copystring(namestr, "unnamed");
                        copystring(ci->name, namestr, MAXNAMELEN+1);
                        ci->colour = max(getint(p), 0);
                        ci->model = max(getint(p), 0);
                        ci->pattern = max(getint(p), 0);
                        getstring(text, p);
                        ci->setvanity(text);
                        int lw = getint(p);
                        ci->loadweap.shrink(0);
                        loopk(lw)
                        {
                            if(k >= W_LOADOUT) getint(p);
                            else ci->loadweap.add(getint(p));
                        }
                        int rw = getint(p);
                        ci->randweap.shrink(0);
                        loopk(rw)
                        {
                            if(k >= W_LOADOUT) getint(p);
                            else ci->randweap.add(getint(p));
                        }

                        stringz(password);
                        stringz(authname);
                        getstring(password, p);
                        getstring(text, p);
                        filterstring(authname, text, true, true, true, true, 100);

                        ci->version.get(p);

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
            int ret = receivefile(sender, p.buf, p.maxlen);
            if(ret == SENDMAP_ALL)
            {
                clientinfo *cs = (clientinfo *)getinfo(mapsending);
                if(hasmapdata())
                {
                    if(cs && !hasgameinfo && !gs_waiting(gamestate)) sendf(cs->clientnum, 1, "ri", N_GETGAMEINFO);
                    mapsending = -1;
                    sendf(-1, 1, "ri", N_SENDMAP);
                    loopv(clients)
                    {
                        clientinfo *cs = clients[i];
                        if(cs->actortype > A_PLAYER || !cs->online || !cs->name[0] || !cs->ready) continue;
                        if(cs->wantsmap || crclocked(cs, true)) getmap(cs);
                    }
                }
                else
                {
                    if(cs) cs->wantsmap = true;
                    resetmapdata(true);
                }
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
                    getuint(p);
                    getuint(p);
                    uint flags = getuint(p);
                    vec pos, floorpos, vel, falling;
                    float yaw, pitch, roll;
                    loopk(3)
                    {
                        int n = p.get();
                        n |= p.get()<<8;
                        if(flags&(1<<k))
                        {
                            n |= p.get()<<16;
                            if(n&0x800000) n |= ~0U<<24;
                        }
                        pos[k] = n/DMF;
                    }
                    loopk(3)
                    {
                        int n = p.get();
                        n |= p.get()<<8;
                        if(flags&(1<<(k+3)))
                        {
                            n |= p.get()<<16;
                            if(n&0x800000) n |= ~0U<<24;
                        }
                        floorpos[k] = n/DMF;
                    }
                    int dir = p.get();
                    dir |= p.get()<<8;
                    yaw = dir%360;
                    pitch = clamp(dir/360, 0, 180)-90;
                    roll = clamp(int(p.get()), 0, 180)-90;
                    int mag = p.get();
                    if(flags&(1<<6)) mag |= p.get()<<8;
                    dir = p.get();
                    dir |= p.get()<<8;
                    vecfromyawpitch(dir%360, clamp(dir/360, 0, 180)-90, 1, 0, vel);
                    vel.mul(mag/DVELF);
                    if(flags&(1<<7))
                    {
                        mag = p.get();
                        if(flags&(1<<8)) mag |= p.get()<<8;
                        if(flags&(1<<9))
                        {
                            dir = p.get();
                            dir |= p.get()<<8;
                            vecfromyawpitch(dir%360, clamp(dir/360, 0, 180)-90, 1, 0, falling);
                        }
                        else falling = vec(0, 0, -1);
                        falling.mul(mag/DVELF);
                    }
                    else falling = vec(0, 0, 0);
                    if(havecn)
                    {
                        vec oldpos = cp->o;
                        cp->o = pos;
                        cp->floorpos = floorpos;
                        cp->vel = vel;
                        cp->falling = falling;
                        cp->yaw = yaw;
                        cp->pitch = pitch;
                        cp->roll = roll;
                        if((!ci->local || demorecord || hasnonlocalclients()) && (cp->state==CS_ALIVE || cp->state==CS_EDITING))
                        {
                            cp->position.setsize(0);
                            while(curmsg<p.length()) cp->position.add(p.buf[curmsg++]);
                        }
                        if(cp->state==CS_ALIVE)
                        {
                            if(smode) smode->moved(cp, oldpos, cp->o);
                            mutate(smuts, mut->moved(cp, oldpos, cp->o));
                        }
                    }
                    break;
                }

                case N_SPHY:
                {
                    int lcn = getint(p), idx = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    bool proceed = hasclient(cp, ci), qmsg = false;
                    switch(idx)
                    {
                        case SPHY_BOOST: case SPHY_DASH: case SPHY_MELEE: case SPHY_KICK: case SPHY_VAULT: case SPHY_GRAB: case SPHY_SKATE:
                        {
                            if(!proceed || cp->state != CS_ALIVE) break;
                            qmsg = true;
                            break;
                        }
                        case SPHY_COOK:
                        {
                            int wstate = getint(p), wlen = getint(p), wtime = getint(p);
                            if(!proceed) break;
                            if(!cp->isalive(gamemillis) || !isweap(cp->weapselect) || (wstate != W_S_IDLE && wstate != W_S_ZOOM && wstate != W_S_POWER))
                            {
                                srvmsgft(cp->clientnum, CON_DEBUG, "sync error: power [%d] failed - unexpected message", cp->weapselect);
                                break;
                            }
                            if(cp->weapstate[cp->weapselect] == W_S_RELOAD && !cp->weapwaited(cp->weapselect, gamemillis))
                            {
                                if(!cp->weapwaited(cp->weapselect, gamemillis, (1<<W_S_RELOAD)))
                                {
                                    if(!cp->hasweap(cp->weapselect, m_weapon(cp->actortype, gamemode, mutators))) cp->entid[cp->weapselect] = -1; // its gone..
                                    srvmsgft(cp->clientnum, CON_DEBUG, "sync error: power [%d] failed - current state disallows it", cp->weapselect);
                                    sendresume(ci, true);
                                    break;
                                }
                                else if(cp->weapload[cp->weapselect] > 0)
                                {
                                    takeammo(cp, cp->weapselect, cp->weapload[cp->weapselect]);
                                    cp->weapload[cp->weapselect] = -cp->weapload[cp->weapselect];
                                    sendf(-1, 1, "ri5x", N_RELOAD, cp->clientnum, cp->weapselect, cp->weapload[cp->weapselect], cp->ammo[cp->weapselect], cp->clientnum);
                                }
                                else break;
                            }
                            cp->setweapstate(cp->weapselect, wstate, wlen, lastmillis, wtime, wstate == W_S_IDLE);
                            qmsg = true;
                            break;
                        }
                        case SPHY_MATERIAL:
                        {
                            int inmaterial = getint(p);
                            float submerged = getfloat(p);
                            if(!proceed) break;
                            int oldmaterial = cp->inmaterial;
                            cp->inmaterial = inmaterial;
                            cp->submerged = submerged;
                            if(cp->state == CS_ALIVE && (cp->inmaterial&MATF_FLAGS)&MAT_DEATH && !((oldmaterial&MATF_FLAGS)&MAT_DEATH))
                            {
                                suicideevent ev;
                                ev.flags = HIT_MATERIAL;
                                ev.material = cp->inmaterial;
                                ev.process(cp); // process death immediately
                            }
                            else if((cp->inmaterial&MATF_VOLUME) == MAT_WATER && cp->burning(gamemillis, G(burntime)) && cp->submerged >= G(liquidextinguish))
                            {
                                cp->lastres[WR_BURN] = cp->lastrestime[WR_BURN] = 0;
                                sendf(-1, 1, "ri3", N_SPHY, cp->clientnum, SPHY_EXTINGUISH);
                            }
                            break; // does not get sent to clients
                        }
                        default: break;
                    }
                    if(qmsg) QUEUE_MSG;
                    break;
                }

                case N_EDITMODE:
                {
                    int val = getint(p);
                    if(!ci || ci->actortype > A_PLAYER) break;
                    if(!allowstate(ci, val ? ALST_EDIT : ALST_WALK, G(editlock)))
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, "sync error: unable to switch state %s - %d [%d, %d]", colourname(ci), ci->state, ci->lastdeath, gamemillis);
                        spectator(ci);
                        break;
                    }
                    ci->editspawn(gamemode, mutators);
                    if(val)
                    {
                        if(smode) smode->leavegame(ci);
                        mutate(smuts, mut->leavegame(ci));
                        ci->state = CS_EDITING;
                        ci->events.deletecontents();
                    }
                    else
                    {
                        ci->state = CS_ALIVE;
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
                    ci->clientcrc = crc;
                    ci->ready = true;
                    ci->wantsmap = ci->gettingmap = false;
                    if(!m_edit(gamemode))
                    {
                        if(hasmapdata()) srvoutf(4, "\fy%s has map crc: \fs\fc0x%.8x\fS (server: \fs\fc0x%.8x\fS)", colourname(ci), ci->clientcrc, smapcrc);
                        else srvoutf(4, "\fy%s has map crc: \fs\fc0x%.8x\fS", colourname(ci), ci->clientcrc);
                    }
                    if(crclocked(ci, true)) getmap(ci);
                    if(ci->isready()) aiman::poke();
                    break;
                }

                case N_TRYSPAWN:
                {
                    int lcn = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    if(!allowstate(cp, ALST_TRY, m_edit(gamemode) ? G(spawneditlock) : G(spawnlock)))
                    {
                        srvmsgft(cp->clientnum, CON_DEBUG, "sync error: unable to spawn %s - %d [%d, %d]", colourname(cp), cp->state, cp->lastdeath, gamemillis);
                        //spectator(cp);
                        break;
                    }
                    int nospawn = 0;
                    if(smode && !smode->canspawn(cp, true)) { nospawn++; }
                    mutate(smuts, if(!mut->canspawn(cp, true)) { nospawn++; });
                    if(!nospawn)
                    {
                        cp->state = CS_DEAD;
                        waiting(cp, DROP_RESET);
                    }
                    break;
                }

                case N_WSELECT:
                {
                    int lcn = getint(p), id = getint(p), weap = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci) || !isweap(weap) || weap >= W_ALL) break;
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
                        srvmsgft(cp->clientnum, CON_DEBUG, "sync error: unable to spawn %s - %d [%d, %d]", colourname(cp), cp->state, cp->lastdeath, gamemillis);
                        //spectator(cp);
                        break;
                    }
                    cp->updatetimeplayed();
                    cp->state = CS_ALIVE;
                    if(smode) smode->spawned(cp);
                    mutate(smuts, mut->spawned(cp););
                    QUEUE_BUF({
                        putint(ci->messages, N_SPAWN);
                        putint(ci->messages, cp->clientnum);
                        sendstate(cp, ci->messages);
                    });
                    break;
                }

                case N_SUICIDE:
                {
                    int lcn = getint(p), flags = getint(p), material = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci)) break;
                    suicideevent ev;
                    ev.flags = flags;
                    cp->inmaterial = ev.material = material;
                    ev.process(cp); // process death immediately
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
                        cp->lastshoot = gamemillis;
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
                    ev->fromweap = getint(p);
                    ev->fromflags = getint(p);
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
                    if(!hasclient(cp, ci) || cp->state != CS_ALIVE) break;
                    if(sents.inrange(ent))
                    {
                        if(sents[ent].type == CHECKPOINT)
                        {
                            if(sents[ent].attrs[5] && sents[ent].attrs[5] != triggerid) break;
                            if(!checkmapvariant(sents[ent].attrs[enttype[sents[ent].type].mvattr])) break;
                            if(!m_check(sents[ent].attrs[3], sents[ent].attrs[4], gamemode, mutators)) break;
                            if(!m_race(gamemode) || (m_ra_gauntlet(gamemode, mutators) && cp->team != T_ALPHA)) break;
                            if(cp->cpnodes.find(ent) >= 0) break;
                            switch(sents[ent].attrs[6])
                            {
                                case CP_LAST: case CP_FINISH:
                                {
                                    if(cp->cpmillis)
                                    {
                                        int laptime = gamemillis-cp->cpmillis, total = 0;
                                        if(cp->cptime <= 0 || laptime < cp->cptime) cp->cptime = laptime;
                                        cp->points++;
                                        sendf(-1, 1, "ri6", N_CHECKPOINT, cp->clientnum, ent, laptime, cp->cptime, cp->points);
                                        if(m_team(gamemode, mutators))
                                        {
                                            if(m_laptime(gamemode, mutators))
                                            {
                                                score &ts = teamscore(cp->team);
                                                if(!ts.total || ts.total > cp->cptime)
                                                {
                                                    total = ts.total = cp->cptime;
                                                    sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                                                }
                                            }
                                            else
                                            {
                                                score &ts = teamscore(cp->team);
                                                total = ++ts.total;
                                                sendf(-1, 1, "ri3", N_SCORE, ts.team, ts.total);
                                            }
                                            if(total && m_ra_gauntlet(gamemode, mutators) && G(racegauntletwinner))
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
                                                        ancmsgft(-1, S_V_NOTIFY, CON_EVENT, "\fyBest score has been reached");
                                                        startintermission();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else waiting(cp);
                                    cp->cpmillis = 0;
                                    cp->cpnodes.shrink(0);
                                    if(sents[ent].attrs[6] == CP_FINISH) waiting(cp);
                                    break;
                                }
                                case CP_START: case CP_RESPAWN:
                                {
                                    if(cp->cpnodes.find(ent) >= 0) break;
                                    if(sents[ent].attrs[6] == CP_START)
                                    {
                                        if(cp->cpmillis) break;
                                        cp->cpmillis = gamemillis;
                                    }
                                    else if(!cp->cpmillis)
                                    {
                                        waiting(cp);
                                        break;
                                    }
                                    sendf(-1, 1, "ri4", N_CHECKPOINT, cp->clientnum, ent, -1);
                                    cp->cpnodes.add(ent);
                                }
                                default: break;
                            }
                        }
                        else if(sents[ent].type == TRIGGER)
                        {
                            if(!checkmapvariant(sents[ent].attrs[enttype[sents[ent].type].mvattr])) break;
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
                                if(sents[sents[ent].kin[j]].type == TRIGGER && !checkmapvariant(sents[sents[ent].kin[j]].attrs[enttype[sents[sents[ent].kin[j]].type].mvattr]) && !m_check(sents[sents[ent].kin[j]].attrs[5], sents[sents[ent].kin[j]].attrs[6], gamemode, mutators))
                                    continue;
                                sents[sents[ent].kin[j]].spawned = sents[ent].spawned;
                                sents[sents[ent].kin[j]].millis = sents[ent].millis;
                            }
                        }
                    }
                    else srvmsgft(cp->clientnum, CON_DEBUG, "sync error: cannot trigger %d - entity does not exist (max: %d)", ent, sents.length());
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
                        loopvrev(fcp->chatmillis)
                        {
                            if(totalmillis-fcp->chatmillis[i] <= G(floodtime)) numlines++;
                            else fcp->chatmillis.remove(i);
                        }
                        if(numlines >= G(floodlines))
                        {
                            if((!fcp->warnings[WARN_CHAT][1] || totalmillis-fcp->warnings[WARN_CHAT][1] >= 1000) && !haspriv(fcp, G(floodlock), "send too many messages consecutively"))
                            {
                                fcp->warnings[WARN_CHAT][0]++;
                                fcp->warnings[WARN_CHAT][1] = totalmillis ? totalmillis : 1;
                                if(ip && G(floodmute) && fcp->warnings[WARN_CHAT][0] >= G(floodmute) && !checkipinfo(control, ipinfo::EXCEPT, ip) && !haspriv(fcp, G(mutelock)))
                                {
                                    ipinfo &c = control.add();
                                    c.ip = ip;
                                    c.mask = 0xFFFFFFFFU;
                                    c.type = ipinfo::MUTE;
                                    c.flag = ipinfo::INTERNAL;
                                    c.time = totalmillis ? totalmillis : 1;
                                    c.reason = newstring("exceeded the number of allowed flood warnings");
                                    srvoutf(3, "\fs\fcmute\fS added on %s: %s", colourname(fcp), c.reason);
                                }
                            }
                            break;
                        }
                        fcp->chatmillis.add(totalmillis ? totalmillis : 1);
                    }
                    bigstring output;
                    copystring(output, text, G(messagelength));
                    filterstring(text, text, true, true, true, true, G(messagelength));
                    if(*(G(censorwords))) filterword(output, G(censorwords));
                    if(flags&SAY_TEAM && !m_team(gamemode, mutators)) flags &= ~SAY_TEAM;
                    sendf(-1, -1, "ri4s", N_TEXT, fcp->clientnum, tcp ? tcp->clientnum : -1, flags, output); // sent to negative chan for recordpacket
                    if(flags&SAY_WHISPER && tcp)
                    {
                        int scn = allowbroadcast(tcp->clientnum) ? tcp->clientnum : tcp->ownernum;
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
                            if(!allowbroadcast(scn) && t->ownernum >= 0)
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
                                scn = t->ownernum;
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

                case N_SETPLAYERINFO: // name colour model checkpoint vanity count <loadweaps> count <randweaps>
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
                            loopk(3) getint(p);
                            getstring(text, p);
                            int lw = getint(p);
                            loopk(lw) getint(p);
                            int rw = getint(p);
                            loopk(rw) getint(p);
                            sendinitclientself(ci);
                            break;
                        }
                    }
                    QUEUE_MSG;
                    defformatstring(oldname, "%s", colourname(ci));
                    getstring(text, p);
                    stringz(namestr);
                    filterstring(namestr, text, true, true, true, true, MAXNAMELEN);
                    if(!*namestr) copystring(namestr, "unnamed");
                    if(strcmp(ci->name, namestr))
                    {
                        copystring(ci->name, namestr, MAXNAMELEN+1);
                        relayf(2, "\fm* %s is now known as %s", oldname, colourname(ci));
                    }
                    ci->colour = max(getint(p), 0);
                    ci->model = max(getint(p), 0);
                    ci->pattern = max(getint(p), 0);
                    ci->checkpointspawn = max(getint(p), 0);
                    getstring(text, p);
                    ci->setvanity(text);
                    ci->loadweap.shrink(0);
                    int lw = getint(p);
                    vector<int> lweaps;
                    loopk(lw)
                    {
                        if(k >= W_LOADOUT) getint(p);
                        else ci->loadweap.add(getint(p));
                    }
                    ci->randweap.shrink(0);
                    int rw = getint(p);
                    loopk(rw)
                    {
                        if(k >= W_LOADOUT) getint(p);
                        else ci->randweap.add(getint(p));
                    }
                    ci->lastplayerinfo = totalmillis ? totalmillis : 1;
                    QUEUE_STR(ci->name);
                    QUEUE_INT(ci->colour);
                    QUEUE_INT(ci->model);
                    QUEUE_INT(ci->checkpointspawn);
                    QUEUE_STR(ci->vanity);
                    QUEUE_INT(ci->loadweap.length());
                    loopvk(ci->loadweap) QUEUE_INT(ci->loadweap[k]);
                    QUEUE_INT(ci->randweap.length());
                    loopvk(ci->randweap) QUEUE_INT(ci->randweap[k]);
                    break;
                }

                case N_SWITCHTEAM:
                {
                    int team = getint(p);
                    if(!m_team(gamemode, mutators) || ci->actortype >= A_ENEMY || !isteam(gamemode, mutators, team, T_FIRST)) break;
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
                    if(ci->state == CS_SPECTATOR)
                    {
                        if(!allowstate(ci, ALST_TRY, m_edit(gamemode) ? G(spawneditlock) : G(spawnlock)))
                        {
                            srvmsgft(ci->clientnum, CON_DEBUG, "sync error: unable to spawn %s - %d [%d, %d]", colourname(ci), ci->state, ci->lastdeath, gamemillis);
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
                        ci->mapvote[0] = '\0';
                        ci->modevote = ci->mutsvote = -1;
                        sendf(-1, 1, "ri2", N_CLEARVOTE, ci->clientnum);
                    }
                    break;
                }

                case N_GAMEINFO:
                {
                    bool skip = hasgameinfo || (mapgameinfo == -2 ? crclocked(ci) : mapgameinfo != sender);
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
                                    if(id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU)
                                    {
                                         if(uint(ret) < uint(id->minval) || uint(ret) > uint(id->maxval)) break;
                                    }
                                    else if(ret < id->minval || ret > id->maxval) break;
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
                    if(!hasclient(cp, ci) || cp->state == CS_SPECTATOR) break;
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
                    int lcn = getint(p), otc = getint(p);
                    vec droploc, inertia;
                    loopk(3) droploc[k] = getint(p)/DMF;
                    loopk(3) inertia[k] = getint(p)/DMF;
                    clientinfo *cp = (clientinfo *)getinfo(lcn);
                    if(!hasclient(cp, ci) || cp->state == CS_SPECTATOR) break;
                    if(smode==&capturemode) capturemode.dropaffinity(cp, droploc, inertia, -1);
                    else if(smode==&bombermode) bombermode.dropaffinity(cp, droploc, inertia, otc);
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
                        loopv(clients) if(clients[i]->ownernum == ci->clientnum) clients[i]->ping = ping;
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
                            if(mastermode >= MM_PRIVATE) loopv(clients)
                            {
                                ipinfo &allow = control.add();
                                allow.ip = getclientip(clients[i]->clientnum);
                                allow.mask = 0xFFFFFFFFU;
                                allow.type = ipinfo::ALLOW;
                                allow.time = totalmillis ? totalmillis : 1;
                                allow.reason = newstring("mastermode set private");
                            }
                            sendf(-1, 1, "i3", N_MASTERMODE, ci->clientnum, mastermode);
                            //srvoutf(3, "\fyMastermode is now \fs\fc%d\fS (\fs\fc%s\fS)", mastermode, mastermodename(mastermode));
                        }
                        else srvmsgft(ci->clientnum, CON_EVENT, "\foThe \fs\fcmastermode\fS of \fs\fc%d\fS (\fs\fc%s\fS) is disabled on this server", mm, mastermodename(mm));
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
                                srvoutf(3, "%s cleared existing \fs\fc" #y "s\fS", colourname(ci)); \
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
                                if(!cp || cp->ownernum >= 0 || (value != ipinfo::EXCEPT && !cmppriv(ci, cp, #y))) break; \
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
                                    c.mask = 0xFFFFFFFFU; \
                                    c.type = value; \
                                    c.time = totalmillis ? totalmillis : 1; \
                                    c.reason = newstring(text); \
                                    if(text[0]) srvoutf(3, "%s added \fs\fc" #y "\fS on %s: %s", name, colourname(cp), text); \
                                    else srvoutf(3, "%s added \fs\fc" #y "\fS on %s", name, colourname(cp)); \
                                    if(value == ipinfo::BAN) updatecontrols = true; \
                                    else if(value == ipinfo::LIMIT) cp->swapteam = 0; \
                                } \
                                else \
                                { \
                                    if(text[0]) srvoutf(3, "%s \fs\fckicked\fS %s: %s", name, colourname(cp), text); \
                                    else srvoutf(3, "%s \fs\fckicked\fS %s", name, colourname(cp)); \
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
                    if(!cp || cp->actortype > A_PLAYER || (val ? cp->state == CS_SPECTATOR : cp->state != CS_SPECTATOR))
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, "sync error: unable to modify spectator %s - %d [%d, %d] - invalid", colourname(cp), cp->state, cp->lastdeath, gamemillis);
                        break;
                    }
                    if(sn != sender ? !haspriv(ci, max(m_edit(gamemode) ? G(spawneditlock) : G(spawnlock), G(speclock)), "control other players") : (!haspriv(ci, max(m_edit(gamemode) ? G(spawneditlock) : G(spawnlock), G(speclock))) && !allowstate(cp, val ? ALST_SPEC : ALST_TRY, m_edit(gamemode) ? G(spawneditlock) : G(spawnlock))))
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, "sync error: unable to modify spectator %s - %d [%d, %d] - restricted", colourname(cp), cp->state, cp->lastdeath, gamemillis);
                        break;
                    }
                    bool spec = val != 0, quarantine = cp != ci && val == 2, wasq = cp->quarantine;
                    if(quarantine && (ci->privilege&PRIV_TYPE) <= (cp->privilege&PRIV_TYPE))
                    {
                        srvmsgft(ci->clientnum, CON_EVENT, "\frAccess denied, you may not quarantine higher or equally privileged player %s", colourname(cp));
                        break;
                    }
                    if(!spectate(cp, spec, quarantine))
                    {
                        srvmsgft(ci->clientnum, CON_DEBUG, "sync error: unable to modify spectator %s - %d [%d, %d] - failed", colourname(cp), cp->state, cp->lastdeath, gamemillis);
                        break;
                    }
                    if(quarantine && cp->quarantine)
                    {
                        defformatstring(name, "%s", colourname(ci));
                        srvoutf(3, "%s \fs\fcquarantined\fS %s", name, colourname(cp));
                    }
                    else if(wasq && !cp->quarantine)
                    {
                        defformatstring(name, "%s", colourname(ci));
                        srvoutf(3, "%s \fs\fcreleased\fS %s from \fs\fcquarantine\fS", name, colourname(cp));
                    }
                    break;
                }

                case N_SETTEAM:
                {
                    int who = getint(p), team = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(who);
                    if(!cp || !m_team(gamemode, mutators) || m_local(gamemode) || cp->actortype >= A_ENEMY) break;
                    if(who < 0 || who >= getnumclients() || !haspriv(ci, G(teamlock), "change the team of others")) break;
                    if(cp->state == CS_SPECTATOR || !allowteam(cp, team, T_FIRST, false)) break;
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
                    int dni = getint(p);
                    senddemo(sender, n, dni);
                    break;
                }

                case N_EDITENT:
                {
                    int n = getint(p), oldtype = NOTUSED, newtype = NOTUSED;
                    ivec o(0, 0, 0);
                    bool tweaked = false, inrange = n < MAXENTS;
                    loopk(3) o[k] = getint(p);
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
                        hasgameinfo = true;
                        sents[n].o = vec(o).div(DMF);
                        packetbuf q(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
                        uchar s[MAXTRANS];
                        ucharbuf r(s, MAXTRANS);
                        putint(q, N_CLIENT);
                        putint(q, ci->clientnum);
                        putint(r, N_EDITENT);
                        putint(r, n);
                        putint(r, o.x);
                        putint(r, o.y);
                        putint(r, o.z);
                        putint(r, sents[n].type);
                        putint(r, sents[n].attrs.length());
                        loopvk(sents[n].attrs) putint(r, sents[n].attrs[k]);
                        putuint(q, r.length());
                        q.put(r.getbuf(), r.length());
                        sendpacket(-1, 1, q.finalize(), ci->clientnum);
                        if(tweaked)
                        {
                            if(enttype[sents[n].type].usetype == EU_ITEM) setspawn(n, true, true, true);
                            if(oldtype == PLAYERSTART || sents[n].type == PLAYERSTART) setupspawns(true);
                            if(oldtype == TRIGGER || sents[n].type == TRIGGER) setuptriggers(true);
                        }
                    }
                    break;
                }

                case N_EDITVAR:
                {
                    int t = getint(p);
                    getstring(text, p);
                    if(!ci || ci->state != CS_EDITING)
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
                    getmap(ci);
                    break;
                }

                case N_NEWMAP:
                {
                    int size = getint(p);
                    getstring(text, p);
                    if(ci->state != CS_EDITING) break;
                    QUEUE_INT(N_NEWMAP);
                    QUEUE_INT(size);
                    if(size >= 0)
                    {
                        if(*text) formatstring(smapname, strstr(text, "maps/")==text || strstr(text, "maps\\")==text ? "%s" : "maps/%s", text);
                        else copystring(smapname, "maps/untitled");
                        sents.shrink(0);
                        hasgameinfo = true;
                        mapgameinfo = -1;
                        if(smode) smode->reset();
                        mutate(smuts, mut->reset());
                        QUEUE_STR(smapname);
                    }
                    else QUEUE_STR(text);
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
                            if(!adminpass[0]) srvmsgft(ci->clientnum, CON_EVENT, "\frAccess denied, no administrator password set");
                            else if(!checkpassword(ci, adminpass, text)) srvmsgft(ci->clientnum, CON_EVENT, "\frAccess denied, invalid administrator password");
                            else auth::setprivilege(ci, 1, PRIV_ADMINISTRATOR|PRIV_LOCAL);
                        }
                        else if((ci->privilege&PRIV_TYPE) < PRIV_ELEVATED)
                        {
                            bool fail = false;
                            if(!(mastermask()&MM_AUTOAPPROVE))
                            {
                                srvmsgft(ci->clientnum, CON_EVENT, "\frAccess denied, you need a \fs\fcpassword/account\fS to \fs\fcelevate privileges\fS");
                                fail = true;
                            }
                            else loopv(clients) if(ci != clients[i] && (clients[i]->privilege&PRIV_TYPE) >= PRIV_ELEVATED)
                            {
                                srvmsgft(ci->clientnum, CON_EVENT, "\frAccess denied, there is already another player with elevated privileges");
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
                    stringz(authname);
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
                    if(ci->state == CS_EDITING) sendclipboard(ci);
                    goto genericmsg;

                case N_CLIPBOARD:
                {
                    int unpacklen = getint(p), packlen = getint(p);
                    ci->cleanclipboard();
                    ci->lastclipboard = totalmillis ? totalmillis : 1;
                    if(ci->state != CS_EDITING)
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
                    if(ci && ci->state!=CS_SPECTATOR) QUEUE_MSG;
                    break;
                }

                case N_UNDO:
                case N_REDO:
                {
                    int unpacklen = getint(p), packlen = getint(p);
                    if(!ci || ci->state==CS_SPECTATOR || packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                    {
                        if(packlen > 0) p.subbuf(packlen);
                        break;
                    }
                    if(p.remaining() < packlen) { disconnect_client(sender, DISC_MSGERR); return; }
                    uchar s[MAXTRANS];
                    ucharbuf q(s, MAXTRANS);
                    putint(q, type);
                    putint(q, ci->clientnum);
                    putint(q, unpacklen);
                    putint(q, packlen);
                    if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                    ci->messages.put(q.buf, q.length());
                    curmsg += q.length();
                    break;
                }

                case N_ADDPRIV:
                {
                    int sn = getint(p), priv = getint(p);
                    clientinfo *cp = (clientinfo *)getinfo(sn);
                    if(!cp)
                    {
                        srvmsgft(ci->clientnum, CON_EVENT, "\frThat client does not exist");
                        break;
                    }
                    if((priv != -1) && (priv < PRIV_SUPPORTER || priv > PRIV_ADMINISTRATOR || cp->actortype != A_PLAYER))
                    {
                        srvmsgft(ci->clientnum, CON_EVENT, "\frYou may not add that privilege");
                        break;
                    }
                    if(priv == -1 && (ci->privilege&PRIV_TYPE) <= (cp->privilege&PRIV_TYPE) && (ci->privilege&PRIV_TYPE) < PRIV_ADMINISTRATOR)
                    {
                        srvmsgft(ci->clientnum, CON_EVENT, "\frYou must be a \fs\fc%s\fS to reset that client's privileges", privname((cp->privilege & PRIV_TYPE) + 1));
                        break;
                    }
                    if(!((ci->privilege&PRIV_TYPE) >= PRIV_ADMINISTRATOR) && !haspriv(ci, priv, "add that privilege")) break;
                    if(priv == -1)
                    {
                        if(cp->oldprivilege == -1)
                        {
                            srvmsgft(ci->clientnum, CON_EVENT, "\fr%s does not have any added privilege", colourname(cp));
                            break;
                        }
                        else
                        {
                            auth::setprivilege(cp, 1, cp->oldprivilege, false, ci);
                            cp->oldprivilege = -1;
                            break;
                        }
                    }
                    if(priv <= (cp->privilege&PRIV_TYPE))
                    {
                        srvmsgft(ci->clientnum, CON_EVENT, "\fr%s is already elevated to \fs\fc%s\fS", colourname(cp), privname(cp->privilege));
                        break;
                    }
                    if(cp->oldprivilege == -1) cp->oldprivilege = cp->privilege;
                    auth::setprivilege(cp, 1, priv|PRIV_LOCAL, false, ci);
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
