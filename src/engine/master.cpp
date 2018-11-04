#ifdef WIN32
#define FD_SETSIZE 4096
#else
#include <sys/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 4096
#endif

#include "engine.h"
#include <enet/time.h>
#include <sqlite3.h>

#define STATSDB_VERSION 7
#define STATSDB_RETRYTIME (5*1000)
// last protocol version in which modes/muts were changed
#define STATSDB_VERSION_GAME 230
// redefining some game.h constants
#define G_MAX 7
#define G_M_GSP 15
#define G_M_NUM 18

#define MASTER_LIMIT 4096
#define CLIENT_TIME (60*1000)
#define SERVER_TIME (35*60*1000)
#define AUTH_TIME (30*1000)

VAR(0, masterserver, 0, 0, 1);
VAR(0, masterport, 1, MASTER_PORT, VAR_MAX);
VAR(0, masterminver, 0, 0, CUR_VERSION);

VAR(0, mastermainver, 0, 0, CUR_VERSION);
VAR(0, mastergamever, 0, 0, VAR_MAX);

SVAR(0, masterip, "");
SVAR(0, masterscriptclient, "");
SVAR(0, masterscriptserver, "");

VAR(0, masterduplimit, 0, 3, VAR_MAX);
VAR(0, masterpingdelay, 1000, 3000, VAR_MAX);
VAR(0, masterpingtries, 1, 5, VAR_MAX);

VAR(0, masterstatsavgposlastgames, 0, 50, VAR_MAX);
VAR(0, masterstatavgposnumplayers, 0, 4, VAR_MAX);
FVAR(0, masterstatsavgposdefault, 0, 0.25, FVAR_MAX);

struct authuser
{
    char *name, *flags, *email, *steamid;
    void *pubkey;
};

struct authreq
{
    enet_uint32 reqtime;
    uint id;
    void *answer;
    authuser *user;
    string hostname;

    authreq() { reset(); }
    ~authreq() {}

    void reset()
    {
        reqtime = 0;
        id = 0;
        answer = NULL;
        user = NULL;
        hostname[0] = '\0';
    }
};

typedef uchar statstring[MAXSTRLEN];

struct masterclient
{
    ENetAddress address;
    ENetSocket socket;
    string name, desc, flags, authhandle, branch;

    char input[4096];
    vector<char> output;
    int inputpos, outputpos, port, numpings, lastcontrol, version;
    enet_uint32 lastping, lastpong, lastactivity, laststats;
    vector<authreq> authreqs;
    authreq serverauthreq;
    bool isserver, isquick, ishttp, listserver, shouldping, shouldpurge, sendstats, instats, wantstats;

    struct statstate
    {
        ulong id;
        statstring map;
        int mode, mutators, timeplayed, normalweapons;
        time_t time;

        statstring desc, version;
        int port;

        int unique;

        struct team
        {
            int index, score;
            statstring name;

            team() { reset(); }
            ~team() {}

            void reset()
            {
                index = score = 0;
                name[0] = '\0';
            }
        };
        vector<team> teams;

        struct player
        {
            statstring name, handle;
            int score, timealive, frags, deaths, wid, timeactive;

            player() { reset(); }
            ~player() {}

            void reset()
            {
                name[0] = handle[0] = '\0';
                score = timealive = frags = deaths = wid = timeactive = 0;
            }
        };
        vector<player> players;

        struct weaponstats
        {
            statstring name;
            int playerid;
            statstring playerhandle;
            int timewielded, timeloadout;
            int hits1, hits2, flakhits1, flakhits2;
            int shots1, shots2, flakshots1, flakshots2;
            int frags1, frags2, damage1, damage2;

            weaponstats() { reset(); }
            ~weaponstats() {}

            void reset()
            {
                name[0] = playerhandle[0] = '\0';
                playerid = -1;
                timewielded = timeloadout = 0;
                hits1 = hits2 = flakhits1 = flakhits2 = 0;
                shots1 = shots2 = flakshots1 = flakshots2 = 0;
                frags1 = frags2 = damage1 = damage2 = 0;
            }
        };
        vector<weaponstats> weapstats;

        struct capturestats
        {
            statstring playerhandle;
            int playerid;
            int capturing, captured;

            capturestats() { reset(); }
            ~capturestats() {}

            void reset()
            {
                playerhandle[0] = '\0';
                playerid = -1;
                capturing = captured = 0;
            }
        };
        vector<capturestats> captures;

        struct bombstats
        {
            statstring playerhandle;
            int playerid;
            int bombing, bombed;

            bombstats() { reset(); }
            ~bombstats() {}

            void reset()
            {
                playerhandle[0] = '\0';
                playerid = -1;
                bombing = bombed = 0;
            }
        };
        vector<bombstats> bombings;

        struct ffaroundstats
        {
            statstring playerhandle;
            int playerid;
            int round;
            bool winner;

            ffaroundstats() { reset(); }
            ~ffaroundstats() {}

            void reset()
            {
                playerhandle[0] = '\0';
                playerid = -1;
                round = 0;
                winner = false;
            }
        };
        vector<ffaroundstats> ffarounds;

        statstate() { reset(); }
        ~statstate() {}

        void reset()
        {
            id = 0;
            map[0] = desc[0] = version[0] = '\0';
            mode = mutators = -1;
            timeplayed = 0;
            unique = time = normalweapons = 0;
            port = 0;
            teams.shrink(0);
            players.shrink(0);
            weapstats.shrink(0);
            captures.shrink(0);
            bombings.shrink(0);
            ffarounds.shrink(0);
        }
    } stats;

    masterclient() { reset(); }
    ~masterclient() {}

