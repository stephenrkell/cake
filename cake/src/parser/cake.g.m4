include(antlr_m4_include_file)
grammar antlr_m4_make_grammar_name(cake,antlr_m4_grammar_name_suffix);
options {
    output=AST;
    language=antlr_m4_language;
    antlr_m4_extra_options
}

tokens { ENCLOSING; MULTIVALUE; IDENT_LIST; SUPPLEMENTARY; INVOCATION; CORRESP; STUB; EVENT_PATTERN; 
VALUE_PATTERN; EVENT_CONTEXT; SET_CONST; CONDITIONAL; TOPLEVEL; OBJECT_CONSTRUCTOR; OBJECT_SPEC_DIRECT; 
OBJECT_SPEC_DERIVING; EXISTS_BODY; DEFINITE_MEMBER_NAME; MEMBERSHIP_CLAIM; VALUE_DESCRIPTION; DWARF_BASE_TYPE; 
DWARF_BASE_TYPE_ATTRIBUTE; DWARF_BASE_TYPE_ATTRIBUTE_LIST; REMAINING_MEMBERS; ANY_VALUE; }


@header {
antlr_m4_header
}
@lexer::header {
antlr_m4_header
}

/* The whole input */
toplevel:   declaration* //-> ^( TOPLEVEL<ToplevelNode> declaration* )
			/*{sys.stdout.write($objectExpr.tree.toStringTree() + '\n');} */
        ;

declaration		: existsDeclaration
				| aliasDeclaration
                | supplementaryDeclaration
                | inlineDeclaration
                | deriveDeclaration 
				;

/* Because ANTLR is *stupid*, I have taken care not to include any literals that
 * would make their way into the AST. Since these are assigned an unpredictable
 * token type, they are not testable-for at runtime. This is so annoying. */

aliasDeclaration	: KEYWORD_ALIAS^ aliasDescription IDENT ';'!
					;
                    
aliasDescription	: IDENT^
					| KEYWORD_ANY^ identList
                    ;
                    
identList			: '[' IDENT ( ',' IDENT )*  ','? ']' -> ^( IDENT_LIST IDENT* )
					;

supplementaryDeclaration 	: IDENT '{' claimGroup* '}' -> ^( SUPPLEMENTARY IDENT claimGroup* )
							;

objectConstructor	: IDENT ( '(' STRING_LIT ')' )?
						-> ^( OBJECT_CONSTRUCTOR IDENT STRING_LIT? )
					;

objectSpec			: (objectConstructor IDENT)=> objectConstructor IDENT 
						-> ^( OBJECT_SPEC_DIRECT objectConstructor IDENT )
					| (objectConstructor KEYWORD_DERIVING)=> objectConstructor KEYWORD_DERIVING objectConstructor IDENT
						-> ^( OBJECT_SPEC_DERIVING objectConstructor objectConstructor IDENT )
					;

existsDeclaration	: KEYWORD_EXISTS^ objectSpec existsBody
					;
existsBody			: '{' ( claimGroup | globalRewrite )* '}' -> ^( EXISTS_BODY claimGroup* globalRewrite* )
					| ';' -> ^( EXISTS_BODY )
					;
                    
globalRewrite		: KEYWORD_STATIC? valueDescriptionExpr LR_DOUBLE_ARROW^ valueDescriptionExpr ';'!
					;

claimGroup			: KEYWORD_CHECK^ '{'! claim* '}'!
					| KEYWORD_DECLARE^ '{'! claim* '}'!
                    | KEYWORD_OVERRIDE^ '{'! claim* '}'!
					;

claim				: membershipClaim
					;
                    
membershipClaim		:  memberNameExpr ':' KEYWORD_CLASS_OF valueDescriptionExpr ';'
						-> ^( MEMBERSHIP_CLAIM memberNameExpr ^( KEYWORD_CLASS_OF  valueDescriptionExpr ) )
					|  memberNameExpr ':' valueDescriptionExpr ';'
						-> ^( MEMBERSHIP_CLAIM memberNameExpr valueDescriptionExpr )
                    | ELLIPSIS ':' valueDescriptionExpr ';'
                    	-> ^( MEMBERSHIP_CLAIM REMAINING_MEMBERS valueDescriptionExpr )
					;
                    
