// HTTP server

#define HTTP_LIMIT  4096
#define HTTP_TIME   15000
#define HTTP_PORT   28888
#define HTTP_HDRS   64

enum
{
    HTTP_R_NONE = 0,
    HTTP_R_OK = 200,
    HTTP_R_NOTFOUND = 404
};

enum
{
    HTTP_S_START = 0,
    HTTP_S_HEADERS,
    HTTP_S_DATA,
    HTTP_S_RESPONSE,
    HTTP_S_PURGE,
    HTTP_S_MAX
};

enum
{
    HTTP_T_ERROR = 0,
    HTTP_T_GET,
    HTTP_T_POST,
    HTTP_T_PUT,
    HTTP_T_DELETE,
    HTTP_T_MAX
};

enum
{
    HTTP_C_NONE = 0,
    HTTP_C_URLENC,
    HTTP_C_JSON,
    HTTP_C_DATA,
    HTTP_C_MAX
};

struct httppair
{
    string name, value;

    httppair()
    {
        name[0] = value[0] = 0;
    }
    ~httppair() {}
};

struct httpvars
{
    vector<httppair> values;

    httpvars() { reset(); }
    ~httpvars() {}

    void reset()
    {
        values.shrink(0);
    }

    httppair &add()
    {
        return values.add();
    }

    httppair *find(const char *name)
    {
        loopv(values) if(!strcasecmp(name, values[i].name)) return &values[i];
        return NULL;
    }

    const char *get(const char *name)
    {
        httppair *v = find(name);
        if(v) return v->value;
        return NULL;
    }

    int length()
    {
        return values.length();
    }
};

struct httpreq
{
    ENetAddress address;
    ENetSocket socket;
    int state, reqtype, contype;
    string name;
    bigstring path, input;
    vector<char> output;
    int inputpos, outputpos, conlength;
    enet_uint32 lastactivity;
    httpvars headers, vars;

    httpreq() { reset(); }
    ~httpreq() {}

    void reset()
    {
        state = HTTP_S_START;
        reqtype = HTTP_T_ERROR;
        contype = HTTP_C_NONE;
        name[0] = path[0] = 0;
        inputpos = outputpos = conlength = 0;
        lastactivity = 0;
        output.shrink(0);
        headers.reset();
        vars.reset();
    }

    void raw(const char c)
    {
        output.add(c);
    }

    void send(const char *msg, int len = 0)
    {
        if(!len) len = strlen(msg);
        output.put(msg, len);
        output.put("\r\n", 2);
    }

    void sendf(const char *fmt, ...)
    {
        bigstring msg;
        va_list args;
        va_start(args, fmt);
        vformatstring(msg, fmt, args);
        va_end(args);
        send(msg);
    }
};

struct httpcmd;
typedef void (__cdecl *httpfun)(httpreq *h);
struct httpcmd
{
    const char *name;
    httpfun fun;

    httpcmd() {}
    httpcmd(const char *n, void *f = NULL) : name(n), fun((httpfun)f) {}
};

#define HTTP(name, fun) UNUSED static bool __http_##fun = http::addcommand(name, (httpfun)fun)

#define JSON_TOKENS 1024

enum
{
    JSON_INIT = 0,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_PRIMITIVE,
    JSON_MAX
};

struct jsobj
{
    string name, data;
    int type;
    vector<jsobj *> children;

    jsobj() { reset(); }
    ~jsobj() { clear(); }

    void clear()
    {
        children.deletecontents();
    }

    void reset()
    {
        name[0] = data[0] = 0;
        type = JSON_INIT;
        children.shrink(0);
    }
};

namespace http
{
    extern vector<httpcmd> cmds;
    extern bool addcommand(const char *name, httpfun fun);
    extern void cleanup();
    extern void init();
    extern void runframe();
}

namespace json
{
    extern int print(jsobj *j, char *dst, int level, size_t len);
    extern jsobj *load(const char *str);
}