    void reset()
    {
        name[0] = desc[0] = flags[0] = authhandle[0] = branch[0] = '\0';
        lastcontrol = -1;
        inputpos = outputpos = numpings = version = 0;
        lastping = lastpong = lastactivity = laststats = 0;
        isserver = isquick = ishttp = listserver = shouldping = shouldpurge = sendstats = instats = wantstats = false;
        port = MASTER_PORT;
        output.shrink(0);
        authreqs.shrink(0);
    }

    bool hasflag(char f)
    {
        if(f == 'b' && *flags) return true;
        for(const char *c = flags; *c; c++) if(*c == f)
            return true;
        return false;
    }

    int priority()
    {
        if(!isserver || !listserver) return -1;
        if(!*authhandle || !*flags) return 0;
        int ret = 1;
        for(const char *c = flags; *c; c++) switch(*c)
        {
            case 'm': ret = max(ret, 4); break;
            case 's': case 'u': ret = max(ret, sendstats ? 3 : 2); break;
            case 'b': ret = max(ret, 2); break;
            default: break;
        }
        return ret;
    }
};

static vector<masterclient *> masterclients;
static ENetSocket mastersocket = ENET_SOCKET_NULL, pingsocket = ENET_SOCKET_NULL;
static time_t starttime;
static sqlite3 *statsdb = NULL;

void statsdb_close()
{
    if(statsdb)
    {
        sqlite3_close(statsdb);
        statsdb = NULL;
    }
}

void statsdb_rollback()
{
    sqlite3_exec(statsdb, "ROLLBACK", 0, 0, NULL);
}

void statsdb_warn(const char *fmt, ...)
{
    defvformatbigstring(errmsg, fmt, fmt);
    if(logfile) logoutf("statistics database error: %s", errmsg);
    #ifndef WIN32
    fprintf(stderr, "statistics database error: %s\n", errmsg);
    #endif
}

inline void statsdb_warn()
{
    statsdb_warn(sqlite3_errmsg(statsdb));
}

void statsdb_die()
{
    statsdb_close();
    fatal("statistics database error");
}

int statsdb_execv(const char *fmt, va_list args)
{
    char *sql = sqlite3_vmprintf(fmt, args);
    int rc = sqlite3_exec(statsdb, sql, 0, 0, NULL);
    sqlite3_free(sql);
    return rc;
}

int statsdb_exec(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int rc = statsdb_execv(fmt, args);
    va_end(args);
    return rc;
}

inline void statsdb_do_or_die(int rc)
{
    if(rc == SQLITE_OK) return;
    statsdb_warn();
    statsdb_die();
}

void statsdb_exec_or_die(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int rc = statsdb_execv(fmt, args);
    va_end(args);
    statsdb_do_or_die(rc);
}

void statsdb_exec_file_or_die(const char *path)
{
    char *buf = loadfile(path, NULL);
    if(!buf)
    {
        statsdb_warn("cannot find %s", path);
        statsdb_die();
    }
    int rc = statsdb_exec(buf);
    DELETEA(buf);
    statsdb_do_or_die(rc);
}

int statsdb_version()
{
    int version = 0;
    sqlite3_stmt *res;
    statsdb_do_or_die(sqlite3_prepare_v2(statsdb, "PRAGMA user_version;", -1, &res, 0));
    while(sqlite3_step(res) == SQLITE_ROW) version = sqlite3_column_int(res, 0);
    sqlite3_finalize(res);
    return version;
}

vector<int> statsdb_modes;
vector<int> statsdb_mutators;

void statsdb_load_mappings()
{
    sqlite3_stmt *stmt;
    statsdb_modes.add(-1, G_MAX);
    statsdb_do_or_die(sqlite3_prepare_v2(statsdb, "SELECT value, mode_id FROM proto_modes WHERE version = ?", -1, &stmt, 0));
    statsdb_do_or_die(sqlite3_bind_int(stmt, 1, STATSDB_VERSION_GAME));
    int count = 0;
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int value = sqlite3_column_int(stmt, 0);
        int mode_id = sqlite3_column_int(stmt, 1);
        statsdb_modes[value] = mode_id;
        count++;
    }
    sqlite3_finalize(stmt);
    if(count != G_MAX)
    {
        statsdb_warn("wrong number of modes %d, need %d", count, G_MAX);
        statsdb_die();
    }

    statsdb_mutators.add(-1, G_M_GSP + (G_M_NUM - G_M_GSP) * G_MAX);
    statsdb_do_or_die(sqlite3_prepare_v2(statsdb, "SELECT mode, bit, mutator_id FROM proto_mutators WHERE version = ?", -1, &stmt, 0));
    statsdb_do_or_die(sqlite3_bind_int(stmt, 1, STATSDB_VERSION_GAME));
    count = 0;
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int mode = sqlite3_column_int(stmt, 0);
        int bit = sqlite3_column_int(stmt, 1);
        int mutator_id = sqlite3_column_int(stmt, 2);
        if(sqlite3_column_type(stmt, 0) == SQLITE_NULL) statsdb_mutators[bit] = mutator_id;
        else statsdb_mutators[bit + 3 * mode] = mutator_id;
        count++;
    }
    sqlite3_finalize(stmt);
    if(count < G_M_NUM - G_M_GSP)
    {
        statsdb_warn("not enough mutators %d, should be at least %d", count, G_M_NUM - G_M_GSP);
        statsdb_die();
    }
}

void statsdb_load()
{
    statsdb_do_or_die(sqlite3_open(findfile("stats.sqlite", "w"), &statsdb));
    statsdb_exec_or_die("BEGIN");
    if(statsdb_version() < 1)
    {
        statsdb_exec_file_or_die("sql/stats/create.sql");
        statsdb_exec_or_die("PRAGMA user_version = %d;", STATSDB_VERSION);
        conoutf("Created statistics database");
    }
    while(statsdb_version() < STATSDB_VERSION)
    {
        int ver = statsdb_version();
        defformatstring(path, "sql/stats/upgrade_%d.sql", ver);
        statsdb_exec_file_or_die(path);
        statsdb_exec_or_die("PRAGMA user_version = %d;", ver + 1);
        conoutf("Upgraded database from %d to %d", ver, statsdb_version());
    }
    statsdb_exec_or_die("COMMIT");
    statsdb_load_mappings();
    conoutf("Statistics database loaded");
}

