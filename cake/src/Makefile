OBJS := cake.o
INCLUDES := -Iparser -Igcjh_generated
CXXFLAGS := -g $(INCLUDES)

default: cake

%.o: %.cpp
	g++ -c -o "$@" ${CXXFLAGS} "$<"

main.o: main.cpp parser-recursive Makefile antlr-headers
	g++ -c -o "$@" ${CXXFLAGS} "$<"

cake: main.o $(OBJS) java-headers
	g++ -o "$@" $(OBJS) "$<" -lstdc++ -lgcj

.PHONY: antlr-headers
antlr-headers: 
	gcjh -d gcjh_generated -classpath .:$(CLASSPATH) org.antlr.runtime.Lexer org.antlr.runtime.Parser org.antlr.runtime.BaseRecognizer

java-headers:
	gcjh -d gcjh_generated -classpath .:$(CLASSPATH) java.io.File java.io.FileInputStream

.PHONY: parser-recursive
parser-recursive:
	make -C parser

clean:
	rm -rf gcjh_generated
