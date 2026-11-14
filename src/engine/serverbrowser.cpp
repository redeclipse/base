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
        // a copy, as a lookup that times out keeps running after whoever asked has let go of the name
        string name;
        copystring(name, rt->query);
        SDL_UnlockMutex(resolvermutex);

        ENetAddress address = { ENET_HOST_ANY, ENET_PORT_ANY };
        enet_address_set_host(&address, name);

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
            *name = rt.query; // before stopping clears it, so a lookup that took too long fails rather than waiting forever
            resolverstop(rt);
            resolved = true;
            break;
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
        resolverqueries.removeobj(name); // one not started yet would otherwise be read after the caller has let go of it
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
#define LEGACYMAXSIZE HTTP_INMEMORY // the text protocol's reply, capped like a list over http
#define LISTPENDING -2

VAR(IDF_READONLY, serverlistfetching, 0, 0, 1);

VAR(IDF_PERSIST, serverlistinterval, 0, 60, VAR_MAX); // seconds between fetches while the browser is open without the live list, 0 only on request
#define LISTBROWSING 2000 // the browser calls updateservers every frame, a gap this long means it closed

struct serverlistfetch
{
    int tier, fetch, lastfetch, legacy, legacystart, legacysent, live, lastlive, lastbrowse;
    bool requested, livesecure; // livesecure: the live list is over wss://
    string master, etag;
    char *query; // the text protocol's lookup of the master, its own each time so a late answer to one that was dropped is never taken for it
    ENetSocket sock;
    ENetAddress address;
    vector<char> data;

    serverlistfetch() : tier(LIST_HTTPS), fetch(-1), lastfetch(0), legacy(LEGACY_IDLE), legacystart(0), legacysent(0), live(-1), lastlive(0), lastbrowse(0), requested(false), livesecure(false), query(NULL), sock(ENET_SOCKET_NULL)
    {
        master[0] = etag[0] = '\0';
    }
};
static serverlistfetch serverlist;
static vector<char *> droppedqueries; // lookups left to the resolver, freed when they answer

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
            si->inlist = true;
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
    if(si && source == serverinfo::MASTER) si->inlist = true;
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
    loopv(servers)
    {
        if(servers[i]->source == serverinfo::MASTER) servers[i]->listed = false;
        servers[i]->inlist = false;
    }
}

static void purgeservers()
{
    loopvrev(servers) if(servers[i]->purge && servers[i]->resolved != serverinfo::RESOLVING)
    {
        delete servers.remove(i);
        sortedservers = false;
    }
}

#define PRESENCELISTAGE 600000 // a Server list older than this can't say a server is still listed
#define PRESENCELISTREFRESH 300000 // how old a list gets while in a match before fetching another
#define PRESENCELISTRETRY 60000
static int listapplied = 0; // when a fetched list last replaced the master's servers, as the ones kept in servers.cfg may be gone

