#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_HLP(x,y,z,r) #x#r#y#r#z
#define VERSION_STR(x,y,z,r) VERSION_HLP(x,y,z,r)
#define VERSION_STRING VERSION_STR(VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,.)
#define VERSION_NAME "Red Eclipse"
#define VERSION_FNAME "Red Eclipse 2"
#define VERSION_UNAME "redeclipse"
#define VERSION_VNAME "REDECLIPSE"
#define VERSION_RELEASE "Jupiter Edition"
#define VERSION_URL "www.redeclipse.net"
#define VERSION_COPY "2010-2020"
#define VERSION_DESC "A fun-filled new take on the first-person arena shooter."
#define VERSION_STEAM_APPID 967460
#define VERSION_STEAM_DEPOT 967461
#define VERSION_DISCORD "506825464946360321"

#ifndef VERSION_BUILD
#define VERSION_BUILD 0
#endif
#ifndef VERSION_BRANCH
#define VERSION_BRANCH "selfbuilt"
#endif
#ifndef VERSION_REVISION
#define VERSION_REVISION ""
#endif

#define LAN_PORT 28799
#define MASTER_PORT 28800
#define SERVER_PORT 28801
