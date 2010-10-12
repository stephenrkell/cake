CXXFLAGS += -fno-eliminate-unused-debug-symbols -fno-eliminate-unused-debug-types

SRC := $(wildcard *.cpp)
DEPS = $(patsubst %.cpp,.%.d,$(SRC))
HDRS := $(wildcard *.hpp)
#HDR_DEPS = $(patsubst %.hpp,.%.hpp.d,$(HDRS))

OBJS += $(patsubst %.cpp,%.o,$(SRC)) 
#OBJS += parser/cakeCLexer.o parser/cakeCParser.o
#$(warning OBJS is $(OBJS))

LDLIBS := -lantlr3c
LDLIBS += -ldwarfpp -ldwarf -lelf
LDLIBS += -lsrk31c++ -lc++fileno -lboost_filesystem -lboost_regex 

CXXFLAGS += -Iparser/include -Iinclude
CXXFLAGS += -g3 -O0 -Wall -std=c++0x 
GCJFLAGS += -findirect-dispatch -fno-indirect-classes -fPIC -g3 --classpath=$(CLASSPATH) -Wall 

CAKE_VERSION := 0.0$(shell stat --format='%y' *.cpp | sort | tail -n1 |  tr -cd '[:digit:]' )
$(warning Generated Cake version macro $(CAKE_VERSION))

CFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"
CXXFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"

.PHONY: default
default: cake

.PHONY: parser-recursive
parser-recursive: 
	$(MAKE) -C parser

# cancel builtin implicit rule
%.o: %.cpp

.PHONY: clean
clean:: 
	rm -rf gcjh_generated
	rm -f cake *.o *.so
	rm -f $(DEPS)
	$(MAKE) -C parser clean

#$(HDR_DEPS) : .%.hpp.d: %.hpp
#	g++ -MM -fpch-deps $(CXXFLAGS) "$<" > "$@"
#	
#include $(HDR_DEPS)

$(DEPS) : .%.d : %.cpp
	g++ -MM $(CXXFLAGS) "$<"  > "$@"
	
include $(DEPS)

# new implicit rule for compiling C++
%.o: %.cpp
	g++ -c -o "$@" ${CXXFLAGS} "$<"

%.hpp.gch: %.hpp
	g++ -o "$@" ${CXXFLAGS} "$<"

#$(warning OBJS is $(OBJS))
cake: parser-recursive $(OBJS) 
	g++ $(CXXFLAGS) $(LDFLAGS) \
	 $(OBJS) parser/cakeC{Parser,Lexer}.o $(LDLIBS) -o "$@" 

.PHONY: test
RUN_PREFIX := gdb --eval-command "source ../src/cake-test.gdb" --eval-command run --args
test: cake
	cd ../examples; LD_LIBRARY_PATH=../src:${LD_LIBRARY_PATH} $(RUN_PREFIX) ../src/cake switch-simple.cake

.PHONY: test_batch
test_batch: cake
	cd ../examples; LD_LIBRARY_PATH=../src:${LD_LIBRARY_PATH} ../src/cake switch-simple.cake

.PHONY: memtest
memtest: cake
	cd ../examples; LD_LIBRARY_PATH=../src:$(LD_LIBRARY_PATH) valgrind --tool=memcheck ../src/cake switch-simple.cake
