#include "engine.h"
#include <enet/time.h>
#include "master.h"

#define HISTORY_BUCKET 900 // seconds a peak covers
#define HISTORY_UNKNOWN 0xFF // a bucket the master knew nothing about, as the server wasn't listed or said nothing
#define HISTORY_MAXPEAK 254
#define HISTORY_VERSION 1
#define HISTORY_MAXDAYS 3650
#define HISTORY_GLOBAL "*" // the whole game's, kept as a server of its own with a name no server can have
#define HISTORY_DEFAULTDAYS 7
#define HISTORY_MAXSERVERS 65536 // histories kept and loaded, the least recently heard from going first past it

namespace master
{
    VAR(0, masteriplock, 0, PRIV_ADMINISTRATOR, PRIV_MAX); // Privilege that may see the addresses of every listed server's players, PRIV_MAX for nobody

    static int parseinclude(const char *s, int priv)
    {
        if(!s || !*s) return 0;
        int mask = 0;
        while(*s)
        {
            while(*s == ',' || *s == ' ' || *s == '\t') s++;
            if(!*s) break;
            const char *start = s;
            while(*s && *s != ',' && *s != ' ' && *s != '\t') s++;
            int len = int(s - start);
            if(len == 3 && !strncmp(start, "map", 3)) mask |= STATUS_INC_MAP;
            else if(len == 4 && !strncmp(start, "vars", 4)) mask |= STATUS_INC_VARS;
            else if(len == 6 && !strncmp(start, "config", 6)) mask |= STATUS_INC_CONFIG;
            else if(len == 3 && !strncmp(start, "ips", 3)) { if(priv >= masteriplock) mask |= STATUS_INC_IPS; }
            else if(len == 3 && !strncmp(start, "all", 3)) mask |= STATUS_INC_PUBLIC | (priv >= masteriplock ? STATUS_INC_IPS : 0);
        }
        return mask;
    }

    void writestatusfiltered(jsonwriter &w, jsonvalue *st, int inc, bool hashmode)
    {
        if(!st || st->type != JSON_OBJECT) { w.beginobject(); w.endobject(); return; }
        w.beginobject();
        loopv(st->items)
        {
            jsonvalue *m = st->items[i];
            if(!m || !m->key) continue;
            const char *k = m->key;
            if(hashmode && (!strcmp(k, "ends") || !strcmp(k, "stateends") || !strcmp(k, "lastmatch"))) continue;
            if(!(inc & STATUS_INC_MAP) && (!strcmp(k, "mapfilecrc") || !strcmp(k, "mapsize"))) continue;
            if(!(inc & STATUS_INC_VARS) && !strcmp(k, "mods")) continue;
            if(!(inc & STATUS_INC_CONFIG) && !strcmp(k, "config")) continue;
            if(hashmode && !strcmp(k, "config") && m->type == JSON_OBJECT)
            {
                w.beginobject("config");
                loopvj(m->items) if(m->items[j] && m->items[j]->key && strcmp(m->items[j]->key, "uptime")) w.value(m->items[j]->key, m->items[j]);
                w.endobject();
                continue;
            }
            if(!(inc & STATUS_INC_IPS) && !strcmp(k, "players") && m->type == JSON_ARRAY)
            {
                w.beginarray("players");
                loopvj(m->items)
                {
                    jsonvalue *p = m->items[j];
                    if(!p || p->type != JSON_OBJECT) continue;
                    w.beginobject();
                    loopvk(p->items) if(p->items[k] && p->items[k]->key && strcmp(p->items[k]->key, "ip")) w.value(p->items[k]->key, p->items[k]);
                    w.endobject();
                }
                w.endarray();
                continue;
            }
            w.value(k, m);
        }
        w.endobject();
    }

    static void writereap(jsonwriter &w, masterclient &s)
    {
        ipinfo *r = checkipinfo(control, ipinfo::REAP, s.address.host);
        if(!r) return;
        w.beginobject("reaped");
        w.str("reason", r->reason ? r->reason : "");
        if(r->source >= 0 && r->source < ipinfo::SRC_MAX) w.str("source", controlsources[r->source]);
        if(r->addhandle[0]) w.str("by", r->addhandle);
        if(r->added > 0) w.num("added", r->added);
        w.endobject();
    }

    static void writeserver(jsonwriter &w, masterclient &s, int inc = 0, bool mark = false)
    {
        w.beginobject();
        w.str("host", s.name);
        w.num("port", s.port);
        w.num("priority", s.priority());
        w.str("description", s.desc);
        if(s.authhandle[0]) w.str("handle", s.authhandle);
        if(s.flags[0]) w.str("flags", s.flags);
        w.str("branch", s.branch);
        w.num("version", s.version);
        if(s.webport) w.num("webport", s.webport);
        w.num("relay", s.link >= 0 ? s.relay : 0);
        if(s.fingerprint[0]) w.str("fingerprint", s.fingerprint);
        if(s.link >= 0 && s.bridge >= 0) w.num("bridge", s.bridge);
        const char *channel = serverchannelname(s);
        if(channel) w.str("channel", channel);
        if(s.reports())
        {
            if(!inc || !s.fullstatus) w.raw("status", s.status);
            else
            {
                jsonvalue *st = json::parse(s.fullstatus);
                if(st)
                {
                    vector<char> tmp;
                    jsonwriter tw(tmp);
                    writestatusfiltered(tw, st, inc);
                    w.raw("status", tmp.getbuf(), tmp.length());
                    delete st;
                }
                else w.raw("status", s.status);
            }
        }
        else if(s.polled) w.raw("status", s.polled);
        if(mark) writereap(w, s);
        w.endobject();
    }

