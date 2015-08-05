// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "engine.h"
#include <signal.h>
#ifdef WIN32
#include <shlobj.h>
#endif

int curtime = 0, totalmillis = 1, lastmillis = 1, timescale = 100, paused = 0, timeerr = 0, shutdownwait = 0;
time_t clocktime = 0, currenttime = 0, clockoffset = 0;
uint totalsecs = 0;

VAR(0, maxruntime, 0, 86400, VAR_MAX); // time in seconds
VAR(0, maxshutdownwait, 0, 3600, VAR_MAX); // time in seconds

const char *load = NULL;
vector<char *> gameargs;

const char *platnames[MAX_PLATFORMS] = {
    "win", "osx", "nix"
}, *platlongnames[MAX_PLATFORMS] = {
    "windows", "macosx", "linux/bsd"
};

VAR(0, versioning, 1, 0, -1);

VAR(IDF_READONLY, version, 1, CUR_VERSION, -1);
VAR(IDF_READONLY, versionmajor, 1, VERSION_MAJOR, -1);
VAR(IDF_READONLY, versionminor, 1, VERSION_MINOR, -1);
VAR(IDF_READONLY, versionpatch, 1, VERSION_PATCH, -1);
SVAR(IDF_READONLY, versionstring, VERSION_STRING);
SVAR(IDF_READONLY, versionname, VERSION_NAME);
SVAR(IDF_READONLY, versionuname, VERSION_UNAME);
SVAR(IDF_READONLY, versionrelease, VERSION_RELEASE);
SVAR(IDF_READONLY, versionurl, VERSION_URL);
SVAR(IDF_READONLY, versioncopy, VERSION_COPY);
SVAR(IDF_READONLY, versiondesc, VERSION_DESC);
SVAR(IDF_READONLY, versionplatname, plat_name(CUR_PLATFORM));
SVAR(IDF_READONLY, versionplatlongname, plat_longname(CUR_PLATFORM));
VAR(IDF_READONLY, versionplatform, 0, CUR_PLATFORM, VAR_MAX);
VAR(IDF_READONLY, versionarch, 0, CUR_ARCH, VAR_MAX);
VAR(IDF_READONLY, versioncrc, 0, 0, VAR_MAX);
SVAR(IDF_READONLY, versionbranch, "?");
#ifdef STANDALONE
VAR(IDF_READONLY, versionisserver, 0, 1, 1);
#else
VAR(IDF_READONLY, versionisserver, 0, 0, 1);
#endif
ICOMMAND(0, platname, "ii", (int *p, int *g), result(*p >= 0 && *p < MAX_PLATFORMS ? (*g!=0 ? plat_longname(*p) : plat_name(*p)) : ""));

VAR(0, rehashing, 1, 0, -1);

const char * const disc_reasons[] = { "normal", "end of packet", "client num", "user was kicked", "message error", "address is banned", "server is in private mode", "server is password protected", "server requires pure official builds", "server is at maximum capacity", "server and client are incompatible", "connection timed out", "packet overflow", "hostname lookup failure", "server shutting down" };

SVAR(IDF_PERSIST, logtimeformat, "%Y-%m-%d %H:%M.%S");
VAR(IDF_PERSIST, logtimelocal, 0, 1, 1); // use clockoffset to localise
SVAR(IDF_PERSIST, filetimeformat, "%Y%m%d%H%M%S");
VAR(IDF_PERSIST, filetimelocal, 0, 1, 1); // use clockoffset to localise

const char *gettime(time_t ctime, const char *format)
{
    static string buf;
    if(!ctime) ctime = logtimelocal ? currenttime : clocktime;
    struct tm *t = localtime(&ctime);
    if(!strftime(buf, sizeof(buf), format && *format ? format : logtimeformat, t)) buf[0] = '\0';
    return buf;
}
ICOMMAND(0, gettime, "isi", (int *n, char *a, int *p), result(gettime(*n+(*p!=0 ? clockoffset : 0), a)));

const char *timestr(int dur, int style)
{
    static string buf; buf[0] = '\0';
    int tm = dur, ms = 0, ss = 0, mn = 0;
    if(tm > 0)
    {
        ms = tm%1000;
        tm = (tm-ms)/1000;
    }
    if(style > 0 && tm > 0)
    {
        ss = tm%60;
        tm = (tm-ss)/60;
        if(tm > 0) mn = tm;
    }
    switch(style)
    {
        case 0: formatstring(buf, "%d.%d", tm, ms/100); break;
        case 1: formatstring(buf, "%d:%02d.%03d", mn, ss, ms); break;
        case 2: formatstring(buf, "%d:%02d.%d", mn, ss, ms/100); break;
        case 3: formatstring(buf, "%d:%02d", mn, ss); break;
        case 4:
        {
            if(mn > 0)
            {
                if(ss > 0) formatstring(buf, "%dm%ds", mn, ss);
                else formatstring(buf, "%dm", mn);
                break;
            }
            formatstring(buf, "%ds", ss);
            break;
        }
    }
    return buf;
}
ICOMMAND(0, timestr, "ii", (int *d, int *s), result(timestr(*d, *s)));

vector<ipinfo> control;
void addipinfo(vector<ipinfo> &info, int type, const char *name, const char *reason)
{
    union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ip, mask;
    ip.i = 0;
    mask.i = 0;
    loopi(4)
    {
        char *end = NULL;
        int n = strtol(name, &end, 10);
        if(!end) break;
        if(end > name) { ip.b[i] = n; mask.b[i] = 0xFF; }
        name = end;
        while(*name && *name++ != '.');
    }
    ipinfo &p = info.add();
    p.ip = ip.i;
    p.mask = mask.i;
    p.type = clamp(type, 0, int(ipinfo::MAXTYPES)-1);
    p.flag = ipinfo::LOCAL;
    p.time = totalmillis ? totalmillis : 1;
#ifdef STANDALONE
    p.version = nextcontrolversion();
#endif
    if(reason && *reason) p.reason = newstring(reason);
    server::updatecontrols = true;
}
ICOMMAND(0, addallow, "ss", (char *name, char *reason), addipinfo(control, ipinfo::ALLOW, name, reason));
ICOMMAND(0, addban, "ss", (char *name, char *reason), addipinfo(control, ipinfo::BAN, name, reason));
ICOMMAND(0, addmute, "ss", (char *name, char *reason), addipinfo(control, ipinfo::MUTE, name, reason));
ICOMMAND(0, addlimit, "ss", (char *name, char *reason), addipinfo(control, ipinfo::LIMIT, name, reason));
ICOMMAND(0, addtrust, "ss", (char *name, char *reason), addipinfo(control, ipinfo::TRUST, name, reason));

const char *ipinfotypes[ipinfo::MAXTYPES] = { "allow", "ban", "mute", "limit", "trust" };
char *printipinfo(const ipinfo &info, char *buf)
{
    static string ipinfobuf = ""; char *str = buf ? buf : (char *)&ipinfobuf;
    union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ip, mask;
    ip.i = info.ip;
    mask.i = info.mask;
    int lastdigit = -1;
    str += sprintf(str, "[%s] ", ipinfotypes[clamp(info.type, 0, int(ipinfo::MAXTYPES)-1)]);
    loopi(4) if(mask.b[i])
    {
        if(lastdigit >= 0) *str++ = '.';
        loopj(i - lastdigit - 1) { *str++ = '*'; *str++ = '.'; }
        str += sprintf(str, "%d", ip.b[i]);
        lastdigit = i;
    }
    return str;
}

