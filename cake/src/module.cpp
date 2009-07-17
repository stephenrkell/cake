#include <gcj/cni.h>
#include <string>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <functional>
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
		bool success = false;
		switch(claimGroup->getType())
		{
			case cakeJavaParser::KEYWORD_CHECK:
			case cakeJavaParser::KEYWORD_DECLARE:
			case cakeJavaParser::KEYWORD_OVERRIDE:
				std::cerr << "Presented with a claim list of strength " << CCP(claimGroup->getText())
					<< std::endl;
				success = eval_claim_depthfirst(
					claimGroup, handler_for_claim_strength(claimGroup), (Dwarf_Off) 0);
				if (!success) RAISE(claimGroup, "referenced file does not satisfy claims");
			break;
			default: RAISE_INTERNAL(claimGroup, "bad claim strength (expected `check', `declare' or `override')");
		}
	}
	
	bool elf_module::do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "DO_NOTHING found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;		
		return false;	
	}
	
	bool elf_module::check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< "at die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		return false;		
	}
	
	bool elf_module::internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "INTERNAL CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< "at die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		return false;		
	}	
	
	bool elf_module::declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "DECLARE found falsifiable claim at token " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< "at die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		// FIXME: want to nondestructively make the predicate true
		
		return false;	
	}
	bool elf_module::override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		std::cerr << "OVERRIDE found falsifying module info, at token " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset " << falsifier << " (tag: " << dwarf::tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
			<< ", continuing" << std::endl;
			
		if (falsifier == 0UL)
		{
			// HACK to avoid compilation unit structure: set falsifier to be the 
			// off of the final compilation unit
			dwarf::die_off_list::iterator fixup_iter = dies[falsifier].children().end() - 1;
			if (fixup_iter < dies[falsifier].children().begin())
			{
				RAISE(falsifiable, "no compilation units in file's DWARF info!");
			}
			falsifier = *fixup_iter;
		}
		
		// We want to alter the DWARF info to make the predicate true, destructively if necessary
		if (falsifiable->getType() == cakeJavaParser::MEMBERSHIP_CLAIM
		&&  dies[falsifier].tag() == DW_TAG_compile_unit)
		{
			/* We didn't find a member of the specified name.
			 * We're creating a *value*, i.e. a subprogram or a global variable. */			
			INIT;
			BIND2(falsifiable, memberName);
			BIND3(falsifiable, valueDescriptionExpr, VALUE_DESCRIPTION);
			
			Dwarf_Off parent_off; // the parent of the DIE we're about to create
			definite_member_name mn = read_definite_member_name(memberName);

			if (mn.size() > 1) 
			{
				definite_member_name containing_die_name(mn.begin(), mn.end() - 1);
					
				// check whether the containing die exists
				boost::optional<Dwarf_Off> found = dwarf::resolve_die_path(info.get_dies(),
					falsifier, containing_die_name, containing_die_name.begin());
				if (!found)	RAISE(falsifiable, "can't create nested die before parent exists");
				else parent_off = *found;
			}
			else
			{
				assert(mn.size() != 0);
				parent_off = falsifier;
			}
			std::string die_name = *(mn.end() - 1);

			// HACK to test for subprogram
			if (valueDescriptionExpr->getChild(0)->getType() == cakeJavaParser::LR_SINGLE_ARROW)
			{
				// create a subprogram
				Dwarf_Off subprogram_die_off;
				//dwarf::die_off_list empty_child_list;
				dwarf::encap::die::attribute_map default_attrs = default_subprogram_attributes;	
				subprogram_die_off = create_new_die(
					falsifier, DW_TAG_subprogram, default_attrs, 
					empty_child_list);
				dies[subprogram_die_off].put_attr(DW_AT_name, dwarf::encap::attribute_value(die_name));
				build_subprogram_die_children(valueDescriptionExpr, subprogram_die_off);
			}
			else
			{
				// create a variable, ensuring the type exists
				Dwarf_Off type_off = ensure_dwarf_type(valueDescriptionExpr, falsifier);
				//dwarf::encap::die::attribute_map empty_attribute_map;
				//dwarf::die_off_list empty_child_list;
				Dwarf_Off variable_die_off = create_new_die(
					falsifier, DW_TAG_variable, empty_attribute_map,
					empty_child_list);
				dies[variable_die_off].put_attr(DW_AT_name, dwarf::encap::attribute_value(die_name));
				dies[variable_die_off].put_attr(DW_AT_type, dwarf::encap::attribute_value(
					dwarf::encap::attribute_value::ref(type_off, false)));
			}
						
			dwarf::print_action action(info);
			dwarf::walker::depthfirst_walker<dwarf::print_action,
				dwarf::walker::offset_greater_equal_matcher_t > walk(
					action,
					dwarf::walker::matcher_for_offset_greater_equal(private_offsets_begin)
					);
			walk(info.get_dies(), dies[private_offsets_begin].parent());
			
			assert(eval_claim_depthfirst(falsifiable, 
					&cake::module::do_nothing_handler, 
					falsifier));
			return true;		
		}
		else
		{
			std::cerr << "Can't override unsupported tree--tag pair: " << CCP(falsifiable->getText())
				<< ", " << dwarf::tag_lookup(dies[falsifier].tag()) << std::endl;
			return false;	
		}
	}
		
	Dwarf_Unsigned elf_module::make_default_dwarf_location_expression_for_arg(int argn)
	{
		return 0U; // FIXME: understand location expressions
	}
	
	dwarf::die_off_list *elf_module::find_dwarf_type_named(antlr::tree::Tree *ident, Dwarf_Off context)
	{
		/* From an identifier, find the DWARF type to which that identifier resolves.
		 * We need this because the Cake programmer can just name a type from within a value
		 * description;
		 * FIXME: we should warn if more than one type of the given name is reachable from
		 * the given context (i.e. by traversing up to parent DWARF entries). 
		 * For now we can just print something to stderr. */
		
		/* Context could be anything, but it's part of some DWARF tree whose structure corresponds
		 * to lexical scoping. So say we have (in some fictitious language)
		 
		 compilation_unit {
		 	namespace {
				module {
					class {
						inner_class {
							typedef {
								IDENT // <-- the ident we care about
							} name;
						}
					}
				}
			}
		 }
		 
		 * then our context will be the typedef, and we're looking for anything that is
		 * - a sibling of the typedef
		 * - a sibling of the inner_class (but *not* something *inside* such a sibling
		 * - a sibling of class (but not inside such a sibling)
		 * ... and so on.
		 */
		
		if (context == 0UL)
		{
			// the termination case
			return new dwarf::die_off_list(); // return empty list
		}
		else
		{
			//dwarf::encap::die& context_die = dies[context];
			Dwarf_Off parent_off  = dies[context].parent();
			dwarf::encap::die& parent_die = dies[parent_off];

			dwarf::die_off_list *p_results_list = new dwarf::die_off_list();
			dwarf::die_off_list &results_list = *p_results_list;
			
			// look through our current level
			for (dwarf::die_off_list::iterator iter = parent_die.children().begin();
				iter != parent_die.children().end();
				iter++)
			{
				// if (*iter != context) { // *DON'T* skip over current
				if (dwarf::tag_is_type(dies[*iter].get_tag())
					&& dies[*iter].has_attr(DW_AT_name)
					&& dies[*iter][DW_AT_name].get_string() == CCP(ident->getText())) // FIXME: allow qualified names
				{
					// we've found a match -- add it to the list
					results_list.push_back(*iter);
				}
				
				// }
			}
			
			// recursively search through our parent's siblings
			std::auto_ptr<dwarf::die_off_list> parent_results_list(
				find_dwarf_type_named(ident, parent_off));
			for (dwarf::die_off_list::iterator iter = parent_results_list->begin();
				iter != parent_results_list->end();
				iter++)
			{
				results_list.push_back(*iter); // FIXME: we can do better than copying here
			}
			
			return p_results_list;				
		}		
	}
	
