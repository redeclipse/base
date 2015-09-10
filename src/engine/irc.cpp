#include "engine.h"

VAR(0, ircfilter, 0, 2, 2);
VAR(0, ircverbose, 0, 0, 2);
VAR(0, ircnickretry, 5, 300, VAR_MAX);
VAR(0, ircautorejoin, 0, 300, VAR_MAX);
VAR(0, ircpingpong, 5, 120, VAR_MAX);
VAR(0, irctimeout, 5, 60, VAR_MAX);
VAR(0, ircautoaway, 0, 15, VAR_MAX); // time in seconds after closing the gui the user is marked as away
VAR(0, ircnickhighlight, 0, 1, 1);

vector<ircnet *> ircnets;

ircnet *ircfind(const char *name)
{
    if(name && *name)
    {
        loopv(ircnets) if(!strcmp(ircnets[i]->name, name)) return ircnets[i];
    }
    return NULL;
}

void ircprintf(ircnet *n, int relay, const char *target, const char *msg, ...)
{
    defvformatbigstring(str, msg, msg);
    string s = "";
    if(target && *target && strcasecmp(target, n->nick))
    {
        ircchan *c = ircfindchan(n, target);
        if(c)
        {
            formatstring(s, "\fs\fa[%s:%s]\fS", n->name, c->name);
            #ifndef STANDALONE
            c->buffer.addline(str, MAXIRCLINES);
            c->updated |= IRCUP_MSG;
            #endif
            if(n->type == IRCT_RELAY && c->relay >= relay)
                server::srvmsgf(relay > 1 ? -2 : -3, "\fs\fa[%s]\fS %s", c->friendly, str);
        }
        else
        {
            formatstring(s, "\fs\fa[%s:%s]\fS", n->name, target);
            #ifndef STANDALONE
            n->buffer.addline(str, MAXIRCLINES);
            n->updated |= IRCUP_MSG;
            #endif
        }
    }
    else
    {
        formatstring(s, "\fs\fa[%s]\fS", n->name);
        #ifndef STANDALONE
        n->buffer.addline(newstring(str), MAXIRCLINES);
        n->updated |= IRCUP_MSG;
        #endif
    }
    if(ircverbose) console(0, "%s %s", s, str); // console is not used to relay
}

void ircestablish(ircnet *n)
{
    if(!n) return;
    n->lastattempt = n->lastactivity = clocktime;
    n->lastping = 0;
    if(n->address.host == ENET_HOST_ANY)
    {
        ircprintf(n, 4, NULL, "looking up %s:[%d]...", n->serv, n->port);
        if(!resolverwait(n->serv, &n->address))
        {
            ircprintf(n, 4, NULL, "unable to resolve %s:[%d]...", n->serv, n->port);
            n->state = IRC_DISC;
            return;
        }
    }

    ENetAddress address = { ENET_HOST_ANY, enet_uint16(n->port) };
    if(*n->ip && enet_address_set_host(&address, n->ip) < 0) ircprintf(n, 4, NULL, "failed to bind address: %s", n->ip);
    n->sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(n->sock != ENET_SOCKET_NULL && *n->ip && enet_socket_bind(n->sock, &address) < 0)
    {
        ircprintf(n, 4, NULL, "failed to bind connection socket: %s", n->ip);
        address.host = ENET_HOST_ANY;
    }
    if(n->sock != ENET_SOCKET_NULL) enet_socket_set_option(n->sock, ENET_SOCKOPT_NONBLOCK, 1);
    if(n->sock == ENET_SOCKET_NULL || enet_socket_connect(n->sock, &n->address) < 0)
    {
        ircprintf(n, 4, NULL, n->sock == ENET_SOCKET_NULL ? "could not open socket to %s:[%d]" : "could not connect to %s:[%d]", n->serv, n->port);
        if(n->sock != ENET_SOCKET_NULL)
        {
            enet_socket_destroy(n->sock);
            n->sock = ENET_SOCKET_NULL;
        }
        n->state = IRC_DISC;
        return;
    }
    n->state = IRC_WAIT;
    ircprintf(n, 4, NULL, "connecting to %s:[%d]...", n->serv, n->port);
}

void ircsend(ircnet *n, const char *msg, ...)
{
    if(!n) return;
    defvformatstring(str, msg, msg);
    if(n->sock == ENET_SOCKET_NULL || !*msg) return; // don't spew \n
    if(ircverbose >= 2) console(0, "[%s] >>> %s", n->name, str);
    concatstring(str, "\n");
    ENetBuffer buf;
    uchar ubuf[512];
    size_t len = strlen(str), carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf)-1, &((uchar *)str)[carry], len - carry, &carry);
        if(carry >= len) ubuf[numu++] = '\n';
        loopi(numu) switch(ubuf[i])
        {
            case '\v': ubuf[i] = '\x01'; break;
            case '\f': ubuf[i] = '\x03'; break; // color code
            case '\r': ubuf[i] = '\x0F'; break; // end color formatting
        }
        buf.data = ubuf;
        buf.dataLength = numu;
        enet_socket_send(n->sock, NULL, &buf, 1);
    }
}

