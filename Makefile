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

TARGET =
SRCDIR = src

include $(SRCDIR)/defines/defines.mk

##########
#
#  Add executables to build to the EXE_TARGETS variable
#
##########

SUBDIRS = gpx planner utils
DIR_TARGETS = $(addprefix $(SRCDIR)/, $(SUBDIRS))

.PHONY: all clean test machines

all:
	for dir in $(DIR_TARGETS); do \
		echo "Entering $$dir"; \
		make -C $$dir $@; \
		echo "Exiting $$dir"; \
	done

mostlyclean:
	for dir in $(DIR_TARGETS); do \
		make -C $$dir clean; \
	done

clean: mostlyclean
	-@$(RM) $(ARCHDIR)/*
	-@$(RMDIR) $(ARCHDIR)
	-@$(RM) $(MACHINEDIR)/*
	-@$(RMDIR) $(MACHINEDIR)
	-@$(RM) $(ARCHIVE).tar.gz
	-@$(RM) $(ARCHIVE).zip
	-@$(RM) $(ARCHIVE).dmg

test:
	for dir in $(DIR_TARGETS); do \
		make -C $$dir $@; \
	done

machines:
	make -C $(SRCDIR)/utils machines

# dist dependencies are not quite correct
dist: $(SRCDIR)/gpx/$(OBJDIR)/gpx$(EXE) machines
	-@$(MKDIR) $(ARCHDIR)
	-@$(MKDIR) $(ARCHDIR)/examples
	-@$(MKDIR) $(ARCHDIR)/machines
	-@$(MKDIR) $(ARCHDIR)/scripts
	@$(CP) $(SRCDIR)/gpx/$(OBJDIR)/gpx$(EXE) $(ARCHDIR)
	@$(CP) examples/example-machine.ini \
		examples/example-pause-at-zpos.ini \
		examples/example-temperature.ini \
		examples/macro-example.gcode \
		examples/rep2-eeprom.ini $(ARCHDIR)/examples
	@$(CP) scripts/gpx.py scripts/s3g-decompiler.py $(ARCHDIR)/scripts
	@$(CP) machines/*.ini $(ARCHDIR)/machines
	@$(CHMOD) -R a+rwx $(ARCHDIR)
ifeq ($(PLATFORM), osx)
ifneq ("$(wildcard sign-osx.sh)","")
	@sign-osx.sh $(ARCHDIR)/gpx$(EXE)
endif
	@test -x /usr/bin/hdiutil && hdiutil create -format UDZO -srcfolder $(ARCHDIR) $(ARCHIVE).dmg
else ifeq ($(PLATFORM), linux)
	$(TAR) - $(ARCHDIR) | $(GZIP) > $(ARCHIVE).tar.gz
else
	$(ZIP) $(ARCHIVE).zip $(ARCHDIR)
endif
