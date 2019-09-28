// HTTP server and client

#include "engine.h"
#include <enet/time.h>
#include "jsmn.h"

namespace http
{
    char *urldecode(char *dst, const char *str, size_t len)
    {
        size_t n = 0;
        for(const char *p = str; *p;)
        {
            if(*p == '%')
            {
                p++;
                if(*p == '%') dst[n++] = *p;
                else if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
                {
                    char val[3];
                    val[0] = *p;
                    p++;
                    if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
                    {
                        val[1] = *p;
                        val[2] = 0;
                        dst[n++] = (char)strtol(val, NULL, 16);
                    }
                    else // permissive
                    {
                        val[1] = val[0];
                        val[0] = '0';
                        val[2] = 0;
                        dst[n++] = (char)strtol(val, NULL, 16);
                    }
                }
            }
            else dst[n++] = *p == '+' ? ' ' : *p;
            if(n >= len) break;
            if(*p) p++;
        }
        dst[n] = 0;
        return dst;
    }

    int vardecode(const char *str, httpvars &vars)
    {
        string data = "", decoded = "";
        int count = 0;
        for(const char *start = str, *p = str; *p; start = p)
        {
            p += strcspn(p, "=&\0");
            copystring(data, start, p-start+1);
            urldecode(decoded, data, sizeof(decoded));
            httppair *v = vars.find(decoded);
            if(!v)
            {
                v = &vars.add();
                copystring(v->name, decoded);
            }
            count++;
            if(*p == '=')
            {
                p++;
                start = p;
                p += strcspn(p, "&\0");
                copystring(data, start, p-start+1);
                urldecode(decoded, data, sizeof(decoded));
                copystring(v->value, decoded);
            }
            if(*p) p++;
        }
        return count;
    }

    vector<httpcmd> cmds;
    vector<httpreq *> reqs;
    ENetSocket socket = ENET_SOCKET_NULL;
    time_t starttime = 0;

    void check();
    #ifdef STANDALONE
    VARF(0, httpserver, 0, 1, 1, check());
    #else
    VARF(0, httpserver, 0, 0, 1, check());
    #endif
    VAR(0, httpserverport, 1, HTTP_LISTEN, VAR_MAX);
    SVAR(0, httpserverip, "");

    httpcmd *findcommand(const char *name)
    {
        loopv(cmds) if(!strcmp(cmds[i].name, name)) return &cmds[i];
        return NULL;
    }

    bool addcommand(const char *name, httpfun fun)
    {
        if(findcommand(name)) return false;
        cmds.add(httpcmd(name, (void *)fun));
        return true;
    }

    void proceed(httpreq *r)
    {
        httpcmd *c = findcommand(r->path);
        if(c)
        {
            if(r->inputpos < 0 || r->inputpos > (int)sizeof(r->input)) r->inputpos = 0;
            r->input[r->inputpos] = '\0';
            r->send("HTTP/1.0 200 OK");
            ((httpfun)c->fun)(r);
        }
        else r->send("HTTP/1.0 404 Not Found");
        r->state = HTTP_S_DONE;
    }

    bool processreqdata(httpreq *r)
    {
        if(r->state != HTTP_S_DATA || r->contype == HTTP_C_URLENC) return false;
        if(r->inputpos >= r->conlength) r->state = HTTP_S_RESPONSE;
        return true;
    }

    static const char *httpreqs[HTTP_T_MAX] = {
        "ERROR",
        "GET",
        "POST",
        "PUT",
        "DELETE"
    };

