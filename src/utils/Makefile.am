# Build GPX, build GPX distributions
# Dan Newman, February 2015

AM_CPPFLAGS = -I$(top_srcdir)/src/shared
MACHINEDIR = $(top_builddir)/machine_inis
GPXDIR = $(top_srcdir)/src/gpx

bin_PROGRAMS = s3gdump machines

s3gdump_SOURCES = s3gdump.c ../shared/s3g.c ../shared/s3g_stdio.c
machines_SOURCES = machines.c ../shared/opt.c ../shared/machine_config.c
EXTRA_DIST = $(MACHINEDIR)

$(MACHINEDIR): $(builddir)/machines$(EXEEXT)
	@$(MKDIR_P) $(MACHINEDIR)
	@$(builddir)/machines$(EXEEXT) $(MACHINEDIR)/

if HAVE_DIFF
extra_test:: $(builddir)/s3gdump$(EXEEXT)
	$(builddir)/s3gdump$(EXEEXT) $(GPXDIR)/tests/lint.x3g > $(builddir)/lint.txt 2>&1
	$(DIFF) $(GPXDIR)/tests/lint.txt $(builddir)/lint.txt
	-@$(RM) $(builddir)/lint.txt
endif