static void endmasterlist()
{
    loopv(servers) if(servers[i]->source == serverinfo::MASTER && !servers[i]->listed) servers[i]->purge = true;
    purgeservers();
    listapplied = totalmillis ? totalmillis : 1;
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
    if(!resolving && serverlist.legacy != LEGACY_RESOLVE && droppedqueries.empty()) return;

    const char *name = NULL;
    for(;;)
    {
        ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
        if(!resolvercheck(&name, &addr)) break;
        if(name && name == serverlist.query)
        {
            if(serverlist.legacy == LEGACY_RESOLVE) legacyresolved(addr);
            continue;
        }
        int dropped = droppedqueries.find((char *)name);
        if(dropped >= 0) { delete[] droppedqueries.remove(dropped); continue; }
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
        loopj(numattr) // the count is the sender's word, so it stops where the packet does
        {
            int attr = getint(p);
            if(p.overread()) break;
            si->attr.add(attr);
        }
        int gver = si->attr.empty() ? 0 : si->attr[0];
        getstring(text, p);
        filterstring(si->map, text, false);
        getstring(text, p);
        filterstring(si->sdesc, text, true, true, true, false, false, MAXSDESCLEN+1);
        si->players.deletearrays();
        si->handles.deletearrays();
        if(gver >= 227)
        {
            getstring(text, p);
            filterstring(si->branch, text, true, true, true, false, false, MAXBRANCHLEN+1);
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
        si.attr[3] = si.statusends >= 0 ? max(si.statusends - now, 0) : -1; // -1 without a time limit, which the list can say and a query can't
        si.attr[14] = max(si.stateends - now, 0)*1000;
    }
    if(totalmillis - lastinfo >= (serverupdateinterval*1000)/(maxservpings ? max(1, (servers.length() + maxservpings - 1) / maxservpings) : 1)) pingservers();
}

static void cleardetail();

void clearservers()
{
    cleardetail();
    resolverclear();
    droppedqueries.deletearrays(); // the resolver forgot them
    if(serverlist.legacy == LEGACY_RESOLVE) resolverquery(serverlist.query);
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
                if(si && (si->hasstatus || si->polled)) { si->hasstatus = si->polled = false; si->cleanup(); } // this list has no status, queries fill it in

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

// how far our clock is ahead of the master's, from the time it stamped on what it sent, as the times in a status are by its clock
static int listskew = 0;

static void setlistskew(jsonvalue &v)
{
    jsonvalue *t = v.get("time");
    listskew = t && t->type == JSON_NUMBER ? int(time(NULL)) - jsonint(t->num) : 0;
}

// what a linked server reported to the master, or what the master's own query of it said, laid out as its query reply would be
static void applystatus(serverinfo &si, jsonvalue *st)
{
    if(!st || st->type != JSON_OBJECT)
    {
        if(si.hasstatus || si.polled) si.cleanup(); // left the link, queries fill it in again
        si.hasstatus = si.polled = false;
        return;
    }
    // a polled server still gets the full query, as it may be one that ignores the ping only one, and its own answer is fresher
    si.polled = st->getbool("polled");
    si.hasstatus = !si.polled;
    // end times by our own clock
    si.statusends = st->getint("ends", -1);
    if(si.statusends >= 0) si.statusends += listskew;
    si.stateends = st->getint("stateends");
    if(si.stateends) si.stateends += listskew;
    int now = int(time(NULL));
    jsonvalue *build = st->get("build");
    si.attr.setsize(0);
    si.attr.add(st->getint("gameversion"));
    si.attr.add(st->getint("modeid"));
    si.attr.add(st->getint("mutators"));
    si.attr.add(si.statusends >= 0 ? max(si.statusends - now, 0) : 0);
    si.attr.add(st->getint("maxplayers"));
    si.attr.add(st->getint("mastermode"));
    si.attr.add(st->getint("gamevars"));
    si.attr.add(st->getint("gamemods"));
    loopi(5) si.attr.add(build && build->at(i) ? jsonint(build->at(i)->num) : 0);
    si.attr.add(st->getint("state"));
    si.attr.add(max(si.stateends - now, 0)*1000);
    string text;
    cubestring(text, st->getstr("map"));
    filterstring(si.map, text, false);
    cubestring(text, st->getstr("maptitle"));
    filterstring(si.maptitle, text, false);
    cubestring(text, st->getstr("mapauthor"));
    filterstring(si.mapauthor, text, false);
    si.mapcrc = uint(st->getnum("mapcrc", 0));
    // only a web address the game would open, as the server checked it too
    cubestring(text, st->getstr("url"));
    copystring(si.url, isvalidurl(text) ? text : "");
    cubestring(text, st->getstr("motd"));
    filterstring(si.motd, text, true, true, true, true);
    jsonvalue *last = st->get("lastmatch");
    si.lastended = last && last->type == JSON_OBJECT ? last->getint("ended") : 0;
    if(si.lastended) si.lastended += listskew;
    if(si.lastended) client::serverlastmatch(si.lastmatch, last);
    else si.lastmatch[0] = '\0';
    si.players.deletearrays();
    si.handles.deletearrays();
    // names come plain and are decorated here as the query reply's are, numbered where two are the same
    jsonvalue *players = st->get("players");
    if(players) loopv(players->items)
    {
        jsonvalue *pl = players->items[i];
        if(pl->type != JSON_OBJECT) continue;
        string name, team, priv;
        cubestring(text, pl->getstr("name", ""));
        filterstring(name, text, true, true, true, true);
        if(si.polled) // only names, from servers that decorate their own
        {
            si.players.add(newstring(name));
            cubestring(text, pl->getstr("handle"));
            si.handles.add(newstring(text));
            continue;
        }
        cubestring(team, pl->getstr("team", ""));
        cubestring(priv, pl->getstr("privilege", ""));
        bool dup = false;
        loopvj(players->items) if(j != i && players->items[j]->type == JSON_OBJECT)
        {
            cubestring(text, players->items[j]->getstr("name", ""));
            string other;
            filterstring(other, text, true, true, true, true);
            if(!strcmp(name, other)) { dup = true; break; }
        }
        si.players.add(newstring(client::serverplayername(name, pl->getint("cn"), team, priv, pl->getbool("local"), pl->getint("colour"), dup)));
        cubestring(text, pl->getstr("handle"));
        si.handles.add(newstring(text));
    }
    si.numplayers = si.players.length();
    si.lastinfo = totalmillis;
    sortedservers = false;
}

// secure: the list came over TLS that checked out, so its fingerprints can be trusted to pin a server's certificate
static serverinfo *listentry(jsonvalue &e, bool secure)
{
    const char *host = e.getstr("host");
    if(!host || !*host) return NULL;
    string name, desc, handle, flags, branch;
    cubestring(name, host);
    cubestring(desc, e.getstr("description"));
    cubestring(handle, e.getstr("handle"));
    cubestring(flags, e.getstr("flags"));
    cubestring(branch, e.getstr("branch"));
    int port = e.getint("port", SERVER_PORT);
    serverinfo *si = addserver(name, port > 0 && port < 65536 ? port : SERVER_PORT, max(e.getint("priority"), 0), desc, handle, flags, branch, serverinfo::MASTER);
    if(si)
    {
        // where its own Web API is, for the details of who is playing
        int webport = e.getint("webport");
        si->webport = webport > 0 && webport < 65536 ? webport : 0;
        si->relay = clamp(e.getint("relay"), 0, 2);
        cubestring(si->channel, e.getstr("channel", ""));
        const char *fp = secure ? e.getstr("fingerprint", "") : "";
        copystring(si->fingerprint, fp);
        for(char *f = si->fingerprint; *f; f++) if(!isxdigit(*f)) { *f = '\0'; break; }
        applystatus(*si, e.get("status"));
    }
    return si;
}

static void applylist(jsonvalue &v, bool secure)
{
    setlistskew(v);
    jsonvalue *ver = v.get("version");
    if(ver && ver->type == JSON_OBJECT)
    {
        defformatstring(cmd, "setversion %d %d", ver->getint("main"), ver->getint("game"));
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
    if(list) loopv(list->items) if(list->items[i]->type == JSON_OBJECT) listentry(*list->items[i], secure);
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
    // only its own lookup is dropped, as clearing the resolver would strand servers being looked up with it
    if(serverlist.query)
    {
        if(serverlist.legacy == LEGACY_RESOLVE) droppedqueries.add(serverlist.query);
        else delete[] serverlist.query;
        serverlist.query = NULL;
    }
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
    serverlist.query = newstring(servermaster);
    serverlist.address.host = ENET_HOST_ANY;
    serverlist.address.port = servermasterport;
    serverlist.legacy = LEGACY_RESOLVE;
    serverlist.legacystart = totalmillis;
    resolverquery(serverlist.query);
}

static void legacyresolved(const ENetAddress &addr)
{
    serverlist.legacy = LEGACY_CONNECT; // its lookup has answered, so a failure from here frees it rather than waiting on an answer
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
                if(serverlist.data.length() > LEGACYMAXSIZE) { legacyfailed("reply too large"); return; }
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
    defformatstring(url, "%s://%s:%d" HTTP_API "/servers", serverlist.tier == LIST_HTTPS ? "https" : "http", servermaster, servermasterlinkport);
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

// the Matchmaking Queue, joined from the browser over the live list's socket for the account in auth.cfg: the game signs in to the
// master with its key first, so the socket is that account's and the master knows who is on a server by the account it signed in
#define QUEUE_MINTHRESHOLD 2
#define QUEUE_MAXTHRESHOLD 8
#define QUEUE_RENEWABLE 1800 // seconds left from which an entry may be renewed
VAR(IDF_READONLY, queueavailable, 0, 0, 1); // the master has a Matchmaking Queue for this game's version
SVAR(IDF_READONLY, queuechannel, ""); // which one: devel or release
VAR(IDF_READONLY, queuethreshold, 0, 0, QUEUE_MAXTHRESHOLD); // this account's Threshold, 0 when it isn't queued
VAR(IDF_READONLY, queueready, 0, 0, VAR_MAX); // entries waiting, not held on a server or on their way to one
VAR(IDF_READONLY, queueheld, 0, 0, VAR_MAX);
VAR(IDF_READONLY, queuetravelling, 0, 0, VAR_MAX);
VAR(IDF_READONLY, queuenextthreshold, 0, 0, QUEUE_MAXTHRESHOLD); // the Threshold closest to a pop, and how many more it needs
VAR(IDF_READONLY, queueneeded, 0, 0, VAR_MAX);
VAR(IDF_READONLY, queuepopport, 0, 0, 65535); // where this account's entry was sent, 0 until it is
SVAR(IDF_READONLY, queuepophost, "");
SVAR(IDF_READONLY, queueerror, "");
VAR(IDF_READONLY, queuesignedin, -1, 0, 2); // signed in to the master for the queue: -1 failed, 0 not, 1 signing in, 2 signed in

enum { QUEUEAUTH_FAILED = -1, QUEUEAUTH_NONE = 0, QUEUEAUTH_PENDING, QUEUEAUTH_DONE };

// who is waiting in this game's channel, as everyone sees them
struct queuewaiting
{
    string handle, display, priv, state;
    int threshold;
};

struct queuestate
{
    int want; // the Threshold asked for, until the master takes it
    bool sent;
    time_t expires; // when this account's entry ends, 0 when it isn't queued
    int auth, authstep, fetch; // authstep: 0 asking for a challenge, 1 answering it
    string handle, token; // the account signed in, and its Session on the master
    vector<queuewaiting> waiting;
} queuest = { 0, false, 0, QUEUEAUTH_NONE, 0, -1, "", "" };

static bool queuewanted() { return queuest.want > 0 || queuest.expires > 0; }

static void queuesetauth(int auth)
{
    queuest.auth = auth;
    setvar("queuesignedin", auth);
}

static void queuesignout()
{
    if(queuest.fetch >= 0) http::cancel(queuest.fetch);
    queuest.fetch = -1;
    memset(queuest.token, 0, sizeof(queuest.token));
    queuest.handle[0] = '\0';
    queuesetauth(QUEUEAUTH_NONE);
}

static void queueauthsend(vector<char> &body);

// the master's /auth: a name gives a challenge, the key's answer a Session
static void queueauthfetched(httpfetch &f)
{
    if(f.id != queuest.fetch) return;
    queuest.fetch = -1;
    jsonvalue *v = f.status == 200 && f.body.length() ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
    const char *name = client::feedaccount();
    if(!v || v->type != JSON_OBJECT || !name || strcmp(name, queuest.handle))
    {
        DELETEP(v);
        if(f.status != 200) conoutf(colourred, "Couldn't sign in to the master for the Matchmaking Queue (%d)", f.status);
        queuesetauth(QUEUEAUTH_FAILED);
        serverlist.lastlive = 0; // the list still opens, without an account
        return;
    }
    vector<char> body;
    jsonwriter w(body);
    if(!queuest.authstep)
    {
        vector<char> answer;
        const char *challenge = v->getstr("challenge", "");
        if(!*challenge || !client::feedanswer(challenge, answer)) { delete v; queuesetauth(QUEUEAUTH_FAILED); serverlist.lastlive = 0; return; }
        answer.add('\0');
        w.beginobject();
        w.num("id", v->getnum("id"));
        w.str("answer", answer.getbuf());
        w.endobject();
        delete v;
        queuest.authstep = 1;
        queueauthsend(body);
        return;
    }
    const char *token = v->getstr("token", "");
    if(!*token || strlen(token) >= sizeof(queuest.token)) { delete v; queuesetauth(QUEUEAUTH_FAILED); serverlist.lastlive = 0; return; }
    copystring(queuest.token, token);
    delete v;
    queuesetauth(QUEUEAUTH_DONE);
    serverlist.lastlive = 0; // opens straight away, as this account
}

static void queueauthsend(vector<char> &body)
{
    defformatstring(url, "%s://%s:%d" HTTP_API "/auth", serverlist.tier == LIST_HTTPS ? "https" : "http", servermaster, servermasterlinkport);
    httpfetch *f = http::newfetch(HTTP_POST, url, queueauthfetched, NULL);
    f->reqheaders.add("Content-Type", "application/json");
    f->reqbody.put(body.getbuf(), body.length());
    queuest.fetch = -2; // a failure answers at once
    int id = http::submit(f);
    if(queuest.fetch == -2) queuest.fetch = id;
}

// signing in comes before the live list opens, so the socket is the account's from the start
static void queuesignin()
{
    const char *name = client::feedaccount();
    if(!name) return;
    queuesignout();
    copystring(queuest.handle, name);
    queuesetauth(QUEUEAUTH_PENDING);
    queuest.authstep = 0;
    vector<char> body;
    jsonwriter w(body);
    w.beginobject();
    w.str("name", name);
    w.endobject();
    queueauthsend(body);
}

static void queuesend()
{
    if(serverlist.live < 0 || queuest.want <= 0 || queuest.sent || queuest.auth != QUEUEAUTH_DONE) return;
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("queue", "join");
    w.str("source", "game");
    w.num("threshold", queuest.want);
    // the master puts the game in the channel for its version, and only sends it where its protocol is played
    defformatstring(version, "%d.%d.%d", versionmajor, versionminor, versionpatch);
    w.str("version", version);
    w.num("protocol", server::getver(1));
    w.endobject();
    if(http::wssend(serverlist.live, buf.getbuf(), buf.length())) queuest.sent = true;
}

static void queuecleared()
{
    queuest.want = 0;
    queuest.sent = false;
    queuest.expires = 0;
    setvar("queuethreshold", 0);
    setvar("queuepopport", 0);
    setsvar("queuepophost", "");
}

static void queuestr(char *dst, jsonvalue *v, const char *name)
{
    vector<char> buf;
    json::tocube(buf, v->getstr(name, ""));
    buf.add('\0');
    filterstring(dst, buf.getbuf(), true, true, true, true, false, MAXSTRLEN-1);
}

static void queueentries(jsonvalue *entries, const char *channel)
{
    queuest.waiting.setsize(0);
    if(!entries || entries->type != JSON_ARRAY) return;
    loopv(entries->items)
    {
        jsonvalue *e = entries->items[i];
        if(e->type != JSON_OBJECT || strcmp(e->getstr("channel", ""), channel)) continue;
        queuewaiting &q = queuest.waiting.add();
        queuestr(q.handle, e, "handle");
        queuestr(q.display, e, "displayname");
        queuestr(q.priv, e, "privilege");
        queuestr(q.state, e, "state");
        q.threshold = clamp(e->getint("threshold"), 0, QUEUE_MAXTHRESHOLD);
    }
}

static void queueevent(jsonvalue &v, const char *event)
{
    if(!strcmp(event, "queue"))
    {
        jsonvalue *queue = v.get("queue"), *channels = queue && queue->type == JSON_OBJECT ? queue->get("channels") : NULL, *q = NULL;
        // only this version's channel is any use to this game
        defformatstring(version, "%d.%d.%d", versionmajor, versionminor, versionpatch);
        if(channels && channels->type == JSON_ARRAY) loopv(channels->items)
        {
            jsonvalue *c = channels->items[i];
            if(c->type == JSON_OBJECT && !strcmp(c->getstr("version", ""), version)) { q = c; break; }
        }
        setvar("queueavailable", q ? 1 : 0);
        if(!q) { queuest.waiting.setsize(0); return; }
        string name;
        cubestring(name, q->getstr("name", ""));
        setsvar("queuechannel", name);
        setvar("queueready", max(q->getint("ready"), 0));
        setvar("queueheld", max(q->getint("held"), 0));
        setvar("queuetravelling", max(q->getint("travelling"), 0));
        jsonvalue *next = q->get("next");
        bool has = next && next->type == JSON_OBJECT;
        setvar("queuenextthreshold", has ? clamp(next->getint("threshold"), 0, QUEUE_MAXTHRESHOLD) : 0);
        setvar("queueneeded", has ? max(next->getint("needed"), 0) : 0);
        queueentries(queue->get("entries"), q->getstr("name", ""));
        queuesend();
    }
    else if(!strcmp(event, "queued"))
    {
        const char *error = v.getstr("error", "");
        if(*error)
        {
            string msg;
            cubestring(msg, error);
            setsvar("queueerror", msg);
            conoutf(colourred, "Couldn't join the Matchmaking Queue: %s", msg);
            if(!queuest.expires) queuecleared();
            queuest.want = 0;
            return;
        }
        // also on connecting, for an entry this account has from anywhere
        queuest.want = 0;
        queuest.sent = false;
        queuest.expires = time_t(v.getnum("expires"));
        setsvar("queueerror", "");
        setvar("queuethreshold", clamp(v.getint("threshold"), 0, QUEUE_MAXTHRESHOLD));
        string host;
        cubestring(host, v.getstr("host", ""));
        int port = v.getint("port");
        if(host[0] && port > 0 && port <= 65535)
        {
            setsvar("queuepophost", host);
            setvar("queuepopport", port);
        }
    }
    else if(!strcmp(event, "unqueued"))
    {
        const char *reason = v.getstr("reason", "");
        if(!strcmp(reason, "satisfied")) conoutf(colourgrey, "Enough players are here now, so you've left the Matchmaking Queue");
        else if(!strcmp(reason, "expired")) conoutf(colourgrey, "Your place in the Matchmaking Queue ran out");
        else if(!strcmp(reason, "popped")) conoutf(colourgrey, "You didn't join your match in time, so you've left the Matchmaking Queue");
        queuecleared();
    }
    else if(!strcmp(event, "pop"))
    {
        // every viewer hears of a pop, and only those it sent are told it's theirs
        if(!v.getbool("mine")) return;
        string host;
        cubestring(host, v.getstr("host", ""));
        int port = v.getint("port");
        if(!host[0] || port <= 0 || port > 65535) return;
        setsvar("queuepophost", host);
        setvar("queuepopport", port);
        conoutf(colourwhite, "A match is ready for you on %s:%d", host, port);
#if SDL_VERSION_ATLEAST(2, 0, 16)
        if(screen) SDL_FlashWindow(screen, SDL_FLASH_UNTIL_FOCUSED);
#endif
        triggereventcallbacks(CMD_EVENT_GAME_QUEUEPOP);
    }
}

ICOMMAND(0, queuejoin, "i", (int *threshold),
{
    if(*threshold < QUEUE_MINTHRESHOLD || *threshold > QUEUE_MAXTHRESHOLD || (*threshold&1)) { conoutf(colourred, "A Threshold is 2, 4, 6 or 8"); return; }
    if(!servermaster[0]) { conoutf(colourred, "There is no master server to queue with"); return; }
    if(!client::feedaccount()) { conoutf(colourred, "You need an account to queue, which you can register on the Account page of the Player menu"); return; }
    queuest.want = *threshold;
    queuest.sent = false;
    setsvar("queueerror", "");
    if(queuest.auth == QUEUEAUTH_FAILED)
    {
        // a sign-in that failed is tried again, on a socket of its own
        closelive();
        queuesetauth(QUEUEAUTH_NONE);
        serverlist.lastlive = 0;
        return;
    }
    queuesend();
});

ICOMMAND(0, queueleave, "", (),
{
    if(serverlist.live >= 0 && queuest.auth == QUEUEAUTH_DONE) http::wssend(serverlist.live, "{\"queue\":\"leave\"}");
    queuecleared();
});

ICOMMAND(0, queuerenew, "", (),
{
    if(serverlist.live >= 0 && queuest.expires && queuest.auth == QUEUEAUTH_DONE) http::wssend(serverlist.live, "{\"queue\":\"renew\"}");
});

// seconds until this account's entry ends, 0 when it isn't queued
ICOMMAND(0, queueleft, "", (), intret(queuest.expires ? int(max(queuest.expires - time(NULL), time_t(0))) : 0));
ICOMMAND(0, queuerenewable, "", (), intret(queuest.expires && queuest.expires - time(NULL) <= QUEUE_RENEWABLE ? 1 : 0));

// getqueuewaiting [index field]: how many are waiting in this game's channel, or one's handle, displayname, privilege, threshold,
// state (ready, held or travelling) or name, which is how the game shows a player, with their privilege's icon
static void getqueuewaiting(int *idx, char *field, int *numargs)
{
    if(*numargs < 1 || *idx < 0) { intret(queuest.waiting.length()); return; }
    if(!queuest.waiting.inrange(*idx)) { result(""); return; }
    queuewaiting &q = queuest.waiting[*idx];
    if(!strcmp(field, "handle")) result(q.handle);
    else if(!strcmp(field, "displayname")) result(q.display);
    else if(!strcmp(field, "privilege")) result(q.priv);
    else if(!strcmp(field, "threshold")) intret(q.threshold);
    else if(!strcmp(field, "state")) result(q.state);
    else if(!strcmp(field, "name"))
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("name", q.display[0] ? q.display : q.handle);
        w.str("privilege", q.priv);
        w.num("colour", 0xFFFFFF); // the icon takes a player's colour, and nobody here has picked one
        w.endobject();
        jsonvalue *p = json::parse(buf.getbuf(), buf.length());
        result(client::serverplayerjson(p));
        DELETEP(p);
    }
    else result("");
}
COMMAND(0, getqueuewaiting, "isN");

// joins the server this account's entry was sent to, where the master sees it arrive by the account
ICOMMAND(0, queuepopjoin, "", (),
{
    if(!queuepopport || !queuepophost[0]) return;
    connectserv(queuepophost, queuepopport, NULL);
});

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
            setlistskew(*v);
            const char *event = v->getstr("event", "");
            if(!strcmp(event, "snapshot"))
            {
                jsonvalue *list = v->get("servers");
                beginmasterlist();
                if(list) loopv(list->items) if(list->items[i]->type == JSON_OBJECT) listentry(*list->items[i], serverlist.livesecure);
                endmasterlist();
            }
            else if(!strcmp(event, "add") || !strcmp(event, "update"))
            {
                jsonvalue *server = v->get("server");
                if(server && server->type == JSON_OBJECT) listentry(*server, serverlist.livesecure);
            }
            else if(!strcmp(event, "remove"))
            {
                string host;
                cubestring(host, v->getstr("host", ""));
                int port = v->getint("port");
                loopv(servers) if(servers[i]->port == port && !strcmp(servers[i]->name, host))
                {
                    servers[i]->inlist = false;
                    if(servers[i]->source != serverinfo::MASTER) continue;
                    servers[i]->purge = true;
                    break;
                }
                purgeservers();
            }
            else queueevent(*v, event);
            delete v;
            break;
        }
        case WSEV_CLOSE: serverlist.live = -1; queuest.sent = false; break;
        default: break;
    }
}

