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
        ifneq (,$(findstring 64,$(PLATFORM)))
            PLATFORM_BIN = arm64
        else
            PLATFORM_BIN = arm
        endif
    else
        ifneq (,$(findstring aarch64,$(PLATFORM)))
            PLATFORM_BIN = arm64
        else
            ifneq (,$(findstring 64,$(PLATFORM)))
                PLATFORM_BIN = amd64
            else
                PLATFORM_BIN = x86
            endif
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
        ifneq (,$(findstring 64,$(PLATFORM)))
            PLATFORM_SUFFIX = _arm64
        else
            PLATFORM_SUFFIX = _arm
        endif
    else
        ifneq (,$(findstring aarch64,$(PLATFORM)))
            PLATFORM_BIN = _arm64
        else
            ifneq (,$(findstring 64,$(PLATFORM)))
                PLATFORM_SUFFIX = _amd64
            else
                PLATFORM_SUFFIX = _x86
            endif
        endif
    endif

    APPMODIFIER = _windows
    BIN_SUFFIX = .exe


    ifneq (,$(findstring msvc,$(PLATFORM)))
        STD_LIBS = -static -Xlinker /NODEFAULTLIB:oldnames.lib -Xlinker /SAFESEH:no
        SUP_LIBS = -Lvcpp/lib/$(PLATFORM_BIN) -Llib/$(PLATFORM_BIN)
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
                  -lSDL2 -lSDL2_image -lOpenAL32 -lsndfile -lzlib1 -lopengl32 -lenet -lws2_32 -lwinmm

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
    engine/mcube.o \
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
    shared/prop-standalone.o \
    support/jsmn.o \
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

