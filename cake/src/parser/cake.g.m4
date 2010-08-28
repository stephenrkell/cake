include(antlr_m4_include_file)
grammar antlr_m4_make_grammar_name(cake,antlr_m4_grammar_name_suffix);
options {
    output=AST;
    backtrack=true; /* seems to be necessary for C-style postfix operators.... */
    language=antlr_m4_language;
    antlr_m4_extra_options
}

tokens { ENCLOSING; MULTIVALUE; IDENT_LIST; SUPPLEMENTARY; INVOCATION; CORRESP; STUB; EVENT_PATTERN; 
ANNOTATED_VALUE_PATTERN; EVENT_CONTEXT; SET_CONST; CONDITIONAL; TOPLEVEL; OBJECT_CONSTRUCTOR; OBJECT_SPEC_DIRECT; 
OBJECT_SPEC_DERIVING; EXISTS_BODY; DEFINITE_MEMBER_NAME; MEMBERSHIP_CLAIM; VALUE_DESCRIPTION; DWARF_BASE_TYPE; 
DWARF_BASE_TYPE_ATTRIBUTE; DWARF_BASE_TYPE_ATTRIBUTE_LIST; REMAINING_MEMBERS; ANY_VALUE; PAIRWISE_BLOCK_LIST; 
EVENT_CORRESP; EVENT_SINK_AS_PATTERN; EVENT_SINK_AS_STUB; CONST_ARITH; KEYWORD_PATTERN; INFIX_STUB_EXPR; 
IDENTS_TO_BIND; ARRAY; VALUE_CONSTRUCT; EVENT_PATTERN_REWRITE_EXPR; RETURN_EVENT; INVOKE_WITH_ARGS; 
EVENT_WITH_CONTEXT_SEQUENCE; CONTEXT_SEQUENCE; EVENT_COUNT_PREDICATE; ARRAY_SUBSCRIPT; FORM_ASSOCIATION; 
VALUE_CORRESPONDENCE_REFINEMENT; }


@header {
antlr_m4_header
}
@lexer::header {
antlr_m4_header
}

/**************************
 *      DWARF syntax      *
 **************************/
 
/* FIXME: rename this to something more neutral
 * like "named element definition" */
/* FIXME: provide explicit compile units */
/* FIXME: allow offset labelling here using the "@" prefix e.g.
 * @ 11: _: compile_unit { ... }; */
membershipClaim		: (memberNameExpr ':' KEYWORD_CLASS_OF)=> 
                    	memberNameExpr ':' KEYWORD_CLASS_OF valueDescriptionExpr SEMICOLON
						-> ^( MEMBERSHIP_CLAIM memberNameExpr ^( KEYWORD_CLASS_OF  valueDescriptionExpr ) )
                    | (memberNameExpr ':' KEYWORD_CONST)=>
                        memberNameExpr ':' KEYWORD_CONST namedDwarfTypeDescription '=' constantValueDescription SEMICOLON
                        -> ^( MEMBERSHIP_CLAIM memberNameExpr ^( KEYWORD_CONST namedDwarfTypeDescription constantValueDescription ) )
                    | namedValueDescription SEMICOLON
						-> ^( MEMBERSHIP_CLAIM namedValueDescription )
					| ELLIPSIS ':' valueDescriptionExpr SEMICOLON
                    	-> ^( MEMBERSHIP_CLAIM REMAINING_MEMBERS valueDescriptionExpr )
                    ;/* FIXME: ellipsis is Cake-specific, or at least Dwarf Predicate -specific...
                      * note that you can just insert extra alternatives easily in ANTLR by doing
                      * myRuleName : mySmallerRuleName
                                   | ELLIPSIS ...;  */

//memberNameExpr		/*: '.'? IDENT ( '.' IDENT )* -> ^( DEFINITE_MEMBER_NAME IDENT* )*/
                                          
memberNameExpr		: definiteMemberName -> ^( DEFINITE_MEMBER_NAME definiteMemberName )
					| INDEFINITE_MEMBER_NAME^ 
                    ;
                    
definiteMemberName : IDENT^ ( memberSuffix^ )*
                   ;
memberSuffix : '.' IDENT
         -> ^( '.' IDENT )
       | '[' constantIntegerArithmeticExpression ']'
         -> ^( ARRAY_SUBSCRIPT constantIntegerArithmeticExpression )
         ;

/* The following alternatives are in precedence order, highest to lowest */
valueDescriptionExpr		: primitiveOrFunctionValueDescription 
	-> ^( VALUE_DESCRIPTION primitiveOrFunctionValueDescription)
                            /*| functionValueDescription*/
                            ;

primitiveOrFunctionValueDescription	: 
	| (argumentMultiValueDescription FUNCTION_ARROW)=> 
    	argumentMultiValueDescription FUNCTION_ARROW primitiveOrFunctionValueDescription 
			-> ^(FUNCTION_ARROW argumentMultiValueDescription primitiveOrFunctionValueDescription )
	| primitiveValueDescription
	; 

