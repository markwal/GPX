# Build GPX, build GPX distributions
# Dan Newman, February 2015
#

bin_PROGRAMS = gpx
gpx_SOURCES = gpx.c gpx-main.c ../shared/machine_config.c ../shared/opt.c vector.c
if HAVE_WINDOWS_H
gpx_SOURCES += winsio.c
endif

gpx_LDADD = -lm

gpx_DEFS = -DSERIAL_SUPPORT

if HAVE_PYTHON
if HAVE_DIFF
extra_test:: $(OBJDIR)/gpx$(EXE)
	$(OBJDIR)/gpx$(EXE) -p -m r2x tests/lint.gcode $(OBJDIR)/lint.x3g > $(OBJDIR)/lint.log 2>&1
	$(OBJDIR)/gpx$(EXE) -g -p -m r2x tests/lint.gcode $(OBJDIR)/lint-g.x3g > $(OBJDIR)/lint-g.log 2>&1
	$(OBJDIR)/gpx$(EXE) -p -m r2x tests/issue13.gcode $(OBJDIR)/issue13.x3g > $(OBJDIR)/issue13.log 2>&1
	$(OBJDIR)/gpx$(EXE) -g -p -m r2x tests/issue13.gcode $(OBJDIR)/issue13-g.x3g > $(OBJDIR)/issue13-g.log 2>&1
	$(PYTHON) $(SRCDIR)/../scripts/s3g-decompiler.py $(OBJDIR)/lint.x3g > $(OBJDIR)/lint.txt 2>&1
	$(PYTHON) $(SRCDIR)/../scripts/s3g-decompiler.py $(OBJDIR)/lint-g.x3g > $(OBJDIR)/lint-g.txt 2>&1
	$(PYTHON) $(SRCDIR)/../scripts/s3g-decompiler.py $(OBJDIR)/issue13.x3g > $(OBJDIR)/issue13.txt 2>&1
	$(PYTHON) $(SRCDIR)/../scripts/s3g-decompiler.py $(OBJDIR)/issue13-g.x3g > $(OBJDIR)/issue13-g.txt 2>&1
	$(DIFF) tests/lint.txt $(OBJDIR)/lint.txt
	$(DIFF) tests/lint-g.txt $(OBJDIR)/lint-g.txt
	$(DIFF) tests/issue13.txt $(OBJDIR)/issue13.txt
	$(DIFF) tests/issue13-g.txt $(OBJDIR)/issue13-g.txt
	$(DIFF) tests/lint.x3g $(OBJDIR)/lint.x3g
	$(DIFF) tests/lint.log $(OBJDIR)/lint.log
	$(DIFF) tests/lint-g.x3g $(OBJDIR)/lint-g.x3g
	$(DIFF) tests/lint-g.log $(OBJDIR)/lint-g.log
	$(DIFF) tests/issue13.x3g $(OBJDIR)/issue13.x3g
	$(DIFF) tests/issue13.log $(OBJDIR)/issue13.log
	$(DIFF) tests/issue13-g.x3g $(OBJDIR)/issue13-g.x3g
	$(DIFF) tests/issue13-g.log $(OBJDIR)/issue13-g.log
	-@$(RM) $(OBJDIR)/lint.x3g $(OBJDIR)/lint.txt $(OBJDIR)/lint.log
	-@$(RM) $(OBJDIR)/lint-g.x3g $(OBJDIR)/lint-g.txt $(OBJDIR)/lint-g.log
	-@$(RM) $(OBJDIR)/issue13.x3g $(OBJDIR)/issue13.txt $(OBJDIR)/issue13.log
	-@$(RM) $(OBJDIR)/issue13-g.x3g $(OBJDIR)/issue13-g.txt $(OBJDIR)/issue13-g.log
endif
endif
