﻿appname=$(APPNAME)
ifneq (,$(APPBRANCH))
appbranch=$(APPBRANCH)
else
appbranch=stable
endif
appnamefull=$(shell sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' engine/version.h)
apprelease=$(shell sed -n 's/.define VERSION_RELEASE *"\([^"]*\)"/\1/p' engine/version.h)
appvermaj=$(shell sed -n 's/.define VERSION_MAJOR \([0-9]*\)/\1/p' engine/version.h)
appvermin=$(shell sed -n 's/.define VERSION_MINOR \([0-9]*\)/\1/p' engine/version.h)
appverpat=$(shell sed -n 's/.define VERSION_PATCH \([0-9]*\)/\1/p' engine/version.h)
appversion=$(appvermaj).$(appvermin).$(appverpat)
appfiles=https://raw.githubusercontent.com/redeclipse/deploy/master/$(appbranch)

dirname=$(appname)-$(appversion)
dirname-win=$(dirname)-win

exename=$(appname)_$(appversion)_win.exe
zipname=$(appname)_$(appversion)_win.zip
tarname=$(appname)_$(appversion)_nix.tar
tarname-combined=$(appname)_$(appversion)_combined.tar

torrent-trackers-url="udp://tracker.openbittorrent.com:80,udp://tracker.publicbt.com:80,udp://open.demonii.com:1337,udp://tracker.coppersurfer.tk:6969,udp://tracker.leechers-paradise.org:6969"
torrent-webseed-baseurl="https://redeclipse.net/files/releases"

DISTFILES=$(shell cd ../ && find . -not -iname . -not -iname *.lo -not -iname *.gch -not -iname *.o || echo "")
CURL=curl --location --insecure --fail

../$(dirname):
	rm -rfv $@
	tar --exclude=.git --exclude=$(dirname) \
		-cf - $(DISTFILES:%=../%) | (mkdir $@/; cd $@/ ; tar -xpf -)
	echo $(appbranch) > $@/branch.txt
	$(CURL) $(appfiles)/base.txt --output $@/version.txt
	$(CURL) $(appfiles)/bins.txt --output $@/bin/version.txt
	for i in `curl --silent --location --insecure --fail $(appfiles)/mods.txt`; do if [ "$${i}" != "base" ]; then mkdir -p $@/data/$${i}; $(CURL) $(appfiles)/$${i}.txt --output $@/data/$${i}/version.txt; fi; done
	$(CURL) $(appfiles)/windows.zip --output windows.zip
	unzip -o windows.zip -d $@
	rm -fv windows.zip
	$(CURL) $(appfiles)/linux.tar.gz --output linux.tar.gz
	tar --gzip --extract --verbose --overwrite --file=linux.tar.gz --directory=$@
	rm -fv linux.tar.gz
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src/enet clean

distdir: ../$(dirname)

../$(tarname): ../$(dirname)
	tar \
		--exclude='$</bin/*/*.exe' \
		-cf $@ $<

dist-tar: ../$(tarname)

../$(tarname-combined): ../$(dirname)
	tar -cf $@ $<

dist-tar-combined: ../$(tarname-combined)

../$(dirname-win): ../$(dirname)
	cp -R $< $@
	rm -rfv $@/bin/*/$(appname)*linux*
	rm -rfv $@/bin/*/$(appname)*bsd*
	rm -rfv $@/bin/*/genkey*linux*
	rm -rfv $@/bin/*/genkey*bsd*
	rm -rfv $@/bin/*/tessfont*linux*
	rm -rfv $@/bin/*/tessfont*bsd*

distdir-win: ../$(dirname-win)

../$(tarname).gz: ../$(tarname)
	gzip -c < $< > $@
	rm -fv ../$(tarname)

dist-gz: ../$(tarname).gz

../$(tarname).bz2: ../$(tarname)
	bzip2 -c < $< > $@
	rm -fv ../$(tarname)

dist-bz2: ../$(tarname).bz2

dist-nix: ../$(tarname).bz2

../$(tarname).xz: ../$(tarname)
	xz -c < $< > $@
	rm -fv ../$(tarname)

dist-xz: ../$(tarname).xz

../$(tarname-combined).gz: ../$(tarname-combined)
	gzip -c < $< > $@
	rm -fv ../$(tarname-combined)

dist-gz-combined: ../$(tarname-combined).gz

../$(tarname-combined).bz2: ../$(tarname-combined)
	bzip2 -c < $< > $@
	rm -fv ../$(tarname-combined)

dist-bz2-combined: ../$(tarname-combined).bz2

dist-combined: ../$(tarname-combined).bz2

../$(tarname-combined).xz: ../$(tarname-combined)
	xz -c < $< > $@
	rm -fv ../$(tarname-combined)

dist-xz-combined: ../$(tarname-combined).xz

../$(exename): ../$(dirname-win)
	sed "s/~REPVERSION~/$(appversion)/g;s/~REPOUTFILE~/$(exename)/g" $</src/install/win/$(appname).nsi > $</src/install/win/$(appname)_$(appversion).nsi
	cat $</src/install/win/$(appname)_$(appversion).nsi
	makensis -V4 $</src/install/win/$(appname)_$(appversion).nsi
	$(MV) $</src/install/win/$(exename) ../
	rm -fv $</src/install/win/$(appname)_$(appversion).nsi
	rm -rfv ../$(dirname-win)

../$(zipname): ../$(dirname-win)
	cd .. && zip -v9r "$(zipname)" "$(dirname-win)"
	rm -rfv ../$(dirname-win)

dist-win: ../$(exename)

dist-zip: ../$(zipname)

dist: dist-clean dist-bz2 dist-bz2-combined dist-win dist-zip

../$(tarname).bz2.torrent: ../$(tarname).bz2
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname).bz2 \
		-n $(tarname).bz2 \
		-c "Red Eclipse v$(appversion) ($(apprelease)) GNU/Linux" \
		$(tarname).bz2

