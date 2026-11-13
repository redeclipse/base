// Content Delivery Platform Integrations

#include "engine.h"
#include <stddef.h>
#if defined(USE_STEAM)
#define HAS_STEAM 1

#ifndef nullptr
#define nullptr NULL
#endif
#ifndef static_assert
#define static_assert(x, y) assert(x)
#endif

#include <steam_gameserver.h>
#include <steam_api_flat.h> // C++ API crashes when compiled with MingW
#endif
#if !defined(STANDALONE) && defined(USE_DISCORD)
#define HAS_DISCORD 1
#define DISCORD_DYNAMIC_LIB 1
#include <discord_rpc.h>
//#include <discord_register.h>
#if !defined(WIN32) && !defined(__APPLE__)
#include <unistd.h>
#endif
#endif

namespace cdpi
{
    int curapis = NONE;
    static string discorduser = ""; // the Discord user that the client connected as, for the Ready queue

#ifndef STANDALONE
    // Presence: what Steam and Discord friends see of the match, and whether they can join it
    VAR(IDF_PERSIST, presence, 0, 2, 2); // 0 = off, 1 = activity, 2 = activity and Join

    #define PRESENCEDESCLEN 48
    #define PRESENCEINTERVAL 100

    struct presenceinfo
    {
        int players, slots, ends; // ends: when the match ends by the clock, 0 without a time limit
        string details, state, status, join, group; // details and state are Discord's two lines, status is both on one line for Steam
    };
    static presenceinfo curpresence;
    static int lastpresence = 0;

    // Steam and Discord take UTF-8
    static void presencetext(char *dst, const char *src, size_t len = MAXSTRLEN)
    {
        size_t n = encodeutf8((uchar *)dst, len-1, (const uchar *)src, strlen(src));
        dst[n] = '\0';
    }

    // a server's description without its colours or runs of spaces, cut short enough for one line
    static void presencedesc(char *dst, const char *src)
    {
        string text;
        filterstring(text, src, true, true, true, true);
        int n = 0;
        for(const char *c = text; *c && n < PRESENCEDESCLEN; c++)
        {
            if(iscubespace(*c) && (!n || iscubespace(dst[n-1]))) continue;
            dst[n++] = iscubespace(*c) ? ' ' : *c;
        }
        while(n && dst[n-1] == ' ') n--;
        dst[n] = '\0';
    }

    static void buildpresence(presenceinfo &p)
    {
        p.players = p.slots = p.ends = 0;
        p.details[0] = p.state[0] = p.status[0] = p.join[0] = p.group[0] = '\0';
        if(client::playingdemo()) copystring(p.details, "Watching a demo");
        else if(!connected(false)) copystring(p.details, "In the menus");
        else if(editmode) formatstring(p.details, "Editing %s", mapctitle(mapname));
        else if(!curpeer) formatstring(p.details, "%s on %s (offline)", game::gametitle(), mapctitle(mapname));
        else
        {
            formatstring(p.details, "%s on %s", game::gametitle(), mapctitle(mapname));
            bool listed = false;
            serverinfo *si = presenceserver(listed);
            p.players = client::otherclients(true);
            if(listed && si->attr.length() > 5) p.slots = si->attr[4];
            string desc = "";
            if(si)
            {
                // the master names a server without a description by its address, which stays out of the text
                defformatstring(addr, "%s:[%d]", si->name, si->port);
                if(strcmp(si->sdesc, addr)) presencedesc(desc, si->sdesc);
            }
            if(p.slots > 0) formatstring(p.state, *desc ? "%s (%d/%d)" : "%s%d/%d", desc, p.players, p.slots);
            else copystring(p.state, desc);
            if(client::state() == CS_SPECTATOR) concformatstring(p.state, "%sspectating", *p.state ? ", " : "");
            if(!client::matchplaying()) concformatstring(p.state, "%s%s", *p.state ? ", " : "", game::gamestatename());
            if(presence >= 2 && p.slots > 0 && p.players < p.slots && client::serveropen(si->attr[5]))
            {
                formatstring(p.join, "%s://%s:%d", VERSION_UNAME, si->name, si->port);
                formatstring(p.group, "%s:%d", si->name, si->port);
            }
        }
        if(*p.state) formatstring(p.status, "%s - %s", p.details, p.state);
        else copystring(p.status, p.details);
        int remain = connected(false) ? game::gettimeremain() : 0;
        if(remain) p.ends = currenttime + remain/1000;
    }

