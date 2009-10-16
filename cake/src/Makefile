# FIXME: for this to build, you must copy or symlink the following JAR files into
# the directory containing this Makefile.
#
# antlr-runtime-3.1.jar
# stringtemplate-3.2.jar
# antlr-3.1.2.jar 
# junit-3.8.2.jar

EXTRA_DEBUG_OPTS := -fno-eliminate-unused-debug-symbols -fno-eliminate-unused-debug-types

JAVA_OBJS := java_glue/cake/SemanticError.o java_glue/cake/InternalError.o java_glue/cake/TreewalkError.o java_glue/cake/CloneableTree.o
CAKE_SRC := main.cpp cppcatch.cpp javacatch.cpp cake.cpp alias.cpp util.cpp module.cpp exists.cpp derive.cpp link.cpp supplementary.cpp
OBJS := $(JAVA_OBJS) $(patsubst %.cpp,%.o,$(CAKE_SRC))
LIBS := -lsrk31c++ -Wl,--whole-archive -ldwarf -Wl,--no-whole-archive -ldwarfpp -lfileno -lelf
INCLUDES += -Iparser -Igcjh_generated -I../../dwarf/libdwarfpp -I../../libsrk31c++ -I$(HOME)/opt/include -I../../c++-fileno/include
HDRS :=
HDRS += $(wildcard *.hpp) $(wildcard ../../dwarf/libdwarfpp/*.hpp)
CXXFLAGS += $(EXTRA_DEBUG_OPTS) -fPIC -g3 $(INCLUDES) -Wall #-D_STREAM_COMPAT
CLASSPATH := .:$(shell ( echo ./java_glue; ls -1 *.jar ) | while read line; do readlink -f "$$line"; done | tr '\n' ':' | sed 's/:+$$/:/' )
GCJFLAGS += $(EXTRA_DEBUG_OPTS) -g3 -classpath $(CLASSPATH) -fPIC -Wall
DWARFPP_PATH ?= ../../dwarf/libdwarfpp
LIBPATHS += -L$(DWARFPP_PATH) -L$(HOME)/opt/lib -L../../c++-fileno/lib -L../../libsrk31c++
CAKE_VERSION := 0.0$(shell hg log --template '{date|isodate}\n' $(CAKE_SRC) | head -n1 | sed 's/+.*//' | tr -cd '[:digit:]' )#$(shell if [[ -n $$( hg status $(patsubst %,cake/src/%,$(CAKE_SRC))) ]]; then echo "+local"; fi )
$(warning Generated Cake version macro $(CAKE_VERSION))
CFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"
CXXFLAGS += -DCAKE_VERSION=\"$(CAKE_VERSION)\"

CLASSES := 	org.antlr.runtime.Lexer \
		org.antlr.runtime.Parser \
		org.antlr.runtime.BaseRecognizer \
		org.antlr.runtime.ANTLRInputStream \
		org.antlr.runtime.CommonTokenStream \
		org.antlr.runtime.TokenStream \
		org.antlr.runtime.ANTLRReaderStream \
		org.antlr.runtime.ANTLRStringStream \
		org.antlr.runtime.CharStream \
		org.antlr.runtime.IntStream \
		org.antlr.runtime.ParserRuleReturnScope \
		org.antlr.runtime.RuleReturnScope \
		org.antlr.runtime.tree.CommonTree \
		org.antlr.runtime.tree.BaseTree \
		org.antlr.runtime.tree.Tree \
		org.antlr.runtime.CommonToken \
		org.antlr.runtime.Token \
		org.antlr.runtime.ClassicToken \
		cake.SemanticError \
		cake.InternalError \
		cake.TreewalkError \
		cake.CloneableTree \
		cakeJavaParser \
		cakeJavaLexer

.PHONY: default
default: cake

java_glue/%.o: 
	$(MAKE) -C java_glue CLASSPATH=$(shell pwd)/antlr-3.1.2.jar:.:

#cake.o:: treewalk_helpers.hpp cake.hpp parser.hpp util.hpp

# cancel builtin implicit rule
%.o: %.cpp

# new implicit rule for compiling C++
%.o: %.cpp  $(HDRS) parser-recursive.stamp gcjh_generated
	g++ -c -o "$@" ${CXXFLAGS} "$<"

#main.o: main.cpp 
#	g++ -c -o "$@" ${CXXFLAGS} "$<"

cake: $(OBJS) stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so $(DWARFPP_PATH)/libdwarfpp.a
	gcj $(GCJFLAGS) $(LIBPATHS) -Wl,-R. -o "$@" $(OBJS) $(LIBS) -lstdc++ parser/cakeJava{Parser,Lexer}*.o stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so

# FIXME: to build, change the following .jar paths

#.PHONY: antlr-headers
#gcjh_generated: java_glue-recursive.stamp parser-recursive.stamp
gcjh_generated: Makefile $(wildcard java_glue/*.class) $(wildcard parser/*.class)
	gcjh -force -d gcjh_generated -classpath ./parser:$(CLASSPATH) $(CLASSES)
		
#antlr-headers.stamp: antlr-headers
#	touch antlr-headers.stamp

%.jar.so: %.jar
	gcj $(GCJFLAGS) -shared -Wl,-Bsymbolic "$<" -o "$@"

#java-headers:
#	gcjh -d gcjh_generated -classpath .:$(CLASSPATH) java.io.File java.io.FileInputStream
	
parser-recursive.stamp: parser/stamp
	touch parser-recursive.stamp

parser/stamp:
	$(MAKE) -C parser CLASSPATH=$(CLASSPATH)
	touch parser/stamp

java_glue-recursive.stamp: java_glue/stamp
	touch java_glue-recursive.stamp
	
java_glue/stamp:
	$(MAKE) -C java_glue CLASSPATH=$(CLASSPATH)
	touch java_glue/stamp

# FIXME: this rule doesn't work (doesn't do anything). Find out why!
#java_glue/%.o: java_glue-recursive.stamp

.PHONY: clean
clean:
	rm -rf gcjh_generated
	rm -f cake *.o *.so
	$(MAKE) -C parser clean

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