shared/crypto.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/crypto.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/crypto.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/crypto.o: shared/igame.h
shared/geom.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/geom.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/geom.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/geom.o: shared/igame.h
shared/glemu.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/glemu.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/glemu.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/glemu.o: shared/igame.h
shared/prop.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/prop.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/prop.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/prop.o: shared/igame.h
shared/stream.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/stream.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/stream.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/stream.o: shared/igame.h
shared/tools.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/tools.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/tools.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/tools.o: shared/igame.h
shared/zip.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
shared/zip.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/zip.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/zip.o: shared/igame.h
engine/aa.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/aa.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/aa.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/aa.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/aa.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/aa.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/aa.o: engine/soundenvprop.h
engine/bih.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/bih.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/bih.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/bih.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/bih.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/bih.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/bih.o: engine/soundenvprop.h
engine/blend.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/blend.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/blend.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/blend.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/blend.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/blend.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/blend.o: engine/soundenvprop.h
engine/cdpi.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/cdpi.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/cdpi.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/cdpi.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/cdpi.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/cdpi.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/cdpi.o: engine/soundenvprop.h
engine/client.o: engine/engine.h engine/version.h shared/cube.h
engine/client.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/client.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/client.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/client.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/client.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/client.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/command.o: engine/engine.h engine/version.h shared/cube.h
engine/command.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/command.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/command.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/command.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/command.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/command.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/console.o: engine/engine.h engine/version.h shared/cube.h
engine/console.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/console.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/console.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/console.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/console.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/console.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/dynlight.o: engine/engine.h engine/version.h shared/cube.h
engine/dynlight.o: shared/tools.h shared/command.h shared/enum.h
engine/dynlight.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/dynlight.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/dynlight.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/dynlight.o: engine/world.h engine/octa.h engine/light.h
engine/dynlight.o: engine/texture.h engine/bih.h engine/model.h
engine/dynlight.o: engine/sound.h engine/soundenvprop.h
engine/fx.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
engine/fx.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/fx.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
engine/fx.o: shared/igame.h engine/engine.h engine/version.h engine/http.h
engine/fx.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/fx.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/fx.o: engine/soundenvprop.h
engine/fxemit.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
engine/fxemit.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/fxemit.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
engine/fxemit.o: shared/igame.h engine/engine.h engine/version.h
engine/fxemit.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/fxemit.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/fxemit.o: engine/sound.h engine/soundenvprop.h
engine/fxprop.o: shared/cube.h shared/tools.h shared/command.h shared/enum.h
engine/fxprop.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/fxprop.o: engine/fx.h shared/glexts.h shared/glemu.h shared/iengine.h
engine/fxprop.o: shared/igame.h engine/engine.h engine/version.h
engine/fxprop.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
engine/fxprop.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
engine/fxprop.o: engine/sound.h engine/soundenvprop.h engine/fxprop.h
engine/grass.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/grass.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/grass.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/grass.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/grass.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/grass.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/grass.o: engine/soundenvprop.h
engine/http.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/http.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/http.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/http.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/http.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/http.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/http.o: engine/soundenvprop.h support/jsmn.h
engine/irc.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/irc.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/irc.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/irc.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/irc.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/irc.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/irc.o: engine/soundenvprop.h
engine/light.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/light.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/light.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/light.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/light.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/light.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/light.o: engine/soundenvprop.h
engine/main.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/main.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/main.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/main.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/main.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/main.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/main.o: engine/soundenvprop.h
engine/master.o: engine/engine.h engine/version.h shared/cube.h
engine/master.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/master.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/master.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/master.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/master.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/master.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/material.o: engine/engine.h engine/version.h shared/cube.h
engine/material.o: shared/tools.h shared/command.h shared/enum.h
engine/material.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/material.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/material.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/material.o: engine/world.h engine/octa.h engine/light.h
engine/material.o: engine/texture.h engine/bih.h engine/model.h
engine/material.o: engine/sound.h engine/soundenvprop.h
engine/mcube.o: engine/engine.h engine/version.h engine/mcube.h shared/cube.h shared/tools.h
engine/mcube.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/mcube.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/mcube.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/mcube.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/mcube.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/mcube.o: engine/soundenvprop.h
engine/menus.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/menus.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/menus.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/menus.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/menus.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/menus.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/menus.o: engine/soundenvprop.h
engine/normal.o: engine/engine.h engine/version.h shared/cube.h
engine/normal.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/normal.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/normal.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/normal.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/normal.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/normal.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/octa.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/octa.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/octa.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/octa.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/octa.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/octa.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/octa.o: engine/soundenvprop.h
engine/octaedit.o: engine/engine.h engine/version.h shared/cube.h
engine/octaedit.o: shared/tools.h shared/command.h shared/enum.h
engine/octaedit.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/octaedit.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/octaedit.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/octaedit.o: engine/world.h engine/octa.h engine/light.h
engine/octaedit.o: engine/texture.h engine/bih.h engine/model.h
engine/octaedit.o: engine/sound.h engine/soundenvprop.h
engine/octarender.o: engine/engine.h engine/version.h shared/cube.h
engine/octarender.o: shared/tools.h shared/command.h shared/enum.h
engine/octarender.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/octarender.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/octarender.o: shared/iengine.h shared/igame.h engine/http.h
engine/octarender.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/octarender.o: engine/texture.h engine/bih.h engine/model.h
engine/octarender.o: engine/sound.h engine/soundenvprop.h
engine/physics.o: engine/engine.h engine/version.h shared/cube.h
engine/physics.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/physics.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/physics.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/physics.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/physics.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/physics.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/physics.o: engine/mpr.h
engine/pvs.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/pvs.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/pvs.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/pvs.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/pvs.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/pvs.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/pvs.o: engine/soundenvprop.h
engine/renderlights.o: engine/engine.h engine/version.h shared/cube.h
engine/renderlights.o: shared/tools.h shared/command.h shared/enum.h
engine/renderlights.o: shared/geom.h shared/prop.h shared/ents.h
engine/renderlights.o: engine/wind.h engine/fx.h shared/glexts.h
engine/renderlights.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/renderlights.o: engine/http.h engine/irc.h engine/world.h
engine/renderlights.o: engine/octa.h engine/light.h engine/texture.h
engine/renderlights.o: engine/bih.h engine/model.h engine/sound.h
engine/renderlights.o: engine/soundenvprop.h
engine/renderfx.o: engine/engine.h engine/version.h shared/cube.h
engine/renderfx.o: shared/tools.h shared/command.h shared/enum.h
engine/renderfx.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/renderfx.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/renderfx.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/renderfx.o: engine/world.h engine/octa.h engine/light.h
engine/renderfx.o: engine/texture.h engine/bih.h engine/model.h
engine/renderfx.o: engine/sound.h engine/soundenvprop.h
engine/rendergl.o: engine/engine.h engine/version.h shared/cube.h
engine/rendergl.o: shared/tools.h shared/command.h shared/enum.h
engine/rendergl.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/rendergl.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/rendergl.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/rendergl.o: engine/world.h engine/octa.h engine/light.h
engine/rendergl.o: engine/texture.h engine/bih.h engine/model.h
engine/rendergl.o: engine/sound.h engine/soundenvprop.h
engine/rendermodel.o: engine/engine.h engine/version.h shared/cube.h
engine/rendermodel.o: shared/tools.h shared/command.h shared/enum.h
engine/rendermodel.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/rendermodel.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/rendermodel.o: shared/iengine.h shared/igame.h engine/http.h
engine/rendermodel.o: engine/irc.h engine/world.h engine/octa.h
engine/rendermodel.o: engine/light.h engine/texture.h engine/bih.h
engine/rendermodel.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/rendermodel.o: engine/ragdoll.h engine/animmodel.h engine/vertmodel.h
engine/rendermodel.o: engine/skelmodel.h engine/hitzone.h engine/md2.h
engine/rendermodel.o: engine/md3.h engine/md5.h engine/obj.h engine/smd.h
engine/rendermodel.o: engine/iqm.h
engine/renderparticles.o: engine/engine.h engine/version.h shared/cube.h
engine/renderparticles.o: shared/tools.h shared/command.h shared/enum.h
engine/renderparticles.o: shared/geom.h shared/prop.h shared/ents.h
engine/renderparticles.o: engine/wind.h engine/fx.h shared/glexts.h
engine/renderparticles.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/renderparticles.o: engine/http.h engine/irc.h engine/world.h
engine/renderparticles.o: engine/octa.h engine/light.h engine/texture.h
engine/renderparticles.o: engine/bih.h engine/model.h engine/sound.h
engine/renderparticles.o: engine/soundenvprop.h engine/explosion.h
engine/renderparticles.o: engine/lensflare.h engine/lightning.h
engine/rendersky.o: engine/engine.h engine/version.h shared/cube.h
engine/rendersky.o: shared/tools.h shared/command.h shared/enum.h
engine/rendersky.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/rendersky.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/rendersky.o: shared/iengine.h shared/igame.h engine/http.h
engine/rendersky.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/rendersky.o: engine/texture.h engine/bih.h engine/model.h
engine/rendersky.o: engine/sound.h engine/soundenvprop.h
engine/rendertext.o: engine/engine.h engine/version.h shared/cube.h
engine/rendertext.o: shared/tools.h shared/command.h shared/enum.h
engine/rendertext.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/rendertext.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/rendertext.o: shared/iengine.h shared/igame.h engine/http.h
engine/rendertext.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/rendertext.o: engine/texture.h engine/bih.h engine/model.h
engine/rendertext.o: engine/sound.h engine/soundenvprop.h
engine/renderva.o: engine/engine.h engine/version.h shared/cube.h
engine/renderva.o: shared/tools.h shared/command.h shared/enum.h
engine/renderva.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
engine/renderva.o: engine/fx.h shared/glexts.h shared/glemu.h
engine/renderva.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/renderva.o: engine/world.h engine/octa.h engine/light.h
engine/renderva.o: engine/texture.h engine/bih.h engine/model.h
engine/renderva.o: engine/sound.h engine/soundenvprop.h
engine/server.o: engine/engine.h engine/version.h shared/cube.h
engine/server.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/server.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/server.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/server.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/server.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/server.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/serverbrowser.o: engine/engine.h engine/version.h shared/cube.h
engine/serverbrowser.o: shared/tools.h shared/command.h shared/enum.h
engine/serverbrowser.o: shared/geom.h shared/prop.h shared/ents.h
engine/serverbrowser.o: engine/wind.h engine/fx.h shared/glexts.h
engine/serverbrowser.o: shared/glemu.h shared/iengine.h shared/igame.h
engine/serverbrowser.o: engine/http.h engine/irc.h engine/world.h
engine/serverbrowser.o: engine/octa.h engine/light.h engine/texture.h
engine/serverbrowser.o: engine/bih.h engine/model.h engine/sound.h
engine/serverbrowser.o: engine/soundenvprop.h
engine/shader.o: engine/engine.h engine/version.h shared/cube.h
engine/shader.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/shader.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/shader.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/shader.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/shader.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/shader.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/sound.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/sound.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/sound.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/sound.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/sound.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/sound.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/sound.o: engine/soundenvprop.h
engine/stain.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/stain.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/stain.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/stain.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/stain.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/stain.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/stain.o: engine/soundenvprop.h
engine/texture.o: engine/engine.h engine/version.h shared/cube.h
engine/texture.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/texture.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/texture.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/texture.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/texture.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/texture.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/ui.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/ui.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/ui.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/ui.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/ui.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/ui.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/ui.o: engine/soundenvprop.h engine/textedit.h
engine/water.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/water.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/water.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/water.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/water.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/water.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/water.o: engine/soundenvprop.h
engine/world.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/world.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/world.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/world.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/world.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/world.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/world.o: engine/soundenvprop.h
engine/worldio.o: engine/engine.h engine/version.h shared/cube.h
engine/worldio.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
engine/worldio.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/worldio.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/worldio.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/worldio.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/worldio.o: engine/model.h engine/sound.h engine/soundenvprop.h
engine/wind.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/wind.o: shared/command.h shared/enum.h shared/geom.h shared/prop.h
engine/wind.o: shared/ents.h engine/wind.h engine/fx.h shared/glexts.h
engine/wind.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h
engine/wind.o: engine/irc.h engine/world.h engine/octa.h engine/light.h
engine/wind.o: engine/texture.h engine/bih.h engine/model.h engine/sound.h
engine/wind.o: engine/soundenvprop.h
game/ai.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/ai.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/ai.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/ai.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/ai.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/ai.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/ai.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/ai.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/ai.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/ai.o: game/defend.h game/bomber.h
game/client.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/client.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/client.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/client.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/client.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/client.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/client.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/client.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/client.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/client.o: game/defend.h game/bomber.h
game/capture.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/capture.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/capture.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/capture.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/capture.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
game/capture.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/capture.o: engine/model.h engine/sound.h engine/soundenvprop.h
game/capture.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/capture.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/capture.o: game/capture.h game/defend.h game/bomber.h
game/defend.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/defend.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/defend.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/defend.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/defend.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/defend.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/defend.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/defend.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/defend.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/defend.o: game/defend.h game/bomber.h
game/bomber.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/bomber.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/bomber.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/bomber.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/bomber.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/bomber.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/bomber.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/bomber.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/bomber.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/bomber.o: game/defend.h game/bomber.h
game/entities.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/entities.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/entities.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/entities.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/entities.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
game/entities.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/entities.o: engine/model.h engine/sound.h engine/soundenvprop.h
game/entities.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/entities.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/entities.o: game/capture.h game/defend.h game/bomber.h
game/game.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/game.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/game.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/game.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/game.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/game.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/game.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/game.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/game.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/game.o: game/defend.h game/bomber.h
game/hud.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/hud.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/hud.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/hud.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/hud.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/hud.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/hud.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/hud.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/hud.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/hud.o: game/defend.h game/bomber.h
game/physics.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/physics.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/physics.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/physics.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/physics.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
game/physics.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/physics.o: engine/model.h engine/sound.h engine/soundenvprop.h
game/physics.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/physics.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/physics.o: game/capture.h game/defend.h game/bomber.h
game/projs.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/projs.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/projs.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/projs.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/projs.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/projs.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/projs.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/projs.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/projs.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/projs.o: game/defend.h game/bomber.h
game/scoreboard.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/scoreboard.o: shared/tools.h shared/command.h shared/enum.h
game/scoreboard.o: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
game/scoreboard.o: engine/fx.h shared/glexts.h shared/glemu.h
game/scoreboard.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
game/scoreboard.o: engine/world.h engine/octa.h engine/light.h
game/scoreboard.o: engine/texture.h engine/bih.h engine/model.h
game/scoreboard.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/scoreboard.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/scoreboard.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/scoreboard.o: game/defend.h game/bomber.h
game/server.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/server.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/server.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/server.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/server.o: engine/http.h engine/irc.h engine/world.h engine/octa.h
game/server.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/server.o: engine/sound.h engine/soundenvprop.h game/gamemode.h
game/server.o: game/weapons.h game/weapdef.h game/player.h game/teamdef.h
game/server.o: game/playerdef.h game/vars.h game/ai.h game/capture.h
game/server.o: game/defend.h game/bomber.h game/auth.h game/capturemode.h
game/server.o: game/defendmode.h game/bombermode.h game/duelmut.h
game/server.o: game/aiman.h
game/waypoint.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/waypoint.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/waypoint.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/waypoint.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/waypoint.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
game/waypoint.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/waypoint.o: engine/model.h engine/sound.h engine/soundenvprop.h
game/waypoint.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/waypoint.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/waypoint.o: game/capture.h game/defend.h game/bomber.h
game/weapons.o: game/game.h game/gamelog.h engine/engine.h engine/version.h shared/cube.h
game/weapons.o: shared/tools.h shared/command.h shared/enum.h shared/geom.h
game/weapons.o: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
game/weapons.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/weapons.o: shared/igame.h engine/http.h engine/irc.h engine/world.h
game/weapons.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/weapons.o: engine/model.h engine/sound.h engine/soundenvprop.h
game/weapons.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/weapons.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/weapons.o: game/capture.h game/defend.h game/bomber.h

