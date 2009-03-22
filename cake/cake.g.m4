include(antlr_m4_include_file)
grammar antlr_m4_make_grammar_name(cake,antlr_m4_grammar_name_suffix);
options {
    output=AST;
    language=antlr_m4_language;
    ASTLabelType=CommonTree; // type of $statement.tree ref etc...
}
tokens { ENCLOSING; MULTIVALUE; IDENT_LIST; SUPPLEMENTARY; INVOCATION; CORRESP; STUB; EVENT_PATTERN; VALUE_PATTERN; 
EVENT_CONTEXT; SET_CONST; CONDITIONAL; }
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
                    
memberNameExpr		: '.'!? IDENT ( '.'^ IDENT )* 
					| '_'^ 
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

constantOrVoidValueDescription	:	constantValueDescription^
								|	'void'^
                                ;

constantValueDescription	: STRING_LIT^
                            | 'null'^
                            | constantSetExpression
							| constantIntegerArithmeticExpression
                            ;
                            
constantSetExpression	: '{' ( IDENT ( ',' IDENT* )* )? '}' -> ^( SET_CONST IDENT* )
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
            
constantIntegerArithmeticExpression	: constantShiftingExpression^
									;

primitiveIntegerArithmeticExpression	: INT^
										| '('! constantIntegerArithmeticExpression^ ')'!
                                        ;
                                        
constantShiftingExpression	: primitiveIntegerArithmeticExpression ( ( '<<'^ | '>>'^ ) primitiveIntegerArithmeticExpression )* 
					;
                           
functionValueDescription	: 
	(functionArgumentDescriptionExpr '->')=> 
    	functionArgumentDescriptionExpr '->' functionResultDescriptionExpr
        	-> ^('->' functionArgumentDescriptionExpr functionResultDescriptionExpr )
	| valueDescriptionExpr^ 
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
                                
eventCorrespondence	:	(eventPattern '-->')=> 	eventPattern	'-->'^ eventPatternRewriteExpr ';'!
					|					eventPatternRewriteExpr '<--'^ eventPattern ';'!
                    /*|	eventPattern	'<-->' eventPattern*/
					;

eventContext	: ( '(' ( stackFramePattern '::' )+ ')' )? -> ^( EVENT_CONTEXT stackFramePattern* )
				;
                
stackFramePattern 	: IDENT^
					;

eventPattern	:	atomicEventPattern
				; /* TODO: add composite (sequence) event patterns */
           
atomicEventPattern	: eventContext memberNameExpr '(' ( ( annotatedValuePattern ( ',' annotatedValuePattern )* ) | '...' )? ')'
						-> ^( EVENT_PATTERN eventContext memberNameExpr annotatedValuePattern* )
					;

annotatedValuePattern 	: valuePattern valuePatternAnnotation? -> ^( VALUE_PATTERN valuePattern valuePatternAnnotation? )
						;

valuePatternAnnotation	: 'as'^ memberNameExpr 
						| '{'! 'names'^ memberNameExpr '}'!
						;

valuePattern		: memberNameExpr^ /* matches a named constant value -- also matches '_' */
					| METAVAR^ /* matches any value, and names it */
					| constantValueDescription^ /* matches that constant */
                    ;
                
eventPatternRewriteExpr	: eventPattern^ /* shorthand for a trivial stub */
						| stubDescription^
                        ;
                        
stubDescription		: '(' stubStatementBody ( ';' stubStatementBody )* ')' -> ^( STUB stubStatementBody* )
					;
          
stubStatementBody	:	assignment^
					|	emitStatement^
                	| 	stubLangExpression^
                    |	'skip'^
                	;
                
stubLangExpression	/*: constantOrVoidValueDescription^
					| ifThenElseExpression^
                    | booleanArithmeticExpression^
					| invocation^
                    | runtimeValueIdentExpr^*/
					/*| stubDescription^ /* sequence */
                    : conditionalExpression^ /* lowest precedence operator */
                    ;
            
stubPrimitiveExpression	: STRING_LIT^
						| INT^
                        | 'true'^
                        | 'false'^
						| METAVAR^
                        | memberNameExpr^
                        | '('! stubLangExpression^ ')'!
                        ;

memberSelectionExpression	: stubPrimitiveExpression^ ('.'^ stubPrimitiveExpression )*
							; /* left-associative 
                               * Note this subsumes memberNameExpr, so we don't need it. */

functionInvocationExpression	: memberSelectionExpression '(' ( stubLangExpression (',' stubLangExpression  )* )? ')'
									-> ^( INVOCATION memberSelectionExpression stubLangExpression* )
    							;
         
unaryOperatorExpression	: ('~'^|'!'^|'-'^|'+'^|'&'^|'*'^)* functionInvocationExpression
						;
                        
multiplicativeOperatorExpression	: unaryOperatorExpression^ ( ( '*' | '/' | '%' )^ unaryOperatorExpression )*
									;
                                    
additiveOperatorExpression 	: multiplicativeOperatorExpression^ ( ( '+' | '-' )^ multiplicativeOperatorExpression )*
							;
                            
