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

    struct listinfo
    {
        char *name;
        vector<info> infos;

        listinfo(const char *_name) : name(_name && *_name ? newstring(_name) : NULL) {}
        ~listinfo() { infos.shrink(0); }
    };

    struct groupinfo
    {
        vector<info> infos;

        groupinfo() {}
        ~groupinfo() { infos.shrink(0); }
    };

    struct taginfo
    {
        char *name;
        vector<groupinfo> groups;

        taginfo(const char *_name) : name(_name && *_name ? newstring(_name) : NULL) {}
        ~taginfo() { groups.shrink(0); }
    };

    vector<listinfo> lists;
    vector<taginfo> tags;

    int millis;
    eventlog() : millis(totalmillis) {}
    ~eventlog() { reset(); }

    void reset()
    {
        tags.shrink(0);
        lists.shrink(0);
    }

    static int findinfo(vector<info> &infos, const char *name, bool create = false)
    {
        if(!name || !*name) return -1;
        loopv(infos)
        {
            if(strcmp(infos[i].name, name)) continue;
            return i;
        }
        if(create)
        {
            int n = infos.length();
            info &v = infos.add();
            v.name = newstring(name);
            return n;
        }
        return -1;
    }

    int findtaginfo(const char *name, bool create = false)
    {
        if(!name || !*name) return -1;
        loopv(tags) if(!strcmp(tags[i].name, name)) return i;
        if(create)
        {
            int n = tags.length();
            tags.add(taginfo(name));
            return n;
        }
        return -1;
    }

    int findlistinfo(const char *name, bool create = false)
    {
        if(!name || !*name) return -1;
        loopv(lists) if(!strcmp(lists[i].name, name)) return i;
        if(create)
        {
            int n = lists.length();
            lists.add(listinfo(name));
            return n;
        }
        return -1;
    }

    const char *constr()
    {
        int a = findlistinfo("args");
        if(!lists.inrange(a)) return NULL;
        listinfo &t = lists[a];
        int c = findinfo(t.infos, "console");
        if(!t.infos.inrange(c)) return NULL;
        info &n = t.infos[c];
        if(n.type == EV_I_STR) return n.s;
        return NULL;
    }