    bool processreq(httpreq *r)
    {
        if(r->inputpos < 0) return false;
        if(processreqdata(r)) return true;
        char *p = r->input;
        for(char *end;; p = end)
        {
            if(r->state == HTTP_S_RESPONSE || (r->state == HTTP_S_DATA && r->contype != HTTP_C_URLENC)) break;
            bool check = true;
            end = (char *)memchr(p, '\r', &r->input[r->inputpos]-p);
            if(!end)
            {
                check = false;
                end = (char *)memchr(p, '\n', &r->input[r->inputpos]-p);
                if(!end) end = (char *)memchr(p, '\0', &r->input[r->inputpos]-p);
            }
            if(!end) break;
            *end++ = '\0';
            if(check && *end == '\n') *end++ = '\0';
            switch(r->state)
            {
                case HTTP_S_START:
                {
                    loopi(HTTP_T_MAX)
                    {
                        int t = strlen(httpreqs[i]);
                        if(!strncmp(p, httpreqs[i], t))
                        {
                            p += t;
                            while(*p == ' ') p++;
                            r->reqtype = i;
                            break;
                        }
                    }
                    if(r->reqtype != HTTP_T_ERROR)
                    {
                        const char *start = p;
                        p += strcspn(p, " \0");
                        bigstring path = "";
                        copystring(path, start, p-start+1);
                        const char *q = path, *beg = q;
                        q += strcspn(q, "?\0");
                        if(*q == '?')
                        {
                            bigstring data = "";
                            copystring(data, beg, q-beg+1);
                            urldecode(r->path, data, sizeof(r->path));
                            q++;
                            vardecode(q, r->vars);
                        }
                        else urldecode(r->path, path, sizeof(r->path));
                        if(r->path[0])
                        {
                            r->state = HTTP_S_HEADERS;
                            break;
                        }
                    }
                    r->send("HTTP/1.0 400 Bad Request");
                    r->state = HTTP_S_DONE; // failed
                    break;
                }
                case HTTP_S_HEADERS:
                {
                    if(!strlen(p))
                    {
                        if(r->reqtype != HTTP_T_GET) // wants data
                        {
                            const char *cont = r->inhdrs.get("Content-Type");
                            if(cont && *cont)
                            {
                                if(!strcasecmp(cont, "application/x-www-form-urlencoded")) r->contype = HTTP_C_URLENC;
                                else if(!strcasecmp(cont, "application/json")) r->contype = HTTP_C_JSON;
                                else r->contype = HTTP_C_DATA;
                                cont = r->inhdrs.get("Content-Length");
                                if(cont && *cont) r->conlength = atoi(cont);
                            }
                            r->state = HTTP_S_DATA;
                            break;
                        }
                        r->state = HTTP_S_RESPONSE;
                        break;
                    }
                    if(r->inhdrs.length() >= HTTP_HDRS) break;
                    const char *start = p;
                    p += strcspn(p, ":\0");
                    string data = "";
                    copystring(data, start, p-start+1);
                    httppair *v = r->inhdrs.find(data);
                    if(!v)
                    {
                        v = &r->inhdrs.add();
                        copystring(v->name, start, p-start+1);
                    }
                    if(*p == ':') p++;
                    while(*p == ' ') p++;
                    copystring(v->value, p);
                    break;
                }
                case HTTP_S_DATA:
                {
                    vardecode(p, r->vars);
                    r->state = HTTP_S_RESPONSE;
                    break;
                }
                default:
                {
                    r->send("HTTP/1.0 400 Bad Request");
                    r->state = HTTP_S_DONE; // failed
                    break;
                }
            }
        }
        r->inputpos = &r->input[r->inputpos]-p;
        memmove(r->input, p, r->inputpos);
        if(r->inputpos < (int)sizeof(r->input))
        {
            processreqdata(r);
            return true;
        }
        return false;
    }

    void purgereq(int n)
    {
        if(n < 0 || n >= reqs.length()) return;
        httpreq *r = reqs[n];
        enet_socket_destroy(r->socket);
        conoutf("HTTP request %s disconnected", r->name);
        delete reqs[n];
        reqs.remove(n);
    }

    vector<httpclient *> clients;

    bool processclientdata(httpclient *c)
    {
        if(c->state != HTTP_S_DATA) return false;
        if(c->inputpos >= c->conlength) c->state = HTTP_S_DONE;
        return true;
    }

