#ifndef CPP_GAME_SERVER

struct gamelog;

#ifdef CPP_GAME_MAIN
#define GAMELOG_DEF(_name) \
    VAR(IDF_PERSIST, _name##lines, 1, 1000, VAR_MAX); \
    VAR(IDF_PERSIST, _name##echo, 0, 1, 1); \
    vector<gamelog *> _name; \
    int _name##seqid = 0;
#else
#define GAMELOG_DEF(_name) \
    extern int _name##lines, _name##echo, _name##seqid; \
    extern vector<gamelog *> _name;
#endif
GAMELOG_DEF(eventlog);
GAMELOG_DEF(messagelog);
GAMELOG_DEF(deathlog);
#undef GAMELOG_DEF
#endif

struct gamelog
{
    struct info : tagval
    {
        char *name;

        info() : name(NULL) { tagval::setnull(); }
        ~info() { reset(); }

        void reset()
        {
            DELETEA(name);
            tagval::reset();
        }

        void set(bool v)
        {
            tagval::reset();
            setint(v ? 1 : 0);
        }

        void set(int v)
        {
            tagval::reset();
            setint(v);
        }

        void set(float v)
        {
            tagval::reset();
            setfloat(v);
        }

        void set(char *v)
        {
            tagval::reset();
            setstr(newstring(v && *v ? v : ""));
        }

        void set(const char *v)
        {
            tagval::reset();
            setstr(newstring(v && *v ? v : ""));
        }

        void comret()
        {
            switch(type)
            {
                case VAL_INT: intret(i); break;
                case VAL_FLOAT: floatret(f); break;
                case VAL_STR: case VAL_CSTR: result(s); break;
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

    int type, millis, seqid;
    gamelog(int _type = GAMELOG_EVENT) : type(_type >= 0 && _type < GAMELOG_MAX ? _type : GAMELOG_EVENT), millis(totalmillis), seqid(-1) {}
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
        return t.infos[c].getstr();
    }

    int concolor()
    {
        int a = findlistinfo("args");
        if(!lists.inrange(a)) return colourwhite;
        listinfo &t = lists[a];
        int c = findinfo(t.infos, "colour");
        if(!t.infos.inrange(c)) return colourwhite;
        return t.infos[c].getint();
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
                case VAL_INT: putint(p, n.i); break;
                case VAL_FLOAT: putfloat(p, n.f); break;
                case VAL_STR: case VAL_CSTR: sendstring(n.s, p); break;
                default: break;
            }
        }
    }

    bool serverpush()
    {
        int target = -1, tid = findlistinfo("args");
        if(lists.inrange(tid))
        {
            listinfo &t = lists[tid];
            int sid = findinfo(t.infos, "target");
            if(t.infos.inrange(sid)) target = t.infos[sid].getint();
        }

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_GAMELOG);
        putint(p, type);
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

        return true;
    }
#else
    static void parseinfo(vector<info> &infos, ucharbuf &p)
    {
        static string text, str;
        getstring(text, p);
        int itype = getint(p);
        switch(itype)
        {
            case VAL_INT: addinfo(infos, text, getint(p)); break;
            case VAL_FLOAT: addinfo(infos, text, getfloat(p)); break;
            case VAL_STR: case VAL_CSTR:
            {
                getstring(str, p);
                addinfo(infos, text, str);
                break;
            }
            default: break;
        }
    }

    static void parselog(ucharbuf &p)
    {
        static string text;
        int type = getint(p);
        gamelog *e = new gamelog(type);
        int ilen = getint(p);
        loopi(ilen)
        {
            getstring(text, p);
            listinfo &t = e->lists.add(listinfo(text));
            int vlen = getint(p);
            loopj(vlen) parseinfo(t.infos, p);
        }
        int tlen = getint(p);
        loopi(tlen)
        {
            getstring(text, p);
            taginfo &t = e->tags.add(taginfo(text));
            int vlen = getint(p);
            loopj(vlen)
            {
                groupinfo &g = t.groups.add();
                int glen = getint(p);
                loopk(glen) parseinfo(g.infos, p);
            }
        }
        e->push();
    }

    bool clientpush(vector<gamelog *> &log)
    {
        int lines = 0, chan = 0;
        #define GAMELOG_DEF(_type, _chan, _name) \
            case _type: \
            { \
                lines = _name##lines; \
                chan = _chan; \
                seqid = _name##seqid++; \
                if(_name##seqid < 0) _name##seqid = 0; \
                break; \
            }

        switch(type)
        {
            GAMELOG_DEF(GAMELOG_EVENT, PLCHAN_ANNOUNCE, eventlog);
            GAMELOG_DEF(GAMELOG_MESSAGE, PLCHAN_MESSAGE, messagelog);
            GAMELOG_DEF(GAMELOG_DEATH, PLCHAN_ANNOUNCE, deathlog);
            default: return false;
        }

        #undef GAMELOG_DEF

        if(log.length() >= lines)
        {
            gamelog *f = log[0];
            log.remove(0);
            delete f;
        }

        int sound = -1, soundflags = 0, flags = 0, tid = findlistinfo("args");
        if(lists.inrange(tid))
        {
            listinfo &t = lists[tid];

            int sid = findinfo(t.infos, "sound");
            if(t.infos.inrange(sid))
            {
                if(t.infos[sid].type == VAL_STR) // server sends strings so client can get index itself
                {
                    sound = gamesounds[t.infos[sid].getstr()].getindex();
                    soundflags = SND_UNMAPPED;
                }
                else sound = t.infos[sid].getint();
            }

            sid = findinfo(t.infos, "flags");
            if(t.infos.inrange(sid)) flags = t.infos[sid].getint();

            sid = findinfo(t.infos, "chan");
            if(t.infos.inrange(sid)) chan = t.infos[sid].getint();
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
                gameent *d = game::getclient(g.infos[f].getint());
                if(!d) continue;
                if(sound >= 0 && flags&(i == 0 ? GAMELOG_F_CLIENT1 : (i == 1 ? GAMELOG_F_CLIENT2 : GAMELOG_F_CLIENTN)))
                    entities::announce(sound, d, chan, soundflags);
            }
        }
        if(sound >= 0 && flags&GAMELOG_F_BROADCAST) entities::announce(sound, NULL, -1, soundflags);

        bool wantecho = false;
        #define GAMELOG_DEF(_type, _name) \
            case _type: wantecho = _name##echo != 0; break;

        switch(type)
        {
            GAMELOG_DEF(GAMELOG_EVENT, eventlog);
            GAMELOG_DEF(GAMELOG_MESSAGE, messagelog);
            GAMELOG_DEF(GAMELOG_DEATH, deathlog);
            default: break;
        }

        #undef GAMELOG_DEF

        if(wantecho)
        {
            const char *con = constr();
            if(con && *con) eventf(concolor(), "%s", con);
        }

        log.add(this);

        return true;
    }
#endif

    bool push()
    {
#ifdef CPP_GAME_SERVER
        return serverpush();
#else
        vector<gamelog *> *log;
        #define GAMELOG_DEF(_type, _name) \
            case _type: log = &_name; break;

        switch(type)
        {
            GAMELOG_DEF(GAMELOG_EVENT, eventlog);
            GAMELOG_DEF(GAMELOG_MESSAGE, messagelog);
            GAMELOG_DEF(GAMELOG_DEATH, deathlog);
            default: return false;
        }

        #undef GAMELOG_DEF

        return clientpush(*log);
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
        if(!g) return NULL;
        int c = gamelog::findinfo(g->infos, name);
        if(!g->infos.inrange(c)) return NULL;
        return &g->infos[c];
    }

    info *getlistinfo(const char *list, int idx)
    {
        gamelog::listinfo *g = getlist(list);
        if(!g || !g->infos.inrange(idx)) return NULL;
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
        if(!g) return NULL;
        int c = gamelog::findinfo(g->infos, name);
        if(!g->infos.inrange(c)) return NULL;
        return &g->infos[c];
    }

    info *gettaginfo(const char *tag, int grp, int idx)
    {
        gamelog::groupinfo *g = getgroup(tag, grp);
        if(!g || !g->infos.inrange(idx)) return NULL;
        return &g->infos[idx];
    }
#ifndef CPP_GAME_SERVER
    static gamelog *getlog(vector<gamelog *> &log, int entry)
    {
        if(!log.inrange(entry)) return NULL;
        return log[entry];
    }

    static listinfo *getlist(vector<gamelog *> &log, int entry, const char *list)
    {
        gamelog *e = getlog(log, entry);
        if(!e) return NULL;
        return e->getlist(list);
    }

    static info *getlistinfo(vector<gamelog *> &log, int entry, const char *list, const char *name)
    {
        gamelog *e = getlog(log, entry);
        if(!e) return NULL;
        return e->getlistinfo(list, name);
    }

    static info *getlistinfo(vector<gamelog *> &log, int entry, const char *list, int idx)
    {
        gamelog *e = getlog(log, entry);
        if(!e) return NULL;
        return e->getlistinfo(list, idx);
    }

    static taginfo *gettag(vector<gamelog *> &log, int entry, const char *tag)
    {
        gamelog *e = getlog(log, entry);
        if(!e) return NULL;
        return e->gettag(tag);
    }

    static groupinfo *getgroup(vector<gamelog *> &log, int entry, const char *tag, int grp)
    {
        gamelog *e = getlog(log, entry);
        if(!e) return NULL;
        return e->getgroup(tag, grp);
    }

    static info *gettaginfo(vector<gamelog *> &log, int entry, const char *tag, int grp, const char *name)
    {
        gamelog *e = getlog(log, entry);
        if(!e) return NULL;
        return e->gettaginfo(tag, grp, name);
    }

    static info *gettaginfo(vector<gamelog *> &log, int entry, const char *tag, int grp, int idx)
    {
        gamelog *e = getlog(log, entry);
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
            addinfo(g.infos, "active", false);
            return r;
        }

        addinfo(g.infos, "clientnum", d->clientnum);
        addinfo(g.infos, "actortype", d->actortype);
        addinfo(g.infos, "team", d->team);
        addinfo(g.infos, "colour", d->colours[0]);
        addinfo(g.infos, "colour2", d->colours[1]);
        addinfo(g.infos, "model", d->model);
        addinfo(g.infos, "privilege", d->privilege);
        addinfo(g.infos, "weapselect", d->weapselect);
        addinfo(g.infos, "health", d->health);
        addinfo(g.infos, "name", d->name);
        addinfo(g.infos, "active", true);

        return r;
    }

#ifdef CPP_GAME_SERVER
    int addclient(const char *tag, int cn) { return addclient(tag, (clientinfo *)::getinfo(cn)); }
#else
    int addclient(const char *tag, int cn) { return addclient(tag, game::getclient(cn)); }

    static void removeclient(vector<gamelog *> &log, gameent *d)
    {
        if(!d) return;

        loopvk(log)
        {
            gamelog *e = log[k];
            loopvj(e->tags)
            {
                taginfo &t = e->tags[j];
                loopv(t.groups)
                {
                    groupinfo &g = t.groups[i];
                    int f = findinfo(g.infos, "clientnum");
                    if(!g.infos.inrange(f) || g.infos[f].getint() != d->clientnum) continue;

                    int q = findinfo(g.infos, "active");
                    if(g.infos.inrange(q))
                    {
                        g.infos[q].set(false); // set as inactive
                        continue;
                    }

                    addinfo(g.infos, "active", false); // add if not present
                }
            }
        }
    }

    static void removeclient(gameent *d)
    {
        if(!d) return;

        #define GAMELOG_DEF(_type, _name) \
            case _type: gamelog::removeclient(_name, d); break;

        loopk(GAMELOG_MAX) switch(k)
        {
            GAMELOG_DEF(GAMELOG_EVENT, eventlog);
            GAMELOG_DEF(GAMELOG_MESSAGE, messagelog);
            GAMELOG_DEF(GAMELOG_DEATH, deathlog);
            default: break;
        }

        #undef GAMELOG_DEF
    }
#endif
};

