// the game's side of the Web API: who a request is, its routes and the Event feed

VAR(0, httpsessiontime, 60000, 1800000, VAR_MAX); // how long a session token lasts unused
VAR(0, httpfeedchat, 0, 1, 1); // public chat goes to the Event feed

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
    };
    vector<pendingauth> pendings;
    uint nextauth = 0;

    struct subscriber
    {
        int id, privilege;
        string handle;
    };
    vector<subscriber> subscribers;

    void (*relayfeed)(const char *json, int len, int minpriv) = NULL; // set by the relay to pass events to the master

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
    }
    COMMANDN(0, addhttptoken, addtoken, "sss");
    ICOMMAND(0, clearhttptokens, "", (), tokens.shrink(0));

    static void cleansessions()
    {
        loopvrev(sessions) if(totalmillis - sessions[i].lastused > httpsessiontime) sessions.remove(i);
        loopvrev(pendings) if(totalmillis - pendings[i].started > 60000) pendings.remove(i);
    }

    static bool lookup(const char *token, int &privilege, char *handle)
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
            sessions[i].lastused = totalmillis;
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

    // everything here is something any player in the match could already see
    void publish(vector<char> &json, int minpriv = HTTP_PUBLIC)
    {
        if(relayfeed) relayfeed(json.getbuf(), json.length(), minpriv);
        if(subscribers.empty()) return;
        static vector<int> ids;
        ids.setsize(0);
        loopv(subscribers) if(subscribers[i].privilege >= minpriv) ids.add(subscribers[i].id);
        if(ids.length()) http::wsbroadcast(ids, json.getbuf(), json.length());
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

    void controlchanged(const ipinfo &c, bool added, const char *by)
    {
        if(!wanted()) return;
        vector<char> buf;
        jsonwriter w(buf);
        begin(w, "control");
        w.str("action", added ? "add" : "remove");
        w.str("type", controlname(c.type));
        w.num("id", c.id);
        if(c.name[0]) w.str("name", c.name);
        if(c.reason && *c.reason) w.str("reason", c.reason);
        if(by && *by) w.str("by", by);
        w.endobject();
        publish(buf, G(iphostlock) ? max(int(PRIV_MODERATOR), G(iphostlock)) : PRIV_MODERATOR);
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

    void checkscores()
    {
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

    void feedevent(httpwsevent &e, void *ctx)
    {
        switch(e.type)
        {
            case WSEV_OPEN:
            {
                subscriber &s = subscribers.add();
                s.id = e.id;
                s.privilege = e.req ? e.req->privilege : HTTP_PUBLIC;
                copystring(s.handle, e.req && e.req->handle ? e.req->handle : "");
                lastscores.shrink(0);
                memset(lastteamscores, -1, sizeof(lastteamscores));
                break;
            }
            case WSEV_CLOSE: loopv(subscribers) if(subscribers[i].id == e.id) { subscribers.remove(i); break; } break;
            case WSEV_MESSAGE:
            {
                // {"token": "..."} raises what this viewer sees, browsers can't send headers on a WebSocket
                jsonvalue *v = json::parse(e.data, e.len);
                const char *token = v ? v->getstr("token") : NULL;
                int priv = HTTP_PUBLIC;
                string handle;
                if(token && lookup(token, priv, handle)) loopv(subscribers) if(subscribers[i].id == e.id)
                {
                    subscribers[i].privilege = priv;
                    copystring(subscribers[i].handle, handle);
                    string name;
                    privname(name, priv);
                    defformatstring(reply, "{\"event\":\"auth\",\"privilege\":\"%s\"}", name);
                    http::wssend(e.id, reply);
                }
                DELETEP(v);
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
            httpresponse res;
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            w.num("id", double(id));
            w.str("challenge", val);
            w.endobject();
            res.json(buf.getbuf(), buf.length());
            http::reply(pendings[i].deferred, res);
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
        jsonvalue *v = bodyjson(req);
        const char *name = field(req, v, "name"), *answer = field(req, v, "answer"), *idstr = field(req, v, "id");
        if(!connectedmaster()) { res.error(503, "not connected to the master server"); DELETEP(v); return; }
        if(answer && idstr)
        {
            uint id = uint(strtoul(idstr, NULL, 10));
            pendingauth *p = NULL;
            loopv(pendings) if(pendings[i].id == id && pendings[i].ip == req.ip && pendings[i].deferred < 0) { p = &pendings[i]; break; }
            if(!p) { res.error(404, "no such challenge"); DELETEP(v); return; }
            string val;
            copystring(val, answer);
            for(char *s = val; *s; s++) if(!isxdigit(*s)) { *s = '\0'; break; }
            p->deferred = http::deferred(req);
            res.defer();
            requestmasterf("confauth %u %s\n", id, val);
        }
        else if(name && *name)
        {
            int count = 0;
            loopv(pendings) if(pendings[i].ip == req.ip) count++;
            if(count >= 4) { res.error(429, "too many challenges in progress"); DELETEP(v); return; }
            string clean;
            filterstring(clean, name, false, true, false, true);
            if(!clean[0] || strpbrk(clean, " \t\"")) { res.error(400, "bad name"); DELETEP(v); return; }
            pendingauth &p = pendings.add();
            if(!nextauth) nextauth = 1;
            p.id = WEBAUTH_ID | (nextauth++ & ~WEBAUTH_ID);
            p.ip = req.ip;
            copystring(p.name, clean);
            p.started = totalmillis;
            p.socket = -1;
            p.deferred = http::deferred(req);
            res.defer();
            requestmasterf("reqauth %u %s %s\n", p.id, p.name, req.addr);
        }
        else res.error(400, "send a name to get a challenge, then the id and answer");
        DELETEP(v);
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
        w.str("map", smapname);
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
        w.boolean("relay", false);
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
        bool showip = req.privilege >= max(int(PRIV_MODERATOR), G(iphostlock));
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
        bool showip = req.privilege >= max(int(PRIV_MODERATOR), G(iphostlock));
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
        if(req.privilege < max(G(modelock), G(mapslock))) { res.error(403, "insufficient privilege"); return; }
        jsonvalue *v = bodyjson(req);
        const char *map = field(req, v, "map"), *mode = field(req, v, "mode"), *muts = field(req, v, "mutators");
        string mapname = "";
        if(map) filterstring(mapname, map, false, true, false, true);
        int m = mode ? atoi(mode) : gamemode, mu = muts ? atoi(muts) : mutators;
        DELETEP(v);
        if(!mapname[0] || strpbrk(mapname, "./\\")) { res.error(400, "send a map name"); return; }
        if(!m_game(m)) { res.error(400, "no such mode"); return; }
        string name;
        actorname(req, name);
        srvoutf(3, colouryellow, "%s changed the map to \fs\fc%s\fS", name, mapname);
        changemap(mapname, m, mu);
        res.json("{\"ok\":true}");
    }

    void routesay(httprequest &req, httpresponse &res)
    {
        jsonvalue *v = bodyjson(req);
        const char *text = field(req, v, "text");
        string msg = "";
        if(text) filterstring(msg, text, false, true, true, true);
        DELETEP(v);
        if(!msg[0]) { res.error(400, "send some text"); return; }
        string name;
        actorname(req, name);
        srvoutf(3, colourwhite, "%s: %s", name, msg);
        res.json("{\"ok\":true}");
    }

    static bool registered = false;
    static void addroutes()
    {
        if(registered) return;
        registered = true;
        http::addroute(HTTP_GET, "/", HTTP_PUBLIC, routestatus);
        http::addroute(HTTP_GET, "/players", HTTP_PUBLIC, routeplayers);
        http::addroute(HTTP_GET, "/teams", HTTP_PUBLIC, routeteams);
        http::addsocket("/events", HTTP_PUBLIC, feedevent);
        http::addroute(HTTP_POST, "/auth", HTTP_PUBLIC, routeauth);
        // these need https, localhost or a trusted proxy, and check the in-game minimum for what they do
        http::addroute(HTTP_GET, "/controls", PRIV_SUPPORTER, routecontrols);
        http::addroute(HTTP_POST, "/controls", PRIV_SUPPORTER, routeaddcontrol);
        http::addroute(HTTP_DELETE, "/controls/:id", PRIV_SUPPORTER, routedelcontrol);
        http::addroute(HTTP_POST, "/map", PRIV_SUPPORTER, routemap);
        http::addroute(HTTP_POST, "/say", PRIV_MODERATOR, routesay);
    }
}

void httpaccess(httprequest &req)
{
    webapi::addroutes();
    const char *token = req.token;
    if(!token && req.upgrade) token = req.var("token"); // browsers can't send headers on a WebSocket
    if(!token) return;
    int privilege = HTTP_PUBLIC;
    string handle = "";
    if(webapi::lookup(token, privilege, handle))
    {
        req.privilege = max(privilege, int(HTTP_PUBLIC));
        DELETEA(req.handle);
        req.handle = newstring(handle);
        if(!req.token) req.token = newstring(token);
    }
}
