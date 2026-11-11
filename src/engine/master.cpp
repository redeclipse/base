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
    char *name, *flags, *email, *steamid;
    void *pubkey;
};

struct authreq
{
    enet_uint32 reqtime;
    uint id;
    void *answer;
    authuser *user;
    string hostname;

    authreq() { reset(); }
    ~authreq() {}

    void reset()
    {
        reqtime = 0;
        id = 0;
        answer = NULL;
        user = NULL;
        hostname[0] = '\0';
    }
};

struct masterclient
{
    ENetAddress address;
    ENetSocket socket;
    string name, desc, flags, authhandle, branch;

    char input[4096];
    vector<char> output;
    int inputpos, outputpos, port, numpings, lastcontrol, version, link, webport, relay;
    string fingerprint;
    char *status; // the last status it reported over the link, as JSON
    char *polled; // what its query reply last said, as JSON, while it reports no status
    uchar pollreq[8];
    int pollreqlen, pollmisses, polledends, polledstateends, polledstate;
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
        fingerprint[0] = '\0';
        lastping = lastpong = lastactivity = lastpoll = 0;
        pollreqlen = pollmisses = polledends = polledstateends = polledstate = 0;
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
static void relayresubscribe(masterclient &c);
static void relayroute(httprequest &req, httpresponse &res);
static void relayviewerevent(httpwsevent &e, void *ctx);
static void viewerevent(httpwsevent &e, void *ctx);
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
        http::addsocket(HTTP_API "/servers/live", HTTP_PUBLIC, viewerevent, NULL, 4096);
        http::addsocket(HTTP_API "/link", HTTP_PUBLIC, linkevent);
        // the feed first, as routes match in the order they were added
        http::addsocket(HTTP_RELAY "/:address/:port" HTTP_API "/events", HTTP_PUBLIC, relayviewerevent, NULL, 4096);
        for(int m = HTTP_GET; m < HTTP_MAXMETHODS; m++) if(m != HTTP_HEAD && m != HTTP_OPTIONS)
        {
            http::addroute(m, HTTP_RELAY "/:address/:port", HTTP_PUBLIC, relayroute);
            http::addroute(m, HTTP_RELAY "/:address/:port/*", HTTP_PUBLIC, relayroute);
        }
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
    u.email = newstring(email);
}
COMMAND(0, addserverauth, "ssss");

void clearauth()
{
    enumerate(authusers, authuser, u, { delete[] u.name; delete[] u.flags; delete[] u.email; delete[] u.steamid; freepubkey(u.pubkey); });
    authusers.clear();
    enumerate(serverauthusers, authuser, u, { delete[] u.name; delete[] u.flags; delete[] u.email; delete[] u.steamid; freepubkey(u.pubkey); });
    serverauthusers.clear();
}
COMMAND(0, clearauth, "");

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
    a.user = u;
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

    c.serverauthreq.user = u;
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
        if(checkchallenge(val, c.authreqs[i].answer))
        {
            masteroutf(c, "succauth %u \"%s\" \"%s\"\n", id, c.authreqs[i].user->name, c.authreqs[i].user->flags);
            conoutf(colourwhite, "Succeeded '%s' [%s] (%u) from %s on server %s\n", c.authreqs[i].user->name, c.authreqs[i].user->flags, id, c.authreqs[i].hostname, ip);
        }
        else
        {
            masteroutf(c, "failauth %u\n", id);
            conoutf(colourred, "Failed '%s' (%u) from %s on server %s (BADKEY)\n", c.authreqs[i].user->name, id, c.authreqs[i].hostname, ip);
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
    if(!c.serverauthreq.reqtime || !c.serverauthreq.user || !c.serverauthreq.answer)
    {
        masteroutf(c, "failserverauth\n");
        return;
    }

    string ip;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");

    if(checkchallenge(val, c.serverauthreq.answer))
    {
        masteroutf(c, "succserverauth \"%s\" \"%s\"\n", c.serverauthreq.user->name, c.serverauthreq.user->flags);
        conoutf(colourwhite, "Succeeded server '%s' [%s]\n", c.serverauthreq.user->name, c.serverauthreq.user->flags);
        copystring(c.authhandle, c.serverauthreq.user->name);
        copystring(c.flags, c.serverauthreq.user->flags);
    }
    else
    {
        masteroutf(c, "failserverauth\n");
        conoutf(colourred, "Failed server '%s' (BADKEY)\n", c.serverauthreq.user->name);
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
    if(s.reports()) w.raw("status", s.status);
    else if(s.polled) w.raw("status", s.polled);
    w.endobject();
}

static void serverlist(vector<char> &buf)
{
    jsonwriter w(buf);
    w.beginobject();
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
    vector<char> buf;
    serverlist(buf);
    defformatstring(etag, "\"%08x\"", listhash(buf));
    res.headers.set("ETag", etag);
    res.headers.set("Cache-Control", "public, max-age=5");
    const char *match = req.headers.get("If-None-Match");
    if(match && !strcmp(match, etag)) { res.status = 304; return; }
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
            break;
        }
        case WSEV_CLOSE: viewers.removeobj(e.id); break;
        default: break; // viewers have nothing to say
    }
}

static void checklisted()
{
    if(totalmillis - lastlistcheck < 1000) return;
    lastlistcheck = totalmillis;
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
            defformatstring(head, "{\"event\":\"%s\",\"server\":", event);
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

// the relay: browsers reach game servers through their link, the master keeps nothing it carries
VAR(0, httprelaycache, 0, 2000, 60000); // how long anonymous GETs are kept
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
    char *key, *type, *etag;
    int status, time;
    vector<char> body;
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

static void relaycachelookup(const char *key, httpresponse &res, bool &found)
{
    found = false;
    loopvrev(relaycache)
    {
        relaycached *e = relaycache[i];
        if(totalmillis - e->time > httprelaycache) { DELETEA(e->key); DELETEA(e->type); DELETEA(e->etag); delete e; relaycache.remove(i); continue; }
        if(found || strcmp(e->key, key)) continue; // the newest answer, once, while the loop still clears out old ones
        res.status = e->status;
        if(e->type) res.headers.set("Content-Type", e->type);
        if(e->etag) res.headers.set("ETag", e->etag);
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
        // written again from the parsed value, so the list only ever carries well formed JSON
        vector<char> buf;
        json::write(buf, status);
        if(buf.length() < STATUS_LIMIT)
        {
            buf.add('\0');
            DELETEA(c.status);
            c.status = newstring(buf.getbuf());
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
                loopvjrev(relaycache) if(!strcmp(relaycache[j]->key, p.cachekey)) { relaycached *old = relaycache.remove(j); DELETEA(old->key); DELETEA(old->type); DELETEA(old->etag); delete old; }
                relaycached *e = new relaycached;
                e->key = p.cachekey;
                p.cachekey = NULL;
                const char *t = res.headers.get("Content-Type"), *etag = res.headers.get("ETag");
                e->type = t ? newstring(t) : NULL;
                e->etag = etag ? newstring(etag) : NULL;
                e->status = res.status;
                e->time = totalmillis;
                e->body.put(res.body.getbuf(), res.body.length());
                relaycache.add(e);
                while(relaycache.length() > 1024) { relaycached *old = relaycache.remove(0); DELETEA(old->key); DELETEA(old->type); DELETEA(old->etag); delete old; }
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
    loopvrev(masterclients) purgemasterclient(i);
    if(mastersocket != ENET_SOCKET_NULL) enet_socket_destroy(mastersocket);
}

void reloadmaster()
{
    clearauth();
}