void cube2irc(char *dst, const char *src)
{
    int colorpos = 0; char colorstack[10];
    memset(colorstack, 'u', sizeof(colorstack)); //indicate user color
    for(int c = *src; c; c = *++src)
    {
        if(c == '\f')
        {
            c = *++src;
            if(c == 'z')
            {
                c = *++src;
                if(c) ++src;
            }
            else if(c == '[' || c == '(' || c == '{')
            {
                const char *end = strchr(src, c == '[' ? ']' : (c == '(' ? ')' : '}'));
                src += end ? end-src : strlen(src);
            }
            else if(c == 's') { if(colorpos < (int)sizeof(colorstack)-1) colorpos++; continue; }
            else if(c == 'S') { if(colorpos > 0) --colorpos; c = colorstack[colorpos]; }
            int oldcolor = colorstack[colorpos]; colorstack[colorpos] = c;
            switch(c)
            {
                case 'B': *dst++ = '\f'; *dst++ = '0'; *dst++ = '2'; break; // dark blue
                case 'G': *dst++ = '\f'; *dst++ = '0'; *dst++ = '3'; break; // dark green
                case 'r': case '3': *dst++ = '\f'; *dst++ = '0'; *dst++ = '4'; break; // red
                case 'n': *dst++ = '\f'; *dst++ = '0'; *dst++ = '5'; break; // brown
                case 'p': case 'v': *dst++ = '\f'; *dst++ = '0'; *dst++ = '6'; break; // purple
                case 'o': case '6': case 'O': *dst++ = '\f'; *dst++ = '0'; *dst++ = '7'; break; // orange
                case 'y': case '2': case 'Y': *dst++ = '\f'; *dst++ = '0'; *dst++ = '8'; break; // yellow
                case 'g': case '0': *dst++ = '\f'; *dst++ = '0'; *dst++ = '9'; break; // green
                case 'C': *dst++ = '\f'; *dst++ = '1'; *dst++ = '0'; break; // dark cyan
                case 'c': case '9': *dst++ = '\f'; *dst++ = '1'; *dst++ = '1'; break; // cyan
                case 'b': case '1': *dst++ = '\f'; *dst++ = '1'; *dst++ = '2'; break; // blue
                case 'm': case '5': case 'M': *dst++ = '\f'; *dst++ = '1'; *dst++ = '3'; break; // magenta
                case 'k': case '8': case 'd': case 'A': *dst++ = '\f'; *dst++ = '1'; *dst++ = '4'; break; // dark grey
                case 'a': case '4': *dst++ = '\f'; *dst++ = '1'; *dst++ = '5'; break; // grey
                case 'u': case 'w': case '7': *dst++ = '\r'; break; // no color
                default: colorstack[colorpos] = oldcolor; break;
            }
            continue;
        }
        if(iscubeprint(c) || iscubespace(c)) *dst++ = c;
    }
    *dst = '\0';
}

void irc2cube(char *dst, const char *src)
{
    for(int c = *src; c; c = *++src)
    {
        if(c == '\f')
        {
            c = *++src;
            switch(c)
            {
                case '0':
                    c = *++src;
                    switch(c)
                    {
                        case '0': *dst++ = '\f'; *dst++ = 'w'; break; // white
                        case '1': *dst++ = '\f'; *dst++ = 'A'; break; // dark grey (black too hard to see)
                        case '2': *dst++ = '\f'; *dst++ = 'B'; break; // dark blue
                        case '3': *dst++ = '\f'; *dst++ = 'G'; break; // dark green
                        case '4': *dst++ = '\f'; *dst++ = 'r'; break; // red
                        case '5': *dst++ = '\f'; *dst++ = 'n'; break; // brown
                        case '6': *dst++ = '\f'; *dst++ = 'p'; break; // purple
                        case '7': *dst++ = '\f'; *dst++ = 'o'; break; // orange
                        case '8': *dst++ = '\f'; *dst++ = 'y'; break; // yellow
                        case '9': *dst++ = '\f'; *dst++ = 'g'; break; // green
                        default: *dst++ = '\f'; *dst++ = 'w'; c = *--src; break;
                    }
                    break;
                case '1':
                    c = *++src;
                    switch(c)
                    {
                        case '0': *dst++ = '\f'; *dst++ = 'C'; break; // dark cyan
                        case '1': *dst++ = '\f'; *dst++ = 'c'; break; // cyan
                        case '2': *dst++ = '\f'; *dst++ = 'b'; break; // blue
                        case '3': *dst++ = '\f'; *dst++ = 'm'; break; // magenta
                        case '4': *dst++ = '\f'; *dst++ = 'A'; break; // dark grey
                        case '5': *dst++ = '\f'; *dst++ = 'a'; break; // grey
                        default: *dst++ = '\f'; *dst++ = 'A'; c = *--src; break;
                    }
                    break;
                case '2': *dst++ = '\f'; *dst++ = 'B'; break; // dark blue
                case '3': *dst++ = '\f'; *dst++ = 'G'; break; // dark green
                case '4': *dst++ = '\f'; *dst++ = 'r'; break; // red
                case '5': *dst++ = '\f'; *dst++ = 'n'; break; // brown
                case '6': *dst++ = '\f'; *dst++ = 'p'; break; // purple
                case '7': *dst++ = '\f'; *dst++ = 'o'; break; // orange
                case '8': *dst++ = '\f'; *dst++ = 'y'; break; // yellow
                case '9': *dst++ = '\f'; *dst++ = 'g'; break; // green
                default: *dst++ = '\f'; *dst++ = 'w'; c = *--src; break;
            }
        }
        else if(c == '\x0F')
        {
            *dst++ = '\f'; *dst++ = 'w';
        }
        else if(iscubeprint(c) || iscubespace(c)) *dst++ = c;
    }
    *dst = '\0';
}

void ircoutf(int relay, const char *msg, ...)
{
    defvformatstring(src, msg, msg);
    string str = "";
    switch(ircfilter)
    {
        case 2: filterstring(str, src); break;
        case 1: cube2irc(str, src); break;
        case 0: default: copystring(str, src); break;
    }
    loopv(ircnets) if(ircnets[i]->sock != ENET_SOCKET_NULL && ircnets[i]->type == IRCT_RELAY && ircnets[i]->state == IRC_ONLINE)
    {
        ircnet *n = ircnets[i];
#if 0 // workaround for freenode's crappy dropping all but the first target of multi-target messages even though they don't state MAXTARGETS=1 in 005 string..
        string s = "";
        loopvj(n->channels) if(n->channels[j].state == IRCC_JOINED && n->channels[j].relay >= relay)
        {
            ircchan *c = &n->channels[j];
            if(s[0]) concatstring(s, ",");
            concatstring(s, c->name);
        }
        if(s[0]) ircsend(n, "PRIVMSG %s :%s", s, str);
#else
        loopvj(n->channels) if(n->channels[j].state == IRCC_JOINED && n->channels[j].relay >= relay)
            ircsend(n, "PRIVMSG %s :%s", n->channels[j].name, str);
#endif
    }
}

