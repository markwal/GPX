TARGET=
EXE=

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
EXE = '.exe'

else ifeq ($(TARGET), mingw32)

CC = /opt/mingw32/cross_win32/bin/i686-w64-mingw32-gcc
CC_FLAGS =
L_FLAGS = -lm
PLATFORM = win32
EXE = '.exe'

else ifeq ($(UNAME_OS), Darwin)

CC = cc
CC_FLAGS = -DSERIAL_SUPPORT
L_FLAGS = -lm
PLATFORM = osx

else ifeq ($(UNAME_OS), Linux)

CC = cc
CC_FLAGS = -DSERIAL_SUPPORT
L_FLAGS = -lm
PLATFORM = linux

else

@echo Unsupported build platform '$(UNAME_OS)'
false

endif

# File names
VERSION = 2.0
ARCHIVE = gpx-$(PLATFORM)-$(VERSION)
PREFIX = /usr/local
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

all: gpx

.PHONY: all

# Main target
gpx: $(OBJECTS)
	$(CC) $(OBJECTS) $(L_FLAGS) -o gpx$(EXE)

# To obtain object files
%.o: %.c
	$(CC) -c $(CC_FLAGS) $< -o $@

# To remove generated files
clean:
	rm -f gpx$(EXE) $(OBJECTS)
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

# To make a distribution archive
release: gpx
	rm -rf $(ARCHIVE)	# Get rid of previous junk, if any.
	rm -f $(ARCHIVE).tar.gz
	rm -f $(ARCHIVE).zip
	rm -f $(ARCHIVE).dmg
	mkdir $(ARCHIVE)
	cp -r gpx examples scripts *.ini $(ARCHIVE)
	tar cf - $(ARCHIVE) | gzip -9c > $(ARCHIVE).tar.gz
	zip -r $(ARCHIVE).zip $(ARCHIVE)
	test -f /usr/bin/hdiutil && hdiutil create -format UDZO -srcfolder $(ARCHIVE) $(ARCHIVE).dmg
	rm -rf $(ARCHIVE)


# Run unit test
test: gpx
	./gpx lint.gcode
	python ./s3g-decompiler.py lint.x3g
