CFLAGS += -g3 -fno-eliminate-unused-debug-types -fno-eliminate-unused-debug-symbols
CFLAGS += -I${HOME}/opt/include
CXXFLAGS += -std=gnu++0x -g3

CAKE ?= ../../src/cake
PMIRROR ?= $(realpath ../../../libpmirror.hg)
DWARFPP ?= $(realpath ../../../libdwarfpp.hg)

CXX ?= g++-4.5
CXXFLAGS += -I/usr/local/include 
CXXFLAGS += -I/usr/pkg/include 
CXXFLAGS += -I$(shell dirname $(CAKE))/../include 
CXXFLAGS += -I$(shell dirname $(CAKE))/../libcake/include
CXXFLAGS += -I$(shell dirname $(CAKE))/../../libsrk31c++.hg/include
LDFLAGS += -L/usr/local/lib -Wl,-R/usr/local/lib 

ifneq ($(NO_LIBUNWIND),yes)
LDLIBS += -lunwind -lunwind-ptrace -lunwind-$(shell uname -m)
endif

LDFLAGS += -L$(shell dirname $(CAKE))/../libcake  -Wl,-R$(realpath $(shell dirname $(CAKE))/../libcake)
LDFLAGS += -L$(PMIRROR)/lib -Wl,-R$(PMIRROR)/lib
LDFLAGS += -L$(DWARFPP)/lib -Wl,-R$(DWARFPP)/lib

LDLIBS += -lcake -lreflect -lpmirror -ldwarfpp -ldwarf -lelf 

ifeq ($(REVERSE_LDLIBS),yes)
$(warning Before reversing, LDLIBS is $(LDLIBS))
$(warning Echoing LDLIBS gives $(shell echo "$(LDLIBS)"))
# On BSD, we don't have "tac", so use sed -n '1! G; $ p; h'
LDLIBS := $(shell echo "$(LDLIBS)" | tr '[:blank:]' '\n' | sed -n '1! G; $$ p; h' )
$(warning After reversing, LDLIBS is $(LDLIBS))
endif

SRC := $(wildcard *.c)
OBJS ?= $(patsubst %.c,%.o,$(SRC))
$(warning OBJS are $(OBJS))
BASENAME := $(shell basename $(shell readlink -f $(shell pwd)))


default: cake-generated.mk p2k_wrap.o #/usr/src/usr.sbin/puffs/rump_caketmpfs/rump_tmpfs

-include cake-generated.mk

access-test: access-test.c
	$(CC) -o "$@" "$<"

%.o: %.c
	$(CC) $(CFLAGS) -c -o "$@" "$<"

# Including this command seems to imply that there are no commands to run for
# $(BASENAME).o, so if cake-generated.mk happens to be empty (after a failing Cake run),
# it will assume $(BASENAME).o is built (and try to build $(BASENAME)) even when
# it doesn't exist. 
#$(BASENAME).o:: $(BASENAME).cake $(CAKE) cake-generated.mk

#	$(MAKE) CXXFLAGS="$(CXXFLAGS)" -f cake-generated.mk "$@"

# cancel builtin rule
$(BASENAME): $(BASENAME).o

# HACK: workaround missing symbol unbinding
# -- don't use xargs, because some implementations will do nothing
# if given an empty stdin (looking at you, BSD xargs)
$(BASENAME): $(BASENAME).o # FIXME: do the PIPESTATUS job on the line below
	extra_args=$$( objdump -t $(BASENAME).o | \
grep __real_ | \
tr -s '[:blank:]' '\t' | \
cut -f6 | \
sed 's/__real_\(.*\)/-Wl,--defsym,\1=__real_\1/' ); \
	$(CXX) -o $(BASENAME) $(BASENAME).o $(LDFLAGS) $(LDLIBS) $${extra_args}

# We remove cake-generated.mk if Cake didn't succeed, because otherwise it will hang around
# and appear to be up-to-date, preventing re-running of Cake. FIXME: shouldn't Make handle
# this problem itself? I think the PIPESTATUS fix may already handle this case.
ifeq ($(DEBUG),)
cake-generated.mk: $(BASENAME).cake $(CAKE) $(OBJS)
	bash -c '( $(CAKE) $(BASENAME).cake -o cake-generated.mk 1>&2 2>&3 ) 3>&1 | tee cake.log 1>&2; test "$${PIPESTATUS[*]}" == "0 0"' || (rm -f cake-generated.mk; false)
else
cake-generated.mk: $(BASENAME).cake $(CAKE) $(OBJS)
	gdb --eval-command "break main" --eval-command "run $(BASENAME).cake -o $@" $(CAKE) #> "$@"
endif

.PHONY: clean
clean:: 
	rm -f *.o.hpp
	rm -f *_wrap.cpp* *_wrap.o *_wrap.d
	rm -f $(BASENAME) $(BASENAME).o
	rm -f cake-generated.mk

/usr/src/usr.sbin/puffs/rump_caketmpfs/rump_tmpfs: \
   /usr/src/lib/libp2kcake/libp2k.a 
	cd $(dir $@) && make $(notdir $@)

/usr/src/lib/libp2kcake/libp2k.a: \
  p2k_wrap.o
	cd $(dir $@) && make $(notdir $@)

HOOKS := /usr/local/src/libpmirror.hg/lib/libheap_index_preload_hooks.so
run: 
	su -- root -c "LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) \
LD_PRELOAD=\"/usr/local/src/dlmalloc.so $(HOOKS)\" \
gdb --args /usr/src/usr.sbin/puffs/rump_caketmpfs/rump_tmpfs tmpfs /mnt"

# some more recipes
#  419  su -- root -c "gdb --command=gdbinit --args ./rump_tmpfs tmpfs   

# 421  su -- root -c "LD_LIBRARY_PATH=/usr/local/lib:/usr/pkg/lib: 
# LD_PRELOAD=\"
# /usr/local/src/libpmirror.hg/lib/libheap_index_preload_hooks.so 
# /usr/local/src/d
# lmalloc.so\" ./rump_tmpfs tmpfs /mnt" /mnt"

