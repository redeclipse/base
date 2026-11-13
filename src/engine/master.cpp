#ifdef WIN32
#define FD_SETSIZE 4096
#else
#include <sys/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 4096
#endif

#include "engine.h"
#include <enet/time.h>

#define MASTER_LIMIT 4096
#define CLIENT_TIME (60*1000)
#define SERVER_TIME (35*60*1000)
#define AUTH_TIME (30*1000)
#define STATUS_LIMIT 16384
#define POLL_MISSES 3 // polls in a row a server can leave unanswered before its polled status is dropped
#define POLL_DRIFT 2 // seconds the end of a match can move between polls without being a change
#define POLL_PLAYING 5 // the playing and overtime game states, numbered the same in every version
#define POLL_OVERTIME 6
#define POLL_STATEMILLIS 266 // the game version whose time left in a state is in milliseconds, where earlier ones send seconds
#define POLL_LEFTSECONDS 278 // the game version that sent the time remaining in seconds again, after 266 sent it in milliseconds
#define QUEUE_TICKETBYTES 16 // a Ready queue Ticket, written in hex

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

struct authuser
{
    char *name, *flags, *email, *steamid, *key;
    void *pubkey;

    authuser() : name(NULL), flags(NULL), email(NULL), steamid(NULL), key(NULL), pubkey(NULL) {}
};

struct authreq
{
    enet_uint32 reqtime;
    uint id;
    void *answer;
    string name, key, hostname; // a rehash frees the accounts, so the answer finds its account again by these

    authreq() { reset(); }
    ~authreq() {}

    void reset()
    {
        reqtime = 0;
        id = 0;
        answer = NULL;
        name[0] = key[0] = hostname[0] = '\0';
    }
};

// who a linked server says is on it with a Ready queue Ticket or a Discord ID, so the master knows who has arrived or is held there
struct queuetag
{
    string ticket, discord;
};

struct masterclient
{
    ENetAddress address;
    ENetSocket socket;
    string name, desc, flags, authhandle, branch;

    char input[4096];
    vector<char> output;
    int inputpos, outputpos, port, numpings, lastcontrol, version, link, webport, relay, bridge; // bridge: -1 when the server doesn't say
    string fingerprint;
    char *status; // the last status it reported over the link, as JSON
    char *polled; // what its query reply last said, as JSON, while it reports no status
    uchar pollreq[8];
    int pollreqlen, pollmisses, polledends, polledstateends, polledstate, numplayers; // numplayers: from its status or poll, -1 when neither says
    int statusmax, statusmode, statusver, statusbuild[3]; // from its reported status, for the Ready queue: room, master mode, protocol and version
    vector<queuetag> queuetags;
    enet_uint32 lastping, lastpong, lastactivity, lastpoll;
    vector<authreq> authreqs;
    authreq serverauthreq;
    bool isserver, isquick, ishttp, listserver, shouldping, shouldpurge, pollwaiting;

    masterclient() : link(-1), status(NULL), polled(NULL) { reset(); }
    ~masterclient()
    {
        DELETEA(status);
        DELETEA(polled);
        loopv(authreqs) if(authreqs[i].answer) freechallenge(authreqs[i].answer);
        if(serverauthreq.answer) freechallenge(serverauthreq.answer);
    }

    void reset()
    {
        name[0] = desc[0] = flags[0] = authhandle[0] = branch[0] = '\0';
        lastcontrol = -1;
        inputpos = outputpos = numpings = version = webport = relay = 0;
        bridge = -1;
        fingerprint[0] = '\0';
        lastping = lastpong = lastactivity = lastpoll = 0;
        pollreqlen = pollmisses = polledends = polledstateends = polledstate = 0;
        numplayers = -1;
        statusmax = statusmode = statusver = 0;
        statusbuild[0] = statusbuild[1] = statusbuild[2] = 0;
        queuetags.shrink(0);
        isserver = isquick = ishttp = listserver = shouldping = shouldpurge = pollwaiting = false;
        port = MASTER_PORT;
        output.shrink(0);
        authreqs.shrink(0);
    }

    bool reports() { return link >= 0 && status; }

    bool hasflag(char f)
    {
        if(f == 'b' && *flags) return true;
        return ::hasflag(flags, f);
    }

    int priority()
    {
        if(!isserver || !listserver) return -1;
        if(!*authhandle || !*flags) return 0;
        int ret = 1;
        for(const char *c = flags; *c; c++) switch(*c)
        {
            case 'm': ret = max(ret, 4); break;
            case 's': case 'u': ret = max(ret, 3); break;
            case 'b': ret = max(ret, 2); break;
            default: break;
        }
        return ret;
    }
};

static vector<masterclient *> masterclients;
static void routeservers(httprequest &req, httpresponse &res);
static void routehistory(httprequest &req, httpresponse &res);
static void routeglobalhistory(httprequest &req, httpresponse &res);
static void routejoin(httprequest &req, httpresponse &res);
static void loadhistory();
static void savehistory();
static void relayresubscribe(masterclient &c);
static void relayroute(httprequest &req, httpresponse &res);
static void relayviewerevent(httpwsevent &e, void *ctx);
static void viewerevent(httpwsevent &e, void *ctx);
static void routequeue(httprequest &req, httpresponse &res);
static void loadqueue();
static void savequeue();
static void checkqueue();
static void queueviewerjoined(httpwsevent &e);
static void queueviewermessage(httpwsevent &e);
static void queueviewerclosed(int id);
static void linkevent(httpwsevent &e, void *ctx);
static void checkmasterlinks();
static ENetSocket mastersocket = ENET_SOCKET_NULL, pingsocket = ENET_SOCKET_NULL;
static time_t starttime;

void masterout(masterclient &c, const char *msg, int len = 0)
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

bool setuppingsocket(ENetAddress *address)
{
    if(pingsocket != ENET_SOCKET_NULL) return true;
    pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if(pingsocket == ENET_SOCKET_NULL) return false;
    if(address && enet_socket_bind(pingsocket, address) < 0) return false;
    enet_socket_set_option(pingsocket, ENET_SOCKOPT_NONBLOCK, 1);
    return true;
}

void setupmaster()
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
        http::addsocket(HTTP_API "/servers/live", HTTP_PUBLIC, viewerevent, NULL, 4096);
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
        starttime = clocktime;
        conoutf(colourwhite, "Master server started on %s:[%d]", *masterip ? masterip : "localhost", masterport);
    }
}

static hashnameset<authuser> authusers;

void addauth(char *name, char *flags, char *pubkey, char *email, char *steamid)
{
    if(!name || !*name || !flags || !*flags || !pubkey || !*pubkey) return;
    string authname;
    if(filterstring(authname, name, true, true, true, true, 100)) name = authname;
    if(authusers.access(name))
    {
        conoutf(colourred, "Auth handle '%s' already exists, skipping (%s)", name, email);
        return;
    }
    string flagbuf;
    int flagidx = 0;
    flagbuf[0] = 0;
    #define ADDFLAG(c, check) \
        if(!hasflag(flagbuf, c)) \
        { \
            flagbuf[flagidx++] = c; \
            flagbuf[flagidx] = 0; \
            if(flagidx >= MAXSTRLEN-1) check; \
        }
    loopi(min(int(strlen(flags)), MAXSTRLEN-1))
    {
        ADDFLAG(flags[i], break);
        if(flags[i] == 'm') { ADDFLAG('o', break); }
        else if(flags[i] == 'o') { ADDFLAG('m', break); }
    }
    if(!flagbuf[0]) ADDFLAG('u', );
    name = newstring(name);
    authuser &u = authusers[name];
    u.name = name;
    u.flags = newstring(flagbuf);
    u.pubkey = parsepubkey(pubkey);
    u.key = newstring(pubkey);
    u.email = newstring(email);
    u.steamid = newstring(steamid);
}
COMMAND(0, addauth, "sssss");

static hashnameset<authuser> serverauthusers;

void addserverauth(char *name, char *flags, char *pubkey, char *email)
{
    string authname;
    if(filterstring(authname, name, true, true, true, true, 100)) name = authname;
    if(serverauthusers.access(name))
    {
        conoutf(colourred, "Server auth handle '%s' already exists, skipping (%s)", name, email);
        return;
    }
    name = newstring(name);
    authuser &u = serverauthusers[name];
    u.name = name;
    u.flags = newstring(flags);
    u.pubkey = parsepubkey(pubkey);
    u.key = newstring(pubkey);
    u.email = newstring(email);
}
COMMAND(0, addserverauth, "ssss");

void clearauth()
{
    enumerate(authusers, authuser, u, { delete[] u.name; delete[] u.flags; delete[] u.email; delete[] u.steamid; delete[] u.key; freepubkey(u.pubkey); });
    authusers.clear();
    enumerate(serverauthusers, authuser, u, { delete[] u.name; delete[] u.flags; delete[] u.email; delete[] u.steamid; delete[] u.key; freepubkey(u.pubkey); });
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

    string ip, host;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");
    copystring(host, hostname && *hostname ? hostname : "-");

    authuser *u = authusers.access(name);
    if(!u)
    {
        masteroutf(c, "failauth %u\n", id);
        conoutf(colourorange, "Failed '%s' (%u) from %s on server %s (NOTFOUND)\n", name, id, host, ip);
        return;
    }
    if(masterauthlimit && c.authreqs.length() >= masterauthlimit)
    {
        masteroutf(c, "failauth %u\n", id);
        conoutf(colourorange, "Failed '%s' (%u) from %s on server %s (LIMIT)\n", name, id, host, ip);
        return;
    }
    conoutf(colourwhite, "Attempting '%s' (%u) from %s on server %s\n", name, id, host, ip);

    authreq &a = c.authreqs.add();
    copystring(a.name, u->name);
    copystring(a.key, u->key);
    a.reqtime = totalmillis;
    a.id = id;
    copystring(a.hostname, host);
    uint seed[3] = { uint(starttime), uint(totalmillis), randomMT() };
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
        conoutf(colourred, "Failed server '%s' (NOTFOUND)\n", name);
        return;
    }
    conoutf(colourwhite, "Attempting server '%s'\n", name);

    copystring(c.serverauthreq.name, u->name);
    copystring(c.serverauthreq.key, u->key);
    c.serverauthreq.reqtime = totalmillis;
    uint seed[3] = { uint(starttime), uint(totalmillis), randomMT() };
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
            conoutf(colourorange, "Failed '%s' (%u) from %s on server %s (CHANGED)\n", c.authreqs[i].name, id, c.authreqs[i].hostname, ip);
        }
        else if(checkchallenge(val, c.authreqs[i].answer))
        {
            masteroutf(c, "succauth %u \"%s\" \"%s\"\n", id, u->name, u->flags);
            conoutf(colourwhite, "Succeeded '%s' [%s] (%u) from %s on server %s\n", u->name, u->flags, id, c.authreqs[i].hostname, ip);
        }
        else
        {
            masteroutf(c, "failauth %u\n", id);
            conoutf(colourred, "Failed '%s' (%u) from %s on server %s (BADKEY)\n", u->name, id, c.authreqs[i].hostname, ip);
        }
        if(c.authreqs[i].answer) freechallenge(c.authreqs[i].answer);
        c.authreqs.remove(i--);
        return;
    }
    masteroutf(c, "failauth %u\n", id);
}

