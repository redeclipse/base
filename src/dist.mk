appname=$(APPNAME)
appnamefull=$(shell sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' engine/version.h)
appversion=$(shell sed -n 's/.define VERSION_STRING *"\([^"]*\)"/\1/p' engine/version.h)
appfiles=http://redeclipse.net/files/stable

dirname=$(appname)-$(appversion)
dirname-mac=$(appname).app
dirname-win=$(dirname)-win

exename=$(appname)_$(appversion)_win.exe

tarname=$(appname)_$(appversion)_nix.tar
tarname-mac=$(appname)_$(appversion)_mac.tar
tarname-combined=$(appname)_$(appversion)_combined.tar

torrent-trackers-url="udp://tracker.openbittorrent.com:80,udp://tracker.publicbt.com:80,udp://open.demonii.com:1337,udp://tracker.coppersurfer.tk:6969,udp://tracker.leechers-paradise.org:6969"
torrent-webseed-baseurl="http://redeclipse.net/files/releases"

MAC_APP=
ifeq ($(APPNAME),redeclipse)
MAC_APP=bin/$(APPNAME).app
endif

DISTFILES=$(shell cd ../ && find . -not -iname . -not -iname *.lo -not -iname *.gch -not -iname *.o || echo "")
CURL=curl --location --insecure --fail

../$(dirname):
	rm -rf $@
	tar --exclude=.git --exclude=$(dirname) \
		-cf - $(DISTFILES:%=../%) | (mkdir $@/; cd $@/ ; tar -xpf -)
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src/enet clean
	echo "stable" > $@/branch.txt
	$(CURL) $(appfiles)/base.txt --output $@/version.txt
	$(CURL) $(appfiles)/bins.txt --output $@/bin/version.txt
	for i in `curl --silent --location --insecure --fail $(appfiles)/mods.txt`; do if [ "$${i}" != "base" ]; then mkdir -p $@/data/$${i}; $(CURL) $(appfiles)/$${i}.txt --output $@/data/$${i}/version.txt; fi; done
	$(CURL) $(appfiles)/linux.tar.gz --output linux.tar.gz
	tar --gzip --extract --verbose --overwrite --file=linux.tar.gz --directory=$@
	rm -f linux.tar.gz
	$(CURL) $(appfiles)/macos.tar.gz --output macos.tar.gz
	tar --gzip --extract --verbose --overwrite --file=macos.tar.gz --directory=$@
	rm -f macos.tar.gz
	$(CURL) $(appfiles)/windows.zip --output windows.zip
	unzip -o windows.zip -d $@
	rm -f windows.zip

distdir: ../$(dirname)

../$(tarname): ../$(dirname)
	tar \
		--exclude='$</bin/*/$(appname)*.exe' \
		--exclude='$</bin/*/genkey*' \
		--exclude='$</bin/$(dirname-mac)/Contents/MacOS/$(appname)_universal' \
		-cf $@ $<

dist-tar: ../$(tarname)

../$(dirname-mac): ../$(dirname)
	cp -R $</bin/$(dirname-mac) $@
	cp -R $</* $@/Contents/Resources
	rm -rf $@/Contents/Resources/bin/*/$(appname)*linux*
	rm -rf $@/Contents/Resources/bin/*/$(appname)*bsd*
	rm -rf $@/Contents/Resources/bin/*/$(appname)*.exe
	rm -rf $@/Contents/Resources/bin/*/genkey*linux*
	rm -rf $@/Contents/Resources/bin/*/genkey*bsd*
	rm -rf $@/Contents/Resources/bin/*/genkey.exe

../$(tarname-mac): ../$(dirname-mac)
	tar -cf $@ $<

dist-tar-mac: ../$(tarname-mac)
	rm -rf ../$(dirname-mac)

../$(tarname-combined): ../$(dirname)
	tar -cf $@ $<

dist-tar-combined: ../$(tarname-combined)

../$(dirname-win): ../$(dirname)
	cp -R $< $@
	rm -rf $@/bin/*/$(appname)*linux*
	rm -rf $@/bin/*/$(appname)*bsd*
	rm -rf $@/bin/*/genkey*linux*
	rm -rf $@/bin/*/genkey*bsd*
	rm -rf $@/bin/$(dirname-mac)/Contents/MacOS/$(appname)_universal

distdir-win: ../$(dirname-win)

../$(tarname).gz: ../$(tarname)
	gzip -c < $< > $@

dist-gz: ../$(tarname).gz

../$(tarname).bz2: ../$(tarname)
	bzip2 -c < $< > $@

dist-bz2: ../$(tarname).bz2

dist-nix: ../$(tarname).bz2

../$(tarname).xz: ../$(tarname)
	xz -c < $< > $@

dist-xz: ../$(tarname).xz

../$(tarname-mac).gz: ../$(tarname-mac)
	gzip -c < $< > $@

dist-gz-mac: ../$(tarname-mac).gz
	rm -rf ../$(dirname-mac)

../$(tarname-mac).bz2: ../$(tarname-mac)
	bzip2 -c < $< > $@

dist-bz2-mac: ../$(tarname-mac).bz2
	rm -rf ../$(dirname-mac)

dist-mac: ../$(tarname-mac).bz2
	rm -rf ../$(dirname-mac)

../$(tarname-mac).xz: ../$(tarname-mac)
	xz -c < $< > $@

dist-xz-mac: ../$(tarname-mac).xz

../$(tarname-combined).gz: ../$(tarname-combined)
	gzip -c < $< > $@

dist-gz-combined: ../$(tarname-combined).gz

../$(tarname-combined).bz2: ../$(tarname-combined)
	bzip2 -c < $< > $@

dist-bz2-combined: ../$(tarname-combined).bz2

dist-combined: ../$(tarname-combined).bz2

../$(tarname-combined).xz: ../$(tarname-combined)
	xz -c < $< > $@

dist-xz-combined: ../$(tarname-combined).xz

../$(exename): ../$(dirname-win)
	makensis -V2 $</src/install/win/$(appname).nsi
	$(MV) $</src/install/win/$(exename) ../

dist-win: ../$(exename)
	rm -rf ../$(dirname-win)

dist: dist-clean dist-bz2 dist-bz2-combined dist-win dist-mac

../$(tarname).bz2.torrent: ../$(tarname).bz2
	rm -f $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname).bz2 \
		-n $(tarname).bz2 \
		-c "Red Eclipse $(appversion) for GNU/Linux" \
		$(tarname).bz2

dist-torrent-nix: ../$(tarname).bz2.torrent

dist-torrent-bz2: ../$(tarname).bz2.torrent

../$(tarname-mac).bz2.torrent: ../$(tarname-mac).bz2
	rm -f $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname-mac).bz2 \
		-n $(tarname-mac).bz2 \
		-c "$(appnamefull) $(appversion) for Mac mac" \
		$(tarname-mac).bz2

dist-torrent-mac: ../$(tarname-mac).bz2.torrent

../$(tarname-combined).bz2.torrent: ../$(tarname-combined).bz2
	rm -f $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname-combined).bz2 \
		-n $(tarname-combined).bz2 \
		-c "$(appnamefull) $(appversion) Combined Platforms" \
		$(tarname-combined).bz2

dist-torrent-combined: ../$(tarname-combined).bz2.torrent

../$(exename).torrent: ../$(exename)
	rm -f $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(exename) \
		-n $(exename) \
		-c "$(appnamefull) $(appversion) for Windows" \
		$(exename)

dist-torrent-win: ../$(exename).torrent

dist-torrents: dist-torrent-bz2 dist-torrent-combined dist-torrent-win dist-torrent-mac

dist-mostlyclean:
	rm -rf ../$(dirname)
	rm -rf ../$(dirname-win)
	rm -rf ../$(dirname-mac)
	rm -f ../$(tarname)
	rm -f ../$(tarname-mac)
	rm -f ../$(tarname-combined)

dist-clean: dist-mostlyclean
	rm -f ../$(tarname)*
	rm -f ../$(tarname-mac)*
	rm -f ../$(tarname-combined)*
	rm -f ../$(exename)*

../doc/cube2font.txt: ../doc/man/cube2font.1
	scripts/cube2font-txt $< $@

cube2font-txt: ../doc/cube2font.txt

../doc/examples/servinit.cfg: ../config/usage.cfg install-server
	scripts/servinit-defaults $@
	scripts/servinit-comments $< $@

update-servinit: ../doc/examples/servinit.cfg