    static bool samepresence(const presenceinfo &a, const presenceinfo &b)
    {
        return a.players == b.players && a.slots == b.slots && !strcmp(a.status, b.status) && !strcmp(a.join, b.join);
    }

    // what friends would see now, for testing without Steam or Discord
    static void getpresence()
    {
        presenceinfo p;
        buildpresence(p);
        vector<char> buf;
        const char *fields[] = { p.details, p.state, p.status, p.join, p.group };
        loopi(sizeof(fields)/sizeof(fields[0]))
        {
            if(i) buf.add(' ');
            const char *s = escapestring(fields[i]);
            buf.put(s, strlen(s));
        }
        defformatstring(nums, " %d %d", p.players, p.slots);
        buf.put(nums, strlen(nums));
        buf.add('\0');
        result(buf.getbuf());
    }
    COMMAND(0, getpresence, "");

    // a Join from Steam or Discord goes the way a clicked Join link does
    static bool presencejoin(const char *link)
    {
        string host;
        int port = 0;
        if(!link || !joinlink::parse(link, host, port)) return false;
        joinlink::queue(host, port);
        return true;
    }
#endif

    namespace steam
    {
        SVAR(IDF_READONLY, steamusername, "");
        SVAR(IDF_READONLY, steamuserid, "");
        SVAR(IDF_READONLY, steamserverid, "");

#ifdef HAS_STEAM
        int curoverlay = 0, curplayers = 0;
        bool servconnected = false;
        ISteamUser *user = NULL;
        ISteamFriends *friends = NULL;
        ISteamUserStats *stats = NULL;
        ISteamApps *apps = NULL;
        ISteamClient *client = NULL, *sclient = NULL;
        ISteamGameServer *serv = NULL;
        HSteamPipe umpipe = 0, smpipe = 0;
        HSteamUser uupipe = 0, supipe = 0;
        HAuthTicket authticket = k_HAuthTicketInvalid;

        VAR(IDF_INIT, steamclient, 0, 1, 1); // read at startup, before config.cfg, so it's kept in init.cfg
        VAR(IDF_PERSIST, steamserver, 0, eServerModeAuthentication, eServerModeAuthenticationAndSecure);

        uint64_steamid strtosid(const char *sid)
        {
            return uint64_steamid(uint64(strtoll(sid, NULL, 10)));
        }

        class steamcb
        {
            public:
                void getnumplayers();
            private:
                STEAM_CALLBACK(steamcb, overlayactivated, GameOverlayActivated_t);
#ifndef STANDALONE
                STEAM_CALLBACK(steamcb, joinrequested, GameRichPresenceJoinRequested_t);
                STEAM_CALLBACK(steamcb, urllaunched, NewUrlLaunchParameters_t);
#endif
                void on_getnumplayers(NumberOfCurrentPlayers_t *p, bool b);
                CCallResult<steamcb, NumberOfCurrentPlayers_t> numplayers;
                STEAM_GAMESERVER_CALLBACK(steamcb, serversconnected, SteamServersConnected_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, serversconnfail, SteamServerConnectFailure_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, serversdisconnected, SteamServersDisconnected_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, policyresponse, GSPolicyResponse_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, authticketresponse, ValidateAuthTicketResponse_t);
        } cbs;

        void steamcb::overlayactivated(GameOverlayActivated_t *p)
        {
            if(p->m_bActive) conoutf(colourwhite, "Steam overlay opened.");
            else conoutf(colourwhite, "Steam overlay closed.");
            curoverlay = p->m_bActive != 0 ? totalmillis : -totalmillis;
        }

#ifndef STANDALONE
        // a Join link in what Steam launched the game with, given there when "Use launch command line" is set
        static void launchlink()
        {
            if(!apps) return;
            char cmd[1024];
            int len = SteamAPI_ISteamApps_GetLaunchCommandLine(apps, cmd, sizeof(cmd));
            if(len <= 0) return;
            cmd[min(len, int(sizeof(cmd))-1)] = '\0';
            const char prefix[] = VERSION_UNAME "://";
            for(char *c = cmd; *c; c++) if(!strncasecmp(c, prefix, sizeof(prefix)-1))
            {
                char *end = c + strcspn(c, " \t\"'");
                *end = '\0';
                if(!presencejoin(c)) conoutf(colourred, "Malformed link from Steam: %s", c);
                return;
            }
        }