memberNameExpr		: '.'? IDENT ( '.' IDENT )* -> ^( DEFINITE_MEMBER_NAME IDENT* )
					| INDEFINITE_MEMBER_NAME^ 
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
	-> ^( VALUE_DESCRIPTION primitiveOrFunctionValueDescription)
                            /*| functionValueDescription*/
                            ;

primitiveValueDescription	: unannotatedValueDescription^
							| KEYWORD_CONST^ constantValueDescription
                            | KEYWORD_OPAQUE^ unannotatedValueDescription
                            | KEYWORD_IGNORED^ unannotatedValueDescription
                        	;
unannotatedValueDescription : /*unspecifiedValueDescription^
							|*/ simpleOrObjectOrPointerValueDescription^
                            ;

constantOrVoidValueDescription	:	constantValueDescription^
								|	KEYWORD_VOID^
                                ;

constantValueDescription	: STRING_LIT^
                            | KEYWORD_NULL^
                            | constantSetExpression
							| constantIntegerArithmeticExpression
                            ;
                            
constantSetExpression	: '{' ( IDENT ( ',' IDENT* )* )? '}' -> ^( SET_CONST IDENT* )
						;

primitiveOrFunctionValueDescription	: 
	(primitiveValueDescription FUNCTION_ARROW)=> 
    	primitiveValueDescription FUNCTION_ARROW primitiveOrFunctionValueDescription 
			-> ^(FUNCTION_ARROW primitiveValueDescription ^( primitiveOrFunctionValueDescription ) )
	| primitiveValueDescription
	; 
							                            
structuredValueDescription	: KEYWORD_OBJECT '{' membershipClaim* '}'
								-> ^(KEYWORD_OBJECT membershipClaim* )
                            ;
                            
simpleOrObjectOrPointerValueDescription : structuredValueDescription^ ( KEYWORD_PTR^ )*
									    | simpleValueDescription^ ( KEYWORD_PTR^ )*
                                        | enumValueDescription^ ( KEYWORD_PTR^ )*
									    ;

simpleValueDescription		: namedDwarfTypeDescription^
                            | INDEFINITE_MEMBER_NAME -> ANY_VALUE
							| '('! valueDescriptionExpr^ ')'! 
							;

byteSizeParameter			: '<'! INT '>'!
							;

namedDwarfTypeDescription	: KEYWORD_BASE^ dwarfBaseTypeDescription
							| IDENT
                            ;
                
dwarfBaseTypeDescription	: encoding=IDENT byteSizeParameter? dwarfBaseTypeAttributeList
							-> ^( DWARF_BASE_TYPE $encoding dwarfBaseTypeAttributeList byteSizeParameter?  )
							;

dwarfBaseTypeAttributeList : ( '{' ( dwarfBaseTypeAttributeDefinition )* '}' )?
								-> ^( DWARF_BASE_TYPE_ATTRIBUTE_LIST dwarfBaseTypeAttributeDefinition* )
                              ;

dwarfBaseTypeAttributeDefinition 	: attr=IDENT '=' ( value=IDENT | value=INT ) ';'
									-> ^( DWARF_BASE_TYPE_ATTRIBUTE $attr $value )
									;

/*enumValueDescription	: KEYWORD_ENUM^ ( ( ( IDENT | '_' ) byteSizeParameter? enumDefinition? ) | ( byteSizeParameter? enumDefinition ) )*/
enumValueDescription	: KEYWORD_ENUM^ dwarfBaseTypeDescription enumDefinition 
						;
                           
enumDefinition	: '{'! enumElement* '}'!
				;

enumElement : KEYWORD_ENUMERATOR^ IDENT EQ constantIntegerArithmeticExpression ';'!
			;
            
constantIntegerArithmeticExpression	: constantShiftingExpression^
									;

primitiveIntegerArithmeticExpression	: INT^
										| '('! constantIntegerArithmeticExpression^ ')'!
                                        ;
                                        
constantShiftingExpression	: primitiveIntegerArithmeticExpression ( ( SHIFT_LEFT^ | SHIFT_RIGHT^ ) primitiveIntegerArithmeticExpression )* 
					;
                           
functionValueDescription	: 
	(functionArgumentDescriptionExpr FUNCTION_ARROW)=> 
    	functionArgumentDescriptionExpr FUNCTION_ARROW functionResultDescriptionExpr
        	-> ^(FUNCTION_ARROW functionArgumentDescriptionExpr functionResultDescriptionExpr )
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

