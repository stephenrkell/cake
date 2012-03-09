.PHONY: default
#default: java-all 
default: cakeCLexer.o cakeCParser.o test_parser stamp

ANTLR_M4_PATH ?= ../../../antlr-m4
M4PATH := $(ANTLR_M4_PATH):$(DWARFIDL_PATH)/src/parser:$(M4_PATH)
export M4PATH
GRAMMAR_OPTIONS += backtrack=true output=AST
GRAMMAR_BASENAME := cake
include $(ANTLR_M4_PATH)/Makerules

stamp:
	touch stamp

test:: test-samples

test-samples: cakePyParser.py samples/ephy.cake samples/mpeg2ffplay.cake samples/p2k.cake samples/xcl.cake
	for fname in samples/xcl.cake samples/ephy.cake samples/mpeg2ffplay.cake samples/p2k.cake ; do \
		python cakePyParser.py --rule=toplevel < "$$fname"; \
	done | $(ANTLR_M4_PATH)/parens-filter.sh
		
test-lexer-samples: cakePyLexer.py # samples/ephy.cake samples/xcl.cake
	for fname in samples/ephy.cake ; do \
		python cakePyLexer.py < "$$fname"; \
	done 

clean::
	rm -f stamp
