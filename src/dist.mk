appname=$(APPNAME)
appnamefull=$(shell sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' version.h)
appversion=$(shell sed -n 's/.define VERSION_STRING *"\([^"]*\)"/\1/p' version.h)
appfiles=http://redeclipse.net/files/stable

dirname=$(appname)-$(appversion)
dirname-osx=$(appname).app
dirname-win=$(dirname)-win

exename=$(appname)_$(appversion)_win.exe

tarname=$(appname)_$(appversion)_nix.tar
tarname-osx=$(appname)_$(appversion)_osx.tar
tarname-combined=$(appname)_$(appversion)_combined.tar

torrent-trackers-url="udp://tracker.openbittorrent.com:80,udp://tracker.publicbt.com:80,udp://open.demonii.com:1337,udp://tracker.coppersurfer.tk:6969,udp://tracker.leechers-paradise.org:6969"
torrent-webseed-baseurl="http://redeclipse.net/files/releases"

OSX_APP=
ifeq ($(APPNAME),redeclipse)
OSX_APP=bin/$(APPNAME).app
endif

DISTFILES=$(shell cd ../ && find . -not -iname . -not -iname *.lo -not -iname *.gch -not -iname *.o || echo "")
CURL=curl --write-out "[%{http_code}] %{filename_effective} (%{content_type})\n%{size_download} Bytes at %{speed_download}/Bps in %{time_total}s\n" --location --insecure --fail

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
	$(CURL) $(appfiles)/macosx.tar.gz --output macosx.tar.gz
	tar --gzip --extract --verbose --overwrite --file=macosx.tar.gz --directory=$@
	rm -f macosx.tar.gz
	$(CURL) $(appfiles)/windows.zip --output windows.zip
	unzip -o windows.zip -d $@
	rm -f windows.zip

distdir: ../$(dirname)

../$(tarname): ../$(dirname)
	tar \
		--exclude='$</bin/*/$(appname)*' \
		--exclude='$</bin/$(dirname-osx)/Contents/MacOS/$(appname)_universal' \
		-cf $@ $<

dist-tar: ../$(tarname)

../$(dirname-osx): ../$(dirname)
	cp -R $</bin/$(dirname-osx) $@
	cp -R $</* $@/Contents/Resources
	rm -rf $@/Contents/Resources/bin/*/$(appname)*linux*
	rm -rf $@/Contents/Resources/bin/*/$(appname)*bsd*
	rm -rf $@/Contents/Resources/bin/*/$(appname)*.exe

../$(tarname-osx): ../$(dirname-osx)
	tar -cf $@ $<

dist-tar-osx: ../$(tarname-osx)
	rm -rf ../$(dirname-osx)

../$(tarname-combined): ../$(dirname)
	tar -cf $@ $<

dist-tar-combined: ../$(tarname-combined)

../$(dirname-win): ../$(dirname)
	cp -R $< $@
	rm -rf $@/bin/*/$(appname)*linux*
	rm -rf $@/bin/*/$(appname)*bsd*
	rm -rf $@/bin/$(dirname-osx)/Contents/MacOS/$(appname)_universal

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

../$(tarname-osx).gz: ../$(tarname-osx)
	gzip -c < $< > $@

dist-gz-osx: ../$(tarname-osx).gz
	rm -rf ../$(dirname-osx)

../$(tarname-osx).bz2: ../$(tarname-osx)
	bzip2 -c < $< > $@

dist-bz2-osx: ../$(tarname-osx).bz2
	rm -rf ../$(dirname-osx)

dist-osx: ../$(tarname-osx).bz2
	rm -rf ../$(dirname-osx)

../$(tarname-osx).xz: ../$(tarname-osx)
	xz -c < $< > $@

dist-xz-osx: ../$(tarname-osx).xz

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

dist: dist-clean dist-bz2 dist-bz2-combined dist-win dist-osx

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

../$(tarname-osx).bz2.torrent: ../$(tarname-osx).bz2
	rm -f $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname-osx).bz2 \
		-n $(tarname-osx).bz2 \
		-c "$(appnamefull) $(appversion) for Mac OSX" \
		$(tarname-osx).bz2

dist-torrent-osx: ../$(tarname-osx).bz2.torrent

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

dist-torrents: dist-torrent-bz2 dist-torrent-combined dist-torrent-win dist-torrent-osx

dist-mostlyclean:
	rm -rf ../$(dirname)
	rm -rf ../$(dirname-win)
	rm -rf ../$(dirname-osx)
	rm -f ../$(tarname)
	rm -f ../$(tarname-osx)
	rm -f ../$(tarname-combined)

dist-clean: dist-mostlyclean
	rm -f ../$(tarname)*
	rm -f ../$(tarname-osx)*
	rm -f ../$(tarname-combined)*
	rm -f ../$(exename)*

../doc/cube2font.txt: ../doc/man/cube2font.1
	scripts/cube2font-txt $< $@

cube2font-txt: ../doc/cube2font.txt

../doc/examples/servinit.cfg: ../config/usage.cfg install-server
	scripts/servinit-defaults $@
	scripts/servinit-comments $< $@

update-servinit: ../doc/examples/servinit.cfg
