include(head.g.m4)

include(dwarfidl-tokens.inc)
tokens { 
dwarfidl_tokens
ENCLOSING; IDENT_LIST; SUPPLEMENTARY; INVOCATION; CORRESP; STUB; EVENT_PATTERN; 
ANNOTATED_VALUE_PATTERN; EVENT_CONTEXT; SET_CONST; CONDITIONAL; TOPLEVEL; OBJECT_CONSTRUCTOR; 
OBJECT_SPEC_DIRECT; OBJECT_SPEC_DERIVING; EXISTS_BODY; REMAINING_MEMBERS; PAIRWISE_BLOCK_LIST; 
EVENT_CORRESP; EVENT_SINK_AS_PATTERN; EVENT_SINK_AS_STUB; KEYWORD_PATTERN; INFIX_STUB_EXPR; 
IDENTS_TO_BIND; VALUE_CONSTRUCT; EVENT_PATTERN_REWRITE_EXPR; RETURN_EVENT; INVOKE_WITH_ARGS; 
EVENT_WITH_CONTEXT_SEQUENCE; CONTEXT_SEQUENCE; EVENT_COUNT_PREDICATE; FORM_ASSOCIATION; 
VALUE_CORRESPONDENCE_REFINEMENT; PATTERN_OF_VALUES; DESCEND_TO_MEMBERS; NAMED_VALUE_CORRESP;
NAME_AND_INTERPRETATION; }

antlr_m4_begin_rules

define(m4_value_description_production,cakeValueDescription)
include(dwarfidl-rules.inc)

/* Cake versions/extensions of dwarfidl */
membershipClaim     : dwarfNamedElement
                    | ELLIPSIS ':' valueDescriptionExpr SEMICOLON
                        -> ^( MEMBERSHIP_CLAIM REMAINING_MEMBERS valueDescriptionExpr )
                    ;

valueInterpretation : KEYWORD_AS^ dwarfType valueConstructionExpression
                    | KEYWORD_OUT_AS^ dwarfType valueConstructionExpression
                    | KEYWORD_INTERPRET_AS^ dwarfType valueConstructionExpression
                    | KEYWORD_IN_AS^ dwarfType valueConstructionExpression
                    ;

valueConstructionExpression : '(' stubNonSequencingExpression ( ',' stubNonSequencingExpression )* ')' 
                              -> ^( VALUE_CONSTRUCT stubNonSequencingExpression* )
                            | -> ^( VALUE_CONSTRUCT )
                            ;
valueIntrinsicAnnotation : KEYWORD_OPAQUE// valueIntrinsicAnnotation?
                         | KEYWORD_IGNORED //valueIntrinsicAnnotation?
                         | KEYWORD_INVALID //valueIntrinsicAnnotation?
                         | KEYWORD_CALLER_FREE^ '('! IDENT ')'! //valueIntrinsicAnnotation?
                         ;

valueModeAnnotation : KEYWORD_OUT -> ^( KEYWORD_OUT) //^ valueAnnotation?
                    | KEYWORD_IN -> ^( KEYWORD_IN ) //^ valueAnnotation?
                    | KEYWORD_INOUT -> ^(KEYWORD_INOUT) //^ valueAnnotation?
                    ;

/* Cake version of dwarfMultiValueDescription */
multiValueDescription	: '(' optionallyNamedWithModeValueDescription (',' optionallyNamedWithModeValueDescription )* ( ',' ELLIPSIS )? ')'
                                -> ^( MULTIVALUE optionallyNamedWithModeValueDescription* ELLIPSIS? )
                                ;
/* used by Cake version */
optionallyNamedWithModeValueDescription: valueModeAnnotation^? optionallyNamedValueDescription
                                    ;
/* ... then resume dwarfidl version */

/* Cake version of dwarfStructuredValueDescription */
structuredValueDescription  : KEYWORD_OBJECT '{' membershipClaim* '}'
                                -> ^(KEYWORD_OBJECT membershipClaim* )
                            ;
/* the multiValueDescription for tuples: must have names! */
namedMultiValueDescription : '(' namedValueDescription (',' namedValueDescription )*  ')'
                               -> ^( MULTIVALUE namedValueDescription* )
                           ;

/* Cake version of dwarfValueDescription */
cakeValueDescription: primitiveOrFunctionValueDescription
                    ;
