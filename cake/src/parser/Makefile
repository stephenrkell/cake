CFLAGS ?=
CFLAGS += -I$(HOME)/opt/include

GCJFLAGS := -fPIC -g

INCS := $(wildcard *.inc)

CLASSPATH ?= .:

ANTLR ?= java -classpath $(CLASSPATH) org.antlr.Tool

.PHONY: default
default: cakeCLexer.c cakeCParser.c cakeJavaLexer.java cakeJavaParser.java cakeJavaLexer.class cakeJavaParser.class cakeJavaLexer.o cakeJavaParser.o cakeJavaLexer.h cakeJavaParser.h
	touch stamp
# cakePyLexer.py cakePyParser.py 

.PHONY: test
test: cakePyParser.py
	while read line; do \
		eval python cakePyParser.py $$line || break; \
	done < cake.g.tests

%Py.g: %.g.m4 $(INCS)
	m4 -Dantlr_m4_include_file=antlr-m4-py.inc "$<" > "$@"

%PyLexer.py %PyParser.py: %Py.g
	$(ANTLR) "$<"

%C.g: %.g.m4 $(INCS)
	m4 -Dantlr_m4_include_file=antlr-m4-c.inc "$<" > "$@"
	
%CLexer.c %CParser.c: %C.g
	$(ANTLR) "$<"

%Cpp.g: %.g.m4 $(INCS)
	m4 -Dantlr_m4_include_file=antlr-m4-cpp.inc "$<" > "$@"
	
%CppLexer.c %CppParser.c: %Cpp.g
	$(ANTLR) "$<"

%Java.g: %.g.m4 $(INCS)
	m4 -Dantlr_m4_include_file=antlr-m4-java.inc "$<" > "$@"
	
%JavaLexer.java %JavaParser.java: %Java.g
	$(ANTLR) "$<"

%.class: %.java
	gcj -MD -C -Wno-unused "$<"

%.o: %.c
	gcc -o "$@" -c $(CFLAGS) "$<"

%.o: %.class
	gcj $(GCJFLAGS) -MD -Wno-unused -c -o "$@" "$<"
# HACK: also make nested classes' object files, because we can't enumerate these statically for Makefile purposes
	for classfile in "$*"\$$*.class; do \
		if [[ $$classfile != "$*"\$$"*".class ]]; then \
			gcj $(GCJFLAGS) -o $$( echo "$$classfile" | sed 's/class$$/o/') -MD -Wno-unused -c "$$classfile"; \
		fi; \
	done

.PHONY: headers
%.h: %.class
	gcjh -force -d . -classpath .:$(CLASSPATH) $**.class

clean:
	rm -f *.class *.h *.o *.pyc *.d *{Py,Java,Cpp,C}.g *{Parser,Lexer}.*