static void relayclosed(masterclient &c);

void purgemasterclient(int n)
{
    masterclient &c = *masterclients[n];
    if(c.link >= 0) { relayclosed(c); http::wsclose(c.link); }
    else enet_socket_destroy(c.socket);
    if(verbose || c.isserver) conoutf(colourwhite, "Master peer %s disconnected", c.name);
    delete masterclients[n];
    masterclients.remove(n);
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
        conoutf(colourorange, "Failed server '%s' (CHANGED)\n", c.serverauthreq.name);
    }
    else if(checkchallenge(val, c.serverauthreq.answer))
    {
        masteroutf(c, "succserverauth \"%s\" \"%s\"\n", u->name, u->flags);
        conoutf(colourwhite, "Succeeded server '%s' [%s]\n", u->name, u->flags);
        copystring(c.authhandle, u->name);
        copystring(c.flags, u->flags);
    }
    else
    {
        masteroutf(c, "failserverauth\n");
        conoutf(colourred, "Failed server '%s' (BADKEY)\n", u->name);
    }

    if(c.serverauthreq.answer) freechallenge(c.serverauthreq.answer);

    c.serverauthreq.reset();
}

// asks a server what it is playing, as the server browser does; the time it echoes is a random token, so only its own answer counts
static void sendquery(masterclient &c)
{
    ucharbuf p(c.pollreq, sizeof(c.pollreq));
    putint(p, int(randomMT()&0x3FFFFFFF)|1);
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
    filterstring(tmp, src, true, true, true, false, len ? len : N-1);
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
                if(w[2] && *w[2] && strcmp(w[2], "*") && (enet_address_set_host(&address, w[2]) < 0 || address.host != c.address.host))
                {
                    c.listserver = c.shouldping = false;
                    masteroutf(c, "echo \"server IP '%s' does not match origin '%s', server will not be listed\n", w[2], c.name);
                }
                else if(masterminver && c.version < masterminver)
                {
                    c.listserver = c.shouldping = false;
                    masteroutf(c, "echo \"server version '%d' is no longer supported (need: %d), please update at %s\n", c.version, masterminver, versionurl);
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
                    }
                    c.isserver = true;
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
            relayresubscribe(c);
            found = true;
        }
        if(c.isserver || c.isquick)
        {
            if(!strcmp(w[0], "reqauth")) { reqauth(c, uint(atoi(w[1])), w[2], w[3]); found = true; }
            if(!strcmp(w[0], "reqserverauth")) { reqserverauth(c, w[1]); found = true; }
            if(!strcmp(w[0], "confauth")) { confauth(c, uint(atoi(w[1])), w[2]); found = true; }
            if(!strcmp(w[0], "confserverauth")) { confserverauth(c, w[1]); found = true; }
        }
        if(w[0] && *w[0] && !found)
        {
            masteroutf(c, "error \"unknown command %s\"\n", w[0]);
            conoutf(colourwhite, "Master peer %s (client) sent unknown command: %s", c.name, w[0]);
        }
        loopi(numargs) DELETEA(w[i]);
    }
    c.inputpos = &c.input[c.inputpos] - p;
    memmove(c.input, p, c.inputpos);
    return c.inputpos < (int)sizeof(c.input)-1;
}

// game servers on the Web API link, carrying the same lines as the socket
static masterclient *findlink(int id)
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

static void relayjson(masterclient &c, const char *data, int len, const char *body = NULL, int bodylen = 0);
static void relayclosed(masterclient &c);
static void queuetags(masterclient &c, jsonvalue *list);