primitiveValueDescription	: KEYWORD_CONST^ constantValueDescription
                            | valueIntrinsicAnnotation? unannotatedValueDescription valueInterpretation?
                        	;

/*** FIXME: the following is Cake-specific, not Dwarf! ***/
valueInterpretation : KEYWORD_AS^ unannotatedValueDescription valueConstructionExpression
                    | KEYWORD_OUT_AS^ unannotatedValueDescription valueConstructionExpression
                    | KEYWORD_INTERPRET_AS^ unannotatedValueDescription valueConstructionExpression
                    | KEYWORD_IN_AS^ unannotatedValueDescription valueConstructionExpression
					;
/*** FIXME: the following is Cake-specific, not Dwarf! ***/
/*valueConstructionExpression : '(' stubStatementBody ( ',' stubStatementBody )* ')' 
								-> ^( VALUE_CONSTRUCT stubStatementBody* )
                            | -> ^( VALUE_CONSTRUCT )
                            ;*/
valueConstructionExpression : '(' stubNonSequencingExpression ( ',' stubNonSequencingExpression )* ')' 
								-> ^( VALUE_CONSTRUCT stubNonSequencingExpression* )
                            | -> ^( VALUE_CONSTRUCT )
                            ;

valueIntrinsicAnnotation : KEYWORD_OPAQUE^ valueIntrinsicAnnotation?
                         | KEYWORD_IGNORED^ valueIntrinsicAnnotation?
                         | KEYWORD_INVALID^ valueIntrinsicAnnotation?
                         | KEYWORD_CALLER_FREE^ '('! IDENT ')'! valueIntrinsicAnnotation?
                         ;

valueModeAnnotation : KEYWORD_OUT -> ^( KEYWORD_OUT) //^ valueAnnotation?
                    | KEYWORD_IN -> ^( KEYWORD_IN ) //^ valueAnnotation?
                    | KEYWORD_INOUT -> ^(KEYWORD_INOUT) //^ valueAnnotation?
                    ;
                            
unannotatedValueDescription : /*unspecifiedValueDescription^
							|*/ simpleOrObjectOrPointerValueDescription^
                            ;


/* the multiValueDescription for argument specs: needn't have names, may have modes! 
 * -- at the moment this is the same as multiValueDescription */
argumentMultiValueDescription : multiValueDescription
                              ; 

multiValueDescription	: '(' optionallyNamedWithModeValueDescription (',' optionallyNamedWithModeValueDescription )*  ')'
								-> ^( MULTIVALUE optionallyNamedWithModeValueDescription* )
                                ;

optionallyNamedWithModeValueDescription: valueModeAnnotation^? optionallyNamedValueDescription
                                    ;
      
optionallyNamedValueDescription 	: (namedValueDescription)=>namedValueDescription
									| primitiveOrFunctionValueDescription
                                    ;

namedValueDescription 	: memberNameExpr ':'! valueDescriptionExpr
						;
/* the multiValueDescription for tuples: must have names! */
namedMultiValueDescription : '(' namedValueDescription (',' namedValueDescription )*  ')'
								-> ^( MULTIVALUE namedValueDescription* )
                           ;
                           
							                            
structuredValueDescription	: KEYWORD_OBJECT '{' membershipClaim* '}'
								-> ^(KEYWORD_OBJECT membershipClaim* )
                            /*| multiValueDescription*/
                            ;
                            
simpleOrObjectOrPointerValueDescription : structuredValueDescription^ ( valueDescriptionModifierSuffix^ )*
                                        | simpleValueDescription^ ( valueDescriptionModifierSuffix^ )*
                                        | enumValueDescription^ ( valueDescriptionModifierSuffix^ )*
                                        | KEYWORD_VOID^ ( valueDescriptionModifierSuffix^ )*
                                        | '('! primitiveValueDescription ')'! ( valueDescriptionModifierSuffix^ )*
                                        ;

valueDescriptionModifierSuffix : KEYWORD_PTR
                               | '[' arraySizeExpr? ']' -> ^( ARRAY arraySizeExpr? )
                               ;

arraySizeExpr : /*  memberNameExpr | INT */ constantIntegerArithmeticExpression;

simpleValueDescription		: namedDwarfTypeDescription^
                            | INDEFINITE_MEMBER_NAME -> ANY_VALUE
							/*| '('! primitiveOrFunctionValueDescription^ ')'!*/
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

dwarfBaseTypeAttributeDefinition 	: attr=IDENT '=' ( value=IDENT | value=INT ) SEMICOLON
									-> ^( DWARF_BASE_TYPE_ATTRIBUTE $attr $value )
									;
