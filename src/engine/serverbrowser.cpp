// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "engine.h"
#include "SDL_thread.h"

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
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverstop(resolverthread &rt)
{
    SDL_LockMutex(resolvermutex);
    if(rt.query)
    {
#ifndef __APPLE__
        SDL_KillThread(rt.thread);
#endif
        rt.thread = SDL_CreateThread(resolverloop, &rt);
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

    defformatstring(text, "resolving %s...", name);
    progress(0, text);

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    int starttime = SDL_GetTicks(), timeout = 0;
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

        timeout = SDL_GetTicks() - starttime;
        progress(min(float(timeout)/RESOLVERLIMIT, 1.0f), text);
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
    return resolved;
}

#define CONNLIMIT 20000

int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &address)
{
    defformatstring(text, "connecting to %s:[%d]...", hostname != NULL ? hostname : "local server", address.port);
    progress(0, text);

    ENetSocketSet readset, writeset;
    if(!enet_socket_connect(sock, &address)) for(int starttime = SDL_GetTicks(), timeout = 0; timeout <= CONNLIMIT;)
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
        timeout = SDL_GetTicks() - starttime;
        progress(min(float(timeout)/CONNLIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) break;
    }

    return -1;
}

vector<serverinfo *> servers;
bool sortedservers = true;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

static serverinfo *newserver(const char *name, int port = SERVER_PORT, int priority = 0, const char *desc = NULL, const char *handle = NULL, const char *flags = NULL, const char *branch = NULL, uint ip = ENET_HOST_ANY)
{
    serverinfo *si = new serverinfo(ip, port, priority);

    if(name) copystring(si->name, name);
    else if(ip == ENET_HOST_ANY || enet_address_get_host_ip(&si->address, si->name, sizeof(si->name)) < 0)
    {
        delete si;
        return NULL;
    }
    if(desc && *desc) copystring(si->sdesc, desc);
    if(handle && *handle) copystring(si->authhandle, handle);
    if(flags && *flags) copystring(si->flags, flags);
    if(branch && *branch) copystring(si->branch, branch);

    servers.add(si);
    sortedservers = false;

    return si;
}

void addserver(const char *name, int port, int priority, const char *desc, const char *handle, const char *flags, const char *branch)
{
    loopv(servers) if(!strcmp(servers[i]->name, name) && servers[i]->port == port) return;
    if(newserver(name, port, priority, desc, handle, flags, branch) && verbose >= 2)
        conoutf("added server %s (%d) [%s]", name, port, desc);
}
ICOMMAND(0, addserver, "siissss", (char *n, int *p, int *r, char *d, char *h, char *f, char *b), addserver(n, *p > 0 ? *p : SERVER_PORT, *r >= 0 ? *r : 0, d, h, f, b));

