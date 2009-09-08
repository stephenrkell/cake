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
#include "parser.hpp"

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
	int module::newline_tabbing_filter::indent_level = 0; // HACK: shouldn't be static!
		
	void module::process_exists_claims(antlr::tree::Tree *existsBody)
	{
		FOR_ALL_CHILDREN(existsBody)
		{
			INIT;
			SELECT_NOT(LR_DOUBLE_ARROW); // we don't want rewrites, only claimGroups
			process_claimgroup(n);
		}
	}

	void module::process_supplementary_claim(antlr::tree::Tree *claimGroup)
	{
		process_claimgroup(claimGroup);
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
				debug_out << "Presented with a claim list of strength " << CCP(claimGroup->getText())
					<< std::endl;
				success = eval_claim_depthfirst(
					claimGroup, handler_for_claim_strength(claimGroup), (Dwarf_Off) 0);
				if (!success) RAISE(claimGroup, "referenced file does not satisfy claims");
			break;
			default: RAISE_INTERNAL(claimGroup, "bad claim strength (expected `check', `declare' or `override')");
		}
	}
	
	elf_module::elf_module(std::string filename) :
			ifstream_holder(filename),
			module(filename),
			dwarf::file(fileno()),
			info(*this),
			dies(info.get_dies()),
			private_offsets_next(private_offsets_begin)
	{
		// if no debug information was imported, set up a dummy compilation unit
		if (dies[0UL].children().size() == 0)
		{
		/*
			Attribute DW_AT_name, value: (string) dwarf_abbrev.c
            Attribute DW_AT_stmt_list, value: (unsigned) 0
            Attribute DW_AT_low_pc, value: (address) 0x8049930
            Attribute DW_AT_high_pc, value: (address) 0x8049e8e
            Attribute DW_AT_language, value: (unsigned) 1
            Attribute DW_AT_comp_dir, value: (string) /home/srk31/scratch-1/dwarf-20080929/libdwarf
            Attribute DW_AT_producer, value: (string) GNU C 4.1.2 20070925 (Red Hat 4.1.2-27)
		*/
			char cwdbuf[4096];
			getcwd(cwdbuf, sizeof cwdbuf);

			dwarf::encap::die::attribute_map::value_type attr_entries[] = {
				std::make_pair(DW_AT_name, dwarf::encap::attribute_value(std::string("__cake_dummy_cu"))),
				std::make_pair(DW_AT_stmt_list, dwarf::encap::attribute_value((Dwarf_Unsigned) 0U)),
				//std::make_pair(DW_AT_low_pc, dwarf::encap::attribute_value(0, (Dwarf_Addr) 0U)),
				//std::make_pair(DW_AT_high_pc, dwarf::encap::attribute_value(0, (Dwarf_Addr) 0U)),
				std::make_pair(DW_AT_language, dwarf::encap::attribute_value((Dwarf_Unsigned) 1U)),
				std::make_pair(DW_AT_comp_dir, dwarf::encap::attribute_value(std::string(cwdbuf))),
				std::make_pair(DW_AT_producer, dwarf::encap::attribute_value(std::string(CAKE_VERSION)))
			};

			dwarf::encap::die::attribute_map new_attribute_map(
					&attr_entries[0], &attr_entries[array_len(attr_entries)]
					);			
			std::vector<Dwarf_Off> no_children;
			create_new_die(0UL, DW_TAG_compile_unit, new_attribute_map, no_children);
		}			
	}

	
	bool elf_module::do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "DO_NOTHING found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;		
		return false;	
	}
	
	bool elf_module::check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		return false;		
	}
	
	bool elf_module::internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "INTERNAL CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		return false;		
	}	
	
#define MAKE_TOKEN_DIE_PAIR(toktype, tag) (((usigned long long) (toktype) << ((sizeof (Dwarf_Half) * 8))) | (tag))
	
