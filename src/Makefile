APPNAME = redeclipse
APPCLIENT = $(APPNAME)
APPSERVER = $(APPNAME)_server

#CXXFLAGS = -ggdb3
CXXFLAGS ?= -O3 -fomit-frame-pointer -ffast-math -fno-finite-math-only
override CXXFLAGS += -Wall -fsigned-char -fno-exceptions -fno-rtti -Wno-invalid-offsetof -std=c++11
PLATFORM = $(shell $(CC) -dumpmachine)
RMFLAGS = -fv
PKG_CONFIG ?= pkg-config

ifeq (,$(PLATFORM_BUILD))
    PLATFORM_BUILD = 0
endif

ifeq (,$(PLATFORM_BRANCH))
    PLATFORM_BRANCH = "none"
endif

ifeq (,$(PLATFORM_REVISION))
    PLATFORM_REVISION = ""
endif

ifeq (,$(PLATFORM_BIN))
    ifneq (,$(findstring arm,$(PLATFORM)))
        PLATFORM_BIN = arm
    else
        ifneq (,$(findstring 64,$(PLATFORM)))
            PLATFORM_BIN = amd64
        else
            PLATFORM_BIN = x86
        endif
    endif
endif

ifeq (,$(INSTDIR))
    INSTDIR = ../bin/$(PLATFORM_BIN)
endif

ifeq (,$(PDBDIR))
    PDBDIR = $(INSTDIR)/../../
endif


TOOLSET_PREFIX =
ifneq (,$(findstring cross,$(PLATFORM)))
    ifneq (,$(findstring 64,$(PLATFORM)))
        TOOLSET_PREFIX = x86_64-w64-mingw32-
    else
        TOOLSET_PREFIX = i686-w64-mingw32-
    endif

    CXX = g++
    CC = gcc
endif

SYS_FLAGS =
WINRES_INC =

ifneq (,$(findstring msvc,$(PLATFORM)))
    INC_MSVC = $(MSVC_DIR)/include
    INC_WINKIT_SHARED = $(WINKIT_INC_DIR)/shared
    INC_WINKIT_UCRT = $(WINKIT_INC_DIR)/ucrt
    INC_WINKIT_UM = $(WINKIT_INC_DIR)/um

    LIB_MSVC = $(MSVC_DIR)/lib
    LIB_UCRT = $(WINKIT_LIB_DIR)/ucrt
    LIB_UM = $(WINKIT_LIB_DIR)/um

    ifneq (,$(findstring 64,$(PLATFORM)))
        override LIB_MSVC := $(LIB_MSVC)/x64
        override LIB_UCRT := $(LIB_UCRT)/x64
        override LIB_UM := $(LIB_UM)/x64
        TOOLSET_PREFIX = x86_64-w64-mingw32-
    else
        override LIB_MSVC := $(LIB_MSVC)/x86
        override LIB_UCRT := $(LIB_UCRT)/x86
        override LIB_UM := $(LIB_UM)/x86
        TOOLSET_PREFIX = i686-w64-mingw32-
    endif

    override SYS_FLAGS += \
        -target $(PLATFORM) \
        -isystem $(INC_MSVC) \
        -isystem $(INC_WINKIT_SHARED) \
        -isystem $(INC_WINKIT_UCRT) \
        -isystem $(INC_WINKIT_UM) \
        -Wno-deprecated-declarations

    override WINRES_INC += \
        -I $(INC_WINKIT_SHARED) \
        -I $(INC_WINKIT_UCRT) \
        -I $(INC_WINKIT_UM)

    override CXXFLAGS += -fhonor-infinities $(SYS_FLAGS)

    override LDFLAGS += \
        -fuse-ld=lld \
        -target $(PLATFORM) \
        -L $(LIB_MSVC) \
        -L $(LIB_UCRT) \
        -L $(LIB_UM)

    ifeq (,$(findstring ggdb,$(CXXFLAGS)))
        override CXXFLAGS += -g -gcodeview
        override LDFLAGS += -g
    endif

    CXX = clang++
    CC = clang
else
    override CXX := $(TOOLSET_PREFIX)$(CXX)
    override CC := $(TOOLSET_PREFIX)$(CC)
endif

INCLUDES = -DVERSION_BUILD=$(PLATFORM_BUILD) \
           -DVERSION_BRANCH=\"$(PLATFORM_BRANCH)\" \
           -DVERSION_REVISION=\"$(PLATFORM_REVISION)\" \
           -I. -Ishared -Iengine -Igame -Ienet/include -Isupport

ifneq (,$(WANT_STEAM))
    override INCLUDES += -DUSE_STEAM=1 -Isteam
endif

USE_DISCORD = 0
ifneq (,$(WANT_DISCORD))
    ifneq (,$(findstring msvc,$(PLATFORM)))
        USE_DISCORD = 1
    endif

    ifneq (,$(findstring mingw,$(PLATFORM)))
        USE_DISCORD = 1
    else
        ifneq (,$(findstring 64,$(PLATFORM)))
            USE_DISCORD = 1
        endif
    endif
endif

