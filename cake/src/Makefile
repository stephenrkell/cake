OBJS := cake.o
INCLUDES := -Iparser -Igcjh_generated
CXXFLAGS := -fPIC -g $(INCLUDES)
JFLAGS := -fPIC

default: cake

%.o: %.cpp
	g++ -c -o "$@" ${CXXFLAGS} "$<"

main.o: main.cpp parser-recursive Makefile antlr-headers
	g++ -c -o "$@" ${CXXFLAGS} "$<"

cake: main.o $(OBJS) antlr-runtime-3.1.jar.so stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so
	gcj $(JFLAGS) -Wl,-R. -o "$@" $(OBJS) "$<" -lstdc++ parser/cakeJava{Parser,Lexer}*.o antlr-runtime-3.1.jar.so stringtemplate-3.2.jar.so antlr-3.1.2.jar.so junit-3.8.2.jar.so

.PHONY: antlr-headers
antlr-headers: 
	gcjh -d gcjh_generated -classpath .:$(HOME)/opt/lib/java/antlr-runtime-3.1.jar:$(HOME)/opt/lib/java/antlr-3.1.2.jar:$(CLASSPATH) \
		org.antlr.runtime.Lexer \
		org.antlr.runtime.Parser \
		org.antlr.runtime.BaseRecognizer \
		org.antlr.runtime.ANTLRInputStream \
		org.antlr.runtime.CommonTokenStream \
		org.antlr.runtime.ANTLRReaderStream \
		org.antlr.runtime.ANTLRStringStream \
		org.antlr.runtime.CharStream

%.jar.so: %.jar
	gcj $(JFLAGS) -shared -fPIC -Wl,-Bsymbolic "$<" -o "$@"

#java-headers:
#	gcjh -d gcjh_generated -classpath .:$(CLASSPATH) java.io.File java.io.FileInputStream

.PHONY: parser-recursive
parser-recursive:
	$(MAKE) -C parser

clean:
	rm -rf gcjh_generated
	rm -f cake *.o *.so

.PHONY: test
test: cake
	gdb --eval-command run --args ./cake ../switch-simple.cake