int ircrecv(ircnet *n)
{
    if(!n) return -1;
    if(n->sock == ENET_SOCKET_NULL) return -2;
    int total = 0;
    for(;;)
    {
        ENetBuffer buf;
        buf.data = n->input + n->inputlen;
        buf.dataLength = sizeof(n->input) - n->inputlen;
        int len = enet_socket_receive(n->sock, NULL, &buf, 1);
        if(!len) break;
        if(len < 0) return -3;
        loopi(len) switch(n->input[n->inputlen+i])
        {
            case '\x01': n->input[n->inputlen+i] = '\v'; break;
            case '\x03': n->input[n->inputlen+i] = '\f'; break;
            case '\v': case '\f': n->input[n->inputlen+i] = ' '; break;
        }
        n->inputlen += len;
        size_t carry = 0, decoded = decodeutf8(&n->input[n->inputcarry], n->inputlen - n->inputcarry, &n->input[n->inputcarry], n->inputlen - n->inputcarry, &carry);
        if(carry > decoded)
        {
            memmove(&n->input[n->inputcarry + decoded], &n->input[n->inputcarry + carry], n->inputlen - (n->inputcarry + carry));
            n->inputlen -= carry - decoded;
        }
        n->inputcarry += decoded;
        total += decoded;
    }
    return total;
}

void ircnewnet(int type, const char *name, const char *serv, int port, const char *nick, const char *ip, const char *passkey)
{
    if(!name || !*name || !serv || !*serv || !port || !nick || !*nick) return;
    ircnet *m = ircfind(name);
    if(m)
    {
        conoutf("ircnet %s already exists", m->name);
        return;
    }
    ircnet &n = *ircnets.add(new ircnet);
    n.type = type;
    n.port = port;
    copystring(n.name, name);
    copystring(n.serv, serv);
    copystring(n.mnick, nick);
    copystring(n.nick, nick);
    copystring(n.ip, ip);
    copystring(n.passkey, passkey);
    n.address.host = ENET_HOST_ANY;
    n.address.port = n.port;
    n.input[0] = n.authname[0] = n.authpass[0] = '\0';
#ifndef STANDALONE
    n.lastseen = clocktime;
#endif
    ircprintf(&n, 4, NULL, "added %s %s (%s:%d) [%s]", type == IRCT_RELAY ? "relay" : "client", name, serv, port, nick);
}

ICOMMAND(0, ircaddclient, "ssisss", (const char *n, const char *s, int *p, const char *c, const char *h, const char *z), {
    ircnewnet(IRCT_CLIENT, n, s, *p, c, h, z);
});
ICOMMAND(0, ircaddrelay, "ssisss", (const char *n, const char *s, int *p, const char *c, const char *h, const char *z), {
    ircnewnet(IRCT_RELAY, n, s, *p, c, h, z);
});
ICOMMAND(0, ircserv, "ss", (const char *name, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "current server is: %s", n->serv); return; }
    copystring(n->serv, s);
});
ICOMMAND(0, ircport, "ss", (const char *name, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s || !parseint(s)) { ircprintf(n, 4, NULL, "current port is: %d", n->port); return; }
    n->port = parseint(s);
});
ICOMMAND(0, ircnick, "ss", (const char *name, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "current main nickname is: %s", n->mnick); return; }
    copystring(n->mnick, s);
});
ICOMMAND(0, ircbind, "ss", (const char *name, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "currently bound to: %s", n->ip); return; }
    copystring(n->ip, s);
});
ICOMMAND(0, ircpass, "ss", (const char *name, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "current password is: %s", n->passkey && *n->passkey ? "<set>" : "<not set>"); return; }
    copystring(n->passkey, s);
});
ICOMMAND(0, ircauthcommand, "ss", (const char *name, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "current auth command is: %s", n->authcommand && *n->authcommand ? "<set>" : "<not set>"); return; }
    copystring(n->authcommand, s);
});
ICOMMAND(0, ircauth, "sss", (const char *name, const char *s, const char *t), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(!s || !*s || !t || !*t) { ircprintf(n, 4, NULL, "current auth details are: %s (%s)", n->authname, n->authpass && *n->authpass ? "<set>" : "<not set>"); return; }
    copystring(n->authname, s);
    copystring(n->authpass, t);
});
ICOMMAND(0, ircconnect, "s", (const char *name), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    if(n->state > IRC_DISC) { ircprintf(n, 4, NULL, "network already already active"); return; }
    ircestablish(n);
});

ircchan *ircfindchan(ircnet *n, const char *name)
{
    if(n && name && *name)
    {
        loopv(n->channels) if(!strcasecmp(n->channels[i].name, name))
            return &n->channels[i];
    }
    return NULL;
}

bool ircjoin(ircnet *n, ircchan *c)
{
    if(!n || !c) return false;
    if(n->state != IRC_ONLINE)
    {
        ircprintf(n, 4, NULL, "cannot join %s until connection is online", c->name);
        return false;
    }
    if(*c->passkey) ircsend(n, "JOIN %s :%s", c->name, c->passkey);
    else ircsend(n, "JOIN %s", c->name);
    c->state = IRCC_JOINING;
    c->lastjoin = clocktime;
    c->lastsync = 0;
    return true;
}