ifeq (1,$(USE_DISCORD))
    override INCLUDES += -DUSE_DISCORD=1
endif

STRIP =
ifeq (,$(findstring -g,$(CXXFLAGS)))
    ifeq (,$(findstring -pg,$(CXXFLAGS)))
        STRIP = strip
    endif
endif

ifneq (,$(STRIP))
    override STRIP := $(TOOLSET_PREFIX)$(STRIP)
endif

MV = mv
CP = cp
MKDIR = mkdir -p

BUILD_WINDOWS = 0
ifneq (,$(findstring mingw,$(PLATFORM)))
    BUILD_WINDOWS = 1
endif

ifneq (,$(findstring msvc,$(PLATFORM)))
    BUILD_WINDOWS = 1
endif

ifeq (1,$(BUILD_WINDOWS))
    ifneq (,$(findstring arm,$(PLATFORM)))
        PLATFORM_SUFFIX = _arm
    else
        ifneq (,$(findstring 64,$(PLATFORM)))
            PLATFORM_SUFFIX = _amd64
        else
            PLATFORM_SUFFIX = _x86
        endif
    endif

    APPMODIFIER = _windows
    BIN_SUFFIX = .exe


    ifneq (,$(findstring msvc,$(PLATFORM)))
        STD_LIBS = -static -Xlinker /NODEFAULTLIB:oldnames.lib -Xlinker /SAFESEH:no
        SUP_LIBS = -Lvcpp/$(PLATFORM_BIN) -Llib/$(PLATFORM_BIN)
        WINDRES = llvm-rc
        WINRES_FLAGS = $(APPNAME).rc $(WINRES_INC)
    else
        STD_LIBS = -static-libgcc -static-libstdc++
        SUP_LIBS = -L../bin/$(PLATFORM_BIN) -Llib/$(PLATFORM_BIN)
        WINDRES = windres
        override WINDRES := $(TOOLSET_PREFIX)$(WINDRES)
        WINRES_FLAGS = -i $(APPNAME).rc -J rc -o $(APPNAME).res -O coff

        ifneq (,$(findstring 64,$(PLATFORM)))
            override WINRES_FLAGS += -F pe-x86-64
        else
            override WINRES_FLAGS += -F pe-i386
        endif
    endif

    CLIENT_INCLUDES = $(INCLUDES) -isystem include

    CLIENT_LIBS = $(STD_LIBS) $(SUP_LIBS) \
                  -lSDL2 -lSDL2_image -lOpenAL32 -lsndfile-1 -lzlib1 -lopengl32 -lenet -lws2_32 -lwinmm

    ifneq (,$(findstring msvc,$(PLATFORM)))
        CLIENT_LIBS += -ldbghelp -luser32 -lshell32
    endif

    ifeq (,$(findstring -D_DEBUG,$(CXXFLAGS)))
        CLIENT_LIBS += -mwindows
    endif

    ifneq (,$(findstring 64,$(PLATFORM)))
        ifneq (,$(WANT_STEAM))
            override CLIENT_LIBS += -lsteam_api64
        endif
    else
        ifneq (,$(WANT_STEAM))
            override CLIENT_LIBS += -lsteam_api
        endif
    endif
    ifneq (,$(WANT_DISCORD))
        override CLIENT_LIBS += -ldiscord-rpc
    endif
else
    PLATFORM_SUFFIX =
    APPMODIFIER =
    ifneq (,$(findstring linux,$(PLATFORM)))
        BIN_SUFFIX = _linux
    else
        ifneq (,$(findstring bsd,$(PLATFORM)))
            BIN_SUFFIX = _bsd
            ifneq (,$(findstring openbsd,$(PLATFORM)))
                RMFLAGS = -f
            endif
        else
            BIN_SUFFIX = _native
        endif
    endif

    CLIENT_INCLUDES = $(INCLUDES) `$(PKG_CONFIG) --cflags x11 sdl2 SDL2_image openal sndfile zlib gl`
    CLIENT_LIBS = -Lenet -lenet `$(PKG_CONFIG) --libs x11 sdl2 SDL2_image openal sndfile zlib gl`

    ifneq (,$(WANT_STEAM))
        override CLIENT_LIBS += -L../bin/$(PLATFORM_BIN) -lsteam_api
    endif
    ifneq (,$(WANT_DISCORD))
        ifneq (,$(findstring 64,$(PLATFORM)))
            override CLIENT_LIBS += -L../bin/$(PLATFORM_BIN) -ldiscord-rpc
        endif
    endif
endif

ifneq (,$(findstring linux,$(PLATFORM)))
    override CLIENT_LIBS += -lrt
else
    ifneq (,$(findstring gnu,$(PLATFORM)))
        override CLIENT_LIBS += -lrt
    endif
endif