VAR(0, searchlan, 0, 0, 1);
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
    uchar ping[MAXTRANS];
    ucharbuf p(ping, sizeof(ping));
    putint(p, totalmillis ? totalmillis : 1);

    static int lastping = 0;
    if(lastping >= servers.length()) lastping = 0;
    loopi(maxservpings ? min(servers.length(), maxservpings) : servers.length())
    {
        serverinfo &si = *servers[lastping];
        if(++lastping >= servers.length()) lastping = 0;
        if(si.address.host == ENET_HOST_ANY) continue;
        buf.data = ping;
        buf.dataLength = p.length();
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

void checkresolver()
{
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
    if(!resolving) return;

    const char *name = NULL;
    for(;;)
    {
        ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
        if(!resolvercheck(&name, &addr)) break;
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
        if(!si && searchlan) si = newserver(NULL, addr.port-1, 1, NULL, NULL, NULL, NULL, addr.host);
        if(!si) continue;
        ucharbuf p(ping, len);
        int millis = getint(p), rtt = clamp(totalmillis - millis, 0, min(serverdecay*1000, totalmillis));
        if(millis >= lastreset && rtt < serverdecay*1000) si->addping(rtt, millis);
        si->lastinfo = totalmillis;
        si->numplayers = getint(p);
        int numattr = getint(p);
        si->attr.shrink(0);
        loopj(numattr) si->attr.add(getint(p));
        int gver = si->attr.empty() ? 0 : si->attr[0];
        getstring(text, p);
        filterstring(si->map, text, false);
        getstring(text, p);
        filterstring(si->sdesc, text);
        si->players.deletearrays();
        si->handles.deletearrays();
        if(gver >= 227)
        {
            getstring(text, p);
            filterstring(si->branch, text);
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
    if(totalmillis - lastinfo >= (serverupdateinterval*1000)/(maxservpings ? max(1, (servers.length() + maxservpings - 1) / maxservpings) : 1)) pingservers();
}

bool reqmaster = false;

void clearservers()
{
    resolverclear();
    servers.deletecontents();
    lastinfo = 0;
}

COMMAND(0, clearservers, "");

#define RETRIEVELIMIT 20000

void retrieveservers(vector<char> &data)
{
    ENetSocket sock = connectmaster(false);
    if(sock == ENET_SOCKET_NULL) return;

    defformatstring(text, "retrieving servers from %s:[%d]", servermaster, servermasterport);
    progress(0, text);

    int starttime = SDL_GetTicks(), timeout = 0;
    const char *req = "update\n";
    int reqlen = strlen(req);
    ENetBuffer buf;
    while(reqlen > 0)
    {
        enet_uint32 events = ENET_SOCKET_WAIT_SEND;
        if(enet_socket_wait(sock, &events, 250) >= 0 && events)
        {
            buf.data = (void *)req;
            buf.dataLength = reqlen;
            int sent = enet_socket_send(sock, NULL, &buf, 1);
            if(sent < 0) break;
            req += sent;
            reqlen -= sent;
            if(reqlen <= 0) break;
        }
        timeout = SDL_GetTicks() - starttime;
        progress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) timeout = RETRIEVELIMIT + 1;
        if(timeout > RETRIEVELIMIT) break;
    }

    if(reqlen <= 0) for(;;)
    {
        enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
        if(enet_socket_wait(sock, &events, 250) >= 0 && events)
        {
            if(data.length() >= data.capacity()) data.reserve(4096);
            buf.data = data.getbuf() + data.length();
            buf.dataLength = data.capacity() - data.length();
            int recv = enet_socket_receive(sock, NULL, &buf, 1);
            if(recv <= 0) break;
            data.advance(recv);
        }
        timeout = SDL_GetTicks() - starttime;
        progress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) timeout = RETRIEVELIMIT + 1;
        if(timeout > RETRIEVELIMIT) break;
    }

    if(data.length()) data.add('\0');
    enet_socket_destroy(sock);
}

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

void updatefrommaster()
{
    pausesortservers = 0;
    vector<char> data;
    retrieveservers(data);
    if(data.length() && data[0])
    {
        //clearservers();
        execute(data.getbuf());
        if(verbose) conoutf("\faretrieved %d server(s) from master", servers.length());
        else conoutf("\faretrieved list from master successfully");//, servers.length());
    }
    else conoutf("master server not replying");
    refreshservers();
    reqmaster = true;
}
COMMAND(0, updatefrommaster, "");

void updateservers()
{
    if(!reqmaster) updatefrommaster();
    refreshservers();
    if(autosortservers && !pausesortservers) sortservers();
    intret(servers.length());
}
COMMAND(0, updateservers, "");

void writeservercfg()
{
    if(servers.empty()) return;
    stream *f = openutf8file("servers.cfg", "w");
    if(!f) return;
    f->printf("// servers which are connected to or queried get added here automatically\n\n");
    loopv(servers)
    {
        serverinfo *s = servers[i];
        f->printf("addserver %s %d %d %s %s %s %s\n", escapeid(s->name), s->port, s->priority, escapeid(s->sdesc[0] ? s->sdesc : s->name), escapeid(s->authhandle), escapeid(s->flags), escapeid(s->branch));
    }
    delete f;
}
