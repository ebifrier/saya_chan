EXE  = saya_chan
COMP = gcc

### Built-in benchmark for pgo-builds
PGOBENCH = ./$(EXE) bench 32 1 10 default depth

### Object files
OBJS = mate1ply.o misc.o timeman.o evaluate.o move.o position.o tt.o main.o \
	 movegen.o search.o uci.o movepick.o thread.o ucioption.o \
	 benchmark.o book.o \
	 shogi.o mate.o problem.o
# bitbase.o bitboard.o \
#	material.o pawns.o
#  endgame.o SearchMateDFPN.o

### ==========================================================================
### Section 2. High-level Configuration
### ==========================================================================
#
# -DNDEBUG (DEBUG)     builds release (debug) version of Bonanza.
# -DHAVE_SSE2 -msse2   use SSE2 instructions for speed
# -DHAVE_SSE4 -msse4.1 use SSE2 and SSE4.1 instructions for speed
# -DUSE_PREFETCH       use prefetch x86 asm-instruction
# -DUSE_BSFQ           use bsfq x86_64 asm-instruction
# -DUSE_POPCNT         use popcnt x86_64 asm-instruction
# -DINANIWA_SHIFT      enables an Inaniwa strategy detection.
# -DIS_64BIT           64-/32-bit operating system
# -DCHK_PERFORM        count performance counter.
#
# flag                --- Comp switch --- Description
# ----------------------------------------------------------------------------
#
# debug = no/yes      --- -DNDEBUG    --- Enable/Disable debug mode
# optimize = yes/no   --- (-O3/-fast etc.) --- Enable/Disable optimizations
# arch = (name)       --- (-arch)     --- Target architecture
# os = (name)         ---             --- Target operating system
# bits = 64/32        --- -DIS_64BIT  --- 64-/32-bit operating system
# bigendian = no/yes  --- -DBIGENDIAN --- big/little-endian byte order
# prefetch = no/yes   --- -DUSE_PREFETCH  --- Use prefetch x86 asm-instruction
# bsfq = no/yes       --- -DUSE_BSFQ  --- Use bsfq x86_64 asm-instruction
#                                     --- (Works only with GCC and ICC 64-bit)
# popcnt = no/yes     --- -DUSE_POPCNT --- Use popcnt x86_64 asm-instruction
#
# mingw
#  CXX: g++
#  CXXFLAGS: -g -Wall -Wcast-qual -fno-exceptions -fno-rtti  -DSHOGI -DNANOHA -DPROMOTE_AS_CAPTURE -Wextra -Wshadow -DNDEBUG -O3 -msse
#  LDFLAGS: -lpthread
#
# gcc
#  CXX: g++
#  CXXFLAGS: -g -Wall -Wcast-qual -fno-exceptions -fno-rtti  -DSHOGI -DNANOHA -DPROMOTE_AS_CAPTURE -ansi -pedantic -Wno-long-long -Wextra -Wshadow -DNDEBUG -O3 -msse
#  LDFLAGS: -lpthread

# x86-section
ifeq ($(ARCH),x86-64)
	arch = x86_64
	os = any
	bits = 64
	bigendian = no
	prefetch = yes
	bsfq = yes
	popcnt = no
endif

ifeq ($(ARCH),x86-64-modern)
	arch = x86_64
	os = any
	bits = 64
	bigendian = no
	prefetch = yes
	bsfq = yes
	popcnt = yes
endif

ifeq ($(ARCH),x86-32)
	arch = i386
	os = any
	bits = 32
	bigendian = no
	prefetch = yes
	bsfq = no
	popcnt = no
endif

ifeq ($(ARCH),x86-32-old)
	arch = i386
	os = any
	bits = 32
	bigendian = no
	prefetch = no
	bsfq = no
	popcnt = no
endif

### ==========================================================================
### Section 3. Low-level configuration
### ==========================================================================

### 3.1 Selecting compiler (default = gcc)
ifeq ($(COMP),)
	COMP=gcc
endif

ifeq ($(COMP),mingw)
	comp=mingw
	CXX=g++
	profile_prepare = gcc-profile-prepare
	profile_make = gcc-profile-make
	profile_use = gcc-profile-use
	profile_clean = gcc-profile-clean
endif

ifeq ($(COMP),gcc)
	comp=gcc
	CXX=g++
	profile_prepare = gcc-profile-prepare
	profile_make = gcc-profile-make
	profile_use = gcc-profile-use
	profile_clean = gcc-profile-clean
endif

