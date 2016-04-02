# Build GPX, build GPX distributions
# Dan Newman, February 2015
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

EXTRA_DIST = examples scripts README.md
SUBDIRS = src/gpx src/utils

AM_CPPFLAGS = -I$(top_srcdir)/src/shared

if HAVE_SIGNOSX
dist-hook:
	$(SIGNOSX) $(builddir)/src/gpx/gpx$(EXEEXT)
	$(SIGNOSX) $(builddir)/src/utils/machines$(EXEEXT)
	$(SIGNOSX) $(builddir)/src/utils/s3gdump$(EXEEXT)
endif

.PHONY : binaries
binaries: distdir
	cp $(builddir)/src/gpx/gpx$(EXEEXT) $(distdir)
	cp $(builddir)/src/utils/machines$(EXEEXT) $(distdir)
	cp $(builddir)/src/utils/s3gdump$(EXEEXT) $(distdir)

.PHONY : bdist
bdist:
	$(MAKE) $(AM_MAKEFLAGS) binaries $(DIST_TARGETS) am__post_remove_distdir='@:'
	$(am__post_remove_distdir)

if HAVE_CREATEDMG
dist-dmg: binaries
	$(CREATEDMG) $(distdir) $(distdir).dmg
	$(am__post_remove_distdir)
endif
