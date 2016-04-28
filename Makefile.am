# Build GPX, build GPX distributions
# Dan Newman, February 2015
# autotools by Mark Walker, April 2016
#
# mkdir build       -- create build folder, name it whatever you like
# cd build
# ../configure      -- analyze your system to create the Makefiles
# make              -- compile gpx and utilities
# sudo make install -- install gpx and utilities, defaults to /usr/local/bin
#
# for a cross compile, give the --host parameter to ../configure
# ../configure host=i686-w64-mingw32   -- build a Makefile for win32 cross-compile
# ../configure host=x86_64-w64-mingw32 -- build a Makefile for win64 cross-compile
#
# make dist         -- create a source distribution archive
# make bdist        -- create a distribution archive that includes the built programs
# make clean        -- remove the build directories but retain distributions
# make distclean    -- remove the build directories and distributions

EXTRA_DIST = examples scripts README.md src/shared
SUBDIRS = src/gpx src/utils

AM_CPPFLAGS = -Wall -I$(top_srcdir)/src/shared

.PHONY : first debug release bdistdir bdist
first debug release: all

bdistdir = $(PACKAGE)-$(VERSION)-$(PLATFORM)

remove_bdistdir = \
  if test -d "$(bdistdir)"; then \
    find "$(bdistdir)" -type d ! -perm -200 -exec chmod u+w {} ';' \
      && rm -rf "$(bdistdir)" \
      || { sleep 5 && rm -rf "$(bdistdir)"; }; \
  else :; fi

BINARIES = $(builddir)/src/gpx/gpx$(EXEEXT) $(builddir)/src/utils/machines$(EXEEXT) $(builddir)/src/utils/s3gdump$(EXEEXT)

$(builddir)/machine_inis:
	$(MKDIR_P) $(builddir)/machine_inis/
	machines $(builddir)/machine_inis/

bdistdir: $(BINARIES) $(builddir)/machine_inis 
	$(MKDIR_P) $(bdistdir)
if HAVE_SIGNOSX
	list='$(BINARIES)'; \
	for file in $$list; do \
	    $(SIGNOSX) $$file; \
	done
endif
	@list='$(BINARIES)'; \
	for file in $$list; do \
	    cp $$file $(bdistdir); \
	done
	cp -r $(srcdir)/examples $(bdistdir)
	cp -r $(builddir)/machine_inis $(bdistdir)
	cp -r $(srcdir)/scripts $(bdistdir)
	chmod -R a+rwx $(bdistdir)

bdist-gzip: bdistdir
	tardir=$(bdistdir) && $(am__tar) | GZIP=$(GZIP_ENV) gzip -c >$(bdistdir).tar.gz
	$(remove_bdistdir)

bdist-xz: bdistdir
	tardir=$(bdistdir) && $(am__tar) | XZ_OPT=$${XZ_OPT--e} xz -c >$(bdistdir).tar.xz
	$(remove_bdistdir)

bdist-zip: bdistdir
	-rm -f $(bdistdir).zip
	zip -rq $(bdistdir).zip $(bdistdir)
	$(remove_bdistdir)

if HAVE_CREATEDMG
bdist-dmg: bdistdir
	$(CREATEDMG) $(bdistdir) $(bdistdir).dmg
	$(remove_bdistdir)
endif

bdist:
	$(MAKE) $(AM_MAKEFLAGS) $(BDIST_TARGET) remove_bdistdir='@:'
	$(remove_bdistdir)

