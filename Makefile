# spitemail Makefile

# Default flags to pass to the C compiler (can be overridden)
CFLAGS ?= -O2 -g

# PROGRAM_NAME determines the installation name and manual page name
PROGRAM_NAME = spitemail

# Required external tools
CC ?= gcc
RM      = rm -f
RMDIR   = rmdir -p
MKDIR   = mkdir -p

# Main object files
OBJS += spitemail.o libjodycode_check.o

# Configuration section
COMPILER_OPTIONS = -Wall -Wwrite-strings -Wcast-align -Wstrict-aliasing -Wstrict-prototypes -Wpointer-arith -Wundef
COMPILER_OPTIONS += -Wshadow -Wfloat-equal -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wformat=2
COMPILER_OPTIONS += -Wextra -Wstrict-overflow=5 -Winit-self
COMPILER_OPTIONS += -std=gnu11 -D_FILE_OFFSET_BITS=64 -fstrict-aliasing -pipe

ifdef USE_LTO
 COMPILER_OPTIONS += -flto
 LINK_OPTIONS += -flto
endif

# Remove unused code if requested
ifdef GC_SECTIONS
 COMPILER_OPTIONS += -fdata-sections -ffunction-sections
 LINK_OPTIONS += -Wl,--gc-sections
endif

UNAME_S=$(shell uname -s)

# Are we running on a Windows OS?
ifeq ($(OS), Windows_NT)
 ifndef NO_WINDOWS
  ON_WINDOWS=1
 endif
else
 exit 1
endif

# Debugging code inclusion
ifdef LOUD
 DEBUG=1
 COMPILER_OPTIONS += -DLOUD_DEBUG
endif
ifdef DEBUG
 COMPILER_OPTIONS += -DDEBUG
else
 COMPILER_OPTIONS += -DNDEBUG
endif
ifdef HARDEN
 COMPILER_OPTIONS += -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fpie -Wl,-z,relro -Wl,-z,now
endif

# MinGW needs this for printf() conversions to work
ifdef ON_WINDOWS
 ifndef NO_UNICODE
  UNICODE=1
  COMPILER_OPTIONS += -municode
 endif
 SUFFIX=.exe
 LIB_EXT=.lib
 COMPILER_OPTIONS += -D__USE_MINGW_ANSI_STDIO=1 -DON_WINDOWS=1
 ifeq ($(UNAME_S), MINGW32_NT-5.1)
  OBJS += winres_xp.o
 else
  OBJS += winres.o
 endif
else
 LIB_EXT=.a
endif

BDYNAMIC = -Wl,-Bdynamic
BSTATIC = -Wl,-Bstatic

### Find and use nearby libjodycode by default
ifndef IGNORE_NEARBY_JC
 ifneq ("$(wildcard ../libjodycode/libjodycode.h)","")
  $(info Found and using nearby libjodycode at ../libjodycode)
  COMPILER_OPTIONS += -I../libjodycode -L../libjodycode
  ifeq ("$(wildcard ../libjodycode/version.o)","")
   $(error You must build libjodycode before building spitemail)
  endif
  ifneq ("$(wildcard ../libjodycode/libjodycode.a)","")
   $(info Overriding static library extension .lib for found extension .a)
   LIB_EXT=.a
  endif
  STATIC_LDFLAGS += ../libjodycode/libjodycode$(LIB_EXT)
 else
  STATIC_LDFLAGS += -ljodycode
 endif
else
 STATIC_LDFLAGS += -ljodycode
endif


CFLAGS += $(COMPILER_OPTIONS) $(CFLAGS_EXTRA)
LDFLAGS += $(LINK_OPTIONS) $(LDFLAGS_EXTRA)


all: libjodycode_hint $(PROGRAM_NAME) static_jc

static_jc: $(PROGRAM_NAME) ljc_vercheck_static
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(BSTATIC) $(STATIC_LDFLAGS) $(BDYNAMIC) -o $(PROGRAM_NAME)$(SUFFIX)

static: $(PROGRAM_NAME) ljc_vercheck_static
	$(CC) $(CFLAGS) $(OBJS) -static $(LDFLAGS) $(BSTATIC) $(STATIC_LDFLAGS) -o $(PROGRAM_NAME)$(SUFFIX)

static_stripped: $(PROGRAM_NAME) static_jc
	-strip $(PROGRAM_NAME)$(SUFFIX)

$(PROGRAM_NAME): $(OBJS)
	@:

winres.o: winres.rc winres.manifest.xml
	./tune_winres.sh
	windres winres.rc winres.o

winres_xp.o: winres_xp.rc
	./tune_winres.sh
	windres winres_xp.rc winres_xp.o

installdirs:
	test -e $(DESTDIR)$(BIN_DIR) || $(MKDIR) $(DESTDIR)$(BIN_DIR)
	test -e $(DESTDIR)$(MAN_DIR) || $(MKDIR) $(DESTDIR)$(MAN_DIR)

install:
	#error

test:
	./test.sh

stripped: $(PROGRAM_NAME) static_jc
	strip $(PROGRAM_NAME)$(SUFFIX)

clean:
	$(RM) $(OBJS) $(OBJS_CLEAN) build_date.h \
		$(PROGRAM_NAME)$(SUFFIX) hashdb_util$(SUFFIX) ljc_vercheck$(SUFFIX) \
		*~ .*.un~ *.gcno *.gcda *.gcov *.obj

distclean: clean
	$(RM) -rf *.pkg.tar* spitemail-*-*/ spitemail-*-*.zip

chrootpackage:
	+./chroot_build.sh

package:
	+./generate_packages.sh $(ARCH)


.PHONY: ljc_vercheck_static

ifeq ($(NOVERCHECK),)
ljc_vercheck_static:
	$(CC) $(CFLAGS) $(CPPFLAGS) libjodycode_check.c -DSTANDALONE $(LDFLAGS) $(BSTATIC) $(STATIC_LDFLAGS) $(BDYNAMIC) -o ljc_vercheck$(SUFFIX)
	@echo
	@./ljc_vercheck 2>/dev/null
	@if [ `(./ljc_vercheck$(SUFFIX) 2>/dev/null || echo ":0:0:0:0") | cut -d: -f4` -lt `grep MY_FEATURELEVEL_REQ libjodycode_check.h | cut -d" " -f3` ]; \
		then echo "The linked libjodycode feature level is too old. Get the latest libjodycode and try again."; \
		else echo "The linked libjodycode feature level is OK."; \
	fi
	@echo
else
ljc_vercheck_static:
	@echo "Version check skipped by request (NOVERCHECK set)"
endif


libjodycode_hint:
	$(info hint: if ../libjodycode is built but spitemail won't run, try doing 'make static_jc')
