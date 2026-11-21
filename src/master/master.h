#ifndef MASTER_H
#define MASTER_H

// Master server internal header, shared by the units in src/master/

#define MASTER_LIMIT 4096
#define CLIENT_TIME (60*1000)
#define SERVER_TIME (35*60*1000)
#define AUTH_TIME (30*1000)
#define AUTHRATE_TIME (60*1000) // the window challenges and wrong answers are counted over
#define AUTHRATE_MAX 4096 // addresses counted at once, the oldest going first
#define STATUS_LIMIT 65536
#define OUTPUT_LIMIT (1024*1024) // the most a peer may have queued to it before it is dropped
#define MASTER_VERIFIED 256 // handles a server is believed about
#define DISPLAY_HOURLY 3600 // seconds between an owner's display name changes
#define LISTPORT_MAX 65534 // the highest port a server may list on, as it is pinged on the next one

#define SHIELD_NAMELEN 64
#define SHIELD_BODYLEN 16384
#define SHIELD_MODULATORS 64

#define SIGNIN_PATH HTTP_API "/signin"
#define SIGNIN_DAY 86400

#define MAIL_MAXADDR 254

#define ADMIN_PATH SIGNIN_PATH "/admin"
#define ACCOUNT_PATH HTTP_API "/accounts"

namespace master
{
    enum { NOTICE_INFO = 0, NOTICE_WARNING, NOTICE_ERROR, NOTICE_LEVELS };
    enum { MFILE_USER = 0, MFILE_SERV, MFILE_CTRL, MFILE_SHIELD, MFILE_MAX };
    enum { SHIELD_NONE = 0, SHIELD_BAN, SHIELD_FLAG, SHIELD_BAD };
    enum { TICKET_REGISTER = 0, TICKET_RESET, TICKET_EMAIL, TICKET_SERVER, TICKET_MAX };

    enum
    {
        STATUS_INC_MAP    = 1 << 0,
        STATUS_INC_VARS   = 1 << 1,
        STATUS_INC_CONFIG = 1 << 2,
        STATUS_INC_IPS    = 1 << 3,
        STATUS_INC_PUBLIC = STATUS_INC_MAP | STATUS_INC_VARS | STATUS_INC_CONFIG,
        STATUS_INC_ALL    = STATUS_INC_PUBLIC | STATUS_INC_IPS
    };

    struct authuser
    {
        char *name, *flags, *email, *desc, *owner, *steamid, *key, *discord, *display;
        char *addrs; // a server account's known addresses, "host:lastseen" with the latest first; NULL until they're kept for it
        void *pubkey;
        time_t displayed; // when its owner last changed its display name, not kept

        authuser() : name(NULL), flags(NULL), email(NULL), desc(NULL), owner(NULL), steamid(NULL), key(NULL), discord(NULL), display(NULL), addrs(NULL), pubkey(NULL), displayed(0) {}
    };

    struct authreq
    {
        enet_uint32 reqtime;
        uint id;
        void *answer;
        string name, key, hostname; // a rehash frees the accounts, so the answer finds its account again by these

        authreq() { reset(); }
        ~authreq() {}

        void reset()
        {
            reqtime = 0;
            id = 0;
            answer = NULL;
            name[0] = key[0] = hostname[0] = '\0';
        }
    };

    struct masterclient
    {
        ENetAddress address;
        ENetSocket socket;
        string name, desc, flags, authhandle, branch;
        string authkey; // the server account's key it signed in with, so one given a new key since loses its standing
        string lastmap; // the map it last reported, so only changes are logged
        int lastmode, lastmuts; // its mode and mutators as last reported, for Master shield
        vector<char *> lastnames; // its players' names, likewise
        vector<char *> lastflags; // what Master shield said of it since its state last changed, so a rehash doesn't say it again
        bool changed; // something Master shield looks at changed since its modulators last ran on it

