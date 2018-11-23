// HTTP server

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
                else if((*p >= '0' && *p <= '9') || ((*p >= 'A' || *p >= 'a') && (*p <= 'F' || *p <= 'f')))
                {
                    char val[3];
                    val[0] = *p;
                    p++;
                    if(*p >= '0' && *p <= '9')
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

    #ifdef STANDALONE
    VAR(0, httpserver, 0, 1, 1);
    #else
    VAR(0, httpserver, 0, 0, 1);
    #endif
    VAR(0, httpserverport, 1, HTTP_PORT, VAR_MAX);
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

    void proceed(httpreq *h)
    {
        httpcmd *c = findcommand(h->path);
        if(c)
        {
            h->input[h->inputpos] = '\0';
            h->send("HTTP/1.0 200 OK");
            ((httpfun)c->fun)(h);
        }
        else h->send("HTTP/1.0 404 Not Found");
        h->state = HTTP_S_PURGE;
    }

    bool getdata(httpreq *h)
    {
        if(h->state != HTTP_S_DATA || h->contype == HTTP_C_URLENC) return false;
        if(h->inputpos >= h->conlength) h->state = HTTP_S_RESPONSE;
        return true;
    }

    bool process(httpreq *h)
    {
        if(h->inputpos < 0) return false;
        if(getdata(h)) return true;
        char *p = h->input;
        for(char *end;; p = end)
        {
            if(h->state == HTTP_S_RESPONSE || (h->state == HTTP_S_DATA && h->contype != HTTP_C_URLENC)) break;
            bool check = true;
            end = (char *)memchr(p, '\r', &h->input[h->inputpos] - p);
            if(!end)
            {
                check = false;
                end = (char *)memchr(p, '\n', &h->input[h->inputpos] - p);
                if(!end) end = (char *)memchr(p, '\0', &h->input[h->inputpos] - p);
            }
            if(!end) break;
            *end++ = '\0';
            if(check && *end == '\n') *end++ = '\0';
            switch(h->state)
            {
                case HTTP_S_START:
                {
                    static const char *httpreqs[HTTP_T_MAX] = {
                        "ERROR",
                        "GET",
                        "POST",
                        "PUT",
                        "DELETE"
                    };
                    loopi(HTTP_T_MAX)
                    {
                        int r = strlen(httpreqs[i]);
                        if(!strncmp(p, httpreqs[i], r))
                        {
                            p += r;
                            while(*p == ' ') p++;
                            h->reqtype = i;
                            break;
                        }
                    }
                    if(h->reqtype != HTTP_T_ERROR)
                    {
                        const char *start = p;
                        p += strcspn(p, " \0");
                        bigstring path = "";
                        copystring(path, start, p-start+1);
                        char *q = path, *beg = q;
                        q += strcspn(q, "?\0");
                        if(*q == '?')
                        {
                            bigstring data = "";
                            copystring(data, beg, q-beg+1);
                            urldecode(h->path, data, sizeof(h->path));
                            q++;
                            vardecode(q, h->vars);
                        }
                        else urldecode(h->path, path, sizeof(h->path));
                        if(h->path[0])
                        {
                            h->state = HTTP_S_HEADERS;
                            break;
                        }
                    }
                    h->send("HTTP/1.0 400 Bad Request");
                    h->state = HTTP_S_PURGE; // failed
                    break;
                }
                case HTTP_S_HEADERS:
                {
                    if(!strlen(p))
                    {
                        if(h->reqtype != HTTP_T_GET) // wants data
                        {
                            const char *cont = h->headers.get("Content-Type");
                            if(cont && *cont)
                            {
                                if(!strcasecmp(cont, "application/x-www-form-urlencoded")) h->contype = HTTP_C_URLENC;
                                else if(!strcasecmp(cont, "application/json")) h->contype = HTTP_C_JSON;
                                else h->contype = HTTP_C_DATA;
                                cont = h->headers.get("Content-Length");
                                if(cont && *cont) h->conlength = atoi(cont);
                            }
                            h->state = HTTP_S_DATA;
                            break;
                        }
                        h->state = HTTP_S_RESPONSE;
                        break;
                    }
                    if(h->headers.length() >= HTTP_HDRS) break;
                    const char *start = p;
                    p += strcspn(p, ":\0");
                    string data = "";
                    copystring(data, start, p-start+1);
                    httppair *v = h->headers.find(data);
                    if(!v)
                    {
                        v = &h->headers.add();
                        copystring(v->name, start, p-start+1);
                    }
                    if(*p == ':') p++;
                    while(*p == ' ') p++;
                    copystring(v->value, p);
                    break;
                }
                case HTTP_S_DATA:
                {
                    vardecode(p, h->vars);
                    h->state = HTTP_S_RESPONSE;
                    break;
                }
                default:
                {
                    h->send("HTTP/1.0 400 Bad Request");
                    h->state = HTTP_S_PURGE; // failed
                    break;
                }
            }
        }
        h->inputpos = &h->input[h->inputpos] - p;
        memmove(h->input, p, h->inputpos);
        getdata(h);
        return h->inputpos < (int)sizeof(h->input);
    }

    void purge(int n)
    {
        httpreq *h = reqs[n];
        enet_socket_destroy(h->socket);
        conoutf("HTTP peer %s disconnected", h->name);
        delete reqs[n];
        reqs.remove(n);
    }

    void init()
    {
        if(!httpserver) return;
        conoutf("Loading HTTP (%s:%d)..", *httpserverip ? httpserverip : "*", httpserverport);
        ENetAddress address = { ENET_HOST_ANY, enet_uint16(httpserverport) };
        if(*httpserverip && enet_address_set_host(&address, httpserverip) < 0) fatal("Failed to resolve HTTP address: %s", httpserverip);
        if((socket = enet_socket_create(ENET_SOCKET_TYPE_STREAM)) == ENET_SOCKET_NULL) fatal("Failed to create HTTP server socket");
        if(enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1) < 0) fatal("Failed to set HTTP server socket option");
        if(enet_socket_bind(socket, &address) < 0) fatal("Failed to bind HTTP server socket");
        if(enet_socket_listen(socket, -1) < 0) fatal("Failed to listen on HTTP server socket");
        if(enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1) < 0) fatal("Failed to make HTTP server socket non-blocking");
        starttime = clocktime;
        conoutf("HTTP server started on %s:[%d]", *httpserverip ? httpserverip : "localhost", httpserverport);
    }

    void runframe()
    {
        if(socket == ENET_SOCKET_NULL) return;

        ENetSocketSet readset, writeset;
        ENetSocket maxsock = socket;
        ENET_SOCKETSET_EMPTY(readset);
        ENET_SOCKETSET_EMPTY(writeset);
        ENET_SOCKETSET_ADD(readset, socket);
        loopv(reqs)
        {
            httpreq *h = reqs[i];
            if(h->outputpos < h->output.length()) ENET_SOCKETSET_ADD(writeset, h->socket);
            else ENET_SOCKETSET_ADD(readset, h->socket);
            maxsock = max(maxsock, h->socket);
        }
        if(enet_socketset_select(maxsock, &readset, &writeset, 0) <= 0) return;
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
                conoutf("HTTP peer %s connected", c->name);
            }
            break;
        }

        loopv(reqs)
        {
            httpreq *h = reqs[i];
            if(h->outputpos < h->output.length() && ENET_SOCKETSET_CHECK(writeset, h->socket))
            {
                ENetBuffer buf;
                buf.data = (void *)&h->output[h->outputpos];
                buf.dataLength = h->output.length()-h->outputpos;
                int res = enet_socket_send(h->socket, NULL, &buf, 1);
                if(res >= 0)
                {
                    h->outputpos += res;
                    if(h->outputpos >= h->output.length())
                    {
                        h->output.setsize(0);
                        h->outputpos = 0;
                        if(h->state == HTTP_S_PURGE) { purge(i--); continue; }
                    }
                }
                else { purge(i--); continue; }
            }
            if(ENET_SOCKETSET_CHECK(readset, h->socket))
            {
                ENetBuffer buf;
                buf.data = &h->input[h->inputpos];
                buf.dataLength = sizeof(h->input) - h->inputpos;
                int res = enet_socket_receive(h->socket, NULL, &buf, 1);
                if(res > 0)
                {
                    h->inputpos += res;
                    h->input[min(h->inputpos, (int)sizeof(h->input)-1)] = '\0';
                    if(!process(h)) { purge(i--); continue; }
                    if(h->state == HTTP_S_RESPONSE) proceed(h);
                }
                else { purge(i--); continue; }
            }
            if(ENET_TIME_DIFFERENCE(totalmillis, h->lastactivity) >= HTTP_TIME || (checkipinfo(control, ipinfo::BAN, h->address.host) && !checkipinfo(control, ipinfo::EXCEPT, h->address.host) && !checkipinfo(control, ipinfo::TRUST, h->address.host)))
            {
                purge(i--);
                continue;
            }
        }
    }

    void cleanup()
    {
        loopv(reqs) purge(i--);
        if(socket != ENET_SOCKET_NULL) enet_socket_destroy(socket);
        socket = ENET_SOCKET_NULL;
    }
}

