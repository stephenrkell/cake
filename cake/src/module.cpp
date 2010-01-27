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

	elf_module::elf_module(std::string filename) :
			ifstream_holder(filename),
			module_described_by_dwarf(this->ds()),
			dwarf::encap::file(fileno())
	{
		// if no debug information was imported, set up a dummy compilation unit
		if (dies[0UL].children().size() == 0)
		{
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
			//create_new_die(0UL, DW_TAG_compile_unit, new_attribute_map, no_children);
		}			
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
			<< ", die offset " << falsifier << " (tag: " << get_spec().tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
			<< ", proceeding to add module info" << std::endl;

		bool retval = false;
        
        return retval;
        
	}
	
	bool module_described_by_dwarf::override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "OVERRIDE found falsifying module info, at token " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", die offset " << falsifier << " (tag: " << get_spec().tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
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
			<< " on " << dies[current_die] << std::endl;
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
}
