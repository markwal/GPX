TARGET=

# Declaration of variables
UNAME_OS := $(subst /,_,$(shell uname -s))
UNAME_ARCH := $(subst /,_,$(shell uname -p))

ifeq ("1", "1")
endif

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

else

@echo Unsupported build platform '$(UNAME_OS)'
false

endif

VERSION = 2.1
OBJDIR = $(PLATFORM)_obj
ARCHIVE = gpx-$(VERSION)-$(PLATFORM)
ARCHDIR = $(OBJDIR)/$(ARCHIVE)
PREFIX = /usr/local

SOURCES = $(wildcard *.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:%.c=%.o))

MKDIR = mkdir -p
CP = cp
CHMOD = chmod

all: $(OBJDIR)/gpx

.PHONY: all

# Main target
$(OBJDIR)/gpx: $(OBJDIR) $(OBJECTS)
	$(CC) $(OBJECTS) $(L_FLAGS) -o $(OBJDIR)/gpx$(EXE)

$(OBJDIR):
	-@$(MKDIR) $(OBJDIR)

# To obtain object files
$(OBJDIR)/%.o: %.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

# To make a package
package: $(OBJDIR)/gpx
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
	tar cf - $(ARCHDIR) | gzip -9c > $(ARCHIVE).tar.gz
else
	zip -r $(ARCHIVE).zip $(ARCHDIR)
endif


# To remove generated files
clean:
	rm -rf $(OBJDIR)
	rm -f $(ARCHIVE).tar.gz
	rm -f $(ARCHIVE).zip
	rm -f $(ARCHIVE).dmg

# To install program and supporting files
install: gpx
	test -d $(PREFIX) || mkdir $(PREFIX)
	test -d $(PREFIX)/bin || mkdir $(PREFIX)/bin
	install -m 0755 gpx $(PREFIX)/bin
#	test -d $(PREFIX)/share || mkdir $(PREFIX)/share
#	test -d $(PREFIX)/share/gpx || mkdir -p $(PREFIX)/share/gpx
#	for INI in *.ini; do \
#		install -m 0644 $$INI $(PREFIX)/share/gpx; \
#	done


# Run unit test
test: gpx
	./gpx lint.gcode
	python ./s3g-decompiler.py lint.x3g