    bool processclient(httpclient *c)
    {
        if(c->inputpos < 0) return false;
        if(processclientdata(c)) return true;
        char *p = c->input;
        for(char *end;; p = end)
        {
            if(c->state == HTTP_S_DATA) break;
            bool check = true;
            end = (char *)memchr(p, '\r', &c->input[c->inputpos]-p);
            if(!end)
            {
                check = false;
                end = (char *)memchr(p, '\n', &c->input[c->inputpos]-p);
                if(!end) end = (char *)memchr(p, '\0', &c->input[c->inputpos]-p);
            }
            if(!end) break;
            *end++ = '\0';
            if(check && *end == '\n') *end++ = '\0';
            switch(c->state)
            {
                case HTTP_S_START:
                {
                    p += strcspn(p, " \0");
                    if(!*p) return false;
                    const char *start = ++p;
                    p += strcspn(p, " \0");
                    string retcode = "";
                    copystring(retcode, start, p-start+1);
                    int retnum = atoi(retcode);
                    conoutf("HTTP retcode %s [%d]: %d", c->name, c->uid, retnum);
                    if(retnum != HTTP_R_OK) return false;
                    c->state = HTTP_S_HEADERS;
                    break;
                }
                case HTTP_S_HEADERS:
                {
                    if(!strlen(p))
                    {
                        const char *cont = c->inhdrs.get("Content-Type");
                        if(!cont && !*cont) return false;
                        if(!strcasecmp(cont, "application/x-www-form-urlencoded")) c->contype = HTTP_C_URLENC;
                        else if(!strcasecmp(cont, "application/json")) c->contype = HTTP_C_JSON;
                        else c->contype = HTTP_C_DATA;
                        cont = c->inhdrs.get("Content-Length");
                        if(cont && *cont) c->conlength = atoi(cont);
                        c->state = HTTP_S_DATA;
                        break;
                    }
                    if(c->inhdrs.length() >= HTTP_HDRS) break;
                    const char *start = p;
                    p += strcspn(p, ":\0");
                    string data = "";
                    copystring(data, start, p-start+1);
                    httppair *v = c->inhdrs.find(data);
                    if(!v)
                    {
                        v = &c->inhdrs.add();
                        copystring(v->name, start, p-start+1);
                    }
                    if(*p == ':') p++;
                    while(*p == ' ') p++;
                    copystring(v->value, p);
                    break;
                }
                default: return false;
            }
        }
        c->inputpos = &c->input[c->inputpos]-p;
        memmove(c->input, p, c->inputpos);
        if(c->inputpos < (int)sizeof(c->input))
        {
            processclientdata(c);
            return true;
        }
        return false;
    }

    void purgeclient(int n, int state = -1)
    {
        if(n < 0 || n >= clients.length()) return;
        httpclient *c = clients[n];
        if(state >= 0) c->state = state;
        if(c->inputpos < 0 || c->inputpos > (int)sizeof(c->input)) c->inputpos = 0;
        c->input[c->inputpos] = '\0';
        if(c->callback) c->callback(c);
        enet_socket_destroy(c->socket);
        conoutf("HTTP peer %s [%d] disconnected (%d)", c->name, c->uid, c->state);
        delete clients[n];
        clients.remove(n);
    }

