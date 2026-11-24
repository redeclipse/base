#include "engine.h"
#include "master.h"

#define QUEUE_VERSION 3
#define QUEUE_MINTHRESHOLD 2
#define QUEUE_MAXTHRESHOLD 8
#define QUEUE_DISCORDLEN 24
#define QUEUE_TIME 3600 // seconds an entry lasts from joining, wherever from
#define QUEUE_RENEW 1800 // what a renewal adds, up to QUEUE_MAXTIME from now
#define QUEUE_MAXTIME 5400
#define QUEUE_MSGRATE 250 // milliseconds between a viewer's queue messages
#define QUEUE_MAXENTRIES 4096

namespace master
{
    VAR(0, masterqueue, 0, 1, 1);
    VAR(0, masterqueuelock, 0, PRIV_MODERATOR, PRIV_DEVELOPER+1); // Privilege that sees each entry rather than only the counts
    VAR(0, masterqueuetravel, 30, 300, 3600); // seconds a popped entry counts towards its server while it makes its way there, and its slot there is kept
    VAR(0, masterqueuediscordlock, 0, PRIV_DEVELOPER, PRIV_DEVELOPER+1); // Privilege that may queue Discord members, the bot's
    SVAR(0, masterqueuefile, "masterqueue.dat");
    SVAR(0, masterverdevel, ""); // the version, such as 2.0.9, whose servers and players make up the devel queue, "" for none
    SVAR(0, masterverrelease, ""); // the same for the release queue

    enum { QUEUE_WEB = 1<<0, QUEUE_GAME = 1<<1, QUEUE_DISCORD = 1<<2 };

    // each channel is the linked servers on one version, and of those only the ones on the newest protocol any of them reports,
    // which is what an up to date game of that version runs
    enum { QUEUE_DEVEL = 0, QUEUE_RELEASE, QUEUE_CHANNELS };
    static const char * const queuechannelnames[QUEUE_CHANNELS] = { "devel", "release" };
    struct queuechannel
    {
        bool on;
        int build[3], protocol; // protocol: 0 while no server of that version is linked
    };
    static queuechannel queuechannels[QUEUE_CHANNELS];

    static bool parsebuild(const char *s, int *b) { return s && sscanf(s, "%d.%d.%d", &b[0], &b[1], &b[2]) == 3; }

    static int findchannel(const char *name)
    {
        loopi(QUEUE_CHANNELS) if(queuechannels[i].on && !strcmp(queuechannelnames[i], name)) return i;
        return -1;
    }

    static int buildchannel(const int *b)
    {
        loopi(QUEUE_CHANNELS) if(queuechannels[i].on && !memcmp(queuechannels[i].build, b, sizeof(queuechannels[i].build))) return i;
        return -1;
    }

    static void updatechannels()
    {
        const char *versions[QUEUE_CHANNELS] = { masterverdevel, masterverrelease };
        loopi(QUEUE_CHANNELS)
        {
            queuechannels[i].on = parsebuild(versions[i], queuechannels[i].build);
            queuechannels[i].protocol = 0;
        }
        loopv(masterclients)
        {
            masterclient &s = *masterclients[i];
            if(!s.listserver || !s.reports() || reaped(s)) continue;
            int c = buildchannel(s.statusbuild);
            if(c >= 0) queuechannels[c].protocol = max(queuechannels[c].protocol, s.statusver);
        }
    }

    // the channel a server is in, -1 for none
    static int serverchannel(masterclient &s)
    {
        if(!s.listserver || !s.reports()) return -1;
        int c = buildchannel(s.statusbuild);
        return c >= 0 && s.statusver == queuechannels[c].protocol ? c : -1;
    }

    const char *serverchannelname(masterclient &s)
    {
        int c = masterqueue ? serverchannel(s) : -1;
        return c >= 0 ? queuechannelnames[c] : NULL;
    }

    // one entry for each Account, however many of the website, the game and Discord it joined from
    struct queueentry
    {
        string handle, pophost, heldhost;
        string addr; // where it last joined from on the website or in the game, for the Admin notice when it pops; never saved
        int sources, threshold, popport, heldport, channel, protocol; // heldport: 0 when not held; protocol: the game's own, 0 when it last joined from elsewhere
        time_t added, expires, popped; // popped: when it was sent

