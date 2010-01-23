#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
// #include <java/lang/Exception.h>
// #include <java/lang/System.h>
// #include <java/io/PrintStream.h>
// #include <org/antlr/runtime/tree/BaseTree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
#include "request.hpp"
#include "parser.hpp"
#include "util.hpp"
//#include "treewalk_helpers.hpp"
//#undef EOF
// #include <org/antlr/runtime/CharStream.h>
// #include <org/antlr/runtime/IntStream.h>

namespace cake
{
	void request::extract_aliases()
	{	
		antlr::tree::Tree *t = (antlr::tree::Tree *) ast;
		/* Process aliases */
		INIT;
		FOR_ALL_CHILDREN(t)
		{	/* Find all the toplevel alias definitions */
			SELECT_ONLY(KEYWORD_ALIAS);
			
			std::cerr << "Processing alias definition: " << CCP(n->toStringTree()) << std::endl;
			
			/* Create an alias record for this alias. */
			module_alias_tbl.insert(std::make_pair(
 				std::string(text),
 				std::vector<std::string>()
 				)
 			);
			/* Process the alias definition body. */
			pass1_visit_alias_declaration(n);
		}
		/* FIXME: check for cycles in the alias graph */
	}

	void request::pass1_visit_alias_declaration(org::antlr::runtime::tree::Tree *t)
	{
		INIT;
		BIND2(t, aliasDescription);
		BIND3(t, aliasName, IDENT);
		
		switch (aliasDescription->getType())
		{
			case cakeJavaParser::KEYWORD_ANY: { 	
				/* case 1: aliasDescription is KEYWORD_ANY followed by an identList */
				INIT;
				BIND3(aliasDescription, identList, cakeJavaParser::IDENT_LIST); 
				FOR_ALL_CHILDREN(identList)
				{
					ALIAS3(n, ident, IDENT);
					module_alias_tbl[CCP(aliasName->getText())].push_back(CCP(ident->getText()));			
				} 
			} return;
			
			case cakeJavaParser::IDENT:
				/* case 2: aliasDescription is a plain IDENT */
				module_alias_tbl[CCP(aliasName->getText())].push_back(CCP(aliasName->getText()));
				return;
				
			default: 
				SEMANTIC_ERROR(aliasDescription);
		}		
	}
}