int printjson(json *j, char *dst, int level, size_t len)
{
    bigstring data = "";
    if(!j) return 0;
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
            else concformatstring(data, "\"%s\"", j->name);
            count++;
            break;
        }
        case JSON_OBJECT: case JSON_ARRAY:
        {
            if(j->name[0]) concformatstring(data, "\"%s\": %c ", j->name, int(j->type != JSON_OBJECT ? '[' : '{'));
            else concformatstring(data, "%c ", int(j->type != JSON_OBJECT ? '[' : '{'));
            loopv(j->children)
            {
                count += printjson(j->children[i], data, level+1, len);
                if(i < j->children.length()-1) concatstring(data, ", ");
            }
            concformatstring(data, " %c", int(j->type != JSON_OBJECT ? ']' : '}'));
        }
        default: break;
    }
    concatstring(dst, data, len);
    return count;
}

int processjs(const char *str, json *j, jsmntok_t *t, int r, int start, int objects = -1)
{
    int q = 0;
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
                json *k = new json;
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
                json *k = new json;
                k->type = t[i].type != JSMN_OBJECT ? JSON_ARRAY : JSON_OBJECT;
                copystring(k->name, name);
                int size = t[i].size, a = processjs(str, k, t, r, i+1, size);
                i += a;
                q += a;
                name[0] = data[0] = 0;
                objs++;
                j->children.add(k);
                break;
            }
            default: break;
        }
        q++;
        if(objects >= 0 && objs >= objects) break;
    }
    return q;
}

