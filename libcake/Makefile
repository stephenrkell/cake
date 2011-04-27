C_SRC := $(wildcard *.c) 
CXX_SRC := $(wildcard *.cpp)
C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CXX_OBJS := $(patsubst %.cpp,%.o,$(CXX_SRC))
OBJS := $(C_OBJS) $(CXX_OBJS)

CFLAGS += -std=c99 -g3 -fPIC
CXXFLAGS += -std=c++0x -g3 -fPIC
CXXFLAGS += -I${HOME}/work/devel/dwarf/libdwarfpp/include
CXXFLAGS += -I${HOME}/work/devel/libsrk31c++
CXXFLAGS += -I${HOME}/work/devel/c++-fileno/include
CXXFLAGS += -I${HOME}/scratch/opt/include
CXXFLAGS += -I${HOME}/opt/include

C_DEPS := $(patsubst %.c,.%.c.d,$(C_SRC)) 
CXX_DEPS := $(patsubst %.cpp,.%.cpp.d,$(CXX_SRC))


.PHONY: default
default: libcake.so

.PHONY: clean
clean:
	rm -f *.o *.so

libcake.so: $(OBJS)
	$(CXX) -shared -o "$@"  $(OBJS) -ldl

$(CXX_DEPS): .%.cpp.d : %.cpp
	g++ -MM $(CXXFLAGS) "$<"  > "$@"
$(C_DEPS): .%.c.d : %.c
	gcc -MM $(CFLAGS) "$<"  > "$@"
	
include $(DEPS)

%.o: %.cpp #$(HDRS)
	g++ $(CXXFLAGS) -c -o "$@" "$<"