ipinfo *checkipinfo(vector<ipinfo> &info, int type, enet_uint32 ip)
{
    loopv(info) if(info[i].type == type && (ip & info[i].mask) == info[i].ip) return &info[i];
    return NULL;
}

#define LOGSTRLEN 512

FILE *logfile = NULL;

void closelogfile()
{
    if(logfile)
    {
        fclose(logfile);
        logfile = NULL;
    }
}

FILE *getlogfile()
{
#ifdef WIN32
    return logfile;
#else
    return logfile ? logfile : stdout;
#endif
}

void setlogfile(const char *fname)
{
    closelogfile();
    if(fname && fname[0])
    {
        fname = copypath(fname, true);
        if(fname[0] != PATHDIV && (fname[0] != '.' || fname[1] != '.')) fname = findfile(fname, "w");
        if(fname) logfile = fopen(fname, "w");
    }
    FILE *f = getlogfile();
    if(f) setvbuf(f, NULL, _IOLBF, BUFSIZ);
}

void logoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logoutfv(fmt, args);
    va_end(args);
}

void console(int type, const char *s, ...)
{
    defvformatbigstring(sf, s, s);
    bigstring osf;
    filterstring(osf, sf);
    if(*logtimeformat) logoutf("%s %s", gettime(logtimelocal ? currenttime : clocktime, logtimeformat), osf);
    else logoutf("%s", osf);
#ifndef STANDALONE
    conline(type, sf, 0);
#endif
}

void conoutft(int type, const char *s, ...)
{
    defvformatbigstring(sf, s, s);
    console(type, "%s", sf);
    ircoutf(5, "%s", sf);
}

void conoutf(const char *s, ...)
{
    defvformatbigstring(sf, s, s);
    conoutft(0, "%s", sf);
}

VAR(0, verbose, 0, 0, 6);

static void writelog(FILE *file, const char *buf)
{
    static uchar ubuf[LOGSTRLEN];
    size_t len = strlen(buf), carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf)-1, &((const uchar *)buf)[carry], len - carry, &carry);
        if(carry >= len) ubuf[numu++] = '\n';
        fwrite(ubuf, 1, numu, file);
    }
}

static void writelogv(FILE *file, const char *fmt, va_list args)
{
    static char buf[LOGSTRLEN];
    vformatstring(buf, fmt, args, sizeof(buf));
    writelog(file, buf);
}

#ifdef STANDALONE
void localservertoclient(int chan, ENetPacket *packet) {}
VAR(0, servertype, 1, 3, 3); // 1: private, 2: public, 3: dedicated
VAR(0, standalone, 1, 1, -1);
#else
VARF(0, servertype, 0, 0, 3, changeservertype()); // 0: local only, 1: private, 2: public, 3: dedicated
VAR(0, standalone, 1, 0, -1);
#endif
VAR(0, serveruprate, 0, 0, VAR_MAX);
VAR(0, serverport, 1, SERVER_PORT, VAR_MAX);
VAR(0, serverlanport, 0, LAN_PORT, VAR_MAX);
SVAR(0, serverip, "");

bool filterword(char *src, const char *list)
{
    bool filtered = false;
    int len = listlen(list);
    loopi(len)
    {
        char *word = indexlist(list, i);
        if(word)
        {
            if(*word)
            {
                char *t = src;
                int count = strlen(word);
                while((t = strstr(t, word)) != NULL)
                {
                    loopj(count) if(*t) *t++ = '*';
                    filtered = true;
                }
            }
            delete[] word;
        }
    }
    return filtered;
}

ICOMMAND(0, filterword, "ss", (char *s, char *t),
{
    char *d = newstring(s);
    filterword(d, t);
    stringret(d);
});


bool filterstring(char *dst, const char *src, bool newline, bool colour, bool whitespace, bool wsstrip, size_t len)
{
    bool filtered = false;
    size_t n = 0;
    for(int c = uchar(*src); c && n <= len; c = uchar(*++src))
    {
        if(newline && (c=='\n' || c=='\r')) c = ' ';
        if(c=='\f')
        {
            if(!colour) dst[n++] = c;
            else
            {
                filtered = true;
                c = *++src;
                if(!c) break;
                else if(c=='z')
                {
                    c = *++src;
                    if(c) c = *++src;
                    if(!c) break;
                }
                else if(c == '[' || c == '(' || c == '{')
                {
                    const char *end = strchr(src, c == '[' ? ']' : (c == '(' ? ')' : '}'));
                    src += end ? end-src : strlen(src);
                }

            }
            continue;
        }
        if(iscubeprint(c) || (iscubespace(c) && whitespace && (!wsstrip || n)))
            dst[n++] = c;
        else filtered = true;
    }
    if(whitespace && wsstrip && n) while(iscubespace(dst[n-1])) dst[--n] = '\0';
    dst[n <= len ? n : len] = '\0';
    return filtered;
}
ICOMMAND(0, filter, "siiiiN", (char *s, int *a, int *b, int *c, int *d, int *numargs),
{
    size_t len = strlen(s);
    char *d = newstring(len);
    filterstring(d, s, *numargs >= 2 ? *a>0 : true, *numargs >= 3 ? *b>0 : true, *numargs >= 4 ? *c>0 : true, *numargs >= 5 ? *d>0 : false, len);
    stringret(d);
});

vector<clientdata *> clients;

ENetHost *serverhost = NULL;
int laststatus = 0;
ENetSocket pongsock = ENET_SOCKET_NULL, lansock = ENET_SOCKET_NULL;

int localclients = 0, nonlocalclients = 0;

bool hasnonlocalclients() { return nonlocalclients!=0; }
bool haslocalclients() { return localclients!=0; }

void delclient(int n)
{
    if(!clients.inrange(n)) return;
    clientdata *c = clients[n];
    switch(c->type)
    {
        case ST_TCPIP: nonlocalclients--; if(c->peer) c->peer->data = NULL; break;
        case ST_LOCAL: localclients--; break;
        case ST_EMPTY: return;
    }
    c->type = ST_EMPTY;
    c->peer = NULL;
    if(c->info)
    {
        server::deleteinfo(c->info);
        c->info = NULL;
    }
}

int addclient(int type)
{
    clientdata *c = NULL;
    loopv(clients) if(clients[i]->type==ST_EMPTY)
    {
        c = clients[i];
        break;
    }
    if(!c)
    {
        c = new clientdata;
        c->num = clients.length();
        clients.add(c);
    }
    c->info = server::newinfo();
    c->type = type;
    switch(type)
    {
        case ST_TCPIP: nonlocalclients++; break;
        case ST_LOCAL: localclients++; break;
    }
    return c->num;
}

