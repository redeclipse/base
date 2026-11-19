#include "engine.h"
#include "master.h"

#define MFILE_MAXARGS 16
#define MFILE_PRUNEDAYS 60 // how far back past the kept days old dated copies are looked for

// what keeps each of the master's files safe: loaded, it was read, so a save can't put an empty state over one never read;

namespace master
{
    // the files the master owns: it loads them after servinit.cfg on every start and rehash, and saves them whenever what they hold changes
    SVAR(0, masterauthuserfile, "authuser.cfg");
    SVAR(0, masterauthservfile, "authserv.cfg");
    SVAR(0, masterservctrlfile, "servctrl.cfg");
    SVAR(0, mastershieldfile, "shield.cfg"); // Master shield's modulators and settings, which administrators change on the website
    VAR(0, masterfilebackups, 0, 14, 365); // days of dated copies kept of each, 0 keeps only the .bak



    static struct masterfilestate
    {
        bool loading, dirty[MFILE_MAX];
        masterfileguard guard[MFILE_MAX];
        int controlsaved; // the control version the file last matched

        masterfilestate() : loading(true), controlsaved(0) { loopi(MFILE_MAX) dirty[i] = false; }
    } mfiles;

    bool masterfileblocked(int type) { return type >= 0 && type < MFILE_MAX && mfiles.guard[type].blocked; }
    void setmasterfilesloading(bool loading) { mfiles.loading = loading; }

    static const char *masterfilename(int type)
    {
        switch(type)
        {
            case MFILE_USER: return masterauthuserfile;
            case MFILE_SERV: return masterauthservfile;
            case MFILE_CTRL: return masterservctrlfile;
            case MFILE_SHIELD: return mastershieldfile;
        }
        return "";
    }

    void dirtymasterfile(int type)
    {
        if(!mfiles.loading && type >= 0 && type < MFILE_MAX) mfiles.dirty[type] = true;
    }

    void addauth(char *name, char *flags, char *pubkey, char *email, char *steamid, char *discord, char *display)
    {
        if(newauth(name, flags, pubkey, email, steamid, discord, display)) dirtymasterfile(MFILE_USER);
    }
    COMMAND(0, addauth, "sssssss");

    void addserverauth(char *name, char *flags, char *pubkey, char *desc, char *owner, char *addrs)
    {
        if(newserverauth(name, flags, pubkey, desc, owner, *addrs ? addrs : NULL)) dirtymasterfile(MFILE_SERV);
    }
    COMMAND(0, addserverauth, "ssssss");