    static int retrieveid = 0;
    httpclient *retrieve(const char *serv, int port, int type, const char *path, httpcb callback, const char *data, int uid)
    {
        if(!serv || !*serv || !port || type <= HTTP_T_ERROR || type >= HTTP_T_MAX || !path || !*path || !callback) return NULL;
        httpclient *c = new httpclient;
        c->lastactivity = totalmillis ? totalmillis : 1;
        copystring(c->name, serv);
        c->port = c->address.port = port;
        if(uid >= 0) c->uid = uid;
        else
        {
            c->uid = retrieveid++;
            if(retrieveid < 0) retrieveid = 0;
        }
        c->reqtype = type;
        copystring(c->path, path);
        if(data) copystring(c->data, data);
        c->callback = callback;
        conoutf("HTTP looking up %s:[%d] [%d]...", c->name, c->port, c->uid);
        if(!resolverwait(c->name, &c->address)) // TODO: don't block here
        {
            conoutf("HTTP unable to resolve %s:[%d] [%d]...", c->name, c->port, c->uid);
            delete c;
            return NULL;
        }
        c->socket = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
        if(c->socket != ENET_SOCKET_NULL) enet_socket_set_option(c->socket, ENET_SOCKOPT_NONBLOCK, 1);
        if(c->socket == ENET_SOCKET_NULL || enet_socket_connect(c->socket, &c->address) < 0)
        {
            conoutf(c->socket == ENET_SOCKET_NULL ? "HTTP could not open socket to %s:[%d] [%d]" : "HTTP could not connect to %s:[%d] [%d]", c->name, c->port, c->uid);
            if(c->socket != ENET_SOCKET_NULL) enet_socket_destroy(c->socket);
            delete c;
            return NULL;
        }
        c->state = HTTP_S_CONNECTING;
        c->outhdrs.add("User-Agent", getverstr());
        clients.add(c);
        conoutf("HTTP connecting to %s:[%d] [%d]...", c->name, c->port, c->uid);
        return c;
    }

    void cleanup()
    {
        loopv(reqs) purgereq(i--);
        loopv(clients) purgeclient(i--, HTTP_S_FAILED);
        if(socket != ENET_SOCKET_NULL) enet_socket_destroy(socket);
        socket = ENET_SOCKET_NULL;
    }

    void check()
    {
        if(httpserver) init();
        else cleanup();
    }

