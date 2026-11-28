#include "engine.h"
#include "master.h"

#define SIGNIN_VERSION 1
#define SIGNIN_MAXSTORED 65536
#define SIGNIN_COOKIE "re_signin"
#define SIGNIN_LABEL 64
#define SIGNIN_CHALLENGES 4 // in progress for each address
#define SIGNIN_CHALTIME 60000
#define SIGNIN_EXCHANGES 8 // waiting on servers for each sign-in
#define SIGNIN_EXCHANGETIME 10000

namespace master
{
    VAR(0, mastersignindays, 0, 182, 3650); // how long a remembered sign-in lasts at most, 0 turns them off
    VAR(0, mastersigninidle, 0, 91, 3650); // days one lasts unused, 0 for no limit
    VAR(0, mastersigninprivdays, 0, 28, 3650); // the same for moderators and above, 0 gives them none
    VAR(0, mastersigninprividle, 0, 14, 3650);
    VAR(0, mastersigninbrief, 1, 1, 3650); // days one that isn't remembered lasts unused, its cookie ending with the browser
    VAR(0, mastersigninmax, 1, 10, 100); // each account's, the least recently used goes first
    VAR(0, mastersigninfails, 1, 10, VAR_MAX); // failed sign-ins and unknown tokens an address may try in a minute
    SVAR(0, mastersigninorigin, ""); // origins whose pages may keep a sign-in as a cookie, such as https://redeclipse.net
    SVAR(0, mastersigninfile, "mastersignin.dat");

    static vector<signin *> signins;
    bool signindirty = false, signincheck = false;

    struct signinchallenge
    {
        uint id, ip;
        int started;
        string name, key, challenge;
    };
    static vector<signinchallenge> signinchallenges;
    static uint lastsigninchallenge = 0;

    struct signinexchange
    {
        uint id;
        int link, deferred, started;
        string signin, name;
    };
    static vector<signinexchange> signinexchanges;
    static uint lastsigninexchange = 0;

    struct signinfail
    {
        uint ip;
        int count, start;
    };
    static vector<signinfail> signinfails;

    static bool signinblocked(uint ip)
    {
        loopv(signinfails) if(signinfails[i].ip == ip)
        {
            if(totalmillis - signinfails[i].start >= 60000) { signinfails.remove(i); return false; }
            return signinfails[i].count >= mastersigninfails;
        }
        return false;
    }

    // unlike a game server's, the master only turns an address away for the minute, as a ban would shut it out of the relay and the list for everyone on it
    void signinfailed(uint ip)
    {
        signinfail *f = NULL;
        loopv(signinfails) if(signinfails[i].ip == ip) { f = &signinfails[i]; break; }
        if(!f)
        {
            if(signinfails.length() >= 4096) signinfails.remove(0);
            f = &signinfails.add();
            f->ip = ip;
            f->count = 0;
            f->start = totalmillis;
        }
        if(totalmillis - f->start >= 60000) { f->start = totalmillis; f->count = 0; }
        f->count++;
    }

    static bool signinorigin(const char *origin)
    {
        if(!origin || !*origin) return false;
        int olen = strlen(origin);
        for(const char *p = mastersigninorigin; *p;)
        {
            p += strspn(p, ", \t");
            int len = strcspn(p, ", \t");
            if(len == olen && !strncasecmp(p, origin, len)) return true;
            p += len;
        }
        return false;
    }

    void hexbytes(char *dst, const uchar *src, int len)
    {
        loopi(len) { dst[i*2] = "0123456789abcdef"[src[i]>>4]; dst[i*2+1] = "0123456789abcdef"[src[i]&15]; }
        dst[len*2] = '\0';
    }

    void hashtoken(char *dst, const char *token)
    {
        uchar digest[32];
        http::sha256((const uchar *)token, strlen(token), digest);
        hexbytes(dst, digest, 32);
    }

    // moderators and above, whose stolen sign-in would do the most harm
    static bool signinhigh(const char *flags)
    {
        for(const char *c = flags; *c; c++) if(strchr("mMoOaAdDcC", *c)) return true;
        return false;
    }

    static time_t signinidle(signin &s)
    {
        if(!s.remember) return time_t(mastersigninbrief)*SIGNIN_DAY;
        int days = signinhigh(s.flags) ? mastersigninprividle : mastersigninidle;
        return days ? time_t(days)*SIGNIN_DAY : 0;
    }

