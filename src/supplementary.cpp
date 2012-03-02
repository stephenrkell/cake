//#include <gcj/cni.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cake/cakeJavaLexer.h>
// #include <cake/cakeJavaParser.h>
// #include <cake/SemanticError.h>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "request.hpp"
#include "util.hpp"
//#include "treewalk_helpers.hpp"
#include "parser.hpp"
#include "module.hpp"

using std::string;
using std::vector;
using std::cerr;
using std::endl;
using std::set;

namespace cake
{
	void request::extract_supplementary(const string& target_module) 
	{
		FOR_ALL_CHILDREN(ast)
		{	/* Find all toplevel supplementary definition */
			SELECT_ONLY(SUPPLEMENTARY);
			INIT;
			BIND3(n, moduleName, IDENT); // name given for module(s) we are supplementing
			// use alias table to find the list of actual modules
			string module_name = CCP(GET_TEXT(moduleName));

			auto vec = module_alias_tbl[module_name];
			set<string> s;
			std::copy(vec.begin(), vec.end(), std::inserter(s, s.begin()));

			if (module_tbl.find(module_name) != module_tbl.end()) s.insert(module_name);
			
			if (s.find(target_module) != s.end())
			{

					//(module_alias_tbl.find(string(CCP(moduleName->getText()))) == module_alias_tbl.end())
					//	? string(CCP(moduleName->getText()))
					//	: module_alias_tbl[string(CCP(moduleName->getText()))];

				cerr << "Processing a supplementary block concerning modules aliased by "
					<< CCP(GET_TEXT(moduleName)) << ", count: " << s.size() << endl;
				cerr << "Block AST: " << CCP(TO_STRING_TREE(n));
				cerr << "Block child count: " << (int) GET_CHILD_COUNT(n) << endl;
					//<< CCP(moduleName->getText()) << ", count: " << s.size() << endl;

				FOR_REMAINING_CHILDREN(n)
				{
					// remaining children are claimgroups
					cerr << "Found supplementary claim group: " << CCP(TO_STRING_TREE(n)) << endl;

					// process each claim separately for each aliased module, 
					for (set<string>::iterator i = s.begin();
						i != s.end();
						i++)
					{
						string ident = *i;
						cerr << "Applying supplementary claim to module " << ident << endl;

						module_tbl[ident]->process_supplementary_claim(n);
					}
				}
	/*			antlr::tree::Tree *parent = n;
				FOR_ALL_CHILDREN(parent)
				{
					cerr << "The parent is: " << CCP(parent->toStringTree());
					cerr << "The child is : " << CCP(n->toStringTree());

				}	*/
			}		
		}
	} // end request::extract_supplementary()
} // end namespace cake
	