    void init()
    {
        if(!httpserver || socket != ENET_SOCKET_NULL) return;
        conoutf("Loading HTTP (%s:%d)..", *httpserverip ? httpserverip : "*", httpserverport);
        ENetAddress address = { ENET_HOST_ANY, enet_uint16(httpserverport) };
        if(*httpserverip && enet_address_set_host(&address, httpserverip) < 0) { conoutf("Failed to resolve HTTP address: %s", httpserverip); cleanup(); return; }
        if((socket = enet_socket_create(ENET_SOCKET_TYPE_STREAM)) == ENET_SOCKET_NULL) { conoutf("Failed to create HTTP server socket"); cleanup(); return; }
        if(enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1) < 0) { conoutf("Failed to set HTTP server socket option"); cleanup(); return; }
        if(enet_socket_bind(socket, &address) < 0) { conoutf("Failed to bind HTTP server socket"); cleanup(); return; }
        if(enet_socket_listen(socket, -1) < 0) { conoutf("Failed to listen on HTTP server socket"); cleanup(); return; }
        if(enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1) < 0) { conoutf("Failed to make HTTP server socket non-blocking"); cleanup(); return; }
        starttime = totalmillis ? totalmillis : 1;
        conoutf("HTTP server started on %s:[%d]", *httpserverip ? httpserverip : "localhost", httpserverport);
    }

    void checkserver(ENetSocket &maxsock, ENetSocketSet &readset, ENetSocketSet &writeset)
    {
        if(socket == ENET_SOCKET_NULL) return;
        ENET_SOCKETSET_ADD(readset, socket);
        loopv(reqs)
        {
            httpreq *r = reqs[i];
            if(r->outputpos < r->output.length()) ENET_SOCKETSET_ADD(writeset, r->socket);
            else ENET_SOCKETSET_ADD(readset, r->socket);
            maxsock = max(maxsock, r->socket);
        }
    }

    void parseserver(ENetSocketSet &readset, ENetSocketSet &writeset)
    {
        if(socket == ENET_SOCKET_NULL) return;
        if(ENET_SOCKETSET_CHECK(readset, socket)) for(;;)
        {
            ENetAddress address;
            ENetSocket reqsocket = enet_socket_accept(socket, &address);
            if(reqs.length() >= HTTP_LIMIT || (checkipinfo(control, ipinfo::BAN, address.host) && !checkipinfo(control, ipinfo::EXCEPT, address.host) && !checkipinfo(control, ipinfo::TRUST, address.host)))
            {
                enet_socket_destroy(reqsocket);
                break;
            }
            if(reqsocket != ENET_SOCKET_NULL)
            {
                httpreq *c = new httpreq;
                c->address = address;
                c->socket = reqsocket;
                c->lastactivity = totalmillis ? totalmillis : 1;
                reqs.add(c);
                if(enet_address_get_host_ip(&c->address, c->name, sizeof(c->name)) < 0) copystring(c->name, "unknown");
                conoutf("HTTP request %s connected", c->name);
            }
            break;
        }
        loopv(reqs)
        {
            httpreq *r = reqs[i];
            if(r->outputpos < r->output.length() && ENET_SOCKETSET_CHECK(writeset, r->socket))
            {
                ENetBuffer buf;
                buf.data = (void *)&r->output[r->outputpos];
                buf.dataLength = r->output.length()-r->outputpos;
                int res = enet_socket_send(r->socket, NULL, &buf, 1);
                if(res >= 0)
                {
                    r->outputpos += res;
                    if(r->outputpos >= r->output.length())
                    {
                        r->output.setsize(0);
                        r->outputpos = 0;
                        if(r->state == HTTP_S_DONE) { purgereq(i--); continue; }
                    }
                }
                else { purgereq(i--); continue; }
            }
            if(ENET_SOCKETSET_CHECK(readset, r->socket))
            {
                ENetBuffer buf;
                buf.data = &r->input[r->inputpos];
                buf.dataLength = sizeof(r->input)-r->inputpos;
                int res = enet_socket_receive(r->socket, NULL, &buf, 1);
                if(res > 0)
                {
                    r->inputpos += res;
                    r->input[min(r->inputpos, (int)sizeof(r->input)-1)] = '\0';
                    if(!processreq(r)) { purgereq(i--); continue; }
                    if(r->state == HTTP_S_RESPONSE) proceed(r);
                }
                else { purgereq(i--); continue; }
            }
            if(ENET_TIME_DIFFERENCE(totalmillis, r->lastactivity) >= HTTP_TIME || (checkipinfo(control, ipinfo::BAN, r->address.host) && !checkipinfo(control, ipinfo::EXCEPT, r->address.host) && !checkipinfo(control, ipinfo::TRUST, r->address.host)))
            {
                purgereq(i--);
                continue;
            }
        }
    }

    void checkclient(ENetSocket &maxsock, ENetSocketSet &readset, ENetSocketSet &writeset)
    {
        loopv(clients)
        {
            httpclient *c = clients[i];
            if(c->state == HTTP_S_CONNECTING || c->outputpos < c->output.length()) ENET_SOCKETSET_ADD(writeset, c->socket);
            else ENET_SOCKETSET_ADD(readset, c->socket);
            maxsock = max(maxsock, c->socket);
        }
    }

    void parseclient(ENetSocketSet &readset, ENetSocketSet &writeset)
    {
        loopv(clients)
        {
            httpclient *c = clients[i];
            if(c->state == HTTP_S_CONNECTING && (ENET_SOCKETSET_CHECK(readset, c->socket) || ENET_SOCKETSET_CHECK(writeset, c->socket)))
            {
                int error = 0;
                if(enet_socket_get_option(c->socket, ENET_SOCKOPT_ERROR, &error) < 0 || error) { purgeclient(i--, HTTP_S_FAILED); continue; }
                else
                {
                    c->state = HTTP_S_START;
                    c->sendf("%s %s HTTP/1.0", httpreqs[c->reqtype], c->path);
                    loopvj(c->outhdrs.values) c->sendf("%s: %s", c->outhdrs.values[j].name, c->outhdrs.values[j].value);
                    c->send("");
                    if(c->data[0]) c->send(c->data);
                    conoutf("HTTP %s %s [%d]: %s", httpreqs[c->reqtype], c->name, c->uid, c->path);
                }
            }
            if(c->outputpos < c->output.length() && ENET_SOCKETSET_CHECK(writeset, c->socket))
            {
                ENetBuffer buf;
                buf.data = (void *)&c->output[c->outputpos];
                buf.dataLength = c->output.length()-c->outputpos;
                int res = enet_socket_send(c->socket, NULL, &buf, 1);
                if(res >= 0)
                {
                    c->outputpos += res;
                    if(c->outputpos >= c->output.length())
                    {
                        c->output.setsize(0);
                        c->outputpos = 0;
                    }
                }
                else { purgeclient(i--, HTTP_S_FAILED); continue; }
            }
            if(ENET_SOCKETSET_CHECK(readset, c->socket))
            {
                ENetBuffer buf;
                buf.data = &c->input[c->inputpos];
                buf.dataLength = sizeof(c->input)-c->inputpos;
                int res = enet_socket_receive(c->socket, NULL, &buf, 1);
                if(res > 0)
                {
                    c->inputpos += res;
                    c->input[min(c->inputpos, (int)sizeof(c->input)-1)] = '\0';
                    if(!processclient(c)) { purgeclient(i--, HTTP_S_FAILED); continue; }
                }
                else { purgeclient(i--, HTTP_S_FAILED); continue; }
            }
            if(c->state == HTTP_S_DONE)
            {
                purgeclient(i--);
                continue;
            }
            if(ENET_TIME_DIFFERENCE(totalmillis, c->lastactivity) >= HTTP_TIME)
            {
                purgeclient(i--, HTTP_S_FAILED);
                continue;
            }
        }
    }

    void runframe()
    {
        ENetSocketSet readset, writeset;
        ENetSocket maxsock = socket;
        ENET_SOCKETSET_EMPTY(readset);
        ENET_SOCKETSET_EMPTY(writeset);

        checkserver(maxsock, readset, writeset);
        checkclient(maxsock, readset, writeset);

        if(enet_socketset_select(maxsock, &readset, &writeset, 0) <= 0) return;

        parseserver(readset, writeset);
        parseclient(readset, writeset);
    }
}