        char input[4096];
        vector<char> output;
        int inputpos, outputpos, port, numpings, lastcontrol, version, link, webport, relay, bridge; // bridge: -1 when the server doesn't say
        bool linksecure, vouch; // vouch: it takes Sessions the master vouches for, from remembered sign-ins
        string fingerprint;
        char *status; // the last status it reported over the link, as public JSON
        char *fullstatus; // the full status it reported over the link, including extra/private groups for Master shield and privileged callers
        char *polled; // what its query reply last said, as JSON, while it reports no status
        uint lastshieldhash; // hash of fullstatus excluding clock-drift fields, so Master shield re-runs on any real change
        int connected; // unix time when this server registered on the master
        vector<int> mapchanges, authfails; // timestamps within the last hour, for Master shield
        uchar pollreq[8];
        int pollreqlen, pollmisses, polledends, polledstateends, polledstate, numplayers; // numplayers: from its status or poll, -1 when neither says
        int statusmax, statusmode, statusver, statusbuild[3]; // from its reported status, for the Matchmaking queue: room, master mode, protocol and version
        int statusslots, statusplaying, statusreserved, statusconnections; // the same: player slots (0 from servers too old to say), those playing, slots it keeps for pops, and how many more can connect (-1 unknown)
        int pollplayers, pollmax, pollmode; // what its last query reply said: players, player slots and master mode, which the master saw itself
        vector<char *> queuehandles; // the Accounts on it, by its status, that the master signed in there
        enet_uint32 lastping, lastpong, lastactivity, lastpoll, lastpolled; // lastpolled: when a query reply last came, 0 for none
        vector<authreq> authreqs;
        authreq serverauthreq;
        vector<char *> verified; // handles the master told it were real, the only ones whose names it believes
        bool isserver, isquick, ishttp, listserver, shouldping, shouldpurge, pollwaiting, outputfull, replaced; // outputfull: it passed OUTPUT_LIMIT, replaced: a newer server connection took its port; either is dropped

        masterclient() : link(-1), status(NULL), fullstatus(NULL), polled(NULL) { reset(); }
        ~masterclient()
        {
            DELETEA(status);
            DELETEA(fullstatus);
            DELETEA(polled);
            loopv(authreqs) if(authreqs[i].answer) freechallenge(authreqs[i].answer);
            if(serverauthreq.answer) freechallenge(serverauthreq.answer);
            verified.deletearrays();
            queuehandles.deletearrays();
            lastnames.deletearrays();
            lastflags.deletearrays();
        }

        void reset()
        {
            name[0] = desc[0] = flags[0] = authhandle[0] = authkey[0] = branch[0] = lastmap[0] = '\0';
            lastmode = lastmuts = 0;
            lastnames.deletearrays();
            lastflags.deletearrays();
            changed = false;
            lastshieldhash = 0;
            connected = 0;
            mapchanges.shrink(0);
            authfails.shrink(0);
            DELETEA(fullstatus);
            lastcontrol = -1;
            inputpos = outputpos = numpings = version = webport = relay = 0;
            bridge = -1;
            linksecure = vouch = false;
            fingerprint[0] = '\0';
            lastping = lastpong = lastactivity = lastpoll = lastpolled = 0;
            pollreqlen = pollmisses = polledends = polledstateends = polledstate = 0;
            numplayers = -1;
            statusmax = statusmode = statusver = 0;
            statusslots = statusplaying = statusreserved = 0;
            statusconnections = -1;
            pollplayers = pollmax = 0;
            pollmode = -1;
            statusbuild[0] = statusbuild[1] = statusbuild[2] = 0;
            queuehandles.deletearrays();
            isserver = isquick = ishttp = listserver = shouldping = shouldpurge = pollwaiting = outputfull = replaced = false;
            port = MASTER_PORT;
            output.shrink(0);
            authreqs.shrink(0);
            verified.deletearrays();
        }

        bool reports() { return link >= 0 && status; }

        // queried even while it reports, when it has no server account, as only then can what it says be checked
        bool shouldpoll() { return !reports() || priority() <= 0; }

        bool hasflag(char f)
        {
            if(f == 'b' && *flags) return true;
            return ::hasflag(flags, f);
        }

        int priority()
        {
            if(!isserver || !listserver) return -1;
            if(!*authhandle || !*flags) return 0;
            int ret = 1;
            for(const char *c = flags; *c; c++) switch(*c)
            {
                case 'm': ret = max(ret, 4); break;
                case 'u': ret = max(ret, 3); break;
                case 'b': ret = max(ret, 2); break;
                default: break;
            }
            return ret;
        }
    };

    // restored, it came from its .bak; blocked, neither would load, so it isn't saved
    struct masterfileguard
    {
        bool loaded, restored, blocked, warned;
        masterfileguard() : loaded(false), restored(false), blocked(false), warned(false) {}
    };

    struct knownaddr
    {
        string host;
        time_t seen;
    };

    struct shieldmodulator
    {
        int id; // for the website's routes, not kept, as the name can change
        string name, link; // link: the GitHub issue or security advisory it answers, if any
        char *body;
        bool enabled;
        vector<int> bans; // when it banned, within the last hour

        shieldmodulator() : id(0), body(NULL), enabled(true) { name[0] = link[0] = '\0'; }
        ~shieldmodulator() { DELETEA(body); }
    };

    struct shieldstate
    {
        vector<shieldmodulator *> modulators, old; // old: those before a load, whose recent bans carry over to the same names
        vector<int> bans; // when any modulator banned, within the last hour
        vector<char *> loaded; // the names after the last load, so a change can be told
        bool rescan;
        int passbans, lastlapse, lastid;