        void steamcb::joinrequested(GameRichPresenceJoinRequested_t *p)
        {
            string link;
            copystring(link, p->m_rgchConnect);
            if(!presencejoin(link)) conoutf(colourred, "Malformed link from Steam: %s", link);
        }

        void steamcb::urllaunched(NewUrlLaunchParameters_t *p)
        {
            launchlink();
        }
#endif

        void steamcb::getnumplayers()
        {
            SteamAPICall_t hSteamAPICall = SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers(stats);
            numplayers.Set(hSteamAPICall, this, &steamcb::on_getnumplayers);
        }

        void steamcb::on_getnumplayers(NumberOfCurrentPlayers_t *p, bool b)
        {
            if(b || !p->m_bSuccess) return;
            curplayers = p->m_cPlayers;
            conoutf(colourwhite, "Number of Steam users currently playing: %d", curplayers);
        }

        void steamcb::serversconnected(SteamServersConnected_t *p)
        {
            conoutf(colourwhite, "Server connected to Steam successfully.");
            servconnected = true;
        }

        void steamcb::serversconnfail(SteamServerConnectFailure_t *p)
        {
            servconnected = false;
            conoutf(colourred, "Server failed to connect to Steam.");
        }

        void steamcb::serversdisconnected(SteamServersDisconnected_t *p)
        {
            servconnected = false;
            conoutf(colourred, "Server got logged out of Steam.");
        }

        void steamcb::policyresponse(GSPolicyResponse_t *p)
        {
            defformatstring(id, SI64, SteamAPI_ISteamGameServer_GetSteamID(serv));
            setsvar("steamserverid", id);
            conoutf(colourwhite, "Game server SteamID: %s (%s)", id, SteamAPI_ISteamGameServer_BSecure(serv) ? "VAC" : "Non-VAC");
        }

        void steamcb::authticketresponse(ValidateAuthTicketResponse_t *p)
        {
            defformatstring(id, SI64, p->m_SteamID.ConvertToUint64());
            server::clientsteamticket(id, p->m_eAuthSessionResponse == k_EAuthSessionResponseOK);
        }

        void clientcancelticket()
        {
            if(authticket == k_HAuthTicketInvalid) return;
            if(curapis&SWCLIENT) SteamAPI_ISteamUser_CancelAuthTicket(user, authticket);
            authticket = k_HAuthTicketInvalid;
        }

        void cleanup(int apis = -1)
        {
            int check = apis >= 0 ? apis : curapis;
            if(check&SWCLIENT)
            {
                clientcancelticket();
                SteamAPI_Shutdown();
                conoutf(colourwhite, "Steam API has been shutdown.");
                curoverlay = 0;
                client = NULL;
                user = NULL;
                friends = NULL;
                stats = NULL;
                apps = NULL;
                umpipe = uupipe = 0;
                curapis &= ~SWCLIENT;
            }
            if(check&SWSERVER)
            {
                SteamAPI_ISteamGameServer_SetAdvertiseServerActive(serv, false);
                SteamAPI_ISteamGameServer_LogOff(serv);
                SteamGameServer_Shutdown();
                sclient = NULL;
                serv = NULL;
                servconnected = false;
                conoutf(colourwhite, "Steam GameServer has been shutdown.");
                curapis &= ~SWSERVER;
            }
        }

