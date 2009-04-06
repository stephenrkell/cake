#include <string>
#include <sstream>
#include "cake.hpp"
#include "module.hpp"
#include "treewalk_helpers.hpp"

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
		s << "tmp_" << ctr++ << module::extension_for_constructor(module_constructor_name);
		return s.str();
	}
	
	std::ostringstream exception_msg_stream("");
	
	std::string unescape_ident(std::string& ident)
	{
		return std::string();
	}
	std::string unescape_string_lit(std::string& lit)
	{	
		return std::string();
	}
	
	std::pair<std::string, std::string> read_object_constructor(org::antlr::runtime::tree::Tree *t)
	{
		std::string snd();
		INIT;
		BIND3(t, id, IDENT);
		std::string fst(CCP(fst));
		if (t->getChildCount() > 1) 
		{
			BIND3(t, quoted_lit, STRING_LIT);
			snd = std::string(CCP(quoted_lit));
		}
		return std::make_pair(fst, snd);
	}
}