ifeq ($(COMP),icc)
	comp=icc
	CXX=icpc
	profile_prepare = icc-profile-prepare
	profile_make = icc-profile-make
	profile_use = icc-profile-use
	profile_clean = icc-profile-clean
endif

ifeq ($(COMP),clang)
	comp=clang
	CXX=clang
	profile_prepare = gcc-profile-prepare
	profile_make = gcc-profile-make
	profile_use = gcc-profile-use
	profile_clean = gcc-profile-clean
endif

### 3.2 General compiler settings
WARNFLAGS = 
#WARNFLAGS = -Wcast-qual -Weffc++ -Wenum-compare -Wold-style-cast -Wparentheses \
#	-Wsequence-point -Wsign-conversion -Wsign-promo -Wswitch -Wswitch-default -Wswitch-enum
#CXXFLAGS = -g -Wall $(WARNFLAGS) -fno-exceptions -fno-rtti $(EXTRACXXFLAGS) -DEVAL_OLD -DNANOHA -DCHK_PERFORM -DPROMOTE_AS_CAPTURE
#CXXFLAGS = -O3 -DNDEBUG -Wall $(WARNFLAGS) -fno-exceptions -fno-rtti $(EXTRACXXFLAGS) -DEVAL_OLD -DNANOHA -DCHK_PERFORM -DPROMOTE_AS_CAPTURE
CXXFLAGS =-march=native -mtune=native -Ofast -DNDEBUG -Wall $(WARNFLAGS) -fno-exceptions -fno-rtti $(EXTRACXXFLAGS) -DNANOHA -DSAYA -DNO_NARAZU -DCHK_PERFORM

ifeq ($(comp),gcc)
	CXXFLAGS += -ansi -pedantic -Wno-long-long -Wextra -Wshadow
endif

ifeq ($(comp),mingw)
	CXXFLAGS += -Wextra -Wshadow
endif

ifeq ($(comp),icc)
	CXXFLAGS += -wd383,981,1418,1419,10187,10188,11505,11503 -Wcheck -Wabi -Wdeprecated -strict-ansi
endif

ifeq ($(comp),clang)
	CXXFLAGS += --analyze -m32 -std=c++0x -Wextra -Wshadow \
	-I/usr/lib/gcc/i686-pc-cygwin/4.7.3/include/c++ -I/usr/lib/gcc/i686-pc-cygwin/4.7.3/include \
	-I/usr/lib/gcc/i686-pc-cygwin/4.7.3/include/c++/i686-pc-cygwin \
	-D_WIN32
endif

### 3.3 General linker settings
LDFLAGS = -lpthread $(EXTRALDFLAGS)

ifeq ($(os),osx)
	LDFLAGS += -arch $(arch)
endif

### 3.4 Debugging
ifeq ($(debug),no)
	CXXFLAGS += -DNDEBUG
endif

### 3.5 Optimization
ifeq ($(optimize),yes)
	ifeq ($(comp),gcc)
		CXXFLAGS += -Ofast
	endif

	ifeq ($(comp),mingw)
		CXXFLAGS += -Ofast
	endif

	ifeq ($(comp),icc)
		ifeq ($(os),osx)
			CXXFLAGS += -fast -mdynamic-no-pic
		else
			CXXFLAGS += -Ofast
		endif
	endif
endif

### 3.6. Bits
ifeq ($(bits),64)
	CXXFLAGS += -DIS_64BIT
endif

### 3.8 prefetch
ifeq ($(prefetch),yes)
	CXXFLAGS += -msse
	DEPENDFLAGS += -msse
else
	CXXFLAGS += -DNO_PREFETCH
endif

### 3.9 bsfq
ifeq ($(bsfq),yes)
	CXXFLAGS += -DUSE_BSFQ
endif

### 3.10 popcnt
ifeq ($(popcnt),yes)
	CXXFLAGS += -DUSE_POPCNT
endif

### ==========================================================================
### Section 4. Public targets
### ==========================================================================

default:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) build

