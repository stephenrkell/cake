//#include <gcj/cni.h>
#include <string>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <functional>
#include <iterator>
#include <limits>
//#include <org/antlr/runtime/ClassicToken.h>
//#include "indenting_ostream.hpp"
#include "request.hpp" // includes module.hpp
#include "util.hpp"
//#include "treewalk_helpers.hpp"
#include "parser.hpp"
#include "module.hpp"

using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using std::string;
using dwarf::spec::opt;
using dwarf::spec::basic_die;
using dwarf::spec::type_die;
using dwarf::spec::base_type_die;
using dwarf::spec::pointer_type_die;
using dwarf::spec::typedef_die;
using namespace dwarf;

namespace cake
{
	ifstream_holder::ifstream_holder(std::string& filename) : this_ifstream(filename.c_str(), std::ios::in) 
	{
		if (!this_ifstream) 
		{ 
            throw std::string("file does not exist! ") + filename;
		}
	}

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
		
	void described_module::process_exists_claims(antlr::tree::Tree *existsBody)
	{
		FOR_ALL_CHILDREN(existsBody)
		{
			INIT;
			SELECT_NOT(LR_DOUBLE_ARROW); // we don't want rewrites, only claimGroups
			process_claimgroup(n);
		}
	}

	void described_module::process_supplementary_claim(antlr::tree::Tree *claimGroup)
	{
		process_claimgroup(claimGroup);
	}
	
	void module_described_by_dwarf::process_claimgroup(antlr::tree::Tree *claimGroup)
	{
		INIT;
		bool success = false;
		switch(GET_TYPE(claimGroup))
		{
			case CAKE_TOKEN(KEYWORD_CHECK):
			case CAKE_TOKEN(KEYWORD_DECLARE):
			case CAKE_TOKEN(KEYWORD_OVERRIDE):
				debug_out << "Presented with a claim list of strength " << CCP(GET_TEXT(claimGroup))
					<< std::endl;
				success = eval_claim_depthfirst(
					claimGroup, handler_for_claim_strength(claimGroup), (Dwarf_Off) 0);
				if (!success) RAISE(claimGroup, "referenced file does not satisfy claims");
			break;
			default: RAISE_INTERNAL(claimGroup, "bad claim strength (expected `check', `declare' or `override')");
		}
	}

	/* We have two filenames to worry about: the name that must appear in Makefiles,
     * i.e. relative to the directory containing the Cake source file,
     * and the name that we must use within Cake, relative to our own working directory. */

	elf_module::elf_module(std::string local_filename, std::string makefile_filename) :
			ifstream_holder(local_filename),
			module_described_by_dwarf(makefile_filename, this->ds()),
			dwarf::encap::file(fileno())
	{
		// if no debug information was imported, set up a dummy compilation unit
		if (dies.map_size() <= 1)
		{
        	std::cerr << "Creating a dummy CU!" << std::endl;
			char cwdbuf[4096];
			getcwd(cwdbuf, sizeof cwdbuf);

			dwarf::encap::die::attribute_map::value_type attr_entries[] = {
				std::make_pair(DW_AT_name, dwarf::encap::attribute_value(dies, std::string("__cake_dummy_cu"))),
				std::make_pair(DW_AT_stmt_list, dwarf::encap::attribute_value(dies, (Dwarf_Unsigned) 0U)),
				//std::make_pair(DW_AT_low_pc, dwarf::encap::attribute_value(0, (Dwarf_Addr) 0U)),
				//std::make_pair(DW_AT_high_pc, dwarf::encap::attribute_value(0, (Dwarf_Addr) 0U)),
				std::make_pair(DW_AT_language, dwarf::encap::attribute_value(dies, (Dwarf_Unsigned) 1U)),
				std::make_pair(DW_AT_comp_dir, dwarf::encap::attribute_value(dies, std::string(cwdbuf))),
				std::make_pair(DW_AT_producer, dwarf::encap::attribute_value(dies, std::string(CAKE_VERSION)))
			};

			dwarf::encap::die::attribute_map new_attribute_map(
					&attr_entries[0], &attr_entries[array_len(attr_entries)]
					);			
			std::vector<Dwarf_Off> no_children;
			//create_new_die(0UL, DW_TAG_compile_unit, new_attribute_map, no_children);
		}
        else
        {
        	std::cerr << "Toplevel children CUs at offsets: ";
            for (auto next = dies[0UL]->first_child_offset();
            	next; 
                next = dies[*next]->next_sibling_offset())
            {
				std::cerr << "0x" << std::hex << *next << std::dec << " ";
            }
            std::cerr << std::endl;
        }
        add_imported_function_descriptions();
	}
    