    static time_t signinlife(const char *flags)
    {
        return time_t(signinhigh(flags) ? mastersigninprivdays : mastersignindays)*SIGNIN_DAY;
    }

    // the account as it stands, unless a rehash removed it or changed its key or flags
    static authuser *signinaccount(signin &s)
    {
        authuser *u = authusers.access(s.name);
        return u && u->key && !strcmp(u->key, s.key) && u->flags && userflag(u->flags) == userflag(s.flags) ? u : NULL;
    }

    static bool signinlapsed(signin &s, time_t now)
    {
        if(now >= s.expires) return true;
        time_t idle = signinidle(s);
        if(idle && now - s.lastused >= idle) return true;
        // a lower limit than it was made under applies to it too
        time_t life = signinlife(s.flags);
        return !life || now - s.created >= life;
    }

    // its Sessions end with it, on the master and every server linked now; those that aren't ended theirs when their link closed
    static void signinunvouch(const char *id)
    {
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.link >= 0 && c.vouch) masteroutf(c, "unvouch %s\n", id);
        }
        server::localunvouch(id); // and those on the master itself
    }

    static void signindrop(int n, const char *why)
    {
        signin *s = signins[n];
        conoutf(colourwhite, "Remembered sign-in %s for '%s' ended (%s)", s->id, s->name, why);
        signinunvouch(s->id);
        loopvrev(signinexchanges) if(!strcmp(signinexchanges[i].signin, s->id))
        {
            httpresponse res;
            res.error(401, "unknown or expired token");
            http::reply(signinexchanges[i].deferred, res);
            signinexchanges.remove(i);
        }
        delete s;
        signins.remove(n);
        signindirty = true;
    }

    static masterfileguard signinguard;

    static int readsignins(const char *name, int)
    {
        signins.deletecontents();
        stream *f = opengzfile(name, "rb");
        if(!f) return masterfilemissing(name);
        char magic[4];
        bool ok = f->read(magic, 4) == 4 && !memcmp(magic, "RESI", 4);
        if(ok && f->getlil<int>() != SIGNIN_VERSION) return masterfileolder(f, name);
        int count = ok ? f->getlil<int>() : 0;
        if(count < 0 || count > SIGNIN_MAXSTORED) ok = false;
        for(int i = 0; ok && i < count; i++)
        {
            signin *s = new signin;
            string *strs[8] = { &s->id, &s->hash, &s->name, &s->key, &s->flags, &s->label, &s->firstaddr, &s->lastaddr };
            loopj(8)
            {
                int len = f->getlil<int>();
                if(len < 0 || len >= MAXSTRLEN || (len && f->read(*strs[j], len) != size_t(len))) { ok = false; break; }
                (*strs[j])[len] = '\0';
            }
            if(!ok) { delete s; break; }
            s->created = time_t(f->getlil<int64_t>());
            s->lastused = time_t(f->getlil<int64_t>());
            s->expires = time_t(f->getlil<int64_t>());
            s->remember = f->getchar() != 0;
            if(!s->id[0] || strlen(s->hash) != 64) { delete s; continue; }
            signins.add(s);
        }
        delete f;
        if(!ok) { signins.deletecontents(); return 1; }
        if(signins.length()) conoutf(colourwhite, "Loaded %d remembered sign-ins from %s", signins.length(), name);
        return 0;
    }

    static void loadsignins()
    {
        signins.deletecontents();
        if(!*mastersigninfile) return;
        if(loadguarded(mastersigninfile, readsignins, 0, signinguard, "the master restarts"))
        {
            foldnoticef("master", NOTICE_WARNING, tempformatstring("file %s bak", mastersigninfile), "Remembered sign-ins ended since %s.bak was saved may be back", mastersigninfile);
            signindirty = true;
        }
        signincheck = true;
    }

    void savesignins()
    {
        signindirty = false;
        if(!*mastersigninfile || saveblocked(signinguard, mastersigninfile)) return;
        defformatstring(newname, "%s.new", mastersigninfile);
        stream *f = openmasterfile(newname, true);
        if(!f) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s save", newname), "Failed to save remembered sign-ins to %s", newname); return; }
        f->write("RESI", 4);
        f->putlil<int>(SIGNIN_VERSION);
        f->putlil<int>(signins.length());
        loopv(signins)
        {
            signin &s = *signins[i];
            const char *strs[8] = { s.id, s.hash, s.name, s.key, s.flags, s.label, s.firstaddr, s.lastaddr };
            loopj(8)
            {
                int len = strlen(strs[j]);
                f->putlil<int>(len);
                if(len) f->write(strs[j], len);
            }
            f->putlil<int64_t>(int64_t(s.created));
            f->putlil<int64_t>(int64_t(s.lastused));
            f->putlil<int64_t>(int64_t(s.expires));
            f->putchar(s.remember ? 1 : 0);
        }
        delete f;
        commitmasterfile(mastersigninfile, signinguard, false); // only its .bak, as dated copies would keep tokens that ended
    }

    // once a second: lapsed sign-ins, accounts a rehash changed, challenges and exchanges that got no answer
    void checksignins()
    {
        time_t now = time(NULL);
        loopvrev(signins)
        {
            signin &s = *signins[i];
            if(signincheck && !signinaccount(s)) signindrop(i, "the account changed");
            else if(signinlapsed(s, now)) signindrop(i, "expired");
        }
        signincheck = false;
        loopvrev(signinchallenges) if(totalmillis - signinchallenges[i].started >= SIGNIN_CHALTIME)
        {
            signinchallenges.remove(i);
        }
        loopvrev(signinexchanges) if(totalmillis - signinexchanges[i].started >= SIGNIN_EXCHANGETIME)
        {
            httpresponse res;
            res.error(504, "that server didn't answer");
            http::reply(signinexchanges[i].deferred, res);
            signinexchanges.remove(i);
        }
        if(signindirty) savesignins();
    }

    // the sign-in a request carries, as a cookie from an allowed page or a Bearer token from a tool
    signin *findsignin(httprequest &req, bool &tried)
    {
        string token = "";
        if(!req.cookie(SIGNIN_COOKIE, token, sizeof(token)) && req.token) copystring(token, req.token);
        tried = token[0] != '\0';
        if(!tried) return NULL;
        string hash;
        hashtoken(hash, token);
        time_t now = time(NULL);
        loopv(signins) if(!strcmp(signins[i]->hash, hash))
        {
            signin &s = *signins[i];
            if(signinlapsed(s, now)) { signindrop(i, "expired"); return NULL; }
            if(!signinaccount(s)) { signindrop(i, "the account changed"); return NULL; }
            s.lastused = now;
            copystring(s.lastaddr, req.addr);
            signindirty = true;
            return &s;
        }
        return NULL;
    }

    // whether a token is one of the master's own sign-ins, without counting it as used
    bool signinknown(const char *token)
    {
        if(!token || !*token) return false;
        string hash;
        hashtoken(hash, token);
        loopv(signins) if(!strcmp(signins[i]->hash, hash)) return true;
        return false;
    }

    // a sign-in only counts over https, localhost or a trusted proxy, as one sent in the clear has been seen on the way
    int requestpriv(httprequest &req)
    {
        if(!http::securereq(req)) return 0;
        bool tried = false;
        signin *s = findsignin(req, tried);
        return s ? accountpriv(s->flags) : 0;
    }

    static void setsignincookie(httpresponse &res, const char *token, time_t maxage)
    {
        string cookie;
        if(maxage > 0) formatstring(cookie, "%s=%s; Path=%s; Max-Age=%d; Secure; HttpOnly; SameSite=Strict", SIGNIN_COOKIE, token, SIGNIN_PATH, int(maxage));
        else if(!maxage) formatstring(cookie, "%s=%s; Path=%s; Secure; HttpOnly; SameSite=Strict", SIGNIN_COOKIE, token, SIGNIN_PATH);
        else formatstring(cookie, "%s=; Path=%s; Max-Age=0; Secure; HttpOnly; SameSite=Strict", SIGNIN_COOKIE, SIGNIN_PATH);
        res.headers.set("Set-Cookie", cookie);
    }

    // what a route answers when the sign-in is missing or no longer stands, counting the failure
    bool signinrefused(httprequest &req, httpresponse &res, signin *s, bool tried)
    {
        if(s) return false;
        if(tried)
        {
            signinfailed(req.ip);
            res.error(401, "unknown or expired token");
            res.headers.set("WWW-Authenticate", "Bearer error=\"invalid_token\"");
            if(req.credentialed) setsignincookie(res, NULL, -1);
        }
        else
        {
            res.error(401, "authentication required");
            res.headers.set("WWW-Authenticate", "Bearer");
        }
        return true;
    }

    bool signinready(httprequest &req, httpresponse &res)
    {
        res.headers.set("Cache-Control", "no-store");
        if(!http::securereq(req)) { res.error(403, "signing in needs https, localhost or a trusted proxy"); return false; }
        if(signinblocked(req.ip)) { res.error(429, "too many failed attempts, try again in a minute"); res.headers.set("Retry-After", "60"); return false; }
        return true;
    }

    // a label people can tell their devices apart by, cut on a whole character
    static void signinlabel(char *dst, const char *src)
    {
        int n = 0;
        for(const uchar *c = (const uchar *)src; *c && n < SIGNIN_LABEL; c++) if(*c >= 0x20 && *c != 0x7F) dst[n++] = *c;
        if(src[n] && n) while(n > 0 && (uchar(dst[n-1])&0xC0) == 0x80) n--;
        if(n > 0 && (uchar(dst[n-1])&0xC0) == 0xC0) n--;
        dst[n] = '\0';
    }

    static void writesignin(jsonwriter &w, signin &s, signin *current)
    {
        w.beginobject();
        w.utf8("id", s.id);
        w.utf8("name", s.name);
        w.utf8("label", s.label);
        w.num("created", double(s.created));
        w.num("lastused", double(s.lastused));
        time_t idle = signinidle(s), expires = s.expires;
        if(idle && s.lastused + idle < expires) expires = s.lastused + idle;
        w.num("expires", double(expires));
        w.utf8("firstaddr", s.firstaddr);
        w.utf8("lastaddr", s.lastaddr);
        w.boolean("remember", s.remember);
        if(&s == current) w.boolean("current", true);
        w.endobject();
    }

    jsonvalue *signinbody(httprequest &req)
    {
        return req.body.length() ? json::parse(req.body.getbuf(), req.body.length()) : NULL;
    }

    const char *signinfield(httprequest &req, jsonvalue *v, const char *name)
    {
        const char *s = v ? v->getstr(name) : NULL;
        return s ? s : req.var(name);
    }

    bool signinflag(httprequest &req, jsonvalue *v, const char *name, bool def)
    {
        jsonvalue *f = v ? v->get(name) : NULL;
        if(f) return f->istrue();
        const char *s = req.var(name);
        return s ? (*s && strcmp(s, "0") && strcasecmp(s, "false")) : def;
    }

    static void signincreated(httprequest &req, httpresponse &res, authuser &u, bool remember, const char *label, bool cookie)
    {
        time_t now = time(NULL), life = signinlife(u.flags);
        if(!life)
        {
            res.error(403, mastersignindays ? "accounts at this standing can't be remembered, sign in on each server instead" : "remembered sign-ins are turned off");
            return;
        }
        // the account's limit, the least recently used going first
        int count = 0, oldest = -1;
        loopv(signins) if(!strcmp(signins[i]->name, u.name))
        {
            count++;
            if(oldest < 0 || signins[i]->lastused < signins[oldest]->lastused) oldest = i;
        }
        if(count >= mastersigninmax && oldest >= 0) signindrop(oldest, "the account has too many");
        signin *s = new signin;
        uchar bytes[32];
        http::randombytes(bytes, 8);
        hexbytes(s->id, bytes, 8);
        string token;
        http::randombytes(bytes, 32);
        hexbytes(token, bytes, 32);
        hashtoken(s->hash, token);
        copystring(s->name, u.name);
        copystring(s->key, u.key);
        copystring(s->flags, u.flags);
        signinlabel(s->label, label && *label ? label : req.headers.get("User-Agent", ""));
        copystring(s->firstaddr, req.addr);
        copystring(s->lastaddr, req.addr);
        s->created = s->lastused = now;
        s->expires = now + life;
        s->remember = remember;
        signins.add(s);
        signindirty = true;
        conoutf(colourwhite, "Remembered sign-in %s for '%s' [%s] from %s%s", s->id, s->name, s->flags, req.addr, remember ? "" : " (until the browser closes)");
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        if(!req.credentialed || !cookie) w.utf8("token", token);
        w.utf8("id", s->id);
        w.utf8("name", s->name);
        w.utf8("flags", s->flags);
        w.num("expires", double(s->expires));
        w.endobject();
        if(req.credentialed && cookie) setsignincookie(res, token, remember ? s->expires - now : 0);
        res.json(buf.getbuf(), buf.length());
    }

    // POST /signin: a name gives a challenge, its answer a remembered sign-in
    static void routesignin(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *name = signinfield(req, v, "name"), *answer = signinfield(req, v, "answer"), *idstr = signinfield(req, v, "id");
        if(answer && idstr)
        {
            uint id = uint(strtoul(idstr, NULL, 10));
            int found = -1;
            loopv(signinchallenges) if(signinchallenges[i].id == id && signinchallenges[i].ip == req.ip) { found = i; break; }
            if(found < 0) res.error(404, "no such challenge");
            else
            {
                signinchallenge c = signinchallenges.remove(found);
                authuser *u = authusers.access(c.name);
                if(!u || !u->key || !u->pubkey || strcmp(u->key, c.key) || !checkmastersignin(u->pubkey, u->name, c.challenge, answer))
                {
                    signinfailed(req.ip);
                    foldnoticef("signin", NOTICE_WARNING, tempformatstring("web %s %s", c.name, req.addr), "Failed remembered sign-in for '%s' from %s (%s)", c.name, req.addr, u ? "BADKEY" : "CHANGED");
                    res.error(403, "identity verification failed");
                }
                else
                {
                    const char *label = signinfield(req, v, "label");
                    signincreated(req, res, *u, signinflag(req, v, "remember", false), label, signinflag(req, v, "cookie", true));
                }
            }
        }
        else if(name && *name)
        {
            int count = 0;
            loopv(signinchallenges) if(signinchallenges[i].ip == req.ip) count++;
            authuser *u = authusers.access(name);
            if(count >= SIGNIN_CHALLENGES) res.error(429, "too many challenges in progress");
            else if(!u || !u->pubkey)
            {
                signinfailed(req.ip);
                res.error(403, "identity verification failed");
            }
            else
            {
                signinchallenge &c = signinchallenges.add();
                if(!lastsigninchallenge) lastsigninchallenge = 1;
                c.id = lastsigninchallenge++;
                c.ip = req.ip;
                c.started = totalmillis;
                copystring(c.name, u->name);
                copystring(c.key, u->key);
                uchar seed[32]; // from the system, as a guessable challenge could be answered without the key
                http::randombytes(seed, sizeof(seed));
                hexbytes(c.challenge, seed, sizeof(seed));
                vector<char> buf;
                jsonwriter w(buf);
                w.beginobject();
                w.num("id", double(c.id));
                w.utf8("challenge", c.challenge);
                w.endobject();
                res.json(buf.getbuf(), buf.length());
            }
        }
        else res.error(400, "send a name to get a challenge, then the id and answer");
        DELETEP(v);
    }

    // GET /signin: the account's own sign-ins
    static void routesigninlist(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return;
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("name", s->name);
        w.utf8("flags", s->flags);
        w.beginarray("signins");
        loopv(signins) if(!strcmp(signins[i]->name, s->name)) writesignin(w, *signins[i], s);
        w.endarray();
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    // DELETE /signin, /signin/<id> or /signin/all
    static void routesigninend(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return;
        const char *id = req.param("id");
        string name, current;
        copystring(name, s->name);
        copystring(current, s->id);
        int ended = 0;
        bool self = false;
        loopvrev(signins)
        {
            signin &o = *signins[i];
            if(strcmp(o.name, name)) continue;
            bool all = id && !strcmp(id, "all"), match = id ? !strcmp(o.id, id) : !strcmp(o.id, current);
            if(!all && !match) continue;
            if(!strcmp(o.id, current)) self = true;
            signindrop(i, "signed out");
            ended++;
        }
        if(id && strcmp(id, "all") && !ended) { res.error(404, "no such sign-in"); return; }
        if(self && req.credentialed) setsignincookie(res, NULL, -1);
        defformatstring(msg, "{\"ok\":true,\"ended\":%d}", ended);
        res.json(msg);
    }

    // POST /signin/session/<address>/<port>: a Session on that server, which the master vouches for over its link
    static void routesigninsession(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return;
        masterclient *c = findrelay(req);
        if(!c) { res.error(404, "no such server linked to the master"); return; }
        if(!c->vouch) { res.error(501, "this server doesn't support sessions from the master"); return; }
        if(!c->linksecure) { res.error(403, "this server's link to the master isn't encrypted"); return; }
        if(c->relay < 2 && !c->fingerprint[0]) { res.error(403, "this server can't be reached with a session, as it has no certificate and doesn't allow signing in through the relay"); return; }
        int waiting = 0;
        loopv(signinexchanges) if(!strcmp(signinexchanges[i].signin, s->id)) waiting++;
        if(waiting >= SIGNIN_EXCHANGES) { res.error(429, "too many sessions in progress"); return; }
        signinexchange &x = signinexchanges.add();
        if(!lastsigninexchange) lastsigninexchange = 1;
        x.id = lastsigninexchange++;
        x.link = c->link;
        x.started = totalmillis;
        copystring(x.signin, s->id);
        copystring(x.name, s->name);
        x.deferred = http::deferred(req);
        masteroutf(*c, "vouch %u %s \"%s\" \"%s\"\n", x.id, s->id, s->name, s->flags);
        res.defer();
    }

    // POST /signin/session: a Session on the master itself, for its live list, where a page queues for its Account
    static void routesigninmaster(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return;
        string token, priv;
        int expires = 0;
        res.headers.set("Cache-Control", "no-store");
        if(!server::localsession(s->id, s->name, s->flags, token, priv, expires)) { res.error(403, "identity verification failed"); return; }
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("token", token);
        w.utf8("name", s->name);
        w.utf8("privilege", priv);
        w.num("expires", expires);
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    // a server's answer to a vouch, the Session token crossing its encrypted link once
    void signinvouched(masterclient &c, uint id, const char *token, const char *priv, bool local, int expires, const char *err)
    {
        loopv(signinexchanges) if(signinexchanges[i].id == id && signinexchanges[i].link == c.link)
        {
            signinexchange x = signinexchanges.remove(i);
            httpresponse res;
            res.headers.set("Cache-Control", "no-store");
            if(err) res.error(403, err);
            else
            {
                vector<char> buf;
                jsonwriter w(buf);
                w.beginobject();
                w.utf8("token", token);
                w.utf8("name", x.name);
                w.utf8("privilege", priv);
                if(local) w.boolean("local", true);
                w.num("expires", expires);
                w.endobject();
                res.json(buf.getbuf(), buf.length());
                conoutf(colourwhite, "Remembered sign-in %s for '%s' got a session on %s:%d (%s)", x.signin, x.name, c.name, c.port, priv);
            }
            http::reply(x.deferred, res);
            return;
        }
    }

    void signinlinkclosed(masterclient &c)
    {
        loopvrev(signinexchanges) if(signinexchanges[i].link == c.link)
        {
            httpresponse res;
            res.error(502, "lost the link to that server");
            http::reply(signinexchanges[i].deferred, res);
            signinexchanges.remove(i);
        }
    }

    void setupsignin()
    {
        http::addcredentials(SIGNIN_PATH, signinorigin);
        setupowner();
        http::addroute(HTTP_POST, SIGNIN_PATH, HTTP_PUBLIC, routesignin);
        http::addroute(HTTP_GET, SIGNIN_PATH, HTTP_PUBLIC, routesigninlist);
        http::addroute(HTTP_DELETE, SIGNIN_PATH, HTTP_PUBLIC, routesigninend);
        http::addroute(HTTP_DELETE, SIGNIN_PATH "/:id", HTTP_PUBLIC, routesigninend);
        http::addroute(HTTP_POST, SIGNIN_PATH "/session", HTTP_PUBLIC, routesigninmaster);
        http::addroute(HTTP_POST, SIGNIN_PATH "/session/:address/:port", HTTP_PUBLIC, routesigninsession);
        setupadmin();
        setupaccounts();
        loadsignins();
    }
}