shiftingExpression	: additiveOperatorExpression^ ( ( '<<' | '>>' )^  additiveOperatorExpression )*
					;
                    
magnitudeComparisonExpression 	: shiftingExpression^ ( ( '<' | '>' | '<=' | '>=' )^ shiftingExpression )?
								;
                                
equalityComparisonExpression	: magnitudeComparisonExpression^ ( ( '==' | '!=' )^ equalityComparisonExpression )*
								;
                                
bitwiseAndExpression	: magnitudeComparisonExpression^ ( '&'^ magnitudeComparisonExpression )*
						;
                        
bitwiseXorExpression	: bitwiseAndExpression^ ( '^'^ bitwiseAndExpression )*
						;
                        
bitwiseOrExpression	: bitwiseXorExpression^ ( '|'^ bitwiseXorExpression )*
					;
                    
logicalAndExpression 	: bitwiseOrExpression^ ( '&&'^ bitwiseOrExpression )*
						;
                        
logicalOrExpression	: logicalAndExpression^ ( '||'^ logicalAndExpression )*
					;
                    
conditionalExpression	: logicalOrExpression^
						| 'if' cond=conditionalExpression 'then' caseTrue=conditionalExpression 'else' caseFalse=conditionalExpression
                        	-> ^( CONDITIONAL cond caseTrue caseFalse )
                        ;
                        
/* FIXME: now add actionExpression and sequenceExpression, then get rid of the separate Statement thing
 * ... and check that ( ... ) notation works like expected! as in, it denotes a separate stub?
 * what does a sub-stub actually denote?
 * well, a delayed computation -- we *don't* want to get ( ... ) stubs evaluated before
 * their containing expressions, but we do want regular brackets to mean that sometimes.... */

/*                    
booleanArithmeticExpression	: booleanNegationExpression^ ( ( '&&' | '||' )^ booleanNegationExpression )*
							;

booleanNegationExpression	:	'!'? runtimeValueIdentExpr -> ^( '!'? runtimeValueIdentExpr )
							;

invocation	: memberNameExpr '(' ( stubLangExpression ( ',' stubLangExpression ',' )* )? ')' -> ^( INVOCATION memberNameExpr stubLangExpression* )
			;
*/                                     
/* Unlike the simpler memberNameExpr, the following can include (and dereference)
 * values bound at runtime during stub execution, for example in a 'let' clause. */                    
/*
runtimeValueIdentExpr	: memberNameExpr^
						| '*'^ runtimeValueIdentExpr
                        | ( '(' runtimeValueIdentExpr ')' '.' )=> 
                        	'(' runtimeValueIdentExpr ')' memberNameExpr
                            -> ^( memberNameExpr runtimeValueIdentExpr )*/
                            /* Only take the above option if we have a dot following
                             * the brackets, to avoid syntactic weirdness where the
                             * memberNameExpr isn't separated by a dot*/
                    	/*;*/

/*                    
ifThenElseExpression	:	'if'^ stubLangExpression 'then'! stubLangExpression 'else'! stubLangExpression
						;  
*/
assignment 	: 'let'^ IDENT '='! stubLangExpression
			;
            
emitStatement	:	'emit'^ stubLangExpression
				;
            
valueCorrespondenceBlock	: 'values'^ '{'! valueCorrespondence* '}'!
							;
                            
valueCorrespondence	: valueCorrespondenceBase^ ( ';'! | valueCorrespondenceRefinement )
					;
                        
valueCorrespondenceBase	: 
	memberNameExpr (
    	leftToRightCorrespondenceOperator |
        rightToLeftCorrespondenceOperator |
        bidirectionalCorrespondenceOperator)^ correspondenceOperatorModifier? memberNameExpr        
 	|	constantValueDescription leftToRightCorrespondenceOperator^ correspondenceOperatorModifier? memberNameExpr
    | 	'void' rightToLeftCorrespondenceOperator^ correspondenceOperatorModifier? memberNameExpr
    |	memberNameExpr rightToLeftCorrespondenceOperator^ correspondenceOperatorModifier? constantValueDescription
 	|	stubDescription leftToRightCorrespondenceOperator^ correspondenceOperatorModifier? memberNameExpr
    |	memberNameExpr rightToLeftCorrespondenceOperator^ correspondenceOperatorModifier? stubDescription
    ;

/*valuePattern ('<-->'|'-->'|'<--')^ valuePattern
						;*/
                            
bidirectionalCorrespondenceOperator	:	'<-->'^
									;
                            
leftToRightCorrespondenceOperator	: '-->'^
                            		| '-->?'^
                                    ;
                                    
rightToLeftCorrespondenceOperator	: '<--'^
									| '<--?'^
                                    ;
                                    
correspondenceOperatorModifier	:	'['  ']'
								;
                    
valueCorrespondenceRefinement	:	'{'! valueCorrespondence* '}'!
								;
                                
/*valueCorrespondenceRefinementElement	: */
                    

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
METAVAR	: '@'('a'..'z'|'A'..'Z')('a'..'z'|'A'..'Z'|'0'..'9'|'_')* ;
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
