CFLAGS += -g3 -fno-eliminate-unused-debug-types -fno-eliminate-unused-debug-symbols
CFLAGS += -I${HOME}/opt/include
CXXFLAGS += $(CFLAGS) -std=c++0x

CAKE ?= ../../src/cake

SRC := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRC))
$(warning OBJS are $(OBJS))
BASENAME := $(shell basename $(shell readlink -f $(shell pwd)))

default: $(BASENAME).o

$(BASENAME).o: cake-generated.mk
	$(MAKE) CXXFLAGS="$(CXXFLAGS)" -f cake-generated.mk "$@"

# cancel builtin rule
$(BASENAME): $(BASENAME).o

# HACK: workaround missing symbol unbinding
$(BASENAME): $(BASENAME).o
	objdump -t $(BASENAME).o | grep __real_ | tr -s '[:blank:]' '\t' | cut -f6 | sed 's/__real_\(.*\)/-Wl,--defsym,\1=__real_\1/' | xargs \
	g++ -o $(BASENAME) $(BASENAME).o 

ifeq ($(DEBUG),)
cake-generated.mk $(BASENAME).o_wrap.cpp: $(BASENAME).cake $(CAKE) $(OBJS)
	$(CAKE) $(BASENAME).cake > "$@"
else
cake-generated.mk $(BASENAME).o_wrap.cpp: $(BASENAME).cake $(CAKE) $(OBJS)
	gdb --args $(CAKE) $(BASENAME).cake #> "$@"
endif

.PHONY: clean
clean:: 
	rm -f *.o.hpp
	rm -f *_wrap.cpp* *_wrap.o *_wrap.d
	rm -f cake-generated.mk

include cake-generated.mk
