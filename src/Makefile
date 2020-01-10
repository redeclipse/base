APPNAME=redeclipse
APPCLIENT=$(APPNAME)
APPSERVER=$(APPNAME)_server

PLATFORM= $(shell $(CC) -dumpmachine)
RMFLAGS= -fv
PKG_CONFIG ?= pkg-config

ifeq (,$(PLATFORM_BUILD))
PLATFORM_BUILD=0
endif
ifeq (,$(PLATFORM_BRANCH))
PLATFORM_BRANCH="none"
endif
ifeq (,$(PLATFORM_REVISION))
PLATFORM_REVISION=""
endif

PLATFORM_BIN?=$(shell ./get_binarch.sh $(PLATFORM))

#CXXFLAGS= -ggdb3
ifneq (,$(filter amd64 x86,$(PLATFORM_BIN)))
CXXFLAGS?= -O3 -fomit-frame-pointer -ffast-math
else
CXXFLAGS?= -O3 -fomit-frame-pointer
endif
override CXXFLAGS+= -Wall -fsigned-char -fno-exceptions -fno-rtti -Wno-invalid-offsetof

ifeq (,$(INSTDIR))
INSTDIR=../bin/$(PLATFORM_BIN)
endif

TOOLSET_PREFIX=
ifneq (,$(findstring cross,$(PLATFORM)))
ifneq (,$(findstring 64,$(PLATFORM)))
TOOLSET_PREFIX=x86_64-w64-mingw32-
else
TOOLSET_PREFIX=i686-w64-mingw32-
endif
CXX=g++
CC=gcc
endif

CXX_TEMP:=$(CXX)
CC_TEMP:=$(CC)
override CXX=$(TOOLSET_PREFIX)$(CXX_TEMP)
override CC=$(TOOLSET_PREFIX)$(CC_TEMP)

INCLUDES= -DVERSION_BUILD=$(PLATFORM_BUILD) -DVERSION_BRANCH=\"$(PLATFORM_BRANCH)\" -DVERSION_REVISION=\"$(PLATFORM_REVISION)\" -I. -Ishared -Iengine -Igame -Ienet/include -Isupport
ifneq (,$(WANT_STEAM))
override INCLUDES+= -DUSE_STEAM=1 -Isteam
endif
ifneq (,$(WANT_DISCORD))
ifneq (,$(findstring mingw,$(PLATFORM)))
override INCLUDES+= -DUSE_DISCORD=1
else
ifneq (,$(findstring x86_64,$(PLATFORM)))
override INCLUDES+= -DUSE_DISCORD=1
endif
endif
endif

STRIP=
ifeq (,$(findstring -g,$(CXXFLAGS)))
ifeq (,$(findstring -pg,$(CXXFLAGS)))
	STRIP=strip
endif
endif
STRIP_TEMP:=$(STRIP)
override STRIP=$(TOOLSET_PREFIX)$(STRIP_TEMP)

MV=mv
CP=cp
MKDIR=mkdir -p

