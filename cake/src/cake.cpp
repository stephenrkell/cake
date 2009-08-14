#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <java/lang/Exception.h>
#include <java/lang/System.h>
#include <java/io/PrintStream.h>
#include <org/antlr/runtime/tree/BaseTree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#include "cake.hpp"
#include "parser.hpp"
#include "util.hpp"
#include "treewalk_helpers.hpp"
#undef EOF
#include <org/antlr/runtime/CharStream.h>
#include <org/antlr/runtime/IntStream.h>

namespace cake
{
	request::request(const char *filename)
		: in_filename(JvNewStringUTF(filename)), 
		  in_fileobj(new java::io::File(in_filename)), 
		  in_file(new java::io::FileInputStream(in_fileobj))/*,
		  ast(build_ast(in_file)),
		  alias(*/
	{
		
	}
	
	int request::process()
	{
		using namespace org::antlr::runtime;
		using namespace java::lang;
		
		// invoke the parser
		stream = new ANTLRInputStream(in_file);
		lexer = new ::cakeJavaLexer((CharStream*) stream);
        tokenStream = new CommonTokenStream((TokenSource*) lexer);
        parser = new ::cakeJavaParser((TokenStream*) tokenStream);
        tree::CommonTree *tree = jcast<tree::CommonTree*>(parser->toplevel()->getTree());
		
		// process the file
		this->ast = tree;
		toplevel();			
		
		return 0;
	}
	
	void request::depthFirst(org::antlr::runtime::tree::Tree *t)
	{
		jint childCount = t->getChildCount();
		std::cout 	<< "found a node with " << (int) childCount << " children" << ", "
				 	<< "type: " << (int) t->getType() << std::endl;
		for (jint i = 0; i < t->getChildCount(); i++)
		{
			depthFirst(t->getChild(i));
		}
		//std::wcout 	<< "node: token: " /*<< JvGetStringChars(t->getToken->getText())*/ << ", " 
		//			<< "type: " /*<< t->getToken->getType()*/ << std::endl;
		std::cout 	<< "text: " << jtocstring_safe (t->getText()) << std::endl;
		
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