shared/crypto-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/crypto-standalone.o: shared/enum.h shared/geom.h shared/prop.h
shared/crypto-standalone.o: shared/ents.h engine/wind.h engine/fx.h
shared/crypto-standalone.o: shared/iengine.h shared/igame.h
shared/geom-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/geom-standalone.o: shared/enum.h shared/geom.h shared/prop.h
shared/geom-standalone.o: shared/ents.h engine/wind.h engine/fx.h
shared/geom-standalone.o: shared/iengine.h shared/igame.h
shared/stream-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/stream-standalone.o: shared/enum.h shared/geom.h shared/prop.h
shared/stream-standalone.o: shared/ents.h engine/wind.h engine/fx.h
shared/stream-standalone.o: shared/iengine.h shared/igame.h
shared/tools-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/tools-standalone.o: shared/enum.h shared/geom.h shared/prop.h
shared/tools-standalone.o: shared/ents.h engine/wind.h engine/fx.h
shared/tools-standalone.o: shared/iengine.h shared/igame.h
shared/zip-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/zip-standalone.o: shared/enum.h shared/geom.h shared/prop.h
shared/zip-standalone.o: shared/ents.h engine/wind.h engine/fx.h
shared/zip-standalone.o: shared/iengine.h shared/igame.h
engine/cdpi-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/cdpi-standalone.o: shared/tools.h shared/command.h shared/enum.h
engine/cdpi-standalone.o: shared/geom.h shared/prop.h shared/ents.h
engine/cdpi-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
engine/cdpi-standalone.o: shared/igame.h engine/http.h engine/irc.h
engine/cdpi-standalone.o: engine/sound.h engine/soundenvprop.h
engine/command-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/command-standalone.o: shared/tools.h shared/command.h shared/enum.h
engine/command-standalone.o: shared/geom.h shared/prop.h shared/ents.h
engine/command-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
engine/command-standalone.o: shared/igame.h engine/http.h engine/irc.h
engine/command-standalone.o: engine/sound.h engine/soundenvprop.h
engine/http-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/http-standalone.o: shared/tools.h shared/command.h shared/enum.h
engine/http-standalone.o: shared/geom.h shared/prop.h shared/ents.h
engine/http-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
engine/http-standalone.o: shared/igame.h engine/http.h engine/irc.h
engine/http-standalone.o: engine/sound.h engine/soundenvprop.h support/jsmn.h
engine/irc-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/irc-standalone.o: shared/tools.h shared/command.h shared/enum.h
engine/irc-standalone.o: shared/geom.h shared/prop.h shared/ents.h
engine/irc-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
engine/irc-standalone.o: shared/igame.h engine/http.h engine/irc.h
engine/irc-standalone.o: engine/sound.h engine/soundenvprop.h
engine/master-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/master-standalone.o: shared/tools.h shared/command.h shared/enum.h
engine/master-standalone.o: shared/geom.h shared/prop.h shared/ents.h
engine/master-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
engine/master-standalone.o: shared/igame.h engine/http.h engine/irc.h
engine/master-standalone.o: engine/sound.h engine/soundenvprop.h
engine/server-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/server-standalone.o: shared/tools.h shared/command.h shared/enum.h
engine/server-standalone.o: shared/geom.h shared/prop.h shared/ents.h
engine/server-standalone.o: engine/wind.h engine/fx.h shared/iengine.h
engine/server-standalone.o: shared/igame.h engine/http.h engine/irc.h
engine/server-standalone.o: engine/sound.h engine/soundenvprop.h
game/server-standalone.o: game/game.h game/gamelog.h engine/engine.h engine/version.h
game/server-standalone.o: shared/cube.h shared/tools.h shared/command.h
game/server-standalone.o: shared/enum.h shared/geom.h shared/prop.h
game/server-standalone.o: shared/ents.h engine/wind.h engine/fx.h
game/server-standalone.o: shared/iengine.h shared/igame.h engine/http.h
game/server-standalone.o: engine/irc.h engine/sound.h engine/soundenvprop.h
game/server-standalone.o: game/gamemode.h game/weapons.h game/weapdef.h
game/server-standalone.o: game/player.h game/teamdef.h game/playerdef.h
game/server-standalone.o: game/vars.h game/capture.h game/defend.h
game/server-standalone.o: game/bomber.h game/auth.h game/capturemode.h
game/server-standalone.o: game/defendmode.h game/bombermode.h game/duelmut.h
game/server-standalone.o: game/aiman.h

shared/cube.h.gch: shared/tools.h shared/command.h shared/enum.h
shared/cube.h.gch: shared/geom.h shared/prop.h shared/ents.h engine/wind.h
shared/cube.h.gch: engine/fx.h shared/glexts.h shared/glemu.h
shared/cube.h.gch: shared/iengine.h shared/igame.h
engine/engine.h.gch: engine/version.h shared/cube.h shared/tools.h
engine/engine.h.gch: shared/command.h shared/enum.h shared/geom.h
engine/engine.h.gch: shared/prop.h shared/ents.h engine/wind.h engine/fx.h
engine/engine.h.gch: shared/glexts.h shared/glemu.h shared/iengine.h
engine/engine.h.gch: shared/igame.h engine/http.h engine/irc.h engine/world.h
engine/engine.h.gch: engine/octa.h engine/light.h engine/texture.h
engine/engine.h.gch: engine/bih.h engine/model.h engine/sound.h
engine/engine.h.gch: engine/soundenvprop.h
