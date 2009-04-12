#include <gcj/cni.h>
#include <string>
#include <cassert>
#include <iostream>
#include "cake.hpp" // includes module.hpp
#include "util.hpp"
#include "treewalk_helpers.hpp"

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
	
	void module::process_exists_claims(antlr::tree::Tree *existsBody)
	{
		FOR_ALL_CHILDREN(existsBody)
		{
			INIT;
			SELECT_NOT(LR_DOUBLE_ARROW); // we don't want rewrites, only claimGroups
			process_claimgroup(n);
		}
	}
	
	void module::process_claimgroup(antlr::tree::Tree *claimGroup)
	{
		INIT;
		module::claim_strength str;
		switch(claimGroup->getType())
		{
			case cakeJavaParser::KEYWORD_CHECK:
				str = CHECK;
				goto call_process_claim;
			case cakeJavaParser::KEYWORD_DECLARE:
				str = DECLARE;
				goto call_process_claim;
			case cakeJavaParser::KEYWORD_OVERRIDE:
				str = OVERRIDE;
				goto call_process_claim;
			call_process_claim: {
				process_claim_list(str, claimGroup);
			} break;
			default: RAISE_INTERNAL(claimGroup, "bad claim strength (expected `check', `declare' or `override')");
		}			
	}
	
	void elf_module::process_claim_list(claim_strength s, antlr::tree::Tree *claimGroup)
	{
		std::cerr << "Presented with a claim list of strength " << (
				(s == CHECK) ? "CHECK"
			:	(s == DECLARE) ? "DECLARE"
			:	(s == OVERRIDE) ? "OVERRIDE"
			:	"(unrecognised)") << std::endl;
		assert(claimGroup->getType() == cakeJavaParser::KEYWORD_CHECK
		|| claimGroup->getType() == cakeJavaParser::KEYWORD_DECLARE
		|| claimGroup->getType() == cakeJavaParser::KEYWORD_OVERRIDE);
		
		FOR_ALL_CHILDREN(claimGroup)
		{
			INIT;
			ALIAS2(n, memberName);
			definite_member_name name;
			switch(memberName->getType())
			{
				case '_':
					std::cerr << "Claim concerns all remaining members" << std::endl;
				break;
				case cakeJavaParser::DEFINITE_MEMBER_NAME: {
					definite_member_name list = read_definite_member_name(memberName);
					std::cerr << "Claim concerns member ";
					for (definite_member_name::iterator i = list.begin(); i != list.end(); i++)
					{
						std::cerr << *i;
						if (i + 1 != list.end()) std::cerr << " :: ";
					}
					std::cerr << std::endl;
				
				} break;
				default: RAISE_INTERNAL(memberName, "bad syntax tree for memberName");			
			}
			BIND2(n, claim);
		
		}
	}	

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
