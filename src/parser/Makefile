CFLAGS += -I./include

GRAMMAR_BASENAME := cake

INCS := $(wildcard *.inc)

.PHONY: default
#default: java-all 
default: cakeCLexer.o cakeCParser.o stamp

stamp:
	touch stamp

#cakeCLexer.c cakeCParser.c 
#cakeJavaLexer.class cakeJavaParser.class 
#cakeJavaLexer.h cakeJavaParser.h

#JAVA_SRC := cake/cakeJavaLexer.java cake/cakeJavaParser.java cake/CakeParserHelper.java
#GCJFLAGS := -findirect-dispatch -fPIC -g -Wno-unused -Wno-dead-code --classpath=$(CLASSPATH)
#include ../Makerules.java.mk

ANTLR ?= java -classpath $(CLASSPATH) org.antlr.Tool

#cake.tokens: Makefile
## create a simple ASCII (32--127) token vocabulary for single-character tokens
#	for num in `seq 32 127`; do printf "'\%o'=$$num\n" "$$num"; done > "$@"

.PHONY: test
test: cakePyParser.py
	while read line; do \
		case "$$line" in \
			(\#*) continue ;; \
			(*) eval python cakePyParser.py $$line  || break ;; \
		esac; \
	done < cake.g.tests
	$(MAKE) test-samples
	
test-samples: cakePyParser.py samples/ephy.cake samples/mpeg2ffplay.cake samples/p2k.cake samples/xcl.cake
	for fname in samples/xcl.cake samples/ephy.cake samples/mpeg2ffplay.cake samples/p2k.cake ; do \
		python cakePyParser.py --rule=toplevel < "$$fname"; \
	done | ~/work/devel/antlr/parens-filter.sh
		
test-lexer-samples: cakePyLexer.py # samples/ephy.cake samples/xcl.cake
	for fname in samples/ephy.cake ; do \
		python cakePyLexer.py < "$$fname"; \
	done 

%Py.g: %.g.m4 $(INCS)
	( test ! -e "$@" || chmod +w "$@" ) && m4 -Dantlr_m4_include_file=antlr-m4-py.inc "$<" > "$@" && chmod ugo-w "$@"

%PyLexer.py %PyParser.py: %Py.g 
	$(ANTLR) "$<"

%C.g: %.g.m4 $(INCS)
	( test ! -e "$@" || chmod +w "$@" ) && m4 -Dantlr_m4_include_file=antlr-m4-c.inc "$<" > "$@" && chmod ugo-w "$@"
	
%CLexer.c %CParser.c: %C.g 
	$(ANTLR) "$<"
	mkdir -p include && mv $*CLexer.h $*CParser.h include/

%Cpp.g: %.g.m4 $(INCS)
	m4 -Dantlr_m4_include_file=antlr-m4-cpp.inc "$<" > "$@"
	
%CppLexer.c %CppParser.c: %Cpp.g 
	$(ANTLR) "$<"

%Java.g: %.g.m4 $(INCS)
	( test ! -e "$@" || chmod +w "$@" ) && m4 -Dantlr_m4_include_file=antlr-m4-java.inc -Dantlr_m4_java_package=$$(echo "$<" | sed 's/\.g\.m4$$//') "$<" > "$@" && chmod ugo-w "$@"

# HACK: we have to use this GRAMMAR_BASENAME thing because make has
# no way of expressing <pattern>/<the-same-pattern>blah.foo
$(GRAMMAR_BASENAME)/%JavaLexer.java $(GRAMMAR_BASENAME)/%JavaParser.java: %Java.g
	$(ANTLR) "$<" -o "$(GRAMMAR_BASENAME)"

%.o: %.c
	gcc $(CFLAGS) -o "$@" -c "$<"

#.PHONY: headers
#%.h: %.class
#	gcjh -force -d . -classpath .:$(CLASSPATH) $**.class

clean::
	rm -f *.class *.h *.o *.a *.pyc *.d *{Py,Java,Cpp,C}.g 
	rm -f *{Parser,Lexer}.c
	rm -f $(GRAMMAR_BASENAME)/*{Parser,Lexer}.java $(GRAMMAR_BASENAME)/*.tokens
	rm -f stamp