int playertotalstat(const char *handle, const char *stat)
{
    int ret = 0;
    sqlite3_stmt *stmt;

    char *sql = sqlite3_mprintf(
            "SELECT SUM(%w) "
            "FROM game_players AS gp JOIN games AS g on g.id = gp.game "
            "WHERE g.normalweapons = 1 AND gp.handle = %Q", stat, handle);
    int rc = sqlite3_prepare_v2(statsdb, sql, -1, &stmt, 0);
    sqlite3_free(sql);
    if(rc != SQLITE_OK)
    {
        statsdb_warn();
        return 0;
    }

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        ret = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return ret;
}

float playeravgpos(const char *handle)
{
    float ret = masterstatsavgposdefault;
    sqlite3_stmt *stmt;
    char *sql = sqlite3_mprintf(
        "SELECT SUM(CAST(thisscore AS FLOAT) / maxscore) / COUNT(*), COUNT(*) "
            "FROM "
            "(SELECT thisscore, MAX(gp.score) AS maxscore "
            "FROM "
                "(SELECT g.id AS gid, gp1.score AS thisscore "
                "FROM games AS g "
                "JOIN game_players AS gp1 ON g.id = gp1.game "
                "WHERE g.uniqueplayers >= %d AND g.normalweapons = 1 AND gp1.handle = %Q "
                "ORDER BY g.id DESC LIMIT %d) "
            "JOIN game_players AS gp ON gid = gp.game "
            "GROUP BY gid) ",
        masterstatavgposnumplayers, handle, masterstatsavgposlastgames);
    int rc = sqlite3_prepare_v2(statsdb, sql, -1, &stmt, 0);
    sqlite3_free(sql);
    if(rc != SQLITE_OK)
    {
        statsdb_warn();
        return ret;
    }

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        ret = sqlite3_column_int(stmt, 1) ? max(sqlite3_column_double(stmt, 0), (double)0) : ret;
    }

    sqlite3_finalize(stmt);
    return ret;
}

void masterout(masterclient &c, const char *msg, int len = 0)
{
    if(!len) len = strlen(msg);
    c.output.put(msg, len);
}

void masteroutf(masterclient &c, const char *fmt, ...)
{
    bigstring msg;
    va_list args;
    va_start(args, fmt);
    vformatstring(msg, fmt, args);
    va_end(args);
    masterout(c, msg);
}

void savestats(masterclient &c)
{
    c.wantstats = true;
    c.laststats = totalmillis ? totalmillis : 1;
    int rc = statsdb_exec("BEGIN EXCLUSIVE");
    if(rc == SQLITE_BUSY) return;
    statsdb_do_or_die(rc);

    c.instats = false;
    c.wantstats = false;

    if(c.stats.players.length() == 0)
    {
        statsdb_warn("game with 0 players");
        statsdb_rollback();
        return;
    }

#define TRY(call) if((call) != SQLITE_OK) { statsdb_warn(); statsdb_rollback(); return; }

    int mode_id = statsdb_modes.inrange(c.stats.mode) ? statsdb_modes[c.stats.mode] : -1;
    if(mode_id < 0)
    {
        statsdb_warn("unknown mode %d", c.stats.mode);
        statsdb_rollback();
        return;
    }
    TRY(statsdb_exec("INSERT INTO games VALUES (NULL, %d, %Q, %d, %d, %d, %d)",
        c.stats.time,
        c.stats.map,
        mode_id,
        c.stats.timeplayed,
        c.stats.unique,
        c.stats.normalweapons));
    c.stats.id = (ulong)sqlite3_last_insert_rowid(statsdb);

    loopi(G_M_NUM) if(c.stats.mutators & (1 << i))
    {
        int idx = i < G_M_GSP ? i : i + c.stats.mode * (G_M_NUM - G_M_GSP);
        int mut_id = statsdb_mutators.inrange(idx) ? statsdb_mutators[idx] : -1;
        if(mut_id < 0)
        {
            statsdb_warn("unknown mutator %d (%d) mode %d", i, c.stats.mutators, c.stats.mode);
            statsdb_rollback();
            return;
        }
        TRY(statsdb_exec("INSERT INTO game_mutators (game_id, mutator_id) VALUES (%d, %d)",
            c.stats.id,
            mut_id));
    }

    TRY(statsdb_exec("INSERT INTO game_servers (game, handle, flags, desc, version, host, port) VALUES (%d, %Q, %Q, %Q, %Q, %Q, %d)",
        c.stats.id,
        c.authhandle,
        c.flags,
        c.stats.desc,
        c.stats.version,
        c.name,
        c.stats.port));

    loopv(c.stats.teams)
    {
        TRY(statsdb_exec("INSERT INTO game_teams (game, team, score, name) VALUES (%d, %d, %d, %Q)",
            c.stats.id,
            c.stats.teams[i].index,
            c.stats.teams[i].score,
            c.stats.teams[i].name));
    }

    loopv(c.stats.players)
    {
        TRY(statsdb_exec("INSERT INTO game_players (game, name, handle, score, timealive, timeactive, frags, deaths, wid) VALUES (%d, %Q, %Q, %d, %d, %d, %d, %d, %d)",
            c.stats.id,
            c.stats.players[i].name,
            c.stats.players[i].handle[0] ? c.stats.players[i].handle : NULL,
            c.stats.players[i].score,
            c.stats.players[i].timealive,
            c.stats.players[i].timeactive,
            c.stats.players[i].frags,
            c.stats.players[i].deaths,
            c.stats.players[i].wid));
    }

    loopv(c.stats.weapstats)
    {
        TRY(statsdb_exec("INSERT INTO game_weapons (game, player, playerhandle, weapon, timewielded, timeloadout, "
                                                   "damage1, frags1, hits1, flakhits1, shots1, flakshots1, "
                                                   "damage2, frags2, hits2, flakhits2, shots2, flakshots2) "
                         "VALUES (%d, %d, %Q, %Q, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
            c.stats.id,
            c.stats.weapstats[i].playerid,
            c.stats.weapstats[i].playerhandle[0] ? c.stats.weapstats[i].playerhandle : NULL,
            c.stats.weapstats[i].name,

            c.stats.weapstats[i].timewielded,
            c.stats.weapstats[i].timeloadout,

            c.stats.weapstats[i].damage1,
            c.stats.weapstats[i].frags1,
            c.stats.weapstats[i].hits1,
            c.stats.weapstats[i].flakhits1,
            c.stats.weapstats[i].shots1,
            c.stats.weapstats[i].flakshots1,

            c.stats.weapstats[i].damage2,
            c.stats.weapstats[i].frags2,
            c.stats.weapstats[i].hits2,
            c.stats.weapstats[i].flakhits2,
            c.stats.weapstats[i].shots2,
            c.stats.weapstats[i].flakshots2
        ));
    }

    loopv(c.stats.ffarounds)
    {
        TRY(statsdb_exec("INSERT INTO game_ffarounds (game, player, playerhandle, round, winner) VALUES (%d, %d, %Q, %d, %d)",
            c.stats.id,
            c.stats.ffarounds[i].playerid,
            c.stats.ffarounds[i].playerhandle,
            c.stats.ffarounds[i].round,
            c.stats.ffarounds[i].winner
        ));
    }

    loopv(c.stats.captures)
    {
        TRY(statsdb_exec("INSERT INTO game_captures (game, player, playerhandle, capturing, captured) VALUES (%d, %d, %Q, %d, %d)",
            c.stats.id,
            c.stats.captures[i].playerid,
            c.stats.captures[i].playerhandle,
            c.stats.captures[i].capturing,
            c.stats.captures[i].captured
        ));
    }

    loopv(c.stats.bombings)
    {
        TRY(statsdb_exec("INSERT INTO game_bombings (game, player, playerhandle, bombing, bombed) VALUES (%d, %d, %Q, %d, %d)",
            c.stats.id,
            c.stats.bombings[i].playerid,
            c.stats.bombings[i].playerhandle,
            c.stats.bombings[i].bombing,
            c.stats.bombings[i].bombed
        ));
    }

    TRY(statsdb_exec("COMMIT"));

#undef TRY

    conoutf("Master peer %s commited stats, game id %lu", c.name, c.stats.id);
    masteroutf(c, "stats success \"game statistics recorded (\fs\fy%lu\fS) \fs\fc%sstats/game/%lu\fS\"\n", c.stats.id, versionurl, c.stats.id);
}

