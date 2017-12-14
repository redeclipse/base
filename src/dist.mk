appname=$(APPNAME)
appnamefull=$(shell sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' engine/version.h)
appversion=$(shell sed -n 's/.define VERSION_STRING *"\([^"]*\)"/\1/p' engine/version.h)
apprelease=$(shell sed -n 's/.define VERSION_RELEASE *"\([^"]*\)"/\1/p' engine/version.h)
appvermaj=$(shell sed -n 's/.define VERSION_MAJOR \([0-9]\)/\1/p' engine/version.h)
appvermin=$(shell sed -n 's/.define VERSION_MINOR \([0-9]\)/\1/p' engine/version.h)
appverpat=$(shell sed -n 's/.define VERSION_PATCH \([0-9]\)/\1/p' engine/version.h)
appversion=$(appvermaj).$(appvermin).$(appverpat)
appfiles=https://redeclipse.net/files/stable

dirname=$(appname)-$(appversion)
dirname-mac=$(appname).app
dirname-win=$(dirname)-win

exename=$(appname)_$(appversion)_win.exe
zipname=$(appname)_$(appversion)_win.zip
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
	rm -rfv $@
	tar --exclude=.git --exclude=$(dirname) \
		-cf - $(DISTFILES:%=../%) | (mkdir $@/; cd $@/ ; tar -xpf -)
	echo "stable" > $@/branch.txt
	$(CURL) $(appfiles)/base.txt --output $@/version.txt
	$(CURL) $(appfiles)/bins.txt --output $@/bin/version.txt
	for i in `curl --silent --location --insecure --fail $(appfiles)/mods.txt`; do if [ "$${i}" != "base" ]; then mkdir -p $@/data/$${i}; $(CURL) $(appfiles)/$${i}.txt --output $@/data/$${i}/version.txt; fi; done
	$(CURL) $(appfiles)/windows.zip --output windows.zip
	unzip -o windows.zip -d $@
	rm -fv windows.zip
	$(CURL) $(appfiles)/linux.tar.gz --output linux.tar.gz
	tar --gzip --extract --verbose --overwrite --file=linux.tar.gz --directory=$@
	rm -fv linux.tar.gz
	$(CURL) $(appfiles)/macos.tar.gz --output macos.tar.gz
	tar --gzip --extract --verbose --overwrite --file=macos.tar.gz --directory=$@
	rm -fv macos.tar.gz
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src/enet clean

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
	rm -rfv $@/Contents/Resources/bin/*/$(appname)*linux*
	rm -rfv $@/Contents/Resources/bin/*/$(appname)*bsd*
	rm -rfv $@/Contents/Resources/bin/*/$(appname)*.exe
	rm -rfv $@/Contents/Resources/bin/*/genkey*linux*
	rm -rfv $@/Contents/Resources/bin/*/genkey*bsd*
	rm -rfv $@/Contents/Resources/bin/*/genkey.exe

../$(tarname-mac): ../$(dirname-mac)
	tar -cf $@ $<

dist-tar-mac: ../$(tarname-mac)
	rm -rfv ../$(dirname-mac)

../$(tarname-combined): ../$(dirname)
	tar -cf $@ $<

dist-tar-combined: ../$(tarname-combined)

../$(dirname-win): ../$(dirname)
	cp -R $< $@
	rm -rfv $@/bin/*/$(appname)*linux*
	rm -rfv $@/bin/*/$(appname)*bsd*
	rm -rfv $@/bin/*/genkey*linux*
	rm -rfv $@/bin/*/genkey*bsd*
	rm -rfv $@/bin/$(dirname-mac)/Contents/MacOS/$(appname)_universal

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
	rm -rfv ../$(dirname-mac)

../$(tarname-mac).bz2: ../$(tarname-mac)
	bzip2 -c < $< > $@

dist-bz2-mac: ../$(tarname-mac).bz2
	rm -rfv ../$(dirname-mac)

dist-mac: ../$(tarname-mac).bz2
	rm -rfv ../$(dirname-mac)

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
	sed "s/~REPVERSION~/$(appversion)/g;s/~REPOUTFILE~/$(exename)/g" $</src/install/win/$(appname).nsi > $</src/install/win/$(appversion)_$(appname).nsi
    cat $</src/install/win/$(appversion)_$(appname).nsi
	cd $</src/install/win && makensis -V4 $(appversion)_$(appname).nsi
	$(MV) $</src/install/win/$(exename) ../
	rm -rfv $</src/install/win/$(appversion)_$(appname).nsi

../$(zipname): ../$(dirname-win)
	zip -r "../$(zipname)" $<

dist-win: ../$(exename)
	rm -rfv ../$(dirname-win)

dist-zip: ../$(zipname)
	rm -rfv ../$(dirname-win)

dist: dist-clean dist-bz2 dist-bz2-combined dist-win dist-mac

../$(tarname).bz2.torrent: ../$(tarname).bz2
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname).bz2 \
		-n $(tarname).bz2 \
		-c "Red Eclipse v$(appversion) ($(apprelease)) for GNU/Linux" \
		$(tarname).bz2

dist-torrent-nix: ../$(tarname).bz2.torrent

dist-torrent-bz2: ../$(tarname).bz2.torrent

../$(tarname-mac).bz2.torrent: ../$(tarname-mac).bz2
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname-mac).bz2 \
		-n $(tarname-mac).bz2 \
		-c "$(appnamefull) v$(appversion) ($(apprelease)) for macOS" \
		$(tarname-mac).bz2

dist-torrent-mac: ../$(tarname-mac).bz2.torrent

../$(tarname-combined).bz2.torrent: ../$(tarname-combined).bz2
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname-combined).bz2 \
		-n $(tarname-combined).bz2 \
		-c "$(appnamefull) v$(appversion) ($(apprelease)) Combined Platforms" \
		$(tarname-combined).bz2

dist-torrent-combined: ../$(tarname-combined).bz2.torrent

../$(exename).torrent: ../$(exename)
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(exename) \
		-n $(exename) \
		-c "$(appnamefull) v$(appversion) ($(apprelease)) for Windows" \
		$(exename)

dist-torrent-win: ../$(exename).torrent

dist-torrents: dist-torrent-bz2 dist-torrent-combined dist-torrent-win dist-torrent-mac

dist-mostlyclean:
	rm -rfv ../$(dirname)
	rm -rfv ../$(dirname-win)
	rm -rfv ../$(dirname-mac)
	rm -fv ../$(tarname)
	rm -fv ../$(tarname-mac)
	rm -fv ../$(tarname-combined)

dist-clean: dist-mostlyclean
	rm -fv ../$(tarname)*
	rm -fv ../$(tarname-mac)*
	rm -fv ../$(tarname-combined)*
	rm -fv ../$(exename)*

../doc/cube2font.txt: ../doc/man/cube2font.1
	scripts/cube2font-txt $< $@

cube2font-txt: ../doc/cube2font.txt

../doc/examples/servinit.cfg: ../config/usage.cfg install-server
	scripts/servinit-defaults $@
	scripts/servinit-comments $< $@

update-servinit: ../doc/examples/servinit.cfg