        shieldstate() : rescan(false), passbans(0), lastlapse(0), lastid(0) {}

        shieldmodulator *find(const char *name, shieldmodulator *other = NULL)
        {
            loopv(modulators) if(modulators[i] != other && !strcasecmp(modulators[i]->name, name)) return modulators[i];
            return NULL;
        }
        shieldmodulator *find(int id)
        {
            loopv(modulators) if(modulators[i]->id == id) return modulators[i];
            return NULL;
        }
    };

    struct signin
    {
        string id, hash, name, key, flags, label, firstaddr, lastaddr; // hash: of the token, which is never kept
        time_t created, lastused, expires;
        bool remember;
    };

    // the master's secrets, which only commands set so no script can read them back
    struct mastersecretstate
    {
        string smtppass, turnstile, discord;

        mastersecretstate() { smtppass[0] = turnstile[0] = discord[0] = '\0'; }
    };

    // something waiting on its owner: a registration until it's activated, then until the game that asked collects the key;
    // for a server account, its handle, the owner's handle, its level (or none for a new key for one that's there) and its description
    struct ticket
    {
        int kind, collected;
        bool done; // activated, and now only waiting for the game
        string handle, email, hash, cancelhash, pickuphash, addr, privkey, owner, level, desc;
        time_t created, expires;

        ticket() : kind(TICKET_REGISTER), collected(0), done(false), created(0), expires(0)
        {
            handle[0] = email[0] = hash[0] = cancelhash[0] = pickuphash[0] = addr[0] = privkey[0] = owner[0] = level[0] = desc[0] = '\0';
        }
        ~ticket() { memset(privkey, 0, sizeof(privkey)); }
    };

    // master.cpp
    extern int masterserver, masterport, mastermainver, mastergamever;
    extern char *masterip, *masterscriptclient;
    extern vector<masterclient *> masterclients;
    extern hashnameset<authuser> authusers, serverauthusers;
    extern mastersecretstate mastersecrets;

    extern void masteroutf(masterclient &c, const char *fmt, ...) PRINTFARGS(2, 3);
    extern const char *noticedate(time_t t);
    extern const char *accountlinks(authuser &u);
    extern const char *serverdetails(masterclient &c);
    extern bool validpubkey(const char *key);
    extern bool validsteamid(const char *s);
    extern void displayfromutf8(char *dst, const char *src);
    extern void descfromutf8(char *dst, const char *src, size_t maxlen = MAXSTRLEN-1);
    extern char userflag(const char *flags);
    extern char serverflag(const char *flags);
    extern bool newauth(const char *name, const char *flags, const char *pubkey, const char *email, const char *steamid = NULL, const char *discord = NULL, const char *display = NULL);
    extern bool newserverauth(const char *name, const char *flags, const char *pubkey, const char *desc = NULL, const char *owner = NULL, const char *addrs = NULL);
    extern void freeauth(authuser &u);
    extern void purgemasterclient(int n);
    extern void checkserverauth(const char *name = NULL);
    extern void readknownaddrs(const char *s, vector<knownaddr> &out);
    extern const char *knownlast(authuser &u);
    extern void noteserverstate(masterclient &c, const char *map, int mode, int muts, const vector<const char *> &names);
    extern masterclient *findlink(int id);
    extern bool pollrecent(masterclient &c);

    // files.cpp
    extern char *mastershieldfile;
    extern void dirtymasterfile(int type);
    extern bool parsecontroladdr(const char *s, enet_uint32 &ip, enet_uint32 &mask);
    extern void controladdr(const ipinfo &c, string &addr);
    extern const char * const controlsources[];
    extern bool masterfileblocked(int type);
    extern void setmasterfilesloading(bool loading);
    extern stream *openmasterfile(const char *name, bool gz);
    extern int masterfileolder(stream *f, const char *name);
    extern int masterfilemissing(const char *name);
    extern bool loadguarded(const char *name, int (*load)(const char *, int), int arg, masterfileguard &g, const char *what);
    extern bool saveblocked(masterfileguard &g, const char *name);
    extern void commitmasterfile(const char *name, masterfileguard &g, bool dated);
    extern void savemasterfiles();

    // relay.cpp
    extern uint listhash(const vector<char> &buf);
    extern void writestatusfiltered(jsonwriter &w, jsonvalue *st, int inc, bool hashmode = false);
    extern void routeservers(httprequest &req, httpresponse &res);
    extern void routehistory(httprequest &req, httpresponse &res);
    extern void routeglobalhistory(httprequest &req, httpresponse &res);
    extern void routejoin(httprequest &req, httpresponse &res);
    extern void loadhistory();
    extern void savehistory();
    extern void viewerevent(httpwsevent &e, void *ctx);
    extern masterclient *findrelay(httprequest &req);
    extern void relayresubscribe(masterclient &c);
    extern void relayroute(httprequest &req, httpresponse &res);
    extern void relayviewerevent(httpwsevent &e, void *ctx);
    extern void relayjson(masterclient &c, const char *data, int len, const char *body = NULL, int bodylen = 0);
    extern void relayclosed(masterclient &c);
    extern void checkmasterlinks();