/*enumValueDescription	: KEYWORD_ENUM^ ( ( ( IDENT | '_' ) byteSizeParameter? enumDefinition? ) | ( byteSizeParameter? enumDefinition ) )*/
enumValueDescription    : KEYWORD_ENUM^ dwarfBaseTypeDescription enumDefinition
                        | KEYWORD_ENUM enumDefinition
                         -> ^( KEYWORD_ENUM ^( DWARF_BASE_TYPE ) enumDefinition ) /* empty base type => int */
                        ;

enumDefinition	: '{'! enumElement* '}'!
				;

enumElement : 
            (KEYWORD_ENUMERATOR IDENT EQ)=>
            KEYWORD_ENUMERATOR name=IDENT EQ constantIntegerArithmeticExpression SEMICOLON
            -> ^( KEYWORD_ENUMERATOR ^( KEYWORD_LET ^( IDENTS_TO_BIND $name ) constantIntegerArithmeticExpression ) )
            | ident=IDENT SEMICOLON
            -> ^( KEYWORD_ENUMERATOR IDENT ) /* using an ident defined elsewhere (must be constant value!) */
            ;

/*********************************************************
 * literal values and compile-time constant expressions  *
 * -- could be useful in DWARF syntax? Or just for Cake? *
 * -- certainly useful in Dwarf Predicates....           *
 *********************************************************/

patternConstantValueDescription : STRING_LIT^
                                | INT ELLIPSIS^ INT
                                ;

constantOrVoidValueDescription	:	constantValueDescription^
								|	KEYWORD_VOID^
                                ;

constantValueDescription	: STRING_LIT^ /* FIXME: need IDENTS here too? depends what we use these for */
                            | KEYWORD_NULL^
                            | constantSetExpression
							| constantIntegerArithmeticExpression 
                            	-> ^( CONST_ARITH constantIntegerArithmeticExpression )
                            ;
                            
/* FIXME: lists are like these but without the "set" keyword */
constantSetExpression	: KEYWORD_SET '[' ( constantValueDescription ( ',' constantValueDescription* )* )? ']' -> ^( SET_CONST constantValueDescription* )
						;

setExpression           : KEYWORD_SET '[' ( stubLangExpression ( ',' stubLangExpression* )* )? ']' -> ^( KEYWORD_SET stubLangExpression* )
						;
           
constantIntegerArithmeticExpression	: constantShiftingExpression^
									;
/* FIXME: rather than repeating these rules, just use stub expressions
 * and push the compile-time-const detection to semantic analysis
 * (where it has to be anyway, because we use memberNameExpr which
 * might actually be referencing non-compile-time-const values). */

primitiveIntegerArithmeticExpression	: INT^
                                        | memberNameExpr /* HACK: so we can use this rule in array suffix */
										| '('! constantIntegerArithmeticExpression^ ')'!
                                        ;
constantUnaryOperatorExpression	: (MINUS^|PLUS^)* primitiveIntegerArithmeticExpression
                        ;
                                        
constantMultiplicativeOperatorExpression	: constantUnaryOperatorExpression ( ( MULTIPLY^ | DIVIDE^ | MODULO^ ) constantUnaryOperatorExpression )*
									;
constantAdditiveOperatorExpression 	: constantMultiplicativeOperatorExpression ( ( PLUS^ | MINUS^ ) constantMultiplicativeOperatorExpression )*
							;
constantShiftingExpression	: constantAdditiveOperatorExpression ( ( SHIFT_LEFT^ | SHIFT_RIGHT^ ) constantAdditiveOperatorExpression )* 
					;
 



/**************************
 *       Cake syntax      *
 **************************/

/* the whole input */
toplevel:   declaration* //-> ^( TOPLEVEL<ToplevelNode> declaration* )
			/*{sys.stdout.write($objectExpr.tree.toStringTree() + '\n');} */
        ;

declaration		: existsDeclaration SEMICOLON!?
				| aliasDeclaration SEMICOLON!?
                | supplementaryDeclaration SEMICOLON!?
                | inlineDeclaration SEMICOLON!?
                | deriveDeclaration SEMICOLON!?
				;

/* Because ANTLR is *stupid*, I have taken care not to include any literals that
 * would make their way into the AST. Since these are assigned an unpredictable
 * token type, they are not testable-for at runtime. This is so annoying. */

aliasDeclaration	: KEYWORD_ALIAS^ aliasDescription IDENT SEMICOLON!
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
					| SEMICOLON -> ^( EXISTS_BODY )
					;
                    
globalRewrite		: KEYWORD_STATIC? valueDescriptionExpr LR_DOUBLE_ARROW^ valueDescriptionExpr SEMICOLON!
					;

claimGroup			: KEYWORD_CHECK^ '{'! claim* '}'!
					| KEYWORD_DECLARE^ '{'! claim* '}'!
                    | KEYWORD_OVERRIDE^ '{'! claim* '}'!
					;

claim				: membershipClaim
					;

deriveDeclaration	: KEYWORD_DERIVE^ objectConstructor IDENT '='! derivedObjectExpression SEMICOLON!
					;
                    
