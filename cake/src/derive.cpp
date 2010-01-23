//#include <gcj/cni.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cake/cakeJavaLexer.h>
// #include <cake/cakeJavaParser.h>
// #include <cake/SemanticError.h>
#include <iostream>
#include <vector>
#include <map>
#include "parser.hpp"
#include "request.hpp"


namespace cake
{
	void request::add_derive_rewrite(std::string& derived_ident,
		std::string& filename_text,
		antlr::tree::Tree *derive_body) 
	{
		std::cout << "Asked to derive a module name " << derived_ident
			<< " with filename " << filename_text << " defined by tree at " << derive_body << std::endl;
	}
}