// 	bool elf_module::dwarf_type_satisfies_description(Dwarf_Off type_offset, antlr::tree::Tree *description)
// 	{
// 		dwarf::die_off_list singleton(1, type_offset); // make a list containing just this type's offset
// 		std::auto_ptr<dwarf::die_off_list> list(find_dwarf_types_satisfying(description,
// 			singleton));
// 		assert(list->size() <= 1);
// 		return list->size() != 0;
// 	}

	bool elf_module::dwarf_type_satisfies(antlr::tree::Tree *description,
		Dwarf_Off off)
	{
		INIT;
		switch(description->getType())
		{
			case cakeJavaParser::KEYWORD_OPAQUE:
			case cakeJavaParser::KEYWORD_IGNORED:
			case cakeJavaParser::VALUE_DESCRIPTION:
				return dwarf_type_satisfies(description->getChild(0), off);
				// no break;
			case cakeJavaParser::KEYWORD_PTR: // look for a pointer type
			{
				BIND2(description, pointed_to_type_description); 
					// description's child describes the pointed-to type
				return 
					// must be a pointer type
					dies[off].tag() == DW_TAG_pointer_type
					&& ( // AND either the normal case
						(dies[off][DW_AT_type] != dwarf::encap::attribute_value::DOES_NOT_EXIST()
							&& dwarf_type_satisfies(pointed_to_type_description,
								dies[off][DW_AT_type].get_ref().off))
						// OR the VOID special case... ("no type attribute" means "void")
						|| (dies[off][DW_AT_type] == dwarf::encap::attribute_value::DOES_NOT_EXIST()
							&& pointed_to_type_description->getType() == cakeJavaParser::KEYWORD_VOID));
			} // no break
			case cakeJavaParser::KEYWORD_OBJECT: // look for a structure/union/class/... type
				return dwarf::tag_is_type(dies[off].tag())
					&& dwarf::tag_has_named_children(dies[off].tag())
					&& eval_claim_depthfirst(
						description, &cake::module::internal_check_handler, off);
					// *** QUESTION: can we re-use eval_claim_depthfirst to test these
					// membership claims? I hope so! Problems? This allows our type description to
					// contain nested check/declare/override blocks... this may or may not be
					// meaningful and may or may not be implemented correctly.
				// no break
			case cakeJavaParser::DWARF_BASE_TYPE: // look for a named base type satisfying this description
			{
				BIND3(description, encoding, IDENT);
				BIND3(description, attributeList, DWARF_BASE_TYPE_ATTRIBUTE_LIST);				
				return dies[off].tag() == DW_TAG_base_type
					&& dies[off][DW_AT_encoding].get_string().substr((std::string("DW_ATE_").size())) 
						== CCP(encoding->getText())
					&& eval_claim_depthfirst(attributeList, &cake::module::internal_check_handler, off)
					&& (description->getChildCount() <= 2 ||
							dies[off][DW_AT_byte_size].get_unsigned() ==
								static_cast<Dwarf_Unsigned>(atoll(CCP(description->getChild(3)->getText()))));
			}
			// no break
			case cakeJavaParser::IDENT: // look for any named type
			// HMM... if we can refer to types by name, we're implicitly inserting their existence
			// -- what to do about this? It just means that...
			// ...TODO: we might want to have explicit syntax for checking/declaring/overriding
			// the existence and structure of types. For now, just ensure existence.
			{
				std::auto_ptr<dwarf::die_off_list> results(find_dwarf_type_named(description, off));
				// FIXME: we take the head; is this correct? I think so; it's the nearest match
				return results.get() != 0 && results->size() > 0;
			} 
			case cakeJavaParser::LR_SINGLE_ARROW: // a subprogram type
				// FIXME: we want this to succeed with a dummy
				assert(false);
				break;
			case cakeJavaParser::INDEFINITE_MEMBER_NAME:
				
			default: 
				std::cerr << "Looking for a DWARF type satisfying unrecognised constructor: " <<
					CCP(description->getText()) << std::endl;
				break;				
		} // end switch
		std::cerr << "Warning: fell through bottom of dwarf_type_satisfies switch statement" << std::endl;
		return false; // default
	}
	
	bool elf_module::dwarf_subprogram_satisfies_description(Dwarf_Off subprogram_offset, 
		antlr::tree::Tree *description)
	{
		bool retval =
			dies[subprogram_offset].tag() == DW_TAG_subprogram
		&& (description->getType() == cakeJavaParser::VALUE_DESCRIPTION) 
				? dwarf_subprogram_satisfies_description(subprogram_offset, description->getChild(0)) :
		   (description->getType() == cakeJavaParser::LR_SINGLE_ARROW)
		   		? (
					dwarf_arguments_satisfy_description(subprogram_offset, description->getChild(0))
					&& (
						(description->getChild(1)->getType() == cakeJavaParser::KEYWORD_VOID
							&& !dies[subprogram_offset].has_attr(DW_AT_type))
						||
						(dies[subprogram_offset].has_attr(DW_AT_type) 
							&& dwarf_type_satisfies(description->getChild(1), 
								dies[subprogram_offset][DW_AT_type].get_ref().off))
					)
				)
				: false;
		
		return retval;
	}

	class add_formal_parameter_action
	{
		dwarf::die_off_list& m_list;			
	public:
		add_formal_parameter_action(dwarf::die_off_list& list) : m_list(list) {}
		void operator()(Dwarf_Off offset) { m_list.push_back(offset); }
	};

	bool elf_module::dwarf_arguments_satisfy_description(Dwarf_Off subprogram_offset,
		antlr::tree::Tree *description)
	{
		assert(dies[subprogram_offset].tag() == DW_TAG_subprogram);
		
		bool retval = true;
		if (description->getType() == cakeJavaParser::INDEFINITE_MEMBER_NAME) {}
		else if (description->getType() == cakeJavaParser::MULTIVALUE)
		{
			/* Build the list of formal parameter dies under the subprogram
			 * die, for testing in turn against elements of the MULTIVALUE. */
			dwarf::die_off_list formal_parameter_children;
			add_formal_parameter_action action(formal_parameter_children);
			
			dwarf::walker::depth_limited_selector selector(1);
			dwarf::walker::depthfirst_walker<add_formal_parameter_action,
				dwarf::walker::tag_equal_to_matcher_t,
				dwarf::walker::depth_limited_selector> walk(
						action,
						dwarf::walker::matcher_for_tag_equal_to(DW_TAG_formal_parameter),
						selector
					);
			walk(dies, subprogram_offset);
		
			FOR_ALL_CHILDREN(description)
			{
				Dwarf_Off param_die_off = formal_parameter_children[i];
				retval &= dwarf_variable_satisfies_description(param_die_off,
					n);

				if (!retval) break;
			}
		}
		else { retval = false; }
		return retval;
	}
	
	// NOTE: "variable" can be a formal parameter too
	bool elf_module::dwarf_variable_satisfies_description(Dwarf_Off variable_offset, 
		antlr::tree::Tree *description)
	{
		return dies[variable_offset].has_attr(DW_AT_type) &&
			dwarf_type_satisfies(description, dies[variable_offset][DW_AT_type].get_ref().off);
	}

	
	dwarf::die_off_list *elf_module::find_dwarf_types_satisfying(antlr::tree::Tree *description,
		dwarf::die_off_list& list_to_search)
	{
		/* From a value description in Cake abstract syntax, find *all* DWARF types
		 * (if any) that satisfy the description. */
		std::cerr << "Looking for DWARF types satisfying description "
			<< CCP(description->toStringTree()) /*<< " starting from offsets ";
		for (dwarf::die_off_list::iterator iter = list_to_search.begin();
			iter != list_to_search.end();
			iter++)
		{
			std::cerr << "0x" << std::hex << *iter << std::dec << " ";			
		}*/ << ", starting from " << list_to_search.size() << " initial offsets, beginning 0x"
		<< std::hex << *list_to_search.begin() << std::dec;
		std::cerr << std::endl;
			
		 
		std::vector<Dwarf_Off> *p_retval = new std::vector<Dwarf_Off>();
		std::vector<Dwarf_Off>& retval = *p_retval;

		while (description->getType() == cakeJavaParser::KEYWORD_OPAQUE
				|| description->getType() == cakeJavaParser::KEYWORD_IGNORED
				|| description->getType() == cakeJavaParser::VALUE_DESCRIPTION)
		{	
			// keep skipping over annotations
			description = description->getChild(0);
			// FIXME: need to remember the annotation?
			// now go and do what we would have done
		}
		for (dwarf::die_off_list::iterator type_iter = list_to_search.begin();
			type_iter != list_to_search.end();
			type_iter++)
		{
			if (dwarf_type_satisfies(description, *type_iter)) retval.push_back(*type_iter);
		}
		 
		// FIXME: warn the user somehow if their overriding/declared value description is ambiguous
		std::cerr << "Found " << p_retval->size() << " DWARF types satisfying description "
			<< CCP(description->toStringTree());
		if (p_retval->size() > 0)
		{
			std::cerr << ", offsets ";
			for (dwarf::die_off_list::iterator iter = p_retval->begin();
				iter != p_retval->end();
				iter++)
			{
				std::cerr << *iter << " ";	
			}
		}
		std::cerr << std::endl;
		
		return p_retval;
	}
	
	Dwarf_Off elf_module::ensure_dwarf_type(antlr::tree::Tree *description, Dwarf_Off context)	
	{
		std::cerr << "Ensuring existence of DWARF type satisfying " << CCP(description->toStringTree()) << std::endl;
		std::auto_ptr<dwarf::die_off_list> plist(find_dwarf_types_satisfying(description, info.type_offsets()));
		if (plist->size() >= 1) return *(plist->begin());
		else
		{
			//assert(build_value_description_handler(description, context));
			return create_dwarf_type_from_value_description(description, context);
			// there is no type, so make one
			// FIXME: factor the type-building function out of the handler
		}
	}
	