bool ircenterchan(ircnet *n, const char *name)
{
    if(!n) return false;
    ircchan *c = ircfindchan(n, name);
    if(!c)
    {
        ircprintf(n, 4, NULL, "no channel called %s available", name);
        return false;
    }
    return ircjoin(n, c);
}

bool ircnewchan(int type, const char *name, const char *channel, const char *friendly, const char *passkey, int relay)
{
    if(!name || !*name || !channel || !*channel) return false;
    ircnet *n = ircfind(name);
    if(!n)
    {
        conoutf("no such ircnet: %s", name);
        return false;
    }
    ircchan *c = ircfindchan(n, channel);
    if(c)
    {
        ircprintf(n, 4, NULL, "channel %s already exists", c->name);
        return false;
    }
    ircchan &d = n->channels.add();
    d.state = IRCC_NONE;
    d.type = type;
    d.relay = relay;
    d.lastjoin = d.lastsync = 0;
    copystring(d.name, channel);
    copystring(d.friendly, friendly && *friendly ? friendly : channel);
    copystring(d.passkey, passkey);
    if(n->state == IRC_ONLINE) ircjoin(n, &d);
    ircprintf(n, 4, NULL, "added channel: %s", d.name);
    return true;
}

ICOMMAND(0, ircaddchan, "ssssi", (const char *n, const char *c, const char *f, const char *z, int *r), {
    ircnewchan(IRCCT_AUTO, n, c, f, z, *r);
});
ICOMMAND(0, ircjoinchan, "ssssi", (const char *n, const char *c, const char *f, const char *z, int *r), {
    ircnewchan(IRCCT_NONE, n, c, f, z, *r);
});
ICOMMAND(0, ircpasschan, "sss", (const char *name, const char *chan, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    ircchan *c = ircfindchan(n, chan);
    if(!c) { ircprintf(n, 4, NULL, "no such channel: %s", chan); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "channel %s current password is: %s", c->name, c->passkey && *c->passkey ? "<set>" : "<not set>"); return; }
    copystring(c->passkey, s);
});
ICOMMAND(0, ircrelaychan, "sss", (const char *name, const char *chan, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    ircchan *c = ircfindchan(n, chan);
    if(!c) { ircprintf(n, 4, NULL, "no such channel: %s", chan); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "channel %s current relay level is: %d", c->name, c->relay); return; }
    c->relay = parseint(s);
});
ICOMMAND(0, ircfriendlychan, "sss", (const char *name, const char *chan, const char *s), {
    ircnet *n = ircfind(name);
    if(!n) { conoutf("no such ircnet: %s", name); return; }
    ircchan *c = ircfindchan(n, chan);
    if(!c) { ircprintf(n, 4, NULL, "no such channel: %s", chan); return; }
    if(!s || !*s) { ircprintf(n, 4, NULL, "channel %s current friendly name is: %s", c->name, c->friendly); return; }
    copystring(c->friendly, s);
});

bool ircmatchnick(ircnet *n, const char *str)
{
    if(!strncasecmp(str, n->mnick, strlen(n->mnick))) return true;
    if(!strncasecmp(str, n->nick, strlen(n->nick))) return true;
    return false;
}

