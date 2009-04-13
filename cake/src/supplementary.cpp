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
#include "util.hpp"
#include "treewalk_helpers.hpp"

namespace cake
{
	void request::extract_supplementary() 
	{
		INIT;
		FOR_ALL_CHILDREN(ast)
		{
			SELECT_ONLY(cakeJavaParser::SUPPLEMENTARY);
		}		
	}
}	
	