namespace json
{
    int print(jsobj *j, char *dst, int level, size_t len)
    {
        if(!j) return 0;
        bigstring data = "";
        int count = 0;
        switch(j->type)
        {
            case JSON_STRING:
            {
                if(j->data[0]) concformatstring(data, "\"%s\": \"%s\"", j->name, j->data);
                else concformatstring(data, "\"%s\"", j->name);
                count++;
                break;
            }
            case JSON_PRIMITIVE:
            {
                if(j->data[0]) concformatstring(data, "\"%s\": %s", j->name, j->data);
                else concformatstring(data, "%s", j->name);
                count++;
                break;
            }
            case JSON_OBJECT: case JSON_ARRAY:
            {
                if(j->name[0]) concformatstring(data, "\"%s\": %c ", j->name, int(j->type != JSON_OBJECT ? '[' : '{'));
                else concformatstring(data, "%c ", int(j->type != JSON_OBJECT ? '[' : '{'));
                loopv(j->children)
                {
                    count += print(j->children[i], data, level+1, len);
                    if(i < j->children.length()-1) concatstring(data, ", ");
                }
                concformatstring(data, " %c", int(j->type != JSON_OBJECT ? ']' : '}'));
                count++;
            }
            default: break;
        }
        concatstring(dst, data, len);
        return count;
    }