        queueentry() : sources(0), threshold(0), popport(0), heldport(0), channel(0), protocol(0), added(0), expires(0), popped(0)
        {
            handle[0] = pophost[0] = heldhost[0] = addr[0] = '\0';
        }

        bool active() const { return !popped && !heldport; }
    };
    static vector<queueentry *> readyqueue;

    struct queueviewer
    {
        int id, privilege, lastmsg;
        string handle; // the Account its token is for, "" for none
        string addr;
    };
    static vector<queueviewer> queueviewers;
    static bool queuechanged = false, queuedirty = false; // changed: viewers need telling; dirty: the file needs writing
    static vector<char> queuelast; // what viewers were last sent

    static queueviewer *findqueueviewer(int id)
    {
        loopv(queueviewers) if(queueviewers[i].id == id) return &queueviewers[i];
        return NULL;
    }

    static bool validdiscord(const char *s)
    {
        int len = strlen(s);
        if(!len || len >= QUEUE_DISCORDLEN) return false;
        for(; *s; s++) if(!isdigit(*s)) return false;
        return true;
    }

    static bool validthreshold(int t) { return t >= QUEUE_MINTHRESHOLD && t <= QUEUE_MAXTHRESHOLD && !(t&1); }

    static queueentry *findentry(const char *handle)
    {
        if(!handle || !*handle) return NULL;
        loopv(readyqueue) if(!strcmp(readyqueue[i]->handle, handle)) return readyqueue[i];
        return NULL;
    }

    static void queuesources(jsonwriter &w, int sources)
    {
        w.beginarray("sources");
        if(sources&QUEUE_WEB) w.str(NULL, "web");
        if(sources&QUEUE_GAME) w.str(NULL, "game");
        if(sources&QUEUE_DISCORD) w.str(NULL, "discord");
        w.endarray();
    }

    // the eligible servers a pop may go to: in a channel, open, with room and signed in with a server account
    static bool queueeligible(masterclient &s)
    {
        if(serverchannel(s) < 0 || s.numplayers < 0 || s.statusmode != 0 || s.statusmax <= 0 || reaped(s)) return false;
        return s.priority() > 0; // what a server says about its room can't be checked, so pops go only to one the master knows
    }

    // popped entries on their way to a server count as on it, until they arrive or masterqueuetravel runs out
    static int queuetravelling(masterclient &s)
    {
        int n = 0;
        loopv(readyqueue) if(readyqueue[i]->popped && readyqueue[i]->popport == s.port && !strcmp(readyqueue[i]->pophost, s.name)) n++;
        return n;
    }

    // room is in player slots, which spectators don't take, less those kept for earlier pops: the server's count once it has
    // one, and before that the popped still on their way; spectators still take connections, so it's never more than can
    // connect besides those on their way; a server too old to say has its room counted from everyone on it
    static void queueplace(masterclient &s, int &humans, int &free)
    {
        int travel = queuetravelling(s);
        humans = s.numplayers + travel;
        if(s.statusslots <= 0) free = s.statusmax - humans;
        else
        {
            free = min(s.statusslots, MAXCLIENTS) - s.statusplaying - max(travel, s.statusreserved);
            if(s.statusconnections >= 0) free = min(free, s.statusconnections - travel);
        }
    }

    static bool sortthreshold(queueentry *a, queueentry *b) { return a->threshold < b->threshold; }

    // a channel's entries that can be popped, which for one last joined from a game means one on the channel's protocol
    static void activeentries(vector<queueentry *> &list, int channel)
    {
        list.setsize(0);
        int protocol = queuechannels[channel].protocol;
        loopv(readyqueue)
        {
            queueentry *e = readyqueue[i];
            if(e->active() && e->channel == channel && (!e->protocol || e->protocol == protocol)) list.add(e);
        }
        list.sort(sortthreshold);
    }

    // the largest group, from the lowest Thresholds up, whose Thresholds are all met by itself and those already there
    static int queuegroup(const vector<queueentry *> &list, int humans, int free)
    {
        for(int k = min(list.length(), free); k > 0; k--) if(list[k-1]->threshold <= humans + k) return k;
        return 0;
    }

