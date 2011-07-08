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
CFLAGS += -I${HOME}/work/devel/libprocessimage/include

CXXFLAGS += -std=c++0x -g3 -fPIC
CXXFLAGS += -I${HOME}/work/devel/libdwarfpp/include
CXXFLAGS += -I${HOME}/work/devel/libprocessimage/include
CXXFLAGS += -I${HOME}/work/devel/libsrk31c++/include
CXXFLAGS += -I${HOME}/work/devel/c++-fileno/include
CXXFLAGS += -I${HOME}/scratch/opt/include
CXXFLAGS += -I${HOME}/opt/include
CXXFLAGS += -Iinclude
CXXFLAGS += -I../include

LDFLAGS += -L${HOME}/work/devel/libdwarfpp/lib -Wl,-R$(realpath ${HOME}/work/devel/libdwarfpp/lib)
LDFLAGS += -L${HOME}/work/devel/libprocessimage/src -Wl,-R$(realpath ${HOME}/work/devel/libprocessimage/src)
LDFLAGS += -L/usr/local/lib

C_DEPS := $(patsubst %.c,.%.c.d,$(C_SRC)) 
CXX_DEPS := $(patsubset %.cc,%,cc.d,$(patsubst %.cpp,.%.cpp.d,$(CXX_SRC)))

.PHONY: clean
clean:
	rm -f *.o *.so

libcake.so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared -o "$@"  $(OBJS) $(LDFLAGS) -lprocessimage -Wl,--whole-archive -ldwarf -Wl,--no-whole-archive -ldl

$(CXX_DEPS): .%.cpp.d : %.cpp
	$(CXX) -MM $(CXXFLAGS) "$<"  > "$@"
$(CXX_DEPS): .%.cc.d : %.cc
	$(CXX) -MM $(CXXFLAGS) "$<"  > "$@"
$(C_DEPS): .%.c.d : %.c
	gcc -MM $(CFLAGS) "$<"  > "$@"
	
include $(DEPS)

%.o: %.cpp #$(HDRS)
	$(CXX) $(CXXFLAGS) -c -o "$@" "$<"
%.o: %.cc #$(HDRS)
	$(CXX) $(CXXFLAGS) -c -o "$@" "$<"