    bool parsecontroladdr(const char *s, enet_uint32 &ip, enet_uint32 &mask)
    {
        union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } addr, bits;
        addr.i = bits.i = 0;
        int octets = 0;
        while(*s)
        {
            if(octets >= 4 || !isdigit(uchar(*s))) return false;
            int n = 0, digits = 0;
            while(isdigit(uchar(*s))) { n = n*10 + (*s++ - '0'); if(++digits > 3) return false; }
            if(n > 255) return false;
            addr.b[octets] = n;
            bits.b[octets] = 0xFF;
            octets++;
            if(*s == '.') { s++; if(!*s) return false; }
            else if(*s) return false;
        }
        ip = addr.i;
        mask = bits.i;
        return true;
    }

    static int controltype(const char *cmd)
    {
        if(strncmp(cmd, "add", 3)) return -1;
        loopi(ipinfo::MAXTYPES) if(!strcmp(cmd+3, ipinfotypes[i])) return i;
        return -1;
    }

    // what added one of the master's own entries, as the game servers' Web API names it; -1 when it wasn't recorded
    const char * const controlsources[ipinfo::SRC_MAX] = { "server", "player", "automatic", "master" };

    static int controlsource(const char *name)
    {
        loopi(ipinfo::SRC_MAX) if(!strcmp(name, controlsources[i])) return i;
        return -1;
    }

    // one line of a file: checks it and, if apply is set, adds what it holds; false means the line is bad
    static bool masterfileline(int type, vector<char *> &args, bool apply)
    {
        const char *cmd = args[0];
        #define ARG(n) (args.inrange(n) ? args[n] : "")
        switch(type)
        {
            case MFILE_USER:
            {
                if(strcmp(cmd, "addauth") || args.length() < 4 || !*ARG(1) || !*ARG(2) || !validpubkey(ARG(3))) return false;
                if(apply && newauth(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)) && *ARG(5) && !validsteamid(ARG(5)))
                    mfiles.dirty[MFILE_USER] = true; // written out without the bad Steam ID on the next save
                return true;
            }
            case MFILE_SERV:
            {
                if(strcmp(cmd, "addserverauth") || args.length() < 4 || !*ARG(1) || !validpubkey(ARG(3))) return false;
                if(apply) newserverauth(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), args.length() > 6 ? args[6] : NULL); // a line saved before known addresses were kept has none
                return true;
            }
            case MFILE_CTRL:
            {
                int ctype = controltype(cmd);
                enet_uint32 ip, mask;
                if(ctype < 0 || args.length() < 2 || !parsecontroladdr(ARG(1), ip, mask)) return false;
                if(apply)
                {
                    loopv(control) if(control[i].flag == ipinfo::LOCAL && control[i].type == ctype && control[i].ip == ip && control[i].mask == mask) return true;
                    addipinfo(control, ctype, ARG(1), ARG(2));
                    ipinfo &c = control.last();
                    c.source = controlsource(ARG(3));
                    copystring(c.addhandle, ARG(4));
                    c.added = c.source >= 0 ? max(atoi(ARG(5)), 0) : 0;
                }
                return true;
            }
            case MFILE_SHIELD: return shieldfileline(args, apply);
        }
        #undef ARG
        return false;
    }

    static const char *masterfilewants(int type)
    {
        switch(type)
        {
            case MFILE_USER: return "addauth";
            case MFILE_SERV: return "addserverauth";
            case MFILE_SHIELD: return "shieldmodulator and Master shield setting";
            default: return "ban, mute, limit, allow, except and trust";
        }
    }

    // -1 when there's no such file, otherwise how many bad lines it has, adding what it holds only if it has none
    static int loadmasterfile(int type, const char *name)
    {
        char *buf = loadfile(name, NULL);
        if(!buf) return -1;
        int errors = 0;
        loopk(2) // a first pass finds bad lines, so a file with any adds nothing
        {
            if(k && errors) break;
            int line = 0;
            for(char *p = buf; *p; )
            {
                char *end = p + strcspn(p, "\r\n");
                char next = *end;
                *end = '\0';
                line++;
                char *text = p;
                while(iscubespace(*text)) text++;
                if(*text && strncmp(text, "//", 2))
                {
                    vector<char *> args;
                    explodelist(text, args, MFILE_MAXARGS);
                    if(args.empty() || !masterfileline(type, args, k != 0))
                    {
                        // only the line's first word, as the rest can be a key or an email address
                        if(!k) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s badline", name), "Bad line %d in %s, starting %s: it only holds %s lines%s", line, name, args.empty() ? "?" : args[0], masterfilewants(type), type == MFILE_SHIELD ? "" : ", settings go in servinit.cfg or secrets.cfg"); errors++; }
                    }
                    args.deletearrays();
                }
                *end = next;
                p = end;
                while(*p == '\r' || *p == '\n') p++;
            }
        }
        delete[] buf;
        return errors;
    }

    // takes names, not paths, as loading and opening find the file themselves
    // owner only, as the master's files hold keys, emails, sign-in tokens and registration links
    static void masterfileprivate(const char *name)
    {
    #ifndef WIN32
        chmod(findfile(name, "w"), S_IRUSR|S_IWUSR);
    #endif
    }

    // a file the master owns, opened to write and made owner only before anything goes in it
    stream *openmasterfile(const char *name, bool gz)
    {
        stream *f = gz ? opengzfile(name, "wb") : openutf8file(name, "w");
        if(f) masterfileprivate(name);
        return f;
    }

    static bool copymasterfile(const char *from, const char *to)
    {
        size_t len = 0;
        char *buf = loadfile(from, &len, false);
        if(!buf) return false;
        stream *f = openrawfile(to, "wb");
        if(f) masterfileprivate(to);
        bool ok = f && f->write(buf, len) == len;
        if(f) delete f;
        delete[] buf;
        return ok;
    }

    // a file in another version's format is read as empty rather than bad, so it's replaced on the next save instead of blocking it
    int masterfileolder(stream *f, const char *name)
    {
        delete f;
        foldnoticef("master", NOTICE_WARNING, tempformatstring("file %s older", name), "Read %s as empty, as it was saved in another version's format", name);
        return 0;
    }

    // -1 for a file that isn't there, 1 for one that is but won't open, which is as bad as one that won't read
    int masterfilemissing(const char *name) { return fileexists(findfile(name, "r"), "r") ? 1 : -1; }

    // loads a file the master owns, or its .bak when it won't read, keeping the bad one apart as .broken; load gives -1 for a missing
    // file, 0 when it read and more when it didn't, leaving nothing loaded then; true when the .bak was loaded, so a save puts it back
    bool loadguarded(const char *name, int (*load)(const char *, int), int arg, masterfileguard &g, const char *until)
    {
        g.restored = g.blocked = g.warned = false;
        g.loaded = true;
        int errors = load(name, arg);
        if(!errors) return false;
        defformatstring(bakname, "%s.bak", name);
        if(errors > 0)
        {
            // keep the bad file, as the next save would otherwise put the .bak's contents over it
            string stamp;
            time_t now = time(NULL);
            strftime(stamp, sizeof(stamp), "%Y%m%d%H%M%S", gmtime(&now));
            defformatstring(brokenname, "%s.%s.broken", name, stamp);
            if(copymasterfile(name, brokenname)) foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s kept", name), "Kept %s, which couldn't be read, as %s", name, brokenname);
        }
        int bakerrors = load(bakname, arg);
        if(!bakerrors)
        {
            foldnoticef("master", NOTICE_WARNING, tempformatstring("file %s bak", name), "Loaded %s from %s, as it couldn't be read", name, bakname);
            g.restored = true;
            return true;
        }
        if(errors > 0 || bakerrors > 0)
        {
            g.blocked = true; // saving now would put next to nothing over them
            foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s failload", name), "Failed to load %s or %s, nothing was loaded from them and %s won't be saved until %s", name, bakname, name, until);
        }
        return false;
    }

    // whether a save has to wait, said once
    bool saveblocked(masterfileguard &g, const char *name)
    {
        if(!g.loaded) return true;
        if(!g.blocked) return false;
        if(!g.warned) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s notsaving", name), "Not saving %s, as it couldn't be loaded", name); g.warned = true; }
        return true;
    }

    static int loadmasterfilearg(const char *name, int type) { return loadmasterfile(type, name); }

    static void loadmasterfiletype(int type)
    {
        mfiles.guard[type] = masterfileguard();
        const char *name = masterfilename(type);
        if(!*name) return;
        if(type == MFILE_SHIELD) shieldloading();
        if(loadguarded(name, loadmasterfilearg, type, mfiles.guard[type], "a rehash loads it")) mfiles.dirty[type] = true; // puts a good copy back in its place
        if(type == MFILE_SHIELD) shieldloaded();
    }

    void loadfiles()
    {
        if(masterserver) loopi(MFILE_MAX) loadmasterfiletype(i);
        mfiles.loading = false;
        mfiles.controlsaved = getcontrolversion();
    }

    static void writeauths(stream *f, hashnameset<authuser> &users, bool server)
    {
        vector<authuser *> sorted;
        enumerate(users, authuser, u, sorted.add(&u));
        sorted.sort([](const authuser *a, const authuser *b) { return strcmp(a->name, b->name) < 0; });
        loopv(sorted)
        {
            authuser &u = *sorted[i];
            f->printf("%s %s", server ? "addserverauth" : "addauth", escapestring(u.name));
            f->printf(" %s", escapestring(u.flags));
            f->printf(" %s", escapestring(u.key));
            if(server)
            {
                f->printf(" %s", escapestring(u.desc ? u.desc : ""));
                f->printf(" %s", escapestring(u.owner ? u.owner : ""));
                if(u.addrs) f->printf(" %s", escapestring(u.addrs));
            }
            else
            {
                f->printf(" %s", escapestring(u.email ? u.email : ""));
                f->printf(" %s", escapestring(u.steamid ? u.steamid : ""));
                f->printf(" %s", escapestring(u.discord ? u.discord : ""));
                f->printf(" %s", escapestring(u.display ? u.display : ""));
            }
            f->printf("\n");
        }
    }

    static void writecontrols(stream *f)
    {
        loopv(control) if(control[i].flag == ipinfo::LOCAL)
        {
            ipinfo &c = control[i];
            string addr;
            controladdr(c, addr);
            f->printf("add%s %s", ipinfotypes[clamp(c.type, 0, int(ipinfo::MAXTYPES)-1)], escapestring(addr));
            f->printf(" %s", escapestring(c.reason ? c.reason : ""));
            if(c.source >= 0 && c.source < ipinfo::SRC_MAX) f->printf(" %s %s %d", controlsources[c.source], escapestring(c.addhandle), c.added);
            f->printf("\n");
        }
    }

    // the day's first save keeps a dated copy of what was there, and the oldest copies go
    static void datedmastercopy(const char *name)
    {
        if(masterfilebackups <= 0) return;
        time_t now = time(NULL);
        string stamp;
        strftime(stamp, sizeof(stamp), "%Y%m%d", gmtime(&now));
        defformatstring(dated, "%s.%s.bak", name, stamp);
        if(fileexists(findfile(dated, "w"), "r")) return;
        if(!copymasterfile(name, dated)) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s dated", name), "Failed to keep a dated copy of %s", name); return; }
        for(int d = masterfilebackups; d < masterfilebackups + MFILE_PRUNEDAYS; d++)
        {
            time_t then = now - time_t(d)*24*60*60;
            strftime(stamp, sizeof(stamp), "%Y%m%d", gmtime(&then));
            defformatstring(old, "%s.%s.bak", name, stamp);
            remove(findfile(old, "w"));
        }
    }

    // a file written as its .new moved into place, the one it replaces kept as its .bak, and a dated copy the day's first time when asked
    void commitmasterfile(const char *name, masterfileguard &g, bool dated)
    {
        defformatstring(newname, "%s.new", name);
        defformatstring(bakname, "%s.bak", name);
        string from, to, bak;
        copystring(from, findfile(newname, "w"));
        copystring(to, findfile(name, "w"));
        copystring(bak, findfile(bakname, "w"));
        if(g.restored) remove(to); // the bad file was kept apart, and the .bak is the good copy
        else if(fileexists(to, "r"))
        {
            if(dated) datedmastercopy(name);
            remove(bak);
            if(rename(to, bak)) foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s movebak", name), "Failed to move %s to %s", name, bakname);
            else masterfileprivate(bakname);
        }
        g.restored = false;
        if(rename(from, to)) foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s movein", name), "Failed to move %s into %s", newname, name);
    }

    static void savemasterfile(int type)
    {
        mfiles.dirty[type] = false;
        const char *name = masterfilename(type);
        if(!*name) return;
        if(saveblocked(mfiles.guard[type], name)) return;
        defformatstring(newname, "%s.new", name);
        stream *f = openmasterfile(newname, false);
        if(!f) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s save", newname), "Failed to save %s", newname); return; }
        f->printf("// written by the master server, edit only while it's stopped or rehash straight after\n");
        switch(type)
        {
            case MFILE_USER: writeauths(f, authusers, false); break;
            case MFILE_SERV: writeauths(f, serverauthusers, true); break;
            case MFILE_CTRL: writecontrols(f); break;
            case MFILE_SHIELD: writeshield(f); break;
        }
        bool ok = f->flush();
        delete f;
        if(!ok) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s write", name), "Failed to write %s, %s is unchanged", newname, name); remove(findfile(newname, "w")); return; }
        commitmasterfile(name, mfiles.guard[type], true);
    }

    void savemasterfiles()
    {
        if(mfiles.loading || !masterserver) return;
        int version = getcontrolversion();
        if(version != mfiles.controlsaved) { mfiles.controlsaved = version; mfiles.dirty[MFILE_CTRL] = true; }
        loopi(MFILE_MAX) if(mfiles.dirty[i]) savemasterfile(i);
    }

}
