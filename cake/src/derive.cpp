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
	void request::add_derive_rewrite(std::string& derived_ident,
		std::string& filename_text,
		org::antlr::runtime::tree::Tree *derive_body) 
	{
		std::cout << "Asked to derive a module name " << derived_ident
			<< " with filename " << filename_text << " defined by tree at " << derive_body << std::endl;
	}
}