dist-torrent-nix: ../$(tarname).bz2.torrent

dist-torrent-bz2: ../$(tarname).bz2.torrent

../$(tarname-combined).bz2.torrent: ../$(tarname-combined).bz2
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(tarname-combined).bz2 \
		-n $(tarname-combined).bz2 \
		-c "$(appnamefull) v$(appversion) ($(apprelease)) Combined" \
		$(tarname-combined).bz2

dist-torrent-combined: ../$(tarname-combined).bz2.torrent

../$(exename).torrent: ../$(exename)
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(exename) \
		-n $(exename) \
		-c "$(appnamefull) v$(appversion) ($(apprelease)) Windows" \
		$(exename)

dist-torrent-win: ../$(exename).torrent

../$(zipname).torrent: ../$(zipname)
	rm -fv $@
	cd ../ &&\
		mktorrent \
		-a $(torrent-trackers-url) \
		-w $(torrent-webseed-baseurl)/$(zipname) \
		-n $(zipname) \
		-c "$(appnamefull) v$(appversion) ($(apprelease)) ZIP" \
		$(zipname)

dist-torrent-zip: ../$(zipname).torrent

dist-torrents: dist-torrent-bz2 dist-torrent-combined dist-torrent-win dist-torrent-zip

dist-mostlyclean:
	rm -rfv ../$(dirname)
	rm -rfv ../$(dirname-win)
	rm -fv ../$(tarname)
	rm -fv ../$(tarname-combined)

dist-clean: dist-mostlyclean
	rm -fv ../$(tarname)*
	rm -fv ../$(tarname-combined)*
	rm -fv ../$(exename)*

../doc/tessfont.txt: ../doc/man/tessfont.1
	scripts/tessfont-txt $< $@

tessfont-txt: ../doc/tessfont.txt

../doc/examples/servinit.cfg: ../config/usage.cfg install-server
	scripts/servinit-defaults $@
	scripts/servinit-comments $< $@

update-servinit: ../doc/examples/servinit.cfg