#ifdef CPP_GAME_MAIN

#define GAMELOG_DEF(_MACRO) \
    _MACRO(event); \
    _MACRO(message); \
    _MACRO(death);

#define GAMELOG_LOOP(_MACRO) \
    _MACRO(event, , loopcsv); \
    _MACRO(message, , loopcsv); \
    _MACRO(death, , loopcsv); \
    _MACRO(event, rev, loopcsvrev); \
    _MACRO(message, rev, loopcsvrev); \
    _MACRO(death, rev, loopcsvrev);

#define GAMELOG_VALS(logt) \
    ICOMMANDV(0, logt##count, logt##log.length()); \
    ICOMMAND(0, get##logt##millis, "i", (int *val), intret(logt##log.inrange(*val) ? logt##log[*val]->millis : -1)); \
    ICOMMAND(0, get##logt##seqid, "i", (int *val), intret(logt##log.inrange(*val) ? logt##log[*val]->seqid : -1)); \
    ICOMMAND(0, get##logt##tags, "i", (int *val), intret(logt##log.inrange(*val) ? logt##log[*val]->tags.length() : -1)); \
    ICOMMAND(0, get##logt##tag, "isis", (int *val, char *tag, int *grp, char *name), \
    { \
        gamelog::info *n = gamelog::gettaginfo(logt##log, *val, tag, *grp, name); \
        if(n) n->comret(); \
    }); \
    ICOMMAND(0, get##logt##tagid, "isii", (int *val, char *tag, int *grp, int *idx), \
    { \
        gamelog::info *n = gamelog::gettaginfo(logt##log, *val, tag, *grp, *idx); \
        if(n) n->comret(); \
    }); \
    ICOMMAND(0, get##logt##taggroups, "is", (int *val, char *tag), \
    { \
        gamelog::taginfo *t = gamelog::gettag(logt##log, *val, tag); \
        if(!t) return; \
        intret(t->groups.length()); \
    }); \
    ICOMMAND(0, get##logt##taggroupinfos, "isi", (int *val, char *tag, int *grp), \
    { \
        gamelog::groupinfo *g = gamelog::getgroup(logt##log, *val, tag, *grp); \
        if(!g) return; \
        intret(g->infos.length()); \
    }); \
    ICOMMAND(0, get##logt##name, "isibib", (int *val, char *tag, int *grp, int *col, int *icon, int *dupname), \
    { \
        gamelog::info *name = gamelog::gettaginfo(logt##log, *val, tag, *grp, "name"); \
        if(!name) return; \
        gamelog::info *clientnum = gamelog::gettaginfo(logt##log, *val, tag, *grp, "clientnum"); \
        if(!clientnum) return; \
        gamelog::info *team = gamelog::gettaginfo(logt##log, *val, tag, *grp, "team"); \
        if(!team) return; \
        gamelog::info *actortype = gamelog::gettaginfo(logt##log, *val, tag, *grp, "actortype"); \
        if(!actortype) return; \
        gamelog::info *colour = gamelog::gettaginfo(logt##log, *val, tag, *grp, "colour"); \
        if(!colour) return; \
        gamelog::info *privilege = gamelog::gettaginfo(logt##log, *val, tag, *grp, "privilege"); \
        if(!privilege) return; \
        gamelog::info *weapselect = gamelog::gettaginfo(logt##log, *val, tag, *grp, "weapselect"); \
        if(!weapselect) return; \
        result(game::colourname(name->getstr(), clientnum->getint(), team->getint(), actortype->getint(), colour->getint(), privilege->getint(), weapselect->getint(), *icon != 0, *dupname != 0, *col >= 0 ? *col : 3)); \
    }); \
    ICOMMAND(0, get##logt##lists, "i", (int *val), intret(logt##log.inrange(*val) ? logt##log[*val]->lists.length() : -1)); \
    ICOMMAND(0, get##logt##list, "iss", (int *val, char *list, char *name), \
    { \
        gamelog::info *n = gamelog::getlistinfo(logt##log, *val, list, name); \
        if(n) n->comret(); \
    });
GAMELOG_DEF(GAMELOG_VALS);

#define GAMELOG_LOOPS(logt, name, op) \
    ICOMMAND(0, loop##logt##s##name, "iire", (int *count, int *skip, ident *id, uint *body), \
    { \
        loopstart(id, stack); \
        op(logt##log, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    }); \
    ICOMMAND(0, loop##logt##s##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
    { \
        loopstart(id, stack); \
        op(logt##log, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            if(executebool(cond)) execute(body); \
        }); \
        loopend(id, stack); \
    }); \
    ICOMMAND(0, loop##logt##s##name##while, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
    { \
        loopstart(id, stack); \
        op(logt##log, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            if(!executebool(cond)) break; \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
GAMELOG_LOOP(GAMELOG_LOOPS);

#define GAMELOG_LOOPTAGS(logt, name, op) \
    ICOMMAND(0, loop##logt##tags##name, "iiire", (int *val, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog *e = gamelog::getlog(logt##log, *val); \
        if(!e) return; \
        loopstart(id, stack); \
        op(e->tags, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
GAMELOG_LOOP(GAMELOG_LOOPTAGS);

#define GAMELOG_LOOPGROUPS(logt, name, op) \
    ICOMMAND(0, loop##logt##groups##name, "isiire", (int *val, char *tag, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog::taginfo *t = gamelog::gettag(logt##log, *val, tag); \
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
GAMELOG_LOOP(GAMELOG_LOOPGROUPS);

#define GAMELOG_LOOPGROUPINFOS(logt, name, op) \
    ICOMMAND(0, loop##logt##groupinfos##name, "isiiire", (int *val, char *tag, int *grp, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog::groupinfo *g = gamelog::getgroup(logt##log, *val, tag, *grp); \
        if(!g) return; \
        loopstart(id, stack); \
        op(g->infos, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
GAMELOG_LOOP(GAMELOG_LOOPGROUPINFOS);

#define GAMELOG_LOOPLISTS(logt, name, op) \
    ICOMMAND(0, loop##logt##lists##name, "isiiire", (int *val, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog *e = gamelog::getlog(logt##log, *val); \
        if(!e) return; \
        loopstart(id, stack); \
        op(e->lists, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
GAMELOG_LOOP(GAMELOG_LOOPLISTS);

#define GAMELOG_LOOPLISTINFOS(logt, name, op) \
    ICOMMAND(0, loop##logt##listinfos##name, "isiiire", (int *val, char *list, int *count, int *skip, ident *id, uint *body), \
    { \
        gamelog::listinfo *g = gamelog::getlist(logt##log, *val, list); \
        if(!g) return; \
        loopstart(id, stack); \
        op(g->infos, *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
GAMELOG_LOOP(GAMELOG_LOOPLISTINFOS);
#endif
