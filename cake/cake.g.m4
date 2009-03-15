include(antlr_m4_include_file)
grammar antlr_m4_make_grammar_name(cake,antlr_m4_grammar_name_suffix);
options {
    output=AST;
    language=antlr_m4_language;
    ASTLabelType=CommonTree; // type of $statement.tree ref etc...
}
tokens { ENCLOSING; MULTIVALUE; }
/* The whole input */
toplevel:   declaration* /*{sys.stdout.write($objectExpr.tree.toStringTree() + '\n');} */
        ;

declaration		: existsDeclaration
/*				| aliasDeclaration
                | deriveDeclaration
                | supplementaryDeclaration
                | inlineDeclaration */
				;

existsDeclaration	: 'exists'^ IDENT '('! FILENAME ')'! IDENT existsBody
					;
existsBody			: '{'! claimGroup* '}'!
					;

claimGroup			: 'check'^ '{'! claim* '}'!
					| 'declare'^ '{'! claim* '}'!
                    | 'override'^ '{'! claim* '}'!
					;
                    
claim				: memberNameExpr ':' valueDescriptionExpr ';'
						-> ^( memberNameExpr ^( valueDescriptionExpr ) )
					;
                    
memberNameExpr		: '.'!? IDENT^ ( '.'^ IDENT )* 
					| '_' ;

      
/*functionDescriptionExpr	: 
						;*/

/* The following alternatives are in precedence order, highest to lowest */
//valueDescriptionExpr	: primitiveValueDescription
//						| annotatedValueDescription
//                        | pointerValueDescription
//                        | structuredValueDescription
//                        | functionValueDescription
//                        ;

/*valueDescriptionExpr 	: structuredValueDescription
						;*/

valueDescriptionExpr		: primitiveOrFunctionValueDescription
                            /*| functionValueDescription*/
                            ;

primitiveValueDescription	: unannotatedValueDescription^
                            | 'opaque'^ unannotatedValueDescription
                            | 'ignored'^ unannotatedValueDescription
                        	;
unannotatedValueDescription : /*unspecifiedValueDescription^
							|*/ simpleOrObjectOrPointerValueDescription^
                            ;


primitiveOrFunctionValueDescription	: 
	(primitiveValueDescription '->')=> 
    	primitiveValueDescription '->' primitiveOrFunctionValueDescription 
			-> ^('->' primitiveValueDescription ^( primitiveOrFunctionValueDescription ) )
	| primitiveValueDescription
	; 
							                            
structuredValueDescription	: 'object' '{' claim* '}'
								-> ^('object' claim* )
                            ;
                           
/*unspecifiedValueDescription	: '_'
							;*/

simpleOrObjectValueDescription 	: simpleValueDescription^
								| structuredValueDescription^
                                ;

simpleOrObjectOrPointerValueDescription 
	: structuredValueDescription^ ( 'ptr'^ )*
    | simpleValueDescription^ ( 'ptr'^ )*
    ;

/*simpleOrPointerValueDescription : simpleOrObjectValueDescription^ ( 'ptr'^ )*
                            	;*/

simpleValueDescription		: /* 'int'^
							|*/ IDENT^
                            | '_'
							| '('! valueDescriptionExpr^ ')'! 
							;

functionValueDescription	: 
	(functionArgumentDescriptionExpr '->')=> 
    	functionArgumentDescriptionExpr '->' functionResultDescriptionExpr
        	-> ^('->' functionArgumentDescriptionExpr functionResultDescriptionExpr )
	| valueDescriptionExpr 
							;

functionArgumentDescriptionExpr	: /*multiValueDescriptionExpr
								|*/ primitiveValueDescription
								;

functionResultDescriptionExpr	: /*multiValueDescriptionExpr
								|*/ primitiveValueDescription
								;
/*                                
multiValueDescriptionExpr	: '<' ( primitiveValueDescription^ ',' )* primitiveValueDescription ',' '>'
	-> ^( MULTIVALUE primitiveValueDescription )

	;*/

/*aliasDeclaration	:
					;
deriveDeclaration	:
					;
supplementaryDeclaration	:
							;
inlineDeclaration			:
							;*/
/* Lexer */
INT :   '0'..'9'+ ;
NEWLINE:'\r'? '\n' {antlr_m4_skip_action} ;
WS  :   (' '|'\t')+ {antlr_m4_skip_action} ;
LINECOMMENT : '/' '/'( ~ '\n' )* {antlr_m4_skip_action} ;
BLOCKCOMMENT : '/' '*' ( ~ '/' | ( ~ '*' ) '/' )* '*' '/' {antlr_m4_skip_action} ;
FILENAME : '\"' ( ~'\"'|'\\\"' )+ '\"' ;
IDENT  :   ('a'..'z'|'A'..'Z')('a'..'z'|'A'..'Z'|'0'..'9'|'_'|'-')*('a'..'z'|'A'..'Z'|'0'..'9'|'_') ;
// FIXME: permit reserved words as identifiers, somehow
