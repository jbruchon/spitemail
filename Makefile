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
OBJS += spitemail.o datastore.o store_ram.o ui-win32.o

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
  COMPILER_OPTIONS += -mwindows
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

CFLAGS += $(COMPILER_OPTIONS) $(CFLAGS_EXTRA)
LDFLAGS += $(LINK_OPTIONS) $(LDFLAGS_EXTRA)
DYNLIBS += -lcomctl32 -luxtheme

all: $(PROGRAM_NAME)
	-strip $(PROGRAM_NAME)$(SUFFIX)

$(PROGRAM_NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -static $(LDFLAGS) $(BSTATIC) $(STATIC_LDFLAGS) $(BDYNAMIC) $(DYNLIBS) -o $(PROGRAM_NAME)$(SUFFIX)

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
	@echo "It's just one binary, bro. Installation is not supported."
	exit 1

test:
	./test.sh

stripped: $(PROGRAM_NAME) static
	strip $(PROGRAM_NAME)$(SUFFIX)

clean:
	$(RM) $(OBJS) $(OBJS_CLEAN) $(PROGRAM_NAME)$(SUFFIX) \
		*~ .*.un~ *.gcno *.gcda *.gcov *.obj

distclean: clean
	$(RM) -rf spitemail-*-*/ spitemail-*-*.zip
