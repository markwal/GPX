# Build GPX, build GPX distributions
# Dan Newman, February 2015
#
# make dist -- compile gpx and assemble a gpx distribution
# make      -- compile gpx
# make mostlyclean -- remove the build directories but retain distributions
# make clean       -- remove the build directories and distributions
#
# For cross compiling with MinGW, two target types may be specified on
# the make command line:
#
#   make [optional-target] TARGET=mingw32    # Build for Win 32
#   make [optional-target] TARGET=mingw64    # Build for Win 64

TARGET=

# OS specific settings

UNAME_OS := $(subst /,_,$(shell uname -s))
UNAME_ARCH := $(subst /,_,$(shell uname -p))

ifeq ($(TARGET), mingw64)

CC = /opt/mingw64/cross_win64/bin/x86_64-w64-mingw32-gcc
CC_FLAGS =
L_FLAGS = -lm
PLATFORM = win64
EXE = .exe

else ifeq ($(TARGET), mingw32)

CC = /opt/mingw32/cross_win32/bin/i686-w64-mingw32-gcc
CC_FLAGS =
L_FLAGS = -lm
PLATFORM = win32
EXE = .exe

else ifeq ($(UNAME_OS), Darwin)

CC = cc
CC_FLAGS = -DSERIAL_SUPPORT
L_FLAGS = -lm
PLATFORM = osx
EXE =

else ifeq ($(UNAME_OS), Linux)

CC = cc
CC_FLAGS = -DSERIAL_SUPPORT
L_FLAGS = -lm
PLATFORM = linux
EXE =

else ifeq ($(findstring MINGW32,$(UNAME_OS)), MINGW32)

CC = gcc
CC_FLAGS =
L_FLAGS = -lm
PLATFORM = win32
EXE = .exe

else

$(error Unsupported build platform $(UNAME_OS))

endif

# Build directories

VERSION = 2.1
OBJDIR  = $(PLATFORM)_obj
ARCHIVE = gpx-$(VERSION)-$(PLATFORM)
ARCHDIR = $(OBJDIR)/$(ARCHIVE)

# OS commands

CHMOD = chmod
CP = cp
GZIP = gzip -9c
MKDIR = mkdir -p
RM = rm -f
RMDIR = rm -rf
PYTHON = python
TAR = tar cf
ZIP = zip -r

HEADERS = $(wildcard *.h)
SOURCES = $(wildcard *.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:%.c=%.o))

all: $(OBJDIR)/gpx

.PHONY: all

$(OBJDIR)/gpx: $(OBJDIR) $(OBJECTS) $(HEADERS)
	$(CC) $(OBJECTS) $(L_FLAGS) -o $(OBJDIR)/gpx$(EXE)

$(OBJDIR):
	-@$(MKDIR) $(OBJDIR)

$(OBJDIR)/%.o: %.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

dist: $(OBJDIR)/gpx
	-@$(MKDIR) $(ARCHDIR)
	-@$(MKDIR) $(ARCHDIR)/examples
	-@$(MKDIR) $(ARCHDIR)/scripts
	@$(CP) $(OBJDIR)/gpx$(EXE) $(ARCHDIR)
	@$(CP) examples/example-machine.ini \
		examples/example-pause-at-zpos.ini \
		examples/example-temperature.ini \
		examples/lint.gcode \
		examples/macro-example.gcode \
		examples/rep2-eeprom.ini $(ARCHDIR)/examples
	@$(CP) scripts/gpx.py scripts/s3g-decompiler.py $(ARCHDIR)/scripts
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

mostlyclean:
	-@$(RMDIR) $(OBJDIR)

clean: mostlyclean
	-@$(RM) $(ARCHIVE).tar.gz
	-@$(RM) $(ARCHIVE).zip
	-@$(RM) $(ARCHIVE).dmg

test: $(OBJDIR)/gpx$(EXE)
	$(OBJDIR)/gpx$(EXE) examples/lint.gcode $(OBJDIR)/lint.x3g
	$(PYTHON) scripts/s3g-decompiler.py $(OBJDIR)/lint.x3g
