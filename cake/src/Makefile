EXTRA_DEBUG_OPTS := -fno-eliminate-unused-debug-symbols -fno-eliminate-unused-debug-types

JAVA_OBJS := $(patsubst %.java,%.o,$(shell find java -name '*.java'))

SRC := $(wildcard *.cpp) #$(wildcard *.hpp)
DEPS = $(patsubst %.cpp,.%.d,$(SRC))
# main.cpp cppcatch.cpp javacatch.cpp cake.cpp alias.cpp util.cpp module.cpp exists.cpp derive.cpp link.cpp supplementary.cpp

#OBJS := $(JAVA_OBJS) 
OBJS :=
OBJS += $(patsubst %.cpp,%.o,$(SRC)) 
#OBJS += parser/parser.a 
#OBJS += antlr-runtime.jar.so stringtemplate.jar.so
#OBJS += parser/cakeCLexer.o parser/cakeCParser.o
$(warning OBJS is $(OBJS))

BOOST_LIB ?= $(shell ls /usr/lib/libboost*system.so | sed 's/.*lib\(.*\)\.so/\1/')

LIBS := -l$(BOOST_LIB) -lantlr3c -lsrk31c++ -Wl,--whole-archive -ldwarf -Wl,--no-whole-archive -ldwarfpp -lfileno -lelf

INCLUDES += -Iparser/include -Iinclude -Ijava/include
INCLUDES += -I../../dwarf/libdwarfpp/include -I../../libsrk31c++ -I../../c++-fileno/include
INCLUDES += -I$(HOME)/opt/include 

CLASSPATH := ./java:$(CLASSPATH)
$(warning CLASSPATH is $(CLASSPATH))

CXXFLAGS += $(EXTRA_DEBUG_OPTS) -fPIC -g3 $(INCLUDES) -Wall #-D_STREAM_COMPAT
GCJFLAGS += $(EXTRA_DEBUG_OPTS) -findirect-dispatch -fno-indirect-classes -fPIC -g3 --classpath=$(CLASSPATH) -Wall 

DWARFPP_PATH ?= ../../dwarf/libdwarfpp/src
LIBPATHS += -L$(DWARFPP_PATH) -L$(HOME)/opt/lib -L../../c++-fileno/lib -L../../libsrk31c++

CAKE_VERSION := 0.0$(shell hg log --template '{date|isodate}\n' $(CAKE_SRC) | head -n1 | sed 's/+.*//' | tr -cd '[:digit:]' )#$(shell if [[ -n $$( hg status $(patsubst %,cake/src/%,$(CAKE_SRC))) ]]; then echo "+local"; fi )
$(warning Generated Cake version macro $(CAKE_VERSION))

CFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"
CXXFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"

#CLASSES := 	org.antlr.runtime.Lexer \
#		org.antlr.runtime.Parser \
#		org.antlr.runtime.BaseRecognizer \
#		org.antlr.runtime.ANTLRInputStream \
#		org.antlr.runtime.CommonTokenStream \
#		org.antlr.runtime.TokenStream \
#		org.antlr.runtime.ANTLRReaderStream \
#		org.antlr.runtime.ANTLRStringStream \
#		org.antlr.runtime.CharStream \
#		org.antlr.runtime.IntStream \
#		org.antlr.runtime.ParserRuleReturnScope \
#		org.antlr.runtime.RuleReturnScope \
#		org.antlr.runtime.tree.CommonTree \
#		org.antlr.runtime.tree.BaseTree \
#		org.antlr.runtime.tree.Tree \
#		org.antlr.runtime.CommonToken \
#		org.antlr.runtime.Token \
#		org.antlr.runtime.ClassicToken 
		
.PHONY: default
default: cake

java: java/stamp
java/stamp: 
	$(MAKE) -C java

#parser: parser/stamp
parser: parser-recursive
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

$(DEPS) : .%.d : %.cpp
	g++ -MM $(CXXFLAGS) "$<"  > "$@"
	
include $(DEPS)

# new implicit rule for compiling C++
%.o: %.cpp  $(HDRS) 
	g++ -c -o "$@" ${CXXFLAGS} "$<"

# cancel builtin implicit rule
%: %.o

$(warning OBJS is $(OBJS))
cake: parser $(OBJS) $(DWARFPP_PATH)/libdwarfpp.a # dependency only
	gcj $(GCJFLAGS) $(LIBPATHS) -Wl,-R. $(OBJS) parser/cakeC{Parser,Lexer}.o $(LIBS) -lstdc++ -o "$@" 

#antlr-runtime.jar.so: antlr-runtime-3*.jar 
#	gcj -shared -o "$@" "$<"

#antlr-runtime-direct.jar.so: antlr-runtime-3*.jar 
#	gcj $(GCJFLAGS) -shared  -o "$@" "$<"

#stringtemplate.jar.so: stringtemplate-*.jar
#	gcj $(GCJFLAGS) -shared -o "$@" "$<"

JAVA_SRC := 
CLASSFILES := $(CLASSES)
include:: Makefile # our list of classes is in this file, so add a dependency
include Makerules.java.mk
		
%.jar.so: %-*.jar.so
	cp "$<" "$@"


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