void cleanupserversockets()
{
    if(serverhost) enet_host_destroy(serverhost);
    serverhost = NULL;

    if(pongsock != ENET_SOCKET_NULL) enet_socket_destroy(pongsock);
    if(lansock != ENET_SOCKET_NULL) enet_socket_destroy(lansock);
    pongsock = lansock = ENET_SOCKET_NULL;
}

void cleanupserver()
{
    server::shutdown();
    cleanupserversockets();
    irccleanup();
}

void reloadserver()
{
    loopvrev(control) if(control[i].flag == ipinfo::LOCAL) control.remove(i);
    server::reload();
}

void process(ENetPacket *packet, int sender, int chan);

int getservermtu() { return serverhost ? serverhost->mtu : -1; }
void *getinfo(int i)            { return !clients.inrange(i) || clients[i]->type==ST_EMPTY ? NULL : clients[i]->info; }
const char *gethostname(int i)  { int o = server::peerowner(i); return !clients.inrange(o) || clients[o]->type==ST_EMPTY ? "unknown" : clients[o]->hostname; }
const char *gethostip(int i)    { int o = server::peerowner(i); return !clients.inrange(o) || clients[o]->type==ST_EMPTY ? "0.0.0.0" : clients[o]->hostip; }
int getnumclients()             { return clients.length(); }
uint getclientip(int n)         { int o = server::peerowner(n); return clients.inrange(o) && clients[o]->type==ST_TCPIP ? clients[o]->peer->address.host : 0; }

void sendpacket(int n, int chan, ENetPacket *packet, int exclude)
{
    if(n < 0 || chan < 0) server::recordpacket(abs(chan), packet->data, packet->dataLength);
    if(chan < 0) return; // was just a record packet
    if(n < 0)
    {
        loopv(clients) if(i != server::peerowner(exclude) && server::allowbroadcast(i)) sendpacket(i, chan, packet, exclude);
        return;
    }
    switch(clients[n]->type)
    {
        case ST_REMOTE:
        {
            int owner = server::peerowner(n);
            if(owner >= 0 && clients.inrange(owner) && owner != n && owner != server::peerowner(exclude))
                sendpacket(owner, chan, packet, exclude);
            break;
        }
        case ST_TCPIP:
        {
            enet_peer_send(clients[n]->peer, chan, packet);
            break;
        }
        case ST_LOCAL:
        {
            localservertoclient(chan, packet);
            break;
        }
        default: break;
    }
}

void sendf(int cn, int chan, const char *format, ...)
{
    int exclude = -1;
    bool reliable = false;
    if(*format=='r') { reliable = true; ++format; }
    packetbuf p(MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'x':
            exclude = va_arg(args, int);
            break;
        case 'v':
        {
            int n = va_arg(args, int);
            int *v = va_arg(args, int *);
            loopi(n) putint(p, v[i]);
            break;
        }
        case 'i':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        }
        case 'u':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putuint(p, va_arg(args, uint));
            break;
        }
        case 'f':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putfloat(p, (float)va_arg(args, double));
            break;
        }
        case 's': sendstring(va_arg(args, const char *), p); break;
        case 'm':
        {
            int n = va_arg(args, int);
            p.put(va_arg(args, uchar *), n);
            break;
        }
    }
    va_end(args);
    sendpacket(cn, chan, p.finalize(), exclude);
}

void sendfile(int cn, int chan, stream *file, const char *format, ...)
{
    if(cn < 0)
    {
#ifdef STANDALONE
            return;
#endif
    }
    else if(!clients.inrange(cn)) return;

    int len = file ? (int)min(file->size(), stream::offset(INT_MAX)) : 0;
    if(len > 16<<20) return;

    packetbuf p(MAXTRANS+len, ENET_PACKET_FLAG_RELIABLE);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'l': putint(p, len); break;
        case 'v':
        {
            int n = va_arg(args, int);
            int *v = va_arg(args, int *);
            loopi(n) putint(p, v[i]);
            break;
        }
        case 'i':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        }
        case 'u':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putuint(p, va_arg(args, uint));
            break;
        }
        case 'f':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putfloat(p, (float)va_arg(args, double));
            break;
        }
        case 's': sendstring(va_arg(args, const char *), p); break;
        case 'm':
        {
            int n = va_arg(args, int);
            p.put(va_arg(args, uchar *), n);
            break;
        }
    }
    va_end(args);
    if(file && len > 0)
    {
        file->seek(0, SEEK_SET);
        file->read(p.subbuf(len).buf, len);
    }
    ENetPacket *packet = p.finalize();
    if(cn >= 0) sendpacket(cn, chan, packet, -1);
#ifndef STANDALONE
    else sendclientpacket(packet, chan);
#endif
}

void disconnect_client(int n, int reason)
{
    if(clients[n]->type==ST_TCPIP) enet_peer_disconnect(clients[n]->peer, reason);
    server::clientdisconnect(n, clients[n]->type==ST_LOCAL, reason);
    delclient(n);
    if(clients[n]->type==ST_LOCAL) loopv(clients) if(i != n && clients[i] && clients[i]->type==ST_LOCAL)
    {
        clientdata &c = *clients[i];
        server::clientdisconnect(c.num, true);
        delclient(c.num);
    }
}

void kicknonlocalclients(int reason)
{
    loopv(clients) if(clients[i]->type==ST_TCPIP) disconnect_client(i, reason);
}

void process(ENetPacket *packet, int sender, int chan)  // sender may be -1
{
    packetbuf p(packet);
    server::parsepacket(sender, chan, p);
    if(p.overread()) { disconnect_client(sender, DISC_EOP); return; }
}

void localclienttoserver(int chan, ENetPacket *packet)
{
    clientdata *c = NULL;
    loopv(clients) if(clients[i]->type==ST_LOCAL) { c = clients[i]; break; }
    if(c) process(packet, c->num, chan);
}

#ifndef STANDALONE
VAR(IDF_PERSIST, autoconnect, 0, 0, 1);

void localconnect(bool force)
{
    if(!connected() && (force || autoconnect))
    {
        setsvar("connectname", "");
        setvar("connectport", 0);
        int cn = addclient(ST_LOCAL);
        clientdata &c = *clients[cn];
        c.peer = NULL;
        copystring(c.hostname, "localhost");
        copystring(c.hostip, "127.0.0.1");
        conoutf("\fglocal client %d connected", c.num);
        client::gameconnect(false);
        server::clientconnect(c.num, 0, true);
    }
}

void localdisconnect()
{
    loopv(clients) if(clients[i] && clients[i]->type==ST_LOCAL)
    {
        clientdata &c = *clients[i];
        server::clientdisconnect(c.num, true);
        delclient(c.num);
    }
}
#endif

#ifdef STANDALONE
bool resolverwait(const char *name, ENetAddress *address)
{
    return enet_address_set_host(address, name) >= 0;
}

int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &remoteaddress)
{
    return enet_socket_connect(sock, &remoteaddress);
}
#endif

static ENetSocket mastersock = ENET_SOCKET_NULL;
ENetAddress masteraddress = { ENET_HOST_ANY, ENET_PORT_ANY };
static ENetAddress serveraddress = { ENET_HOST_ANY, ENET_PORT_ANY };
static int masterconnecting = 0, masterconnected = 0;
static vector<char> masterout, masterin;
static int masteroutpos = 0, masterinpos = 0;

