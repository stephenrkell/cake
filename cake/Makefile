.PHONY: default
default: cakeLexer.py cakeParser.py

.PHONY: test
test: default
	cd examples/foobarbaz; ../../cake foo.cake

#%.pyc: %.py

%Lexer.py %Parser.py: %.g
	antlr "$<"
