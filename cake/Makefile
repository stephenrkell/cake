.PHONY: default
default: cakeLexer.py cakeParser.py

.PHONY: test
test: default
	./cake test-input.cake

#%.pyc: %.py

%Lexer.py %Parser.py: %.g
	antlr "$<"