    // how close a channel is: the fewest more people some Threshold waits for, on the server that would get them
    static void queuenext(const vector<queueentry *> &list, int channel, int &threshold, int &needed)
    {
        threshold = needed = 0;
        loopv(masterclients)
        {
            masterclient &s = *masterclients[i];
            if(!queueeligible(s) || serverchannel(s) != channel) continue;
            int humans, free;
            queueplace(s, humans, free);
            loopvj(list)
            {
                if(j + 1 < list.length() && list[j+1]->threshold == list[j]->threshold) continue;
                int t = list[j]->threshold, count = min(j + 1, free), want = max(t - humans - count, 1);
                if(count + want > free) continue;
                if(!needed || want < needed || (want == needed && t < threshold)) { threshold = t; needed = want; }
            }
        }
    }

    static void queueprivname(char *buf, int priv)
    {
        copystring(buf, PRIV_STR[clamp(priv&PRIV_TYPE, 0, PRIV_MAX-1)], MAXSTRLEN);
        for(char *s = buf; *s; s++) *s = tolower(*s);
    }

    // who is waiting is for everyone, where they joined from for those at masterqueuelock, and their Discord for the bot
    static void writeentry(jsonwriter &w, queueentry &e, int privilege)
    {
        authuser *u = authusers.access(e.handle);
        string priv;
        queueprivname(priv, u ? accountpriv(u->flags) : PRIV_NONE);
        w.beginobject();
        w.utf8("handle", e.handle);
        w.str("displayname", u && u->display ? u->display : "");
        w.str("privilege", priv);
        w.str("channel", queuechannelnames[clamp(e.channel, 0, QUEUE_CHANNELS-1)]);
        w.num("threshold", e.threshold);
        w.str("state", e.popped ? "travelling" : (e.heldport ? "held" : "ready"));
        w.num("added", double(e.added));
        w.num("expires", double(e.expires));
        if(e.heldport && !e.popped)
        {
            w.beginobject("held");
            w.str("host", e.heldhost);
            w.num("port", e.heldport);
            w.endobject();
        }
        if(e.popped)
        {
            w.beginobject("popped");
            w.str("host", e.pophost);
            w.num("port", e.popport);
            w.num("time", double(e.popped));
            w.endobject();
        }
        if(privilege >= masterqueuelock)
        {
            queuesources(w, e.sources);
            if(e.protocol) w.num("protocol", e.protocol);
        }
        if(privilege >= masterqueuediscordlock && u && u->discord && *u->discord) w.str("discord", u->discord);
        w.endobject();
    }

    static void writenext(jsonwriter &w, int threshold, int needed)
    {
        if(!needed) { w.null("next"); return; }
        w.beginobject("next");
        w.num("threshold", threshold);
        w.num("needed", needed);
        w.endobject();
    }

    static bool sortentries(queueentry *a, queueentry *b)
    {
        int sa = a->popped ? 2 : (a->heldport ? 1 : 0), sb = b->popped ? 2 : (b->heldport ? 1 : 0);
        if(sa != sb) return sa < sb;
        if(a->channel != b->channel) return a->channel < b->channel;
        if(a->threshold != b->threshold) return a->threshold < b->threshold;
        return a->added < b->added;
    }

