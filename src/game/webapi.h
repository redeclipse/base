// the game's side of the Web API: who a request is, its routes and the Event feed

VAR(0, httpsessiontime, 60000, 1800000, VAR_MAX); // how long a session token lasts unused
VAR(0, httpfeedchat, 0, 1, 1); // public chat goes to the Event feed
VAR(0, httprelay, 0, 2, 2); // what the master may relay: 0 nothing, 1 public routes and the Event feed, 2 everything
VAR(0, httpmaplock, PRIV_PLAYER, PRIV_MODERATOR, PRIV_MAX); // who may change the map over the Web API, which skips the vote
VAR(0, httpsaylock, PRIV_PLAYER, PRIV_PLAYER, PRIV_MAX); // who may chat over the Web API, on top of messagelock
VAR(0, httpauthfails, 1, 10, VAR_MAX); // failed tokens or challenges an address may try in a minute
VAR(0, httpmatches, 0, 10, 100); // results of the last matches kept for GET /api/v1/matches, 0 keeps none
VAR(0, httpmatchstatus, 1, 3, 8); // players the status's last match carries to the Server list, the winner at least
VAR(0, httpauthban, 0, 3, VAR_MAX); // minutes an address may be locked out in an hour before it's banned for bantimeout, 0 never

namespace webapi
{
    struct apitoken
    {
        char *name, *token;
        int privilege;

        apitoken() : name(NULL), token(NULL), privilege(PRIV_NONE) {}
        ~apitoken() { DELETEA(name); DELETEA(token); }
    };
    vector<apitoken> tokens;

    struct session
    {
        string token, name;
        int privilege, lastused;
    };
    vector<session> sessions;

    struct pendingauth
    {
        uint id, ip;
        string name;
        int deferred, socket, started;
        bool relayed;
    };
    vector<pendingauth> pendings;
    uint nextauth = 0;

    // viewers of the Event feed, here or through the master's relay, where their id is the master's
    struct subscriber
    {
        int id, privilege;
        uint ip;
        bool relayed, secure; // secure: encrypted all the way, so it may sign in
        string handle, addr, token; // the token it was let in with, checked again as tokens change
    };
    vector<subscriber> subscribers;

    static subscriber *findsub(int id, bool relayed)
    {
        loopv(subscribers) if(subscribers[i].id == id && subscribers[i].relayed == relayed) return &subscribers[i];
        return NULL;
    }

    // a relayed viewer is told through the master, which delivers only to the viewers named
    static void tellrelayed(const vector<int> &ids, const char *data, int len)
    {
        vector<char> msg;
        jsonwriter w(msg);
        w.beginobject();
        w.utf8("relay", "tell");
        w.beginarray("ids");
        loopv(ids) w.num(NULL, ids[i]);
        w.endarray();
        w.utf8("data", data, len);
        w.endobject();
        masterlinksend(msg.getbuf(), msg.length());
    }

    static void tell(subscriber &sub, const char *data, int len = -1)
    {
        if(len < 0) len = strlen(data);
        if(!sub.relayed) { http::wssend(sub.id, data, len); return; }
        vector<int> ids;
        ids.add(sub.id);
        tellrelayed(ids, data, len);
    }

    bool relayfeed = false; // the master has viewers of our Event feed

    // failed tokens and challenges by address, so guessing tokens gets nowhere
    struct authfail
    {
        uint ip;
        int count, start, strikes, struck;
    };
    vector<authfail> authfails;

    static bool authblocked(uint ip)
    {
        loopv(authfails) if(authfails[i].ip == ip)
        {
            authfail &f = authfails[i];
            if(totalmillis - f.start >= 60000)
            {
                if(!f.strikes || totalmillis - f.struck >= 3600000) authfails.remove(i);
                else f.count = 0;
                return false;
            }
            return f.count >= httpauthfails;
        }
        return false;
    }

    // a temporary ban like any other, so it shuts the address out of the game too and moderators can lift it
    static void authban(uint ip)
    {
        if(!ip || checkipinfo(control, ipinfo::BAN, ip) || checkipinfo(control, ipinfo::EXCEPT, ip) || checkipinfo(control, ipinfo::TRUST, ip)) return;
        ipinfo &c = control.add();
        c.ip = ip;
        c.mask = 0xFFFFFFFFU;
        c.type = ipinfo::BAN;
        c.flag = ipinfo::INTERNAL;
        c.time = totalmillis ? totalmillis : 1;
        c.reason = newstring("too many failed sign-in attempts");
        setcontrolsource(c, ipinfo::SRC_AUTO, NULL, NULL);
        string addr = "";
        ENetAddress a;
        a.host = ip;
        enet_address_get_host_ip(&a, addr, sizeof(addr));
        conoutf(colourcyan, "Web API: banned %s: %s", addr, c.reason);
        updatecontrols = true;
        controlchanged(c, true, NULL);
        // closing it here means no close event comes back, so it goes from the list now
        loopvrev(subscribers) if(subscribers[i].ip == ip)
        {
            if(subscribers[i].relayed)
            {
                defformatstring(msg, "{\"relay\":\"deny\",\"id\":%d}", subscribers[i].id);
                masterlinksend(msg, strlen(msg));
            }
            else http::wsclose(subscribers[i].id, 1008);
            subscribers.remove(i);
        }
    }

    static void authfailed(uint ip)
    {
        authfail *f = NULL;
        loopv(authfails) if(authfails[i].ip == ip) { f = &authfails[i]; break; }
        if(!f)
        {
            if(authfails.length() >= 4096) authfails.remove(0);
            f = &authfails.add();
            f->ip = ip;
            f->count = f->strikes = 0;
            f->start = f->struck = totalmillis;
        }
        if(totalmillis - f->start >= 60000) { f->start = totalmillis; f->count = 0; }
        if(f->strikes && totalmillis - f->struck >= 3600000) f->strikes = 0;
        if(++f->count != httpauthfails) return;
        f->strikes++;
        f->struck = totalmillis;
        if(httpauthban && f->strikes >= httpauthban) authban(ip);
    }

    // compare without stopping early, so timing gives nothing away
    static bool sametoken(const char *a, const char *b)
    {
        int alen = strlen(a), blen = strlen(b);
        uchar diff = alen != blen;
        loopi(alen) diff |= uchar(a[i]) ^ uchar(b[i % max(blen, 1)]);
        return !diff;
    }

    static int parsepriv(const char *s)
    {
        loopi(PRIV_MAX) if(!strcasecmp(s, PRIV_STR[i])) return i;
        int n = PRIV_NONE;
        for(const char *c = s; *c; c++) switch(*c)
        {
            case 'c': case 'C': n = max(n, int(PRIV_CREATOR)); break;
            case 'd': case 'D': n = max(n, int(PRIV_DEVELOPER)); break;
            case 'a': case 'A': n = max(n, int(PRIV_ADMINISTRATOR)); break;
            case 'o': case 'O': case 'm': case 'M': n = max(n, int(PRIV_MODERATOR)); break;
            case 's': case 'S': n = max(n, int(PRIV_SUPPORTER)); break;
            case 'u': case 'U': case 'p': case 'P': n = max(n, int(PRIV_PLAYER)); break;
            default: break;
        }
        return n;
    }

    static void privname(char *buf, int priv)
    {
        copystring(buf, PRIV_STR[clamp(priv&PRIV_TYPE, 0, PRIV_MAX-1)], MAXSTRLEN);
        for(char *s = buf; *s; s++) *s = tolower(*s);
    }

    static void newtoken(string &token)
    {
        uchar bytes[24];
        http::randombytes(bytes, sizeof(bytes));
        token[0] = '\0';
        loopi(int(sizeof(bytes))) { defformatstring(h, "%02x", bytes[i]); concatstring(token, h); }
    }

    static void recheck();

    static void addtoken(char *name, char *token, char *priv)
    {
        if(!*name || strlen(token) < 16) { conoutf(colourred, "API tokens need a name and at least 16 characters"); return; }
        int p = parsepriv(priv);
        if(p <= PRIV_NONE) { conoutf(colourred, "Unknown privilege for API token %s: %s", name, priv); return; }
        loopv(tokens) if(!strcmp(tokens[i].name, name)) tokens.remove(i--);
        apitoken &t = tokens.add();
        t.name = newstring(name);
        t.token = newstring(token);
        t.privilege = p;
        recheck();
    }
    COMMANDN(0, addhttptoken, addtoken, "sss");
    ICOMMAND(0, clearhttptokens, "", (), { tokens.shrink(0); recheck(); });

