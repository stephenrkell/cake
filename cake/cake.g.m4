include(antlr_m4_include_file)
grammar antlr_m4_make_grammar_name(cake,antlr_m4_grammar_name_suffix);
options {
    output=AST;
    language=antlr_m4_language;
    ASTLabelType=CommonTree; // type of $statement.tree ref etc...
}
tokens { ENCLOSING; MULTIVALUE; IDENT_LIST; SUPPLEMENTARY; INVOCATION; CORRESP; STUB; }
/* The whole input */
toplevel:   declaration* /*{sys.stdout.write($objectExpr.tree.toStringTree() + '\n');} */
        ;

declaration		: existsDeclaration
				| aliasDeclaration
                | supplementaryDeclaration
                | inlineDeclaration
                | deriveDeclaration 
				;

aliasDeclaration	: 'alias'^ aliasDescription IDENT ';'!
					;
                    
aliasDescription	: IDENT^
					| 'any'^ identList
                    ;
                    
identList			: '[' IDENT ( ',' IDENT )*  ','? ']' -> ^( IDENT_LIST IDENT* )
					;

supplementaryDeclaration 	: IDENT '{' claimGroup* '}' -> ^( SUPPLEMENTARY IDENT claimGroup* )
							;

objectConstructor	: IDENT^ ( '('! STRING_LIT ')'! )?
							;

objectSpec			: objectConstructor^ ( IDENT | 'deriving' objectConstructor IDENT )
					;

existsDeclaration	: 'exists'^ objectSpec existsBody
					;
existsBody			: '{'! ( claimGroup | globalRewrite )* '}'!
					| ';'!
					;
                    
globalRewrite		: 'static'? valueDescriptionExpr '-->'^ valueDescriptionExpr ';'!
					;

claimGroup			: 'check'^ '{'! claim* '}'!
					| 'declare'^ '{'! claim* '}'!
                    | 'override'^ '{'! claim* '}'!
					;
                    
claim				: memberNameExpr ':' valueDescriptionExpr ';'
						-> ^( memberNameExpr ^( valueDescriptionExpr ) )
					;
                    
memberNameExpr		: '.'!? IDENT^ ( '.'^ IDENT )* 
					| '_' 
                    ;

      
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
							| 'const'^ constantValueDescription
                            | 'opaque'^ unannotatedValueDescription
                            | 'ignored'^ unannotatedValueDescription
                        	;
unannotatedValueDescription : /*unspecifiedValueDescription^
							|*/ simpleOrObjectOrPointerValueDescription^
                            ;

constantValueDescription	: STRING_LIT^
							| 'void'^
							| constantIntegerArithmeticExpression
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
                            
simpleOrObjectOrPointerValueDescription : structuredValueDescription^ ( 'ptr'^ )*
									    | simpleValueDescription^ ( 'ptr'^ )*
                                        | enumValueDescription^ ( 'ptr'^ )*
									    ;

simpleValueDescription		: dwarfBaseTypeDescription^
                            | '_'
							| '('! valueDescriptionExpr^ ')'! 
							;

byteSizeParameter			: '<'! INT '>'!
							;
                
dwarfBaseTypeDescription	: IDENT^ ( byteSizeParameter ( '{'! ( IDENT '=' ( IDENT | INT ) ';' )* '}'! )? )?
							;

enumValueDescription	: 'enum'^ ( ( ( IDENT | '_' ) byteSizeParameter? enumDefinition? ) | ( byteSizeParameter? enumDefinition ) )
						;
                           
enumDefinition	: '{'! enumElement* '}'!
				;

enumElement : 'enumerator'^ IDENT '==' constantIntegerArithmeticExpression ';'!
			;
            
constantIntegerArithmeticExpression	: shiftingExpression^
									;

primitiveIntegerArithmeticExpression	: INT^
										| '('! constantIntegerArithmeticExpression^ ')'!
                                        ;
                                        
shiftingExpression	: primitiveIntegerArithmeticExpression ( ( '<<'^ | '>>'^ ) primitiveIntegerArithmeticExpression )* 
					;
                           


functionValueDescription	: 
	(functionArgumentDescriptionExpr '->')=> 
    	functionArgumentDescriptionExpr '->' functionResultDescriptionExpr
        	-> ^('->' functionArgumentDescriptionExpr functionResultDescriptionExpr )
	| valueDescriptionExpr 
							;

functionArgumentDescriptionExpr	: multiValueDescriptionExpr^
								| primitiveValueDescription^
								;

functionResultDescriptionExpr	: multiValueDescriptionExpr^
								| primitiveValueDescription^
								;
                               
multiValueDescriptionExpr	: '<' primitiveValueDescription (',' primitiveValueDescription )*  '>'
	-> ^( MULTIVALUE primitiveValueDescription )
	;

deriveDeclaration	: 'derive'^ objectConstructor IDENT '=' derivedObjectExpression
					;
                    