    // stamped with our clock, as the times in it are, so viewers can count down without their clocks agreeing with ours
    static void serverlist(vector<char> &buf, bool stamped = true, int inc = 0, uint host = 0, int priv = HTTP_PUBLIC)
    {
        jsonwriter w(buf);
        w.beginobject();
        if(stamped) w.num("time", double(time(NULL)));
        w.beginobject("version");
        w.num("main", mastermainver ? mastermainver : server::getver(0));
        w.num("game", mastergamever ? mastergamever : server::getver(1));
        w.endobject();
        if(*masterscriptclient) w.str("script", masterscriptclient);
        w.beginarray("servers");
        loopv(masterclients) if(masterclients[i]->listserver && showserver(*masterclients[i], host, priv)) writeserver(w, *masterclients[i], inc, priv >= mastershieldlock);
        w.endarray();
        w.endobject();
    }

    uint listhash(const vector<char> &buf)
    {
        uint h = 2166136261U;
        loopv(buf) h = (h ^ uchar(buf[i])) * 16777619U;
        return h;
    }

    void routeservers(httprequest &req, httpresponse &res)
    {
        const char *incstr = req.var("include", "");
        int priv = max(req.privilege, requestpriv(req)), inc = *incstr ? parseinclude(incstr, priv) : 0;
        bool privvary = (*incstr && (parseinclude(incstr, PRIV_FOUNDER) & STATUS_INC_IPS)) || priv >= mastershieldlock;
        // the tag leaves out the time, which changes every second when nothing else has
        vector<char> buf;
        serverlist(buf, false, inc, req.ip, priv);
        defformatstring(etag, "\"%08x\"", listhash(buf));
        res.headers.set("ETag", etag);
        res.headers.set("Cache-Control", privvary ? "private, no-store" : "public, max-age=5");
        const char *match = req.headers.get("If-None-Match");
        if(match && !strcmp(match, etag)) { res.status = 304; return; }
        buf.setsize(0);
        serverlist(buf, true, inc, req.ip, priv);
        res.json(buf.getbuf(), buf.length());
    }


    struct listedserver
    {
        string host;
        int port;
        uint addr;
        char *json, *marked;
        bool reaped;
    };
    static vector<listedserver> listed;

    struct listviewer
    {
        int id, priv;
        uint ip;
    };
    static vector<listviewer> viewers;
    static int lastlistcheck = 0;

    // how busy each server has been: its peak player count in each HISTORY_BUCKET, kept for masterhistorydays and saved to masterhistoryfile

    VAR(0, masterhistorydays, 0, 365, HISTORY_MAXDAYS); // days of history kept for each server, 0 keeps none
    SVAR(0, masterhistoryfile, "masterhistory.dat");

    struct serverhistory
    {
        string host;
        int port, first; // first: the bucket, counted from the epoch, that peaks starts at
        vector<uchar> peaks;

        int last() const { return first + peaks.length() - 1; }

        void record(int bucket, int players)
        {
            if(peaks.empty()) first = bucket;
            else if(bucket < first) return; // the clock went back
            while(last() < bucket) peaks.add(HISTORY_UNKNOWN);
            uchar &p = peaks[bucket - first], n = uchar(clamp(players, 0, HISTORY_MAXPEAK));
            if(p == HISTORY_UNKNOWN || n > p) p = n;
        }

        void trim(int keep)
        {
            int over = peaks.length() - keep;
            if(over <= 0) return;
            peaks.remove(0, over);
            first += over;
        }
    };
    static vector<serverhistory *> histories;
    static int lasthistorybucket = -1;

    static int historybucket() { return int(time(NULL) / HISTORY_BUCKET); }
    static int historykeep() { return masterhistorydays * (86400 / HISTORY_BUCKET); }

    static serverhistory *findhistory(const char *host, int port, bool add = false)
    {
        loopv(histories) if(histories[i]->port == port && !strcmp(histories[i]->host, host)) return histories[i];
        if(!add) return NULL;
        if(histories.length() >= HISTORY_MAXSERVERS)
        {
            int oldest = -1;
            loopv(histories) if(strcmp(histories[i]->host, HISTORY_GLOBAL) && (oldest < 0 || histories[i]->last() < histories[oldest]->last())) oldest = i;
            if(oldest >= 0) delete histories.remove(oldest);
        }
        serverhistory *h = new serverhistory;
        copystring(h->host, host);
        h->port = port;
        h->first = 0;
        histories.add(h);
        return h;
    }

    static masterfileguard historyguard;

    static int readhistory(const char *name, int)
    {
        histories.deletecontents();
        stream *f = opengzfile(name, "rb");
        if(!f) return masterfilemissing(name);
        char magic[4];
        int maxlen = HISTORY_MAXDAYS * (86400 / HISTORY_BUCKET);
        bool ok = f->read(magic, 4) == 4 && !memcmp(magic, "REMH", 4);
        if(ok && (f->getlil<int>() != HISTORY_VERSION || f->getlil<int>() != HISTORY_BUCKET)) return masterfileolder(f, name);
        int count = ok ? f->getlil<int>() : 0;
        if(count < 0 || count > HISTORY_MAXSERVERS) ok = false;
        for(int i = 0; ok && i < count; i++)
        {
            int hostlen = f->getlil<int>();
            string host;
            if(hostlen <= 0 || hostlen >= MAXSTRLEN || f->read(host, hostlen) != size_t(hostlen)) { ok = false; break; }
            host[hostlen] = '\0';
            int port = f->getlil<int>(), first = f->getlil<int>(), len = f->getlil<int>();
            if(port < 0 || port > 65535 || (!port && strcmp(host, HISTORY_GLOBAL)) || first < 0 || len < 0 || len > maxlen) { ok = false; break; }
            serverhistory *h = findhistory(host, port, true);
            h->first = first;
            h->peaks.setsize(0);
            if(len && f->read(h->peaks.pad(len), len) != size_t(len)) { ok = false; break; }
            h->trim(historykeep());
        }
        delete f;
        if(!ok) { histories.deletecontents(); return 1; }
        conoutf(colourwhite, "Loaded the history of %d server(s) from %s", histories.length(), name);
        return 0;
    }

