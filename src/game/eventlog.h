#ifndef CPP_GAME_SERVER
struct eventlog;
#ifdef CPP_GAME_MAIN
VAR(IDF_PERSIST, eventloglines, 1, 50, VAR_MAX);
vector<eventlog *> eventlogs;
#else
extern int eventloglines;
extern vector<eventlog *> eventlogs;
#endif
#endif

struct eventlog
{
    struct info
    {
        char *name;
        int type;
        union
        {
            int i;
            bool b;
            float f;
            char *s;
        };

        info() : name(NULL), type(-1) {}
        ~info() { reset(); }

        void cleanup()
        {
            if(type == EV_I_STR) DELETEA(s);
        }

        void reset()
        {
            DELETEA(name);
            if(type == EV_I_STR) DELETEA(s);
        }

        void set(bool v)
        {
            cleanup();
            type = EV_I_BOOL;
            b = v;
        }

        void set(int v)
        {
            cleanup();
            type = EV_I_INT;
            i = v;
        }

        void set(float v)
        {
            cleanup();
            type = EV_I_FLOAT;
            f = v;
        }

        void set(char *v)
        {
            cleanup();
            type = EV_I_STR;
            s = newstring(v && *v ? v : "");
        }

        void set(const char *v)
        {
            cleanup();
            type = EV_I_STR;
            s = newstring(v && *v ? v : "");
        }
    };

    struct cinfo
    {
        int cn;
        vector<info> infos;

        cinfo() {}
        ~cinfo() { infos.shrink(0); }
    };

#ifdef CPP_GAME_SERVER
    int cn, type, subtype, sndflags;
    char *sndidx;
#else
    int type, subtype, sndidx, sndflags, millis;
#endif
    vector<cinfo> clients;
    vector<info> infos;

#ifdef CPP_GAME_SERVER
    eventlog(int _cn, int _type, int _subtype, const char *_sndidx = NULL, int _sndflags = 0) :
        cn(_cn), type(_type), subtype(_subtype), sndflags(_sndflags), sndidx(newstring(_sndidx && *_sndidx ? _sndidx : "")) {}
#else
    eventlog(int _type, int _subtype, int _sndidx = -1, int _sndflags = 0, int _millis = -1) :
        type(_type), subtype(_subtype), sndidx(_sndidx), sndflags(_sndflags), millis(_millis >= 0 ? millis : totalmillis) {}
#endif
    ~eventlog() { reset(); }

    void reset()
    {
#ifdef CPP_GAME_SERVER
        DELETEA(sndidx);
#endif
        clients.shrink(0);
        infos.shrink(0);
    }

    int findclient(int cn, bool create = false)
    {
        loopv(clients) if(clients[i].cn == cn) return i;
        if(create)
        {
            int n = clients.length();
            clients.add().cn = cn;
            return n;
        }
        return -1;
    }

    int findinfov(vector<info> &list, const char *name, bool create = false)
    {
        if(!name || !*name) return -1;
        loopv(list)
        {
            if(strcmp(list[i].name, name)) continue;
            return i;
        }
        if(create)
        {
            int n = list.length();
            info &v = list.add();
            v.name = newstring(name);
            return n;
        }
        return -1;
    }

    int findclient(int cn, const char *name, bool create = false)
    {
        int n = findclient(cn);
        if(!clients.inrange(n)) return -1;
        return findinfov(clients[n].infos, name, create);
    }

    int findinfo(const char *name, bool create = false) { return findinfov(infos, name, create); }

    info *getinfov(vector<info> &list, const char *name, bool create = false)
    {
        int n = findinfov(list, name, create);
        if(!list.inrange(n)) return NULL;
        return &list[n];
    }

    info *getclient(int cn, const char *name, bool create = false)
    {
        int n = findclient(cn);
        if(!clients.inrange(n)) return NULL;
        return getinfov(clients[n].infos, name, create);
    }

    info *getinfo(const char *name, bool create = false) { return getinfov(infos, name, create); }

    const char *constr()
    {
        info *con = getinfov(infos, "console");
        if(con && con->type == EV_I_STR) return con->s;
        return NULL;
    }

#ifdef CPP_GAME_SERVER
    void sendinfo(packetbuf &p, vector<info> &list)
    {
        putint(p, list.length());
        loopv(list)
        {
            info &n = list[i];
            sendstring(n.name, p);
            putint(p, n.type);
            switch(n.type)
            {
                case EV_I_INT: putint(p, n.i); break;
                case EV_I_BOOL: putint(p, n.b ? 1 : 0); break;
                case EV_I_FLOAT: putfloat(p, n.f); break;
                case EV_I_STR: sendstring(n.s, p); break;
                default: break;
            }
        }
    }
#endif

