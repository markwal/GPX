# Build GPX, build GPX distributions
# Dan Newman, February 2015

SRCDIR = ..

include $(SRCDIR)/defines/defines.mk

EXE_TARGETS = s3gdump machines

s3gdump_SRCS = \
	s3gdump.c \
	$(SHAREDIR)/s3g.c \
	$(SHAREDIR)/s3g_stdio.c
s3gdump_OBJS = $(notdir $(s3gdump_SRCS:.c=$(OBJ)))
s3gdump_LIBS =

machines_SRCS = \
	machines.c \
	$(SHAREDIR)/opt.c \
	$(SHAREDIR)/config.c
machines_OBJS = $(notdir $(machines_SRCS:.c=$(OBJ)))
machines_LIBS =

all::

machines: $(OBJDIR)/machines$(EXE)
	@test -d $(MACHINEDIR) || $(MKDIR) $(MACHINEDIR)
	@$(OBJDIR)/machines$(EXE) $(MACHINEDIR)/

ifdef DIFF

test:: $(OBJDIR)/s3gdump$(EXE)
	$(OBJDIR)/s3gdump$(EXE) $(GPXDIR)/tests/lint.x3g > $(OBJDIR)/lint.txt 2>&1
	$(DIFF) $(GPXDIR)/tests/lint.txt $(OBJDIR)/lint.txt
	-@$(RM) $(OBJDIR)/lint.txt

endif

##########
#
# This should be the last line of the file
#

include $(SRCDIR)/defines/rules.mk
