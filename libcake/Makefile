CXX ?= g++-4.5

.PHONY: default
default: libcake.so

C_SRC := $(wildcard *.c) 
CXX_SRC := $(wildcard *.cpp) $(wildcard *.cc)
C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CXX_OBJS := $(patsubst %.cpp,%.o,$(filter %.cpp,$(CXX_SRC))) \
 $(patsubst %.cc,%.o,$(filter %.cc,$(CXX_SRC)))
OBJS := $(C_OBJS) $(CXX_OBJS)

CPP_DEPS := $(patsubst %.cpp,.%.d,$(CPP_SRC))
CC_DEPS := $(patsubst %.cc,.%.d,$(CPP_SRC))
C_DEPS := $(patsubst %.c,.%.d,$(C_SRC))
DEPS := $(CC_DEPS) $(CPP_DEPS) $(C_DEPS)

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
CXXFLAGS += -I../include

C_DEPS := $(patsubst %.c,.%.c.d,$(C_SRC)) 
CXX_DEPS := $(patsubset %.cc,%,cc.d,$(patsubst %.cpp,.%.cpp.d,$(CXX_SRC)))

.PHONY: clean
clean:
	rm -f *.o *.so $(C_DEPS) $(CXX_DEPS)

libcake.so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared -o "$@"  $(OBJS) $(LDFLAGS) -lreflect -Wl,--whole-archive -ldwarf -Wl,--no-whole-archive -ldl

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