void ircprocess(ircnet *n, char *user[3], int g, int numargs, char *w[])
{
    if(!strcasecmp(w[g], "NOTICE") || !strcasecmp(w[g], "PRIVMSG"))
    {
        if(numargs > g+2)
        {
            bool ismsg = strcasecmp(w[g], "NOTICE");
            int len = strlen(w[g+2]);
            if(w[g+2][0] == '\v' && w[g+2][len-1] == '\v')
            {
                char *p = w[g+2];
                p++;
                const char *word = p;
                p += strcspn(p, " \v\0");
                if(p-word > 0)
                {
                    char *q = newstring(word, p-word);
                    p++;
                    const char *start = p;
                    p += strcspn(p, "\v\0");
                    char *r = p-start > 0 ? newstring(start, p-start) : newstring("");
                    if(ismsg)
                    {
                        if(!strcasecmp(q, "ACTION"))
                        {
                            string str = "";
                            irc2cube(str, r);
                            ircprintf(n, 1, g ? w[g+1] : NULL, "\fv* %s %s", user[0], str);
                        }
                        else
                        {
                            ircprintf(n, 4, g ? w[g+1] : NULL, "\fr%s requests: %s %s", user[0], q, r);

                            if(!strcasecmp(q, "VERSION"))
                                ircsend(n, "NOTICE %s :\vVERSION %s v%s-%s%d-%s (%s)%s%s\v", user[0], VERSION_NAME, VERSION_STRING, versionplatname, versionarch, versionbranch, VERSION_RELEASE, *VERSION_URL ? ", " : "", VERSION_URL);
                            else if(!strcasecmp(q, "PING")) // eh, echo back
                                ircsend(n, "NOTICE %s :\vPING %s\v", user[0], r);
                        }
                    }
                    else ircprintf(n, 4, g ? w[g+1] : NULL, "\fr%s replied: %s %s", user[0], q, r);
                    DELETEA(q); DELETEA(r);
                }
            }
            else if(ismsg)
            {
                string str = "";
                if(n->type == IRCT_RELAY && g && strcasecmp(w[g+1], n->nick) && ircmatchnick(n, w[g+2]))
                {
                    const char *p = &w[g+2][strlen(n->nick)];
                    while(p && (*p == ':' || *p == ';' || *p == ',' || *p == '.' || *p == ' ' || *p == '\t')) p++;
                    if(p && *p)
                    {
                        irc2cube(str, p);
                        ircprintf(n, 0, w[g+1], "\fa<\fw%s\fa>\fw %s", user[0], str);
                    }
                }
                else
                {
                    irc2cube(str, w[g+2]);
                    ircprintf(n, 1, g ? w[g+1] : NULL, "\fa<\fw%s\fa>\fw %s", user[0], str);
                    if(g)
                    {
                        if(!strcasecmp(w[g+1], n->nick))
                            console(CON_SELF, "\fa[%s] <\fw%s\fa (to you)>\fw %s", n->name, user[0], str);
                        else if(ircnickhighlight && ircmatchnick(n, w[g+2]))
                            console(CON_SELF, "\fa[%s] <\fw%s\fa (to %s)>\fw %s", n->name, user[0], w[g+1], str);
                    }
                }
            }
            else ircprintf(n, 2, g ? w[g+1] : NULL, "\fo-%s- %s", user[0], w[g+2]);
        }
    }
    else if(!strcasecmp(w[g], "NICK"))
    {
        if(numargs > g+1)
        {
            if(!strcasecmp(user[0], n->nick)) copystring(n->nick, w[g+1]);
            ircprintf(n, 3, NULL, "\fm%s (%s@%s) is now known as %s", user[0], user[1], user[2], w[g+1]);
        }
    }
    else if(!strcasecmp(w[g], "JOIN"))
    {
        if(numargs > g+1)
        {
            ircchan *c = ircfindchan(n, w[g+1]);
            if(c && !strcasecmp(user[0], n->nick))
            {
                c->state = IRCC_JOINED;
                c->lastjoin = c->lastsync = clocktime;
                #ifndef STANDALONE
                c->updated |= IRCUP_NEW;
                #endif
            }
            ircprintf(n, 3, w[g+1], "\fg%s (%s@%s) has joined", user[0], user[1], user[2]);
        }
    }
    else if(!strcasecmp(w[g], "PART"))
    {
        if(numargs > g+1)
        {
            ircchan *c = ircfindchan(n, w[g+1]);
            if(c && !strcasecmp(user[0], n->nick))
            {
                c->state = IRCC_NONE;
                c->lastjoin = clocktime;
                c->lastsync = 0;
            }
            ircprintf(n, 3, w[g+1], "\fo%s (%s@%s) has left", user[0], user[1], user[2]);
        }
    }
    else if(!strcasecmp(w[g], "QUIT"))
    {
        if(numargs > g+1) ircprintf(n, 3, NULL, "\fr%s (%s@%s) has quit: %s", user[0], user[1], user[2], w[g+1]);
        else ircprintf(n, 3, NULL, "\fr%s (%s@%s) has quit", user[0], user[1], user[2]);
    }
    else if(!strcasecmp(w[g], "KICK"))
    {
        if(numargs > g+2)
        {
            ircchan *c = ircfindchan(n, w[g+1]);
            if(c && !strcasecmp(w[g+2], n->nick))
            {
                c->state = IRCC_KICKED;
                c->lastjoin = clocktime;
                c->lastsync = 0;
            }
            ircprintf(n, 3, w[g+1], "\fr%s (%s@%s) has kicked %s from %s", user[0], user[1], user[2], w[g+2], w[g+1]);
        }
    }
    else if(!strcasecmp(w[g], "MODE"))
    {
        if(numargs > g+2)
        {
            string modestr = "";
            loopi(numargs-g-2)
            {
                if(i) concatstring(modestr, " ");
                concatstring(modestr, w[g+2+i]);
            }
            ircprintf(n, 4, w[g+1], "\fr%s (%s@%s) sets mode: %s %s", user[0], user[1], user[2], w[g+1], modestr);
        }
        else if(numargs > g+1)
            ircprintf(n, 4, w[g+1], "\fr%s (%s@%s) sets mode: %s", user[0], user[1], user[2], w[g+1]);
    }
    else if(!strcasecmp(w[g], "PING"))
    {
        if(numargs > g+1)
        {
            ircprintf(n, 4, NULL, "%s PING %s", user[0], w[g+1]);
            ircsend(n, "PONG %s", w[g+1]);
        }
        else
        {
            ircprintf(n, 4, NULL, "%s PING", user[0]);
            ircsend(n, "PONG %d", clocktime);
        }
    }
    else if(!strcasecmp(w[g], "ERROR"))
    {
        if(numargs > g+1) ircprintf(n, 4, NULL, "%s ERROR %s", user[0], w[g+1]);
        else ircprintf(n, 4, NULL, "%s ERROR", user[0]);
        n->state = IRC_QUIT;
    }
    else
    {
        int numeric = *w[g] && *w[g] >= '0' && *w[g] <= '9' ? atoi(w[g]) : 0, off = 0;
        string s = "";
        #define irctarget(a) (!strcasecmp(n->nick, a) || *a == '#' || ircfindchan(n, a))
        char *targ = numargs > g+1 && irctarget(w[g+1]) ? w[g+1] : NULL;
        if(numeric)
        {
            off = numeric == 353 ? 2 : 1;
            if(numargs > g+off+1 && irctarget(w[g+off+1]))
            {
                targ = w[g+off+1];
                off++;
            }
        }
        else concatstring(s, user[0]);
        for(int i = g+off+1; numargs > i && w[i]; i++)
        {
            if(s[0]) concatstring(s, " ");
            concatstring(s, w[i]);
        }
        if(numeric) switch(numeric)
        {
            case 1:
            {
                if(n->state == IRC_CONN)
                {
                    n->state = IRC_ONLINE;
                    ircprintf(n, 4, NULL, "\fbnow connected to %s as %s (%s)", user[0], n->nick, n->mnick);
                    if(*n->authname && *n->authpass) ircsend(n, "PRIVMSG %s :%s", n->authname, n->authpass);
                    if(*n->authcommand) ircsend(n, "%s", n->authcommand);
                }
                break;
            }
            case 433:
            {
                concatstring(n->nick, "_");
                ircsend(n, "NICK %s", n->nick);
                n->lastnick = clocktime;
                break;
            }
            case 471:
            case 473:
            case 474:
            case 475:
            {
                ircchan *c = ircfindchan(n, w[g+2]);
                if(c)
                {
                    c->state = IRCC_BANNED;
                    c->lastjoin = clocktime;
                    c->lastsync = 0;
                    if(c->type == IRCCT_AUTO)
                        ircprintf(n, 4, w[g+2], "\fbwaiting %ds to rejoin %s", ircautorejoin, c->name);
                    else ircprintf(n, 4, NULL, "\fbbanned from channel: %s", c->name);
                }
                break;
            }
            default: break;
        }
        if(s[0]) ircprintf(n, 4, targ, "\fw%s %s", w[g], s);
        else ircprintf(n, 4, targ, "\fw%s", w[g]);
    }
}

