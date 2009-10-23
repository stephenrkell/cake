namespace org { namespace antlr { namespace runtime { namespace tree { class Tree; } } } }
namespace antlr = org::antlr::runtime;

#include "module.hpp"

/* This translation unit exists only to separate out problematic
 * C++ exception handling to avoid g++'s 
 * "mixing C++ and Java catches in a single translation unit" errors. */

namespace cake
{
	module::module(std::string& filename) : filename(filename), debug_out(&debug_outbuf) 
	{
		debug_outbuf.push(debug_out_filter);
		debug_outbuf.push(std::cerr);
	}
}