void sendauthstats(masterclient &c, const char *name)
{
    masteroutf(c, "authstats \"%s\" %d %d %d %d %d %f\n", name,
        playertotalstat(name, "score"),
        playertotalstat(name, "frags"),
        playertotalstat(name, "deaths"),
        playertotalstat(name, "timealive"),
        playertotalstat(name, "timeactive"),
        playeravgpos(name)
    );
}

bool setuppingsocket(ENetAddress *address)
{
    if(pingsocket != ENET_SOCKET_NULL) return true;
    pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if(pingsocket == ENET_SOCKET_NULL) return false;
    if(address && enet_socket_bind(pingsocket, address) < 0) return false;
    enet_socket_set_option(pingsocket, ENET_SOCKOPT_NONBLOCK, 1);
    return true;
}

void setupmaster()
{
    if(masterserver)
    {
        conoutf("Loading master (%s:%d)..", *masterip ? masterip : "*", masterport);
        ENetAddress address = { ENET_HOST_ANY, enet_uint16(masterport) };
        if(*masterip && enet_address_set_host(&address, masterip) < 0) fatal("failed to resolve master address: %s", masterip);
        if((mastersocket = enet_socket_create(ENET_SOCKET_TYPE_STREAM)) == ENET_SOCKET_NULL) fatal("failed to create master server socket");
        if(enet_socket_set_option(mastersocket, ENET_SOCKOPT_REUSEADDR, 1) < 0) fatal("failed to set master server socket option");
        if(enet_socket_bind(mastersocket, &address) < 0) fatal("failed to bind master server socket");
        if(enet_socket_listen(mastersocket, -1) < 0) fatal("failed to listen on master server socket");
        if(enet_socket_set_option(mastersocket, ENET_SOCKOPT_NONBLOCK, 1) < 0) fatal("failed to make master server socket non-blocking");
        if(!setuppingsocket(&address)) fatal("failed to create ping socket");
        starttime = clocktime;
        statsdb_load();
        conoutf("Master server started on %s:[%d]", *masterip ? masterip : "localhost", masterport);
    }
}

static hashnameset<authuser> authusers;

void addauth(char *name, char *flags, char *pubkey, char *email, char *steamid)
{
    string authname;
    if(filterstring(authname, name, true, true, true, true, 100)) name = authname;
    if(authusers.access(name))
    {
        conoutf("Auth handle '%s' already exists, skipping (%s)", name, email);
        return;
    }
    name = newstring(name);
    authuser &u = authusers[name];
    u.name = name;
    u.flags = newstring(flags);
    u.pubkey = parsepubkey(pubkey);
    u.email = newstring(email);
    u.steamid = newstring(steamid);
}
COMMAND(0, addauth, "sssss");

static hashnameset<authuser> serverauthusers;

