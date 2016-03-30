# Build GPX, build GPX distributions
# Dan Newman, February 2015
#
# make dist        -- compile gpx and assemble a gpx distribution
# make             -- compile gpx
# make mostlyclean -- remove the build directories but retain distributions
# make clean       -- remove the build directories and distributions
#
# For cross compiling with MinGW, two target types may be specified on
# the make command line:
#
#   make [optional-target] TARGET=mingw32    # Build for Win 32
#   make [optional-target] TARGET=mingw64    # Build for Win 64

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
binaries:
	cp $(builddir)/src/gpx/gpx$(EXEEXT) $(distdir)
	cp $(builddir)/src/utils/machines$(EXEEXT) $(distdir)
	cp $(builddir)/src/utils/s3gdump$(EXEEXT) $(distdir)

bdist: distdir binaries
	$(MAKE) $(AM_MAKEFLAGS) $(DIST_TARGETS) am__post_remove_distdir='@:'
	$(am__post_remove_distdir)

if HAVE_CREATEDMG
dist-dmg: binaries
	$(CREATEDMG) $(distdir) $(distdir).dmg
	$(am__post_remove_distdir)
endif