ifneq (,$(findstring mingw,$(PLATFORM)))
APPMODIFIER=_windows
BIN_SUFFIX=.exe
WINDRES=windres
WINDRES_TEMP:=$(WINDRES)
WINLIB=lib/$(PLATFORM_BIN)
WINDLL=../bin/$(PLATFORM_BIN)
override WINDRES=$(TOOLSET_PREFIX)$(WINDRES_TEMP)
STD_LIBS= -static-libgcc -static-libstdc++
CLIENT_INCLUDES= $(INCLUDES) -Iinclude
CLIENT_LIBS= -mwindows $(STD_LIBS) -L$(WINDLL) -L$(WINLIB) -lSDL2 -lSDL2_image -lSDL2_mixer -lzlib1 -lopengl32 -lenet -lws2_32 -lwinmm
ifneq (,$(findstring x86_64,$(PLATFORM)))
override WINDRES+= -F pe-x86-64
ifneq (,$(WANT_STEAM))
override CLIENT_LIBS+= -lsteam_api64
endif
else
override WINDRES+= -F pe-i386
ifneq (,$(WANT_STEAM))
override CLIENT_LIBS+= -lsteam_api
endif
endif
ifneq (,$(WANT_DISCORD))
override CLIENT_LIBS+= -ldiscord-rpc
endif
else
APPMODIFIER=
ifneq (,$(findstring linux,$(PLATFORM)))
BIN_SUFFIX=_linux
else
ifneq (,$(findstring bsd,$(PLATFORM)))
BIN_SUFFIX=_bsd
ifneq (,$(findstring openbsd,$(PLATFORM)))
RMFLAGS= -f
endif
else
BIN_SUFFIX=_native
endif
endif
CLIENT_INCLUDES= $(INCLUDES) `$(PKG_CONFIG) --cflags x11 sdl2 SDL2_image SDL2_mixer zlib gl`
CLIENT_LIBS= -Lenet -lenet `$(PKG_CONFIG) --libs x11 sdl2 SDL2_image SDL2_mixer zlib gl`
ifneq (,$(WANT_STEAM))
override CLIENT_LIBS+= -L../bin/$(PLATFORM_BIN) -lsteam_api
endif
ifneq (,$(WANT_DISCORD))
ifneq (,$(findstring x86_64,$(PLATFORM)))
override CLIENT_LIBS+= -L../bin/$(PLATFORM_BIN) -ldiscord-rpc
endif
endif
endif
ifneq (,$(findstring linux,$(PLATFORM)))
override CLIENT_LIBS+= -lrt
else
ifneq (,$(findstring gnu,$(PLATFORM)))
override CLIENT_LIBS+= -lrt
endif
endif
CLIENT_OBJS= \
	shared/crypto.o \
	shared/geom.o \
	shared/glemu.o \
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
	engine/grass.o \
	engine/http.o \
	engine/irc.o \
	engine/light.o \
	engine/main.o \
	engine/master.o \
	engine/material.o \
	engine/menus.o \
	engine/movie.o \
	engine/normal.o \
	engine/octa.o \
	engine/octaedit.o \
	engine/octarender.o \
	engine/physics.o \
	engine/pvs.o \
	engine/rendergl.o \
	engine/renderlights.o \
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

CLIENT_PCH= shared/cube.h.gch engine/engine.h.gch

ifneq (,$(findstring mingw,$(PLATFORM)))
SERVER_INCLUDES= -DSTANDALONE $(INCLUDES) -Iinclude
SERVER_LIBS= -mwindows $(STD_LIBS) -L$(WINDLL) -L$(WINLIB) -lzlib1 -lenet -lws2_32 -lwinmm
CTFONT_INCLUDES= -DSTANDALONE -Iinclude
CTFONT_LIBS= -mwindows $(STD_LIBS) -L$(WINDLL) -L$(WINLIB) -lzlib1 -lfreetype
ifneq (,$(WANT_STEAM))
ifneq (,$(findstring x86_64,$(PLATFORM)))
override SERVER_LIBS+= -lsteam_api64
else
override SERVER_LIBS+= -lsteam_api
endif
endif
else
SERVER_INCLUDES= -DSTANDALONE $(INCLUDES) `pkg-config --cflags zlib`
SERVER_LIBS= -Lenet -lenet `pkg-config --libs zlib`
CTFONT_INCLUDES= -DSTANDALONE `pkg-config --cflags zlib freetype2`
CTFONT_LIBS= `pkg-config --libs zlib freetype2`
ifneq (,$(WANT_STEAM))
override SERVER_LIBS+= -L../bin/$(PLATFORM_BIN) -lsteam_api
endif
endif
SERVER_OBJS= \
	shared/crypto-standalone.o \
	shared/geom-standalone.o \
	shared/stream-standalone.o \
	shared/tools-standalone.o \
	shared/zip-standalone.o \
	support/jsmn.o \
	support/sqlite3.o \
	engine/cdpi-standalone.o \
	engine/command-standalone.o \
	engine/http-standalone.o \
	engine/irc-standalone.o \
	engine/master-standalone.o \
	engine/server-standalone.o \
	game/server-standalone.o

LIBENET= enet/libenet.a

GENKEY_OBJS= shared/genkey.o shared/crypto-standalone.o
CTFONT_OBJS= shared/cube2font.o

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

clean-cube2font:
	@rm $(RMFLAGS) $(CTFONT_OBJS) cube2font$(BIN_SUFFIX)

clean: clean-enet clean-client clean-server clean-genkey clean-cube2font

%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $(subst .h.gch,.tmp.h.gch,$@) $(subst .h.gch,.h,$@)
	$(MV) $(subst .h.gch,.tmp.h.gch,$@) $@

