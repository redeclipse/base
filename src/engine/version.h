#define VERSION_MAJOR 1
#define VERSION_MINOR 9
#define VERSION_PATCH 9
#define VERSION_HLP(x,y,z,r) #x#r#y#r#z
#define VERSION_STR(x,y,z,r) VERSION_HLP(x,y,z,r)
#define VERSION_STRING VERSION_STR(VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,.)
#define VERSION_NAME "Red Eclipse"
#define VERSION_UNAME "redeclipse"
#define VERSION_VNAME "REDECLIPSE"
#define VERSION_RELEASE "Development Build"
#define VERSION_URL "https://redeclipse.net/"
#define VERSION_COPY "2010-2018"
#define VERSION_DESC "A fun-filled new take on the first-person arena shooter."

#ifndef VERSION_BUILD
#define VERSION_BUILD 0
#endif
#ifndef VERSION_BRANCH
#define VERSION_BRANCH "none"
#endif
#ifndef VERSION_REVISION
#define VERSION_REVISION ""
#endif

#define LAN_PORT 28799
#define MASTER_PORT 28800
#define SERVER_PORT 28801
