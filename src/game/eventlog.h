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

    struct grpinfo
    {
        vector<info> infos;

        grpinfo() {}
        ~grpinfo() { infos.shrink(0); }
    };

    struct taginfo
    {
        char *name;
        vector<grpinfo> infos;

        taginfo(const char *_name) : name(_name && *_name ? newstring(_name) : NULL) {}
        ~taginfo() { infos.shrink(0); }
    };

#ifdef CPP_GAME_SERVER
    int cn, type, subtype, sndflags;
    char *sndidx;
#else
    int type, subtype, sndidx, sndflags, millis;
#endif
    vector<info> infos;
    vector<taginfo> taginfos;

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
        taginfos.shrink(0);
        infos.shrink(0);
    }

    static int findinfo(vector<info> &list, const char *name, bool create = false)
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

    int findtaginfo(const char *name, bool create = false)
    {
        loopv(taginfos) if(!strcmp(taginfos[i].name, name)) return i;
        if(create)
        {
            int n = taginfos.length();
            taginfos.add(taginfo(name));
            return n;
        }
        return -1;
    }

    const char *constr()
    {
        int c = findinfo(infos, "console");
        if(!infos.inrange(c)) return NULL;
        info &n = infos[c];
        if(n.type == EV_I_STR) return n.s;
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
        sendinfo(p, infos);
        putint(p, taginfos.length());
        loopv(taginfos)
        {
            taginfo &t = taginfos[i];
            sendstring(t.name, p);
            putint(p, t.infos.length());
            loopvk(t.infos)
            {
                grpinfo &g = t.infos[i];
                sendinfo(p, g.infos);
            }
        }
        sendpacket(cn, 1, p.finalize());
#else
        if(eventlogs.length() >= eventloglines)
        {
            eventlog *f = eventlogs[0];
            eventlogs.remove(0);
            delete f;
        }
        eventlogs.add(this);

        int c = findtaginfo("client");
        if(taginfos.inrange(c))
        {
            taginfo &t = taginfos[c];
            loopv(t.infos)
            {
                grpinfo &g = t.infos[i];
                int f = findinfo(g.infos, "clientnum");
                if(!g.infos.inrange(f)) continue;
                info &n = g.infos[f];
                if(n.type != EV_I_INT) continue;
                gameent *d = game::getclient(n.i);
                if(!d) continue;
                if(sndflags&(i == 0 ? EV_S_CLIENT1 : (i == 1 ? EV_S_CLIENT2 : EV_S_CLIENTN)))
                    entities::announce(sndidx, d);
            }
        }
        if(sndidx >= 0 && sndflags&EV_S_BROADCAST) entities::announce(sndidx);

        const char *con = constr();
        if(con && *con) conoutft(CON_EVENT, "%s", con);
#endif
    }

    static void addinfov(vector<info> &list, const char *name, int i)
    {
        if(!name || !*name) return;
        int n = findinfo(list, name, true);
        list[n].set(i);
    }
    void addinfo(const char *name, int i) { addinfov(infos, name, i); }

    static void addinfov(vector<info> &list, const char *name, float f)
    {
        if(!name || !*name) return;
        int n = findinfo(list, name, true);
        list[n].set(f);
    }
    void addinfo(const char *name, float f) { addinfov(infos, name, f); }

    static void addinfov(vector<info> &list, const char *name, const char *str)
    {
        if(!name || !*name) return;
        int n = findinfo(list, name, true);
        list[n].set(str);
    }
    void addinfo(const char *name, const char *str) { addinfov(infos, name, str); }

    static void addinfovf(vector<info> &list, const char *name, const char *str, ...)
    {
        if(!name || !*name) return;
        defvformatbigstring(s, str, str);
        addinfov(list, name, s);
    }

    void addinfof(const char *name, const char *str, ...)
    {
        if(!name || !*name) return;
        defvformatbigstring(s, str, str);
        addinfov(infos, name, s);
    }

#ifdef CPP_GAME_SERVER
    void addclient(clientinfo *d, const char *tag = "client")
#else
    void addclient(gameent *d, const char *tag = "client")
#endif
    {
        int n = findtaginfo(tag, true);
        grpinfo &g = taginfos[n].infos.add();
        if(!d)
        {
            addinfov(g.infos, "clientnum", -1);
            return;
        }
        addinfov(g.infos, "clientnum", d->clientnum);
        addinfov(g.infos, "actortype", d->actortype);
        addinfov(g.infos, "team", d->team);
        addinfov(g.infos, "colour", d->colour);
        addinfov(g.infos, "model", d->model);
        addinfov(g.infos, "privilege", d->privilege);
        addinfov(g.infos, "weapselect", d->weapselect);
        addinfov(g.infos, "health", d->health);
        addinfov(g.infos, "name", d->name);
    }