CLIENT_OBJS = \
    shared/crypto.o \
    shared/geom.o \
    shared/glemu.o \
    shared/prop.o \
    shared/stream.o \
    shared/tools.o \
    shared/zip.o \
    support/jsmn.o \
    support/sqlite3.o \
    engine/aa.o \
    engine/bih.o \
    engine/blend.o \
    engine/cdpi.o \
    engine/client.o \
    engine/command.o \
    engine/console.o \
    engine/dynlight.o \
    engine/fx.o \
    engine/fxemit.o \
    engine/fxprop.o \
    engine/grass.o \
    engine/http.o \
    engine/irc.o \
    engine/light.o \
    engine/main.o \
    engine/master.o \
    engine/material.o \
    engine/menus.o \
    engine/normal.o \
    engine/octa.o \
    engine/octaedit.o \
    engine/octarender.o \
    engine/physics.o \
    engine/pvs.o \
    engine/renderlights.o \
    engine/renderfx.o \
    engine/rendergl.o \
    engine/rendermodel.o \
    engine/renderparticles.o \
    engine/rendersky.o \
    engine/rendertext.o \
    engine/renderva.o \
    engine/server.o \
    engine/serverbrowser.o \
    engine/shader.o \
    engine/sound.o \
    engine/stain.o \
    engine/texture.o \
    engine/ui.o \
    engine/water.o \
    engine/world.o \
    engine/worldio.o \
    engine/wind.o \
    game/ai.o \
    game/client.o \
    game/capture.o \
    game/defend.o \
    game/bomber.o \
    game/entities.o \
    game/game.o \
    game/hud.o \
    game/physics.o \
    game/projs.o \
    game/scoreboard.o \
    game/server.o \
    game/waypoint.o \
    game/weapons.o

# Build tests
ifneq (,$(findstring -D_DEBUG,$(CXXFLAGS)))
    CLIENT_OBJS += tests/slotmanager.o
endif

CLIENT_PCH = shared/cube.h.gch engine/engine.h.gch

ifeq (1,$(BUILD_WINDOWS))
    SERVER_INCLUDES = -DSTANDALONE $(INCLUDES) -Iinclude
    SERVER_LIBS = -mwindows $(STD_LIBS) $(SUP_LIBS) -lzlib1 -lenet -lws2_32 -lwinmm
    CTFONT_INCLUDES = -DSTANDALONE -Iinclude
    CTFONT_LIBS = -mwindows $(STD_LIBS) $(SUP_LIBS) -lzlib1 -lfreetype

    ifneq (,$(findstring msvc,$(PLATFORM)))
        SERVER_LIBS += -ldbghelp -luser32 -lshell32
    endif

    ifneq (,$(WANT_STEAM))
        ifneq (,$(findstring 64,$(PLATFORM)))
            override SERVER_LIBS+= -lsteam_api64
        else
            override SERVER_LIBS+= -lsteam_api
        endif
    endif
else
    SERVER_INCLUDES = -DSTANDALONE $(INCLUDES) `pkg-config --cflags zlib`
    SERVER_LIBS = -Lenet -lenet `pkg-config --libs zlib`
    CTFONT_INCLUDES = -DSTANDALONE `pkg-config --cflags zlib freetype2`
    CTFONT_LIBS = `pkg-config --libs zlib freetype2`

    ifneq (,$(WANT_STEAM))
        override SERVER_LIBS += -L../bin/$(PLATFORM_BIN) -lsteam_api
    endif
endif

SERVER_OBJS = \
    shared/crypto-standalone.o \
    shared/geom-standalone.o \
    shared/stream-standalone.o \
    shared/tools-standalone.o \
    shared/zip-standalone.o \
    shared/prop.o \
    support/jsmn.o \
    support/sqlite3.o \
    engine/cdpi-standalone.o \
    engine/command-standalone.o \
    engine/http-standalone.o \
    engine/irc-standalone.o \
    engine/master-standalone.o \
    engine/server-standalone.o \
    game/server-standalone.o

LIBENET = enet/libenet.a

GENKEY_OBJS = shared/genkey.o shared/crypto-standalone.o
CTFONT_OBJS = shared/tessfont.o

all:

default: all

clean-enet:
	$(MAKE) -C enet clean

clean-client:
	@rm $(RMFLAGS) $(CLIENT_PCH) $(CLIENT_OBJS) $(APPCLIENT)$(BIN_SUFFIX)

clean-server:
	@rm $(RMFLAGS) $(SERVER_OBJS) $(APPSERVER)$(BIN_SUFFIX)

clean-genkey:
	@rm $(RMFLAGS) $(GENKEY_OBJS) genkey$(BIN_SUFFIX)

clean-tessfont:
	@rm $(RMFLAGS) $(CTFONT_OBJS) tessfont$(BIN_SUFFIX)

clean: clean-enet clean-client clean-server clean-genkey clean-tessfont

%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $(subst .h.gch,.tmp.h.gch,$@) $(subst .h.gch,.h,$@)
	$(MV) $(subst .h.gch,.tmp.h.gch,$@) $@

%-standalone.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(subst -standalone.o,.cpp,$@)