    // counts for each channel and all together, then every entry, ready ones first by channel and Threshold, then held, then travelling
    static void writequeue(jsonwriter &w, int privilege, const char *name = "queue")
    {
        static vector<queueentry *> list;
        int ready = 0, held = 0, popped = 0, threshold = 0, needed = 0;
        w.beginobject(name);
        w.beginarray("channels");
        loopi(QUEUE_CHANNELS)
        {
            queuechannel &c = queuechannels[i];
            if(!c.on) continue;
            activeentries(list, i);
            int cheld = 0, cpopped = 0, ct, cn;
            loopvj(readyqueue) if(readyqueue[j]->channel == i) { if(readyqueue[j]->popped) cpopped++; else if(readyqueue[j]->heldport) cheld++; }
            queuenext(list, i, ct, cn);
            w.beginobject();
            w.str("name", queuechannelnames[i]);
            defformatstring(version, "%d.%d.%d", c.build[0], c.build[1], c.build[2]);
            w.str("version", version);
            int servers = 0;
            loopvj(masterclients) if(serverchannel(*masterclients[j]) == i && !reaped(*masterclients[j])) servers++;
            w.num("servers", servers);
            w.num("ready", list.length());
            w.num("held", cheld);
            w.num("travelling", cpopped);
            writenext(w, ct, cn);
            w.endobject();
            ready += list.length();
            held += cheld;
            popped += cpopped;
            if(cn && (!needed || cn < needed || (cn == needed && ct < threshold))) { threshold = ct; needed = cn; }
        }
        w.endarray();
        w.num("ready", ready);
        w.num("held", held);
        w.num("travelling", popped);
        w.beginarray("thresholds");
        for(int t = QUEUE_MINTHRESHOLD; t <= QUEUE_MAXTHRESHOLD; t += 2) w.num(NULL, t);
        w.endarray();
        writenext(w, threshold, needed);
        list.setsize(0);
        loopv(readyqueue) list.add(readyqueue[i]);
        list.sort(sortentries);
        w.beginarray("entries");
        loopv(list) writeentry(w, *list[i], privilege);
        w.endarray();
        w.endobject();
    }