#ifdef CPP_GAME_SERVER
    void addclient(int cn, const char *tag = "client") { addclient((clientinfo *)::getinfo(cn), tag); }
#else
    void addclient(int cn, const char *tag = "client") { addclient(game::getclient(cn), tag); }
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
ICOMMAND(0, geteventtype, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->type : -1));
ICOMMAND(0, geteventsubtype, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->subtype : -1));
ICOMMAND(0, geteventsndidx, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->sndidx : -1));
ICOMMAND(0, geteventsndflags, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->sndflags : -1));
ICOMMAND(0, geteventmillis, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->millis : -1));

ICOMMAND(0, geteventtag, "isis", (int *id, char *tag, int *grp, char *name),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int b = e->findtaginfo(tag);
    if(!e->taginfos.inrange(b)) return;
    eventlog::taginfo &t = e->taginfos[b];

    if(!t.infos.inrange(*grp)) return;
    eventlog::grpinfo &g = t.infos[*grp];

    int c = eventlog::findinfo(g.infos, name);
    if(!g.infos.inrange(c)) return;
    eventlog::info &n = g.infos[c];
    switch(n.type)
    {
        case EV_I_INT: intret(n.i); break;
        case EV_I_BOOL: intret(n.b ? 1 : 0); break;
        case EV_I_FLOAT: floatret(n.f); break;
        case EV_I_STR: result(n.s); break;
        default: break;
    }
});

ICOMMAND(0, geteventtags, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->taginfos.length() : -1));

ICOMMAND(0, geteventtaggroups, "is", (int *id, char *tag),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = e->findtaginfo(tag);
    if(!e->taginfos.inrange(c)) return;

    eventlog::taginfo &t = e->taginfos[c];
    intret(t.infos.length());
});

ICOMMAND(0, geteventtaginfos, "isi", (int *id, char *tag, int *grp),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = e->findtaginfo(tag);
    if(!e->taginfos.inrange(c)) return;

    eventlog::taginfo &t = e->taginfos[c];
    if(!t.infos.inrange(*grp)) return;

    eventlog::grpinfo &g = t.infos[*grp];
    intret(g.infos.length());
});

ICOMMAND(0, geteventname, "iibbb", (int *id, int *grp, int *col, int *icon, int *dupname),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = e->findtaginfo("client");
    if(!e->taginfos.inrange(c)) return;

    eventlog::taginfo &t = e->taginfos[c];
    if(!t.infos.inrange(*grp)) return;

    eventlog::grpinfo &g = t.infos[*grp];

    int _name = eventlog::findinfo(g.infos, "name");
    if(!g.infos.inrange(_name)) return;
    eventlog::info &name = g.infos[_name];
    if(name.type != EV_I_STR) return;

    int _clientnum = eventlog::findinfo(g.infos, "clientnum");
    if(!g.infos.inrange(_clientnum)) return;
    eventlog::info &clientnum = g.infos[_clientnum];
    if(clientnum.type != EV_I_INT) return;

    int _team = eventlog::findinfo(g.infos, "team");
    if(!g.infos.inrange(_team)) return;
    eventlog::info &team = g.infos[_team];
    if(team.type != EV_I_INT) return;

    int _actortype = eventlog::findinfo(g.infos, "actortype");
    if(!g.infos.inrange(_actortype)) return;
    eventlog::info &actortype = g.infos[_actortype];
    if(actortype.type != EV_I_INT) return;

    int _colour = eventlog::findinfo(g.infos, "colour");
    if(!g.infos.inrange(_colour)) return;
    eventlog::info &colour = g.infos[_colour];
    if(colour.type != EV_I_INT) return;

    int _privilege = eventlog::findinfo(g.infos, "privilege");
    if(!g.infos.inrange(_privilege)) return;
    eventlog::info &privilege = g.infos[_privilege];
    if(privilege.type != EV_I_INT) return;

    int _weapselect = eventlog::findinfo(g.infos, "weapselect");
    if(!g.infos.inrange(_weapselect)) return;
    eventlog::info &weapselect = g.infos[_weapselect];
    if(weapselect.type != EV_I_INT) return;

    result(game::colourname(name.s, clientnum.i, team.i, actortype.i, colour.i, privilege.i, weapselect.i, *icon != 0, *dupname != 0, *col >= 0 ? *col : 3));
});

ICOMMAND(0, geteventinfo, "is", (int *id, char *name),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = eventlog::findinfo(e->infos, name);
    if(!e->infos.inrange(c)) return;

    eventlog::info &n = e->infos[c];
    switch(n.type)
    {
        case EV_I_INT: intret(n.i); break;
        case EV_I_BOOL: intret(n.b ? 1 : 0); break;
        case EV_I_FLOAT: floatret(n.f); break;
        case EV_I_STR: result(n.s); break;
        default: break;
    }
});
ICOMMAND(0, geteventinfos, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->infos.length() : -1));
#endif