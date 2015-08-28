enum { IRCC_NONE = 0, IRCC_JOINING, IRCC_JOINED, IRCC_KICKED, IRCC_BANNED };
enum { IRCCT_NONE = 0, IRCCT_AUTO };
#ifndef STANDALONE
enum { IRCUP_NEW = 1<<0, IRCUP_MSG = 1<<1, IRCUP_LEAVE = 1<<2 };
#define MAXIRCLINES 100
struct ircbuf
{
    int newlines;
    vector<char *> lines;

    void reset()
    {
        lines.deletearrays();
        newlines = 0;
    }

    void choplines(int limit)
    {
        while(lines.length() >= limit)
        {
            delete[] lines.remove(0);
            newlines = min(newlines, lines.length());
        }
    }

    void addline(const char *str, int limit = -1)
    {
        if(limit >= 0) choplines(limit);
        lines.add(newstring(str));
    }
};
#endif
struct ircchan
{
    int state, type, relay, lastjoin, lastsync;
    string name, friendly, passkey;
#ifndef STANDALONE
    int updated;
    ircbuf buffer;
#endif
    ircchan() { reset(); }
    ~ircchan() { reset(); }

    void reset()
    {
        state = IRCC_NONE;
        type = IRCCT_NONE;
        relay = lastjoin = lastsync = 0;
        name[0] = friendly[0] = passkey[0] = '\0';
#ifndef STANDALONE
        updated = 0;
        buffer.reset();
#endif
    }
};
enum { IRCT_NONE = 0, IRCT_CLIENT, IRCT_RELAY, IRCT_MAX };
enum { IRC_NEW = 0, IRC_DISC, IRC_WAIT, IRC_ATTEMPT, IRC_CONN, IRC_ONLINE, IRC_QUIT, IRC_MAX };
struct ircnet
{
    int type, state, port, lastattempt, lastactivity, lastping, lastnick, inputcarry, inputlen;
    string name, serv, mnick, nick, ip, passkey, authcommand, authname, authpass;
    ENetAddress address;
    ENetSocket sock;
    vector<ircchan> channels;
    uchar input[4096];
#ifndef STANDALONE
    int updated, lastseen, away;
    ircbuf buffer;
#endif

    ircnet() { reset(true); }
    ~ircnet() { reset(true); }

    void reset(bool start = false)
    {
        if(start)
        {
            type = IRCT_NONE;
            state = IRC_NEW;
            sock = ENET_SOCKET_NULL;
            address.host = ENET_HOST_ANY;
            address.port = 6667;
        }
        else state = IRC_DISC;
        inputcarry = inputlen = 0;
        port = lastattempt = lastactivity = lastping = lastnick = 0;
        name[0] = serv[0] = mnick[0] = nick[0] = ip[0] = passkey[0] = authcommand[0] = authname[0] = authpass[0] = '\0';
        channels.shrink(0);
#ifndef STANDALONE
        updated = IRCUP_NEW;
        lastseen = clocktime;
        away = 0;
        buffer.reset();
#endif
    }
};

extern vector<ircnet *> ircnets;

extern ircnet *ircfind(const char *name);
extern void ircestablish(ircnet *n);
extern void ircsend(ircnet *n, const char *msg, ...);
extern void ircoutf(int relay, const char *msg, ...);
extern int ircrecv(ircnet *n);
extern void ircnewnet(int type, const char *name, const char *serv, int port, const char *nick, const char *ip = "", const char *passkey = "");
extern ircchan *ircfindchan(ircnet *n, const char *name);
extern bool ircjoin(ircnet *n, ircchan *c);
extern bool ircenterchan(ircnet *n, const char *name);
extern bool ircnewchan(int type, const char *name, const char *channel, const char *friendly = "", const char *passkey = "", int relay = 0);
extern void ircparse(ircnet *n);
extern void irccleanup();
extern bool ircaddsockets(ENetSocket &maxsock, ENetSocketSet &readset, ENetSocketSet &writeset);
extern void ircchecksockets(ENetSocketSet &readset, ENetSocketSet &writeset);
extern void ircslice();
