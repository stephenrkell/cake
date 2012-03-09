CXX ?= g++-4.5

.PHONY: default
default: libcake.so

C_SRC := $(wildcard *.c) 
CPP_SRC := $(wildcard *.cpp)
CC_SRC := $(wildcard *.cc)
C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CC_OBJS := $(patsubst %.cc,%.o,$(filter %.cc,$(CC_SRC)))
CPP_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRC)) 
OBJS := $(C_OBJS) $(CC_OBJS) $(CPP_OBJS)

CPP_DEPS := $(patsubst %.cpp,.%.d,$(CPP_SRC))
CC_DEPS := $(patsubst %.cc,.%.d,$(CC_SRC))
C_DEPS := $(patsubst %.c,.%.d,$(C_SRC))
DEPS := $(CC_DEPS) $(CPP_DEPS) $(C_DEPS)
$(warning DEPS are $(DEPS))

$(CPP_DEPS): .%.d : %.cpp
	g++ -MM $(CXXFLAGS) "$<"  > "$@"
$(CC_DEPS): .%.d : %.cc
	g++ -MM $(CXXFLAGS) "$<"  > "$@"
$(C_DEPS): .%.d : %.c
	gcc -MM $(CFLAGS) "$<"  > "$@"

-include $(DEPS)

CFLAGS += -std=c99 -g3 -fPIC
CFLAGS += -Iinclude

CXXFLAGS += -std=c++0x -g3 -fPIC
CXXFLAGS += -Iinclude
CXXFLAGS += -I../include -Wno-deprecated-declarations

C_DEPS := $(patsubst %.c,.%.c.d,$(C_SRC)) 
CXX_DEPS := $(patsubset %.cc,%,cc.d,$(patsubst %.cpp,.%.cpp.d,$(CXX_SRC)))

.PHONY: clean
clean:
	rm -f *.o *.so $(C_DEPS) $(CXX_DEPS)

LDLIBS += -lreflect -Wl,--whole-archive -ldwarf -Wl,--no-whole-archive 
ifneq ($(LIBC_INCLUDES_LIBDL),yes)
LDLIBS += -ldl
endif

libcake.so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared -o "$@"  $(OBJS) $(LDFLAGS) $(LDLIBS)

$(CXX_DEPS): .%.cpp.d : %.cpp
	$(CXX) -MM $(CXXFLAGS) "$<"  > "$@"
$(CXX_DEPS): .%.cc.d : %.cc
	$(CXX) -MM $(CXXFLAGS) "$<"  > "$@"
$(C_DEPS): .%.c.d : %.c
	gcc -MM $(CFLAGS) "$<"  > "$@"
	
-include $(DEPS)

%.o: %.cpp #$(HDRS)
	$(CXX) $(CXXFLAGS) -c -o "$@" "$<"
%.o: %.cc #$(HDRS)
	$(CXX) $(CXXFLAGS) -c -o "$@" "$<"
