namespace org { namespace antlr { namespace runtime { namespace tree { class Tree; } } } }
namespace antlr = org::antlr::runtime;

#include "module.hpp"
//#include <gcj/cni.h>
//#include <org/antlr/runtime/tree/Tree.h>
//#include "cake/SemanticError.h"

namespace cake
{
	ifstream_holder::ifstream_holder(std::string& filename) : this_ifstream(filename.c_str(), std::ios::in) 
	{
		if (!this_ifstream) 
		{ 
			//throw new SemanticError(
			//	0, 
			//	JvNewStringUTF(
			//		"file does not exist! ")->concat(
			//		JvNewStringUTF(filename.c_str())));
            throw std::string("file does not exist! ") + filename;
		}
	}
}
