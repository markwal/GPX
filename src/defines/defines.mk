TARGET=

# OS specific settings

UNAME_OS := $(subst /,_,$(shell uname -s))
UNAME_ARCH := $(subst /,_,$(shell uname -p))

ifeq ($(TARGET), mingw64)

CC = /opt/mingw64/cross_win64/bin/x86_64-w64-mingw32-gcc
CC_FLAGS =
LD_FLAGS =
PLATFORM = win64
EXE = .exe
OBJ = .o
DEP = .d
PYTHON = python

else ifeq ($(TARGET), mingw32)

CC = /opt/mingw32/cross_win32/bin/i686-w64-mingw32-gcc
CC_FLAGS =
LD_FLAGS =
PLATFORM = win32
EXE = .exe
OBJ = .o
DEP = .d
PYTHON = python

else ifeq ($(UNAME_OS), Darwin)

CC = cc
CC_FLAGS =
LD_FLAGS =
PLATFORM = osx
EXE =
OBJ = .o
DEP = .d
PYTHON = python

else ifeq ($(UNAME_OS), Linux)

CC = cc
CC_FLAGS =
LD_FLAGS =
PLATFORM = linux
EXE =
OBJ = .o
DEP = .d
PYTHON = python

else ifeq ($(findstring MINGW32,$(UNAME_OS)), MINGW32)

CC = gcc
CC_FLAGS =
LD_FLAGS =
PLATFORM = win32
EXE = .exe
OBJ = .o
DEP = .d
PYTHON =

else

$(error Unsupported build platform $(UNAME_OS))

endif

# Build directories

VERSION = 2.1
INCDIR  = -I$(SRCDIR)/shared -I$(SRCDIR)/gpx
SHAREDIR = $(SRCDIR)/shared
OBJDIR  = $(PLATFORM)_obj
ARCHIVE = gpx-$(VERSION)-$(PLATFORM)
ARCHDIR = $(OBJDIR)/$(ARCHIVE)
MACHINEDIR = $(SRCDIR)/../machines

#######
#
#  Since we may need to compile sources from other directories,
#  use make's VPATH functionality

VPATH=./ $(SHAREDIR)

# OS commands

CD = cd
CHMOD = chmod
CP = cp
DIFF = diff
GZIP = gzip -9c
MKDIR = mkdir -p
RM = rm -f
RMDIR = rm -rf
TAR = tar cf
ZIP = zip -r

#  End OS/Platform dependencies (hopefully)
#
##########
