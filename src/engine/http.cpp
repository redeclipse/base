// Web API, the main thread's side: routes, replies, outgoing requests and WebSockets

#include "engine.h"

namespace http
{
    const char *methodnames[HTTP_MAXMETHODS] = { "", "GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS" };

    int findmethod(const char *name, int len)
    {
        if(len < 0) len = strlen(name);
        for(int i = 1; i < HTTP_MAXMETHODS; i++) if(int(strlen(methodnames[i])) == len && !strncmp(methodnames[i], name, len)) return i;
        return HTTP_NONE;
    }

    const char *statusname(int code)
    {
        switch(code)
        {
            case 100: return "Continue";
            case 101: return "Switching Protocols";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 413: return "Content Too Large";
            case 415: return "Unsupported Media Type";
            case 426: return "Upgrade Required";
            case 429: return "Too Many Requests";
            case 431: return "Request Header Fields Too Large";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            default: return code < 300 ? "OK" : (code < 400 ? "Redirect" : (code < 500 ? "Client Error" : "Server Error"));
        }
    }

    static int hexdigit(char c)
    {
        if(c >= '0' && c <= '9') return c - '0';
        if(c >= 'a' && c <= 'f') return c - 'a' + 10;
        if(c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    char *urldecode(char *dst, const char *str, size_t len)
    {
        size_t n = 0;
        for(const char *p = str; *p && n+1 < len; p++)
        {
            if(*p == '%' && hexdigit(p[1]) >= 0 && hexdigit(p[2]) >= 0)
            {
                dst[n++] = char((hexdigit(p[1])<<4)|hexdigit(p[2]));
                p += 2;
            }
            else dst[n++] = *p == '+' ? ' ' : *p;
        }
        dst[n] = '\0';
        return dst;
    }

    static char *urldecodenew(const char *str, int len)
    {
        char *src = newstring(str, len), *dst = newstring(len);
        urldecode(dst, src, len+1);
        delete[] src;
        return dst;
    }

    void urlencode(vector<char> &buf, const char *str)
    {
        static const char hex[] = "0123456789ABCDEF";
        for(const uchar *p = (const uchar *)str; *p; p++)
        {
            if(isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') buf.add(*p);
            else { buf.add('%'); buf.add(hex[*p>>4]); buf.add(hex[*p&15]); }
        }
    }

    int vardecode(const char *str, httpheaders &vars)
    {
        int count = 0;
        for(const char *p = str; *p;)
        {
            int len = strcspn(p, "&");
            const char *eq = (const char *)memchr(p, '=', len);
            if(len > 0)
            {
                char *name = urldecodenew(p, eq ? eq - p : len), *value = eq ? urldecodenew(eq + 1, len - (eq + 1 - p)) : newstring("");
                httppair &v = vars.list.add();
                v.name = name;
                v.value = value;
                count++;
            }
            p += len;
            if(*p) p++;
        }
        return count;
    }
}

bool httpheaders::hastoken(const char *name, const char *token)
{
    int tlen = strlen(token);
    loopv(list) if(!strcasecmp(list[i].name, name))
    {
        for(const char *p = list[i].value; *p;)
        {
            p += strspn(p, " \t,");
            int len = strcspn(p, ",;");
            const char *end = p + len;
            while(end > p && (end[-1] == ' ' || end[-1] == '\t')) end--;
            if(end - p == tlen && !strncasecmp(p, token, tlen)) return true;
            p += len;
            if(*p == ';') p += strcspn(p, ",");
        }
    }
    return false;
}

bool httpurl::parse(const char *url)
{
    const char *p = url;
    if(!strncasecmp(p, "https://", 8)) { tls = true; ws = false; p += 8; }
    else if(!strncasecmp(p, "http://", 7)) { tls = false; ws = false; p += 7; }
    else if(!strncasecmp(p, "wss://", 6)) { tls = true; ws = true; p += 6; }
    else if(!strncasecmp(p, "ws://", 5)) { tls = false; ws = true; p += 5; }
    else return false;
    int hostlen = strcspn(p, ":/?#");
    if(hostlen <= 0 || hostlen >= MAXSTRLEN) return false;
    copystring(host, p, hostlen+1);
    p += hostlen;
    port = tls ? HTTPS_PORT : HTTP_PORT;
    if(*p == ':')
    {
        p++;
        char *end = NULL;
        long n = strtol(p, &end, 10);
        if(end == p || n <= 0 || n > 65535) return false;
        port = int(n);
        p = end;
    }
    int pathlen = strcspn(p, "#");
    DELETEA(path);
    if(*p != '/' && *p != '?' && pathlen) return false;
    if(*p == '?') { path = newstring(pathlen + 1); path[0] = '/'; copystring(path + 1, p, pathlen + 1); }
    else path = newstring(pathlen ? p : "/", pathlen ? pathlen : 1);
    for(char *s = path; *s; s++) if(*s <= ' ' || *s == 0x7F) return false;
    for(char *s = host; *s; s++) if(*s <= ' ' || *s == '@') return false;
    return true;
}

void httpresponse::error(int code, const char *msg)
{
    status = code;
    vector<char> b;
    const char *text = msg ? msg : http::statusname(code);
    b.put("{\"error\":\"", 10);
    for(const char *s = text; *s; s++)
    {
        if(*s == '"' || *s == '\\') { b.add('\\'); b.add(*s); }
        else if(uchar(*s) < 0x20) { defformatstring(esc, "\\u%04x", *s); b.put(esc, strlen(esc)); }
        else b.add(*s);
    }
    b.put("\"}", 2);
    json(b.getbuf(), b.length());
}

namespace http
{
    void check();
#ifdef STANDALONE
    VARF(0, httpserver, 0, 1, 1, check());
#else
    VARF(0, httpserver, 0, 0, 1, check());
#endif
    VARF(0, httpserverport, 1, HTTP_LISTEN, 65535, check());
    VARF(0, httpserverportrange, 0, 16, 1000, check());
    SVARF(0, httpserverip, "", check());
    VAR(0, httpverbose, 0, 0, 2);

    void loadcert();
    SVARF(0, httpcertfile, "", loadcert());
    SVARF(0, httpkeyfile, "", loadcert());
    SVARF(0, httpcafile, "", { if(httpnet::tlsready()) httpnet::cafile(httpcafile); });

    void sync();
    VARF(0, httpmaxconns, 1, 256, 65535, sync());
    VARF(0, httpmaxperaddr, 1, 16, 65535, sync());
    VARF(0, httpmaxheaders, 8, 64, 1024, sync());
    VARF(0, httpmaxline, 256, 8192, 1<<20, sync());
    VARF(0, httpmaxbody, 0, 1<<20, 1<<30, sync());
    VARF(0, httpmaxmessage, 125, 16<<20, 1<<30, sync());
    VARF(0, httpidletime, 1000, 15000, VAR_MAX, sync());
    VARF(0, httppingtime, 1000, 30000, VAR_MAX, sync());
    VARF(0, httphandshakethreads, 0, 2, 16, sync());
    VARF(0, httphandshakerate, 1, 30, VAR_MAX, sync()); // TLS handshakes per address every ten seconds
    VAR(0, httpdefertime, 1000, 30000, VAR_MAX);
    VAR(0, httpallowinsecure, 0, 0, 1); // allow routes above Player over plain HTTP from anywhere
    SVAR(0, httptrustproxy, ""); // addresses of reverse proxies whose X-Forwarded-For is believed
    SVAR(0, httpalloworigin, ""); // origins browsers may use the Web API from, empty allows any

    struct deferral
    {
        int id, conn, reply, method;
        bool keepalive, gzip;
        char *origin;
        void *relay;
        uint time;

        deferral() : id(-1), conn(-1), reply(HTTP_REPLY_NET), method(HTTP_GET), keepalive(false), gzip(false), origin(NULL), relay(NULL), time(0) {}
        ~deferral() { DELETEA(origin); }
    };

    struct socketconn
    {
        int id;
        httpwscb fun;
        void *ctx;
        bool client, open;
    };

    static vector<httproute> &routes()
    {
        static vector<httproute> list;
        return list;
    }

    static vector<deferral *> deferrals;
    static vector<httpfetch *> fetches;
    static vector<socketconn> sockets;
    static int lastid = 0, lastdeferral = 0, boundport = 0, lastcertcheck = 0;
    static time_t certtime = 0, keytime = 0;
    static bool started = false;
    static string certfingerprint = "";

    static int nextid()
    {
        lastid = lastid >= 0x3FFFFFFF ? 1 : lastid + 1;
        return lastid;
    }

    static void getconfig(httpnet::config &c)
    {
        c.maxconns = httpmaxconns;
        if(masterserver) c.maxconns = max(c.maxconns, 8192); // browser viewers of the master's live server list
        c.maxperaddr = httpmaxperaddr;
        c.maxheaders = httpmaxheaders;
        c.maxline = httpmaxline;
        c.maxbody = httpmaxbody;
        c.maxmessage = httpmaxmessage;
        c.idletime = httpidletime;
        c.pingtime = httppingtime;
        c.handshakethreads = httphandshakethreads;
        c.handshakerate = httphandshakerate;
    }

    void sync()
    {
        if(!started) return;
        httpnet::config c;
        getconfig(c);
        httpnet::setconfig(c);
    }

    bool addroute(int method, const char *pattern, int minpriv, httproutefun fun)
    {
        vector<httproute> &list = routes();
        loopv(list) if(list[i].method == method && !strcmp(list[i].pattern, pattern)) return false;
        httproute &r = list.add();
        r.method = method;
        r.minpriv = minpriv;
        r.pattern = pattern;
        r.fun = fun;
        r.ws = NULL;
        r.ctx = NULL;
        r.script = false;
        return true;
    }

    bool addsocket(const char *pattern, int minpriv, httpwscb fun, void *ctx)
    {
        vector<httproute> &list = routes();
        loopv(list) if(list[i].method == HTTP_GET && !strcmp(list[i].pattern, pattern)) return false;
        httproute &r = list.add();
        r.method = HTTP_GET;
        r.minpriv = minpriv;
        r.pattern = pattern;
        r.fun = NULL;
        r.ws = fun;
        r.ctx = ctx;
        r.script = false;
        return true;
    }

    bool removeroute(int method, const char *pattern)
    {
        vector<httproute> &list = routes();
        loopv(list) if(list[i].method == method && !strcmp(list[i].pattern, pattern)) { list.remove(i); return true; }
        return false;
    }

    // ":name" matches one segment, a final "*" matches the rest
    static bool matchroute(const char *pattern, const char *path, httpheaders &params)
    {
        const char *p = pattern, *s = path;
        while(*p)
        {
            if(*p == '*' && !p[1]) { params.add("*", s); return true; }
            if(*p == ':')
            {
                const char *name = ++p;
                int namelen = strcspn(p, "/"), seglen = strcspn(s, "/");
                if(!seglen) return false;
                params.add(name, s, namelen, seglen);
                p += namelen;
                s += seglen;
                continue;
            }
            if(*p != *s) return false;
            p++;
            s++;
        }
        return !*s;
    }

    static bool parseip(const char *s, uint &ip)
    {
        ENetAddress a;
        string host;
        int len = strcspn(s, ", \t");
        copystring(host, s, min(len + 1, int(sizeof(host))));
        if(enet_address_set_host_ip(&a, host) < 0) return false;
        ip = a.host;
        return true;
    }

    bool trustedproxy(uint ip)
    {
        if(!*httptrustproxy) return false;
        for(const char *p = httptrustproxy; *p;)
        {
            p += strspn(p, ", \t");
            if(!*p) break;
            uint addr = 0;
            if(parseip(p, addr) && addr == ip) return true;
            p += strcspn(p, ", \t");
        }
        return false;
    }

    bool alloworigin(const char *origin)
    {
        if(!origin || !*httpalloworigin) return true;
        int olen = strlen(origin);
        for(const char *p = httpalloworigin; *p;)
        {
            p += strspn(p, ", \t");
            int len = strcspn(p, ", \t");
            if(len == 1 && *p == '*') return true;
            if(len == olen && !strncasecmp(p, origin, len)) return true;
            p += len;
        }
        return false;
    }

    static bool isloopback(uint ip)
    {
        return (ENET_NET_TO_HOST_32(ip)>>24) == 127;
    }

    static bool compressible(httpresponse &res)
    {
        const char *type = res.headers.get("Content-Type", "");
        return !strncmp(type, "text/", 5) || strstr(type, "json") || strstr(type, "javascript") || strstr(type, "xml");
    }

    static bool gzipbody(vector<char> &body)
    {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        if(deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15+16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
        vector<char> out;
        out.reserve(deflateBound(&zs, body.length()) + 32);
        zs.next_in = (Bytef *)body.getbuf();
        zs.avail_in = body.length();
        zs.next_out = (Bytef *)out.getbuf();
        zs.avail_out = out.capacity();
        int ret = deflate(&zs, Z_FINISH);
        int len = out.capacity() - zs.avail_out;
        deflateEnd(&zs);
        if(ret != Z_STREAM_END || len >= body.length()) return false;
        out.advance(len);
        body.setsize(0);
        body.put(out.getbuf(), out.length());
        return true;
    }

    static void corsheaders(httpresponse &res, const char *origin)
    {
        if(!origin || !alloworigin(origin)) return;
        res.headers.set("Access-Control-Allow-Origin", *httpalloworigin ? origin : "*");
        if(*httpalloworigin) res.headers.set("Vary", "Origin");
    }

    // the master's relay sets this to carry replies back over a game server's link
    void (*relayreply)(void *relay, httpresponse &res) = NULL;

    static void sendreply(int conn, int method, bool keepalive, bool gzip, const char *origin, httpresponse &res, int flags = httpnet::SEND_DONE)
    {
        corsheaders(res, origin);
        if(gzip && res.body.length() > 1024 && !res.headers.find("Content-Encoding") && compressible(res) && gzipbody(res.body))
        {
            res.headers.set("Content-Encoding", "gzip");
            res.headers.set("Vary", res.headers.find("Vary") ? "Origin, Accept-Encoding" : "Accept-Encoding");
        }
        if(res.close) keepalive = false;
        vector<char> buf;
        defformatstring(line, "HTTP/1.1 %d %s\r\n", res.status, statusname(res.status));
        buf.put(line, strlen(line));
        if(!res.headers.find("Server")) { defformatstring(server, "Server: %s\r\n", getverstr()); buf.put(server, strlen(server)); }
        loopv(res.headers.list)
        {
            httppair &h = res.headers.list[i];
            if(!strcasecmp(h.name, "Content-Length") || !strcasecmp(h.name, "Connection") || !strcasecmp(h.name, "Transfer-Encoding")) continue;
            if(strpbrk(h.name, "\r\n:") || strpbrk(h.value, "\r\n")) continue;
            buf.put(h.name, strlen(h.name));
            buf.put(": ", 2);
            buf.put(h.value, strlen(h.value));
            buf.put("\r\n", 2);
        }
        if(res.status != 101)
        {
            if(res.status >= 200 && res.status != 204 && res.status != 304)
            {
                defformatstring(len, "Content-Length: %d\r\n", res.body.length());
                buf.put(len, strlen(len));
            }
            const char *c = keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
            buf.put(c, strlen(c));
        }
        buf.put("\r\n", 2);
        if(method != HTTP_HEAD && res.status >= 200 && res.status != 204 && res.status != 304) buf.put(res.body.getbuf(), res.body.length());
        if(!keepalive) flags |= httpnet::SEND_CLOSE;
        httpnet::send(conn, buf.getbuf(), buf.length(), flags);
    }

    void respond(httprequest &req, httpresponse &res)
    {
        if(httpverbose) conoutf(colourwhite, "HTTP %s %s %s -> %d", req.addr, methodnames[req.method], req.path ? req.path : "", res.status);
        switch(req.reply)
        {
            case HTTP_REPLY_RELAY: corsheaders(res, req.origin); if(relayreply) relayreply(req.relay, res); break;
            case HTTP_REPLY_SCRIPT: break;
            default: sendreply(req.id, req.method, req.keepalive, req.gzip, req.origin, res); break;
        }
    }

    int deferred(httprequest &req)
    {
        deferral *d = new deferral;
        d->id = lastdeferral = lastdeferral >= 0x3FFFFFFF ? 1 : lastdeferral + 1;
        d->conn = req.id;
        d->reply = req.reply;
        d->method = req.method;
        d->keepalive = req.keepalive;
        d->gzip = req.gzip;
        d->origin = req.origin ? newstring(req.origin) : NULL;
        d->relay = req.relay;
        d->time = totalmillis;
        deferrals.add(d);
        return d->id;
    }

    bool reply(int id, httpresponse &res)
    {
        loopv(deferrals) if(deferrals[i]->id == id)
        {
            deferral *d = deferrals.remove(i);
            switch(d->reply)
            {
                case HTTP_REPLY_RELAY: corsheaders(res, d->origin); if(relayreply) relayreply(d->relay, res); break;
                case HTTP_REPLY_SCRIPT: break;
                default: sendreply(d->conn, d->method, d->keepalive, d->gzip, d->origin, res); break;
            }
            delete d;
            return true;
        }
        return false;
    }

    static void opensocket(httprequest &req, httproute &r)
    {
        const char *key = req.headers.get("Sec-WebSocket-Key");
        string accept;
        defformatstring(s, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
        uchar digest[20];
        sha1((const uchar *)s, strlen(s), digest);
        vector<char> b;
        base64(b, digest, 20);
        b.add('\0');
        copystring(accept, b.getbuf());
        httpresponse res(101);
        res.headers.set("Upgrade", "websocket");
        res.headers.set("Connection", "Upgrade");
        res.headers.set("Sec-WebSocket-Accept", accept);
        sendreply(req.id, req.method, true, false, NULL, res, httpnet::SEND_UPGRADE);
        socketconn &sc = sockets.add();
        sc.id = req.id;
        sc.fun = r.ws;
        sc.ctx = r.ctx;
        sc.client = false;
        sc.open = true;
        httpwsevent e;
        e.type = WSEV_OPEN;
        e.id = req.id;
        e.opcode = e.code = 0;
        e.data = NULL;
        e.len = 0;
        e.req = &req;
        r.ws(e, r.ctx);
    }

    void dispatch(httprequest &req)
    {
        httpresponse res;
        if(req.reply == HTTP_REPLY_NET && checkipinfo(control, ipinfo::BAN, req.ip) && !checkipinfo(control, ipinfo::EXCEPT, req.ip) && !checkipinfo(control, ipinfo::TRUST, req.ip))
        {
            res.error(403, "banned");
            res.close = true;
            respond(req, res);
            return;
        }
        if(req.method == HTTP_OPTIONS)
        {
            res.status = 204;
            if(req.origin && alloworigin(req.origin))
            {
                res.headers.set("Access-Control-Allow-Methods", "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS");
                res.headers.set("Access-Control-Allow-Headers", "Authorization, Content-Type");
                res.headers.set("Access-Control-Max-Age", "86400");
            }
            respond(req, res);
            return;
        }
        req.privilege = HTTP_PUBLIC;
        server::httpaccess(req);
        vector<httproute> &list = routes();
        httproute *found = NULL;
        bool pathmatched = false;
        string allow = "";
        loopv(list)
        {
            httproute &r = list[i];
            httpheaders params;
            if(!matchroute(r.pattern, req.path, params)) continue;
            pathmatched = true;
            if(r.method != req.method && !(req.method == HTTP_HEAD && r.method == HTTP_GET))
            {
                if(!strstr(allow, methodnames[r.method])) { if(*allow) concatstring(allow, ", "); concatstring(allow, methodnames[r.method]); }
                continue;
            }
            found = &r;
            req.route = r.pattern;
            req.params.clear();
            loopvj(params.list) req.params.add(params.list[j].name, params.list[j].value);
            break;
        }
        if(!found)
        {
            if(pathmatched) { res.error(405); res.headers.set("Allow", allow); }
            else res.error(404);
            respond(req, res);
            return;
        }
        if(req.privilege < found->minpriv)
        {
            res.error(req.token ? 403 : 401, req.token ? "insufficient privilege" : "authentication required");
            if(!req.token) res.headers.set("WWW-Authenticate", "Bearer");
            respond(req, res);
            return;
        }
        if(found->minpriv > HTTP_PUBLIC && !req.secure && !req.local && !httpallowinsecure)
        {
            res.error(403, "this route needs https, localhost or a trusted proxy");
            respond(req, res);
            return;
        }
        if(found->ws)
        {
            if(req.reply != HTTP_REPLY_NET || !req.upgrade) { res.error(426); res.headers.set("Upgrade", "websocket"); respond(req, res); return; }
            if(!alloworigin(req.origin)) { res.error(403, "origin not allowed"); respond(req, res); return; }
            opensocket(req, *found);
            return;
        }
        found->fun(req, res);
        if(!res.deferred) respond(req, res);
    }

    static void handlerequest(httpnet::event &e)
    {
        httpmessage *m = e.msg;
        httprequest req;
        req.id = e.id;
        req.method = m->method;
        req.version = m->version;
        req.ip = req.peer = e.ip;
        req.secure = e.secure;
        loopv(m->headers.list) req.headers.add(m->headers.list[i].name, m->headers.list[i].value);
        if(trustedproxy(req.peer))
        {
            const char *fwd = req.headers.get("X-Forwarded-For");
            if(fwd)
            {
                const char *last = strrchr(fwd, ',');
                uint ip = 0;
                if(parseip(last ? last + 1 + strspn(last + 1, " \t") : fwd, ip)) req.ip = ip;
            }
            const char *proto = req.headers.get("X-Forwarded-Proto");
            if(proto && !strcasecmp(proto, "https")) req.secure = true;
        }
        req.local = isloopback(req.ip) || (isloopback(req.peer) && req.ip == req.peer);
        ENetAddress a;
        a.host = req.ip;
        if(enet_address_get_host_ip(&a, req.addr, sizeof(req.addr)) < 0) copystring(req.addr, "unknown");
        const char *target = m->target ? m->target : "/", *q = strchr(target, '?');
        req.path = urldecodenew(target, q ? q - target : strlen(target));
        if(q)
        {
            req.query = newstring(q + 1);
            vardecode(req.query, req.vars);
        }
        const char *conn = req.headers.get("Connection");
        req.keepalive = req.version >= 11 ? !(conn && req.headers.hastoken("Connection", "close")) : (conn && req.headers.hastoken("Connection", "keep-alive"));
        req.upgrade = req.method == HTTP_GET && req.headers.hastoken("Upgrade", "websocket") && req.headers.hastoken("Connection", "upgrade") &&
            req.headers.get("Sec-WebSocket-Key") && req.headers.hastoken("Sec-WebSocket-Version", "13");
        req.gzip = req.headers.hastoken("Accept-Encoding", "gzip");
        const char *origin = req.headers.get("Origin");
        if(origin) req.origin = newstring(origin);
        const char *auth = req.headers.get("Authorization");
        if(auth && !strncasecmp(auth, "Bearer ", 7)) req.token = newstring(auth + 7 + strspn(auth + 7, " "));
        const char *type = req.headers.get("Content-Type");
        req.body.put(m->body.getbuf(), m->body.length());
        if(type && !strncasecmp(type, "application/x-www-form-urlencoded", 33) && req.body.length())
        {
            req.body.add('\0');
            vardecode(req.body.getbuf(), req.vars);
            req.body.pop();
        }
        if(req.path[0] != '/') { httpresponse res; res.error(400); res.close = true; respond(req, res); return; }
        dispatch(req);
    }

    static socketconn *findsocket(int id)
    {
        loopv(sockets) if(sockets[i].id == id) return &sockets[i];
        return NULL;
    }

    static void socketevent(int type, int id, int opcode, int code, const char *data, int len)
    {
        socketconn *sc = findsocket(id);
        if(!sc) return;
        httpwsevent e;
        e.type = type;
        e.id = id;
        e.opcode = opcode;
        e.code = code;
        e.data = data;
        e.len = len;
        e.req = NULL;
        httpwscb fun = sc->fun;
        void *ctx = sc->ctx;
        if(type == WSEV_OPEN) sc->open = true;
        if(type == WSEV_CLOSE) loopv(sockets) if(sockets[i].id == id) { sockets.remove(i); break; }
        fun(e, ctx);
    }

    static void fetched(httpnet::event &e)
    {
        loopv(fetches) if(fetches[i]->id == e.id)
        {
            httpfetch *f = fetches.remove(i);
            if(e.msg)
            {
                f->status = e.msg->status;
                loopvj(e.msg->headers.list) f->headers.add(e.msg->headers.list[j].name, e.msg->headers.list[j].value);
                f->body.put(e.msg->body.getbuf(), e.msg->body.length());
                if(e.msg->file && f->file)
                {
                    remove(f->file);
                    if(rename(e.msg->file, f->file)) { remove(e.msg->file); f->error = newstring("unable to move the download into place"); f->status = 0; }
                }
            }
            if(e.error) f->error = newstring(e.error);
            if(httpverbose) conoutf(colourwhite, "HTTP fetch %s -> %d%s%s", f->url, f->status, f->error ? " " : "", f->error ? f->error : "");
            if(f->callback && !f->cancelled) f->callback(*f);
            delete f;
            return;
        }
        if(e.msg && e.msg->file) remove(e.msg->file);
    }

    void runframe()
    {
        if(!started) return;
        if(httpserver && boundport && *httpcertfile && totalmillis - lastcertcheck >= 60000)
        {
            lastcertcheck = totalmillis;
            struct stat cs, ks;
            if(!stat(httpcertfile, &cs) && !stat(*httpkeyfile ? httpkeyfile : httpcertfile, &ks) && (cs.st_mtime != certtime || ks.st_mtime != keytime)) loadcert();
        }
        static vector<httpnet::event *> evs;
        evs.setsize(0);
        httpnet::getevents(evs);
        loopv(evs)
        {
            httpnet::event &e = *evs[i];
            switch(e.type)
            {
                case httpnet::EV_REQUEST: if(e.msg) handlerequest(e); break;
                case httpnet::EV_CLOSED: loopvjrev(deferrals) if(deferrals[j]->conn == e.id && deferrals[j]->reply == HTTP_REPLY_NET) delete deferrals.remove(j); break;
                case httpnet::EV_FETCHED: fetched(e); break;
                case httpnet::EV_WSOPEN: socketevent(WSEV_OPEN, e.id, 0, 0, NULL, 0); break;
                case httpnet::EV_WSMESSAGE: socketevent(WSEV_MESSAGE, e.id, e.opcode, 0, e.data.getbuf(), e.data.length()); break;
                case httpnet::EV_WSCLOSE:
                {
                    if(e.error && httpverbose) conoutf(colourorange, "WebSocket %d closed: %s", e.id, e.error);
                    socketevent(WSEV_CLOSE, e.id, 0, e.code, e.error, e.error ? strlen(e.error) : 0);
                    break;
                }
            }
            delete evs[i];
        }
        evs.setsize(0);
        loopvrev(deferrals) if(totalmillis - deferrals[i]->time > uint(httpdefertime))
        {
            httpresponse res;
            res.error(504, "timed out waiting for an answer");
            reply(deferrals[i]->id, res);
        }
    }

    httpfetch *newfetch(int method, const char *url, httpfetchcb callback, void *ctx)
    {
        httpfetch *f = new httpfetch;
        f->method = method;
        f->url = newstring(url);
        f->callback = callback;
        f->ctx = ctx;
        return f;
    }

    static void fetchfail(httpfetch *f, const char *error)
    {
        f->error = newstring(error);
        if(f->callback) f->callback(*f);
        delete f;
    }

    int submit(httpfetch *f)
    {
        if(!started) { fetchfail(f, "the network thread is not running"); return -1; }
        if(!f->reqheaders.find("User-Agent")) f->reqheaders.add("User-Agent", getverstr());
        f->id = nextid();
        if(!httpnet::fetch(f->id, *f, false, f->maxsize)) { fetchfail(f, "invalid URL"); return -1; }
        fetches.add(f);
        return f->id;
    }

    bool cancel(int id)
    {
        loopv(fetches) if(fetches[i]->id == id)
        {
            delete fetches.remove(i);
            httpnet::close(id);
            return true;
        }
        socketconn *sc = findsocket(id);
        if(sc && sc->client)
        {
            loopv(sockets) if(sockets[i].id == id) { sockets.remove(i); break; }
            httpnet::close(id);
            return true;
        }
        return false;
    }

    int fetchprogress(int id, int &total)
    {
        return httpnet::progress(id, total);
    }

    int wsopen(const char *url, httpwscb fun, void *ctx, const char *pin)
    {
        if(!started) return -1;
        httpfetch f;
        f.url = newstring(url);
        if(pin && *pin) f.pin = newstring(pin);
        f.reqheaders.add("User-Agent", getverstr());
        int id = nextid();
        if(!httpnet::fetch(id, f, true, httpmaxmessage)) return -1;
        socketconn &sc = sockets.add();
        sc.id = id;
        sc.fun = fun;
        sc.ctx = ctx;
        sc.client = true;
        sc.open = false;
        return id;
    }

    bool wssend(int id, const char *data, int len, int opcode)
    {
        socketconn *sc = findsocket(id);
        if(!sc || !sc->open) return false;
        if(len < 0) len = strlen(data);
        vector<char> frame;
        httpnet::wsframe(frame, opcode, data, len, sc->client);
        httpnet::send(id, frame.getbuf(), frame.length());
        return true;
    }

    int wsbroadcast(const vector<int> &ids, const char *data, int len, int opcode)
    {
        if(len < 0) len = strlen(data);
        vector<int> servers;
        loopv(ids)
        {
            socketconn *sc = findsocket(ids[i]);
            if(!sc || !sc->open) continue;
            if(sc->client) wssend(ids[i], data, len, opcode);
            else servers.add(ids[i]);
        }
        if(servers.empty()) return 0;
        vector<char> frame;
        httpnet::wsframe(frame, opcode, data, len, false);
        return httpnet::sendshared(servers, frame.getbuf(), frame.length());
    }

    bool wsclose(int id, int code)
    {
        socketconn *sc = findsocket(id);
        if(!sc) return false;
        loopv(sockets) if(sockets[i].id == id) { sockets.remove(i); break; }
        httpnet::close(id);
        return true;
    }

    bool wsalive(int id)
    {
        return findsocket(id) != NULL;
    }

    int serverport() { return boundport; }
    const char *fingerprint() { return certfingerprint; }

    void loadcert()
    {
        if(!started) return;
        certfingerprint[0] = '\0';
        if(!*httpcertfile) { httpnet::clearservercert(); certtime = keytime = 0; return; }
        struct stat cs, ks;
        const char *keyfile = *httpkeyfile ? httpkeyfile : httpcertfile;
        if(stat(httpcertfile, &cs) || stat(keyfile, &ks)) { conoutf(colourred, "TLS: unable to read %s or %s", httpcertfile, keyfile); return; }
        certtime = cs.st_mtime;
        keytime = ks.st_mtime;
        if(httpnet::servercert(httpcertfile, keyfile, certfingerprint, sizeof(certfingerprint)))
            conoutf(colourwhite, "TLS: serving HTTPS with %s (sha256 %s)", httpcertfile, certfingerprint);
    }

    static void listen()
    {
        if(!started) return;
        if(!httpserver) { if(boundport) { httpnet::unlisten(); boundport = 0; conoutf(colourwhite, "HTTP server stopped"); } return; }
        int range = masterserver ? 0 : httpserverportrange, port = 0;
        if(!httpnet::listen(httpserverip, httpserverport, range, port))
        {
            conoutf(colourred, "Failed to start the HTTP server on %s:%d%s", *httpserverip ? httpserverip : "*", httpserverport, range ? " or the ports after it" : "");
            boundport = 0;
            return;
        }
        boundport = port;
        conoutf(colourwhite, "HTTP server started on %s:[%d]", *httpserverip ? httpserverip : "*", boundport);
    }

    void check()
    {
        if(!started) return;
        listen();
    }

    void init()
    {
        if(started) { check(); return; }
        httpnet::config c;
        getconfig(c);
        if(!httpnet::start(c)) { conoutf(colourred, "Failed to start the network thread for the Web API"); return; }
        started = true;
        if(httpnet::tlsready()) httpnet::cafile(httpcafile);
        loadcert();
        listen();
    }

    void cleanup()
    {
        if(!started) return;
        httpnet::stop();
        started = false;
        boundport = 0;
        deferrals.deletecontents();
        loopv(fetches) if(fetches[i]->callback) { fetches[i]->error = newstring("shutting down"); fetches[i]->callback(*fetches[i]); }
        fetches.deletecontents();
        sockets.setsize(0);
    }
}