deriveDeclaration	: KEYWORD_DERIVE^ objectConstructor IDENT '=' derivedObjectExpression
					;
                    
derivedObjectExpression	: IDENT^ '('! derivedObjectExpression ')'!
						| KEYWORD_LINK^ identList linkRefinement
						;

linkRefinement	: '{'! pairwiseCorrespondenceBlock^ '}'!
				| ;
                
pairwiseCorrespondenceBlock	:	IDENT ( BI_DOUBLE_ARROW^ | RL_DOUBLE_ARROW^ | LR_DOUBLE_ARROW^ ) IDENT pairwiseCorrespondenceBody
							;
                            
pairwiseCorrespondenceBody	: '{' pairwiseCorrespondenceElement* '}' -> ^( CORRESP pairwiseCorrespondenceElement* )
							;
                            
pairwiseCorrespondenceElement	:	eventCorrespondence^
								|	valueCorrespondenceBlock^
                                ;
                                
eventCorrespondence	:	(eventPattern LR_DOUBLE_ARROW)=> 	eventPattern	LR_DOUBLE_ARROW^ eventPatternRewriteExpr ';'!
					|					eventPatternRewriteExpr RL_DOUBLE_ARROW^ eventPattern ';'!
                    /*|	eventPattern	'<-->' eventPattern*/
					;

eventContext	: ( '(' ( stackFramePattern SCOPE_RESOLUTION )+ ')' )? -> ^( EVENT_CONTEXT stackFramePattern* )
				;
                
stackFramePattern 	: IDENT^
					;

eventPattern	:	atomicEventPattern
				; /* TODO: add composite (sequence) event patterns */
           
atomicEventPattern	: eventContext memberNameExpr '(' ( ( annotatedValuePattern ( ',' annotatedValuePattern )* ) | ELLIPSIS )? ')'
						-> ^( EVENT_PATTERN eventContext memberNameExpr annotatedValuePattern* )
					;

annotatedValuePattern 	: valuePattern valuePatternAnnotation? -> ^( VALUE_PATTERN valuePattern valuePatternAnnotation? )
						;

valuePatternAnnotation	: KEYWORD_AS^ memberNameExpr 
						| '{'! KEYWORD_NAMES^ memberNameExpr '}'!
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
          
stubStatementBody	:	assignment
					|	emitStatement
                	| 	stubLangExpression
                    |	KEYWORD_SKIP
                	;
                
stubLangExpression	/*: constantOrVoidValueDescription^
					| ifThenElseExpression^
                    | booleanArithmeticExpression^
					| invocation^
                    | runtimeValueIdentExpr^*/
					/*| stubDescription^ /* sequence */
                    : conditionalExpression /* lowest precedence operator */
                    ;

stubLiteralExpression	: STRING_LIT
						| INT
                        | KEYWORD_VOID
                        | KEYWORD_NULL
                        | KEYWORD_TRUE
                        | KEYWORD_FALSE
                        ;
            
stubPrimitiveExpression	: stubLiteralExpression
						| METAVAR
                        | memberNameExpr
                        | '('! stubLangExpression ')'!
                        ;

memberSelectionExpression	: stubPrimitiveExpression (MEMBER_SELECT^ stubPrimitiveExpression )*
							; /* left-associative 
                               * FIXME: Note this subsumes memberNameExpr, so we don't need it. */

functionInvocationExpression	: (memberSelectionExpression '(') => memberSelectionExpression '(' ( stubLangExpression (',' stubLangExpression )* )? ')'
									-> ^( INVOCATION memberSelectionExpression stubLangExpression* )
                                | memberSelectionExpression
    							;
         
/* Here MULTIPLY is actually unary * (dereference) */
unaryOperatorExpression	: (COMPLEMENT^|NOT^|MINUS^|PLUS^|MULTIPLY^)* functionInvocationExpression
						;
                        
castExpression	: unaryOperatorExpression reinterpretation?
				;
                                        
multiplicativeOperatorExpression	: castExpression ( ( MULTIPLY^ | DIVIDE^ | MODULO^ ) castExpression )*
									;
                                    
additiveOperatorExpression 	: multiplicativeOperatorExpression ( ( PLUS^ | MINUS^ ) multiplicativeOperatorExpression )*
							;
                            