derivedObjectExpression	: IDENT^ '('! derivedObjectExpression ')'!
						| 'link'^ identList linkRefinement
						;

linkRefinement	: '{'! pairwiseCorrespondenceBlock^ '}'!
				| ;
                
pairwiseCorrespondenceBlock	:	IDENT '<-->'^ IDENT pairwiseCorrespondenceBody
							;
                            
pairwiseCorrespondenceBody	: '{' pairwiseCorrespondenceElement* '}' -> ^( CORRESP pairwiseCorrespondenceElement* )
							;
                            
pairwiseCorrespondenceElement	:	eventCorrespondence^
								|	valueCorrespondenceBlock^
                                ;
                                
eventCorrespondence	:	eventPattern	'-->'^ eventPatternRewriteExpr ';'!
					|	eventPatternRewriteExpr '<--'^ eventPattern ';'!
                    /*|	eventPattern	'<-->' eventPattern*/
					;
                    
eventPattern	:	memberNameExpr^ '('! ( identOrAnonymous ( ',' identOrAnonymous )* )? ')'!
				;

identOrAnonymous	:	IDENT^ 
					| 	'_'^
                    ;
                
eventPatternRewriteExpr	: eventPattern^ /* shorthand for a trivial stub */
						| stubDescription^
                        ;
                        
stubDescription		: '(' stubStatementBody ( ';' stubStatementBody )* ')' -> ^( STUB stubStatementBody* )
					;
          
stubStatementBody	:	assignment^
                	| 	stubLangExpression^
                    |	'skip'^
                	;
                
stubLangExpression	: constantValueDescription^
					| ifThenElseExpression^
					| invocation^
                    | IDENT^
					/*| stubDescription^ /* sequence */
                    ;
                    
ifThenElseExpression	:	'if'^ stubLangExpression 'then'! stubLangExpression 'else'! stubLangExpression
						;  

assignment 	: 'let'^ IDENT '='! stubLangExpression
			;
            
invocation	: memberNameExpr '(' ( stubLangExpression ( ',' stubLangExpression ',' )* )? ')' -> ^( INVOCATION memberNameExpr stubLangExpression* )
			;

valueCorrespondenceBlock	: 'values'^ '{'! valueCorrespondence* '}'!
							;
                            
valueCorrespondence	:	IDENT '<-->'^ IDENT ';'!
					;

inlineDeclaration			:	'inline'^ objectSpec wellNestedTokenBlock
							;
                            
wellNestedTokenBlock : '{' ( INT | STRING_LIT | IDENT | 'alias' | 'any' | ';' | '[' | ',' | ']' | 'exists' | 'check' | 'declare' | 'override' | ':' | '.' | '_' | '->' | 'object' | 'ptr' | '<' | '>' | '=' | 'enum' | 'enumerator' | '==' | '<<' | '>>' | wellNestedTokenBlock | '<-->' | '<--' | '-->' | '#' | '::' | '?' | '+' | '-' | '/' | '|' | '(' | ')' | 'let' )* '}';

/* Lexer */
INT :   '0'..'9'+ ;
NEWLINE:'\r'? '\n' {antlr_m4_skip_action} ;
WS  :   (' '|'\t')+ {antlr_m4_skip_action} ;
LINECOMMENT : '/' '/'( ~ '\n' )* {antlr_m4_skip_action} ;
BLOCKCOMMENT : '/' '*' ( ~ '/' | ( ~ '*' ) '/' )* '*' '/' {antlr_m4_skip_action} ;
STRING_LIT : '\"' ( ~'\"'|'\\\"' )+ '\"' ;
IDENT  :   ('a'..'z'|'A'..'Z'|'_''a'..'z'|'_''A'..'Z'|'_''0'..'9') /* begin with a-zA-Z or non-terminal '_' */
(
	('a'..'z'|'A'..'Z'|'0'..'9'|'_'|'-'|'.'/*'0'..'9'*/)*('a'..'z'|'A'..'Z'|'0'..'9'|'_')
   /*|('.''0'..'9') /* ending with dot-digit is okay */
)? ;
/* The ident rule is a bit different from the conventional -- idents must be at
 * least two characters, and may embed '-' and '.' characters (not at the start or end). 
 * The first of these quirks reduces ambiguity, since '_' is given a unique and special
 * meaning. On the other hand, symbols which are only one character will cause problems.
 * The second quirk is really odd, but I'm running with it so that we can use library names
 * in a natural fashion, e.g. glib-2.0 and so on. Note that the ambiguity is less than
 * you think, in the common case that the dots fall in between digits, since a digit
 * can't begin an ident anyway.
 * FIXME: at the moment, to support the allow-digits-after-dots rule, 
 * we require extra spaces in a name.name.name expression. Fix ANTLR's lexing 
 * behaviour so that we don't need this (i.e. that blah.blah.blah works as expected). */
// FIXME: permit reserved words as identifiers, somehow