static void linkevent(httpwsevent &e, void *ctx)
{
    switch(e.type)
    {
        case WSEV_OPEN:
        {
            // game servers send no Origin, so one that does is a web page using its visitor's address
            if(masterclients.length() >= MASTER_LIMIT || !e.req || e.req->origin || !allowpeer(e.req->ip)) { http::wsclose(e.id, 1008); break; }
            masterclient *c = new masterclient;
            c->link = e.id;
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

static const char *serverchannelname(masterclient &s);

static void writeserver(jsonwriter &w, masterclient &s)
{
    w.beginobject();
    w.str("host", s.name);
    w.num("port", s.port);
    w.num("priority", s.priority());
    w.str("description", s.desc);
    if(s.authhandle[0]) w.str("handle", s.authhandle);
    if(s.flags[0]) w.str("flags", s.flags);
    w.str("branch", s.branch);
    w.num("version", s.version);
    if(s.webport) w.num("webport", s.webport);
    w.num("relay", s.link >= 0 ? s.relay : 0);
    if(s.fingerprint[0]) w.str("fingerprint", s.fingerprint);
    if(s.link >= 0 && s.bridge >= 0) w.num("bridge", s.bridge);
    const char *channel = serverchannelname(s);
    if(channel) w.str("channel", channel);
    if(s.reports()) w.raw("status", s.status);
    else if(s.polled) w.raw("status", s.polled);
    w.endobject();
}

// stamped with our clock, as the times in it are, so viewers can count down without their clocks agreeing with ours
static void serverlist(vector<char> &buf, bool stamped = true)
{
    jsonwriter w(buf);
    w.beginobject();
    if(stamped) w.num("time", double(time(NULL)));
    w.beginobject("version");
    w.num("main", mastermainver ? mastermainver : server::getver(0));
    w.num("game", mastergamever ? mastergamever : server::getver(1));
    w.endobject();
    if(*masterscriptclient) w.str("script", masterscriptclient);
    w.beginarray("servers");
    loopv(masterclients) if(masterclients[i]->listserver) writeserver(w, *masterclients[i]);
    w.endarray();
    w.endobject();
}

static uint listhash(const vector<char> &buf)
{
    uint h = 2166136261U;
    loopv(buf) h = (h ^ uchar(buf[i])) * 16777619U;
    return h;
}

static void routeservers(httprequest &req, httpresponse &res)
{
    // the tag leaves out the time, which changes every second when nothing else has
    vector<char> buf;
    serverlist(buf, false);
    defformatstring(etag, "\"%08x\"", listhash(buf));
    res.headers.set("ETag", etag);
    res.headers.set("Cache-Control", "public, max-age=5");
    const char *match = req.headers.get("If-None-Match");
    if(match && !strcmp(match, etag)) { res.status = 304; return; }
    buf.setsize(0);
    serverlist(buf);
    res.json(buf.getbuf(), buf.length());
}

// browser viewers of the live list: a snapshot when they connect, then only what changed
struct listedserver
{
    string host;
    int port;
    char *json;
};
static vector<listedserver> listed;
static vector<int> viewers;
static int lastlistcheck = 0;

// how busy each server has been: its peak player count in each HISTORY_BUCKET, kept for masterhistorydays and saved to masterhistoryfile
#define HISTORY_BUCKET 900 // seconds a peak covers
#define HISTORY_UNKNOWN 0xFF // a bucket the master knew nothing about, as the server wasn't listed or said nothing
#define HISTORY_MAXPEAK 254
#define HISTORY_VERSION 1
#define HISTORY_MAXDAYS 3650
#define HISTORY_GLOBAL "*" // the whole game's, kept as a server of its own with a name no server can have
VAR(0, masterhistorydays, 0, 365, HISTORY_MAXDAYS); // days of history kept for each server, 0 keeps none
SVAR(0, masterhistoryfile, "masterhistory.dat");

struct serverhistory
{
    string host;
    int port, first; // first: the bucket, counted from the epoch, that peaks starts at
    vector<uchar> peaks;

    int last() const { return first + peaks.length() - 1; }

    void record(int bucket, int players)
    {
        if(peaks.empty()) first = bucket;
        else if(bucket < first) return; // the clock went back
        while(last() < bucket) peaks.add(HISTORY_UNKNOWN);
        uchar &p = peaks[bucket - first], n = uchar(clamp(players, 0, HISTORY_MAXPEAK));
        if(p == HISTORY_UNKNOWN || n > p) p = n;
    }

    void trim(int keep)
    {
        int over = peaks.length() - keep;
        if(over <= 0) return;
        peaks.remove(0, over);
        first += over;
    }
};
static vector<serverhistory *> histories;
static int lasthistorybucket = -1;

static int historybucket() { return int(time(NULL) / HISTORY_BUCKET); }
static int historykeep() { return masterhistorydays * (86400 / HISTORY_BUCKET); }

static serverhistory *findhistory(const char *host, int port, bool add = false)
{
    loopv(histories) if(histories[i]->port == port && !strcmp(histories[i]->host, host)) return histories[i];
    if(!add) return NULL;
    serverhistory *h = new serverhistory;
    copystring(h->host, host);
    h->port = port;
    h->first = 0;
    histories.add(h);
    return h;
}

static void loadhistory()
{
    histories.deletecontents();
    if(!masterhistorydays || !*masterhistoryfile) return;
    stream *f = opengzfile(masterhistoryfile, "rb");
    if(!f) return;
    char magic[4];
    int maxlen = HISTORY_MAXDAYS * (86400 / HISTORY_BUCKET);
    bool ok = f->read(magic, 4) == 4 && !memcmp(magic, "REMH", 4) && f->getlil<int>() == HISTORY_VERSION && f->getlil<int>() == HISTORY_BUCKET;
    int count = ok ? f->getlil<int>() : 0;
    if(count < 0 || count > 65536) ok = false;
    for(int i = 0; ok && i < count; i++)
    {
        int hostlen = f->getlil<int>();
        string host;
        if(hostlen <= 0 || hostlen >= MAXSTRLEN || f->read(host, hostlen) != size_t(hostlen)) { ok = false; break; }
        host[hostlen] = '\0';
        int port = f->getlil<int>(), first = f->getlil<int>(), len = f->getlil<int>();
        if(port < 0 || port > 65535 || (!port && strcmp(host, HISTORY_GLOBAL)) || first < 0 || len < 0 || len > maxlen) { ok = false; break; }
        serverhistory *h = findhistory(host, port, true);
        h->first = first;
        h->peaks.setsize(0);
        if(len && f->read(h->peaks.pad(len), len) != size_t(len)) { ok = false; break; }
        h->trim(historykeep());
    }
    delete f;
    if(!ok)
    {
        conoutf(colourred, "Failed to read server history from %s, starting a new one", masterhistoryfile);
        histories.deletecontents();
        return;
    }
    conoutf(colourwhite, "Loaded the history of %d server(s) from %s", histories.length(), masterhistoryfile);
}

// written beside the old one then moved over it, so a crash part way through leaves the last good copy
static void savehistory()
{
    if(!masterhistorydays || !*masterhistoryfile) return;
    int now = historybucket(), keep = historykeep();
    loopvrev(histories)
    {
        serverhistory *h = histories[i];
        h->trim(keep);
        if(h->peaks.empty() || h->last() < now - keep) { delete h; histories.remove(i); }
    }
    defformatstring(newname, "%s.new", masterhistoryfile);
    stream *f = opengzfile(newname, "wb");
    if(!f) { conoutf(colourred, "Failed to save server history to %s", newname); return; }
    f->write("REMH", 4);
    f->putlil<int>(HISTORY_VERSION);
    f->putlil<int>(HISTORY_BUCKET);
    f->putlil<int>(histories.length());
    loopv(histories)
    {
        serverhistory *h = histories[i];
        int hostlen = strlen(h->host);
        f->putlil<int>(hostlen);
        f->write(h->host, hostlen);
        f->putlil<int>(h->port);
        f->putlil<int>(h->first);
        f->putlil<int>(h->peaks.length());
        f->write(h->peaks.getbuf(), h->peaks.length());
    }
    delete f;
    string from, to;
    copystring(from, findfile(newname, "w"));
    copystring(to, findfile(masterhistoryfile, "w"));
    remove(to);
    if(rename(from, to)) conoutf(colourred, "Failed to move the server history into %s", masterhistoryfile);
}

// every listed server that says how many are on, and all of them together, once a second, and the file once a bucket
static void checkhistory()
{
    if(!masterhistorydays) return;
    int bucket = historybucket(), total = 0;
    bool known = false;
    loopv(masterclients)
    {
        masterclient &c = *masterclients[i];
        if(!c.listserver || c.numplayers < 0) continue;
        findhistory(c.name, c.port, true)->record(bucket, c.numplayers);
        total += c.numplayers;
        known = true;
    }
    if(known) findhistory(HISTORY_GLOBAL, 0, true)->record(bucket, total);
    if(lasthistorybucket >= 0 && bucket != lasthistorybucket) savehistory();
    lasthistorybucket = bucket;
}

// GET /api/v1/servers/<address>/<port>/history?days=N: peaks oldest first, null where nothing was known
#define HISTORY_DEFAULTDAYS 7
static void writehistory(httprequest &req, httpresponse &res, serverhistory *h);

static void routehistory(httprequest &req, httpresponse &res)
{
    const char *address = req.param("address"), *port = req.param("port");
    if(!address || !port || !*port || port[strspn(port, "0123456789")]) { res.error(404, "no history for that server"); return; }
    writehistory(req, res, findhistory(address, atoi(port)));
}

// GET /api/v1/history?days=N: the same for every listed server's players added together
static void routeglobalhistory(httprequest &req, httpresponse &res)
{
    writehistory(req, res, findhistory(HISTORY_GLOBAL, 0));
}

// a bounce link: a web page that sends the browser on to a listed server's redeclipse:// join link, for places such as
// Discord that only make https links clickable
static void htmlput(vector<char> &buf, const char *s) { buf.put(s, strlen(s)); }

static void htmltext(vector<char> &buf, const char *s, bool cube)
{
    for(; *s; s++)
    {
        uchar c = uchar(*s);
        switch(c)
        {
            case '&': htmlput(buf, "&amp;"); break;
            case '<': htmlput(buf, "&lt;"); break;
            case '>': htmlput(buf, "&gt;"); break;
            case '"': htmlput(buf, "&quot;"); break;
            case '\'': htmlput(buf, "&#39;"); break;
            default:
                if(cube && c >= 0x80)
                {
                    uchar u[4];
                    size_t n = encodeutf8(u, sizeof(u), &c, 1);
                    buf.put((const char *)u, n);
                }
                else buf.add(c);
                break;
        }
    }
}

static void routejoin(httprequest &req, httpresponse &res)
{
    const char *address = req.param("address"), *port = req.param("port");
    masterclient *found = NULL;
    if(address && port && *port && !port[strspn(port, "0123456789")])
    {
        int p = atoi(port);
        loopv(masterclients) if(masterclients[i]->listserver && masterclients[i]->port == p && !strcmp(masterclients[i]->name, address))
        {
            found = masterclients[i];
            break;
        }
    }
    vector<char> buf;
    const char *head = "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<style>body{background:#111;color:#ddd;font:16px/1.5 sans-serif;margin:0;display:flex;min-height:100vh;align-items:center;justify-content:center;text-align:center}"
        "main{padding:16px;max-width:32em}h1{font-size:1.4em;margin:0 0 .5em}a{color:#f66}a.join{display:inline-block;margin:1em 0;padding:.6em 2em;background:#c22;color:#fff;"
        "border-radius:4px;text-decoration:none;font-weight:bold}p{margin:.5em 0}</style>";
    htmlput(buf, head);
    res.headers.set("Cache-Control", "no-store");
    if(!found)
    {
        const char *body = "<title>Red Eclipse</title></head><body><main><h1>That server isn't on the list right now</h1>"
            "<p>It may have stopped, or moved to another address. <a href=\"https://www.redeclipse.net/servers\">See the servers playing now</a>.</p></main></body></html>\n";
        htmlput(buf, body);
        res.status = 404;
        res.settype("text/html; charset=utf-8");
        res.body.setsize(0);
        res.body.put(buf.getbuf(), buf.length());
        return;
    }
    string desc;
    filterstring(desc, found->desc);
    if(!desc[0]) formatstring(desc, "%s:%d", found->name, found->port);
    defformatstring(link, "%s://%s:%d", versionuname, found->name, found->port);
    // a Ready queue pop's link carries its Ticket on to the game, so the master sees that player arrive
    const char *ticket = req.var("ticket");
    if(ticket && *ticket && strlen(ticket) <= QUEUE_TICKETBYTES*2 && !ticket[strspn(ticket, "0123456789abcdef")]) concformatstring(link, "?ticket=%s", ticket);
    htmlput(buf, "<title>Join ");
    htmltext(buf, desc, true);
    htmlput(buf, " - Red Eclipse</title></head><body><main><h1>");
    htmltext(buf, desc, true);
    htmlput(buf, "</h1><p>Opening Red Eclipse to join this server.</p><a class=\"join\" href=\"");
    htmltext(buf, link, false);
    htmlput(buf, "\">Join</a><p>Nothing happened? <a href=\"https://www.redeclipse.net/download\">Get Red Eclipse</a>, start it once, then try again.</p>"
        "</main><script>location.href=");
    // the link is only digits, dots and a host name the master checked, but it is written as a JSON string all the same
    json::writestring(buf, link);
    htmlput(buf, ";</script></body></html>\n");
    res.settype("text/html; charset=utf-8");
    res.body.setsize(0);
    res.body.put(buf.getbuf(), buf.length());
}

static void writehistory(httprequest &req, httpresponse &res, serverhistory *h)
{
    const char *daysvar = req.var("days");
    int days = clamp(daysvar && *daysvar ? atoi(daysvar) : HISTORY_DEFAULTDAYS, 1, max(masterhistorydays, 1));
    int now = historybucket(), from = now - days * (86400 / HISTORY_BUCKET) + 1;
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.num("bucket", HISTORY_BUCKET);
    w.num("start", double(from) * HISTORY_BUCKET);
    w.beginarray("peaks");
    for(int b = from; b <= now; b++)
    {
        int p = h && b >= h->first && b <= h->last() ? h->peaks[b - h->first] : HISTORY_UNKNOWN;
        if(p == HISTORY_UNKNOWN) w.null(NULL);
        else w.num(NULL, p);
    }
    w.endarray();
    w.endobject();
    res.headers.set("Cache-Control", "public, max-age=60");
    res.json(buf.getbuf(), buf.length());
}

static void viewerevent(httpwsevent &e, void *ctx)
{
    switch(e.type)
    {
        case WSEV_OPEN:
        {
            viewers.add(e.id);
            vector<char> buf;
            static const char head[] = "{\"event\":\"snapshot\",";
            buf.put(head, sizeof(head)-1);
            vector<char> list;
            serverlist(list);
            buf.put(list.getbuf() + 1, list.length() - 1);
            http::wssend(e.id, buf.getbuf(), buf.length());
            queueviewerjoined(e);
            break;
        }
        case WSEV_MESSAGE: queueviewermessage(e); break; // all a viewer says is about the Ready queue
        case WSEV_CLOSE: viewers.removeobj(e.id); queueviewerclosed(e.id); break;
        default: break;
    }
}

static void checklisted()
{
    if(totalmillis - lastlistcheck < 1000) return;
    lastlistcheck = totalmillis;
    checkhistory();
    checkqueue();
    vector<bool> seen;
    loopv(listed) seen.add(false);
    loopv(masterclients)
    {
        masterclient &s = *masterclients[i];
        if(!s.listserver) continue;
        vector<char> one;
        jsonwriter w(one);
        writeserver(w, s);
        one.add('\0');
        int found = -1;
        loopvj(listed) if(listed[j].port == s.port && !strcmp(listed[j].host, s.name)) { found = j; break; }
        const char *event = NULL;
        if(found < 0)
        {
            listedserver &l = listed.add();
            copystring(l.host, s.name);
            l.port = s.port;
            l.json = newstring(one.getbuf());
            seen.add(true);
            event = "add";
        }
        else
        {
            seen[found] = true;
            if(strcmp(listed[found].json, one.getbuf()))
            {
                DELETEA(listed[found].json);
                listed[found].json = newstring(one.getbuf());
                event = "update";
            }
        }
        if(event && viewers.length())
        {
            defformatstring(head, "{\"event\":\"%s\",\"time\":%d,\"server\":", event, int(time(NULL)));
            vector<char> msg;
            msg.put(head, strlen(head));
            msg.put(one.getbuf(), one.length() - 1);
            msg.add('}');
            http::wsbroadcast(viewers, msg.getbuf(), msg.length());
        }
    }
    loopvrev(listed) if(!seen[i])
    {
        if(viewers.length())
        {
            vector<char> msg;
            jsonwriter w(msg);
            w.beginobject();
            w.str("event", "remove");
            w.str("host", listed[i].host);
            w.num("port", listed[i].port);
            w.endobject();
            http::wsbroadcast(viewers, msg.getbuf(), msg.length());
        }
        DELETEA(listed[i].json);
        listed.remove(i);
    }
}

// the Ready queue: people waiting for enough others to play with, each with a Threshold, sent to a server together once theirs are all met;
// web pages and the game queue over the live list's socket and own their entry while it's open, the Discord bot queues its members for a time
#define QUEUE_VERSION 2
#define QUEUE_MINTHRESHOLD 2
#define QUEUE_MAXTHRESHOLD 8
#define QUEUE_DISCORDLEN 24
#define QUEUE_ADDRLIMIT 4 // entries owned by sockets from one address, as carrier-grade NAT puts strangers behind one
#define QUEUE_RECLAIM 300 // seconds an entry outlives its socket, for one that reconnects to claim it back with its Ticket
#define QUEUE_TRAVEL 300 // seconds a popped entry counts towards its server while it makes its way there
#define QUEUE_DISCORDTIME 3600 // seconds a Discord entry lasts, and what a renewal adds, up to QUEUE_DISCORDMAX from now
#define QUEUE_DISCORDRENEW 1800
#define QUEUE_DISCORDMAX 5400
#define QUEUE_MSGRATE 250 // milliseconds between a viewer's queue messages
#define QUEUE_MAXENTRIES 4096
#define QUEUE_PRIVMODERATOR 3 // the game's Privileges, which the master has no names for
#define QUEUE_PRIVDEVELOPER 5
VAR(0, masterqueue, 0, 1, 1);
VAR(0, masterqueuelock, 0, QUEUE_PRIVMODERATOR, QUEUE_PRIVDEVELOPER+1); // Privilege that sees each entry rather than only the counts
VAR(0, masterqueuediscordlock, 0, QUEUE_PRIVDEVELOPER, QUEUE_PRIVDEVELOPER+1); // Privilege that may queue Discord members, the bot's
SVAR(0, masterqueuefile, "masterqueue.dat");
SVAR(0, masterqueuedevel, ""); // the version, such as 2.0.9, whose servers and players make up the devel queue, "" for none
SVAR(0, masterqueuerelease, ""); // the same for the release queue

enum { QUEUE_WEB = 1<<0, QUEUE_GAME = 1<<1, QUEUE_DISCORD = 1<<2 };

// each channel is the linked servers on one version, and of those only the ones on the newest protocol any of them reports,
// which is what an up to date game of that version runs
enum { QUEUE_DEVEL = 0, QUEUE_RELEASE, QUEUE_CHANNELS };
static const char * const queuechannelnames[QUEUE_CHANNELS] = { "devel", "release" };
struct queuechannel
{
    bool on;
    int build[3], protocol; // protocol: 0 while no server of that version is linked
};
static queuechannel queuechannels[QUEUE_CHANNELS];

static bool parsebuild(const char *s, int *b) { return s && sscanf(s, "%d.%d.%d", &b[0], &b[1], &b[2]) == 3; }

static int findchannel(const char *name)
{
    loopi(QUEUE_CHANNELS) if(queuechannels[i].on && !strcmp(queuechannelnames[i], name)) return i;
    return -1;
}

static int buildchannel(const int *b)
{
    loopi(QUEUE_CHANNELS) if(queuechannels[i].on && !memcmp(queuechannels[i].build, b, sizeof(queuechannels[i].build))) return i;
    return -1;
}

static void updatechannels()
{
    const char *versions[QUEUE_CHANNELS] = { masterqueuedevel, masterqueuerelease };
    loopi(QUEUE_CHANNELS)
    {
        queuechannels[i].on = parsebuild(versions[i], queuechannels[i].build);
        queuechannels[i].protocol = 0;
    }
    loopv(masterclients)
    {
        masterclient &s = *masterclients[i];
        if(!s.listserver || !s.reports()) continue;
        int c = buildchannel(s.statusbuild);
        if(c >= 0) queuechannels[c].protocol = max(queuechannels[c].protocol, s.statusver);
    }
}

// the channel a server is in, -1 for none
static int serverchannel(masterclient &s)
{
    if(!s.listserver || !s.reports()) return -1;
    int c = buildchannel(s.statusbuild);
    return c >= 0 && s.statusver == queuechannels[c].protocol ? c : -1;
}

static const char *serverchannelname(masterclient &s)
{
    int c = masterqueue ? serverchannel(s) : -1;
    return c >= 0 ? queuechannelnames[c] : NULL;
}

struct queueentry
{
    string ticket, discord, addr, pophost, heldhost;
    uint ip;
    int sources, threshold, socket, popport, heldport, channel, protocol; // socket: the viewer that owns it, -1 for none; heldport: 0 when not held;
                                                                        // protocol: the game's own, 0 when it came from elsewhere
    time_t added, expires, orphaned, popped; // expires: a Discord entry's end; orphaned: when its socket went; popped: when it was sent

    queueentry() : ip(0), sources(0), threshold(0), socket(-1), popport(0), heldport(0), channel(0), protocol(0), added(0), expires(0), orphaned(0), popped(0)
    {
        ticket[0] = discord[0] = addr[0] = pophost[0] = heldhost[0] = '\0';
    }

    // an orphan has nobody to tell where it was sent, so it waits until its socket claims it back
    bool active() const { return !popped && !heldport && (socket >= 0 || (sources&QUEUE_DISCORD)); }
};
static vector<queueentry *> readyqueue;

struct queueviewer
{
    int id, privilege, lastmsg;
    uint ip;
    string addr;
};
static vector<queueviewer> queueviewers;
static bool queuechanged = false, queuedirty = false; // changed: viewers need telling; dirty: the file needs writing
static vector<char> queuelast; // the summary viewers were last sent

static queueviewer *findqueueviewer(int id)
{
    loopv(queueviewers) if(queueviewers[i].id == id) return &queueviewers[i];
    return NULL;
}

static bool validticket(const char *s)
{
    if(int(strlen(s)) != QUEUE_TICKETBYTES*2) return false;
    for(; *s; s++) if(!isxdigit(*s)) return false;
    return true;
}

static bool validdiscord(const char *s)
{
    int len = strlen(s);
    if(!len || len >= QUEUE_DISCORDLEN) return false;
    for(; *s; s++) if(!isdigit(*s)) return false;
    return true;
}

static bool validthreshold(int t) { return t >= QUEUE_MINTHRESHOLD && t <= QUEUE_MAXTHRESHOLD && !(t&1); }

static void newticket(string &ticket)
{
    uchar bytes[QUEUE_TICKETBYTES];
    http::randombytes(bytes, sizeof(bytes));
    ticket[0] = '\0';
    loopi(QUEUE_TICKETBYTES) { defformatstring(h, "%02x", bytes[i]); concatstring(ticket, h); }
}

static queueentry *findticket(const char *ticket)
{
    if(!*ticket) return NULL;
    loopv(readyqueue) if(!strcmp(readyqueue[i]->ticket, ticket)) return readyqueue[i];
    return NULL;
}

static queueentry *finddiscord(const char *discord)
{
    if(!*discord) return NULL;
    loopv(readyqueue) if(!strcmp(readyqueue[i]->discord, discord)) return readyqueue[i];
    return NULL;
}

static queueentry *findqueuesocket(int id)
{
    loopv(readyqueue) if(readyqueue[i]->socket == id) return readyqueue[i];
    return NULL;
}

static void queuesources(jsonwriter &w, int sources)
{
    w.beginarray("sources");
    if(sources&QUEUE_WEB) w.str(NULL, "web");
    if(sources&QUEUE_GAME) w.str(NULL, "game");
    if(sources&QUEUE_DISCORD) w.str(NULL, "discord");
    w.endarray();
}

// the eligible servers a pop may go to: in a channel, open and with room
static bool queueeligible(masterclient &s)
{
    if(serverchannel(s) < 0 || s.numplayers < 0 || s.statusmode != 0) return false;
    return s.statusmax > 0;
}

// popped entries on their way to a server count as on it, until they arrive or QUEUE_TRAVEL runs out
static int queuetravelling(masterclient &s)
{
    int n = 0;
    loopv(readyqueue) if(readyqueue[i]->popped && readyqueue[i]->popport == s.port && !strcmp(readyqueue[i]->pophost, s.name)) n++;
    return n;
}

static void queueplace(masterclient &s, int &humans, int &free)
{
    int travel = queuetravelling(s);
    humans = s.numplayers + travel;
    free = s.statusmax - humans;
}

static bool sortthreshold(queueentry *a, queueentry *b) { return a->threshold < b->threshold; }

// a channel's entries that can be popped, which for a game means one on the channel's protocol
static void activeentries(vector<queueentry *> &list, int channel)
{
    list.setsize(0);
    int protocol = queuechannels[channel].protocol;
    loopv(readyqueue)
    {
        queueentry *e = readyqueue[i];
        if(e->active() && e->channel == channel && (!e->protocol || e->protocol == protocol)) list.add(e);
    }
    list.sort(sortthreshold);
}

// the largest group, from the lowest Thresholds up, whose Thresholds are all met by itself and those already there
static int queuegroup(const vector<queueentry *> &list, int humans, int free)
{
    for(int k = min(list.length(), free); k > 0; k--) if(list[k-1]->threshold <= humans + k) return k;
    return 0;
}

// how close a channel is: the fewest more people some Threshold waits for, on the server that would get them
static void queuenext(const vector<queueentry *> &list, int channel, int &threshold, int &needed)
{
    threshold = needed = 0;
    loopv(masterclients)
    {
        masterclient &s = *masterclients[i];
        if(!queueeligible(s) || serverchannel(s) != channel) continue;
        int humans, free;
        queueplace(s, humans, free);
        loopvj(list)
        {
            if(j + 1 < list.length() && list[j+1]->threshold == list[j]->threshold) continue;
            int t = list[j]->threshold, count = min(j + 1, free), want = max(t - humans - count, 1);
            if(count + want > free) continue;
            if(!needed || want < needed || (want == needed && t < threshold)) { threshold = t; needed = want; }
        }
    }
}

static void writeentry(jsonwriter &w, queueentry &e, bool tickets)
{
    w.beginobject();
    if(tickets) w.str("ticket", e.ticket);
    queuesources(w, e.sources);
    w.str("channel", queuechannelnames[clamp(e.channel, 0, QUEUE_CHANNELS-1)]);
    if(e.protocol) w.num("protocol", e.protocol);
    if(e.discord[0]) w.str("discord", e.discord);
    if(e.addr[0]) w.str("ip", e.addr);
    w.num("threshold", e.threshold);
    w.num("added", double(e.added));
    if(e.expires) w.num("expires", double(e.expires));
    if(e.orphaned) w.num("orphaned", double(e.orphaned));
    if(e.heldport)
    {
        w.beginobject("held");
        w.str("host", e.heldhost);
        w.num("port", e.heldport);
        w.endobject();
    }
    if(e.popped)
    {
        w.beginobject("popped");
        w.str("host", e.pophost);
        w.num("port", e.popport);
        w.num("time", double(e.popped));
        w.endobject();
    }
    w.endobject();
}

static void writenext(jsonwriter &w, int threshold, int needed)
{
    if(!needed) { w.null("next"); return; }
    w.beginobject("next");
    w.num("threshold", threshold);
    w.num("needed", needed);
    w.endobject();
}

// counts for everyone, for each channel and all together, and each entry for those at masterqueuelock and above
static void writequeue(jsonwriter &w, int privilege, const char *name = "queue")
{
    static vector<queueentry *> list;
    int ready = 0, held = 0, popped = 0, threshold = 0, needed = 0;
    w.beginobject(name);
    w.beginarray("channels");
    loopi(QUEUE_CHANNELS)
    {
        queuechannel &c = queuechannels[i];
        if(!c.on) continue;
        activeentries(list, i);
        int cheld = 0, cpopped = 0, ct, cn;
        loopvj(readyqueue) if(readyqueue[j]->channel == i) { if(readyqueue[j]->heldport) cheld++; else if(readyqueue[j]->popped) cpopped++; }
        queuenext(list, i, ct, cn);
        w.beginobject();
        w.str("name", queuechannelnames[i]);
        defformatstring(version, "%d.%d.%d", c.build[0], c.build[1], c.build[2]);
        w.str("version", version);
        int servers = 0;
        loopvj(masterclients) if(serverchannel(*masterclients[j]) == i) servers++;
        w.num("servers", servers);
        w.num("ready", list.length());
        w.num("held", cheld);
        w.num("travelling", cpopped);
        writenext(w, ct, cn);
        w.endobject();
        ready += list.length();
        held += cheld;
        popped += cpopped;
        if(cn && (!needed || cn < needed || (cn == needed && ct < threshold))) { threshold = ct; needed = cn; }
    }
    w.endarray();
    w.num("ready", ready);
    w.num("held", held);
    w.num("travelling", popped);
    w.beginarray("thresholds");
    for(int t = QUEUE_MINTHRESHOLD; t <= QUEUE_MAXTHRESHOLD; t += 2) w.num(NULL, t);
    w.endarray();
    writenext(w, threshold, needed);
    if(privilege >= masterqueuelock)
    {
        w.beginarray("entries");
        loopv(readyqueue) writeentry(w, *readyqueue[i], privilege >= masterqueuediscordlock);
        w.endarray();
    }
    w.endobject();
}

static void queuemessage(vector<char> &buf, int privilege)
{
    jsonwriter w(buf);
    w.beginobject();
    w.str("event", "queue");
    w.num("time", double(time(NULL)));
    writequeue(w, privilege);
    w.endobject();
}

static void sendqueue(int id, int privilege)
{
    vector<char> buf;
    queuemessage(buf, privilege);
    http::wssend(id, buf.getbuf(), buf.length());
}

static void routequeue(httprequest &req, httpresponse &res)
{
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.num("time", double(time(NULL)));
    writequeue(w, req.privilege);
    w.endobject();
    res.headers.set("Cache-Control", "no-store");
    res.json(buf.getbuf(), buf.length());
}

// what the owner of an entry hears of it
static void tellowner(queueentry &e, const char *event, const char *reason = NULL)
{
    if(e.socket < 0) return;
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("event", event);
    w.num("time", double(time(NULL)));
    if(!strcmp(event, "queued"))
    {
        w.str("ticket", e.ticket);
        w.num("threshold", e.threshold);
        w.str("channel", queuechannelnames[clamp(e.channel, 0, QUEUE_CHANNELS-1)]);
    }
    if(reason) w.str("reason", reason);
    w.endobject();
    http::wssend(e.socket, buf.getbuf(), buf.length());
}

// those who may queue Discord members hear when one of theirs leaves, so the bot can keep its own view and tell them
static void tellbots(queueentry &e, const char *reason)
{
    if(!e.discord[0] || !(e.sources&QUEUE_DISCORD)) return;
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("event", "unqueued");
    w.num("time", double(time(NULL)));
    w.str("discord", e.discord);
    w.str("ticket", e.ticket);
    w.str("reason", reason);
    w.endobject();
    loopv(queueviewers) if(queueviewers[i].privilege >= masterqueuediscordlock) http::wssend(queueviewers[i].id, buf.getbuf(), buf.length());
}

static void removeentry(int i, const char *reason)
{
    queueentry *e = readyqueue[i];
    tellowner(*e, "unqueued", reason);
    tellbots(*e, reason);
    delete e;
    readyqueue.remove(i);
    queuechanged = queuedirty = true;
}

static void removeentry(queueentry *e, const char *reason)
{
    int i = readyqueue.find(e);
    if(i >= 0) removeentry(i, reason);
}

// a group sent to a server: each owner hears where with its Ticket for the join link, the bot hears the lot with theirs
static void popgroup(masterclient &s, vector<queueentry *> &group)
{
    time_t now = time(NULL);
    loopv(group)
    {
        queueentry &e = *group[i];
        e.popped = now;
        copystring(e.pophost, s.name);
        e.popport = s.port;
    }
    loopv(queueviewers)
    {
        queueviewer &v = queueviewers[i];
        queueentry *own = findqueuesocket(v.id);
        bool mine = own && group.find(own) >= 0, bot = v.privilege >= masterqueuediscordlock;
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "pop");
        w.num("time", double(now));
        w.str("host", s.name);
        w.num("port", s.port);
        w.num("count", group.length());
        if(mine) w.str("ticket", own->ticket);
        if(v.privilege >= masterqueuelock)
        {
            w.beginarray("entries");
            loopvj(group) writeentry(w, *group[j], bot);
            w.endarray();
        }
        w.endobject();
        http::wssend(v.id, buf.getbuf(), buf.length());
    }
    conoutf(colourwhite, "Ready queue sent %d to %s:%d", group.length(), s.name, s.port);
    queuechanged = queuedirty = true;
}

static masterclient *queuetagged(queueentry &e)
{
    loopv(masterclients)
    {
        masterclient &s = *masterclients[i];
        if(!s.listserver || s.link < 0) continue;
        loopvj(s.queuetags)
        {
            queuetag &t = s.queuetags[j];
            if(!strcmp(t.ticket, e.ticket) || (e.discord[0] && !strcmp(t.discord, e.discord))) return &s;
        }
    }
    return NULL;
}

static void checkpops()
{
    static vector<queueentry *> list, group;
    for(;;)
    {
        masterclient *best = NULL;
        int bestk = 0, besthumans = 0;
        loopv(masterclients)
        {
            masterclient &s = *masterclients[i];
            if(!queueeligible(s)) continue;
            activeentries(list, serverchannel(s));
            int humans, free;
            queueplace(s, humans, free);
            int k = queuegroup(list, humans, free);
            if(!k) continue;
            if(!best || k > bestk || (k == bestk && (humans > besthumans || (humans == besthumans && s.priority() > best->priority()))))
            {
                best = &s;
                bestk = k;
                besthumans = humans;
            }
        }
        if(!best) return;
        activeentries(list, serverchannel(*best));
        group.setsize(0);
        loopi(bestk) group.add(list[i]);
        popgroup(*best, group);
    }
}

// once a second: endings, arrivals and holds, then any pops, then viewers hear of any change
static void checkqueue()
{
    updatechannels();
    if(!masterqueue) return;
    time_t now = time(NULL);
    loopvrev(readyqueue)
    {
        queueentry &e = *readyqueue[i];
        if(!queuechannels[e.channel].on) { removeentry(i, "closed"); continue; } // its channel was taken out of the config
        masterclient *on = queuetagged(e);
        if(e.popped)
        {
            if(on && on->port == e.popport && !strcmp(on->name, e.pophost)) { removeentry(i, "arrived"); continue; }
            if(now - e.popped >= QUEUE_TRAVEL) { removeentry(i, "popped"); continue; }
        }
        else
        {
            int port = on ? on->port : 0;
            if(port != e.heldport || (on && strcmp(on->name, e.heldhost)))
            {
                e.heldport = port;
                copystring(e.heldhost, on ? on->name : "");
                queuechanged = true;
            }
            if(on && on->numplayers >= e.threshold) { removeentry(i, "satisfied"); continue; }
        }
        bool owned = e.socket >= 0 || (e.orphaned && now - e.orphaned < QUEUE_RECLAIM), timed = e.expires > now;
        if(!owned && !timed && !e.popped) { removeentry(i, e.expires ? "expired" : "left"); continue; }
        if(e.expires && !timed && (e.sources&QUEUE_DISCORD))
        {
            e.sources &= ~QUEUE_DISCORD;
            e.expires = 0;
            tellbots(e, "expired");
            queuechanged = queuedirty = true;
        }
    }
    checkpops();
    if(queuedirty) savequeue(); // at most once a second
    // the counts move with the servers as well as the queue, so they're compared every time, and anonymous viewers only hear when they change
    vector<char> counts;
    jsonwriter cw(counts);
    writequeue(cw, HTTP_PUBLIC, NULL);
    bool same = counts.length() == queuelast.length() && !memcmp(counts.getbuf(), queuelast.getbuf(), counts.length());
    if(same && !queuechanged) return;
    queuelast = counts;
    vector<char> pub;
    if(!same) queuemessage(pub, HTTP_PUBLIC);
    loopv(queueviewers)
    {
        queueviewer &qv = queueviewers[i];
        if(qv.privilege >= masterqueuelock) sendqueue(qv.id, qv.privilege);
        else if(!same) http::wssend(qv.id, pub.getbuf(), pub.length());
    }
    queuechanged = false;
}

static int queueowned(uint ip)
{
    int n = 0;
    loopv(readyqueue) if(readyqueue[i]->ip == ip && (readyqueue[i]->sources&(QUEUE_WEB|QUEUE_GAME))) n++;
    return n;
}

// a refusal, naming the member when it was about one, so the bot knows whose it is
static void queuereply(int id, const char *error, const char *discord = NULL)
{
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("event", "queued");
    w.num("time", double(time(NULL)));
    if(discord && *discord) w.str("discord", discord);
    w.str("error", error);
    w.endobject();
    http::wssend(id, buf.getbuf(), buf.length());
}

// a web page or the game joining for itself; a Ticket claims an entry back, a Discord ID merges with that member's Discord entry
static void queuejoin(queueviewer &v, jsonvalue *msg)
{
    int threshold = msg->getint("threshold");
    if(!validthreshold(threshold)) { queuereply(v.id, "bad threshold"); return; }
    const char *source = msg->getstr("source", "web"), *ticket = msg->getstr("ticket", ""), *discord = msg->getstr("discord", "");
    int sources = !strcmp(source, "game") ? QUEUE_GAME : QUEUE_WEB, channel = -1, protocol = 0;
    if(sources == QUEUE_GAME)
    {
        // a game's channel is its version, and it can only be sent where its protocol is played
        int build[3];
        if(parsebuild(msg->getstr("version", ""), build)) channel = buildchannel(build);
        protocol = msg->getint("protocol");
        if(channel < 0) { queuereply(v.id, "this version of the game has no queue"); return; }
        if(protocol <= 0 || (queuechannels[channel].protocol && protocol != queuechannels[channel].protocol)) { queuereply(v.id, "update your game to queue"); return; }
    }
    else if((channel = findchannel(msg->getstr("channel", ""))) < 0) { queuereply(v.id, "no such channel"); return; }
    if(*discord && !validdiscord(discord)) discord = "";
    queueentry *e = findqueuesocket(v.id);
    if(!e && validticket(ticket)) e = findticket(ticket);
    if(!e && *discord) e = finddiscord(discord);
    if(e && e->socket >= 0 && e->socket != v.id)
    {
        // a Ticket was handed to this socket, so the entry follows it
        tellowner(*e, "unqueued", "moved");
        e->socket = -1;
    }
    if(!e)
    {
        if(readyqueue.length() >= QUEUE_MAXENTRIES) { queuereply(v.id, "the queue is full"); return; }
        if(queueowned(v.ip) >= QUEUE_ADDRLIMIT) { queuereply(v.id, "too many entries from your address"); return; }
        e = readyqueue.add(new queueentry);
        newticket(e->ticket);
        e->added = time(NULL);
    }
    else if(e->popped) e->popped = 0; // queuing again after being sent somewhere starts over
    e->socket = v.id;
    e->orphaned = 0;
    e->ip = v.ip;
    copystring(e->addr, v.addr);
    e->sources |= sources;
    e->threshold = threshold;
    e->channel = channel;
    e->protocol = protocol;
    if(*discord) copystring(e->discord, discord);
    tellowner(*e, "queued");
    queuechanged = queuedirty = true;
}

// the bot queuing, renewing or removing one of its Discord members
static void queuediscord(queueviewer &v, const char *op, jsonvalue *msg)
{
    const char *discord = msg->getstr("discord", "");
    if(!validdiscord(discord)) { queuereply(v.id, "bad discord id"); return; }
    queueentry *e = finddiscord(discord);
    time_t now = time(NULL);
    if(!strcmp(op, "leave"))
    {
        if(e) removeentry(e, "left");
        else queuereply(v.id, "not queued", discord);
        return;
    }
    if(!strcmp(op, "renew"))
    {
        if(!e || !(e->sources&QUEUE_DISCORD)) { queuereply(v.id, "not queued", discord); return; }
        e->expires = min(max(e->expires, now) + QUEUE_DISCORDRENEW, now + QUEUE_DISCORDMAX);
    }
    else
    {
        int threshold = msg->getint("threshold"), channel = findchannel(msg->getstr("channel", ""));
        if(!validthreshold(threshold)) { queuereply(v.id, "bad threshold", discord); return; }
        if(channel < 0) { queuereply(v.id, "no such channel", discord); return; }
        if(!e)
        {
            if(readyqueue.length() >= QUEUE_MAXENTRIES) { queuereply(v.id, "the queue is full", discord); return; }
            e = readyqueue.add(new queueentry);
            newticket(e->ticket);
            copystring(e->discord, discord);
            e->added = now;
        }
        else if(e->popped) e->popped = 0;
        e->threshold = threshold;
        e->channel = channel;
        e->protocol = 0; // a member's place goes wherever the channel's players can join, whatever their game was
        if(!(e->sources&QUEUE_DISCORD) || e->expires < now + QUEUE_DISCORDTIME) e->expires = now + QUEUE_DISCORDTIME;
        e->sources |= QUEUE_DISCORD;
    }
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("event", "queued");
    w.num("time", double(now));
    w.str("discord", e->discord);
    w.str("ticket", e->ticket);
    w.num("threshold", e->threshold);
    w.str("channel", queuechannelnames[e->channel]);
    w.num("expires", double(e->expires));
    w.endobject();
    http::wssend(v.id, buf.getbuf(), buf.length());
    queuechanged = queuedirty = true;
}

static void queueviewermessage(httpwsevent &e)
{
    queueviewer *v = findqueueviewer(e.id);
    if(!v || !masterqueue || e.opcode != WS_TEXT) return;
    bool bot = v->privilege >= masterqueuediscordlock;
    if(!bot && v->lastmsg && totalmillis - v->lastmsg < QUEUE_MSGRATE) return;
    v->lastmsg = totalmillis ? totalmillis : 1;
    jsonvalue *msg = json::parse(e.data, e.len);
    if(!msg) return;
    if(msg->type != JSON_OBJECT) { delete msg; return; }
    const char *op = msg->getstr("queue", "");
    // the bot's messages name a member and no source, a page's or the game's name their source
    if(!msg->get("source") && *msg->getstr("discord", ""))
    {
        if(bot) queuediscord(*v, op, msg);
        else queuereply(v->id, "insufficient privilege");
    }
    else if(!strcmp(op, "join")) queuejoin(*v, msg);
    else if(!strcmp(op, "leave"))
    {
        queueentry *q = findqueuesocket(v->id);
        if(q && (q->sources&QUEUE_DISCORD))
        {
            // a Discord ID merged it, and an ID is only claimed, so the member's own entry stays theirs
            tellowner(*q, "unqueued", "left");
            q->socket = -1;
            q->orphaned = 0;
            q->sources &= ~(QUEUE_WEB|QUEUE_GAME);
            queuechanged = queuedirty = true;
        }
        else if(q) removeentry(q, "left");
    }
    delete msg;
}

// a viewer's Privilege is whatever its token gave it as it connected
static void queueviewerjoined(httpwsevent &e)
{
    queueviewer &v = queueviewers.add();
    v.id = e.id;
    v.privilege = e.req ? e.req->privilege : HTTP_PUBLIC;
    v.lastmsg = 0;
    v.ip = e.req ? e.req->ip : 0;
    copystring(v.addr, e.req ? e.req->addr : "");
    if(masterqueue) sendqueue(e.id, v.privilege);
}

static void queueviewerclosed(int id)
{
    loopv(queueviewers) if(queueviewers[i].id == id) { queueviewers.remove(i); break; }
    queueentry *e = findqueuesocket(id);
    if(!e) return;
    e->socket = -1;
    e->orphaned = time(NULL);
    queuedirty = true;
}

static void queuetags(masterclient &c, jsonvalue *list)
{
    c.queuetags.shrink(0);
    if(!list || list->type != JSON_ARRAY) return;
    loopv(list->items)
    {
        jsonvalue *t = list->items[i];
        if(t->type != JSON_OBJECT || c.queuetags.length() >= MAXCLIENTS) continue;
        const char *ticket = t->getstr("ticket", ""), *discord = t->getstr("discord", "");
        if(!validticket(ticket)) ticket = "";
        if(!validdiscord(discord)) discord = "";
        if(!*ticket && !*discord) continue;
        queuetag &q = c.queuetags.add();
        copystring(q.ticket, ticket);
        copystring(q.discord, discord);
    }
}

static void loadqueue()
{
    readyqueue.deletecontents();
    if(!*masterqueuefile) return;
    stream *f = opengzfile(masterqueuefile, "rb");
    if(!f) return;
    char magic[4];
    bool ok = f->read(magic, 4) == 4 && !memcmp(magic, "REMQ", 4) && f->getlil<int>() == QUEUE_VERSION;
    int count = ok ? f->getlil<int>() : 0;
    if(count < 0 || count > QUEUE_MAXENTRIES) ok = false;
    time_t now = time(NULL);
    for(int i = 0; ok && i < count; i++)
    {
        queueentry *e = new queueentry;
        string *strs[4] = { &e->ticket, &e->discord, &e->addr, &e->pophost };
        loopj(4)
        {
            int len = f->getlil<int>();
            if(len < 0 || len >= MAXSTRLEN || (len && f->read(*strs[j], len) != size_t(len))) { ok = false; break; }
            (*strs[j])[len] = '\0';
        }
        if(!ok) { delete e; break; }
        e->ip = f->getlil<uint>();
        e->sources = f->getlil<int>();
        e->threshold = f->getlil<int>();
        e->popport = f->getlil<int>();
        e->channel = f->getlil<int>();
        e->protocol = f->getlil<int>();
        e->added = time_t(f->getlil<int64_t>());
        e->expires = time_t(f->getlil<int64_t>());
        e->orphaned = time_t(f->getlil<int64_t>());
        e->popped = time_t(f->getlil<int64_t>());
        if(!validticket(e->ticket) || !validthreshold(e->threshold) || e->channel < 0 || e->channel >= QUEUE_CHANNELS || findticket(e->ticket)) { delete e; continue; }
        // their sockets are gone, so each gets the time to reconnect that a dropped one would
        if(e->sources&(QUEUE_WEB|QUEUE_GAME)) e->orphaned = now;
        readyqueue.add(e);
    }
    delete f;
    if(!ok)
    {
        conoutf(colourred, "Failed to read the Ready queue from %s, starting a new one", masterqueuefile);
        readyqueue.deletecontents();
        return;
    }
    if(readyqueue.length()) conoutf(colourwhite, "Loaded %d Ready queue entries from %s", readyqueue.length(), masterqueuefile);
}

// written beside the old one then moved over it, as the history is; an entry that ends is gone from the next copy, its Discord ID with it
static void savequeue()
{
    queuedirty = false;
    if(!*masterqueuefile) return;
    defformatstring(newname, "%s.new", masterqueuefile);
    stream *f = opengzfile(newname, "wb");
    if(!f) { conoutf(colourred, "Failed to save the Ready queue to %s", newname); return; }
    f->write("REMQ", 4);
    f->putlil<int>(QUEUE_VERSION);
    f->putlil<int>(readyqueue.length());
    loopv(readyqueue)
    {
        queueentry &e = *readyqueue[i];
        const char *strs[4] = { e.ticket, e.discord, e.addr, e.pophost };
        loopj(4)
        {
            int len = strlen(strs[j]);
            f->putlil<int>(len);
            if(len) f->write(strs[j], len);
        }
        f->putlil<uint>(e.ip);
        f->putlil<int>(e.sources);
        f->putlil<int>(e.threshold);
        f->putlil<int>(e.popport);
        f->putlil<int>(e.channel);
        f->putlil<int>(e.protocol);
        f->putlil<int64_t>(int64_t(e.added));
        f->putlil<int64_t>(int64_t(e.expires));
        f->putlil<int64_t>(int64_t(e.orphaned));
        f->putlil<int64_t>(int64_t(e.popped));
    }
    delete f;
    string from, to;
    copystring(from, findfile(newname, "w"));
    copystring(to, findfile(masterqueuefile, "w"));
    remove(to);
    if(rename(from, to)) conoutf(colourred, "Failed to move the Ready queue into %s", masterqueuefile);
}

// the relay: browsers reach game servers through their link, the master keeps nothing it carries
VAR(0, httprelaycache, 0, 2000, 60000); // how long anonymous GETs are kept
VAR(0, httprelaycachemax, 0, 3600, 86400); // seconds an anonymous GET may be kept for when the server's Cache-Control asks for longer
VAR(0, httprelaycachesize, 0, 32<<20, VAR_MAX); // bytes the relay may keep, the oldest go first
VAR(0, httprelaymaxrate, 0, 262144, VAR_MAX); // bytes a second relayed for each server, 0 is unlimited

struct relaypending
{
    int id, deferred, link, time;
    char *cachekey;
};
static vector<relaypending> relaypendings;
static int lastrelayid = 0;

struct relaycached
{
    char *key, *type, *etag, *control;
    int status, time, life;
    vector<char> body;

    relaycached() : key(NULL), type(NULL), etag(NULL), control(NULL), status(0), time(0), life(0) {}
    ~relaycached() { DELETEA(key); DELETEA(type); DELETEA(etag); DELETEA(control); }
};
static vector<relaycached *> relaycache;

struct relayviewer
{
    int id, link;
    bool secure; // only viewers on wss:// may sign in
};
static vector<relayviewer> relayviewers;

struct relayrate
{
    int link, window, bytes;
};
static vector<relayrate> relayrates;

// the address and port are separate segments, as an IPv6 address has colons of its own
static masterclient *findrelay(httprequest &req)
{
    const char *address = req.param("address"), *port = req.param("port");
    if(!address || !port || !*port || port[strspn(port, "0123456789")]) return NULL;
    int p = atoi(port);
    loopv(masterclients)
    {
        masterclient &c = *masterclients[i];
        if(c.link < 0 || !c.isserver || c.port != p) continue;
        if(!strcmp(c.name, address)) return &c;
    }
    return NULL;
}

static bool relayallowed(masterclient &c, int bytes)
{
    if(!httprelaymaxrate) return true;
    relayrate *r = NULL;
    loopv(relayrates) if(relayrates[i].link == c.link) { r = &relayrates[i]; break; }
    if(!r) { r = &relayrates.add(); r->link = c.link; r->window = totalmillis; r->bytes = 0; }
    if(totalmillis - r->window >= 1000) { r->window = totalmillis; r->bytes = 0; }
    if(r->bytes > httprelaymaxrate) return false;
    r->bytes += bytes;
    return true;
}

// an answer the server says may be kept, such as a map's preview, is kept that long up to httprelaycachemax, others for httprelaycache
static int relaycachelife(const char *control)
{
    if(!control || strstr(control, "no-store") || strstr(control, "no-cache") || strstr(control, "private")) return httprelaycache;
    const char *age = strstr(control, "max-age=");
    if(!age) return httprelaycache;
    return max(httprelaycache, clamp(atoi(age + 8), 0, httprelaycachemax) * 1000);
}

static void relaycachelookup(const char *key, httpresponse &res, bool &found)
{
    found = false;
    loopvrev(relaycache)
    {
        relaycached *e = relaycache[i];
        if(totalmillis - e->time > e->life) { delete e; relaycache.remove(i); continue; }
        if(found || strcmp(e->key, key)) continue; // the newest answer, once, while the loop still clears out old ones
        res.status = e->status;
        if(e->type) res.headers.set("Content-Type", e->type);
        if(e->etag) res.headers.set("ETag", e->etag);
        if(e->control) res.headers.set("Cache-Control", e->control);
        res.body.put(e->body.getbuf(), e->body.length());
        found = true;
    }
}

// what a game server answers is shown on the master's origin, so it gets only these headers and its pages run in a sandbox of their own
static const char * const relaykeep[] = { "Content-Type", "ETag", "Cache-Control", "Allow", "WWW-Authenticate", "Retry-After", NULL };

static void relayheaders(httpresponse &res)
{
    res.headers.set("X-Content-Type-Options", "nosniff");
    res.headers.set("Content-Security-Policy", "sandbox allow-scripts allow-forms allow-popups");
}

static void relayroute(httprequest &req, httpresponse &res)
{
    masterclient *c = findrelay(req);
    if(!c) { res.error(404, "no such server on the relay"); return; }
    if(c->relay < 1) { res.error(403, "this server doesn't allow relaying"); return; }
    const char *rest = req.param("*");
    if(rest && strlen(rest) >= MAXSTRLEN - 1) { res.error(414, "that path is too long to relay"); return; } // rather than cut short, asking for another path
    defformatstring(path, "/%s", rest ? rest : "");
    // a token that came over plain http from somewhere else has been seen on the way, so it doesn't cross
    bool anon = !req.headers.get("Authorization") || c->relay < 2 || (!req.secure && !req.local);
    string cachekey = "";
    if(anon && httprelaycache && req.method == HTTP_GET)
    {
        // a key cut short would let two requests share an answer, so those aren't kept
        int keylen = snprintf(cachekey, sizeof(cachekey), "%s:%d%s?%s", c->name, c->port, path, req.query ? req.query : "");
        if(keylen < 0 || keylen >= int(sizeof(cachekey))) cachekey[0] = '\0';
        else
        {
            bool found = false;
            relaycachelookup(cachekey, res, found);
            if(found) { relayheaders(res); return; }
        }
    }
    if(!relayallowed(*c, req.body.length() + 512)) { res.error(503, "this server's relay is busy, try again shortly"); res.headers.set("Retry-After", "1"); return; }
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.utf8("relay", "request");
    int id = lastrelayid = lastrelayid >= 0x3FFFFFFF ? 1 : lastrelayid + 1;
    w.num("id", id);
    w.utf8("method", http::methodnames[req.method]);
    w.utf8("path", path);
    if(req.query) w.utf8("query", req.query);
    w.utf8("ip", req.addr);
    w.beginobject("headers");
    static const char * const keep[] = { "Content-Type", "Accept", "Origin", "If-None-Match", NULL };
    for(int i = 0; keep[i]; i++) { const char *v = req.headers.get(keep[i]); if(v) w.utf8(keep[i], v); }
    if(!anon) w.utf8("Authorization", req.headers.get("Authorization"));
    w.endobject();
    if(req.body.length()) w.utf8("body", req.body.getbuf(), req.body.length());
    w.endobject();
    if(!http::wssend(c->link, buf.getbuf(), buf.length())) { res.error(502, "lost the link to that server"); return; }
    relaypending &p = relaypendings.add();
    p.id = id;
    p.link = c->link;
    p.time = totalmillis;
    p.deferred = http::deferred(req);
    p.cachekey = cachekey[0] ? newstring(cachekey) : NULL;
    res.defer();
}

static void relayfeedcount(masterclient &c)
{
    int viewers = 0;
    loopv(relayviewers) if(relayviewers[i].link == c.link) viewers++;
    defformatstring(msg, "{\"relay\":\"feed\",\"on\":%s}", viewers ? "true" : "false");
    http::wssend(c.link, msg);
}

static void relayresubscribe(masterclient &c)
{
    loopv(relayviewers) if(relayviewers[i].link == c.link) { relayfeedcount(c); return; }
}

static void relayviewerevent(httpwsevent &e, void *ctx)
{
    switch(e.type)
    {
        case WSEV_OPEN:
        {
            masterclient *c = e.req ? findrelay(*e.req) : NULL;
            if(!c || c->relay < 1) { http::wsclose(e.id, 1008); break; }
            relayviewer &v = relayviewers.add();
            v.id = e.id;
            v.link = c->link;
            v.secure = e.req->secure || e.req->local;
            vector<char> msg;
            jsonwriter w(msg);
            w.beginobject();
            w.utf8("relay", "viewer");
            w.num("id", e.id);
            w.utf8("ip", e.req->addr);
            const char *token = e.req->var("token");
            if(token && *token && c->relay >= 2 && (e.req->secure || e.req->local)) w.utf8("token", token); // only where the server lets tokens cross the master, and never one sent in the clear
            w.endobject();
            http::wssend(c->link, msg.getbuf(), msg.length());
            int count = 0;
            loopv(relayviewers) if(relayviewers[i].link == c->link) count++;
            if(count == 1) relayfeedcount(*c);
            break;
        }
        case WSEV_CLOSE:
        {
            loopv(relayviewers) if(relayviewers[i].id == e.id)
            {
                int link = relayviewers[i].link;
                relayviewers.remove(i);
                defformatstring(gone, "{\"relay\":\"viewergone\",\"id\":%d}", e.id);
                http::wssend(link, gone);
                bool others = false;
                loopvj(relayviewers) if(relayviewers[j].link == link) { others = true; break; }
                if(!others) { masterclient *c = findlink(link); if(c) relayfeedcount(*c); }
                break;
            }
            break;
        }
        case WSEV_MESSAGE:
        {
            // at httprelay 2 a viewer may sign in, which the game server decides
            relayviewer *v = NULL;
            loopv(relayviewers) if(relayviewers[i].id == e.id) { v = &relayviewers[i]; break; }
            masterclient *c = v ? findlink(v->link) : NULL;
            if(!c || e.opcode != WS_TEXT) break;
            if(c->relay < 2) { http::wssend(e.id, "{\"event\":\"auth\",\"error\":\"this server doesn't allow signing in through the relay\"}"); break; }
            // what a signed in viewer is sent, addresses among it, mustn't cross in the clear
            if(!v->secure) { http::wssend(e.id, "{\"event\":\"auth\",\"error\":\"signing in through the relay needs wss://\"}"); break; }
            if(!relayallowed(*c, e.len + 64)) { http::wssend(e.id, "{\"event\":\"auth\",\"error\":\"this server's relay is busy, try again shortly\"}"); break; }
            vector<char> msg;
            jsonwriter w(msg);
            w.beginobject();
            w.utf8("relay", "viewermsg");
            w.num("id", e.id);
            w.utf8("data", e.data, e.len);
            w.endobject();
            http::wssend(c->link, msg.getbuf(), msg.length());
            break;
        }
        default: break;
    }
}

// a binary answer comes as its JSON head, a NUL, then the body as it is
static void relayjson(masterclient &c, const char *data, int len, const char *body, int bodylen)
{
    jsonvalue *v = json::parse(data, len);
    if(!v) return;
    jsonvalue *status = v->get("status");
    if(status && status->type == JSON_OBJECT)
    {
        // its times are by its own clock, which needn't agree with ours, so they're moved onto ours
        jsonvalue *sent = v->get("time");
        if(sent && sent->type == JSON_NUMBER)
        {
            double offset = double(time(NULL)) - sent->num;
            jsonvalue *last = status->get("lastmatch");
            jsonvalue *times[3] = { status->get("ends"), status->get("stateends"), last && last->type == JSON_OBJECT ? last->get("ended") : NULL };
            loopi(3) if(times[i] && times[i]->type == JSON_NUMBER && (i || times[i]->num >= 0))
            {
                times[i]->num += offset;
                DELETEA(times[i]->str);
            }
        }
        // written again from the parsed value, so the list only ever carries well formed JSON
        vector<char> buf;
        json::write(buf, status);
        if(buf.length() < STATUS_LIMIT)
        {
            buf.add('\0');
            DELETEA(c.status);
            c.status = newstring(buf.getbuf());
            jsonvalue *players = status->get("players");
            c.numplayers = players && players->type == JSON_ARRAY ? players->length() : -1;
            c.statusmax = status->getint("maxplayers");
            c.statusmode = status->getint("mastermode", -1);
            c.statusver = status->getint("gameversion");
            jsonvalue *build = status->get("build");
            loopi(3) c.statusbuild[i] = build && build->type == JSON_ARRAY && build->at(i) ? int(build->at(i)->num) : 0;
            queuetags(c, v->get("queue"));
            DELETEA(c.polled); // what it reports from now on is better than any poll
            c.pollwaiting = false;
            c.pollmisses = 0;
        }
        delete v;
        return;
    }
    const char *type = v->getstr("relay", "");
    if(!strcmp(type, "response"))
    {
        int id = v->getint("id", -1);
        loopv(relaypendings) if(relaypendings[i].id == id && relaypendings[i].link == c.link)
        {
            relaypending p = relaypendings.remove(i);
            double status = v->getnum("status", 502);
            httpresponse res(status >= 200 && status <= 599 ? int(status) : 502);
            jsonvalue *headers = v->get("headers");
            for(int k = 0; headers && relaykeep[k]; k++)
            {
                const char *val = headers->getstr(relaykeep[k]);
                if(val) res.headers.set(relaykeep[k], val);
            }
            relayheaders(res);
            if(body) res.body.put(body, bodylen);
            else
            {
                jsonvalue *text = v->get("body");
                if(text && text->str) res.body.put(text->str, strlen(text->str));
            }
            relayallowed(c, res.body.length());
            if(p.cachekey && res.status == 200)
            {
                // two misses for the same key can both be answered, only the newest is kept
                loopvjrev(relaycache) if(!strcmp(relaycache[j]->key, p.cachekey)) delete relaycache.remove(j);
                relaycached *e = new relaycached;
                e->key = p.cachekey;
                p.cachekey = NULL;
                const char *t = res.headers.get("Content-Type"), *etag = res.headers.get("ETag"), *control = res.headers.get("Cache-Control");
                e->type = t ? newstring(t) : NULL;
                e->etag = etag ? newstring(etag) : NULL;
                e->control = control ? newstring(control) : NULL;
                e->status = res.status;
                e->time = totalmillis;
                e->life = relaycachelife(control);
                e->body.put(res.body.getbuf(), res.body.length());
                relaycache.add(e);
                int bytes = 0;
                loopvj(relaycache) bytes += relaycache[j]->body.length();
                while(relaycache.length() > 1024 || (httprelaycachesize && bytes > httprelaycachesize && relaycache.length() > 1))
                {
                    relaycached *old = relaycache.remove(0);
                    bytes -= old->body.length();
                    delete old;
                }
            }
            DELETEA(p.cachekey);
            http::reply(p.deferred, res);
            break;
        }
    }
    else if(!strcmp(type, "event"))
    {
        jsonvalue *event = v->get("event");
        if(event)
        {
            static vector<int> ids;
            ids.setsize(0);
            loopv(relayviewers) if(relayviewers[i].link == c.link) ids.add(relayviewers[i].id);
            if(ids.length())
            {
                vector<char> buf;
                json::write(buf, event);
                http::wsbroadcast(ids, buf.getbuf(), buf.length());
            }
        }
    }
    else if(!strcmp(type, "tell"))
    {
        // for the viewers the game server names, which must be its own
        jsonvalue *ids = v->get("ids"), *msg = v->get("data");
        if(ids && msg && msg->str)
        {
            static vector<int> to;
            to.setsize(0);
            loopv(ids->items)
            {
                int id = jsonint(ids->items[i]->num);
                loopvj(relayviewers) if(relayviewers[j].id == id && relayviewers[j].link == c.link) { to.add(id); break; }
            }
            if(to.length()) http::wsbroadcast(to, msg->str, strlen(msg->str));
        }
    }
    else if(!strcmp(type, "deny"))
    {
        int id = v->getint("id", -1);
        loopv(relayviewers) if(relayviewers[i].id == id && relayviewers[i].link == c.link) { http::wsclose(id, 1008); relayviewers.remove(i); break; }
    }
    delete v;
}

static void relayclosed(masterclient &c)
{
    if(c.link < 0) return;
    loopvrev(relaypendings) if(relaypendings[i].link == c.link)
    {
        relaypending p = relaypendings.remove(i);
        DELETEA(p.cachekey);
        httpresponse res;
        res.error(502, "lost the link to that server");
        http::reply(p.deferred, res);
    }
    loopvrev(relayviewers) if(relayviewers[i].link == c.link) { http::wsclose(relayviewers[i].id, 1001); relayviewers.remove(i); }
    loopvrev(relayrates) if(relayrates[i].link == c.link) relayrates.remove(i);
}

static void checkmasterlinks()
{
    loopvrev(relaypendings) if(totalmillis - relaypendings[i].time > 60000) { DELETEA(relaypendings[i].cachekey); relaypendings.remove(i); }
    checklisted();
    loopv(masterclients)
    {
        masterclient &c = *masterclients[i];
        if(c.link < 0) continue;
        if(c.output.length())
        {
            http::wssend(c.link, c.output.getbuf(), c.output.length());
            c.output.setsize(0);
            c.outputpos = 0;
            if(c.shouldpurge) { purgemasterclient(i--); continue; }
        }
        if(ENET_TIME_DIFFERENCE(totalmillis, c.lastactivity) >= (c.isserver ? SERVER_TIME : CLIENT_TIME) ||
           (checkipinfo(control, ipinfo::BAN, c.address.host) && c.priority() <= 0 && !checkipinfo(control, ipinfo::EXCEPT, c.address.host) && !checkipinfo(control, ipinfo::TRUST, c.address.host)))
        {
            purgemasterclient(i--);
            continue;
        }
    }
}

void checkmaster()
{
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
                c.listserver = c.shouldping = false;
                masteroutf(c, "error \"ping attempts failed (tried %d times on port %d), server will not be listed\"\n", c.numpings, c.port+1);
            }
        }

        if(masterpollinterval && c.listserver && !c.shouldping && !c.reports() && ENET_TIME_DIFFERENCE(totalmillis, c.lastpoll) >= uint(masterpollinterval)*1000)
        {
            if(c.pollwaiting && ++c.pollmisses >= POLL_MISSES)
            {
                DELETEA(c.polled);
                c.numplayers = -1;
                c.pollmisses = 0;
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

void cleanupmaster()
{
    if(masterserver) { savehistory(); savequeue(); }
    loopvrev(masterclients) purgemasterclient(i);
    if(mastersocket != ENET_SOCKET_NULL) enet_socket_destroy(mastersocket);
}

void reloadmaster()
{
    clearauth();
}
