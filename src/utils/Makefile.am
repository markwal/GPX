# Build GPX, build GPX distributions
# Dan Newman, February 2015

SRCDIR = ..

bin_PROGRAMS = s3gdump machines

s3gdump_SOURCES = s3gdump.c ../shared/s3g.c ../shared/s3g_stdio.c
machines_SOURCES = machines.c ../shared/opt.c ../shared/machine_config.c

generate_machines: machines$(EXEEXT)
	@test -d $(MACHINEDIR) || $(MKDIR) $(MACHINEDIR)
	@$(OBJDIR)/machines$(EXE) $(MACHINEDIR)/

if HAVE_DIFF
test:: $(OBJDIR)/s3gdump$(EXE)
	$(OBJDIR)/s3gdump$(EXE) $(GPXDIR)/tests/lint.x3g > $(OBJDIR)/lint.txt 2>&1
	$(DIFF) $(GPXDIR)/tests/lint.txt $(OBJDIR)/lint.txt
	-@$(RM) $(OBJDIR)/lint.txt

endif
