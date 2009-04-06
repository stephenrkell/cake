#include <string>
#include <iostream>
#include "cake.hpp"
#include "module.hpp"

namespace cake
{
	/* define static members */	
	std::pair<const std::string, const char *> module::known_constructor_extensions[] = {
			make_pair(std::string("elf_reloc"), "o"),
			make_pair(std::string("elf_external_sharedlib"), "so")
	};	
	std::map<std::string, const char *> module::known_constructors(
		&module::known_constructor_extensions[0],
		&known_constructor_extensions[(sizeof known_constructor_extensions) 
			/ sizeof (std::pair<const std::string, const char *>)
		]
	);
}