#define TOKEN_DIE_PAIR(tokname, tag) (((unsigned long long) \
	(cakeJavaParser::tokname << ((sizeof (Dwarf_Half) * 8))) | (tag))

	boost::optional<Dwarf_Off> elf_module::find_immediate_container(const definite_member_name& mn, 
		Dwarf_Off context) const
	{
		boost::optional<Dwarf_Off> found = dwarf::resolve_die_path(dies, context, mn, mn.begin());
		if (found) return dies[*found].parent();
		else return found; // i.e. empty
	}
	
	Dwarf_Off elf_module::ensure_non_toplevel_falsifier(antlr::tree::Tree *falsifiable,
		Dwarf_Off falsifier) const
	{
		if (falsifier == 0UL)
		{
			// HACK to avoid compilation unit structure: set falsifier to be the 
			// off of the final compilation unit
			dwarf::die_off_list::iterator fixup_iter = dies[falsifier].children().end() - 1;
			if (dies[falsifier].children().size() == 0
				|| fixup_iter < dies[falsifier].children().begin())
			{
				RAISE(falsifiable, "no compilation units in file's DWARF info!");
			}
			falsifier = *fixup_iter;
		}
		return falsifier;	
	}	
	
	void elf_module::debug_print_artificial_dies() 
	{
		dwarf::print_action action(info);
		dwarf::walker::depthfirst_walker<dwarf::print_action,
			dwarf::walker::offset_greater_equal_matcher_t > walk(
				action,
				dwarf::walker::matcher_for_offset_greater_equal(private_offsets_begin)
				);
		walk(info.get_dies(), dies[private_offsets_begin].parent());
	}	
	
	bool elf_module::declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "DECLARE found falsifiable claim at token " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset " << falsifier << " (tag: " << dwarf::tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
			<< ", proceeding to add module info" << std::endl;

		bool retval = false;
		falsifier = ensure_non_toplevel_falsifier(falsifiable, falsifier);
				
		// want to nondestructively make the predicate true
		
		//switch(MAKE_TOKEN_DIE_PAIR(falsifiable->getType(), dies[falsifier].tag()))
		switch(falsifiable->getType())
		{
			//case TOKEN_DIE_PAIR(MEMBERSHIP_CLAIM, DW_TAG_compile_unit):
			case cakeJavaParser::MEMBERSHIP_CLAIM:
			{
				/* We didn't find a member of the specified name. So create one.
				 * We might be creating a *value*,      i.e. a subprogram or a global variable,
				 *                 *or* a *named type*, i.e. a DW_TAG_..._type definition.
				 *                 *or* a *member*      i.e. a DW_TAG_data_member definition. */
				INIT;
				BIND2(falsifiable, memberName);
				BIND2(falsifiable, valueDescriptionExpr); // what do we need to create?
				std::string die_name; // the name of the thing to create
				Dwarf_Off parent_off; // where to create it
				
				switch(memberName->getType())
				{
					case cakeJavaParser::DEFINITE_MEMBER_NAME:
					{
						// is this a toplevel member, or a member in something nested?								
						definite_member_name mn = read_definite_member_name(memberName);
						assert(mn.size() != 0);
						if (mn.size() > 1) 
						{
							definite_member_name containing_die_name(mn.begin(), mn.end() - 1);
							// check whether the containing die exists
							boost::optional<Dwarf_Off> found = dwarf::resolve_die_path(info.get_dies(),
								falsifier, containing_die_name, containing_die_name.begin());
							if (!found)	RAISE(falsifiable, "can't create nested die before parent exists");
							else parent_off = *found;
						}
						else parent_off = falsifier;
						die_name = *(mn.end() - 1);

						/* The DIE doesn't contain a member of this name. 
						 * Make one, if it makes sense to do so. */
						Dwarf_Half meaning_of_value_description;
						bool makes_sense =
							(dies[parent_off].tag() == DW_TAG_compile_unit)
								? (    	valueDescriptionExpr->getType() == cakeJavaParser::VALUE_DESCRIPTION
									|| 	valueDescriptionExpr->getType() == cakeJavaParser::KEYWORD_CLASS_OF)
						:   (dies[parent_off].tag() == DW_TAG_structure_type)
								? (meaning_of_value_description = DW_TAG_member,
								 		valueDescriptionExpr->getType() == cakeJavaParser::VALUE_DESCRIPTION
											&& valueDescriptionExpr->getChild(0)->getType() != cakeJavaParser::LR_SINGLE_ARROW
												// FIXME: refine this: what other value descriptions may appear in a struct?
											)
						: false; // FIXME: add more here
						if (!makes_sense) assert(false); // FIXME: informative output please
						
						break;
					}
					case cakeJavaParser::INDEFINITE_MEMBER_NAME:
						/* only falsified at this point if *no* members? */				
					case cakeJavaParser::REMAINING_MEMBERS:
						/* never falsified at this point? */
						// not supported yet -- I'm not entirely sure what these mean
						assert(false); break;					
					default: RAISE_INTERNAL(falsifiable, "unexpected memberName AST node");
				} // end switch(memberName->getType())

				/* Work out what a value description means in this context. */
				Dwarf_Half tag_to_create =
					(dies[parent_off].tag() == DW_TAG_compile_unit
						&& valueDescriptionExpr->getType() == cakeJavaParser::VALUE_DESCRIPTION
						&& valueDescriptionExpr->getChild(0)->getType() == cakeJavaParser::LR_SINGLE_ARROW)
						? DW_TAG_subprogram
				:	(dies[parent_off].tag() == DW_TAG_compile_unit
						&& valueDescriptionExpr->getType() == cakeJavaParser::VALUE_DESCRIPTION
						&& valueDescriptionExpr->getChild(0)->getType() != cakeJavaParser::LR_SINGLE_ARROW)
						? DW_TAG_variable // FIXME: more precise than   ^-- this
				:	(dies[parent_off].tag() == DW_TAG_compile_unit // other contexts for type definitions?
						&& valueDescriptionExpr->getType() == cakeJavaParser::KEYWORD_CLASS_OF)
						? DW_TAG_structure_type // FIXME: more precise: could be class/union/...
				:	(dies[parent_off].tag() == DW_TAG_structure_type
						&& valueDescriptionExpr->getType() != cakeJavaParser::LR_SINGLE_ARROW) // FIXME
						? DW_TAG_member
				: std::numeric_limits<Dwarf_Half>::max()
				;
				
				if (tag_to_create == std::numeric_limits<Dwarf_Half>::max()) assert(false); // FIXME: informative

				// create the new die
				Dwarf_Off new_die_off;
				dwarf::encap::die::attribute_map default_attrs = default_subprogram_attributes;
				new_die_off = create_new_die(
					parent_off, tag_to_create, empty_attribute_map, empty_child_list);

				// add name, if nonempty
				if (die_name.size() > 0) dies[new_die_off].put_attr(DW_AT_name, dwarf::encap::attribute_value(die_name));

				// FIXME: we want a simple, clean   build_dieset(valueDescription, context) function
				// -- is this what this function does? I think so.
				// Note that build_subprogram_die_children also does the DW_AT_type
				if (tag_to_create == DW_TAG_subprogram)
				{
					build_subprogram_die_children(valueDescriptionExpr, new_die_off);
				}
				// add type
				else
				{
					Dwarf_Off type_off = ensure_dwarf_type(valueDescriptionExpr, falsifier,
						type_name_from_value_description(valueDescriptionExpr));
					dies[new_die_off].put_attr(DW_AT_type, dwarf::encap::attribute_value(
						dwarf::encap::attribute_value::ref(type_off, false)));
				}
				retval = true;
				break;

				//Dwarf_Off parent_off; // the parent of the DIE we're about to create				
				//boost::optional<Dwarf_Off> parent_off = 
				//	find_immediate_container(read_definite_member_name(memberName), falsifier);


			}
			//case TOKEN_DIE_PAIR(MEMBERSHIP_CLAIM, DW_TAG_structure_type):
// 			{
// 				/* Ensure that this structure has this member.
// 				 * We can assume that it doesn't, otherwise this handler
// 				 * wouldn't have been called.
// 				 */
// 
// 				INIT;
// 				BIND2(falsifiable, memberName);
// 				BIND2(falsifiable, valueDescription);
// 
// 				// at the moment, we don't support overriding REMAINING_MEMBERS or nested types
// 				if (memberName->getType() == cakeJavaParser::REMAINING_MEMBERS) RAISE_INTERNAL("override + REMAINING_MEMBERS not yet supported");
// 				if (valueDescription->getType() == KEYWORD_CLASS_OF) RAISE_INTERNAL("creating nested types not yet supported");
// 
// 
// 				retval = true; break; true;
// 			}
			bad_pair:
			default:
				debug_out << "Can't declare unsupported tree--tag pair: " << CCP(falsifiable->getText())
					<< ", " << dwarf::tag_lookup(dies[falsifier].tag()) << std::endl;
				retval = false; break;
		} // end switch
	out:
		// debug printout: print the artificially created DIEs
		debug_print_artificial_dies();
		
		// if we succeeded, the claim should now be true
		if (retval) assert(eval_claim_depthfirst(falsifiable, 
				&cake::module::do_nothing_handler, 
				falsifier));
				
		return retval;	
	}
	
	bool elf_module::override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "OVERRIDE found falsifying module info, at token " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset " << falsifier << " (tag: " << dwarf::tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
			<< ", proceeding to modify module info" << std::endl;
			
		bool retval = false;	
		falsifier = ensure_non_toplevel_falsifier(falsifiable, falsifier);
		
		/* We want to alter the DWARF info to make the predicate true, destructively if necessary.
		 * Do this by
		 * - identifying conflicts and removing them;
		 * - delegating to declare_handler once this is done.
		 */
		 
		switch(falsifiable->getType())
		{
			case cakeJavaParser::MEMBERSHIP_CLAIM:
				/* If we got here, it means that the context doesn't have a member of
				 * the appropriate name/path. Make it so. This just means declare. 
				 * If it *does* have a member of the name, but not matching the description,
				 * we would have got a VALUE_DESCRIPTION or KEYWORD_CLASS_OF falsifiable. */
				debug_out << "override_handler delegating to declare_handler" << std::endl;
				retval = declare_handler(falsifiable, falsifier);
				break;			
			case cakeJavaParser::VALUE_DESCRIPTION:			
			case cakeJavaParser::KEYWORD_CLASS_OF:				
			default:
				assert(false); 
				break;			
		} // end switch

		// debug printout: print the artificially created DIEs
		debug_print_artificial_dies();
		
		// if we succeeded, the claim should now be true
		if (retval) assert(eval_claim_depthfirst(falsifiable, 
				&cake::module::do_nothing_handler, 
				falsifier));
				
		return retval;		
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
		debug_out << "Testing whether DWARF type at offset 0x" << std::hex << off << std::dec
			<< " satisfies description: " << CCP(description->toStringTree()) 
			<< ". DWARF type is currently: " << dies[off] << std::endl;
		INIT;
		/* Notes on typedefs: if we're seeing whether it satisfies an ident, 
		 * that could be a typedef name, so don't follow typedefs right away but one-at-a-time. 
		 * Otherwise, follow typedefs straight away. 
		 * We do this by reassigning off = follow_typedefs(off). */
		switch(description->getType())
		{
			case cakeJavaParser::KEYWORD_OPAQUE:
			case cakeJavaParser::KEYWORD_IGNORED:
			case cakeJavaParser::VALUE_DESCRIPTION:
				// no break;
			case cakeJavaParser::KEYWORD_CLASS_OF:
				return dwarf_type_satisfies(description->getChild(0), off);
				// no break;
			case cakeJavaParser::KEYWORD_PTR: // look for a pointer type
			{
				BIND2(description, pointed_to_type_description); 
					// description's child describes the pointed-to type
				off = follow_typedefs(off);
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
				off = follow_typedefs(off);
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
				off = follow_typedefs(off);
				BIND3(description, encoding, IDENT);
				BIND3(description, attributeList, DWARF_BASE_TYPE_ATTRIBUTE_LIST);				
				return dies[off].tag() == DW_TAG_base_type 		// is a base type
					&& std::string(								// whose encoding matches the encoding ident
						dwarf::encoding_lookup(
							static_cast<Dwarf_Half>(dies[off][DW_AT_encoding].get_unsigned())
						)).substr((std::string("DW_ATE_").size()))
						== CCP(encoding->getText())
					&& eval_claim_depthfirst(attributeList, &cake::module::internal_check_handler, off)
					&& (description->getChildCount() <= 2 ||
							dies[off][DW_AT_byte_size].get_unsigned() ==
								static_cast<Dwarf_Unsigned>(atoll(CCP(description->getChild(2)->getText()))));
			}
			// no break
			case cakeJavaParser::IDENT: // look for any named type
			// HMM... if we can refer to types by name, we're implicitly inserting their existence
			// -- what to do about this? It just means that...
			// ...TODO: we might want to have explicit syntax for checking/declaring/overriding
			// the existence and structure of types. For now, just ensure existence.
			{
				//boost::optional<Dwarf_Off> found = 
				//	find_nearest_type_named(off, CCP(description->getText()));
				//return found != 0;
				return dwarf::tag_is_type(dies[off].tag()) &&
					(
						(dies[off].has_attr(DW_AT_name) && 
							dies[off][DW_AT_name].get_string() == CCP(description->getText()))
						// OR follow a typedef -- FIXME: what about siblings in the typedef tree?
						|| (dies[off].tag() == DW_TAG_typedef && // anon typedefs possible? don't need DW_AT_name
							dies[off].has_attr(DW_AT_type) && // opaque typedefs don't have a type attr
							dwarf_type_satisfies(description, dies[off][DW_AT_type].get_ref().off))
					);
				//std::auto_ptr<dwarf::die_off_list> results(find_dwarf_type_named(description, off));
				// FIXME: we take the head; is this correct? I think so; it's the nearest match
				//return results.get() != 0 && results->size() > 0;
					//results.get() != 0 && std::find(*results, description->getText()) != results->end();
					// FIXME: no, the original logic was correct, but the find_dwarf_type_named() call
					// isn't doing the right thing (it's returning a list containing
					// the offset for struct _GMutex, even though that's not named GtkDialog)
			} 
			case cakeJavaParser::LR_SINGLE_ARROW: // a subprogram type
				// FIXME: we want this to succeed with a dummy
				assert(false);
				break;
			case cakeJavaParser::INDEFINITE_MEMBER_NAME: // always true
				return true;
			case cakeJavaParser::KEYWORD_BASE:
			{
				BIND3(description, baseTypeHead, DWARF_BASE_TYPE);
				return dwarf_type_satisfies(baseTypeHead, off);
			}
			default: 
				debug_out << "Looking for a DWARF type satisfying unrecognised constructor: " <<
					CCP(description->getText()) << std::endl;
				assert(false);
				break;				
		} // end switch
		debug_out << "Warning: fell through bottom of dwarf_type_satisfies switch statement" << std::endl;
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
		debug_out << "Looking for DWARF types satisfying description "
			<< CCP(description->toStringTree()) /*<< " starting from offsets ";
		for (dwarf::die_off_list::iterator iter = list_to_search.begin();
			iter != list_to_search.end();
			iter++)
		{
			debug_out << "0x" << std::hex << *iter << std::dec << " ";			
		}*/ << ", starting from " << list_to_search.size() << " initial offsets";
		if (list_to_search.size() > 0) 
		{
			debug_out << ", beginning 0x"
			<< std::hex << *list_to_search.begin() << std::dec;
		}
		debug_out << std::endl;
		 
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
		debug_out << "Found " << p_retval->size() << " DWARF types satisfying description "
			<< CCP(description->toStringTree());
		if (p_retval->size() > 0)
		{
			debug_out << ", offsets ";
			for (dwarf::die_off_list::iterator iter = p_retval->begin();
				iter != p_retval->end();
				iter++)
			{
				debug_out << "0x" << std::hex << *iter << std::dec << " ";	
			}
		}
		debug_out << std::endl;
		
		return p_retval;
	}
	
	Dwarf_Off elf_module::ensure_dwarf_type(antlr::tree::Tree *description, Dwarf_Off context, boost::optional<std::string> name)
	{
		debug_out << "Ensuring existence of DWARF type satisfying " 
			<< CCP(description->toStringTree());
		if (name) debug_out << " with name " << *name;
		debug_out << std::endl;
		std::auto_ptr<dwarf::die_off_list> plist(find_dwarf_types_satisfying(description, info.type_offsets()));
		if (plist->size() >= 1) return *(plist->begin());
		else
		{
			//assert(build_value_description_handler(description, context));
			return create_dwarf_type_from_value_description(description, context, name);
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
							dies[subprogram_die_off].parent(),
							// the name is just whatever the value description specifies, if anything
							type_name_from_value_description(valueDescriptionExpr)
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
								dies[subprogram_die_off].parent(),
								type_name_from_value_description(n))
							),
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
					debug_out << "Unrecognised function argument description head: " <<
						CCP(functionArgumentDescriptionExpr->getText()) << ", char number " << 
						(int) functionArgumentDescriptionExpr->getType() << " ('_' is " << (int) '_' << ")" << std::endl;
					assert(false); /* only support multivalues and catch-all for now */
					break;
			} // end switch
		} // end binding block	
	} //end function
	
	boost::optional<std::string> elf_module::type_name_from_value_description(antlr::tree::Tree *valueDescription)
	{
		/* This function is used to determine whether a value description is insisting
		 * a particular named type. The only way to do this in current Cake syntax is to
		 * be a toplevel IDENT, since value descriptions are (intentionally) primarily structural. */
		 
		if (valueDescription->getType() == cakeJavaParser::IDENT)
		{
			return std::string(CCP(valueDescription->getText()));
		}
		else
		{
			return 0;
		}
	}	
	
	Dwarf_Off elf_module::create_dwarf_type_from_value_description(antlr::tree::Tree *valueDescription, 
		Dwarf_Off context, boost::optional<std::string> name)
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
				return create_dwarf_type_from_value_description(descriptionHead, context, name);
				} break;
			case cakeJavaParser::KEYWORD_CLASS_OF: {
				// recurse
				BIND2(valueDescription, descriptionHead);
				return create_dwarf_type_from_value_description(descriptionHead, context, name);
				} break;
			case cakeJavaParser::KEYWORD_PTR: {
				// We've been asked to build a new pointer type. First ensure the pointed-to
				// type exists, then create the pointer type.
				BIND2(valueDescription, pointed_to_type_description);
				Dwarf_Off existing_type = ensure_dwarf_type(pointed_to_type_description, 
					context, type_name_from_value_description(valueDescription));				
				dwarf::encap::die::attribute_map::value_type attr_entries[] = {
					std::make_pair(DW_AT_type, dwarf::encap::attribute_value(
						dwarf::encap::attribute_value::ref(existing_type, false)))
				};
				dwarf::encap::die::attribute_map new_attribute_map(
						&attr_entries[0], &attr_entries[array_len(attr_entries)]
						);
				if (name) new_attribute_map.insert(std::make_pair(DW_AT_name,
					dwarf::encap::attribute_value(*name)));
				
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
				if (!name || (std::string(CCP(valueDescription->getText())) == *name))
				{
					// name matches, or no explicit name -- okay
					dwarf::encap::die::attribute_map attribute_map = empty_attribute_map;
					attribute_map.insert(std::make_pair(DW_AT_name, 
						dwarf::encap::attribute_value(std::string(CCP(valueDescription->getText())))));
					return create_new_die(context, DW_TAG_unspecified_type, 
						attribute_map, empty_child_list);
				}
				else // name
				{
					assert(name && *name != std::string(CCP(valueDescription->getText())));
					
					/* We have to create the type, then a typedef with the name. */
					// FIXME -- for now, just flag up occurrences of this peculiar case
					assert(false);
					return 0UL;
				}				
			} break;
			case cakeJavaParser::KEYWORD_OBJECT: {
				/* We've been asked to build a structured object. This means (for now) a struct and not
				 * a union, because in Cake there is no way (yet) to express overlapping members. */
				/*
                        Attribute DW_AT_name, value: (string) Dwarf_Debug_s
                        Attribute DW_AT_byte_size, value: (unsigned) 1216
                        Attribute DW_AT_decl_file, value: (unsigned) 2
                        Attribute DW_AT_decl_line, value: (unsigned) 415
                        DIE, child of 0x11d, tag: DW_TAG_member, offset: 0x12b, name: (string) de_obj_file
                                Attribute DW_AT_name, value: (string) de_obj_file
                                Attribute DW_AT_data_member_location, value: (loclist) {loc described by {0x1: DW_OP_plus_uconst((unsigned) 0); } (for all vaddrs)}
                                Attribute DW_AT_decl_file, value: (unsigned) 5
                                Attribute DW_AT_decl_line, value: (unsigned) 122
                                Attribute DW_AT_type, value: (reference, nonglobal) 0xa81
				*/
				 
				 
				dwarf::encap::die::attribute_map::value_type attr_entries[] = {
					//std::make_pair(DW_AT_byte_size, dwarf::encap::attribute_value((Dwarf_Unsigned 0)) // NOTE: placeholder!
				};
				dwarf::encap::die::attribute_map new_attribute_map(
						&attr_entries[0], &attr_entries[/*array_len(attr_entries)*/0]
						);
				if (name) new_attribute_map.insert(std::make_pair(DW_AT_name,
					dwarf::encap::attribute_value(*name)));
				assert(dwarf::tag_has_named_children(context)); // is it okay to create a type here? this is sloppy! FIXME
				Dwarf_Off new_type_off = create_new_die(context, DW_TAG_structure_type,
					new_attribute_map, empty_child_list);
				
				// now create children for the members
				INIT;
				FOR_ALL_CHILDREN(valueDescription)
				{
					/* Recursively, ensure each member of the structure exists. We do this by
					 * invoking the override handler. */					
					eval_claim_depthfirst(n, &cake::module::override_handler, new_type_off);
				}
				
				// now substitute the complete byte size by looping over all children
				Dwarf_Unsigned total_byte_size = 0;
				for (dwarf::die_off_list::iterator i = dies[new_type_off].children().begin();
					i != dies[new_type_off].children().begin(); i++)
				{
					// FIXME: we should really use a dwarf walker for this
					total_byte_size = std::max(
						total_byte_size, 
						dwarf::evaluator((dies[*i][DW_AT_data_member_location])).tos() + dies[*i][DW_AT_byte_size].get_unsigned());
				}			
				dies[new_type_off].get_attrs().insert(std::make_pair(DW_AT_byte_size, dwarf::encap::attribute_value(total_byte_size)));
				
				return new_type_off;
			}
			case cakeJavaParser::KEYWORD_BASE: {
				/* We've been asked to build a base type. First, build the tag DIE. */
				 dwarf::encap::die::attribute_map::value_type attr_entries[] = {
					std::make_pair(DW_AT_byte_size, dwarf::encap::attribute_value((Dwarf_Unsigned 0)) // NOTE: placeholder!
				};
				dwarf::encap::die::attribute_map new_attribute_map(
						&attr_entries[0], &attr_entries[array_len(attr_entries)]
						);
				if (name) new_attribute_map.insert(std::make_pair(DW_AT_name,
					dwarf::encap::attribute_value(*name)));
				assert(dwarf::tag_has_named_children(context)); // is it okay to create a type here? this is sloppy! FIXME
				Dwarf_Off new_type_off = create_new_die(context, DW_TAG_structure_type,
					new_attribute_map, empty_child_list);
				
				// now build the children
				BIND3(description, header, cakeJavaParser::DWARF_BASE_TYPE_ATTRIBUTE);
				{
					// FIXME: pasted code in need of tailoring
					INIT;
					BIND3(header, attr, IDENT);
					BIND3(header, value, INT);

					std::map<const char *, Dwarf_Half>::iterator found
						= dwarf::attr_forward_map.find(CCP(attr->getText()));
					if (found == dwarf::attr_forward_map.end()) 
					{
						retval = false; break; // not found -- note **break** exits FOR loop!
					}

					// if we got here, the token is a recognised attribute

					std::istringstream istr(CCP(value->getText()));
					signed valueAsInt; istr >> valueAsInt;

					if (dies[current_die][found->second].get_signed() != valueAsInt) 
					{
						// the attribute value in the Cake file doesn't match that in the DWARF info								
						retval = false; break; // doesn't match -- note **break** exits FOR loop!
					}						
				}
					
				
			} break;
			default:
				debug_out << "Asked to build a DWARF type from unsupported value description node: "
					<< CCP(valueDescription->getText()) << std::endl;
				assert(false);
			break;
		}
		debug_out << "Warning: fell through the bottom of create_dwarf_type_from_value_description" << std::endl;
		assert(false);
	}
	
	Dwarf_Off elf_module::create_new_die(const Dwarf_Off parent, const Dwarf_Half tag, 
		dwarf::encap::die::attribute_map const& attrs, dwarf::die_off_list const& children)	
	{
		Dwarf_Off new_off = next_private_offset();
		debug_out << "Warning: creating new DIE at offset 0x" << std::hex << new_off << std::dec 
			<< ", tag " << dwarf::tag_lookup(tag)
			<< " as child of unchecked parent DIE at 0x"
			<< std::hex << parent << std::dec << ", tag " << dwarf::tag_lookup(dies[parent].tag())
			<< std::endl;
		dies.insert(std::make_pair(new_off, dwarf::encap::die(*this, parent, tag, new_off, 
			tag != DW_TAG_compile_unit ? new_off - *find_containing_cu(parent) : 0UL, attrs, children)));
		dies[parent].children().push_back(new_off);	
		return new_off;
	}

	boost::optional<Dwarf_Off> elf_module::find_nearest_type_named(Dwarf_Off context, const char *name)
	{
		namespace w = dwarf::walker;
		
		const w::name_equal_to_matcher_t matcher = w::matcher_for_name_equal_to(std::string(name));
		
		//Dwarf_Half test_tag = dies[context].tag();
		//const w::tag_equal_to_matcher_t test_matcher = w::matcher_for_name_equal_to(t_tag);
		//assert(test_matcher(dies[context]));
		
		typedef w::func1_do_nothing<Dwarf_Half> do_nothing_t;
		typedef w::siblings_upward_walker<do_nothing_t, 
			w::name_equal_to_matcher_t> walker_t;
		
		//boost::optional<Dwarf_Off> (*f)(dwarf::dieset&, Dwarf_Off, const w::tag_equal_to_matcher_t&) 
		//	= &(w::find_first_match<w::tag_equal_to_matcher_t, walker_t>);
		
		return w::find_first_match<w::name_equal_to_matcher_t, walker_t>(dies, context, matcher);
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
		if (dies[off].tag() == DW_TAG_typedef && /* CARE: opaque typedefs have no DW_AT_type! */
			dies[off].has_attr(DW_AT_type)) return dies[off][DW_AT_type].get_ref().off;
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
	
	struct AND_success : dwarf::walker::action {
		typedef bool (elf_module::*module_eval_func)
			(antlr::tree::Tree *, module::eval_event_handler_t, Dwarf_Off);
		bool all_success;
		elf_module& m_module;
		module_eval_func m_evaluator;
		module::eval_event_handler_t m_handler;
		antlr::tree::Tree *m_claim;
		AND_success(elf_module& module, 
			module_eval_func evaluator,
			module::eval_event_handler_t handler, antlr::tree::Tree *claim) 
			: all_success(true), m_module(module), m_evaluator(evaluator), 
			m_handler(handler), m_claim(claim) {}
		void operator()(Dwarf_Off off)
		{
			all_success &= (m_module.*m_evaluator)(m_claim, m_handler, off);
		}
	};
	
	struct match_not_already_claimed : dwarf::walker::matcher {
		std::vector<std::string>& already_claimed;
		const dwarf::encap::die& parent;
		match_not_already_claimed(std::vector<std::string>& already_claimed, const dwarf::encap::die& parent)
			: already_claimed(already_claimed), parent(parent) {}
		bool operator()(dwarf::encap::die& d)
		{
			std::cerr << "match_not_already_claimed: testing DIE at " << std::hex << d.offset() << std::dec
				<< ", idents already claimed (" << already_claimed.size() << "): ";
			for (std::vector<std::string>::iterator i = already_claimed.begin();
				i != already_claimed.end(); i++) 
			{ 
				if (i != already_claimed.begin()) std::cerr << ", ";
				std::cerr << *i;
			}
			std::cerr << std::endl;
			
			return (
				!d.has_attr(DW_AT_name) // match if the thing isn't named...
				||  std::find(already_claimed.begin(), already_claimed.end(),  // or it's *not* already claimed
								d[DW_AT_name].get_string()) == already_claimed.end()
			) && (
			  (parent.tag() == DW_TAG_compile_unit) ? (d.tag() == DW_TAG_subprogram || d.tag() == DW_TAG_variable)
			: (parent.tag() == DW_TAG_structure_type) ? (d.tag() == DW_TAG_member)
			: true
			);
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
		static std::map<antlr::tree::Tree *, std::vector<std::string> > member_state;
		debug_out_filter.indent_level++;
		// HACK: also print another tab
		debug_out << '\t';
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
				debug_out << "Changing handler to " << CCP(strength->getText()) << std::endl;
				recursive_event_handler = handler_for_claim_strength(strength);
				} goto recursively_AND_subclaims;
			case cakeJavaParser::DWARF_BASE_TYPE_ATTRIBUTE_LIST: {
				retval =  true;
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
							retval = false; break; // not found -- note **break** exits FOR loop!
						}
						
						// if we got here, the token is a recognised attribute

						std::istringstream istr(CCP(value->getText()));
						signed valueAsInt; istr >> valueAsInt;

						if (dies[current_die][found->second].get_signed() != valueAsInt) 
						{
							// the attribute value in the Cake file doesn't match that in the DWARF info								
							retval = false; break; // doesn't match -- note **break** exits FOR loop!
						}						
					}							
				}
				break; // retval will be true only if all attributes exist and match
			}
			case cakeJavaParser::KEYWORD_OBJECT: {
				/* We hit a block of claims about named members of the current die. So:
				 * 
				 * claim heads a list of CLAIMs to be evaluated recursively;
				 *
				 * current_die is any DIE defining a *type* with named children. */
				assert(dwarf::tag_is_type(info[current_die].tag()) 
					&& dwarf::tag_has_named_children(info[current_die].tag()));
					
				/* This kind of block supports REMAINING_MEMBERS, which must remember
				 * state across subclaim calls, so create some state. */
				member_state[claim]; // funny syntax; avoids creating a temporary empty vector
				
			 	} goto recursively_AND_subclaims;
				
			case cakeJavaParser::MEMBERSHIP_CLAIM: {
				/* We hit a claim about a named member of the current die. So:
				 * 
				 * claim heads a pair (memberNameExpr, valueDescription);
				 
				 * current_die is either the ultimate parent (0UL), or a compilation unit,
				 * or anything else with named children. */
				BIND2(claim, memberNameExpr);
				//BIND3(claim, valueDescription, VALUE_DESCRIPTION);
				BIND2(claim, valueDescriptionOrClassOf);
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
				 * true if we find some child that satisfies the claim. 
				 *
				 * If we have a REMAINING_MEMBERS claim, we behave much like the first
				 * two cases, but returning true only if *all* remanining members satisfy. */
				
				dwarf::walker::depth_limited_selector max_depth_1(1);
				if (current_die == 0UL && memberNameExpr->getType() == cakeJavaParser::INDEFINITE_MEMBER_NAME)
				{
					debug_out << "looking for any toplevel name satisfying " << CCP(claim->toStringTree()) << std::endl;
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
					debug_out << "looking for any non-toplevel name satisfying " << CCP(claim->toStringTree()) << std::endl;	
					// Success of the claim is success of *the valueDescription claim*
					// on *any* of our child dies.
					OR_success accumulator(*this, &cake::elf_module::eval_claim_depthfirst, 
						handler, valueDescriptionOrClassOf);
						
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
					debug_out << "looking for a toplevel name " << read_definite_member_name(memberNameExpr) << " satisfying " << CCP(claim->toStringTree()) << std::endl;	
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
					debug_out << "looking for a non-toplevel name " << read_definite_member_name(memberNameExpr) << " satisfying " << CCP(claim->toStringTree()) << std::endl;
					// This is the simple case: we just need to find the named element and
					// recursively evaluate the claim on it.
					definite_member_name nm = read_definite_member_name(memberNameExpr);
					
					// truth of the claim is simply existence of the name 
					// (we handle this recursively)
					// and that named element's satisfaction of the property
					
					boost::optional<Dwarf_Off> found = dwarf::resolve_die_path(info.get_dies(),
						current_die, nm, nm.begin());

					retval = (found != 0) //info[current_die].children().end())
						&& eval_claim_depthfirst(valueDescriptionOrClassOf, handler, *found);
				}
				else if (current_die != 0UL && memberNameExpr->getType() == cakeJavaParser::REMAINING_MEMBERS)
				{
					INIT;
					//BIND3(claim, remainingMembers, REMAINING_MEMBERS);
					//BIND3(claim, valueDescription, VALUE_DESCRIPTION);
				
					debug_out << "checking that all non-toplevel names, EXCEPT those already named, satisfy " << CCP(valueDescriptionOrClassOf->toStringTree()) << std::endl;
					AND_success accumulator(*this, &cake::elf_module::eval_claim_depthfirst, 
						&cake::module::do_nothing_handler, valueDescriptionOrClassOf);
					
					// Perform the action (i.e. recursive evaluation) on all dies found
					// which
					// - have a name?
					// - can be said to satisfy a value description (variables, subprograms...)
					//  *and* are among the "primary" classes of nested definition appropriate for the 
					//  containing definition! so globals/subprograms in CUs, members in structs
					// - have not already been claimed about in a preceding sibling claim
					//dwarf::walker::always_match matcher;
					match_not_already_claimed matcher(member_state[claim->getParent()], dies[current_die]);
							
					typedef dwarf::walker::depthfirst_walker<
						AND_success, 
						match_not_already_claimed,
						dwarf::walker::depth_limited_selector> type_of_walker;
					
					type_of_walker walker(
						accumulator, 
						matcher,
						max_depth_1);
					
					// do the walk
					walker(info.get_dies(), current_die);
					
					// now we have a success value in the accumulator
					retval = accumulator.all_success;						
				}
				else if (current_die == 0UL && memberNameExpr->getType() == cakeJavaParser::REMAINING_MEMBERS)
				{
					RAISE(claim, "can't (yet) make claims about all remaining members at top level");
					// FIXME: think carefully how to handle the toplevel "all compilation units" case
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
			
			case cakeJavaParser::KEYWORD_CLASS_OF: {
				/* We hit a claim that a DIE describes a type satisfying a Cake value description. */
				INIT;
				BIND3(claim, valueDescription, VALUE_DESCRIPTION);
				debug_out << "Trying claim about value description with class_of head node " 
					<< CCP(valueDescription->getText())
					<< " on DIE of tag " << dwarf::tag_lookup(dies[current_die].tag())
					<< std::endl;
				retval = dwarf::tag_is_type(dies[current_die].tag())
					&& dwarf_type_satisfies(claim, current_die);					
				break;
			}						
			case cakeJavaParser::VALUE_DESCRIPTION:
				{
					INIT;
					BIND2(claim, valueDescriptionHead);
					debug_out << "Trying claim about value description with head node " 
						<< CCP(valueDescriptionHead->getText())
						<< " on DIE of tag " << dwarf::tag_lookup(dies[current_die].tag())
						<< std::endl;
					if (dies[current_die].tag() == DW_TAG_subprogram)
					{
						retval = dwarf_subprogram_satisfies_description(current_die, claim);
					}
					else if (dies[current_die].tag() == DW_TAG_variable
						|| dies[current_die].tag() == DW_TAG_member)
					{
						retval = dwarf_variable_satisfies_description(current_die, claim);
					}
					//else if (dwarf::tag_is_type(dies[current_die].tag()))
					//{
					//	retval = dwarf_type_satisfies(claim, /*follow_typedefs(*/current_die/*)*/);
					//} // -- this case obsoleted by class_of / KEYWORD_CLASS_OF
					else
					{
						debug_out << "Error: claim is a value description, "
							<< "but DIE is not a subprogram, variable or member." << std::endl;
						retval = false;
					}
						

				} break;
			default: 
				//debug_out << "Unsupported claim head node: " << CCP(claim->getText()) << std::endl;
				RAISE_INTERNAL(claim, "unsupported claim head node");
			recursively_AND_subclaims:
				retval = true;
				FOR_ALL_CHILDREN(claim)
				{
					INIT;
					ALIAS3(n, subclaim, cakeJavaParser::MEMBERSHIP_CLAIM);
					retval &= eval_claim_depthfirst(subclaim, recursive_event_handler, current_die);
					/* Note: if a subclaim is found to be false, the handler will be called *before*
					 * we get a false result. So potentially there is another bite at the cherry here --
					 * we might locally fail to override some DWARF data, but then get the chance
					 * to override it with a bigger, uppermore hammer here. E.g. if the claim is that
					 * something defines a subprogram, and override fails because structs can't contain
					 * subprograms, we get the chance to replace the whole struct with a compile_unit
					 * (slightly fake example). */
					
					/* If we just processed a definite *toplevel* member name, remember it,
					 * so we can evaluate claims about `...' i.e. toplevel members *not* named. */ 
					if (subclaim->getChild(0)->getType() == cakeJavaParser::DEFINITE_MEMBER_NAME
					 && subclaim->getChild(0)->getChildCount() == 1) member_state[claim].push_back(
					 	CCP(subclaim->getChild(0)->getChild(0)->getText()));
				}
				// clear out the per-member state
				member_state.erase(claim);
				//return success;
		}	// end switch
		
		debug_out << "Result of evaluating claim " << CCP(claim->getText()) << " was " << retval << std::endl;
		if (!retval)
		{
			debug_out << "Claim failed, so invoking handler." << std::endl;
			retval |= (this->*handler)(claim, current_die);
		}
	
	out:
		/* Check whether this claim node has any state associated with it, 
		 * -- we should have already cleared it. */
		assert(member_state.find(claim) == member_state.end());
		debug_out_filter.indent_level--;
		// HACK: delete a tab, since they are output eagerly on newlines
		debug_out << '\b';
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