derivedObjectExpression	: IDENT^ ( '('! derivedObjectFunctionArgument (','! derivedObjectFunctionArgument)* ')'! )?
						| KEYWORD_LINK^ identList linkRefinement
						;
                        
derivedObjectFunctionArgument: derivedObjectExpression
                             | STRING_LIT
                             ;

linkRefinement	: '{' pairwiseCorrespondenceBlock* '}' -> ^( PAIRWISE_BLOCK_LIST pairwiseCorrespondenceBlock* )
				| -> ^( PAIRWISE_BLOCK_LIST );
                
pairwiseCorrespondenceBlock	:	IDENT BI_DOUBLE_ARROW^ IDENT pairwiseCorrespondenceBody
							;
                            
pairwiseCorrespondenceBody	: '{' pairwiseCorrespondenceElement* '}' -> ^( CORRESP pairwiseCorrespondenceElement* )
							;
pairwiseCorrespondenceElementEOFHack: pairwiseCorrespondenceElement^ EOF!; /* HACK for testing */
pairwiseCorrespondenceElement   : (IDENT ':' eventCorrespondence) =>
                                    IDENT ':' eventCorrespondence
                                    -> ^( EVENT_CORRESP eventCorrespondence IDENT ) /* named form */
                                |   eventCorrespondence -> ^( EVENT_CORRESP eventCorrespondence ) /* ordinary form */
                                | (IDENT ':' KEYWORD_VALUES '{')=> /* named form */
                                    IDENT ':' valueCorrespondenceBlock
                                    -> ^( valueCorrespondenceBlock IDENT )
                                | (KEYWORD_VALUES '{')=>  /* ordinary form */
                                    valueCorrespondenceBlock
                                | singleValueCorrespondence /* ordinary form */
                                | IDENT ':' singleValueCorrespondence /* named form */
                                   -> ^( singleValueCorrespondence IDENT )
                                ;
                                
eventCorrespondence	:	
 (eventPattern infixStubExpression LR_DOUBLE_ARROW)=>            
  eventPattern            infixStubExpression LR_DOUBLE_ARROW^ infixStubExpression eventPatternRewriteExpr leftRightEventCorrespondenceTerminator
|(eventPatternRewriteExpr infixStubExpression RL_DOUBLE_ARROW)=> 
  eventPatternRewriteExpr infixStubExpression RL_DOUBLE_ARROW^ infixStubExpression eventPattern            rightLeftEventCorrespondenceTerminator
|(atomicEventPattern infixStubExpression BI_DOUBLE_ARROW)=>            
  atomicEventPattern      infixStubExpression BI_DOUBLE_ARROW^ infixStubExpression atomicEventPattern      bidirectionalEventCorrespondenceTerminator
                    /*| KEYWORD_PATTERN eventCorrespondence -> ^(KEYWORD_PATTERN eventCorrespondence)*/
					;
                    
bidirectionalEventCorrespondenceTerminator : SEMICOLON -> ^( RETURN_EVENT )
                                           ;
leftRightEventCorrespondenceTerminator: SEMICOLON -> ^( RETURN_EVENT )
                             | '<--' stubNonSequencingExpression SEMICOLON
                               -> ^( RETURN_EVENT stubNonSequencingExpression )
                             | '<--' CONT_LBRACE sequencingExpression ';'? '}' SEMICOLON
                               -> ^( RETURN_EVENT sequencingExpression )
                             ;
rightLeftEventCorrespondenceTerminator: SEMICOLON -> ^( RETURN_EVENT )
                             | '-->' stubNonSequencingExpression SEMICOLON
                               -> ^( RETURN_EVENT stubNonSequencingExpression )
                             | '-->' CONT_LBRACE sequencingExpression ';'? '}' SEMICOLON
                               -> ^( RETURN_EVENT sequencingExpression )
                             ;
eventContext    : ( '(' ( stackFramePattern SCOPE_RESOLUTION )+ ')' )? -> ^( EVENT_CONTEXT stackFramePattern* )
                ;
                
stackFramePattern   : IDENT^
                    ;

eventPatternEOFHack: eventPattern^ EOF!;
eventPattern    :   atomicEventPattern
                |   contextBindingEventPattern ( ',' contextBindingEventPattern )* ',' atomicEventPattern
                  -> ^( EVENT_WITH_CONTEXT_SEQUENCE ^( CONTEXT_SEQUENCE contextBindingEventPattern* ) atomicEventPattern )
                ;
                
contextBindingEventPattern : bindingPrefix^ atomicEventPattern
                           | ELLIPSIS
                           ;