static void checklive()
{
    bool browsing = (serverlist.lastbrowse && totalmillis - serverlist.lastbrowse < LISTBROWSING) || queuewanted();
    if(!browsing) { closelive(); return; }
    // a different account in auth.cfg is signed in again, on a socket of its own
    const char *name = client::feedaccount();
    if(queuest.auth != QUEUEAUTH_NONE && (!name || strcmp(name, queuest.handle)))
    {
        closelive();
        queuesignout();
        queuecleared();
        serverlist.lastlive = 0;
    }
    if(serverlist.live >= 0 || serverlistfetching || serverlist.tier >= LIST_LEGACY || !serverlist.lastfetch) return;
    if(name && queuest.auth == QUEUEAUTH_NONE) { queuesignin(); return; }
    if(queuest.auth == QUEUEAUTH_PENDING) return;
    int interval = serverlistinterval*1000;
    if(serverlist.lastlive && totalmillis - serverlist.lastlive < max(interval, 10000)) return;
    serverlist.lastlive = totalmillis ? totalmillis : 1;
    defformatstring(url, "%s://%s:%d" HTTP_API "/servers/live", serverlist.tier == LIST_HTTPS ? "wss" : "ws", servermaster, servermasterlinkport);
    if(queuest.auth == QUEUEAUTH_DONE) concformatstring(url, "?token=%s", queuest.token);
    serverlist.livesecure = serverlist.tier == LIST_HTTPS;
    serverlist.live = http::wsopen(url, liveevent);
}