void ircparse(ircnet *n)
{
    const int MAXWORDS = 25;
    char *w[MAXWORDS], *p = (char *)n->input, *start = p, *end = &p[n->inputcarry];
    loopi(MAXWORDS) w[i] = NULL;
    while(p < end)
    {
        bool full = false;
        int numargs = 0, g = 0;
        while(iscubespace(*p)) { if(++p >= end) goto cleanup; }
        start = p;
        if(*p == ':') { g = 1; ++p; }
        for(;;)
        {
            const char *word = p;
            if(*p == ':') { word++; full = true; } // uses the rest of the input line then
            while(*p != '\r' && *p != '\n' && (full || *p != ' ')) { if(++p >= end) goto cleanup; }

            if(numargs < MAXWORDS) w[numargs++] = newstring(word, p-word);

            if(*p == '\n' || *p == '\r') { ++p; start = p; break; }
            else while(*p == ' ') { if(++p >= end) goto cleanup; }
        }
        if(numargs)
        {
            char *user[3] = { NULL, NULL, NULL };
            if(g)
            {
                char *t = w[0], *u = strrchr(t, '!');
                if(u)
                {
                    user[0] = newstring(t, u-t);
                    t = u + 1;
                    u = strrchr(t, '@');
                    if(u)
                    {
                        user[1] = newstring(t, u-t);
                        if(*u++) user[2] = newstring(u);
                        else user[2] = newstring("*");
                    }
                    else
                    {
                        user[1] = newstring("*");
                        user[2] = newstring("*");
                    }
                }
                else
                {
                    user[0] = newstring(t);
                    user[1] = newstring("*");
                    user[2] = newstring("*");
                }
            }
            else
            {
                user[0] = newstring("*");
                user[1] = newstring("*");
                user[2] = newstring(n->serv);
            }
            if(numargs > g) ircprocess(n, user, g, numargs, w);
            loopi(3) DELETEA(user[i]);
        }
    cleanup:
        loopi(numargs) DELETEA(w[i]);
    }
    int parsed = start - (char *)n->input;
    if(parsed > 0)
    {
        memmove(n->input, start, n->inputlen - parsed);
        n->inputcarry -= parsed;
        n->inputlen -= parsed;
    }
}

void ircdiscon(ircnet *n, const char *msg = NULL)
{
    if(!n) return;
    if(n->state == IRC_WAIT) ircprintf(n, 4, NULL, "could not connect to %s:[%d]", n->serv, n->port);
    else if(msg) ircprintf(n, 4, NULL, "disconnected from %s (%s:[%d]): %s", n->name, n->serv, n->port, msg);
    else ircprintf(n, 4, NULL, "disconnected from %s (%s:[%d])", n->name, n->serv, n->port);
    enet_socket_destroy(n->sock);
    n->state = IRC_DISC;
    n->sock = ENET_SOCKET_NULL;
    n->lastattempt = clocktime;
    n->lastactivity = n->lastping = 0;
}

void irccleanup()
{
    loopv(ircnets) if(ircnets[i]->sock != ENET_SOCKET_NULL)
    {
        ircnet *n = ircnets[i];
        ircsend(n, "QUIT :%s%s%s", VERSION_NAME, *VERSION_URL ? ", " : "", VERSION_URL);
        ircdiscon(n, "shutdown");
    }
}

bool ircaddsockets(ENetSocket &maxsock, ENetSocketSet &readset, ENetSocketSet &writeset)
{
    int numsocks = 0;
    loopv(ircnets)
    {
        ircnet *n = ircnets[i];
        if(n->sock != ENET_SOCKET_NULL && n->state > IRC_DISC) switch(n->state)
        {
            case IRC_WAIT:
                ENET_SOCKETSET_ADD(writeset, n->sock);
                // fall-through
            case IRC_ONLINE: case IRC_CONN: case IRC_QUIT:
                maxsock = maxsock == ENET_SOCKET_NULL ? n->sock : max(maxsock, n->sock);
                ENET_SOCKETSET_ADD(readset, n->sock);
                numsocks++;
                break;
        }
    }
    return numsocks > 0;
}

void ircchecksockets(ENetSocketSet &readset, ENetSocketSet &writeset)
{
    loopv(ircnets)
    {
        ircnet *n = ircnets[i];
        if(n->sock != ENET_SOCKET_NULL && n->state > IRC_DISC) switch(n->state)
        {
            case IRC_WAIT:
                if(ENET_SOCKETSET_CHECK(readset, n->sock) || ENET_SOCKETSET_CHECK(writeset, n->sock))
                {
                    int error = 0;
                    if(enet_socket_get_option(n->sock, ENET_SOCKOPT_ERROR, &error) < 0 || error) ircdiscon(n);
                    else n->state = IRC_ATTEMPT;
                }
                break;
            case IRC_ONLINE: case IRC_CONN: case IRC_QUIT:
                if(ENET_SOCKETSET_CHECK(readset, n->sock)) switch(ircrecv(n))
                {
                    case -3: ircdiscon(n, "read error"); break;
                    case -2: ircdiscon(n, "connection reset"); break;
                    case -1: ircdiscon(n, "invalid connection"); break;
                    case 0: break;
                    default:
                    {
                        ircparse(n);
                        n->lastactivity = clocktime;
                        n->lastping = 0;
                        break;
                    }
                }
                break;
         }
    }
}

