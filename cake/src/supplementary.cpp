//#include <gcj/cni.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cake/cakeJavaLexer.h>
// #include <cake/cakeJavaParser.h>
// #include <cake/SemanticError.h>
#include <iostream>
#include <vector>
#include <map>
#include "request.hpp"
#include "util.hpp"
//#include "treewalk_helpers.hpp"
#include "parser.hpp"

namespace cake
{
	void request::extract_supplementary() 
	{
		FOR_ALL_CHILDREN(ast)
		{	/* Find all toplevel supplementary definition */
			SELECT_ONLY(SUPPLEMENTARY);
			INIT;
			BIND3(n, moduleName, IDENT); // name given for module(s) we are supplementing
			// use alias table to find the list of actual modules
			std::vector<std::string>& s = module_alias_tbl[std::string(CCP(moduleName->getText()))];
				//(module_alias_tbl.find(std::string(CCP(moduleName->getText()))) == module_alias_tbl.end())
				//	? std::string(CCP(moduleName->getText()))
				//	: module_alias_tbl[std::string(CCP(moduleName->getText()))];
				
			std::cerr << "Processing a supplementary block concerning modules aliased by "
				<< CCP(moduleName->getText()) << ", count: " << s.size() << std::endl;
			std::cerr << "Block AST: " << CCP(n->toStringTree());
			std::cerr << "Block child count: " << (int) n->getChildCount() << std::endl;
				//<< CCP(moduleName->getText()) << ", count: " << s.size() << std::endl;
			
			FOR_REMAINING_CHILDREN(n)
			{
				// remaining children are claimgroups
				std::cerr << "Found supplementary claim group: " << CCP(n->toStringTree()) << std::endl;
				
				// process each claim separately for each aliased module, 
				for (std::vector<std::string>::iterator i = s.begin();
					i != s.end();
					i++)
				{
					std::string& ident = *i;
					std::cerr << "Applying supplementary claim to module " << ident << std::endl;

					module_tbl[ident]->process_supplementary_claim(n);
				}
			}
/*			org::antlr::runtime::tree::Tree *parent = n;
			FOR_ALL_CHILDREN(parent)
			{
				std::cerr << "The parent is: " << CCP(parent->toStringTree());
				std::cerr << "The child is : " << CCP(n->toStringTree());
			
			}	*/		
		}
	} // end request::extract_supplementary()
} // end namespace cake
	