        bool initclient()
        {
            if(!steamclient) return true;
#ifndef STANDALONE
            if(joinlink::steamlaunch()) return false; // a Join link goes through Steam, which would otherwise drop it
#endif
            if(versionsteamid && !strncmp(versionbranch, "steam", 5) && SteamAPI_RestartAppIfNecessary(versionsteamid)) return false;

            SteamErrMsg errmsg;
            if(SteamAPI_InitFlat(&errmsg) != ESteamAPIInitResult::k_ESteamAPIInitResult_OK)
            {
                conoutf(colourred, "Steam API failed to start: %s", errmsg);
                return true;
            }
            curapis |= SWCLIENT;

            client = (ISteamClient *)SteamClient(); //SteamInternal_CreateInterface(STEAMCLIENT_INTERFACE_VERSION);
            if(!client) { conoutf(colourred, "Failed to get Steam client interface."); cleanup(SWCLIENT); return true; }
            umpipe = SteamAPI_GetHSteamPipe();
            if(!umpipe) { conoutf(colourred, "Failed to get Steam main pipe."); cleanup(SWCLIENT); return true; }
            uupipe = SteamAPI_GetHSteamUser();
            if(!uupipe)
            {
                conoutf(colourred, "Current Steam user pipe is invalid, trying to connect to global user.");
                uupipe = SteamAPI_ISteamClient_ConnectToGlobalUser(client, umpipe);
                if(!uupipe) { conoutf(colourred, "Failed to get Steam user pipe."); cleanup(SWCLIENT); return true; }
            }
            user = (ISteamUser *)SteamAPI_ISteamClient_GetISteamUser(client, uupipe, umpipe, STEAMUSER_INTERFACE_VERSION);
            if(!user) { conoutf(colourred, "Failed to get Steam user interface."); cleanup(SWCLIENT); return true; }
            friends = (ISteamFriends *)SteamAPI_ISteamClient_GetISteamFriends(client, uupipe, umpipe, STEAMFRIENDS_INTERFACE_VERSION);
            if(!friends) { conoutf(colourred, "Failed to get Steam friends interface."); cleanup(SWCLIENT); return true; }
            stats = (ISteamUserStats *)SteamAPI_ISteamClient_GetISteamUserStats(client, uupipe, umpipe, STEAMUSERSTATS_INTERFACE_VERSION);
            if(!stats) { conoutf(colourred, "Failed to get Steam stats interface."); cleanup(SWCLIENT); return true; }
            apps = (ISteamApps *)SteamAPI_ISteamClient_GetISteamApps(client, uupipe, umpipe, STEAMAPPS_INTERFACE_VERSION);
            if(!apps) conoutf(colourred, "Failed to get Steam apps interface.");

            const char *name = SteamAPI_ISteamFriends_GetPersonaName(friends);
            if(name && *name)
            {
                setsvar("steamusername", name);
                conoutf(colourwhite, "Current Steam Handle: %s", name);
            }
            if(SteamAPI_ISteamUser_BLoggedOn(user))
            {
                defformatstring(id, SI64, SteamAPI_ISteamUser_GetSteamID(user));
                setsvar("steamuserid", id);
                conoutf(colourwhite, "Current Steam UserID: %s", id);
            }
            conoutf(colourwhite, "Steam API initialised successfully.");
            cbs.getnumplayers();
#ifndef STANDALONE
            launchlink();
#endif
            return true;
        }

        void initserver()
        {
            if(!steamserver) return;
            if(!SteamGameServer_Init(INADDR_ANY, serverport, serverport+2, EServerMode(steamserver), versionstring))
            {
                conoutf(colourred, "Steam GameServer API failed to start.");
                return;
            }

            curapis |= SWSERVER;

            sclient = (ISteamClient *)SteamGameServerClient(); //SteamInternal_CreateInterface(STEAMCLIENT_INTERFACE_VERSION);
            if(!sclient) { conoutf(colourred, "Failed to get Steam client interface."); cleanup(SWSERVER); return; }
            smpipe = SteamGameServer_GetHSteamPipe();
            if(!smpipe) { conoutf(colourred, "Failed to get Steam main pipe."); cleanup(SWSERVER); return; }
            supipe = SteamGameServer_GetHSteamUser();
            if(!supipe) { conoutf(colourred, "Failed to get Steam user pipe."); cleanup(SWSERVER); return; }
            serv = (ISteamGameServer *)SteamAPI_ISteamClient_GetISteamGameServer(sclient, supipe, smpipe, STEAMGAMESERVER_INTERFACE_VERSION);
            if(!serv) { conoutf(colourred, "Failed to get Steam server interface."); cleanup(SWSERVER); return; }

            defformatstring(steamappid, "%d", versionsteamid);
            SteamAPI_ISteamGameServer_SetModDir(serv, versionfname);
            SteamAPI_ISteamGameServer_SetProduct(serv, steamappid);
            SteamAPI_ISteamGameServer_SetGameDescription(serv, versiondesc);
            SteamAPI_ISteamGameServer_SetDedicatedServer(serv, servertype >= 3);
            SteamAPI_ISteamGameServer_LogOnAnonymous(serv);
            SteamAPI_ISteamGameServer_SetAdvertiseServerActive(serv, true);
            conoutf(colourwhite, "Steam GameServer API initialised successfully.");
        }

