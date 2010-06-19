C_SRC := $(wildcard *.c) 
CXX_SRC := $(wildcard *.cpp)
C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CXX_OBJS := $(patsubst %.cpp,%.o,$(CXX_SRC))
OBJS := $(C_OBJS) $(CXX_OBJS)

CFLAGS += -std=c99 -g3
CXXFLAGS += -std=c++0x -g3 
CXXFLAGS += -I${HOME}/work/devel/dwarf/libdwarfpp/include
CXXFLAGS += -I${HOME}/work/devel/libsrk31c++
CXXFLAGS += -I${HOME}/work/devel/c++-fileno/include
CXXFLAGS += -I${HOME}/scratch/opt/include
CXXFLAGS += -I${HOME}/opt/include

default: libcake.so

libcake.so: $(OBJS)
	$(CXX) -shared -o "$@"  $(OBJS) -ldl