support/sqlite3.o: support/sqlite3.c
	$(CC) $(SYS_FLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c -o support/sqlite3.o support/sqlite3.c -w

support/jsmn.o: support/jsmn.c
	$(CC) $(SYS_FLAGS) -c -o support/jsmn.o support/jsmn.c -w

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)
$(filter shared/%,$(CLIENT_OBJS)): $(filter shared/%,$(CLIENT_PCH))
$(filter engine/%,$(CLIENT_OBJS)): $(filter engine/%,$(CLIENT_PCH))
$(filter game/%,$(CLIENT_OBJS)): $(filter game/%,$(CLIENT_PCH))

$(SERVER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)

$(APPCLIENT)_windows$(PLATFORM_SUFFIX)$(BIN_SUFFIX): $(CLIENT_OBJS)
	$(WINDRES) $(WINRES_FLAGS)
	$(CXX) $(LDFLAGS) -o $@ $(APPNAME).res $(CLIENT_OBJS) $(CLIENT_LIBS)

$(APPSERVER)_windows$(PLATFORM_SUFFIX)$(BIN_SUFFIX): $(SERVER_OBJS)
	$(WINDRES) $(WINRES_FLAGS)
	$(CXX) $(LDFLAGS) -o $@ $(APPNAME).res $(SERVER_OBJS) $(SERVER_LIBS)