    static void cleansessions()
    {
        loopvrev(sessions) if(totalmillis - sessions[i].lastused > httpsessiontime) sessions.remove(i);
        loopvrev(pendings) if(totalmillis - pendings[i].started > 60000) pendings.remove(i);
    }

    static bool lookup(const char *token, int &privilege, char *handle, bool use = true)
    {
        if(!token || !*token) return false;
        loopv(tokens) if(sametoken(token, tokens[i].token))
        {
            privilege = tokens[i].privilege;
            copystring(handle, tokens[i].name, MAXSTRLEN);
            return true;
        }
        cleansessions();
        loopv(sessions) if(sametoken(token, sessions[i].token))
        {
            if(use) sessions[i].lastused = totalmillis;
            privilege = sessions[i].privilege;
            copystring(handle, sessions[i].name, MAXSTRLEN);
            return true;
        }
        return false;
    }

    void writeclient(jsonwriter &w, const char *name, clientinfo *ci, int privilege = HTTP_PUBLIC)
    {
        w.beginobject(name);
        if(!ci) { w.num("cn", -1); w.endobject(); return; }
        string priv;
        privname(priv, ci->privilege);
        w.num("cn", ci->clientnum);
        w.str("name", ci->name);
        w.str("team", T_STR[clamp(ci->team, 0, T_MAX-1)]);
        w.boolean("bot", ci->actortype > A_PLAYER);
        if(ci->handle[0]) w.str("handle", ci->handle);
        w.str("privilege", priv);
        w.endobject();
    }

    // events go to viewers from minpriv up to, but not including, belowpriv
    void publish(vector<char> &json, int minpriv = HTTP_PUBLIC, int belowpriv = INT_MAX)
    {
        if(relayfeed && minpriv <= HTTP_PUBLIC && httprelay >= 1)
        {
            static const char head[] = "{\"relay\":\"event\",\"event\":";
            vector<char> msg;
            msg.put(head, sizeof(head)-1);
            msg.put(json.getbuf(), json.length());
            msg.add('}');
            masterlinksend(msg.getbuf(), msg.length());
        }
        if(subscribers.empty()) return;
        static vector<int> ids, relayed;
        ids.setsize(0);
        relayed.setsize(0);
        loopv(subscribers) if(subscribers[i].privilege >= minpriv && subscribers[i].privilege < belowpriv)
        {
            // relayed viewers had public events from the master's fan-out above
            if(!subscribers[i].relayed) ids.add(subscribers[i].id);
            else if(minpriv > HTTP_PUBLIC && httprelay >= 2) relayed.add(subscribers[i].id);
        }
        if(ids.length()) http::wsbroadcast(ids, json.getbuf(), json.length());
        if(relayed.length()) tellrelayed(relayed, json.getbuf(), json.length());
    }

    // the Server list's status for this server, what the query reply has, sent over the link when it changes
    static vector<char> laststatus;
    static int laststatuscheck = 0, laststatusends = 0, laststateends = 0;
    static bool sendstatus = false;

    // the map's title and author, from the variables at the head of its file, read once for each map
    #define MAPINFO_SKIP 1024 // the most header past the universal one a map can have
    struct mapinfocache
    {
        int crc;
        string title, author;
    } mapinfo = { 0, "", "" };

    static bool mapinfoskip(stream *f, int len)
    {
        char buf[256];
        while(len > 0)
        {
            int n = min(len, int(sizeof(buf)));
            if(f->read(buf, n) != size_t(n)) return false;
            len -= n;
        }
        return true;
    }

    static void readmapinfo(stream *f)
    {
        char head[4];
        if(f->read(head, 4) != 4 || memcmp(head, "MAPZ", 4)) return;
        int version = f->getlil<int>(), headersize = f->getlil<int>();
        // version 43 has a header of its own size, later ones say how big theirs is
        int skip = version == 43 ? 36 : headersize - 12;
        if(version < 43 || skip < 0 || skip > MAPINFO_SKIP || !mapinfoskip(f, skip)) return;
        int numvars = f->getlil<int>();
        loopi(numvars)
        {
            int len = f->getlil<int>();
            if(!len) continue;
            string name;
            if(len < 0 || len >= MAXSTRLEN || f->read(name, len+1) != size_t(len+1)) return;
            name[len] = '\0';
            int type = f->getlil<int>();
            if(type == ID_VAR) f->getlil<int>();
            else if(type == ID_FVAR) f->getlil<float>();
            else if(type == ID_SVAR)
            {
                int vlen = f->getlil<int>();
                char *dst = !strcmp(name, "maptitle") ? mapinfo.title : (!strcmp(name, "mapauthor") ? mapinfo.author : NULL);
                if(vlen < 0 || vlen > (1<<20)) return;
                if(dst && vlen < MAXSTRLEN)
                {
                    string val;
                    if(f->read(val, vlen+1) != size_t(vlen+1)) return;
                    val[vlen] = '\0';
                    filterstring(dst, val, false, true, true, true, MAXSTRLEN-1);
                    if(mapinfo.title[0] && mapinfo.author[0]) return;
                }
                else if(!mapinfoskip(f, vlen+1)) return;
            }
            else return;
        }
    }

    static mapinfocache &getmapinfo()
    {
        stream *raw = smapcrc ? mapdata[SENDMAP_MPZ] : NULL;
        if(!raw) { mapinfo.crc = 0; mapinfo.title[0] = mapinfo.author[0] = '\0'; return mapinfo; }
        if(mapinfo.crc == smapcrc) return mapinfo;
        mapinfo.crc = smapcrc;
        mapinfo.title[0] = mapinfo.author[0] = '\0';
        // the same stream is sent to players who need the map, so it's left where it was
        stream::offset pos = raw->tell();
        raw->seek(0, SEEK_SET);
        stream *f = opengzfile(NULL, "rb", raw);
        if(f) { readmapinfo(f); delete f; }
        raw->seek(pos, SEEK_SET);
        return mapinfo;
    }

    // the map's preview, when the server has the map's files; players who loaded it from elsewhere have one of their own
    static bool hasmapshot() { return smapcrc && mapdata[SENDMAP_MPZ] && mapdata[SENDMAP_PNG]; }

    static void writemapinfo(jsonwriter &w)
    {
        mapinfocache &m = getmapinfo();
        if(m.title[0]) w.str("maptitle", m.title);
        if(m.author[0]) w.str("mapauthor", m.author);
        if(hasmapshot()) w.num("mapcrc", double(uint(smapcrc)));
    }

    struct matchresult;
    static vector<matchresult *> matches;
    static void writematch(jsonwriter &w, const char *name, matchresult &m, int maxplayers);

    // what the operator wrote about the server: its web address, only when it's one a browser can safely open, and its message of the day as plain text
    static void writeabout(jsonwriter &w)
    {
        const char *url = G(serverurl);
        bool safe = (!strncmp(url, "http://", 7) && url[7]) || (!strncmp(url, "https://", 8) && url[8]);
        for(const char *c = url; safe && *c; c++) if(uchar(*c) <= ' ' || uchar(*c) >= 0x7F || *c == '"' || *c == '<' || *c == '>' || *c == '\\') safe = false;
        if(safe) w.str("url", url);
        if(*G(servermotd))
        {
            string motd;
            filterstring(motd, G(servermotd), true, true, true, false);
            if(motd[0]) w.str("motd", motd);
        }
    }

    static void writestatus(jsonwriter &w, int ends, int stateends)
    {
        static vector<clientinfo *> named;
        named.setsize(0);
        loopv(clients) if(clients[i]->clientnum >= 0 && clients[i]->name[0] && clients[i]->actortype == A_PLAYER) named.add(clients[i]);
        named.sort(querysort);
        w.beginobject();
        w.num("gameversion", VERSION_GAME);
        w.str("map", smapname);
        writemapinfo(w);
        w.num("modeid", gamemode);
        w.num("mutators", mutators);
        w.num("ends", double(ends));
        w.num("maxplayers", maxslots());
        w.num("mastermode", serverpass[0] || G(connectlock) ? MASTERMODE_PASSWORD : (m_local(gamemode) ? MASTERMODE_PRIVATE : mastermode));
        w.num("gamevars", numgamevars);
        w.num("gamemods", numgamemods);
        w.beginarray("build");
        w.num(NULL, versionmajor);
        w.num(NULL, versionminor);
        w.num(NULL, versionpatch);
        w.num(NULL, versionplatform);
        w.num(NULL, versionarch);
        w.endarray();
        w.num("state", gamestate);
        w.num("stateends", double(stateends));
        w.num("spectators", numspectators());
        w.num("bots", numclients(-1, false, A_BOT));
        writeabout(w);
        if(matches.length()) writematch(w, "lastmatch", *matches.last(), httpmatchstatus);
        w.beginarray("players");
        // plain, with what the client decorates them by as fields of their own; the chosen colour, as the one shown falls back to the weapon's and would change with it
        loopv(named)
        {
            clientinfo *ci = named[i];
            string priv;
            privname(priv, ci->privilege);
            w.beginobject();
            w.num("cn", ci->clientnum);
            w.str("name", ci->name);
            if(ci->handle[0]) w.str("handle", ci->handle);
            w.str("team", T_STR[clamp(ci->team, 0, T_MAX-1)]);
            w.str("privilege", priv);
            if(ci->privilege&PRIV_LOCAL) w.boolean("local", true);
            if(ci->state == CS_SPECTATOR) w.boolean("spectator", true);
            w.num("colour", ci->colours[0]);
            w.endobject();
        }
        w.endarray();
        w.endobject();
    }

