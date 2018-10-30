// Content Delivery Platform Integrations

#include "engine.h"
#include <stddef.h>
#if defined(USE_STEAM) && USE_STEAM == 1
#define HAS_STEAM 1
#include <steam_gameserver.h>
#include <steam_api_flat.h>
#endif
#if !defined(STANDALONE) && defined(USE_DISCORD) && USE_DISCORD == 1
#define HAS_DISCORD 1
#define DISCORD_DYNAMIC_LIB 1
#include <discord_rpc.h>
//#include <discord_register.h>
#endif

namespace cdpi
{
    int curapis = NONE;

#ifdef HAS_STEAM
    namespace steam
    {
        int curoverlay = 0, curplayers = 0;
        bool servconnected;
        intptr_t client = 0, user = 0, friends = 0, stats = 0;
        intptr_t sclient = 0, serv = 0;
        HSteamPipe umpipe = 0, smpipe = 0;
        HSteamUser uupipe = 0, supipe = 0;

        SVAR(IDF_READONLY, steamusername, "");
        SVAR(IDF_READONLY, steamuserid, "");

        class steamcb
        {
            public:
                void getnumplayers();
            private:
                STEAM_CALLBACK(steamcb, overlayactivated, GameOverlayActivated_t);
                void on_getnumplayers(NumberOfCurrentPlayers_t *p, bool b);
                CCallResult<steamcb, NumberOfCurrentPlayers_t> numplayers;
                STEAM_GAMESERVER_CALLBACK(steamcb, serversconnected, SteamServersConnected_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, serversconnfail, SteamServerConnectFailure_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, serversdisconnected, SteamServersDisconnected_t);
                STEAM_GAMESERVER_CALLBACK(steamcb, policyresponse, GSPolicyResponse_t);
        } cbs;

        void steamcb::overlayactivated(GameOverlayActivated_t *p)
        {
            if(p->m_bActive) conoutf("Steam overlay opened.");
            else conoutf("Steam overlay closed.");
            curoverlay = p->m_bActive != 0 ? totalmillis : -totalmillis;
        }

        void steamcb::getnumplayers()
        {
            SteamAPICall_t hSteamAPICall = SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers(stats);
            numplayers.Set(hSteamAPICall, this, &steamcb::on_getnumplayers);
        }

        void steamcb::on_getnumplayers(NumberOfCurrentPlayers_t *p, bool b)
        {
            if(b || !p->m_bSuccess) return;
            curplayers = p->m_cPlayers;
            conoutf("Number of Steam users currently playing: %d", curplayers);
        }

        void steamcb::serversconnected(SteamServersConnected_t *p)
        {
            conoutf("Server connected to Steam successfully.");
            servconnected = true;
        }

        void steamcb::serversconnfail(SteamServerConnectFailure_t *p)
        {
            servconnected = false;
            conoutf("Server failed to connect to Steam.");
        }

        void steamcb::serversdisconnected(SteamServersDisconnected_t *p)
        {
            servconnected = false;
            conoutf("Server got logged out of Steam.");
        }

        void steamcb::policyresponse(GSPolicyResponse_t *p)
        {
            const char *vac = SteamAPI_ISteamGameServer_BSecure(serv) ? "VAC" : "Non-VAC";
            uint64 serverid = SteamAPI_ISteamGameServer_GetSteamID(serv);
            #ifdef WIN32
            conoutf("Game server SteamID: %I64u (%s)", serverid, vac);
            #else
            conoutf("Game server SteamID: %llu (%s)", serverid, vac);
            #endif
        }

        void cleanup(int apis = -1)
        {
            int check = apis >= 0 ? apis : curapis;
            if(check&SWCLIENT)
            {
                SteamAPI_Shutdown();
                conoutf("Steam API has been shutdown.");
                curoverlay = 0;
                client = user = friends = stats = 0;
                umpipe = uupipe = 0;
                curapis &= ~SWCLIENT;
            }
            if(check&SWSERVER)
            {
                //SteamAPI_ISteamGameServer_EnableHeartbeats(serv, false);
                SteamAPI_ISteamGameServer_LogOff(serv);
                SteamGameServer_Shutdown();
                sclient = serv = 0;
                servconnected = false;
                conoutf("Steam GameServer has been shutdown.");
                curapis &= ~SWSERVER;
            }
        }

        bool initclient()
        {
            if(versionsteamid && !strcmp(versionbranch, "steam") && SteamAPI_RestartAppIfNecessary(versionsteamid)) return false;
            if(SteamAPI_Init())
            {
                curapis |= SWCLIENT;

                client = (intptr_t)SteamClient(); //SteamInternal_CreateInterface(STEAMCLIENT_INTERFACE_VERSION);
                if(!client) { conoutf("Failed to get Steam client interface."); cleanup(SWCLIENT); return false; }
                umpipe = SteamAPI_GetHSteamPipe();
                if(!umpipe) { conoutf("Failed to get Steam main pipe."); cleanup(SWCLIENT); return false; }
                uupipe = SteamAPI_GetHSteamUser();
                if(!uupipe)
                {
                    conoutf("Current Steam user pipe is invalid, trying to connect to global user.");
                    uupipe = SteamAPI_ISteamClient_ConnectToGlobalUser(client, umpipe);
                    if(!uupipe) { conoutf("Failed to get Steam user pipe."); cleanup(SWCLIENT); return false; }
                }
                user = (intptr_t)SteamAPI_ISteamClient_GetISteamUser(client, uupipe, umpipe, STEAMUSER_INTERFACE_VERSION);
                if(!user) { conoutf("Failed to get Steam user interface."); cleanup(SWCLIENT); return false; }
                friends = (intptr_t)SteamAPI_ISteamClient_GetISteamFriends(client, uupipe, umpipe, STEAMFRIENDS_INTERFACE_VERSION);
                if(!friends) { conoutf("Failed to get Steam friends interface."); cleanup(SWCLIENT); return false; }
                stats = (intptr_t)SteamAPI_ISteamClient_GetISteamUserStats(client, uupipe, umpipe, STEAMUSERSTATS_INTERFACE_VERSION);
                if(!stats) { conoutf("Failed to get Steam stats interface."); cleanup(SWCLIENT); return false; }

                const char *name = SteamAPI_ISteamFriends_GetPersonaName(friends);
                if(name && *name)
                {
                    setsvar("steamusername", name);
                    conoutf("Current Steam Persona: %s", name);
                }
                if(SteamAPI_ISteamUser_BLoggedOn(user))
                {
                    CSteamID iduser = SteamAPI_ISteamUser_GetSteamID(user);
                    if(iduser.IsValid())
                    {
                        #ifdef WIN32
                        defformatstring(id, "%I64u", iduser.ConvertToUint64());
                        #else
                        defformatstring(id, "%llu", iduser.ConvertToUint64());
                        #endif
                        setsvar("steamuserid", id);
                        conoutf("Current Steam UserID: %s", id);
                    }
                }
                conoutf("Steam API initialised successfully.");
                cbs.getnumplayers();
            }
            return true;
        }