$(APPCLIENT)$(PLATFORM_SUFFIX)$(BIN_SUFFIX): $(LIBENET) $(CLIENT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(CLIENT_OBJS) $(CLIENT_LIBS)

$(APPSERVER)$(PLATFORM_SUFFIX)$(BIN_SUFFIX): $(LIBENET) $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(SERVER_LIBS)

client: $(APPCLIENT)$(APPMODIFIER)$(PLATFORM_SUFFIX)$(BIN_SUFFIX)

server: $(APPSERVER)$(APPMODIFIER)$(PLATFORM_SUFFIX)$(BIN_SUFFIX)

genkey: genkey$(BIN_SUFFIX)

genkey$(BIN_SUFFIX): $(GENKEY_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(GENKEY_OBJS) $(SERVER_LIBS)

shared/genkey.o: shared/genkey.cpp
	$(CXX) $(CXXFLAGS) $(SERVER_INCLUDES) -c -o shared/genkey.o shared/genkey.cpp

tessfont: tessfont$(BIN_SUFFIX)

tessfont$(BIN_SUFFIX): $(CTFONT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(CTFONT_OBJS) $(CTFONT_LIBS)

shared/tessfont.o: shared/tessfont.cpp
	$(CXX) $(SYS_FLAGS) $(CTFONT_INCLUDES) -c -o shared/tessfont.o shared/tessfont.cpp

$(PDBDIR)/$(APPCLIENT)$(APPMODIFIER)$(PLATFORM_SUFFIX).pdb: $(APPCLIENT)$(APPMODIFIER)$(PLATFORM_SUFFIX)$(BIN_SUFFIX)
ifneq (,$(findstring msvc,$(PLATFORM)))
	$(MKDIR) $(PDBDIR)
	$(CP) $(APPCLIENT)$(APPMODIFIER)$(PLATFORM_SUFFIX).pdb $(PDBDIR)/
endif

$(PDBDIR)/$(APPSERVER)$(APPMODIFIER)$(PLATFORM_SUFFIX).pdb: $(APPSERVER)$(APPMODIFIER)$(PLATFORM_SUFFIX)$(BIN_SUFFIX)
ifneq (,$(findstring msvc,$(PLATFORM)))
	$(MKDIR) $(PDBDIR)
	$(CP) $(APPSERVER)$(APPMODIFIER)$(PLATFORM_SUFFIX).pdb $(PDBDIR)/
endif

$(INSTDIR)/%$(BIN_SUFFIX): %$(APPMODIFIER)$(PLATFORM_SUFFIX)$(BIN_SUFFIX)
	$(MKDIR) $(INSTDIR)
	$(CP) $< $@
ifneq (,$(STRIP))
	$(STRIP) $@
endif

$(INSTDIR)/genkey$(BIN_SUFFIX): genkey$(BIN_SUFFIX)
	$(MKDIR) $(INSTDIR)
	$(CP) $< $@
ifneq (,$(STRIP))
	$(STRIP) $@
endif

$(INSTDIR)/tessfont$(BIN_SUFFIX): tessfont$(BIN_SUFFIX)
	$(MKDIR) $(INSTDIR)
	$(CP) $< $@
ifneq (,$(STRIP))
	$(STRIP) $@
endif

install-client: $(INSTDIR)/$(APPCLIENT)$(BIN_SUFFIX) $(PDBDIR)/$(APPCLIENT)$(APPMODIFIER)$(PLATFORM_SUFFIX).pdb

install-server: $(INSTDIR)/$(APPSERVER)$(BIN_SUFFIX) $(PDBDIR)/$(APPSERVER)$(APPMODIFIER)$(PLATFORM_SUFFIX).pdb

install-genkey: $(INSTDIR)/genkey$(BIN_SUFFIX)

install-tessfont: $(INSTDIR)/tessfont$(BIN_SUFFIX)

install: install-client install-server install-genkey install-tessfont

$(LIBENET):
	$(MAKE) -C enet

depend:
	makedepend -Y -I. -Ishared -Iengine -Igame -Isupport $(subst .o,.cpp,$(CLIENT_OBJS))
	makedepend -a -o-standalone.o -Y -I. -Ishared -Iengine -Igame -Isupport -DSTANDALONE $(subst -standalone.o,.cpp,$(SERVER_OBJS))
	makedepend -a -o.h.gch -Y -I. -Ishared -Iengine -Igame -Isupport $(subst .h.gch,.h,$(CLIENT_PCH))

all: client server genkey tessfont

include system-install.mk
include dist.mk
include wiki.mk

engine/engine.h.gch: engine/version.h shared/cube.h.gch

# DO NOT DELETE

shared/crypto.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/crypto.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/crypto.o: shared/glexts.h shared/glemu.h shared/iengine.h
shared/crypto.o: shared/igame.h
shared/geom.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/geom.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/geom.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
shared/glemu.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/glemu.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/glemu.o: shared/glexts.h shared/glemu.h shared/iengine.h
shared/glemu.o: shared/igame.h
shared/prop.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/prop.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/prop.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
shared/stream.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/stream.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/stream.o: shared/glexts.h shared/glemu.h shared/iengine.h
shared/stream.o: shared/igame.h
shared/tools.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/tools.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/tools.o: shared/glexts.h shared/glemu.h shared/iengine.h
shared/tools.o: shared/igame.h
shared/zip.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/zip.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/zip.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
engine/aa.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/aa.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/aa.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/aa.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/aa.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/aa.o: engine/bih.h engine/model.h engine/sound.h engine/soundenvprop.h
engine/bih.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/bih.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/bih.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/bih.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/bih.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/bih.o: engine/bih.h engine/model.h engine/sound.h
engine/bih.o: engine/soundenvprop.h
engine/blend.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/blend.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/blend.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/blend.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/blend.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/blend.o: engine/bih.h engine/model.h engine/sound.h
engine/blend.o: engine/soundenvprop.h
engine/cdpi.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/cdpi.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/cdpi.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/cdpi.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/cdpi.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/cdpi.o: engine/bih.h engine/model.h engine/sound.h
engine/cdpi.o: engine/soundenvprop.h
engine/client.o: engine/engine.h engine/version.h shared/cube.h
engine/client.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/client.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/client.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/client.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/client.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/client.o: engine/soundenvprop.h
engine/command.o: engine/engine.h engine/version.h shared/cube.h
engine/command.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/command.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/command.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/command.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/command.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/command.o: engine/sound.h engine/soundenvprop.h
engine/console.o: engine/engine.h engine/version.h shared/cube.h
engine/console.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/console.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/console.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/console.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/console.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/console.o: engine/sound.h engine/soundenvprop.h
engine/dynlight.o: engine/engine.h engine/version.h shared/cube.h
engine/dynlight.o: shared/tools.h shared/command.h shared/geom.h
engine/dynlight.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/dynlight.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/dynlight.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/dynlight.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/dynlight.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/fx.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
engine/fx.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/fx.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
engine/fx.o: engine/engine.h engine/version.h engine/http.h engine/irc.h
engine/fx.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/fx.o: engine/bih.h engine/model.h engine/sound.h engine/soundenvprop.h
engine/fxemit.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
engine/fxemit.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/fxemit.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/fxemit.o: shared/igame.h engine/engine.h engine/version.h
engine/fxemit.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/fxemit.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/fxemit.o: engine/sound.h engine/soundenvprop.h
engine/fxprop.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
engine/fxprop.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/fxprop.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/fxprop.o: shared/igame.h engine/engine.h engine/version.h
engine/fxprop.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/fxprop.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/fxprop.o: engine/sound.h engine/soundenvprop.h engine/fxprop.h
engine/grass.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/grass.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/grass.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/grass.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/grass.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/grass.o: engine/bih.h engine/model.h engine/sound.h
engine/grass.o: engine/soundenvprop.h
engine/http.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/http.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/http.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/http.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/http.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/http.o: engine/bih.h engine/model.h engine/sound.h
engine/http.o: engine/soundenvprop.h support/jsmn.h
engine/irc.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/irc.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/irc.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/irc.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/irc.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/irc.o: engine/bih.h engine/model.h engine/sound.h
engine/irc.o: engine/soundenvprop.h
engine/light.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/light.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/light.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/light.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/light.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/light.o: engine/bih.h engine/model.h engine/sound.h
engine/light.o: engine/soundenvprop.h
engine/main.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/main.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/main.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/main.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/main.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/main.o: engine/bih.h engine/model.h engine/sound.h
engine/main.o: engine/soundenvprop.h
engine/master.o: engine/engine.h engine/version.h shared/cube.h
engine/master.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/master.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/master.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/master.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/master.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/master.o: engine/soundenvprop.h support/sqlite3.h
engine/material.o: engine/engine.h engine/version.h shared/cube.h
engine/material.o: shared/tools.h shared/command.h shared/geom.h
engine/material.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/material.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/material.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/material.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/material.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/menus.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/menus.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/menus.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/menus.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/menus.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/menus.o: engine/bih.h engine/model.h engine/sound.h
engine/menus.o: engine/soundenvprop.h
engine/normal.o: engine/engine.h engine/version.h shared/cube.h
engine/normal.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/normal.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/normal.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/normal.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/normal.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/normal.o: engine/soundenvprop.h
engine/octa.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/octa.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/octa.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/octa.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/octa.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/octa.o: engine/bih.h engine/model.h engine/sound.h
engine/octa.o: engine/soundenvprop.h
engine/octaedit.o: engine/engine.h engine/version.h shared/cube.h
engine/octaedit.o: shared/tools.h shared/command.h shared/geom.h
engine/octaedit.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/octaedit.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/octaedit.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/octaedit.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/octaedit.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/octarender.o: engine/engine.h engine/version.h shared/cube.h
engine/octarender.o: shared/tools.h shared/command.h shared/geom.h
engine/octarender.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/octarender.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/octarender.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/octarender.o: engine/octa.h engine/light.h engine/texture.h
engine/octarender.o: engine/bih.h engine/model.h engine/sound.h
engine/octarender.o: engine/soundenvprop.h
engine/physics.o: engine/engine.h engine/version.h shared/cube.h
engine/physics.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/physics.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/physics.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/physics.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/physics.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/physics.o: engine/sound.h engine/soundenvprop.h engine/mpr.h
engine/pvs.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/pvs.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/pvs.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/pvs.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/pvs.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/pvs.o: engine/bih.h engine/model.h engine/sound.h
engine/pvs.o: engine/soundenvprop.h
engine/renderlights.o: engine/engine.h engine/version.h shared/cube.h
engine/renderlights.o: shared/tools.h shared/command.h shared/geom.h
engine/renderlights.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/renderlights.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/renderlights.o: shared/igame.h engine/http.h engine/irc.h
engine/renderlights.o: engine/world.h engine/octa.h engine/light.h
engine/renderlights.o: engine/texture.h engine/bih.h engine/model.h
engine/renderlights.o: engine/sound.h engine/soundenvprop.h
engine/renderfx.o: engine/engine.h engine/version.h shared/cube.h
engine/renderfx.o: shared/tools.h shared/command.h shared/geom.h
engine/renderfx.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/renderfx.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/renderfx.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/renderfx.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/renderfx.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/rendergl.o: engine/engine.h engine/version.h shared/cube.h
engine/rendergl.o: shared/tools.h shared/command.h shared/geom.h
engine/rendergl.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/rendergl.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/rendergl.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/rendergl.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/rendergl.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/rendermodel.o: engine/engine.h engine/version.h shared/cube.h
engine/rendermodel.o: shared/tools.h shared/command.h shared/geom.h
engine/rendermodel.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/rendermodel.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/rendermodel.o: shared/igame.h engine/http.h engine/irc.h
engine/rendermodel.o: engine/world.h engine/octa.h engine/light.h
engine/rendermodel.o: engine/texture.h engine/bih.h engine/model.h
engine/rendermodel.o: engine/sound.h engine/soundenvprop.h engine/ragdoll.h
engine/rendermodel.o: engine/animmodel.h engine/vertmodel.h
engine/rendermodel.o: engine/skelmodel.h engine/hitzone.h engine/md2.h
engine/rendermodel.o: engine/md3.h engine/md5.h engine/obj.h engine/smd.h
engine/rendermodel.o: engine/iqm.h
engine/renderparticles.o: engine/engine.h engine/version.h shared/cube.h
engine/renderparticles.o: shared/tools.h shared/command.h shared/geom.h
engine/renderparticles.o: shared/prop.h shared/ents.h engine/wind.h
engine/renderparticles.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/renderparticles.o: shared/iengine.h shared/igame.h engine/http.h
engine/renderparticles.o: engine/irc.h engine/world.h engine/octa.h
engine/renderparticles.o: engine/light.h engine/texture.h engine/bih.h
engine/renderparticles.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/renderparticles.o: engine/explosion.h engine/lensflare.h
engine/renderparticles.o: engine/lightning.h
engine/rendersky.o: engine/engine.h engine/version.h shared/cube.h
engine/rendersky.o: shared/tools.h shared/command.h shared/geom.h
engine/rendersky.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/rendersky.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/rendersky.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/rendersky.o: engine/octa.h engine/light.h engine/texture.h
engine/rendersky.o: engine/bih.h engine/model.h engine/sound.h
engine/rendersky.o: engine/soundenvprop.h
engine/rendertext.o: engine/engine.h engine/version.h shared/cube.h
engine/rendertext.o: shared/tools.h shared/command.h shared/geom.h
engine/rendertext.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/rendertext.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/rendertext.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/rendertext.o: engine/octa.h engine/light.h engine/texture.h
engine/rendertext.o: engine/bih.h engine/model.h engine/sound.h
engine/rendertext.o: engine/soundenvprop.h
engine/renderva.o: engine/engine.h engine/version.h shared/cube.h
engine/renderva.o: shared/tools.h shared/command.h shared/geom.h
engine/renderva.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/renderva.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/renderva.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/renderva.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/renderva.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/server.o: engine/engine.h engine/version.h shared/cube.h
engine/server.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/server.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/server.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/server.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/server.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/server.o: engine/soundenvprop.h
engine/serverbrowser.o: engine/engine.h engine/version.h shared/cube.h
engine/serverbrowser.o: shared/tools.h shared/command.h shared/geom.h
engine/serverbrowser.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/serverbrowser.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/serverbrowser.o: shared/igame.h engine/http.h engine/irc.h
engine/serverbrowser.o: engine/world.h engine/octa.h engine/light.h
engine/serverbrowser.o: engine/texture.h engine/bih.h engine/model.h
engine/serverbrowser.o: engine/sound.h engine/soundenvprop.h
engine/shader.o: engine/engine.h engine/version.h shared/cube.h
engine/shader.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/shader.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/shader.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/shader.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/shader.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/shader.o: engine/soundenvprop.h
engine/sound.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/sound.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/sound.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/sound.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/sound.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/sound.o: engine/bih.h engine/model.h engine/sound.h
engine/sound.o: engine/soundenvprop.h
engine/stain.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/stain.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/stain.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/stain.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/stain.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/stain.o: engine/bih.h engine/model.h engine/sound.h
engine/stain.o: engine/soundenvprop.h
engine/texture.o: engine/engine.h engine/version.h shared/cube.h
engine/texture.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/texture.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/texture.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/texture.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/texture.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/texture.o: engine/sound.h engine/soundenvprop.h
engine/ui.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/ui.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/ui.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/ui.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/ui.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/ui.o: engine/bih.h engine/model.h engine/sound.h engine/soundenvprop.h
engine/ui.o: engine/textedit.h
engine/water.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/water.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/water.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/water.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/water.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/water.o: engine/bih.h engine/model.h engine/sound.h
engine/water.o: engine/soundenvprop.h
engine/world.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/world.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/world.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/world.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/world.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/world.o: engine/bih.h engine/model.h engine/sound.h
engine/world.o: engine/soundenvprop.h
engine/worldio.o: engine/engine.h engine/version.h shared/cube.h
engine/worldio.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
engine/worldio.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/worldio.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/worldio.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/worldio.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/worldio.o: engine/sound.h engine/soundenvprop.h
engine/wind.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/wind.o: shared/command.h shared/geom.h shared/prop.h shared/ents.h
engine/wind.o: engine/wind.h engine/fx.h shared/glexts.h shared/glemu.h
engine/wind.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/wind.o: engine/world.h engine/octa.h engine/light.h engine/texture.h
engine/wind.o: engine/bih.h engine/model.h engine/sound.h
engine/wind.o: engine/soundenvprop.h
game/ai.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/ai.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/ai.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/ai.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/ai.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/ai.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/ai.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/ai.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/ai.o: game/vars.h game/ai.h game/capture.h game/defend.h game/bomber.h
game/client.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/client.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/client.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/client.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/client.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/client.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/client.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/client.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/client.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/client.o: game/bomber.h
game/capture.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/capture.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/capture.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/capture.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/capture.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/capture.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/capture.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/capture.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/capture.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/capture.o: game/bomber.h
game/defend.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/defend.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/defend.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/defend.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/defend.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/defend.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/defend.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/defend.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/defend.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/defend.o: game/bomber.h
game/bomber.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/bomber.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/bomber.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/bomber.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/bomber.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/bomber.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/bomber.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/bomber.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/bomber.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/bomber.o: game/bomber.h
game/entities.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/entities.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/entities.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/entities.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/entities.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/entities.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/entities.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/entities.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/entities.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/entities.o: game/bomber.h
game/game.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/game.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/game.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/game.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/game.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/game.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/game.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/game.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/game.o: game/vars.h game/ai.h game/capture.h game/defend.h game/bomber.h
game/hud.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/hud.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/hud.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/hud.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/hud.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/hud.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/hud.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/hud.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/hud.o: game/vars.h game/ai.h game/capture.h game/defend.h game/bomber.h
game/hud.o: game/compass.h
game/physics.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/physics.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/physics.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/physics.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/physics.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/physics.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/physics.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/physics.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/physics.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/physics.o: game/bomber.h
game/projs.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/projs.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/projs.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/projs.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/projs.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/projs.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/projs.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/projs.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/projs.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/projs.o: game/bomber.h
game/scoreboard.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/scoreboard.o: shared/tools.h shared/command.h shared/geom.h
game/scoreboard.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/scoreboard.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/scoreboard.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
game/scoreboard.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/scoreboard.o: engine/model.h engine/sound.h engine/soundenvprop.h
game/scoreboard.o: game/gamemode.h game/weapons.h game/weapdef.h
game/scoreboard.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/scoreboard.o: game/ai.h game/capture.h game/defend.h game/bomber.h
game/server.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/server.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/server.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/server.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/server.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/server.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/server.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/server.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/server.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/server.o: game/bomber.h game/auth.h game/capturemode.h game/defendmode.h
game/server.o: game/bombermode.h game/duelmut.h game/aiman.h
game/waypoint.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/waypoint.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/waypoint.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/waypoint.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/waypoint.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/waypoint.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/waypoint.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/waypoint.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/waypoint.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/waypoint.o: game/bomber.h
game/weapons.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/weapons.o: shared/tools.h shared/command.h shared/geom.h shared/prop.h
game/weapons.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
game/weapons.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
game/weapons.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
game/weapons.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
game/weapons.o: engine/soundenvprop.h game/gamemode.h game/weapons.h
game/weapons.o: game/weapdef.h game/player.h game/teamdef.h game/playerdef.h
game/weapons.o: game/vars.h game/ai.h game/capture.h game/defend.h
game/weapons.o: game/bomber.h
tests/slotmanager.o: shared/cube.h shared/tools.h shared/command.h
tests/slotmanager.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
tests/slotmanager.o: engine/fx.h shared/glexts.h shared/glemu.h
tests/slotmanager.o: shared/iengine.h shared/igame.h

shared/crypto-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/crypto-standalone.o: shared/geom.h shared/prop.h shared/ents.h
shared/crypto-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
shared/crypto-standalone.o: shared/igame.h
shared/geom-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/geom-standalone.o: shared/geom.h shared/prop.h shared/ents.h
shared/geom-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
shared/geom-standalone.o: shared/igame.h
shared/stream-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/stream-standalone.o: shared/geom.h shared/prop.h shared/ents.h
shared/stream-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
shared/stream-standalone.o: shared/igame.h
shared/tools-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/tools-standalone.o: shared/geom.h shared/prop.h shared/ents.h
shared/tools-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
shared/tools-standalone.o: shared/igame.h
shared/zip-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/zip-standalone.o: shared/geom.h shared/prop.h shared/ents.h
shared/zip-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
shared/zip-standalone.o: shared/igame.h
engine/cdpi-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/cdpi-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/cdpi-standalone.o: shared/prop.h shared/ents.h engine/wind.h
engine/cdpi-standalone.o: engine/fx.h shared/iengine.h shared/igame.h
engine/cdpi-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/cdpi-standalone.o: engine/soundenvprop.h
engine/command-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/command-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/command-standalone.o: shared/prop.h shared/ents.h engine/wind.h
engine/command-standalone.o: engine/fx.h shared/iengine.h shared/igame.h
engine/command-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/command-standalone.o: engine/soundenvprop.h
engine/http-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/http-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/http-standalone.o: shared/prop.h shared/ents.h engine/wind.h
engine/http-standalone.o: engine/fx.h shared/iengine.h shared/igame.h
engine/http-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/http-standalone.o: engine/soundenvprop.h support/jsmn.h
engine/irc-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/irc-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/irc-standalone.o: shared/prop.h shared/ents.h engine/wind.h
engine/irc-standalone.o: engine/fx.h shared/iengine.h shared/igame.h
engine/irc-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/irc-standalone.o: engine/soundenvprop.h
engine/master-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/master-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/master-standalone.o: shared/prop.h shared/ents.h engine/wind.h
engine/master-standalone.o: engine/fx.h shared/iengine.h shared/igame.h
engine/master-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/master-standalone.o: engine/soundenvprop.h support/sqlite3.h
engine/server-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/server-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/server-standalone.o: shared/prop.h shared/ents.h engine/wind.h
engine/server-standalone.o: engine/fx.h shared/iengine.h shared/igame.h
engine/server-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/server-standalone.o: engine/soundenvprop.h
game/server-standalone.o: game/game.h engine/engine.h engine/version.h
game/server-standalone.o: shared/cube.h shared/tools.h shared/command.h
game/server-standalone.o: shared/geom.h shared/prop.h shared/ents.h
game/server-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
game/server-standalone.o: shared/igame.h engine/http.h engine/irc.h
game/server-standalone.o: engine/sound.h engine/soundenvprop.h
game/server-standalone.o: game/gamemode.h game/weapons.h game/weapdef.h
game/server-standalone.o: game/player.h game/teamdef.h game/playerdef.h
game/server-standalone.o: game/vars.h game/capture.h game/defend.h
game/server-standalone.o: game/bomber.h game/auth.h game/capturemode.h
game/server-standalone.o: game/defendmode.h game/bombermode.h game/duelmut.h
game/server-standalone.o: game/aiman.h

shared/cube.h.gch: shared/tools.h shared/command.h shared/geom.h
shared/cube.h.gch: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
shared/cube.h.gch: shared/glexts.h shared/glemu.h shared/iengine.h
shared/cube.h.gch: shared/igame.h
engine/engine.h.gch: engine/version.h shared/cube.h shared/tools.h
engine/engine.h.gch: shared/command.h shared/geom.h shared/prop.h
engine/engine.h.gch: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/engine.h.gch: shared/glemu.h shared/iengine.h shared/igame.h
engine/engine.h.gch: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/engine.h.gch: engine/light.h engine/texture.h engine/bih.h
engine/engine.h.gch: engine/model.h engine/sound.h engine/soundenvprop.h