void disconnectmaster()
{
    if(mastersock != ENET_SOCKET_NULL)
    {
        server::masterdisconnected();
        enet_socket_destroy(mastersock);
        mastersock = ENET_SOCKET_NULL;
        if(servertype >= 2 && masterconnected) conoutf("disconnected from master server");
    }

    masterout.setsize(0);
    masterin.setsize(0);
    masteroutpos = masterinpos = 0;

    masteraddress.host = ENET_HOST_ANY;
    masteraddress.port = ENET_PORT_ANY;
    masterconnecting = masterconnected = 0;
}

VARF(0, servermasterport, 1, MASTER_PORT, INT_MAX-1, disconnectmaster());
SVARF(0, servermaster, "", disconnectmaster());

ENetSocket connectmaster(bool reuse)
{
    if(reuse && mastersock != ENET_SOCKET_NULL) return mastersock;
    if(!servermaster[0]) return ENET_SOCKET_NULL;
    if(masteraddress.host == ENET_HOST_ANY)
    {
        if(servertype >= 2) conoutf("\falooking up %s:[%d]...", servermaster, servermasterport);
        masteraddress.port = servermasterport;
        if(!resolverwait(servermaster, &masteraddress))
        {
            conoutf("\frfailed resolving %s:[%d]", servermaster, servermasterport);
            return ENET_SOCKET_NULL;
        }
    }
    ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(sock == ENET_SOCKET_NULL)
    {
        conoutf("\frcould not open master server socket");
        return ENET_SOCKET_NULL;
    }
    if(!reuse || serveraddress.host == ENET_HOST_ANY || !enet_socket_bind(sock, &serveraddress))
    {
        enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
        if(!reuse)
        {
            if(!connectwithtimeout(sock, servermaster, masteraddress)) return sock;
        }
        else if(!enet_socket_connect(sock, &masteraddress))
        {
            masterconnecting = totalmillis ? totalmillis : 1;
            mastersock = sock;
            return sock;
        }
    }
    enet_socket_destroy(sock);
    conoutf("\frcould not connect to master server");
    return ENET_SOCKET_NULL;
}

bool connectedmaster() { return mastersock != ENET_SOCKET_NULL; }

bool requestmaster(const char *req)
{
    if(mastersock == ENET_SOCKET_NULL) return false;
    if(masterout.length() >= 4096) return false;
    masterout.put(req, strlen(req));
    return true;
}

bool requestmasterf(const char *fmt, ...)
{
    defvformatstring(req, fmt, fmt);
    return requestmaster(req);
}

void processmasterinput()
{
    if(masterinpos >= masterin.length()) return;

    char *input = &masterin[masterinpos], *end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
    while(end)
    {
        *end++ = '\0';

        const char *args = input;
        while(args < end && !iscubespace(*args)) args++;
        int cmdlen = args - input;
        while(args < end && iscubespace(*args)) args++;

        server::processmasterinput(input, cmdlen, args);

        masterinpos = end - masterin.getbuf();
        input = end;
        end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
    }

    if(masterinpos >= masterin.length())
    {
        masterin.setsize(0);
        masterinpos = 0;
    }
}

void flushmasteroutput()
{
    if(masterconnecting && totalmillis - masterconnecting >= 60000)
    {
        conoutf("\frcould not connect to master server");
        disconnectmaster();
    }
    if(masterout.empty() || !masterconnected) return;

    ENetBuffer buf;
    buf.data = &masterout[masteroutpos];
    buf.dataLength = masterout.length() - masteroutpos;
    int sent = enet_socket_send(mastersock, NULL, &buf, 1);
    if(sent >= 0)
    {
        masteroutpos += sent;
        if(masteroutpos >= masterout.length())
        {
            masterout.setsize(0);
            masteroutpos = 0;
        }
    }
    else disconnectmaster();
}

void flushmasterinput()
{
    if(masterin.length() >= masterin.capacity())
        masterin.reserve(4096);

    ENetBuffer buf;
    buf.data = masterin.getbuf() + masterin.length();
    buf.dataLength = masterin.capacity() - masterin.length();
    int recv = enet_socket_receive(mastersock, NULL, &buf, 1);
    if(recv > 0)
    {
        masterin.advance(recv);
        processmasterinput();
    }
    else disconnectmaster();
}

static ENetAddress pongaddr;

void sendqueryreply(ucharbuf &p)
{
    ENetBuffer buf;
    buf.data = p.buf;
    buf.dataLength = p.length();
    enet_socket_send(pongsock, &pongaddr, &buf, 1);
}

#define MAXPINGDATA 32

void checkserversockets()        // reply all server info requests
{
    static ENetSocketSet readset, writeset;
    ENET_SOCKETSET_EMPTY(readset);
    ENET_SOCKETSET_EMPTY(writeset);
    ENetSocket maxsock = ENET_SOCKET_NULL;
#define ADDSOCKET(sock, write) \
    if(sock != ENET_SOCKET_NULL) \
    { \
        maxsock = maxsock == ENET_SOCKET_NULL ? sock : max(maxsock, sock); \
        ENET_SOCKETSET_ADD(readset, sock); \
        if(write) ENET_SOCKETSET_ADD(writeset, sock); \
    }
    ADDSOCKET(pongsock, false);
    ADDSOCKET(mastersock, true);
    ADDSOCKET(lansock, false);
    bool ircsocks = ircaddsockets(maxsock, readset, writeset);
    if(maxsock == ENET_SOCKET_NULL || enet_socketset_select(maxsock, &readset, &writeset, 0) <= 0) return;

    if(serverhost)
    {
        ENetBuffer buf;
        uchar pong[MAXTRANS];
        loopi(2)
        {
            ENetSocket sock = i ? lansock : pongsock;
            if(sock == ENET_SOCKET_NULL || !ENET_SOCKETSET_CHECK(readset, sock)) continue;

            buf.data = pong;
            buf.dataLength = sizeof(pong);
            int len = enet_socket_receive(sock, &pongaddr, &buf, 1);
            if(len < 0 || len > MAXPINGDATA) continue;
            ucharbuf req(pong, len), p(pong, sizeof(pong));
            p.len += len;
            server::queryreply(req, p);
        }
    }

    if(mastersock != ENET_SOCKET_NULL)
    {
        if(!masterconnected)
        {
            if(ENET_SOCKETSET_CHECK(readset, mastersock) || ENET_SOCKETSET_CHECK(writeset, mastersock))
            {
                int error = 0;
                if(enet_socket_get_option(mastersock, ENET_SOCKOPT_ERROR, &error) < 0 || error) disconnectmaster();
                else
                {
                    masterconnecting = 0;
                    masterconnected = totalmillis ? totalmillis : 1;
                    server::masterconnected();
                }
            }
        }
        if(mastersock != ENET_SOCKET_NULL && ENET_SOCKETSET_CHECK(readset, mastersock)) flushmasterinput();
    }

    if(ircsocks) ircchecksockets(readset, writeset);
}

