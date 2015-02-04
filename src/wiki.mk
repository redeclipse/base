weapon-names=$(shell sed -n '/WPSVAR(0, name,/,/);/s/ *"\([^"]*\)",*/\1 /g;s/ $$//p' game/weapons.h)
weapon-wiki-pages=$(shell for w in $(weapon-names); do echo "../doc/wiki-weapon-$${w}.txt"; done)

../doc/wiki-contributors.txt: ../readme.txt
	scripts/wiki-contributors $< $@

wiki-contributors: ../doc/wiki-contributors.txt

../doc/wiki-guidelines.txt: ../doc/guidelines.txt
	scripts/wiki-guidelines $< $@

wiki-guidelines: ../doc/wiki-guidelines.txt

../doc/wiki-manpage-%.txt: ../doc/man/%.am
	# first substitute all placeholders via sed
	# then convert to mediawiki syntax in two steps
	# then via sed remove the "Content-type" header
	# and remove the end links from the "Section" header
	# and remove some separators and stray single-chars
	# and remove some unneeded parts of the tail end
	# then squeeze all multiple empty lines
	sed -e 's,@LIBEXECDIR@,$(patsubst $(DESTDIR)%,%,$(libexecdir)),g' \
		-e 's,@DATADIR@,$(patsubst $(DESTDIR)%,%,$(datadir)),g' \
		-e 's,@DOCDIR@,$(patsubst $(DESTDIR)%,%,$(docdir)),g' \
		-e 's,@APPNAME@,$(appname),g' \
		-e 's,@CAPPNAME@,$(cappname),g' $< | \
		man2html | \
		pandoc -f html -t mediawiki | \
		sed -e '1,2d' \
			-e 's/\(^Section:[^[]*\).*/\1/' \
			-e '/-----/d' \
			-e '/^.$$/d' \
			-e '/== SEE ALSO ==/,$$d' | \
		cat -s > $@

wiki-manpages: ../doc/wiki-manpage-$(APPNAME).6.txt ../doc/wiki-manpage-$(APPNAME)-server.6.txt

../doc/varsinfo-all.txt: install-client
	RE_TEMPHOME="$$(mktemp -d)"; \
		../redeclipse.sh -h"$$RE_TEMPHOME" -df0 -w640 -dh480 -du0 -x"writevarsinfo; quit"; \
		mv "$$RE_TEMPHOME/varsinfo.txt" $@; \
		rm -r "$$RE_TEMPHOME"

../doc/varsinfo-weapon-%.txt: ../doc/varsinfo-all.txt
	# check if beginning matches weapon name
	awk 'match($$1, /^$*/) {print}' $^ > $@

../doc/varsinfo-non-weapons.txt: ../doc/varsinfo-all.txt
	# don't match weapons, do match VAR, FVAR and SVAR types
	awk '!match($$1, /^('"$$(echo $(weapon-names) | tr ' ' '|' )"')/) && \
		(match($$2, 0) || match($$2, 1) || match($$2, 2)) \
		{print}' $^ > $@

../doc/varsinfo-client-and-admin.txt: ../doc/varsinfo-all.txt
	# don't match weapons, commands
	# do match client or admin flags
	# overlaps world vars
	gawk '!match($$1, /^('"$$(echo $(weapon-names) | tr ' ' '|' )"')/) && \
		!match($$2, "3") && \
		!and($$3, lshift(1, 3)) && \
		and($$3, or(lshift(1, 6), lshift(1,9))) \
		{print}' $^ > $@

../doc/varsinfo-textures.txt: ../doc/varsinfo-all.txt
	# check if texture flag is set
	# overlaps weapon tex vars
	gawk 'and($$3, lshift(1, 5)) {print}' $^ > $@

../doc/varsinfo-world.txt: ../doc/varsinfo-all.txt
	# check if world flag is set
	# overlaps client-and-admin vars
	gawk 'and($$3, lshift(1, 3)) {print}' $^ > $@

../doc/varsinfo-commands.txt: ../doc/varsinfo-all.txt
	# check if type is == 3
	awk 'match($$2, "3") {print}' $^ > $@

../doc/varsinfo-aliases.txt: ../doc/varsinfo-all.txt
	# check if type is == 4
	awk 'match($$2, "4") {print}' $^ > $@

../doc/wiki-%.txt: ../doc/varsinfo-%.txt
	scripts/wiki-convert $^ > $@

../doc/wiki-all-vars-commands.txt: ../doc/varsinfo-all.txt
	scripts/wiki-convert $^ > $@

wiki-all-vars-commands: ../doc/wiki-all-vars-commands.txt

wiki-weapons: $(weapon-wiki-pages)

wiki-non-weapons: ../doc/wiki-non-weapons.txt

wiki-client-and-admin: ../doc/wiki-client-and-admin.txt

wiki-textures: ../doc/wiki-textures.txt

wiki-world: ../doc/wiki-world.txt

wiki-commands: ../doc/wiki-commands.txt

wiki-aliases: ../doc/wiki-aliases.txt

wiki-all: ../doc/wiki-all-vars-commands.txt $(weapon-wiki-pages) ../doc/wiki-non-weapons.txt ../doc/wiki-client-and-admin.txt ../doc/wiki-textures.txt ../doc/wiki-world.txt ../doc/wiki-commands.txt ../doc/wiki-aliases.txt

wiki-clean:
	rm -f ../doc/varsinfo-*.txt ../doc/wiki-*.txt