primitiveOrFunctionValueDescription	: 
    | (multiValueDescription FUNCTION_ARROW)=> 
       multiValueDescription FUNCTION_ARROW primitiveOrFunctionValueDescription 
           -> ^(FUNCTION_ARROW multiValueDescription primitiveOrFunctionValueDescription )
    | primitiveValueDescription
    ;
/* Cake version of dwarfPrimitiveValueDescription -- has interpretations, annotations
 -- and we hack the Cake extended structuredValueDescription in here too */
primitiveValueDescription   : dwarfConstantValueDescription
                         /*   | (valueIntrinsicAnnotation structuredValueDescription)=>
                              valueIntrinsicAnnotation structuredValueDescription valueInterpretation? */
                            | annotatedDwarfPrimitiveValueDescription valueInterpretation?
                            ;
/* HACK: for now we don't seem to support opaque/ignored structure members... because
of the above commenting-out of our Cake extension of dwarfStructuredValueDescription. FIXME! */
annotatedDwarfPrimitiveValueDescription: dwarfType
                                       | valueIntrinsicAnnotation^ annotatedDwarfPrimitiveValueDescription
                                       ;
                           
/* Cake only */
setExpression           : KEYWORD_SET '[' ( stubLangExpression ( ',' stubLangExpression* )* )? ']' -> ^( KEYWORD_SET stubLangExpression* )
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
                                | (IDENT ':' KEYWORD_VALUES '{')=> /* named form -- FIXME: do we need this? */
                                    IDENT ':' valueCorrespondenceBlock
                                    -> ^( NAMED_VALUE_CORRESP valueCorrespondenceBlock IDENT ) /* FIXME */
                                | (KEYWORD_VALUES '{')=>  /* ordinary form */
                                    valueCorrespondenceBlock
                                | singleValueCorrespondence /* ordinary form */
                                | IDENT ':' singleValueCorrespondence /* named form */
                                   -> ^( NAMED_VALUE_CORRESP singleValueCorrespondence IDENT )
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
	eventContext memberNameExpr eventParameterNamesAnnotation ( '(' ( ( annotatedValueBindingPattern ( ',' annotatedValueBindingPattern )* (',' ELLIPSIS )? ) | ELLIPSIS )? ')' eventCountPredicate? )?
		-> ^( EVENT_PATTERN eventContext memberNameExpr ^( EVENT_COUNT_PREDICATE eventCountPredicate? ) eventParameterNamesAnnotation annotatedValueBindingPattern* ELLIPSIS? )
    | eventContext identPattern eventParameterNamesAnnotation ( '(' ( ( annotatedValueBindingPattern ( ',' annotatedValueBindingPattern )* (',' ELLIPSIS )? ) | ELLIPSIS )? ')' eventCountPredicate? )?
		-> ^( EVENT_PATTERN eventContext identPattern ^( EVENT_COUNT_PREDICATE eventCountPredicate? ) eventParameterNamesAnnotation annotatedValueBindingPattern* ELLIPSIS? )
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
                      -> ^( NAME_AND_INTERPRETATION ^( memberNameExpr ^( ARRAY_SUBSCRIPT constantIntegerArithmeticExpression ) valueInterpretation? ) )
                    | memberNameExpr valueInterpretation? /* matches a named constant value -- also matches '_' */
                      -> ^( NAME_AND_INTERPRETATION memberNameExpr valueInterpretation? )
					/*| METAVAR^ */ /* matches any value, and names it */
					| constantValueDescription -> ^( KEYWORD_CONST constantValueDescription ) /* matches that constant */
                    | KEYWORD_VOID^ /* used for "no value" correspondences & inserting "arbitrary" code */
                    | KEYWORD_THIS^ /* only for correspondences nested in refinement blocks -- see ephy.cake */
                    | KEYWORD_PATTERN PATTERN_IDENT /* for use in pattern-based value corresps */
                     -> ^( KEYWORD_PATTERN PATTERN_IDENT )
                    | KEYWORD_PATTERN patternConstantValueDescription /* this does n..n and string regexps */
                     -> ^( PATTERN_OF_VALUES patternConstantValueDescription )
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
            
bindableIdentSet 	: 
                       /* bindableIdentWithOptionalInterpretation 
                        -> ^( IDENTS_TO_BIND bindableIdentWithOptionalInterpretation )
					|*/ '(' bindableIdentWithOptionalInterpretation ( ',' bindableIdentWithOptionalInterpretation )+ ')' 
                        -> ^( IDENTS_TO_BIND bindableIdentWithOptionalInterpretation* )
