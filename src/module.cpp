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
using std::vector;
using dwarf::spec::opt;
using dwarf::spec::basic_die;
using dwarf::spec::type_die;
using dwarf::spec::base_type_die;
using dwarf::spec::pointer_type_die;
using dwarf::spec::typedef_die;
using dwarf::spec::file_toplevel_die;
using dwarf::spec::with_type_describing_layout_die;
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
		cerr << "Finished processing claims for module " << filename << ". Modified DIEs: " << endl;
		cerr << "*****************************************************" << endl;
		for (auto i_off = touched_dies.begin(); i_off != touched_dies.end(); ++i_off)
		{
			cerr << **(get_ds().find(*i_off)) << endl;
		}
		cerr << "*****************************************************" << endl;
		cerr << "New DIEs:" << endl;
		
		abstract_dieset::iterator first_added_die(
				get_ds().find(greatest_preexisting_offset() + 1),
				abstract_dieset::siblings_policy_sg);
		for (abstract_dieset::iterator i_die = first_added_die;
				i_die != get_ds().end();
				++i_die)
		{
			cerr << **i_die << endl;
		}
		
		//cerr << get_ds() << endl;
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
			<< "Hinted missing element: " << (missing ? CCP(TO_STRING_TREE(missing)) : "(none given)") << endl
			<< "Proceeding to add module info, if we know how." << std::endl;

		assert(falsifier);
		assert(falsifiable);
		bool retval = false;

		// we use the same double-dispatch structure as in eval
		#define TAG_AND_TOKEN(tag, token) ((((long long)(tag)) << ((sizeof (int))<<3)) | (token))
		switch(TAG_AND_TOKEN(falsifier->get_tag(), GET_TYPE(falsifiable)))
		{
			case TAG_AND_TOKEN(0, MEMBERSHIP_CLAIM):
			{
				/* Instead of doing this here, we rely on eval_claim_depthfirst
				 * to find the first CU and recursively eval the claim on that,
				 * hence only calling out to this handler once it finds the part
				 * of the claim that isn't satisfied. */
				assert(false);
// 				/* This is a claim about some top-level member that doesn't exist.
// 				 * We choose the first compile_unit and recurse. */
// 				auto p_toplevel = dynamic_pointer_cast<encap::file_toplevel_die>(falsifier);
// 				if (!p_toplevel 
// 					|| p_toplevel->compile_unit_children_begin()
// 					== p_toplevel->compile_unit_children_end())
// 				{
// 					goto not_supported;
// 				}
// 				else 
// 				{
// 					auto first_cu = *p_toplevel->compile_unit_children_begin();
// 					/* Rather than recursing directly using ourselves, we use 
// 					 * eval_claim_depthfirst to cut */
// 					return declare_handler(
// 						falsifiable, 
// 						dynamic_pointer_cast<spec::basic_die>(first_cu),
// 						missing, 
// 						p_resolver);
// 				}
			} break;
			case TAG_AND_TOKEN(DW_TAG_compile_unit, MEMBERSHIP_CLAIM):
			{
				assert(missing);
				
				// we really should be a compile_unit
				auto p_cu = dynamic_pointer_cast<encap::compile_unit_die>(falsifier);
				assert(p_cu);
				
				INIT;
				BIND2(falsifiable, name);
				BIND2(falsifiable, el);
				assert(missing == el);
				auto mn_to_create = read_definite_member_name(name);
				assert(mn_to_create.size() == 1);
				string name_to_create = mn_to_create.at(0); // unescape_ident(CCP(GET_TEXT(name)));
				
				// we create the missing thing, and give it the name the claim demands
				// NOTE: ideally we would create a thing with a temporary name,
				// by recursively calling ourselves with a "_ : ..."; claim,
				// then just handle the name thing here. 
				// but for now, just do both stages right here.
				// What toplevel things do we know how to create?
				switch(GET_TYPE(el))
				{
					case CAKE_TOKEN(KEYWORD_CLASS_OF): {
						/* This means we should create a type. */
						INIT;
						BIND3(el, descr, VALUE_DESCRIPTION);
						assert(GET_CHILD_COUNT(descr) > 0);
						auto child_descr = GET_CHILD(descr, 0);
						shared_ptr<type_die> t = create_dwarf_type(child_descr);
						if (!t) goto bad_descr; // goes for void too, since we can't name it
						shared_ptr<encap::basic_die> encap_t
						 = dynamic_pointer_cast<encap::basic_die>(t);
						assert(encap_t);
						encap_t->set_name(name_to_create);
						cerr << "Added DWARF type: " << *t << endl;
						assert(eval_claim_depthfirst(
								el,
								t,
								p_resolver,
								&module_described_by_dwarf::do_nothing_handler
							)
						);
						return true;
					}
					case CAKE_TOKEN(VALUE_DESCRIPTION): {
						/* This means we are creating an object (variable or function). */
						INIT;
						BIND2(el, valueDescrHead);
						switch (GET_TYPE(valueDescrHead))
						{
							case CAKE_TOKEN(FUNCTION_ARROW): {
								auto encap_subp = dwarf::encap::factory::for_spec(
									dwarf::spec::DEFAULT_DWARF_SPEC
								).create_die(DW_TAG_subprogram,
									dynamic_pointer_cast<encap::basic_die>(falsifier),
									opt<string>(name_to_create)
								);
								assert(encap_subp);
								cerr << "Added DWARF subprogram: " << *encap_subp << endl;
								// recurse to sort out the arguments
								return eval_claim_depthfirst(
										el, // i.e. the VALUE_DESCRIPTION
										dynamic_pointer_cast<spec::subprogram_die>(encap_subp),
										p_resolver,
										&module_described_by_dwarf::declare_handler
									);
							}
							default: goto bad_descr;
						}
					}
					bad_descr:
					default: cerr << "Don't know how to create a DIE satisfying "
						<< CCP(TO_STRING_TREE(falsifiable)) << endl;
					goto not_supported;
				}
				
				// should not get here
				assert(false);
				
			} break;
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
				
				antlr::tree::Tree *recursive_claim;
				if (GET_TYPE(missing) == CAKE_TOKEN(MEMBERSHIP_CLAIM))
				{
					/* This means we have a name. We handle this now rather than
					 * issuing a confusing recursive claim. */
					INIT;
					BIND2(missing, memberName);
					BIND2(missing, description);
					recursive_claim = description;
					if (GET_TYPE(memberName) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
					{
						auto dmn = read_definite_member_name(memberName);
						assert(dmn.size() == 1);
						created->set_name(dmn.at(0));
						touched_dies.insert(created->get_offset());
					}
					else cerr << "Not setting name of this fp" << endl;
					
				} else recursive_claim = missing;
				
				// recurse on the parameter, to set up its attributes
				bool param_success = eval_claim_depthfirst(
					recursive_claim,
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
				touched_dies.insert(fp->get_offset());
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
			case TAG_AND_TOKEN(DW_TAG_formal_parameter, DEFINITE_MEMBER_NAME): // HACK
			case TAG_AND_TOKEN(DW_TAG_formal_parameter, IDENT): // HACK
			{
				auto fp = dynamic_pointer_cast<encap::formal_parameter_die>(falsifier);
				assert(fp);
				string raw_name = 
					(GET_TYPE(falsifiable) == CAKE_TOKEN(DEFINITE_MEMBER_NAME)) 
						? read_definite_member_name(falsifiable).at(0)
						: string(CCP(GET_TEXT(falsifiable)));
				string unescaped_name = unescape_ident(raw_name);
				fp->set_name(unescaped_name);
				touched_dies.insert(fp->get_offset());
				cerr << "Added name '" << unescaped_name
					<< "' to fp at 0x" 
					<< std::hex << fp->get_offset() << std::dec << endl;
				return true;
			}
			case TAG_AND_TOKEN(DW_TAG_formal_parameter, VALUE_DESCRIPTION):
			{
				auto fp = dynamic_pointer_cast<encap::formal_parameter_die>(falsifier);
				assert(fp);
				// we can only proceed if we currently have no type
				if (fp->get_type()) return false;
				else
				{
					auto type_ast = GET_CHILD(falsifiable, 0);
					auto existing_type = ensure_dwarf_type(type_ast);
					fp->set_type(existing_type);
					touched_dies.insert(fp->get_offset());
					return true;
				}
			}
			not_supported:
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

		assert(falsifier);
		assert(falsifiable);
		bool retval = false;

		// we use the same double-dispatch structure as in eval
		switch(TAG_AND_TOKEN(falsifier->get_tag(), GET_TYPE(falsifiable)))
		{
			case TAG_AND_TOKEN(DW_TAG_typedef, VALUE_DESCRIPTION): {
//				INIT;
//				BIND2(falsifiable, valueDescr);
//				{
					INIT;
					BIND2(/*valueDescr*/falsifiable, typeDescr);
					shared_ptr<encap::typedef_die> is_typedef
					 = dynamic_pointer_cast<encap::typedef_die>(falsifier);
					assert(is_typedef);
					cerr << "Overriding type of typedef " << *is_typedef
						<< " to be that described by " << CCP(TO_STRING_TREE(falsifiable)) << endl;
					shared_ptr<type_die> new_target_type = ensure_dwarf_type(typeDescr);
					assert(new_target_type);
					// for all types identical to our typedef...
					for_all_identical_types(shared_from_this(), is_typedef, 
						// replace their target type with the new target
						[new_target_type, is_typedef](shared_ptr<type_die> t) {
							if (t->get_tag() == DW_TAG_typedef)
							{
								auto typedef_t = dynamic_pointer_cast<spec::typedef_die>(t);
								shared_ptr<type_die> analogous_target_type
								 = get_analogous_type(new_target_type,
									t->enclosing_compile_unit());
								if (!analogous_target_type) 
								{
									cerr << "Warning: analogous target is null in CU " 
										<< t->enclosing_compile_unit()->summary() << endl;
								}
								dynamic_pointer_cast<encap::typedef_die>(t)->set_type(
									analogous_target_type
								);
								cerr << "Success; typedef is now " << *t << endl;
							}
							else cerr << "Hmm: wasn't a typedef: " << *t << endl;
						}
					);
					retval = true;
				}
				break;
//			}
			default: assert(false);
		}

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
						case TAG_AND_TOKEN(DW_TAG_subprogram, MEMBERSHIP_CLAIM):
						case TAG_AND_TOKEN(DW_TAG_compile_unit, MEMBERSHIP_CLAIM):
							RETURN_VALUE_IS( eval_claim_for_with_named_children_and_MEMBERSHIP_CLAIM(
								claim,
								dynamic_pointer_cast<spec::with_named_children_die>(p_d),
								p_resolver,
								handler
							) );
						case TAG_AND_TOKEN(DW_TAG_typedef, ANY_VALUE):
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
						case TAG_AND_TOKEN(DW_TAG_subprogram, VALUE_DESCRIPTION): {
							// a subprogram is its own type/description, so
							// we simply unpack the description and continue
							INIT;
							BIND2(claim, valueDescription);
							RETURN_VALUE_IS( eval_claim_depthfirst(
								valueDescription,
								p_d,
								p_resolver,
								handler) );
						}
						case TAG_AND_TOKEN(DW_TAG_variable, VALUE_DESCRIPTION):
						case TAG_AND_TOKEN(DW_TAG_formal_parameter, VALUE_DESCRIPTION): {
							// variables and fps are described by their type,
							// which may be missing. Check that first.
							auto with_type = dynamic_pointer_cast<with_type_describing_layout_die>(
								p_d);
							if (with_type && with_type->get_type())
							{
								// unpack the description and recurse on the type
								INIT;
								BIND2(claim, valueDescription);
								RETURN_VALUE_IS( eval_claim_depthfirst(
									valueDescription,
									with_type->get_type(),
									p_resolver,
									handler) );
							}
							else
							{
								// we're missing some information.
								RETURN_VALUE_IS ( (this->*handler)(claim, p_d, 0, p_resolver) );
							}
						}
						case TAG_AND_TOKEN(DW_TAG_structure_type, VALUE_DESCRIPTION): {
							INIT;
							BIND2(claim, typeDescr);
							bool success = (GET_TYPE(typeDescr) == CAKE_TOKEN(KEYWORD_OBJECT));
							FOR_ALL_CHILDREN(typeDescr)
							{
								success &= true;
								// check members here
								assert(false);
							}
							RETURN_VALUE_IS(success);
						} break;
						case TAG_AND_TOKEN(DW_TAG_typedef, VALUE_DESCRIPTION): {
							INIT;
							BIND2(claim, typeDescr);
							auto found_all = all_existing_dwarf_types(typeDescr);
							/* If we satisfy one of them, we are okay. */
							bool success = false;
							cerr << "Proceeding to test typedef against " << found_all.size()
								 << " types matching descr." << endl;
							auto is_typedef = dynamic_pointer_cast<dwarf::spec::typedef_die>(p_d);
							unsigned count = 0;
							for (auto i_found = found_all.begin(); i_found != found_all.end();
								++i_found)
							{
								cerr << **i_found << endl;
								success |= data_types_are_identical(is_typedef->get_type(),
									dynamic_pointer_cast<type_die>(*i_found));
// 									
// 									eval_claim_depthfirst(
// 									typeDescr,
// 									p_d,
// 									p_resolver,
// 									&module_described_by_dwarf::do_nothing_handler);
								++count;
								cerr << "After test " << count << " value is " << success << endl;
							}
							if (success) RETURN_VALUE_IS(true);
							else RETURN_VALUE_IS( (this->*handler)(claim, p_d, 0, p_resolver) );
						}
						case TAG_AND_TOKEN(DW_TAG_structure_type, KEYWORD_CLASS_OF): 
						case TAG_AND_TOKEN(DW_TAG_typedef, KEYWORD_CLASS_OF): {
							INIT;
							BIND2(claim, typeDescription);
							//auto is_typedef = dynamic_pointer_cast<spec::typedef_die>(p_d);
							return eval_claim_depthfirst(
								typeDescription,
								p_d,
								p_resolver,
								handler);
						}
						case TAG_AND_TOKEN(DW_TAG_formal_parameter, DEFINITE_MEMBER_NAME):
							// HACK
							if (read_definite_member_name(claim).size() != 1) RAISE(claim,
								"formal parameter names must be atomic");
							// else fall through
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
							// FIXME: we should use recursively_and_subclaims at this point
							// ... but need to accommodate workarounds for too-few-fps
							// ... and ellipsis
							FOR_ALL_CHILDREN(claim)
							{
								if (GET_TYPE(n) == CAKE_TOKEN(ELLIPSIS))
								{
									break; // HACK
								}
								
								if (i_fp == subprogram->formal_parameter_children_end())
								{ 
									ran_out_of_fps = true;
								}
								if (ran_out_of_fps)
								{
									RETURN_VALUE_IS( (this->*handler)(claim, p_d, n, p_resolver) );
								}

								// else
								bool subclaim_is_positional = (GET_TYPE(n) != CAKE_TOKEN(MEMBERSHIP_CLAIM));
								auto p_fp = ran_out_of_fps ? shared_ptr<spec::basic_die>() : *i_fp;
								if (!eval_claim_depthfirst(
									n,
									subclaim_is_positional ? p_fp : subprogram, // HACK
									p_resolver,
									handler)) RETURN_VALUE_IS( (this->*handler)(n, p_fp, 0, p_resolver) );

								if (!ran_out_of_fps) ++i_fp;
							}
							RETURN_VALUE_IS(true);
						}
						case TAG_AND_TOKEN(DW_TAG_pointer_type, KEYWORD_PTR): {
							// this part is true, but overall truth depends on the pointed-to type
							auto as_pointer = dynamic_pointer_cast<spec::pointer_type_die>(p_d);
							assert(as_pointer);
							assert(GET_CHILD_COUNT(claim) == 1);
							//if (!as_pointer->get_type()) 
							//{
							//	// this is a void pointer type
							//}
							//else if (GET_CHILD_COUNT(claim) == 0 || !as_pointer->get_type())
							//{
							//	// we're missing something
							//}
							RETURN_VALUE_IS( eval_claim_depthfirst(
								GET_CHILD(claim, 0),
								as_pointer->get_type(),
								p_resolver,
								handler) );
						}
						case TAG_AND_TOKEN(DW_TAG_unspecified_parameters, VALUE_DESCRIPTION): {
							cerr << "Warning: unspecifid_parameters satisfies no value descriptions."
								<< endl;
							return false; // HACK: do not call handler, for now
						}
						case TAG_AND_TOKEN(DW_TAG_base_type, IDENT):
						case TAG_AND_TOKEN(DW_TAG_pointer_type, IDENT): {
							if (p_d->get_name()
							&& *p_d->get_name() == unescape_ident(CCP(GET_TEXT(claim))))
								RETURN_VALUE_IS(true);
							else
							{
								// HACK: don't call handler, for now
								RETURN_VALUE_IS(false);
							}
						}
						case TAG_AND_TOKEN(0, MEMBERSHIP_CLAIM): {
// 							INIT;
// 							BIND2(claim, name);
// 							if (GET_TYPE(name) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
// 							{
// 								/* This is where we allow the option of resolving names
// 								 * against visible CU-toplevel members instead of
// 								 * starting with the CU names themselves. 
// 								 * The CU-resolver needs a specific CU. */
// 
// 								auto dmn = read_definite_member_name(name);
// 								if (dmn.size() > 0 && dynamic_pointer_cast<spec::with_named_children_die>(p_d) 
// 									&& dynamic_pointer_cast<spec::with_named_children_die>(p_d)->named_child(*dmn.begin()) 
// 									&&
// 									(assert(false), eval_claim_depthfirst( 
// 										claim, // FIXME: this is WRONG! Make a new claim that lacks the first name component
// 										p_d,
// 										&cu_resolver,
// 										handler
// 									))) RETURN_VALUE_IS( true );
// 							}
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
							};// cu_resolver(dynamic_pointer_cast<spec::compile_unit_die>(p_d));

							// we iteratively OR the claims across all CUs
							for (auto i_cu = p_toplevel->compile_unit_children_begin();
								i_cu != p_toplevel->compile_unit_children_end(); ++i_cu)
							{
								cu_resolver this_cu_resolver(*i_cu);
								if (!eval_claim_depthfirst(claim, 
									dynamic_pointer_cast<spec::basic_die>(*i_cu), 
									&this_cu_resolver, 
									&cake::module_described_by_dwarf::do_nothing_handler)) continue;
								else RETURN_VALUE_IS( true );
							}
							// if we got here, all CUs failed. 
							// make the call on the first CU
							if (p_toplevel->compile_unit_children_begin() ==
								p_toplevel->compile_unit_children_end()) RAISE(
									claim, "no compile units");
							auto first_cu = *p_toplevel->compile_unit_children_begin();
							/* Note that when we go through each CU, some of them may come
							 * closer to satisfying the decl than others. We should really fix the
							 * "closest" one. For now, we approximate this as follows. 
							 * - if we are a MEMBERSHIP_CLAIM,
							     and a CU satisfies the bare membership claim
							     (i.e. has a member of that name) but not its subclaims
							     (i.e. the named member doesn't satsify the whole claim)
							     then we run the handler on that CU first. */
							
							if (GET_TYPE(claim) == CAKE_TOKEN(MEMBERSHIP_CLAIM))
							{
								std::ostringstream s;
								s << read_definite_member_name(GET_CHILD(claim, 0))
									<< ": _";
								string simpler_claim_text = s.str();
								auto simpler_claim = build_ast(
									GET_FACTORY(claim),
									CAKE_TOKEN(MEMBERSHIP_CLAIM),
									simpler_claim_text,
									/*(vector<antlr::tree::Tree *>)*/{
										GET_CHILD(claim, 0),
										build_ast(
											GET_FACTORY(claim),
											CAKE_TOKEN(ANY_VALUE),
											"_",
											/*(vector<antlr::tree::Tree *>)*/{}
										)
									}
								);
								for (auto i_cu = p_toplevel->compile_unit_children_begin();
									i_cu != p_toplevel->compile_unit_children_end(); ++i_cu)
								{
									if ( eval_claim_depthfirst(simpler_claim,
											dynamic_pointer_cast<spec::basic_die>(*i_cu), 
											p_resolver,
											&cake::module_described_by_dwarf::do_nothing_handler
										)) 
									{
										/* okay -- we commit to this CU */
										RETURN_VALUE_IS(
											eval_claim_depthfirst(claim,
												dynamic_pointer_cast<spec::basic_die>(*i_cu), 
												p_resolver,
												handler
											)
										);
									}
								}
							}
							// if we got here, either it's not a membership claim
							// or no CU passed the "has this member" test. 
							for (auto i_cu = p_toplevel->compile_unit_children_begin();
								i_cu != p_toplevel->compile_unit_children_end(); ++i_cu)
							{
								if( eval_claim_depthfirst(claim,
										dynamic_pointer_cast<spec::basic_die>(*i_cu), 
										p_resolver,
										handler
									)) RETURN_VALUE_IS(true);
							}
							RETURN_VALUE_IS(false);
							//assert(false);
						}
						abort: 
							cerr << "Claim: " << CCP(TO_STRING_TREE(claim)) << endl;
							cerr << *p_d << endl;
							RAISE_INTERNAL(claim, "not supported");
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
						if (!retval) cerr << "Iterative ANDing of claim group failed at this point." << endl;
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
			<< " on " << (p_d ? p_d->summary() : "(null DIE ptr)")
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
		auto returned = all_existing_dwarf_types(t);
		if (returned.size() > 0) return returned.at(0);
		else return shared_ptr<type_die>();
	}
	
	vector< shared_ptr<type_die> > 
	module_described_by_dwarf::all_existing_dwarf_types(
		antlr::tree::Tree *t
	)
	{
		vector< shared_ptr<type_die> > retval;
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
								&& attributes_matched) retval.push_back(
									dynamic_pointer_cast<type_die>(as_base_type)
									);
							}
						}
					}
				}
			} break;
			case CAKE_TOKEN(IDENT): {
				// we resolve the ident and check it resolves to a type
				definite_member_name dmn; dmn.push_back(unescape_ident(CCP(GET_TEXT(t))));
				auto found = this->get_ds().toplevel()->visible_resolve_all(
					dmn.begin(),
					dmn.end()
				);
				for (auto i_found = found.begin(); i_found != found.end(); ++i_found)
				{
					auto as_type = dynamic_pointer_cast<type_die>(*i_found);
					if (as_type) retval.push_back(as_type);
				}
			} break;
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
								auto candidate = dynamic_pointer_cast<spec::type_die>(as_pointer_type);
								if (candidate) retval.push_back(candidate);
							}
						}
					}
				}
				// if we got here, either 
				// -- we didn't find the pointed-to type (for non-void), so definitely no pointer type; or
				// -- we found the pointed-to but not the pointer
				break;
			}
			case CAKE_TOKEN(ARRAY): assert(false);
			case CAKE_TOKEN(KEYWORD_VOID): break; // void is not reified
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
							retval.push_back(dynamic_pointer_cast<spec::type_die>(as_pointer_type));
						}
					}
				}
				assert(false); //return shared_ptr<spec::type_die>();
			}
			default: assert(false);
		} // end switch
		
		return retval;
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
	
	shared_ptr<spec::structure_type_die>
	module_described_by_dwarf::create_empty_structure_type(
		const string& name
	)
	{
		auto cu = *get_ds().toplevel()->compile_unit_children_begin();
		shared_ptr<encap::basic_die> encap_cu = dynamic_pointer_cast<encap::basic_die>(cu);
		auto created =
			dwarf::encap::factory::for_spec(
				dwarf::spec::DEFAULT_DWARF_SPEC
			).create_die(DW_TAG_structure_type,
				encap_cu,
				opt<string>(name) 
			);
		return dynamic_pointer_cast<spec::structure_type_die>(created);
	}
	
	shared_ptr<spec::reference_type_die>
	module_described_by_dwarf::ensure_reference_type_with_target(
		shared_ptr<spec::type_die> t
	)
	{
		auto gchildren_seq =  get_ds().toplevel()->grandchildren_sequence();
		for (auto i_gchild = gchildren_seq->begin(); i_gchild != gchildren_seq->end();
			++i_gchild)
		{
			if ((*i_gchild)->get_tag() == DW_TAG_reference_type)
			{
				cerr << "Found: " << **i_gchild << endl;
				auto reftype = dynamic_pointer_cast<spec::reference_type_die>(*i_gchild);
				assert(reftype);
				if (reftype->get_type() && reftype->get_type() == t)
				{
					return reftype;
				}
			}
		}
		return create_reference_type_with_target(t);
	}
	
	shared_ptr<spec::pointer_type_die>
	module_described_by_dwarf::ensure_pointer_type_with_target(
		shared_ptr<spec::type_die> t
	)
	{
		auto gchildren_seq =  get_ds().toplevel()->grandchildren_sequence();
		for (auto i_gchild = gchildren_seq->begin(); i_gchild != gchildren_seq->end();
			++i_gchild)
		{
			if ((*i_gchild)->get_tag() == DW_TAG_pointer_type)
			{
				cerr << "Found: " << **i_gchild << endl;
				auto ptrtype = dynamic_pointer_cast<spec::pointer_type_die>(*i_gchild);
				assert(ptrtype);
				if (ptrtype->get_type() && ptrtype->get_type() == t)
				{
					return ptrtype;
				}
			}
		}
		return create_pointer_type_with_target(t);
	}
	
	shared_ptr<spec::reference_type_die>
	module_described_by_dwarf::create_reference_type_with_target(
		shared_ptr<spec::type_die> t
	)
	{
		auto cu = t->enclosing_compile_unit();
		shared_ptr<encap::basic_die> encap_cu = dynamic_pointer_cast<encap::basic_die>(cu);
		auto created =
			dwarf::encap::factory::for_spec(
				dwarf::spec::DEFAULT_DWARF_SPEC
			).create_die(DW_TAG_reference_type,
				encap_cu,
				opt<string>() 
			);
		dynamic_pointer_cast<encap::reference_type_die>(created)->set_type(t);
		return dynamic_pointer_cast<spec::reference_type_die>(created);
	}
	
	shared_ptr<spec::pointer_type_die>
	module_described_by_dwarf::create_pointer_type_with_target(
		shared_ptr<spec::type_die> t
	)
	{
		auto cu = t->enclosing_compile_unit();
		shared_ptr<encap::basic_die> encap_cu = dynamic_pointer_cast<encap::basic_die>(cu);
		auto created =
			dwarf::encap::factory::for_spec(
				dwarf::spec::DEFAULT_DWARF_SPEC
			).create_die(DW_TAG_pointer_type,
				encap_cu,
				opt<string>() 
			);
		dynamic_pointer_cast<encap::pointer_type_die>(created)->set_type(t);
		return dynamic_pointer_cast<spec::pointer_type_die>(created);
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
			
			case CAKE_TOKEN(KEYWORD_OBJECT): {
				// dependencies are the type of each member
				// FIXME: recursive data types require special support to avoid infinite loops here
				auto created =
					dwarf::encap::factory::for_spec(
						dwarf::spec::DEFAULT_DWARF_SPEC
					).create_die(DW_TAG_structure_type,
						first_encap_cu,
						opt<string>() // no name
					);
				auto created_as_structure_type = dynamic_pointer_cast<encap::structure_type_die>(created);
				// FIXME: handle members here
				return dynamic_pointer_cast<spec::type_die>(created_as_structure_type);
			}	
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
			// NOTE: the caller needs to check whether they passed void to distinguish these cases!
			case CAKE_TOKEN(FUNCTION_ARROW): return shared_ptr<spec::type_die>(); // error
			case CAKE_TOKEN(ARRAY): assert(false); // for now
			case CAKE_TOKEN(KEYWORD_ENUM): assert(false); // for now
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
			touched_dies.insert(p_d->get_offset());
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
				touched_dies.insert(p_d->get_offset());
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
				if (found) return eval_claim_depthfirst(subclaim, found, p_resolver, handler);
				else
				{
					// the "not found" case: we call the handler
					// which we hope will create the relevant thing
					return (this->*handler)( claim, p_d, subclaim, p_resolver);
				}
			}
			case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
			case CAKE_TOKEN(ANY_VALUE): {
				for (auto i_child = p_d->children_begin(); 
					i_child != p_d->children_end();
					++i_child)
				{
					// here we are ORing together subclaims
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
