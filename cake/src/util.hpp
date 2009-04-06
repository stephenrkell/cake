#include <string>
#include <sstream>

namespace cake
{
	std::string new_anon_ident();	
	std::string new_tmp_filename(const char *module_constructor_name);
	extern std::ostringstream exception_msg_stream;
	std::string unescape_ident(std::string& ident);
	std::string unescape_string_lit(std::string& lit);
	std::pair<std::string, std::string> read_object_constructor(org::antlr::runtime::tree::Tree *t);
}
