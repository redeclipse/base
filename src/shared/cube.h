#ifndef CPP_CUBE_HEADER
#define CPP_CUBE_HEADER

#define _FILE_OFFSET_BITS 64

#if !defined(WIN32) && defined(_WIN32)
#define WIN32
#endif

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>

#ifdef WIN32
  #define WIN32_LEAN_AND_MEAN
  #ifdef _WIN32_WINNT
  #undef _WIN32_WINNT
  #endif
  #define _WIN32_WINNT 0x0500
  #include "windows.h"
  #include "io.h"
  #ifndef _WINDOWS
    #define _WINDOWS
  #endif
  #ifndef __GNUC__
    #include <eh.h>
    #include <dbghelp.h>
    #include <intrin.h>
  #endif
  #define ZLIB_DLL
#endif

#ifndef STANDALONE
  #include <SDL.h>
  #include <SDL_opengl.h>
#endif

#include <enet/enet.h>

#include <zlib.h>

#include "tools.h"
#include "command.h"
#include "enum.h"
#include "geom.h"
#include "prop.h"
#include "ents.h"
#include "wind.h"
#include "fx.h"

#ifndef STANDALONE
#include "glexts.h"
#include "glemu.h"
#else
typedef enet_uint32 Uint32;
#endif

#include "iengine.h"
#include "igame.h"

#endif