shiftingExpression	: additiveOperatorExpression ( (SHIFT_LEFT^ | SHIFT_RIGHT^ )  additiveOperatorExpression )*
					;
                    
magnitudeComparisonExpression 	: shiftingExpression ( ( LESS^ | GREATER^ | LE^ | GE^ ) shiftingExpression )?
								;
                                
equalityComparisonExpression	: magnitudeComparisonExpression ( ( EQ^ | NEQ^ ) magnitudeComparisonExpression )*
								;
                                
bitwiseAndExpression	: equalityComparisonExpression ( BITWISE_AND^ equalityComparisonExpression )*
						;
                        
bitwiseXorExpression	: bitwiseAndExpression ( BITWISE_XOR^ bitwiseAndExpression )*
						;
                        
bitwiseOrExpression	: bitwiseXorExpression ( BITWISE_OR^ bitwiseXorExpression )*
					;
                    
logicalAndExpression 	: bitwiseOrExpression ( LOGICAL_AND^ bitwiseOrExpression )*
						;
                        
logicalOrExpression	: logicalAndExpression ( LOGICAL_OR^ logicalAndExpression )*
					;
                    
conditionalExpression	: logicalOrExpression
						| KEYWORD_IF cond=conditionalExpression KEYWORD_THEN caseTrue=conditionalExpression KEYWORD_ELSE caseFalse=conditionalExpression
                        	-> ^( CONDITIONAL $cond $caseTrue $caseFalse )
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
assignment 	: KEYWORD_LET^ IDENT '='! stubLangExpression
			;
            
emitStatement	:	KEYWORD_EMIT^ stubLangExpression
				;
            
valueCorrespondenceBlock	: KEYWORD_VALUES^ '{'! valueCorrespondence* '}'!
							;
                            
valueCorrespondence	: valueCorrespondenceBase^ ( ';'! | valueCorrespondenceRefinement )
					;

reinterpretation 	: KEYWORD_AS memberNameExpr
					;
                        
valueCorrespondenceBase	: 
		valueCorrespondenceSide correspondenceOperator^ correspondenceOperatorModifier? valueCorrespondenceSide
/*    |   'const'! constantValueDescription ( leftToRightCorrespondenceOperator^ | bidirectionalCorrespondenceOperator^ ) correspondenceOperatorModifier? memberNameExpr reinterpretation?
    | 	'void' rightToLeftCorrespondenceOperator^ correspondenceOperatorModifier? memberNameExpr
 	|	stubDescription leftToRightCorrespondenceOperator^ correspondenceOperatorModifier? memberNameExpr reinterpretation?
    |	memberNameExpr reinterpretation? rightToLeftCorrespondenceOperator^ stubDescription*/
    ;

valueCorrespondenceSide	: memberNameExpr reinterpretation? 
						| KEYWORD_CONST! constantValueDescription 
                        | KEYWORD_VOID
                        | stubDescription
                        ;

/*valuePattern ('<-->'|'-->'|'<--')^ valuePattern
						;*/

correspondenceOperator 	: bidirectionalCorrespondenceOperator
						| leftToRightCorrespondenceOperator
                        | rightToLeftCorrespondenceOperator
						;
                            
bidirectionalCorrespondenceOperator	:	BI_DOUBLE_ARROW^
									;
                            
leftToRightCorrespondenceOperator	: LR_DOUBLE_ARROW^
                            		| LR_DOUBLE_ARROW_Q^
                                    ;
                                    
rightToLeftCorrespondenceOperator	: RL_DOUBLE_ARROW^
									| RL_DOUBLE_ARROW_Q^
                                    ;
                                    
correspondenceOperatorModifier	:	'{'! stubDescription '}'!
								|	'['!  ']'!
								;
                    
valueCorrespondenceRefinement	:	'{'! valueCorrespondence* '}'!
								;
                                
/*valueCorrespondenceRefinementElement	: */
                    

inlineDeclaration			:	KEYWORD_INLINE^ objectSpec wellNestedTokenBlock
							;
                            
