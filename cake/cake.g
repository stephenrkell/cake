grammar cake;
options {
    output=AST;
    language=Python;
    ASTLabelType=CommonTree; // type of $statement.tree ref etc...
}

/* The whole input */
toplevel:   objectExpr /*{sys.stdout.write($objectExpr.tree.toStringTree() + '\n');} */
        ;

objectExpr      :   atomicExpr
                |   'link'^ objectList linkBody?
                |   'mediate'^ '('! objectExpr ','! objectExpr ')'! mediateBody
                ;
objectList      :   '['! commaSeparatedObjects ']'!
                ;
commaSeparatedObjects	: objectExpr ( ','!? | ','! commaSeparatedObjects )
                		;
linkBody        :   '{'! ( IDENT '<-'^ IDENT ';'! )* ';'!? '}'!
                ;
mediateBody     :   '{'! ( IDENT '<-'^ IDENT ';'! )* ';'!? '}'! // TODO: replace this
                ;
atomicExpr      :   'file'^ FILENAME
                ;

/* Lexer */
IDENT  :   ('a'..'z'|'A'..'Z')('a'..'z'|'A'..'Z'|'0'..'9'|'_')* ;
INT :   '0'..'9'+ ;
NEWLINE:'\r'? '\n' ;
WS  :   (' '|'\t')+ {self.skip();} ;
FILENAME : '\"' ( ~'\"'|'\\\"' )+ '\"';
