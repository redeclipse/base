#ifdef WIN32
#define FD_SETSIZE 4096
#else
#include <sys/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 4096
#endif

#include "engine.h"
#include <enet/time.h>
#include "master.h"

#define POLL_MISSES 3 // polls in a row a server can leave unanswered before its polled status is dropped
#define POLL_DRIFT 2 // seconds the end of a match can move between polls without being a change
#define POLL_PLAYING 5 // the playing and overtime game states, numbered the same in every version
#define POLL_OVERTIME 6
#define POLL_STATEMILLIS 266 // the game version whose time left in a state is in milliseconds, where earlier ones send seconds
#define POLL_LEFTSECONDS 278 // the game version that sent the time remaining in seconds again, after 266 sent it in milliseconds

namespace master
{
    VAR(0, masterserver, 0, 0, 1);
    VAR(0, masterport, 1, MASTER_PORT, VAR_MAX);
    VAR(0, masterminver, 0, 0, CUR_VERSION);

    VAR(0, mastermainver, 0, 0, CUR_VERSION);
    VAR(0, mastergamever, 0, 0, VAR_MAX);

    SVAR(0, masterip, "");
    SVAR(0, masterscriptclient, "");
    SVAR(0, masterscriptserver, "");

    VAR(0, masterduplimit, 0, 3, VAR_MAX);
    VAR(0, masterauthlimit, 0, MAXCLIENTS, VAR_MAX); // maximum pending auth requests per peer, 0 = unlimited
    VAR(0, masterpingdelay, 1000, 3000, VAR_MAX);
    VAR(0, masterpingtries, 1, 5, VAR_MAX);
    VAR(0, masterpollinterval, 0, 30, VAR_MAX); // seconds between queries to listed servers that report no status, 0 = never

    vector<masterclient *> masterclients;
    mastersecretstate mastersecrets;
    ICOMMAND(0, mastersmtppass, "sN", (char *s, int *n), if(*n > 0) copystring(mastersecrets.smtppass, s)); // for Gmail an App Password, kept in a file that isn't shared
    ICOMMAND(0, masterturnstilesecret, "sN", (char *s, int *n), if(*n > 0) copystring(mastersecrets.turnstile, s)); // Cloudflare Turnstile's secret key, without it the website can't register
    ICOMMAND(0, masterdiscordsecret, "sN", (char *s, int *n), if(*n > 0) copystring(mastersecrets.discord, s)); // the Discord application's client secret

    static ENetSocket mastersocket = ENET_SOCKET_NULL, pingsocket = ENET_SOCKET_NULL;
    static time_t starttime;

    void masterout(masterclient &c, const char *msg, int len)
    {
        if(!len) len = strlen(msg);
        c.output.put(msg, len);
    }

    void masteroutf(masterclient &c, const char *fmt, ...)
    {
        bigstring msg;
        va_list args;
        va_start(args, fmt);
        vformatstring(msg, fmt, args);
        va_end(args);
        masterout(c, msg);
    }

    const char *noticedate(time_t t)
    {
        static string s;
        if(!strftime(s, sizeof(s), "%Y-%m-%d", gmtime(&t))) copystring(s, "?");
        return s;
    }

    // the Steam and Discord accounts an Admin notice names an account by, so it can be found there
    const char *accountlinks(authuser &u)
    {
        static string s;
        s[0] = '\0';
        if(u.steamid && *u.steamid) concformatstring(s, ", Steam %s", u.steamid);
        if(u.discord && *u.discord) concformatstring(s, ", Discord %s", u.discord);
        return s;
    }

    // what an Admin notice says of a server beyond its address
    const char *serverdetails(masterclient &c)
    {
        static bigstring s;
        string desc, branch;
        filterstring(desc, c.desc);
        filterstring(branch, c.branch[0] ? c.branch : "?");
        formatstring(s, "'%s', version %d, branch %s", desc, c.version, branch);
        if(c.authhandle[0]) concformatstring(s, ", server account '%s'", c.authhandle);
        if(c.numplayers >= 0) concformatstring(s, ", %d player(s)", c.numplayers);
        return s;
    }