    // end times are unix seconds and drift by a second now and then, which isn't worth telling every viewer about
    void checkstatus()
    {
        if(!masterlinked() || (!sendstatus && totalmillis - laststatuscheck < 1000)) return;
        laststatuscheck = totalmillis;
        int now = int(time(NULL)), ends = timeremaining >= 0 ? now + timeremaining/1000 : -1, stateends = now + timeleft()/1000;
        vector<char> body;
        jsonwriter b(body);
        writestatus(b, 0, 0);
        // the time left stands still outside play, and a state without a timer ends now, so those move with the clock without being a change
        bool playing = gamestate == G_S_PLAYING || gamestate == G_S_OVERTIME,
             endsheld = !playing || abs(ends - laststatusends) <= 2,
             stateheld = abs(stateends - laststateends) <= 2 || (stateends <= now && laststateends <= now);
        if(!sendstatus && endsheld && stateheld && body.length() == laststatus.length() && !memcmp(body.getbuf(), laststatus.getbuf(), body.length())) return;
        sendstatus = false;
        laststatusends = ends;
        laststateends = stateends;
        laststatus = body;
        vector<char> full, msg;
        jsonwriter f(full), w(msg);
        writestatus(f, ends, stateends);
        w.beginobject();
        w.raw("status", full.getbuf(), full.length());
        w.endobject();
        masterlinksend(msg.getbuf(), msg.length());
    }

    // only over the link, older masters answer anything unknown with an error
    void registered()
    {
        if(!masterlinked()) return;
        relayfeed = false; // the master asks again if it has viewers
        loopvrev(subscribers) if(subscribers[i].relayed) subscribers.remove(i);
        loopvrev(pendings) if(pendings[i].relayed) pendings.remove(i);
        const char *fp = http::fingerprint();
        requestmasterf("webapi %d %d %s\n", http::serverport(), httprelay, *fp ? fp : "-");
        sendstatus = true;
    }

    bool wanted()
    {
        return subscribers.length() || relayfeed;
    }

    static void begin(jsonwriter &w, const char *event)
    {
        w.beginobject();
        w.str("event", event);
        w.num("time", double(time(NULL)));
    }

