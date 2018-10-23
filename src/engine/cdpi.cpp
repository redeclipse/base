// Content Delivery Platform Integrations

#include "engine.h"
#include <stddef.h>
#include <steam_gameserver.h>

namespace cdpi
{
    int curapis = NONE;

    namespace steam
    {
        int curoverlay = 0, curplayers = 0;
        uint64 serverid = 0;
        bool servconnected;

        SVAR(IDF_READONLY, steamusername, "");

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
            if(p->m_bActive) conoutf("Steam overlay opened");
            else conoutf("Steam overlay closed");
            curoverlay = p->m_bActive != 0 ? totalmillis : -totalmillis;
        }

        void steamcb::getnumplayers()
        {
            SteamAPICall_t hSteamAPICall = SteamUserStats()->GetNumberOfCurrentPlayers();
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

        void getusername()
        {
            const char *name = SteamFriends()->GetPersonaName();
            if(name && *name) setsvar("steamusername", name);
            conoutf("Currently logged in as Steam user: %s", name);
        }

        void cleanup()
        {
            if(curapis&SWCLIENT)
            {
                SteamAPI_Shutdown();
                conoutf("Steam API has been shutdown.");
                curoverlay = 0;
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
            //if(versionsteamid && SteamAPI_RestartAppIfNecessary(versionsteamid)) return false;
            if(SteamAPI_Init())
            {
                curapis |= SWCLIENT;
                getusername();
                cbs.getnumplayers();
                conoutf("Steam API initialised successfully.");
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

    void cleanup()
    {
        steam::cleanup();
    }

    bool init()
    {
        curapis = NONE;
#ifndef STANDALONE
        if(servertype < 3) if(!steam::initclient()) return false;
#endif
        if(servertype >= 2) steam::initserver();
        return true;
    }

    void runframe()
    {
        steam::runframe();
    }

    int getoverlay()
    {
        return steam::curoverlay;
    }
}