wellNestedTokenBlock : '{' ( INT | STRING_LIT | IDENT | 'alias' | 'any' | ';' | '[' | ',' | ']' | 'exists' | 'check' | 'declare' | 'override' | ':' | '.' | '_' | '->' | 'object' | 'ptr' | '<' | '>' | '=' | 'enum' | 'enumerator' | '==' | '<<' | '>>' | wellNestedTokenBlock | '<-->' | '<--' | '-->' | '#' | '::' | '?' | '+' | '-' | '/' | '|' | '(' | ')' | 'let' )* '}';

/* Lexer */

/* boilerplate tokens */
KEYWORD_ALIAS : 'alias';
KEYWORD_ANY : 'any';
KEYWORD_DERIVING : 'deriving';
KEYWORD_EXISTS : 'exists';
KEYWORD_STATIC : 'static' ;
KEYWORD_CHECK : 'check';
KEYWORD_DECLARE : 'declare';
KEYWORD_OVERRIDE : 'override';
KEYWORD_CONST : 'const';
KEYWORD_OPAQUE : 'opaque';
KEYWORD_IGNORED : 'ignored';
KEYWORD_VOID : 'void';
KEYWORD_NULL : 'null';
FUNCTION_ARROW : '=>';
KEYWORD_BASE : 'base' ;
KEYWORD_OBJECT : 'object';
KEYWORD_PTR : 'ptr';
KEYWORD_CLASS_OF : 'class_of';
KEYWORD_ENUM : 'enum';
KEYWORD_ENUMERATOR : 'enumerator';
SHIFT_LEFT : '<<';
SHIFT_RIGHT : '>>';
KEYWORD_LINK : 'link';
KEYWORD_DERIVE : 'derive';
BI_DOUBLE_ARROW : '<-->';
RL_DOUBLE_ARROW : '<--';
LR_DOUBLE_ARROW : '-->';
SCOPE_RESOLUTION : '::';
ELLIPSIS : '...';
KEYWORD_AS : 'as';
KEYWORD_NAMES : 'names';
KEYWORD_SKIP : 'skip';
KEYWORD_TRUE : 'true';
KEYWORD_FALSE : 'false';
LE : '<=';
GE : '>=';
EQ : '==';
NEQ : '!=';
LOGICAL_AND : '&&';
LOGICAL_OR : '||';
KEYWORD_IF : 'if';
KEYWORD_THEN : 'then';
KEYWORD_LET : 'let';
KEYWORD_EMIT : 'emit';
KEYWORD_VALUES : 'values';
KEYWORD_ELSE : 'else';
LR_DOUBLE_ARROW_Q : '-->?' ;
RL_DOUBLE_ARROW_Q : '<--?' ;
KEYWORD_INLINE : 'inline';
//UNDERSCORE : '_';
INDEFINITE_MEMBER_NAME : '_';
// DOT : '.';
MEMBER_SELECT : '.';
//TILDE : '~';
COMPLEMENT : '~';
//EXCLAMATION : '!';
NOT : '!';
MINUS : '-';
PLUS : '+';
//AMPERSAND : '&';
BITWISE_AND : '&';
//STAR : '*';
MULTIPLY : '*';
//SLASH : '/';
DIVIDE : '/';
//PERCENT : '%';
MODULO : '%';
LESS : '<';
GREATER : '>';
//CARET : '^';
BITWISE_XOR : '^';
//BAR : '|';
BITWISE_OR : '|';

/* Fallback (interesting) tokens */
INT :   '0'..'9'+ ;
NEWLINE:'\r'? '\n' {antlr_m4_newline_action} ;
WS  :   (' '|'\t')+ {antlr_m4_skip_action} ;
LINECOMMENT : '/' '/'( ~ '\n' )* {antlr_m4_skip_action} ;
BLOCKCOMMENT : '/' '*' ( ~ '/' | ( ~ '*' ) '/' )* '*' '/' {antlr_m4_skip_action} ;
STRING_LIT : '\"' ( ~'\"'|'\\\"' )* '\"' ;
IDENT  :   ('a'..'z'|'A'..'Z'|'_''a'..'z'|'_''A'..'Z'|'_''0'..'9'|'\\'.) /* begin with a-zA-Z or non-terminating '_' */
(
	('a'..'z'|'A'..'Z'|'0'..'9'|'\\'.|'_'|'-'|'.'/*'0'..'9'*/)*('a'..'z'|'A'..'Z'|'0'..'9'|'\\'.|'_')
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
/* FIXME: semantic action to process backslash-escapes within IDENT? */
