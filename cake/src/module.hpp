#include <string>
#include <map>

namespace cake
{
	class module
	{
		static std::pair<const std::string, const char *> known_constructor_extensions[];		
		static std::map<std::string, const char *> known_constructors;
		
	public:
		static std::string extension_for_constructor(const char *module_constructor_name)
		{ return known_constructors[module_constructor_name]; }
	};
}
