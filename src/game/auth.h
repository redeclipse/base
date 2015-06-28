// WARNING: Before modifying this file, please read our Guidelines
// This file can be found in the distribution under: ./docs/guidelines.txt
// Or at: http://redeclipse.net/wiki/Multiplayer_Guidelines
//
// The Red Eclipse Team provides the play.redeclipse.net master server for the
// benefit of the Red Eclipse Community. We impose a general set of guidelines
// for any server/user which connects to the play.redeclipse.net master server.
// The team reserves the right to block any attempt to connect to the master
// server at their discretion. Access to services provided by the project are
// considered to be a privilege, not a right.

// These guidelines are imposed to ensure the integrity of both the Red Eclipse
// game, and its community. If you do not agree to these terms, you should not
// connect to the play.redeclipse.net master server, or any servers which are
// connected to it. These guidelines are not designed to limit your opinion or
// freedoms granted to you by the open source licenses employed by the project,
// nor do they cover usage of the game in either offline play or on servers
// which are not connected to the Red Eclipse master.

// If you have questions or comments regarding these guidelines please contact
// the Red Eclipse Team. Any person seeking to modify their game or server for
// use on the master server should first seek permission from the Red Eclipse
// Team, each modification must be approved and will be done on a case-by-case
// basis.


void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen)
{
    char buf[2*MAXSTRLEN];
    formatstring(buf)("%d %d ", cn, sessionid);
    concatstring(buf, pwd, sizeof(buf));
    if(!hashstring(buf, result, maxlen)) *result = '\0';
}

bool checkpassword(clientinfo *ci, const char *wanted, const char *given)
{
    string hash;
    hashpassword(ci->clientnum, ci->sessionid, wanted, hash, MAXSTRLEN);
    return !strcmp(hash, given);
}

struct localop
{
    char *name, *flags;

    localop() : name(NULL), flags(NULL) {}
    localop(const char *n, const char *f) : name(newstring(n)), flags(newstring(f)) {}
    ~localop()
    {
        if(name) delete[] name;
        if(flags) delete[] flags;
    }
};
vector<localop> localops;

void localopreset()
{
    loopvrev(localops) localops.remove(i);
}
ICOMMAND(0, resetlocalop, "", (), localopreset());

void localopadd(const char *name, const char *flags)
{
    if(!name || !flags) return;
    loopv(localops) if(!strcmp(name, localops[i].name))
    {
        conoutf("local operator %s already exists with flags %s", localops[i].name, localops[i].flags);
        return;
    }
    localop &o = localops.add();
    o.name = newstring(name);
    o.flags = newstring(flags);
}
ICOMMAND(0, addlocalop, "ss", (char *n, char *f), localopadd(n, f));

VAR(IDF_PERSIST, quickauthchecks, 0, 0, 1);
namespace auth
{
    int lastconnect = 0, lastregister = 0, quickcheck = 0;
    uint nextauthreq = 1;

    clientinfo *findauth(uint id)
    {
        loopv(clients) if(clients[i]->authreq == id) return clients[i];
        loopv(connects) if(connects[i]->authreq == id) return connects[i];
        return NULL;
    }

    void reqauth(clientinfo *ci)
    {
        if(!nextauthreq) nextauthreq = 1;
        ci->authreq = nextauthreq++;
        if(!connectedmaster())
        {
            if(quickauthchecks)
            {
                if(!ci->connectauth)
                    srvmsgftforce(ci->clientnum, CON_EVENT, "\fyplease wait, connecting to master server for a quick match..");
                quickcheck = totalmillis ? totalmillis : 1;
            }
            else if(!ci->local) srvmsgftforce(ci->clientnum, CON_EVENT, "\founable to verify, not connected to master server");
            return;
        }
        if(!ci->connectauth)
            srvmsgftforce(ci->clientnum, CON_EVENT, "\fyplease wait, requesting credential match from master server..");
        requestmasterf("reqauth %u %s %s\n", ci->authreq, ci->authname, gethostname(ci->clientnum));
    }

    bool tryauth(clientinfo *ci, const char *authname = "")
    {
        if(!ci) return false;
        if(!connectedmaster() && !quickauthchecks)
        {
            if(!ci->local) srvmsgftforce(ci->clientnum, CON_EVENT, "\founable to verify, not connected to master server");
            return false;
        }
        else if(ci->authreq)
        {
            srvmsgftforce(ci->clientnum, CON_EVENT, "\foplease wait, still processing previous attempt..");
            return true;
        }
        copystring(ci->authname, authname);
        reqauth(ci);
        return true;
    }