    int process(const char *str, jsobj *j, jsmntok_t *t, int r, int start, int objects = -1)
    {
        int count = 0;
        string name = "", data = "";
        for(int i = start, objs = 0; i < r; i++)
        {
            bool hasname = name[0] != 0;
            switch(t[i].type)
            {
                case JSMN_STRING: case JSMN_PRIMITIVE:
                {
                    copystring(hasname ? data : name, &str[t[i].start], t[i].end-t[i].start+1);
                    if(j->type != JSON_ARRAY && !hasname) break;
                    jsobj *k = new jsobj;
                    k->type = t[i].type != JSMN_PRIMITIVE ? JSON_STRING : JSON_PRIMITIVE;
                    copystring(k->name, name);
                    if(hasname) copystring(k->data, data);
                    name[0] = data[0] = 0;
                    objs++;
                    j->children.add(k);
                    break;
                }
                case JSMN_ARRAY: case JSMN_OBJECT:
                {
                    jsobj *k = new jsobj;
                    k->type = t[i].type != JSMN_OBJECT ? JSON_ARRAY : JSON_OBJECT;
                    copystring(k->name, name);
                    int a = process(str, k, t, r, i+1, t[i].size);
                    i += a;
                    count += a;
                    name[0] = data[0] = 0;
                    objs++;
                    j->children.add(k);
                    break;
                }
                default: break;
            }
            count++;
            if(objects >= 0 && objs >= objects) break;
        }
        return count;
    }

    jsobj *load(const char *str)
    {
        jsmn_parser p;
        jsmntok_t t[JSON_TOKENS];
        jsmn_init(&p);
        int r = jsmn_parse(&p, str, strlen(str), t, sizeof(t)/sizeof(t[0]));
        if(r < 0)
        {
            conoutf("JSON: Failed to parse (%d)", r);
            return NULL;
        }
        if(r < 1 || t[0].type != JSMN_OBJECT)
        {
            conoutf("JSON: Object expected");
            return NULL;
        }
        jsobj *j = new jsobj;
        j->type = JSON_OBJECT;
        if(!process(str, j, &t[0], r, 1))
        {
            delete j;
            return NULL;
        }
        return j;
    }
}

void httpindex(httpreq *r)
{
    r->send("Content-Type: text/plain");
    r->send("\r\n");
    defformatstring(branch, "%s", versionbranch);
    r->sendf("%s", getverstr());
    r->send("");
    r->send("Headers:");
    loopv(r->inhdrs.values) r->sendf("- %s = %s", r->inhdrs.values[i].name, r->inhdrs.values[i].value);
    r->send("");
    r->send("Vars:");
    loopv(r->vars.values) r->sendf("- %s = %s", r->vars.values[i].name, r->vars.values[i].value);
    r->send("");
    switch(r->contype)
    {
        case HTTP_C_DATA: r->sendf("Generic Data: [%d] %s", r->inputpos, r->input); break;
        case HTTP_C_JSON:
        {
            jsobj *j = json::load(r->input);
            r->sendf("JSON Data: [%d] (%d)", r->inputpos, j ? j->children.length() : 0);
            if(!j) break;
            r->send("-");
            r->send(r->input);
            r->send("-");
            bigstring data = "";
            int count = json::print(j, data, 0, sizeof(data));
            r->sendf("[%d] %s", count, data);
            delete j;
            break;
        }
    }
}
HTTP("/", httpindex);

void testcb(httpclient *c)
{
    conoutf("HTTP callback %s [%d]: %d", c->name, c->uid, c->state);
    switch(c->state)
    {
        case HTTP_S_DONE:
        {
            bigstring data = "";
            filterstring(data, c->input, true, false);
            conoutf("[%d:%d] %s", c->inputpos, c->conlength, data);
            if(c->contype == HTTP_C_JSON)
            {
                jsobj *j = json::load(c->input);
                if(!j) return;
                bigstring data = "";
                int count = json::print(j, data, 0, sizeof(data));
                conoutf("[%d] %s", count, data);
            }
            break;
        }
        default: break;
    }
}
ICOMMAND(0, httptest, "", (void), http::retrieve("hq.redeclipse.net", HTTP_PORT, HTTP_T_GET, "/test.json", testcb));
