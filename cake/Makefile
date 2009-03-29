.PHONY: default
default: cakePyLexer.py cakePyParser.py cakeCppLexer.c cakeCppParser.c

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