        void servupdate()
        {
            SteamAPI_ISteamGameServer_SetMaxPlayerCount(serv, server::maxslots());
            SteamAPI_ISteamGameServer_SetPasswordProtected(serv, false);
            SteamAPI_ISteamGameServer_SetServerName(serv, server::getserverdesc());
            SteamAPI_ISteamGameServer_SetSpectatorPort(serv, serverport);
            SteamAPI_ISteamGameServer_SetSpectatorServerName(serv, server::getserverdesc());
            SteamAPI_ISteamGameServer_SetBotPlayerCount(serv, 0);
            SteamAPI_ISteamGameServer_SetMapName(serv, server::getmapname());
        }

        static int lastframe = 0;
        void runframe()
        {
            if(lastframe && totalmillis-lastframe < 100) return; // 10 Hz
            if(curapis&SWCLIENT) SteamAPI_RunCallbacks();
            if(curapis&SWSERVER)
            {
                SteamGameServer_RunCallbacks();
                if(servconnected) servupdate();
            }
            lastframe = totalmillis;
        }

#ifndef STANDALONE
        // steam_display names a token in the rich presence localization uploaded to Steamworks, which shows %status%
        static presenceinfo steampresence;
        static bool steamshown = false;

        static void setkey(const char *key, const char *value)
        {
            string text;
            presencetext(text, value, k_cchMaxRichPresenceValueLength);
            SteamAPI_ISteamFriends_SetRichPresence(friends, key, text);
        }

        void updatepresence(const presenceinfo &p)
        {
            if(!(curapis&SWCLIENT) || !friends) return;
            if(!presence)
            {
                if(steamshown) SteamAPI_ISteamFriends_ClearRichPresence(friends);
                steamshown = false;
                return;
            }
            if(steamshown && samepresence(p, steampresence)) return;
            setkey("status", p.status);
            setkey("steam_display", "#Status");
            // the group and Join only while friends can get in, so the address goes nowhere else
            if(*p.join)
            {
                defformatstring(size, "%d", p.players);
                setkey("steam_player_group", p.group);
                setkey("steam_player_group_size", size);
                setkey("connect", p.join);
            }
            else
            {
                SteamAPI_ISteamFriends_SetRichPresence(friends, "steam_player_group", NULL);
                SteamAPI_ISteamFriends_SetRichPresence(friends, "steam_player_group_size", NULL);
                SteamAPI_ISteamFriends_SetRichPresence(friends, "connect", NULL);
            }
            steampresence = p;
            steamshown = true;
        }
#endif

        void clientconnect()
        {
        }

        void clientdisconnect()
        {
            if(!(curapis&SWCLIENT)) return;
            clientcancelticket();
        }

        bool clientauthticket(char *token, uint *tokenlen, ENetAddress *addr)
        {
            if(!(curapis&SWCLIENT)) return false;
            clientcancelticket();
            if(!addr) return false;

            SteamNetworkingIdentity snid;
            SteamAPI_SteamNetworkingIdentity_SetIPv4Addr(&snid, addr->host, addr->port);
            authticket = SteamAPI_ISteamUser_GetAuthSessionTicket(user, token, 1024, tokenlen, &snid);
            conoutf(colourwhite, "Generating Auth Ticket: %u (%u)", authticket, *tokenlen);
            return authticket != k_HAuthTicketInvalid;
        }

        bool clientready()
        {
            return curapis&SWCLIENT ? true : false;
        }

        int serverauthmode()
        {
            if(!(curapis&SWSERVER)) return 0;
            switch(steamserver)
            {
                case eServerModeAuthenticationAndSecure: return 2;
                case eServerModeAuthentication: return 1;
                case eServerModeNoAuthentication: default: return 0;
            }
            return 0;
        }

        bool serverparseticket(const char *steamid, const uchar *token, uint tokenlen)
        {
            if(!(curapis&SWSERVER)) return false;
            uint64_steamid sid = strtosid(steamid);
            if(SteamAPI_ISteamGameServer_BeginAuthSession(serv, token, tokenlen, sid) != k_EBeginAuthSessionResultOK) return false;
            return true;
        }