void serverslice(uint timeout)  // main server update, called from main loop in sp, or from below in dedicated server
{
    server::serverupdate();

    flushmasteroutput();
    checkserversockets();

    if(!serverhost)
    {
        server::sendpackets();
        return;
    }

    if(servertype >= 2 && totalmillis-laststatus >= 60000)  // display bandwidth stats, useful for server ops
    {
        laststatus = totalmillis;
        if(serverhost->totalSentData || serverhost->totalReceivedData || server::numclients())
            conoutf("status: %d clients, %.1f send, %.1f rec (K/sec)\n", server::numclients(), serverhost->totalSentData/60.0f/1024, serverhost->totalReceivedData/60.0f/1024);
        serverhost->totalSentData = serverhost->totalReceivedData = 0;
    }

    ENetEvent event;
    bool serviced = false;
    while(!serviced)
    {
        if(enet_host_check_events(serverhost, &event) <= 0)
        {
            if(enet_host_service(serverhost, &event, timeout) <= 0) break;
            serviced = true;
        }
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                int cn = addclient(ST_TCPIP);
                clientdata &c = *clients[cn];
                c.peer = event.peer;
                c.peer->data = &c;
                if(enet_address_get_host_ip(&c.peer->address, c.hostip, sizeof(c.hostip)) >= 0)
                {
                    if(enet_address_get_host(&c.peer->address, c.hostname, sizeof(c.hostname)) >= 0)
                    {
                        ENetAddress address;
                        string hostname;
                        if(enet_address_set_host(&address, c.hostname) < 0 || enet_address_get_host_ip(&address, hostname, sizeof(hostname)) < 0 || strcmp(hostname, c.hostname))
                            copystring(c.hostname, c.hostip);
                    }
                    else copystring(c.hostname, c.hostip);
                    int reason = server::clientconnect(c.num, c.peer->address.host);
                    if(reason) disconnect_client(c.num, reason);
                }
                else disconnect_client(c.num, DISC_HOSTFAIL);
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                clientdata *c = (clientdata *)event.peer->data;
                if(c) process(event.packet, c->num, event.channelID);
                if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                clientdata *c = (clientdata *)event.peer->data;
                if(!c) break;
                server::clientdisconnect(c->num);
                delclient(c->num);
                break;
            }
            default:
                break;
        }
    }
    if(server::sendpackets()) enet_host_flush(serverhost);
}

void flushserver(bool force)
{
    if(server::sendpackets(force) && serverhost) enet_host_flush(serverhost);
}

#ifndef STANDALONE
int clockrealbase = 0, clockvirtbase = 0;
void clockreset() { clockrealbase = SDL_GetTicks(); clockvirtbase = totalmillis; }
VARF(IDF_PERSIST, clockerror, 990000, 1000000, 1010000, clockreset());
VARF(IDF_PERSIST, clockfix, 0, 0, 1, clockreset());
#endif

int getclockmillis()
{
#ifdef STANDALONE
    return (int)enet_time_get();
#else
    int millis = SDL_GetTicks() - clockrealbase;
    if(clockfix) millis = int(millis*(double(clockerror)/1000000));
    millis += clockvirtbase;
    return max(millis, totalmillis);
#endif
}

int updatetimer(bool limit)
{
    currenttime = time(NULL);
    clocktime = mktime(gmtime(&currenttime));
    clockoffset = currenttime-clocktime;

    int millis = getclockmillis();
#ifndef STANDALONE
    if(limit) limitfps(millis, totalmillis);
#endif
    int elapsed = millis-totalmillis;
    if(paused) curtime = 0;
    else if(timescale != 100)
    {
        int scaledtime = elapsed*timescale + timeerr;
        curtime = scaledtime/100;
        timeerr = scaledtime%100;
    }
    else
    {
        curtime = elapsed + timeerr;
        timeerr = 0;
    }
#ifndef STANDALONE
    if(limit && curtime > 200 && !connected(false, false) && !hasnonlocalclients()) curtime = 200;
#endif
    lastmillis += curtime;
    totalmillis = millis;
    static int lastsec = 0;
    if(totalmillis-lastsec >= 1000)
    {
        int cursecs = (totalmillis-lastsec)/1000;
        totalsecs += cursecs;
        lastsec += cursecs*1000;
        ifserver(maxruntime && !shutdownwait && int(totalsecs) >= maxruntime)
        {
            server::srvoutf(-3, "\fymax run time reached (\fs\fc%s\fS), waiting for server to empty", timestr(maxruntime*1000, 4));
            shutdownwait = totalmillis;
        }
    }
    return elapsed;
}

#ifdef WIN32
#include "shellapi.h"

#define IDI_ICON1 1

static string apptip = "";
static HINSTANCE appinstance = NULL;
static ATOM wndclass = 0;
static HWND appwindow = NULL, conwindow = NULL;
static HICON appicon = NULL;
static HMENU appmenu = NULL;
static HANDLE outhandle = NULL;
static const int MAXLOGLINES = 200;
struct logline { int len; char buf[LOGSTRLEN]; };
static queue<logline, MAXLOGLINES> loglines;

static void cleanupsystemtray()
{
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = appwindow;
    nid.uID = IDI_ICON1;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

static bool setupsystemtray(UINT uCallbackMessage)
{
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = appwindow;
    nid.uID = IDI_ICON1;
    nid.uCallbackMessage = uCallbackMessage;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.hIcon = appicon;
    strcpy(nid.szTip, apptip);
    if(Shell_NotifyIcon(NIM_ADD, &nid) != TRUE)
        return false;
    atexit(cleanupsystemtray);
    return true;
}

#if 0
static bool modifysystemtray()
{
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = appwindow;
    nid.uID = IDI_ICON1;
    nid.uFlags = NIF_TIP;
    strcpy(nid.szTip, apptip);
    return Shell_NotifyIcon(NIM_MODIFY, &nid) == TRUE;
}
#endif

static void cleanupwindow()
{
    if(!appinstance) return;
    if(appmenu)
    {
        DestroyMenu(appmenu);
        appmenu = NULL;
    }
    if(wndclass)
    {
        UnregisterClass(MAKEINTATOM(wndclass), appinstance);
        wndclass = 0;
    }
}

static BOOL WINAPI consolehandler(DWORD dwCtrlType)
{
    switch(dwCtrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            exit(EXIT_SUCCESS);
            return TRUE;
    }
    return FALSE;
}

static void writeline(logline &line)
{
    static uchar ubuf[512];
    size_t len = strlen(line.buf), carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf), &((uchar *)line.buf)[carry], len - carry, &carry);
        DWORD written = 0;
        WriteConsole(outhandle, ubuf, numu, &written, NULL);
    }
}

static void setupconsole()
{
    if(conwindow) return;
    if(!AllocConsole()) return;
    SetConsoleCtrlHandler(consolehandler, TRUE);
    conwindow = GetConsoleWindow();
    SetConsoleTitle(apptip);
    //SendMessage(conwindow, WM_SETICON, ICON_SMALL, (LPARAM)appicon);
    SendMessage(conwindow, WM_SETICON, ICON_BIG, (LPARAM)appicon);
    outhandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(outhandle, &coninfo);
    coninfo.dwSize.Y = MAXLOGLINES;
    SetConsoleScreenBufferSize(outhandle, coninfo.dwSize);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    loopv(loglines) writeline(loglines[i]);
}

