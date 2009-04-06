#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <java/lang/Exception.h>
#include <java/lang/System.h>
#include <java/io/PrintStream.h>
#include <org/antlr/runtime/tree/BaseTree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#undef EOF
#include <org/antlr/runtime/CharStream.h>
#include <org/antlr/runtime/IntStream.h>
#include "cake.hpp"
#include "parser.hpp"
#include "util.hpp"
#include "treewalk_helpers.hpp"

namespace cake
{
	void request::pass1_visit_alias_declaration(org::antlr::runtime::tree::Tree *t)
	{
		INIT;
		BIND2(t, aliasDescription);
		BIND3(t, aliasName, IDENT);
		{ 	/* case 1: aliasDescription is KEYWORD_ANY followed by an identList */
			FOR_ALL_CHILDREN(aliasDescription)
			{
				SELECT_ONLY(KEYWORD_ANY);
				{ 
					INIT; 
					FOR_ALL_CHILDREN(n)
					{
						BIND3(t, ident, IDENT);
						module_alias_tbl[CCP(aliasName->getText())].push_back(CCP(ident->getText()));			
					} 
				}
				return;
			} 
		}		

		{ 	/* case 2: aliasDescription is a plain IDENT */
			FOR_ALL_CHILDREN(aliasDescription)
			{
				SELECT_ONLY(IDENT);
				module_alias_tbl[CCP(aliasName->getText())].push_back(CCP(n->getText()));
				return;
			} 
		}		
	}
}
