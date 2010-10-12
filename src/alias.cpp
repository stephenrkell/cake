#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include "request.hpp"
#include "parser.hpp"
#include "util.hpp"
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
			
			std::cerr << "Processing alias definition: " << CCP(TO_STRING_TREE(n)) << std::endl;
			
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

	void request::pass1_visit_alias_declaration(antlr::tree::Tree *t)
	{
		INIT;
		BIND2(t, aliasDescription);
		BIND3(t, aliasName, IDENT);
		
		switch (GET_TYPE(aliasDescription))
		{
			case CAKE_TOKEN(KEYWORD_ANY): { 	
				/* case 1: aliasDescription is KEYWORD_ANY followed by an identList */
				INIT;
				BIND3(aliasDescription, identList, CAKE_TOKEN(IDENT_LIST)); 
				FOR_ALL_CHILDREN(identList)
				{
					ALIAS3(n, ident, IDENT);
					module_alias_tbl[CCP(GET_TEXT(aliasName))].push_back(CCP(GET_TEXT(ident)));
				} 
			} return;
			
			case CAKE_TOKEN(IDENT):
				/* case 2: aliasDescription is a plain IDENT */
				module_alias_tbl[CCP(GET_TEXT(aliasName))].push_back(CCP(GET_TEXT(aliasName)));
				return;
				
			default: 
				SEMANTIC_ERROR(aliasDescription);
		}		
	}
}