json *loadjson(const char *str)
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
    json *j = new json;
    j->type = JSON_OBJECT;
    if(!processjs(str, j, &t[0], r, 1))
    {
        DELETEP(j);
        return NULL;
    }
    return j;
}

void httpindex(httpreq *h)
{
    h->send("Content-Type: text/plain");
    h->send("\r\n");
    defformatstring(branch, "%s", versionbranch);
    if(versionbuild > 0) concformatstring(branch, "-%d", versionbuild);
    h->sendf("%s %s-%s%d-%s %s (%s) [0x%.8x]", versionname, versionstring, versionplatname, versionarch, branch, versionisserver ? "server" : "client", versionrelease, versioncrc);
    h->send("");
    h->send("Headers:");
    loopv(h->headers.values) h->sendf("- %s = %s", h->headers.values[i].name, h->headers.values[i].value);
    h->send("");
    h->send("Vars:");
    loopv(h->vars.values) h->sendf("- %s = %s", h->vars.values[i].name, h->vars.values[i].value);
    h->send("");
    switch(h->contype)
    {
        case HTTP_C_DATA: h->sendf("Generic Data: [%d] %s", h->inputpos, h->input); break;
        case HTTP_C_JSON:
        {
            json *j = loadjson(h->input);
            if(!j) break;
            h->sendf("JSON Data: [%d] (%d)", h->inputpos, j->children.length());
            h->send("-");
            h->send(h->input);
            h->send("-");
            bigstring data = "";
            int count = printjson(j, data, 0, sizeof(data));
            h->sendf("[%d] %s", count, data);
            h->send("-");
            DELETEP(j);
            break;
        }
    }
}
HTTP("/", httpindex);
