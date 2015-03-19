# Build GPX, build GPX distributions
# Dan Newman, February 2015

SRCDIR = ..

include $(SRCDIR)/defines/defines.mk

EXE_TARGETS = machines

machines_SRCS = \
	machines.c \
	$(SHAREDIR)/opt.c \
	$(SHAREDIR)/config.c
machines_OBJS = $(notdir $(machines_SRCS:.c=$(OBJ)))
machines_LIBS =

machines: $(OBJDIR)/machines$(EXE)
	@test -d $(MACHINEDIR) || $(MKDIR) $(MACHINEDIR)
	@$(OBJDIR)/machines$(EXE) $(MACHINEDIR)/

##########
#
# This should be the last line of the file
#

include $(SRCDIR)/defines/rules.mk