#ifdef CPP_GAME_SERVER
    static void sendinfo(packetbuf &p, vector<info> &infos)
    {
        putint(p, infos.length());
        loopv(infos)
        {
            info &n = infos[i];
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
#else
    static void parseinfo(vector<eventlog::info> &infos, ucharbuf &p)
    {
        static string info, str;
        getstring(info, p);
        int itype = getint(p);
        switch(itype)
        {
            case EV_I_INT: eventlog::addinfo(infos, info, getint(p)); break;
            case EV_I_BOOL: eventlog::addinfo(infos, info, getint(p) != 0); break;
            case EV_I_FLOAT: eventlog::addinfo(infos, info, getfloat(p)); break;
            case EV_I_STR:
            {
                getstring(str, p);
                eventlog::addinfo(infos, info, str);
                break;
            }
            default: break;
        }
    }

    static void parseevent(ucharbuf &p)
    {
        string text;
        eventlog *evt = new eventlog;
        int ilen = getint(p);
        loopi(ilen)
        {
            getstring(text, p);
            eventlog::listinfo &t = evt->lists.add(eventlog::listinfo(text));
            int vlen = getint(p);
            loopj(vlen) parseinfo(t.infos, p);
        }
        int tlen = getint(p);
        loopi(tlen)
        {
            getstring(text, p);
            eventlog::taginfo &t = evt->tags.add(eventlog::taginfo(text));
            int vlen = getint(p);
            loopj(vlen)
            {
                eventlog::groupinfo &g = t.groups.add();
                int glen = getint(p);
                loopk(glen) parseinfo(g.infos, p);
            }
        }
        evt->push();
    }
#endif

    void push()
    {
#ifdef CPP_GAME_SERVER
        int target = -1, tid = findlistinfo("this");
        if(lists.inrange(tid))
        {
            eventlog::listinfo &t = lists[tid];
            int sid = findinfo(t.infos, "target");
            if(t.infos.inrange(sid) && t.infos[sid].type == EV_I_INT)
                target = t.infos[sid].i;
        }

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_EVENTLOG);
        putint(p, lists.length());
        loopv(lists)
        {
            listinfo &t = lists[i];
            sendstring(t.name, p);
            sendinfo(p, t.infos);
        }

        putint(p, tags.length());
        loopv(tags)
        {
            taginfo &t = tags[i];
            sendstring(t.name, p);
            putint(p, t.groups.length());
            loopvk(t.groups)
            {
                groupinfo &g = t.groups[i];
                sendinfo(p, g.infos);
            }
        }

        sendpacket(target, 1, p.finalize());
#else
        if(eventlogs.length() >= eventloglines)
        {
            eventlog *f = eventlogs[0];
            eventlogs.remove(0);
            delete f;
        }

        int sound = -1, flags = 0, tid = findlistinfo("this");
        if(lists.inrange(tid))
        {
            eventlog::listinfo &t = lists[tid];
            int sid = findinfo(t.infos, "sound");
            if(t.infos.inrange(sid))
            {
                info &n = t.infos[sid];
                switch(n.type)
                {
                    case EV_I_INT: sound = n.i; break;
                    case EV_I_STR: sound = gamesounds[n.s].getindex(); break;
                    default: break;
                }
            }
            sid = findinfo(t.infos, "flags");
            if(t.infos.inrange(sid) && t.infos[sid].type == EV_I_INT)
                flags = t.infos[sid].i;
        }

        int c = findtaginfo("client");
        if(tags.inrange(c))
        {
            taginfo &t = tags[c];
            loopv(t.groups)
            {
                groupinfo &g = t.groups[i];
                int f = findinfo(g.infos, "clientnum");
                if(!g.infos.inrange(f)) continue;
                info &n = g.infos[f];
                if(n.type != EV_I_INT) continue;
                gameent *d = game::getclient(n.i);
                if(!d) continue;
                if(sound >= 0 && flags&(i == 0 ? EV_F_CLIENT1 : (i == 1 ? EV_F_CLIENT2 : EV_F_CLIENTN)))
                    entities::announce(sound, d);
            }
        }
        if(sound >= 0 && flags&EV_F_BROADCAST) entities::announce(sound);

        const char *con = constr();
        if(con && *con) conoutft(CON_EVENT, "%s", con);

        eventlogs.add(this);
#endif
    }

    static int addinfo(vector<info> &infos, const char *name, int i)
    {
        if(!name || !*name) return -1;
        int n = findinfo(infos, name, true);
        infos[n].set(i);
        return n;
    }

    int addlist(const char *list, const char *name, int i)
    {
        int n = findlistinfo(list, true);
        return addinfo(lists[n].infos, name, i);
    }

    int addgroup(const char *tag, const char *name, int i)
    {
        int n = findtaginfo(tag, true), r = tags[n].groups.length();
        groupinfo &g = tags[n].groups.add();
        addinfo(g.infos, name, i);
        return r;
    }

    static int addinfo(vector<info> &infos, const char *name, bool b)
    {
        if(!name || !*name) return -1;
        int n = findinfo(infos, name, true);
        infos[n].set(b);
        return n;
    }

    int addlist(const char *list, const char *name, bool b)
    {
        int n = findlistinfo(list, true);
        return addinfo(lists[n].infos, name, b);
    }

    int addgroup(const char *tag, const char *name, bool b)
    {
        int n = findtaginfo(tag, true), r = tags[n].groups.length();
        groupinfo &g = tags[n].groups.add();
        addinfo(g.infos, name, b);
        return r;
    }

    static int addinfo(vector<info> &infos, const char *name, float f)
    {
        if(!name || !*name) return -1;
        int n = findinfo(infos, name, true);
        infos[n].set(f);
        return n;
    }

    int addlist(const char *list, const char *name, float f)
    {
        int n = findlistinfo(list, true);
        return addinfo(lists[n].infos, name, f);
    }

    int addgroup(const char *tag, const char *name, float f)
    {
        int n = findtaginfo(tag, true), r = tags[n].groups.length();
        groupinfo &g = tags[n].groups.add();
        addinfo(g.infos, name, f);
        return r;
    }

    static int addinfo(vector<info> &infos, const char *name, const char *str)
    {
        if(!name || !*name) return -1;
        int n = findinfo(infos, name, true);
        infos[n].set(str);
        return n;
    }

    int addlist(const char *list, const char *name, const char *str)
    {
        int n = findlistinfo(list, true);
        addinfo(lists[n].infos, name, str);
        return n;
    }

    int addgroup(const char *tag, const char *name, const char *str)
    {
        int n = findtaginfo(tag, true), r = tags[n].groups.length();
        groupinfo &g = tags[n].groups.add();
        addinfo(g.infos, name, str);
        return r;
    }

    static int addinfof(vector<info> &infos, const char *name, const char *str, ...)
    {
        if(!name || !*name) return -1;
        defvformatbigstring(s, str, str);
        return addinfo(infos, name, s);
    }

    int addlistf(const char *list, const char *name, const char *str, ...)
    {
        int n = findlistinfo(list, true);
        defvformatbigstring(s, str, str);
        return addinfo(lists[n].infos, name, s);
    }

    int addgroupf(const char *tag, const char *name, const char *str, ...)
    {
        int n = findtaginfo(tag, true), r = tags[n].groups.length();
        groupinfo &g = tags[n].groups.add();
        defvformatbigstring(s, str, str);
        addinfo(g.infos, name, s);
        return r;
    }

#ifdef CPP_GAME_SERVER
    int addclient(const char *tag, clientinfo *d)
#else
    int addclient(const char *tag, gameent *d)
#endif
    {
        int n = findtaginfo(tag, true), r = tags[n].groups.length();
        groupinfo &g = tags[n].groups.add();

        if(!d)
        {
            addinfo(g.infos, "clientnum", -1);
            return r;
        }

        addinfo(g.infos, "clientnum", d->clientnum);
        addinfo(g.infos, "actortype", d->actortype);
        addinfo(g.infos, "team", d->team);
        addinfo(g.infos, "colour", d->colour);
        addinfo(g.infos, "model", d->model);
        addinfo(g.infos, "privilege", d->privilege);
        addinfo(g.infos, "weapselect", d->weapselect);
        addinfo(g.infos, "health", d->health);
        addinfo(g.infos, "name", d->name);

        return r;
    }

#ifdef CPP_GAME_SERVER
    int addclient(const char *tag, int cn) { return addclient(tag, (clientinfo *)::getinfo(cn)); }
#else
    int addclient(const char *tag, int cn) { return addclient(tag, game::getclient(cn)); }
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
ICOMMAND(0, geteventmillis, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->millis : -1));