atomicEventPattern	: 
	eventContext memberNameExpr eventParameterNamesAnnotation ( '(' ( ( annotatedValueBindingPattern ( ',' annotatedValueBindingPattern )* ) | ELLIPSIS )? ')' eventCountPredicate? )?
		-> ^( EVENT_PATTERN eventContext memberNameExpr ^( EVENT_COUNT_PREDICATE eventCountPredicate? ) eventParameterNamesAnnotation annotatedValueBindingPattern* )
    | eventContext identPattern eventParameterNamesAnnotation ( '(' ( ( annotatedValueBindingPattern ( ',' annotatedValueBindingPattern )* ) | ELLIPSIS )? ')' eventCountPredicate? )?
		-> ^( EVENT_PATTERN eventContext identPattern ^( EVENT_COUNT_PREDICATE eventCountPredicate? ) eventParameterNamesAnnotation annotatedValueBindingPattern* )
	;

eventCountPredicate : '['! ( INT | IDENT )^ ']'!
                    ;

                
//eventPatternRewriteExpr	: simpleEventPatternRewriteExpr 
//                             -> ^( EVENT_SINK_AS_PATTERN simpleEventPatternRewriteExpr ) /* shorthand for a trivial stub */
//                        | stubDescription 
//                             -> ^( EVENT_SINK_AS_STUB stubDescription )
eventPatternRewriteExpr : stubNonSequencingExpression
                        -> ^( EVENT_SINK_AS_STUB stubNonSequencingExpression )
                        | '{' sequencingExpression ';'? CONT_RBRACE
                        -> ^( EVENT_SINK_AS_STUB sequencingExpression )
                        ;
//simpleEventPatternRewriteExpr:	memberNameExpr ( '(' ( stubLangExpression ( ',' stubLangExpression )* )  ')' )?
//                                   -> ^( EVENT_PATTERN_REWRITE_EXPR memberNameExpr stubLangExpression* )
//                             ;

                    
identPattern : KEYWORD_PATTERN^ PATTERN_IDENT; 

annotatedValueBindingPattern 	: valueModeAnnotation? valuePattern valueBindingPatternAnnotation? 
								-> ^( ANNOTATED_VALUE_PATTERN valuePattern valueBindingPatternAnnotation?  )
						;
/* valueModeAnnotations are "in", "out", "inout" and can be specified here as a convenience
 * instead of declaring these annotations in an exists block. 
 *
 * Meanwhile, valueBindingPatternAnnotations are things like "as" and "names" and refer to the
 * particular treatment of a value captured by a pattern.
 * The line is not particularly clean... note how "names" might reasonably be made into a
 * DWARF attribute, much like "opaque" and "ignored", but "as" wouldn't. */

valueBindingPatternAnnotation	: valueInterpretation
						| '{'! KEYWORD_NAMES^ memberNameExpr '}'!
						;

eventParameterNamesAnnotation	: ('{')=> '{'! KEYWORD_NAMES^ namedMultiValueDescription '}'!
								| -> ^( KEYWORD_NAMES )
								;
/* valuePatterns are used 
 * - in eventCorrespondences, when matching *source* or bidirectional argument values etc.
 * - in valueCorrespondences, when matching *target* or bidirectional corresponded values
 * ... are the requirements of these different?
 *     -- the latter wants to support valueInterpretations
 *        -- this is a mechanism for selecting an alternative value correspondences
 *        -- therefore it should be available for event correspondences
 *     -- the latter also wants to support array subscripting
 *        -- this is only (so far) with compile-time-constant subscripts, so...
 *           ... is just like member selection, and should be available for event corresps too
 * */
valuePatternEOFHack: valuePattern^ EOF!;
valuePattern		: (memberNameExpr '[')=>
                      memberNameExpr '[' constantIntegerArithmeticExpression ']' valueInterpretation?
                      -> ^( memberNameExpr ^( ARRAY_SUBSCRIPT constantIntegerArithmeticExpression ) valueInterpretation? )
                    | memberNameExpr valueInterpretation? /* matches a named constant value -- also matches '_' */
                      -> ^( memberNameExpr valueInterpretation? )
					/*| METAVAR^ */ /* matches any value, and names it */
					| constantValueDescription -> ^( KEYWORD_CONST constantValueDescription ) /* matches that constant */
                    | KEYWORD_VOID^ /* used for "no value" correspondences & inserting "arbitrary" code */
                    | KEYWORD_THIS^ /* only for correspondences nested in refinement blocks -- see ephy.cake */
                    | KEYWORD_PATTERN^ patternConstantValueDescription
                   /* | namedMultiValueDescription */ /* can't do this---introduces ambiguity with infixStubExpression in valueCorrespondence */
                    ;

bindingEOFHack: binding EOF; /* HACK: see stubLangExpressionEOFHack */
binding 	: bindingPrefix^ stubLangExpression
            ; /*           | bindingPrefix stubLangExpression
              -> ^( KEYWORD_LET bindableIdentSet stubLangExpression )
			;*/

bindingPrefix : bindingKeyword^ bindableIdentSet '='!
              | bindableIdentSet RL_SINGLE_ARROW
                -> ^( KEYWORD_LET bindableIdentSet )
              ;
            
