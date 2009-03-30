grammar cake;
options {
    output=AST;
    language=Python;
    ASTLabelType=CommonTree; // type of $statement.tree ref etc...
}

/* The whole input */
toplevel:   ( statement {sys.stdout.write($statement.tree.toStringTree());} )+ ;

/* A statement, terminated by newline */
statement   :   expr NEWLINE        -> expr // just discard the newline
            |   IDENT '=' expr NEWLINE -> ^('=' IDENT expr) // `^' begins a tree element expression
            |   NEWLINE             -> // ignore lone newlines
            ;

/* An expression is a sum of products */
expr    :   multExpr (('+'^|'-'^) multExpr)*
        ; 

/* A product is a list of one or more multiplied atoms */
multExpr    :   atom ('*'^ atom)*
            ; 

/* An atom is an integer, an identifier or a bracketed expression */
atom:   INT 
    |   IDENT
    |   '('! expr ')'!
    ;

/* Lexer */
IDENT  :   ('a'..'z'|'A'..'Z')('a'..'z'|'A'..'Z'|'0'..'9'|'_')* ;
INT :   '0'..'9'+ ;
NEWLINE:'\r'? '\n' ;
WS  :   (' '|'\t')+ {self.skip();} ;
FILENAME : 
