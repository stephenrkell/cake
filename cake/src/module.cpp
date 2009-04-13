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
	
	bool elf_module::check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "CHECK found falsifying module info, aborting" << std::endl;
		
		return false;		
	}
	bool elf_module::declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "DECLARE found falsifying module info, aborting" << std::endl;
		
		// FIXME: want to nondestructively make the predicate true
		
		return false;	
	}
	bool elf_module::override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "OVERRIDE found falsifying module info, continuing" << std::endl;
		
		// FIXME: want to make the predicate true, destructively if necessary
		
		return true;	
	}
	
	eval_event_handler_t elf_module::handler_for_claim_strength(antlr::tree::Tree *strength)
	{
		return
			strength->getType() == cakeJavaParser::KEYWORD_CHECK 	? check_handler
		: 	strength->getType() == cakeJavaParser::KEYWORD_DECLARE 	? declare_handler
		: 	strength->getType() == cakeJavaParser::KEYWORD_OVERRIDE ? override_handler : 0;
	}
	
	void elf_module::process_claim_list(claim_strength s, antlr::tree::Tree *claimGroup)
	{
		std::cerr << "Presented with a claim list of strength " << (
				(s == CHECK) ? "CHECK"
			:	(s == DECLARE) ? "DECLARE"
			:	(s == OVERRIDE) ? "OVERRIDE"
			:	"(unrecognised)") << std::endl;
		assert(handler_for_claim_strength(s) != 0);
		
		eval_claim_depthfirst(claimGroup, handler_for_claim_strength(s), (Dwarf_Off) 0);		
	}

// from earlier notes:
/*	
	component elf_reloc("switch.o") switch12 {
        override {
                .gtk_dialog_new : _ -> GtkDialog ptr
        }
        declare {
                .gtk_dialog_new : _ -> object { .vbox: opaque } ptr
        }
}

(Use of named member entities asserts their existence, Russell-style, with the assertion semantics
of the containing block.)

This syntax isn't ideal, because implicitly we're overriding all the way from the root. For example,
if switch12 turned out not to have an element .gtk_dialog_new, we would be overriding this and
asserting that it does. So we really want finer grain, i.e. the ability to change from "check" or
"declare" to "assert" mid-tree. This will complicate the syntax, so I won't do this yet.
*/
	
	
	bool elf_module::eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
		Dwarf_Off current_die)
	{
		switch(claim->getType())
		{
			// separate out the cases where we have a list of claims about members
			case cakeJavaParser::KEYWORD_CHECK:
			case cakeJavaParser::KEYWORD_DECLARE:
			case cakeJavaParser::KEYWORD_OVERRIDE:
			case cakeJavaParser::KEYWORD_OBJECT:
				if (current_die == 0) // toplevel claim group
				{
					/* SPECIAL CASE: because we want to ignore information on compilation units, 
					 * we loop through each compilation unit when evaluating a toplevel claim.
					 * Each immediate subclaim will be about membership, and may *not* be */
					bool all_sat = true;
					FOR_ALL_CHILDREN(claim)
					{
						INIT;
						bool this_sat = false;
						ALIAS3(n, claimHeader, cakeJavaParser::CLAIM); // skip over the CLAIM token
						BIND2(n, memberName); // either `_' or a memberClaim
						BIND2(n, valueDescriptionExpr);
						if (memberName->getType() == '_') RAISE_INTERNAL(memberClaim, "`_' is not allowed at module level");
						for (std::vector<Dwarf_Off>::iterator i_cu = info.get_compilation_units().first.begin();
							i_cu != info.get_compilation_units().first.end();
							i_cu++)
						{
							std::cerr << "Trying claim about member " << CCP(memberName->getText()) << " in compilation unit " << info.get_compilation_units().second[*i_cu][DW_AT_name].get_string() << std::endl;
							
							/* This is a CLAIM, so of the form "member : predicate".
							 * We check that the member exists, and that it satisfies
							 * the predicate.							
							 */
							definite_member_name mn = read_definite_member_name(memberName);
							dwarf::encap::die& parent = info.get_compilation_units().second[*i_cu]
							for (std::vector<Dwarf_Off>::iterator i_child = 
								parent.children().begin();
								i_child != parent.children().end();
								i_child++)
							{	
								dwarf::encap::die& child = info.get_dies()[*i];
								this_sat |= (
									(	child.tag() == DW_TAG_subprogram
									|| 	child.tag() == DW_TAG_variable )
								||  (	child.attrs()[DW_AT_name].get_string() == 
								/* FIXME: need proper n-deep finding of members, for multipart memberNames */

									)
								&& eval_claim_depthfirst(valueDescriptionExpr, handler,
								);
								
								if (this_sat) break; // succeed-fast
							}
								
							
							//eval_claim_depthfirst(memberClaim, 
							//	handler_for_claim_strength(claim), *i_cu);
							if (this_sat) break;
						}
						all_sat &= this_sat;
					}
					if (!all_sat) all_sat |= handler(claim, current_die); // fallback
					return all_sat;
				}
				else
				{
					assert(claim->getType() == cakeJavaParser::KEYWORD_OBJECT && current_die != 0);
					
					// check the current die: it must be something that has members
					switch (info.get_dies().find(current_die)->second.get_tag())
					{
						case DW_TAG_compile_unit:
						case DW_TAG_structure_type:
						case DW_TAG_class_type:
						case DW_TAG_enumeration_type:
						case DW_TAG_interface_type:
						case DW_TAG_set_type:
						case DW_TAG_union_type:
							break; // this is okay
						default:
							RAISE_INTERNAL(claim, "found a membership claim about a structureless object");
					}
					
					// now process each immediate subclaim in turn
					bool sat = true;
					FOR_ALL_CHILDREN(claim)
					{
						INIT;
						ALIAS3(n, claimHeader, cakeJavaParser::CLAIM); // skip over the CLAIM token
						BIND2(n, memberName); // either `_' or a memberName
						definite_member_name name;
						switch (memberName->getType())
						{
							case '_':
								std::cerr << "Claim concerns all remaining members" << std::endl;
								// FIXME: now do something
								sat &= true;							
							break;
							case cakeJavaParser::DEFINITE_MEMBER_NAME:
								definite_member_name list = read_definite_member_name(memberName);
								std::cerr << "Claim concerns member ";
								for (definite_member_name::iterator i = list.begin(); i != list.end(); i++)
								{
									std::cerr << *i;
									if (i + 1 != list.end()) std::cerr << " :: ";
								}
								std::cerr << std::endl;
								// FIXME: now do something and recurse
								sat &= true;
							break;						
						}
						if (!sat) sat |= handler(claim, current_die); // fallback
						if (!sat) break; // fail fast
					} // end FOR_ALL_CHILDREN
					
					return sat;
				} // end else we_have_an_object
			break;
		
		}	// end switch	
	} // end function

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
				std::cerr << ", name: " << info.func_dies()[*i][DW_AT_name].get_string() << std::endl;
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
				std::cerr << ", name: " << info.toplevel_var_dies()[*i][DW_AT_name].get_string() << std::endl;
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
				std::cerr << ", name: " << info.type_dies()[*i][DW_AT_name].get_string() << std::endl;
			}
			else
			{
				std::cerr << ", no DW_AT_name attribute" << std::endl;
			}
		}
	}
}