    void loadhistory()
    {
        histories.deletecontents();
        if(!masterhistorydays || !*masterhistoryfile) return;
        loadguarded(masterhistoryfile, readhistory, 0, historyguard, "the master restarts");
    }

    // written beside the old one then moved over it, so a crash part way through leaves the last good copy
    void savehistory()
    {
        if(!masterhistorydays || !*masterhistoryfile || saveblocked(historyguard, masterhistoryfile)) return;
        int now = historybucket(), keep = historykeep();
        loopvrev(histories)
        {
            serverhistory *h = histories[i];
            h->trim(keep);
            if(h->peaks.empty() || h->last() < now - keep) { delete h; histories.remove(i); }
        }
        defformatstring(newname, "%s.new", masterhistoryfile);
        stream *f = openmasterfile(newname, true);
        if(!f) { foldnoticef("master", NOTICE_ERROR, tempformatstring("file %s save", newname), "Failed to save server history to %s", newname); return; }
        f->write("REMH", 4);
        f->putlil<int>(HISTORY_VERSION);
        f->putlil<int>(HISTORY_BUCKET);
        f->putlil<int>(histories.length());
        loopv(histories)
        {
            serverhistory *h = histories[i];
            int hostlen = strlen(h->host);
            f->putlil<int>(hostlen);
            f->write(h->host, hostlen);
            f->putlil<int>(h->port);
            f->putlil<int>(h->first);
            f->putlil<int>(h->peaks.length());
            f->write(h->peaks.getbuf(), h->peaks.length());
        }
        delete f;
        commitmasterfile(masterhistoryfile, historyguard, true);
    }

