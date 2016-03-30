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

EXTRA_DIST = examples machines scripts
SUBDIRS = src/gpx src/utils

if HAVE_CREATEDMG
dist-dmg: distdir
if HAVE_SIGNOSX
	$(SIGNOSX) $(distdir)/gpx$(EXE)
endif
	$(CREATEDMG) $(distdir) $(distdir).dmg
	tardir=$(distdir) && $(am__tar) | GZIP=$(GZIP_ENV) gzip -c >$(distdir).tar.gz
	$(am__post_remove_distdir)
endif