void checkserverlist()
{
    if(serverlist.legacy == LEGACY_RESOLVE) checkresolver();
    else if(serverlist.legacy != LEGACY_IDLE) checklegacy();
    checklive();
}

// The browser's entry for the server this client is on, if it has one; listed is set when a recent Server list has it,
// and while in a match a list is fetched now and then so it stays recent
serverinfo *presenceserver(bool &listed)
{
    listed = false;
    if(!curpeer || !*connectname) return NULL;
    int age = listapplied ? totalmillis - listapplied : INT_MAX;
    if(age >= PRESENCELISTREFRESH && !serverlistfetching && (!serverlist.lastfetch || totalmillis - serverlist.lastfetch >= PRESENCELISTRETRY)) startlist();
    serverinfo *found = NULL;
    loopv(servers)
    {
        serverinfo *si = servers[i];
        if(si->port != connectport) continue;
        bool match = !strcasecmp(si->name, connectname);
        if(!match)
        {
            ENetAddress addr;
            match = enet_address_set_host_ip(&addr, si->name) >= 0 && addr.host == curpeer->address.host;
        }
        if(!match) continue;
        if(si->inlist && age < PRESENCELISTAGE)
        {
            listed = true;
            return si;
        }
        if(!found) found = si;
    }
    return found;
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
#define DETAILPARTS 3
#define DETAILNEEDED 2 // the parts every server has; the objectives are newer, and a server without them still shows who is playing

struct serverdetail
{
    string name;
    int port, state, route, fetch, part, last, failed; // failed: the HTTP status of the last fetch that failed, 0 when none answered, -1 with no way to it
    jsonvalue *parts[DETAILPARTS]; // players, teams, then objectives

    serverdetail() : port(0), state(DETAIL_NONE), route(DETAIL_DIRECT), fetch(-1), part(0), last(0), failed(0) { name[0] = '\0'; loopi(DETAILPARTS) parts[i] = NULL; }
};
static serverdetail detail;
static vector<jsonvalue *> detailnext; // a refresh is kept apart until every part is in, so the panel never shows half of each
static const char *detailpaths[DETAILPARTS] = { "players", "teams", "objectives" };

static serverinfo *detailserver()
{
    loopv(servers) if(servers[i]->port == detail.port && !strcmp(servers[i]->name, detail.name)) return servers[i];
    return NULL;
}

// the server's Event feed while its details are shown, as lines of text, oldest first
#define FEEDLINES 100
#define FEEDRETRY 5000

VAR(IDF_PERSIST, serverdetailsignin, 0, 1, 1); // sign in on a server's Event feed with your account while its details are open, to see your standing there and chat

enum { FEEDAUTH_FAILED = -1, FEEDAUTH_NONE, FEEDAUTH_PENDING, FEEDAUTH_DONE };

struct serverfeed
{
    int ws, route, lastopen, auth;
    bool opened, authlocal;
    string authname, authpriv, autherror;
    vector<char *> lines;

    serverfeed() : ws(-1), route(DETAIL_DIRECT), lastopen(0), auth(FEEDAUTH_NONE), opened(false), authlocal(false) { authname[0] = authpriv[0] = autherror[0] = '\0'; }

    void resetauth()
    {
        auth = FEEDAUTH_NONE;
        authlocal = false;
        authname[0] = authpriv[0] = autherror[0] = '\0';
    }
};
static serverfeed feed;

static void feedline(const char *text)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    defformatstring(line, "\fa%02d:%02d:%02d\fw %s", lt ? lt->tm_hour : 0, lt ? lt->tm_min : 0, lt ? lt->tm_sec : 0, text);
    feed.lines.add(newstring(line));
    while(feed.lines.length() > FEEDLINES) delete[] feed.lines.remove(0);
}

