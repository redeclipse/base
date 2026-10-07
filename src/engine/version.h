#define STR_MACRO_HELPER(s) #s
#define STR_MACRO(s) STR_MACRO_HELPER(s)

#ifdef __clang__
  #define VERSION_COMP "clang-" \
    STR_MACRO(__clang_major__) "." \
    STR_MACRO(__clang_minor__) "." \
    STR_MACRO(__clang_patchlevel__)
#elif defined(_MSC_VER)
  #define VERSION_COMP "msvc-" STR_MACRO(_MSC_VER)
#else
  #define VERSION_COMP "gnuc-" \
    STR_MACRO(__GNUC__) "." \
    STR_MACRO(__GNUC_MINOR__) "." \
    STR_MACRO(__GNUC_PATCHLEVEL__)
#endif

#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_PATCH 9
#define VERSION_HLP(x,y,z,r) #x#r#y#r#z
#define VERSION_STR(x,y,z,r) VERSION_HLP(x,y,z,r)
#define VERSION_STRING VERSION_STR(VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,.)
#define VERSION_NAME "Red Eclipse"
#define VERSION_FNAME "Red Eclipse"
#define VERSION_UNAME "redeclipse"
#define VERSION_VNAME "REDECLIPSE"
#define VERSION_RELEASE "Big Bang Beta"
#define VERSION_URL "www.redeclipse.net"
#define VERSION_COPY "2009-2025"
#define VERSION_DESC "An arena shooter for the modern era"
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