void addserverauth(char *name, char *flags, char *pubkey, char *email)
{
    string authname;
    if(filterstring(authname, name, true, true, true, true, 100)) name = authname;
    if(serverauthusers.access(name))
    {
        conoutf("Server auth handle '%s' already exists, skipping (%s)", name, email);
        return;
    }
    name = newstring(name);
    authuser &u = serverauthusers[name];
    u.name = name;
    u.flags = newstring(flags);
    u.pubkey = parsepubkey(pubkey);
    u.email = newstring(email);
}
COMMAND(0, addserverauth, "ssss");

void clearauth()
{
    enumerate(authusers, authuser, u, { delete[] u.name; delete[] u.flags; delete[] u.email; delete[] u.steamid; freepubkey(u.pubkey); });
    authusers.clear();
    enumerate(serverauthusers, authuser, u, { delete[] u.name; delete[] u.flags; delete[] u.email; delete[] u.steamid; freepubkey(u.pubkey); });
    serverauthusers.clear();
}
COMMAND(0, clearauth, "");

void purgeauths(masterclient &c)
{
    int expired = 0;
    loopv(c.authreqs)
    {
        if(ENET_TIME_DIFFERENCE(totalmillis, c.authreqs[i].reqtime) >= AUTH_TIME)
        {
            masteroutf(c, "failauth %u\n", c.authreqs[i].id);
            freechallenge(c.authreqs[i].answer);
            expired = i + 1;
        }
        else break;
    }
    if(expired > 0) c.authreqs.remove(0, expired);
}

void reqauth(masterclient &c, uint id, char *name, char *hostname)
{
    string ip, host;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");
    copystring(host, hostname && *hostname ? hostname : "-");

    authuser *u = authusers.access(name);
    if(!u)
    {
        masteroutf(c, "failauth %u\n", id);
        conoutf("Failed '%s' (%u) from %s on server %s (NOTFOUND)\n", name, id, host, ip);
        return;
    }
    conoutf("Attempting '%s' (%u) from %s on server %s\n", name, id, host, ip);

    authreq &a = c.authreqs.add();
    a.user = u;
    a.reqtime = totalmillis;
    a.id = id;
    copystring(a.hostname, host);
    uint seed[3] = { uint(starttime), uint(totalmillis), randomMT() };
    static vector<char> buf;
    buf.setsize(0);
    a.answer = genchallenge(u->pubkey, seed, sizeof(seed), buf);

    masteroutf(c, "chalauth %u %s\n", id, buf.getbuf());
}

void reqserverauth(masterclient &c, char *name)
{
    purgeauths(c);

    string ip;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");

    authuser *u = serverauthusers.access(name);
    if(!u)
    {
        masteroutf(c, "failserverauth\n");
        conoutf("Failed server '%s' (NOTFOUND)\n", name);
        return;
    }
    conoutf("Attempting server '%s'\n", name);

    c.serverauthreq.user = u;
    c.serverauthreq.reqtime = totalmillis;
    uint seed[3] = { uint(starttime), uint(totalmillis), randomMT() };
    static vector<char> buf;
    buf.setsize(0);
    c.serverauthreq.answer = genchallenge(u->pubkey, seed, sizeof(seed), buf);

    masteroutf(c, "chalserverauth %s\n", buf.getbuf());
}

void confauth(masterclient &c, uint id, const char *val)
{
    purgeauths(c);
    string ip;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");
    loopv(c.authreqs) if(c.authreqs[i].id == id)
    {
        if(checkchallenge(val, c.authreqs[i].answer))
        {
            masteroutf(c, "succauth %u \"%s\" \"%s\"\n", id, c.authreqs[i].user->name, c.authreqs[i].user->flags);
            sendauthstats(c, c.authreqs[i].user->name);
            conoutf("Succeeded '%s' [%s] (%u) from %s on server %s\n", c.authreqs[i].user->name, c.authreqs[i].user->flags, id, c.authreqs[i].hostname, ip);
        }
        else
        {
            masteroutf(c, "failauth %u\n", id);
            conoutf("Failed '%s' (%u) from %s on server %s (BADKEY)\n", c.authreqs[i].user->name, id, c.authreqs[i].hostname, ip);
        }
        freechallenge(c.authreqs[i].answer);
        c.authreqs.remove(i--);
        return;
    }
    masteroutf(c, "failauth %u\n", id);
}

void purgemasterclient(int n)
{
    masterclient &c = *masterclients[n];
    enet_socket_destroy(c.socket);
    if(verbose || c.isserver) conoutf("Master peer %s disconnected", c.name);
    delete masterclients[n];
    masterclients.remove(n);
}

void confserverauth(masterclient &c, const char *val)
{
    string ip;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0) copystring(ip, "-");
    if(checkchallenge(val, c.serverauthreq.answer))
    {
        masteroutf(c, "succserverauth \"%s\" \"%s\"\n", c.serverauthreq.user->name, c.serverauthreq.user->flags);
        conoutf("Succeeded server '%s' [%s]\n", c.serverauthreq.user->name, c.serverauthreq.user->flags);
        copystring(c.authhandle, c.serverauthreq.user->name);
        copystring(c.flags, c.serverauthreq.user->flags);
    }
    else
    {
        masteroutf(c, "failserverauth\n");
        conoutf("Failed server '%s' (BADKEY)\n", c.serverauthreq.user->name);
    }
    freechallenge(c.serverauthreq.answer);
}

void checkmasterpongs()
{
    ENetBuffer buf;
    ENetAddress addr;
    static uchar pong[MAXTRANS];
    for(;;)
    {
        buf.data = pong;
        buf.dataLength = sizeof(pong);
        int len = enet_socket_receive(pingsocket, &addr, &buf, 1);
        if(len <= 0) break;
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(c.address.host == addr.host && c.port+1 == addr.port)
            {
                c.lastpong = totalmillis ? totalmillis : 1;
                c.listserver = true;
                c.shouldping = false;
                masteroutf(c, "echo \"ping reply confirmed (on port %d), server is now listed\"\n", addr.port);
                conoutf("Master peer %s responded to ping request on port %d successfully", c.name, addr.port);
                break;
            }
        }
    }
}