    void push()
    {
#ifdef CPP_GAME_SERVER
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_EVENTLOG);
        putint(p, type);
        putint(p, subtype);
        sendstring(sndidx, p);
        putint(p, sndflags);
        putint(p, clients.length());
        loopv(clients)
        {
            cinfo &c = clients[i];
            putint(p, c.cn);
            putint(p, c.infos.length());
            sendinfo(p, c.infos);
        }
        sendinfo(p, infos);
        sendpacket(cn, 1, p.finalize());
#else
        if(eventlogs.length() >= eventloglines)
        {
            eventlog *f = eventlogs[0];
            eventlogs.remove(0);
            delete f;
        }
        eventlogs.add(this);

        loopv(clients)
        {
            gameent *d = game::getclient(clients[i].cn);
            if(!d) continue;
            if(sndflags&(i == 0 ? EV_S_CLIENT1 : (i == 1 ? EV_S_CLIENT2 : EV_S_CLIENTN)))
                entities::announce(sndidx, d);
        }
        if(sndidx >= 0 && sndflags&EV_S_BROADCAST) entities::announce(sndidx);

        const char *con = constr();
        if(con && *con) conoutft(CON_EVENT, "%s", con);
#endif
    }

    void addinfov(vector<info> &list, const char *name, int i)
    {
        if(!name || !*name) return;
        getinfov(list, name, true)->set(i);
    }

    void addclient(int cn, const char *name, int i)
    {
        int n = findclient(cn, true);
        addinfov(clients[n].infos, name, i);
    }

    void addinfo(const char *name, int i) { addinfov(infos, name, i); }

    void addinfov(vector<info> &list, const char *name, float f)
    {
        if(!name || !*name) return;
        getinfov(list, name, true)->set(f);
    }

    void addclient(int cn, const char *name, float f)
    {
        int n = findclient(cn, true);
        addinfov(clients[n].infos, name, f);
    }

    void addinfo(const char *name, float f) { addinfov(infos, name, f); }

    void addinfov(vector<info> &list, const char *name, const char *str)
    {
        if(!name || !*name) return;
        getinfov(list, name, true)->set(str);
    }

    void addclient(int cn, const char *name, const char *str)
    {
        int n = findclient(cn, true);
        addinfov(clients[n].infos, name, str);
    }

    void addinfo(const char *name, const char *str) { addinfov(infos, name, str); }

    void addinfovf(vector<info> &list, const char *name, const char *str, ...)
    {
        if(!name || !*name) return;
        defvformatbigstring(s, str, str);
        addinfov(list, name, s);
    }

    void addclientf(int cn, const char *name, const char *str, ...)
    {
        int n = findclient(cn, true);
        defvformatbigstring(s, str, str);
        addinfov(clients[n].infos, name, s);
    }

    void addinfof(const char *name, const char *str, ...)
    {
        if(!name || !*name) return;
        defvformatbigstring(s, str, str);
        addinfov(infos, name, s);
    }

#ifdef CPP_GAME_SERVER
    void addclient(clientinfo *d)
#else
    void addclient(gameent *d)
#endif
    {
        if(!d)
        {
            cinfo &c = clients.add();
            c.cn = -1; // dummy client
            addinfov(c.infos, "clientnum", -1);
            return;
        }
        addclient(d->clientnum, "clientnum", d->clientnum);
        addclient(d->clientnum, "actortype", d->actortype);
        addclient(d->clientnum, "team", d->team);
        addclient(d->clientnum, "colour", d->colour);
        addclient(d->clientnum, "model", d->model);
        addclient(d->clientnum, "privilege", d->privilege);
        addclient(d->clientnum, "weapselect", d->weapselect);
        addclient(d->clientnum, "health", d->health);
        addclient(d->clientnum, "name", d->name);
    }

#ifdef CPP_GAME_SERVER
    void addclient(int cn) { addclient((clientinfo *)::getinfo(cn)); }
#else
    void addclient(int cn) { addclient(game::getclient(cn)); }
#endif
};