        void servercancelticket(const char *steamid)
        {
            if(!(curapis&SWSERVER)) return;
            uint64_steamid sid = strtosid(steamid);
            SteamAPI_ISteamGameServer_EndAuthSession(serv, sid);
        }
#else
        bool clientauthticket(char *token, uint *tokenlen, ENetAddress *addr) { return false; }
        int serverauthmode() { return 0; }
        bool serverparseticket(const char *steamid, const uchar *token, uint tokenlen) { return false; }
        void servercancelticket(const char *steamid) {}
#endif
    }
#ifdef HAS_DISCORD
    namespace discord
    {
        VAR(IDF_INIT, discordenabled, 0, 1, 1); // read at startup, before config.cfg, so it's kept in init.cfg

        void ready(const DiscordUser *u)
        {
            conoutf(colourwhite, "Discord: connected to user %s#%s - %s", u->username, u->discriminator, u->userId);
            copystring(discorduser, u->userId && strspn(u->userId, "0123456789") == strlen(u->userId) ? u->userId : "", 24);
        }

        void disconnected(int errcode, const char *message)
        {
            discorduser[0] = '\0';
            conoutf(colourwhite, "Discord: disconnected (%d: %s)", errcode, message);
        }

        void error(int errcode, const char  *message)
        {
            conoutf(colourred, "Discord: error (%d: %s)", errcode, message);
        }

        // the join secret is the Join link, which Presence only offers for a server friends can get into
        void joingame(const char *secret)
        {
            if(!presencejoin(secret)) conoutf(colourred, "Discord: malformed join (%s)", secret);
        }

        void spectategame(const char *secret)
        {
            conoutf(colourwhite, "Discord: spectate (%s)", secret);
        }

        // asking to join is answered as the Join button would be, yes while Presence offers it
        void joinrequest(const DiscordUser *u)
        {
            bool yes = presence >= 2 && *curpresence.join;
            conoutf(colourwhite, "Discord: join request from %s (%s)", u->username, yes ? "accepted" : "declined");
            Discord_Respond(u->userId, yes ? DISCORD_REPLY_YES : DISCORD_REPLY_NO);
        }

        void cleanup()
        {
            if(!(curapis&DISCORD)) return;;
            Discord_Shutdown();
            conoutf(colourwhite, "Discord API has been shutdown.");
        }

#if !defined(WIN32) && !defined(__APPLE__)
        // discord-rpc only looks for discord-ipc-N in $XDG_RUNTIME_DIR, but a sandboxed client such as a Flatpak or Snap keeps
        // its socket in a folder of its own there, which config/discord.cfg lists, so each one is linked to where it looks
        #define DISCORDSOCKETS 10 // discord-ipc-0 to discord-ipc-9, as many as discord-rpc tries

        struct socketlinks
        {
            vector<char *> dirs; // folders under $XDG_RUNTIME_DIR, in the order they're tried

            // a link into one of the folders, which is ours to replace once its client has gone
            bool ours(const char *path, const char *runtime)
            {
                char target[PATH_MAX];
                ssize_t len = readlink(path, target, sizeof(target)-1);
                if(len <= 0) return false;
                target[len] = '\0';
                loopv(dirs)
                {
                    defformatstring(dir, "%s/%s/", runtime, dirs[i]);
                    if(!strncmp(target, dir, strlen(dir))) return true;
                }
                return false;
            }

            void link()
            {
                const char *runtime = getenv("XDG_RUNTIME_DIR");
                if(!runtime || !*runtime || dirs.empty()) return;
                loopi(DISCORDSOCKETS)
                {
                    defformatstring(path, "%s/discord-ipc-%d", runtime, i);
                    struct stat st;
                    if(!lstat(path, &st))
                    {
                        // a real socket, a link that still works, or someone else's link stays
                        if(!S_ISLNK(st.st_mode) || !stat(path, &st) || !ours(path, runtime)) continue;
                        unlink(path);
                    }
                    loopvj(dirs)
                    {
                        defformatstring(sock, "%s/%s/discord-ipc-%d", runtime, dirs[j], i);
                        if(stat(sock, &st) || !S_ISSOCK(st.st_mode)) continue;
                        if(!symlink(sock, path)) conoutf(colourwhite, "Discord: found %s", sock);
                        break;
                    }
                }
            }
        } sockets;

        static void discordsocketdir(char *dir)
        {
            if(*dir) sockets.dirs.add(newstring(dir));
        }
        COMMAND(0, discordsocketdir, "s");
#endif

