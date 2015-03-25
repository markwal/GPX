# Build GPX, build GPX distributions
# Dan Newman, February 2015
#

SRCDIR = ..

include $(SRCDIR)/defines/defines.mk

EXE_TARGETS = gpx

gpx_SRCS = gpx.c gpx-main.c config.c opt.c $(EXTRA_SRCS)
gpx_OBJS = $(notdir $(gpx_SRCS:.c=$(OBJ)))
gpx_LIBS = m

ifeq ($(PLATFORM), osx)
gpx-main_DEFS = -DSERIAL_SUPPORT
endif

ifeq ($(PLATFORM), linux)
gpx-main_DEFS = -DSERIAL_SUPPORT
endif

ifdef PYTHON
ifdef DIFF

all::

test:: $(OBJDIR)/gpx$(EXE)
	$(OBJDIR)/gpx$(EXE) -p -m r2x tests/lint.gcode $(OBJDIR)/lint.x3g > $(OBJDIR)/lint.log 2>&1
	$(DIFF) tests/lint.x3g $(OBJDIR)/lint.x3g
	$(PYTHON) $(SRCDIR)/../scripts/s3g-decompiler.py $(OBJDIR)/lint.x3g > $(OBJDIR)/lint.txt 2>&1
	$(DIFF) tests/lint.txt $(OBJDIR)/lint.txt
	$(DIFF) tests/lint.log $(OBJDIR)/lint.log
	-@$(RM) $(OBJDIR)/lint.x3g $(OBJDIR)/lint.txt $(OBJDIR)/lint.log

endif
endif

##########
#
# This should be the last line of the file
#

include $(SRCDIR)/defines/rules.mk