    // queue.cpp
    extern const char *serverchannelname(masterclient &s);
    extern void routequeue(httprequest &req, httpresponse &res);
    extern void loadqueue();
    extern void savequeue();
    extern void checkqueue();
    extern void queueviewerjoined(httpwsevent &e);
    extern void queueviewermessage(httpwsevent &e);
    extern void queueviewerclosed(int id);
    extern void queuehandles(masterclient &c, jsonvalue *players);
    extern void broadcastnotice(int minpriv, const char *buf, int len);

    // shield.cpp
    extern int mastershieldlock;
    extern const char * const shieldsettings[];
    extern const int numshieldsettings;
    extern shieldstate shield;
    extern void noticeviewerjoined(httpwsevent &e);
    extern void postnotice(const char *kind, int level, const char *text);
    extern void noticef(const char *kind, int level, const char *fmt, ...) PRINTFARGS(3, 4);
    extern void foldnotice(const char *kind, int level, const char *key, const char *text);
    extern void foldnoticef(const char *kind, int level, const char *key, const char *fmt, ...) PRINTFARGS(4, 5);
    extern void checknotices();
    extern void checkshield();
    extern void noteauthfail(masterclient &c);
    extern void notereconnect(enet_uint32 host);
    extern void marksiblingschanged(enet_uint32 host, const char *handle);
    extern bool shieldname(char *dst, const char *name);
    extern bool shieldlink(const char *s);
    extern shieldmodulator *addshieldmodulator(const char *name, const char *link, bool enabled, const char *body);
    extern bool shieldfileline(vector<char *> &args, bool apply);
    extern void writeshield(stream *f);
    extern void shieldloading();
    extern void shieldloaded();
    extern int shieldrun(const char *code, masterclient &c, char *out);

    // signin.cpp
    extern bool signindirty, signincheck;
    extern void hexbytes(char *dst, const uchar *src, int len);
    extern void hashtoken(char *dst, const char *token);
    extern bool signinready(httprequest &req, httpresponse &res);
    extern signin *findsignin(httprequest &req, bool &tried);
    extern bool signinrefused(httprequest &req, httpresponse &res, signin *s, bool tried);
    extern int requestpriv(httprequest &req);
    extern jsonvalue *signinbody(httprequest &req);
    extern const char *signinfield(httprequest &req, jsonvalue *v, const char *name);
    extern bool signinflag(httprequest &req, jsonvalue *v, const char *name, bool def = false);
    extern void signinfailed(uint ip);
    extern void savesignins();
    extern void checksignins();
    extern void signinvouched(masterclient &c, uint id, const char *token, const char *priv, bool local, int expires, const char *err);
    extern void signinlinkclosed(masterclient &c);
    extern void setupsignin();

    // mail.cpp
    extern bool validemail(const char *s);
    extern bool queuemaildef(const char *name, const char *to, const char *subject, const char *text, const tagval *args = NULL, int numargs = 0);
    extern void checkmail();
    extern void cleanupmail();

    // admin.cpp
    extern int accountpriv(const char *flags);
    extern bool removeauth(hashnameset<authuser> &users, const char *name);
    extern void setdisplay(authuser &u, const char *raw);
    extern void setupadmin();

    // accounts.cpp
    extern char *masteraccounturl;
    extern bool parsehandle(const char *s, char *out);
    extern void dropticket(ticket *t);
    extern ticket *serverticket(const char *handle);
    extern ticket *serverlink(const char *handle, const char *owner, const char *email, const char *level, const char *desc, const char *by);
    extern void writependingserver(jsonwriter &w, ticket &t);
    extern vector<ticket *> &accounttickets();
    extern void dirtyaccounts();
    extern signin *ownersignin(httprequest &req, httpresponse &res, authuser *&u);
    extern void writeowner(jsonwriter &w, authuser &u);
    extern void fillname(masterclient &c, const char *handle, const char *shown);
    extern void fillnames(masterclient &c, jsonvalue *players);
    extern void checktickets();
    extern void cleanupaccounts();
    extern void setupowner();
    extern void setupaccounts();

    // link.cpp
    extern authuser *discordaccount(const char *id);
    extern void cleanuplinks();
    extern void setuplinks();
    extern void routediscordlookup(httprequest &req, httpresponse &res);
}

#endif
