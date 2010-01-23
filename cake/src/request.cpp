#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
// #include <java/lang/Exception.h>
// #include <java/lang/System.h>
// #include <java/io/PrintStream.h>
// #include <java/lang/Throwable.h>
// #include <java/lang/ClassLoader.h>
// #include <java/lang/ClassCastException.h>
// #include <org/antlr/runtime/tree/BaseTree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
#include "request.hpp"
#include "parser.hpp"
#include "util.hpp"
//#include "treewalk_helpers.hpp"
// #undef EOF
// //#include <org/antlr/runtime/CharStream.h>
// //#include <org/antlr/runtime/IntStream.h>
// #include <cake/CakeParserHelper.h>

namespace cake
{
	request::request(char *filename)
		: //in_filename(JvNewStringUTF(filename)), 
        	in_filename(filename),
            in_fileobj(antlr3AsciiFileStreamNew(reinterpret_cast<uint8_t*>(filename)))
		  //in_fileobj(new java::io::File(in_filename)), 
		  //in_file(new java::io::FileInputStream(in_fileobj))
	{
		
	}
	
	int request::process()
	{
		//using namespace org::antlr::runtime;
		//using namespace java::lang;
        
		// invoke the parser
		//stream = new ANTLRInputStream(in_file);
        //stream = 
		//lexer = new cakeJavaLexer((CharStream*) stream);
        lexer = cakeCLexerNew(in_fileobj);
        //tokenStream = new CommonTokenStream((TokenSource*) lexer);
        tokenStream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
        parser = cakeCParserNew(tokenStream); //new cakeJavaParser((TokenStream*) tokenStream);
        //tree::CommonTree *tree = jcast<tree::CommonTree*>(parser->toplevel()->getTree());
        cakeCParser_toplevel_return ret = parser->toplevel(parser);
        antlr::tree::Tree *tree = ret.tree;
		
		// process the file
		this->ast = tree;
		toplevel();
		
		return 0;
	}
	
	void request::depthFirst(antlr::tree::Tree *t)
	{
		unsigned childCount = t->getChildCount(t);
		std::cout 	<< "found a node with " << childCount << " children" << ", "
				 	<< "type: " << (int) t->getType(t) << std::endl;
		for (/*jint*/ unsigned i = 0; i < t->getChildCount(t); i++)
		{
			depthFirst(reinterpret_cast<antlr::tree::Tree*>(t->getChild(t, i)));
		}
		//std::wcout 	<< "node: token: " /*<< JvGetStringChars(t->getToken->getText())*/ << ", " 
		//			<< "type: " /*<< t->getToken->getType()*/ << std::endl;
		std::cout 	<< "text: " << /*jtocstring_safe (*/t->getText(t)/*)*/ << std::endl;
		
	}

	
	/* FIXME: what's the right way to do this? tree parsers? Visitors? hand-crafted? */
	void request::toplevel()
	{
		extract_aliases();	
		//extract_inlines();
		//build_inlines();
		extract_exists();
		extract_supplementary();
		extract_derivations();
		
		// derivations may have to happen in some order -- that doesn't mean
		// we have to process them in that order, although it might if we
		// end up supporting a derivation algebra (see below) since we might
		// have to compute an "exists" block for intermediate results.
		compute_derivation_dependencies();
		
		for (//each derive request...
		/// ... output the Make rules that will build it
		derivation *pd = 0; pd != 0; )
		{
			pd->write_makerules(std::cout);
		}
	}
		
	// there are three kinds of derivation (derivation function) at the moment:
	// make_exec builds an executable out of an object file
	// link builds an object file out of a list of object files
	// rewrite performs substitutions on a single object file
	// ... and they do *not* form a general derivation algebra (yet).
	
	void request::extract_derivations() {}
		
		// derivations may have to happen in some order -- that doesn't mean
		// we have to process them in that order, although it might if we
		// end up supporting a derivation algebra (see below) since we might
		// have to compute an "exists" block for intermediate results.
	void request::compute_derivation_dependencies() {}

}
