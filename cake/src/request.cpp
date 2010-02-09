#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
#include "request.hpp"
#include "parser.hpp"
#include "util.hpp"

namespace cake
{
	request::request(const char *filename)
		: 	in_filename(filename),
            in_fileobj(antlr3AsciiFileStreamNew(
                reinterpret_cast<uint8_t*>(
                    const_cast<char*>(filename)))) {}

	int request::process()
	{
        lexer = cakeCLexerNew(in_fileobj);
        tokenStream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
        parser = cakeCParserNew(tokenStream); 
        cakeCParser_toplevel_return ret = parser->toplevel(parser);
        antlr::tree::Tree *tree = ret.tree;
		
		// process the file
		this->ast = tree;
		toplevel();
		
		return 0;
	}
	
	void request::depthFirst(antlr::tree::Tree *t)
	{
		unsigned childCount = GET_CHILD_COUNT(t);
		std::cout 	<< "found a node with " << childCount << " children" << ", "
				 	<< "type: " << GET_TYPE(t) << std::endl;
		for (unsigned i = 0; i < GET_CHILD_COUNT(t); i++)
		{
			depthFirst(reinterpret_cast<antlr::tree::Tree*>(GET_CHILD(t, i)));
		}
		std::cout 	<< "text: " << CCP(GET_TEXT(t)) << std::endl;
		
	}
	
	void request::toplevel()
	{
    	// build our tables
		extract_aliases();	
		//extract_inlines();
		//build_inlines();
		extract_exists();
		extract_supplementary();
		extract_derivations();
		
		// topsort derive dependencies
        
		// output makerules *and* source code for each derive
		for (derivation_tbl_t::iterator pd = derivation_tbl.begin(); 
        	pd != derivation_tbl.end(); pd++)
		{
			(*pd)->write_makerules(std::cout);
		}
	}

	void request::extract_exists()
	{	
		/* Process 'exists' */
		INIT;
		FOR_ALL_CHILDREN(ast)
		{	/* Find all toplevel exists definition */
			SELECT_ONLY(KEYWORD_EXISTS);
            add_exists(n);
        }
    }

	void request::extract_derivations() 
    {
    	INIT;
    	FOR_ALL_CHILDREN(this->ast)
        {
        	SELECT_ONLY(CAKE_TOKEN(KEYWORD_DERIVE));            
            add_derivation(n);
        }
    }
    
	void request::sort_derivations() {}

}