    static void queuemessage(vector<char> &buf, int privilege)
    {
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "queue");
        w.num("time", double(time(NULL)));
        writequeue(w, privilege);
        w.endobject();
    }

    static void sendqueue(int id, int privilege)
    {
        vector<char> buf;
        queuemessage(buf, privilege);
        http::wssend(id, buf.getbuf(), buf.length());
    }

    void routequeue(httprequest &req, httpresponse &res)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.num("time", double(time(NULL)));
        writequeue(w, req.privilege);
        w.endobject();
        res.headers.set("Cache-Control", "no-store");
        res.json(buf.getbuf(), buf.length());
    }

    // what an entry's Account hears of it, on every socket signed in as it
    static void tellowner(queueentry &e, const char *event, const char *reason = NULL, int only = -1)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", event);
        w.num("time", double(time(NULL)));
        if(!strcmp(event, "queued"))
        {
            w.num("threshold", e.threshold);
            w.str("channel", queuechannelnames[clamp(e.channel, 0, QUEUE_CHANNELS-1)]);
            w.num("expires", double(e.expires));
            if(e.popped)
            {
                w.str("host", e.pophost);
                w.num("port", e.popport);
            }
        }
        if(reason) w.str("reason", reason);
        w.endobject();
        loopv(queueviewers) if((only < 0 || queueviewers[i].id == only) && !strcmp(queueviewers[i].handle, e.handle)) http::wssend(queueviewers[i].id, buf.getbuf(), buf.length());
    }

    // those who may queue Discord members hear when any entry ends, with the member when its Account is linked, so the bot can keep its own view and tell them
    static void tellbots(queueentry &e, const char *reason)
    {
        authuser *u = authusers.access(e.handle);
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "unqueued");
        w.num("time", double(time(NULL)));
        w.utf8("handle", e.handle);
        if(u && u->discord && *u->discord) w.str("discord", u->discord);
        w.str("reason", reason);
        w.endobject();
        loopv(queueviewers) if(queueviewers[i].privilege >= masterqueuediscordlock) http::wssend(queueviewers[i].id, buf.getbuf(), buf.length());
    }

    static void removeentry(int i, const char *reason)
    {
        queueentry *e = readyqueue[i];
        tellowner(*e, "unqueued", reason);
        tellbots(*e, reason);
        delete e;
        readyqueue.remove(i);
        queuechanged = queuedirty = true;
    }

    static void removeentry(queueentry *e, const char *reason)
    {
        int i = readyqueue.find(e);
        if(i >= 0) removeentry(i, reason);
    }

    // a group sent to a server: every viewer hears where, and whether it was one of them
    static void popgroup(masterclient &s, vector<queueentry *> &group)
    {
        time_t now = time(NULL);
        loopv(group)
        {
            queueentry &e = *group[i];
            e.popped = now;
            copystring(e.pophost, s.name);
            e.popport = s.port;
        }
        // so nobody joining by themselves takes the slots first; older servers don't know it and pass it by
        if(s.link >= 0)
        {
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            w.str("queue", "reserve");
            w.beginarray("handles");
            loopv(group) w.str(NULL, group[i]->handle);
            w.endarray();
            w.num("seconds", masterqueuetravel);
            w.endobject();
            http::wssend(s.link, buf.getbuf(), buf.length());
        }
        loopv(queueviewers)
        {
            queueviewer &v = queueviewers[i];
            bool mine = false;
            loopvj(group) if(v.handle[0] && !strcmp(group[j]->handle, v.handle)) { mine = true; break; }
            vector<char> buf;
            jsonwriter w(buf);
            w.beginobject();
            w.str("event", "pop");
            w.num("time", double(now));
            w.str("host", s.name);
            w.num("port", s.port);
            w.num("count", group.length());
            if(mine) w.boolean("mine", true);
            w.beginarray("entries");
            loopvj(group) writeentry(w, *group[j], v.privilege);
            w.endarray();
            w.endobject();
            http::wssend(v.id, buf.getbuf(), buf.length());
        }
        bigstring who = "";
        loopv(group)
        {
            queueentry &e = *group[i];
            string from = "";
            if(e.sources&QUEUE_WEB) concatstring(from, "website");
            if(e.sources&QUEUE_GAME) concformatstring(from, "%sgame", from[0] ? ", " : "");
            if(e.sources&QUEUE_DISCORD) concformatstring(from, "%sDiscord", from[0] ? ", " : "");
            concformatstring(who, "%s'%s' from %s%s%s", i ? "; " : "", e.handle, from[0] ? from : "?", e.addr[0] ? " at " : "", e.addr);
        }
        noticef("queue", NOTICE_INFO, "Matchmaking queue sent %d to %s:%d (%s): %s", group.length(), s.name, s.port, serverdetails(s), who);
        queuechanged = queuedirty = true;
    }

    // the linked server an Account is on, by the handles it reports that the master signed in there
    static masterclient *queueon(queueentry &e)
    {
        loopv(masterclients)
        {
            masterclient &s = *masterclients[i];
            if(!s.listserver || s.link < 0) continue;
            loopvj(s.queuehandles) if(!strcmp(s.queuehandles[j], e.handle)) return &s;
        }
        return NULL;
    }

    static void checkpops()
    {
        static vector<queueentry *> list, group;
        for(;;)
        {
            masterclient *best = NULL;
            int bestk = 0, besthumans = 0;
            loopv(masterclients)
            {
                masterclient &s = *masterclients[i];
                if(!queueeligible(s)) continue;
                activeentries(list, serverchannel(s));
                int humans, free;
                queueplace(s, humans, free);
                int k = queuegroup(list, humans, free);
                if(!k) continue;
                // the largest group, then the most people, then the highest priority
                if(!best || k > bestk || (k == bestk && (humans > besthumans || (humans == besthumans && s.priority() > best->priority()))))
                {
                    best = &s;
                    bestk = k;
                    besthumans = humans;
                }
            }
            if(!best) return;
            activeentries(list, serverchannel(*best));
            group.setsize(0);
            loopi(bestk) group.add(list[i]);
            popgroup(*best, group);
        }
    }

    // once a second: endings, arrivals and holds, then any pops, then viewers hear of any change
    void checkqueue()
    {
        updatechannels();
        if(!masterqueue) return;
        time_t now = time(NULL);
        loopvrev(readyqueue)
        {
            queueentry &e = *readyqueue[i];
            // so a handle registered again never takes it over, unless the accounts couldn't be loaded at all
            if(!masterfileblocked(MFILE_USER) && !authusers.access(e.handle)) { removeentry(i, "deleted"); continue; }
            if(!queuechannels[e.channel].on) { removeentry(i, "closed"); continue; } // its channel was taken out of the config
            masterclient *on = queueon(e);
            if(e.popped)
            {
                if(on && on->port == e.popport && !strcmp(on->name, e.pophost)) { removeentry(i, "arrived"); continue; }
                if(now - e.popped >= masterqueuetravel) { removeentry(i, "popped"); continue; }
                continue;
            }
            int port = on ? on->port : 0;
            if(port != e.heldport || (on && strcmp(on->name, e.heldhost)))
            {
                e.heldport = port;
                copystring(e.heldhost, on ? on->name : "");
                queuechanged = true;
            }
            if(on && on->numplayers >= e.threshold) { removeentry(i, "satisfied"); continue; }
            if(e.expires <= now) { removeentry(i, "expired"); continue; }
        }
        checkpops();
        if(queuedirty) savequeue(); // at most once a second
        // the counts move with the servers as well as the queue, so they're compared every time, and viewers only hear when what they see changes
        vector<char> pub;
        jsonwriter cw(pub);
        writequeue(cw, HTTP_PUBLIC, NULL);
        bool same = pub.length() == queuelast.length() && !memcmp(pub.getbuf(), queuelast.getbuf(), pub.length());
        if(same && !queuechanged) return;
        queuelast = pub;
        vector<char> msg;
        if(!same) queuemessage(msg, HTTP_PUBLIC);
        loopv(queueviewers)
        {
            queueviewer &qv = queueviewers[i];
            if(qv.privilege >= masterqueuelock) sendqueue(qv.id, qv.privilege);
            else if(!same) http::wssend(qv.id, msg.getbuf(), msg.length());
        }
        queuechanged = false;
    }

    // a refusal, naming the member when it was about one, so the bot knows whose it is
    static void queuereply(int id, const char *error, const char *discord = NULL)
    {
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "queued");
        w.num("time", double(time(NULL)));
        if(discord && *discord) w.str("discord", discord);
        w.str("error", error);
        w.endobject();
        http::wssend(id, buf.getbuf(), buf.length());
    }

    // joining, or changing the Threshold or channel; joining again never shortens the time left
    static queueentry *queueadd(int id, authuser &u, int source, int threshold, int channel, int protocol, const char *discord = NULL)
    {
        time_t now = time(NULL);
        queueentry *e = findentry(u.name);
        if(!e)
        {
            if(readyqueue.length() >= QUEUE_MAXENTRIES) { queuereply(id, "the queue is full", discord); return NULL; }
            e = readyqueue.add(new queueentry);
            copystring(e->handle, u.name);
            e->added = now;
        }
        else if(e->popped) e->popped = 0; // queuing again after being sent somewhere starts over
        e->sources |= source;
        e->threshold = threshold;
        e->channel = channel;
        e->protocol = protocol;
        e->expires = max(e->expires, now + QUEUE_TIME);
        queuechanged = queuedirty = true;
        return e;
    }

    static void queuerenew(queueentry &e)
    {
        time_t now = time(NULL);
        e.expires = min(max(e.expires, now) + QUEUE_RENEW, now + QUEUE_MAXTIME);
        queuechanged = queuedirty = true;
    }

    // a web page or the game, for the Account its token is for
    static void queueself(queueviewer &v, const char *op, jsonvalue *msg)
    {
        authuser *u = v.handle[0] ? authusers.access(v.handle) : NULL;
        if(!u) { queuereply(v.id, "sign in with an account to queue"); return; }
        queueentry *e = findentry(u->name);
        if(!strcmp(op, "leave"))
        {
            if(e) removeentry(e, "left");
            else queuereply(v.id, "not queued");
            return;
        }
        if(!strcmp(op, "renew"))
        {
            if(!e) { queuereply(v.id, "not queued"); return; }
            queuerenew(*e);
            tellowner(*e, "queued");
            return;
        }
        if(strcmp(op, "join")) return;
        int threshold = msg->getint("threshold");
        if(!validthreshold(threshold)) { queuereply(v.id, "bad threshold"); return; }
        int source = !strcmp(msg->getstr("source", "web"), "game") ? QUEUE_GAME : QUEUE_WEB, channel = -1, protocol = 0;
        if(source == QUEUE_GAME)
        {
            // a game's channel is its version, and it can only be sent where its protocol is played
            int build[3];
            if(parsebuild(msg->getstr("version", ""), build)) channel = buildchannel(build);
            protocol = msg->getint("protocol");
            if(channel < 0) { queuereply(v.id, "this version of the game has no queue"); return; }
            if(protocol <= 0 || (queuechannels[channel].protocol && protocol != queuechannels[channel].protocol)) { queuereply(v.id, "update your game to queue"); return; }
        }
        else if((channel = findchannel(msg->getstr("channel", ""))) < 0) { queuereply(v.id, "no such channel"); return; }
        if((e = queueadd(v.id, *u, source, threshold, channel, protocol)))
        {
            copystring(e->addr, v.addr);
            tellowner(*e, "queued");
        }
    }

    // the bot queuing, renewing or removing a Discord member, through the Account they linked
    static void queuediscord(queueviewer &v, const char *op, jsonvalue *msg)
    {
        const char *discord = msg->getstr("discord", "");
        if(!validdiscord(discord)) { queuereply(v.id, "bad discord id"); return; }
        authuser *u = discordaccount(discord);
        if(!u) { queuereply(v.id, "no account", discord); return; }
        queueentry *e = findentry(u->name);
        if(!strcmp(op, "leave"))
        {
            if(e) removeentry(e, "left");
            else queuereply(v.id, "not queued", discord);
            return;
        }
        if(!strcmp(op, "renew"))
        {
            if(!e) { queuereply(v.id, "not queued", discord); return; }
            queuerenew(*e);
        }
        else
        {
            int threshold = msg->getint("threshold"), channel = findchannel(msg->getstr("channel", ""));
            if(!validthreshold(threshold)) { queuereply(v.id, "bad threshold", discord); return; }
            if(channel < 0) { queuereply(v.id, "no such channel", discord); return; }
            // a member's place goes wherever the channel's players can join, whatever their game was
            if(!(e = queueadd(v.id, *u, QUEUE_DISCORD, threshold, channel, 0, discord))) return;
        }
        tellowner(*e, "queued");
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.str("event", "queued");
        w.num("time", double(time(NULL)));
        w.str("discord", discord);
        w.utf8("handle", e->handle);
        w.num("threshold", e->threshold);
        w.str("channel", queuechannelnames[e->channel]);
        w.num("expires", double(e->expires));
        w.endobject();
        http::wssend(v.id, buf.getbuf(), buf.length());
    }

    void queueviewermessage(httpwsevent &e)
    {
        queueviewer *v = findqueueviewer(e.id);
        if(!v || !masterqueue || e.opcode != WS_TEXT) return;
        bool bot = v->privilege >= masterqueuediscordlock;
        if(!bot && v->lastmsg && totalmillis - v->lastmsg < QUEUE_MSGRATE) return;
        v->lastmsg = totalmillis ? totalmillis : 1;
        jsonvalue *msg = json::parse(e.data, e.len);
        if(!msg) return;
        if(msg->type != JSON_OBJECT) { delete msg; return; }
        const char *op = msg->getstr("queue", "");
        // the bot's messages name a member and no source, a page's or the game's are for their own Account
        if(!msg->get("source") && *msg->getstr("discord", ""))
        {
            if(bot) queuediscord(*v, op, msg);
            else queuereply(v->id, "insufficient privilege");
        }
        else queueself(*v, op, msg);
        delete msg;
    }

    // a viewer's Privilege and Account are whatever its token gave it as it connected, and one with an entry hears of it straight away
    void queueviewerjoined(httpwsevent &e)
    {
        queueviewer &v = queueviewers.add();
        v.id = e.id;
        v.privilege = e.req ? e.req->privilege : HTTP_PUBLIC;
        v.lastmsg = 0;
        copystring(v.addr, e.req ? e.req->addr : "");
        copystring(v.handle, e.req && e.req->handle && authusers.access(e.req->handle) ? e.req->handle : "");
        if(!masterqueue) return;
        sendqueue(e.id, v.privilege);
        queueentry *q = findentry(v.handle);
        if(q) tellowner(*q, "queued", NULL, e.id);
    }

    void queueviewerclosed(int id)
    {
        loopv(queueviewers) if(queueviewers[i].id == id) { queueviewers.remove(i); break; }
    }

    void broadcastnotice(int minpriv, const char *buf, int len)
    {
        loopv(queueviewers) if(queueviewers[i].privilege >= minpriv) http::wssend(queueviewers[i].id, buf, len);
    }

    // the handles a linked server reports, of those the master signed in there, so a player can't claim to be someone else
    void queuehandles(masterclient &c, jsonvalue *players)
    {
        c.queuehandles.deletearrays();
        if(!players || players->type != JSON_ARRAY) return;
        loopv(players->items)
        {
            jsonvalue *p = players->items[i];
            const char *handle = p->type == JSON_OBJECT ? p->getstr("handle", "") : "";
            if(!*handle || c.queuehandles.length() >= MAXCLIENTS) continue;
            loopvj(c.verified) if(!strcmp(c.verified[j], handle)) { c.queuehandles.add(newstring(handle)); break; }
        }
    }

    static masterfileguard queueguard;

    static int readqueue(const char *name, int)
    {
        readyqueue.deletecontents();
        stream *f = opengzfile(name, "rb");
        if(!f) return masterfilemissing(name);
        char magic[4];
        bool ok = f->read(magic, 4) == 4 && !memcmp(magic, "REMQ", 4);
        int version = ok ? f->getlil<int>() : 0;
        if(ok && version != QUEUE_VERSION) return masterfileolder(f, name);
        int count = ok ? f->getlil<int>() : 0;
        if(count < 0 || count > QUEUE_MAXENTRIES) ok = false;
        for(int i = 0; ok && i < count; i++)
        {
            queueentry *e = new queueentry;
            string *strs[2] = { &e->handle, &e->pophost };
            loopj(2)
            {
                int len = f->getlil<int>();
                if(len < 0 || len >= MAXSTRLEN || (len && f->read(*strs[j], len) != size_t(len))) { ok = false; break; }
                (*strs[j])[len] = '\0';
            }
            if(!ok) { delete e; break; }
            e->sources = f->getlil<int>();
            e->threshold = f->getlil<int>();
            e->popport = f->getlil<int>();
            e->channel = f->getlil<int>();
            e->protocol = f->getlil<int>();
            e->added = time_t(f->getlil<int64_t>());
            e->expires = time_t(f->getlil<int64_t>());
            e->popped = time_t(f->getlil<int64_t>());
            if(!e->handle[0] || !validthreshold(e->threshold) || e->channel < 0 || e->channel >= QUEUE_CHANNELS || findentry(e->handle)) { delete e; continue; }
            readyqueue.add(e);
        }
        delete f;
        if(!ok) { readyqueue.deletecontents(); return 1; }
        if(readyqueue.length()) conoutf(colourwhite, "Loaded %d Matchmaking queue entries from %s", readyqueue.length(), name);
        return 0;
    }

    void loadqueue()
    {
        readyqueue.deletecontents();
        if(!*masterqueuefile) return;
        if(loadguarded(masterqueuefile, readqueue, 0, queueguard, "the master restarts")) queuedirty = true;
    }

    // written beside the old one then moved over it, as the history is; an entry that ends is gone from the next copy
    void savequeue()
    {
        queuedirty = false;
        if(!*masterqueuefile || saveblocked(queueguard, masterqueuefile)) return;
        defformatstring(newname, "%s.new", masterqueuefile);
        stream *f = openmasterfile(newname, true);
        if(!f) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s save", newname), "Failed to save the Matchmaking queue to %s", newname); return; }
        f->write("REMQ", 4);
        f->putlil<int>(QUEUE_VERSION);
        f->putlil<int>(readyqueue.length());
        loopv(readyqueue)
        {
            queueentry &e = *readyqueue[i];
            const char *strs[2] = { e.handle, e.pophost };
            loopj(2)
            {
                int len = strlen(strs[j]);
                f->putlil<int>(len);
                if(len) f->write(strs[j], len);
            }
            f->putlil<int>(e.sources);
            f->putlil<int>(e.threshold);
            f->putlil<int>(e.popport);
            f->putlil<int>(e.channel);
            f->putlil<int>(e.protocol);
            f->putlil<int64_t>(int64_t(e.added));
            f->putlil<int64_t>(int64_t(e.expires));
            f->putlil<int64_t>(int64_t(e.popped));
        }
        delete f;
        commitmasterfile(masterqueuefile, queueguard, false); // only its .bak, as it holds an hour at most
    }
}