    void join(clientinfo *ci)
    {
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "join");
        writeclient(w, "client", ci);
        w.endobject();
        publish(buf);
    }

    void leave(clientinfo *ci, int reason)
    {
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "leave");
        writeclient(w, "client", ci);
        if(reason > DISC_NONE && reason < DISC_NUM) w.str("reason", disc_reasons[reason]);
        w.endobject();
        publish(buf);
    }

    void team(clientinfo *ci)
    {
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "team");
        writeclient(w, "client", ci);
        w.endobject();
        publish(buf);
    }

    void frag(clientinfo *victim, clientinfo *killer, int weap, int flags)
    {
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, victim == killer ? "suicide" : "frag");
        writeclient(w, "victim", victim);
        if(killer && killer != victim) writeclient(w, "killer", killer);
        if(isweap(weap)) w.str("weapon", W_STR[weap]);
        w.num("flags", flags);
        w.endobject();
        publish(buf);
    }

    void chat(clientinfo *ci, const char *text, int flags)
    {
        if(!wanted() || !httpfeedchat || flags&(SAY_TEAM|SAY_WHISPER)) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, flags&SAY_ACTION ? "action" : "chat");
        writeclient(w, "client", ci);
        string filtered;
        filterstring(filtered, text, true, true, true, false);
        w.str("text", filtered);
        w.endobject();
        publish(buf);
    }

    // chat sent over the Web API, from a web user rather than a client
    void webchat(const char *handle, int privilege, const char *text)
    {
        if(!wanted() || !httpfeedchat) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "chat");
        string priv;
        privname(priv, privilege);
        w.beginobject("client");
        w.num("cn", -1);
        w.str("name", *handle ? handle : "someone");
        if(*handle) w.str("handle", handle);
        w.str("privilege", priv);
        w.boolean("web", true);
        w.endobject();
        w.str("text", text);
        w.endobject();
        publish(buf);
    }

    void mapchange()
    {
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "map");
        w.str("map", smapname);
        string mode;
        filterstring(mode, gamename(gamemode, mutators), true, true, true, false);
        w.str("mode", mode);
        w.num("modeid", gamemode);
        w.num("mutators", mutators);
        w.endobject();
        publish(buf);
    }

    static int maskbits(uint netmask)
    {
        uint mask = ENET_NET_TO_HOST_32(netmask);
        int bits = 0;
        while(bits < 32 && (mask & (0x80000000U >> bits))) bits++;
        return bits;
    }

    // seen by those who may see that type of entry, with the address for those who may also see addresses
    void controlchanged(const ipinfo &c, bool added, const char *by)
    {
        if(!wanted() || c.type < 0 || c.type >= ipinfo::SYNCTYPES) return;
        int minpriv = max(controllock(c.type), int(HTTP_PUBLIC)+1), iplevel = max(minpriv, G(iphostlock));
        loopk(2)
        {
            vector<char> buf;
            jsonwriter w(buf);
            begin(w, "control");
            w.str("action", added ? "add" : "remove");
            w.str("type", controlname(c.type));
            w.num("id", c.id);
            if(c.name[0]) w.str("name", c.name);
            if(c.reason && *c.reason) w.str("reason", c.reason);
            if(by && *by) w.str("by", by);
            if(k)
            {
                string ip;
                ENetAddress a;
                a.host = c.ip;
                if(enet_address_get_host_ip(&a, ip, sizeof(ip)) >= 0) w.str("ip", ip);
                int bits = maskbits(c.mask);
                if(bits < 32) w.num("mask", bits);
            }
            w.endobject();
            if(k) publish(buf, iplevel);
            else publish(buf, minpriv, iplevel);
        }
    }

    // broadcast game log lines, those every player sees anyway
    void gamelogged(gamelog &log)
    {
        if(!wanted()) return;
        int tid = log.findlistinfo("args");
        if(!log.lists.inrange(tid)) return;
        vector<gamelog::info> &args = log.lists[tid].infos;
        int target = gamelog::findinfo(args, "target");
        if(args.inrange(target) && args[target].getint() >= 0) return;
        vector<char> buf;
        jsonwriter w(buf);
        int type = gamelog::findinfo(args, "type");
        begin(w, args.inrange(type) ? args[type].getstr() : "log");
        loopv(args)
        {
            gamelog::info &n = args[i];
            if(!strcmp(n.name, "type") || !strcmp(n.name, "target") || !strcmp(n.name, "flags") || !strcmp(n.name, "relay") || !strcmp(n.name, "health") ||
               !strcmp(n.name, "colour") || !strcmp(n.name, "concol") || !strcmp(n.name, "sound")) continue;
            if(!strcmp(n.name, "console"))
            {
                string text;
                filterstring(text, n.getstr(), true, true, true, false);
                w.str("text", text);
                continue;
            }
            switch(n.type)
            {
                case VAL_INT: w.num(n.name, n.i); break;
                case VAL_FLOAT: w.num(n.name, double(n.f)); break;
                case VAL_STR: case VAL_CSTR: w.str(n.name, n.s); break;
                default: break;
            }
        }
        loopv(log.tags)
        {
            gamelog::taginfo &t = log.tags[i];
            w.beginarray(t.name);
            loopvj(t.groups)
            {
                vector<gamelog::info> &infos = t.groups[j].infos;
                w.beginobject();
                loopvk(infos)
                {
                    gamelog::info &n = infos[k];
                    if(strcmp(n.name, "clientnum") && strcmp(n.name, "name") && strcmp(n.name, "team") && strcmp(n.name, "actortype") && strcmp(n.name, "privilege")) continue;
                    if(!strcmp(n.name, "team") && n.type == VAL_INT) { w.str("team", T_STR[clamp(n.i, 0, T_MAX-1)]); continue; }
                    if(!strcmp(n.name, "clientnum") && n.type == VAL_INT) { w.num("cn", n.i); continue; }
                    switch(n.type)
                    {
                        case VAL_INT: w.num(n.name, n.i); break;
                        case VAL_FLOAT: w.num(n.name, double(n.f)); break;
                        case VAL_STR: case VAL_CSTR: w.str(n.name, n.s); break;
                        default: break;
                    }
                }
                w.endobject();
            }
            w.endarray();
        }
        w.endobject();
        publish(buf);
    }

    // scores change in many places in every mode, so compare them once a second
    struct scoreseen
    {
        int cn, points, frags, deaths;
    };
    static vector<scoreseen> lastscores;
    static int lastteamscores[T_MAX], lastscorecheck = 0;

    static int lastrecheck = 0;

    void checkscores()
    {
        if(totalmillis - lastrecheck >= 10000) { lastrecheck = totalmillis; recheck(); }
        if(!wanted() || totalmillis - lastscorecheck < 1000) return;
        lastscorecheck = totalmillis;
        vector<char> buf;
        jsonwriter w(buf);
        bool changed = false;
        begin(w, "score");
        if(m_team(gamemode, mutators))
        {
            w.beginarray("teams");
            loopv(scores)
            {
                score &ts = scores[i];
                if(ts.team < 0 || ts.team >= T_MAX) continue;
                if(lastteamscores[ts.team] != ts.total) { lastteamscores[ts.team] = ts.total; changed = true; }
                w.beginobject();
                w.str("team", T_STR[ts.team]);
                w.num("score", ts.total);
                w.endobject();
            }
            w.endarray();
        }
        w.beginarray("players");
        int count = 0;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            scoreseen *seen = NULL;
            loopvj(lastscores) if(lastscores[j].cn == ci->clientnum) { seen = &lastscores[j]; break; }
            if(seen && seen->points == ci->points && seen->frags == ci->frags && seen->deaths == ci->deaths) continue;
            if(!seen) { seen = &lastscores.add(); seen->cn = ci->clientnum; }
            seen->points = ci->points;
            seen->frags = ci->frags;
            seen->deaths = ci->deaths;
            w.beginobject();
            w.num("cn", ci->clientnum);
            w.num("points", ci->points);
            w.num("frags", ci->frags);
            w.num("deaths", ci->deaths);
            w.endobject();
            count++;
        }
        w.endarray();
        w.endobject();
        loopvrev(lastscores)
        {
            bool found = false;
            loopvj(clients) if(clients[j]->clientnum == lastscores[i].cn) { found = true; break; }
            if(!found) lastscores.remove(i);
        }
        if(changed || count) publish(buf);
    }

    // the results of the last few matches, kept until the server stops
    struct matchplayer
    {
        string name, handle;
        int cn, team, points, frags, deaths, cptime;
        bool bot;
    };
    struct matchteam
    {
        int team, score;
    };
    struct matchresult
    {
        int ended, duration, modeid, mutators;
        bool timed, draw;
        string map, maptitle;
        vector<matchteam> teams;
        vector<matchplayer> players;
    };

    // best first, as the scoreboard has them; a timed speedrun's best is the shortest time, and no time at all is last
    static bool timedbetter(int a, int b) { return a > 0 && (b <= 0 || a < b); }

    static bool matchplayersort(const matchplayer &a, const matchplayer &b)
    {
        if(m_sr_timed(gamemode, mutators))
        {
            if(timedbetter(a.cptime, b.cptime)) return true;
            if(timedbetter(b.cptime, a.cptime)) return false;
        }
        if(a.points != b.points) return a.points > b.points;
        if(!m_speedrun(gamemode) && a.frags != b.frags) return a.frags > b.frags;
        return strcmp(a.name, b.name) < 0;
    }

    static bool matchteamsort(const matchteam &a, const matchteam &b)
    {
        if(m_sr_timed(gamemode, mutators))
        {
            if(timedbetter(a.score, b.score)) return true;
            if(timedbetter(b.score, a.score)) return false;
        }
        else if(a.score != b.score) return a.score > b.score;
        return a.team < b.team;
    }

    static void writematch(jsonwriter &w, const char *name, matchresult &m, int maxplayers)
    {
        w.beginobject(name);
        w.num("ended", double(m.ended));
        w.num("duration", m.duration);
        w.str("map", m.map);
        if(m.maptitle[0]) w.str("maptitle", m.maptitle);
        w.num("modeid", m.modeid);
        w.num("mutators", m.mutators);
        if(m.timed) w.boolean("timed", true);
        if(m.draw) w.boolean("draw", true);
        if(m.teams.length())
        {
            w.beginarray("teams");
            loopv(m.teams)
            {
                w.beginobject();
                w.str("team", T_STR[clamp(m.teams[i].team, 0, T_MAX-1)]);
                w.num("score", m.teams[i].score);
                w.endobject();
            }
            w.endarray();
        }
        w.beginarray("players");
        loopv(m.players)
        {
            if(i >= maxplayers) break;
            matchplayer &p = m.players[i];
            w.beginobject();
            w.str("name", p.name);
            if(p.handle[0]) w.str("handle", p.handle);
            if(m.teams.length()) w.str("team", T_STR[clamp(p.team, 0, T_MAX-1)]);
            w.num("points", m.timed ? p.cptime : p.points);
            w.num("frags", p.frags);
            w.num("deaths", p.deaths);
            if(p.bot) w.boolean("bot", true);
            w.endobject();
        }
        w.endarray();
        w.endobject();
    }

    // when play stops with a winner, which a demo, editing, or a match nobody played in has none of
    void matchended()
    {
        if(!httpmatches || m_demo(gamemode) || m_edit(gamemode)) return;
        bool played = false;
        loopv(clients) if(clients[i]->actortype == A_PLAYER && clients[i]->state != CS_SPECTATOR) { played = true; break; }
        if(!played) return;
        matchresult *m = new matchresult;
        m->ended = int(time(NULL));
        m->duration = gamemillis/1000;
        m->modeid = gamemode;
        m->mutators = mutators;
        m->timed = m_sr_timed(gamemode, mutators);
        copystring(m->map, smapname);
        copystring(m->maptitle, getmapinfo().title);
        if(m_team(gamemode, mutators))
        {
            loopi(numteams(gamemode, mutators))
            {
                matchteam &t = m->teams.add();
                t.team = i+T_FIRST;
                t.score = teamscore(t.team).total;
            }
            m->teams.sort(matchteamsort);
        }
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->actortype >= A_ENEMY || ci->state == CS_SPECTATOR || !ci->name[0]) continue;
            matchplayer &p = m->players.add();
            copystring(p.name, ci->name);
            copystring(p.handle, ci->handle);
            p.cn = ci->clientnum;
            p.team = ci->team;
            p.points = ci->points;
            p.frags = ci->frags;
            p.deaths = ci->deaths;
            p.cptime = ci->cptime;
            p.bot = ci->actortype > A_PLAYER;
        }
        m->players.sort(matchplayersort);
        if(m->teams.length() >= 2) m->draw = m->teams[0].score == m->teams[1].score;
        else if(!m->teams.length() && m->players.length() >= 2)
        {
            matchplayer &a = m->players[0], &b = m->players[1];
            m->draw = m->timed ? a.cptime == b.cptime : a.points == b.points;
        }
        else m->draw = false;
        matches.add(m);
        while(matches.length() > httpmatches) delete matches.remove(0);
        sendstatus = true;
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "result");
        writematch(w, "match", *m, m->players.length());
        w.endobject();
        publish(buf);
    }

    void routematches(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginarray();
        loopvrev(matches) writematch(w, NULL, *matches[i], matches[i]->players.length());
        w.endarray();
        res.json(buf.getbuf(), buf.length());
    }

    // a flag or ball: who carries it, or whether it lies dropped or waits at home
    void writeaffinity(jsonwriter &w, int owner, bool dropped)
    {
        clientinfo *ci = owner >= 0 ? (clientinfo *)getinfo(owner) : NULL;
        w.str("state", ci ? "taken" : (dropped ? "dropped" : "home"));
        if(!ci) return;
        w.beginobject("carrier");
        w.num("cn", ci->clientnum);
        w.str("name", ci->name);
        w.str("team", T_STR[clamp(ci->team, 0, T_MAX-1)]);
        w.endobject();
    }

    static void writeobjectives(jsonwriter &w)
    {
        if(smode && gs_playing(gamestate)) smode->objectives(w);
    }

    // they change as players move, so like scores they're compared once a second
    static vector<char> lastobjectives;
    static int lastobjectivecheck = 0;

    void checkobjectives()
    {
        if(!wanted() || totalmillis - lastobjectivecheck < 1000) return;
        lastobjectivecheck = totalmillis;
        if(!smode) { lastobjectives.setsize(0); return; } // a mode without any, and the next one's are all news
        vector<char> body;
        jsonwriter b(body);
        b.beginobject();
        writeobjectives(b);
        b.endobject();
        if(body.length() == lastobjectives.length() && !memcmp(body.getbuf(), lastobjectives.getbuf(), body.length())) return;
        lastobjectives = body;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "objectives");
        writeobjectives(w);
        w.endobject();
        publish(buf);
    }

    void routeobjectives(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        writeobjectives(w);
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    static void autherror(subscriber &sub, const char *err)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "auth");
        w.str("error", err);
        w.endobject();
        tell(sub, buf.getbuf(), buf.length());
    }

    static void authorised(subscriber &sub, int priv, const char *handle, const char *token = NULL)
    {
        sub.privilege = priv;
        copystring(sub.handle, handle);
        copystring(sub.token, token ? token : "");
        string name;
        privname(name, priv);
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "auth");
        w.str("privilege", name);
        if(handle && *handle) w.str("name", handle);
        w.endobject();
        tell(sub, buf.getbuf(), buf.length());
    }

    // the first half of the master's challenge, for a deferred request or a WebSocket
    static const char *startchallenge(const char *name, uint ip, const char *addr, int socket, bool relayed = false, pendingauth **started = NULL)
    {
        if(!connectedmaster()) return "not connected to the master server";
        if(authblocked(ip)) return "too many failed attempts, try again in a minute";
        int count = 0;
        loopv(pendings) if(pendings[i].ip == ip) count++;
        if(count >= 4) return "too many challenges in progress";
        string clean;
        filterstring(clean, name, false, true, false, true);
        if(!clean[0] || strpbrk(clean, " \t\"")) return "bad name";
        pendingauth &p = pendings.add();
        if(!nextauth) nextauth = 1;
        p.id = WEBAUTH_ID | (nextauth++ & ~WEBAUTH_ID);
        p.ip = ip;
        copystring(p.name, clean);
        p.started = totalmillis;
        p.socket = socket;
        p.relayed = relayed;
        p.deferred = -1;
        requestmasterf("reqauth %u %s %s\n", p.id, p.name, addr && *addr ? addr : "-");
        if(started) *started = &p;
        return NULL;
    }

    static const char *answerchallenge(uint id, const char *answer, uint ip, int socket, bool relayed = false, pendingauth **answered = NULL)
    {
        if(!connectedmaster()) return "not connected to the master server";
        pendingauth *p = NULL;
        loopv(pendings) if(pendings[i].id == id && pendings[i].ip == ip && pendings[i].socket == socket && pendings[i].relayed == relayed && pendings[i].deferred < 0) { p = &pendings[i]; break; }
        if(!p) return "no such challenge";
        string val;
        copystring(val, answer);
        for(char *s = val; *s; s++) if(!isxdigit(*s)) { *s = '\0'; break; }
        requestmasterf("confauth %u %s\n", id, val);
        if(answered) *answered = p;
        return NULL;
    }

    // a socket keeps what its token gave it only while the token stands
    static void recheck()
    {
        cleansessions();
        loopv(subscribers)
        {
            subscriber &sub = subscribers[i];
            if(!sub.token[0]) continue;
            int priv = HTTP_PUBLIC;
            string handle;
            if(!lookup(sub.token, priv, handle, false)) { authorised(sub, HTTP_PUBLIC, ""); continue; }
            if(priv != sub.privilege) authorised(sub, priv, handle, sub.token);
        }
    }

    static void signin(subscriber &sub, const char *token)
    {
        int priv = HTTP_PUBLIC;
        string handle;
        if(authblocked(sub.ip)) autherror(sub, "too many failed attempts, try again in a minute");
        else if(lookup(token, priv, handle)) authorised(sub, priv, handle, token);
        else { authfailed(sub.ip); autherror(sub, "unknown token"); }
    }

    // browsers can't send headers on a WebSocket, so a viewer raises what it sees with
    // {"token": "..."}, or {"name": "..."} and then {"id": ..., "answer": "..."} for the master's challenge
    static void viewermessage(subscriber &sub, const char *data, int len)
    {
        if(sub.relayed && httprelay < 2) { autherror(sub, "this server doesn't allow signing in through the relay"); return; }
        // what a signed in viewer is sent, addresses among it, mustn't cross in the clear
        if(!sub.secure) { autherror(sub, "signing in on the Event feed needs wss://, localhost or a trusted proxy"); return; }
        jsonvalue *v = json::parse(data, len);
        if(!v) return;
        const char *token = v->getstr("token"), *name = v->getstr("name"), *answer = v->getstr("answer"), *err = NULL;
        if(token) signin(sub, token);
        else if(answer) err = answerchallenge(uint(v->getint("id", 0)), answer, sub.ip, sub.id, sub.relayed);
        else if(name) err = startchallenge(name, sub.ip, sub.addr, sub.id, sub.relayed);
        if(err) autherror(sub, err);
        delete v;
    }

    static void addsub(int id, bool relayed, bool secure, uint ip, const char *addr, int privilege = HTTP_PUBLIC, const char *handle = "", const char *token = "")
    {
        subscriber &s = subscribers.add();
        s.id = id;
        s.relayed = relayed;
        s.secure = secure;
        s.privilege = privilege;
        s.ip = ip;
        copystring(s.handle, handle);
        copystring(s.addr, addr);
        copystring(s.token, privilege > HTTP_PUBLIC ? token : "");
        lastscores.shrink(0);
        memset(lastteamscores, -1, sizeof(lastteamscores));
    }

    static void dropsub(int id, bool relayed)
    {
        loopv(subscribers) if(subscribers[i].id == id && subscribers[i].relayed == relayed) { subscribers.remove(i); break; }
        loopvrev(pendings) if(pendings[i].socket == id && pendings[i].relayed == relayed) pendings.remove(i);
    }

    void feedevent(httpwsevent &e, void *ctx)
    {
        switch(e.type)
        {
            case WSEV_OPEN:
                if(e.req) addsub(e.id, false, http::securereq(*e.req), e.req->ip, e.req->addr, e.req->privilege, e.req->handle ? e.req->handle : "", e.req->token ? e.req->token : "");
                else addsub(e.id, false, false, 0, "");
                break;
            case WSEV_CLOSE: dropsub(e.id, false); break;
            case WSEV_MESSAGE:
            {
                subscriber *sub = findsub(e.id, false);
                if(sub) viewermessage(*sub, e.data, e.len);
                break;
            }
        }
    }

    // the master sends challenges and answers for these back through processinput
    bool authchallenged(uint id, const char *val)
    {
        if(!(id&WEBAUTH_ID)) return false;
        loopv(pendings) if(pendings[i].id == id)
        {
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            if(pendings[i].socket >= 0) w.str("event", "challenge");
            w.num("id", double(id));
            w.str("challenge", val);
            w.endobject();
            if(pendings[i].socket >= 0)
            {
                subscriber *sub = findsub(pendings[i].socket, pendings[i].relayed);
                if(sub) tell(*sub, buf.getbuf(), buf.length());
            }
            else
            {
                httpresponse res;
                res.json(buf.getbuf(), buf.length());
                http::reply(pendings[i].deferred, res);
            }
            pendings[i].deferred = -1;
            break;
        }
        return true;
    }

    bool authfinished(uint id, const char *name, const char *flags)
    {
        if(!(id&WEBAUTH_ID)) return false;
        loopv(pendings) if(pendings[i].id == id)
        {
            pendingauth p = pendings.remove(i);
            httpresponse res;
            int n = name ? parsepriv(flags) : PRIV_NONE;
            if(name) loopvj(localops) if(!strcmp(localops[j].name, name)) n = max(n, parsepriv(localops[j].flags));
            if(!name || n <= PRIV_NONE) authfailed(p.ip);
            if(p.socket >= 0)
            {
                // a WebSocket keeps its standing for as long as it stays open, so it needs no session
                subscriber *sub = findsub(p.socket, p.relayed);
                if(!sub) return true;
                if(!name || n <= PRIV_NONE) autherror(*sub, "identity verification failed");
                else authorised(*sub, n, name);
                return true;
            }
            if(!name || n <= PRIV_NONE)
            {
                res.error(403, "identity verification failed");
                http::reply(p.deferred, res);
                return true;
            }
            session &s = sessions.add();
            newtoken(s.token);
            copystring(s.name, name);
            s.privilege = n;
            s.lastused = totalmillis;
            string priv;
            privname(priv, n);
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            w.str("token", s.token);
            w.str("name", name);
            w.str("privilege", priv);
            w.num("expires", httpsessiontime/1000);
            w.endobject();
            res.json(buf.getbuf(), buf.length());
            http::reply(p.deferred, res);
            conoutf(colourwhite, "Web API session for %s (%s) from %s", name, priv, p.ip ? "" : "the relay");
            return true;
        }
        return true;
    }

    static jsonvalue *bodyjson(httprequest &req)
    {
        if(!req.body.length()) return NULL;
        return json::parse(req.body.getbuf(), req.body.length());
    }

    static const char *field(httprequest &req, jsonvalue *v, const char *name)
    {
        const char *s = v ? v->getstr(name) : NULL;
        return s ? s : req.var(name);
    }

    void routeauth(httprequest &req, httpresponse &res)
    {
        // the session token in the answer would cross in the clear, and work over https for whoever saw it
        if(!http::securereq(req)) { res.error(403, "signing in needs https, localhost or a trusted proxy"); return; }
        if(req.reply == HTTP_REPLY_RELAY && httprelay < 2) { res.error(403, "this server doesn't allow signing in through the relay"); return; } // the token would cross the master
        jsonvalue *v = bodyjson(req);
        const char *name = field(req, v, "name"), *answer = field(req, v, "answer"), *idstr = field(req, v, "id"), *err = NULL;
        pendingauth *p = NULL;
        if(answer && idstr) err = answerchallenge(uint(strtoul(idstr, NULL, 10)), answer, req.ip, -1, false, &p);
        else if(name && *name) err = startchallenge(name, req.ip, req.addr, -1, false, &p);
        else err = "send a name to get a challenge, then the id and answer";
        if(err) res.error(!strcmp(err, "not connected to the master server") ? 503 : (!strcmp(err, "no such challenge") ? 404 : (!strncmp(err, "too many", 8) ? 429 : 400)), err);
        else
        {
            p->deferred = http::deferred(req);
            res.defer();
        }
        DELETEP(v);
    }

    // the root is kept for web pages, for now it only says where the Web API is
    void routeroot(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("name", "Red Eclipse");
        w.str("api", HTTP_API);
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    // what this server is and the routes the caller may use, read from the route table so the master's and script routes are there too
    void routeindex(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("name", "Red Eclipse");
        w.utf8("version", getverstr());
        w.str("type", masterserver ? "master" : "server");
        w.beginarray("routes");
        // one entry a path, as the relay takes every method on its paths; a socket's method is WS
        const vector<httproute> &list = http::routelist();
        int priv = max(req.privilege, int(HTTP_PUBLIC));
        loopv(list)
        {
            const httproute &r = list[i];
            if(r.minpriv > priv) continue;
            bool seen = false;
            loopj(i) if(list[j].minpriv <= priv && !strcmp(list[j].pattern, r.pattern)) { seen = true; break; }
            if(seen) continue;
            w.beginobject();
            w.str("path", r.pattern);
            w.beginobject("methods"); // each method and the privilege it needs
            for(int j = i; j < list.length(); j++) if(list[j].minpriv <= priv && !strcmp(list[j].pattern, r.pattern))
            {
                string name;
                privname(name, list[j].minpriv);
                w.str(list[j].ws ? "WS" : http::methodnames[clamp(list[j].method, 0, HTTP_MAXMETHODS-1)], name);
            }
            w.endobject();
            w.endobject();
        }
        w.endarray();
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    void routestatus(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        string desc, mode;
        filterstring(desc, getserverdesc(), true, true, true, false);
        filterstring(mode, gamename(gamemode, mutators), true, true, true, false);
        w.str("description", desc);
        w.utf8("version", getverstr());
        writeabout(w);
        w.str("map", smapname);
        writemapinfo(w);
        w.str("mode", mode);
        w.num("modeid", gamemode);
        w.num("mutators", mutators);
        w.num("players", numclients(-1, true, A_PLAYER));
        w.num("spectators", numspectators());
        w.num("bots", numclients(-1, false, A_BOT));
        w.num("maxplayers", maxslots());
        w.str("mastermode", mastermodename(mastermode));
        w.str("state", G_S_STR[clamp(gamestate, 0, G_S_MAX-1)]);
        w.num("timeleft", timeremaining >= 0 ? timeremaining/1000 : -1);
        w.num("relay", httprelay);
        if(req.privilege > HTTP_PUBLIC)
        {
            string priv;
            privname(priv, req.privilege);
            w.str("you", priv);
        }
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    void routeplayers(httprequest &req, httpresponse &res)
    {
        bool showip = req.privilege >= G(iphostlock);
        vector<char> buf;
        jsonwriter w(buf);
        w.beginarray();
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            string priv;
            privname(priv, ci->privilege);
            w.beginobject();
            w.num("cn", ci->clientnum);
            w.str("name", ci->name);
            w.str("team", T_STR[clamp(ci->team, 0, T_MAX-1)]);
            w.str("state", CS_STR[clamp(int(ci->state), 0, CS_MAX-1)]);
            w.boolean("bot", ci->actortype > A_PLAYER);
            w.str("privilege", priv);
            if(ci->handle[0]) w.str("handle", ci->handle);
            w.num("points", ci->points);
            w.num("frags", ci->frags);
            w.num("deaths", ci->deaths);
            w.num("ping", ci->ping);
            if(showip && ci->actortype == A_PLAYER) w.str("ip", gethostip(ci->clientnum));
            w.endobject();
        }
        w.endarray();
        res.json(buf.getbuf(), buf.length());
    }

    // the PNG as it is; asked for by the map's crc it never changes, so caches along the way may keep it
    void routemapshot(httprequest &req, httpresponse &res)
    {
        if(!hasmapshot()) { res.error(404, "this server has no preview of its map"); return; }
        const char *crc = req.var("crc");
        defformatstring(want, "%u", uint(smapcrc));
        if(crc && strcmp(crc, want)) { res.error(404, "the map has changed"); return; }
        defformatstring(etag, "\"%08x\"", uint(smapcrc));
        res.headers.set("ETag", etag);
        res.headers.set("Cache-Control", crc ? "public, max-age=86400, immutable" : "no-cache");
        const char *match = req.headers.get("If-None-Match");
        if(match && !strcmp(match, etag)) { res.status = 304; return; }
        stream *f = mapdata[SENDMAP_PNG];
        stream::offset pos = f->tell(), size = f->size();
        if(size <= 0 || size > HTTP_INMEMORY*4) { res.error(404, "this server has no preview of its map"); return; }
        res.settype("image/png");
        res.body.setsize(0);
        res.body.reserve(int(size));
        f->seek(0, SEEK_SET);
        res.body.advance(int(f->read(res.body.getbuf(), size_t(size))));
        f->seek(pos, SEEK_SET);
        if(res.body.length() != int(size)) { res.body.setsize(0); res.error(500, "couldn't read the map's preview"); }
    }

    void routeteams(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginarray();
        if(m_team(gamemode, mutators)) loopv(scores)
        {
            score &ts = scores[i];
            if(ts.team < 0 || ts.team >= T_MAX) continue;
            w.beginobject();
            w.str("team", T_STR[ts.team]);
            w.num("score", ts.total);
            w.endobject();
        }
        w.endarray();
        res.json(buf.getbuf(), buf.length());
    }

    void routecontrols(httprequest &req, httpresponse &res)
    {
        bool showip = req.privilege >= G(iphostlock);
        vector<char> buf;
        jsonwriter w(buf);
        w.beginarray();
        loopv(control)
        {
            ipinfo &c = control[i];
            if(c.type < 0 || c.type >= ipinfo::SYNCTYPES || req.privilege < controllock(c.type)) continue;
            static const char * const flags[] = { "temporary", "internal", "local", "global" };
            static const char * const sources[] = { "server", "player", "automatic", "master" };
            w.beginobject();
            w.num("id", c.id);
            w.str("type", controlname(c.type));
            w.str("scope", flags[clamp(c.flag, 0, 3)]);
            w.str("source", sources[clamp(c.source, 0, ipinfo::SRC_MAX-1)]);
            if(c.name[0]) w.str("name", c.name);
            if(c.handle[0]) w.str("handle", c.handle);
            if(c.addname[0]) w.str("by", c.addname);
            if(c.reason && *c.reason) w.str("reason", c.reason);
            if(showip)
            {
                string ip;
                ENetAddress a;
                a.host = c.ip;
                if(enet_address_get_host_ip(&a, ip, sizeof(ip)) >= 0) w.str("ip", ip);
                int bits = maskbits(c.mask);
                if(bits < 32) w.num("mask", bits);
            }
            w.endobject();
        }
        w.endarray();
        res.json(buf.getbuf(), buf.length());
    }

    static void actorname(httprequest &req, char *name)
    {
        string priv;
        privname(priv, req.privilege);
        nformatstring(name, MAXSTRLEN, "%s (%s, web)", req.handle && *req.handle ? req.handle : "someone", priv);
    }

    // mirrors N_ADDCONTROL, with a web user in place of a player
    void routeaddcontrol(httprequest &req, httpresponse &res)
    {
        jsonvalue *v = bodyjson(req);
        const char *type = field(req, v, "type"), *cnstr = field(req, v, "cn"), *reason = field(req, v, "reason");
        int value = -2;
        if(type)
        {
            if(!strcasecmp(type, "kick")) value = -1;
            else loopi(ipinfo::SYNCTYPES) if(!strcasecmp(type, controlname(i))) value = i;
        }
        clientinfo *cp = cnstr ? (clientinfo *)getinfo(atoi(cnstr)) : NULL;
        DELETEP(v);
        if(value < -1) { res.error(400, "type must be kick, allow, ban, mute, limit or except"); return; }
        if(!cp || cp->ownernum >= 0) { res.error(404, "no such player"); return; }
        int lock = value < 0 ? G(kicklock) : controllock(value);
        if(req.privilege < lock || (value != ipinfo::EXCEPT && req.privilege < (cp->local ? int(PRIV_ADMINISTRATOR) : (cp->privilege&PRIV_TYPE)))) { res.error(403, "insufficient privilege"); return; }
        uint ip = getclientip(cp->clientnum);
        if(!ip) { res.error(409, "that player has no address"); return; }
        if(checkipinfo(control, ipinfo::EXCEPT, ip))
        {
            if(req.privilege < PRIV_ADMINISTRATOR) { res.error(403, "that player is protected"); return; }
            if(value >= ipinfo::BAN) loopvrev(control) if(control[i].type == ipinfo::EXCEPT && (ip & control[i].mask) == control[i].ip) control.remove(i);
        }
        string name;
        actorname(req, name);
        string text = "";
        if(reason) filterstring(text, reason, false, true, true, true);
        if(value >= 0)
        {
            ipinfo &c = control.add();
            c.ip = ip;
            c.mask = 0xFFFFFFFFU;
            c.type = value;
            c.time = totalmillis ? totalmillis : 1;
            c.reason = newstring(text);
            setcontrolsource(c, ipinfo::SRC_PLAYER, NULL, cp);
            copystring(c.addname, req.handle && *req.handle ? req.handle : "web");
            copystring(c.addhandle, req.handle ? req.handle : "");
            c.addip = req.ip;
            c.addpriv = req.privilege;
            if(text[0]) srvoutf(3, colouryellow, "%s added \fs\fc%s\fS on %s: %s", name, controlname(value), colourname(cp), text);
            else srvoutf(3, colouryellow, "%s added \fs\fc%s\fS on %s", name, controlname(value), colourname(cp));
            if(value == ipinfo::BAN) updatecontrols = true;
            else if(value == ipinfo::LIMIT) cp->swapteam = 0;
            controlchanged(c, true, name);
        }
        else
        {
            if(text[0]) srvoutf(3, colouryellow, "%s \fs\fckicked\fS %s: %s", name, colourname(cp), text);
            else srvoutf(3, colouryellow, "%s \fs\fckicked\fS %s", name, colourname(cp));
            cp->kicked = updatecontrols = true;
        }
        res.status = 201;
        res.json("{\"ok\":true}");
    }

    void routedelcontrol(httprequest &req, httpresponse &res)
    {
        int id = atoi(req.param("id", "0"));
        loopv(control) if(control[i].id == id)
        {
            ipinfo &c = control[i];
            if(c.type < 0 || c.type >= ipinfo::SYNCTYPES) break;
            if(c.flag == ipinfo::GLOBAL) { res.error(403, "that comes from the master server and can't be removed here"); return; }
            if(req.privilege < controllock(c.type)) { res.error(403, "insufficient privilege"); return; }
            string name;
            actorname(req, name);
            if(c.name[0]) srvoutf(3, colouryellow, "%s removed \fs\fc%s\fS on %s", name, controlname(c.type), c.name);
            else srvoutf(3, colouryellow, "%s removed a \fs\fc%s\fS", name, controlname(c.type));
            if(c.type == ipinfo::EXCEPT) updatecontrols = true;
            controlchanged(c, false, name);
            control.remove(i);
            res.json("{\"ok\":true}");
            return;
        }
        res.error(404, "no such control entry");
    }

    void routemap(httprequest &req, httpresponse &res)
    {
        if(req.privilege < httpmaplock) { res.error(403, "insufficient privilege"); return; }
        jsonvalue *v = bodyjson(req);
        const char *map = field(req, v, "map"), *mode = field(req, v, "mode"), *muts = field(req, v, "mutators");
        string mapname = "";
        if(map) filterstring(mapname, map, false, true, false, true);
        int m = mode ? atoi(mode) : gamemode, mu = muts ? atoi(muts) : mutators;
        DELETEP(v);
        if(!mapname[0] || strpbrk(mapname, "./\\")) { res.error(400, "send a map name"); return; }
        // as a vote is: forced mutators added and the combination made valid, and modes for the local player refused
        mu = (mu & G_M_ALL) | G(mutslockforce);
        modecheck(m, mu);
        if(!m_game(m)) { res.error(400, "no such mode"); return; }
        if(m_local(m)) { res.error(403, "that mode is only for a local game"); return; }
        // the locks a vote is held to still apply when the vote is skipped, as they do for a veto
        if(req.privilege < votelocked(mapname, m, false)) { res.error(403, G(votelocktype) == 2 ? "that map was played recently" : "insufficient privilege to vote"); return; }
        if(req.privilege < modelocked(m, mu)) { res.error(403, "that game mode is locked"); return; }
        if(req.privilege < mapslocked(mapname, m, mu, false)) { res.error(403, "that map isn't in the rotation"); return; }
        string name;
        actorname(req, name);
        srvoutf(3, colouryellow, "%s changed the map to \fs\fc%s\fS", name, mapname);
        changemap(mapname, m, mu);
        res.json("{\"ok\":true}");
    }

    // web chat by address, under the same flood rule as in-game chat
    struct chatflood
    {
        uint ip;
        int start, count, warnings, warned;
    };
    vector<chatflood> chatfloods;

    // true when this message is one too many, warning and then muting the address as in-game chat does
    static bool chatflooded(httprequest &req)
    {
        if(!G(floodlock)) return false;
        loopvrev(chatfloods)
        {
            chatflood &c = chatfloods[i];
            if(totalmillis - c.start > G(floodtime) && (!c.warnings || totalmillis - c.warned >= G(floodwarnreset))) chatfloods.remove(i);
        }
        chatflood *f = NULL;
        loopv(chatfloods) if(chatfloods[i].ip == req.ip) { f = &chatfloods[i]; break; }
        if(!f)
        {
            f = &chatfloods.add();
            f->ip = req.ip;
            f->start = totalmillis;
            f->count = f->warnings = f->warned = 0;
        }
        else if(totalmillis - f->start > G(floodtime))
        {
            f->start = totalmillis;
            f->count = 0;
        }
        if(f->count < G(floodlines)) { f->count++; return false; }
        if(req.privilege < G(floodlock) && (!f->warned || totalmillis - f->warned >= G(floodwarntime)))
        {
            f->warnings++;
            f->warned = totalmillis ? totalmillis : 1;
            if(G(floodmute) && f->warnings >= G(floodmute) && req.ip && !req.local && req.privilege < G(mutelock) && !checkipinfo(control, ipinfo::EXCEPT, req.ip))
            {
                ipinfo &c = control.add();
                c.ip = req.ip;
                c.mask = 0xFFFFFFFFU;
                c.type = ipinfo::MUTE;
                c.flag = ipinfo::INTERNAL;
                c.time = totalmillis ? totalmillis : 1;
                c.reason = newstring("exceeded the number of allowed flood warnings");
                setcontrolsource(c, ipinfo::SRC_AUTO, NULL, NULL);
                string name;
                actorname(req, name);
                srvoutf(3, colourcyan, "mute added on %s: %s", name, c.reason);
                controlchanged(c, true, NULL);
            }
        }
        return true;
    }

    // mirrors N_TEXT's checks, with a web user in place of a player
    void routesay(httprequest &req, httpresponse &res)
    {
        if(req.privilege < httpsaylock || req.privilege < G(messagelock)) { res.error(403, "insufficient privilege"); return; }
        if(checkipinfo(control, ipinfo::MUTE, req.ip) && !checkipinfo(control, ipinfo::EXCEPT, req.ip) && req.privilege < G(mutelock)) { res.error(403, "you are muted"); return; }
        jsonvalue *v = bodyjson(req);
        const char *text = field(req, v, "text");
        bigstring msg = "";
        if(text) filterstring(msg, text, false, true, true, true, G(messagelength));
        DELETEP(v);
        if(!msg[0]) { res.error(400, "send some text"); return; }
        if(chatflooded(req))
        {
            res.error(429, "too many messages, slow down");
            res.headers.set("Retry-After", "1");
            return;
        }
        if(*(G(censorwords))) filterword(msg, G(censorwords));
        string name;
        actorname(req, name);
        srvoutf(3, colourwhite, "%s: %s", name, msg);
        webchat(req.handle && *req.handle ? req.handle : "", req.privilege, msg);
        res.json("{\"ok\":true}");
    }

    // requests and Event feed viewers the master relays over its link
    struct relayctx
    {
        int id;
    };

    static void relayreply(void *relay, httpresponse &res)
    {
        relayctx *r = (relayctx *)relay;
        if(!r) return;
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("relay", "response");
        w.num("id", r->id);
        w.num("status", res.status);
        w.beginobject("headers");
        static const char * const keep[] = { "Content-Type", "ETag", "Cache-Control", "Allow", "WWW-Authenticate", "Retry-After", NULL };
        for(int i = 0; keep[i]; i++) { const char *v = res.headers.get(keep[i]); if(v) w.utf8(keep[i], v); }
        w.endobject();
        // text goes in the JSON, anything else after it as it is in a binary frame
        const char *type = res.headers.get("Content-Type");
        bool binary = res.body.length() && type && strncmp(type, "text/", 5) && strncmp(type, "application/json", 16);
        if(!binary) w.utf8("body", res.body.getbuf(), res.body.length());
        w.endobject();
        if(binary)
        {
            buf.add('\0');
            buf.put(res.body.getbuf(), res.body.length());
        }
        masterlinksend(buf.getbuf(), buf.length(), binary);
        delete r;
    }

    static bool relaybanned(uint ip)
    {
        return ip && checkipinfo(control, ipinfo::BAN, ip) && !checkipinfo(control, ipinfo::EXCEPT, ip) && !checkipinfo(control, ipinfo::TRUST, ip);
    }

    static void linkjson(const char *data, int len)
    {
        jsonvalue *v = json::parse(data, len);
        if(!v) return;
        const char *type = v->getstr("relay", "");
        ENetAddress a;
        a.host = 0;
        const char *ipstr = v->getstr("ip");
        if(ipstr && enet_address_set_host_ip(&a, ipstr) < 0) a.host = 0;
        // the address as we read it, not the master's text, as it goes back out in the challenge
        string addr = "relay";
        if(a.host && enet_address_get_host_ip(&a, addr, sizeof(addr)) < 0) copystring(addr, "relay");
        if(!strcmp(type, "request"))
        {
            relayctx *r = new relayctx;
            r->id = v->getint("id", -1);
            httprequest req;
            req.reply = HTTP_REPLY_RELAY;
            req.relay = r;
            req.method = http::findmethod(v->getstr("method", "GET"));
            req.ip = a.host;
            req.secure = masterlinksecure(); // https to the master, then wss:// on the link; the master holds back tokens sent to it in the clear
            copystring(req.addr, addr);
            const char *path = v->getstr("path", "/"), *query = v->getstr("query");
            req.path = newstring(path);
            if(query && *query) { req.query = newstring(query); http::vardecode(query, req.vars); }
            jsonvalue *headers = v->get("headers");
            if(headers) loopv(headers->items) if(headers->items[i]->key && headers->items[i]->str) req.headers.add(headers->items[i]->key, headers->items[i]->str);
            const char *auth = req.headers.get("Authorization");
            if(auth && !strncasecmp(auth, "Bearer ", 7)) req.token = newstring(auth + 7);
            const char *origin = req.headers.get("Origin");
            if(origin) req.origin = newstring(origin);
            jsonvalue *body = v->get("body");
            if(body && body->str) req.body.put(body->str, strlen(body->str));
            const char *ctype = req.headers.get("Content-Type");
            if(ctype && !strncasecmp(ctype, "application/x-www-form-urlencoded", 33) && req.body.length()) { req.body.add('\0'); http::vardecode(req.body.getbuf(), req.vars); req.body.pop(); }
            if(httprelay < 1 || req.method == HTTP_NONE || path[0] != '/')
            {
                httpresponse res;
                res.error(httprelay < 1 ? 403 : 400, httprelay < 1 ? "this server doesn't allow relaying" : NULL);
                http::respond(req, res);
            }
            else http::dispatch(req);
        }
        else if(!strcmp(type, "feed")) relayfeed = v->getbool("on");
        else if(!strcmp(type, "viewer"))
        {
            int id = v->getint("id", -1);
            if(relaybanned(a.host) || httprelay < 1)
            {
                defformatstring(msg, "{\"relay\":\"deny\",\"id\":%d}", id);
                masterlinksend(msg, strlen(msg));
            }
            else if(!findsub(id, true))
            {
                addsub(id, true, masterlinksecure(), a.host, addr); // the master only lets an encrypted viewer sign in
                const char *token = v->getstr("token");
                subscriber *sub = findsub(id, true);
                if(sub && sub->secure && token && *token && httprelay >= 2) signin(*sub, token);
            }
        }
        else if(!strcmp(type, "viewermsg"))
        {
            subscriber *sub = findsub(v->getint("id", -1), true);
            jsonvalue *msg = v->get("data");
            if(sub && msg && msg->str) viewermessage(*sub, msg->str, strlen(msg->str));
        }
        else if(!strcmp(type, "viewergone")) dropsub(v->getint("id", -1), true);
        delete v;
    }

    static bool routesadded = false;
    void addroutes()
    {
        if(routesadded) return;
        routesadded = true;
        http::relayreply = relayreply;
        serverlinkjson = linkjson;
        http::addroute(HTTP_GET, "/", HTTP_PUBLIC, routeroot);
        http::addroute(HTTP_GET, HTTP_API, HTTP_PUBLIC, routeindex);
        http::addroute(HTTP_GET, HTTP_API "/status", HTTP_PUBLIC, routestatus);
        http::addroute(HTTP_GET, HTTP_API "/players", HTTP_PUBLIC, routeplayers);
        http::addroute(HTTP_GET, HTTP_API "/teams", HTTP_PUBLIC, routeteams);
        http::addroute(HTTP_GET, HTTP_API "/mapshot", HTTP_PUBLIC, routemapshot);
        http::addroute(HTTP_GET, HTTP_API "/matches", HTTP_PUBLIC, routematches);
        http::addroute(HTTP_GET, HTTP_API "/objectives", HTTP_PUBLIC, routeobjectives);
        http::addsocket(HTTP_API "/events", HTTP_PUBLIC, feedevent);
        http::addroute(HTTP_POST, HTTP_API "/auth", HTTP_PUBLIC, routeauth);
        // these need an account, so https, localhost or a trusted proxy, and the in-game lock decides who may do what they do
        http::addroute(HTTP_GET, HTTP_API "/controls", PRIV_PLAYER, routecontrols);
        http::addroute(HTTP_POST, HTTP_API "/controls", PRIV_PLAYER, routeaddcontrol);
        http::addroute(HTTP_DELETE, HTTP_API "/controls/:id", PRIV_PLAYER, routedelcontrol);
        http::addroute(HTTP_POST, HTTP_API "/map", PRIV_PLAYER, routemap);
        http::addroute(HTTP_POST, HTTP_API "/say", PRIV_PLAYER, routesay);
    }
}

void httpaccess(httprequest &req)
{
    webapi::addroutes();
    if(req.reply == HTTP_REPLY_RELAY && httprelay < 2) return; // tokens only cross the master if the operator allows it
    const char *token = req.token;
    if(!token && req.upgrade) token = req.var("token"); // browsers can't send headers on a WebSocket
    if(!token || !http::securereq(req)) return; // a request in the clear stays public, so public routes don't send it addresses
    if(webapi::authblocked(req.ip)) { req.throttled = true; return; }
    int privilege = HTTP_PUBLIC;
    string handle = "";
    if(webapi::lookup(token, privilege, handle))
    {
        req.privilege = max(privilege, int(HTTP_PUBLIC));
        DELETEA(req.handle);
        req.handle = newstring(handle);
        if(!req.token) req.token = newstring(token);
    }
    else webapi::authfailed(req.ip);
}
