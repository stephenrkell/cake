.PHONY: default
default: cakePyLexer.py cakePyParser.py

.PHONY: test
test: default
	cd examples/foobarbaz; ../../cake foo.cake

%Py.g: %.g.m4
	m4 -Dantlr_m4_include_file=antlr-m4-py.inc "$<" > "$@"

%PyLexer.py %PyParser.py: %Py.g
	antlr "$<"

%Cpp.g: %.g.m4
	m4 -Dantlr_m4_include_file=antlr-m4-cpp.inc "$<" > "$@"
	
%CppLexer.cc %CppParser.cc: %Cpp.g
	antlr "$<"