/*                    | lhs=IDENT ELLIPSIS rhs=IDENT
                        -> ^( FORM_ASSOCIATION $lhs $rhs )*/ /* subsumed by postfixExpression */
                   /* | IDENT ELLIPSIS
                   //     -> ^( DESCEND_TO_MEMBERS IDENT ) */
                    | postfixExpression valueInterpretation? /* I give in -- mutation is okay */
                    ;

bindableIdentWithOptionalInterpretation: IDENT valueInterpretation? -> ^( IDENT valueInterpretation? )
                                       ;
            
bindingKeyword : KEYWORD_LET | KEYWORD_OUT | KEYWORD_SET ;
            
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
     valuePattern infixStubExpression leftToRightCorrespondenceOperator^ infixStubExpression /*restrictedPostfixExpression*/memberNameExpr
  | (/*restrictedPostfixExpression*/memberNameExpr infixStubExpression rightToLeftCorrespondenceOperator)=>
    /*restrictedPostfixExpression*/memberNameExpr infixStubExpression rightToLeftCorrespondenceOperator^ infixStubExpression /*annotatedValueBindingPattern*/ valuePattern
  | (valuePattern infixStubExpression bidirectionalCorrespondenceOperator)=>
    valuePattern infixStubExpression bidirectionalCorrespondenceOperator^ infixStubExpression valuePattern
  | (singleOrNamedMultiValueDescription bidirectionalCorrespondenceOperator)=>
    singleOrNamedMultiValueDescription bidirectionalCorrespondenceOperator^ singleOrNamedMultiValueDescription
  ;

/* NOTE: we had stubNonSequencingExpression for a reason: to be able to write
 * field rules like
 *
 *  field1 + field2 --> fieldA; 
 *
 * ... which now we can't do. 
 * This is because the grammar is ambiguous: if we wrote
 *
 *   field1 + field2 (infixStub) --> field A; 
 * 
 * ... then it's not clear whether we have an invocation
 * or an infix stub. 
 * How to fix this?
 * We could force programmers to write
 * 
 * void (field1 + field2) --> fieldA;
 *
 * ... but then "void" is really unintuitive.
 * We could dispense with infix stubs, but then
 *
 * fieldX ({lock(field); that})--> fieldA;
 * 
 * ... would have to be expressed like so:
 *
 * { lock(&fieldX); fieldX }    --> fieldA;
 * { unlock(&fieldX); fieldA } <--  fieldA;
 *
 * ... Is this so bad? Is this even a sensible example ( / locking strategy)?
 * What about aliasing in this locking example?
 * e.g. struct { obj *my_obj}; <--> struct { obj *should_lock; int blah; }
 * ... here we are fine *unless* another alias for the lock exists
 * in some co-object... then each sync will attempt to lock/unlock *multiple* times.
 * The real way to solve this is to put the locking on the corresp for the
 * object itself, not some pointer to it.
 * And actually we might want this in p2k, because p2k cookies are actually
 * pointers -- something we have been finessing until now.
 * Maybe the right solution is to allow corresps for opaque types?! YES.
 * Instances of opaque types have identities, but no contents.
 * So we use malloc(0) for them?! YES. Means we require a libc that supports malloc(0) properly.
 * The opaque types just can't define any field corresps or whatnot.
 * 
 * It's going to get even more interesting when we consider
 * sharing objects that we want to insert locking calls for.*/

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
                        | KEYWORD_HERE^
                        | KEYWORD_THERE^
                        | KEYWORD_SUCCESS^
                        /*| memberNameExpr*/
                        | KEYWORD_OUT^ /*memberNameExpr*/ IDENT /* for threading output parameters to function calls */
                        | KEYWORD_OUT^ INDEFINITE_MEMBER_NAME /* ditto */
                        | '('! stubNonSequencingExpression^ ')'!
                        | '{'! sequencingExpression^ ';'!? '}'!
                        | KEYWORD_IN_ARGS^
                        | KEYWORD_OUT_ARGS^
                        ;

memberSelectionOperator : MEMBER_SELECT | INDIRECT_MEMBER_SELECT | ELLIPSIS; /* ellipsis is 'access associated' */

memberSelectionSuffix : memberSelectionOperator IDENT 
         -> ^( memberSelectionOperator IDENT )
                      ;