enum
{
    MENU_OPENCONSOLE = 0,
    MENU_SHOWCONSOLE,
    MENU_HIDECONSOLE,
    MENU_EXIT
};

static LRESULT CALLBACK handlemessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_APP:
            SetForegroundWindow(hWnd);
            switch(lParam)
            {
                //case WM_MOUSEMOVE:
                //  break;
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                {
                    POINT pos;
                    GetCursorPos(&pos);
                    TrackPopupMenu(appmenu, TPM_CENTERALIGN|TPM_BOTTOMALIGN|TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
                    PostMessage(hWnd, WM_NULL, 0, 0);
                    break;
                }
            }
            return 0;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case MENU_OPENCONSOLE:
                    setupconsole();
                    if(conwindow) ModifyMenu(appmenu, 0, MF_BYPOSITION|MF_STRING, MENU_HIDECONSOLE, "Hide Console");
                    break;
                case MENU_SHOWCONSOLE:
                    ShowWindow(conwindow, SW_SHOWNORMAL);
                    ModifyMenu(appmenu, 0, MF_BYPOSITION|MF_STRING, MENU_HIDECONSOLE, "Hide Console");
                    break;
                case MENU_HIDECONSOLE:
                    ShowWindow(conwindow, SW_HIDE);
                    ModifyMenu(appmenu, 0, MF_BYPOSITION|MF_STRING, MENU_SHOWCONSOLE, "Show Console");
                    break;
                case MENU_EXIT:
                    PostMessage(hWnd, WM_CLOSE, 0, 0);
                    break;
            }
            return 0;
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void setupwindow(const char *title)
{
    copystring(apptip, title);
    //appinstance = GetModuleHandle(NULL);
    if(!appinstance) fatal("failed getting application instance");
    appicon = LoadIcon(appinstance, MAKEINTRESOURCE(IDI_ICON1));//(HICON)LoadImage(appinstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if(!appicon) fatal("failed loading icon");

    appmenu = CreatePopupMenu();
    if(!appmenu) fatal("failed creating popup menu");
    AppendMenu(appmenu, MF_STRING, MENU_OPENCONSOLE, "Open Console");
    AppendMenu(appmenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(appmenu, MF_STRING, MENU_EXIT, "Exit");
    //SetMenuDefaultItem(appmenu, 0, FALSE);

    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = appicon;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = title;
    wc.style = 0;
    wc.hInstance = appinstance;
    wc.lpfnWndProc = handlemessages;
    wc.cbWndExtra = 0;
    wc.cbClsExtra = 0;
    wndclass = RegisterClass(&wc);
    if(!wndclass) fatal("failed registering window class");

    appwindow = CreateWindow(MAKEINTATOM(wndclass), title, 0, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, HWND_MESSAGE, NULL, appinstance, NULL);
    if(!appwindow) fatal("failed creating window");

    atexit(cleanupwindow);

    if(!setupsystemtray(WM_APP)) fatal("failed adding to system tray");
    conoutf("identity: v%s-%s%d %s [%s] (%s) [0x%.8x]", VERSION_STRING, versionplatname, versionarch, versionisserver ? "server" : "client", versionbranch, VERSION_RELEASE, versioncrc);
}

static char *parsecommandline(const char *src, vector<char *> &args)
{
    char *buf = new char[strlen(src) + 1], *dst = buf;
    for(;;)
    {
        while(isspace(*src)) src++;
        if(!*src) break;
        args.add(dst);
         for(bool quoted = false; *src && (quoted || !isspace(*src)); src++)
        {
            if(*src != '"') *dst++ = *src;
            else if(dst > buf && src[-1] == '\\') dst[-1] = '"';
            else quoted = !quoted;
        }
        *dst++ = '\0';
    }
    args.add(NULL);
    return buf;
}


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    vector<char *> args;
    char *buf = parsecommandline(GetCommandLine(), args);
    appinstance = hInst;
#ifdef STANDALONE
    int standalonemain(int argc, char **argv);
    int status = standalonemain(args.length()-1, args.getbuf());
    #define main standalonemain
#else
    SDL_SetModuleHandle(hInst);
    int status = SDL_main(args.length()-1, args.getbuf());
#endif
    delete[] buf;
    exit(status);
    return 0;
}

void logoutfv(const char *fmt, va_list args)
{
    if(appwindow)
    {
        logline &line = loglines.add();
        vformatstring(line.buf, fmt, args, sizeof(line.buf));
        if(logfile) writelog(logfile, line.buf);
        line.len = min(strlen(line.buf), sizeof(line.buf)-2);
        line.buf[line.len++] = '\n';
        line.buf[line.len] = '\0';
        if(outhandle) writeline(line);
    }
    else if(logfile) writelogv(logfile, fmt, args);
}

#else

void logoutfv(const char *fmt, va_list args)
{
    FILE *f = getlogfile();
    if(f) writelogv(f, fmt, args);
}

#endif

void serverloop()
{
#ifdef WIN32
    defformatstring(cap, "%s server", VERSION_NAME);
    setupwindow(cap);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
    conoutf("\fgdedicated server started, waiting for clients...");
    for(;;)
    {
        //int _lastmillis = lastmillis;
        //lastmillis = totalmillis = (int)enet_time_get();
        //curtime = lastmillis-_lastmillis;
        updatetimer(false);
        checkmaster();
        serverslice(5);
        ircslice();
#ifdef WIN32
        MSG msg;
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT) exit(EXIT_SUCCESS);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#endif
    }
    exit(EXIT_SUCCESS);
}

void limitdupclients()
{
    if(!serverhost) return;
    int dupclients = server::dupclients();
    serverhost->duplicatePeers = dupclients ? dupclients : serverhost->peerCount;
}

int setupserversockets()
{
    if(!servertype || (serverhost && pongsock != ENET_SOCKET_NULL)) return servertype;

    ENetAddress address = { ENET_HOST_ANY, enet_uint16(serverport) };
    if(*serverip)
    {
        if(enet_address_set_host(&address, serverip) < 0)
        {
            setsvar("serverip", "");
            conoutf("\frWARNING: server address not resolved");
        }
        else serveraddress.host = address.host;
    }

    if(!serverhost)
    {
        serverhost = enet_host_create(&address, server::reserveclients(), server::numchannels(), 0, serveruprate);
        if(!serverhost)
        {
#ifdef STANDALONE
            fatal("could not create server socket on port %d", serverport);
#else
            conoutf("\frcould not create server socket on port %d", serverport);
            setvar("servertype", 0);
#endif
            return servertype;
        }
        limitdupclients();
    }

    if(pongsock == ENET_SOCKET_NULL)
    {
        address.port = serverport+1;
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pongsock != ENET_SOCKET_NULL && enet_socket_bind(pongsock, &address) < 0)
        {
            enet_socket_destroy(pongsock);
            pongsock = ENET_SOCKET_NULL;
        }
        if(pongsock == ENET_SOCKET_NULL)
        {
#ifdef STANDALONE
            fatal("could not create server info socket on port %d", serverport+1);
#else
            conoutf("\frcould not create server info socket on port %d, publicity disabled", serverport+1);
            setvar("servertype", 1);
#endif
            return servertype;
        }
        enet_socket_set_option(pongsock, ENET_SOCKOPT_NONBLOCK, 1);
        if(serverlanport)
        {
            address.port = LAN_PORT;
            lansock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
            if(lansock != ENET_SOCKET_NULL && (enet_socket_set_option(lansock, ENET_SOCKOPT_REUSEADDR, 1) < 0 || enet_socket_bind(lansock, &address) < 0))
            {
                enet_socket_destroy(lansock);
                lansock = ENET_SOCKET_NULL;
            }
            if(lansock == ENET_SOCKET_NULL) conoutf("\frcould not create LAN server info socket");
            else enet_socket_set_option(lansock, ENET_SOCKOPT_NONBLOCK, 1);
        }
    }

    return servertype;
}