#ifdef CPP_GAME_MAIN
#define LOOPEVENTS(name,op) \
    ICOMMAND(0, loopevents##name, "iire", (int *count, int *skip, ident *id, uint *body), \
    { \
        loopstart(id, stack); \
        op(eventlogs, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });

LOOPEVENTS(,loopcsv);
LOOPEVENTS(rev,loopcsvrev);

ICOMMANDV(0, eventcount, eventlogs.length());
ICOMMAND(0, geteventclients, "ib", (int *n, int *pos), intret(eventlogs.inrange(*n) ? (*pos >= 0 ? (eventlogs[*n]->clients.inrange(*pos) ? eventlogs[*n]->clients[*pos].infos.length() : -1) : eventlogs[*n]->clients.length()) : -1));
ICOMMAND(0, geteventinfos, "i", (int *n), intret(eventlogs.inrange(*n) ? eventlogs[*n]->infos.length() : -1));
ICOMMAND(0, geteventtype, "i", (int *n), intret(eventlogs.inrange(*n) ? eventlogs[*n]->type : -1));
ICOMMAND(0, geteventsubtype, "i", (int *n), intret(eventlogs.inrange(*n) ? eventlogs[*n]->subtype : -1));
ICOMMAND(0, geteventsndidx, "i", (int *n), intret(eventlogs.inrange(*n) ? eventlogs[*n]->sndidx : -1));
ICOMMAND(0, geteventsndflags, "i", (int *n), intret(eventlogs.inrange(*n) ? eventlogs[*n]->sndflags : -1));
ICOMMAND(0, geteventmillis, "i", (int *n), intret(eventlogs.inrange(*n) ? eventlogs[*n]->millis : -1));

ICOMMAND(0, geteventclient, "iis", (int *n, int *pos, char *name),
{
    if(!eventlogs.inrange(*n)) return;
    eventlog *e = eventlogs[*n];

    if(!e->clients.inrange(*pos)) return;
    eventlog::cinfo &c = e->clients[*pos];

    eventlog::info *n = e->getinfov(c.infos, name);
    if(!n) return;
    switch(n->type)
    {
        case EV_I_INT: intret(n->i); break;
        case EV_I_BOOL: intret(n->b ? 1 : 0); break;
        case EV_I_FLOAT: floatret(n->f); break;
        case EV_I_STR: result(n->s); break;
        default: break;
    }
});

ICOMMAND(0, geteventname, "iibbb", (int *n, int *pos, int *col, int *icon, int *dupname),
{
    if(!eventlogs.inrange(*n)) return;
    eventlog *e = eventlogs[*n];

    if(!e->clients.inrange(*pos)) return;
    eventlog::cinfo &c = e->clients[*pos];

    eventlog::info *name = e->getinfov(c.infos, "name");
    if(!name || name->type != EV_I_STR) return;
    eventlog::info *clientnum = e->getinfov(c.infos, "clientnum");
    if(!clientnum || clientnum->type != EV_I_INT) return;
    eventlog::info *team = e->getinfov(c.infos, "team");
    if(!team || team->type != EV_I_INT) return;
    eventlog::info *actortype = e->getinfov(c.infos, "actortype");
    if(!actortype || actortype->type != EV_I_INT) return;
    eventlog::info *colour = e->getinfov(c.infos, "colour");
    if(!colour || colour->type != EV_I_INT) return;
    eventlog::info *privilege = e->getinfov(c.infos, "privilege");
    if(!privilege || privilege->type != EV_I_INT) return;
    eventlog::info *weapselect = e->getinfov(c.infos, "weapselect");
    if(!weapselect || weapselect->type != EV_I_INT) return;

    result(game::colourname(name->s, clientnum->i, team->i, actortype->i, colour->i, privilege->i, weapselect->i, *icon != 0, *dupname != 0, *col >= 0 ? *col : 3));
});

ICOMMAND(0, geteventinfo, "is", (int *n, char *name),
{
    if(!eventlogs.inrange(*n)) return;
    eventlog *e = eventlogs[*n];
    eventlog::info *n = e->getinfo(name);
    if(!n) return;
    switch(n->type)
    {
        case EV_I_INT: intret(n->i); break;
        case EV_I_BOOL: intret(n->b ? 1 : 0); break;
        case EV_I_FLOAT: floatret(n->f); break;
        case EV_I_STR: result(n->s); break;
        default: break;
    }
});
#endif