void ircslice()
{
    #ifndef STANDALONE
    loopvrev(ircnets)
    {
        ircnet *n = ircnets[i];
        if((n->sock == ENET_SOCKET_NULL || n->state <= IRC_DISC) && n->updated&IRCUP_LEAVE)
        {
            delete n;
            ircnets.remove(i);
            continue;
        }
        loopvjrev(n->channels)
        {
            ircchan &c = n->channels[j];
            if(c.state != IRCC_JOINED && c.state != IRCC_JOINING && (c.type != IRCCT_AUTO || c.updated&IRCUP_LEAVE))
                n->channels.remove(j);
        }
        if(n->type != IRCT_RELAY && n->state == IRC_ONLINE)
        {
            if(n->away && n->lastseen > n->away)
            {
                ircsend(n, "AWAY");
                n->away = 0;
            }
            else if(!n->away && totalmillis-n->lastseen >= ircautoaway)
            {
                ircsend(n, "AWAY :Auto-away after %d second%s", ircautoaway, ircautoaway != 1 ? "s" : "");
                n->away = totalmillis;
            }
        }
    }
    #endif
    loopv(ircnets)
    {
        ircnet *n = ircnets[i];
        if(n->sock != ENET_SOCKET_NULL && n->state > IRC_DISC)
        {
            switch(n->state)
            {
                case IRC_WAIT:
                {
                    if(!n->lastattempt || clocktime-n->lastattempt >= irctimeout) ircdiscon(n);
                    break;
                }
                case IRC_ATTEMPT:
                {
                    if(*n->passkey) ircsend(n, "PASS %s", n->passkey);
                    copystring(n->nick, n->mnick);
                    ircsend(n, "NICK %s", n->mnick);
                    ircsend(n, "USER %s +iw %s :%s v%s-%s%d-%s (%s)", VERSION_UNAME, VERSION_UNAME, VERSION_NAME, VERSION_STRING, versionplatname, versionarch, versionbranch, VERSION_RELEASE);
                    n->lastnick = clocktime;
                    n->state = IRC_CONN;
                    loopvj(n->channels)
                    {
                        ircchan *c = &n->channels[j];
                        c->state = IRCC_NONE;
                        c->lastjoin = clocktime;
                        c->lastsync = 0;
                    }
                    break;
                }
                case IRC_ONLINE:
                {
                    loopvj(n->channels)
                    {
                        ircchan *c = &n->channels[j];
                        if(c->type == IRCCT_AUTO && c->state != IRCC_JOINED && (!c->lastjoin || clocktime-c->lastjoin >= (c->state != IRCC_BANNED ? 5 : ircautorejoin)))
                            ircjoin(n, c);
                    }
                    // fall through
                }
                case IRC_CONN:
                {
                    if(n->state == IRC_CONN && (!n->lastattempt || clocktime-n->lastattempt >= irctimeout))
                    {
                        ircdiscon(n, "connection attempt timed out");
                        break;
                    }
                    if(ircnickretry && clocktime-n->lastnick >= ircnickretry && strcmp(n->nick, n->mnick))
                    {
                        copystring(n->nick, n->mnick);
                        ircsend(n, "NICK %s", n->mnick);
                        n->lastnick = clocktime;
                    }
                    if(!n->lastactivity)
                    {
                        n->lastactivity = clocktime;
                        n->lastping = 0;
                    }
                    else if(clocktime-n->lastactivity >= ircpingpong)
                    {
                        if(!n->lastping)
                        {
                            ircsend(n, "PING %d", clocktime);
                            n->lastping = clocktime;
                        }
                        else if(clocktime-n->lastping >= 120) ircdiscon(n, "connection timed out");
                    }
                    break;
                }
                case IRC_QUIT: ircdiscon(n, "closing link"); break;
                default: ircdiscon(n, "encountered unknown connection state"); break;
            }
        }
        else if(!n->lastattempt || clocktime-n->lastattempt >= 60) ircestablish(n);
    }
}
#ifndef STANDALONE
void irccmd(ircnet *n, ircchan *c, char *s)
{
    char *p = s;
    if(*p == '/')
    {
        p++;
        const char *word = p;
        p += strcspn(p, " \n\0");
        if(p-word > 0)
        {
            char *q = newstring(word, p-word), *r = newstring(*p ? ++p : "");
            if(!strcasecmp(q, "ME"))
            {
                if(c)
                {
                    ircsend(n, "PRIVMSG %s :\vACTION %s\v", c->name, r);
                    ircprintf(n, 1, c->name, "\fv* %s %s", n->nick, r);
                }
                else ircprintf(n, 4, NULL, "\fyyou are not on a channel");
            }
            else if(!strcasecmp(q, "JOIN"))
            {
                ircchan *d = ircfindchan(n, r);
                if(d) ircjoin(n, d);
                else ircnewchan(IRCCT_AUTO, n->name, r);
            }
            else if(!strcasecmp(q, "PART"))
            {
                ircchan *d = ircfindchan(n, r);
                if(!d && c) d = c;
                if(d)
                {
                    ircsend(n, "PART %s", d->name);
                    d->updated |= IRCUP_LEAVE;
                }
                else ircprintf(n, 4, NULL, "\fyyou are not on a channel");
            }
            else if(!strcasecmp(q, "QUIT"))
            {
                ircsend(n, "QUIT :%s", r);
                n->updated |= IRCUP_LEAVE;
            }
            else if(!strcasecmp(q, "SYSINFO"))
            {
                if(c)
                {
                    ircsend(n, "PRIVMSG %s :%s v%s-%s%d-%s (%s); %s (%s v%s)", c->name, VERSION_NAME, VERSION_STRING, versionplatname, versionarch, versionbranch, VERSION_RELEASE, gfxrenderer, gfxvendor, gfxversion);
                    ircprintf(n, 1, c->name, "\fw<%s> %s v%s-%s%d-%s (%s); %s (%s v%s)", n->nick, VERSION_NAME, VERSION_STRING, versionplatname, versionarch, versionbranch, VERSION_RELEASE, gfxrenderer, gfxvendor, gfxversion);
                }
                else ircprintf(n, 4, NULL, "\fyyou are not on a channel");
            }
            else if(*r) ircsend(n, "%s %s", q, r); // send it raw so we support any command
            else ircsend(n, "%s", q);
            DELETEA(q); DELETEA(r);
            return;
        }
        ircprintf(n, 4, c ? c->name : NULL, "\fyyou are not on a channel");
    }
    else if(c)
    {
        ircsend(n, "PRIVMSG %s :%s", c->name, p);
        ircprintf(n, 1, c->name, "\fw<%s> %s", n->nick, p);
    }
    else
    {
        ircsend(n, "%s", p);
        ircprintf(n, 4, NULL, "\fa>%s< %s", n->nick, p);
    }
}

