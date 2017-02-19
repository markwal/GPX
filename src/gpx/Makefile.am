# Build GPX, build GPX distributions
# Dan Newman, February 2015
#

AM_CPPFLAGS = -Wall -Wstrict-prototypes -Wformat -Werror=format-security -DSERIAL_SUPPORT -I$(top_srcdir)/src/shared

bin_PROGRAMS = gpx
gpx_SOURCES = gpx.c gpx-main.c gpxresp.c ../shared/machine_config.c ../shared/opt.c vector.c vector.h gpx.h winsio.h
if HAVE_WINDOWS_H
gpx_SOURCES += winsio.c
endif
gpx_LDADD = -lm

if HAVE_PYTHON
if HAVE_DIFF
test-local: $(builddir)/gpx$(EXEEXT)
	$(builddir)/gpx$(EXEEXT) -I -p -m r2x $(srcdir)/tests/lint.gcode $(builddir)/lint.x3g > $(builddir)/lint.log 2>&1
	$(builddir)/gpx$(EXEEXT) -I -g -p -m r2x $(srcdir)/tests/lint.gcode $(builddir)/lint-g.x3g > $(builddir)/lint-g.log 2>&1
	$(builddir)/gpx$(EXEEXT) -I -p -m r2x $(srcdir)/tests/issue13.gcode $(builddir)/issue13.x3g > $(builddir)/issue13.log 2>&1
	$(builddir)/gpx$(EXEEXT) -I -g -p -m r2x $(srcdir)/tests/issue13.gcode $(builddir)/issue13-g.x3g > $(builddir)/issue13-g.log 2>&1
	$(PYTHON) $(top_srcdir)/scripts/s3g-decompiler.py $(builddir)/lint.x3g > $(builddir)/lint.txt 2>&1
	$(PYTHON) $(top_srcdir)/scripts/s3g-decompiler.py $(builddir)/lint-g.x3g > $(builddir)/lint-g.txt 2>&1
	$(PYTHON) $(top_srcdir)/scripts/s3g-decompiler.py $(builddir)/issue13.x3g > $(builddir)/issue13.txt 2>&1
	$(PYTHON) $(top_srcdir)/scripts/s3g-decompiler.py $(builddir)/issue13-g.x3g > $(builddir)/issue13-g.txt 2>&1
	$(DIFF) $(srcdir)/tests/lint.txt $(builddir)/lint.txt
	$(DIFF) $(srcdir)/tests/lint-g.txt $(builddir)/lint-g.txt
	$(DIFF) $(srcdir)/tests/issue13.txt $(builddir)/issue13.txt
	$(DIFF) $(srcdir)/tests/issue13-g.txt $(builddir)/issue13-g.txt
	$(DIFF) $(srcdir)/tests/lint.x3g $(builddir)/lint.x3g
	$(DIFF) $(srcdir)/tests/lint.log $(builddir)/lint.log
	$(DIFF) $(srcdir)/tests/lint-g.x3g $(builddir)/lint-g.x3g
	$(DIFF) $(srcdir)/tests/lint-g.log $(builddir)/lint-g.log
	$(DIFF) $(srcdir)/tests/issue13.x3g $(builddir)/issue13.x3g
	$(DIFF) $(srcdir)/tests/issue13.log $(builddir)/issue13.log
	$(DIFF) $(srcdir)/tests/issue13-g.x3g $(builddir)/issue13-g.x3g
	$(DIFF) $(srcdir)/tests/issue13-g.log $(builddir)/issue13-g.log
	-@$(RM) $(builddir)/lint.x3g $(builddir)/lint.txt $(builddir)/lint.log
	-@$(RM) $(builddir)/lint-g.x3g $(builddir)/lint-g.txt $(builddir)/lint-g.log
	-@$(RM) $(builddir)/issue13.x3g $(builddir)/issue13.txt $(builddir)/issue13.log
	-@$(RM) $(builddir)/issue13-g.x3g $(builddir)/issue13-g.txt $(builddir)/issue13-g.log
endif
endif
