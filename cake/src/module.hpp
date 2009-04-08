#include <string>
#include <map>

namespace cake
{
	class module
	{
		typedef std::pair<const std::string, const std::string> constructor_map_entry;
		static constructor_map_entry known_constructor_extensions[];		
		static std::map<std::string, std::string> known_constructors;
		
	public:
		static std::string extension_for_constructor(std::string& module_constructor_name)
		{ return known_constructors[module_constructor_name]; }
	};
}