static void feedsignin()
{
    const char *name = client::feedaccount();
    if(feed.ws < 0 || !feed.opened || !name || feed.auth == FEEDAUTH_PENDING || feed.auth == FEEDAUTH_DONE) return;
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("name", name);
    w.endobject();
    feed.resetauth();
    feed.auth = FEEDAUTH_PENDING;
    http::wssend(feed.ws, buf.getbuf(), buf.length());
}

// what the feed says about signing in and chatting, rather than the match
static bool feedauthevent(jsonvalue *v)
{
    const char *event = v->getstr("event", "");
    if(!strcmp(event, "challenge"))
    {
        vector<char> answer;
        const char *chal = v->getstr("challenge", "");
        if(!client::feedanswer(chal, answer))
        {
            feed.auth = FEEDAUTH_FAILED;
            copystring(feed.autherror, "couldn't answer the challenge");
            return true;
        }
        answer.add('\0');
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.num("id", v->getnum("id"));
        w.str("answer", answer.getbuf());
        w.endobject();
        http::wssend(feed.ws, buf.getbuf(), buf.length());
        return true;
    }
    if(!strcmp(event, "auth"))
    {
        const char *err = v->getstr("error");
        if(err)
        {
            feed.resetauth();
            feed.auth = FEEDAUTH_FAILED;
            vector<char> text;
            json::tocube(text, err);
            text.add('\0');
            copystring(feed.autherror, text.getbuf());
        }
        else
        {
            const char *priv = v->getstr("privilege", "none"), *name = v->getstr("name", "");
            feed.auth = strcmp(priv, "none") ? FEEDAUTH_DONE : FEEDAUTH_NONE;
            feed.authlocal = v->getbool("local");
            copystring(feed.authpriv, priv);
            vector<char> text;
            json::tocube(text, name);
            text.add('\0');
            copystring(feed.authname, text.getbuf());
        }
        return true;
    }
    if(!strcmp(event, "said"))
    {
        const char *err = v->getstr("error");
        if(err)
        {
            vector<char> text;
            json::tocube(text, err);
            text.add('\0');
            defformatstring(line, "\frNot sent: %s", text.getbuf());
            feedline(line);
        }
        else if(v->get("cut"))
        {
            defformatstring(line, "\fyCut short to %d characters", v->getint("cut"));
            feedline(line);
        }
        return true;
    }
    return false;
}

