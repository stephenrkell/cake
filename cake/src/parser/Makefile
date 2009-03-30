.PHONY: default
default: cakeJavaLexer.java cakeJavaParser.java cakeJavaLexer.o cakeJavaParser.o cakeJavaLexer.h cakeJavaParser.h
# cakePyLexer.py cakePyParser.py 

.PHONY: test
test: default
	while read line; do \
		eval python cakePyParser.py $$line || break; \
	done < cake.g.tests

%Py.g: %.g.m4
	m4 -Dantlr_m4_include_file=antlr-m4-py.inc "$<" > "$@"

%PyLexer.py %PyParser.py: %Py.g
	antlr "$<"

%Cpp.g: %.g.m4
	m4 -Dantlr_m4_include_file=antlr-m4-cpp.inc "$<" > "$@"
	
%CppLexer.c %CppParser.c: %Cpp.g
	antlr "$<"

%Java.g: %.g.m4
	m4 -Dantlr_m4_include_file=antlr-m4-java.inc "$<" > "$@"
	
%JavaLexer.java %JavaParser.java: %Java.g
	antlr "$<"

%.class: %.java
	gcj -C -Wno-unused "$<"

%.o: %.class
	gcj -Wno-unused -c -o "$@" "$<"

.PHONY: headers
%.h: %.class
	gcjh -classpath .:$(CLASSPATH) $**.class

clean:
	rm -f *.class *.h *.o *.pyc *{Py,Java,Cpp}.g *{Parser,Lexer}.*