bindableIdentSet 	: bindableIdentWithOptionalInterpretation 
                        -> ^( IDENTS_TO_BIND bindableIdentWithOptionalInterpretation )
					| '(' bindableIdentWithOptionalInterpretation ( ',' bindableIdentWithOptionalInterpretation )+ ')' 
                        -> ^( IDENTS_TO_BIND bindableIdentWithOptionalInterpretation* )
                    | lhs=IDENT ELLIPSIS rhs=IDENT
                        -> ^( FORM_ASSOCIATION $lhs $rhs )
                    ;

bindableIdentWithOptionalInterpretation: IDENT valueInterpretation? -> ^( IDENT valueInterpretation? )
                                       ;
            
bindingKeyword : KEYWORD_LET | KEYWORD_OUT ;
            
//emitStatement	:	/*KEYWORD_EMIT^*/ KEYWORD_OUT stubLangExpression
//				;
            
valueCorrespondenceBlock	: KEYWORD_VALUES^ '{'! valueCorrespondence* '}'!
							;
                            
singleValueCorrespondence 	: KEYWORD_VALUES^ valueCorrespondence
							;

valueCorrespondenceEOFHack: valueCorrespondence^ EOF!;
valueCorrespondence	: valueCorrespondenceBase^ valueCorrespondenceTerminator;

valueCorrespondenceTerminator: SEMICOLON
                               -> ^( VALUE_CORRESPONDENCE_REFINEMENT )
                             | valueCorrespondenceRefinement
                             ;

/*reinterpretation 	: KEYWORD_AS memberNameExpr
					;*/
                        
	  /*annotatedValueBindingPattern infixStubExpression correspondenceOperator^ infixStubExpression valuePattern*/
valueCorrespondenceBase	: 
    (valuePattern infixStubExpression leftToRightCorrespondenceOperator)=>
     valuePattern infixStubExpression leftToRightCorrespondenceOperator^ infixStubExpression stubNonSequencingExpression
  | (stubNonSequencingExpression infixStubExpression rightToLeftCorrespondenceOperator)=>
    stubNonSequencingExpression infixStubExpression rightToLeftCorrespondenceOperator^ infixStubExpression /*annotatedValueBindingPattern*/ valuePattern
  | (valuePattern infixStubExpression bidirectionalCorrespondenceOperator)=>
    valuePattern infixStubExpression bidirectionalCorrespondenceOperator^ infixStubExpression valuePattern
  | (singleOrNamedMultiValueDescription bidirectionalCorrespondenceOperator)=>
    singleOrNamedMultiValueDescription bidirectionalCorrespondenceOperator^ singleOrNamedMultiValueDescription
  ;
  
singleOrNamedMultiValueDescription: memberNameExpr^
                                  | namedMultiValueDescription^
                                  ;

//		valueCorrespondenceSide rightToLeftCorrespondenceOperator^ correspondenceOperatorModifier? valueCorrespondenceSide*/
//    | memberNameExpr infixStubExpression? rightToLeftCorrespondenceOperator infixStubExpression? valuePattern
//    | memberNameExpr infixStubExpression? bidirectionalCorrespondenceOperator infixStubExpression? memberNameExpr
//    ;

/*valueCorrespondenceSide	: memberNameExpr valueInterpretation? 
						| KEYWORD_CONST! constantValueDescription 
                        | KEYWORD_VOID
                        | stubDescription
                        ;*/

/*valuePattern ('<-->'|'-->'|'<--')^ valuePattern
						;*/

correspondenceOperator  : bidirectionalCorrespondenceOperator
                        | leftToRightCorrespondenceOperator
                        | rightToLeftCorrespondenceOperator
                        ;

infixStubExpression	: ('(')=> '(' stubNonSequencingExpression ')' -> ^( INFIX_STUB_EXPR stubNonSequencingExpression )
					| -> ^( INFIX_STUB_EXPR )
					;
                            
bidirectionalCorrespondenceOperator	:	BI_DOUBLE_ARROW^
									;
                            
leftToRightCorrespondenceOperator	: LR_DOUBLE_ARROW^
                            		| LR_DOUBLE_ARROW_Q^
                                    ;
                                    
rightToLeftCorrespondenceOperator	: RL_DOUBLE_ARROW^
									| RL_DOUBLE_ARROW_Q^
                                    ;
                                    
/*correspondenceOperatorModifier	:	'{'! stubDescription '}'!
								|	'['!  ']'!
								;*/
                    
valueCorrespondenceRefinement  : '{' valueCorrespondence* '}' ';'
                                 -> ^( VALUE_CORRESPONDENCE_REFINEMENT valueCorrespondence* )
								;
                                
/*valueCorrespondenceRefinementElement	: */
                        
/*stubDescription		: ('{' | CONT_LBRACE) stubStatementBody SEMICOLON? ('}' | CONT_RBRACE) 
						-> ^( STUB stubStatementBody )
					;*/
          
