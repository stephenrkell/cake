#include <string>
#include <sstream>
#include "cake.hpp"
#include "module.hpp"

namespace cake
{
	std::string new_anon_ident()
	{
		static int ctr = 0;
		std::ostringstream s;
		s << "anon_" << ctr++;
		return s.str();
	}
	
	std::string new_tmp_filename(const char *module_constructor_name)
	{
		static int ctr = 0;
		std::ostringstream s;
		s << "tmp_" << ctr++ << "." << module::extension_for_constructor(module_constructor_name);
		return s.str();
	}
	
	std::ostringstream exception_msg_stream;
}