    void setprivilege(clientinfo *ci, int val, int flags = 0, bool authed = false)
    {
        string msg = "";
        if(val > 0)
        {
            if((ci->privilege&PRIV_TYPE) >= (flags&PRIV_TYPE)) return;
            ci->privilege = flags;
            if(authed)
            {
                formatstring(msg)("\fy%s identified as \fs\fc%s\fS", colourname(ci), ci->authname);
                if((ci->privilege&PRIV_TYPE) > PRIV_PLAYER)
                {
                    defformatstring(msgx)(" (\fs\fc%s\fS)", privname(ci->privilege));
                    concatstring(msg, msgx);
                }
                copystring(ci->handle, ci->authname);
            }
            else formatstring(msg)("\fy%s elevated to \fs\fc%s\fS", colourname(ci), privname(ci->privilege));
        }
        else
        {
            if(!(ci->privilege&PRIV_TYPE)) return;
            int privilege = ci->privilege;
            ci->privilege = PRIV_NONE;
            ci->handle[0] = 0;
            int others = 0;
            loopv(clients) if((clients[i]->privilege&PRIV_TYPE) >= PRIV_MODERATOR || clients[i]->local) others++;
            if(!others) mastermode = MM_OPEN;
            if(!val && (privilege&PRIV_TYPE) >= PRIV_ELEVATED)
                formatstring(msg)("\fy%s relinquished \fs\fc%s\fS status", colourname(ci), privname(privilege));
        }
        if(val >= 0)
        {
            if(*msg)
            {
                if(ci->connected) srvoutforce(ci, -2, "%s", msg);
                else sendf(ci->clientnum, 1, "ri2s", N_SERVMSG, CON_EVENT, msg);
            }
            sendf(ci->connected ? -1 : ci->clientnum, 1, "ri3s", N_CURRENTPRIV, ci->clientnum, ci->privilege, ci->handle);
        }
        if(paused)
        {
            int others = 0;
            loopv(clients) if((clients[i]->privilege&PRIV_TYPE) >= PRIV_ADMINISTRATOR || clients[i]->local) others++;
            if(!others) setpause(false);
        }
    }

    bool tryident(clientinfo *ci, const char *authname = "", const char *pwd = "")
    {
        if(*authname)
        {
            if(ci->connectauth) return true;
            if(tryauth(ci, authname))
            {
                ci->connectauth = true;
                return true;
            }
        }
        if(*pwd)
        {
            if(adminpass[0] && checkpassword(ci, adminpass, pwd))
            {
                if(G(autoadmin) || G(connectlock)) setprivilege(ci, 1, PRIV_ADMINISTRATOR|PRIV_LOCAL);
                return true;
            }
            if(serverpass[0] && checkpassword(ci, serverpass, pwd)) return true;
        }
        int minpriv = max(int(PRIV_ELEVATED), G(connectlock));
        if(haspriv(ci, minpriv)) return true;
        return false;
    }

    int allowconnect(clientinfo *ci, const char *authname = "", const char *pwd = "")
    {
        if(ci->local) { tryident(ci, authname, pwd); return DISC_NONE; }
        if(ci->state.version.game != VERSION_GAME) return DISC_INCOMPATIBLE;
        if(m_local(gamemode)) return DISC_PRIVATE;
        if(tryident(ci, authname, pwd)) return DISC_NONE;
        // above here are short circuits
        if(numclients() >= G(serverclients)) return DISC_MAXCLIENTS;
        uint ip = getclientip(ci->clientnum);
        if(!ip || !checkipinfo(control, ipinfo::EXCEPT, ip))
        {
            if(mastermode >= MM_PRIVATE || serverpass[0] || (G(connectlock) && !haspriv(ci, G(connectlock)))) return DISC_PASSWORD;
            ipinfo *info = checkipinfo(control, ipinfo::BAN, ip);
            if(info)
            {
                srvmsgftforce(ci->clientnum, CON_EVENT, "\foyou are \fs\fcbanned\fS: \fy%s", info->reason && *info->reason ? info->reason : "no reason specified");
                return DISC_IPBAN;
            }
        }
        return DISC_NONE;
    }

    void authfailed(clientinfo *ci)
    {
        if(!ci) return;
        ci->authreq = ci->authname[0] = ci->handle[0] = 0;
        srvmsgftforce(ci->clientnum, CON_EVENT, "\foidentity verification failed, please check your credentials");
        if(ci->connectauth)
        {
            ci->connectauth = false;
            int disc = allowconnect(ci);
            if(disc) { disconnect_client(ci->clientnum, disc); return; }
            connected(ci);
        }
    }

    void authfailed(uint id)
    {
        authfailed(findauth(id));
    }

