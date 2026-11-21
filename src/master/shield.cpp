#include "engine.h"
#include "master.h"

#define NOTICE_KEEP 100
#define NOTICE_TEXTLEN 1500 // leaves room in a Discord post for the bot's own words
#define NOTICE_MAXFOLDS 1024
#define SHIELD_HOUR 3600
#define SHIELD_LAPSECHECK 60000
#define SHIELD_CHURN_MAX 4096

namespace master
{
    // Admin notices: what the master's administrators should see, sent on the live list to viewers at masternoticelock,
    // with the last few kept so one that was away can ask for those after the last id it saw. What can happen over and over, such as
    // failed sign-ins, is folded: the first is sent at once, and the rest within masternoticefold seconds go as one line after
    VAR(0, masternoticelock, 0, PRIV_ADMINISTRATOR, PRIV_DEVELOPER+1);
    VAR(0, masternoticefold, 0, 3600, VAR_MAX); // seconds repeats of a notice are folded into one line, in the log too, 0 sends and logs every one


    static const char * const noticelevels[NOTICE_LEVELS] = { "info", "warning", "error" };

    struct adminnotice
    {
        int id, time, level;
        string kind;
        char *text;

        adminnotice() : id(0), time(0), level(NOTICE_INFO), text(NULL) { kind[0] = '\0'; }
        ~adminnotice() { DELETEA(text); }
    };

    // a notice that was sent, and how many more like it came since
    struct noticefold
    {
        string key, kind;
        int level, count;
        time_t start;
        char *last;
        bool logged; // repeats were kept out of the log too, so the window's end goes in it

        noticefold() : level(NOTICE_INFO), count(0), start(0), last(NULL), logged(false) { key[0] = kind[0] = '\0'; }
        ~noticefold() { DELETEA(last); }
    };

    static struct noticestate
    {
        vector<adminnotice *> kept;
        vector<noticefold *> folds;
        int lastid;

        noticestate() : lastid(0) {}
    } notices;

