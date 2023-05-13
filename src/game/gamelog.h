#ifndef CPP_GAME_SERVER
struct gamelog;
#ifdef CPP_GAME_MAIN
VAR(IDF_PERSIST, gameloglines, 1, 50, VAR_MAX);
vector<gamelog *> eventlog;
#else
extern int gameloglines;
extern vector<gamelog *> eventlog;
#endif
#endif

struct gamelog
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

        void comret()
        {
            switch(type)
            {
                case EV_I_INT: intret(i); break;
                case EV_I_BOOL: intret(b ? 1 : 0); break;
                case EV_I_FLOAT: floatret(f); break;
                case EV_I_STR: result(s); break;
                default: break;
            }
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
    gamelog() : millis(totalmillis) {}
    ~gamelog() { reset(); }

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
    static void parseinfo(vector<info> &infos, ucharbuf &p)
    {
        static string text, str;
        getstring(text, p);
        int itype = getint(p);
        switch(itype)
        {
            case EV_I_INT: addinfo(infos, text, getint(p)); break;
            case EV_I_BOOL: addinfo(infos, text, getint(p) != 0); break;
            case EV_I_FLOAT: addinfo(infos, text, getfloat(p)); break;
            case EV_I_STR:
            {
                getstring(str, p);
                addinfo(infos, text, str);
                break;
            }
            default: break;
        }
    }

    static void parseevent(ucharbuf &p)
    {
        static string text;
        gamelog *log = new gamelog;
        int ilen = getint(p);
        loopi(ilen)
        {
            getstring(text, p);
            listinfo &t = log->lists.add(listinfo(text));
            int vlen = getint(p);
            loopj(vlen) parseinfo(t.infos, p);
        }
        int tlen = getint(p);
        loopi(tlen)
        {
            getstring(text, p);
            taginfo &t = log->tags.add(taginfo(text));
            int vlen = getint(p);
            loopj(vlen)
            {
                groupinfo &g = t.groups.add();
                int glen = getint(p);
                loopk(glen) parseinfo(g.infos, p);
            }
        }
        log->push();
    }
#endif

    void push()
    {
#ifdef CPP_GAME_SERVER
        int target = -1, tid = findlistinfo("this");
        if(lists.inrange(tid))
        {
            listinfo &t = lists[tid];
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
            loopvj(t.groups)
            {
                groupinfo &g = t.groups[j];
                sendinfo(p, g.infos);
            }
        }

        sendpacket(target, 1, p.finalize());
#else
        if(eventlog.length() >= gameloglines)
        {
            gamelog *f = eventlog[0];
            eventlog.remove(0);
            delete f;
        }

        int sound = -1, flags = 0, tid = findlistinfo("this");
        if(lists.inrange(tid))
        {
            listinfo &t = lists[tid];
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

        eventlog.add(this);
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

    listinfo *getlist(const char *list)
    {
        int b = findlistinfo(list);
        if(!lists.inrange(b)) return NULL;
        return &lists[b];
    }

    info *getlistinfo(const char *list, const char *name)
    {
        gamelog::listinfo *g = getlist(list);
        int c = gamelog::findinfo(g->infos, name);
        if(!g->infos.inrange(c)) return NULL;
        return &g->infos[c];
    }

    info *getlistinfo(const char *list, int idx)
    {
        gamelog::listinfo *g = getlist(list);
        if(!g->infos.inrange(idx)) return NULL;
        return &g->infos[idx];
    }

    taginfo *gettag(const char *tag)
    {
        int b = findtaginfo(tag);
        if(!tags.inrange(b)) return NULL;
        return &tags[b];
    }

    groupinfo *getgroup(const char *tag, int grp)
    {
        taginfo *t = gettag(tag);
        if(!t) return NULL;
        if(!t->groups.inrange(grp)) return NULL;
        return &t->groups[grp];
    }

    info *gettaginfo(const char *tag, int grp, const char *name)
    {
        gamelog::groupinfo *g = getgroup(tag, grp);
        int c = gamelog::findinfo(g->infos, name);
        if(!g->infos.inrange(c)) return NULL;
        return &g->infos[c];
    }

    info *gettaginfo(const char *tag, int grp, int idx)
    {
        gamelog::groupinfo *g = getgroup(tag, grp);
        if(!g->infos.inrange(idx)) return NULL;
        return &g->infos[idx];
    }
#ifndef CPP_GAME_SERVER
    static gamelog *getevent(int entry)
    {
        if(!eventlog.inrange(entry)) return NULL;
        return eventlog[entry];
    }

    static listinfo *getlist(int entry, const char *list)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->getlist(list);
    }

    static info *getlistinfo(int entry, const char *list, const char *name)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->getlistinfo(list, name);
    }

    static info *getlistinfo(int entry, const char *list, int idx)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->getlistinfo(list, idx);
    }

    static taginfo *gettag(int entry, const char *tag)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->gettag(tag);
    }

    static groupinfo *getgroup(int entry, const char *tag, int grp)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->getgroup(tag, grp);
    }

    static info *gettaginfo(int entry, const char *tag, int grp, const char *name)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->gettaginfo(tag, grp, name);
    }

    static info *gettaginfo(int entry, const char *tag, int grp, int idx)
    {
        gamelog *e = getevent(entry);
        if(!e) return NULL;
        return e->gettaginfo(tag, grp, idx);
    }