/*stubStatementBody	:	binding^
                	| 	stubLangExpression^
                    |	stubDescription^
                    |	KEYWORD_SKIP^
                	;*/
  
stubLangExpressionEOFHack: stubLangExpression EOF; /* HACK for testing. See gUnit documentation:
<http://www.antlr.org/wiki/display/ANTLR3/gUnit+-+Grammar+Unit+Testing> */
stubLangExpression	/*: constantOrVoidValueDescription^
					| ifThenElseExpression^
                    | booleanArithmeticExpression^
					| invocation^
                    | runtimeValueIdentExpr^*/
					/*| stubDescription^ /* sequence */
                    /*: conditionalExpression^*/
                    : sequencingExpression^ /* lowest precedence operator */
                    ;

stubLiteralExpression	: STRING_LIT^
						| INT^
                        | FLOAT^
                        | KEYWORD_VOID^
                        | KEYWORD_NULL^
                        | KEYWORD_TRUE^
                        | KEYWORD_FALSE^
                        ;
            
stubPrimitiveExpression	: stubLiteralExpression^
                        | setExpression^
						/*| METAVAR*/
                        | IDENT^
                        | KEYWORD_THIS^
                        | KEYWORD_THAT^
                        | KEYWORD_SUCCESS^
                        /*| memberNameExpr*/
                        | KEYWORD_OUT^ /*memberNameExpr*/ IDENT /* for threading output parameters to function calls */
                        | KEYWORD_OUT^ INDEFINITE_MEMBER_NAME /* ditto */
                        | '('! stubNonSequencingExpression^ ')'!
                        | '{'! sequencingExpression^ ';'!? '}'!
                        ;

memberSelectionOperator : MEMBER_SELECT | INDIRECT_MEMBER_SELECT | ELLIPSIS; /* ellipsis is 'access associated' */

memberSelectionSuffix : memberSelectionOperator^ IDENT ;

/*functionInvocationInfix : '('! ( stubLangExpression (','! stubLangExpression )* )? ')'! 
						;*/
postfixExpressionEOFHack: postfixExpression EOF;
postfixExpression : stubPrimitiveExpression^ ( suffix^ )* /* raising SUFFIX will bring ...                      */
                  ;                                       /* ...a function invocation arglist...                */
                                                          /* up so that the func name (stubPrimitiveExpression) */
                                                          /* sits after it in the same tree node,               */
                                                          /* although I'm not sure why.                         */
                                                          /* Same deal for array subscripting. */
suffix : '(' ( stubLangExpression (',' stubLangExpression )* )? ')' 
         -> ^(INVOKE_WITH_ARGS ^( MULTIVALUE stubLangExpression* ) )
       | memberSelectionOperator IDENT
         -> ^( memberSelectionOperator IDENT )
       | '[' stubLangExpression ']'
         -> ^( ARRAY_SUBSCRIPT stubLangExpression )
       ;

/* Here MULTIPLY is actually unary * (dereference) 
 * and BITWISE_AND is actually unary & (address-of) -- mea culpa */
unaryOperatorExpression	: (COMPLEMENT^|NOT^|MINUS^|PLUS^|MULTIPLY^|KEYWORD_DELETE^|BITWISE_AND^)* postfixExpression
                        | KEYWORD_NEW^ memberNameExpr
                        ;
tieExpression : unaryOperatorExpression ( KEYWORD_TIE^ postfixExpression )? /* "tie" is quite general syntactically, but... */
              ;                                                             /* only semantically valid on pointers.*/
                        
castExpression	: tieExpression valueInterpretation^?
				;
                                        
multiplicativeOperatorExpression	: castExpression ( ( MULTIPLY^ | DIVIDE^ | MODULO^ ) castExpression )*
									;
                                    
additiveOperatorExpression 	: multiplicativeOperatorExpression ( ( PLUS^ | MINUS^ ) multiplicativeOperatorExpression )*
							;
                            
shiftingExpression	: additiveOperatorExpression ( (SHIFT_LEFT^ | SHIFT_RIGHT^ )  additiveOperatorExpression )*
					;
                    
magnitudeComparisonExpression 	: shiftingExpression ( ( LESS^ | GREATER^ | LE^ | GE^ ) shiftingExpression )?
								;
                                
equalityComparisonExpression	: magnitudeComparisonExpression ( ( EQ^ | NEQ^ ) magnitudeComparisonExpression )?
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
						| KEYWORD_IF cond=stubNonSequencingExpression KEYWORD_THEN caseTrue=stubNonSequencingExpression KEYWORD_ELSE caseFalse=stubNonSequencingExpression
                        	-> ^( CONDITIONAL $cond $caseTrue $caseFalse )
                        ;
optionalBindingExpression : binding
                          | conditionalExpression
                          ;
                        
optionalLambdaExpression : KEYWORD_FN^ bindableIdentSet FUNCTION_ARROW! optionalLambdaExpression
                         | optionalBindingExpression^
                         ;
                         