arrayIndexingSuffix: '[' stubLangExpression ']'
         -> ^( ARRAY_SUBSCRIPT stubLangExpression )
         ;

/*functionInvocationInfix : '('! ( stubLangExpression (','! stubLangExpression )* )? ')'! 
						;*/
postfixExpressionEOFHack: postfixExpression EOF;
postfixExpression : stubPrimitiveExpression^ ( suffix^ )* ELLIPSIS? /* raising SUFFIX will bring ...                      */
                  ;                                       /* ...a function invocation arglist...                */
                                                          /* up so that the func name (stubPrimitiveExpression) */
                                                          /* sits after it in the same tree node,               */
                                                          /* although I'm not sure why.                         */
                                                          /* Same deal for array subscripting. */
suffix : '(' ( stubLangExpression (',' stubLangExpression )* )? ')' 
         -> ^(INVOKE_WITH_ARGS ^( MULTIVALUE stubLangExpression* ) )
/*       | memberSelectionOperator IDENT
         -> ^( memberSelectionOperator IDENT )*/
       | memberSelectionSuffix
/*       | '[' stubLangExpression ']'
         -> ^( ARRAY_SUBSCRIPT stubLangExpression )*/
       | arrayIndexingSuffix
       ;

/* These "restricted" ones are for value corresp left-/right-hand sides.
 * We don't want to include function invocations because they lead the
 * syntactic predicates the wrong way when we have infix stub expressions --
 * the brackets get misparsed as function call brackets. */
restrictedSuffix: memberSelectionSuffix
       | arrayIndexingSuffix
       ;

restrictedPostfixExpression: stubPrimitiveExpression^ ( restrictedSuffix^ )* ELLIPSIS?
                          ;


/* Here MULTIPLY is actually unary * (dereference) 
 * and BITWISE_AND is actually unary & (address-of) -- mea culpa */
unaryOperatorExpression	: (COMPLEMENT^|NOT^|MINUS^|PLUS^|MULTIPLY^|KEYWORD_DELETE^|BITWISE_AND^)* postfixExpression
                        | KEYWORD_NEW^ memberNameExpr namedMultiValueDescription?
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

/* Named tokens */
include(dwarfidl-abbrev.inc)

/* extra boilerplate tokens for Cake */
KEYWORD_ALIAS : 'alias';
KEYWORD_ANY : 'any';
KEYWORD_DERIVING : 'deriving';
KEYWORD_EXISTS : 'exists';
KEYWORD_STATIC : 'static' ;
KEYWORD_CHECK : 'check';
KEYWORD_DECLARE : 'declare';
KEYWORD_OVERRIDE : 'override';
KEYWORD_OPAQUE : 'opaque';
KEYWORD_IGNORED : 'ignored';
KEYWORD_INVALID : 'invalid';
KEYWORD_FN : 'fn';
KEYWORD_LINK : 'link';
KEYWORD_DERIVE : 'derive';
BI_DOUBLE_ARROW : '<-->';
RL_DOUBLE_ARROW : '<--';
LR_DOUBLE_ARROW : '-->';
SCOPE_RESOLUTION : '::';
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
KEYWORD_SUCCESS : 'success';
KEYWORD_NEW: 'new';
KEYWORD_DELETE: 'delete';
KEYWORD_TIE: 'tie';
KEYWORD_THIS: 'this';
KEYWORD_THAT: 'that';
KEYWORD_HERE: 'here';
KEYWORD_THERE: 'there';
KEYWORD_IN_ARGS: 'in_args';
KEYWORD_OUT_ARGS: 'out_args';
KEYWORD_IF : 'if';
KEYWORD_THEN : 'then';
KEYWORD_EMIT : 'emit';
KEYWORD_VALUES : 'values';
KEYWORD_ELSE : 'else';
LR_DOUBLE_ARROW_Q : '-->?' ;
RL_DOUBLE_ARROW_Q : '<--?' ;
KEYWORD_INLINE : 'inline';
MEMBER_SELECT : '.';
INDIRECT_MEMBER_SELECT : '->';
RL_SINGLE_ARROW: '<-';
CONT_LBRACE : '--{';
CONT_RBRACE : '}--';
ANDALSO_THEN : ';&';
ORELSE_THEN : ';|';

/* Lexer */
include(dwarfidl-lex.inc)