static void closefeed()
{
    if(feed.ws >= 0) http::wsclose(feed.ws);
    feed.ws = -1;
    feed.route = DETAIL_DIRECT;
    feed.lastopen = 0;
    feed.opened = false;
    feed.resetauth();
    feed.lines.deletearrays();
}

static void feedevent(httpwsevent &e, void *ctx)
{
    if(e.id != feed.ws) return;
    switch(e.type)
    {
        case WSEV_OPEN:
            feed.opened = true;
            if(serverdetailsignin) feedsignin();
            break;
        case WSEV_MESSAGE:
        {
            jsonvalue *v = json::parse(e.data, e.len);
            if(!v) break;
            string text;
            if(v->type == JSON_OBJECT && !feedauthevent(v) && client::servereventline(text, v)) feedline(text);
            delete v;
            break;
        }
        case WSEV_CLOSE:
            // one that never opened may be firewalled, so the next try goes through the relay
            if(!feed.opened) feed.route++;
            feed.opened = false;
            feed.ws = -1;
            feed.resetauth();
            break;
    }
}

static void checkfeed(serverinfo *si)
{
    if(feed.ws >= 0 || (feed.lastopen && totalmillis - feed.lastopen < FEEDRETRY)) return;
    feed.lastopen = totalmillis ? totalmillis : 1;
    loopi(DETAIL_ROUTES)
    {
        if(feed.route >= DETAIL_ROUTES) feed.route = DETAIL_DIRECT;
        string url;
        const char *pin = NULL;
        if(feed.route == DETAIL_DIRECT && si->webport)
        {
            formatstring(url, "%s://%s:%d" HTTP_API "/events", si->fingerprint[0] ? "wss" : "ws", si->name, si->webport);
            if(si->fingerprint[0]) pin = si->fingerprint;
        }
        else if(feed.route == DETAIL_RELAY && si->relay >= 1 && servermaster[0])
            formatstring(url, "%s://%s:%d" HTTP_RELAY "/%s/%d" HTTP_API "/events", servermasterlinktls ? "wss" : "ws", servermaster, servermasterlinkport, si->name, si->port);
        else { feed.route++; continue; }
        feed.ws = http::wsopen(url, feedevent, NULL, pin);
        return;
    }
}

static void cleardetail()
{
    closefeed();
    if(detail.fetch >= 0) http::cancel(detail.fetch);
    detail.fetch = -1;
    loopi(DETAILPARTS) DELETEP(detail.parts[i]);
    detailnext.deletecontents();
    detail.name[0] = '\0';
    detail.port = 0;
    detail.state = DETAIL_NONE;
    detail.failed = 0;
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
            formatstring(url, "%s://%s:%d" HTTP_API "/%s", si->fingerprint[0] ? "https" : "http", si->name, si->webport, detailpaths[detail.part]);
            if(si->fingerprint[0]) pin = si->fingerprint;
        }
        else if(detail.route == DETAIL_RELAY && si->relay >= 1 && servermaster[0])
            formatstring(url, "%s://%s:%d" HTTP_RELAY "/%s/%d" HTTP_API "/%s", servermasterlinktls ? "https" : "http", servermaster, servermasterlinkport, si->name, si->port, detailpaths[detail.part]);
        else { detail.route++; continue; }
        httpfetch *f = http::newfetch(HTTP_GET, url, detailfetched);
        if(pin) f->pin = newstring(pin);
        detail.fetch = LISTPENDING;
        int id = http::submit(f);
        if(detail.fetch == LISTPENDING) detail.fetch = id;
        return true;
    }
    // no way to it yet, as when the list that says where its Web API is hasn't come, so the next try starts again from the top
    if(!si->webport && si->relay < 1) detail.failed = -1;
    detail.route = DETAIL_DIRECT;
    detail.last = totalmillis ? totalmillis : 1;
    return false;
}

