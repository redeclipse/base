// Content Delivery Platform Integrations

#include "engine.h"
#include <steam_gameserver.h>

namespace cdpi
{
    int steamapi = CDPI_NONE, steamoverlay = 0;

    SVAR(IDF_READONLY, steamusername, "");

    class steamcb
    {
        private:
            STEAM_CALLBACK(steamcb, overlayactivated, GameOverlayActivated_t);
    } steamcbs;

    void steamcb::overlayactivated(GameOverlayActivated_t *p)
    {
        if(p->m_bActive) conoutf("Steam overlay now active");
        else conoutf("Steam overlay now inactive");
        steamoverlay = p->m_bActive != 0 ? totalmillis : -totalmillis;
    }

    void cleanup()
    {
        if(steamapi&CDPI_SWCLIENT) SteamAPI_Shutdown();
        if(steamapi&CDPI_SWSERVER) SteamGameServer_Shutdown();
    }

    bool init()
    {
        cdpi::steamapi = CDPI_NONE;
#ifndef STANDALONE
        if(versionsteamid && SteamAPI_RestartAppIfNecessary(versionsteamid)) return false;
        if(SteamAPI_Init())
        {
            steamapi |= CDPI_SWCLIENT;
            const char *name = SteamFriends()->GetPersonaName();
            if(name && *name) setsvar("steamusername", name);
        }
#endif
        //if(servertype >= 2 && SteamGameServer_Init(INADDR_ANY, 0, serverport, serverport+1, eServerModeNoAuthentication, versionstring)) steamapi |= CDPI_SWSERVER;
        return true;
    }

    void runframe()
    {
        if(steamapi) SteamAPI_RunCallbacks();
    }
}