	bool module_described_by_dwarf::do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "DO_NOTHING found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;		
		return false;	
	}
	
	bool module_described_by_dwarf::check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		return false;		
	}
	
	bool module_described_by_dwarf::internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "INTERNAL CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;
		
		return false;		
	}	

	bool module_described_by_dwarf::declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "DECLARE found falsifiable claim at token " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", die offset " << falsifier << " (tag: " << get_spec().tag_lookup(dies[falsifier]->get_tag())
			<< ", name: " << (dies[falsifier]->get_name() ? 
				*dies[falsifier]->get_name(): "no name") << ")"
			<< ", proceeding to add module info" << std::endl;

		bool retval = false;
        
        return retval;
        
	}
	
	bool module_described_by_dwarf::override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "OVERRIDE found falsifying module info, at token " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", die offset " << falsifier << " (tag: " << get_spec().tag_lookup(dies[falsifier]->get_tag())
			<< ", name: " << (dies[falsifier]->get_name() ? 
				*dies[falsifier]->get_name() : "no name") << ")"
			<< ", proceeding to modify module info" << std::endl;
			
		bool retval = false;	

		return retval;
	}

	bool module_described_by_dwarf::eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
		Dwarf_Off current_die)
	{
		// For cases where we recursively AND subclaims together, which handler should we use?
		// This variable gets modified by the CHECK, DECLARE and OVERRIDE case-handlers, and
		// otherwise is left alone.
		eval_event_handler_t recursive_event_handler = handler;
		bool retval;
		static std::map<antlr::tree::Tree *, std::vector<std::string> > member_state;
		debug_out.inc_level();
        // HACK: don't print anything if we're "do nothing" (avoid confusing things)
		if (handler != &cake::module_described_by_dwarf::do_nothing_handler) debug_out 
        	<< "Evaluating claim " << CCP(TO_STRING_TREE(claim)) 
			<< " on " << *(dies[current_die]) << std::endl;
		INIT;
		switch(GET_TYPE(claim))
		{
			case CAKE_TOKEN(KEYWORD_CHECK):
			case CAKE_TOKEN(KEYWORD_DECLARE):
			case CAKE_TOKEN(KEYWORD_OVERRIDE): {
				/* We've hit a new handler specification, so:
				 * 
				 * claim heads a list of claims to be evaluated recursively;
				 *
				 * current_die could be anything, and is simply passed on. */
				
				// HACK: disallow if we're re-using this routine
				if (handler == &cake::described_module::check_handler) assert(false);
				 
				ALIAS2(claim, strength);
				debug_out << "Changing handler to " << CCP(GET_TEXT(strength)) << std::endl;
				recursive_event_handler = handler_for_claim_strength(strength);
				} goto recursively_AND_subclaims;
			default: 
				//debug_out << "Unsupported claim head node: " << CCP(claim->getText()) << std::endl;
				RAISE_INTERNAL(claim, "unsupported claim head node");
				assert(false); break; // never hit
			recursively_AND_subclaims:
				retval = true;
				FOR_ALL_CHILDREN(claim)
				{
					INIT;
					ALIAS3(n, subclaim, CAKE_TOKEN(MEMBERSHIP_CLAIM));
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
					if (GET_TYPE(GET_CHILD(subclaim, 0)) == CAKE_TOKEN(DEFINITE_MEMBER_NAME)
					 && GET_CHILD_COUNT(GET_CHILD(subclaim, 0)) == 1) member_state[claim].push_back(
					 	CCP(GET_TEXT(GET_CHILD(GET_CHILD(subclaim, 0), 0))));
				}
				/* We've now finished a whole bunch of claims about this has-named-children DIE, 
				 * so clear out the per-member state. */
				member_state.erase(claim);
		}	// end switch
		
		if (handler != &cake::module_described_by_dwarf::do_nothing_handler) debug_out 
        	<< "Result of evaluating claim " << CCP(GET_TEXT(claim)) 
            << " was " << retval << std::endl;
		if (!retval)
		{
			if (handler != &cake::module_described_by_dwarf::do_nothing_handler) debug_out 
            	<< "Claim failed, so invoking handler." << std::endl;
			retval |= (this->*handler)(claim, current_die);
		}
	
	out:
		/* Check whether this claim node has any state associated with it, 
		 * -- we should have already cleared it. */
		assert(member_state.find(claim) == member_state.end());
		debug_out.dec_level();
		return retval;
	} // end function

	const Dwarf_Off module_described_by_dwarf::private_offsets_begin = 1<<30; // 1GB of original DWARF information should be enough
	
				
	shared_ptr<type_die> module_described_by_dwarf::existing_dwarf_type(antlr::tree::Tree *t)
	{
		/* We descend the type AST looking for a type that matches. */
		switch(GET_TYPE(t))
		{
			case CAKE_TOKEN(KEYWORD_BASE): {
				// we match base types structurally, i.e. in terms of the encoding
				INIT;
				BIND3(t, header, DWARF_BASE_TYPE);
				{
					int bytesize = -1;
					INIT;
					BIND3(header, encoding, IDENT);
					BIND3(header, baseTypeAttributeList, DWARF_BASE_TYPE_ATTRIBUTE_LIST);
					if (GET_CHILD_COUNT(t) > 2)
					{
						BIND3(header, byteSizeParameter, INT);
						bytesize = atoi(CCP(GET_TEXT(byteSizeParameter)));
					}
					
					/* For each base type in the DWARF info, does it match these? */
					for (auto i_dfs = get_ds().begin(); i_dfs != get_ds().end(); ++i_dfs)
					{
						if ((*i_dfs)->get_tag() == DW_TAG_base_type)
						{
							auto as_base_type = dynamic_pointer_cast<base_type_die>(*i_dfs);
							if (as_base_type)
							{
								bool encoding_matched = false;
								bool attributes_matched = false;
								bool bytesize_matched = false;
								// some things are not supported in dwarfidl yet
								assert(!as_base_type->get_bit_offset() && !as_base_type->get_bit_size());

								if (as_base_type->get_encoding() && 
									string((*i_dfs)->get_spec().encoding_lookup(
										as_base_type->get_encoding()
									)).substr((sizeof "DW_AT_") - 1)
									 == CCP(GET_TEXT(encoding))) encoding_matched = true;
								if ((bytesize == -1 && !as_base_type->get_byte_size())
								|| (as_base_type->get_byte_size() && 
									*as_base_type->get_byte_size() == bytesize)) bytesize_matched = true;
								// HACK: no support for attributes yet
								if (GET_CHILD_COUNT(baseTypeAttributeList) == 0) attributes_matched = true;
								
								if (bytesize_matched 
								&& encoding_matched 
								&& attributes_matched) return dynamic_pointer_cast<type_die>(as_base_type);
							}
						}
					}
				}
			} break;
			case CAKE_TOKEN(IDENT): {
				// we resolve the ident and check it resolves to a type
				definite_member_name dmn; dmn.push_back(CCP(GET_TEXT(t)));
				auto found = this->all_compile_units()->visible_resolve(
					dmn.begin(),
					dmn.end()
				);
				if (found)
				{
					auto as_type = dynamic_pointer_cast<type_die>(found);
					return as_type;
				} else return dynamic_pointer_cast<type_die>(found);
			}
			case CAKE_TOKEN(KEYWORD_OBJECT): {
				// HMM... structural treatment also, it seems. 
				// FIXME: this means dwarfidl can't express 
				// "a structure type, named <like so>, structured <like so...>"
				assert(false);
				} break;
			case CAKE_TOKEN(KEYWORD_PTR): {
				// find the pointed-to type, then find a pointer type pointing to it
				antlr::tree::Tree *pointed_to = GET_CHILD(t, 0);
				auto found = existing_dwarf_type(t);
				
				// special case
				bool is_void_ptr = (GET_CHILD_COUNT(t) == 1)
				 && GET_TYPE(GET_CHILD(t, 0)) == CAKE_TOKEN(KEYWORD_VOID);
				
				if (found || is_void_ptr)
				{
					for (auto i_dfs = get_ds().begin(); i_dfs != get_ds().end(); ++i_dfs)
					{
						if ((*i_dfs)->get_tag() == DW_TAG_pointer_type)
						{
							shared_ptr<pointer_type_die> as_pointer_type
							 = dynamic_pointer_cast<pointer_type_die>(*i_dfs);
							assert(as_pointer_type);
							if (( is_void_ptr && !as_pointer_type->get_type())
							||  (!is_void_ptr &&  (
									as_pointer_type->get_type()->get_offset()
									 == found->get_offset())
								)
							)
							{
								return dynamic_pointer_cast<type_die>(as_pointer_type);
							}
						}
					}
				}
				return found;
			}
			case CAKE_TOKEN(ARRAY): assert(false);
			case CAKE_TOKEN(KEYWORD_VOID): assert(false); // for now
			case CAKE_TOKEN(KEYWORD_ENUM): assert(false); // for now
			case CAKE_TOKEN(FUNCTION_ARROW): assert(false); // for now
			default: assert(false);
		}
	}
	
	shared_ptr<type_die> module_described_by_dwarf::ensure_dwarf_type(antlr::tree::Tree *t)
	{
		auto found = existing_dwarf_type(t);
		if (!found) return create_dwarf_type(t);
		else return found;
	}
	
	shared_ptr<type_die> module_described_by_dwarf::create_dwarf_type(antlr::tree::Tree *t)
	{
		auto first_cu = *this->get_ds().toplevel()->compile_unit_children_begin();
		shared_ptr<encap::basic_die> first_encap_cu = dynamic_pointer_cast<encap::basic_die>(first_cu);

		/* We descend the type AST looking for dependencies. */
		switch(GET_TYPE(t))
		{
			case CAKE_TOKEN(KEYWORD_BASE): 
				// no dependencies
				assert(false);
			case CAKE_TOKEN(IDENT): 
				// error: we can't create a named type without its definition
				throw Not_supported("creating named type without definition");
			
			case CAKE_TOKEN(KEYWORD_OBJECT): 
				// dependencies are the type of each member
				// FIXME: recursive data types require special support to avoid infinite loops here
				assert(false);
			case CAKE_TOKEN(KEYWORD_PTR): {
				auto pointed_to = ensure_dwarf_type(GET_CHILD(t, 0));
				assert(pointed_to);
				auto created =
					dwarf::encap::factory::for_spec(
						dwarf::spec::DEFAULT_DWARF_SPEC
					).create_die(DW_TAG_pointer_type,
						first_encap_cu,
						opt<const string&>() // no name
					);
				auto created_as_pointer_type = dynamic_pointer_cast<encap::pointer_type_die>(created);
				created_as_pointer_type->set_type(pointed_to);
				return dynamic_pointer_cast<spec::type_die>(created_as_pointer_type);
			}
			case CAKE_TOKEN(ARRAY): assert(false);
			case CAKE_TOKEN(KEYWORD_VOID): assert(false); // for now
			case CAKE_TOKEN(KEYWORD_ENUM): assert(false); // for now
			case CAKE_TOKEN(FUNCTION_ARROW): assert(false); // for now
			default: assert(false);
		}
	}

}