#endif
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
        op(eventlog, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });

LOOPEVENTS(,loopcsv);
LOOPEVENTS(rev,loopcsvrev);

ICOMMANDV(0, eventcount, eventlog.length());
ICOMMAND(0, geteventmillis, "i", (int *val), intret(eventlog.inrange(*val) ? eventlog[*val]->millis : -1));

ICOMMAND(0, geteventtags, "i", (int *val), intret(eventlog.inrange(*val) ? eventlog[*val]->tags.length() : -1));

ICOMMAND(0, geteventtag, "isis", (int *val, char *tag, int *grp, char *name),
{
    gamelog::info *n = gamelog::gettaginfo(*val, tag, *grp, name);
    if(n) n->comret();
});

ICOMMAND(0, geteventtagid, "isii", (int *val, char *tag, int *grp, int *idx),
{
    gamelog::info *n = gamelog::gettaginfo(*val, tag, *grp, *idx);
    if(n) n->comret();
});

#define LOOPEVENTTAGS(name,op) \
    ICOMMAND(0, loopeventtags##name, "iiire", (int *val, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog *e = gamelog::getevent(*val); \
        if(!e) return; \
        loopstart(id, stack); \
        op(e->tags, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
LOOPEVENTTAGS(,loopcsv);
LOOPEVENTTAGS(rev,loopcsvrev);

ICOMMAND(0, geteventtaggroups, "is", (int *val, char *tag),
{
    gamelog::taginfo *t = gamelog::gettag(*val, tag);
    if(!t) return;
    intret(t->groups.length());
});

#define LOOPEVENTGROUPS(name,op) \
    ICOMMAND(0, loopeventgroups##name, "isiire", (int *val, char *tag, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog::taginfo *t = gamelog::gettag(*val, tag); \
        if(!t) return; \
        intret(t->groups.length()); \
        loopstart(id, stack); \
        op(t->groups, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
LOOPEVENTGROUPS(,loopcsv);
LOOPEVENTGROUPS(rev,loopcsvrev);

ICOMMAND(0, geteventtaggroupinfos, "isi", (int *val, char *tag, int *grp),
{
    gamelog::groupinfo *g = gamelog::getgroup(*val, tag, *grp);
    if(!g) return;
    intret(g->infos.length());
});

#define LOOPEVENTGROUPINFOS(name,op) \
    ICOMMAND(0, loopeventgroupinfos##name, "isiiire", (int *val, char *tag, int *grp, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog::groupinfo *g = gamelog::getgroup(*val, tag, *grp); \
        if(!g) return; \
        loopstart(id, stack); \
        op(g->infos, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
LOOPEVENTGROUPINFOS(,loopcsv);
LOOPEVENTGROUPINFOS(rev,loopcsvrev);

ICOMMAND(0, geteventname, "isibbb", (int *val, char *tag, int *grp, int *col, int *icon, int *dupname),
{
    gamelog::info *name = gamelog::gettaginfo(*val, tag, *grp, "name");
    if(!name || name->type != EV_I_STR) return;
    gamelog::info *clientnum = gamelog::gettaginfo(*val, tag, *grp, "clientnum");
    if(!clientnum || clientnum->type != EV_I_INT) return;
    gamelog::info *team = gamelog::gettaginfo(*val, tag, *grp, "team");
    if(!team || team->type != EV_I_INT) return;
    gamelog::info *actortype = gamelog::gettaginfo(*val, tag, *grp, "actortype");
    if(!actortype || actortype->type != EV_I_INT) return;
    gamelog::info *colour = gamelog::gettaginfo(*val, tag, *grp, "colour");
    if(!colour || colour->type != EV_I_INT) return;
    gamelog::info *privilege = gamelog::gettaginfo(*val, tag, *grp, "privilege");
    if(!privilege || privilege->type != EV_I_INT) return;
    gamelog::info *weapselect = gamelog::gettaginfo(*val, tag, *grp, "weapselect");
    if(!weapselect || weapselect->type != EV_I_INT) return;
    result(game::colourname(name->s, clientnum->i, team->i, actortype->i, colour->i, privilege->i, weapselect->i, *icon != 0, *dupname != 0, *col >= 0 ? *col : 3));
});

ICOMMAND(0, geteventlists, "i", (int *val), intret(eventlog.inrange(*val) ? eventlog[*val]->lists.length() : -1));

ICOMMAND(0, geteventlist, "iss", (int *val, char *list, char *name),
{
    gamelog::info *n = gamelog::getlistinfo(*val, list, name);
    if(n) n->comret();
});

#define LOOPEVENTLISTS(name,op) \
    ICOMMAND(0, loopeventlists##name, "isiiire", (int *val, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog *e = gamelog::getevent(*val); \
        if(!e) return; \
        loopstart(id, stack); \
        op(e->lists, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
LOOPEVENTLISTS(,loopcsv);
LOOPEVENTLISTS(rev,loopcsvrev);

#define LOOPEVENTLISTINFOS(name,op) \
    ICOMMAND(0, loopeventlistinfos##name, "isiiire", (int *val, char *list, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog::listinfo *g = gamelog::getlist(*val, list); \
        if(!g) return; \
        loopstart(id, stack); \
        op(g->infos, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
LOOPEVENTLISTINFOS(,loopcsv);
LOOPEVENTLISTINFOS(rev,loopcsvrev);
#endif