    static void writenotice(vector<char> &buf, const adminnotice &n)
    {
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "notice");
        w.num("id", n.id);
        w.num("time", n.time);
        w.str("kind", n.kind);
        w.str("level", noticelevels[clamp(n.level, 0, NOTICE_LEVELS-1)]);
        w.str("text", n.text);
        w.endobject();
    }

    static void noticeline(int level, const char *text)
    {
        conoutf(level == NOTICE_ERROR ? colourred : (level == NOTICE_WARNING ? colourorange : colourwhite), "%s", text);
    }

    // the line goes in the log as it always has, and to the administrators watching
    void noticef(const char *kind, int level, const char *fmt, ...)
    {
        bigstring text;
        va_list args;
        va_start(args, fmt);
        vformatstring(text, fmt, args);
        va_end(args);
        noticeline(level, text);
        postnotice(kind, level, text);
    }

    // posts the first for each key in masternoticefold seconds, counting the rest, with the line in the log when logged is set
    static void addfold(const char *kind, int level, const char *key, const char *text, bool logged)
    {
        if(!masternoticefold)
        {
            if(logged) noticeline(level, text);
            postnotice(kind, level, text);
            return;
        }
        // with too many at once, as from a scan by many addresses, the rest of that kind share one
        string fullkey;
        if(notices.folds.length() >= NOTICE_MAXFOLDS) formatstring(fullkey, "%s *", kind);
        else formatstring(fullkey, "%s %s", kind, key);
        loopv(notices.folds) if(!strcmp(notices.folds[i]->key, fullkey))
        {
            noticefold &f = *notices.folds[i];
            f.count++;
            f.level = max(f.level, level);
            if(logged) f.logged = true;
            DELETEA(f.last);
            f.last = newstring(text);
            return;
        }
        noticefold *f = notices.folds.add(new noticefold);
        copystring(f->key, fullkey);
        copystring(f->kind, kind);
        f->level = level;
        f->start = time(NULL);
        if(logged) noticeline(level, text);
        postnotice(kind, level, text);
    }

    // as noticef, but folded in the log as for the administrators, so a flood of repeats writes one line a window
    void foldnoticef(const char *kind, int level, const char *key, const char *fmt, ...)
    {
        bigstring text;
        va_list args;
        va_start(args, fmt);
        vformatstring(text, fmt, args);
        va_end(args);
        addfold(kind, level, key, text, true);
    }

    // a folded notice whose line in the log says more than the administrators may see, so it was logged apart
    void foldnotice(const char *kind, int level, const char *key, const char *text)
    {
        addfold(kind, level, key, text, false);
    }

    static void foldwindow(string &s)
    {
        if(masternoticefold % 3600 == 0) formatstring(s, masternoticefold == 3600 ? "hour" : "%d hours", masternoticefold / 3600);
        else if(masternoticefold % 60 == 0) formatstring(s, masternoticefold == 60 ? "minute" : "%d minutes", masternoticefold / 60);
        else formatstring(s, "%d seconds", masternoticefold);
    }

    // a window with repeats says how many and the last of them, and starts another, so something that keeps on happening is said once a window
    void checknotices()
    {
        time_t now = time(NULL);
        loopv(notices.folds)
        {
            noticefold &f = *notices.folds[i];
            if(masternoticefold && now - f.start < masternoticefold) continue;
            if(!f.count || !f.last) { delete notices.folds.remove(i--); continue; }
            string window;
            foldwindow(window);
            bigstring text;
            formatstring(text, "%d more like this in the last %s, the last: %s", f.count, window, f.last);
            if(f.logged) noticeline(f.level, text);
            postnotice(f.kind, f.level, text);
            f.logged = false;
            f.count = 0;
            f.start = now;
            DELETEA(f.last);
        }
    }

    void postnotice(const char *kind, int level, const char *text)
    {
        adminnotice *n = new adminnotice;
        // ids follow the clock, so after a restart they still come after any a watcher has already seen
        n->time = int(time(NULL));
        n->id = notices.lastid = max(notices.lastid + 1, n->time);
        n->level = level;
        copystring(n->kind, kind);
        // cut short on a whole character
        size_t len = strlen(text);
        if(len > NOTICE_TEXTLEN)
        {
            len = NOTICE_TEXTLEN - 3;
            while(len > 0 && (uchar(text[len]) & 0xC0) == 0x80) len--;
            n->text = new char[len + 4];
            memcpy(n->text, text, len);
            memcpy(n->text + len, "...", 4);
        }
        else n->text = newstring(text);
        notices.kept.add(n);
        while(notices.kept.length() > NOTICE_KEEP) delete notices.kept.remove(0);
        vector<char> buf;
        writenotice(buf, *n);
        broadcastnotice(masternoticelock, buf.getbuf(), buf.length());
    }

    // a viewer that says ?since=<id> gets every kept notice after it
    void noticeviewerjoined(httpwsevent &e)
    {
        if(!e.req || e.req->privilege < masternoticelock) return;
        const char *since = e.req->var("since");
        if(!since || !*since) return;
        int after = atoi(since);
        loopv(notices.kept) if(notices.kept[i]->id > after)
        {
            vector<char> buf;
            writenotice(buf, *notices.kept[i]);
            http::wssend(e.id, buf.getbuf(), buf.length());
        }
    }

    // Master shield: named Shield modulators test each listed server, and ban or flag it. The master keeps them and these settings in
    // mastershieldfile, which administrators change on the website. A modulator's body runs in the Sandbox using shieldinfo,
    // shieldplayer, shieldvar and shieldcontrol, and its result is what to do: nothing, "ban <reason>" or "flag <note>"
    VAR(0, mastershieldlock, 0, PRIV_ADMINISTRATOR, PRIV_MAX); // Privilege that may see and change Master shield, PRIV_MAX for nobody
    VAR(0, shielddryrun, 0, 1, 1); // only say what would be banned
    VAR(0, shieldbanlimit, 0, 3, VAR_MAX); // bans in any hour by all modulators together, 0 for no limit
    VAR(0, shieldmodulatorlimit, 0, 0, VAR_MAX); // bans in any hour by one modulator, 0 for no limit
    VAR(0, shieldbanshare, 0, 10, 100); // the most of the list one pass may ban, as a percentage, though one ban is always allowed; 0 for no limit
    VAR(0, shieldbanlapse, 0, 90, VAR_MAX); // days before a ban it made lapses, 0 to keep them

    const char * const shieldsettings[] = { "shielddryrun", "shieldbanlimit", "shieldmodulatorlimit", "shieldbanshare", "shieldbanlapse" };

    struct ipchurn
    {
        enet_uint32 host;
        vector<int> times;
    };
    static vector<ipchurn *> shieldchurn;

    static void prunetimes(vector<int> &times, int now)
    {
        while(times.length() && now - times[0] >= SHIELD_HOUR) times.remove(0);
    }

    void noteauthfail(masterclient &c)
    {
        int now = int(time(NULL));
        prunetimes(c.authfails, now);
        c.authfails.add(now);
        c.changed = true;
    }

    void notereconnect(enet_uint32 host)
    {
        int now = int(time(NULL));
        ipchurn *found = NULL;
        loopvrev(shieldchurn)
        {
            prunetimes(shieldchurn[i]->times, now);
            if(shieldchurn[i]->host == host) found = shieldchurn[i];
            else if(shieldchurn[i]->times.empty()) delete shieldchurn.remove(i);
        }
        if(!found)
        {
            if(shieldchurn.length() >= SHIELD_CHURN_MAX) delete shieldchurn.remove(0);
            found = shieldchurn.add(new ipchurn);
            found->host = host;
        }
        found->times.add(now);
    }

    static int ipreconnects(enet_uint32 host)
    {
        int now = int(time(NULL));
        loopv(shieldchurn) if(shieldchurn[i]->host == host)
        {
            prunetimes(shieldchurn[i]->times, now);
            return shieldchurn[i]->times.length();
        }
        return 0;
    }

    static const enet_uint32 SUBNET24_MASK = ENET_HOST_TO_NET_32(0xFFFFFF00U);

    void marksiblingschanged(enet_uint32 host, const char *handle)
    {
        enet_uint32 net = host & SUBNET24_MASK;
        loopv(masterclients)
        {
            masterclient &s = *masterclients[i];
            if(!s.listserver) continue;
            if((s.address.host & SUBNET24_MASK) == net || (handle && *handle && !strcasecmp(s.authhandle, handle))) s.changed = true;
        }
    }


    const int numshieldsettings = sizeof(shieldsettings)/sizeof(shieldsettings[0]);
    shieldstate shield;

    // a name for the log and the website, such as "Map Escape Exploit": plain text, filtered into dst; false when nothing is left
    bool shieldname(char *dst, const char *name)
    {
        if(strlen(name) > SHIELD_NAMELEN) return false;
        filterstring(dst, name, false, true, true, true, true, SHIELD_NAMELEN);
        return dst[0] != '\0';
    }

    // a GitHub issue or security advisory, https://github.com/<owner>/<repo>/issues/<n> or .../security/advisories/GHSA-..., or none
    bool shieldlink(const char *link)
    {
        if(!*link) return true;
        const char *p = link;
        if(strncmp(p, "https://github.com/", 19) || strlen(link) >= MAXSTRLEN) return false;
        p += 19;
        loopi(2)
        {
            int n = strspn(p, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.");
            if(!n || p[n] != '/') return false;
            p += n + 1;
        }
        if(!strncmp(p, "issues/", 7)) { p += 7; int n = strspn(p, "0123456789"); return n > 0 && !p[n]; }
        if(!strncmp(p, "security/advisories/GHSA-", 25)) { p += 25; int n = strspn(p, "abcdefghijklmnopqrstuvwxyz0123456789-"); return n > 0 && !p[n]; }
        return false;
    }

    shieldmodulator *addshieldmodulator(const char *name, const char *link, bool enabled, const char *body)
    {
        shieldmodulator *m = shield.modulators.add(new shieldmodulator);
        copystring(m->name, name);
        copystring(m->link, link);
        m->enabled = enabled;
        m->body = newstring(body);
        // one a rehash loaded again keeps its id, so a page open on the website still reaches it, and its recent bans
        loopv(shield.old) if(!strcmp(shield.old[i]->name, name)) { m->id = shield.old[i]->id; m->bans = shield.old[i]->bans; break; }
        if(!m->id) m->id = ++shield.lastid;
        return m;
    }

    bool shieldfileline(vector<char *> &args, bool apply)
    {
        const char *cmd = args[0];
        if(!strcmp(cmd, "shieldmodulator")) // name, on or off, link and body
        {
            string name;
            if(args.length() < 5 || !shieldname(name, args[1]) || !shieldlink(args[3]) || strlen(args[4]) > SHIELD_BODYLEN) return false;
            if(apply && !shield.find(name)) addshieldmodulator(name, args[3], atoi(args[2]) != 0, args[4]);
            return true;
        }
        loopi(sizeof(shieldsettings)/sizeof(shieldsettings[0])) if(!strcmp(cmd, shieldsettings[i]))
        {
            if(args.length() < 2) return false;
            if(apply) setvar(cmd, atoi(args[1]), true, false, true);
            return true;
        }
        return false;
    }

    void writeshield(stream *f)
    {
        loopi(sizeof(shieldsettings)/sizeof(shieldsettings[0])) f->printf("%s %d\n", shieldsettings[i], getvar(shieldsettings[i]));
        loopv(shield.modulators)
        {
            shieldmodulator &m = *shield.modulators[i];
            f->printf("shieldmodulator %s %d", escapestring(m.name), m.enabled ? 1 : 0);
            f->printf(" %s", escapestring(m.link));
            f->printf(" %s\n", escapestring(m.body ? m.body : ""));
        }
    }

    // a rehash replaces every modulator with what the file says now
    void shieldloading()
    {
        shield.old.deletecontents();
        shield.old.move(shield.modulators);
    }

    void shieldloaded()
    {
        shield.old.deletecontents();
        bool same = shield.loaded.length() == shield.modulators.length();
        if(same) loopv(shield.modulators) if(strcmp(shield.loaded[i], shield.modulators[i]->name)) { same = false; break; }
        if(!same)
        {
            shield.loaded.deletearrays();
            string names = "";
            loopv(shield.modulators)
            {
                shield.loaded.add(newstring(shield.modulators[i]->name));
                concformatstring(names, "%s'%s'", i ? ", " : "", shield.modulators[i]->name);
            }
            if(shield.modulators.empty()) noticef("shield", NOTICE_INFO, "Master shield has no modulators");
            else noticef("shield", NOTICE_INFO, "Master shield has %d modulator(s): %s", shield.modulators.length(), names);
        }
        shield.rescan = true;
    }

    static masterclient *shieldcur = NULL;
    static jsonvalue *shieldjson = NULL;

    static int shieldbuildnum(int idx)
    {
        if(!shieldjson) return 0;
        jsonvalue *b = shieldjson->get("build");
        if(!b || b->type != JSON_ARRAY || !b->at(idx)) return 0;
        return jsonint(b->at(idx)->num);
    }

    ICOMMAND(IDF_SAFE, shieldinfo, "s", (const char *field),
    {
        if(!shieldcur || !field || !*field) { result(""); return; }
        masterclient &c = *shieldcur;
        jsonvalue *st = shieldjson;
        jsonvalue *cfg = st ? st->get("config") : NULL;
        if(cfg && cfg->type != JSON_OBJECT) cfg = NULL;
        // Identity & registration
        if(!strcmp(field, "host")) { result(c.name); return; }
        if(!strcmp(field, "port")) { intret(c.port); return; }
        if(!strcmp(field, "desc")) { result(c.desc); return; }
        if(!strcmp(field, "branch")) { result(c.branch); return; }
        if(!strcmp(field, "version")) { intret(c.version); return; }
        if(!strcmp(field, "gameversion")) { intret(st ? st->getint("gameversion") : 0); return; }
        if(!strcmp(field, "build")) { defformatstring(s, "%d.%d.%d", shieldbuildnum(0), shieldbuildnum(1), shieldbuildnum(2)); result(s); return; }
        if(!strcmp(field, "buildmajor")) { intret(shieldbuildnum(0)); return; }
        if(!strcmp(field, "buildminor")) { intret(shieldbuildnum(1)); return; }
        if(!strcmp(field, "buildpatch")) { intret(shieldbuildnum(2)); return; }
        if(!strcmp(field, "platform")) { intret(shieldbuildnum(3)); return; }
        if(!strcmp(field, "arch")) { intret(shieldbuildnum(4)); return; }
        if(!strcmp(field, "handle")) { result(c.authhandle); return; }
        if(!strcmp(field, "flags")) { result(c.flags); return; }
        if(!strcmp(field, "priority")) { intret(c.priority()); return; }
        // Link & operator
        if(!strcmp(field, "linked")) { intret(c.link >= 0 ? 1 : 0); return; }
        if(!strcmp(field, "secure")) { intret(c.linksecure ? 1 : 0); return; }
        if(!strcmp(field, "vouch")) { intret(c.vouch ? 1 : 0); return; }
        if(!strcmp(field, "webport")) { intret(c.webport); return; }
        if(!strcmp(field, "relay")) { intret(c.link >= 0 ? c.relay : 0); return; }
        if(!strcmp(field, "bridge")) { intret(c.link >= 0 ? c.bridge : -1); return; }
        if(!strcmp(field, "url")) { result(st ? st->getstr("url", "") : ""); return; }
        if(!strcmp(field, "motd")) { result(st ? st->getstr("motd", "") : ""); return; }
        // Map & match state
        if(!strcmp(field, "map")) { result(c.lastmap); return; }
        if(!strcmp(field, "maptitle")) { result(st ? st->getstr("maptitle", "") : ""); return; }
        if(!strcmp(field, "mapauthor")) { result(st ? st->getstr("mapauthor", "") : ""); return; }
        if(!strcmp(field, "mapcrc"))
        {
            double crc = st ? st->getnum("mapfilecrc", 0) : 0;
            if(!crc && st) crc = st->getnum("mapcrc", 0);
            defformatstring(s, "%u", uint(crc));
            result(crc ? s : "0");
            return;
        }
        if(!strcmp(field, "mapsize")) { intret(st ? st->getint("mapsize") : 0); return; }
        if(!strcmp(field, "mode")) { intret(c.lastmode); return; }
        if(!strcmp(field, "mutators")) { intret(c.lastmuts); return; }
        if(!strcmp(field, "mastermode")) { intret(st ? st->getint("mastermode", -1) : -1); return; }
        if(!strcmp(field, "state")) { intret(st ? st->getint("state", -1) : -1); return; }
        if(!strcmp(field, "gamevars")) { intret(st ? st->getint("gamevars") : 0); return; }
        if(!strcmp(field, "gamemods")) { intret(st ? st->getint("gamemods") : 0); return; }
        // Capacity & counts
        if(!strcmp(field, "numplayers")) { intret(c.numplayers >= 0 ? c.numplayers : c.lastnames.length()); return; }
        if(!strcmp(field, "maxplayers")) { intret(st ? st->getint("maxplayers") : 0); return; }
        if(!strcmp(field, "playerslots")) { intret(st ? st->getint("playerslots") : 0); return; }
        if(!strcmp(field, "spectators")) { intret(st ? st->getint("spectators") : 0); return; }
        if(!strcmp(field, "bots")) { intret(st ? st->getint("bots") : 0); return; }
        if(!strcmp(field, "reserved")) { intret(st ? st->getint("reserved") : 0); return; }
        if(!strcmp(field, "connections")) { intret(st ? st->getint("connections", -1) : -1); return; }
        // Server config (from linked status)
        if(!strcmp(field, "uptime")) { intret(cfg ? cfg->getint("uptime") : 0); return; }
        if(!strcmp(field, "password")) { intret(cfg && cfg->getbool("password") ? 1 : 0); return; }
        if(!strcmp(field, "open")) { intret(cfg ? cfg->getint("open", -1) : -1); return; }
        if(!strcmp(field, "connectlock")) { intret(cfg ? cfg->getint("connectlock", -1) : -1); return; }
        if(!strcmp(field, "editlock")) { intret(cfg ? cfg->getint("editlock", -1) : -1); return; }
        if(!strcmp(field, "varslock")) { intret(cfg ? cfg->getint("varslock", -1) : -1); return; }
        // Cross-server & behavioral signals
        if(!strcmp(field, "hostservers"))
        {
            int n = 0;
            loopv(masterclients) if(masterclients[i]->listserver && masterclients[i]->address.host == c.address.host) n++;
            intret(n);
            return;
        }
        if(!strcmp(field, "subnetservers"))
        {
            int n = 0;
            enet_uint32 net = c.address.host & SUBNET24_MASK;
            loopv(masterclients) if(masterclients[i]->listserver && (masterclients[i]->address.host & SUBNET24_MASK) == net) n++;
            intret(n);
            return;
        }
        if(!strcmp(field, "handleservers"))
        {
            int n = 0;
            if(c.authhandle[0]) loopv(masterclients) if(masterclients[i]->listserver && !strcasecmp(masterclients[i]->authhandle, c.authhandle)) n++;
            intret(n);
            return;
        }
        if(!strcmp(field, "age")) { intret(c.connected ? max(int(time(NULL)) - c.connected, 0) : 0); return; }
        if(!strcmp(field, "mapchanges")) { prunetimes(c.mapchanges, int(time(NULL))); intret(c.mapchanges.length()); return; }
        if(!strcmp(field, "reconnects")) { intret(ipreconnects(c.address.host)); return; }
        if(!strcmp(field, "pollmisses")) { intret(c.pollmisses); return; }
        if(!strcmp(field, "authfails")) { prunetimes(c.authfails, int(time(NULL))); intret(c.authfails.length()); return; }
        result("");
    });

    ICOMMAND(IDF_SAFE, shieldplayer, "ss", (const char *idx, const char *field),
    {
        if(!shieldcur) { result(""); return; }
        masterclient &c = *shieldcur;
        jsonvalue *players = shieldjson ? shieldjson->get("players") : NULL;
        if(players && players->type != JSON_ARRAY) players = NULL;
        int count = players ? players->length() : c.lastnames.length();
        if(!idx || !*idx) { intret(count); return; }
        int i = atoi(idx);
        if(i < 0 || i >= count) { result(""); return; }
        const char *f = field && *field ? field : "name";
        if(!players)
        {
            if(!strcmp(f, "name")) result(c.lastnames[i]);
            else result("");
            return;
        }
        jsonvalue *p = players->at(i);
        if(!p || p->type != JSON_OBJECT) { result(""); return; }
        if(!strcmp(f, "name")) { result(p->getstr("name", "")); return; }
        if(!strcmp(f, "handle")) { result(p->getstr("handle", "")); return; }
        if(!strcmp(f, "verified"))
        {
            const char *h = p->getstr("handle", "");
            bool ok = false;
            if(*h) loopvj(c.verified) if(!strcmp(c.verified[j], h)) { ok = true; break; }
            intret(ok ? 1 : 0);
            return;
        }
        if(!strcmp(f, "privilege")) { result(p->getstr("privilege", "")); return; }
        if(!strcmp(f, "local")) { intret(p->getbool("local") ? 1 : 0); return; }
        if(!strcmp(f, "team")) { result(p->getstr("team", "")); return; }
        if(!strcmp(f, "spectator")) { intret(p->getbool("spectator") ? 1 : 0); return; }
        if(!strcmp(f, "colour")) { intret(p->getint("colour", -1)); return; }
        if(!strcmp(f, "cn")) { intret(p->getint("cn", -1)); return; }
        if(!strcmp(f, "ip")) { result(p->getstr("ip", "")); return; }
        result("");
    });

    ICOMMAND(IDF_SAFE, shieldvar, "s", (const char *name),
    {
        if(!shieldcur || !shieldjson) { result(""); return; }
        jsonvalue *mods = shieldjson->get("mods");
        if(!mods || mods->type != JSON_OBJECT) { result(""); return; }
        if(!name || !*name)
        {
            vector<char> list;
            loopv(mods->items)
            {
                if(!mods->items[i] || !mods->items[i]->key) continue;
                if(list.length()) list.add(' ');
                const char *quoted = escapestring(mods->items[i]->key);
                list.put(quoted, strlen(quoted));
            }
            list.add('\0');
            result(list.getbuf());
            return;
        }
        jsonvalue *v = mods->get(name);
        if(!v) { result(""); return; }
        if(v->type == JSON_STRING) { result(v->str ? v->str : ""); return; }
        if(v->type == JSON_NUMBER)
        {
            if(v->str && *v->str) result(v->str);
            else { defformatstring(s, "%g", v->num); result(s); }
            return;
        }
        if(v->type == JSON_TRUE) { intret(1); return; }
        if(v->type == JSON_FALSE) { intret(0); return; }
        result("");
    });

    ICOMMAND(IDF_SAFE, shieldcontrol, "ss", (const char *type, const char *ip),
    {
        if(!shieldcur || !type || !*type) { intret(0); return; }
        int ctype = -1;
        loopi(ipinfo::MAXTYPES) if(!strcmp(type, ipinfotypes[i])) { ctype = i; break; }
        if(ctype < 0) { intret(0); return; }
        enet_uint32 host = shieldcur->address.host;
        if(ip && *ip)
        {
            ENetAddress addr;
            addr.host = ENET_HOST_ANY;
            addr.port = 0;
            if(enet_address_set_host_ip(&addr, ip) < 0) { intret(0); return; }
            host = addr.host;
        }
        intret(checkipinfo(control, ctype, host) ? 1 : 0);
    });

    // runs a body on one server in the Sandbox, giving what it said to do and the reason or note with it
    int shieldrun(const char *body, masterclient &c, char *text)
    {
        text[0] = '\0';
        const char *raw = c.fullstatus ? c.fullstatus : (c.status ? c.status : c.polled);
        shieldcur = &c;
        shieldjson = raw ? json::parse(raw) : NULL;
        char *ret = NULL;
        WITHMAP(ret = executestr(body));
        shieldcur = NULL;
        DELETEP(shieldjson);
        vector<char *> words;
        explodelist(ret ? ret : "", words);
        int action = SHIELD_NONE;
        if(words.length())
        {
            if(!strcmp(words[0], "ban")) action = SHIELD_BAN;
            else if(!strcmp(words[0], "flag")) action = SHIELD_FLAG;
            else action = SHIELD_BAD;
            string said = "";
            for(int i = action == SHIELD_BAD ? 0 : 1; i < words.length(); i++) concformatstring(said, "%s%s", said[0] ? " " : "", words[i]);
            filterstring(text, said, true, true, true, true, false, MAXSTRLEN-1);
        }
        words.deletearrays();
        DELETEA(ret);
        return action;
    }

    // true the first time a server gets this from a modulator since its state changed
    static bool shieldnew(masterclient &c, const char *what)
    {
        loopv(c.lastflags) if(!strcmp(c.lastflags[i], what)) return false;
        c.lastflags.add(newstring(what));
        return true;
    }

    static void prunebans(vector<int> &bans, int now)
    {
        while(bans.length() && now - bans[0] >= SHIELD_HOUR) bans.remove(0);
    }

    // what a modulator said to do with a server; true when it was banned, so no more modulators run on it
    static bool shieldact(masterclient &c, shieldmodulator &m, int action, const char *text)
    {
        if(action == SHIELD_FLAG)
        {
            if(shieldnew(c, tempformatstring("%s flag %s", m.name, text))) noticef("shield", NOTICE_WARNING, "Master shield modulator '%s' flagged %s:%d (%s) on map '%s': %s", m.name, c.name, c.port, serverdetails(c), c.lastmap, text);
            return false;
        }
        if(action == SHIELD_BAD)
        {
            if(shieldnew(c, tempformatstring("%s bad %s", m.name, text))) noticef("shield", NOTICE_WARNING, "Master shield modulator '%s' gave %s:%d (%s) a result that isn't ban or flag: %s", m.name, c.name, c.port, serverdetails(c), text);
            return false;
        }
        if(action != SHIELD_BAN) return false;
        string reason;
        copystring(reason, *text ? text : "Banned by the master server");
        const char *held = NULL;
        int now = int(time(NULL)), listed = 0;
        loopv(masterclients) if(masterclients[i]->listserver) listed++;
        prunebans(shield.bans, now);
        prunebans(m.bans, now);
        if(checkipinfo(control, ipinfo::BAN, c.address.host)) return true; // already banned, and on its way off the list
        if(checkipinfo(control, ipinfo::EXCEPT, c.address.host) || checkipinfo(control, ipinfo::TRUST, c.address.host)) held = "its address is excepted or trusted";
        else if(c.priority() > 0) held = "it has a server account";
        else if(shieldbanlimit && shield.bans.length() >= shieldbanlimit) held = "the hourly ban limit was reached";
        else if(shieldmodulatorlimit && m.bans.length() >= shieldmodulatorlimit) held = "the modulator's hourly ban limit was reached";
        else if(shieldbanshare && shield.passbans >= max(listed * shieldbanshare / 100, 1)) held = "too much of the list was banned at once";
        else if(shielddryrun) held = "dry run";
        if(held)
        {
            if(!shieldnew(c, tempformatstring("%s held %s %s", m.name, held, reason))) return false;
            noticef("shield", NOTICE_WARNING, "Master shield modulator '%s' would ban %s:%d (%s) on map '%s', but didn't because %s: %s", m.name, c.name, c.port, serverdetails(c), c.lastmap, held, reason);
            if(strcmp(held, "dry run")) conoutf(colourred, "Warning: Master shield held back a ban of %s:%d (%s)", c.name, c.port, held);
            return false;
        }
        addipinfo(control, ipinfo::BAN, c.name, reason);
        ipinfo &e = control.last();
        e.source = ipinfo::SRC_AUTO;
        shield.bans.add(now);
        m.bans.add(now);
        shield.passbans++;
        noticef("shield", NOTICE_WARNING, "Master shield modulator '%s' banned %s:%d (%s) on map '%s': %s", m.name, c.name, c.port, serverdetails(c), c.lastmap, reason);
        return true;
    }

    // a ban that Master shield made goes after shieldbanlapse days
    static void lapseshieldbans()
    {
        if(shield.lastlapse && totalmillis - shield.lastlapse < SHIELD_LAPSECHECK) return;
        shield.lastlapse = totalmillis ? totalmillis : 1;
        if(!shieldbanlapse) return;
        int now = int(time(NULL));
        bool changed = false;
        loopvrev(control)
        {
            ipinfo &e = control[i];
            if(e.flag != ipinfo::LOCAL || e.type != ipinfo::BAN || e.source != ipinfo::SRC_AUTO || e.added <= 0 || now - e.added < shieldbanlapse * 86400) continue;
            string addr;
            controladdr(e, addr);
            noticef("shield", NOTICE_INFO, "Master shield's ban of %s lapsed after %d day(s): %s", addr, shieldbanlapse, e.reason ? e.reason : "");
            control.remove(i);
            changed = true;
        }
        if(changed) nextcontrolversion(); // linked servers get the list again, and the file is saved
    }

    void checkshield()
    {
        if(!masterserver) return;
        lapseshieldbans();
        shield.passbans = 0;
        bool rescan = shield.rescan;
        shield.rescan = false;
        if(rescan) clearsandboxdenials(); // what the Sandbox refused is said again once the modulators change
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(!c.listserver || (!c.changed && !rescan)) continue;
            if(c.changed) c.lastflags.deletearrays();
            c.changed = false;
            loopvj(shield.modulators)
            {
                shieldmodulator *m = shield.modulators[j];
                if(!m->enabled || !m->body || !*m->body) continue;
                char *body = newstring(m->body);
                string text;
                int action = shieldrun(body, c, text);
                delete[] body;
                if(shieldact(c, *m, action, text)) break;
            }
        }
    }
}