static int controlversion = 0;

int nextcontrolversion()
{
    ++controlversion;
    if(controlversion < 0)
    {
        controlversion = 0;
        loopv(masterclients) masterclients[i]->lastcontrol = -1;
        loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL) control[i].version = controlversion;
    }
    return controlversion;
}

template<size_t N> static inline void statfilterstring(uchar (&dst)[N], const char *src, size_t len = 0)
{
    char tmp[N];
    filterstring(tmp, src, true, true, true, false, len ? len : N-1);
    encodeutf8(dst, N-1, (uchar*)tmp, N-1);
}

bool checkmasterclientinput(masterclient &c)
{
    if(c.inputpos < 0) return false;
    const int MAXWORDS = 24;
    char *w[MAXWORDS];
    int numargs = MAXWORDS;
    const char *p = c.input;
    for(char *end;; p = end)
    {
        end = (char *)memchr(p, '\n', &c.input[c.inputpos] - p);
        if(!end) end = (char *)memchr(p, '\0', &c.input[c.inputpos] - p);
        if(!end) break;
        *end++ = '\0';
        if(c.ishttp) continue; // eat up the rest of the bytes, we've done our bit

        //conoutf("{%s} %s", c.name, p);
        loopi(MAXWORDS)
        {
            w[i] = (char *)"";
            if(i > numargs) continue;
            char *s = parsetext(p);
            if(s) w[i] = s;
            else numargs = i;
        }

        p += strcspn(p, ";\n\0"); p++;

        if(!strcmp(w[0], "GET") && w[1] && *w[1] == '/') // cheap server-to-http hack
        {
            loopi(numargs)
            {
                if(i)
                {
                    if(i == 1)
                    {
                        char *q = newstring(&w[i][1]);
                        delete[] w[i];
                        w[i] = q;
                    }
                    w[i-1] = w[i];
                }
                else delete[] w[i];
            }
            w[numargs-1] = (char *)"";
            numargs--;
            c.ishttp = c.shouldpurge = true;
        }
        bool found = false, server = !strcmp(w[0], "server");
        if((server || !strcmp(w[0], "quick")) && !c.ishttp)
        {
            c.port = SERVER_PORT;
            c.lastactivity = totalmillis ? totalmillis : 1;
            if(!server)
            {
                c.isquick = true;
                masteroutf(c, "echo \"session initiated, awaiting auth requests\"\n");
                conoutf("Master peer %s quickly checking auth request", c.name);
            }
            else
            {
                if(w[1] && *w[1]) c.port = clamp(atoi(w[1]), 1, VAR_MAX);
                ENetAddress address = { ENET_HOST_ANY, enet_uint16(c.port) };
                c.version = w[3] && *w[3] ? atoi(w[3]) : (w[2] && *w[2] ? 150 : 0);
                if(w[4] && *w[4]) copystring(c.desc, w[4], MAXSDESCLEN+1);
                else formatstring(c.desc, "%s:[%d]", c.name, c.port);
                //if(w[5] && *w[5]) c.sendstats = atoi(w[5]) != 0;
                copystring(c.branch, w[6] && *w[6] ? w[6] : "?", MAXBRANCHLEN+1);
                if(w[2] && *w[2] && strcmp(w[2], "*") && (enet_address_set_host(&address, w[2]) < 0 || address.host != c.address.host))
                {
                    c.listserver = c.shouldping = false;
                    masteroutf(c, "echo \"server IP '%s' does not match origin '%s', server will not be listed\n", w[2], c.name);
                }
                else if(masterminver && c.version < masterminver)
                {
                    c.listserver = c.shouldping = false;
                    masteroutf(c, "echo \"server version '%d' is no longer supported (need: %d), please update at %s\n", c.version, masterminver, versionurl);
                }
                else
                {
                    c.shouldping = true;
                    c.numpings = 0;
                    c.lastcontrol = controlversion;
                    loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL)
                        masteroutf(c, "%s %u %u \"%s\"\n", ipinfotypes[control[i].type], control[i].ip, control[i].mask, control[i].reason);
                    if(c.isserver)
                    {
                        masteroutf(c, "echo \"server updated (port %d), sending ping request (on port %d)\"\n", c.port, c.port+1);
                        conoutf("Master peer %s updated server info (%d)", c.name, c.port);
                    }
                    else
                    {
                        if(*masterscriptserver) masteroutf(c, "%s\n", masterscriptserver);
                        masteroutf(c, "echo \"server registered (port %d), sending ping request (on port %d)\"\n", c.port, c.port+1);
                        conoutf("Master peer %s registered as a server (%d)", c.name, c.port);
                    }
                    c.isserver = true;
                }
            }
            found = true;
        }
        if(!strcmp(w[0], "version") || !strcmp(w[0], "update"))
        {
            masteroutf(c, "setversion %d %d\n", mastermainver ? mastermainver : server::getver(0), mastergamever ? mastergamever : server::getver(1));
            if(*masterscriptclient && !strcmp(w[0], "update")) masteroutf(c, "%s\n", masterscriptclient);
            if(verbose) conoutf("Master peer %s was sent the version", c.name);
            c.shouldpurge = found = true;
        }
        if(!strcmp(w[0], "list") || !strcmp(w[0], "update"))
        {
            int servs = 0;
            masteroutf(c, "clearservers\n");
            loopvj(masterclients)
            {
                masterclient &s = *masterclients[j];
                if(!s.listserver) continue;
                stringz(filteredflags);
                for(const char *flag = s.flags; *flag; flag++)
                {
                    if(*flag == 's' && !s.sendstats) continue;
                    concformatstring(filteredflags, "%c", *flag);
                }
                masteroutf(c, "addserver %s %d %d %s %s %s %s\n", s.name, s.port, s.priority(), escapestring(s.desc), escapestring(s.authhandle), escapestring(filteredflags), escapestring(s.branch));
                servs++;
            }
            conoutf("Master peer %s was sent %d server(s)", c.name, servs);
            c.shouldpurge = found = true;
        }
        if(c.isserver && !strcmp(w[0], "stats"))
        {
            if(!strcmp(w[1], "begin"))
            {
                c.stats.reset();
                if(c.hasflag('s'))
                {
                    conoutf("Master peer %s began sending statistics", c.name);
                    c.instats = true;
                    if(c.wantstats)
                    {
                        c.wantstats = false;
                        conoutf("Master peer %s is overwriting previous statistics", c.name);
                    }
                }
                else
                {
                    conoutf("Master peer %s attempted to send stats without proper privilege", c.name);
                    masteroutf(c, "stats failure \"statistics not submitted, no statistics privilege\"\n");
                }
            }
            else if(c.instats)
            {
                if(!strcmp(w[1], "end"))
                {
                    savestats(c);
                }
                else if(!strcmp(w[1], "game"))
                {
                    statfilterstring(c.stats.map, w[2]);
                    c.stats.mode = atoi(w[3]);
                    c.stats.mutators = atoi(w[4]);
                    c.stats.timeplayed = atoi(w[5]);
                    c.stats.time = currenttime;
                    c.stats.unique = atoi(w[6]);
                    c.stats.normalweapons = atoi(w[7]);
                }
                else if(!strcmp(w[1], "server"))
                {
                    statfilterstring(c.stats.desc, w[2], MAXSDESCLEN+1);
                    statfilterstring(c.stats.version, w[3]);
                    c.stats.port = atoi(w[4]);
                }
                else if(!strcmp(w[1], "team"))
                {
                    masterclient::statstate::team t;
                    t.index = atoi(w[2]);
                    t.score = atoi(w[3]);
                    statfilterstring(t.name, w[4]);
                    c.stats.teams.add(t);
                }
                else if(!strcmp(w[1], "player"))
                {
                    masterclient::statstate::player p;
                    statfilterstring(p.name, w[2]);
                    statfilterstring(p.handle, w[3]);
                    p.score = atoi(w[4]);
                    p.timealive = atoi(w[5]);
                    p.frags = atoi(w[6]);
                    p.deaths = atoi(w[7]);
                    p.wid = atoi(w[8]);
                    p.timeactive = atoi(w[9]);
                    c.stats.players.add(p);
                }
                else if(!strcmp(w[1], "weapon"))
                {
                    #define wint(n) ws.n = atoi(w[qidx++]);
                    masterclient::statstate::weaponstats ws;
                    ws.playerid = atoi(w[2]);
                    statfilterstring(ws.playerhandle, w[3]);
                    statfilterstring(ws.name, w[4]);
                    int qidx = 5;

                    wint(timewielded);
                    wint(timeloadout);

                    wint(damage1);
                    wint(frags1);
                    wint(hits1);
                    wint(flakhits1);
                    wint(shots1);
                    wint(flakshots1);

                    wint(damage2);
                    wint(frags2);
                    wint(hits2);
                    wint(flakhits2);
                    wint(shots2);
                    wint(flakshots2);

                    c.stats.weapstats.add(ws);
                }
                else if(!strcmp(w[1], "capture"))
                {
                    masterclient::statstate::capturestats cs;
                    cs.playerid = atoi(w[2]);
                    statfilterstring(cs.playerhandle, w[3]);
                    cs.capturing = atoi(w[4]);
                    cs.captured = atoi(w[5]);
                    c.stats.captures.add(cs);
                }
                else if(!strcmp(w[1], "bombing"))
                {
                    masterclient::statstate::bombstats bs;
                    bs.playerid = atoi(w[2]);
                    statfilterstring(bs.playerhandle, w[3]);
                    bs.bombing = atoi(w[4]);
                    bs.bombed = atoi(w[5]);
                    c.stats.bombings.add(bs);
                }
                else if(!strcmp(w[1], "ffaround"))
                {
                    masterclient::statstate::ffaroundstats fr;
                    fr.playerid = atoi(w[2]);
                    statfilterstring(fr.playerhandle, w[3]);
                    fr.round = atoi(w[4]);
                    fr.winner = atoi(w[5]);
                    c.stats.ffarounds.add(fr);
                }
            }
            found = true;
        }
        if(c.isserver || c.isquick)
        {
            if(!strcmp(w[0], "reqauth")) { reqauth(c, uint(atoi(w[1])), w[2], w[3]); found = true; }
            if(!strcmp(w[0], "reqserverauth")) { reqserverauth(c, w[1]); found = true; }
            if(!strcmp(w[0], "confauth")) { confauth(c, uint(atoi(w[1])), w[2]); found = true; }
            if(!strcmp(w[0], "confserverauth")) { confserverauth(c, w[1]); found = true; }
            if(!strcmp(w[0], "reqauthstats")) { sendauthstats(c, w[1]); found = true; }
        }
        if(w[0] && *w[0] && !found)
        {
            masteroutf(c, "error \"unknown command %s\"\n", w[0]);
            conoutf("Master peer %s (client) sent unknown command: %s", c.name, w[0]);
        }
        loopi(numargs) DELETEA(w[i]);
    }
    c.inputpos = &c.input[c.inputpos] - p;
    memmove(c.input, p, c.inputpos);
    return c.inputpos < (int)sizeof(c.input);
}