static void detailfetched(httpfetch &f)
{
    if(f.id != detail.fetch && detail.fetch != LISTPENDING) return;
    detail.fetch = -1;
    jsonvalue *v = f.status == 200 ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
    bool optional = detail.part >= DETAILNEEDED;
    if(optional)
    {
        if(v && v->type != JSON_OBJECT) DELETEP(v);
    }
    else if(!v || v->type != JSON_ARRAY)
    {
        DELETEP(v);
        detailnext.deletecontents();
        detail.part = 0;
        detail.failed = f.status;
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
    if(++detail.part < DETAILPARTS) { fetchdetail(); return; }
    loopi(DETAILPARTS) { DELETEP(detail.parts[i]); detail.parts[i] = detailnext[i]; }
    detailnext.setsize(0);
    detail.part = 0;
    detail.state = DETAIL_READY;
    detail.failed = 0;
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
    // as the website does, a relay that refused it or doesn't have it isn't asked again until the details are opened again
    else if(detail.fetch < 0 && totalmillis - detail.last >= DETAILREFRESH &&
            (detail.state != DETAIL_UNAVAILABLE || (detail.failed != 403 && detail.failed != 404 && detail.failed != 414)))
    {
        detail.part = 0;
        if(detail.state == DETAIL_UNAVAILABLE) detail.state = DETAIL_FETCHING;
        if(!fetchdetail()) detail.state = DETAIL_UNAVAILABLE;
    }
    checkfeed(si);
    intret(detail.state);
}
COMMAND(0, serverdetails, "i");
ICOMMAND(0, getserverevents, "", (), intret(feed.lines.length()));
ICOMMAND(0, getserverevent, "i", (int *n), result(feed.lines.inrange(*n) ? feed.lines[*n] : ""));
ICOMMAND(0, serverfeedopen, "", (), intret(feed.opened ? 1 : 0));
// signing in on the feed: -1 failed, 0 not signed in, 1 signing in, 2 signed in, with the standing it gave and why it failed
ICOMMAND(0, serverfeedauth, "", (), intret(feed.auth));
ICOMMAND(0, serverfeedauthname, "", (), result(feed.authname));
ICOMMAND(0, serverfeedauthpriv, "", (), result(feed.authpriv));
ICOMMAND(0, serverfeedauthlocal, "", (), intret(feed.authlocal ? 1 : 0));
ICOMMAND(0, serverfeedautherror, "", (), result(feed.autherror));
ICOMMAND(0, serverfeedsignin, "", (), feedsignin());
ICOMMAND(0, serverfeedhasaccount, "", (), intret(client::feedaccount() ? 1 : 0));

// chat there, under the server's rules for web chat, its answer coming back on the feed
static void serverfeedsay(char *text, int *action)
{
    if(feed.ws < 0 || feed.auth != FEEDAUTH_DONE || !text || !*text) return;
    vector<char> buf;
    jsonwriter w(buf);
    w.beginobject();
    w.str("say", text);
    if(*action) w.boolean("action", true);
    w.endobject();
    http::wssend(feed.ws, buf.getbuf(), buf.length());
}
COMMAND(0, serverfeedsay, "si");
// why the details couldn't be fetched: the HTTP status, 0 when nothing answered, -1 when there's no way to the server's Web API
ICOMMAND(0, serverdetailfailed, "", (), intret(detail.failed));
ICOMMAND(0, clearserverdetails, "", (), cleardetail());

// getserverdetail players|teams|flags|points|balls [index] [field]: the count without an index, an entry's field otherwise,
// where a field inside another is named with a dot, as in carrier.name
static void getserverdetail(char *list, int *idx, char *field, int *numargs)
{
    jsonvalue *v = NULL;
    if(detail.state == DETAIL_READY)
    {
        if(!strcmp(list, "players")) v = detail.parts[0];
        else if(!strcmp(list, "teams")) v = detail.parts[1];
        else if(detail.parts[2] && (!strcmp(list, "flags") || !strcmp(list, "points") || !strcmp(list, "balls")))
        {
            v = detail.parts[2]->get(list);
            if(v && v->type != JSON_ARRAY) v = NULL;
        }
    }
    if(*numargs < 2 || *idx < 0) { intret(v ? v->length() : 0); return; }
    jsonvalue *f = v ? v->at(*idx) : NULL;
    for(const char *name = field; f && name;)
    {
        const char *dot = strchr(name, '.');
        string key;
        copystring(key, name, dot ? min(size_t(dot-name+1), sizeof(key)) : sizeof(key));
        f = f->type == JSON_OBJECT ? f->get(key) : NULL;
        name = dot ? dot+1 : NULL;
    }
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
        case JSON_NUMBER: intret(jsonint(f->num)); break;
        case JSON_TRUE: intret(1); break;
        default: intret(0); break;
    }
}
COMMAND(0, getserverdetail, "sisN");
// getserverdetailname players|flags|balls index [field]: a player there, or in that entry's field such as carrier, decorated as the game would
static void getserverdetailname(char *list, int *idx, char *field)
{
    jsonvalue *v = NULL;
    if(detail.state == DETAIL_READY)
    {
        if(!strcmp(list, "players")) v = detail.parts[0];
        else if(detail.parts[2] && (!strcmp(list, "flags") || !strcmp(list, "balls"))) v = detail.parts[2]->get(list);
    }
    jsonvalue *p = v && v->type == JSON_ARRAY ? v->at(*idx) : NULL;
    if(p && *field) p = p->type == JSON_OBJECT ? p->get(field) : NULL;
    result(p && p->type == JSON_OBJECT ? client::serverplayerjson(p) : "");
}
COMMAND(0, getserverdetailname, "sis");


// a map's preview for the browser: our own copy when we have the map, otherwise the server's, fetched once into cache/mapshots
#define MAPSHOT_RETRY 60000 // how long a preview that couldn't be fetched waits before it's tried again
#define MAPSHOT_MAXSIZE (4<<20)

struct mapshotfetch
{
    string name;
    uint crc;
    int fetch, failed;
};
static vector<mapshotfetch> mapshotfetches;

// the map's own name, which the server chose, reduced to what's safe in a file name
static void mapshotpath(char *path, const char *map, uint crc)
{
    const char *name = strrchr(map, '/') ? strrchr(map, '/') + 1 : map;
    string safe;
    int len = 0;
    for(const char *c = name; *c && len < 64; c++) if(isalnum(uchar(*c)) || *c == '-' || *c == '_') safe[len++] = *c;
    safe[len] = '\0';
    nformatstring(path, MAXSTRLEN, "cache/mapshots/%s_%u.png", len ? safe : "map", crc);
}

static void mapshotfetched(httpfetch &f)
{
    // submit can answer before it returns, while the fetch is still pending
    loopv(mapshotfetches) if(mapshotfetches[i].fetch == f.id || mapshotfetches[i].fetch == LISTPENDING)
    {
        mapshotfetch &m = mapshotfetches[i];
        m.fetch = -1;
        static const uchar pngsig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        bool png = f.status == 200 && f.body.length() > 8 && !memcmp(f.body.getbuf(), pngsig, 8);
        string path;
        mapshotpath(path, m.name, m.crc);
        stream *file = png ? openfile(path, "wb") : NULL;
        if(!file || file->write(f.body.getbuf(), f.body.length()) != size_t(f.body.length())) m.failed = totalmillis ? totalmillis : 1;
        DELETEP(file);
        break;
    }
}

// asked every frame for every server shown, so what's found on disk is remembered
struct mapshotfound
{
    string map, path;
    uint crc;
};
static vector<mapshotfound> mapshotsfound;

static void getservermapshot(int *n)
{
    if(!servers.inrange(*n) || !servers[*n]->map[0]) { result(""); return; }
    serverinfo *si = servers[*n];
    loopv(mapshotsfound) if(mapshotsfound[i].crc == si->mapcrc && !strcmp(mapshotsfound[i].map, si->map)) { result(mapshotsfound[i].path); return; }
    string path, found = "";
    const char *name = strrchr(si->map, '/') ? strrchr(si->map, '/') + 1 : si->map;
    formatstring(path, "maps/%s.png", name);
    bool local = findfile(path, "e") != NULL;
    if(!local) { formatstring(path, "maps/%s.jpg", name); local = findfile(path, "e") != NULL; }
    // our own preview, unless the server's map is another version of it under the same name
    int localcrc = local && si->mapcrc ? mapccrc(si->map) : 0;
    if(localcrc && uint(localcrc) != si->mapcrc) local = false;
    if(local || !si->mapcrc) formatstring(found, "maps/%s", name);
    else
    {
        mapshotpath(path, si->map, si->mapcrc);
        if(findfile(path, "e")) copystring(found, path);
    }
    if(found[0])
    {
        if(mapshotsfound.length() >= 256) mapshotsfound.remove(0);
        mapshotfound &f = mapshotsfound.add();
        copystring(f.map, si->map);
        copystring(f.path, found);
        f.crc = si->mapcrc;
        result(found);
        return;
    }
    result("");
    mapshotfetch *m = NULL;
    loopv(mapshotfetches) if(mapshotfetches[i].crc == si->mapcrc && !strcmp(mapshotfetches[i].name, si->map)) { m = &mapshotfetches[i]; break; }
    if(m && (m->fetch >= 0 || (m->failed && totalmillis - m->failed < MAPSHOT_RETRY))) return;
    // through the relay when it allows one, as the master keeps it for everyone, otherwise straight from the server
    string url;
    const char *pin = NULL;
    if(si->relay >= 1 && servermaster[0])
        formatstring(url, "%s://%s:%d" HTTP_RELAY "/%s/%d" HTTP_API "/mapshot?crc=%u", servermasterlinktls ? "https" : "http", servermaster, servermasterlinkport, si->name, si->port, si->mapcrc);
    else if(si->webport)
    {
        formatstring(url, "%s://%s:%d" HTTP_API "/mapshot?crc=%u", si->fingerprint[0] ? "https" : "http", si->name, si->webport, si->mapcrc);
        if(si->fingerprint[0]) pin = si->fingerprint;
    }
    else return;
    if(!m) { m = &mapshotfetches.add(); copystring(m->name, si->map); m->crc = si->mapcrc; }
    m->failed = 0;
    httpfetch *f = http::newfetch(HTTP_GET, url, mapshotfetched);
    if(pin) f->pin = newstring(pin);
    f->maxsize = MAPSHOT_MAXSIZE;
    m->fetch = LISTPENDING;
    int id = http::submit(f);
    loopv(mapshotfetches) if(mapshotfetches[i].fetch == LISTPENDING) { mapshotfetches[i].fetch = id; break; }
}
COMMAND(0, getservermapshot, "i");