%-standalone.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(subst -standalone.o,.cpp,$@)

support/sqlite3.o: support/sqlite3.c
	$(CC) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c -o support/sqlite3.o support/sqlite3.c

support/jsmn.o: support/jsmn.c
	$(CC) -c -o support/jsmn.o support/jsmn.c

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)
$(filter shared/%,$(CLIENT_OBJS)): $(filter shared/%,$(CLIENT_PCH))
$(filter engine/%,$(CLIENT_OBJS)): $(filter engine/%,$(CLIENT_PCH))
$(filter game/%,$(CLIENT_OBJS)): $(filter game/%,$(CLIENT_PCH))

$(SERVER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)

$(APPCLIENT)_windows$(BIN_SUFFIX): $(CLIENT_OBJS)
	$(WINDRES) -i $(APPNAME).rc -J rc -o $(APPNAME).res -O coff
	$(CXX) $(CXXFLAGS) -o $@ $(APPNAME).res $(CLIENT_OBJS) $(CLIENT_LIBS)

$(APPSERVER)_windows$(BIN_SUFFIX): $(SERVER_OBJS)
	$(WINDRES) -i $(APPNAME).rc -J rc -o $(APPNAME).res -O coff
	$(CXX) $(CXXFLAGS) -o $@ $(APPNAME).res $(SERVER_OBJS) $(SERVER_LIBS)

$(APPCLIENT)$(BIN_SUFFIX): $(LIBENET) $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(CLIENT_OBJS) $(CLIENT_LIBS)

$(APPSERVER)$(BIN_SUFFIX): $(LIBENET) $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(SERVER_LIBS)

client: $(APPCLIENT)$(APPMODIFIER)$(BIN_SUFFIX)

server: $(APPSERVER)$(APPMODIFIER)$(BIN_SUFFIX)

genkey: genkey$(BIN_SUFFIX)

