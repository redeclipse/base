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

VAR(0, masterserver, 0, 0, 1);
VAR(0, masterport, 1, MASTER_PORT, VAR_MAX);
VAR(0, masterminver, 0, 0, CUR_VERSION);

VAR(0, mastermainver, 0, 0, CUR_VERSION);
VAR(0, mastergamever, 0, 0, VAR_MAX);

SVAR(0, masterip, "");
SVAR(0, masterscriptclient, "");
SVAR(0, masterscriptserver, "");

VAR(0, masterduplimit, 0, 3, VAR_MAX);
VAR(0, masterpingdelay, 1000, 3000, VAR_MAX);
VAR(0, masterpingtries, 1, 5, VAR_MAX);

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
    int inputpos, outputpos, port, numpings, lastcontrol, version;
    enet_uint32 lastping, lastpong, lastactivity;
    vector<authreq> authreqs;
    authreq serverauthreq;
    bool isserver, isquick, ishttp, listserver, shouldping, shouldpurge;

    masterclient() { reset(); }
    ~masterclient() {}

    void reset()
    {
        name[0] = desc[0] = flags[0] = authhandle[0] = branch[0] = '\0';
        lastcontrol = -1;
        inputpos = outputpos = numpings = version = 0;
        lastping = lastpong = lastactivity = 0;
        isserver = isquick = ishttp = listserver = shouldping = shouldpurge = false;
        port = MASTER_PORT;
        output.shrink(0);
        authreqs.shrink(0);
    }

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
}

void reqauth(masterclient &c, uint id, char *name, char *hostname)
{
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
    if(c.serverauthreq.reqtime) return;

    purgeauths(c);

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

void purgemasterclient(int n)
{
    masterclient &c = *masterclients[n];
    enet_socket_destroy(c.socket);
    if(verbose || c.isserver) conoutf(colourwhite, "Master peer %s disconnected", c.name);
    delete masterclients[n];
    masterclients.remove(n);
}

void confserverauth(masterclient &c, const char *val)
{
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
                c.lastpong = totalmillis ? totalmillis : 1;
                c.listserver = true;
                c.shouldping = false;
                masteroutf(c, "echo \"ping reply confirmed (on port %d), server is now listed\"\n", addr.port);
                conoutf(colourwhite, "Master peer %s responded to ping request on port %d successfully", c.name, addr.port);
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
                    c.numpings = 0;
                    c.lastcontrol = controlversion;
                    loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL)
                        masteroutf(c, "%s %u %u \"%s\"\n", ipinfotypes[control[i].type], control[i].ip, control[i].mask, control[i].reason);
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
    return c.inputpos < (int)sizeof(c.input);
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

    loopv(masterclients)
    {
        masterclient &c = *masterclients[i];
        if(c.authreqs.length()) purgeauths(c);

        if(c.shouldping && (!c.lastping || ((!c.lastpong || ENET_TIME_GREATER(c.lastping, c.lastpong)) && ENET_TIME_DIFFERENCE(totalmillis, c.lastping) > uint(masterpingdelay))))
        {
            if(c.numpings < masterpingtries)
            {
                static const uchar ping[] = { 1 };
                ENetBuffer buf;
                buf.data = (void *)ping;
                buf.dataLength = sizeof(ping);
                ENetAddress addr = { c.address.host, enet_uint16(c.port+1) };
                c.numpings++;
                c.lastping = totalmillis ? totalmillis : 1;
                enet_socket_send(pingsocket, &addr, &buf, 1);
            }
            else
            {
                c.listserver = c.shouldping = false;
                masteroutf(c, "error \"ping attempts failed (tried %d times on port %d), server will not be listed\"\n", c.numpings, c.port+1);
            }
        }

        if(c.isserver && c.lastcontrol < controlversion)
        {
            loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL && control[i].version > c.lastcontrol)
                masteroutf(c, "%s %u %u %s\n", ipinfotypes[control[i].type], control[i].ip, control[i].mask, control[i].reason);
            c.lastcontrol = controlversion;
        }

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
            buf.dataLength = sizeof(c.input) - c.inputpos;

            int res = enet_socket_receive(c.socket, NULL, &buf, 1);
            if(res > 0)
            {
                c.inputpos += res;
                c.input[min(c.inputpos, (int)sizeof(c.input)-1)] = '\0';
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
    if(mastersocket != ENET_SOCKET_NULL) enet_socket_destroy(mastersocket);
}

void reloadmaster()
{
    clearauth();
}
