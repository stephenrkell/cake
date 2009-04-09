#include <string>
#include <sstream>

namespace cake
{
	extern const char *guessed_system_library_path;
	extern const char *guessed_system_library_prefix;
	std::string new_anon_ident();	
	std::string new_tmp_filename(std::string& module_constructor_name);
	extern std::ostringstream exception_msg_stream;
	std::string unescape_ident(std::string& ident);
	std::string unescape_string_lit(std::string& lit);
	std::pair<std::string, std::string> read_object_constructor(org::antlr::runtime::tree::Tree *t);
	std::string lookup_shared_lib(std::string const& basename);
	extern std::string shared_lib_constructor;
}