    bool setuppingsocket(ENetAddress *address)
    {
        if(pingsocket != ENET_SOCKET_NULL) return true;
        pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pingsocket == ENET_SOCKET_NULL) return false;
        if(address && enet_socket_bind(pingsocket, address) < 0) return false;
        enet_socket_set_option(pingsocket, ENET_SOCKOPT_NONBLOCK, 1);
        return true;
    }

    void setup()
    {
        if(masterserver)
        {
            conoutf(colourwhite, "Loading master (%s:%d)..", *masterip ? masterip : "*", masterport);
            ENetAddress address = { ENET_HOST_ANY, enet_uint16(masterport) };
            if(*masterip && enet_address_set_host(&address, masterip) < 0) fatal("Failed to resolve master address: %s", masterip);
            if((mastersocket = enet_socket_create(ENET_SOCKET_TYPE_STREAM)) == ENET_SOCKET_NULL) fatal("Failed to create master server socket");
            if(enet_socket_set_option(mastersocket, ENET_SOCKOPT_REUSEADDR, 1) < 0) fatal("Failed to set master server socket option");
            if(enet_socket_bind(mastersocket, &address) < 0) fatal("Failed to bind master server socket");
            if(enet_socket_listen(mastersocket, -1) < 0) fatal("Failed to listen on master server socket");
            if(enet_socket_set_option(mastersocket, ENET_SOCKOPT_NONBLOCK, 1) < 0) fatal("Failed to make master server socket non-blocking");
            if(!setuppingsocket(&address)) fatal("Failed to create ping socket");
            http::addroute(HTTP_GET, HTTP_API "/servers", HTTP_PUBLIC, routeservers);
            http::addroute(HTTP_GET, HTTP_API "/servers/:address/:port/history", HTTP_PUBLIC, routehistory);
            http::addroute(HTTP_GET, HTTP_API "/history", HTTP_PUBLIC, routeglobalhistory);
            http::addroute(HTTP_GET, "/join/:address/:port", HTTP_PUBLIC, routejoin);
            http::addroute(HTTP_GET, HTTP_API "/queue", HTTP_PUBLIC, routequeue);
            http::addsocket(HTTP_API "/live", HTTP_PUBLIC, viewerevent, NULL, 4096);
            http::addsocket(HTTP_API "/servers/live", HTTP_PUBLIC, viewerevent, NULL, 4096); // its old path, which released games use
            http::addsocket(HTTP_API "/link", HTTP_PUBLIC, linkevent);
            // the feed first, as routes match in the order they were added
            http::addsocket(HTTP_RELAY "/:address/:port" HTTP_API "/events", HTTP_PUBLIC, relayviewerevent, NULL, 4096);
            for(int m = HTTP_GET; m < HTTP_MAXMETHODS; m++) if(m != HTTP_HEAD && m != HTTP_OPTIONS)
            {
                http::addroute(m, HTTP_RELAY "/:address/:port", HTTP_PUBLIC, relayroute);
                http::addroute(m, HTTP_RELAY "/:address/:port/*", HTTP_PUBLIC, relayroute);
            }
            loadhistory();
            loadqueue();
            setupsignin();
            starttime = clocktime;
            conoutf(colourwhite, "Master server started on %s:[%d]", *masterip ? masterip : "localhost", masterport);
        }
    }

    hashnameset<authuser> authusers;

    // a public key as genkey prints it, a sign then hex digits
    bool validpubkey(const char *key)
    {
        if(*key == '+' || *key == '-') key++;
        int len = 0;
        for(; *key; key++, len++) if(!isxdigit(uchar(*key))) return false;
        return len > 0 && len <= 256;
    }


    // a display name as names in game are, which never holds what could pass for a handle beside it
    void displayfilter(char *dst, const char *src)
    {
        filterstring(dst, src ? src : "", true, true, true, true, true, MAXNAMELEN);
    }

    // the same from UTF-8, as JSON brings
    void displayfromutf8(char *dst, const char *src)
    {
        string decoded;
        size_t len = decodeutf8((uchar *)decoded, sizeof(decoded)-1, (const uchar *)src, strlen(src));
        decoded[len] = '\0';
        displayfilter(dst, decoded);
    }

    void descfromutf8(char *dst, const char *src, size_t maxlen)
    {
        string decoded;
        size_t len = decodeutf8((uchar *)decoded, sizeof(decoded)-1, (const uchar *)(src ? src : ""), src ? strlen(src) : 0);
        decoded[len] = '\0';
        filterstring(dst, decoded, true, true, true, true, false, maxlen);
    }

    // the one letter an account keeps, its highest Privilege's from any it was given in either case, o read as m, and u if none
    char userflag(const char *flags)
    {
        static const char ranks[] = "usmadc";
        int best = 0;
        for(const char *c = flags ? flags : ""; *c; c++)
        {
            char l = tolower(uchar(*c));
            if(l == 'o') l = 'm';
            const char *r = l ? strchr(ranks, l) : NULL;
            if(r && int(r - ranks) + 1 > best) best = int(r - ranks) + 1;
        }
        return best ? ranks[best - 1] : 'u';
    }

    // a server account's one level: m Official and u Featured, anything else b Basic
    char serverflag(const char *flags)
    {
        if(flags && strpbrk(flags, "mM")) return 'm';
        if(flags && strpbrk(flags, "uU")) return 'u';
        return 'b';
    }

    // a SteamID64, the only thing a link stores; older files hold placeholders such as 0 for none
    bool validsteamid(const char *id) { return id && strlen(id) == 17 && strspn(id, "0123456789") == 17; }

    bool newauth(const char *name, const char *flags, const char *pubkey, const char *email, const char *steamid, const char *discord, const char *display)
    {
        if(!name || !*name || !flags || !*flags || !pubkey || !validpubkey(pubkey)) return false;
        string authname;
        if(filterstring(authname, name, true, true, true, true, false, 100)) name = authname;
        if(authusers.access(name))
        {
            conoutf(colourred, "Auth handle '%s' already exists, skipping (%s)", name, email);
            return false;
        }
        char flagbuf[2] = { userflag(flags), '\0' };
        char *key = newstring(name);
        authuser &u = authusers[key];
        u.name = key;
        u.flags = newstring(flagbuf);
        u.pubkey = parsepubkey(pubkey);
        u.key = newstring(pubkey);
        u.email = newstring(email ? email : "");
        if(steamid && *steamid && !validsteamid(steamid))
        {
            conoutf(colouryellow, "Account '%s' had an invalid Steam ID '%s', treating it as unlinked", u.name, steamid);
            steamid = NULL;
        }
        u.steamid = newstring(steamid ? steamid : "");
        u.discord = newstring(discord ? discord : "");
        string shown;
        displayfilter(shown, display);
        u.display = newstring(shown);
        return true;
    }

    hashnameset<authuser> serverauthusers;

    bool newserverauth(const char *name, const char *flags, const char *pubkey, const char *desc, const char *owner, const char *addrs)
    {
        if(!name || !*name || !pubkey || !validpubkey(pubkey)) return false;
        string authname;
        if(filterstring(authname, name, true, true, true, true, false, 100)) name = authname;
        if(serverauthusers.access(name))
        {
            conoutf(colourred, "Server auth handle '%s' already exists, skipping (%s)", name, owner && *owner ? owner : (desc ? desc : ""));
            return false;
        }
        char *key = newstring(name);
        authuser &u = serverauthusers[key];
        u.name = key;
        char flagbuf[2] = { serverflag(flags), '\0' };
        u.flags = newstring(flagbuf);
        u.pubkey = parsepubkey(pubkey);
        u.key = newstring(pubkey);
        u.desc = newstring(desc ? desc : "");
        u.owner = newstring(owner ? owner : "");
        u.addrs = addrs ? newstring(addrs) : NULL;
        return true;
    }

    void freeauth(authuser &u)
    {
        delete[] u.name;
        delete[] u.flags;
        delete[] u.email;
        delete[] u.desc;
        delete[] u.owner;
        delete[] u.steamid;
        delete[] u.key;
        delete[] u.discord;
        delete[] u.display;
        delete[] u.addrs;
        freepubkey(u.pubkey);
    }

    void clearauth()
    {
        enumerate(authusers, authuser, u, freeauth(u));
        authusers.clear();
        enumerate(serverauthusers, authuser, u, freeauth(u));
        serverauthusers.clear();
    }
    COMMAND(0, clearauth, "");


    // the account a request was made for, unless a rehash has since removed it or changed its key
    static authuser *findauth(hashnameset<authuser> &users, authreq &a)
    {
        authuser *u = users.access(a.name);
        return u && u->key && !strcmp(u->key, a.key) ? u : NULL;
    }

    void purgeauths(masterclient &c)
    {
        int expired = 0;
        loopv(c.authreqs)
        {
            if(ENET_TIME_DIFFERENCE(totalmillis, c.authreqs[i].reqtime) >= AUTH_TIME)
            {
                masteroutf(c, "failauth %u\n", c.authreqs[i].id);
                if(c.authreqs[i].answer) freechallenge(c.authreqs[i].answer);
                expired = i + 1;
            }
            else break;
        }
        if(expired > 0) c.authreqs.remove(0, expired);

        if(c.serverauthreq.reqtime && ENET_TIME_DIFFERENCE(totalmillis, c.serverauthreq.reqtime) >= AUTH_TIME)
        {
            masteroutf(c, "failserverauth\n");
            if(c.serverauthreq.answer) freechallenge(c.serverauthreq.answer);
            c.serverauthreq.reset();
        }
    }

    void reqauth(masterclient &c, uint id, char *name, char *hostname)
    {
        purgeauths(c);

        string ip, host, shown;
        if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");
        filterstring(host, hostname && *hostname ? hostname : "-"); // both as the server sent them
        filterstring(shown, name);

        authuser *u = authusers.access(name);
        if(!u)
        {
            masteroutf(c, "failauth %u\n", id);
            noteauthfail(c);
            foldnoticef("signin", NOTICE_WARNING, tempformatstring("notfound %s", shown), "Failed '%s' (%u) from %s on server %s:%d (NOTFOUND)", shown, id, host, ip, c.port);
            return;
        }
        if(masterauthlimit && c.authreqs.length() >= masterauthlimit)
        {
            masteroutf(c, "failauth %u\n", id);
            noteauthfail(c);
            foldnoticef("signin", NOTICE_WARNING, tempformatstring("limit %s", ip), "Failed '%s' (%u) from %s on server %s:%d (LIMIT)", shown, id, host, ip, c.port);
            return;
        }
        conoutf(colourwhite, "Attempting '%s' (%u) from %s on server %s\n", name, id, host, ip);

        authreq &a = c.authreqs.add();
        copystring(a.name, u->name);
        copystring(a.key, u->key);
        a.reqtime = totalmillis;
        a.id = id;
        copystring(a.hostname, host);
        uchar seed[32]; // from the system, as a guessable challenge could be answered without the key
        http::randombytes(seed, sizeof(seed));
        static vector<char> buf;
        buf.setsize(0);
        a.answer = genchallenge(u->pubkey, seed, sizeof(seed), buf);

        masteroutf(c, "chalauth %u %s\n", id, buf.getbuf());
    }

    void reqserverauth(masterclient &c, char *name)
    {
        purgeauths(c);

        if(c.serverauthreq.reqtime) return;

        string ip;
        if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");

        authuser *u = serverauthusers.access(name);
        if(!u)
        {
            masteroutf(c, "failserverauth\n");
            noteauthfail(c);
            string shown;
            filterstring(shown, name);
            foldnoticef("signin", NOTICE_WARNING, tempformatstring("server notfound %s", ip), "Failed server '%s' from %s:%d (NOTFOUND)", shown, ip, c.port);
            return;
        }
        conoutf(colourwhite, "Attempting server '%s'\n", name);

        copystring(c.serverauthreq.name, u->name);
        copystring(c.serverauthreq.key, u->key);
        c.serverauthreq.reqtime = totalmillis;
        uchar seed[32]; // from the system, as a guessable challenge could be answered without the key
        http::randombytes(seed, sizeof(seed));
        static vector<char> buf;
        buf.setsize(0);
        c.serverauthreq.answer = genchallenge(u->pubkey, seed, sizeof(seed), buf);

        masteroutf(c, "chalserverauth %s\n", buf.getbuf());
    }

    void confauth(masterclient &c, uint id, const char *val)
    {
        purgeauths(c);
        string ip;
        if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");
        loopv(c.authreqs) if(c.authreqs[i].id == id)
        {
            authuser *u = findauth(authusers, c.authreqs[i]);
            if(!u)
            {
                masteroutf(c, "failauth %u\n", id);
                foldnoticef("signin", NOTICE_WARNING, tempformatstring("changed %s", c.authreqs[i].name), "Failed '%s' (%u) from %s on server %s:%d (CHANGED)", c.authreqs[i].name, id, c.authreqs[i].hostname, ip, c.port);
            }
            else if(checkchallenge(val, c.authreqs[i].answer))
            {
                masteroutf(c, "succauth %u \"%s\" \"%s\"\n", id, u->name, u->flags);
                bool known = false;
                loopvj(c.verified) if(!strcmp(c.verified[j], u->name)) { known = true; break; }
                if(!known) { if(c.verified.length() >= MASTER_VERIFIED) delete[] c.verified.remove(0); c.verified.add(newstring(u->name)); }
                conoutf(colourwhite, "Succeeded '%s' [%s] (%u) from %s on server %s\n", u->name, u->flags, id, c.authreqs[i].hostname, ip);
            }
            else
            {
                masteroutf(c, "failauth %u\n", id);
                noteauthfail(c);
                foldnoticef("signin", NOTICE_WARNING, tempformatstring("badkey %s", u->name), "Failed '%s' (%u) from %s on server %s:%d (BADKEY)", u->name, id, c.authreqs[i].hostname, ip, c.port);
            }
            if(c.authreqs[i].answer) freechallenge(c.authreqs[i].answer);
            c.authreqs.remove(i--);
            return;
        }
        masteroutf(c, "failauth %u\n", id);
    }

    void relayclosed(masterclient &c);

    void purgemasterclient(int n)
    {
        masterclient &c = *masterclients[n];
        bool waslisted = c.listserver;
        enet_uint32 host = c.address.host;
        string handle;
        copystring(handle, c.authhandle);
        if(c.link >= 0) { relayclosed(c); signinlinkclosed(c); http::wsclose(c.link); }
        else enet_socket_destroy(c.socket);
        if(verbose || c.isserver) conoutf(colourwhite, "Master peer %s disconnected", c.name);
        delete masterclients[n];
        masterclients.remove(n);
        if(waslisted) marksiblingschanged(host, handle);
    }

    // known addresses: where each server account has signed in from, so the administrators hear when one signs in from anywhere else
    VAR(0, masterserveraddrs, 1, 8, 64); // known addresses kept for each server account, the oldest going first
    #define KNOWN_RESAVE (24*60*60) // seconds before a known address seen again is saved with its new time

    void readknownaddrs(const char *s, vector<knownaddr> &out)
    {
        out.setsize(0);
        if(!s) return;
        vector<char *> words;
        explodelist(s, words);
        loopv(words)
        {
            char *colon = strrchr(words[i], ':');
            if(!colon || colon == words[i]) continue;
            *colon = '\0';
            knownaddr &k = out.add();
            copystring(k.host, words[i]);
            k.seen = time_t(max(atoll(colon + 1), 0LL));
        }
        words.deletearrays();
    }

    static char *writeknownaddrs(vector<knownaddr> &addrs)
    {
        vector<char> buf;
        loopv(addrs)
        {
            defformatstring(word, "%s%s:%lld", i ? " " : "", addrs[i].host, (long long)addrs[i].seen);
            buf.put(word, strlen(word));
        }
        buf.add('\0');
        return newstring(buf.getbuf());
    }

    // where an Admin notice says a server account last signed in from, if anywhere
    const char *knownlast(authuser &u)
    {
        static string s;
        s[0] = '\0';
        vector<knownaddr> addrs;
        readknownaddrs(u.addrs, addrs);
        if(addrs.length()) formatstring(s, ", last signed in from %s on %s", addrs[0].host, addrs[0].seen > 0 ? noticedate(addrs[0].seen) : "?");
        return s;
    }

    // a server account that signed in: a host it hasn't used before is an Admin notice, unless its addresses aren't kept yet, as for
    // one saved before they were kept, which keeps the first quietly
    static void serverauthseen(authuser &u, masterclient &c)
    {
        time_t now = time(NULL);
        vector<knownaddr> addrs;
        readknownaddrs(u.addrs, addrs);
        int found = -1;
        loopv(addrs) if(!strcmp(addrs[i].host, c.name)) { found = i; break; }
        if(found >= 0)
        {
            bool resave = now - addrs[found].seen >= KNOWN_RESAVE;
            knownaddr k = addrs.remove(found);
            k.seen = now;
            addrs.insert(0, k);
            DELETEA(u.addrs);
            u.addrs = writeknownaddrs(addrs);
            if(resave) dirtymasterfile(MFILE_SERV);
            return;
        }
        if(u.addrs)
        {
            if(addrs.empty()) noticef("serveraccount", NOTICE_WARNING, "Server account '%s' signed in for the first time, from %s:%d (%s)", u.name, c.name, c.port, serverdetails(c));
            else
            {
                bigstring known = "";
                loopv(addrs) concformatstring(known, "%s%s (%s)", i ? ", " : "", addrs[i].host, addrs[i].seen > 0 ? noticedate(addrs[i].seen) : "?");
                noticef("serveraccount", NOTICE_WARNING, "Server account '%s' signed in from a new address, %s:%d (%s), where before it signed in from %s", u.name, c.name, c.port, serverdetails(c), known);
            }
        }
        knownaddr k;
        copystring(k.host, c.name);
        k.seen = now;
        addrs.insert(0, k);
        while(addrs.length() > masterserveraddrs) addrs.pop();
        DELETEA(u.addrs);
        u.addrs = writeknownaddrs(addrs);
        dirtymasterfile(MFILE_SERV);
    }

    void confserverauth(masterclient &c, const char *val)
    {
        if(!c.serverauthreq.reqtime || !c.serverauthreq.name[0] || !c.serverauthreq.answer)
        {
            masteroutf(c, "failserverauth\n");
            return;
        }

        string ip;
        if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");

        authuser *u = findauth(serverauthusers, c.serverauthreq);
        if(!u)
        {
            masteroutf(c, "failserverauth\n");
            foldnoticef("signin", NOTICE_WARNING, tempformatstring("server changed %s", c.serverauthreq.name), "Failed server '%s' from %s:%d (CHANGED)", c.serverauthreq.name, ip, c.port);
        }
        else if(checkchallenge(val, c.serverauthreq.answer))
        {
            masteroutf(c, "succserverauth \"%s\" \"%s\"\n", u->name, u->flags);
            conoutf(colourwhite, "Succeeded server '%s' [%s]\n", u->name, u->flags);
            copystring(c.authhandle, u->name);
            copystring(c.flags, u->flags);
            serverauthseen(*u, c);
            c.changed = true;
            if(c.listserver) marksiblingschanged(c.address.host, c.authhandle);
        }
        else
        {
            masteroutf(c, "failserverauth\n");
            noteauthfail(c);
            foldnoticef("signin", NOTICE_WARNING, tempformatstring("server badkey %s", u->name), "Failed server '%s' from %s:%d (BADKEY)", u->name, ip, c.port);
        }

        if(c.serverauthreq.answer) freechallenge(c.serverauthreq.answer);

        c.serverauthreq.reset();
    }

    // asks a server what it is playing, as the server browser does; the time it echoes is a random token, so only its own answer counts
    static void sendquery(masterclient &c)
    {
        ucharbuf p(c.pollreq, sizeof(c.pollreq));
        uint token = 0; // from the system, so it shows nothing of a generator anything else uses
        http::randombytes((uchar *)&token, sizeof(token));
        putint(p, int(token&0x3FFFFFFF)|1);
        c.pollreqlen = p.length();
        ENetBuffer buf;
        buf.data = c.pollreq;
        buf.dataLength = c.pollreqlen;
        ENetAddress addr = { c.address.host, enet_uint16(c.port+1) };
        enet_socket_send(pingsocket, &addr, &buf, 1);
    }

    // getstring stops quietly at the end of a packet, so a string counts only when its terminator was there
    static bool pollstring(char *text, ucharbuf &p)
    {
        if(!p.remaining()) return false;
        getstring(text, p, MAXSTRLEN);
        return p.len > 0 && !p.buf[p.len-1];
    }

    // every version sends a name as its colour, Privilege icon and team colour before the name itself, as the server browser shows it
    struct pollplayer
    {
        string name, handle, privilege;
        int colour, teamcolour;
        bool local;
    };

    static void pollname(pollplayer &pp, const char *text)
    {
        pp.colour = pp.teamcolour = -1;
        pp.privilege[0] = '\0';
        pp.local = false;
        const char *s = text;
        if(s[0] == '\f' && s[1] == 's') s += 2;
        int colours = 0;
        while(s[0] == '\f' && (s[1] == '[' || s[1] == '('))
        {
            const char *end = strchr(s+2, s[1] == '[' ? ']' : ')');
            if(!end) break;
            if(s[1] == '[')
            {
                char *num = NULL;
                int c = int(strtol(s+2, &num, 10));
                if(num == end && c >= 0 && c <= 0xFFFFFF)
                {
                    if(!colours) pp.colour = c;
                    else if(colours == 1) pp.teamcolour = c;
                }
                colours++;
            }
            else if(!strncmp(s+2, "$priv", 5) && end-s-7 > 3 && !strncmp(end-3, "tex", 3))
            {
                const char *word = s+7;
                if(!strncmp(word, "local", 5) && end-3 > word+5) { word += 5; pp.local = true; }
                int len = min(int(end-3-word), MAXSTRLEN-1);
                loopi(len) pp.privilege[i] = isalpha(word[i]) ? word[i] : '\0';
                pp.privilege[len] = '\0';
            }
            s = end+1;
        }
        filterstring(pp.name, s, true, true, true, true);
    }

    // what a listed server says it's playing: its map goes in the log when it first says one and whenever it changes,
    // and Master shield looks at it again when any of it changes
    void noteserverstate(masterclient &c, const char *map, int mode, int muts, const vector<const char *> &names)
    {
        if(!c.isserver) return;
        if(strcmp(c.lastmap, map))
        {
            if(c.lastmap[0] && *map)
            {
                int now = int(time(NULL));
                while(c.mapchanges.length() && now - c.mapchanges[0] >= 3600) c.mapchanges.remove(0);
                c.mapchanges.add(now);
            }
            copystring(c.lastmap, map);
            conoutf(colourwhite, "Master peer %s on map '%s' (%d)", c.name, c.lastmap, c.port);
            c.changed = true;
        }
        if(mode != c.lastmode || muts != c.lastmuts) { c.lastmode = mode; c.lastmuts = muts; c.changed = true; }
        bool same = names.length() == c.lastnames.length();
        if(same) loopv(names) if(strcmp(names[i], c.lastnames[i])) { same = false; break; }
        if(!same)
        {
            c.lastnames.deletearrays();
            loopv(names) c.lastnames.add(newstring(names[i]));
            c.changed = true;
        }
    }

    // the query reply has kept one layout since v1.6, so this needs no game knowledge beyond the unit of a state's time left; numbers are passed on as the server's version means them
    static bool parsepoll(masterclient &c, uchar *data, int len)
    {
        if(!c.pollreqlen || len <= c.pollreqlen || memcmp(data, c.pollreq, c.pollreqlen)) return false;
        ucharbuf p(data, len);
        p.len = c.pollreqlen;
        int numplayers = getint(p), numattr = getint(p);
        if(p.overread() || numplayers < 0 || numplayers > MAXCLIENTS || numattr < 14) return false;
        int attr[15] = { 0 };
        loopi(numattr) // the count is the sender's word, so it stops where the packet does
        {
            int a = getint(p);
            if(p.overread()) break;
            if(i < 15) attr[i] = a;
        }
        if(p.overread()) return false;
        string map, text;
        if(!pollstring(text, p)) return false;
        filterstring(map, text, false);
        if(!pollstring(text, p)) return false; // the description, which the list has from registration
        if(attr[0] >= 227 && !pollstring(text, p)) return false; // the branch, likewise
        static vector<pollplayer> players;
        players.setsize(0);
        loopi(numplayers)
        {
            if(!pollstring(text, p)) return false;
            pollplayer &pp = players.add();
            pollname(pp, text);
            pp.handle[0] = '\0';
        }
        if(attr[0] >= 225) loopi(numplayers)
        {
            if(!pollstring(text, p)) return false;
            filterstring(players[i].handle, text, true, true, true, true);
            string shown;
            displayfilter(shown, players[i].name);
            fillname(c, players[i].handle, shown);
        }

        // seconds left, though newer servers send 0 for a match without a limit, as older ones send -1
        bool playing = attr[13] == POLL_PLAYING || attr[13] == POLL_OVERTIME;
        // except from 266 into 278, which sent milliseconds; while playing the state's time left is the same number then, as it's
        // the time remaining in milliseconds in every version since, so that tells builds of 278 apart; otherwise the version does
        int now = int(time(NULL)), left = attr[3];
        if(attr[0] >= POLL_STATEMILLIS && left > 0 && (playing ? left == attr[14] : attr[0] < POLL_LEFTSECONDS)) left /= 1000;
        int ends = left < 0 || (!left && attr[13] == POLL_PLAYING) ? -1 : now + left;
        // the time left only counts down while playing, so otherwise the end moves with every poll and is kept until the state changes
        if(c.polled && attr[13] == c.polledstate && (!playing || (ends >= 0 && c.polledends >= 0 && abs(ends - c.polledends) <= POLL_DRIFT))) ends = c.polledends;
        c.polledends = ends;
        // a state without a timer ends now, so that moves with the clock without being a change
        int stateends = now + max(attr[0] >= POLL_STATEMILLIS ? attr[14]/1000 : attr[14], 0);
        if(c.polled && attr[13] == c.polledstate && (abs(stateends - c.polledstateends) <= POLL_DRIFT || (stateends <= now && c.polledstateends <= now))) stateends = c.polledstateends;
        c.polledstateends = stateends;
        c.polledstate = attr[13];

        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.boolean("polled", true);
        w.num("gameversion", attr[0]);
        w.str("map", map);
        w.num("modeid", attr[1]);
        w.num("mutators", attr[2]);
        w.num("ends", double(ends));
        w.num("maxplayers", attr[4]);
        w.num("mastermode", attr[5]);
        w.num("gamevars", attr[6]);
        w.num("gamemods", attr[7]);
        w.beginarray("build");
        loopi(5) w.num(NULL, attr[8+i]);
        w.endarray();
        w.num("state", attr[13]);
        w.num("stateends", double(stateends));
        w.beginarray("players");
        loopv(players)
        {
            pollplayer &pp = players[i];
            w.beginobject();
            w.str("name", pp.name);
            if(pp.handle[0]) w.str("handle", pp.handle);
            if(pp.privilege[0]) w.str("privilege", pp.privilege);
            if(pp.local) w.boolean("local", true);
            if(pp.colour >= 0) w.num("colour", pp.colour);
            if(pp.teamcolour >= 0) w.num("teamcolour", pp.teamcolour);
            w.endobject();
        }
        w.endarray();
        w.endobject();
        if(buf.length() >= STATUS_LIMIT) return false;
        buf.add('\0');
        DELETEA(c.polled);
        c.polled = newstring(buf.getbuf());
        c.numplayers = players.length();
        jsonvalue *pst = json::parse(c.polled);
        if(pst)
        {
            vector<char> hashbuf;
            jsonwriter hw(hashbuf);
            writestatusfiltered(hw, pst, STATUS_INC_ALL, true);
            uint sh = listhash(hashbuf);
            if(sh != c.lastshieldhash) { c.lastshieldhash = sh; c.changed = true; }
            delete pst;
        }
        vector<const char *> names;
        loopv(players) names.add(players[i].name);
        noteserverstate(c, map, attr[1], attr[2], names);
        return true;
    }

    void checkmasterpongs()
    {
        ENetBuffer buf;
        ENetAddress addr;
        static uchar pong[MAXTRANS];
        for(;;)
        {
            buf.data = pong;
            buf.dataLength = sizeof(pong);
            int len = enet_socket_receive(pingsocket, &addr, &buf, 1);
            if(len <= 0) break;
            loopv(masterclients)
            {
                masterclient &c = *masterclients[i];
                if(c.address.host == addr.host && c.port+1 == addr.port)
                {
                    if(c.shouldping)
                    {
                        c.lastpong = totalmillis ? totalmillis : 1;
                        c.listserver = true;
                        c.shouldping = false;
                        c.lastpoll = c.lastpong; // its answer counts as the first poll
                        c.changed = true;
                        marksiblingschanged(c.address.host, c.authhandle);
                        masteroutf(c, "echo \"ping reply confirmed (on port %d), server is now listed\"\n", addr.port);
                        conoutf(colourwhite, "Master peer %s responded to ping request on port %d successfully", c.name, addr.port);
                    }
                    if(!c.reports() && parsepoll(c, pong, len))
                    {
                        c.pollwaiting = false;
                        c.pollmisses = 0;
                    }
                    break;
                }
            }
        }
    }

    static int controlversion = 0;

    int getcontrolversion() { return controlversion; }

    int nextcontrolversion()
    {
        ++controlversion;
        if(controlversion < 0)
        {
            controlversion = 0;
            loopv(masterclients) masterclients[i]->lastcontrol = -1;
            loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL) control[i].version = controlversion;
        }
        return controlversion;
    }

    // sends the whole list in place of the server's last copy, so entries removed here go there too
    static void sendcontrols(masterclient &c)
    {
        masteroutf(c, "clearcontrols\n");
        loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL)
            masteroutf(c, "%s %u %u %s\n", ipinfotypes[control[i].type], control[i].ip, control[i].mask, escapestring(control[i].reason ? control[i].reason : ""));
        c.lastcontrol = controlversion;
    }

    template<size_t N> static inline void statfilterstring(uchar (&dst)[N], const char *src, size_t len = 0)
    {
        char tmp[N];
        filterstring(tmp, src, true, true, true, false, false, len ? len : N-1);
        encodeutf8(dst, N-1, (uchar*)tmp, N-1);
    }

    bool checkmasterclientinput(masterclient &c)
    {
        if(c.inputpos < 0) return false;
        const int MAXWORDS = 24;
        char *w[MAXWORDS];
        int numargs = MAXWORDS;
        const char *p = c.input;
        for(char *end;; p = end)
        {
            end = (char *)memchr(p, '\n', &c.input[c.inputpos] - p);
            if(!end) end = (char *)memchr(p, '\0', &c.input[c.inputpos] - p);
            if(!end) break;
            *end++ = '\0';
            if(c.ishttp) continue; // eat up the rest of the bytes, we've done our bit

            //conoutf(colourwhite, "{%s} %s", c.name, p);
            loopi(MAXWORDS)
            {
                w[i] = (char *)"";
                if(i > numargs) continue;
                char *s = parsetext(p);
                if(s) w[i] = s;
                else numargs = i;
            }

            p += strcspn(p, ";\n\0"); p++;

            if(!strcmp(w[0], "GET") && w[1] && *w[1] == '/') // cheap server-to-http hack
            {
                loopi(numargs)
                {
                    if(i)
                    {
                        if(i == 1)
                        {
                            char *q = newstring(&w[i][1]);
                            delete[] w[i];
                            w[i] = q;
                        }
                        w[i-1] = w[i];
                    }
                    else delete[] w[i];
                }
                w[numargs-1] = (char *)"";
                numargs--;
                c.ishttp = c.shouldpurge = true;
            }
            bool found = false, server = !strcmp(w[0], "server");
            if((server || !strcmp(w[0], "quick")) && !c.ishttp)
            {
                c.port = SERVER_PORT;
                c.lastactivity = totalmillis ? totalmillis : 1;
                if(!server)
                {
                    c.isquick = true;
                    masteroutf(c, "echo \"session initiated, awaiting auth requests\"\n");
                    conoutf(colourwhite, "Master peer %s quickly checking auth request", c.name);
                }
                else
                {
                    if(w[1] && *w[1]) c.port = clamp(atoi(w[1]), 1, VAR_MAX);
                    ENetAddress address = { ENET_HOST_ANY, enet_uint16(c.port) };
                    c.version = w[3] && *w[3] ? atoi(w[3]) : (w[2] && *w[2] ? 150 : 0);
                    if(w[4] && *w[4]) copystring(c.desc, w[4], MAXSDESCLEN+1);
                    else formatstring(c.desc, "%s:[%d]", c.name, c.port);
                    copystring(c.branch, w[6] && *w[6] ? w[6] : "?", MAXBRANCHLEN+1);
                    // only an address, as a name would be looked up, waiting on whoever answers for it
                    if(w[2] && *w[2] && strcmp(w[2], "*") && (enet_address_set_host_ip(&address, w[2]) < 0 || address.host != c.address.host))
                    {
                        c.listserver = c.shouldping = false;
                        masteroutf(c, "echo \"server IP '%s' does not match origin '%s', server will not be listed\n", w[2], c.name);
                        string said;
                        filterstring(said, w[2], true, true, true, true, false, 64);
                        foldnoticef("listing", NOTICE_WARNING, tempformatstring("mismatch %s:%d", c.name, c.port), "Server %s:%d (%s) wasn't listed, as it said its address is '%s'", c.name, c.port, serverdetails(c), said);
                    }
                    else if(masterminver && c.version < masterminver)
                    {
                        c.listserver = c.shouldping = false;
                        masteroutf(c, "echo \"server version '%d' is no longer supported (need: %d), please update at %s\n", c.version, masterminver, versionurl);
                        foldnoticef("listing", NOTICE_WARNING, tempformatstring("version %s:%d", c.name, c.port), "Server %s:%d (%s) wasn't listed, as version %d is older than the %d it needs", c.name, c.port, serverdetails(c), c.version, masterminver);
                    }
                    else
                    {
                        c.shouldping = true;
                        c.numpings = c.lastping = 0; // servers repeat this every masterinterval, and an older ping would never be retried, which stopped polling for good
                        sendcontrols(c);
                        if(c.isserver)
                        {
                            masteroutf(c, "echo \"server updated (port %d), sending ping request (on port %d)\"\n", c.port, c.port+1);
                            conoutf(colourwhite, "Master peer %s updated server info (%d)", c.name, c.port);
                        }
                        else
                        {
                            if(*masterscriptserver) masteroutf(c, "%s\n", masterscriptserver);
                            masteroutf(c, "echo \"server registered (port %d), sending ping request (on port %d)\"\n", c.port, c.port+1);
                            conoutf(colourwhite, "Master peer %s registered as a server (%d)", c.name, c.port);
                            c.connected = int(time(NULL));
                            notereconnect(c.address.host);
                        }
                        c.isserver = true;
                        c.changed = true;
                    }
                }
                found = true;
            }
            if(!strcmp(w[0], "version") || !strcmp(w[0], "update"))
            {
                masteroutf(c, "setversion %d %d\n", mastermainver ? mastermainver : server::getver(0), mastergamever ? mastergamever : server::getver(1));
                if(*masterscriptclient && !strcmp(w[0], "update")) masteroutf(c, "%s\n", masterscriptclient);
                if(verbose) conoutf(colourwhite, "Master peer %s was sent the version", c.name);
                c.shouldpurge = found = true;
            }
            if(!strcmp(w[0], "list") || !strcmp(w[0], "update"))
            {
                int servs = 0;
                masteroutf(c, "clearservers\n");
                loopvj(masterclients)
                {
                    masterclient &s = *masterclients[j];
                    if(!s.listserver) continue;
                    masteroutf(c, "addserver %s %d %d %s %s %s %s\n", s.name, s.port, s.priority(), escapestring(s.desc), escapestring(s.authhandle), escapestring(s.flags), escapestring(s.branch));
                    servs++;
                }
                conoutf(colourwhite, "Master peer %s was sent %d server(s)", c.name, servs);
                c.shouldpurge = found = true;
            }
            if(!strcmp(w[0], "webapi") && c.link >= 0)
            {
                // only sent over the link, as older masters answer anything unknown with an error
                c.webport = clamp(atoi(w[1]), 0, 65535);
                c.relay = clamp(atoi(w[2]), 0, 2);
                copystring(c.fingerprint, w[3]);
                for(char *f = c.fingerprint; *f; f++) if(!isxdigit(*f)) { *f = '\0'; break; }
                c.bridge = w[4] && *w[4] ? clamp(atoi(w[4]), 0, 1) : -1; // older servers don't send it, and can't be bridged
                c.vouch = w[5] && atoi(w[5]) > 0; // nor this, and can't be given Sessions from remembered sign-ins
                relayresubscribe(c);
                found = true;
            }
            if(c.isserver || c.isquick)
            {
                if(!strcmp(w[0], "reqauth")) { reqauth(c, uint(atoi(w[1])), w[2], w[3]); found = true; }
                if(!strcmp(w[0], "vouched") && c.link >= 0) { signinvouched(c, uint(strtoul(w[1], NULL, 10)), w[2], w[3], atoi(w[4]) != 0, atoi(w[5]), NULL); found = true; }
                if(!strcmp(w[0], "failvouch") && c.link >= 0) { signinvouched(c, uint(strtoul(w[1], NULL, 10)), NULL, NULL, false, 0, *w[2] ? w[2] : "this server refused the session"); found = true; }
                if(!strcmp(w[0], "reqserverauth")) { reqserverauth(c, w[1]); found = true; }
                if(!strcmp(w[0], "confauth")) { confauth(c, uint(atoi(w[1])), w[2]); found = true; }
                if(!strcmp(w[0], "confserverauth")) { confserverauth(c, w[1]); found = true; }
            }
            if(w[0] && *w[0] && !found)
            {
                masteroutf(c, "error \"unknown command %s\"\n", w[0]);
                string cmd;
                filterstring(cmd, w[0], true, true, true, true, false, 64);
                // from peers that aren't listing a server, such as scans, they share one, so many addresses can't flood the notices
                foldnoticef("listing", NOTICE_WARNING, c.isserver ? tempformatstring("unknown %s:%d", c.name, c.port) : "unknown peer", "Master peer %s%s sent an unknown command: %s", c.name, c.isserver ? tempformatstring(":%d (%s)", c.port, serverdetails(c)) : "", cmd);
            }
            loopi(numargs) DELETEA(w[i]);
        }
        c.inputpos = &c.input[c.inputpos] - p;
        memmove(c.input, p, c.inputpos);
        return c.inputpos < (int)sizeof(c.input)-1;
    }

    // game servers on the Web API link, carrying the same lines as the socket
    masterclient *findlink(int id)
    {
        loopv(masterclients) if(masterclients[i]->link == id) return masterclients[i];
        return NULL;
    }

    static bool allowpeer(uint host)
    {
        if(checkipinfo(control, ipinfo::EXCEPT, host) || checkipinfo(control, ipinfo::TRUST, host)) return true;
        if(checkipinfo(control, ipinfo::BAN, host)) return false;
        if(masterduplimit)
        {
            int dups = 0;
            loopv(masterclients) if(masterclients[i]->priority() <= 0 && masterclients[i]->address.host == host) dups++;
            if(dups >= masterduplimit) return false;
        }
        return true;
    }

    void linkevent(httpwsevent &e, void *ctx)
    {
        switch(e.type)
        {
            case WSEV_OPEN:
            {
                // game servers send no Origin, so one that does is a web page using its visitor's address
                if(masterclients.length() >= MASTER_LIMIT || !e.req || e.req->origin || !allowpeer(e.req->ip)) { http::wsclose(e.id, 1008); break; }
                masterclient *c = new masterclient;
                c->link = e.id;
                c->linksecure = e.req->secure || e.req->local;
                c->socket = ENET_SOCKET_NULL;
                c->address.host = e.req->ip;
                c->address.port = 0;
                c->lastactivity = totalmillis ? totalmillis : 1;
                copystring(c->name, e.req->addr);
                masterclients.add(c);
                if(verbose) conoutf(colourwhite, "Master peer %s connected over the Web API link", c->name);
                break;
            }
            case WSEV_MESSAGE:
            {
                masterclient *c = findlink(e.id);
                if(!c) break;
                c->lastactivity = totalmillis ? totalmillis : 1;
                if(e.opcode == WS_BINARY)
                {
                    const char *end = e.len && e.data[0] == '{' ? (const char *)memchr(e.data, '\0', e.len) : NULL;
                    if(end) relayjson(*c, e.data, int(end - e.data), end + 1, e.len - int(end - e.data) - 1);
                    break;
                }
                if(e.len && e.data[0] == '{') { relayjson(*c, e.data, e.len); break; }
                if(c->inputpos + e.len >= int(sizeof(c->input)) - 1) { loopv(masterclients) if(masterclients[i] == c) { purgemasterclient(i); break; } break; }
                memcpy(&c->input[c->inputpos], e.data, e.len);
                c->inputpos += e.len;
                c->input[c->inputpos] = '\0';
                if(!checkmasterclientinput(*c)) loopv(masterclients) if(masterclients[i] == c) { purgemasterclient(i); break; }
                break;
            }
            case WSEV_CLOSE:
            {
                loopv(masterclients) if(masterclients[i]->link == e.id)
                {
                    masterclient *c = masterclients[i];
                    relayclosed(*c);
                    if(verbose || c->isserver) conoutf(colourwhite, "Master peer %s disconnected", c->name);
                    delete c;
                    masterclients.remove(i);
                    break;
                }
                break;
            }
        }
    }

    void check()
    {
        savemasterfiles();
        if(masterserver) { checkmail(); checktickets(); }
        if(mastersocket == ENET_SOCKET_NULL || pingsocket == ENET_SOCKET_NULL) return;

        ENetSocketSet readset, writeset;
        ENetSocket maxsock = max(mastersocket, pingsocket);

        ENET_SOCKETSET_EMPTY(readset);
        ENET_SOCKETSET_EMPTY(writeset);
        ENET_SOCKETSET_ADD(readset, mastersocket);
        ENET_SOCKETSET_ADD(readset, pingsocket);

        checkmasterlinks();
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.authreqs.length()) purgeauths(c);

            if(c.shouldping && (!c.lastping || ((!c.lastpong || ENET_TIME_GREATER(c.lastping, c.lastpong)) && ENET_TIME_DIFFERENCE(totalmillis, c.lastping) > uint(masterpingdelay))))
            {
                if(c.numpings < masterpingtries)
                {
                    c.numpings++;
                    c.lastping = totalmillis ? totalmillis : 1;
                    sendquery(c);
                }
                else
                {
                    bool waslisted = c.listserver;
                    c.listserver = c.shouldping = false;
                    if(waslisted) marksiblingschanged(c.address.host, c.authhandle);
                    masteroutf(c, "error \"ping attempts failed (tried %d times on port %d), server will not be listed\"\n", c.numpings, c.port+1);
                    foldnoticef("listing", NOTICE_WARNING, tempformatstring("ping %s:%d", c.name, c.port), "Server %s:%d (%s) wasn't listed, as it didn't answer %d pings on port %d", c.name, c.port, serverdetails(c), c.numpings, c.port+1);
                }
            }

            if(masterpollinterval && c.listserver && !c.shouldping && !c.reports() && ENET_TIME_DIFFERENCE(totalmillis, c.lastpoll) >= uint(masterpollinterval)*1000)
            {
                if(c.pollwaiting)
                {
                    c.pollmisses++;
                    c.changed = true;
                    if(c.pollmisses >= POLL_MISSES)
                    {
                        DELETEA(c.polled);
                        c.numplayers = -1;
                    }
                }
                c.pollwaiting = true;
                c.lastpoll = totalmillis ? totalmillis : 1;
                sendquery(c);
            }

            if(c.isserver && c.lastcontrol < controlversion) sendcontrols(c);
            if(c.link >= 0) continue;

            if(c.outputpos < c.output.length()) ENET_SOCKETSET_ADD(writeset, c.socket);
            else ENET_SOCKETSET_ADD(readset, c.socket);

            maxsock = max(maxsock, c.socket);
        }

        if(enet_socketset_select(maxsock, &readset, &writeset, 0) <= 0) return;

        if(ENET_SOCKETSET_CHECK(readset, pingsocket)) checkmasterpongs();

        if(ENET_SOCKETSET_CHECK(readset, mastersocket)) for(;;)
        {
            ENetAddress address;
            ENetSocket masterclientsocket = enet_socket_accept(mastersocket, &address);

            if(masterclients.length() >= MASTER_LIMIT)
            {
                enet_socket_destroy(masterclientsocket);
                break;
            }

            if(!checkipinfo(control, ipinfo::EXCEPT, address.host) && !checkipinfo(control, ipinfo::TRUST, address.host))
            {
                if(checkipinfo(control, ipinfo::BAN, address.host))
                {
                    enet_socket_destroy(masterclientsocket);
                    break;
                }

                if(masterduplimit && !checkipinfo(control, ipinfo::TRUST, address.host))
                {
                    int dups = 0;
                    loopv(masterclients)
                        if(masterclients[i]->priority() <= 0 && masterclients[i]->address.host == address.host)
                            dups++;

                    if(dups >= masterduplimit)
                    {
                        enet_socket_destroy(masterclientsocket);
                        break;
                    }
                }
            }

            if(masterclientsocket != ENET_SOCKET_NULL)
            {
                masterclient *c = new masterclient;
                c->address = address;
                c->socket = masterclientsocket;
                c->lastactivity = totalmillis ? totalmillis : 1;
                masterclients.add(c);
                if(enet_address_get_host_ip(&c->address, c->name, sizeof(c->name)) < 0) copystring(c->name, "unknown");
                if(verbose) conoutf(colourwhite, "Master peer %s connected", c->name);
            }

            break;
        }

        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.link >= 0) continue;
            if(c.outputpos < c.output.length() && ENET_SOCKETSET_CHECK(writeset, c.socket))
            {
                ENetBuffer buf;
                buf.data = (void *)&c.output[c.outputpos];
                buf.dataLength = c.output.length()-c.outputpos;

                int res = enet_socket_send(c.socket, NULL, &buf, 1);
                if(res >= 0)
                {
                    c.outputpos += res;
                    if(c.outputpos >= c.output.length())
                    {
                        c.output.setsize(0);
                        c.outputpos = 0;
                        if(c.shouldpurge) { purgemasterclient(i--); continue; }
                    }
                }
                else { purgemasterclient(i--); continue; }
            }

            if(ENET_SOCKETSET_CHECK(readset, c.socket))
            {
                ENetBuffer buf;
                buf.data = &c.input[c.inputpos];
                buf.dataLength = sizeof(c.input)-1 - c.inputpos;

                int res = enet_socket_receive(c.socket, NULL, &buf, 1);
                if(res > 0)
                {
                    c.inputpos += res;
                    c.input[c.inputpos] = '\0';
                    if(!checkmasterclientinput(c)) { purgemasterclient(i--); continue; }
                }
                else { purgemasterclient(i--); continue; }
            }

            /* if(c.output.length() > OUTPUT_LIMIT) { purgemasterclient(i--); continue; } */

            if(ENET_TIME_DIFFERENCE(totalmillis, c.lastactivity) >= (c.isserver ? SERVER_TIME : CLIENT_TIME))
            {
                purgemasterclient(i--);
                continue;
            }

            if(checkipinfo(control, ipinfo::BAN, c.address.host) && c.priority() <= 0 && !checkipinfo(control, ipinfo::EXCEPT, c.address.host) && !checkipinfo(control, ipinfo::TRUST, c.address.host))
            {
                purgemasterclient(i--);
                continue;
            }
        }
    }

    // remembered sign-ins: an account's long-lived sign-in on the master, which vouches for Sessions on linked game servers so they never see it

    void cleanup()
    {
        if(masterserver) { savehistory(); savequeue(); if(signindirty) savesignins(); savemasterfiles(); cleanupmail(); cleanupaccounts(); cleanuplinks(); }
        loopvrev(masterclients) purgemasterclient(i);
        if(mastersocket != ENET_SOCKET_NULL) enet_socket_destroy(mastersocket);
    }

    void reload()
    {
        savemasterfiles(); // anything changed since the last frame, before the files are read again
        setmasterfilesloading(true);
        clearauth();
        signincheck = true; // the accounts are loaded again before the next check, which drops sign-ins whose account changed
    }
}
