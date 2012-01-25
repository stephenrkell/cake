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
#include <dwarfpp/adt.hpp>

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
using dwarf::spec::file_toplevel_die;
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
    
	module_described_by_dwarf::eval_event_handler_t 
	module_described_by_dwarf::handler_for_claim_strength(antlr::tree::Tree *strength)
	{
		return
			GET_TYPE(strength) == CAKE_TOKEN(KEYWORD_CHECK) 
			? &module_described_by_dwarf::check_handler
			: GET_TYPE(strength) == CAKE_TOKEN(KEYWORD_DECLARE) 
			? &module_described_by_dwarf::declare_handler
			: GET_TYPE(strength) == CAKE_TOKEN(KEYWORD_OVERRIDE) 
			? &module_described_by_dwarf::override_handler : 0;
	}
	
	void module_described_by_dwarf::process_exists_claims(antlr::tree::Tree *existsBody)
	{
		FOR_ALL_CHILDREN(existsBody)
		{
			INIT;
			SELECT_NOT(LR_DOUBLE_ARROW); // we don't want rewrites, only claimGroups
			process_claimgroup(n);
		}
		cerr << "Finished processing claims for module " << filename << endl;
		cerr << "*****************************************************" << endl;
		cerr << get_ds() << endl;
		cerr << "*****************************************************" << endl;
	}

	void module_described_by_dwarf::process_supplementary_claim(antlr::tree::Tree *claimGroup)
	{
		process_claimgroup(claimGroup);
	}
	
	void module_described_by_dwarf::process_claimgroup(antlr::tree::Tree *claimGroup)
	{
		INIT;
		bool success = false;
		/* We have two ways of resolving names:
		 * using visible_resolve on a file_toplevel_die
		 * or
		 * using resolve on any other kind of DIE. 
		 * How to represent this function? Lambdas are no good
		 * because we can't predict their type, so we'd have to
		 * make all these functions templates. Instead, we use a trick
		 * much like anonymous inner classes in Java. */
		struct toplevel_resolver : public name_resolver_t
		{
			shared_ptr<spec::file_toplevel_die> p_toplevel;
			toplevel_resolver(shared_ptr<spec::file_toplevel_die> arg)
			 : p_toplevel(arg) {}
			
			shared_ptr<basic_die> 
			resolve(const definite_member_name& mn)
			{
				return p_toplevel->visible_resolve(mn.begin(), mn.end());
			}
		} resolver(get_ds().toplevel());
		 
		switch(GET_TYPE(claimGroup))
		{
			case CAKE_TOKEN(KEYWORD_CHECK):
			case CAKE_TOKEN(KEYWORD_DECLARE):
			case CAKE_TOKEN(KEYWORD_OVERRIDE):
				debug_out << "Presented with a claim list of strength " << CCP(GET_TEXT(claimGroup))
					<< std::endl;
				success = eval_claim_depthfirst(
					claimGroup, 
					dynamic_pointer_cast<spec::basic_die>(get_ds().toplevel()), 
					&resolver,
					handler_for_claim_strength(claimGroup)
				);
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
			dwarf::encap::file(fileno()),
			module_described_by_dwarf(makefile_filename, this->ds())
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
    
	bool module_described_by_dwarf::do_nothing_handler(
		antlr::tree::Tree *falsifiable, 
		shared_ptr<basic_die> falsifier,
		antlr::tree::Tree *missing,
		module_described_by_dwarf::name_resolver_ptr p_resolver)
	{
		debug_out << "DO_NOTHING found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", " << *falsifier << ", aborting" << std::endl;		
		return false;	
	}
	
	bool module_described_by_dwarf::check_handler(
		antlr::tree::Tree *falsifiable, 
		shared_ptr<basic_die> falsifier,
		antlr::tree::Tree *missing,
		module_described_by_dwarf::name_resolver_ptr p_resolver)
	{
		debug_out << "CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", " << *falsifier << ", aborting" << std::endl;
		
		return false;
	}
	
	bool module_described_by_dwarf::internal_check_handler(
		antlr::tree::Tree *falsifiable, 
		shared_ptr<basic_die> falsifier,
		antlr::tree::Tree *missing,
		module_described_by_dwarf::name_resolver_ptr p_resolver)
	{
		debug_out << "INTERNAL CHECK found falsifiable claim " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< "," << *falsifier << ", aborting" << std::endl;
		
		return false;		
	}	

	bool module_described_by_dwarf::declare_handler(
		antlr::tree::Tree *falsifiable, 
		shared_ptr<basic_die> falsifier,
		antlr::tree::Tree *missing,
		module_described_by_dwarf::name_resolver_ptr p_resolver)
	{
		debug_out << "DECLARE found falsifiable claim at token " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", " << *falsifier << endl
			<< "Proceeding to add module info, if we know how." << std::endl;

		assert(falsifier);
		assert(falsifiable);
		bool retval = false;

		// we use the same double-dispatch structure as in eval
		#define TAG_AND_TOKEN(tag, token) ((((long long)(tag)) << ((sizeof (int))<<3)) | (token))
		switch(TAG_AND_TOKEN(falsifier->get_tag(), GET_TYPE(falsifiable)))
		{
			case TAG_AND_TOKEN(DW_TAG_subprogram, MULTIVALUE):
			{
				// This means that some element in the MULTIVALUE
				// does not have a corresponding fp satisfying it.
				assert(missing);
				// we really should be a subprogram
				shared_ptr<encap::subprogram_die> subprogram
				 = dynamic_pointer_cast<encap::subprogram_die>(falsifier);
				assert(subprogram);
				// in what position should be missing arg be?
				unsigned pos = 0;
				while (GET_CHILD(falsifiable, pos) != missing) 
				{
					++pos;
					assert(pos < GET_CHILD_COUNT(falsifiable));
				}
				
				// assert that this is one more than the current # args 
				// (otherwise we should have been called for the earlier one first)
				assert(pos == srk31::count(
					subprogram->formal_parameter_children_begin(),
					subprogram->formal_parameter_children_end()));
				
				// create the formal parameter
				auto created =
					dwarf::encap::factory::for_spec(
						dwarf::spec::DEFAULT_DWARF_SPEC
					).create_die(DW_TAG_formal_parameter,
						subprogram,
						opt<string>());
						
				cerr << "Added formal parameter at 0x" << std::hex
					<< created->get_offset() << std::dec << endl;
				
				// recurse on the parameter, to set up its attributes
				bool param_success = eval_claim_depthfirst(
					missing,
					dynamic_pointer_cast<spec::basic_die>(created),
					p_resolver,
					&module_described_by_dwarf::declare_handler);
				if (!param_success) return false;
				
				// now continue the evaluation... on the subprogram!
				// NOT on the parameter 
				// -- this is in case there are *more* missing parameters.
				return eval_claim_depthfirst(
					falsifiable,
					falsifier,
					p_resolver,
					&module_described_by_dwarf::declare_handler);
			}
			case TAG_AND_TOKEN(DW_TAG_formal_parameter, KEYWORD_OUT):
			{
				auto fp = dynamic_pointer_cast<encap::formal_parameter_die>(falsifier);
				assert(fp);
				fp->set_is_optional(true);
				fp->set_variable_parameter(true);
				fp->set_const_value(true);
				cerr << "Added 'out' attributes to fp at 0x" 
					<< std::hex << fp->get_offset() << std::dec << endl;
				// now continue the evaluation, on the parameter, in case
				// we have to add more attributes
				return eval_claim_depthfirst(
					falsifiable,
					falsifier,
					p_resolver,
					&module_described_by_dwarf::declare_handler);
			}
			case TAG_AND_TOKEN(DW_TAG_formal_parameter, IDENT): // HACK
			{
				auto fp = dynamic_pointer_cast<encap::formal_parameter_die>(falsifier);
				assert(fp);
				fp->set_name(string(CCP(GET_TEXT(falsifiable))));
				cerr << "Added name '" << CCP(GET_TEXT(falsifiable)) << "' to fp at 0x" 
					<< std::hex << fp->get_offset() << std::dec << endl;
				return true;
			}
			default:
				cerr << "Warning: did not know how to handle 'declare' claim " 
					<< CCP(TO_STRING_TREE(falsifiable)) << " on " << *falsifier
					<< " for module having filename " << filename
					<< endl;
				break;
			
		}

		return retval;
	}
	
	bool module_described_by_dwarf::override_handler(
		antlr::tree::Tree *falsifiable, 
		shared_ptr<basic_die> falsifier,
		antlr::tree::Tree *missing,
		module_described_by_dwarf::name_resolver_ptr p_resolver
	)
	{
		debug_out << "OVERRIDE found falsifying module info, at token " //<< CCP(falsifiable->getText())
			<< CCP(TO_STRING_TREE(falsifiable))
			<< ", " << *falsifier 
			<< ", proceeding to modify module info" << std::endl;
			
		bool retval = false;	

		return retval;
	}

	bool module_described_by_dwarf::eval_claim_depthfirst(
		antlr::tree::Tree *claim, 
		shared_ptr<spec::basic_die> p_d, 
		name_resolver_ptr p_resolver,
		eval_event_handler_t handler)
	{
#define RETURN_VALUE_IS(e)  do { retval = (e); goto return_label; } while (0)
		/* NOTE: we *never* return false directly in this function.
		 * We ALWAYS return what the handler() gives us.
		 * It is okay to return true. */
	
		// For cases where we recursively AND subclaims together, which handler should we use?
		// This variable gets modified by the CHECK, DECLARE and OVERRIDE case-handlers, and
		// otherwise is left alone.
		eval_event_handler_t recursive_event_handler = handler;
		bool retval;
		static std::map<antlr::tree::Tree *, std::vector<std::string> > member_state;
		//debug_out.inc_level();
		
		// HACK: a null pointer satisfies the "void" value description
		// ... or any unnamed member reference
		// HACK: don't print anything if we're "do nothing" (avoid confusing things)
		if (handler != &cake::module_described_by_dwarf::do_nothing_handler)
		{
			debug_out << "Evaluating claim " << CCP(TO_STRING_TREE(claim)) 
				<< " (token type: " << GET_TYPE(claim) << ")"
				<< " on ";
			if (p_d) { debug_out << *p_d; }
			else     { debug_out << "(null DIE)"; }
			debug_out << std::endl;
		}
		// HACK: a null pointer satisfies the "void" value description
		// (but NOT any unnamed member reference
		if (!p_d && (GET_TYPE(claim) == CAKE_TOKEN(KEYWORD_VOID) ))
		          //|| GET_TYPE(claim) == CAKE_TOKEN(INDEFINITE_MEMBER_NAME)
				  //|| GET_TYPE(claim) == CAKE_TOKEN(ANY_VALUE)))
		{ debug_out << ", trivially true." << endl; RETURN_VALUE_IS(true); }
		else if (!p_d && (GET_TYPE(claim) == CAKE_TOKEN(INDEFINITE_MEMBER_NAME)
				  || GET_TYPE(claim) == CAKE_TOKEN(ANY_VALUE)))
		{
			// we can't handle this from here
			assert(false);
		}
		else if (!p_d)
		{
			debug_out << "Warning: eval_claim_depthfirst received an unexpected null DIE." << endl;
			RETURN_VALUE_IS( (this->*handler)(claim, p_d, 0, p_resolver) ); // this just returns false, because of null p_d
		}
		else
		{
			assert(p_d);
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
					if (handler == &module_described_by_dwarf::check_handler) assert(false);

					ALIAS2(claim, strength);
					debug_out << "Changing handler to " << CCP(GET_TEXT(strength)) << std::endl;
					recursive_event_handler = handler_for_claim_strength(strength);
					} goto recursively_AND_subclaims;
				/* We have one case for each dwarfidl head node. */ 
				#define CASE(token) case CAKE_TOKEN(token): { \
			    	 \
				}
				default: {
					/* Let's enumerate the pairings of tag and token that might work. */
					switch(TAG_AND_TOKEN(p_d->get_tag(), GET_TYPE(claim)))
					{
						case TAG_AND_TOKEN(DW_TAG_subprogram, FUNCTION_ARROW):
							RETURN_VALUE_IS( eval_claim_for_subprogram_and_FUNCTION_ARROW(
								claim,
								dynamic_pointer_cast<spec::subprogram_die>(p_d),
								p_resolver,
								handler
							) );
						// FIXME: other with named children can go here
						case TAG_AND_TOKEN(DW_TAG_compile_unit, MEMBERSHIP_CLAIM):
							RETURN_VALUE_IS( eval_claim_for_with_named_children_and_MEMBERSHIP_CLAIM(
								claim,
								dynamic_pointer_cast<spec::with_named_children_die>(p_d),
								p_resolver,
								handler
							) );
						case TAG_AND_TOKEN(DW_TAG_subprogram, ANY_VALUE): 
						case TAG_AND_TOKEN(DW_TAG_variable, ANY_VALUE):
						case TAG_AND_TOKEN(DW_TAG_formal_parameter, ANY_VALUE):
						case TAG_AND_TOKEN(DW_TAG_member, ANY_VALUE):
						case TAG_AND_TOKEN(DW_TAG_base_type, ANY_VALUE): // Hmm?
							RETURN_VALUE_IS(true);
						case TAG_AND_TOKEN(DW_TAG_base_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_structure_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_union_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_class_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_pointer_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_reference_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_const_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_volatile_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_mutable_type, KEYWORD_VOID): // Hmm?
						case TAG_AND_TOKEN(DW_TAG_restrict_type, KEYWORD_VOID): // Hmm?
							RETURN_VALUE_IS(false);
						// typedefs might be void
						case TAG_AND_TOKEN(DW_TAG_typedef, KEYWORD_VOID): 
							RETURN_VALUE_IS( eval_claim_depthfirst(
								claim,
								dynamic_pointer_cast<spec::typedef_die>(p_d)->get_type(),
								p_resolver,
								handler) );
						// FIXME: others which satisfy value descriptions go here
						case TAG_AND_TOKEN(DW_TAG_subprogram, VALUE_DESCRIPTION): 
						case TAG_AND_TOKEN(DW_TAG_variable, VALUE_DESCRIPTION):
						case TAG_AND_TOKEN(DW_TAG_formal_parameter, VALUE_DESCRIPTION): {
							// we simply unpack the description and continue
							INIT;
							BIND2(claim, valueDescription);
							RETURN_VALUE_IS( eval_claim_depthfirst(
								valueDescription,
								dynamic_pointer_cast<spec::with_named_children_die>(p_d),
								p_resolver,
								handler) );
						}
						case TAG_AND_TOKEN(DW_TAG_formal_parameter, IDENT): // HACK
							if (p_d->get_name() && *p_d->get_name() == string(CCP(GET_TEXT(claim))))
							RETURN_VALUE_IS( true );
							else RETURN_VALUE_IS ( (this->*handler)(claim, p_d, 0, p_resolver) );
						case TAG_AND_TOKEN(DW_TAG_formal_parameter, KEYWORD_OUT):
						{
							// is this parameter declared as "out"?
							auto fp = dynamic_pointer_cast<encap::formal_parameter_die>(p_d);
							assert(fp);
							if (fp->get_is_optional() && *fp->get_is_optional()
							 && fp->get_variable_parameter() && *fp->get_variable_parameter()
							 && fp->get_const_value() && *fp->get_const_value()) 
							{
								// okay -- we have to recurse down the chain
								INIT;
								BIND2(claim, nextDescriptor);
								fp->get_type();
								RETURN_VALUE_IS( eval_claim_depthfirst(
									nextDescriptor,
									p_d,
									p_resolver,
									handler) );
							}
							else RETURN_VALUE_IS( (this->*handler)(claim, p_d, 0, p_resolver) );
						}

						case TAG_AND_TOKEN(DW_TAG_subprogram, MULTIVALUE): {
							INIT;
							auto subprogram = dynamic_pointer_cast<spec::subprogram_die>(p_d);
							assert(subprogram);
							bool ran_out_of_fps = false;
							auto i_fp = subprogram->formal_parameter_children_begin();
							FOR_ALL_CHILDREN(claim)
							{
								if (i_fp == subprogram->formal_parameter_children_end())
								{ 
									ran_out_of_fps = true;
								}
								if (ran_out_of_fps)
								{
									RETURN_VALUE_IS( (this->*handler)(claim, p_d, n, p_resolver) );
								}

								// else
								auto p_fp = ran_out_of_fps ? shared_ptr<spec::basic_die>() : *i_fp;
								if (!eval_claim_depthfirst(
									n,
									p_fp,
									p_resolver,
									handler)) RETURN_VALUE_IS( (this->*handler)(n, p_fp, 0, p_resolver) );

								if (!ran_out_of_fps) ++i_fp;
							}
							RETURN_VALUE_IS(true);
						}
						case TAG_AND_TOKEN(0, MEMBERSHIP_CLAIM): {
							INIT;
							BIND2(claim, name);
							if (GET_TYPE(name) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
							{

								// if it succeeds, we have to substitute the resolver...
								struct cu_resolver : public name_resolver_t
								{
									shared_ptr<spec::compile_unit_die> p_cu;
									cu_resolver(shared_ptr<spec::compile_unit_die> arg)
									 : p_cu(arg) {}

									shared_ptr<basic_die> 
									resolve(const definite_member_name& mn)
									{
										return p_cu->resolve(mn.begin(), mn.end());
									}
								} cu_resolver(dynamic_pointer_cast<spec::compile_unit_die>(p_d));

								auto dmn = read_definite_member_name(name);
								if (dmn.size() > 0 && dynamic_pointer_cast<spec::with_named_children_die>(p_d) 
									&& dynamic_pointer_cast<spec::with_named_children_die>(p_d)->named_child(*dmn.begin()) 
									&&
									(assert(false), eval_claim_depthfirst( 
										claim, // FIXME: this is WRONG! Make a new claim that lacks the first name component
										p_d,
										&cu_resolver,
										handler
									))) RETURN_VALUE_IS( true );
							}
							goto try_all_cus;
						}
						default:
						if (p_d->get_tag() == 0)
						{
						try_all_cus:
							// the toplevel case is special
							assert(p_d->get_offset() == 0UL);
							auto p_toplevel = dynamic_pointer_cast<spec::file_toplevel_die>(p_d);
							assert(p_toplevel);

							// we iteratively OR the claims across all CUs
							for (auto i_cu = p_toplevel->compile_unit_children_begin();
								i_cu != p_toplevel->compile_unit_children_end(); ++i_cu)
							{
								if (!eval_claim_depthfirst(claim, *i_cu, p_resolver, handler)) continue;
								else RETURN_VALUE_IS( true );
							}
							RETURN_VALUE_IS( (this->*handler)(claim, p_d, 0, p_resolver) );
							assert(false);
						}
						abort: RAISE_INTERNAL(claim, "not supported");
					}
					debug_out << "Unsupported claim head node: " << CCP(GET_TEXT(claim)) << std::endl;
					RAISE_INTERNAL(claim, "unsupported claim head node");
					assert(false); 
				} break; // never hit
				recursively_AND_subclaims:
					retval = true;
					FOR_ALL_CHILDREN(claim)
					{
						INIT;
						ALIAS3(n, subclaim, CAKE_TOKEN(MEMBERSHIP_CLAIM));
						retval &= eval_claim_depthfirst(
							subclaim, 
							p_d,
							p_resolver,
							recursive_event_handler);
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
		} // end else nonnull

	return_label:		
		if (handler != &cake::module_described_by_dwarf::do_nothing_handler) debug_out 
        	<< "Result of evaluating claim " << CCP(GET_TEXT(claim)) 
            << " was " << retval << std::endl;
		// if (!retval)
		// {
		// 	if (handler != &cake::module_described_by_dwarf::do_nothing_handler) debug_out 
        //     	<< "Claim failed, so invoking handler." << std::endl;
		// 	retval |= (this->*handler)(claim, p_d, 0, p_resolver);
		// }
	
	out:
		/* Check whether this claim node has any state associated with it, 
		 * -- we should have already cleared it. */
		assert(member_state.find(claim) == member_state.end());
		//debug_out.dec_level();
		return retval;
	} // end function

	//const Dwarf_Off module_described_by_dwarf::private_offsets_begin = 1<<30; // 1GB of original DWARF information should be enough
	
				
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
							auto as_base_type = dynamic_pointer_cast<spec::base_type_die>(*i_dfs);
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
									(signed) *as_base_type->get_byte_size() == bytesize)) bytesize_matched = true;
								// HACK: no support for attributes yet
								if (GET_CHILD_COUNT(baseTypeAttributeList) == 0) attributes_matched = true;
								
								if (bytesize_matched 
								&& encoding_matched 
								&& attributes_matched) return dynamic_pointer_cast<type_die>(as_base_type);
							}
						}
					}
				}
				// if we got here, we didn't find it
				return shared_ptr<type_die>();
			} break;
			case CAKE_TOKEN(IDENT): {
				// we resolve the ident and check it resolves to a type
				definite_member_name dmn; dmn.push_back(CCP(GET_TEXT(t)));
				auto found = this->get_ds().toplevel()->visible_resolve(
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
				auto found_pointed_to = existing_dwarf_type(pointed_to);
				
				// special case
				bool is_void_ptr = (GET_CHILD_COUNT(t) == 1)
				 && GET_TYPE(GET_CHILD(t, 0)) == CAKE_TOKEN(KEYWORD_VOID);
				
				if (found_pointed_to || is_void_ptr)
				{
					for (auto i_dfs = get_ds().begin(); i_dfs != get_ds().end(); ++i_dfs)
					{
						if ((*i_dfs)->get_tag() == DW_TAG_pointer_type)
						{
							shared_ptr<spec::pointer_type_die> as_pointer_type
							 = dynamic_pointer_cast<spec::pointer_type_die>(*i_dfs);
							assert(as_pointer_type);
							if (( is_void_ptr && !as_pointer_type->get_type())
							||  (!is_void_ptr &&  (as_pointer_type->get_type() &&
									as_pointer_type->get_type()->get_offset()
									 == found_pointed_to->get_offset())
								)
							)
							{
								return dynamic_pointer_cast<spec::type_die>(as_pointer_type);
							}
						}
					}
				}
				// if we got here, either 
				// -- we didn't find the pointed-to type (for non-void), so definitely no pointer type; or
				// -- we found the pointed-to but not the pointer
				return shared_ptr<spec::type_die>();
			}
			case CAKE_TOKEN(ARRAY): assert(false);
			case CAKE_TOKEN(KEYWORD_VOID): assert(false); // for now
			case CAKE_TOKEN(KEYWORD_ENUM): assert(false); // for now
			case CAKE_TOKEN(FUNCTION_ARROW): assert(false); // for now
			case CAKE_TOKEN(ANY_VALUE):
			{
				// If we're asked to ensure that the "_" type exists, we behave
				// as if asked for a pointer-to-void. HACK: paste the code from above
				for (auto i_dfs = get_ds().begin(); i_dfs != get_ds().end(); ++i_dfs)
				{
					if ((*i_dfs)->get_tag() == DW_TAG_pointer_type)
					{
						shared_ptr<spec::pointer_type_die> as_pointer_type
						 = dynamic_pointer_cast<spec::pointer_type_die>(*i_dfs);
						assert(as_pointer_type);
						if (!as_pointer_type->get_type()) 
						{
							return dynamic_pointer_cast<spec::type_die>(as_pointer_type);
						}
					}
				}
				return shared_ptr<spec::type_die>();
			}
			default: assert(false);
		}
	}
	
	shared_ptr<type_die> module_described_by_dwarf::ensure_dwarf_type(antlr::tree::Tree *t)
	{
		auto found = existing_dwarf_type(t);
		if (!found) return create_dwarf_type(t);
		else return found;
	}
	
	shared_ptr<spec::type_die> 
	module_described_by_dwarf::create_typedef(
		shared_ptr<type_die> p_d,
		const string& name
	)
	{
		auto cu = p_d->enclosing_compile_unit();
		shared_ptr<encap::basic_die> encap_cu = dynamic_pointer_cast<encap::basic_die>(cu);
		auto created =
			dwarf::encap::factory::for_spec(
				dwarf::spec::DEFAULT_DWARF_SPEC
			).create_die(DW_TAG_typedef,
				encap_cu,
				opt<string>(name) 
			);
		dynamic_pointer_cast<encap::typedef_die>(created)->set_type(p_d);
		return dynamic_pointer_cast<spec::type_die>(created);
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
						opt<string>() // no name
					);
				auto created_as_pointer_type = dynamic_pointer_cast<encap::pointer_type_die>(created);
				created_as_pointer_type->set_type(pointed_to);
				return dynamic_pointer_cast<spec::type_die>(created_as_pointer_type);
			}
			// void is not reified in DWARF
			case CAKE_TOKEN(KEYWORD_VOID): return shared_ptr<spec::type_die>(); 
			case CAKE_TOKEN(ARRAY): assert(false); // for now
			case CAKE_TOKEN(KEYWORD_ENUM): assert(false); // for now
			case CAKE_TOKEN(FUNCTION_ARROW): assert(false); // for now
			default: assert(false);
		}
	}
	
	bool 
	module_described_by_dwarf::eval_claim_for_subprogram_and_FUNCTION_ARROW(
		antlr::tree::Tree *claim, 
		shared_ptr<spec::subprogram_die> p_d, 
		name_resolver_ptr p_resolver,
		eval_event_handler_t handler
	)
	{
		INIT;
		BIND2(claim, args);
		BIND2(claim, returnValue);
		
		bool subprogram_began_as_untyped = treat_subprogram_as_untyped(p_d);
		
		bool args_success = eval_claim_depthfirst(
			args,
			p_d,
			p_resolver,
			handler) /* || (this->*handler)(claim, p_d, 0, p_resolver) */;
		if (!args_success) return false;
		// do we have a return type? If not, use the resolver to find one
		if (!p_d->get_type())
		{
			if (GET_TYPE(returnValue) == CAKE_TOKEN(KEYWORD_VOID)) return true;
			shared_ptr<spec::type_die> p_t;
			try
			{
				p_t = ensure_dwarf_type(returnValue);
			}
			catch (Not_supported)
			{
				// this means we just have an ident
				definite_member_name dmn(1, CCP(GET_TEXT(returnValue)));
				auto found = p_resolver->resolve(dmn);
				if (!found) return false;
				auto found_type = dynamic_pointer_cast<spec::type_die>(found);
				if (!found_type) return false;
				
				p_t = found_type;
			}
			// set the type attribute to that type
			dynamic_pointer_cast<encap::subprogram_die>(p_d)->set_type(p_t);
			cerr << "Set type attribute of subprogram at 0x" 
				<< std::hex << p_d->get_offset() << std::dec
				<< " to reference ";
			if (p_t) cerr << *p_t;
			else cerr << "(null reference)";
			cerr << endl;
			return true;
		}
		else 
		{
			cerr << "Non-null return type: " << *p_d->get_type() 
				<< " of subprogram " << *p_d << endl;
				
			// DELETING information: if returnValue is KEYWORD_VOID
			// and we have nonnull type here, delete it.
			// HACK: what's the right way to do this?
			// Since we are 'declare', we don't normally delete information;
			// this is a special case, because we might have "guessed" the
			// 
			
			if (subprogram_began_as_untyped 
				&& GET_TYPE(returnValue) == CAKE_TOKEN(KEYWORD_VOID))
			{
				cerr << "Return type was a guess made by Cake, so removing it." << endl;
				dynamic_pointer_cast<encap::subprogram_die>(p_d)->set_type(
					shared_ptr<spec::type_die>());
				return true;
			}
			else
			{
				return eval_claim_depthfirst(
					returnValue,
					p_d->get_type(),
					p_resolver,
					handler);
			}
		}
	}
	bool module_described_by_dwarf::eval_claim_for_with_named_children_and_MEMBERSHIP_CLAIM(
		antlr::tree::Tree *claim, 
		shared_ptr<spec::with_named_children_die> p_d, 
		name_resolver_ptr p_resolver,
		eval_event_handler_t handler)
	{
		INIT;
		BIND2(claim, name);
		BIND2(claim, subclaim);
		switch(GET_TYPE(name))
		{
			case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
				auto mn = read_definite_member_name(name);
				auto found = p_d->resolve(mn.begin(), mn.end());
				return found && eval_claim_depthfirst(subclaim, found, p_resolver, handler);
			}
			case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
			case CAKE_TOKEN(ANY_VALUE): {
				for (auto i_child = p_d->children_begin(); 
					i_child != p_d->children_end();
					++i_child)
				{
					if (eval_claim_depthfirst(subclaim, *i_child, p_resolver, handler))
					{
						return true;
					}
				}
				return false;
			}
			default: RAISE_INTERNAL(name, "not a valid name");
		}
	}
}
#undef TAG_AND_TOKEN
