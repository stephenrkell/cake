#include <string>
#include <java/lang/Exception.h>
#include <java/lang/System.h>
#include <java/io/PrintStream.h>
#include <org/antlr/runtime/ANTLRInputStream.h>
#include <org/antlr/runtime/ANTLRStringStream.h>
#include <org/antlr/runtime/CommonTokenStream.h>
#undef EOF
#include <org/antlr/runtime/CharStream.h>
#include "cake.hpp"
#include "parser.hpp"

namespace cake
{
	request::request(jstring filename)
		: in_filename(filename), 
		  in_fileobj(new java::io::File(in_filename)), 
		  in_file(new java::io::FileInputStream(in_fileobj))
	{
		
	}
	int request::process()
	{
		// invoke the parser
		try
		{
			org::antlr::runtime::ANTLRInputStream *stream = new org::antlr::runtime::ANTLRInputStream(in_file);
			cakeJavaLexer *lexer = new cakeJavaLexer(
				jcast<org::antlr::runtime::CharStream>(stream));
			/*cakeJavaLexer *lexer = new cakeJavaLexer((org::antlr::runtime::CharStream*) stream);
        	/*org::antlr::runtime::CommonTokenStream *tokenStream = new org::antlr::runtime::CommonTokenStream((org::antlr::runtime::TokenSource*) lexer);
        	cakeJavaParser *parser = new cakeJavaParser((org::antlr::runtime::TokenStream*) tokenStream);
        	parser->toplevel();*/
		}
		catch (java::lang::Exception *ex)
		{
			java::lang::System::out->print(ex->toString());
		}
		
		return 0;
	}
}
