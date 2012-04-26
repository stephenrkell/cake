#CXX ?= c++
#CXX ?= g++-4.5

CXXFLAGS += -fno-eliminate-unused-debug-symbols -fno-eliminate-unused-debug-types 
CXXFLAGS += -DBOOST_HAS_RVALUE_REFS 
CXXFLAGS += -Wno-deprecated-declarations

SRC := $(wildcard *.cpp)
DEPS = $(patsubst %.cpp,.%.d,$(SRC))

OBJS += $(patsubst %.cpp,%.o,$(SRC)) 

LDLIBS += -ldwarfpp -ldwarf -lelf
LDLIBS += -lsrk31c++ -lc++fileno -lboost_filesystem -lboost_regex -lboost_system

CXXFLAGS += -g3 -O0 -Wall -std=c++0x 

.PHONY: default
default: shareability

# cancel builtin implicit rule
%.o: %.cpp

%: %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o "$@" "$<" $(LDLIBS)

.PHONY: clean
clean:: 
	rm -f sharebility *.o *.so
	rm -f $(DEPS)

#$(HDR_DEPS) : .%.hpp.d: %.hpp
#	g++ -MM -fpch-deps $(CXXFLAGS) "$<" > "$@"
#	
#include $(HDR_DEPS)

$(DEPS) : .%.d : %.cpp
	$(CXX) -MM $(CXXFLAGS) "$<"  > "$@"

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif	
