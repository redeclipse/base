// Web API: HTTP/1.1 and WebSockets, over TLS when built with it, on a network thread

#define HTTP_LISTEN 28888
#define HTTP_API "/api/v1" // every route of the Web API is below this, leaving the rest for web pages
#define HTTP_RELAY "/relay" // the master's relay, /relay/<address>/<port> then the path on that server as it is
#define HTTP_PORT 80
#define HTTPS_PORT 443
#define HTTP_PUBLIC 0 // privilege of an anonymous request, mirrors PRIV_NONE as PRIV_PLAYER needs an account
#define HTTP_INMEMORY (1<<20) // larger fetches can stream to a file
#define HTTP_REDIRECTS 5

enum { HTTP_NONE = 0, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS, HTTP_MAXMETHODS };
enum { WS_CONT = 0, WS_TEXT = 1, WS_BINARY = 2, WS_CLOSE = 8, WS_PING = 9, WS_PONG = 10 };
enum { HTTP_REPLY_NET = 0, HTTP_REPLY_RELAY, HTTP_REPLY_SCRIPT };

struct httppair
{
    char *name, *value;

    httppair() : name(NULL), value(NULL) {}
    ~httppair() { DELETEA(name); DELETEA(value); }
};

struct httpheaders
{
    vector<httppair> list;

    ~httpheaders() { clear(); }

    void clear() { list.shrink(0); }
    int length() const { return list.length(); }

    httppair *find(const char *name)
    {
        loopv(list) if(!strcasecmp(list[i].name, name)) return &list[i];
        return NULL;
    }

    const char *get(const char *name, const char *def = NULL)
    {
        httppair *p = find(name);
        return p ? p->value : def;
    }

    void add(const char *name, const char *value, int namelen = -1, int valuelen = -1)
    {
        httppair &p = list.add();
        p.name = namelen >= 0 ? newstring(name, namelen) : newstring(name);
        p.value = valuelen >= 0 ? newstring(value, valuelen) : newstring(value);
    }

    void set(const char *name, const char *value)
    {
        httppair *p = find(name);
        if(!p) { add(name, value); return; }
        DELETEA(p->value);
        p->value = newstring(value);
    }

    void remove(const char *name)
    {
        loopvrev(list) if(!strcasecmp(list[i].name, name)) list.remove(i);
    }

    void copy(httpheaders &h)
    {
        loopv(h.list) add(h.list[i].name, h.list[i].value);
    }

    bool hastoken(const char *name, const char *token); // comma separated header values, case-insensitive
};

// one HTTP message as the network thread parsed it
struct httpmessage
{
    int method, status, version; // version: 10 or 11
    char *target, *reason, *file;
    httpheaders headers;
    vector<char> body;

    httpmessage() : method(HTTP_NONE), status(0), version(11), target(NULL), reason(NULL), file(NULL) {}
    ~httpmessage() { DELETEA(target); DELETEA(reason); DELETEA(file); }
};

struct httpurl
{
    bool tls, ws;
    int port;
    string host;
    char *path; // path and query

    httpurl() : tls(false), ws(false), port(0), path(NULL) { host[0] = '\0'; }
    ~httpurl() { DELETEA(path); }

    bool parse(const char *url);
};

// a request to this server's Web API, directly or through the master's relay
struct httprequest
{
    int id, method, reply, privilege, version;
    uint ip, peer;
    bool secure, local, keepalive, upgrade, gzip, throttled, privlocal, badtoken; // privlocal: the privilege is this server's own, not the master's; badtoken: unknown or expired
    char *path, *query, *token, *handle, *origin;
    const char *route; // the pattern that matched
    string addr;
    httpheaders headers, params, vars; // params from the route, vars from the query or a form body
    vector<char> body;
    void *relay;

    httprequest() : id(-1), method(HTTP_NONE), reply(HTTP_REPLY_NET), privilege(0), version(11), ip(0), peer(0), secure(false), local(false), keepalive(false),
        upgrade(false), gzip(false), throttled(false), privlocal(false), badtoken(false), path(NULL), query(NULL), token(NULL), handle(NULL), origin(NULL), route(NULL), relay(NULL) { addr[0] = '\0'; }
    ~httprequest() { DELETEA(path); DELETEA(query); DELETEA(token); DELETEA(handle); DELETEA(origin); }

    const char *param(const char *name, const char *def = NULL) { return params.get(name, def); }
    const char *var(const char *name, const char *def = NULL) { return vars.get(name, def); }
};

struct httpresponse
{
    int status;
    bool deferred, close;
    httpheaders headers;
    vector<char> body;

    httpresponse(int status = 200) : status(status), deferred(false), close(false) {}

    void settype(const char *type) { headers.set("Content-Type", type); }
    void text(const char *s, int len = -1) { settype("text/plain; charset=utf-8"); body.setsize(0); body.put(s, len >= 0 ? len : strlen(s)); }
    void json(const char *s, int len = -1) { settype("application/json"); body.setsize(0); body.put(s, len >= 0 ? len : strlen(s)); }
    void error(int code, const char *msg = NULL);
    void defer() { deferred = true; }
};

