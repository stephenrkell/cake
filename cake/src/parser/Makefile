JFLAGS := -fPIC -g

.PHONY: default
default: cakeJavaLexer.java cakeJavaParser.java cakeJavaLexer.class cakeJavaParser.class cakeJavaLexer.o cakeJavaParser.o cakeJavaLexer.h cakeJavaParser.h
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
	gcj -MD -C -Wno-unused "$<"

%.o: %.class
	gcj $(JFLAGS) -MD -Wno-unused -c -o "$@" "$<"
# HACK: also make nested classes' object files, because we can't enumerate these statically for Makefile purposes
	for classfile in "$*"\$$*.class; do \
		if [[ $$classfile != "$*"\$$"*".class ]]; then \
			gcj -o $$( echo "$$classfile" | sed 's/class$$/o/') -MD -Wno-unused -c "$$classfile"; \
		fi; \
	done

.PHONY: headers
%.h: %.class
	gcjh -classpath .:$(CLASSPATH) $**.class

clean:
	rm -f *.class *.h *.o *.pyc *.d *{Py,Java,Cpp}.g *{Parser,Lexer}.*
