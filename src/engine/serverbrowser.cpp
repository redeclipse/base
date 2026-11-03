// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "engine.h"

struct resolverthread
{
    SDL_Thread *thread;
    const char *query;
    int starttime;
};

struct resolverresult
{
    const char *query;
    ENetAddress address;
};

vector<resolverthread> resolverthreads;
vector<const char *> resolverqueries;
vector<resolverresult> resolverresults;
SDL_mutex *resolvermutex;
SDL_cond *querycond, *resultcond;

#define RESOLVERTHREADS 2
#define RESOLVERLIMIT 3500

int resolverloop(void * data)
{
    resolverthread *rt = (resolverthread *)data;
    SDL_LockMutex(resolvermutex);
    SDL_Thread *thread = rt->thread;
    SDL_UnlockMutex(resolvermutex);
    if(!thread || SDL_GetThreadID(thread) != SDL_ThreadID())
        return 0;
    while(thread == rt->thread)
    {
        SDL_LockMutex(resolvermutex);
        while(resolverqueries.empty()) SDL_CondWait(querycond, resolvermutex);
        rt->query = resolverqueries.pop();
        rt->starttime = totalmillis;
        SDL_UnlockMutex(resolvermutex);

        ENetAddress address = { ENET_HOST_ANY, ENET_PORT_ANY };
        enet_address_set_host(&address, rt->query);

        SDL_LockMutex(resolvermutex);
        if(rt->query && thread == rt->thread)
        {
            resolverresult &rr = resolverresults.add();
            rr.query = rt->query;
            rr.address = address;
            rt->query = NULL;
            rt->starttime = 0;
            SDL_CondSignal(resultcond);
        }
        SDL_UnlockMutex(resolvermutex);
    }
    return 0;
}