help:
	@echo ""
	@echo "To compile stockfish, type: "
	@echo ""
	@echo "make target ARCH=arch [COMP=comp]"
	@echo ""
	@echo "Supported targets:"
	@echo ""
	@echo "build                > Build unoptimized version"
	@echo "profile-build        > Build PGO-optimized version"
	@echo "popcnt-profile-build > Build PGO-optimized version with optional popcnt-support"
	@echo "clean                > Clean up"
	@echo "testrun              > Make sample run"
	@echo ""
	@echo "Supported archs:"
	@echo ""
	@echo "x86-64               > x86 64-bit"
	@echo "x86-64-modern        > x86 64-bit with runtime support for popcnt-instruction"
	@echo "x86-32               > x86 32-bit excluding very old hardware without SSE-support"
	@echo "x86-32-old           > x86 32-bit including also very old hardware"
	@echo ""
	@echo "Supported comps:"
	@echo ""
	@echo "gcc                  > Gnu compiler (default)"
	@echo "icc                  > Intel compiler"
	@echo "mingw                > Gnu compiler with MinGW under Windows"
	@echo ""
	@echo "Non-standard targets:"
	@echo ""
	@echo "make hpux           >  Compile for HP-UX. Compiler = aCC"
	@echo ""
	@echo "Examples. If you don't know what to do, you likely want to run: "
	@echo ""
	@echo "make profile-build ARCH=x86-64    (This is for 64-bit systems)"
	@echo "make profile-build ARCH=x86-32    (This is for 32-bit systems)"
	@echo ""

build:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) config-sanity
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) all

profile-build:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) config-sanity
	@echo ""
	@echo "Step 0/4. Preparing for profile build."
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_prepare)
	@echo ""
	@echo "Step 1/4. Building executable for benchmark ..."
	@touch *.cpp *.h
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_make)
	@echo ""
	@echo "Step 2/4. Running benchmark for pgo-build ..."
	@$(PGOBENCH) > /dev/null
	@echo ""
	@echo "Step 3/4. Building final executable ..."
	@touch *.cpp
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_use)
	@echo ""
	@echo "Step 4/4. Deleting profile data ..."
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_clean)

popcnt-profile-build:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) config-sanity
	@echo ""
	@echo "Step 0/6. Preparing for profile build."
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_prepare)
	@echo ""
	@echo "Step 1/6. Building executable for benchmark (popcnt disabled)..."
	@touch *.cpp *.h
	$(MAKE) ARCH=x86-64 COMP=$(COMP) $(profile_make)
	@echo ""
	@echo "Step 2/6. Running benchmark for pgo-build (popcnt disabled)..."
	@$(PGOBENCH) > /dev/null
	@echo ""
	@echo "Step 3/6. Building executable for benchmark (popcnt enabled)..."
	@touch *.cpp *.h
	$(MAKE) ARCH=x86-64-modern COMP=$(COMP) $(profile_make)
	@echo ""
	@echo "Step 4/6. Running benchmark for pgo-build (popcnt enabled)..."
	@$(PGOBENCH) > /dev/null
	@echo ""
	@echo "Step 5/6. Building final executable ..."
	@touch *.cpp *.h
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_use)
	@echo ""
	@echo "Step 6/6. Deleting profile data ..."
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_clean)
	@echo ""

clean:
	$(RM) $(EXE) $(EXE).exe *.o .depend *~ core bench.txt *.gcda

testrun:
	@$(PGOBENCH)

### ==========================================================================
### Section 5. Private targets
### ==========================================================================

all: $(EXE)

config-sanity:
	@echo ""
	@echo "Config:"
	@echo "debug: '$(debug)'"
	@echo "optimize: '$(optimize)'"
	@echo "arch: '$(arch)'"
	@echo "os: '$(os)'"
	@echo "bits: '$(bits)'"
	@echo "bigendian: '$(bigendian)'"
	@echo "prefetch: '$(prefetch)'"
	@echo "bsfq: '$(bsfq)'"
	@echo "popcnt: '$(popcnt)'"
	@echo ""
	@echo "Flags:"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo ""
	@echo "Testing config sanity. If this fails, try 'make help' ..."
	@echo ""

$(EXE): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

gcc-profile-prepare:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) gcc-profile-clean

gcc-profile-make:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) \
	EXTRACXXFLAGS='-fprofile-generate' \
	EXTRALDFLAGS='-lgcov' \
	all

gcc-profile-use:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) \
	EXTRACXXFLAGS='-fprofile-use' \
	all

gcc-profile-clean:
	@rm -rf *.gcda *.gcno bench.txt

icc-profile-prepare:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) icc-profile-clean
	@mkdir profdir

icc-profile-make:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) \
	EXTRACXXFLAGS='-prof-gen=srcpos -prof_dir ./profdir' \
	all

icc-profile-use:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) \
	EXTRACXXFLAGS='-prof_use -prof_dir ./profdir' \
	all

icc-profile-clean:
	@rm -rf profdir bench.txt