        void initserver()
        {
            if(SteamGameServer_Init(INADDR_ANY, serverport+3, serverport, serverport+2, eServerModeNoAuthentication, versionstring))
            {
                curapis |= SWSERVER;

                sclient = (intptr_t)SteamGameServerClient(); //SteamInternal_CreateInterface(STEAMCLIENT_INTERFACE_VERSION);
                if(!sclient) { conoutf("Failed to get Steam client interface."); cleanup(SWSERVER); return; }
                smpipe = SteamGameServer_GetHSteamPipe();
                if(!smpipe) { conoutf("Failed to get Steam main pipe."); cleanup(SWSERVER); return; }
                supipe = SteamGameServer_GetHSteamUser();
                if(!supipe) { conoutf("Failed to get Steam user pipe."); cleanup(SWSERVER); return; }
                serv = (intptr_t)SteamAPI_ISteamClient_GetISteamGameServer(sclient, supipe, smpipe, STEAMGAMESERVER_INTERFACE_VERSION);
                if(!serv) { conoutf("Failed to get Steam server interface."); cleanup(SWSERVER); return; }

                SteamAPI_ISteamGameServer_SetModDir(serv, versionuname);
                SteamAPI_ISteamGameServer_SetProduct(serv, versionname);
                SteamAPI_ISteamGameServer_SetGameDescription(serv, versiondesc);
                SteamAPI_ISteamGameServer_SetDedicatedServer(serv, servertype >= 3);
                SteamAPI_ISteamGameServer_LogOnAnonymous(serv);
                //SteamAPI_ISteamGameServer_EnableHeartbeats(serv, true);
                conoutf("Steam GameServer API initialised successfully.");
            }
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
    }
#endif
#ifdef HAS_DISCORD
    namespace discord
    {
        VAR(IDF_PERSIST, discordenabled, 0, 1, 1);
        VAR(IDF_PERSIST, discordpresence, 0, 1, 1);

        void ready(const DiscordUser *u)
        {
            conoutf("Discord: connected to user %s#%s - %s", u->username, u->discriminator, u->userId);
        }

        void disconnected(int errcode, const char *message)
        {
            conoutf("Discord: disconnected (%d: %s)", errcode, message);
        }

        static void error(int errcode, const char  *message)
        {
            conoutf("Discord: error (%d: %s)", errcode, message);
        }

        static void joingame(const char *secret)
        {
            conoutf("Discord: join (%s)", secret);
        }

        static void spectategame(const char *secret)
        {
            conoutf("Discord: spectate (%s)", secret);
        }

        static void joinrequest(const DiscordUser *u)
        {
            printf("\nDiscord: join request from %s#%s - %s\n",
            u->username,
            u->discriminator,
            u->userId);
            //response = DISCORD_REPLY_YES;
            //response = DISCORD_REPLY_NO;
            //Discord_Respond(u->userId, response);
        }

        void cleanup()
        {
            if(!(curapis&DISCORD))
            Discord_Shutdown();
            conoutf("Discord API has been shutdown.");
        }

        void init()
        {
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
            conoutf("Discord API initialised successfully.");
            curapis |= DISCORD;
        }

        static int lastframe = 0, lastpresence = 0;
        void runframe()
        {
            if(!(curapis&DISCORD) || (lastframe && totalmillis-lastframe < 100)) return;
            if(!lastpresence || totalmillis-lastpresence >= 15000)  // 15s rate limit
            {
                if(discordpresence)
                {
                    DiscordRichPresence discordPresence;
                    memset(&discordPresence, 0, sizeof(discordPresence));
                    discordPresence.state = game::gamestatename(4);
                    discordPresence.details = game::gametitle();
                    discordPresence.startTimestamp = 0;
                    int g = game::gametime();
                    discordPresence.endTimestamp = g ? (time(0) + g/1000) : 0;
                    discordPresence.largeImageKey = "emblem";
                    discordPresence.smallImageKey = "player";
                    //discordPresence.partyId = "party1234";
                    //discordPresence.partySize = 1;
                    //discordPresence.partyMax = 6;
                    //discordPresence.matchSecret = "xyzzy";
                    //discordPresence.joinSecret = "join";
                    //discordPresence.spectateSecret = "look";
                    //discordPresence.instance = 0;
                    Discord_UpdatePresence(&discordPresence);
                }
                else Discord_ClearPresence();
                lastpresence = totalmillis;
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
        return steam::curoverlay;
#else
        return 0;
#endif
    }
}
