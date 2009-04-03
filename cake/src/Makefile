# FIXME: for this to build, you must copy or symlink the following JAR files into
# the toplevel build directory (i.e. the one containing this Makefile).
#
# antlr-runtime-3.1.jar
# stringtemplate-3.2.jar
# antlr-3.1.2.jar 
# junit-3.8.2.jar

OBJS := cake.o
INCLUDES += -Iparser -Igcjh_generated
CXXFLAGS += -fPIC -g $(INCLUDES) -Wall
CLASSPATH := .:$(shell ls -1 *.jar | while read line; do readlink -f "$$line"; done | tr '\n' ':' | sed 's/:+$$/:/' )
GCJFLAGS += -classpath $(CLASSPATH) -fPIC -Wall

.PHONY: default
default: cake

%.o: %.cpp
	g++ -c -o "$@" ${CXXFLAGS} "$<"

main.o: main.cpp parser-recursive Makefile antlr-headers
	g++ -c -o "$@" ${CXXFLAGS} "$<"

cake: main.o $(OBJS) antlr-runtime-3.1.jar.so stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so
	gcj $(GCJFLAGS) -Wl,-R. -o "$@" $(OBJS) "$<" -lstdc++ parser/cakeJava{Parser,Lexer}*.o antlr-runtime-3.1.jar.so stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so

# FIXME: to build, change the following .jar paths

.PHONY: antlr-headers
antlr-headers: 
	gcjh -d gcjh_generated -classpath $(CLASSPATH) \
		org.antlr.runtime.Lexer \
		org.antlr.runtime.Parser \
		org.antlr.runtime.BaseRecognizer \
		org.antlr.runtime.ANTLRInputStream \
		org.antlr.runtime.CommonTokenStream \
		org.antlr.runtime.TokenStream \
		org.antlr.runtime.ANTLRReaderStream \
		org.antlr.runtime.ANTLRStringStream \
		org.antlr.runtime.CharStream \
		org.antlr.runtime.IntStream 

%.jar.so: %.jar
	gcj $(GCJFLAGS) -shared -fPIC -Wl,-Bsymbolic "$<" -o "$@"

#java-headers:
#	gcjh -d gcjh_generated -classpath .:$(CLASSPATH) java.io.File java.io.FileInputStream

.PHONY: parser-recursive
parser-recursive:
	$(MAKE) -C parser

.PHONY: clean
clean:
	rm -rf gcjh_generated
	rm -f cake *.o *.so
	$(MAKE) -C parser clean

.PHONY: test
test: cake
	gdb --eval-command run --args ./cake ../switch-simple.cake
