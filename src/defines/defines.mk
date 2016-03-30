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
CROSS = true

else ifeq ($(TARGET), mingw32)

CC = /opt/mingw32/cross_win32/bin/i686-w64-mingw32-gcc
CC_FLAGS =
LD_FLAGS =
PLATFORM = win32
EXE = .exe
OBJ = .o
DEP = .d
PYTHON = python
CROSS = true

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
CC_FLAGS = -Wstrict-prototypes -Wformat -Werror=format-security
LD_FLAGS =
PLATFORM = linux
EXE =
OBJ = .o
DEP = .d
PYTHON = python

else ifeq ($(findstring MINGW32,$(UNAME_OS)), MINGW32)

CC = gcc
CC_FLAGS = -DHAS_NANOSLEEP
LD_FLAGS = -static -static-libgcc
PLATFORM = win32
EXE = .exe
OBJ = .o
DEP = .d
PYTHON =

else ifeq ($(findstring CYGWIN,$(UNAME_OS)), CYGWIN)

CC = gcc
CC_FLAGS = -DHAS_NANOSLEEP
LD_FLAGS = -static -static-libgcc
PLATFORM = win32
EXE = .exe
OBJ = .o
DEP = .d
PYTHON = python

else

$(error Unsupported build platform $(UNAME_OS))

endif

# Build directories

SHAREDIR = $(SRCDIR)/shared
PLANNERDIR = $(SRCDIR)/planner
UTILSDIR = $(SRCDIR)/utils
GPXDIR = $(SRCDIR)/gpx
OBJDIR = $(PLATFORM)_obj


INCDIR = -I$(SHAREDIR) -I$(GPXDIR) -I$(PLANNERDIR)

ARCHIVE = $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(PLATFORM)
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
DIFF = diff -b
GZIP = gzip -9c
MKDIR = mkdir -p
RM = rm -f
RMDIR = rm -rf
TAR = tar cf
ZIP = zip -r

#  End OS/Platform dependencies (hopefully)
#
##########
