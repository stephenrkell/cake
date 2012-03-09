#CXX ?= c++
CXX ?= g++-4.5

CXXFLAGS += -fno-eliminate-unused-debug-symbols -fno-eliminate-unused-debug-types 
CXXFLAGS += -DBOOST_HAS_RVALUE_REFS 

SRC := $(wildcard *.cpp)
DEPS = $(patsubst %.cpp,.%.d,$(SRC))
HDRS := $(wildcard *.hpp)
#HDR_DEPS = $(patsubst %.hpp,.%.hpp.d,$(HDRS))

OBJS += $(patsubst %.cpp,%.o,$(SRC)) 
#OBJS += parser/cakeCLexer.o parser/cakeCParser.o
#$(warning OBJS is $(OBJS))

LDLIBS := -lantlr3c
LDLIBS += -ldwarfpp -ldwarf -lelf
LDLIBS += -lsrk31c++ -lc++fileno -lboost_filesystem -lboost_regex -lboost_system

CXXFLAGS += -Iparser/include -I../include
CXXFLAGS += -g3 -O0 -Wall -std=c++0x 

# HACK while various bits of Cake source code are not optimal....
CXXFLAGS += -Wno-deprecated-declarations -Wno-sequence-point -Wno-unused-variable -Wno-unused-label

GCJFLAGS += -findirect-dispatch -fno-indirect-classes -fPIC -g3 --classpath=$(CLASSPATH) -Wall 

CAKE_VERSION := 0.0$(shell stat --format='%y' *.cpp | sort | tail -n1 |  tr -cd '[:digit:]' )
$(warning Generated Cake version macro $(CAKE_VERSION))

CFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"
CXXFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"

.PHONY: default
default: cake parser/test_parser

# cancel builtin implicit rule
%.o: %.cpp

.PHONY: clean
clean:: 
	rm -rf gcjh_generated
	rm -f cake *.o *.so
	rm -f $(DEPS)

#$(HDR_DEPS) : .%.hpp.d: %.hpp
#	g++ -MM -fpch-deps $(CXXFLAGS) "$<" > "$@"
#	
#include $(HDR_DEPS)

$(DEPS) : .%.d : %.cpp parser/include/cakeCParser.h parser/include/cakeCLexer.h
	$(CXX) -MM $(CXXFLAGS) "$<"  > "$@"

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif	

# new implicit rule for compiling C++
%.o: %.cpp
	$(CXX) -c -o "$@" ${CXXFLAGS} "$<"

%.hpp.gch: %.hpp
	$(CXX) -o "$@" ${CXXFLAGS} "$<"

#$(warning OBJS is $(OBJS))
cake: $(OBJS) parser/cakeCParser.o parser/cakeCLexer.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) \
	 $(OBJS) parser/cakeCParser.o parser/cakeCLexer.o $(LDLIBS) -o "$@" 

.PHONY: test
RUN_PREFIX := gdb --eval-command "source ../src/cake-test.gdb" --eval-command run --args
test:: cake
	cd ../examples; LD_LIBRARY_PATH=../src:${LD_LIBRARY_PATH} $(RUN_PREFIX) ../src/cake switch-simple.cake

.PHONY: test_batch
test_batch: cake
	cd ../examples; LD_LIBRARY_PATH=../src:${LD_LIBRARY_PATH} ../src/cake switch-simple.cake

.PHONY: memtest
memtest: cake
	cd ../examples; LD_LIBRARY_PATH=../src:$(LD_LIBRARY_PATH) valgrind --tool=memcheck ../src/cake switch-simple.cake
.PHONY: default
#default: java-all 

# Begin parser rules 
ANTLR_M4_PATH ?= ../../../antlr-m4
M4PATH := $(ANTLR_M4_PATH):$(DWARFIDL_PATH)/src/parser:$(M4_PATH)
export M4PATH
GRAMMAR_OPTIONS += backtrack=true output=AST
GRAMMAR_BASENAME := cake
GRAMMAR_PATH := parser
include $(ANTLR_M4_PATH)/Makerules

test:: test-samples

.PHONY: test-samples
test-samples: parser/cakePyParser.py parser/samples/ephy.cake parser/samples/mpeg2ffplay.cake parser/samples/p2k.cake parser/samples/xcl.cake
	for fname in parser/samples/xcl.cake samples/ephy.cake samples/mpeg2ffplay.cake samples/p2k.cake ; do \
		python parser/cakePyParser.py --rule=toplevel < "$$fname"; \
	done | $(ANTLR_M4_PATH)/parens-filter.sh
		
.PHONY: test-lexer-samples
test-lexer-samples: parser/cakePyLexer.py # samples/ephy.cake samples/xcl.cake
	for fname in parser/samples/ephy.cake ; do \
		python parser/cakePyLexer.py < "$$fname"; \
	done 
