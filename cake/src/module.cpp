#include <string>
#include <iostream>
#include "cake.hpp"

namespace cake
{
	/* define static members */
	module::constructor_map_entry module::known_constructor_extensions[] = {
			make_pair(std::string("elf_reloc"), std::string("o")),
			make_pair(std::string("elf_external_sharedlib"), std::string("so"))
	};	
	std::map<std::string, std::string> module::known_constructors(
		&module::known_constructor_extensions[0],
		&known_constructor_extensions[(sizeof known_constructor_extensions) 
			/ sizeof (module::constructor_map_entry)
		]
	);

	void elf_module::print_abi_info()
	{
		std::cerr << "Got ABI information for file " << get_filename() << ", " 
			<< info.func_offsets().size() << " function entries, " 
			<< info.toplevel_var_offsets().size() << " toplevel variable entries, "
			<< info.type_offsets().size() << " type entries" << std::endl;
			
		for (std::vector<Dwarf_Off>::iterator i = info.func_offsets().begin();
			i != info.func_offsets().end();
			i++)
		{
			std::cerr << "offset: " << std::hex << *i << std::dec;
			if (info.func_dies()[*i][DW_AT_name] != dwarf::encap::attribute_value::DOES_NOT_EXIST())
			{
				std::cerr << ", name: " << *(info.func_dies()[*i][DW_AT_name].get_string()) << std::endl;
			}
			else
			{
				std::cerr << ", no DW_AT_name attribute" << std::endl;
			}
		}
		for (std::vector<Dwarf_Off>::iterator i = info.toplevel_var_offsets().begin();
			i != info.toplevel_var_offsets().end();
			i++)
		{
			std::cerr << "offset: " << std::hex << *i << std::dec;
			if (info.toplevel_var_dies()[*i][DW_AT_name] != dwarf::encap::attribute_value::DOES_NOT_EXIST())
			{
				std::cerr << ", name: " << *(info.toplevel_var_dies()[*i][DW_AT_name].get_string()) << std::endl;
			}
			else
			{
				std::cerr << ", no DW_AT_name attribute" << std::endl;
			}
		}
		for (std::vector<Dwarf_Off>::iterator i = info.type_offsets().begin();
			i != info.type_offsets().end();
			i++)
		{
			std::cerr << "offset: " << std::hex << *i << std::dec;
			if (info.type_dies()[*i][DW_AT_name] != dwarf::encap::attribute_value::DOES_NOT_EXIST())
			{
				std::cerr << ", name: " << *(info.type_dies()[*i][DW_AT_name].get_string()) << std::endl;
			}
			else
			{
				std::cerr << ", no DW_AT_name attribute" << std::endl;
			}
		}
	}
}
