# Build GPX utilities
# Dan Newman, February 2015

AM_CPPFLAGS = -Wall -I$(top_srcdir)/src/shared
MACHINEDIR = $(top_builddir)/machine_inis
GPXDIR = $(top_srcdir)/src/gpx

# if we're cross-compiling, we're depending on an earlier build=host build
# being installed and available
if CROSS_COMPILING
MACHINES = machines
MACHINES_PROGRAM =
else
MACHINES = $(builddir)/machines$(EXEEXT)
MACHINES_PROGRAM = $(MACHINES)
endif

bin_PROGRAMS = s3gdump machines
EXTRA_DIST = $(MACHINEDIR)

s3gdump_SOURCES = s3gdump.c ../shared/s3g.c ../shared/s3g_stdio.c
machines_SOURCES = machines.c ../shared/opt.c ../shared/machine_config.c

$(MACHINEDIR): $(MACHINES_PROGRAM)
	@$(MKDIR_P) $(MACHINEDIR)
	@$(MACHINES) $(MACHINEDIR)/

if HAVE_DIFF
test-local: $(builddir)/s3gdump$(EXEEXT)
	$(builddir)/s3gdump$(EXEEXT) $(GPXDIR)/tests/lint.x3g > $(builddir)/lint.txt 2>&1
	$(DIFF) $(GPXDIR)/tests/lint.txt $(builddir)/lint.txt
#	-@$(RM) $(builddir)/lint.txt
endif
