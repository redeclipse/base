// the game's side of the Web API: who a request is, its routes and the Event feed

VAR(0, httpsessiontime, 60000, 1800000, VAR_MAX); // how long a session token lasts unused
VAR(0, httpfeedchat, 0, 1, 1); // public chat goes to the Event feed
VAR(0, httprelay, 0, 1, 2); // what the master may relay: 0 nothing, 1 public routes and the Event feed, 2 everything
VAR(0, httpauthfails, 1, 10, VAR_MAX); // failed tokens or challenges an address may try in a minute

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
        uint ip;
        string handle, addr;
    };
    vector<subscriber> subscribers;

    bool relayfeed = false; // the master has viewers of our Event feed

    // failed tokens and challenges by address, so guessing tokens gets nowhere
    struct authfail
    {
        uint ip;
        int count, start;
    };
    vector<authfail> authfails;

    static bool authblocked(uint ip)
    {
        loopv(authfails) if(authfails[i].ip == ip)
        {
            if(totalmillis - authfails[i].start >= 60000) { authfails.remove(i); return false; }
            return authfails[i].count >= httpauthfails;
        }
        return false;
    }

    static void authfailed(uint ip)
    {
        loopv(authfails) if(authfails[i].ip == ip)
        {
            if(totalmillis - authfails[i].start >= 60000) { authfails[i].start = totalmillis; authfails[i].count = 0; }
            authfails[i].count++;
            return;
        }
        if(authfails.length() >= 4096) authfails.remove(0);
        authfail &f = authfails.add();
        f.ip = ip;
        f.count = 1;
        f.start = totalmillis;
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
        static vector<int> ids;
        ids.setsize(0);
        loopv(subscribers) if(subscribers[i].privilege >= minpriv) ids.add(subscribers[i].id);
        if(ids.length()) http::wsbroadcast(ids, json.getbuf(), json.length());
    }

    // only over the link, older masters answer anything unknown with an error
    void registered()
    {
        if(!masterlinked()) return;
        const char *fp = http::fingerprint();
        requestmasterf("webapi %d %d %s\n", http::serverport(), httprelay, *fp ? fp : "-");
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

    static void autherror(int socket, const char *err)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "auth");
        w.str("error", err);
        w.endobject();
        http::wssend(socket, buf.getbuf(), buf.length());
    }

    static void authorised(subscriber &sub, int priv, const char *handle)
    {
        sub.privilege = priv;
        copystring(sub.handle, handle);
        string name;
        privname(name, priv);
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "auth");
        w.str("privilege", name);
        if(handle && *handle) w.str("name", handle);
        w.endobject();
        http::wssend(sub.id, buf.getbuf(), buf.length());
    }

    // the first half of the master's challenge, for a deferred request or a WebSocket
    static const char *startchallenge(const char *name, uint ip, const char *addr, int socket, pendingauth **started = NULL)
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
        p.deferred = -1;
        requestmasterf("reqauth %u %s %s\n", p.id, p.name, addr && *addr ? addr : "-");
        if(started) *started = &p;
        return NULL;
    }

    static const char *answerchallenge(uint id, const char *answer, uint ip, int socket, pendingauth **answered = NULL)
    {
        if(!connectedmaster()) return "not connected to the master server";
        pendingauth *p = NULL;
        loopv(pendings) if(pendings[i].id == id && pendings[i].ip == ip && pendings[i].socket == socket && pendings[i].deferred < 0) { p = &pendings[i]; break; }
        if(!p) return "no such challenge";
        string val;
        copystring(val, answer);
        for(char *s = val; *s; s++) if(!isxdigit(*s)) { *s = '\0'; break; }
        requestmasterf("confauth %u %s\n", id, val);
        if(answered) *answered = p;
        return NULL;
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
                s.ip = e.req ? e.req->ip : 0;
                copystring(s.handle, e.req && e.req->handle ? e.req->handle : "");
                copystring(s.addr, e.req ? e.req->addr : "");
                lastscores.shrink(0);
                memset(lastteamscores, -1, sizeof(lastteamscores));
                break;
            }
            case WSEV_CLOSE:
                loopv(subscribers) if(subscribers[i].id == e.id) { subscribers.remove(i); break; }
                loopvrev(pendings) if(pendings[i].socket == e.id) pendings.remove(i);
                break;
            case WSEV_MESSAGE:
            {
                // browsers can't send headers on a WebSocket, so a viewer raises what it sees with
                // {"token": "..."}, or {"name": "..."} and then {"id": ..., "answer": "..."} for the master's challenge
                jsonvalue *v = json::parse(e.data, e.len);
                subscriber *sub = NULL;
                loopv(subscribers) if(subscribers[i].id == e.id) { sub = &subscribers[i]; break; }
                if(!v || !sub) { DELETEP(v); break; }
                const char *token = v->getstr("token"), *name = v->getstr("name"), *answer = v->getstr("answer");
                if(token)
                {
                    int priv = HTTP_PUBLIC;
                    string handle;
                    if(authblocked(sub->ip)) autherror(e.id, "too many failed attempts, try again in a minute");
                    else if(lookup(token, priv, handle)) authorised(*sub, priv, handle);
                    else { authfailed(sub->ip); autherror(e.id, "unknown token"); }
                }
                else if(answer) { const char *err = answerchallenge(uint(v->getnum("id", 0)), answer, sub->ip, e.id); if(err) autherror(e.id, err); }
                else if(name) { const char *err = startchallenge(name, sub->ip, sub->addr, e.id); if(err) autherror(e.id, err); }
                delete v;
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
            if(pendings[i].socket >= 0) http::wssend(pendings[i].socket, buf.getbuf(), buf.length());
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
                if(!name || n <= PRIV_NONE) { autherror(p.socket, "identity verification failed"); return true; }
                loopvj(subscribers) if(subscribers[j].id == p.socket) { authorised(subscribers[j], n, name); break; }
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
        jsonvalue *v = bodyjson(req);
        const char *name = field(req, v, "name"), *answer = field(req, v, "answer"), *idstr = field(req, v, "id"), *err = NULL;
        pendingauth *p = NULL;
        if(answer && idstr) err = answerchallenge(uint(strtoul(idstr, NULL, 10)), answer, req.ip, -1, &p);
        else if(name && *name) err = startchallenge(name, req.ip, req.addr, -1, &p);
        else err = "send a name to get a challenge, then the id and answer";
        if(err) res.error(!strcmp(err, "not connected to the master server") ? 503 : (!strcmp(err, "no such challenge") ? 404 : (!strncmp(err, "too many", 8) ? 429 : 400)), err);
        else
        {
            p->deferred = http::deferred(req);
            res.defer();
        }
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
        w.utf8("body", res.body.getbuf(), res.body.length());
        w.endobject();
        masterlinksend(buf.getbuf(), buf.length());
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
        if(ipstr) enet_address_set_host_ip(&a, ipstr);
        if(!strcmp(type, "request"))
        {
            relayctx *r = new relayctx;
            r->id = int(v->getnum("id", -1));
            httprequest req;
            req.reply = HTTP_REPLY_RELAY;
            req.relay = r;
            req.method = http::findmethod(v->getstr("method", "GET"));
            req.ip = a.host;
            req.secure = true; // https to the master, then the link
            copystring(req.addr, ipstr ? ipstr : "relay");
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
        else if(!strcmp(type, "viewer") && (relaybanned(a.host) || httprelay < 1))
        {
            defformatstring(msg, "{\"relay\":\"deny\",\"id\":%d}", int(v->getnum("id", -1)));
            masterlinksend(msg, strlen(msg));
        }
        delete v;
    }

    static bool routesadded = false;
    void addroutes()
    {
        if(routesadded) return;
        routesadded = true;
        http::relayreply = relayreply;
        serverlinkjson = linkjson;
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
    if(req.reply == HTTP_REPLY_RELAY && httprelay < 2) return; // tokens only cross the master if the operator allows it
    const char *token = req.token;
    if(!token && req.upgrade) token = req.var("token"); // browsers can't send headers on a WebSocket
    if(!token) return;
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