void resolverinit()
{
    resolvermutex = SDL_CreateMutex();
    querycond = SDL_CreateCond();
    resultcond = SDL_CreateCond();

    SDL_LockMutex(resolvermutex);
    loopi(RESOLVERTHREADS)
    {
        resolverthread &rt = resolverthreads.add();
        rt.query = NULL;
        rt.starttime = 0;
        rt.thread = SDL_CreateThread(resolverloop, "resolver", &rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverstop(resolverthread &rt)
{
    SDL_LockMutex(resolvermutex);
    if(rt.query)
    {
#if SDL_VERSION_ATLEAST(2, 0, 2)
        SDL_DetachThread(rt.thread);
#endif
        rt.thread = SDL_CreateThread(resolverloop, "resolver", &rt);
    }
    rt.query = NULL;
    rt.starttime = 0;
    SDL_UnlockMutex(resolvermutex);
}

void resolverclear()
{
    if(resolverthreads.empty()) return;

    SDL_LockMutex(resolvermutex);
    resolverqueries.shrink(0);
    resolverresults.shrink(0);
    loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        resolverstop(rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverquery(const char *name)
{
    if(resolverthreads.empty()) resolverinit();

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    SDL_UnlockMutex(resolvermutex);
}

bool resolvercheck(const char **name, ENetAddress *address)
{
    bool resolved = false;
    SDL_LockMutex(resolvermutex);
    if(!resolverresults.empty())
    {
        resolverresult &rr = resolverresults.pop();
        *name = rr.query;
        address->host = rr.address.host;
        resolved = true;
    }
    else loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        if(rt.query && totalmillis - rt.starttime > RESOLVERLIMIT)
        {
            resolverstop(rt);
            *name = rt.query;
            resolved = true;
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

bool resolverwait(const char *name, ENetAddress *address)
{
    if(resolverthreads.empty()) resolverinit();

    defformatstring(text, "Resolving %s..", name);
    progress(0, "%s", text);

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    int starttime = getclockticks(), timeout = 0;
    bool resolved = false;
    for(;;)
    {
        SDL_CondWaitTimeout(resultcond, resolvermutex, 250);
        loopv(resolverresults) if(resolverresults[i].query == name)
        {
            address->host = resolverresults[i].address.host;
            resolverresults.remove(i);
            resolved = true;
            break;
        }
        if(resolved) break;

        timeout = getclockticks() - starttime;
        progress(min(float(timeout)/RESOLVERLIMIT, 1.0f), "%s", text);
        if(interceptkey(SDLK_ESCAPE)) timeout = RESOLVERLIMIT + 1;
        if(timeout > RESOLVERLIMIT) break;
    }
    if(!resolved && timeout > RESOLVERLIMIT)
    {
        loopv(resolverthreads)
        {
            resolverthread &rt = resolverthreads[i];
            if(rt.query == name) { resolverstop(rt); break; }
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved && address->host != ENET_HOST_ANY;
}

#define CONNLIMIT 20000

int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &address)
{
    defformatstring(text, "Connecting to %s:[%d]..", hostname != NULL ? hostname : "local server", address.port);
    progress(0, "%s", text);

    ENetSocketSet readset, writeset;
    if(!enet_socket_connect(sock, &address)) for(int starttime = getclockticks(), timeout = 0; timeout <= CONNLIMIT;)
    {
        ENET_SOCKETSET_EMPTY(readset);
        ENET_SOCKETSET_EMPTY(writeset);
        ENET_SOCKETSET_ADD(readset, sock);
        ENET_SOCKETSET_ADD(writeset, sock);
        int result = enet_socketset_select(sock, &readset, &writeset, 250);
        if(result < 0) break;
        else if(result > 0)
        {
            if(ENET_SOCKETSET_CHECK(readset, sock) || ENET_SOCKETSET_CHECK(writeset, sock))
            {
                int error = 0;
                if(enet_socket_get_option(sock, ENET_SOCKOPT_ERROR, &error) < 0 || error) break;
                return 0;
            }
        }
        timeout = getclockticks() - starttime;
        progress(min(float(timeout)/CONNLIMIT, 1.0f), "%s", text);
        if(interceptkey(SDLK_ESCAPE)) break;
    }

    return -1;
}

vector<serverinfo *> servers;
bool sortedservers = true;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

// the Server list, fetched in the background: https, then plain http, then the text protocol, keeping whichever works for the session
enum { LIST_HTTPS = 0, LIST_HTTP, LIST_LEGACY, LIST_TIERS };
enum { LEGACY_IDLE = 0, LEGACY_RESOLVE, LEGACY_CONNECT, LEGACY_SEND, LEGACY_RECV };
#define LEGACYLIMIT 20000
#define LISTPENDING -2

VAR(IDF_READONLY, serverlistfetching, 0, 0, 1);

VAR(IDF_PERSIST, serverlistinterval, 0, 60, VAR_MAX); // seconds between fetches while the browser is open without the live list, 0 only on request
#define LISTBROWSING 2000 // the browser calls updateservers every frame, a gap this long means it closed

struct serverlistfetch
{
    int tier, fetch, lastfetch, legacy, legacystart, legacysent, live, lastlive, lastbrowse;
    bool requested;
    string master, etag, query;
    ENetSocket sock;
    ENetAddress address;
    vector<char> data;

    serverlistfetch() : tier(LIST_HTTPS), fetch(-1), lastfetch(0), legacy(LEGACY_IDLE), legacystart(0), legacysent(0), live(-1), lastlive(0), lastbrowse(0), requested(false), sock(ENET_SOCKET_NULL)
    {
        master[0] = etag[0] = query[0] = '\0';
    }
};
static serverlistfetch serverlist;

static serverinfo *newserver(const char *name, int port = SERVER_PORT, int priority = 0, const char *desc = NULL, const char *handle = NULL, const char *flags = NULL, const char *branch = NULL, uint ip = ENET_HOST_ANY, int source = serverinfo::MANUAL)
{
    serverinfo *si = new serverinfo(ip, port, priority, source);

    if(name) copystring(si->name, name);
    else if(ip == ENET_HOST_ANY || enet_address_get_host_ip(&si->address, si->name, sizeof(si->name)) < 0)
    {
        delete si;
        return NULL;
    }
    if(desc && *desc) copystring(si->sdesc, desc, MAXSDESCLEN+1);
    if(handle && *handle) copystring(si->authhandle, handle);
    if(flags && *flags) copystring(si->flags, flags);
    if(branch && *branch) copystring(si->branch, branch, MAXBRANCHLEN+1);

    servers.add(si);
    sortedservers = false;

    return si;
}

serverinfo *addserver(const char *name, int port, int priority, const char *desc, const char *handle, const char *flags, const char *branch, int source)
{
    loopv(servers) if(!strcmp(servers[i]->name, name) && servers[i]->port == port)
    {
        serverinfo *si = servers[i];
        if(source == serverinfo::MASTER)
        {
            // the master's word on these is newer than a cached or saved one, a server added by hand stays so
            si->listed = true;
            si->purge = false;
            si->priority = priority;
            copystring(si->sdesc, desc ? desc : "", MAXSDESCLEN+1);
            copystring(si->authhandle, handle ? handle : "");
            copystring(si->flags, flags ? flags : "");
            copystring(si->branch, branch ? branch : "", MAXBRANCHLEN+1);
            sortedservers = false;
        }
        return si;
    }
    serverinfo *si = newserver(name, port, priority, desc, handle, flags, branch, ENET_HOST_ANY, source);
    if(si && verbose >= 2) conoutf(colourwhite, "Added server %s (%d) [%s]", name, port, desc);
    return si;
}

// servers.cfg keeps the last Server list for when the master can't be reached, files before version 2 saved everything without saying where it came from
VAR(0, serverscfgversion, 0, 0, 2);
static bool loadingservercfg = false;

static void addservercmd(char *n, int *p, int *r, char *d, char *h, char *f, char *b, char *src)
{
    int source = serverinfo::MANUAL;
    if(!strcmp(src, "master") || (!*src && loadingservercfg && serverscfgversion < 2)) source = serverinfo::MASTER;
    else if(!strcmp(src, "lan")) source = serverinfo::LAN;
    addserver(n, *p > 0 ? *p : SERVER_PORT, *r >= 0 ? *r : 0, d, h, f, b, source);
}
ICOMMAND(0, addserver, "siisssss", (char *n, int *p, int *r, char *d, char *h, char *f, char *b, char *src), addservercmd(n, p, r, d, h, f, b, src));

void loadservercfg()
{
    serverscfgversion = 0;
    loadingservercfg = true;
    execfile("servers.cfg", false);
    loadingservercfg = false;
}

// a fetched Server list replaces the master's servers, ones it no longer lists go once nothing else holds their name
static void beginmasterlist()
{
    loopv(servers) if(servers[i]->source == serverinfo::MASTER) servers[i]->listed = false;
}

static void purgeservers()
{
    loopvrev(servers) if(servers[i]->purge && servers[i]->resolved != serverinfo::RESOLVING)
    {
        delete servers.remove(i);
        sortedservers = false;
    }
}

static void endmasterlist()
{
    loopv(servers) if(servers[i]->source == serverinfo::MASTER && !servers[i]->listed) servers[i]->purge = true;
    purgeservers();
}

VAR(IDF_PERSIST, searchlan, 0, 0, 1);
VAR(IDF_PERSIST, maxservpings, 0, 10, 1000);
VAR(IDF_PERSIST, serverupdateinterval, 0, 10, VAR_MAX);
VAR(IDF_PERSIST, serverdecay, 0, 20, VAR_MAX);
VAR(0, serverwaiting, 1, serverinfo::WAITING, 0);

void pingservers()
{
    if(pingsock == ENET_SOCKET_NULL)
    {
        pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pingsock == ENET_SOCKET_NULL)
        {
            lastinfo = totalmillis;
            return;
        }
        enet_socket_set_option(pingsock, ENET_SOCKOPT_NONBLOCK, 1);
        enet_socket_set_option(pingsock, ENET_SOCKOPT_BROADCAST, 1);
    }
    ENetBuffer buf;
    uchar ping[MAXTRANS], pingonly[16];
    ucharbuf p(ping, sizeof(ping)), q(pingonly, sizeof(pingonly));
    putint(p, totalmillis ? totalmillis : 1);
    putint(q, totalmillis ? totalmillis : 1);
    putint(q, -1);

    static int lastping = 0;
    if(lastping >= servers.length()) lastping = 0;
    loopi(maxservpings ? min(servers.length(), maxservpings) : servers.length())
    {
        serverinfo &si = *servers[lastping];
        if(++lastping >= servers.length()) lastping = 0;
        if(si.address.host == ENET_HOST_ANY) continue;
        buf.data = si.hasstatus ? pingonly : ping;
        buf.dataLength = si.hasstatus ? q.length() : p.length();
        enet_socket_send(pingsock, &si.address, &buf, 1);

        si.checkdecay(serverdecay*1000);
    }

    if(searchlan && serverlanport)
    {
        ENetAddress address;
        address.host = ENET_HOST_BROADCAST;
        address.port = serverlanport;
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &address, &buf, 1);
    }
    lastinfo = totalmillis;
}

static void legacyresolved(const ENetAddress &addr);

void checkresolver()
{
    purgeservers();
    int resolving = 0;
    loopv(servers)
    {
        serverinfo &si = *servers[i];
        if(si.resolved == serverinfo::RESOLVED) continue;
        if(si.address.host == ENET_HOST_ANY)
        {
            if(si.resolved == serverinfo::UNRESOLVED) { si.resolved = serverinfo::RESOLVING; resolverquery(si.name); }
            resolving++;
        }
    }
    if(!resolving && serverlist.legacy != LEGACY_RESOLVE) return;

    const char *name = NULL;
    for(;;)
    {
        ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
        if(!resolvercheck(&name, &addr)) break;
        if(name == serverlist.query)
        {
            if(serverlist.legacy == LEGACY_RESOLVE) legacyresolved(addr);
            continue;
        }
        loopv(servers)
        {
            serverinfo &si = *servers[i];
            if(name == si.name)
            {
                si.resolved = serverinfo::RESOLVED;
                si.address.host = addr.host;
                break;
            }
        }
    }
}

static int lastreset = 0;

void checkpings()
{
    if(pingsock==ENET_SOCKET_NULL) return;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    ENetBuffer buf;
    ENetAddress addr;
    uchar ping[MAXTRANS];
    char text[MAXTRANS];
    buf.data = ping;
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(pingsock, &addr, &buf, 1);
        if(len <= 0) return;
        serverinfo *si = NULL;
        loopv(servers) if(addr.host == servers[i]->address.host && addr.port == servers[i]->address.port) { si = servers[i]; break; }
        if(!si && searchlan) si = newserver(NULL, addr.port-1, 1, NULL, NULL, NULL, NULL, addr.host, serverinfo::LAN);
        if(!si) continue;
        ucharbuf p(ping, len);
        int millis = getint(p), rtt = clamp(totalmillis - millis, 0, min(serverdecay*1000, totalmillis));
        if(millis >= lastreset && rtt < serverdecay*1000) si->addping(rtt, millis);
        si->lastinfo = totalmillis;
        // a player count is never negative, so -1 is the echo of a ping only query, which older servers answer in full anyway
        int numplayers = getint(p);
        if(numplayers == -1)
        {
            if(!p.remaining()) continue;
            numplayers = getint(p);
        }
        si->numplayers = numplayers;
        int numattr = getint(p);
        si->attr.shrink(0);
        loopj(numattr) si->attr.add(getint(p));
        int gver = si->attr.empty() ? 0 : si->attr[0];
        getstring(text, p);
        filterstring(si->map, text, false);
        getstring(text, p);
        filterstring(si->sdesc, text, true, true, true, false, MAXSDESCLEN+1);
        si->players.deletearrays();
        si->handles.deletearrays();
        if(gver >= 227)
        {
            getstring(text, p);
            filterstring(si->branch, text, true, true, true, false, MAXBRANCHLEN+1);
        }
        loopi(si->numplayers)
        {
            if(p.overread()) break;
            getstring(text, p);
            si->players.add(newstring(text));
        }
        if(gver >= 225) loopi(si->numplayers)
        {
            if(p.overread()) break;
            getstring(text, p);
            si->handles.add(newstring(text));
        }
        sortedservers = false;
    }
}

static inline int serverinfocompare(serverinfo *a, serverinfo *b) { return client::servercompare(a, b) < 0; }

void refreshservers()
{
    static int lastrefresh = 0;
    if(lastrefresh == totalmillis) return;
    if(totalmillis - lastrefresh > 1000)
    {
        loopv(servers) servers[i]->reset();
        sortedservers = false;
        lastreset = totalmillis;
    }
    lastrefresh = totalmillis;

    checkresolver();
    checkpings();
    int now = int(time(NULL));
    loopv(servers) if(servers[i]->hasstatus && servers[i]->attr.length() > 14)
    {
        serverinfo &si = *servers[i];
        si.attr[3] = si.statusends >= 0 ? max(si.statusends - now, 0) : 0;
        si.attr[14] = max(si.stateends - now, 0)*1000;
    }
    if(totalmillis - lastinfo >= (serverupdateinterval*1000)/(maxservpings ? max(1, (servers.length() + maxservpings - 1) / maxservpings) : 1)) pingservers();
}

static void cleardetail();

void clearservers()
{
    cleardetail();
    resolverclear();
    servers.deletecontents();
    lastinfo = 0;
    serverlist.etag[0] = '\0';
}

COMMAND(0, clearservers, "");

void sortservers()
{
    if(!sortedservers)
    {
        servers.sort(serverinfocompare);
        sortedservers = true;
    }
}
COMMAND(0, sortservers, "");

VAR(IDF_PERSIST, autosortservers, 0, 1, 1);
VAR(0, pausesortservers, 0, 0, 1);

// the text protocol's reply is script, read for the commands a list needs rather than run, as it comes in plain text
static void parselegacylist(char *data)
{
    vector<char *> args;
    for(char *line = data; line && *line;)
    {
        char *next = strchr(line, '\n');
        if(next) *next++ = '\0';
        args.deletearrays();
        explodelist(line, args, 8);
        if(args.length())
        {
            if(!strcmp(args[0], "setversion") && args.length() >= 3)
            {
                defformatstring(cmd, "setversion %d %d", parseint(args[1]), parseint(args[2]));
                execute(cmd);
            }
            else if(!strcmp(args[0], "clearservers")) beginmasterlist();
            else if(!strcmp(args[0], "addserver") && args.length() >= 2)
            {
                int port = args.length() > 2 ? parseint(args[2]) : 0, priority = args.length() > 3 ? parseint(args[3]) : 0;
                serverinfo *si = addserver(args[1], port > 0 ? port : SERVER_PORT, max(priority, 0), args.length() > 4 ? args[4] : NULL, args.length() > 5 ? args[5] : NULL,
                    args.length() > 6 ? args[6] : NULL, args.length() > 7 ? args[7] : NULL, serverinfo::MASTER);
                if(si && si->hasstatus) { si->hasstatus = false; si->cleanup(); } // this list has no status, queries fill it in

            }
        }
        line = next;
    }
    args.deletearrays();
}


static void startlist();

static void listdone(bool ok, const char *how)
{
    serverlistfetching = 0;
    if(ok)
    {
        if(verbose) conoutf(colourgrey, "Retrieved %d server(s) from master (%s)", servers.length(), how);
        else conoutf(colourgrey, "Retrieved list from master successfully");
        return;
    }
    serverlist.tier = LIST_HTTPS; // nothing worked, so the next try starts again from the top
    conoutf(colourred, "Master server not replying");
}

static void cubestring(char *dst, const char *s, size_t len = MAXSTRLEN)
{
    vector<char> buf;
    if(s) json::tocube(buf, s);
    buf.add('\0');
    copystring(dst, buf.getbuf(), len);
}

// what a linked server reported to the master, laid out as its query reply would be
static void applystatus(serverinfo &si, jsonvalue *st)
{
    if(!st || st->type != JSON_OBJECT)
    {
        if(si.hasstatus) si.cleanup(); // left the link, queries fill it in again
        si.hasstatus = false;
        return;
    }
    si.hasstatus = true;
    si.statusends = int(st->getnum("ends", -1));
    si.stateends = int(st->getnum("stateends"));
    int now = int(time(NULL));
    jsonvalue *build = st->get("build");
    si.attr.setsize(0);
    si.attr.add(int(st->getnum("gameversion")));
    si.attr.add(int(st->getnum("modeid")));
    si.attr.add(int(st->getnum("mutators")));
    si.attr.add(si.statusends >= 0 ? max(si.statusends - now, 0) : 0);
    si.attr.add(int(st->getnum("maxplayers")));
    si.attr.add(int(st->getnum("mastermode")));
    si.attr.add(int(st->getnum("gamevars")));
    si.attr.add(int(st->getnum("gamemods")));
    loopi(5) si.attr.add(build && build->at(i) ? int(build->at(i)->num) : 0);
    si.attr.add(int(st->getnum("state")));
    si.attr.add(max(si.stateends - now, 0)*1000);
    string text;
    cubestring(text, st->getstr("map"));
    filterstring(si.map, text, false);
    si.players.deletearrays();
    si.handles.deletearrays();
    jsonvalue *players = st->get("players");
    if(players) loopv(players->items)
    {
        jsonvalue *pl = players->items[i];
        if(pl->type != JSON_OBJECT) continue;
        cubestring(text, pl->getstr("name"));
        si.players.add(newstring(text));
        cubestring(text, pl->getstr("handle"));
        si.handles.add(newstring(text));
    }
    si.numplayers = si.players.length();
    si.lastinfo = totalmillis;
    sortedservers = false;
}

static serverinfo *listentry(jsonvalue &e)
{
    const char *host = e.getstr("host");
    if(!host || !*host) return NULL;
    string name, desc, handle, flags, branch;
    cubestring(name, host);
    cubestring(desc, e.getstr("description"));
    cubestring(handle, e.getstr("handle"));
    cubestring(flags, e.getstr("flags"));
    cubestring(branch, e.getstr("branch"));
    int port = int(e.getnum("port", SERVER_PORT));
    serverinfo *si = addserver(name, port > 0 && port < 65536 ? port : SERVER_PORT, max(int(e.getnum("priority")), 0), desc, handle, flags, branch, serverinfo::MASTER);
    if(si)
    {
        // where its own Web API is, for the details of who is playing
        int webport = int(e.getnum("webport"));
        si->webport = webport > 0 && webport < 65536 ? webport : 0;
        si->relay = clamp(int(e.getnum("relay")), 0, 2);
        const char *fp = e.getstr("fingerprint", "");
        copystring(si->fingerprint, fp);
        for(char *f = si->fingerprint; *f; f++) if(!isxdigit(*f)) { *f = '\0'; break; }
        applystatus(*si, e.get("status"));
    }
    return si;
}

static void applylist(jsonvalue &v, bool secure)
{
    jsonvalue *ver = v.get("version");
    if(ver && ver->type == JSON_OBJECT)
    {
        defformatstring(cmd, "setversion %d %d", int(ver->getnum("main")), int(ver->getnum("game")));
        execute(cmd);
    }
    // the master's script only runs when the list came over TLS that checked out
    const char *script = v.getstr("script");
    if(secure && script && *script)
    {
        vector<char> buf;
        json::tocube(buf, script);
        buf.add('\0');
        execute(buf.getbuf());
    }
    jsonvalue *list = v.get("servers");
    beginmasterlist();
    if(list) loopv(list->items) if(list->items[i]->type == JSON_OBJECT) listentry(*list->items[i]);
    endmasterlist();
}

static void listfetched(httpfetch &f)
{
    if(f.id != serverlist.fetch && serverlist.fetch != LISTPENDING) return;
    serverlist.fetch = -1;
    const char *how = serverlist.tier == LIST_HTTPS ? "https" : "http";
    if(f.status == 304) { listdone(true, how); return; }
    if(f.status == 200)
    {
        jsonvalue *v = json::parse(f.body.getbuf(), f.body.length());
        if(v && v->type == JSON_OBJECT && v->get("servers"))
        {
            applylist(*v, serverlist.tier == LIST_HTTPS);
            const char *etag = f.headers.get("ETag");
            copystring(serverlist.etag, etag ? etag : "");
            delete v;
            listdone(true, how);
            return;
        }
        DELETEP(v);
    }
    // falling back is safe as only a list over checked TLS runs script, but a certificate that fails is always worth seeing
    if(f.error && !strncmp(f.error, "certificate for", 15)) conoutf(colourred, "Server list from %s: %s", servermaster, f.error);
    else if(verbose) conoutf(colourgrey, "Server list from %s failed: %s", f.url, f.error ? f.error : http::statusname(f.status));
    serverlist.tier++;
    startlist();
}

static void stoplegacy()
{
    if(serverlist.sock != ENET_SOCKET_NULL) enet_socket_destroy(serverlist.sock);
    serverlist.sock = ENET_SOCKET_NULL;
    if(serverlist.legacy == LEGACY_RESOLVE) resolverclear();
    serverlist.legacy = LEGACY_IDLE;
    serverlist.data.setsize(0);
}

static void legacyfailed(const char *why)
{
    if(verbose) conoutf(colourgrey, "Server list from %s:[%d] failed: %s", servermaster, servermasterport, why);
    stoplegacy();
    listdone(false, "text protocol");
}

static void startlegacy()
{
    stoplegacy();
    copystring(serverlist.query, servermaster);
    serverlist.address.host = ENET_HOST_ANY;
    serverlist.address.port = servermasterport;
    serverlist.legacy = LEGACY_RESOLVE;
    serverlist.legacystart = totalmillis;
    resolverquery(serverlist.query);
}

static void legacyresolved(const ENetAddress &addr)
{
    if(addr.host == ENET_HOST_ANY) { legacyfailed("unable to resolve"); return; }
    serverlist.address.host = addr.host;
    serverlist.sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(serverlist.sock == ENET_SOCKET_NULL) { legacyfailed("unable to open a socket"); return; }
    enet_socket_set_option(serverlist.sock, ENET_SOCKOPT_NONBLOCK, 1);
    if(enet_socket_connect(serverlist.sock, &serverlist.address) < 0) { legacyfailed("unable to connect"); return; }
    serverlist.legacy = LEGACY_CONNECT;
}

static void checklegacy()
{
    if(totalmillis - serverlist.legacystart > LEGACYLIMIT) { legacyfailed("timed out"); return; }
    enet_uint32 events = serverlist.legacy == LEGACY_RECV ? ENET_SOCKET_WAIT_RECEIVE : ENET_SOCKET_WAIT_SEND;
    if(enet_socket_wait(serverlist.sock, &events, 0) < 0) { legacyfailed("connection lost"); return; }
    if(!events) return;
    switch(serverlist.legacy)
    {
        case LEGACY_CONNECT:
        {
            int error = 0;
            if(enet_socket_get_option(serverlist.sock, ENET_SOCKOPT_ERROR, &error) < 0 || error) { legacyfailed("unable to connect"); return; }
            serverlist.legacy = LEGACY_SEND;
            serverlist.legacysent = 0;
            break;
        }
        case LEGACY_SEND:
        {
            static const char req[] = "update\n";
            ENetBuffer buf;
            buf.data = (void *)&req[serverlist.legacysent];
            buf.dataLength = sizeof(req) - 1 - serverlist.legacysent;
            int sent = enet_socket_send(serverlist.sock, NULL, &buf, 1);
            if(sent < 0) { legacyfailed("connection lost while sending"); return; }
            serverlist.legacysent += sent;
            if(serverlist.legacysent >= int(sizeof(req)) - 1) serverlist.legacy = LEGACY_RECV;
            break;
        }
        case LEGACY_RECV:
        {
            // readable with nothing to read is the master closing, once it has sent the lot
            for(;;)
            {
                if(serverlist.data.length() >= serverlist.data.capacity()) serverlist.data.reserve(4096);
                ENetBuffer buf;
                buf.data = serverlist.data.getbuf() + serverlist.data.length();
                buf.dataLength = serverlist.data.capacity() - serverlist.data.length();
                int recv = enet_socket_receive(serverlist.sock, NULL, &buf, 1);
                if(recv <= 0) break;
                serverlist.data.advance(recv);
                events = ENET_SOCKET_WAIT_RECEIVE;
                if(enet_socket_wait(serverlist.sock, &events, 0) < 0) break;
                if(!events) return;
            }
            serverlist.data.add('\0');
            bool ok = serverlist.data[0] != '\0';
            if(ok)
            {
                beginmasterlist();
                parselegacylist(serverlist.data.getbuf());
                endmasterlist();
            }
            stoplegacy();
            listdone(ok, "text protocol");
            break;
        }
    }
}

static void closelive()
{
    if(serverlist.live >= 0) http::wsclose(serverlist.live);
    serverlist.live = -1;
}

static void startlist()
{
    if(!servermaster[0]) { serverlistfetching = 0; return; }
    if(strcmp(serverlist.master, servermaster))
    {
        copystring(serverlist.master, servermaster);
        serverlist.tier = LIST_HTTPS;
        serverlist.etag[0] = '\0';
        closelive();
    }
    if(serverlist.tier == LIST_HTTPS && !servermasterlinktls) serverlist.tier = LIST_HTTP;
    serverlistfetching = 1;
    serverlist.lastfetch = totalmillis ? totalmillis : 1;
    if(serverlist.tier >= LIST_LEGACY) { startlegacy(); return; }
    defformatstring(url, "%s://%s:%d/servers", serverlist.tier == LIST_HTTPS ? "https" : "http", servermaster, servermasterlinkport);
    httpfetch *f = http::newfetch(HTTP_GET, url, listfetched);
    if(serverlist.etag[0]) f->reqheaders.add("If-None-Match", serverlist.etag);
    // a fetch that fails straight away calls back before submit returns, and may have started the next tier
    serverlist.fetch = LISTPENDING;
    int id = http::submit(f);
    if(serverlist.fetch == LISTPENDING) serverlist.fetch = id;
}

static void cancellist()
{
    if(serverlist.fetch >= 0) http::cancel(serverlist.fetch);
    serverlist.fetch = -1;
    stoplegacy();
    serverlistfetching = 0;
}

// the live list sends a snapshot, then each server's changes as they happen, and nothing it sends is ever run
static void liveevent(httpwsevent &e, void *ctx)
{
    if(e.id != serverlist.live) return;
    switch(e.type)
    {
        case WSEV_MESSAGE:
        {
            jsonvalue *v = json::parse(e.data, e.len);
            if(!v) break;
            const char *event = v->getstr("event", "");
            if(!strcmp(event, "snapshot"))
            {
                jsonvalue *list = v->get("servers");
                beginmasterlist();
                if(list) loopv(list->items) if(list->items[i]->type == JSON_OBJECT) listentry(*list->items[i]);
                endmasterlist();
            }
            else if(!strcmp(event, "add") || !strcmp(event, "update"))
            {
                jsonvalue *server = v->get("server");
                if(server && server->type == JSON_OBJECT) listentry(*server);
            }
            else if(!strcmp(event, "remove"))
            {
                const char *key = v->getstr("key", "");
                loopv(servers) if(servers[i]->source == serverinfo::MASTER)
                {
                    defformatstring(name, "%s:%d", servers[i]->name, servers[i]->port);
                    if(!strcmp(name, key)) { servers[i]->purge = true; break; }
                }
                purgeservers();
            }
            delete v;
            break;
        }
        case WSEV_CLOSE: serverlist.live = -1; break;
        default: break;
    }
}

static void checklive()
{
    bool browsing = serverlist.lastbrowse && totalmillis - serverlist.lastbrowse < LISTBROWSING;
    if(!browsing) { closelive(); return; }
    if(serverlist.live >= 0 || serverlistfetching || serverlist.tier >= LIST_LEGACY || !serverlist.lastfetch) return;
    int interval = serverlistinterval*1000;
    if(serverlist.lastlive && totalmillis - serverlist.lastlive < max(interval, 10000)) return;
    serverlist.lastlive = totalmillis ? totalmillis : 1;
    defformatstring(url, "%s://%s:%d/servers/live", serverlist.tier == LIST_HTTPS ? "wss" : "ws", servermaster, servermasterlinkport);
    serverlist.live = http::wsopen(url, liveevent);
}

void checkserverlist()
{
    if(serverlist.legacy == LEGACY_RESOLVE) checkresolver();
    else if(serverlist.legacy != LEGACY_IDLE) checklegacy();
    checklive();
}

void updatefrommaster()
{
    pausesortservers = 0;
    serverlist.requested = true;
    cancellist();
    serverlist.tier = LIST_HTTPS;
    startlist();
}
COMMAND(IDF_NOECHO, updatefrommaster, "");

void updateservers()
{
    serverlist.lastbrowse = totalmillis ? totalmillis : 1;
    if(!serverlist.requested)
    {
        serverlist.requested = true;
        startlist();
    }
    else if(serverlist.live < 0 && !serverlistfetching && serverlistinterval && totalmillis - serverlist.lastfetch >= serverlistinterval*1000) startlist();
    refreshservers();
    if(autosortservers && !pausesortservers) sortservers();
    intret(servers.length());
}
COMMAND(IDF_NOECHO, updateservers, "");

// details of one server's players and teams from its own Web API, directly or through the master's Relay, for the browser's details panel
enum { DETAIL_UNAVAILABLE = -1, DETAIL_NONE = 0, DETAIL_FETCHING, DETAIL_READY };
enum { DETAIL_DIRECT = 0, DETAIL_RELAY, DETAIL_ROUTES };
#define DETAILREFRESH 5000

struct serverdetail
{
    string name;
    int port, state, route, fetch, part, last;
    jsonvalue *parts[2]; // players, then teams

    serverdetail() : port(0), state(DETAIL_NONE), route(DETAIL_DIRECT), fetch(-1), part(0), last(0) { name[0] = '\0'; parts[0] = parts[1] = NULL; }
};
static serverdetail detail;
static vector<jsonvalue *> detailnext; // a refresh is kept apart until both parts are in, so the panel never shows half of each
static const char *detailpaths[2] = { "players", "teams" };

static serverinfo *detailserver()
{
    loopv(servers) if(servers[i]->port == detail.port && !strcmp(servers[i]->name, detail.name)) return servers[i];
    return NULL;
}

static void cleardetail()
{
    if(detail.fetch >= 0) http::cancel(detail.fetch);
    detail.fetch = -1;
    loopi(2) DELETEP(detail.parts[i]);
    detailnext.deletecontents();
    detail.name[0] = '\0';
    detail.port = 0;
    detail.state = DETAIL_NONE;
}

static void detailfetched(httpfetch &f);

static bool fetchdetail()
{
    serverinfo *si = detailserver();
    if(!si) return false;
    while(detail.route < DETAIL_ROUTES)
    {
        string url;
        const char *pin = NULL;
        if(detail.route == DETAIL_DIRECT && si->webport)
        {
            formatstring(url, "%s://%s:%d/%s", si->fingerprint[0] ? "https" : "http", si->name, si->webport, detailpaths[detail.part]);
            if(si->fingerprint[0]) pin = si->fingerprint;
        }
        else if(detail.route == DETAIL_RELAY && si->relay >= 1 && servermaster[0])
            formatstring(url, "%s://%s:%d/servers/%s:%d/%s", servermasterlinktls ? "https" : "http", servermaster, servermasterlinkport, si->name, si->port, detailpaths[detail.part]);
        else { detail.route++; continue; }
        httpfetch *f = http::newfetch(HTTP_GET, url, detailfetched);
        if(pin) f->pin = newstring(pin);
        detail.fetch = LISTPENDING;
        int id = http::submit(f);
        if(detail.fetch == LISTPENDING) detail.fetch = id;
        return true;
    }
    return false;
}

static void detailfetched(httpfetch &f)
{
    if(f.id != detail.fetch && detail.fetch != LISTPENDING) return;
    detail.fetch = -1;
    jsonvalue *v = f.status == 200 ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
    if(!v || v->type != JSON_ARRAY)
    {
        DELETEP(v);
        detailnext.deletecontents();
        detail.part = 0;
        detail.route++; // the server may be behind a firewall, so try the relay
        if(!fetchdetail())
        {
            detail.route = DETAIL_DIRECT;
            if(detail.state != DETAIL_READY) detail.state = DETAIL_UNAVAILABLE;
            detail.last = totalmillis ? totalmillis : 1;
        }
        return;
    }
    detailnext.add(v);
    if(++detail.part < 2) { fetchdetail(); return; }
    loopi(2) { DELETEP(detail.parts[i]); detail.parts[i] = detailnext[i]; }
    detailnext.setsize(0);
    detail.part = 0;
    detail.state = DETAIL_READY;
    detail.last = totalmillis ? totalmillis : 1;
}

// asked every frame while the panel shows a server, so it fetches the first time and every few seconds after
static void serverdetails(int *n)
{
    if(!servers.inrange(*n)) { intret(DETAIL_UNAVAILABLE); return; }
    serverinfo *si = servers[*n];
    if(detail.port != si->port || strcmp(detail.name, si->name))
    {
        cleardetail();
        copystring(detail.name, si->name);
        detail.port = si->port;
        detail.state = DETAIL_FETCHING;
        detail.route = DETAIL_DIRECT;
        detail.part = 0;
        if(!fetchdetail()) detail.state = DETAIL_UNAVAILABLE;
    }
    else if(detail.fetch < 0 && totalmillis - detail.last >= DETAILREFRESH)
    {
        detail.part = 0;
        if(detail.state == DETAIL_UNAVAILABLE) detail.state = DETAIL_FETCHING;
        if(!fetchdetail()) detail.state = DETAIL_UNAVAILABLE;
    }
    intret(detail.state);
}
COMMAND(0, serverdetails, "i");
ICOMMAND(0, clearserverdetails, "", (), cleardetail());

// getserverdetail players|teams [index] [field]: the count without an index, an entry's field otherwise
static void getserverdetail(char *list, int *idx, char *field, int *numargs)
{
    int which = !strcmp(list, "teams") ? 1 : (!strcmp(list, "players") ? 0 : -1);
    jsonvalue *v = which >= 0 && detail.state == DETAIL_READY ? detail.parts[which] : NULL;
    if(*numargs < 2 || *idx < 0) { intret(v ? v->length() : 0); return; }
    jsonvalue *e = v ? v->at(*idx) : NULL;
    jsonvalue *f = e && e->type == JSON_OBJECT ? e->get(field) : NULL;
    if(!f) { result(""); return; }
    switch(f->type)
    {
        case JSON_STRING:
        {
            vector<char> buf;
            json::tocube(buf, f->str);
            buf.add('\0');
            result(buf.getbuf());
            break;
        }
        case JSON_NUMBER: intret(int(f->num)); break;
        case JSON_TRUE: intret(1); break;
        default: intret(0); break;
    }
}
COMMAND(0, getserverdetail, "sisN");

void writeservercfg()
{
    if(servers.empty()) return;
    stream *f = openutf8file("servers.cfg", "w");
    if(!f) return;
    f->printf("// servers which are connected to or queried get added here automatically, master ones are kept for when it can't be reached\n\n");
    f->printf("serverscfgversion 2\n");
    loopv(servers)
    {
        serverinfo *s = servers[i];
        if(s->source == serverinfo::LAN || s->purge) continue;
        f->printf("addserver %s %d %d %s %s %s %s %s\n", escapestring(s->name), s->port, s->priority, escapestring(s->sdesc[0] ? s->sdesc : s->name), escapestring(s->authhandle), escapestring(s->flags), escapestring(s->branch),
            s->source == serverinfo::MASTER ? "master" : "manual");
    }
    delete f;
}
