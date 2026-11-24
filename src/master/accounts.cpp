#include "engine.h"
#include "master.h"

#define TICKET_VERSION 3 // 2 added the owner and level of a server account waiting for its owner, 3 added its description
#define TICKET_MAXSTORED 65536 // tickets kept and loaded, the oldest going first past it
#define TICKET_DECOY "-" // the handle of a game's reset pickup for no account, which no account can have
#define DAILY_MAXADDRS 65536 // addresses counted per day, earlier days' going first past it
#define CAPTCHA_PERADDR 2 // captcha checks waiting on Turnstile for each address
#define HANDLE_MIN 3
#define HANDLE_MAX 24

namespace master
{
    // registrations, and the other things an account's owner confirms by following a link mailed to them

    VAR(0, masterregister, 0, 1, 1); // whether new accounts can be registered
    VAR(0, masterregisterdays, 1, 7, 90); // days a registration has to be activated before it's purged
    VAR(0, masterregisterdaily, 0, 3, VAR_MAX); // registrations an address may make in a day, 0 for no limit
    VAR(0, masterresethours, 1, 24, 168); // hours a key reset's link lasts
    VAR(0, masterresetdaily, 0, 5, VAR_MAX); // key resets an address may ask for in a day, 0 for no limit
    SVAR(0, masterreservedhandles, ""); // space-separated handles nobody may register
    SVAR(0, masteraccounturl, ""); // the page the mailed links open, such as https://redeclipse.net/account
    SVAR(0, masterticketfile, "masteraccount.dat");
    SVAR(0, masterturnstileurl, ""); // Cloudflare Turnstile's siteverify endpoint, such as https://challenges.cloudflare.com/turnstile/v0/siteverify
    SVAR(0, masterturnstilehost, ""); // the hostname a solved challenge must come from, such as redeclipse.net, "" for any
    VAR(0, masterturnstilemax, 1, 32, 1024); // captcha checks waiting on Turnstile at once, from everyone
    VAR(0, masterturnstiledaily, 0, 20, VAR_MAX); // captcha checks an address may start in a day, 0 for no limit


    struct regcount
    {
        int day, count;
    };

    enum { CAPTCHA_REGISTER = 0, CAPTCHA_RESET };
    struct captchacheck
    {
        int deferred, fetch, action;
        uint ip;
        string handle, email, addr;
    };

    static struct accountstate
    {
        vector<ticket *> tickets;
        hashtable<uint, regcount> registered, resets, checked; // per address, today's
        vector<captchacheck *> captchas;
        bool dirty;
        int lastcheck;

        accountstate() : dirty(false), lastcheck(0) {}
    } accounts;

    vector<ticket *> &accounttickets() { return accounts.tickets; }
    void dirtyaccounts() { accounts.dirty = true; }

    static void newtoken(char *token, char *hash)
    {
        uchar bytes[32];
        http::randombytes(bytes, 32);
        hexbytes(token, bytes, 32);
        hashtoken(hash, token);
    }

    static ticket *findticket(const char *token, int kind, int which) // which: 0 the link, 1 the cancel link, 2 the game's pickup
    {
        if(!token || strlen(token) != 64) return NULL;
        string hash;
        hashtoken(hash, token);
        loopv(accounts.tickets)
        {
            ticket &t = *accounts.tickets[i];
            if(kind >= 0 && t.kind != kind) continue;
            if(!strcmp(which == 2 ? t.pickuphash : (which == 1 ? t.cancelhash : t.hash), hash)) return &t;
        }
        return NULL;
    }

    void dropticket(ticket *t)
    {
        accounts.tickets.removeobj(t);
        delete t;
        accounts.dirty = true;
    }

    // past the most a load takes, the oldest decoy goes first, then the oldest of any
    static void addticket(ticket *t)
    {
        if(accounts.tickets.length() >= TICKET_MAXSTORED)
        {
            int oldest = -1;
            bool decoy = false;
            loopv(accounts.tickets)
            {
                ticket &o = *accounts.tickets[i];
                bool isdecoy = !strcmp(o.handle, TICKET_DECOY);
                if(oldest < 0 || (isdecoy && !decoy) || (isdecoy == decoy && o.created < accounts.tickets[oldest]->created)) { oldest = i; decoy = isdecoy; }
            }
            if(oldest >= 0) dropticket(accounts.tickets[oldest]);
        }
        accounts.tickets.add(t);
        accounts.dirty = true;
    }

    static masterfileguard ticketguard;

    static int readtickets(const char *name, int)
    {
        accounts.tickets.deletecontents();
        stream *f = opengzfile(name, "rb");
        if(!f) return masterfilemissing(name);
        char magic[4];
        bool ok = f->read(magic, 4) == 4 && !memcmp(magic, "RETK", 4);
        int version = ok ? f->getlil<int>() : 0;
        if(ok && (version < 1 || version > TICKET_VERSION)) return masterfileolder(f, name);
        int count = ok ? f->getlil<int>() : 0;
        if(count < 0 || count > TICKET_MAXSTORED) ok = false;
        for(int i = 0; ok && i < count; i++)
        {
            ticket *t = new ticket;
            string *strs[10] = { &t->handle, &t->email, &t->hash, &t->cancelhash, &t->pickuphash, &t->addr, &t->privkey, &t->owner, &t->level, &t->desc };
            loopj(version >= 3 ? 10 : (version >= 2 ? 9 : 7))
            {
                int len = f->getlil<int>();
                if(len < 0 || len >= MAXSTRLEN || (len && f->read(*strs[j], len) != size_t(len))) { ok = false; break; }
                (*strs[j])[len] = '\0';
            }
            if(!ok) { delete t; break; }
            t->kind = f->getlil<int>();
            t->collected = f->getlil<int>();
            t->done = f->getchar() != 0;
            t->created = time_t(f->getlil<int64_t>());
            t->expires = time_t(f->getlil<int64_t>());
            if(t->kind < 0 || t->kind >= TICKET_MAX || !t->handle[0]) { delete t; continue; }
            accounts.tickets.add(t);
        }
        delete f;
        if(!ok) { accounts.tickets.deletecontents(); return 1; }
        if(accounts.tickets.length()) conoutf(colourwhite, "Loaded %d registration(s) from %s", accounts.tickets.length(), name);
        return 0;
    }