genkey$(BIN_SUFFIX): $(GENKEY_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(GENKEY_OBJS) $(SERVER_LIBS)

shared/genkey.o: shared/genkey.cpp
	$(CXX) $(CXXFLAGS) $(SERVER_INCLUDES) -c -o shared/genkey.o shared/genkey.cpp

cube2font: cube2font$(BIN_SUFFIX)

cube2font$(BIN_SUFFIX): $(CTFONT_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(CTFONT_OBJS) $(CTFONT_LIBS)

shared/cube2font.o: shared/cube2font.cpp
	$(CXX) $(CXXFLAGS) $(CTFONT_INCLUDES) -c -o shared/cube2font.o shared/cube2font.cpp

$(INSTDIR)/%$(BIN_SUFFIX): %$(APPMODIFIER)$(BIN_SUFFIX)
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

$(INSTDIR)/cube2font$(BIN_SUFFIX): cube2font$(BIN_SUFFIX)
	$(MKDIR) $(INSTDIR)
	$(CP) $< $@
ifneq (,$(STRIP))
	$(STRIP) $@
endif

install-client: $(INSTDIR)/$(APPCLIENT)$(BIN_SUFFIX)

install-server: $(INSTDIR)/$(APPSERVER)$(BIN_SUFFIX)

install-genkey: $(INSTDIR)/genkey$(BIN_SUFFIX)

install-cube2font: $(INSTDIR)/cube2font$(BIN_SUFFIX)

install: install-client install-server install-genkey install-cube2font

$(LIBENET):
	$(MAKE) -C enet

depend:
	makedepend -Y -I. -Ishared -Iengine -Igame -Isupport $(subst .o,.cpp,$(CLIENT_OBJS))
	makedepend -a -o-standalone.o -Y -I. -Ishared -Iengine -Igame -Isupport -DSTANDALONE $(subst -standalone.o,.cpp,$(SERVER_OBJS))
	makedepend -a -o.h.gch -Y -I. -Ishared -Iengine -Igame -Isupport $(subst .h.gch,.h,$(CLIENT_PCH))

all: client server genkey cube2font

include system-install.mk
include dist.mk
include wiki.mk

engine/engine.h.gch: engine/version.h shared/cube.h.gch

# DO NOT DELETE

shared/crypto.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/crypto.o: shared/ents.h shared/glexts.h shared/glemu.h
shared/crypto.o: shared/iengine.h shared/igame.h
shared/geom.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/geom.o: shared/ents.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/geom.o: shared/igame.h
shared/glemu.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/glemu.o: shared/ents.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/glemu.o: shared/igame.h
shared/stream.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/stream.o: shared/ents.h shared/glexts.h shared/glemu.h
shared/stream.o: shared/iengine.h shared/igame.h
shared/tools.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/tools.o: shared/ents.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/tools.o: shared/igame.h
shared/zip.o: shared/cube.h shared/tools.h shared/command.h shared/geom.h
shared/zip.o: shared/ents.h shared/glexts.h shared/glemu.h shared/iengine.h
shared/zip.o: shared/igame.h
engine/aa.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/aa.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/aa.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/aa.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/aa.o: engine/texture.h engine/bih.h engine/model.h
engine/bih.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/bih.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/bih.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/bih.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/bih.o: engine/texture.h engine/bih.h engine/model.h
engine/blend.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/blend.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/blend.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/blend.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/blend.o: engine/texture.h engine/bih.h engine/model.h
engine/cdpi.o: engine/engine.h engine/version.h shared/cube.h
engine/cdpi.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/cdpi.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/cdpi.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/cdpi.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/cdpi.o: engine/model.h
engine/client.o: engine/engine.h engine/version.h shared/cube.h
engine/client.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/client.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/client.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/client.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/client.o: engine/model.h
engine/command.o: engine/engine.h engine/version.h shared/cube.h
engine/command.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/command.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/command.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/command.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/command.o: engine/model.h
engine/console.o: engine/engine.h engine/version.h shared/cube.h
engine/console.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/console.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/console.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/console.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/console.o: engine/model.h
engine/dynlight.o: engine/engine.h engine/version.h shared/cube.h
engine/dynlight.o: shared/tools.h shared/command.h shared/geom.h
engine/dynlight.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/dynlight.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/dynlight.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/dynlight.o: engine/texture.h engine/bih.h engine/model.h
engine/grass.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/grass.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/grass.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/grass.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/grass.o: engine/texture.h engine/bih.h engine/model.h
engine/http.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/http.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/http.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/http.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/http.o: engine/texture.h engine/bih.h engine/model.h support/jsmn.h
engine/irc.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/irc.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/irc.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/irc.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/irc.o: engine/texture.h engine/bih.h engine/model.h
engine/light.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/light.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/light.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/light.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/light.o: engine/texture.h engine/bih.h engine/model.h
engine/main.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/main.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/main.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/main.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/main.o: engine/texture.h engine/bih.h engine/model.h
engine/master.o: engine/engine.h engine/version.h shared/cube.h
engine/master.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/master.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/master.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/master.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/master.o: engine/model.h support/sqlite3.h
engine/material.o: engine/engine.h engine/version.h shared/cube.h
engine/material.o: shared/tools.h shared/command.h shared/geom.h
engine/material.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/material.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/material.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/material.o: engine/texture.h engine/bih.h engine/model.h
engine/menus.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/menus.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/menus.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/menus.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/menus.o: engine/texture.h engine/bih.h engine/model.h
engine/movie.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/movie.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/movie.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/movie.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/movie.o: engine/texture.h engine/bih.h engine/model.h
engine/normal.o: engine/engine.h engine/version.h shared/cube.h
engine/normal.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/normal.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/normal.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/normal.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/normal.o: engine/model.h
engine/octa.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/octa.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/octa.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/octa.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/octa.o: engine/texture.h engine/bih.h engine/model.h
engine/octaedit.o: engine/engine.h engine/version.h shared/cube.h
engine/octaedit.o: shared/tools.h shared/command.h shared/geom.h
engine/octaedit.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/octaedit.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/octaedit.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/octaedit.o: engine/texture.h engine/bih.h engine/model.h
engine/octarender.o: engine/engine.h engine/version.h shared/cube.h
engine/octarender.o: shared/tools.h shared/command.h shared/geom.h
engine/octarender.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/octarender.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/octarender.o: engine/sound.h engine/world.h engine/octa.h
engine/octarender.o: engine/light.h engine/texture.h engine/bih.h
engine/octarender.o: engine/model.h
engine/physics.o: engine/engine.h engine/version.h shared/cube.h
engine/physics.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/physics.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/physics.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/physics.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/physics.o: engine/model.h engine/mpr.h
engine/pvs.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/pvs.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/pvs.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/pvs.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/pvs.o: engine/texture.h engine/bih.h engine/model.h
engine/rendergl.o: engine/engine.h engine/version.h shared/cube.h
engine/rendergl.o: shared/tools.h shared/command.h shared/geom.h
engine/rendergl.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/rendergl.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/rendergl.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/rendergl.o: engine/texture.h engine/bih.h engine/model.h
engine/renderlights.o: engine/engine.h engine/version.h shared/cube.h
engine/renderlights.o: shared/tools.h shared/command.h shared/geom.h
engine/renderlights.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/renderlights.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/renderlights.o: engine/sound.h engine/world.h engine/octa.h
engine/renderlights.o: engine/light.h engine/texture.h engine/bih.h
engine/renderlights.o: engine/model.h
engine/rendermodel.o: engine/engine.h engine/version.h shared/cube.h
engine/rendermodel.o: shared/tools.h shared/command.h shared/geom.h
engine/rendermodel.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/rendermodel.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/rendermodel.o: engine/sound.h engine/world.h engine/octa.h
engine/rendermodel.o: engine/light.h engine/texture.h engine/bih.h
engine/rendermodel.o: engine/model.h engine/ragdoll.h engine/animmodel.h
engine/rendermodel.o: engine/vertmodel.h engine/skelmodel.h engine/hitzone.h
engine/rendermodel.o: engine/md2.h engine/md3.h engine/md5.h engine/obj.h
engine/rendermodel.o: engine/smd.h engine/iqm.h
engine/renderparticles.o: engine/engine.h engine/version.h shared/cube.h
engine/renderparticles.o: shared/tools.h shared/command.h shared/geom.h
engine/renderparticles.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/renderparticles.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/renderparticles.o: engine/sound.h engine/world.h engine/octa.h
engine/renderparticles.o: engine/light.h engine/texture.h engine/bih.h
engine/renderparticles.o: engine/model.h engine/explosion.h
engine/renderparticles.o: engine/lensflare.h engine/lightning.h
engine/rendersky.o: engine/engine.h engine/version.h shared/cube.h
engine/rendersky.o: shared/tools.h shared/command.h shared/geom.h
engine/rendersky.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/rendersky.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/rendersky.o: engine/sound.h engine/world.h engine/octa.h
engine/rendersky.o: engine/light.h engine/texture.h engine/bih.h
engine/rendersky.o: engine/model.h
engine/rendertext.o: engine/engine.h engine/version.h shared/cube.h
engine/rendertext.o: shared/tools.h shared/command.h shared/geom.h
engine/rendertext.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/rendertext.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/rendertext.o: engine/sound.h engine/world.h engine/octa.h
engine/rendertext.o: engine/light.h engine/texture.h engine/bih.h
engine/rendertext.o: engine/model.h
engine/renderva.o: engine/engine.h engine/version.h shared/cube.h
engine/renderva.o: shared/tools.h shared/command.h shared/geom.h
engine/renderva.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/renderva.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/renderva.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/renderva.o: engine/texture.h engine/bih.h engine/model.h
engine/server.o: engine/engine.h engine/version.h shared/cube.h
engine/server.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/server.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/server.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/server.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/server.o: engine/model.h
engine/serverbrowser.o: engine/engine.h engine/version.h shared/cube.h
engine/serverbrowser.o: shared/tools.h shared/command.h shared/geom.h
engine/serverbrowser.o: shared/ents.h shared/glexts.h shared/glemu.h
engine/serverbrowser.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/serverbrowser.o: engine/sound.h engine/world.h engine/octa.h
engine/serverbrowser.o: engine/light.h engine/texture.h engine/bih.h
engine/serverbrowser.o: engine/model.h
engine/shader.o: engine/engine.h engine/version.h shared/cube.h
engine/shader.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/shader.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/shader.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/shader.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/shader.o: engine/model.h
engine/sound.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/sound.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/sound.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/sound.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/sound.o: engine/texture.h engine/bih.h engine/model.h
engine/stain.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/stain.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/stain.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/stain.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/stain.o: engine/texture.h engine/bih.h engine/model.h
engine/texture.o: engine/engine.h engine/version.h shared/cube.h
engine/texture.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/texture.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/texture.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/texture.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/texture.o: engine/model.h
engine/ui.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/ui.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/ui.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/ui.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/ui.o: engine/texture.h engine/bih.h engine/model.h engine/textedit.h
engine/water.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/water.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/water.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/water.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/water.o: engine/texture.h engine/bih.h engine/model.h
engine/world.o: engine/engine.h engine/version.h shared/cube.h shared/tools.h
engine/world.o: shared/command.h shared/geom.h shared/ents.h shared/glexts.h
engine/world.o: shared/glemu.h shared/iengine.h shared/igame.h engine/http.h engine/irc.h
engine/world.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
engine/world.o: engine/texture.h engine/bih.h engine/model.h
engine/worldio.o: engine/engine.h engine/version.h shared/cube.h
engine/worldio.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
engine/worldio.o: shared/glexts.h shared/glemu.h shared/iengine.h
engine/worldio.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
engine/worldio.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
engine/worldio.o: engine/model.h
game/ai.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/ai.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/ai.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/ai.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/ai.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/ai.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/ai.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/ai.o: game/capture.h game/defend.h game/bomber.h
game/client.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/client.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/client.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/client.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/client.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/client.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/client.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/client.o: game/capture.h game/defend.h game/bomber.h
game/capture.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/capture.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/capture.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/capture.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
game/capture.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/capture.o: engine/model.h game/gamemode.h game/weapons.h game/weapdef.h
game/capture.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/capture.o: game/ai.h game/capture.h game/defend.h game/bomber.h
game/defend.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/defend.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/defend.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/defend.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/defend.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/defend.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/defend.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/defend.o: game/capture.h game/defend.h game/bomber.h
game/bomber.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/bomber.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/bomber.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/bomber.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/bomber.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/bomber.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/bomber.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/bomber.o: game/capture.h game/defend.h game/bomber.h
game/entities.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/entities.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/entities.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/entities.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
game/entities.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/entities.o: engine/model.h game/gamemode.h game/weapons.h game/weapdef.h
game/entities.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/entities.o: game/ai.h game/capture.h game/defend.h game/bomber.h
game/game.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/game.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/game.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/game.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/game.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/game.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/game.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/game.o: game/capture.h game/defend.h game/bomber.h
game/hud.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/hud.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/hud.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/hud.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/hud.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/hud.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/hud.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/hud.o: game/capture.h game/defend.h game/bomber.h game/compass.h
game/physics.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/physics.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/physics.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/physics.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
game/physics.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/physics.o: engine/model.h game/gamemode.h game/weapons.h game/weapdef.h
game/physics.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/physics.o: game/ai.h game/capture.h game/defend.h game/bomber.h
game/projs.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/projs.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/projs.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/projs.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/projs.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/projs.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/projs.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/projs.o: game/capture.h game/defend.h game/bomber.h
game/scoreboard.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/scoreboard.o: shared/tools.h shared/command.h shared/geom.h
game/scoreboard.o: shared/ents.h shared/glexts.h shared/glemu.h
game/scoreboard.o: shared/iengine.h shared/igame.h engine/http.h engine/irc.h
game/scoreboard.o: engine/sound.h engine/world.h engine/octa.h engine/light.h
game/scoreboard.o: engine/texture.h engine/bih.h engine/model.h
game/scoreboard.o: game/gamemode.h game/weapons.h game/weapdef.h
game/scoreboard.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/scoreboard.o: game/ai.h game/capture.h game/defend.h game/bomber.h
game/server.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/server.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/server.o: shared/glexts.h shared/glemu.h shared/iengine.h shared/igame.h
game/server.o: engine/http.h engine/irc.h engine/sound.h engine/world.h engine/octa.h
game/server.o: engine/light.h engine/texture.h engine/bih.h engine/model.h
game/server.o: game/gamemode.h game/weapons.h game/weapdef.h game/player.h
game/server.o: game/teamdef.h game/playerdef.h game/vars.h game/ai.h
game/server.o: game/capture.h game/defend.h game/bomber.h game/auth.h
game/server.o: game/capturemode.h game/defendmode.h game/bombermode.h
game/server.o: game/duelmut.h game/aiman.h
game/waypoint.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/waypoint.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/waypoint.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/waypoint.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
game/waypoint.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/waypoint.o: engine/model.h game/gamemode.h game/weapons.h game/weapdef.h
game/waypoint.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/waypoint.o: game/ai.h game/capture.h game/defend.h game/bomber.h
game/weapons.o: game/game.h engine/engine.h engine/version.h shared/cube.h
game/weapons.o: shared/tools.h shared/command.h shared/geom.h shared/ents.h
game/weapons.o: shared/glexts.h shared/glemu.h shared/iengine.h
game/weapons.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h engine/world.h
game/weapons.o: engine/octa.h engine/light.h engine/texture.h engine/bih.h
game/weapons.o: engine/model.h game/gamemode.h game/weapons.h game/weapdef.h
game/weapons.o: game/player.h game/teamdef.h game/playerdef.h game/vars.h
game/weapons.o: game/ai.h game/capture.h game/defend.h game/bomber.h

shared/crypto-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/crypto-standalone.o: shared/geom.h shared/ents.h shared/iengine.h
shared/crypto-standalone.o: shared/igame.h
shared/geom-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/geom-standalone.o: shared/geom.h shared/ents.h shared/iengine.h
shared/geom-standalone.o: shared/igame.h
shared/stream-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/stream-standalone.o: shared/geom.h shared/ents.h shared/iengine.h
shared/stream-standalone.o: shared/igame.h
shared/tools-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/tools-standalone.o: shared/geom.h shared/ents.h shared/iengine.h
shared/tools-standalone.o: shared/igame.h
shared/zip-standalone.o: shared/cube.h shared/tools.h shared/command.h
shared/zip-standalone.o: shared/geom.h shared/ents.h shared/iengine.h
shared/zip-standalone.o: shared/igame.h
engine/cdpi-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/cdpi-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/cdpi-standalone.o: shared/ents.h shared/iengine.h shared/igame.h
engine/cdpi-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/command-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/command-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/command-standalone.o: shared/ents.h shared/iengine.h shared/igame.h
engine/command-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/http-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/http-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/http-standalone.o: shared/ents.h shared/iengine.h shared/igame.h
engine/http-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/irc-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/irc-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/irc-standalone.o: shared/ents.h shared/iengine.h shared/igame.h
engine/irc-standalone.o: engine/http.h engine/irc.h engine/sound.h
engine/master-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/master-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/master-standalone.o: shared/ents.h shared/iengine.h shared/igame.h
engine/master-standalone.o: engine/http.h engine/irc.h engine/sound.h support/sqlite3.h
engine/server-standalone.o: engine/engine.h engine/version.h shared/cube.h
engine/server-standalone.o: shared/tools.h shared/command.h shared/geom.h
engine/server-standalone.o: shared/ents.h shared/iengine.h shared/igame.h
engine/server-standalone.o: engine/http.h engine/irc.h engine/sound.h
game/server-standalone.o: game/game.h engine/engine.h engine/version.h
game/server-standalone.o: shared/cube.h shared/tools.h shared/command.h
game/server-standalone.o: shared/geom.h shared/ents.h shared/iengine.h
game/server-standalone.o: shared/igame.h engine/http.h engine/irc.h engine/sound.h
game/server-standalone.o: game/gamemode.h game/weapons.h game/weapdef.h
game/server-standalone.o: game/player.h game/teamdef.h game/playerdef.h
game/server-standalone.o: game/vars.h game/capture.h game/defend.h
game/server-standalone.o: game/bomber.h game/auth.h game/capturemode.h
game/server-standalone.o: game/defendmode.h game/bombermode.h game/duelmut.h
game/server-standalone.o: game/aiman.h

shared/cube.h.gch: shared/tools.h shared/command.h shared/geom.h
shared/cube.h.gch: shared/ents.h shared/glexts.h shared/glemu.h
shared/cube.h.gch: shared/iengine.h shared/igame.h
engine/engine.h.gch: engine/version.h shared/cube.h shared/tools.h
engine/engine.h.gch: shared/command.h shared/geom.h shared/ents.h
engine/engine.h.gch: shared/glexts.h shared/glemu.h shared/iengine.h
engine/engine.h.gch: shared/igame.h engine/http.h engine/irc.h engine/sound.h
engine/engine.h.gch: engine/world.h engine/octa.h engine/light.h
engine/engine.h.gch: engine/texture.h engine/bih.h engine/model.h