// when people usually play, from the master's history: the peak players averaged for each weekday and hour in our own time,
// for one server or, as -1, the whole game
#define HISTORY_DAYS 56 // eight of each weekday
#define HISTORY_REFRESH (15*60*1000) // the master's peaks cover 15 minutes

struct serverhistory
{
    string name;
    int port, state, fetch, last;
    float cells[7][24]; // Monday first, -1 where nothing is known
};
static serverhistory histories[2] = { { "", -1, DETAIL_NONE, -1, 0, {} }, { "", -1, DETAIL_NONE, -1, 0, {} } }; // a server, then the game

static void historyfetched(httpfetch &f)
{
    serverhistory *h = NULL;
    loopi(2) if(histories[i].fetch == f.id || histories[i].fetch == LISTPENDING) { h = &histories[i]; break; }
    if(!h) return;
    h->fetch = -1;
    h->last = totalmillis ? totalmillis : 1;
    jsonvalue *v = f.status == 200 ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
    jsonvalue *peaks = v && v->type == JSON_OBJECT ? v->get("peaks") : NULL;
    double start = v ? v->getnum("start", 0) : 0, bucket = v ? v->getnum("bucket", 0) : 0;
    if(!peaks || peaks->type != JSON_ARRAY || bucket <= 0)
    {
        DELETEP(v);
        h->state = DETAIL_UNAVAILABLE;
        return;
    }
    float sums[7][24] = { { 0 } };
    int counts[7][24] = { { 0 } };
    loopv(peaks->items)
    {
        jsonvalue *p = peaks->items[i];
        if(p->type != JSON_NUMBER) continue;
        time_t t = time_t(start + i*bucket);
        struct tm *lt = localtime(&t);
        if(!lt) continue;
        int day = (lt->tm_wday + 6) % 7;
        sums[day][lt->tm_hour] += float(p->num);
        counts[day][lt->tm_hour]++;
    }
    loopi(7) loopj(24) h->cells[i][j] = counts[i][j] ? sums[i][j]/counts[i][j] : -1;
    DELETEP(v);
    h->state = DETAIL_READY;
}

// asked every frame while it's shown, so it fetches once and again every HISTORY_REFRESH
static void serverhistorystate(int *n)
{
    bool game = *n < 0;
    if((!game && !servers.inrange(*n)) || !servermaster[0]) { intret(DETAIL_UNAVAILABLE); return; }
    serverhistory &h = histories[game ? 1 : 0];
    const char *name = game ? "" : servers[*n]->name;
    int port = game ? 0 : servers[*n]->port;
    bool other = h.port != port || strcmp(h.name, name);
    if(other || (h.fetch < 0 && totalmillis - h.last >= HISTORY_REFRESH))
    {
        if(other)
        {
            if(h.fetch >= 0) http::cancel(h.fetch);
            h.fetch = -1;
            copystring(h.name, name);
            h.port = port;
            h.state = DETAIL_FETCHING;
        }
        string url;
        if(game) formatstring(url, "%s://%s:%d" HTTP_API "/history?days=%d", servermasterlinktls ? "https" : "http", servermaster, servermasterlinkport, HISTORY_DAYS);
        else formatstring(url, "%s://%s:%d" HTTP_API "/servers/%s/%d/history?days=%d", servermasterlinktls ? "https" : "http", servermaster, servermasterlinkport, name, port, HISTORY_DAYS);
        h.last = totalmillis ? totalmillis : 1;
        h.fetch = LISTPENDING;
        int id = http::submit(http::newfetch(HTTP_GET, url, historyfetched));
        if(h.fetch == LISTPENDING) h.fetch = id;
    }
    intret(h.state);
}
COMMANDN(0, serverhistory, serverhistorystate, "i");

// getserverhistory <n> <day> <hour>: the usual players then, Monday as day 0, -1 when unknown
static void getserverhistory(int *n, int *day, int *hour)
{
    serverhistory &h = histories[*n < 0 ? 1 : 0];
    floatret(h.state == DETAIL_READY && *day >= 0 && *day < 7 && *hour >= 0 && *hour < 24 ? h.cells[*day][*hour] : -1);
}
COMMAND(0, getserverhistory, "iii");

// getserverhistorytop <n>: the busiest day, hour and players, or nothing when there's no history
static void getserverhistorytop(int *n)
{
    serverhistory &h = histories[*n < 0 ? 1 : 0];
    int bd = -1, bh = -1;
    if(h.state == DETAIL_READY) loopi(7) loopj(24) if(h.cells[i][j] >= 0 && (bd < 0 || h.cells[i][j] > h.cells[bd][bh])) { bd = i; bh = j; }
    if(bd < 0) { result(""); return; }
    defformatstring(top, "%d %d %f", bd, bh, h.cells[bd][bh]);
    result(top);
}
COMMAND(0, getserverhistorytop, "i");

// the weekday, Monday as 0, and hour now in our own time, to mark them in the history
static void serverhistorynow()
{
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    defformatstring(when, "%d %d", lt ? (lt->tm_wday + 6) % 7 : 0, lt ? lt->tm_hour : 0);
    result(when);
}
COMMAND(0, serverhistorynow, "");

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
        f->printf("addserver %s %d %d %s %s %s %s %s\n", escapestring(s->name), s->port, s->priority, escapestring(s->sdesc), escapestring(s->authhandle), escapestring(s->flags), escapestring(s->branch),
            s->source == serverinfo::MASTER ? "master" : "manual");
    }
    delete f;
}
