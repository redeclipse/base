appname=$(APPNAME)
appnamefull=$(shell sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' version.h)
appsrcname=$(APPNAME)
cappname=$(shell echo $(appname) | tr '[:lower:]' '[:upper:]')# Captial appname
appclient=$(APPCLIENT)$(APPMODIFIER)$(BIN_SUFFIX)
appserver=$(APPSERVER)$(APPMODIFIER)$(BIN_SUFFIX)

prefix=/usr/local
games=
gamesbin=/bin
bindir=$(DESTDIR)$(prefix)/bin
gamesbindir=$(DESTDIR)$(prefix)$(gamesbin)
libexecdir=$(DESTDIR)$(prefix)/lib$(games)
datadir=$(DESTDIR)$(prefix)/share$(games)
docdir=$(DESTDIR)$(prefix)/share/doc
mandir=$(DESTDIR)$(prefix)/share/man
menudir=$(DESTDIR)$(prefix)/share/applications
icondir=$(DESTDIR)$(prefix)/share/icons/hicolor
pixmapdir=$(DESTDIR)$(prefix)/share/pixmaps

ICONS= \
	install/nix/$(appsrcname)_x16.png \
	install/nix/$(appsrcname)_x32.png \
	install/nix/$(appsrcname)_x48.png \
	install/nix/$(appsrcname)_x64.png \
	install/nix/$(appsrcname)_x128.png \
	install/nix/$(appsrcname)_x32.xpm

install/nix/$(appsrcname)_x16.png: $(ICON)
	gm convert '$<' -trim -resize 16x16 -background transparent \
		-gravity center -extent 16x16 '$@'

install/nix/$(appsrcname)_x32.png: $(ICON)
	gm convert '$<' -trim -resize 32x32 -background transparent \
		-gravity center -extent 32x32 '$@'

install/nix/$(appsrcname)_x48.png: $(ICON)
	gm convert '$<' -trim -resize 48x48 -background transparent \
		-gravity center -extent 48x48 '$@'

install/nix/$(appsrcname)_x64.png: $(ICON)
	gm convert '$<' -trim -resize 64x64 -background transparent \
		-gravity center -extent 64x64 '$@'

install/nix/$(appsrcname)_x128.png: $(ICON)
	gm convert '$<' -trim -resize 128x128 -background transparent \
		-gravity center -extent 128x128 '$@'

install/nix/$(appsrcname)_x32.xpm: $(ICON)
	gm convert '$<' -trim -resize 32x32 -background transparent \
		-gravity center -extent 32x32 '$@'

icons: $(ICONS)

system-install-client: client
	$(MKDIR) $(libexecdir)/$(appname)
	$(MKDIR) $(gamesbindir)
	install -m755 $(appclient) $(libexecdir)/$(appname)/$(appname)
	install -m755 install/nix/$(appsrcname).am \
		$(gamesbindir)/$(appname)
	printf "\
	g,@LIBEXECDIR@,\
	s,@LIBEXECDIR@,$(patsubst $(DESTDIR)%,%,$(libexecdir)),g\n\
	g,@DATADIR@,\
	s,@DATADIR@,$(patsubst $(DESTDIR)%,%,$(datadir)),g\n\
	g,@DOCDIR@,\
	s,@DOCDIR@,$(patsubst $(DESTDIR)%,%,$(docdir)),g\n\
	g,@APPNAME@,\
	s,@APPNAME@,$(appname),g\n\
	w\n" | ed -s $(gamesbindir)/$(appname)
	ln -s $(patsubst $(DESTDIR)%,%,$(datadir))/$(appname)/data \
		$(libexecdir)/$(appname)/data

system-install-server: server
	$(MKDIR) $(libexecdir)/$(appname)
	$(MKDIR) $(gamesbindir)
	$(MKDIR) $(datadir)/$(appname)
	install -m755 $(appserver) \
		$(libexecdir)/$(appname)/$(appname)-server
	install -m755 install/nix/$(appsrcname)-server.am \
		$(gamesbindir)/$(appname)-server
	printf "\
	g,@LIBEXECDIR@,\
	s,@LIBEXECDIR@,$(patsubst $(DESTDIR)%,%,$(libexecdir)),g\n\
	g,@DATADIR@,\
	s,@DATADIR@,$(patsubst $(DESTDIR)%,%,$(datadir)),g\n\
	g,@DOCDIR@,\
	s,@DOCDIR@,$(patsubst $(DESTDIR)%,%,$(docdir)),g\n\
	g,@APPNAME@,\
	s,@APPNAME@,$(appname),g\n\
	w\n" | ed -s $(gamesbindir)/$(appname)-server
	install -m644 ../config/version.cfg \
		$(datadir)/$(appname)/version.cfg
	ln -s $(patsubst $(DESTDIR)%,%,$(datadir))/$(appname)/version.cfg \
		$(libexecdir)/$(appname)/version.cfg

system-install-data:
	cp -r ../data $(datadir)/$(appname)/data
	rm -f $(datadir)/$(appname)/data/misc/largeandincharge.png

system-install-docs: $(MANPAGES)
	$(MKDIR) $(mandir)/man6
	$(MKDIR) $(docdir)/$(appname)
	sed -e 's,@LIBEXECDIR@,$(patsubst $(DESTDIR)%,%,$(libexecdir)),g' \
		-e 's,@DATADIR@,$(patsubst $(DESTDIR)%,%,$(datadir)),g' \
		-e 's,@DOCDIR@,$(patsubst $(DESTDIR)%,%,$(docdir)),g' \
		-e 's,@APPNAME@,$(appname),g' \
		-e 's,@CAPPNAME@,$(cappname),g' \
		../doc/man/$(appsrcname).6.am | \
		gzip -9 -n -c > $(mandir)/man6/$(appname).6.gz
	sed -e 's,@LIBEXECDIR@,$(patsubst $(DESTDIR)%,%,$(libexecdir)),g' \
		-e 's,@DATADIR@,$(patsubst $(DESTDIR)%,%,$(datadir)),g' \
		-e 's,@DOCDIR@,$(patsubst $(DESTDIR)%,%,$(docdir)),g' \
		-e 's,@APPNAME@,$(appname),g' \
		-e 's,@CAPPNAME@,$(cappname),g' \
		../doc/man/$(appsrcname)-server.6.am | \
		gzip -9 -n -c > $(mandir)/man6/$(appname)-server.6.gz
	cp -r ../doc/examples $(docdir)/$(appname)/examples
	cp ../doc/guidelines.txt $(docdir)/$(appname)/guidelines.txt

system-install-menus: icons
	$(MKDIR) $(menudir)
	$(MKDIR) $(icondir)/16x16/apps
	$(MKDIR) $(icondir)/32x32/apps
	$(MKDIR) $(icondir)/48x48/apps
	$(MKDIR) $(icondir)/64x64/apps
	$(MKDIR) $(icondir)/128x128/apps
	$(MKDIR) $(pixmapdir)
	sed -e 's,@LIBEXECDIR@,$(patsubst $(DESTDIR)%,%,$(libexecdir)),g' \
		-e 's,@DATADIR@,$(patsubst $(DESTDIR)%,%,$(datadir)),g' \
		-e 's,@DOCDIR@,$(patsubst $(DESTDIR)%,%,$(docdir)),g' \
		-e 's,@APPNAME@,$(appname),g' \
		install/nix/$(appsrcname).desktop.am > \
		$(menudir)/$(appname).desktop
	install -m644 install/nix/$(appsrcname)_x16.png \
		$(icondir)/16x16/apps/$(appname).png
	install -m644 install/nix/$(appsrcname)_x32.png \
		$(icondir)/32x32/apps/$(appname).png
	install -m644 install/nix/$(appsrcname)_x48.png \
		$(icondir)/48x48/apps/$(appname).png
	install -m644 install/nix/$(appsrcname)_x64.png \
		$(icondir)/64x64/apps/$(appname).png
	install -m644 install/nix/$(appsrcname)_x128.png \
		$(icondir)/128x128/apps/$(appname).png
	install -m644 install/nix/$(appsrcname)_x32.xpm \
		$(pixmapdir)/$(appname).xpm

system-install-cube2font: cube2font system-install-cube2font-docs
	$(MKDIR) $(bindir)
	install -m755 cube2font $(bindir)/cube2font

system-install-cube2font-docs: ../doc/man/cube2font.1
	$(MKDIR) $(mandir)/man1
	gzip -9 -n -c < ../doc/man/cube2font.1 \
		> $(mandir)/man1/cube2font.1.gz

system-install: system-install-client system-install-server system-install-data system-install-docs system-install-menus

system-uninstall-client:
	@rm -fv $(libexecdir)/$(appname)/$(appname)
	@rm -fv $(libexecdir)/$(appname)/data
	@rm -fv	$(libexecdir)/$(appname)/version.cfg
	@rm -fv $(gamesbindir)/$(appname)

system-uninstall-server:
	@rm -fv $(libexecdir)/$(appname)/$(appname)-server
	@rm -fv $(gamesbindir)/$(appname)-server

system-uninstall-data:
	rm -rf $(datadir)/$(appname)/data
	rm -fv $(datadir)/$(appname)/version.cfg

system-uninstall-docs:
	@rm -rfv $(docdir)/$(appname)/examples
	@rm -fv $(docdir)/$(appname)/guidelines.txt
	@rm -fv $(mandir)/man6/$(appname).6.gz
	@rm -fv $(mandir)/man6/$(appname)-server.6.gz

system-uninstall-menus:
	@rm -fv $(menudir)/$(appname).desktop
	@rm -fv $(icondir)/16x16/apps/$(appname).png
	@rm -fv $(icondir)/32x32/apps/$(appname).png
	@rm -fv $(icondir)/48x48/apps/$(appname).png
	@rm -fv $(icondir)/64x64/apps/$(appname).png
	@rm -fv $(icondir)/128x128/apps/$(appname).png
	@rm -fv $(pixmapdir)/$(appname).xpm

system-uninstall: system-uninstall-client system-uninstall-server system-uninstall-data system-uninstall-docs system-uninstall-menus
	-@rmdir -v $(libexecdir)/$(appname)
	-@rmdir -v $(datadir)/$(appname)
	-@rmdir -v $(docdir)/$(appname)

system-uninstall-cube2font-docs:
	@rm -fv $(mandir)/man1/cube2font.1.gz

system-uninstall-cube2font: system-uninstall-cube2font-docs
	@rm -fv $(bindir)/bin/cube2font