void changeservertype()
{
    if(!servertype)
    {
        kicknonlocalclients(DISC_PRIVATE);
        cleanupserversockets();
    }
    else setupserversockets();
}

void setupserver()
{
    server::changemap(load && *load ? load : NULL);
    if(!servertype) return;
    setupmaster();
    conoutf("loading server (%s:%d)..", *serverip ? serverip : "*", serverport);
    if(setupserversockets() && verbose) conoutf("game server started");
#ifndef STANDALONE
    if(servertype >= 3) serverloop();
#endif
}

void initgame()
{
    conoutf("identity: v%s-%s%d %s [%s] (%s) [0x%.8x]", VERSION_STRING, versionplatname, versionarch, versionisserver ? "server" : "client", versionbranch, VERSION_RELEASE, versioncrc);
    server::start();
    loopv(gameargs)
    {
#ifndef STANDALONE
        if(game::clientoption(gameargs[i])) continue;
#endif
        if(server::serveroption(gameargs[i])) continue;
        conoutf("\frunknown command-line option: %s", gameargs[i]);
    }
#ifdef STANDALONE
    rehash(false);
#endif
    setupserver();
}

VAR(0, hasoctapaks, 1, 0, 0); // mega hack; try to find Cube 2, done after our own data so as to not clobber stuff

void octadirchanged()
{
#ifdef STANDALONE
    if(rehashing) trytofindocta();
#else
    if(initing == NOT_INITING) trytofindocta();
#endif
}

SVARF(IDF_PERSIST, octadir, "", octadirchanged());

bool serveroption(char *opt)
{
    switch(opt[1])
    {
        case 'h': sethomedir(&opt[2]); logoutf("set home directory: %s", &opt[2]); return true;
        case 'o': setsvar("octadir", &opt[2]); return true;
        case 'p': addpackagedir(&opt[2]); logoutf("add package directory: %s", &opt[2]); return true;
        case 'g': setlogfile(&opt[2]); logoutf("set log file: %s", opt[2] ? &opt[2] : "<stdout>"); return true;
        case 'v': setvar("verbose", atoi(opt+2)); return true;
        case 's':
        {
            switch(opt[2])
            {
                case 'u': setvar("serveruprate", atoi(opt+3)); return true;
                case 'i': setsvar("serverip", opt+3); return true;
                case 'm': setsvar("servermaster", opt+3); return true;
                case 'l': load = opt+3; return true;
                case 's': setvar("servertype", atoi(opt+3)); return true;
                case 'p': setvar("serverport", atoi(opt+3)); return true;
                case 'a': setvar("servermasterport", atoi(opt+3)); return true;
                case 'g': setvar("logtimelocal", atoi(opt+3)); return true;
                default: return false;
            }
        }
        case 'm':
        {
            switch(opt[2])
            {
                case 's': setvar("masterserver", atoi(opt+3)); return true;
                case 'i': setsvar("masterip", opt+3); return true;
                case 'p': setvar("masterport", atoi(opt+3)); return true;
                default: return false;
            }
            return false;
        }
        default: return false;
    }
    return false;
}

bool findoctadir(const char *name, bool fallback)
{
    string s = "";
    copystring(s, name);
    path(s);
    defformatstring(octadefs, "%s/data/default_map_settings.cfg", s);
    if(fileexists(findfile(octadefs, "r"), "r"))
    {
        conoutf("\fwfound octa directory: %s", s);
        defformatstring(octadata, "%s/data", s);
        defformatstring(octapaks, "%s/packages", s);
        addpackagedir(s, PACKAGEDIR_OCTA);
        addpackagedir(octadata, PACKAGEDIR_OCTA);
        addpackagedir(octapaks, PACKAGEDIR_OCTA);
        maskpackagedirs(~PACKAGEDIR_OCTA);
        hasoctapaks = fallback ? 1 : 2;
        return true;
    }
    hasoctapaks = 0;
    return false;
}
void octapaks(uint *contents)
{
    int mask = maskpackagedirs(~0);
    execute(contents);
    maskpackagedirs(mask);
}
COMMAND(0, octapaks, "e");

void trytofindocta(bool fallback)
{
    if(!octadir || !*octadir)
    {
        const char *dir = getenv("OCTA_DATA");
        if(dir && *dir) setsvar("octadir", dir, false);
        else
        {
            dir = getenv("OCTA_DIR"); // backward compat
            if(dir && *dir) setsvar("octadir", dir, false);
        }
    }
    if((!octadir || !*octadir || !findoctadir(octadir, false)) && fallback)
    { // user hasn't specifically set it, try some common locations alongside our folder
#if defined(WIN32)
        string dir = "";
        if(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, dir) == S_OK)
        {
            defformatstring(s, "%s\\Sauerbraten", dir);
            if(findoctadir(s, true)) return;
        }
#elif defined(__APPLE__)
        extern const char *mac_sauerbratendir();
        const char *dir = mac_sauerbratendir();
        if(dir && findoctadir(dir, true)) return;
#endif
        const char *tryoctadirs[4] = { // by no means an accurate or definitive list either..
            "../Sauerbraten", "../sauerbraten", "../sauer",
#if defined(WIN32)
            "/Program Files/Sauerbraten"
#elif defined(__APPLE__)
            "/Applications/sauerbraten.app/Contents/gamedata"
#else
            "/usr/games/sauerbraten"
#endif
        };
        loopi(4) if(findoctadir(tryoctadirs[i], true)) return;
    }
}

