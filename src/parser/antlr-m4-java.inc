include(antlr-m4.inc)
define(antlr_m4_grammar_name_suffix,Java)
define(antlr_m4_language,Java)
define(antlr_m4_skip_action,skip();)
define(antlr_m4_newline_action,$channel=HIDDEN; $line = $line + 1;)
define(antlr_m4_header,package antlr_m4_java_package;)
define(antlr_m4_extra_options,ASTLabelType=CommonTree; // type of $statement.tree ref etc...)