// an outgoing request, as handed back to its callback on the main thread
struct httpfetch;
typedef void (__cdecl *httpfetchcb)(httpfetch &f);
struct httpfetch
{
    int id, method, status, maxsize;
    bool cancelled;
    char *url, *error, *file, *pin;
    httpheaders headers, reqheaders;
    vector<char> body, reqbody;
    httpfetchcb callback;
    void *ctx;

    httpfetch() : id(-1), method(HTTP_GET), status(0), maxsize(HTTP_INMEMORY), cancelled(false), url(NULL), error(NULL), file(NULL), pin(NULL), callback(NULL), ctx(NULL) {}
    ~httpfetch() { DELETEA(url); DELETEA(error); DELETEA(file); DELETEA(pin); }
};

// WebSocket events, for both connections this side opened and ones accepted by a socket route
enum { WSEV_OPEN = 0, WSEV_MESSAGE, WSEV_CLOSE };
struct httpwsevent
{
    int type, id, opcode, code;
    const char *data;
    int len;
    httprequest *req; // only for WSEV_OPEN on a socket route
};
typedef void (__cdecl *httpwscb)(httpwsevent &e, void *ctx);

typedef void (__cdecl *httproutefun)(httprequest &req, httpresponse &res);
struct httproute
{
    int method, minpriv;
    const char *pattern;
    httproutefun fun;
    httpwscb ws; // a socket route, upgraded instead of answered
    void *ctx;
    int maxmessage;
    bool script;
};

namespace http
{
    extern const char *methodnames[HTTP_MAXMETHODS];
    extern int findmethod(const char *name, int len = -1);
    extern const char *statusname(int code);
    extern char *urldecode(char *dst, const char *str, size_t len);
    extern void urlencode(vector<char> &buf, const char *str);
    extern int vardecode(const char *str, httpheaders &vars);
    extern void base64(vector<char> &buf, const uchar *data, int len);
    extern void sha1(const uchar *data, int len, uchar digest[20]);
    extern void randombytes(uchar *buf, int len);

    // routes; a socket route answers an upgrade request with a WebSocket
    extern bool addroute(int method, const char *pattern, int minpriv, httproutefun fun);
    extern bool addsocket(const char *pattern, int minpriv, httpwscb fun, void *ctx = NULL, int maxmessage = 0);
    extern bool removeroute(int method, const char *pattern);
    extern const vector<httproute> &routelist();
    extern void dispatch(httprequest &req);
    extern void respond(httprequest &req, httpresponse &res);
    extern int deferred(httprequest &req);
    extern bool reply(int id, httpresponse &res);
    extern void (*relayreply)(void *relay, httpresponse &res); // set by a game server to answer requests the master relayed

    // outgoing requests
    extern httpfetch *newfetch(int method, const char *url, httpfetchcb callback, void *ctx = NULL);
    extern int submit(httpfetch *f);
    extern bool cancel(int id);
    extern int fetchprogress(int id, int &total);

    // WebSockets, by connection id from either side
    extern int wsopen(const char *url, httpwscb fun, void *ctx = NULL, const char *pin = NULL);
    extern bool wssend(int id, const char *data, int len = -1, int opcode = WS_TEXT);
    extern int wsbroadcast(const vector<int> &ids, const char *data, int len = -1, int opcode = WS_TEXT);
    extern bool wsclose(int id, int code = 1000);
    extern bool wsalive(int id);

    extern void init();
    extern void cleanup();
    extern void runframe();
    extern int serverport();
    extern const char *fingerprint();
    extern bool trustedproxy(uint ip);
    extern bool alloworigin(const char *origin);
    extern bool securereq(const httprequest &req); // https, localhost, a trusted proxy's https or httpallowinsecure
}

// the network thread's side, engine/httpnet.cpp
namespace httpnet
{
    struct config
    {
        int maxconns, maxperaddr, maxheaders, maxline, maxbody, maxmessage, idletime, pingtime, handshakethreads, handshakerate, verbose;
    };

    enum { EV_REQUEST = 0, EV_CLOSED, EV_FETCHED, EV_WSOPEN, EV_WSMESSAGE, EV_WSCLOSE, EV_LOG };
    struct event
    {
        int type, id, code, opcode;
        uint ip;
        bool secure;
        httpmessage *msg;
        vector<char> data;
        char *error;

        event() : type(-1), id(-1), code(0), opcode(0), ip(0), secure(false), msg(NULL), error(NULL) {}
        ~event() { DELETEP(msg); DELETEA(error); }
    };

    enum { SEND_DONE = 1<<0, SEND_CLOSE = 1<<1, SEND_UPGRADE = 1<<2 };

    extern bool start(config &c);
    extern void stop();
    extern void setconfig(config &c);
    extern void getevents(vector<event *> &evs);
    extern bool listen(const char *ip, int port, int range, int &bound);
    extern void unlisten();
    extern bool tlsready();
    extern bool servercert(const char *certfile, const char *keyfile, char *fingerprint, int fplen);
    extern void clearservercert();
    extern bool cafile(const char *file);
    extern void send(int id, const char *data, int len, int flags = 0, int maxmessage = 0);
    extern int sendshared(const vector<int> &ids, const char *data, int len);
    extern void close(int id, int code = 1000);
    extern bool fetch(int id, httpfetch &f, bool ws, int maxsize);
    extern int progress(int id, int &total);
    extern void wsframe(vector<char> &buf, int opcode, const char *data, int len, bool mask);
}