void checkmaster()
{
    if(mastersocket == ENET_SOCKET_NULL || pingsocket == ENET_SOCKET_NULL) return;

    ENetSocketSet readset, writeset;
    ENetSocket maxsock = max(mastersocket, pingsocket);
    ENET_SOCKETSET_EMPTY(readset);
    ENET_SOCKETSET_EMPTY(writeset);
    ENET_SOCKETSET_ADD(readset, mastersocket);
    ENET_SOCKETSET_ADD(readset, pingsocket);
    loopv(masterclients)
    {
        masterclient &c = *masterclients[i];
        if(c.authreqs.length()) purgeauths(c);
        if(c.shouldping && (!c.lastping || ((!c.lastpong || ENET_TIME_GREATER(c.lastping, c.lastpong)) && ENET_TIME_DIFFERENCE(totalmillis, c.lastping) > uint(masterpingdelay))))
        {
            if(c.numpings < masterpingtries)
            {
                static const uchar ping[] = { 1 };
                ENetBuffer buf;
                buf.data = (void *)ping;
                buf.dataLength = sizeof(ping);
                ENetAddress addr = { c.address.host, enet_uint16(c.port+1) };
                c.numpings++;
                c.lastping = totalmillis ? totalmillis : 1;
                enet_socket_send(pingsocket, &addr, &buf, 1);
            }
            else
            {
                c.listserver = c.shouldping = false;
                masteroutf(c, "error \"ping attempts failed (tried %d times on port %d), server will not be listed\"\n", c.numpings, c.port+1);
            }
        }
        if(c.isserver && c.lastcontrol < controlversion)
        {
            loopv(control) if(control[i].type < ipinfo::SYNCTYPES && control[i].flag == ipinfo::LOCAL && control[i].version > c.lastcontrol)
                masteroutf(c, "%s %u %u %s\n", ipinfotypes[control[i].type], control[i].ip, control[i].mask, control[i].reason);
            c.lastcontrol = controlversion;
        }
        if(c.outputpos < c.output.length()) ENET_SOCKETSET_ADD(writeset, c.socket);
        else ENET_SOCKETSET_ADD(readset, c.socket);
        maxsock = max(maxsock, c.socket);

        if(c.wantstats && ENET_TIME_DIFFERENCE(totalmillis, c.laststats) > STATSDB_RETRYTIME) savestats(c);
    }
    if(enet_socketset_select(maxsock, &readset, &writeset, 0) <= 0) return;

    if(ENET_SOCKETSET_CHECK(readset, pingsocket)) checkmasterpongs();

    if(ENET_SOCKETSET_CHECK(readset, mastersocket)) for(;;)
    {
        ENetAddress address;
        ENetSocket masterclientsocket = enet_socket_accept(mastersocket, &address);
        if(masterclients.length() >= MASTER_LIMIT || (checkipinfo(control, ipinfo::BAN, address.host) && !checkipinfo(control, ipinfo::EXCEPT, address.host) && !checkipinfo(control, ipinfo::TRUST, address.host)))
        {
            enet_socket_destroy(masterclientsocket);
            break;
        }
        if(masterduplimit && !checkipinfo(control, ipinfo::TRUST, address.host))
        {
            int dups = 0;
            loopv(masterclients) if(masterclients[i]->address.host == address.host) dups++;
            if(dups >= masterduplimit)
            {
                enet_socket_destroy(masterclientsocket);
                break;
            }
        }
        if(masterclientsocket != ENET_SOCKET_NULL)
        {
            masterclient *c = new masterclient;
            c->address = address;
            c->socket = masterclientsocket;
            c->lastactivity = totalmillis ? totalmillis : 1;
            masterclients.add(c);
            if(enet_address_get_host_ip(&c->address, c->name, sizeof(c->name)) < 0) copystring(c->name, "unknown");
            if(verbose) conoutf("Master peer %s connected", c->name);
        }
        break;
    }

    loopv(masterclients)
    {
        masterclient &c = *masterclients[i];
        if(c.outputpos < c.output.length() && ENET_SOCKETSET_CHECK(writeset, c.socket))
        {
            ENetBuffer buf;
            buf.data = (void *)&c.output[c.outputpos];
            buf.dataLength = c.output.length()-c.outputpos;
            int res = enet_socket_send(c.socket, NULL, &buf, 1);
            if(res >= 0)
            {
                c.outputpos += res;
                if(c.outputpos >= c.output.length())
                {
                    c.output.setsize(0);
                    c.outputpos = 0;
                    if(c.shouldpurge) { purgemasterclient(i--); continue; }
                }
            }
            else { purgemasterclient(i--); continue; }
        }
        if(ENET_SOCKETSET_CHECK(readset, c.socket))
        {
            ENetBuffer buf;
            buf.data = &c.input[c.inputpos];
            buf.dataLength = sizeof(c.input) - c.inputpos;
            int res = enet_socket_receive(c.socket, NULL, &buf, 1);
            if(res > 0)
            {
                c.inputpos += res;
                c.input[min(c.inputpos, (int)sizeof(c.input)-1)] = '\0';
                if(!checkmasterclientinput(c)) { purgemasterclient(i--); continue; }
            }
            else { purgemasterclient(i--); continue; }
        }
        /* if(c.output.length() > OUTPUT_LIMIT) { purgemasterclient(i--); continue; } */
        if(ENET_TIME_DIFFERENCE(totalmillis, c.lastactivity) >= (c.isserver ? SERVER_TIME : CLIENT_TIME) || (checkipinfo(control, ipinfo::BAN, c.address.host) && !checkipinfo(control, ipinfo::EXCEPT, c.address.host) && !checkipinfo(control, ipinfo::TRUST, c.address.host)))
        {
            purgemasterclient(i--);
            continue;
        }
    }
}

void cleanupmaster()
{
    if(mastersocket != ENET_SOCKET_NULL) enet_socket_destroy(mastersocket);
    statsdb_close();
}

void reloadmaster()
{
    clearauth();
}