    static void loadtickets()
    {
        accounts.tickets.deletecontents();
        if(!*masterticketfile) return;
        if(loadguarded(masterticketfile, readtickets, 0, ticketguard, "the master restarts")) accounts.dirty = true;
    }

    static void savetickets()
    {
        accounts.dirty = false;
        if(!*masterticketfile || saveblocked(ticketguard, masterticketfile)) return;
        defformatstring(newname, "%s.new", masterticketfile);
        stream *f = openmasterfile(newname, true);
        if(!f) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s save", newname), "Failed to save registrations to %s", newname); return; }
        f->write("RETK", 4);
        f->putlil<int>(TICKET_VERSION);
        f->putlil<int>(accounts.tickets.length());
        loopv(accounts.tickets)
        {
            ticket &t = *accounts.tickets[i];
            const char *strs[10] = { t.handle, t.email, t.hash, t.cancelhash, t.pickuphash, t.addr, t.privkey, t.owner, t.level, t.desc };
            loopj(10)
            {
                int len = strlen(strs[j]);
                f->putlil<int>(len);
                if(len) f->write(strs[j], len);
            }
            f->putlil<int>(t.kind);
            f->putlil<int>(t.collected);
            f->putchar(t.done ? 1 : 0);
            f->putlil<int64_t>(int64_t(t.created));
            f->putlil<int64_t>(int64_t(t.expires));
        }
        delete f;
        commitmasterfile(masterticketfile, ticketguard, false); // only its .bak, as dated copies would keep the private keys games collected
    }

    static void purgetickets();

    void checktickets()
    {
        if(totalmillis - accounts.lastcheck >= 1000) purgetickets();
        if(accounts.dirty) savetickets(); // at the end of the frame anything changed in, so a crash loses none
    }

    // once a second: registrations that ran out
    static void purgetickets()
    {
        accounts.lastcheck = totalmillis;
        time_t now = time(NULL);
        loopvrev(accounts.tickets)
        {
            ticket &t = *accounts.tickets[i];
            if(t.expires > now) continue;
            if(t.kind == TICKET_REGISTER && !t.done) noticef("account", NOTICE_INFO, "Purged the registration of '%s' from %s, it wasn't activated in time", t.handle, t.addr);
            else if(t.kind == TICKET_SERVER)
            {
                // the notice never has the email address a server account's link went to
                conoutf(colourwhite, "Purged %s the server account '%s', '%s' didn't follow the link in time", t.level[0] ? "the waiting" : "a new key for", t.handle, t.owner[0] ? t.owner : t.email);
                postnotice("serveraccount", NOTICE_INFO, tempformatstring("Purged %s the server account '%s', %s didn't follow the link in time", t.level[0] ? "the waiting" : "a new key for", t.handle, t.owner[0] ? tempformatstring("'%s'", t.owner) : "its owner"));
            }
            dropticket(&t);
        }
    }

