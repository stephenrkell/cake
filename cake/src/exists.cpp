#include <gcj/cni.h>
#include <org/antlr/runtime/tree/Tree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#include <cakeJavaLexer.h>
#include <cakeJavaParser.h>
#include <cake/SemanticError.h>
#include <iostream>
#include <vector>
#include <map>
#undef EOF
#include "cake.hpp"

namespace cake
{
	void request::add_exists(const char *module_constructor_name,
		std::string filename,
		std::string module_ident) 
	{
		std::cout << "Told that exists a module, constructor name " << module_constructor_name
			<< " with quoted filename " << filename 
			<< " and module identifier " << module_ident << std::endl;
	}
}