stubNonSequencingExpression : optionalLambdaExpression
                            ;
                        
sequencingExpression : stubNonSequencingExpression ( stubSequenceOperator^ stubNonSequencingExpression )*
                     ;
                     
stubSequenceOperator : SEMICOLON
                     | ANDALSO_THEN
                     | ORELSE_THEN
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

inlineDeclaration			:	KEYWORD_INLINE^ objectSpec wellNestedTokenBlock
							;
                            
wellNestedTokenBlock : '{' ( INT | STRING_LIT | IDENT | 'alias' | 'any' | SEMICOLON | '[' | ',' | ']' | 'exists' | 'check' | 'declare' | 'override' | ':' | '.' | '_' | '->' | 'object' | 'ptr' | '<' | '>' | '=' | 'enum' | 'enumerator' | '==' | '<<' | '>>' | wellNestedTokenBlock | '<-->' | '<--' | '-->' | '#' | '::' | '?' | '+' | '-' | '/' | '|' | '(' | ')' | 'let' )* '}';

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
KEYWORD_INVALID : 'invalid';
KEYWORD_VOID : 'void';
KEYWORD_NULL : 'null';
FUNCTION_ARROW : '=>';
KEYWORD_FN : 'fn';
KEYWORD_BASE : 'base' ;
KEYWORD_OBJECT : 'object';
KEYWORD_PTR : 'ptr';
KEYWORD_CLASS_OF : 'class_of';
KEYWORD_ENUM : 'enum';
KEYWORD_ENUMERATOR : 'enumerator';
KEYWORD_SET : 'set';
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
KEYWORD_IN_AS: 'in_as';
KEYWORD_INTERPRET_AS: 'interpret_as';
KEYWORD_OUT_AS: 'out_as'; 
KEYWORD_CALLER_FREE: 'caller_free';
KEYWORD_IN: 'in';
KEYWORD_OUT: 'out';
KEYWORD_INOUT: 'inout';
KEYWORD_NAMES : 'names';
KEYWORD_PATTERN : 'pattern' ;
KEYWORD_SKIP : 'skip';
KEYWORD_TRUE : 'true';
KEYWORD_FALSE : 'false';
KEYWORD_SUCCESS : 'success';
KEYWORD_NEW: 'new';
KEYWORD_DELETE: 'delete';
KEYWORD_TIE: 'tie';
KEYWORD_THIS: 'this';
KEYWORD_THAT: 'that';
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
INDIRECT_MEMBER_SELECT : '->';
RL_SINGLE_ARROW: '<-';
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
CONT_LBRACE : '--{';
CONT_RBRACE : '}--';
SEMICOLON : ';';
ANDALSO_THEN : ';&';
ORELSE_THEN : ';|';

/* Fallback (interesting) tokens */
INT :   '0'..'9'+ ;
FLOAT : '0'..'9'+ '.''0'..'9'+ ;
NEWLINE:'\r'? '\n' {antlr_m4_newline_action} ;
WS  :   (' '|'\t')+ {antlr_m4_skip_action} ;
LINECOMMENT : '/' '/'( ~ '\n' )* {antlr_m4_skip_action} ;
BLOCKCOMMENT : '/' '*' ( ~ '/' | ( ~ '*' ) '/' )* '*' '/' {antlr_m4_skip_action} ;
STRING_LIT : '\"' ( ~'\"'|'\\\"' )* '\"' ;
//IDENT  :   ('a'..'z'|'A'..'Z'|'_''a'..'z'|'_''A'..'Z'|'_''0'..'9'|'\\'.) /* begin with a-zA-Z or non-terminating '_' */
//(
//	('a'..'z'|'A'..'Z'|'0'..'9'|'\\'.|'_'|'-'|('.'('0'..'9')+))*('a'..'z'|'A'..'Z'|'0'..'9'|'\\'.|'_')
//   /*|('.''0'..'9') /* ending with dot-digit is okay */
//)? ;
/* FIXME: scrapped the fancy IDENT rule until antlr does maximum munch. GRR! */
IDENT : ('a'..'z'|'A'..'Z'|'_'+'a'..'z'|'_'+'A'..'Z'|'_'+'0'..'9')('a'..'z'|'A'..'Z'|'0'..'9'|'_'|'\\'.)*;
PATTERN_IDENT : '/'('a'..'z'|'A'..'Z'|'_''a'..'z'|'_''A'..'Z'|'_''0'..'9')('a'..'z'|'A'..'Z'|'0'..'9'|'_'|'|'|'*'|'('|')'|'.')*'/';
METAVAR	: '@'('a'..'z'|'A'..'Z')('a'..'z'|'A'..'Z'|'0'..'9'|'_')* ;
/* The ident rule is a bit different from the conventional -- idents must be at
 * least two characters, and may embed '-' characters, and '.' preceding a number.
 * (but not at the start or end). 
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