    static bool handlechar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }

    // lowercases a handle someone asked for, false if it can't be one
    bool parsehandle(const char *src, char *dst)
    {
        int len = strlen(src);
        if(len < HANDLE_MIN || len > HANDLE_MAX) return false;
        loopi(len)
        {
            char c = tolower(uchar(src[i]));
            if(!handlechar(c)) return false;
            dst[i] = c;
        }
        dst[len] = '\0';
        return true;
    }

    // a registration still waiting for the same handle and email, which asking again replaces with a fresh link
    static ticket *samependingreg(const char *handle, const char *email)
    {
        loopv(accounts.tickets)
        {
            ticket &t = *accounts.tickets[i];
            if(t.kind == TICKET_REGISTER && !t.done && !strcasecmp(t.handle, handle) && !strcasecmp(t.email, email)) return &t;
        }
        return NULL;
    }

    static bool handletaken(const char *handle)
    {
        bool taken = false;
        enumerate(authusers, authuser, u, if(!strcasecmp(u.name, handle)) taken = true);
        enumerate(serverauthusers, authuser, u, if(!strcasecmp(u.name, handle)) taken = true);
        loopv(accounts.tickets) if(accounts.tickets[i]->kind == TICKET_REGISTER && !accounts.tickets[i]->done && !strcasecmp(accounts.tickets[i]->handle, handle)) taken = true;
        loopv(accounts.tickets) if(accounts.tickets[i]->kind == TICKET_SERVER && accounts.tickets[i]->level[0] && !strcasecmp(accounts.tickets[i]->handle, handle)) taken = true;
        if(!taken)
        {
            vector<char *> reserved;
            explodelist(masterreservedhandles, reserved);
            loopv(reserved) if(!strcasecmp(reserved[i], handle)) taken = true;
            reserved.deletearrays();
        }
        return taken;
    }

    static bool emailtaken(const char *email)
    {
        bool taken = false;
        enumerate(authusers, authuser, u, if(u.email && !strcasecmp(u.email, email)) taken = true);
        loopv(accounts.tickets) if((accounts.tickets[i]->kind == TICKET_REGISTER || accounts.tickets[i]->kind == TICKET_EMAIL) && !accounts.tickets[i]->done && !strcasecmp(accounts.tickets[i]->email, email)) taken = true;
        return taken;
    }

    // counts one more for the address, false when it's had its day's worth
    static bool dailycount(hashtable<uint, regcount> &counts, int limit, uint ip, bool add)
    {
        if(!limit) return true;
        int day = int(time(NULL) / SIGNIN_DAY);
        regcount *c = counts.access(ip);
        if(!c && counts.numelems >= DAILY_MAXADDRS)
        {
            static vector<uint> stale;
            stale.setsize(0);
            uint any = 0;
            bool found = false;
            enumeratekt(counts, uint, k, regcount, r, { if(r.day != day) stale.add(k); else if(!found) { any = k; found = true; } });
            if(stale.empty() && found) stale.add(any); // all today's, so one gives way
            loopv(stale) counts.remove(stale[i]);
        }
        if(!c) { c = &counts[ip]; c->day = day; c->count = 0; } // new entries aren't zeroed
        else if(c->day != day) { c->day = day; c->count = 0; }
        if(c->count >= limit) return false;
        if(add) c->count++;
        return true;
    }

    static bool registercount(uint ip, bool add) { return dailycount(accounts.registered, masterregisterdaily, ip, add); }

    static void lowerstring(char *s) { for(; *s; s++) *s = tolower(uchar(*s)); }

    static void registermail(ticket &t, const char *token, const char *canceltoken, bool game)
    {
        defformatstring(subject, "Activate your %s account %s", versionname, t.handle);
        defformatstring(activateurl, "%s/activate#%s", masteraccounturl, token);
        defformatstring(cancelurl, "%s/cancel#%s", masteraccounturl, canceltoken);
        bigstring text;
        formatstring(text,
            "Hello,\n\n"
            "Someone, hopefully you, registered the %s account \"%s\" with this email address.\n\n"
            "To activate it, follow this link within %d days:\n%s\n\n"
            "Once it's activated, the page gives you the account's key and how to use it in the game.%s\n\n"
            "If this wasn't you, follow this link and the registration is removed straight away:\n%s\n\n"
            "Otherwise you can ignore this email, and the registration is removed after %d days.\n\n"
            "%s\nhttps://%s\n",
            versionname, t.handle, masterregisterdays, activateurl,
            game ? " If the game you registered from is still running, it collects the key itself." : "",
            cancelurl, masterregisterdays, versionname, versionurl);
        tagval args[5];
        args[0].setcstr(t.handle);
        args[1].setint(masterregisterdays);
        args[2].setcstr(activateurl);
        args[3].setcstr(cancelurl);
        args[4].setint(game ? 1 : 0);
        queuemaildef("register", t.email, subject, text, args, 5);
    }

    // makes the registration once everything has been checked, answering the request
    static void registercreate(httpresponse &res, uint ip, const char *addr, const char *handle, const char *email, bool game)
    {
        ticket *again = samependingreg(handle, email);
        if(!registercount(ip, false)) { res.error(429, "too many registrations from your address today"); return; }
        if(again)
        {
            // its mail may never have gone, so asking again sends a fresh link in its place
            noticef("account", NOTICE_INFO, "Registration of '%s' asked for again from %s, its earlier link no longer works", again->handle, addr);
            dropticket(again);
        }
        if(handletaken(handle)) { res.error(409, "that handle is taken"); return; }
        if(emailtaken(email)) { res.error(409, "that email address already has an account"); return; }
        registercount(ip, true);
        ticket *t = new ticket;
        t->kind = TICKET_REGISTER;
        copystring(t->handle, handle);
        copystring(t->email, email);
        copystring(t->addr, addr);
        t->created = time(NULL);
        t->expires = t->created + time_t(masterregisterdays)*SIGNIN_DAY;
        string token, canceltoken, pickup;
        newtoken(token, t->hash);
        newtoken(canceltoken, t->cancelhash);
        if(game) newtoken(pickup, t->pickuphash);
        addticket(t);
        registermail(*t, token, canceltoken, game);
        memset(token, 0, sizeof(token));
        noticef("account", NOTICE_INFO, "Registration of '%s' from %s%s, waiting for activation", t->handle, addr, game ? " in game" : "");
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("handle", t->handle);
        w.num("expires", double(t->expires));
        if(game) w.utf8("pickup", pickup);
        w.endobject();
        res.status = 202;
        res.json(buf.getbuf(), buf.length());
    }

    static authuser *findaccount(const char *who)
    {
        authuser *found = NULL;
        if(!who || !*who) return NULL;
        enumerate(authusers, authuser, u, if(!found && (!strcasecmp(u.name, who) || (u.email && *u.email && !strcasecmp(u.email, who)))) found = &u);
        return found;
    }

    static void resetmail(ticket &t, const char *token)
    {
        defformatstring(subject, "Reset the key of your %s account %s", versionname, t.handle);
        defformatstring(reseturl, "%s/reset#%s", masteraccounturl, token);
        bigstring text;
        formatstring(text,
            "Hello,\n\n"
            "Someone, hopefully you, asked for a new key for the %s account \"%s\".\n\n"
            "To get one, follow this link within %d hours:\n%s\n\n"
            "The new key replaces the old one, which stops working, and signs the account out of every browser and device that remembered it.\n\n"
            "If this wasn't you, ignore this email and nothing changes.\n\n"
            "%s\nhttps://%s\n",
            versionname, t.handle, masterresethours, reseturl, versionname, versionurl);
        tagval args[3];
        args[0].setcstr(t.handle);
        args[1].setint(masterresethours);
        args[2].setcstr(reseturl);
        queuemaildef("reset", t.email, subject, text, args, 3);
    }

    // a reset for whichever account the handle or email is, answering the same whether there is one or not
    static void resetcreate(httpresponse &res, uint ip, const char *addr, const char *who, bool game)
    {
        if(!dailycount(accounts.resets, masterresetdaily, ip, true)) { res.error(429, "too many key resets from your address today"); return; }
        authuser *u = findaccount(who);
        if(u && (!u->email || !*u->email)) u = NULL; // nowhere to send the link
        ticket *t = NULL;
        string token, pickup;
        if(u)
        {
            loopvrev(accounts.tickets) if(accounts.tickets[i]->kind == TICKET_RESET && !strcmp(accounts.tickets[i]->handle, u->name)) dropticket(accounts.tickets[i]); // the newest link is the one that works
            t = new ticket;
            t->kind = TICKET_RESET;
            copystring(t->handle, u->name);
            copystring(t->email, u->email);
            copystring(t->addr, addr);
            t->created = time(NULL);
            t->expires = t->created + time_t(masterresethours)*60*60;
            newtoken(token, t->hash);
            if(game) newtoken(pickup, t->pickuphash);
            addticket(t);
            resetmail(*t, token);
            memset(token, 0, sizeof(token));
            noticef("account", NOTICE_INFO, "Key reset of '%s' asked for from %s%s", t->handle, addr, game ? " in game" : "");
        }
        else if(game)
        {
            // a pickup that never becomes ready, so the game's answer gives nothing away
            t = new ticket;
            t->kind = TICKET_RESET;
            t->done = false;
            copystring(t->handle, TICKET_DECOY);
            copystring(t->addr, addr);
            t->created = time(NULL);
            t->expires = t->created + time_t(masterresethours)*60*60;
            newtoken(pickup, t->pickuphash);
            t->hash[0] = '\0';
            addticket(t);
        }
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.num("expires", double(time(NULL) + time_t(masterresethours)*60*60));
        if(game) w.utf8("pickup", pickup);
        w.endobject();
        res.status = 202;
        res.json(buf.getbuf(), buf.length());
    }

    static void captchadone(httpfetch &f)
    {
        captchacheck *c = (captchacheck *)f.ctx;
        if(accounts.captchas.find(c) < 0) return;
        accounts.captchas.removeobj(c);
        httpresponse res;
        jsonvalue *v = f.status == 200 && f.body.length() ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
        jsonvalue *ok = v ? v->get("success") : NULL;
        const char *host = v ? v->getstr("hostname", "") : "";
        if(!v) res.error(502, "the captcha couldn't be checked, try again");
        else if(!ok || !ok->istrue()) res.error(403, "the captcha wasn't solved, try again");
        else if(*masterturnstilehost && strcasecmp(host, masterturnstilehost)) res.error(403, "the captcha was solved somewhere else");
        else if(c->action == CAPTCHA_RESET) resetcreate(res, c->ip, c->addr, c->handle, false);
        else registercreate(res, c->ip, c->addr, c->handle, c->email, false);
        DELETEP(v);
        http::reply(c->deferred, res);
        delete c;
    }

    void cleanupaccounts()
    {
        loopv(accounts.captchas) http::cancel(accounts.captchas[i]->fetch);
        accounts.captchas.deletecontents();
        if(accounts.dirty) savetickets();
    }

    static bool accountready(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return false;
        return true;
    }

    // asks Turnstile whether the website's captcha was solved, the request waiting for the answer
    static void startcaptcha(httprequest &req, httpresponse &res, const char *captcha, int action, const char *handle, const char *email)
    {
        int waiting = 0;
        loopv(accounts.captchas) if(accounts.captchas[i]->ip == req.ip) waiting++;
        if(waiting >= CAPTCHA_PERADDR) { res.error(429, "a captcha from your address is still being checked, try again in a moment"); return; }
        if(accounts.captchas.length() >= masterturnstilemax) { res.error(503, "too many captchas are being checked, try again in a moment"); res.headers.set("Retry-After", "10"); return; }
        if(!dailycount(accounts.checked, masterturnstiledaily, req.ip, true)) { res.error(429, "too many captchas from your address today"); return; }
        captchacheck *c = new captchacheck;
        c->action = action;
        c->ip = req.ip;
        copystring(c->addr, req.addr);
        copystring(c->handle, handle);
        copystring(c->email, email);
        httpfetch *f = http::newfetch(HTTP_POST, masterturnstileurl, captchadone, c);
        f->reqheaders.add("Content-Type", "application/x-www-form-urlencoded");
        f->reqbody.put("secret=", 7);
        http::urlencode(f->reqbody, mastersecrets.turnstile);
        f->reqbody.put("&response=", 10);
        http::urlencode(f->reqbody, captcha);
        f->reqbody.put("&remoteip=", 10);
        http::urlencode(f->reqbody, req.addr);
        c->deferred = http::deferred(req);
        accounts.captchas.add(c);
        res.defer();
        int id = http::submit(f);
        if(accounts.captchas.find(c) >= 0) c->fetch = id;
    }

    // GET /accounts: whether registration is open, for the game and the website to say so
    static void routeaccounts(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.boolean("register", masterregister && *masteraccounturl);
        w.boolean("website", masterregister && *masteraccounturl && *mastersecrets.turnstile && *masterturnstileurl);
        w.num("days", masterregisterdays);
        w.endobject();
        res.headers.set("Cache-Control", "no-store");
        res.json(buf.getbuf(), buf.length());
    }

    // POST /accounts with a handle and email, and from the website a solved captcha, or from the game "game": true
    static void routeregister(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *name = signinfield(req, v, "handle"), *mail = signinfield(req, v, "email"), *captcha = signinfield(req, v, "captcha");
        bool game = signinflag(req, v, "game", false);
        string handle, email;
        if(mail) { copystring(email, mail); lowerstring(email); }
        if(!masterregister || !*masteraccounturl) res.error(503, "registration is closed");
        else if(!name || !parsehandle(name, handle)) res.error(400, "a handle is 3 to 24 letters, numbers, - or _");
        else if(!mail || !validemail(email)) res.error(400, "that isn't an email address");
        else if(!samependingreg(handle, email) && handletaken(handle)) res.error(409, "that handle is taken");
        else if(!samependingreg(handle, email) && emailtaken(email)) res.error(409, "that email address already has an account");
        else if(!registercount(req.ip, false)) res.error(429, "too many registrations from your address today");
        else if(game) registercreate(res, req.ip, req.addr, handle, email, true);
        else if(!*mastersecrets.turnstile || !*masterturnstileurl) res.error(503, "registration on the website isn't set up, register in the game instead");
        else if(!captcha || !*captcha || strlen(captcha) > 2048) res.error(400, "solve the captcha first");
        else startcaptcha(req, res, captcha, CAPTCHA_REGISTER, handle, email);
        DELETEP(v);
    }

    static void writekey(jsonwriter &w, const char *handle, const char *privkey, const char *pubkey)
    {
        w.utf8("handle", handle);
        w.utf8("privkey", privkey);
        if(pubkey) w.utf8("pubkey", pubkey);
        defformatstring(line, "authkey %s %s", handle, privkey);
        w.utf8("authkey", line);
    }

    // POST /accounts/activate with the token from the link
    static void routeactivate(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *token = signinfield(req, v, "token");
        ticket *t = findticket(token, TICKET_REGISTER, 0);
        res.headers.set("Cache-Control", "no-store");
        if(!t) { signinfailed(req.ip); res.error(404, "that link is unknown or ran out, register again"); }
        else if(t->done) res.error(410, "that account is already activated");
        else if(authusers.access(t->handle)) { res.error(409, "that handle was taken in the meantime"); dropticket(t); }
        else
        {
            uchar seed[32];
            http::randombytes(seed, 32);
            string seedhex;
            hexbytes(seedhex, seed, 32);
            vector<char> priv, pub;
            genprivkey(seedhex, priv, pub);
            memset(seedhex, 0, sizeof(seedhex));
            if(!newauth(t->handle, "u", pub.getbuf(), t->email, ""))
            {
                res.error(500, "the account couldn't be made");
                DELETEP(v);
                return;
            }
            dirtymasterfile(MFILE_USER);
            conoutf(colourwhite, "Activated the account '%s' (%s)", t->handle, t->email);
            postnotice("account", NOTICE_INFO, tempformatstring("Activated the account '%s' from %s, registered from %s", t->handle, req.addr, t->addr));
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            writekey(w, t->handle, priv.getbuf(), pub.getbuf());
            w.endobject();
            res.json(buf.getbuf(), buf.length());
            if(t->pickuphash[0])
            {
                // the game that asked collects it the same way
                t->done = true;
                t->collected = 1;
                copystring(t->privkey, priv.getbuf());
                t->cancelhash[0] = '\0'; // the link stays known, so following it again says it's done
                accounts.dirty = true;
            }
            else dropticket(t);
            memset(priv.getbuf(), 0, priv.length());
        }
        DELETEP(v);
    }

    // ---- server accounts, which an administrator makes for an owner, who gets the key from a mailed link

    // the ticket waiting on a server account, a new one's or a new key's
    ticket *serverticket(const char *handle)
    {
        loopv(accounts.tickets) if(accounts.tickets[i]->kind == TICKET_SERVER && !strcasecmp(accounts.tickets[i]->handle, handle)) return accounts.tickets[i];
        return NULL;
    }

    static void servermail(ticket &t, const char *token)
    {
        defformatstring(subject, "%s %s server account %s", t.level[0] ? "Your" : "A new key for your", versionname, t.handle);
        defformatstring(serverurl, "%s/server#%s", masteraccounturl, token);
        bigstring text;
        formatstring(text,
            "Hello,\n\n"
            "%s\n\n"
            "To get its key, follow this link within %d days:\n%s\n\n"
            "The page gives you a line to put in secrets.cfg in your server's home folder, which the server reads when it starts "
            "and on rehash, and signs in to the master server with.%s\n\n"
            "If you weren't expecting this, you can ignore this email, and the link stops working after %d days.\n\n"
            "%s\nhttps://%s\n",
            t.level[0] ? tempformatstring("An administrator of the %s master server made the server account \"%s\" with you%s as its owner.", versionname, t.handle, t.owner[0] ? tempformatstring(" (%s)", t.owner) : "")
                : tempformatstring("A new key was asked for the %s server account \"%s\".", versionname, t.handle),
            masterregisterdays, serverurl,
            t.level[0] ? "" : " The old key keeps working until you follow the link.",
            masterregisterdays, versionname, versionurl);
        tagval args[5];
        args[0].setcstr(t.handle);
        args[1].setcstr(t.owner);
        args[2].setcstr(t.level);
        args[3].setint(masterregisterdays);
        args[4].setcstr(serverurl);
        queuemaildef("server", t.email, subject, text, args, 5);
    }

    // a fresh link for a server account, in place of any it had
    ticket *serverlink(const char *handle, const char *owner, const char *email, const char *level, const char *desc, const char *by)
    {
        ticket *old = serverticket(handle);
        if(old) dropticket(old);
        ticket *t = new ticket;
        t->kind = TICKET_SERVER;
        copystring(t->handle, handle);
        copystring(t->owner, owner ? owner : "");
        copystring(t->email, email);
        copystring(t->level, level ? level : "");
        copystring(t->desc, desc ? desc : "");
        copystring(t->addr, by);
        t->created = time(NULL);
        t->expires = t->created + time_t(masterregisterdays)*SIGNIN_DAY;
        string token;
        newtoken(token, t->hash);
        addticket(t);
        servermail(*t, token);
        memset(token, 0, sizeof(token));
        return t;
    }

    void writependingserver(jsonwriter &w, ticket &t)
    {
        w.beginobject();
        w.utf8("handle", t.handle);
        w.utf8("flags", t.level);
        w.str("desc", t.desc);
        w.utf8("owner", t.owner);
        w.boolean("pending", true);
        w.num("expires", double(t.expires));
        w.endobject();
    }

    // GET /signin/admin/servers?q=, with those waiting for their owner, and when a new key was asked for one

    // POST /accounts/server with the token from the owner's link: makes the server account, or its new key, and gives the key once
    static void routeserveractivate(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *token = signinfield(req, v, "token");
        ticket *t = findticket(token, TICKET_SERVER, 0);
        res.headers.set("Cache-Control", "no-store");
        authuser *u = t ? serverauthusers.access(t->handle) : NULL;
        if(!t) { signinfailed(req.ip); res.error(404, "that link is unknown or ran out, ask an administrator for a new one"); }
        else if(t->level[0] && u) { res.error(409, "that handle was taken in the meantime"); dropticket(t); }
        else if(!t->level[0] && !u) { res.error(404, "that server account was removed"); dropticket(t); }
        else
        {
            uchar seed[32];
            http::randombytes(seed, 32);
            string seedhex;
            hexbytes(seedhex, seed, 32);
            vector<char> priv, pub;
            genprivkey(seedhex, priv, pub);
            memset(seedhex, 0, sizeof(seedhex));
            bool ok = true;
            if(u)
            {
                delete[] u->key;
                u->key = newstring(pub.getbuf());
                freepubkey(u->pubkey);
                u->pubkey = parsepubkey(pub.getbuf());
                checkserverauth(u->name);
            }
            else ok = newserverauth(t->handle, t->level, pub.getbuf(), t->desc, t->owner, ""); // its known addresses are kept from the start
            if(!ok) res.error(500, "the server account couldn't be made");
            else
            {
                dirtymasterfile(MFILE_SERV);
                conoutf(colourwhite, "%s the server account '%s' (%s)", u ? "Gave a new key to" : "Activated", t->handle, t->owner[0] ? t->owner : t->email);
                if(u)
                {
                    // where the old key signed in from says nothing of the new one
                    DELETEA(u->addrs);
                    u->addrs = newstring("");
                }
                postnotice("serveraccount", NOTICE_INFO, tempformatstring("%s the server account '%s' for %s from %s", u ? "Gave a new key to" : "Activated", t->handle, t->owner[0] ? tempformatstring("'%s'", t->owner) : "its owner", req.addr));
                vector<char> buf;
                jsonwriter w(buf);
                w.beginobject();
                w.utf8("handle", t->handle);
                w.utf8("privkey", priv.getbuf());
                w.utf8("pubkey", pub.getbuf());
                defformatstring(line, "serverauthkey %s %s", t->handle, priv.getbuf());
                w.utf8("serverauthkey", line);
                w.boolean("newkey", u != NULL);
                w.endobject();
                res.json(buf.getbuf(), buf.length());
                dropticket(t);
            }
            memset(priv.getbuf(), 0, priv.length());
        }
        DELETEP(v);
    }

    // POST /accounts/cancel with the token from the "this wasn't me" link
    static void routecancel(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *token = signinfield(req, v, "token");
        ticket *t = findticket(token, TICKET_REGISTER, 1);
        if(!t || t->done) { signinfailed(req.ip); res.error(404, "that link is unknown, ran out or the account was already activated"); }
        else
        {
            noticef("account", NOTICE_INFO, "Removed the registration of '%s' from %s, its email's owner said it wasn't them from %s", t->handle, t->addr, req.addr);
            dropticket(t);
            res.status = 204;
        }
        DELETEP(v);
    }

    // POST /accounts/pickup with the secret the game was given: waiting, or the key once
    static void routepickup(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *token = signinfield(req, v, "pickup");
        ticket *t = findticket(token, -1, 2);
        res.headers.set("Cache-Control", "no-store");
        vector<char> buf;
        jsonwriter w(buf);
        if(!t) { signinfailed(req.ip); res.error(404, "unknown, ran out or already collected"); }
        else if(!t->done)
        {
            w.beginobject();
            if(t->kind == TICKET_REGISTER) w.utf8("handle", t->handle); // a reset's could give away whether there's an account
            w.boolean("ready", false);
            w.num("expires", double(t->expires));
            w.endobject();
            res.json(buf.getbuf(), buf.length());
        }
        else
        {
            authuser *u = authusers.access(t->handle);
            w.beginobject();
            w.boolean("ready", true);
            writekey(w, t->handle, t->privkey, u ? u->key : NULL);
            w.endobject();
            res.json(buf.getbuf(), buf.length());
            noticef("account", NOTICE_INFO, "The game collected the key of '%s' from %s", t->handle, req.addr);
            dropticket(t);
        }
        DELETEP(v);
    }

    // POST /accounts/reset with a handle or email, and from the website a solved captcha, or from the game "game": true
    static void routereset(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *who = signinfield(req, v, "handle"), *captcha = signinfield(req, v, "captcha");
        if(!who || !*who) who = signinfield(req, v, "email");
        bool game = signinflag(req, v, "game", false);
        string what = "";
        if(who) filterstring(what, who, true, true, false, true, false, MAIL_MAXADDR);
        if(!*masteraccounturl) res.error(503, "key resets aren't set up");
        else if(!what[0]) res.error(400, "send the account's handle or email address");
        else if(!dailycount(accounts.resets, masterresetdaily, req.ip, false)) res.error(429, "too many key resets from your address today");
        else if(game) resetcreate(res, req.ip, req.addr, what, true);
        else if(!*mastersecrets.turnstile || !*masterturnstileurl) res.error(503, "key resets on the website aren't set up, ask in the game instead");
        else if(!captcha || !*captcha || strlen(captcha) > 2048) res.error(400, "solve the captcha first");
        else startcaptcha(req, res, captcha, CAPTCHA_RESET, what, "");
        DELETEP(v);
    }

    // POST /accounts/reset/confirm with the token from the link: the new key, once
    static void routeresetconfirm(httprequest &req, httpresponse &res)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *token = signinfield(req, v, "token");
        ticket *t = findticket(token, TICKET_RESET, 0);
        res.headers.set("Cache-Control", "no-store");
        authuser *u = t ? authusers.access(t->handle) : NULL;
        if(!t) { signinfailed(req.ip); res.error(404, "that link is unknown or ran out, ask for another"); }
        else if(t->done) res.error(410, "that link was already used");
        else if(!u) { res.error(404, "that account no longer exists"); dropticket(t); }
        else
        {
            uchar seed[32];
            http::randombytes(seed, 32);
            string seedhex;
            hexbytes(seedhex, seed, 32);
            vector<char> priv, pub;
            genprivkey(seedhex, priv, pub);
            memset(seedhex, 0, sizeof(seedhex));
            delete[] u->key;
            freepubkey(u->pubkey);
            u->key = newstring(pub.getbuf());
            u->pubkey = parsepubkey(u->key);
            dirtymasterfile(MFILE_USER);
            signincheck = true; // sign-ins made with the old key end
            noticef("account", NOTICE_INFO, "Reset the key of '%s'%s from %s, asked for from %s", u->name, accountlinks(*u), req.addr, t->addr);
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            writekey(w, u->name, priv.getbuf(), u->key);
            w.endobject();
            res.json(buf.getbuf(), buf.length());
            if(t->pickuphash[0])
            {
                t->done = true;
                t->collected = 1;
                copystring(t->privkey, priv.getbuf());
                accounts.dirty = true;
            }
            else dropticket(t);
            memset(priv.getbuf(), 0, priv.length());
        }
        DELETEP(v);
    }

    // the signed in owner's own account
    signin *ownersignin(httprequest &req, httpresponse &res, authuser *&u)
    {
        u = NULL;
        if(!signinready(req, res)) return NULL;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return NULL;
        u = authusers.access(s->name);
        if(!u) { res.error(404, "your account no longer exists"); return NULL; }
        res.headers.set("Cache-Control", "no-store");
        return s;
    }

    void writeowner(jsonwriter &w, authuser &u)
    {
        w.beginobject();
        w.utf8("handle", u.name);
        w.utf8("flags", u.flags);
        w.utf8("email", u.email ? u.email : "");
        w.utf8("steamid", u.steamid ? u.steamid : "");
        w.utf8("discord", u.discord ? u.discord : "");
        w.str("displayname", u.display ? u.display : "");
        loopv(accounts.tickets) if(accounts.tickets[i]->kind == TICKET_EMAIL && !strcmp(accounts.tickets[i]->handle, u.name)) { w.utf8("newemail", accounts.tickets[i]->email); break; }
        w.endobject();
    }

    // GET /signin/account
    static void routeowner(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        vector<char> buf;
        jsonwriter w(buf);
        writeowner(w, *u);
        res.json(buf.getbuf(), buf.length());
    }

    // an account with no display name takes the name it's playing under, if the server it's on had it signed in through the master
    void fillname(masterclient &c, const char *handle, const char *shown)
    {
        if(!handle || !*handle || !shown || !*shown) return;
        bool known = false;
        loopv(c.verified) if(!strcmp(c.verified[i], handle)) { known = true; break; }
        if(!known) return;
        authuser *u = authusers.access(handle);
        if(!u || (u->display && *u->display)) return;
        setdisplay(*u, shown);
        noticef("account", NOTICE_INFO, "Account '%s' took the display name '%s' in game on %s:%d", u->name, shown, c.name, c.port);
    }

    void fillnames(masterclient &c, jsonvalue *players)
    {
        loopv(players->items)
        {
            jsonvalue *p = players->items[i];
            const char *name = p->getstr("name");
            if(!name || !*name) continue;
            string shown;
            displayfromutf8(shown, name);
            fillname(c, p->getstr("handle"), shown);
        }
    }

    // PATCH /signin/account with a displayname, at most once an hour; "" clears it, so it's filled in again from the game or Discord
    static void routeownerset(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        jsonvalue *v = signinbody(req);
        const char *display = signinfield(req, v, "displayname");
        time_t now = time(NULL);
        string shown = "";
        if(display) displayfromutf8(shown, display);
        if(!display) res.error(400, "send a displayname");
        else if(*display && !shown[0]) res.error(400, "that display name has nothing left once it's filtered");
        else if(u->displayed && now - u->displayed < DISPLAY_HOURLY) { res.error(429, "you can change your display name once an hour"); res.headers.set("Retry-After", "3600"); }
        else
        {
            u->displayed = now;
            setdisplay(*u, shown);
            noticef("account", NOTICE_INFO, "Account '%s' changed its display name from '%s' to '%s' from %s", u->name, u->display ? u->display : "", shown, req.addr);
            vector<char> buf;
            jsonwriter w(buf);
            writeowner(w, *u);
            res.json(buf.getbuf(), buf.length());
        }
        DELETEP(v);
    }

    // POST /signin/account/email with the new address, which has to confirm it
    static void routeowneremail(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        if(!*masteraccounturl) { res.error(503, "email changes aren't set up"); return; }
        jsonvalue *v = signinbody(req);
        const char *mail = signinfield(req, v, "email");
        string email = "";
        if(mail) { copystring(email, mail); lowerstring(email); }
        if(!mail || !validemail(email)) res.error(400, "that isn't an email address");
        else if(u->email && !strcasecmp(u->email, email)) res.error(400, "that's already your email address");
        else if(emailtaken(email)) res.error(409, "that email address already has an account");
        else if(!dailycount(accounts.resets, masterresetdaily, req.ip, true)) res.error(429, "too many changes from your address today");
        else
        {
            loopvrev(accounts.tickets) if(accounts.tickets[i]->kind == TICKET_EMAIL && !strcmp(accounts.tickets[i]->handle, u->name)) dropticket(accounts.tickets[i]);
            ticket *t = new ticket;
            t->kind = TICKET_EMAIL;
            copystring(t->handle, u->name);
            copystring(t->email, email);
            copystring(t->addr, req.addr);
            t->created = time(NULL);
            t->expires = t->created + time_t(masterresethours)*60*60;
            string token, canceltoken;
            newtoken(token, t->hash);
            newtoken(canceltoken, t->cancelhash);
            addticket(t);
            defformatstring(subject, "Confirm the new email address of your %s account %s", versionname, t->handle);
            defformatstring(confirmurl, "%s/email#%s", masteraccounturl, token);
            bigstring text;
            formatstring(text,
                "Hello,\n\n"
                "The %s account \"%s\" asked to use this email address from now on.\n\n"
                "To confirm it, follow this link within %d hours:\n%s\n\n"
                "If this wasn't you, ignore this email and nothing changes.\n\n"
                "%s\nhttps://%s\n",
                versionname, t->handle, masterresethours, confirmurl, versionname, versionurl);
            tagval args[3];
            args[0].setcstr(t->handle);
            args[1].setint(masterresethours);
            args[2].setcstr(confirmurl);
            queuemaildef("emailconfirm", t->email, subject, text, args, 3);
            if(u->email && validemail(u->email))
            {
                defformatstring(oldsubject, "The email address of your %s account %s is changing", versionname, t->handle);
                defformatstring(cancelurl, "%s/email-cancel#%s", masteraccounturl, canceltoken);
                formatstring(text,
                    "Hello,\n\n"
                    "The %s account \"%s\" asked to use another email address, %s, instead of this one. "
                    "It changes once that address confirms it.\n\n"
                    "If this wasn't you, follow this link to stop it, then reset the account's key to sign out whoever asked:\n%s\n\n"
                    "%s\nhttps://%s\n",
                    versionname, t->handle, t->email, cancelurl, versionname, versionurl);
                args[0].setcstr(t->handle);
                args[1].setcstr(t->email);
                args[2].setcstr(cancelurl);
                queuemaildef("emailcancel", u->email, oldsubject, text, args, 3);
            }
            memset(token, 0, sizeof(token));
            memset(canceltoken, 0, sizeof(canceltoken));
            noticef("account", NOTICE_INFO, "Email change of '%s' asked for from %s", t->handle, req.addr);
            res.status = 202;
        }
        DELETEP(v);
    }

    // POST /accounts/email/confirm, or /accounts/email/cancel from the old address
    static void routeemailconfirm(httprequest &req, httpresponse &res, bool cancel)
    {
        if(!accountready(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *token = signinfield(req, v, "token");
        ticket *t = findticket(token, TICKET_EMAIL, cancel ? 1 : 0);
        authuser *u = t ? authusers.access(t->handle) : NULL;
        if(!t) { signinfailed(req.ip); res.error(404, "that link is unknown, ran out or was already used"); }
        else if(!u) { res.error(404, "that account no longer exists"); dropticket(t); }
        else if(cancel)
        {
            noticef("account", NOTICE_INFO, "The email change of '%s' was stopped from its old address, from %s", t->handle, req.addr);
            dropticket(t);
            res.status = 204;
        }
        else
        {
            bool taken = false;
            enumerate(authusers, authuser, o, if(&o != u && o.email && !strcasecmp(o.email, t->email)) taken = true);
            if(taken) { res.error(409, "that email address has an account now"); dropticket(t); }
            else
            {
                noticef("account", NOTICE_INFO, "Changed the email address of '%s' from %s, asked for from %s", u->name, req.addr, t->addr);
                delete[] u->email;
                u->email = newstring(t->email);
                dirtymasterfile(MFILE_USER);
                vector<char> buf;
                jsonwriter w(buf);
                w.beginobject();
                w.utf8("handle", u->name);
                w.utf8("email", u->email);
                w.endobject();
                res.json(buf.getbuf(), buf.length());
                dropticket(t);
            }
        }
        DELETEP(v);
    }
    static void routeemailconfirm(httprequest &req, httpresponse &res) { routeemailconfirm(req, res, false); }
    static void routeemailcancel(httprequest &req, httpresponse &res) { routeemailconfirm(req, res, true); }

    // DELETE /signin/account, with the handle sent back to show it's meant
    static void routeownerdelete(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        jsonvalue *v = signinbody(req);
        const char *confirm = signinfield(req, v, "confirm");
        if(!confirm || strcmp(confirm, u->name)) res.error(400, "send your handle as confirm to delete your account");
        else if(accountpriv(u->flags) >= PRIV_MODERATOR) res.error(403, "accounts with a standing are removed by an administrator");
        else
        {
            string handle, email, links;
            copystring(handle, u->name);
            copystring(email, u->email ? u->email : "");
            copystring(links, accountlinks(*u));
            loopvrev(accounts.tickets) if(!strcmp(accounts.tickets[i]->handle, handle)) dropticket(accounts.tickets[i]);
            removeauth(authusers, handle);
            dirtymasterfile(MFILE_USER);
            signincheck = true;
            noticef("account", NOTICE_INFO, "The account '%s'%s was deleted by its owner from %s", handle, links, req.addr);
            if(validemail(email))
            {
                defformatstring(subject, "Your %s account %s was deleted", versionname, handle);
                bigstring text;
                formatstring(text,
                    "Hello,\n\n"
                    "The %s account \"%s\" was deleted, as asked for by someone signed in to it. Its key no longer works, "
                    "and its handle is free for anyone to register.\n\n"
                    "%s\nhttps://%s\n", versionname, handle, versionname, versionurl);
                tagval args[1];
                args[0].setcstr(handle);
                queuemaildef("deleted", email, subject, text, args, 1);
            }
            res.status = 204;
        }
        DELETEP(v);
    }

    // ahead of the signin routes, as DELETE /signin/<id> would take /signin/account
    void setupowner()
    {
        http::addroute(HTTP_GET, SIGNIN_PATH "/account", HTTP_PUBLIC, routeowner);
        http::addroute(HTTP_DELETE, SIGNIN_PATH "/account", HTTP_PUBLIC, routeownerdelete);
        http::addroute(HTTP_PATCH, SIGNIN_PATH "/account", HTTP_PUBLIC, routeownerset);
        http::addroute(HTTP_POST, SIGNIN_PATH "/account/email", HTTP_PUBLIC, routeowneremail);
        setuplinks();
    }

    void setupaccounts()
    {
        http::addroute(HTTP_GET, ACCOUNT_PATH "/discord/:id", HTTP_PUBLIC, routediscordlookup);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/email/confirm", HTTP_PUBLIC, routeemailconfirm);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/email/cancel", HTTP_PUBLIC, routeemailcancel);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/reset", HTTP_PUBLIC, routereset);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/reset/confirm", HTTP_PUBLIC, routeresetconfirm);
        http::addroute(HTTP_GET, ACCOUNT_PATH, HTTP_PUBLIC, routeaccounts);
        http::addroute(HTTP_POST, ACCOUNT_PATH, HTTP_PUBLIC, routeregister);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/activate", HTTP_PUBLIC, routeactivate);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/server", HTTP_PUBLIC, routeserveractivate);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/cancel", HTTP_PUBLIC, routecancel);
        http::addroute(HTTP_POST, ACCOUNT_PATH "/pickup", HTTP_PUBLIC, routepickup);
        loadtickets();
    }
}