    // every listed server that says how many are on, and all of them together, once a second, and the file once a bucket
    static void checkhistory()
    {
        if(!masterhistorydays) return;
        int bucket = historybucket(), total = 0;
        bool known = false;
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(!c.listserver || c.numplayers < 0 || reaped(c)) continue;
            findhistory(c.name, c.port, true)->record(bucket, c.numplayers);
            total += c.numplayers;
            known = true;
        }
        if(known) findhistory(HISTORY_GLOBAL, 0, true)->record(bucket, total);
        if(lasthistorybucket >= 0 && bucket != lasthistorybucket) savehistory();
        lasthistorybucket = bucket;
    }

    // whether any server from this address was ever listed, by its history on any port
    bool knownhost(const char *host)
    {
        loopv(histories) if(histories[i]->port && !strcmp(histories[i]->host, host)) return true;
        return false;
    }

    // GET /api/v1/servers/<address>/<port>/history?days=N: peaks oldest first, null where nothing was known

    static void writehistory(httprequest &req, httpresponse &res, serverhistory *h, masterclient *own = NULL);

    void routehistory(httprequest &req, httpresponse &res)
    {
        const char *address = req.param("address"), *port = req.param("port");
        if(!address || !port || !*port || port[strspn(port, "0123456789")]) { res.error(404, "no history for that server"); return; }
        int p = atoi(port);
        masterclient *own = NULL;
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.listserver && c.port == p && c.address.host == req.ip && !strcmp(c.name, address) && reaped(c)) { own = &c; break; }
        }
        writehistory(req, res, findhistory(address, p), own);
    }

    // GET /api/v1/history?days=N: the same for every listed server's players added together
    void routeglobalhistory(httprequest &req, httpresponse &res)
    {
        writehistory(req, res, findhistory(HISTORY_GLOBAL, 0));
    }

    // a bounce link: a web page that sends the browser on to a listed server's redeclipse:// join link, for places such as
    // Discord that only make https links clickable
    static void htmlput(vector<char> &buf, const char *s) { buf.put(s, strlen(s)); }

    static void htmltext(vector<char> &buf, const char *s, bool cube)
    {
        for(; *s; s++)
        {
            uchar c = uchar(*s);
            switch(c)
            {
                case '&': htmlput(buf, "&amp;"); break;
                case '<': htmlput(buf, "&lt;"); break;
                case '>': htmlput(buf, "&gt;"); break;
                case '"': htmlput(buf, "&quot;"); break;
                case '\'': htmlput(buf, "&#39;"); break;
                default:
                    if(cube && c >= 0x80)
                    {
                        uchar u[4];
                        size_t n = encodeutf8(u, sizeof(u), &c, 1);
                        buf.put((const char *)u, n);
                    }
                    else buf.add(c);
                    break;
            }
        }
    }

    void routejoin(httprequest &req, httpresponse &res)
    {
        const char *address = req.param("address"), *port = req.param("port");
        masterclient *found = NULL;
        if(address && port && *port && !port[strspn(port, "0123456789")])
        {
            int p = atoi(port);
            loopv(masterclients) if(masterclients[i]->listserver && masterclients[i]->port == p && !strcmp(masterclients[i]->name, address) && showserver(*masterclients[i], req.ip, reaped(*masterclients[i]) ? max(req.privilege, requestpriv(req)) : HTTP_PUBLIC))
            {
                found = masterclients[i];
                break;
            }
        }
        res.headers.set("Cache-Control", "no-store");
        res.settype("text/html; charset=utf-8");
        tagval args[5];
        int numargs = 1;
        string desc, link;
        char *page = NULL;
        if(!found)
        {
            res.status = 404;
            args[0].setcstr(versionname);
            page = renderhtmlpage("joinmissing", args, numargs);
        }
        else
        {
            filterstring(desc, found->desc);
            if(!desc[0]) formatstring(desc, "%s:%d", found->name, found->port);
            formatstring(link, "%s://%s:%d", versionuname, found->name, found->port);
            args[0].setstr(newstring(tempformatstring("Join %s - %s", desc, versionname)));
            args[1].setcstr(desc);
            args[2].setcstr(link);
            args[3].setcstr(found->name);
            args[4].setint(found->port);
            numargs = 5;
            page = renderhtmlpage("join", args, numargs);
        }
        loopi(numargs) args[i].cleanup();
        res.body.setsize(0);
        if(page)
        {
            res.body.put(page, strlen(page));
            DELETEA(page);
            return;
        }
        // no page template: a bare page that still sends people on
        static int warned = 0;
        if(!(warned&(found ? 1 : 2)))
        {
            conoutf(colouryellow, "Page template '%s' is not defined, using the plain fallback", found ? "join" : "joinmissing");
            warned |= found ? 1 : 2;
        }
        vector<char> &buf = res.body;
        htmlput(buf, "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><title>");
        if(!found)
        {
            htmltext(buf, versionname, true);
            htmlput(buf, "</title></head><body><p>That server isn't on the list right now. <a href=\"https://");
            htmltext(buf, versionurl, false);
            htmlput(buf, "/servers\">See the servers playing now</a>.</p></body></html>\n");
            return;
        }
        htmltext(buf, desc, true);
        htmlput(buf, "</title></head><body><p><a href=\"");
        htmltext(buf, link, false);
        htmlput(buf, "\">Join</a> &middot; <a href=\"https://");
        htmltext(buf, versionurl, false);
        htmlput(buf, "/download\">Get ");
        htmltext(buf, versionname, true);
        htmlput(buf, "</a></p><script>location.href=");
        // the link is only digits, dots and a host name the master checked, but it is written as a JSON string all the same
        json::writestring(buf, link);
        htmlput(buf, ";</script></body></html>\n");
    }

    static int reappeak(masterclient &c, int bucket)
    {
        uint h = 2166136261U;
        const char *s = c.name;
        while(*s) h = (h ^ uchar(*s++)) * 16777619U;
        h = (h ^ uint(c.port)) * 16777619U;
        h = (h ^ uint(bucket)) * 16777619U;
        h ^= h >> 13;
        h *= 0x5bd1e995U;
        h ^= h >> 15;
        if(h % 11) return 0;
        return h % 7 ? 1 : 2;
    }

    static void writehistory(httprequest &req, httpresponse &res, serverhistory *h, masterclient *own)
    {
        const char *daysvar = req.var("days");
        int days = clamp(daysvar && *daysvar ? atoi(daysvar) : HISTORY_DEFAULTDAYS, 1, max(masterhistorydays, 1));
        int now = historybucket(), from = now - days * (86400 / HISTORY_BUCKET) + 1;
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.num("bucket", HISTORY_BUCKET);
        w.num("start", double(from) * HISTORY_BUCKET);
        w.beginarray("peaks");
        for(int b = from; b <= now; b++)
        {
            int p = h && b >= h->first && b <= h->last() ? h->peaks[b - h->first] : HISTORY_UNKNOWN;
            if(own && p == HISTORY_UNKNOWN && own->connected && b >= int(own->connected / HISTORY_BUCKET)) p = reappeak(*own, b);
            if(p == HISTORY_UNKNOWN) w.null(NULL);
            else w.num(NULL, p);
        }
        w.endarray();
        w.endobject();
        res.headers.set("Cache-Control", "public, max-age=60");
        res.json(buf.getbuf(), buf.length());
    }

    void viewerevent(httpwsevent &e, void *ctx)
    {
        switch(e.type)
        {
            case WSEV_OPEN:
            {
                listviewer &v = viewers.add();
                v.id = e.id;
                v.priv = e.req ? e.req->privilege : HTTP_PUBLIC;
                v.ip = e.req ? e.req->ip : 0;
                vector<char> buf;
                static const char head[] = "{\"event\":\"snapshot\",";
                buf.put(head, sizeof(head)-1);
                vector<char> list;
                serverlist(list, true, 0, v.ip, v.priv);
                buf.put(list.getbuf() + 1, list.length() - 1);
                http::wssend(e.id, buf.getbuf(), buf.length());
                queueviewerjoined(e);
                noticeviewerjoined(e);
                break;
            }
            case WSEV_MESSAGE: queueviewermessage(e); break; // all a viewer says is about the Matchmaking queue
            case WSEV_CLOSE: loopv(viewers) if(viewers[i].id == e.id) { viewers.remove(i); break; } queueviewerclosed(e.id); break;
            default: break;
        }
    }

    static void checklisted()
    {
        if(totalmillis - lastlistcheck < 1000) return;
        lastlistcheck = totalmillis;
        checkshield();
        checknotices();
        checkhistory();
        checkqueue();
        checksignins();
        vector<bool> seen;
        loopv(listed) seen.add(false);
        loopv(masterclients)
        {
            masterclient &s = *masterclients[i];
            if(!s.listserver) continue;
            vector<char> one, marked;
            jsonwriter w(one);
            writeserver(w, s);
            one.add('\0');
            bool isreaped = reaped(s);
            if(isreaped)
            {
                jsonwriter mw(marked);
                writeserver(mw, s, 0, true);
                marked.add('\0');
            }
            const char *mine = isreaped ? marked.getbuf() : one.getbuf();
            int found = -1;
            loopvj(listed) if(listed[j].port == s.port && !strcmp(listed[j].host, s.name)) { found = j; break; }
            bool existed = found >= 0, wasreaped = false, changed = false, markchanged = false;
            if(!existed)
            {
                listedserver &l = listed.add();
                copystring(l.host, s.name);
                l.port = s.port;
                l.addr = s.address.host;
                l.json = newstring(one.getbuf());
                l.marked = isreaped ? newstring(mine) : NULL;
                l.reaped = isreaped;
                seen.add(true);
            }
            else
            {
                listedserver &l = listed[found];
                seen[found] = true;
                wasreaped = l.reaped;
                changed = strcmp(l.json, one.getbuf()) != 0;
                markchanged = strcmp(l.marked ? l.marked : l.json, mine) != 0;
                if(changed) { DELETEA(l.json); l.json = newstring(one.getbuf()); }
                DELETEA(l.marked);
                l.marked = isreaped ? newstring(mine) : NULL;
                l.reaped = isreaped;
            }
            vector<char> msgs[5];
            loopvj(viewers)
            {
                listviewer &v = viewers[j];
                bool admin = v.priv >= mastershieldlock, own = v.ip == s.address.host;
                bool before = existed && (!wasreaped || admin || own), after = !isreaped || admin || own;
                if(!before && !after) continue;
                int kind = 0;
                if(after)
                {
                    if(before && !(admin ? markchanged : changed)) continue;
                    kind = 1 + (before ? 2 : 0) + (admin ? 1 : 0);
                }
                vector<char> &msg = msgs[kind];
                if(msg.empty())
                {
                    if(!kind)
                    {
                        jsonwriter rw(msg);
                        rw.beginobject();
                        rw.str("event", "remove");
                        rw.str("host", s.name);
                        rw.num("port", s.port);
                        rw.endobject();
                    }
                    else
                    {
                        const char *json = admin ? mine : one.getbuf();
                        defformatstring(head, "{\"event\":\"%s\",\"time\":%d,\"server\":", before ? "update" : "add", int(time(NULL)));
                        msg.put(head, strlen(head));
                        msg.put(json, strlen(json));
                        msg.add('}');
                    }
                }
                http::wssend(v.id, msg.getbuf(), msg.length());
            }
        }
        loopvrev(listed) if(!seen[i])
        {
            listedserver &l = listed[i];
            vector<char> msg;
            jsonwriter w(msg);
            w.beginobject();
            w.str("event", "remove");
            w.str("host", l.host);
            w.num("port", l.port);
            w.endobject();
            loopvj(viewers)
            {
                listviewer &v = viewers[j];
                if(!l.reaped || v.priv >= mastershieldlock || v.ip == l.addr) http::wssend(v.id, msg.getbuf(), msg.length());
            }
            DELETEA(l.json);
            DELETEA(l.marked);
            listed.remove(i);
        }
    }

    // the relay: browsers reach game servers through their link, the master keeps nothing it carries
    VAR(0, httprelaycache, 0, 2000, 60000); // how long anonymous GETs are kept
    VAR(0, httprelaycachemax, 0, 3600, 86400); // seconds an anonymous GET may be kept for when the server's Cache-Control asks for longer
    VAR(0, httprelaycachesize, 0, 32<<20, VAR_MAX); // bytes the relay may keep, the oldest go first
    VAR(0, httprelaymaxrate, 0, 262144, VAR_MAX); // bytes a second relayed for each server, 0 is unlimited

    struct relaypending
    {
        int id, deferred, link, time;
        char *cachekey;
    };
    static vector<relaypending> relaypendings;
    static int lastrelayid = 0;

    struct relaycached
    {
        char *key, *type, *etag, *control;
        int status, time, life;
        vector<char> body;

        relaycached() : key(NULL), type(NULL), etag(NULL), control(NULL), status(0), time(0), life(0) {}
        ~relaycached() { DELETEA(key); DELETEA(type); DELETEA(etag); DELETEA(control); }
    };
    static vector<relaycached *> relaycache;

    struct relayviewer
    {
        int id, link;
        bool secure; // only viewers on wss:// may sign in
        char events[FEEDFILTERLEN]; // the events it asked for, see http::feedfilter
    };
    static vector<relayviewer> relayviewers;

    struct relayrate
    {
        int link, window, bytes;
    };
    static vector<relayrate> relayrates;

    // the address and port are separate segments, as an IPv6 address has colons of its own
    masterclient *findrelay(httprequest &req)
    {
        const char *address = req.param("address"), *port = req.param("port");
        if(!address || !port || !*port || port[strspn(port, "0123456789")]) return NULL;
        int p = atoi(port);
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.link < 0 || !c.isserver || c.port != p) continue;
            if(!strcmp(c.name, address)) return showserver(c, req.ip, reaped(c) ? max(req.privilege, requestpriv(req)) : HTTP_PUBLIC) ? &c : NULL;
        }
        return NULL;
    }

    static bool relayallowed(masterclient &c, int bytes)
    {
        if(!httprelaymaxrate) return true;
        relayrate *r = NULL;
        loopv(relayrates) if(relayrates[i].link == c.link) { r = &relayrates[i]; break; }
        if(!r) { r = &relayrates.add(); r->link = c.link; r->window = totalmillis; r->bytes = 0; }
        if(totalmillis - r->window >= 1000) { r->window = totalmillis; r->bytes = 0; }
        if(r->bytes > httprelaymaxrate) return false;
        r->bytes += bytes;
        return true;
    }

    // an answer the server says may be kept, such as a map's preview, is kept that long up to httprelaycachemax, others for httprelaycache
    static int relaycachelife(const char *control)
    {
        if(!control || strstr(control, "no-store") || strstr(control, "no-cache") || strstr(control, "private")) return httprelaycache;
        const char *age = strstr(control, "max-age=");
        if(!age) return httprelaycache;
        return max(httprelaycache, clamp(atoi(age + 8), 0, httprelaycachemax) * 1000);
    }

    static void relaycachelookup(const char *key, httpresponse &res, bool &found)
    {
        found = false;
        loopvrev(relaycache)
        {
            relaycached *e = relaycache[i];
            if(totalmillis - e->time > e->life) { delete e; relaycache.remove(i); continue; }
            if(found || strcmp(e->key, key)) continue; // the newest answer, once, while the loop still clears out old ones
            res.status = e->status;
            if(e->type) res.headers.set("Content-Type", e->type);
            if(e->etag) res.headers.set("ETag", e->etag);
            if(e->control) res.headers.set("Cache-Control", e->control);
            res.body.put(e->body.getbuf(), e->body.length());
            found = true;
        }
    }

    // what a game server answers is shown on the master's origin, so it gets only these headers and its pages run in a sandbox of their own
    static const char * const relaykeep[] = { "Content-Type", "ETag", "Cache-Control", "Allow", "WWW-Authenticate", "Retry-After", NULL };

    static void relayheaders(httpresponse &res)
    {
        res.headers.set("X-Content-Type-Options", "nosniff");
        res.headers.set("Content-Security-Policy", "sandbox allow-scripts allow-forms allow-popups");
    }

    void relayroute(httprequest &req, httpresponse &res)
    {
        masterclient *c = findrelay(req);
        if(!c) { res.error(404, "no such server on the relay"); return; }
        if(c->relay < 1) { res.error(403, "this server doesn't allow relaying"); return; }
        const char *rest = req.param("*");
        if(rest && strlen(rest) >= MAXSTRLEN - 1) { res.error(414, "that path is too long to relay"); return; } // rather than cut short, asking for another path
        defformatstring(path, "/%s", rest ? rest : "");
        // a token that came over plain http from somewhere else has been seen on the way, so it doesn't cross
        bool anon = !req.headers.get("Authorization") || c->relay < 2 || (!req.secure && !req.local);
        string cachekey = "";
        if(anon && httprelaycache && req.method == HTTP_GET)
        {
            // a key cut short would let two requests share an answer, so those aren't kept
            int keylen = snprintf(cachekey, sizeof(cachekey), "%s:%d%s?%s", c->name, c->port, path, req.query ? req.query : "");
            if(keylen < 0 || keylen >= int(sizeof(cachekey))) cachekey[0] = '\0';
            else
            {
                bool found = false;
                relaycachelookup(cachekey, res, found);
                if(found) { relayheaders(res); return; }
            }
        }
        if(!relayallowed(*c, req.body.length() + 512)) { res.error(503, "this server's relay is busy, try again shortly"); res.headers.set("Retry-After", "1"); return; }
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("relay", "request");
        int id = lastrelayid = lastrelayid >= 0x3FFFFFFF ? 1 : lastrelayid + 1;
        w.num("id", id);
        w.utf8("method", http::methodnames[req.method]);
        w.utf8("path", path);
        if(req.query) w.utf8("query", req.query);
        w.utf8("ip", req.addr);
        w.beginobject("headers");
        static const char * const keep[] = { "Content-Type", "Accept", "Origin", "If-None-Match", NULL };
        for(int i = 0; keep[i]; i++) { const char *v = req.headers.get(keep[i]); if(v) w.utf8(keep[i], v); }
        if(!anon) w.utf8("Authorization", req.headers.get("Authorization"));
        w.endobject();
        if(req.body.length()) w.utf8("body", req.body.getbuf(), req.body.length());
        w.endobject();
        if(!http::wssend(c->link, buf.getbuf(), buf.length())) { res.error(502, "lost the link to that server"); return; }
        relaypending &p = relaypendings.add();
        p.id = id;
        p.link = c->link;
        p.time = totalmillis;
        p.deferred = http::deferred(req);
        p.cachekey = cachekey[0] ? newstring(cachekey) : NULL;
        res.defer();
    }

    // whether the server has viewers here, and the events they asked for between them, so it works out only those
    static void relayfeedcount(masterclient &c)
    {
        int viewers = 0;
        vector<char> names;
        loopv(relayviewers) if(relayviewers[i].link == c.link)
        {
            if(viewers++) names.add(',');
            names.put(relayviewers[i].events, strlen(relayviewers[i].events));
        }
        names.add('\0');
        char events[FEEDFILTERLEN];
        http::feedfilter(events, names.getbuf());
        vector<char> msg;
        jsonwriter w(msg);
        w.beginobject();
        w.utf8("relay", "feed");
        w.boolean("on", viewers > 0);
        if(viewers) w.utf8("events", events);
        w.endobject();
        http::wssend(c.link, msg.getbuf(), msg.length());
    }

    void relayresubscribe(masterclient &c)
    {
        loopv(relayviewers) if(relayviewers[i].link == c.link) { relayfeedcount(c); return; }
    }

    void relayviewerevent(httpwsevent &e, void *ctx)
    {
        switch(e.type)
        {
            case WSEV_OPEN:
            {
                masterclient *c = e.req ? findrelay(*e.req) : NULL;
                if(!c || c->relay < 1) { http::wsclose(e.id, 1008); break; }
                relayviewer &v = relayviewers.add();
                v.id = e.id;
                v.link = c->link;
                v.secure = e.req->secure || e.req->local;
                http::feedfilter(v.events, e.req->var("events"));
                vector<char> msg;
                jsonwriter w(msg);
                w.beginobject();
                w.utf8("relay", "viewer");
                w.num("id", e.id);
                w.utf8("ip", e.req->addr);
                const char *token = e.req->var("token");
                if(token && *token && c->relay >= 2 && (e.req->secure || e.req->local)) w.utf8("token", token); // only where the server lets tokens cross the master, and never one sent in the clear
                w.utf8("events", v.events);
                w.endobject();
                http::wssend(c->link, msg.getbuf(), msg.length());
                relayfeedcount(*c);
                break;
            }
            case WSEV_CLOSE:
            {
                loopv(relayviewers) if(relayviewers[i].id == e.id)
                {
                    int link = relayviewers[i].link;
                    relayviewers.remove(i);
                    defformatstring(gone, "{\"relay\":\"viewergone\",\"id\":%d}", e.id);
                    http::wssend(link, gone);
                    masterclient *c = findlink(link);
                    if(c) relayfeedcount(*c);
                    break;
                }
                break;
            }
            case WSEV_MESSAGE:
            {
                // at httprelay 2 a viewer may sign in, which the game server decides
                relayviewer *v = NULL;
                loopv(relayviewers) if(relayviewers[i].id == e.id) { v = &relayviewers[i]; break; }
                masterclient *c = v ? findlink(v->link) : NULL;
                if(!c || e.opcode != WS_TEXT) break;
                if(c->relay < 2) { http::wssend(e.id, "{\"event\":\"auth\",\"error\":\"this server doesn't allow signing in through the relay\"}"); break; }
                // what a signed in viewer is sent, addresses among it, mustn't cross in the clear
                if(!v->secure) { http::wssend(e.id, "{\"event\":\"auth\",\"error\":\"signing in through the relay needs wss://\"}"); break; }
                if(!relayallowed(*c, e.len + 64)) { http::wssend(e.id, "{\"event\":\"auth\",\"error\":\"this server's relay is busy, try again shortly\"}"); break; }
                vector<char> msg;
                jsonwriter w(msg);
                w.beginobject();
                w.utf8("relay", "viewermsg");
                w.num("id", e.id);
                w.utf8("data", e.data, e.len);
                w.endobject();
                http::wssend(c->link, msg.getbuf(), msg.length());
                break;
            }
            default: break;
        }
    }

    // a binary answer comes as its JSON head, a NUL, then the body as it is
    void relayjson(masterclient &c, const char *data, int len, const char *body, int bodylen)
    {
        jsonvalue *v = json::parse(data, len);
        if(!v) return;
        jsonvalue *status = v->get("status");
        if(status && status->type == JSON_OBJECT)
        {
            // its times are by its own clock, which needn't agree with ours, so they're moved onto ours
            jsonvalue *sent = v->get("time");
            if(sent && sent->type == JSON_NUMBER)
            {
                double offset = double(time(NULL)) - sent->num;
                jsonvalue *last = status->get("lastmatch");
                jsonvalue *times[3] = { status->get("ends"), status->get("stateends"), last && last->type == JSON_OBJECT ? last->get("ended") : NULL };
                loopi(3) if(times[i] && times[i]->type == JSON_NUMBER && (i || times[i]->num >= 0))
                {
                    times[i]->num += offset;
                    DELETEA(times[i]->str);
                }
            }
            // written again from the parsed value, so the list only ever carries well formed JSON:
            // fullstatus keeps all groups for Master shield and privileged callers; status keeps only default public fields
            vector<char> fullbuf, pubbuf, hashbuf;
            json::write(fullbuf, status);
            if(fullbuf.length() < STATUS_LIMIT)
            {
                fullbuf.add('\0');
                DELETEA(c.fullstatus);
                c.fullstatus = newstring(fullbuf.getbuf());
                jsonwriter pw(pubbuf), hw(hashbuf);
                writestatusfiltered(pw, status, 0);
                pubbuf.add('\0');
                DELETEA(c.status);
                c.status = newstring(pubbuf.getbuf());
                writestatusfiltered(hw, status, STATUS_INC_ALL, true);
                uint sh = listhash(hashbuf);
                if(sh != c.lastshieldhash) { c.lastshieldhash = sh; c.changed = true; }
                jsonvalue *players = status->get("players");
                c.numplayers = players && players->type == JSON_ARRAY ? players->length() : -1;
                if(players && players->type == JSON_ARRAY) fillnames(c, players);
                c.statusmax = clamp(status->getint("maxplayers"), 0, MAXCLIENTS);
                c.statusslots = max(status->getint("playerslots"), 0);
                c.statusplaying = max(c.numplayers - status->getint("spectators"), 0);
                c.statusreserved = max(status->getint("reserved"), 0);
                c.statusconnections = status->getint("connections", -1);
                c.statusmode = status->getint("mastermode", -1);
                c.statusver = status->getint("gameversion");
                if(!c.gamever && c.statusver > 0) c.gamever = c.statusver;
                jsonvalue *build = status->get("build");
                loopi(3) c.statusbuild[i] = build && build->type == JSON_ARRAY && build->at(i) ? int(build->at(i)->num) : 0;
                queuehandles(c, players);
                string map;
                filterstring(map, status->getstr("map", ""), false);
                vector<const char *> names;
                if(players && players->type == JSON_ARRAY) loopv(players->items) if(players->items[i]->type == JSON_OBJECT) names.add(players->items[i]->getstr("name", ""));
                noteserverstate(c, map, status->getint("modeid"), status->getint("mutators"), names);
                DELETEA(c.polled); // what it reports from now on is better than any poll
                c.pollwaiting = false;
                c.pollmisses = 0;
            }
            delete v;
            return;
        }
        const char *type = v->getstr("relay", "");
        if(!strcmp(type, "response"))
        {
            int id = v->getint("id", -1);
            loopv(relaypendings) if(relaypendings[i].id == id && relaypendings[i].link == c.link)
            {
                relaypending p = relaypendings.remove(i);
                double status = v->getnum("status", 502);
                httpresponse res(status >= 200 && status <= 599 ? int(status) : 502);
                jsonvalue *headers = v->get("headers");
                for(int k = 0; headers && relaykeep[k]; k++)
                {
                    const char *val = headers->getstr(relaykeep[k]);
                    if(val) res.headers.set(relaykeep[k], val);
                }
                relayheaders(res);
                if(body) res.body.put(body, bodylen);
                else
                {
                    jsonvalue *text = v->get("body");
                    if(text && text->str) res.body.put(text->str, strlen(text->str));
                }
                relayallowed(c, res.body.length());
                if(p.cachekey && res.status == 200)
                {
                    // two misses for the same key can both be answered, only the newest is kept
                    loopvjrev(relaycache) if(!strcmp(relaycache[j]->key, p.cachekey)) delete relaycache.remove(j);
                    relaycached *e = new relaycached;
                    e->key = p.cachekey;
                    p.cachekey = NULL;
                    const char *t = res.headers.get("Content-Type"), *etag = res.headers.get("ETag"), *control = res.headers.get("Cache-Control");
                    e->type = t ? newstring(t) : NULL;
                    e->etag = etag ? newstring(etag) : NULL;
                    e->control = control ? newstring(control) : NULL;
                    e->status = res.status;
                    e->time = totalmillis;
                    e->life = relaycachelife(control);
                    e->body.put(res.body.getbuf(), res.body.length());
                    relaycache.add(e);
                    int bytes = 0;
                    loopvj(relaycache) bytes += relaycache[j]->body.length();
                    while(relaycache.length() > 1024 || (httprelaycachesize && bytes > httprelaycachesize && relaycache.length() > 1))
                    {
                        relaycached *old = relaycache.remove(0);
                        bytes -= old->body.length();
                        delete old;
                    }
                }
                DELETEA(p.cachekey);
                http::reply(p.deferred, res);
                break;
            }
        }
        else if(!strcmp(type, "event"))
        {
            jsonvalue *event = v->get("event");
            if(event)
            {
                static vector<int> ids;
                ids.setsize(0);
                const char *name = event->getstr("event", "");
                loopv(relayviewers) if(relayviewers[i].link == c.link && http::feedwants(relayviewers[i].events, name)) ids.add(relayviewers[i].id);
                if(ids.length())
                {
                    vector<char> buf;
                    json::write(buf, event);
                    http::wsbroadcast(ids, buf.getbuf(), buf.length());
                }
            }
        }
        else if(!strcmp(type, "tell"))
        {
            // for the viewers the game server names, which must be its own
            jsonvalue *ids = v->get("ids"), *msg = v->get("data");
            if(ids && msg && msg->str)
            {
                static vector<int> to;
                to.setsize(0);
                loopv(ids->items)
                {
                    int id = jsonint(ids->items[i]->num);
                    loopvj(relayviewers) if(relayviewers[j].id == id && relayviewers[j].link == c.link) { to.add(id); break; }
                }
                if(to.length()) http::wsbroadcast(to, msg->str, strlen(msg->str));
            }
        }
        else if(!strcmp(type, "deny"))
        {
            int id = v->getint("id", -1);
            loopv(relayviewers) if(relayviewers[i].id == id && relayviewers[i].link == c.link) { http::wsclose(id, 1008); relayviewers.remove(i); break; }
        }
        delete v;
    }

    void relayclosed(masterclient &c)
    {
        if(c.link < 0) return;
        loopvrev(relaypendings) if(relaypendings[i].link == c.link)
        {
            relaypending p = relaypendings.remove(i);
            DELETEA(p.cachekey);
            httpresponse res;
            res.error(502, "lost the link to that server");
            http::reply(p.deferred, res);
        }
        loopvrev(relayviewers) if(relayviewers[i].link == c.link) { http::wsclose(relayviewers[i].id, 1001); relayviewers.remove(i); }
        loopvrev(relayrates) if(relayrates[i].link == c.link) relayrates.remove(i);
    }

    void checkmasterlinks()
    {
        loopvrev(relaypendings) if(totalmillis - relaypendings[i].time > 60000) { DELETEA(relaypendings[i].cachekey); relaypendings.remove(i); }
        checklisted();
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.link < 0) continue;
            if(c.output.length())
            {
                http::wssend(c.link, c.output.getbuf(), c.output.length());
                c.output.setsize(0);
                c.outputpos = 0;
                if(c.shouldpurge) { purgemasterclient(i--); continue; }
            }
            if(ENET_TIME_DIFFERENCE(totalmillis, c.lastactivity) >= (c.isserver ? SERVER_TIME : CLIENT_TIME) ||
               (checkipinfo(control, ipinfo::BAN, c.address.host) && c.priority() <= 0 && !checkipinfo(control, ipinfo::EXCEPT, c.address.host) && !checkipinfo(control, ipinfo::TRUST, c.address.host)))
            {
                purgemasterclient(i--);
                continue;
            }
        }
    }

}