// 	//build_dwarf_type -- again, recursive just like build_value_description_handler is
// 	bool elf_module::build_value_description_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
// 	{
// 		// First, see if it exists already
// 		std::auto_ptr<dwarf::die_off_list> plist(
// 			find_dwarf_types_satisfying(falsifiable, info.type_offsets()));
// 		
// 		if (plist->size() > 0)
// 		{
// 			return true;
// 		}
// 		else
// 		{
// 			Dwarf_Off new_type_off = create_dwarf_type_from_value_description(
// 				falsifiable, falsifier);
// 			
// 			// DEBUG: now repeat the test, for self-checking
// 			std::auto_ptr<dwarf::die_off_list> plist(find_dwarf_types_satisfying(falsifiable, info.type_offsets()));
// 			assert(plist->size() > 0);
// 		}
// 	}
	
	void elf_module::build_subprogram_die_children(
		antlr::tree::Tree *valueDescriptionExpr, Dwarf_Off subprogram_die_off)
	{
		INIT;
		BIND2(valueDescriptionExpr, descriptionHead);
		assert(descriptionHead->getType() == cakeJavaParser::LR_SINGLE_ARROW);
		{
			INIT;
			BIND2(descriptionHead, functionArgumentDescriptionExpr);
			BIND2(descriptionHead, functionResultDescriptionExpr);

			// find or create a DWARF type that satisfies the return value description expression
			// and add it to the dieset				
			dies[subprogram_die_off].attrs().insert(std::make_pair(DW_AT_type,
				dwarf::encap::attribute_value(dwarf::encap::attribute_value::ref(
						ensure_dwarf_type(
							functionResultDescriptionExpr, 
							// create type in the *parent* context, i.e. not under the subprogram
							// but under whatever contains it
							dies[subprogram_die_off].parent()
				), false))));

			// now process arguments (children of DW_TAG_formal_parameter) 
			switch(functionArgumentDescriptionExpr->getType())
			{
				case cakeJavaParser::MULTIVALUE: {
					int argn = -1;
					FOR_ALL_CHILDREN(functionArgumentDescriptionExpr)
					{
						argn++;
						// *build* a formal_parameter die
						dwarf::encap::die::attribute_map::value_type attr_entries[] = {
							//std::make_pair(DW_AT_name, dwarf::encap::attribute_value(CCP(
							std::make_pair(DW_AT_type, ensure_dwarf_type(n, 
								// create new type under the *parent* die, i.e. not under
								// the subprogram, but under whatever contains it
								dies[subprogram_die_off].parent())),
							std::make_pair(DW_AT_location, 
								make_default_dwarf_location_expression_for_arg(argn))
						};
						dwarf::encap::die::attribute_map new_attribute_map(
								&attr_entries[0], &attr_entries[array_len(attr_entries)]
							);
						//dwarf::die_off_list empty_child_list;
						/*Dwarf_Off parameter_off = */create_new_die(subprogram_die_off,  DW_TAG_formal_parameter,
							new_attribute_map, empty_child_list);
					} // end for
				} break;
				case cakeJavaParser::INDEFINITE_MEMBER_NAME: {
					// arguments are "don't care" -- so create an "unspecified parameters" DIE
					/*Dwarf_Off parameter_off = */create_new_die(subprogram_die_off, DW_TAG_unspecified_parameters,
							empty_attribute_map, empty_child_list);
				} break;
				default:
					std::cerr << "Unrecognised function argument description head: " <<
						CCP(functionArgumentDescriptionExpr->getText()) << ", char number " << 
						(int) functionArgumentDescriptionExpr->getType() << " ('_' is " << (int) '_' << ")" << std::endl;
					assert(false); /* only support multivalues and catch-all for now */
					break;
			} // end switch
		} // end binding block	
	} //end function
	
	Dwarf_Off elf_module::create_dwarf_type_from_value_description(antlr::tree::Tree *valueDescription, Dwarf_Off context)
	{
		//assert(valueDescription->getType() == cakeJavaParser::VALUE_DESCRIPTION);
		//Dwarf_Off new_off;
		INIT;
		//BIND2(valueDescription, descriptionHead);
		
		switch(valueDescription->getType())
		{
			case cakeJavaParser::VALUE_DESCRIPTION: // header
			case cakeJavaParser::KEYWORD_OPAQUE:
			case cakeJavaParser::KEYWORD_IGNORED: {
				// recurse
				BIND2(valueDescription, descriptionHead);
				return create_dwarf_type_from_value_description(descriptionHead, context);
				} break;
			case cakeJavaParser::KEYWORD_PTR: {
				// We've been asked to build a new pointer type. First ensure the pointed-to
				// type exists, then create the pointer type.
				BIND2(valueDescription, pointed_to_type_description);
				Dwarf_Off existing_type = ensure_dwarf_type(pointed_to_type_description, 
					context);
				
				dwarf::encap::die::attribute_map::value_type attr_entries[] = {
					std::make_pair(DW_AT_type, dwarf::encap::attribute_value(
						dwarf::encap::attribute_value::ref(existing_type, false)))
				};
				dwarf::encap::die::attribute_map new_attribute_map(
						&attr_entries[0], &attr_entries[array_len(attr_entries)]
						);			
				
				//dwarf::die_off_list empty_child_list;
				Dwarf_Off new_off = create_new_die(context, DW_TAG_pointer_type, new_attribute_map,
					empty_child_list);
			
				return new_off;
			} break;
//			case cake
			case cakeJavaParser::IDENT: {
				// We've been asked to build a named type. Usee DWARF's DW_TAG_unspecified_type
				//dwarf::encap::die::attribute_map empty_attribute_map;
				//dwarf::die_off_list empty_child_list;
				return create_new_die(context, DW_TAG_unspecified_type, empty_attribute_map,
					empty_child_list);
			} break;
			default:
				std::cerr << "Asked to build a DWARF type from unsupported value description node: "
					<< CCP(valueDescription->getText()) << std::endl;
				assert(false);
			break;
		}		
	}
	
	Dwarf_Off elf_module::create_new_die(const Dwarf_Off parent, const Dwarf_Half tag, 
		dwarf::encap::die::attribute_map const& attrs, dwarf::die_off_list const& children)	
	{
		Dwarf_Off new_off = next_private_offset();
		std::cerr << "Warning: creating new DIE at offset 0x" << std::hex << new_off << std::dec 
			<< ", tag " << dwarf::tag_lookup(tag)
			<< " as child of unchecked parent DIE at 0x"
			<< std::hex << parent << std::dec << ", tag " << dwarf::tag_lookup(dies[parent].tag())
			<< std::endl;
		dies.insert(std::make_pair(new_off, dwarf::encap::die(*this, parent, tag, new_off, 
			new_off - *find_containing_cu(parent), attrs, children)));
		dies[parent].children().push_back(new_off);	
		return new_off;
	}
	
	boost::optional<Dwarf_Off> elf_module::find_nearest_containing_die_having_tag(Dwarf_Off context, Dwarf_Half tag)
	{
		namespace w = dwarf::walker;
		
		const w::tag_equal_to_matcher_t matcher = w::matcher_for_tag_equal_to(tag);
		
		Dwarf_Half test_tag = dies[context].tag();
		const w::tag_equal_to_matcher_t test_matcher = w::matcher_for_tag_equal_to(test_tag);
		assert(test_matcher(dies[context]));
		
		typedef w::func1_do_nothing<Dwarf_Half> do_nothing_t;
		typedef w::siblings_upward_walker<do_nothing_t, 
			w::tag_equal_to_matcher_t> walker_t;
		
		//boost::optional<Dwarf_Off> (*f)(dwarf::dieset&, Dwarf_Off, const w::tag_equal_to_matcher_t&) 
		//	= &(w::find_first_match<w::tag_equal_to_matcher_t, walker_t>);
		
		return w::find_first_match<w::tag_equal_to_matcher_t, walker_t>(dies, context, matcher);
					
				
// 		
// 		dwarf::walker::find_first_match<<
// 			 dwarf::walker::capture_func<Dwarf_Off>, 
// 				dwarf::walker::tag_equal_to_matcher_t,
// 					dwarf::walker::select_until_captured>(
// 		
//  		dwarf::walker::siblings_upward_walker<dwarf::walker::capture_func<Dwarf_Off>, 
// 			dwarf::walker::tag_equal_to_matcher_t,
// 			dwarf::walker::func2_true<dwarf::encap::die&, dwarf::walker::walker&>
// 			> walk(dwarf::walker::capture_func<Dwarf_Off>(), matcher, 
// 				dwarf::walker::func2_true<dwarf::encap::die&, dwarf::walker::walker&>());
//  		return (
//  			dies, context, walk);
	}

	boost::optional<Dwarf_Off> elf_module::find_containing_cu(Dwarf_Off context)
	{
		return find_nearest_containing_die_having_tag(context, DW_TAG_compile_unit);
	}
	
	Dwarf_Off elf_module::follow_typedefs(Dwarf_Off off)
	{
		assert(dwarf::tag_is_type(dies[off].tag()));
		if (dies[off].tag() == DW_TAG_typedef) return dies[off][DW_AT_type].get_ref().off;
		else return off;
	}
	
	module::eval_event_handler_t elf_module::handler_for_claim_strength(antlr::tree::Tree *strength)
	{
		return
			strength->getType() == cakeJavaParser::KEYWORD_CHECK 	? &cake::module::check_handler
		: 	strength->getType() == cakeJavaParser::KEYWORD_DECLARE 	? &cake::module::declare_handler
		: 	strength->getType() == cakeJavaParser::KEYWORD_OVERRIDE ? &cake::module::override_handler : 0;
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

	struct OR_success : dwarf::walker::action {
		typedef bool (elf_module::*module_eval_func)
			(antlr::tree::Tree *, module::eval_event_handler_t, Dwarf_Off);
		bool any_success;
		elf_module& m_module;
		module_eval_func m_evaluator;
		module::eval_event_handler_t m_handler;
		antlr::tree::Tree *m_claim;
		OR_success(elf_module& module, 
			module_eval_func evaluator,
			module::eval_event_handler_t handler, antlr::tree::Tree *claim) 
			: any_success(false), m_module(module), m_evaluator(evaluator), 
			m_handler(handler), m_claim(claim) {}
		void operator()(Dwarf_Off off)
		{
			any_success |= (m_module.*m_evaluator)(m_claim, m_handler, off);
		}
	};
		
	bool elf_module::eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
		Dwarf_Off current_die)
	{
		// For cases where we recursively AND subclaims together, which handler should we use?
		// This variable gets modified by the CHECK, DECLARE and OVERRIDE case-handlers, and
		// otherwise is left alone.
		eval_event_handler_t recursive_event_handler = handler;
		bool retval;
		INIT;
		switch(claim->getType())
		{
			case cakeJavaParser::KEYWORD_CHECK:
			case cakeJavaParser::KEYWORD_DECLARE:
			case cakeJavaParser::KEYWORD_OVERRIDE: {
				/* We've hit a new handler specification, so:
				 * 
				 * claim heads a list of claims to be evaluated recursively;
				 *
				 * current_die could be anything, and is simply passed on. */
				
				// HACK: disallow if we're re-using this routine
				if (handler == &cake::module::check_handler) assert(false);
				 
				ALIAS2(claim, strength);
				std::cerr << "Changing handler to " << CCP(strength->getText()) << std::endl;
				recursive_event_handler = handler_for_claim_strength(strength);
				} goto recursively_AND_subclaims;
			case cakeJavaParser::DWARF_BASE_TYPE_ATTRIBUTE_LIST: {
				FOR_ALL_CHILDREN(claim)
				{
					INIT;
					BIND3(n, header, cakeJavaParser::DWARF_BASE_TYPE_ATTRIBUTE);
					{
						INIT;
						BIND3(header, attr, IDENT);
						BIND3(header, value, INT);

						std::map<const char *, Dwarf_Half>::iterator found
							= dwarf::attr_forward_map.find(CCP(attr->getText()));
						if (found == dwarf::attr_forward_map.end()) 
						{
							retval = false; break; // not found
						}

						std::istringstream istr(CCP(value->getText()));
						signed valueAsInt; istr >> valueAsInt;

						if (dies[current_die][found->second].get_signed() != valueAsInt) 
						{								
							retval = false; break;
						}
					}							
				}
			}
			case cakeJavaParser::KEYWORD_OBJECT: {
				/* We hit a block of claims about named members of the current die. So:
				 * 
				 * claim heads a list of CLAIMs to be evaluated recursively;
				 *
				 * current_die is any DIE defining a *type* with named children. */
				assert(dwarf::tag_is_type(info[current_die].tag()) 
					&& dwarf::tag_has_named_children(info[current_die].tag()));
			 	} goto recursively_AND_subclaims;
				
			case cakeJavaParser::MEMBERSHIP_CLAIM: {
				/* We hit a claim about a named member of the current die. So:
				 * 
				 * claim heads a pair (memberNameExpr, valueDescription);
				 
				 * current_die is either the ultimate parent (0UL), or a compilation unit,
				 * or anything else with named children. */
				BIND2(claim, memberNameExpr);
				BIND3(claim, valueDescription, VALUE_DESCRIPTION);
				assert(current_die == 0UL || dwarf::tag_has_named_children(info[current_die].tag()));
				
				/* If current_die == 0 and memberNameExpr is indefinite, 
				 * we try the claim over *all definitions in all compilation units*
				 * and return true if any definition satisfies the claim.
				 *
				 * If current_die != 0 and memberNameExpr is indefinite,
				 * we try the claim over children of current_die only, and return
				 * true if any satisfies the claim.
				 *
				 * If current_die == 0 and memberNameExpr is definite,
				 * we try the claim over *all definitions in all compilation units*,
				 * and return true when we find *some member* in *some compilation unit*
				 * which satisfies the claim.
				 *
				 * If current_die != 0 and memberName is definite,
				 * we try the claim over children of current_die only, and return
				 * true if we find some child that satisfies the claim. */
				
				dwarf::walker::depth_limited_selector max_depth_1(1);
				if (current_die == 0UL && memberNameExpr->getType() == cakeJavaParser::INDEFINITE_MEMBER_NAME)
				{
					std::cerr << "looking for any toplevel name satisfying " << CCP(claim->getText()) << std::endl;
					// Success of the claim is success of *the same claim we're currently processing*
					// on *any* of our child dies. Note: this means we'll recursively evaluate the same
					// claim, but on children of the current die; this will hit some of the lower-down
					// if--else cases than this one.
					// If the claim fails, we fall back to here.
					OR_success accumulator(*this, &cake::elf_module::eval_claim_depthfirst, 
						&cake::module::do_nothing_handler, claim);
					
					// only perform the action (i.e. recursive evaluation) on dies that have named children
					dwarf::walker::tag_satisfying_func_matcher_t matcher =
						dwarf::walker::matcher_for_tag_satisfying_func(
							&dwarf::tag_has_named_children);
							
					typedef dwarf::walker::depthfirst_walker<
						OR_success, 
						dwarf::walker::tag_satisfying_func_matcher_t,
						dwarf::walker::depth_limited_selector> type_of_walker;
					
					type_of_walker walker(
						accumulator, 
						matcher,
						max_depth_1);
					
					// do the walk
					walker(info.get_dies(), current_die);
					
					// now we have a success value in the accumulator
					retval = accumulator.any_success;				
				}
				else if (current_die != 0UL && memberNameExpr->getType() == cakeJavaParser::INDEFINITE_MEMBER_NAME)
				{
					std::cerr << "looking for any non-toplevel name satisfying " << CCP(claim->getText()) << std::endl;	
					// Success of the claim is success of *the valueDescription claim*
					// on *any* of our child dies.
					OR_success accumulator(*this, &cake::elf_module::eval_claim_depthfirst, 
						handler, valueDescription);
						
					// Perform the action (i.e. recursive evaluation) on all dies found.
					// FIXME: this is a bit underapproximate -- we only want dies that
					// can be said to satisfy a value description: so variables, types,
					// subprograms....
					dwarf::walker::always_match matcher;
					
					// We use max_depth_1, i.e. select only immediate children
					dwarf::walker::depthfirst_walker<OR_success, dwarf::walker::always_match,
						dwarf::walker::depth_limited_selector> walker(accumulator, matcher, max_depth_1);

					// do the walk
					walker(info.get_dies(), current_die);

					// now we have a success value in the accumulator						
					retval = accumulator.any_success;				
				}
				else if (current_die == 0UL && memberNameExpr->getType() == cakeJavaParser::DEFINITE_MEMBER_NAME)
				{
					std::cerr << "looking for a toplevel name " << read_definite_member_name(memberNameExpr) << " satisfying " << CCP(claim->getText()) << std::endl;	
					// Success of the claim is success of *our current claim* on *any* of our child dies.
					// In other words, the recursive call will evaluate whether the CU
					// firstly has a member of the given name, and
					// secondly that that member satisfies the description.
					// If the claim fails, we fall back and handle it at this level.
					OR_success accumulator(*this, &cake::elf_module::eval_claim_depthfirst, 
						&cake::module::do_nothing_handler, claim);
						
					// Perform the action on all compilation unit dies. Conveniently,
					// this will exclude offset 0, preventing infinite recursion. 
					dwarf::walker::tag_equal_to_matcher_t matcher =
						dwarf::walker::matcher_for_tag_equal_to(DW_TAG_compile_unit);
					
					// Again, use max_depth_1, i.e. select only immediate children
					dwarf::walker::depthfirst_walker<OR_success, dwarf::walker::tag_equal_to_matcher_t,
						dwarf::walker::depth_limited_selector> walker(accumulator, matcher, max_depth_1);

					// do the walk
					walker(info.get_dies(), current_die);

					// now we have a success value in the accumulator						
					retval = accumulator.any_success;
				}
				else if (current_die != 0UL && memberNameExpr->getType() == cakeJavaParser::DEFINITE_MEMBER_NAME)
				{
					std::cerr << "looking for a non-toplevel name " << read_definite_member_name(memberNameExpr) << " satisfying " << CCP(claim->getText()) << std::endl;
					// This is the simple case: we just need to find the named element and
					// recursively evaluate the claim on it.
					definite_member_name nm = read_definite_member_name(memberNameExpr);
					
					// truth of the claim is simply existence of the name 
					// (we handle this recursively)
					// and that named element's satisfaction of the property
					
					boost::optional<Dwarf_Off> found = dwarf::resolve_die_path(info.get_dies(),
						current_die, nm, nm.begin());

					retval = (found != 0) //info[current_die].children().end())
						&& eval_claim_depthfirst(valueDescription, handler, *found);
				}
				else
				{
					RAISE_INTERNAL(memberNameExpr, "expected a memberNameExpr");
				}

				//return retval;
			} break;
			
			case cakeJavaParser::DEFINITE_MEMBER_NAME: {
				/* We hit a claim that is asserting the existence of a named child
				 * of the current die. The name is a list of member elements, i.e.
				 * the child might be >1 generation down the line. Also, our search
				 * has to follow DW_AT_type links if we're in a type die, and will just
				 * follow children links otherwise. 
				 *
				 * We can assert that claim simply heads a list of idents, and 
				 * current_die is something with members (children or types). */
				definite_member_name nm = read_definite_member_name(claim);
				assert(dwarf::tag_has_named_children(info[current_die].tag()));
				
				retval = dwarf::resolve_die_path(info.get_dies(),
					dwarf::tag_is_type(dies[current_die].tag()) ? follow_typedefs(current_die) : current_die,
					nm, nm.begin()) != 0;
				
			} break;			
// 			case cakeJavaParser::KEYWORD_VOID: // ****FIXME*** dummy to NOP-out following code
// 			{
// 				if (current_die == 0) // toplevel claim group
// 				{
// 					/* SPECIAL CASE: because we want to ignore information on compilation units, 
// 					 * we loop through each compilation unit when evaluating a toplevel claim.
// 					 * Each immediate subclaim will be about membership, so we just want *any*
// 					 * compilation unit to satisfy it. */
// 					bool all_claims_sat = true;
// 					bool any_cu_sat = false;
// 					FOR_ALL_CHILDREN(claim) // for all *claims*
// 					{
// 						INIT;
// 						bool this_cu_sat = false;
// 						ALIAS3(n, claimHeader, cakeJavaParser::MEMBERSHIP_CLAIM); // skip over the CLAIM token
// 						BIND2(n, memberName); // either `_' or a memberClaim
// 						BIND2(n, valueDescriptionExpr);
// 						if (memberName->getType() == cakeJavaParser::INDEFINITE_MEMBER_NAME) RAISE_INTERNAL(memberName, "`_' is not allowed at module level");
// 						std::vector<Dwarf_Off>::iterator i_cu;
// 						for (i_cu = info.compilation_unit_offsets().begin();
// 							i_cu != info.compilation_unit_offsets().end();
// 							i_cu++)
// 						{
// 							/* This is a CLAIM, so of the form "member : predicate".
// 							 * We check that the member exists, and that it satisfies
// 							 * the predicate.							
// 							 */
// 							definite_member_name mn = read_definite_member_name(memberName);
// 
// 							std::cerr << "Trying claim about member " << mn 
// 								<< " in compilation unit " 
// 								<< (info[*i_cu].has_attr(DW_AT_name) ? info[*i_cu][DW_AT_name].get_string().c_str() : "(anonymous)")
// 								<< std::endl;
// 
// 							dwarf::encap::die& parent = info[*i_cu];
// 							std::vector<Dwarf_Off>::iterator i_child;
// 							for (i_child = parent.children().begin();
// 								i_child != parent.children().end();
// 								i_child++)
// 							{	
// 								dwarf::encap::die& child = dies[*i_child];
// 								this_cu_sat |= ( // 1. child must be something we understand
// 									(	child.tag() == DW_TAG_subprogram
// 									|| 	child.tag() == DW_TAG_variable )
// 								// 2. child must have the name given
// 								&&  (	child.has_attr(DW_AT_name) && child[DW_AT_name].get_string() == mn[0]
// 								/* FIXME: need proper n-deep finding of members, for multipart memberNames */
// 									)
// 								&& eval_claim_depthfirst(valueDescriptionExpr, handler, *i_child)
// 								);
// 								
// 								if (this_cu_sat) break; // succeed-fast
// 							}
// 							if (!this_cu_sat) /* DO NOTHING -- try the next cu */ {}
// 							any_cu_sat |= this_cu_sat;
// 							if (any_cu_sat) break;
// 						} // end for all compilation units
// 						
// 						/* Now we've tried all compilation units. If we still haven't satisfied
// 						 * the predicate, call the handler passing the *last* compilation unit.
// 						 * FIXME: this is a bit of a hack. It's okay because we don't care about
// 						 * how the input is divided into compilation units. However, it will bite
// 						 * us if e.g. one compilation unit defines functions/types/variables with
// 						 * the same name as each other but different definitions.
// 						 */
// 						if (!any_cu_sat) any_cu_sat |= (this->*handler)(memberName, *(i_cu - 1));
// 					}
// 					all_claims_sat &= any_cu_sat;
// 					if (!all_claims_sat) all_claims_sat |= (this->*handler)(claim, current_die); // fallback
// 					return all_claims_sat;
// 				}
// 				else
// 				{
// 					assert(claim->getType() == cakeJavaParser::KEYWORD_OBJECT && current_die != 0);
// 					
// 					// check the current die: it must be something that has members
// 					if (!dwarf::tag_has_named_children(dies.find(current_die)->second.get_tag()))
// 					{
// 						RAISE_INTERNAL(claim, "found a membership claim about a structureless object");
// 					}
// 					
// 					// now process each immediate subclaim in turn
// 					bool sat = true;
// 					FOR_ALL_CHILDREN(claim)
// 					{
// 						INIT;
// 						ALIAS3(n, claimHeader, cakeJavaParser::MEMBERSHIP_CLAIM); // skip over the CLAIM token
// 						BIND2(n, memberName); // either `_' or a memberName
// 						definite_member_name name;
// 						switch (memberName->getType())
// 						{
// 							case cakeJavaParser::INDEFINITE_MEMBER_NAME:
// 								std::cerr << "Claim concerns all remaining members" << std::endl;
// 								// FIXME: now do something
// 								sat &= true;							
// 							break;
// 							case cakeJavaParser::DEFINITE_MEMBER_NAME:
// 								definite_member_name list = read_definite_member_name(memberName);
// 								std::cerr << "Claim concerns member ";
// 								for (definite_member_name::iterator i = list.begin(); i != list.end(); i++)
// 								{
// 									std::cerr << *i;
// 									if (i + 1 != list.end()) std::cerr << " :: ";
// 								}
// 								std::cerr << std::endl;
// 								// FIXME: now do something and recurse
// 								sat &= true;
// 							break;						
// 						}
// 						if (!sat) sat |= (this->*handler)(claim, current_die); // fallback
// 						if (!sat) break; // fail fast
// 					} // end FOR_ALL_CHILDREN
// 					
// 					return sat;
// 				} // end else we_have_an_object
// 			} break;
			case cakeJavaParser::VALUE_DESCRIPTION:
				{
					INIT;
					BIND2(claim, valueDescriptionHead);
					std::cerr << "Trying claim about value description with head node " 
						<< CCP(valueDescriptionHead->getText())
						<< " on DIE of tag " << dwarf::tag_lookup(dies[current_die].tag())
						<< std::endl;
					if (dies[current_die].tag() == DW_TAG_subprogram)
					{
						retval = dwarf_subprogram_satisfies_description(current_die, claim);
					}
					else if (dies[current_die].tag() == DW_TAG_variable)
					{
						retval = dwarf_variable_satisfies_description(current_die, claim);
					}
					else if (dwarf::tag_is_type(dies[current_die].tag()))
					{
						retval = dwarf_type_satisfies(claim, follow_typedefs(current_die));
					}
					else
					{
						std::cerr << "Error: claim is a value description, "
							<< "but DIE is not a subprogram or type." << std::endl;
						retval = false;
					}
						

				} break;
			default: 
				std::cerr << "Unsupported claim head node: " << CCP(claim->getText()) << std::endl;
				return false;
			recursively_AND_subclaims:
				retval = true;
				FOR_ALL_CHILDREN(claim)
				{
					INIT;
					ALIAS3(n, subclaim, cakeJavaParser::MEMBERSHIP_CLAIM);
					retval &= eval_claim_depthfirst(subclaim, recursive_event_handler, current_die);
				}
				//return success;
		}	// end switch
		
		std::cerr << "Result of evaluating claim " << CCP(claim->getText()) << " was " << retval << std::endl;
		if (!retval)
		{
			std::cerr << "Claim failed, so invoking handler." << std::endl;
			retval |= (this->*handler)(claim, current_die);
		}
		return retval;
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
			if (info[*i][DW_AT_name] != dwarf::encap::attribute_value::DOES_NOT_EXIST())
			{
				std::cerr << ", name: " << info[*i][DW_AT_name].get_string() << std::endl;
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
			if (info[*i][DW_AT_name] != dwarf::encap::attribute_value::DOES_NOT_EXIST())
			{
				std::cerr << ", name: " << info[*i][DW_AT_name].get_string() << std::endl;
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
			if (info[*i][DW_AT_name] != dwarf::encap::attribute_value::DOES_NOT_EXIST())
			{
				std::cerr << ", name: " << info[*i][DW_AT_name].get_string() << std::endl;
			}
			else
			{
				std::cerr << ", no DW_AT_name attribute" << std::endl;
			}
		}
	}
	
	const dwarf::die_off_list elf_module::empty_child_list;
	const dwarf::encap::die::attribute_map elf_module::empty_attribute_map;
		
	const dwarf::encap::die::attribute_map::value_type elf_module::default_subprogram_attr_entries[] = {
		//std::make_pair(DW_AT_name, dwarf::encap::attribute_value(CCP(
		std::make_pair(DW_AT_frame_base, (Dwarf_Unsigned) 0UL)
// 			// FIXME: write a meaningful DW_AT_frame_base here!
	};
	const dwarf::encap::die::attribute_map elf_module::default_subprogram_attributes(
		&elf_module::default_subprogram_attr_entries[0], 
		&elf_module::default_subprogram_attr_entries[array_len(elf_module::default_subprogram_attr_entries)]
	);
	const Dwarf_Off elf_module::private_offsets_begin = 1<<30; // 1GB of original DWARF information should be enough
}