void setlocations(bool wanthome)
{
    int backstep = 3;
    loopirev(backstep) if(!fileexists(findfile("config/version.cfg", "r"), "r"))
    { // standalone solution to this is: pebkac
        if(!i || chdir("..") < 0) fatal("could not find config directory");
    }
    if(!execfile("config/version.cfg", false, EXEC_VERSION|EXEC_BUILTIN)) fatal("cannot exec 'config/version.cfg'");
    // pseudo directory with game content
    const char *dir = getenv("GAME_DATA");
    if(dir && *dir) addpackagedir(dir);
    else addpackagedir("data");
    if(!fileexists(findfile("maps/readme.txt", "r"), "r")) fatal("could not find game data");
    if(wanthome)
    {
#if defined(WIN32)
        string dir = "";
        if(SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, dir) == S_OK)
        {
            defformatstring(s, "%s\\My Games\\%s", dir, VERSION_NAME);
            sethomedir(s);
        }
#elif defined(__APPLE__)
        extern const char *mac_personaldir();
        const char *dir = mac_personaldir(); // typically  /Users/<name>/Application Support/
        if(dir && *dir)
        {
            defformatstring(s, "%s/%s", dir, VERSION_NAME);
            sethomedir(s);
        }
#else
        const char *dir = getenv("HOME");
        if(dir && *dir)
        {
            defformatstring(s, "%s/.%s", dir, VERSION_UNAME);
            sethomedir(s);
        }
#endif
        else sethomedir("home");
    }
}

void writecfg()
{
#ifndef STANDALONE
    stream *f = openutf8file("config.cfg", "w");
    if(!f) return;
    vector<ident *> ids;
    enumerate(idents, ident, id, ids.add(&id));
    ids.sort(ident::compare);
    bool found = false;
    loopv(ids)
    {
        ident &id = *ids[i];
        if(id.flags&IDF_PERSIST) switch(id.type)
        {
            case ID_VAR: if(*id.storage.i != id.def.i) { found = true; f->printf("%s %s\n", escapeid(id), intstr(&id)); } break;
            case ID_FVAR: if(*id.storage.f != id.def.f) { found = true; f->printf("%s %s\n", escapeid(id), floatstr(*id.storage.f)); } break;
            case ID_SVAR: if(strcmp(*id.storage.s, id.def.s)) { found = true; f->printf("%s %s\n", escapeid(id), escapestring(*id.storage.s)); } break;
        }
    }
    if(found) f->printf("\n");
    found = false;
    loopv(ids)
    {
        ident &id = *ids[i];
        if(id.flags&IDF_PERSIST) switch(id.type)
        {
            case ID_ALIAS:
            {
                const char *str = id.getstr();
                found = true;
                if(validateblock(str)) f->printf("%s = [%s]\n", escapeid(id), str);
                else f->printf("%s = %s\n", escapeid(id), escapestring(str));
            }
            break;
        }
    }
    if(found) f->printf("\n");
    writebinds(f);
    delete f;
#endif
}

ICOMMAND(0, writecfg, "", (void), if(!(identflags&IDF_WORLD)) writecfg());

void rehash(bool reload)
{
    if(reload)
    {
        rehashing = 1;
        writecfg();
    }
    reloadserver();
    reloadmaster();
#ifdef STANDALONE
    execfile("servinit.cfg", false, EXEC_VERSION);
#else
    if(servertype >= 3) execfile("servinit.cfg", false, EXEC_VERSION);
    else execfile("localinit.cfg", false, EXEC_VERSION);
    initing = INIT_DEFAULTS;
    execfile("config/defaults.cfg");
    initing = INIT_LOAD;
    interactive = true;
    execfile("servers.cfg", false);
    execfile("config.cfg", false);
    execfile("autoexec.cfg", false);
    interactive = false;
    initing = NOT_INITING;
#endif
    conoutf("\fwconfiguration %s", reload ? "reloaded" : "loaded");
    rehashing = 0;
}
ICOMMAND(0, rehash, "i", (int *nosave), if(!(identflags&IDF_WORLD)) rehash(*nosave ? false : true));

void setverinfo(const char *bin)
{
    setvar("versioncrc", crcfile(bin));
    const char *branch = getenv("REDECLIPSE_BRANCH");
    setsvar("versionbranch", branch);
}

volatile bool fatalsig = false;
void fatalsignal(int signum)
{
    if(!fatalsig)
    {
        fatalsig = true;
        const char *str = "";
        switch(signum)
        {
            case SIGINT: str = "Exit signal %d (Interrupt)"; break;
            case SIGILL: str = "Fatal signal %d (Illegal Instruction)"; break;
            case SIGABRT: str = "Fatal signal %d (Aborted)"; break;
            case SIGFPE: str = "Fatal signal %d (Floating-point Exception)"; break;
            case SIGSEGV: str = "Fatal signal %d (Segmentation Violation)"; break;
            case SIGTERM: str = "Exit signal %d (Terminated)"; break;
#if !defined(WIN32) && !defined(__APPLE__)
            case SIGQUIT: str = "Exit signal %d (Quit)"; break;
            case SIGKILL: str = "Fatal signal %d (Killed)"; break;
            case SIGPIPE: str = "Fatal signal %d (Broken Pipe)"; break;
            case SIGALRM: str = "Fatal signal %d (Alarm)"; break;
#endif
            default: str = "Error: Fatal signal %d (Unknown Error)"; break;
        }
        fatal(str, signum);
    }
}

void reloadsignal(int signum)
{
    rehash(true);
}

void shutdownsignal(int signum)
{
#ifndef STANDALONE
    if(servertype < 3) fatalsignal(signum);
#endif
    server::srvoutf(-3, "\fyshutdown signal received, waiting for server to empty");
    shutdownwait = totalmillis;
}

#ifdef STANDALONE
volatile int errors = 0;
void fatal(const char *s, ...)    // failure exit
{
    if(++errors <= 2) // print up to one extra recursive error
    {
        defvformatbigstring(msg, s, s);
        if(logfile) logoutf("%s", msg);
#ifndef WIN32
        fprintf(stderr, "Exiting: %s\n", msg);
#endif
        if(errors <= 1) // avoid recursion
        {
            cleanupserver();
            cleanupmaster();
            enet_deinitialize();
#ifdef WIN32
            defformatstring(cap, "%s: Error", VERSION_NAME);
            MessageBox(NULL, msg, cap, MB_OK|MB_SYSTEMMODAL);
#endif
        }
    }
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    currenttime = time(NULL); // initialise
    clocktime = mktime(gmtime(&currenttime));
    clockoffset = currenttime-clocktime;

    setlogfile(NULL);
    setlocations(true);
    setverinfo(argv[0]);

    char *initscript = NULL;
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'x': initscript = &argv[i][2]; break;
            default: if(!serveroption(argv[i])) gameargs.add(argv[i]); break;
        }
        else gameargs.add(argv[i]);
    }

    if(enet_initialize()<0) fatal("Unable to initialise network module");

    signal(SIGINT, fatalsignal);
    signal(SIGILL, fatalsignal);
    signal(SIGABRT, fatalsignal);
    signal(SIGFPE, fatalsignal);
    signal(SIGSEGV, fatalsignal);
    signal(SIGTERM, shutdownsignal);
    signal(SIGINT, fatalsignal);
#if !defined(WIN32) && !defined(__APPLE__)
    signal(SIGHUP, reloadsignal);
    signal(SIGQUIT, fatalsignal);
    signal(SIGKILL, fatalsignal);
    signal(SIGPIPE, fatalsignal);
    signal(SIGALRM, fatalsignal);
    signal(SIGSTOP, fatalsignal);
#endif
    enet_time_set(0);
    initgame();
    trytofindocta();
    if(initscript) execute(initscript);
    serverloop();
    return 0;
}
#endif
