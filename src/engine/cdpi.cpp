// Content Delivery Platform Integrations

#include "engine.h"
#include <stddef.h>
#if defined(USE_STEAM) && USE_STEAM == 1
#define HAS_STEAM 1
#include <steam_gameserver.h>
#include <steam_api_flat.h>
#endif

namespace cdpi
{
    int curapis = NONE;

#ifdef HAS_STEAM
    namespace steam
    {
        int curoverlay = 0, curplayers = 0;
        uint64 serverid = 0;
        bool servconnected;
        intptr_t client = 0, user = 0, friends = 0, stats = 0;
        HSteamPipe spipe = 0;
        HSteamUser upipe = 0;

        SVAR(IDF_READONLY, steamusername, "");
        SVAR(IDF_READONLY, steamuserid, "");

        class steamcb
        {
            public:
                void getnumplayers();
            private:
                STEAM_CALLBACK(steamcb, overlayactivated, GameOverlayActivated_t);
                STEAM_CALLBACK(steamcb, userconnected, SteamServersConnected_t);
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

        void steamcb::userconnected(SteamServersConnected_t *p)
        {
            conoutf("Steam servers connected.");
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
            //SteamGameServer()->BSecure() ? "VAC" : "Non-VAC"
            serverid = SteamGameServer()->GetSteamID().ConvertToUint64();
            #ifdef WIN32
            conoutf("Game server SteamID: %I64u", serverid);
            #else
            conoutf("Game server SteamID: %llu", serverid);
            #endif
        }

        void cleanup()
        {
            if(curapis&SWCLIENT)
            {
                SteamAPI_Shutdown();
                conoutf("Steam API has been shutdown.");
                curoverlay = 0;
                client = user = friends = stats = 0;
                spipe = upipe = 0;
                curapis &= ~SWCLIENT;
            }
            if(curapis&SWSERVER)
            {
                //SteamGameServer()->EnableHeartbeats(false);
                SteamGameServer()->LogOff();
                SteamGameServer_Shutdown();
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
                client = (intptr_t)SteamInternal_CreateInterface(STEAMCLIENT_INTERFACE_VERSION);
                spipe = SteamAPI_GetHSteamPipe();
                upipe = SteamAPI_ISteamClient_ConnectToGlobalUser(client, spipe);
                user = (intptr_t)SteamAPI_ISteamClient_GetISteamUser(client, upipe, spipe, STEAMUSER_INTERFACE_VERSION);
                friends = (intptr_t)SteamAPI_ISteamClient_GetISteamFriends(client, upipe, spipe, STEAMFRIENDS_INTERFACE_VERSION);
                stats = (intptr_t)SteamAPI_ISteamClient_GetISteamUserStats(client, upipe, spipe, STEAMUSERSTATS_INTERFACE_VERSION);
                const char *name = SteamAPI_ISteamFriends_GetPersonaName(friends);
                if(name && *name)
                {
                    setsvar("steamusername", name);
                    conoutf("Logged in as Steam user: %s", name);
                }
                if(SteamAPI_ISteamUser_BLoggedOn(user))
                {
                    CSteamID iduser = SteamAPI_ISteamUser_GetSteamID(user);
                    if(iduser.IsValid())
                    {
                        uint64 id = iduser.ConvertToUint64();
                        string idname = "";
                        #ifdef WIN32
                        formatstring(idname, "%I64u", id);
                        #else
                        formatstring(idname, "%llu", id);
                        #endif
                        if(*idname)
                        {
                            setsvar("steamuserid", idname);
                            conoutf("Current Steam UserID: %s", idname);
                        }
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
                SteamGameServer()->SetModDir(versionuname);
                SteamGameServer()->SetProduct(versionname);
                SteamGameServer()->SetGameDescription(versiondesc);
                SteamGameServer()->SetDedicatedServer(servertype >= 3);
                SteamGameServer()->LogOnAnonymous();
                //SteamGameServer()->EnableHeartbeats(true);
                conoutf("Steam GameServer API initialised successfully.");
            }
        }

        void servupdate()
        {
            SteamGameServer()->SetMaxPlayerCount(server::maxslots());
            SteamGameServer()->SetPasswordProtected(false);
            SteamGameServer()->SetServerName(server::getserverdesc());
            SteamGameServer()->SetSpectatorPort(serverport);
            SteamGameServer()->SetSpectatorServerName(server::getserverdesc());
            SteamGameServer()->SetBotPlayerCount(0);
            SteamGameServer()->SetMapName(server::getmapname());
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

    void cleanup()
    {
#ifdef HAS_STEAM
        steam::cleanup();
#endif
    }

    bool init()
    {
        curapis = NONE;
#ifdef HAS_STEAM
#ifndef STANDALONE
        if(servertype < 3) if(!steam::initclient()) return false;
#endif
        if(servertype >= 2) steam::initserver();
#endif
        return true;
    }

    void runframe()
    {
#ifdef HAS_STEAM
        steam::runframe();
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