ICOMMAND(0, geteventtag, "isis", (int *id, char *tag, int *grp, char *name),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int b = e->findtaginfo(tag);
    if(!e->tags.inrange(b)) return;
    eventlog::taginfo &t = e->tags[b];

    if(!t.groups.inrange(*grp)) return;
    eventlog::groupinfo &g = t.groups[*grp];

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

ICOMMAND(0, geteventtags, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->tags.length() : -1));

ICOMMAND(0, geteventtaggroups, "is", (int *id, char *tag),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = e->findtaginfo(tag);
    if(!e->tags.inrange(c)) return;

    eventlog::taginfo &t = e->tags[c];
    intret(t.groups.length());
});

ICOMMAND(0, geteventtaggroupinfos, "isi", (int *id, char *tag, int *grp),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = e->findtaginfo(tag);
    if(!e->tags.inrange(c)) return;
    eventlog::taginfo &t = e->tags[c];

    if(!t.groups.inrange(*grp)) return;
    eventlog::groupinfo &g = t.groups[*grp];

    intret(g.infos.length());
});

ICOMMAND(0, geteventname, "isibbb", (int *id, char *tag, int *grp, int *col, int *icon, int *dupname),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int c = e->findtaginfo(tag);
    if(!e->tags.inrange(c)) return;
    eventlog::taginfo &t = e->tags[c];

    if(!t.groups.inrange(*grp)) return;
    eventlog::groupinfo &g = t.groups[*grp];

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

ICOMMAND(0, geteventlist, "iss", (int *id, char *list, char *name),
{
    if(!eventlogs.inrange(*id)) return;
    eventlog *e = eventlogs[*id];

    int b = e->findlistinfo(list);
    if(!e->lists.inrange(b)) return;
    eventlog::listinfo &t = e->lists[b];

    int c = eventlog::findinfo(t.infos, name);
    if(!t.infos.inrange(c)) return;
    eventlog::info &n = t.infos[c];
    switch(n.type)
    {
        case EV_I_INT: intret(n.i); break;
        case EV_I_BOOL: intret(n.b ? 1 : 0); break;
        case EV_I_FLOAT: floatret(n.f); break;
        case EV_I_STR: result(n.s); break;
        default: break;
    }
});

ICOMMAND(0, geteventlists, "i", (int *id), intret(eventlogs.inrange(*id) ? eventlogs[*id]->lists.length() : -1));
#endif