bool ircchangui(guient *g, ircnet *n, ircchan *c, bool tab)
{
    if(tab)
    {
        bool front = c->updated&IRCUP_NEW, msg = c->updated&IRCUP_MSG;
        g->tab(c->name, msg ? 0x00FFFF : 0xFFFFFF, front);
        if(front) c->updated &= ~IRCUP_NEW;
        if(msg && g->visible()) c->updated &= ~IRCUP_MSG;
    }

    defformatstring(cwindow, "%s_%s_window", n->name, c->name);
    if(c->buffer.newlines < c->buffer.lines.length())
    {
        editor *e = UI::geteditor(cwindow, EDITORREADONLY);
        if(e) while(c->buffer.newlines < c->buffer.lines.length()) UI::editorline(e, c->buffer.lines[c->buffer.newlines++], MAXIRCLINES);
    }
    g->field(cwindow, 0x666666, -100, 25, NULL, EDITORREADONLY);

    defformatstring(cinput, "%s_%s_input", n->name, c->name);
    char *v = g->field(cinput, 0x666666, -100, 0, "", EDITORFOREVER, g->visible(), cwindow);
    if(v && *v)
    {
        irccmd(n, c, v);
        UI::editoredit(UI::geteditor(cinput, EDITORFOREVER, NULL, cwindow));
    }
    return true;
}

bool ircnetgui(guient *g, ircnet *n, bool tab)
{
    if(tab)
    {
        bool front = n->updated&IRCUP_NEW, msg = n->updated&IRCUP_MSG;
        g->tab(n->name, msg ? 0x00FFFF : 0xFFFFFF, front);
        if(front) n->updated &= ~IRCUP_NEW;
        if(msg && g->visible()) n->updated &= ~IRCUP_MSG;
    }
    n->lastseen = clocktime;
    defformatstring(window, "%s_window", n->name);
    if(n->buffer.newlines < n->buffer.lines.length())
    {
        editor *e = UI::geteditor(window, EDITORREADONLY);
        if(e) while(n->buffer.newlines < n->buffer.lines.length()) UI::editorline(e, n->buffer.lines[n->buffer.newlines++], MAXIRCLINES);
    }
    g->field(window, 0x666666, -100, 25, NULL, EDITORREADONLY);

    defformatstring(input, "%s_input", n->name);
    char *w = g->field(input, 0x666666, -100, 0, "", EDITORFOREVER, g->visible(), window);
    if(w && *w)
    {
        irccmd(n, NULL, w);
        UI::editoredit(UI::geteditor(input, EDITORFOREVER, NULL, window));
    }

    loopvj(n->channels) if(n->channels[j].state != IRCC_NONE && n->channels[j].name[0])
    {
        ircchan *c = &n->channels[j];
        if(!ircchangui(g, n, c, true)) return false;
    }
    return true;
}

static const char * const ircstates[IRC_MAX] = { "\fowaiting", "\froffline", "\foconnecting", "\fynegotiating", "\fgonline", "\foquitting" };
bool ircgui(guient *g, const char *s)
{
    g->strut(94);
    if(s && *s)
    {
        ircnet *n = ircfind(s);
        if(n)
        {
            if(!ircnetgui(g, n, false)) return false;
        }
        else g->textf("not currently connected to %s", 0xFFFFFF, NULL, 0, -1, s);
    }
    else
    {
        int nets = 0;
        loopv(ircnets) if(ircnets[i]->name[0] && ircnets[i]->sock != ENET_SOCKET_NULL)
        {
            ircnet *n = ircnets[i];
            uilist(*g, {
                g->buttonf("%s via %s:[%d]", 0xFFFFFF, NULL, 0, -1, true, n->name, n->serv, n->port);
                g->space(1);
                g->buttonf("\fs%s\fS as %s", 0xFFFFFF, NULL, 0, -1, true, ircstates[n->state], n->nick);
            });
            nets++;
        }
        if(nets)
        {
            loopv(ircnets)
            {
                ircnet *n = ircnets[i];
                if(!ircnetgui(g, n, true)) return false;
            }
        }
        else g->text("no current connections..", 0xFFFFFF);
    }
    return true;
}

#endif
ICOMMAND(0, ircconns, "", (void), { int num = 0; loopv(ircnets) if(ircnets[i]->state >= IRC_ATTEMPT) num++; intret(num); });
