C_SRC := $(wildcard *.c) 
CXX_SRC := $(wildcard *.cpp)
C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CXX_OBJS := $(patsubst %.cpp,%.o,$(CXX_SRC))
OBJS := $(C_OBJS) $(CXX_OBJS)

CFLAGS += -std=c99

default: libcake.so

libcake.so: $(OBJS)
	$(CXX) -shared -o "$@"  $(OBJS)
