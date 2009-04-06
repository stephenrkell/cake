#include <string>
#include <sstream>

namespace cake
{
	std::string new_anon_ident();	
	std::string new_tmp_filename(const char *module_constructor_name);
}
