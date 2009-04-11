# FIXME: for this to build, you must copy or symlink the following JAR files into
# the directory containing this Makefile.
#
# antlr-runtime-3.1.jar
# stringtemplate-3.2.jar
# antlr-3.1.2.jar 
# junit-3.8.2.jar

OBJS := main.o cake.o java_glue/cake/SemanticError.o alias.o util.o module.o exists.o derive.o link.o supplementary.o
LIBS := -Wl,--whole-archive -ldwarf -Wl,--no-whole-archive -ldwarfpp -lfileno -lelf
INCLUDES += -Iparser -Igcjh_generated -I../../dwarf/libdwarfpp -I$(HOME)/opt/include -I../../c++-fileno/include
HDRS :=
HDRS += $(wildcard *.hpp) ../../dwarf/libdwarfpp/dwarfpp.h ../../dwarf/libdwarfpp/dwarfpp_simple.hpp
CXXFLAGS += -fPIC -g $(INCLUDES) -Wall #-D_STREAM_COMPAT
CLASSPATH := .:$(shell ( echo ./java_glue; ls -1 *.jar ) | while read line; do readlink -f "$$line"; done | tr '\n' ':' | sed 's/:+$$/:/' )
GCJFLAGS += -g -classpath $(CLASSPATH) -fPIC -Wall
LIBPATHS += -L../../dwarf/libdwarfpp -L$(HOME)/opt/lib -L../../c++-fileno/lib

.PHONY: default
default: cake

#cake.o:: treewalk_helpers.hpp cake.hpp parser.hpp util.hpp

# cancel builtin implicit rule
%.o: %.cpp

# new implicit rule for compiling C++
%.o: %.cpp  $(HDRS) parser-recursive.stamp gcjh_generated
	g++ -c -o "$@" ${CXXFLAGS} "$<"

#main.o: main.cpp 
#	g++ -c -o "$@" ${CXXFLAGS} "$<"

cake: $(OBJS) stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so
	gcj $(GCJFLAGS) $(LIBPATHS) -Wl,-R. -o "$@" $(OBJS) $(LIBS) -lstdc++ parser/cakeJava{Parser,Lexer}*.o stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so

# FIXME: to build, change the following .jar paths

#.PHONY: antlr-headers
gcjh_generated: java_glue-recursive.stamp parser-recursive.stamp
	gcjh -force -d gcjh_generated -classpath $(CLASSPATH) \
		org.antlr.runtime.Lexer \
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
		org.antlr.runtime.Token \
		cake.SemanticError
		
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

java_glue/%.o: java_glue-recursive.stamp

.PHONY: clean
clean:
	rm -rf gcjh_generated
	rm -f cake *.o *.so
	$(MAKE) -C parser clean

.PHONY: test
RUN_PREFIX := gdb --eval-command "break main" --eval-command run --args
test: cake
	cd ../examples; MALLOC_CHECK_=3 LD_LIBRARY_PATH=../src:${LD_LIBRARY_PATH} $(RUN_PREFIX) ../src/cake switch-simple.cake

.PHONY: memtest
memtest: cake
	cd ../examples; LD_LIBRARY_PATH=../src:$(LD_LIBRARY_PATH) valgrind --tool=memcheck ../src/cake switch-simple.cake
