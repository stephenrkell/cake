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
			dwarf::encap::file(fileno()),
			dies(this->ds()),
			private_offsets_next(private_offsets_begin)
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

	
	bool elf_module::do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "DO_NOTHING found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree(falsifiable))
			<< ", die offset 0x" << std::hex << falsifier << std::dec << ", aborting" << std::endl;		
		return false;	
	}
	
	bool elf_module::check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree(falsifiable))
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

	bool elf_module::declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "DECLARE found falsifiable claim at token " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset " << falsifier << " (tag: " << get_spec().tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
			<< ", proceeding to add module info" << std::endl;

		bool retval = false;
        
        return retval;
        
	}
	
	bool elf_module::override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier)
	{
		debug_out << "OVERRIDE found falsifying module info, at token " //<< CCP(falsifiable->getText())
			<< CCP(falsifiable->toStringTree())
			<< ", die offset " << falsifier << " (tag: " << get_spec().tag_lookup(dies[falsifier].tag())
			<< ", name: " << (dies[falsifier].has_attr(DW_AT_name) ? 
				dies[falsifier][DW_AT_name].get_string() : "no name") << ")"
			<< ", proceeding to modify module info" << std::endl;
			
		bool retval = false;	

		return retval;
	}

	module::eval_event_handler_t elf_module::handler_for_claim_strength(antlr::tree::Tree *strength)
	{
		return
			strength->getType() == cakeJavaParser::KEYWORD_CHECK 	? &cake::module::check_handler
		: 	strength->getType() == cakeJavaParser::KEYWORD_DECLARE 	? &cake::module::declare_handler
		: 	strength->getType() == cakeJavaParser::KEYWORD_OVERRIDE ? &cake::module::override_handler : 0;
	}

	bool elf_module::eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
		Dwarf_Off current_die)
	{
		// For cases where we recursively AND subclaims together, which handler should we use?
		// This variable gets modified by the CHECK, DECLARE and OVERRIDE case-handlers, and
		// otherwise is left alone.
		eval_event_handler_t recursive_event_handler = handler;
		bool retval;
		static std::map<antlr::tree::Tree *, std::vector<std::string> > member_state;
		debug_out.inc_level();
		if (handler != &cake::module::do_nothing_handler) debug_out << "Evaluating claim " << CCP(claim->toStringTree()) 
			<< " on " << dies[current_die] << std::endl;
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
			default: 
				//debug_out << "Unsupported claim head node: " << CCP(claim->getText()) << std::endl;
				RAISE_INTERNAL(claim, "unsupported claim head node");
				assert(false); break; // never hit
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
				/* We've now finished a whole bunch of claims about this has-named-children DIE, 
				 * so clear out the per-member state. */
				member_state.erase(claim);
		}	// end switch
		
		if (handler != &cake::module::do_nothing_handler) debug_out << "Result of evaluating claim " << CCP(claim->getText()) << " was " << retval << std::endl;
		if (!retval)
		{
			if (handler != &cake::module::do_nothing_handler) debug_out << "Claim failed, so invoking handler." << std::endl;
			retval |= (this->*handler)(claim, current_die);
		}
	
	out:
		/* Check whether this claim node has any state associated with it, 
		 * -- we should have already cleared it. */
		assert(member_state.find(claim) == member_state.end());
		debug_out.dec_level();
		return retval;
	} // end function

    const Dwarf_Off elf_module::private_offsets_begin = 1<<30; // 1GB of original DWARF information should be enough
}