    void authsucceeded(uint id, const char *name, const char *flags)
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        ci->authreq = 0;
        int n = -1;
        for(const char *c = flags; *c; c++) switch(*c)
        {
            case 'c': case 'C': n = PRIV_CREATOR; break;
            case 'd': case 'D': n = PRIV_DEVELOPER; break;
            case 'a': case 'A': n = PRIV_ADMINISTRATOR; break;
            case 'o': case 'O': n = PRIV_OPERATOR; break;
            case 'm': case 'M': n = PRIV_MODERATOR; break;
            case 's': case 'S': n = PRIV_SUPPORTER; break;
            case 'u': case 'U': n = PRIV_PLAYER; break;
            default: break;
        }
        bool local = false;
        loopv(localops) if(!strcmp(localops[i].name, name))
        {
            int o = -1;
            for(const char *c = localops[i].flags; *c; c++) switch(*c)
            {
                case 'a': case 'A': o = PRIV_ADMINISTRATOR; break;
                case 'o': case 'O': o = PRIV_OPERATOR; break;
                case 'm': case 'M': o = PRIV_MODERATOR; break;
                case 's': case 'S': o = PRIV_SUPPORTER; break;
                default: break;
            }
            if(o > n)
            {
                n = o;
                local = true;
            }
        }
        if(n > PRIV_NONE) setprivilege(ci, 1, n|(local ? PRIV_LOCAL : 0), true);
        else ci->authname[0] = ci->handle[0] = 0;
        if(ci->connectauth)
        {
            ci->connectauth = false;
            int disc = allowconnect(ci);
            if(disc) { disconnect_client(ci->clientnum, disc); return; }
            connected(ci);
        }
    }

    void authchallenged(uint id, const char *val)
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        sendf(ci->clientnum, 1, "riis", N_AUTHCHAL, id, val);
    }

    bool answerchallenge(clientinfo *ci, uint id, char *val)
    {
        if(ci->authreq != id) return false;
        for(char *s = val; *s; s++)
        {
            if(!isxdigit(*s)) { *s = '\0'; break; }
        }
        //srvmsgftforce(ci->clientnum, CON_EVENT, "\fyconfirming identity with master server..");
        requestmasterf("confauth %u %s\n", id, val);
        return true;
    }

    void processinput(const char *p)
    {
        const int MAXWORDS = 8;
        char *w[MAXWORDS];
        int numargs = MAXWORDS;
        loopi(MAXWORDS)
        {
            w[i] = (char *)"";
            if(i > numargs) continue;
            char *s = parsetext(p);
            if(s) w[i] = s;
            else numargs = i;
        }
        if(!strcmp(w[0], "error")) conoutf("master server error: %s", w[1]);
        else if(!strcmp(w[0], "echo")) { conoutf("master server reply: %s", w[1]); }
        else if(!strcmp(w[0], "failauth")) authfailed((uint)(atoi(w[1])));
        else if(!strcmp(w[0], "succauth")) authsucceeded((uint)(atoi(w[1])), w[2], w[3]);
        else if(!strcmp(w[0], "chalauth")) authchallenged((uint)(atoi(w[1])), w[2]);
        else if(!strcmp(w[0], "sync"))
        {
            int oldversion = versioning;
            versioning = 2;
            if(servcmd(2, w[1], w[2])) conoutf("master server variable synced: %s", w[1]);
            versioning = oldversion;
        }
        else loopj(ipinfo::SYNCTYPES) if(!strcmp(w[0], ipinfotypes[j]))
        {
            ipinfo &c = control.add();
            c.ip = uint(atoi(w[1]));
            c.mask = uint(atoi(w[2]));
            c.type = j;
            c.flag = ipinfo::GLOBAL; // master info
            c.time = totalmillis ? totalmillis : 1;
            if(w[3] && *w[3]) c.reason = newstring(w[3]);
            updatecontrols = true;
            break;
        }
        loopj(numargs) if(w[j]) delete[] w[j];
    }

    void regserver()
    {
        loopvrev(control) if(control[i].flag == ipinfo::GLOBAL) control.remove(i);
        lastregister = totalmillis ? totalmillis : 1;
        if(quickcheck)
        {
            quickcheck = lastregister;
            requestmasterf("quick\n");
        }
        else
        {
            conoutf("updating master server");
            requestmasterf("server %d %s %d\n", serverport, *serverip ? serverip : "*", CUR_VERSION);
        }
    }

    void update()
    {
        if(servertype < 2 && (!quickcheck || totalmillis-quickcheck >= 60*1000))
        {
            if(connectedmaster()) disconnectmaster();
            return;
        }
        if(connectedmaster())
        {
            if(!quickcheck && totalmillis-lastregister >= G(masterinterval)) regserver();
        }
        else if(!lastconnect || totalmillis-lastconnect >= 60*1000)
        {
            lastconnect = totalmillis ? totalmillis : 1;
            if(connectmaster() == ENET_SOCKET_NULL) return;
        }
    }

    void masterconnected()
    {
        regserver();
        loopv(clients) if(clients[i]->authreq) reqauth(clients[i]);
        loopv(connects) if(connects[i]->authreq) reqauth(connects[i]);
    }

    void masterdisconnected()
    {
        quickcheck = 0;
        loopv(clients) if(clients[i]->authreq) authfailed(clients[i]);
        loopv(connects) if(connects[i]->authreq) authfailed(connects[i]);
    }
}

void processmasterinput(const char *cmd, int cmdlen, const char *args)
{
    auth::processinput(cmd);
}

void masterconnected()
{
    auth::masterconnected();
}

void masterdisconnected()
{
    auth::masterdisconnected();
}