        void init()
        {
            if(!discordenabled || !*versiondiscordid) return;
#if !defined(WIN32) && !defined(__APPLE__)
            sockets.dirs.deletearrays();
            execfile("config/discord.cfg", false);
            sockets.link();
#endif
            DiscordEventHandlers handlers;
            memset(&handlers, 0, sizeof(handlers));
            handlers.ready = ready;
            handlers.errored = error;
            handlers.disconnected = disconnected;
            handlers.joinGame = joingame;
            handlers.spectateGame = spectategame;
            handlers.joinRequest = joinrequest;
            defformatstring(str, "%d", versionsteamid);
            Discord_Initialize(versiondiscordid, &handlers, 0, str); // todo: update for autoregister
            conoutf(colourwhite, "Discord: waiting for the client"); // ready says when it has connected
            curapis |= DISCORD;
        }

        #define DISCORDINTERVAL 15000 // Discord's rate limit
        #define DISCORDENDSLACK 5 // seconds the countdown can drift before it's sent again

        static int lastframe = 0, lastsent = 0;
        static presenceinfo discordsent;
        static bool discordshown = false;
        static string details = "", state = "", partyid = "", joinsecret = "";
        void runframe()
        {
            if(!(curapis&DISCORD) || (lastframe && totalmillis-lastframe < 100)) return;
            const presenceinfo &p = curpresence;
            bool changed = !discordshown || !samepresence(p, discordsent) || abs(p.ends - discordsent.ends) > DISCORDENDSLACK;
            if((!lastsent || totalmillis-lastsent >= DISCORDINTERVAL) && (presence ? changed : discordshown))
            {
                if(presence)
                {
                    DiscordRichPresence discordPresence;
                    memset(&discordPresence, 0, sizeof(discordPresence));

                    presencetext(details, p.details, 128);
                    discordPresence.details = details;
                    presencetext(state, p.state, 128);
                    if(*state) discordPresence.state = state;

                    discordPresence.startTimestamp = 0;
                    discordPresence.endTimestamp = p.ends;
                    discordPresence.largeImageKey = "emblem";
                    discordPresence.largeImageText = versionfname;
                    discordPresence.smallImageKey = hud::modeimage();
                    discordPresence.smallImageText = details;

                    // the party and Join only while friends can get in, so the address goes nowhere else
                    if(*p.join)
                    {
                        presencetext(partyid, p.group, 128);
                        presencetext(joinsecret, p.join, 128);
                        discordPresence.partyId = partyid;
                        discordPresence.partySize = p.players;
                        discordPresence.partyMax = p.slots;
                        discordPresence.joinSecret = joinsecret;
                    }

                    Discord_UpdatePresence(&discordPresence);
                    discordsent = p;
                    discordshown = true;
                }
                else
                {
                    Discord_ClearPresence();
                    discordshown = false;
                }
                lastsent = totalmillis;
            }
            Discord_RunCallbacks();
            lastframe = totalmillis;
        }
    }
#endif

    void cleanup()
    {
#ifdef HAS_STEAM
        steam::cleanup();
#endif
#ifdef HAS_DISCORD
        discord::cleanup();
#endif
    }

    bool init()
    {
        curapis = NONE;
#ifdef HAS_STEAM
#ifndef STANDALONE
        if(servertype < 3) if(!steam::initclient()) return false; // steam says die
#endif
        if(servertype >= 2) steam::initserver();
#endif
#ifdef HAS_DISCORD
        discord::init();
#endif
        return true;
    }

    void runframe()
    {
#ifndef STANDALONE
        if(curapis&(SWCLIENT|DISCORD) && (!lastpresence || totalmillis-lastpresence >= PRESENCEINTERVAL))
        {
            buildpresence(curpresence);
            lastpresence = totalmillis;
#ifdef HAS_STEAM
            steam::updatepresence(curpresence);
#endif
        }
#endif
#ifdef HAS_STEAM
        steam::runframe();
#endif
#ifdef HAS_DISCORD
        discord::runframe();
#endif
    }

    int getoverlay()
    {
#ifdef HAS_STEAM
        if(steam::curoverlay) return steam::curoverlay;
#endif
        return 0;
    }

    void clientconnect()
    {
#ifdef HAS_STEAM
        steam::clientconnect();
#endif
    }

    void clientdisconnect()
    {
#ifdef HAS_STEAM
        steam::clientdisconnect();
#endif
    }
}

const char *discorduserid() { return cdpi::discorduser; }
