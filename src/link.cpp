#include <iostream>
#include <vector>
#include <map>
#include <boost/filesystem/path.hpp>

#include "request.hpp"
#include "parser.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"
#include "cake/cxx_target.hpp"

using boost::make_shared;
using boost::dynamic_pointer_cast;
using boost::shared_ptr;
using boost::optional;
using std::endl;
using std::cerr;
using std::clog;
using std::ostream;
using std::ostringstream;
using std::string;
using std::pair;
using std::make_pair;
using std::boolalpha;
using std::vector;
using std::map;
using std::multimap;
using dwarf::spec::type_die;
using dwarf::spec::subprogram_die;
using dwarf::spec::base_type_die;
using dwarf::spec::pointer_type_die;
using dwarf::spec::opt;
using dwarf::spec::abstract_dieset;
using srk31::indenting_ostream;

namespace cake
{
	string
	link_derivation::get_ns_prefix()
	{
		return "cake::" + namespace_name();
	}

	string 
	link_derivation::get_type_name(shared_ptr<type_die> t)
	{
		const std::string& namespace_prefix
		 = get_type_name_prefix(t);
		 
		return /*m_out <<*/ ((t->get_tag() == DW_TAG_base_type) ?
			*compiler.name_for_base_type(dynamic_pointer_cast<base_type_die>(t))
			: (namespace_prefix + "::" + compiler.fq_name_for(t)));
	}
	
	string 
	link_derivation::get_type_name_prefix(shared_ptr<type_die> t)
	{
		return get_ns_prefix() + "::" + name_of_module(module_of_die(t));
	}
	
	module_ptr 
	link_derivation::module_for_die(boost::shared_ptr<dwarf::spec::basic_die> p_d)
	{
		assert(p_d);
		return module_of_die(p_d);
	}	

	module_ptr 
	link_derivation::module_of_die(boost::shared_ptr<dwarf::spec::basic_die> p_d)
	{
		assert(p_d);
		return module_for_dieset(p_d->get_ds());
	}
	string 
	link_derivation::component_pair_typename(iface_pair ifaces)
	{
		return "component_pair<" 
			+ namespace_name() + "::" + r.module_inverse_tbl[ifaces.first]
			+ "::marker, "
			+ namespace_name() + "::" + r.module_inverse_tbl[ifaces.second]
			+ "::marker>";
	}
	
	string
	link_derivation::get_emitted_sourcefile_name()
	{
		return wrap_file_name;
	}
			
	link_derivation::link_derivation(cake::request& r, antlr::tree::Tree *t,
		const string& id,
		const string& output_module_filename) 
	:	derivation(r, t), 
		compiler(r.compiler),
		output_namespace("link_" + id + "_"), 
		wrap_file_makefile_name(
			boost::filesystem::path(id + "_wrap.cpp").string()),
		wrap_file_name((boost::filesystem::path(r.in_filename).branch_path() 
				/ wrap_file_makefile_name).string()) ,
		//raw_wrap_file(wrap_file_name.c_str()),
		//wrap_file(raw_wrap_file),
		p_wrap_code(new wrapper_file(*this, compiler)), wrap_code(*p_wrap_code)
	{
		/* Add a derived module to the module table, and keep the pointer. */
		assert(r.module_tbl.find(output_module_filename) == r.module_tbl.end());
		this->output_module = module_ptr(
			r.create_derived_module(*this, id, output_module_filename));
		r.module_tbl[id] = output_module;
		r.module_inverse_tbl[output_module] = id;
		
		assert(GET_TYPE(t) == CAKE_TOKEN(KEYWORD_LINK));
		INIT;
		BIND3(t, identList, IDENT_LIST);
		BIND3(t, linkRefinement, PAIRWISE_BLOCK_LIST);
		this->ast = t;
		this->refinement_ast = linkRefinement;
		
		{
			INIT;
			cerr << "Link expression at " << t << " links modules: ";
			FOR_ALL_CHILDREN(identList)
			{
				string module_name = CCP(GET_TEXT(n));
				cerr << module_name << " ";
				request::module_tbl_t::iterator found = r.module_tbl.find(module_name);
				if (found == r.module_tbl.end()) RAISE(n, "module not defined!");
				input_modules.push_back(found->second);
				m_depended_upon_module_names.push_back(module_name);
			}
			cerr << endl;
		}
	}
	
	link_derivation::~link_derivation() 
	{ /* raw_wrap_file.flush(); raw_wrap_file.close(); */ delete p_wrap_code; /* &(*opt_wrap_code);*/ }

	void link_derivation::init()
	{
		// enumerate all interface pairs
		for (vector<module_ptr>::iterator i_mod = input_modules.begin();
				i_mod != input_modules.end();
				++i_mod)
		{
			vector<module_ptr>::iterator copy_of_i_mod = i_mod;
			for (vector<module_ptr>::iterator j_mod = ++copy_of_i_mod;
				j_mod != input_modules.end();
				++j_mod)
			{
				all_iface_pairs.insert(sorted(make_pair(*i_mod, *j_mod)));
			}
		}
			
		// propagate guessed argument info
		// -- we do this sooner rather than later, so that artificial types are available
		// when the relevant correspondences are read in. 
		merge_implicit_dwarf_info();
		// BUT circular dependency....
		
		// add explicit correspondences from the Cake syntax tree
		{
			INIT;
			cerr << "Link expression at " << t << " has pairwise blocks as follows: ";
			FOR_ALL_CHILDREN(refinement_ast)
			{
				INIT;
				assert(GET_TYPE(refinement_ast) == CAKE_TOKEN(PAIRWISE_BLOCK_LIST));
				ALIAS3(n, arrow, BI_DOUBLE_ARROW);
				// now walk each pairwise block and add the correspondences
				{
					INIT;
					BIND3(arrow, leftChild, IDENT);
					BIND3(arrow, rightChild, IDENT);
					BIND3(arrow, pairwiseCorrespondenceBody, CORRESP);
					cerr << CCP(GET_TEXT(leftChild))
						<< " <--> "
						<< CCP(GET_TEXT(rightChild))
						<< endl;
					request::module_tbl_t::iterator found_left = 
						r.module_tbl.find(string(CCP(GET_TEXT(leftChild))));
					if (found_left == r.module_tbl.end()) RAISE(n, "module not defined!");
					request::module_tbl_t::iterator found_right = 
						r.module_tbl.find(string(CCP(GET_TEXT(rightChild))));
					if (found_right == r.module_tbl.end()) RAISE(n, "module not defined!");
					
					add_corresps_from_block(
						found_left->second, 
						found_right->second,
						pairwiseCorrespondenceBody);
				}
			}
			cerr << endl;
			
			// add implicit correpsondences *last*, s.t. explicit ones can take priority
			for (auto i_pair = all_iface_pairs.begin(); 
				i_pair != all_iface_pairs.end();
				++i_pair)
			{
				add_implicit_corresps(*i_pair);
			}
		} // end INIT block
		
		// now add corresps generated by dependency
		for (auto i_pair = all_iface_pairs.begin();
				i_pair != all_iface_pairs.end();
				++i_pair)
		{
			cerr << "Considering interface pair (" << i_pair->first->get_filename()
				<< ", " << i_pair->second->get_filename() << ")" << endl;
			// FIXME: this code is SLOW!
			for (auto i_val_corresp = this->val_corresps.equal_range(*i_pair).first;
					i_val_corresp != this->val_corresps.equal_range(*i_pair).second;
					++i_val_corresp)
			{
				assert(i_val_corresp->second->source == i_pair->first || 
					i_val_corresp->second->source == i_pair->second);
				assert(module_of_die(i_val_corresp->second->source_data_type) == i_pair->first || 
					module_of_die(i_val_corresp->second->source_data_type) == i_pair->second);
				assert(module_of_die(i_val_corresp->second->sink_data_type) == i_pair->first || 
					module_of_die(i_val_corresp->second->sink_data_type) == i_pair->second);
			
				// get the dependencies of this rule
				auto deps_vec = i_val_corresp->second->get_dependencies();
				
				// check that they are satisfied
				for (auto i_dep = deps_vec.begin(); i_dep != deps_vec.end(); ++i_dep)
				{
					auto satisfying_candidates = value_conversion::dep_is_satisfied(
						this->val_corresps.equal_range(*i_pair).first,
						this->val_corresps.equal_range(*i_pair).second,
						*i_dep);
					// if we found a satisfying corresp, carry on...
					if (satisfying_candidates.size() != 0
					/* != this->val_corresps.equal_range(*i_pair).second*/ ) continue; 
					// FIXME: need to do anything here? Hmm, no, C++ compiler will choose. 
					// ... otherwise we need to take action.
					else 
					{
						if (compiler.cxx_assignable_from( /* dest, src */
							i_dep->second, i_dep->first))
						{
						// first check for C++-assignability -- this catches cases of
						// non-name-matched base types that are nevertheless convertible,
						// like char and int.
						// (FIXME: this is a quick-and-dirty C++-specific HACK right now
						// and should be fixed to use knowledge of DWARF base type encodings.)
							cerr << "Falling back on C++-assignability to convert from "
								<< get_type_name(i_dep->first) 
								<< " to " << get_type_name(i_dep->second) << endl;
							continue;
						}

						auto first_fq_name = compiler.fq_name_for(i_dep->first);
						auto second_fq_name = compiler.fq_name_for(i_dep->second);
						cerr << "Found unsatisfied dependency: from "
							<< first_fq_name
							<< " to " << second_fq_name
							<< " required by rule "
							<< *i_val_corresp->second
							<< endl;
						// If the two types do not have the same name, print a
						// bit more info.
						if (first_fq_name != second_fq_name)
						{
							cerr << "Depending rule has source data type as follows." << endl;
							
							auto i_source_start = i_val_corresp->second->source_data_type->iterator_here();
							for (auto i_dfs = i_source_start;
								i_dfs != i_val_corresp->second->source_data_type->get_ds().end()
								&& (i_dfs == i_source_start ||
								 		i_dfs.base().path_from_root.size() > 
											i_source_start.base().path_from_root.size());
								++i_dfs)
							{
								cerr << **i_dfs;
							}

							cerr << " and sink data type as follows." << endl;
							auto i_sink_start = i_val_corresp->second->sink_data_type->iterator_here();
							for (auto i_dfs = i_sink_start;
								i_dfs != i_val_corresp->second->sink_data_type->get_ds().end()
								&& (i_dfs == i_sink_start ||
								 		i_dfs.base().path_from_root.size() > 
											i_sink_start.base().path_from_root.size());
								++i_dfs)
							{
								cerr << **i_dfs;
							}
						}
						
						auto source_data_type = i_dep->first;
						auto sink_data_type = i_dep->second;
						//assert(false);
						auto source_name_parts = compiler.fq_name_parts_for(source_data_type);
						auto sink_name_parts = compiler.fq_name_parts_for(sink_data_type);
						auto source_module = module_of_die(source_data_type);
						assert(source_module == i_pair->first || source_module == i_pair->second);
						auto sink_module = module_of_die(sink_data_type);
						assert(sink_module == i_pair->first || sink_module == i_pair->second);
						add_value_corresp(source_module, 
							source_data_type,
							0,
							sink_module,
							sink_data_type,
							0,
							0,
							i_val_corresp->second->source_is_on_left, // source_is_on_left -- irrelevant as we have no refinement
							make_simple_corresp_expression(i_val_corresp->second->source_is_on_left ?
									source_name_parts : sink_name_parts,
								 i_val_corresp->second->source_is_on_left ?
									sink_name_parts : source_name_parts ));
						
						// FIXME: now we recompute dependencies until we get a fixed point
					}
				}
			}
		}
		
		// assign numbers to value corresps
		// -- this will add any value corresps required for completeness
		assign_value_corresp_numbers();
		
		// which value corresps should we use as initialization rules?
		compute_init_rules();
		
		// generate wrappers
		compute_wrappers();
		
		// which wrappers are actually needed?
		compute_wrappers_needed_and_linker_args();
		
	}
	
	void link_derivation::compute_init_rules()
	{
		for (auto i_pair = all_iface_pairs.begin();
			i_pair != all_iface_pairs.end();
			++i_pair)
		{
			auto candidate_groups = candidate_init_rules[*i_pair];
// 			cerr << "For interface pair (" << name_of_module(i_pair->first)
// 				<< ", " << name_of_module(i_pair->second) << ") there are "
// 				<< candidate_groups.size() << " value corresps that are candidate init rules"
// 				<< endl;
			
			for (auto i_key = candidate_init_rules_tbl_keys[*i_pair].begin();
				i_key != candidate_init_rules_tbl_keys[*i_pair].end();
				++i_key)
			{
				// for each key, we scan its candidates looking for either
				// -- a unique init rule, and any number of update rules
				// -- a unique update rule, and no init rule
				
				auto candidates = candidate_groups.equal_range(*i_key);
				unsigned candidates_count = srk31::count(candidates.first, candidates.second);
				cerr << "For data type " 
					<< (i_key->source_type->get_name() ? *i_key->source_type->get_name() : "(anonymous)")
					<< " at 0x" << std::hex << i_key->source_type->get_offset() << std::dec
					<< " in module " << module_of_die(i_key->source_type)->get_filename()
				//<< *i_key->source_type 
					<< " there are " << candidates_count << " candidate init rules." << endl;
				
				if (candidates_count == 1)
				{
					// we have a unique rule, so we're good
					init_rules_tbl[*i_pair][candidates.first->first]
					 = candidates.first->second;
				}
				else
				{
					optional<
						pair<init_rules_key_t,
							init_rules_value_t>
						> found_init;
					
					unsigned count = 0;
					for (auto i_candidate = candidates.first;
						i_candidate != candidates.second;
						++i_candidate)
					{
						cerr << "candidate " << ++count << " is val_corresp at " << i_candidate->second.get()
							<< " with " << *i_candidate->second << endl;
						
						// we want a unique init rule in here
						if (i_candidate->second->init_only)
						{
							if (found_init) RAISE(i_candidate->second->corresp,
								"multiple initialization rules for the same type "
								"not currently supported");
							else found_init = *i_candidate;
						}
					}

					// if we still don't have it, choose a rule relating a type
					// with the "same" (nominally) type on the other side
					if (!found_init) 
					{
						for (auto i_candidate = candidates.first;
							i_candidate != candidates.second;
							++i_candidate)
						{
							if (data_types_are_nominally_identical(
								i_candidate->second->sink_data_type,
								i_candidate->second->source_data_type
								)
							)
							{
								if (found_init) RAISE_INTERNAL(
									i_candidate->second->corresp,
						"multiple non-init rules declared relating nominally identical data types");
								else found_init = *i_candidate;
							}
						}
					}
					
					
					// if we still don't have it, choose the rules whose
					// declared types (on each side) are their own concrete type
					if (!found_init) 
					{
						for (auto i_candidate = candidates.first;
							i_candidate != candidates.second;
							++i_candidate)
						{
							if (data_types_are_identical(
								i_candidate->second->sink_data_type->get_concrete_type(),
								i_candidate->second->sink_data_type)
							 && data_types_are_identical(
								i_candidate->second->source_data_type->get_concrete_type(),
								i_candidate->second->source_data_type)
							)
							{
								if (found_init) RAISE_INTERNAL(
									i_candidate->second->corresp,
						"multiple non-init rules declared for the same concrete types");
								else found_init = *i_candidate;
							}
						}
					}
					
					// if we still don't have it,
					// prefer skipped rules
					if (!found_init) 
					{
						for (auto i_candidate = candidates.first;
							i_candidate != candidates.second;
							++i_candidate)
						{
							if (dynamic_pointer_cast<skipped_value_conversion>(i_candidate->second))
							{
								if (found_init) RAISE_INTERNAL(i_candidate->second->corresp,
									"multiple rep-compatible and C++-assignable rules for the same concrete types");
								else found_init = *i_candidate;
							}
						}
					}
					
					// if we still don't have it,
					// prefer reinterpret rules
					if (!found_init) 
					{
						for (auto i_candidate = candidates.first;
							i_candidate != candidates.second;
							++i_candidate)
						{
							if (dynamic_pointer_cast<reinterpret_value_conversion>(i_candidate->second))
							{
								if (found_init) RAISE_INTERNAL(i_candidate->second->corresp,
									"multiple rep-compatible (non-C++-assignable) rules for the same concrete types");
								else found_init = *i_candidate;
							}
						}
					}

					// if we still don't have it, prefer rules between like-named types
					if (!found_init) 
					{
						for (auto i_candidate = candidates.first;
							i_candidate != candidates.second;
							++i_candidate)
						{
							if (i_candidate->second->sink_data_type->get_name()
								&& i_candidate->second->source_data_type->get_name()
								&& *i_candidate->second->sink_data_type->get_name()
								== *i_candidate->second->source_data_type->get_name())
							{
								if (found_init) RAISE_INTERNAL(i_candidate->second->corresp,
									"multiple primitive rules for the same concrete types");
								else found_init = *i_candidate;
							}
						}
					}
										
					// if we still don't have it, prefer primitive rules
					if (!found_init) 
					{
						for (auto i_candidate = candidates.first;
							i_candidate != candidates.second;
							++i_candidate)
						{
							if (dynamic_pointer_cast<primitive_value_conversion>(i_candidate->second))
							{
								if (found_init) RAISE_INTERNAL(i_candidate->second->corresp,
									"multiple primitive rules for the same concrete types");
								else found_init = *i_candidate;
							}
						}
					}
					
					if (!found_init) RAISE_INTERNAL(
						candidates.first->second->corresp,
						"BUG: can't handle ambiguous selection of initialization rule ");
					
					init_rules_tbl[*i_pair][found_init->first] = found_init->second;
				}
			}
		}
	}
	
	void link_derivation::merge_implicit_dwarf_info()
	{
		// we walk various parts of the derivation AST and look for
		// - typenames, which we ensure exist
		// - interpretations of arguments, which we use to 
		//   supplement caller-side info that is currently missing
		
		/*
		    This approach is broken because
			we don't know enough about the semantics of the AST at this point.
			We want to do a pre-pass to identify typenames that need adding
			to the DWARF info so that we can correctly read in the correspondences.
			BUT we can't do the more clever inferences,
			such as discovering data types of caller-side arguments,
			until we have build the event correspondences table
			so can associate event patterns with their sink-side call expressions.
			
			In short, we need to do a pre-phase and a post-phase.
			This function just does the pre-phase.
			I have commented out the (unfinished, anyway) code for argument type guessing,
			which needs to be moved to the post-phase.
			Note that there is a second post-phase buried in initial_environment,
			which is guessing artificial typestrings for bindings.
			FIXME: refactor all this stuff into coherent form.
			
		 */
		
		std::vector<antlr::tree::Tree *> event_patterns;
		walk_ast_depthfirst(this->t, event_patterns, 
			[](antlr::tree::Tree *t)
			{
				return (GET_TYPE(t) == CAKE_TOKEN(EVENT_PATTERN));
			});
		std::vector<antlr::tree::Tree *> interpretations;
		walk_ast_depthfirst(this->t, interpretations, 
			[](antlr::tree::Tree *t)
			{
				return (GET_TYPE(t) == CAKE_TOKEN(KEYWORD_AS)
				||      GET_TYPE(t) == CAKE_TOKEN(KEYWORD_INTERPRET_AS)
				||      GET_TYPE(t) == CAKE_TOKEN(KEYWORD_IN_AS)
				||      GET_TYPE(t) == CAKE_TOKEN(KEYWORD_OUT_AS));
			});
		std::vector<antlr::tree::Tree *> stub_calls;
		walk_ast_depthfirst(this->t, stub_calls, 
			[](antlr::tree::Tree *t)
			{
				return (GET_TYPE(t) == CAKE_TOKEN(INVOKE_WITH_ARGS));
			});
		// also build a list of the functions that are arginfo-less
		vector<shared_ptr<spec::subprogram_die> > bare_subprograms;
		for (auto i_mod = input_modules.begin(); i_mod != input_modules.end(); ++i_mod)
		{
			for (auto i_dfs = (*i_mod)->get_ds().begin();
					i_dfs != (*i_mod)->get_ds().end();
					++i_dfs)
			{
				if ((*i_dfs)->get_tag() == DW_TAG_subprogram)
				{
					auto subprogram = dynamic_pointer_cast<
						dwarf::spec::subprogram_die>(*i_dfs);
					if (subprogram->get_declaration()
						&& *subprogram->get_declaration())
					{
						if (subprogram->formal_parameter_children_begin()
							== subprogram->formal_parameter_children_end()
							&& subprogram->unspecified_parameters_children_begin()
							!= subprogram->unspecified_parameters_children_end())
						{
							bare_subprograms.push_back(subprogram);
						}
					}
				}
			}
		}
		
		// we add argument records (with names if we can) to untyped (caller-side) subprograms
		merge_event_pattern_info_into_bare_subprograms(event_patterns, bare_subprograms);
		
		// we ensure that any artificial types mentioned in interpretations exist in their target modules
		// ensure_existence_of_artificial_types(interpretations);
		
	}
// 	void
// 	link_derivation::ensure_existence_of_artificial_types(
// 		const std::vector<antlr::tree::Tree *>& interpretations
// 	)
// 	{
// 		for (auto i_interp = intepretations.begin();
// 			i_interp != interpretations.end(); ++i_interp)
// 		{
// 			// which module?
// 			// GAH: this requires some semantic analysis too.
// 			// So the soonest we can do it is at the same time as reading corresps
// 			source_module->ensure_dwarf_type(
// 				GET_CHILD(interpretation, 0));
// 		}
// 	}
	
	void
	link_derivation::merge_event_pattern_info_into_bare_subprograms(
		const vector<antlr::tree::Tree *>& event_patterns,
		const vector<shared_ptr<spec::subprogram_die> >& bare_subprograms
	)
	{
		/* For each declared function that is untyped, 
		 * we look for an event pattern which matches it
		 * (across all event correspondences)
		 * -- we might get zero or more.
		 * If they don't agree on at least the cardinality of arguments,
		 * we raise a warning (since varargs doesn't work as hoped yet).
		 * We add a DWARF formal_parameter DIE for each of these.
		 * For each of these, we may add a name
		 * -- if each of the found event patterns uses the same name, make that the name.
		 * For each, we may also add a type
		 * -- if the pattern has an ident,
		 *    and that ident is used in the RHS stub,
		 *    and we can work out a type from this
		 */
		
		for (auto i_subp = bare_subprograms.begin(); i_subp != bare_subprograms.end(); ++i_subp)
		{
			
			auto subprogram = *i_subp;
			
			struct pattern_info
			{
				vector<optional<string> > call_argnames;
				vector<antlr::tree::Tree *> call_interps;
				antlr::tree::Tree *pattern;
				//ev_corresp *corresp;
				antlr::tree::Tree *corresp;
			};
			multimap<string, pattern_info> patterns;
			set<string> callnames;
			
			// we've already harvested all the event patterns and calls we know about...
			// BUT they are deprived of module context
			
//					case CAKE_TOKEN(EVENT_WITH_CONTEXT_SEQUENCE): {
// 							INIT;
// 							BIND3(p, sequenceHead, CONTEXT_SEQUENCE);
// 							{
// 								// descend looking for atomic patterns
// 								FOR_ALL_CHILDREN(sequenceHead)
// 								{
// 									switch(GET_TYPE(n))
// 									{
// 										case CAKE_TOKEN(KEYWORD_LET):
// 
// 
// 										case CAKE_TOKEN(ELLIPSIS):
// 											continue;
// 										default: RAISE_INTERNAL(n, 
// 											"not a context predicate");
// 								}
// 							}
// 						assert(false); // see to this later
			
			// FIXME: patterns could be in any link block featuring this module...
			// ... so we just want to check against the source module
			cerr << "Walking our list of " << event_patterns.size() << " event patterns." << endl;
			for (auto i_pattern = event_patterns.begin(); 
				i_pattern != event_patterns.end();
				++i_pattern)
			{
				//auto p = i_corresp->second.source_pattern;
				auto p = *i_pattern;
				assert(GET_TYPE(p) == CAKE_TOKEN(EVENT_PATTERN));

				INIT;
				BIND3(p, eventContext, EVENT_CONTEXT);
				BIND2(p, memberNameExprOrPattern); 
				BIND3(p, eventCountPredicate, EVENT_COUNT_PREDICATE);
				BIND3(p, eventParameterNamesAnnotation, KEYWORD_NAMES);
				vector<optional<string> > argnames;
				vector<antlr::tree::Tree *> interps;
				
				optional<definite_member_name> opt_dmn;
				boost::smatch m;
				if (
					((GET_TYPE(memberNameExprOrPattern) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
					 && (subprogram->get_name() 
					 	&& (opt_dmn = read_definite_member_name(memberNameExprOrPattern))->size() == 1 
							&& opt_dmn->at(0) == *subprogram->get_name())
					)
				|| ((GET_TYPE(memberNameExprOrPattern) == CAKE_TOKEN(KEYWORD_PATTERN))
					 && (subprogram->get_name() 
					 	&& boost::regex_match(
							*subprogram->get_name(), m, 
							regex_from_pattern_ast(memberNameExprOrPattern))
					 	)
					)
				)
				{
					cerr << "Pattern " << CCP(TO_STRING_TREE(p)) << " matched function "
						<< (subprogram->get_name() ? *subprogram->get_name() : "(no name)")
						<< endl;
					// if we came by the pattern route, we didn't read a dmn; fix that
					if (!opt_dmn)
					{
						assert(subprogram->get_name());
						opt_dmn = definite_member_name(1, *subprogram->get_name());
					}
					FOR_REMAINING_CHILDREN(p)
					{
						INIT;
						if (GET_TYPE(n) == CAKE_TOKEN(ELLIPSIS)) break;
						ALIAS3(n, annotatedValueBindingPatternHead, 
							ANNOTATED_VALUE_PATTERN);
						{
							INIT;
							BIND2(annotatedValueBindingPatternHead, arg);
							assert(arg);
							switch(GET_TYPE(arg))
							{
								/* Each arm MUST push an argname and an interp,
								 * even if null. */
							
								case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
									assert(false); // no longer used
								case CAKE_TOKEN(NAME_AND_INTERPRETATION):
								{
									INIT;
									antlr::tree::Tree *interpretation = 0;
									BIND2(arg, memberName);
									if (GET_TYPE(memberName) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
									{
										if (GET_CHILD_COUNT(arg) > 1) {
											interpretation = GET_CHILD(arg, 1);
										}
										ostringstream s;
										s << read_definite_member_name(memberName);
										argnames.push_back(s.str());
										cerr << "Pushed an argument name: " 
											<< s.str() << endl;
									}
									else argnames.push_back(optional<string>());

									interps.push_back(interpretation);
									break;
								}
								case CAKE_TOKEN(KEYWORD_CONST):
									// no information about arg name here
									argnames.push_back(
										optional<string>());
									interps.push_back(0);
									break;
								default: assert(false);

							}
						}
					}
					auto p_corresp = GET_PARENT(*i_pattern);
					assert(GET_TYPE(p_corresp) == CAKE_TOKEN(BI_DOUBLE_ARROW)
					||     GET_TYPE(p_corresp) == CAKE_TOKEN(LR_DOUBLE_ARROW)
					||     GET_TYPE(p_corresp) == CAKE_TOKEN(RL_DOUBLE_ARROW));
					
					// add this pattern
					assert(opt_dmn);
					patterns.insert(make_pair(
						*opt_dmn,
						(pattern_info) {
						argnames,
						interps,
						p,
						p_corresp
						}
					));
					callnames.insert(*opt_dmn);
				} // end if name matches
				else 
				{
// 					cerr << "Pattern " << CCP(TO_STRING_TREE(p)) << " did not match function "
// 						//<< (subprogram->get_name() ? *subprogram->get_name() : "(no name)")
// 						<< *subprogram
// 						<< endl;
					if (!opt_dmn || opt_dmn->size() != 1)
					{
						cerr << "Warning: encountered function name ";
						if (!opt_dmn) cerr << "that is empty.";
						else cerr << "of size " << opt_dmn->size();
						cerr << endl;
					}
				}
			} // end for pattern

			// now we've gathered all the patterns we can, 
			// iterate through them by call names
			for (auto i_callname = callnames.begin(); i_callname != callnames.end();
				++i_callname)
			{
				auto patterns_seq = patterns.equal_range(*i_callname);
				optional<vector<optional< string> > >
					seen_argnames;
				bool argnames_identical = true;
				for (auto i_pattern = patterns_seq.first;
					i_pattern != patterns_seq.second;
					++i_pattern)
				{
					cerr << "Considering argnames vector: (";
					for (auto i_argname = i_pattern->second.call_argnames.begin();
						i_argname != i_pattern->second.call_argnames.end(); ++i_argname)
					{
						if (i_argname != i_pattern->second.call_argnames.begin())
						{ cerr << ", "; }
						cerr << *i_argname;
					}
					cerr << ")" << endl;
					if (!seen_argnames) seen_argnames = i_pattern->second.call_argnames;
					else if (i_pattern->second.call_argnames != *seen_argnames)
					{
						// HMM: seen non-equivalent argnames for the same arg
						argnames_identical = false;
						cerr << "Warning: different patterns use non-identical argnames "
							<< "for subprogram " 
							<< *subprogram->get_name() << endl;
						seen_argnames = optional<vector<optional<string> > >();
					}
				}
				cerr << "Finished reducing patterns for call " << *i_callname
					<< ", ended with ";
				if (!seen_argnames) cerr << "(none)"; else cerr << seen_argnames->size();
				cerr << " args." << endl;
				if (argnames_identical)
				{
					/* Okay, go ahead and add name attrs */
					assert(seen_argnames);
					auto encap_subprogram =
						dynamic_pointer_cast<dwarf::encap::subprogram_die>(
							subprogram);
					for (auto i_name = seen_argnames->begin();
						i_name != seen_argnames->end();
						++i_name)
					{
						auto created =
							dwarf::encap::factory::for_spec(
								dwarf::spec::DEFAULT_DWARF_SPEC
							).create_die(DW_TAG_formal_parameter,
								encap_subprogram,
								*i_name ? 
								opt<string>(**i_name)
								: opt<string>());
						cerr << "created fp of subprogram "
							<< *subprogram->get_name()
							<< " with name " <<  (*i_name ? **i_name : "(no name)")
							<< " at offset " << created->get_offset()
							<< endl;
					}
				}

				for (auto i_pattern = patterns_seq.first;
					i_pattern != patterns_seq.second;
					++i_pattern)
				{
					// plough onwards: for each arg, 
					// where is it used in the RHS?
					// try to get a type expectation,
					// and if we find one, 
					// translate it to the source context
					// HMM. The right way to solve this whole problem
					// is to patch gcc.
					// The next right way to solve this problem
					// is to write a ld wrapper 
					// (except we can't yet *produce* DWARF).
					// This is the third-best way. 
					// So don't spend too much time on it.

					vector<antlr::tree::Tree *>::iterator i_interp
					 =  i_pattern->second.call_interps.begin();
					auto i_fp = subprogram->formal_parameter_children_begin();
					for (auto i_arg = i_pattern->second.call_argnames.begin();
						i_arg != i_pattern->second.call_argnames.end();
						++i_arg, ++i_interp, ++i_fp)
					{
						assert(i_interp != i_pattern->second.call_interps.end()
							&& "as many interps as arguments");

						vector<antlr::tree::Tree *> contexts;
						if (*i_interp)
						{
							INIT;
							/* Given an interpretation, there are several cases: 
							 * - the type just names an existing type;
							 * - the type AST explicitly defines a type that doesn't currently exist in DWARF info;
							 * - the type is an ident which should become an artificial data type
							 * - the type is an ident which should become a virtual data type
							 **/
							BIND2(*i_interp, dwarfType);
							// FIXME: pay attention to the kind (as, in_as, out_as)
							// of the interpretation
							auto existing = module_for_die(subprogram)
								->existing_dwarf_type(dwarfType);
							// if no existing, then try creating one
							if (!existing)
							{
								try
								{
									existing = module_for_die(subprogram)
										->create_dwarf_type(dwarfType);
								} 
								catch (dwarf::lib::Not_supported)
								{
									// If this failed, it means it was just a reference
									// (ident) not a definition.
									// There is nothing we can do now,
									// BUT we will return to this problem
									// in ensure_all_artificial_data_types. 
								}
							}
							if (existing)
							{
								cerr << "existing type: " << *existing << endl;
								// Now let's update the fp
								// FIXME: check for unanimity
								auto encap_fp = dynamic_pointer_cast<encap::formal_parameter_die>(
									*i_fp);
								encap_fp->set_type(existing);
							}

						} // end if *i_interp
						
						// if we get here, there is no explicit interp
						// -- we save handling this for later

					} // end for i_arg
				} // end for i_pattern
			} // end for i_callname
		} // end for i_subp
	}
	
	void 
	link_derivation::find_usage_contexts(const string& ident,
		antlr::tree::Tree *t, vector<antlr::tree::Tree *>& out)
	{
		INIT;
		// first check ourselves
		if (GET_TYPE(t) == CAKE_TOKEN(IDENT) && CCP(GET_TEXT(t)) == ident)
		{
			out.push_back(t);
		}
		// now check our children
		FOR_ALL_CHILDREN(t)
		{
			find_usage_contexts(ident, n, out);
		}
	}
	
	void 
	link_derivation::find_type_expectations_in_stub(const wrapper_file::environment& env,
		module_ptr module,
		antlr::tree::Tree *ast, 
		shared_ptr<dwarf::spec::type_die> current_type_expectation,
		multimap< string, pair< antlr::tree::Tree *, shared_ptr<dwarf::spec::type_die> > >& out)
	{
		/* We walk the stub AST structure, resolving names against the module.
		 * Roughly, where there are static type annotations in the module,
		 * e.g. type info for C function signatures,
		 * we infer C++ static type requirements for any idents used.
		 * This might include typedefs, i.e. we don't concretise. */
		 
		// HACK: First, record the lowest ellipsis begin argpos in the environment
		unsigned lowest_ellipsis_argpos = std::numeric_limits<unsigned>::max();
		for (auto i_binding = env.begin(); i_binding != env.end(); ++i_binding)
		{
			if (i_binding->second.from_ellipsis)
			{
				std::istringstream cur_s(i_binding->first.substr(string("__cake_arg").length()));
				unsigned curpos;
				cur_s >> curpos;

				if (curpos < lowest_ellipsis_argpos) lowest_ellipsis_argpos = curpos;
			}
		}
		cerr << "Finding type expectations in stub " << CCP(TO_STRING_TREE(ast)) << endl;
		cerr << "Lowest ellipsis argpos: " << lowest_ellipsis_argpos << endl;
		
		/* This node matches the current Cakename --
		 * except in the case of ellipsis, in which case
		 * it only potentially matches.  Anyway, find the DWARF expectations
		 * for this element. */
		auto dwarf_context = map_ast_context_to_dwarf_element(ast,
			module,
			false);
		cerr << "DWARF context is " << (dwarf_context ? dwarf_context->summary() : "(none)") << endl;
		auto with_type = dynamic_pointer_cast<spec::with_type_describing_layout_die>(dwarf_context); 

		/* New implementation: for each ident / DMN in the stub, we map it to a 
		 * DWARF context and see if that has a type. */
		optional<definite_member_name> opt_dmn;
		string ident;
		if (GET_TYPE(ast) == CAKE_TOKEN(IDENT)) ident = unescape_ident(CCP(GET_TEXT(ast)));
		if (GET_TYPE(ast) == CAKE_TOKEN(DEFINITE_MEMBER_NAME)) 
		{
			opt_dmn = read_definite_member_name(ast);
		}

		if (GET_TYPE(ast) == CAKE_TOKEN(IDENT)
		||  GET_TYPE(ast) == CAKE_TOKEN(DEFINITE_MEMBER_NAME)
		||  GET_TYPE(ast) == CAKE_TOKEN(ELLIPSIS)
		||  GET_TYPE(ast) == CAKE_TOKEN(KEYWORD_IN_ARGS))
		{
			cerr << "Hit leaf case." << endl;
			if (with_type)
			{

				if (GET_TYPE(ast) == CAKE_TOKEN(ELLIPSIS)
				||  GET_TYPE(ast) == CAKE_TOKEN(KEYWORD_IN_ARGS))
				{
					cerr << "Hit ellipsis/in_args case" << endl;
					// This means we found an ellipsis in the stub,
					// and just retrieved the *first* fp in the position of that ellipsis.
					// It only matches if the current cakename corresponds to an argument
					// that is going to be mapped to that position.
					// We might be doing this positionally or by name.
					// For the by-name case, it means
					// -- our cakename should be friendly, and
					//    we look for an fp having that name;
					// For the positional case, it means
					// -- our cakename should be nonfriendly, and
					//    we look for the fp at the same offset within the ellipsis as our nonfriendly name.

					assert(with_type->get_tag() == DW_TAG_formal_parameter);
					auto subprogram = dynamic_pointer_cast<subprogram_die>(with_type->get_parent());
					abstract_dieset::iterator tmp_iter(with_type->iterator_here(),
						abstract_dieset::siblings_policy_sg);
					subprogram_die::formal_parameter_iterator i_fp
					 = subprogram->formal_parameter_children_begin();
					while (i_fp.base().base() != tmp_iter) ++i_fp;

					auto fps_end = subprogram->formal_parameter_children_end();

					for (auto i_binding = env.begin(); i_binding != env.end(); ++i_binding)
					{

						if (i_binding->second.from_ellipsis
						&& *i_binding->second.from_ellipsis == -1) // non-positional
						{
							cerr << "Cakename " << i_binding->first << " comes from non-positional ellipsis." << endl;
							// the name-matching case -- look for an arg that matches
							for (; i_fp != fps_end; ++i_fp)
							{
								if ((*i_fp)->get_name() && *(*i_fp)->get_name() == i_binding->first) break;
							}
						}
						else if (i_binding->second.from_ellipsis
						&& *i_binding->second.from_ellipsis != -1) // positional
						{
							cerr << "Cakename " << i_binding->first << " comes from positional ellipsis, ";
							// what position does the current cakename fall within the ellipsis?
							std::istringstream cur_s(i_binding->first.substr(string("__cake_arg").length()));
							unsigned curpos;
							cur_s >> curpos;

							unsigned offset = curpos - lowest_ellipsis_argpos;
							cerr << "arglist position " << curpos << ", offset " << offset
								<< " from start of ellipsis." << endl;

							for (unsigned j = 0; j < offset; ++j) 
							{
								if (i_fp != fps_end) ++i_fp;
							}
						}
						else
						{
							// this is a binding not from an ellipsis, so don't match it here
							continue;
						}
						if (i_fp != subprogram->formal_parameter_children_end()
							&& (*i_fp)->get_type())
						{
							cerr << "Adding an expectation for Cakename " << i_binding->first
								<< " originating in fp " << (*i_fp)->summary()
								<< " having type " 
								<< ((*i_fp)->get_type() ? (*i_fp)->get_type()->summary() : "") 
								<< endl;
							out.insert(make_pair(
								i_binding->first, 
								make_pair(
									ast, 
									(*i_fp)->get_type()
								)
							));
						}
					} // next binding
				}
				else // not ellipsis
				{
					out.insert(make_pair(opt_dmn ? opt_dmn->at(0) : ident, make_pair(ast, with_type->get_type())));
				}
			} // end if with_type
		} // end if a leaf node
		else // not ident, dmn, ellipsis or in_args
		{
			// recurse
			INIT;
			FOR_ALL_CHILDREN(ast)
			{
				find_type_expectations_in_stub(env, module, n, current_type_expectation, out);
			}
		}
		
// 		map<int, shared_ptr<dwarf::spec::type_die> > child_type_expectations;
// 		antlr::tree::Tree *parent_whose_children_to_walk = stub;
// 		switch(GET_TYPE(stub))
// 		{
// 			case CAKE_TOKEN(INVOKE_WITH_ARGS):
// 				{
// 					INIT;
// 					BIND3(stub, argsExpr, MULTIVALUE);
// 					parent_whose_children_to_walk = argsExpr;
// 					BIND2(stub, functionExpr);
// 					optional<definite_member_name> function_dmn;
// 					switch(GET_TYPE(functionExpr))
// 					{
// 						case IDENT:
// 							function_dmn = definite_member_name(1, CCP(GET_TEXT(functionExpr)));
// 							break;
// 						case DEFINITE_MEMBER_NAME:
// 							{
// 								definite_member_name dmn(functionExpr);
// 								function_dmn = dmn;
// 							}
// 							break;
// 						default:
// 						{
// 							cerr << "Warning: saw a functionExpr that wasn't of simple form: "
// 								<< CCP(GET_TEXT(functionExpr)) << endl;
// 							break;
// 						}
// 					}
// 					if (function_dmn) 
// 					{
// 						/* Resolve the function name against the module. */
// 						auto found = module->get_ds().toplevel()->visible_resolve(
// 							function_dmn->begin(), function_dmn->end());
// 						if (found)
// 						{
// 							auto subprogram_die = dynamic_pointer_cast<
// 								dwarf::spec::subprogram_die>(found);
// 							if (found)
// 							{
// 								auto i_arg = subprogram_die->formal_parameter_children_begin();
// 								INIT;
// 								FOR_ALL_CHILDREN(argsExpr)
// 								{
// 									if (i_arg == subprogram_die->formal_parameter_children_end())
// 									{
// 										cerr << "Expression " << CCP(TO_STRING_TREE(argsExpr))
// 											<< ", function " << *subprogram_die << endl;
// 										if (GET_TYPE(n) != CAKE_TOKEN(ELLIPSIS)
// 											&& GET_TYPE(n) != CAKE_TOKEN(KEYWORD_IN_ARGS))
// 											RAISE(stub, "too many args for function");
// 									}
// 									if (i_arg != subprogram_die->formal_parameter_children_end()
// 									 && (*i_arg)->get_type())
// 									{
// 										/* We found a type expectation */
// 										child_type_expectations[i] = (*i_arg)->get_type();
// 									}
// 									
// 									if (i_arg != subprogram_die->formal_parameter_children_end()) ++i_arg;
// 								}
// 							}
// 							else
// 							{
// 								RAISE(stub, "invokes nonexistent function");
// 								// FIXME: this will need to be tweaked to support function ptrs
// 							}
// 						}
// 					}
// 				}
// 				goto walk_children;
// 			case CAKE_TOKEN(IDENT):
// 				if (current_type_expectation) out.insert(make_pair(
// 					CCP(GET_TEXT(stub)), current_type_expectation));
// 				break; /* idents have no children */
// 			default:
// 				/* In all other cases, we have no expectation, so we leave
// 				 * the child_type_expectations at default values. */
// 			walk_children:
// 			{
// 				INIT;
// 				FOR_ALL_CHILDREN(parent_whose_children_to_walk)
// 				{
// 					/* recurse */
// 					find_type_expectations_in_stub(
// 						module,
// 						n, /* child */
// 						child_type_expectations[i],
// 						out
// 					);
// 				}
// 			}
// 				
// 		} // end switch
	}	
//     string link_derivation::namespace_name()
//     {
//     	ostringstream s;
//         s << "link_" << r.module_inverse_tbl[output_module] << '_';
//         return s.str();
// 	}

	int link_derivation::compute_wrappers_needed_and_linker_args()
	{
		bool wrapped_some = false;
		int num_wrappers = 0;
		// output wrapped symbol names (and the wrappers, to a separate file)
		for (wrappers_map_t::iterator i_wrap = wrappers.begin(); i_wrap != wrappers.end();
				++i_wrap)
		{
			// i_wrap->first is the wrapped symbol name
			// i_wrap->second is a list of pointer to the event correspondences
			//   whose *source* pattern invokes that symbol
			
			// ... however, if we have no source specifying an argument pattern,
			// don't emit a wrapper (because we can't provide args to invoke the __real_ function),
			// just --defsym __wrap_ = __real_ (i.e. undo the wrapping)
			bool can_simply_rebind = true;
			optional<string> symname_bound_to;
			for (ev_corresp_pair_ptr_list::iterator i_corresp_ptr = i_wrap->second.begin();
						i_corresp_ptr != i_wrap->second.end();
						++i_corresp_ptr)
			{
				// we can only simply rebind if our symbol is only invoked
				// with simple name-only patterns mapped to an argument-free
				// simple expression.
				// AND if all arguments are rep-compatible
				// AND return value too
				optional<string> source_symname =
					source_pattern_is_simple_function_name((*i_corresp_ptr)->second.source_pattern);
				optional<string> sink_symname =
					sink_expr_is_simple_function_name((*i_corresp_ptr)->second.sink_expr);
				if (source_symname && sink_symname) 
				{
					assert(source_symname == i_wrap->first);
					definite_member_name source_sym_mn(1, *source_symname);
					definite_member_name sink_sym_mn(1, *sink_symname);
					auto source_subprogram = 
						(*i_corresp_ptr)->second.source->get_ds().toplevel()->visible_resolve(source_sym_mn.begin(), source_sym_mn.end());
					auto sink_subprogram =
						(*i_corresp_ptr)->second.sink->get_ds().toplevel()->visible_resolve(sink_sym_mn.begin(), sink_sym_mn.end());
					assert(source_subprogram->get_tag() == DW_TAG_subprogram
					 && sink_subprogram->get_tag() == DW_TAG_subprogram);
					dwarf::spec::abstract_dieset::iterator i_source_arg 
					 = source_subprogram->children_begin();
					for (auto i_sink_arg = sink_subprogram->children_begin();
						i_sink_arg != sink_subprogram->children_end();
						++i_sink_arg)
					{
						if ((*i_sink_arg)->get_tag() == DW_TAG_unspecified_parameters)
						{
							// exhausted described sink args at/before exhausting source args
							assert(false); 
							// what do we want to do here?
							// we COULD force a wrap... would that be any good?
							// we can't really do anything with the arguments in the wrapper
						}
						if (i_source_arg == source_subprogram->children_end()
							|| (*i_source_arg)->get_tag() == DW_TAG_unspecified_parameters)
						{
							// reached the last source arg before exhausting sink args
							//assert(false);
							// force a wrap here -- will our wrapper take its arg types from the sink?
							can_simply_rebind = false;
							break;
						}
						shared_ptr<dwarf::spec::formal_parameter_die> source_arg, sink_arg;
						source_arg = dynamic_pointer_cast<dwarf::spec::formal_parameter_die>
							(*i_source_arg);
						assert(source_arg && source_arg->get_type());
						sink_arg = dynamic_pointer_cast<dwarf::spec::formal_parameter_die>
							(*i_sink_arg);
						assert(sink_arg && sink_arg->get_type());
						if (!source_arg->get_type()->is_rep_compatible(sink_arg->get_type()))
						{
							cerr << "Detected that required symbol " << i_wrap->first
								<< " is not a simple rebinding of a required symbol "
								<< " because arguments are not rep-compatible." << endl;
							can_simply_rebind = false;
							//can't continue
							break;
						}
						else { /* okay, continue */ }
						
						++i_source_arg;
					}
							
				}
				else 
				{
					cerr << "Detected that required symbol " << i_wrap->first
						<< " is not a simple rebinding of a required symbol "
						<< " because source pattern "
						<< CCP(TO_STRING_TREE((*i_corresp_ptr)->second.source_pattern))
						<< " is not a simple function name." << endl;
					can_simply_rebind = false;
				}
				if (sink_symname)
				{
					if (symname_bound_to) RAISE((*i_corresp_ptr)->second.sink_expr,
						"previous event correspondence has bound caller to different symname");
					else symname_bound_to = sink_symname;
				}
				else 
				{
					cerr << "Detected that required symbol " << i_wrap->first
						<< " is not a simple rebinding of a required symbol "
						<< " because sink pattern "
						<< CCP(TO_STRING_TREE((*i_corresp_ptr)->second.sink_expr))
						<< " is not a simple function name." << endl;
					can_simply_rebind = false;
				}
				
				/* If a given module both requires and provides the same symbol, i.e.
				 * if we are trying to interpose on an internal reference, we have to
				 * emit some extra make rules. FIXME: support this! */
				
			} // end for each corresp

			// (**WILL THIS WORK in ld?**)
			// It's an error to have some but not all sources specifyin an arg pattern.
			// FIXME: check for this

			if (!can_simply_rebind)
			{
				wrapped_some = true;
				++num_wrappers;

				// tell the linker that we're wrapping this symbol
				linker_args.push_back("--wrap ");
				symbols_to_protect.push_back(i_wrap->first);
				linker_args.push_back(i_wrap->first);
				
				// also tell the linker that unprefixed references to the symbol
				// should go to __real_<sym>
				// FIXME: can't do this as at the time of processing, there is no
				// symbol __real_<sym>. Instead must do as a separate stage,
				// e.g. when building executable.
				//linker_args << "--defsym " << i_wrap->first 
				//	<< '=' << "__real_" << i_wrap->first << ' ';
			}
			else
			{
				// don't emit wrapper, just use --defsym 
				if (!symname_bound_to || i_wrap->first != *symname_bound_to)
				{
					linker_args.push_back("--defsym ");
					linker_args.push_back(i_wrap->first  + "=" 
						// FIXME: if the callee symname is the same as the wrapped symbol,
						// we should use __real_; otherwise we should use
						// the plain old callee symbol name. 
						/*<< "__real_"*/ 
						+ *symname_bound_to); //i_wrap->first
				}
				else { /* do nothing! */ }
				
			}
			
			wrappers_needed[i_wrap->first] = !can_simply_rebind;
			
			//wrap_file.flush();
		} // end for each wrapper
		
		assert(num_wrappers == wrappers_needed.size());
		// temporary HACK: this isn't always true, but during testing, it should be		
		assert(num_wrappers > 0);
		return num_wrappers;
	}

	void link_derivation::write_makerules(ostream& out)
	{
		// implicit rule for making hpp files
		//out << "%.o.hpp: %.o" << endl
		//	<< '\t' << "dwarfhpp \"$<\" > \"$@\"" << endl;
		
		// dependencies for generated cpp file
		// -- don't do this because it forces early evaluation, in our multi-invoc era
		//out << wrap_file_makefile_name << ".d: " << wrap_file_makefile_name << endl
		//	<< '\t' << "$(CXX) $(CXXFLAGS) -MM -MG -I. -c \"$<\" > \"$@\"" << endl;
		//out << "-include " << wrap_file_makefile_name << ".d" << endl;

		write_object_dependency_makerules(out);
		
		// Now output the linker args.
		// If wrapped some, first add the wrapper as a dependency (and an argument)
		if (wrappers_needed.size() > 0)
		{
			out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< endl*/;
			// output the first objcopy
			out << endl << '\t' << "objcopy ";
			for (auto i_sym = symbols_to_protect.begin();
				i_sym != symbols_to_protect.end();
				++i_sym)
			{
				out << "--redefine-sym " << *i_sym << "=__cake_protect_" << *i_sym << " ";
			}
			out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< endl*/;
			out << endl << '\t' << "ld -r $(LD_RELOC_FLAGS) -o " << output_module->get_filename() << ' ';
			for (auto i_linker_arg = linker_args.begin(); 
				i_linker_arg != linker_args.end();
				++i_linker_arg)
			{
				out << *i_linker_arg << ' ';
			}
			out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< endl*/;
			// add the other object files to the input file list
			for (vector<module_ptr>::iterator i = input_modules.begin();
				i != input_modules.end(); ++i)
			{
				out << (*i)->get_filename() << ' ';
			}
			// output the second objcopy
			out << endl << '\t' << "objcopy ";
			for (auto i_sym = symbols_to_protect.begin();
				i_sym != symbols_to_protect.end();
				++i_sym)
			{
				out << "--redefine-sym __cake_protect_" << *i_sym << "=" << *i_sym << " ";
			}
			out << output_module->get_filename() << endl;
		}  // Else just output the args
		else 
		{
			out << endl << '\t' << "ld -r $(LD_RELOC_FLAGS) -o " << output_module->get_filename();
			for (auto i_linker_arg = linker_args.begin(); 
				i_linker_arg != linker_args.end();
				++i_linker_arg)
			{
				out << *i_linker_arg << ' ';
			}
			out << ' ';
			// add the other object files to the input file list
			for (vector<module_ptr>::iterator i = input_modules.begin();
				i != input_modules.end(); i++)
			{
				out << (*i)->get_filename() << ' ';
			}
			out << endl;
		}
	}
	
	void link_derivation::write_cxx()
	{
		std::ofstream raw_wrap_file(wrap_file_name);
		indenting_ostream wrap_file(raw_wrap_file);
		wrap_code.set_output_stream(wrap_file);

		// output the wrapper file header
		wrap_file << "// generated by Cake version " << CAKE_VERSION << endl;
		wrap_file << "#include <cake/prelude.hpp>" << endl;
		wrap_file << "extern \"C\" {\n#include <libcake/repman.h>\n}" << endl;

		// for each component, include its dwarfpp header in its own namespace
		for (vector<module_ptr>::iterator i_mod = input_modules.begin();
				i_mod != input_modules.end();
				i_mod++)
		{
			wrap_file << "namespace cake { namespace " << namespace_name()
					<< " { namespace " << r.module_inverse_tbl[*i_mod] << " {" << endl;
					
			wrap_file << "\t#include \"" << (*i_mod)->get_filename() << ".hpp\"" << endl;
			// also output any extra definitions we have added to the dieset.
			// We start with the first new offset, then emit toplevel DIEs (only).
			
			// This is subtle because we might also have added (non-empty) compile_unit DIEs.
			// We want to skip these. Our approach is
			// to find ourselves in the list of visible grandchildren,
			// which necessarily goes in CU order,
			// and continue from there.
			// FIXME: this inefficient because it walks the whole list of DIEs.
			// We could probably just construct the conjoining_iterator directly,
			// knowing its offset.
			auto visible_toplevel_seq = (*i_mod)->get_ds().toplevel()
				->visible_grandchildren_sequence();
			if (!visible_toplevel_seq->is_empty())
			{
				auto i_first_added = visible_toplevel_seq->begin();
				while (i_first_added != visible_toplevel_seq->end()
					&& (*i_first_added)->get_offset() <= (*i_mod)->greatest_preexisting_offset())
					++i_first_added;
				if (i_first_added != visible_toplevel_seq->end())
				{
					wrap_file << "// DIEs added by Cake begin at 0x" 
						<< std::hex << (*i_first_added)->get_offset() << std::dec
						<< " (greatest preexisting: 0x" 
						<< std::hex << (*i_mod)->greatest_preexisting_offset() << std::dec
						<< ")" << endl
						<< "// (and do not necessarily begin with lowest offset)" << endl;

		// 			abstract_dieset::iterator first_added_die(
		// 					(*i_mod)->get_ds().find((*i_mod)->greatest_preexisting_offset() + 1),
		// 					abstract_dieset::siblings_policy_sg);
		// 			// special case: if first added DIE is a CU, descend inside it
		// 			if ((*first_added_die)->get_tag() == DW_TAG_compile_unit)
		// 			{
		// 				wrap_file << "// ... which is a compile_unit, so really beginning at 0x" ;
		// 				first_added_die = (*first_added_die)->children_begin();
		// 				wrap_file << std::hex << (*first_added_die)->get_offset() << std::dec
		// 					<< endl;
		// 			}

					for (auto i_die = i_first_added;
							i_die != visible_toplevel_seq->end();
							++i_die)
					{
						// hmm -- we seem to need the extra test, probably because
						// we add new stuff to the first CU, then end up traversing
						// preexisting DIEs too?
						if ((*i_die)->get_offset() > (*i_mod)->greatest_preexisting_offset())
						{
							wrap_file << "/* 0x" << std::hex << (*i_die)->get_offset() 
							<< std::dec << " */" << endl;
							compiler.dispatch_to_model_emitter(wrap_file, i_die.base().base());
						}
					}
				}
				else wrap_file << "// no DIEs added by Cake, except in declare/override" << endl;
			}
			
			// also define a marker class, and code to grab a rep_id at init time
			wrap_file << "\tstruct marker { static int rep_id; }; // used for per-component template specializations" 
					<< endl;
			wrap_file << "\tint marker::rep_id;" << endl;
			wrap_file << "\tstatic void get_rep_id(void) __attribute__((constructor)); static void get_rep_id(void) { marker::rep_id = next_rep_id++; rep_component_names[marker::rep_id] = \""
			<< r.module_inverse_tbl[*i_mod] << "\"; }" << endl; // FIXME: C-escape this
			// also define the Cake component as a set of compilation units
			wrap_file << "extern \"C\" {" << endl;
			wrap_file << "\tconst char *__cake_component_" << r.module_inverse_tbl[*i_mod]
				<< " = ";
			/* output a bunch of string literals of the form
			 * <compilation-unit-name>-<full-compil-directory-name>-<compiler-ident> */
			for (auto i_cu = (*i_mod)->get_ds().toplevel()->compile_unit_children_begin();
					i_cu != (*i_mod)->get_ds().toplevel()->compile_unit_children_end();
					++i_cu)
			{
				wrap_file << "\t\t\"^" // FIXME: escape these!
					<< ((*i_cu)->get_name() ? *(*i_cu)->get_name() : "(anonymous)" )
					<< '^' // FIXME: complain if this char is used elsewhere
					<< ((*i_cu)->get_comp_dir() ? *(*i_cu)->get_comp_dir(): "(unknown directory)")
					<< '^'
					<< ((*i_cu)->get_producer() ? *(*i_cu)->get_producer() : "(unknown producer)")
					<< "^\""
					<< endl;
			}
			wrap_file << ";" << endl;
			wrap_file << "} /* end extern \"C\" */" << endl;
			wrap_file << "} } }" << endl; 

		} // end for input module
		
		// FIXME: collapse type synonymy
		// (AFTER doing name-matching, s.t. can match maximally)

		// for each pair of components, forward-declare the value conversions
		wrap_file << "namespace cake {" << endl;
		for (auto i_pair = all_iface_pairs.begin(); i_pair != all_iface_pairs.end();
			++i_pair)
		{
			// emit each as a value_convert template
			auto all_value_corresps = val_corresps.equal_range(*i_pair);
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				++i_corresp)
			{
				wrap_file << "// forward declaration: " << CCP(TO_STRING_TREE(i_corresp->second->corresp)) << endl;
				i_corresp->second->emit_forward_declaration();
			}
		}
		wrap_file << "} // end namespace cake" << endl;

		wrap_file << "// we have " << all_iface_pairs.size() << " iface pairs" << endl;
		
		// for each pair of components, output the value conversions
		for (auto i_pair = all_iface_pairs.begin(); i_pair != all_iface_pairs.end();
			++i_pair)
		{
			// first emit the component_pair specialisation which describes the rules
			// applying for this pair of components
			wrap_file << "namespace cake {" << endl;
			wrap_file << "\ttemplate<> struct component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker> {" << endl;

			// FIXME: emit mapping
			wrap_file 
// << endl << "        template <"
// << endl << "            typename To,"
// << endl << "            typename From /* = ::cake::unspecified_wordsize_type */, "
// << endl << "            int RuleTag = 0"
// << endl << "        >"
// << endl << "        static"
// << endl << "        To"
// << endl << "        value_convert_from_first_to_second(const From& arg)"
// << endl << "        {"
// << endl << "            return value_convert<From, "
// << endl << "                To,"
// << endl << "                " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker,"
// << endl << "                " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker,"
// << endl << "                RuleTag"
// << endl << "                >().operator()(arg);"
// << endl << "        }"
// << endl << "        template <"
// << endl << "            typename To,"
// << endl << "            typename From /* = ::cake::unspecified_wordsize_type */, "
// << endl << "            int RuleTag = 0"
// << endl << "        >"
// << endl << "        static "
// << endl << "        To"
// << endl << "        value_convert_from_second_to_first(const From& arg)"
// << endl << "        {"
// << endl << "            return value_convert<From, "
// << endl << "                To,"
// << endl << "                " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker,"
// << endl << "                " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker,"
// << endl << "                RuleTag"
// << endl << "                >().operator()(arg);"
// << endl << "        }	"
<< endl << "        static conv_table_t conv_table_first_to_second;"
<< endl << "        static conv_table_t conv_table_second_to_first;"
<< endl << "        static init_table_t init_table_first_to_second;"
<< endl << "        static init_table_t init_table_second_to_first;"
           /* We add the (65535) below, and the init_priority(101) further below,
            * to ensure that the constructors of the tables above
            * run *before* the init_conv_tables function. */
<< endl << "        static void init_conv_tables() __attribute__((constructor(65535)));"
<< endl << "    }; // end component_pair specialization"
<< endl << "\tconv_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::conv_table_first_to_second __attribute__((init_priority(101)));"
<< endl << "\tconv_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::conv_table_second_to_first __attribute__((init_priority(101)));"
<< endl << "\tinit_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::init_table_first_to_second __attribute__((init_priority(101)));"
<< endl << "\tinit_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::init_table_second_to_first __attribute__((init_priority(101)));"
<< endl;

			/* Now output the correspondence for unspecified_wordsize_type
			 * (FIXME: make these emissions just invoke macros in the prelude) */
			/* Given a pair of modules
			 * with a pair of bidirectionally corresponding types, 
			 * we need how many specializations? 
			 * given second, in first, 1-->2
			 * given second, in first, 1<--2
			 * given first, in second, 1-->2
			 * given first, in second, 1<--2 */
				string typedef_name
				 = r.module_inverse_tbl[i_pair->first] 
				 	+ "_" + r.module_inverse_tbl[i_pair->second] 
					+ "_pair";
				wrap_file << "// default corresponding_type specializations " << endl <<
				"    typedef "
<< endl << "        component_pair<" 
<< endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, "
<< endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker> "
				<< typedef_name << ";" << endl
				<< "default_corresponding_type_specializations(" << typedef_name << ");" << endl;
			//auto all_value_corresps = val_corresps.equal_range(*i_pair);
			auto all_value_corresp_groups = val_corresp_groups[*i_pair];

			// collect mappings of artificial type names (tags) in each group
			typedef map< val_corresp_group_key, map<string, vector< val_corresp *> > >
			corresps_by_artificial_names_map_t;
			corresps_by_artificial_names_map_t
			all_corresps_by_artificial_names_for_first_type;
			corresps_by_artificial_names_map_t
			all_corresps_by_artificial_names_for_second_type;
			for (auto i_corresp_group = all_value_corresp_groups.begin();
				i_corresp_group != all_value_corresp_groups.end();
				++i_corresp_group)
			{
				const val_corresp_group_key& k = i_corresp_group->first;
				vector<val_corresp *>& vec = i_corresp_group->second;
				cerr << "Group (size " << vec.size() 
					<< "): source module " << name_of_module(k.source_module)
					<< ", source type " << k.source_data_type->summary()
					<< ", sink module " << name_of_module(k.sink_module)
					<< ", sink type " << k.sink_data_type->summary() << endl;
				
				// collect mappings of artificial type names (tags) in this group
				map<string, vector< val_corresp *> >& corresps_by_artificial_names_for_first_type
				 = all_corresps_by_artificial_names_for_first_type[k];
				map<string, vector< val_corresp *> >& corresps_by_artificial_names_for_second_type
				 = all_corresps_by_artificial_names_for_second_type[k];
				for (auto i_p_corresp = vec.begin(); i_p_corresp != vec.end(); ++i_p_corresp)
				{
					cerr << "Corresp : " << **i_p_corresp << endl;
					auto source_type = (*i_p_corresp)->source_data_type;
					auto sink_type = (*i_p_corresp)->sink_data_type;
					// for this corresp, is the source module the first in our iface_pair?
					bool source_is_first = (k.source_module == i_pair->first);
					// two-iteration for-loop
					for (auto current_type = source_type; current_type; 
						current_type = (current_type == source_type) ? sink_type : shared_ptr<type_die>())
					{
						bool current_is_first = (current_type == source_type) ? source_is_first : !source_is_first;
						map<string, vector< val_corresp *> >& current_map = 
							current_is_first ? corresps_by_artificial_names_for_first_type
							                 : corresps_by_artificial_names_for_second_type;
						// is this data type artificial?
						cerr << ((current_type == source_type) ? "Tag: source" : "Tag: sink") 
							<< " type has ";
						if (current_type->get_concrete_type() != current_type)
						{
							// HOW do we encode "as"? Answer: by creating an artificial typedef 
							// Get the name. HACK: unqualified, for now
							assert(current_type->get_name());
							cerr << " artificial tag " << *current_type->get_name() << endl;
							current_map[*current_type->get_name()].push_back(*i_p_corresp);
						}
						else 
						{
							cerr << " no artificial tag (a.k.a. __cake_default)" << endl;
							current_map[string("__cake_default")].push_back(*i_p_corresp);
							assert(current_map.find(string("__cake_default")) != current_map.end());
						}
						/* NOTE: this *doesn't* guarantee that we have a __cake_default entry
						 * in the map -- we will only have the entries that had correspondences
						 * defined.
						 
						 * In the case of "char" though, we should have a correspondence
						 * defined, without artificial names. HMM. */
					}
				}
			}
			
			/* Now we iterate over half-keys, first matching against second module, later first. */
			auto half_keys = val_corresp_supergroup_keys[*i_pair]; //.equal_range(*i_pair);
			for (auto i_half_key = half_keys.begin();
			          i_half_key != half_keys.end();
			          ++i_half_key)
			{
				/* This half-key refers *either* to our first module *or* to our second. */
				bool half_key_is_first = (i_half_key->first == i_pair->first);
			
				// the "first" (lhs) type is unknown; we have fixed the second
				// first output the corresponding_type specializations:
				// there is a sink-to-source and source-to-sink relationship
				for (int direction_is_first_to_second = false; 
					direction_is_first_to_second < 2;
					++direction_is_first_to_second)
				{
					wrap_file << "// " 
						<< "(from half key: " 
						<< i_half_key->first->get_filename() << ", "
						<< i_half_key->second->summary() << ") "
						<< (half_key_is_first ? get_type_name(i_half_key->second) : "    ...    ")
						<< ( (direction_is_first_to_second) ? " --> " : " <-- " )
						<< (half_key_is_first ? "    ...    " : get_type_name(i_half_key->second))
						<< endl;
					if (i_half_key->second->get_tag() == DW_TAG_base_type)
					{
						wrap_file << "// half key is base type: " 
							<< cxx_compiler::base_type(
								dynamic_pointer_cast<spec::base_type_die>(i_half_key->second)
								) << endl;
					}
						
					bool half_key_is_source = (half_key_is_first && direction_is_first_to_second)
					                       || (!half_key_is_first && !direction_is_first_to_second);

					// In the template struct corresponding_type_to_second<, the boolean is DirectionIsFromSecondToFirst
					// In the template struct corresponding_type_to_first< , the boolean is DirectionIsFromFirstToSecond

					wrap_file << 
					"    template <>"
	<< endl << "    struct corresponding_type_to_" << (half_key_is_first ? "first" : "second") << "<" 
	<< endl << "        component_pair<" 
	<< endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, "
	<< endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, " 
                    	  << get_type_name(i_half_key->second) << ", " // ALWAYS our half-key
                    	  << boolalpha << ((bool)(direction_is_first_to_second ^ !half_key_is_first)) << ">" 
						  // if half_key is second, this bool is DirectionIsFromSecondToFirst, i.e. the opposite sense as our flag
						  // if half_key is first, this bool is DirectionIsFromFirstToSecond, i.e. the same sense as our flag
						  // so this bool is our flag XORed with !half_key_is_first
	<< endl << "    {"
	<< endl;
	
					module_ptr the_other_module = 
						(i_pair->first == i_half_key->first)
							? i_pair->second
							: i_pair->first;
	
					/* Now gather together all rules from this half-key in this direction. */
					auto bothways_vec = val_corresp_supergroups[*i_pair].equal_range(*i_half_key);
					vector< val_corresp * > vec;
					for (auto i_p_corresp = bothways_vec.first; i_p_corresp != bothways_vec.second;
						++i_p_corresp)
					{
						// whether this corresp is relevant depends on our half-key and our direction
						// -- we only match rules whose source/sink (as appropriate, w.r.t. our direction)
						bool relevant;
						if (i_p_corresp->second->sink == i_half_key->first)
						{
							assert(i_p_corresp->second->source == the_other_module);
							relevant = !half_key_is_source;
						}
						else 
						{
							// it shouldn't be from a different iface pair, so 
							// we should have the source be the half-key
							assert(i_p_corresp->second->source == i_half_key->first);
							assert(i_p_corresp->second->sink == the_other_module);
							//  our half-key module is the source module
							// else it's relevant if its sink is the other module
							relevant = half_key_is_source;
						}
						if (relevant)
						{
							vec.push_back(i_p_corresp->second);
						}
					}
					auto& corresps_by_artificial_names_for_half_key_type_map
					= (half_key_is_first) 
					  ? all_corresps_by_artificial_names_for_first_type
					  : all_corresps_by_artificial_names_for_second_type;
					auto& corresps_by_artificial_names_for_the_other_type_map
					= (half_key_is_first) 
					  ? all_corresps_by_artificial_names_for_second_type
					  : all_corresps_by_artificial_names_for_first_type;
					/* Now we have all the relevant rules. Output about them. */
					wrap_file <<
			  "         // this supergroup/direction has " << vec.size() << " rules" << endl;

					val_corresp *emitted_default_to_default = /*false */ 0;
					set<string> emitted_outer_tags_to_default;
					set<string> emitted_default_to_inner_tags;
					for (auto i_p_corresp = vec.begin(); i_p_corresp != vec.end(); ++i_p_corresp)
					{
						// our relevance criteria ensured that 
						// whichever way the half-key is, this should hold
						module_ptr first_module = (direction_is_first_to_second) 
							? (*i_p_corresp)->source : (*i_p_corresp)->sink;
						// depending on the half key...
						assert((half_key_is_first) ? first_module == i_half_key->first
						                           : first_module == the_other_module);
						// 
						assert((half_key_is_source) 
							? (((*i_p_corresp)->source == i_half_key->first) && ((*i_p_corresp)->sink == the_other_module))
							: (((*i_p_corresp)->sink == i_half_key->first)   && ((*i_p_corresp)->source == the_other_module)));
							
						// the template argument data type, i.e. always the half-key type
						shared_ptr<type_die> outer_type = 
						(half_key_is_source)
							? (*i_p_corresp)->source_data_type
							: (*i_p_corresp)->sink_data_type;
						auto outer_type_synonymy_chain = type_synonymy_chain(outer_type);

						// the type to be typedef'd in the struct body -- the one from the *other* module
						shared_ptr<type_die> inner_type =   
						(half_key_is_source)
							? (*i_p_corresp)->sink_data_type
							: (*i_p_corresp)->source_data_type;
						auto inner_type_synonymy_chain = type_synonymy_chain(inner_type);

						cerr << "outer type: " << *outer_type
							<< ", inner type: " << *inner_type
							<< endl;

						wrap_file << "         // from corresp at " << *i_p_corresp << " " << **i_p_corresp 
							<< ", rule " << CCP(TO_STRING_TREE((*i_p_corresp)->corresp)) << endl;
						wrap_file << "         // inner type name " 
							<< (inner_type->get_name() ? *inner_type->get_name() : "(no name)") 
							<< ", outer type name " 
							<< (outer_type->get_name() ? *outer_type->get_name() : "(no name)")  
							<< endl;
						wrap_file.flush();
						
						// HACK: repeat for debugging
						clog << "         // from corresp at " << *i_p_corresp << " " << **i_p_corresp 
							<< ", rule " << CCP(TO_STRING_TREE((*i_p_corresp)->corresp)) << endl;
						clog << "         // inner type name " 
							<< (inner_type->get_name() ? *inner_type->get_name() : "(no name)") 
							<< ", outer type name " 
							<< (outer_type->get_name() ? *outer_type->get_name() : "(no name)")  
							<< endl;
						if (outer_type_synonymy_chain.size() == 0
						&&    inner_type_synonymy_chain.size() == 0
						&&    !(*i_p_corresp)->init_only 
						&&    emitted_default_to_default)
						{
							clog << "Previous corresp was: " << *emitted_default_to_default << endl;
						}

						assert(outer_type_synonymy_chain.size() != 0
						||    inner_type_synonymy_chain.size() != 0
						||    (*i_p_corresp)->init_only 
						||    !emitted_default_to_default);

						if (outer_type_synonymy_chain.size() == 0
						&&   inner_type_synonymy_chain.size() == 0
						&& !(*i_p_corresp)->init_only) emitted_default_to_default = *i_p_corresp;

						wrap_file << "         typedef "
							// target of the typedef is always the inner type
							<< get_type_name(inner_type)
							<< " "
							<< ( (*i_p_corresp)->init_only ? "__init_only_" : "" )
							<< ((outer_type_synonymy_chain.size() == 0) ? "__cake_default" : 
								*(*outer_type_synonymy_chain.begin())->get_name())
							<< "_to_"
							<< ((inner_type_synonymy_chain.size() == 0) ? "__cake_default" : 
								*(*inner_type_synonymy_chain.begin())->get_name())
							// always the converse of the outer module
							<< "_in_" << (half_key_is_first ? "second" : "first") 
							<< ";" << endl;

						if (inner_type_synonymy_chain.size() == 0 
							&& !(*i_p_corresp)->init_only) 
						{
							emitted_outer_tags_to_default.insert(
								((outer_type_synonymy_chain.size() == 0) ? "__cake_default" : 
								*(*outer_type_synonymy_chain.begin())->get_name())
							);
						}
						if (outer_type_synonymy_chain.size() == 0 
							&& !(*i_p_corresp)->init_only) 
						{
							emitted_default_to_inner_tags.insert(
								((inner_type_synonymy_chain.size() == 0) ? "__cake_default" : 
								*(*inner_type_synonymy_chain.begin())->get_name())
							);
						}
					} // end outputting typedefs
					
					// compute groups of rules 
					// by inner ("other") and outer ("half-key") tagstrings
					
					/* For each full key containing this half key...
					 * output the tag enums.
					 * We have to group the corresps by the tag string on the half key side,
					 * so that each enum element is output together
					 * without trying to close and reopen the same struct. 
					 * This means that we can't do a two-level iteration
					 * of full_key then corresp; 
					 * we have to flip between distinct full keys
					 * while outputting a single enum.
					 * So, expand keys into <half_key_tagstring, full_key> -> corresp map. */

					std::set< val_corresp_group_key>& keys_for_this_supergroup
					 = val_corresp_group_keys_by_supergroup[*i_pair][*i_half_key];

					multimap< string, val_corresp * > by_outer_tagstring;
					set< string > outer_tagstrings;
					multimap< string, val_corresp * > by_inner_tagstring;
					set< string > inner_tagstrings;
					
					for (auto i_full_key = keys_for_this_supergroup.begin();
						i_full_key != keys_for_this_supergroup.end();
						++i_full_key)
					{
						if (!(i_full_key->source_module == (half_key_is_source ? i_half_key->first : the_other_module))
						||  !(i_full_key->sink_module   == (half_key_is_source ? the_other_module : i_half_key->first)))
						continue;
					
						auto& corresps_by_artificial_names_for_half_key_type
						 = corresps_by_artificial_names_for_half_key_type_map[*i_full_key];
						auto& corresps_by_artificial_names_for_the_other_type
						 = corresps_by_artificial_names_for_the_other_type_map[*i_full_key];
						 
						for (auto i_tag_in_half_key
							 = corresps_by_artificial_names_for_half_key_type.begin();
							 i_tag_in_half_key 
							 != corresps_by_artificial_names_for_half_key_type.end();
							/* prev_half_key_tagstring = i_tag_in_half_key->first, */
							++i_tag_in_half_key)
						{
							for (auto i_p_corresp = i_tag_in_half_key->second.begin(); 
								i_p_corresp != i_tag_in_half_key->second.end(); ++i_p_corresp)
							{
								// COMPLETE HACK: skip init-only rules
								if ((*i_p_corresp)->init_only) continue;
								
								by_outer_tagstring.insert(make_pair(i_tag_in_half_key->first, *i_p_corresp));
								outer_tagstrings.insert(i_tag_in_half_key->first);
							}
						}
						for (auto i_tag_in_other
							 = corresps_by_artificial_names_for_the_other_type.begin();
							 i_tag_in_other
							 != corresps_by_artificial_names_for_the_other_type.end();
							/* prev_half_key_tagstring = i_tag_in_half_key->first, */
							++i_tag_in_other)
						{
							for (auto i_p_corresp = i_tag_in_other->second.begin(); 
								i_p_corresp != i_tag_in_other->second.end(); ++i_p_corresp)
							{
								// COMPLETE HACK: skip init-only rules
								if ((*i_p_corresp)->init_only) continue;
								
								by_inner_tagstring.insert(make_pair(i_tag_in_other->first, *i_p_corresp));
								inner_tagstrings.insert(i_tag_in_other->first);
							}
						}
					}
					
					// make sure there is always an X-to-default
					// mapping, i.e. inner-to-default -- HACK: only if unique, for now.
					for (auto i_outer = outer_tagstrings.begin(); i_outer != outer_tagstrings.end(); ++i_outer)
					{
						if (*i_outer == "__cake_default"
						|| emitted_outer_tags_to_default.find(*i_outer)
							!= emitted_outer_tags_to_default.end()) continue;
						auto outer_seq = by_outer_tagstring.equal_range(*i_outer);
						if (srk31::count(outer_seq.first, outer_seq.second) == 1)
						{
							wrap_file << "typedef " 
								// we typedef the inner type, i.e. the non-half-key one
								<< ((half_key_is_source) ? 
										get_type_name(outer_seq.first->second->sink_data_type)
									: 	get_type_name(outer_seq.first->second->source_data_type))
								<< " " << *i_outer << "_to___cake_default_in_"
								<< (half_key_is_first ? "second" : "first") 
								<< ";" << endl;
						}
						else wrap_file << "// not emitting a " << *i_outer << "_to___cake_default typedef "
							<< "because there is no unique candidate" << endl;
					}
					// sim. for default-to-outer? 
					for (auto i_inner = inner_tagstrings.begin(); i_inner != inner_tagstrings.end(); ++i_inner)
					{
						if (*i_inner == "__cake_default"
						|| emitted_default_to_inner_tags.find(*i_inner)
							!= emitted_default_to_inner_tags.end()) continue;
						auto inner_seq = by_inner_tagstring.equal_range(*i_inner);
						if (srk31::count(inner_seq.first, inner_seq.second) == 1)
						{
							wrap_file << "typedef " 
								// we STILL (ALWAYS) typedef the inner type, i.e. the non-half-key one
								<< ((half_key_is_source) ? 
										get_type_name(inner_seq.first->second->sink_data_type)
									: 	get_type_name(inner_seq.first->second->source_data_type))
								<< 	" __cake_default_to_" << *i_inner << "_in_"
								<< (half_key_is_first ? "second" : "first") 
								<< ";" << endl;
						}
						else wrap_file << "// not emitting a __cake_default_to_" << *i_inner << " typedef "
							<< "because there is no unique candidate" << endl;
					}
					
					// NO -- actually we can't assume a default is already in there,
					// e.g. when we have an anonymous struct that is typedef'd to something
					// that then gets name-matched.
					//assert(outer_tagstrings.find("__cake_default") != outer_tagstrings.end());
					// BUT we should ensure that even if default isn't there,
					// we emita default case! 
					
					for (auto i_outer = outer_tagstrings.begin(); i_outer != outer_tagstrings.end(); ++i_outer)
					{
						wrap_file << "         struct rule_tag_in_" << (half_key_is_first ? "second" : "first") 
						  << "_given_" << (half_key_is_first ? "first" : "second") 
						  << "_artificial_name_" << *i_outer 
							<< " { enum __cake_rule_tags {" << endl;
						
						auto outer_seq = by_outer_tagstring.equal_range(*i_outer);
						bool emitted_default = false;
						set<string> emitted_inner_tagstrings;
						for (auto i_p_corresp_pair = outer_seq.first;
							i_p_corresp_pair != outer_seq.second;
							++i_p_corresp_pair)
						{
							if (i_p_corresp_pair != outer_seq.first) wrap_file << ", " << endl;
							// here we want the data type in the *other* module, but not concretised
							auto half_key_die = (half_key_is_source) ?
								(*i_p_corresp_pair).second->source_data_type : (*i_p_corresp_pair).second->sink_data_type;
							auto the_other_die = (half_key_is_source) ? 
								(*i_p_corresp_pair).second->sink_data_type : (*i_p_corresp_pair).second->source_data_type;
							//auto artificial_name_for_half_key_die =
							//	(!data_types_are_identical(half_key_die->get_concrete_type(), half_key_die)) 
							//	? *half_key_die->get_name()
							//	 : "__cake_default";
							auto artificial_name_for_the_other_die =
								(!data_types_are_identical(the_other_die->get_concrete_type(), the_other_die)) 
								? *the_other_die->get_name()
								 : "__cake_default";

							assert(val_corresp_numbering.find(i_p_corresp_pair->second->shared_from_this())
							    != val_corresp_numbering.end());
							wrap_file << artificial_name_for_the_other_die // artificial_name_for_half_key_die 
								<< " = " 
								<< val_corresp_numbering[(*i_p_corresp_pair).second->shared_from_this()];
							emitted_inner_tagstrings.insert(artificial_name_for_the_other_die);
						}
						/* Now also emit a "__cake_default" enum element if it's unique and we haven't yet; */
						if (srk31::count(outer_seq.first, outer_seq.second) == 1
						 && emitted_inner_tagstrings.find("__cake_default") == emitted_inner_tagstrings.end())
						{
							auto i_p_corresp_pair = outer_seq.first;
							wrap_file << ", " << endl;
							auto artificial_name_for_the_other_die = "__cake_default";
							wrap_file << artificial_name_for_the_other_die 
								<< " = " 
								<< val_corresp_numbering[(*i_p_corresp_pair).second->shared_from_this()]
								<< "/* by uniqueness */";
						}
						if (*i_outer == "__cake_default")
						{
							/* In this case, we also go through the *inner* strings and
							 * do likewise, so that 
							 * in the enum named "..._artificial_name___cake_default"
							 * we have a full set of options. */
							for (auto i_inner = inner_tagstrings.begin(); i_inner != inner_tagstrings.end(); ++i_inner)
							{
								// we should skip any case we've handled already
								if (emitted_inner_tagstrings.find(*i_inner) != emitted_inner_tagstrings.end()) continue;
								auto inner_seq = by_inner_tagstring.equal_range(*i_inner);
								
								if (srk31::count(inner_seq.first, inner_seq.second) == 1)
								{
									auto i_p_corresp_pair = inner_seq.first;
									auto artificial_name_for_the_other_die = *i_inner;
									wrap_file << ", " << endl
										<< artificial_name_for_the_other_die // artificial_name_for_half_key_die 
										<< " = " 
										<< val_corresp_numbering[(*i_p_corresp_pair).second->shared_from_this()];
								}
							}
						}
							
						
						wrap_file << "         }; };" << endl; // ends a tag enum/struct
					}
     wrap_file << "    };" // ends a specialization
<< endl;
				} // end for direction (first-to-second, second-to-first)
			} // end for all value corresp supergroups 
			
			/* Now emit the table */
wrap_file    << "\tvoid component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::init_conv_tables()"
<< endl << "\t{\n";
			int hack_ctr = 0;
			auto all_value_corresps = val_corresps.equal_range(*i_pair);
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				++i_corresp)
			{
				// HACK: instantiate all the template function instnaces we need
				// -- will be unnecessary once gcc bug 49609 is fixed
				ostringstream hack_varname;
				hack_varname << "_hack_" << hack_ctr++;
				wrap_file << "\t\tstatic ";
				i_corresp->second->emit_cxx_function_ptr_type(hack_varname.str());
				wrap_file << " = &(";
				i_corresp->second->emit_function_name();
				wrap_file << ");" << endl;
				
				if (!i_corresp->second->init_only)
				{
					if (i_corresp->second->source == i_pair->first)
					{
						wrap_file << "\t\tconv_table_first_to_second";
					}
					else 
					{
						wrap_file << "\t\tconv_table_second_to_first";
					}

					wrap_file << ".insert(std::make_pair((conv_table_key) {" << endl;
					// output the fq type name as an initializer list
					wrap_file << "\t\t\t{ ";
					auto source_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->source_data_type);
					for (auto i_source_name_piece = source_fq_name.begin();
						i_source_name_piece != source_fq_name.end();
						++i_source_name_piece)
					{
						/*assert(*i_source_name_piece);*/ // FIXME: escape these!
						wrap_file << "\"" << /*escape_c_literal(*/*i_source_name_piece/*)*/
							<< "\"";
						if (i_source_name_piece != source_fq_name.begin()) wrap_file << ", ";
					}
					wrap_file << " }, { ";
					auto sink_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->sink_data_type);
					for (auto i_sink_name_piece = sink_fq_name.begin();
						i_sink_name_piece != sink_fq_name.end();
						++i_sink_name_piece)
					{
						/*assert(*i_sink_name_piece);*/ // FIXME: escape these!
						wrap_file << "\"" << /*escape_c_literal(*/*i_sink_name_piece/*)*/
							<< "\"";
						if (i_sink_name_piece != sink_fq_name.begin()) wrap_file << ", ";
					}
					wrap_file << " }, ";
					if (i_corresp->second->source == i_pair->first)
					{
						wrap_file << "true, "; // from first to second
					} else wrap_file << "false, "; // from first to second
					wrap_file << /* i_corresp->second->rule_tag */ "0 " << "}, " 
						<< endl << "\t\t\t (conv_table_value) {";
					// output the size of the object -- hey, we can use sizeof
					wrap_file << " sizeof ( ::cake::" << namespace_name() << "::"
						<< name_of_module(i_corresp->second->sink) << "::"
						<< compiler.local_name_for(i_corresp->second->sink_data_type, false) << "), ";
					// now output the address 
					wrap_file << "reinterpret_cast<void*(*)(void*,void*)>(&";
					i_corresp->second->emit_function_name();
					wrap_file << "\t\t )}));" << endl;

				} // end if not init-only
				else
				{
					wrap_file << "\t\t// init-only rule" << endl;
				}
				
				// if it's an init rule, add it to the init table
				if (init_rules_tbl[*i_pair][
					(init_rules_key_t) { 
						(i_corresp->second->source == i_pair->first),
						i_corresp->second->source_data_type
					}] == i_corresp->second)
				{
					if (i_corresp->second->source == i_pair->first)
					{
						wrap_file << "\t\tinit_table_first_to_second";
					}
					else 
					{
						wrap_file << "\t\tinit_table_second_to_first";
					}

					wrap_file << ".insert(std::make_pair((init_table_key) {" << endl;
					// output the fq type name as an initializer list
					wrap_file << "\t\t\t{ ";
					auto source_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->source_data_type);
					for (auto i_source_name_piece = source_fq_name.begin();
						i_source_name_piece != source_fq_name.end();
						++i_source_name_piece)
					{
						/*assert(*i_source_name_piece);*/ // FIXME: escape these!
						wrap_file << "\"" << /*escape_c_literal(*/*i_source_name_piece/*)*/
							<< "\"";
						if (i_source_name_piece != source_fq_name.begin()) wrap_file << ", ";
					}
					wrap_file << " }, ";
					if (i_corresp->second->source == i_pair->first)
					{
						wrap_file << "true"; // from first to second
					} else wrap_file << "false"; // from first to second
					wrap_file << "}, " 
						<< endl << "\t\t\t (init_table_value) {";
					// output the size of the object -- hey, we can use sizeof
					wrap_file << " sizeof ( ::cake::" << namespace_name() << "::"
						<< name_of_module(i_corresp->second->sink) << "::"
						<< compiler.local_name_for(i_corresp->second->sink_data_type, false) << "), ";
					wrap_file << endl << "\t\t\t{ ";
					auto sink_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->sink_data_type);
					for (auto i_sink_name_piece = sink_fq_name.begin();
						i_sink_name_piece != sink_fq_name.end();
						++i_sink_name_piece)
					{
						/*assert(*i_source_name_piece);*/ // FIXME: escape these!
						wrap_file << "\"" << /*escape_c_literal(*/*i_sink_name_piece/*)*/
							<< "\"";
						if (i_sink_name_piece != sink_fq_name.begin()) wrap_file << ", ";
					}
					wrap_file << " }, ";
					// now output the function address 
					wrap_file << "reinterpret_cast<void*(*)(void*,void*)>(&";
					i_corresp->second->emit_function_name();
					wrap_file << "\t\t )}));" << endl;
				}
				else
				{
					wrap_file << "\t\t// not an init rule" << endl;
				}
			}

wrap_file  
<< endl << "\t} /* end conv table initializer */" << endl;
wrap_file << "extern \"C\" {" << endl;
wrap_file << "void *__cake_componentpair_" 
<< name_of_module(i_pair->first).size() << name_of_module(i_pair->first)
<< "_" 
<< name_of_module(i_pair->second).size() << name_of_module(i_pair->second)
<< "_first_to_second[2] = { &component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>:: conv_table_first_to_second, " 
<< " &component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>:: init_table_first_to_second };\n" << endl;
wrap_file << "void *__cake_componentpair_" 
<< name_of_module(i_pair->first).size() << name_of_module(i_pair->first) 
<< "_" 
<< name_of_module(i_pair->second).size() << name_of_module(i_pair->second) 
<< "_second_to_first[2] = { &component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>:: conv_table_second_to_first, " 
<< "&component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>:: init_table_second_to_first };\n" << endl;
wrap_file << "} /* end extern \"C\" */" << endl;

			// emit each as a value_convert template
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				++i_corresp)
			{
//				 auto opt_from_type = //i_corresp->second.source->get_ds().toplevel()->resolve(
//					 i_corresp->second.source_data_type/*)*/;
//				 auto opt_to_type = //i_corresp->second.sink->get_ds().toplevel()->resolve(
//					 i_corresp->second.sink_data_type/*)*/;
//				 if (!opt_from_type) 
// 				{ RAISE(i_corresp->second.corresp, 
//					 "named source type does not exist"); }
//				 if (!opt_to_type) 
// 				{ RAISE(i_corresp->second.corresp, 
//					 "named sink type does not exist"); }
// 				auto p_from_type = dynamic_pointer_cast<dwarf::spec::type_die>(opt_from_type);
// 				auto p_to_type = dynamic_pointer_cast<dwarf::spec::type_die>(opt_to_type);
//				 if (!p_from_type) RAISE(i_corresp->second.corresp, 
//					 "named source of value correspondence is not a DWARF type");
//				 if (!p_to_type) RAISE(i_corresp->second.corresp, 
//					 "named target of value correspondence is not a DWARF type");

				wrap_file << "// " << CCP(TO_STRING_TREE(i_corresp->second->corresp)) << endl;
				i_corresp->second->emit();
				
// 				emit_value_conversion(
//				 	i_corresp->second.source,
//			 		p_from_type,
//			 		i_corresp->second.source_infix_stub,
//			 		i_corresp->second.sink,
//			 		p_to_type,
//			 		i_corresp->second.sink_infix_stub,
//			 		i_corresp->second.refinement,
// 					i_corresp->second.source_is_on_left,
// 					i_corresp->second.corresp);
			}
			wrap_file << "} // end namespace cake" << endl;
			
		} // end for iface pair
		
		// now emit the wrappers!
		//wrapper_file wrap_code(*this, compiler, wrap_file);
		for (auto i_wrap = wrappers.begin(); i_wrap != wrappers.end();
			++i_wrap)
		{
			if (wrappers_needed[i_wrap->first]) 
			{
				wrap_code.emit_wrapper(i_wrap->first, i_wrap->second);
			}
		}
	}
	
	void link_derivation::add_corresps_from_block(
		module_ptr left,
		module_ptr right,
		antlr::tree::Tree *corresps)
	{
		cerr << "Adding explicit corresps from block: " << CCP(TO_STRING_TREE(corresps))
			<< endl;
		assert(GET_TYPE(corresps) == CAKE_TOKEN(CORRESP));
		
		// We make two passes, to accommodate the lower priority of ident-pattern rules.
		// In the first pass, we remember pattern rules.
		std::vector<antlr::tree::Tree *> deferred;
		{ INIT;
		FOR_ALL_CHILDREN(corresps)
		{
			SELECT_ONLY(EVENT_CORRESP);
			assert(GET_TYPE(n) == CAKE_TOKEN(EVENT_CORRESP));

			INIT;
			BIND2(n, correspHead);

			/* We filter out the ident-pattern rules here, and defer processing. */
			if (
				(GET_TYPE(correspHead) == CAKE_TOKEN(LR_DOUBLE_ARROW)
				&& GET_TYPE(GET_CHILD(GET_CHILD(correspHead, 0), 1)) == CAKE_TOKEN(KEYWORD_PATTERN))
			||  (GET_TYPE(correspHead) == CAKE_TOKEN(RL_DOUBLE_ARROW)
				&& GET_TYPE(GET_CHILD(GET_CHILD(correspHead, 3), 1)) == CAKE_TOKEN(KEYWORD_PATTERN))
			||  (GET_TYPE(correspHead) == CAKE_TOKEN(BI_DOUBLE_ARROW)
				&& (GET_TYPE(GET_CHILD(GET_CHILD(correspHead, 0), 1)) == CAKE_TOKEN(KEYWORD_PATTERN)
				  || GET_TYPE(GET_CHILD(GET_CHILD(correspHead, 3), 1)) == CAKE_TOKEN(KEYWORD_PATTERN)))
			)
			{
				// it's a pattern rule that we need to expand separately, later
				deferred.push_back(n);
			}
			else
			{
				process_non_ident_pattern_event_corresp(left, right, n, false);
			}
		}}
		/* Now process deferred ident-pattern rules. */
		expand_patterns_and_process(left, right, deferred);
		{ INIT;
		FOR_ALL_CHILDREN(corresps)
		{
			SELECT_ONLY(KEYWORD_VALUES);
			assert(GET_TYPE(n) == CAKE_TOKEN(KEYWORD_VALUES));

			INIT;
			// we may have multiple value corresps here
			FOR_ALL_CHILDREN(n)
			{
				ALIAS2(n, correspHead);
				assert(GET_TYPE(correspHead) == CAKE_TOKEN(LR_DOUBLE_ARROW)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(LR_DOUBLE_ARROW_Q)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(BI_DOUBLE_ARROW)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(RL_DOUBLE_ARROW)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(RL_DOUBLE_ARROW_Q)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(NAMED_VALUE_CORRESP));

				string ruleName;
				if (GET_TYPE(correspHead) == CAKE_TOKEN(NAMED_VALUE_CORRESP))
				{
					if (GET_CHILD_COUNT(correspHead) != 2) RAISE_INTERNAL(correspHead,
						"named rules must be a simple pair");
					auto ruleNameIdent = GET_CHILD(correspHead, 1); 
					if (GET_TYPE(ruleNameIdent) != CAKE_TOKEN(IDENT))
					{ RAISE_INTERNAL(ruleNameIdent, "must be a simple ident"); }
					ruleName = CCP(GET_TEXT(ruleNameIdent));
					// HACK: move down one
					correspHead = GET_CHILD(correspHead, 0);
				}
				assert(GET_TYPE(correspHead) == CAKE_TOKEN(LR_DOUBLE_ARROW)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(LR_DOUBLE_ARROW_Q)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(BI_DOUBLE_ARROW)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(RL_DOUBLE_ARROW)
					|| GET_TYPE(correspHead) == CAKE_TOKEN(RL_DOUBLE_ARROW_Q));

				switch(GET_TYPE(correspHead))
				{
					case CAKE_TOKEN(BI_DOUBLE_ARROW):
					{
						INIT;
						/* The BI_DOUBLE_ARROW is special because 
						* - it might have multivalue children (for many-to-many)
						* - it should not have nonempty infix stubs
							(because these would be ambiguous) */
						BIND2(correspHead, leftValDecl);
						BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
						BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
						if (GET_CHILD_COUNT(leftInfixStub)
						|| GET_CHILD_COUNT(rightInfixStub) > 0)
						{
							RAISE(correspHead, 
							"infix stubs are ambiguous for bidirectional value correspondences");
						}
						BIND2(correspHead, rightValDecl);
						BIND3(correspHead, valueCorrespondenceRefinement, 
							VALUE_CORRESPONDENCE_REFINEMENT);
						// we don't support many-to-many yet
						assert(GET_TYPE(leftValDecl) != CAKE_TOKEN(MULTIVALUE)
						&& GET_TYPE(rightValDecl) != CAKE_TOKEN(MULTIVALUE));
						ALIAS3(leftValDecl, leftMember, NAME_AND_INTERPRETATION);
						ALIAS3(rightValDecl, rightMember, NAME_AND_INTERPRETATION);
						// each add_value_corresp call denotes a 
						// value conversion function that needs to be generated
						add_value_corresp(left, GET_CHILD(leftMember, 0), leftInfixStub,
							right, GET_CHILD(rightMember, 0), rightInfixStub,
							valueCorrespondenceRefinement, true, correspHead);
						add_value_corresp(right, GET_CHILD(rightMember, 0), rightInfixStub,
							left, GET_CHILD(leftMember, 0), leftInfixStub, 
							valueCorrespondenceRefinement, false, correspHead);

					}
					break;
					case CAKE_TOKEN(LR_DOUBLE_ARROW):
					case CAKE_TOKEN(RL_DOUBLE_ARROW):
					case CAKE_TOKEN(LR_DOUBLE_ARROW_Q):
					case CAKE_TOKEN(RL_DOUBLE_ARROW_Q):
					{
						INIT;
						BIND2(correspHead, leftValDecl);
						BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
						BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
						BIND2(correspHead, rightValDecl);
						BIND3(correspHead, valueCorrespondenceRefinement, 
							VALUE_CORRESPONDENCE_REFINEMENT);
						// many-to-many not allowed
						if(!(GET_TYPE(leftValDecl) != CAKE_TOKEN(MULTIVALUE)
						&& GET_TYPE(rightValDecl) != CAKE_TOKEN(MULTIVALUE)))
						{ RAISE(correspHead, "many-to-many value correspondences must be bidirectional"); }

						// one of these is a valuePattern, so will be a NAME_AND_INTERPRETATION;
						// the other is a stub expression, so need not have an interpretation.
						ALIAS2(leftValDecl, leftMember);
						ALIAS2(rightValDecl, rightMember);
						bool init_only = false;
						bool left_is_pattern = false;
						bool right_is_pattern = false;
						switch(GET_TYPE(correspHead))
						{
							case CAKE_TOKEN(LR_DOUBLE_ARROW_Q):
								init_only = true;
							case CAKE_TOKEN(LR_DOUBLE_ARROW):
								// the valuePattern is the left-hand one
								assert(GET_TYPE(leftMember) == CAKE_TOKEN(NAME_AND_INTERPRETATION));
								add_value_corresp(left, GET_CHILD(leftMember, 0), leftInfixStub,
									right, rightMember, rightInfixStub,
									valueCorrespondenceRefinement, true, correspHead,
									init_only);
								left_is_pattern = true;
								goto record_touched;
							case CAKE_TOKEN(RL_DOUBLE_ARROW_Q):
								init_only = true;
							case CAKE_TOKEN(RL_DOUBLE_ARROW):
								// the valuePattern is the right-hand one
								assert(GET_TYPE(rightMember) == CAKE_TOKEN(NAME_AND_INTERPRETATION));
								add_value_corresp(right, GET_CHILD(rightMember, 0), rightInfixStub,
									left, leftMember, leftInfixStub, 
									valueCorrespondenceRefinement, false, correspHead,
									init_only);
								right_is_pattern = true;
								goto record_touched;
							record_touched:
								touched_data_types[left].insert(
									read_definite_member_name(left_is_pattern ? 
										(GET_CHILD(leftMember, 0)) : leftMember));
								touched_data_types[right].insert(
									read_definite_member_name(right_is_pattern ? 
										(GET_CHILD(rightMember, 0)): rightMember));
								break;
							default: assert(false);
						}
					}
					break;
					default: assert(false);
				} // end switch on values correspHead type
			} // end FOR_ALL_CHILDREN of VALUES
		}} // end FOR_ALL_CHILDREN of pairwise
	}
	
	void link_derivation::expand_patterns_and_process(
		module_ptr left,
		module_ptr right,
		const std::vector<antlr::tree::Tree *>& deferred
	)
	{
		for (auto i_d = deferred.begin(); i_d != deferred.end(); ++i_d)
		{
			antlr::tree::Tree *n = *i_d;
			INIT;
			BIND2(n, correspHead);
			switch (GET_TYPE(correspHead))
			{
				case CAKE_TOKEN(LR_DOUBLE_ARROW):
				// left is the source, right is the sink
				{
					INIT;
					matches_info_t matched;
					BIND3(correspHead, sourcePattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(sourcePattern, eventContext);
						BIND3(sourcePattern, pattern, KEYWORD_PATTERN);
						matched = find_subprograms_matching_pattern(
							left, regex_from_pattern_ast(pattern)
						);
					}
					BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
					BIND2(correspHead, sinkExpr);
					BIND3(correspHead, returnEvent, RETURN_EVENT);
					
					// for each matching caller, add an event corresp
					for (auto i_match = matched.begin(); i_match != matched.end(); ++i_match)
					{
						// only if not already touched
						if (touched_events[left].find(
								definite_member_name(1, *i_match->first->get_name())) 
							== touched_events[left].end())
						{
							process_non_ident_pattern_event_corresp(
								left,
								right,
								make_non_ident_pattern_event_corresp(
									true, /* is_left_to_right */
									*i_match->first->get_name(),
									i_match->second,
									// then as before
									sourcePattern,
									sourceInfixStub,
									sinkInfixStub,
									sinkExpr,
									returnEvent
								),
								true // is_compiler_generated
							);
						
						}
					}
					
				}
				break;
				case CAKE_TOKEN(RL_DOUBLE_ARROW):
				// right is the source, left is the sink
				{
					INIT;
					matches_info_t matched;
					BIND2(correspHead, sinkExpr);
					BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, pattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(pattern, eventContext);
						BIND3(pattern, pattern, KEYWORD_PATTERN);
						matched = find_subprograms_matching_pattern(
							right, regex_from_pattern_ast(pattern)
						);
						
					}
					BIND3(correspHead, returnEvent, RETURN_EVENT);
					
					// for each match, add an event corresp
					for (auto i_match = matched.begin(); i_match != matched.end(); ++i_match)
					{
						// only if not already touched
						if (touched_events[right].find(
							definite_member_name(1, *i_match->first->get_name())) 
							== touched_events[right].end())
						{
							process_non_ident_pattern_event_corresp(
								left,
								right,
								make_non_ident_pattern_event_corresp(
									false, /* is_left_to_right */
									*i_match->first->get_name(),
									i_match->second,
									// then as before
									pattern,
									sourceInfixStub,
									sinkInfixStub,
									sinkExpr,
									returnEvent
								),
								true
							);
						}
					}
				}
				break;
				case CAKE_TOKEN(BI_DOUBLE_ARROW):
				// do it up to twice
				{
					INIT;
					matches_info_t lr_matched;
					matches_info_t rl_matched;
					BIND3(correspHead, leftPattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(leftPattern, eventContext);
						BIND2(leftPattern, memberNameExprOrPattern);
						if (GET_TYPE(memberNameExprOrPattern) == CAKE_TOKEN(KEYWORD_PATTERN))
						{
							lr_matched = find_subprograms_matching_pattern(
								left,
								regex_from_pattern_ast(memberNameExprOrPattern)
							);
						}
						else
						{
							// the left is *not* a pattern, so the right *must* be.
							// *** just add one rule, in this direction?
							// This doesn't make sense, because what will the stub be?
							cerr << "Warning: ident pattern rule " << CCP(TO_STRING_TREE(correspHead))
								<< " will only generate correspondences with source "
								<< right->get_filename()
								<< " and sink " << left->get_filename() << endl;
						}
					}
					BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, rightPattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(rightPattern, eventContext);
						BIND2(rightPattern, memberNameExprOrPattern);
						if (GET_TYPE(memberNameExprOrPattern) == CAKE_TOKEN(KEYWORD_PATTERN))
						{
							rl_matched = find_subprograms_matching_pattern(
								left,
								regex_from_pattern_ast(memberNameExprOrPattern)
							);
						}
						else
						{
							// the right is *not* a pattern, so the left *must* be.
							// *** just add one rule, in this direction?
							// This doesn't make sense, because what will the stub be?
							cerr << "Warning: ident pattern rule " << CCP(TO_STRING_TREE(correspHead))
								<< " will only generate correspondences with source "
								<< left->get_filename()
								<< " and sink " << right->get_filename() << endl;
						}

					}
					BIND3(correspHead, returnEvent, RETURN_EVENT);
					
					// in each direction
					unsigned total = 0;
					if (lr_matched.size() > 0)
					{
						for (auto i_match = lr_matched.begin(); i_match != lr_matched.end(); ++i_match)
						{
							// only if not already touched
							if (touched_events[left].find(
								definite_member_name(1, *i_match->first->get_name())) 
								== touched_events[left].end())
							{
								process_non_ident_pattern_event_corresp(
									left,
									right,
									make_non_ident_pattern_event_corresp(
										true, /* is_left_to_right */
										*i_match->first->get_name(),
										i_match->second,
										// then as before
										leftPattern,
										leftInfixStub,
										rightInfixStub,
										rightPattern,
										returnEvent
									),
									true // is_compiler_generated
								);
								++total;
							}
						}
					
					}
					else if (rl_matched.size() > 0)
					{
						for (auto i_match = rl_matched.begin(); i_match != rl_matched.end(); ++i_match)
						{
							if (touched_events[right].find(
								definite_member_name(1, *i_match->first->get_name())) 
								== touched_events[right].end())
							{
								process_non_ident_pattern_event_corresp(
									left,
									right,
									make_non_ident_pattern_event_corresp(
										false, /* is_left_to_right */
										*i_match->first->get_name(),
										i_match->second,
										// then as before
										rightPattern,
										rightInfixStub,
										leftInfixStub,
										leftPattern,
										returnEvent
									),
									true // is_compiler_generated
								);
								++total;
							}
						}
					}
					cerr << "Pattern rule " << CCP(TO_STRING_TREE(correspHead))
						<< " generated " << total << " non-pattern corresps." << endl;
					
				}
				break;
				default: RAISE(correspHead, "expected a long arrow (<--, -->, <-->)");
			} // end switch
		} // end for deferred
	}
	
	link_derivation::matches_info_t
	link_derivation::find_subprograms_matching_pattern(
		module_ptr module,
		const boost::regex& re
	)
	{
		matches_info_t v;
		boost::smatch m;

		/* First we match all event names against the pattern */
		// DEBUG: print out CUs
// 		cerr << "Module is at " << module.get() << endl;
// 		for (auto i_cu = module->get_ds().toplevel()->compile_unit_children_begin();
// 				i_cu !=  module->get_ds().toplevel()->compile_unit_children_end();
// 				++i_cu)
// 		{
// 			cerr << "Module has a CU: " << *(*i_cu)->get_name() 
// 				<< " with " << srk31::count((*i_cu)->children_begin(), (*i_cu)->children_end())
// 					<< " children." << endl;
// 			
// 		}
		
		auto seq = module->get_ds().toplevel()->visible_grandchildren_sequence();
		unsigned seq_count = srk31::count(seq->begin(), seq->end());
		//cerr << "Module has " << seq_count << " visible grandchildren." << endl;
		unsigned visible_grandchildren_count = 0;
		unsigned subprogram_count = 0;
		for (auto i_child = seq->begin(); i_child != seq->end(); ++i_child, ++visible_grandchildren_count)
		{
			//cerr << "Module has a CU (" << *(*i_child)->enclosing_compile_unit()->get_name() 
			//	<< ")-toplevel " << (*i_child)->summary() << endl;
			if ((*i_child)->get_tag() == DW_TAG_subprogram)
			{
				++subprogram_count;
				auto subp = dynamic_pointer_cast<subprogram_die>(*i_child);
				if (subp && subp->get_declaration() && *subp->get_declaration()
				 && subp->get_name())
				{
					auto name = *subp->get_name();
					cerr << "Does name " << name << " match pattern " << re << "? ";
					// it's a declared-not-defined function...
					// ... does it match the pattern?
					if (boost::regex_match(name, m, re))
					{
						cerr << "yes." << endl;
						cerr << "pattern " << re
							<< " matched " << *subp
							<< endl;

						v.push_back(make_pair(subp, m));
					}
					else cerr << "no." << endl;
				}
			}
		}
		//cerr << "Considering pattern " << re << " we saw " << visible_grandchildren_count 
		//	<< " visible DIEs and " << subprogram_count << " subprograms. " << endl;
	
		return v;
	}

	void link_derivation::process_non_ident_pattern_event_corresp(
		module_ptr left,
		module_ptr right, 
		antlr::tree::Tree *n,
		bool is_compiler_generated)
	{
		assert(GET_TYPE(n) == CAKE_TOKEN(EVENT_CORRESP));
		INIT;
		BIND2(n, correspHead);
		
		switch (GET_TYPE(correspHead))
		{
			case CAKE_TOKEN(LR_DOUBLE_ARROW):
				// left is the source, right is the sink
				{
					INIT;
					BIND3(correspHead, sourcePattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(sourcePattern, eventContext);
						BIND3(sourcePattern, memberNameExpr, DEFINITE_MEMBER_NAME);
						touched_events[left].insert(
							read_definite_member_name(memberNameExpr));
					}
					BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
					BIND2(correspHead, sinkExpr);
					BIND3(correspHead, returnEvent, RETURN_EVENT);

					add_event_corresp(left, sourcePattern, sourceInfixStub,
						right, sinkExpr, sinkInfixStub, returnEvent, correspHead,
						false, false, false, is_compiler_generated);
				}
				break;
			case CAKE_TOKEN(RL_DOUBLE_ARROW):
				// right is the source, left is the sink
				{
					INIT;
					BIND2(correspHead, sinkExpr);
					BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, sourcePattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(sourcePattern, eventContext);
						BIND3(sourcePattern, memberNameExpr, DEFINITE_MEMBER_NAME);
						touched_events[right].insert(
							read_definite_member_name(memberNameExpr));
					}
					BIND3(correspHead, returnEvent, RETURN_EVENT);
					add_event_corresp(right, sourcePattern, sourceInfixStub,
						left, sinkExpr, sinkInfixStub, returnEvent, correspHead,
						false, false, false, is_compiler_generated);
				}
				break;
			case CAKE_TOKEN(BI_DOUBLE_ARROW):
				// add *two* correspondences
				{
					INIT;
					BIND3(correspHead, leftPattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(leftPattern, eventContext);
						BIND3(leftPattern, memberNameExpr, DEFINITE_MEMBER_NAME);
						touched_events[left].insert(
							read_definite_member_name(memberNameExpr));
					}
					BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
					BIND3(correspHead, rightPattern, EVENT_PATTERN);
					{
						INIT;
						BIND2(rightPattern, eventContext);
						BIND3(rightPattern, memberNameExpr, DEFINITE_MEMBER_NAME);
						touched_events[right].insert(
							read_definite_member_name(memberNameExpr));
					}
					BIND3(correspHead, returnEvent, RETURN_EVENT);
					add_event_corresp(left, leftPattern, leftInfixStub, 
						right, rightPattern, rightInfixStub, returnEvent, correspHead,
						false, false, false, is_compiler_generated);
					add_event_corresp(right, rightPattern, rightInfixStub,
						left, leftPattern, leftInfixStub, returnEvent, correspHead,
						false, false, false, is_compiler_generated);
				}
				break;
			default: RAISE(correspHead, "expected a long arrow (<--, -->, <-->)");
		}
	}
	
	void link_derivation::ensure_all_artificial_data_types(
		antlr::tree::Tree *ast,
		module_ptr p_module,
		optional<const ev_corresp&> opt_corresp)
	{
		// ensure that any artificial data types mentioned 
		// in this corresp exist in the DWARF info
		std::vector<antlr::tree::Tree *> interpretations;
		walk_ast_depthfirst(ast, interpretations, 
			[p_module, this, opt_corresp, ast](antlr::tree::Tree *t)
			{
				bool relevant = (GET_TYPE(t) == CAKE_TOKEN(KEYWORD_AS)
				||      GET_TYPE(t) == CAKE_TOKEN(KEYWORD_INTERPRET_AS)
				||      GET_TYPE(t) == CAKE_TOKEN(KEYWORD_IN_AS)
				||      GET_TYPE(t) == CAKE_TOKEN(KEYWORD_OUT_AS));
				return relevant;
			});
		for (auto i_interp = interpretations.begin(); 
			i_interp != interpretations.end();
			++i_interp)
		{
			auto t = *i_interp;
			auto target_type_in_ast = GET_CHILD(t, 0);
			auto interpreted_expr = GET_CHILD(GET_PARENT(t), 0);
			try
			{
				auto p_t = p_module->ensure_dwarf_type(target_type_in_ast);
				assert(p_t);
			}
			catch (...) //(dwarf::lib::Not_supported)
			{
				// this means we asked to create a typedef with no definition
				// -- try to get the *existing* DWARF type of this AST
				// context.
				auto found = map_ast_context_to_dwarf_element(
					target_type_in_ast,
					p_module,
					false
				);
				assert(found);
				cerr << "found " << *found << " as DWARF context of " 
					<< CCP(TO_STRING_TREE(target_type_in_ast)) << endl;
				if (found->get_tag() == DW_TAG_unspecified_parameters)
				{
					cerr << "Was not expecting to find unspecified_parameters "
						<< "for subprogram " << *found->get_parent() << endl
						<< " with children: ";
					for (auto i_child = found->get_parent()->children_begin();
						i_child != found->get_parent()->children_end();
						++i_child)
					{ cerr << **i_child << endl; }

					assert(false);
				}
				auto can_have_type = dynamic_pointer_cast<spec::with_type_describing_layout_die>(found);
				assert(can_have_type);
				auto maybe_subprogram = can_have_type->get_parent();
				assert(maybe_subprogram);
				shared_ptr<subprogram_die> subprogram;
				subprogram = dynamic_pointer_cast<subprogram_die>(maybe_subprogram);
				shared_ptr<type_die> is_type = can_have_type->get_type();
				shared_ptr<spec::with_type_describing_layout_die> has_type = can_have_type;
				if (can_have_type && !is_type)
				{
					cerr << "Guessing type info for arg "
						<< *can_have_type
						<< endl;
					// this means we have no type info for this arg.
					// This might happen if we use "as" in an
					// event pattern (=> caller-side => no DWARF info).

					// we need an identifier (argument name)
					string ident;
					if (GET_TYPE(interpreted_expr) == CAKE_TOKEN(IDENT))
					{
						ident = unescape_ident(CCP(GET_TEXT(interpreted_expr)));
					}
					else if (GET_TYPE(interpreted_expr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
					{
						auto dmn = read_definite_member_name(interpreted_expr);
						assert(dmn.size() == 1);
						ident = dmn.at(0);
					}
					else assert (false);
					// we also need an enclosing corresp
					antlr::tree::Tree *enclosing_corresp = interpreted_expr;
					antlr::tree::Tree *enclosing_event_pattern = 0;
					assert(opt_corresp);

// 							while (GET_TYPE(enclosing_corresp) != CAKE_TOKEN(BI_DOUBLE_ARROW)
// 							 &&    GET_TYPE(enclosing_corresp) != CAKE_TOKEN(LR_DOUBLE_ARROW)
// 							 &&    GET_TYPE(enclosing_corresp) != CAKE_TOKEN(RL_DOUBLE_ARROW) )
// 							{
// 								cerr << "Ascending through " << CCP(TO_STRING_TREE(enclosing_corresp)) << endl;
// 								if (GET_TYPE(enclosing_corresp) == CAKE_TOKEN(EVENT_PATTERN))
// 								{
// 									enclosing_event_pattern = enclosing_corresp;
// 								}
// 								enclosing_corresp = GET_PARENT(enclosing_corresp);
// 							}
// 							assert(enclosing_event_pattern);

					// Now use that to recover our module-level context
					// which module's DWARF info should we search through?
// 							// HACK HACK HACK HACK
// 							ev_corresp_map_t::iterator i_corresp;
// 							for (i_corresp = ev_corresps.begin();
// 									i_corresp != ev_corresps.end();
// 									++i_corresp)
// 							{
// 								if (i_corresp->second.corresp
// 								 && i_corresp->second.corresp == enclosing_corresp)
// 											break;
// 							}
// 							assert(i_corresp != ev_corresps.end());

					//assert(module_name && string(module_name) != "");
					//auto source_module = r.module_inverse_tbl[module_name];
					//assert(source_module);

					module_ptr source_module = opt_corresp->source;
					vector<antlr::tree::Tree *> contexts;
					find_usage_contexts(ident,
						opt_corresp->sink_expr,
						contexts);
					if (contexts.size() > 0)
					{
						cerr << "Considering type expectations "
							<< "for stub uses of identifier " 
							<< ident << endl;
						shared_ptr<type_die> seen_concrete_type;
						for (auto i_ctxt = contexts.begin(); 
							i_ctxt != contexts.end();
							++i_ctxt)
						{
							shared_ptr<spec::basic_die> found_die
							 = map_ast_context_to_dwarf_element(
								*i_ctxt,
								opt_corresp->sink, false);

							if (found_die)
							{
								cerr << "Found a stub function call using ident " 
									<< ident 
									<< " as " << *found_die
									<< ", child of " << *found_die->get_parent()
									<< endl;

								has_type = dynamic_pointer_cast<spec::with_type_describing_layout_die>(found_die);
								assert(has_type);
								shared_ptr<type_die> is_type = has_type->get_type();
								if (has_type && !is_type)
								{
									// this means we found an arg with no type info
									continue;
								}

								if (seen_concrete_type)
								{
									if (seen_concrete_type != is_type->get_concrete_type())
									{
										// disagreement in usage contexts of ident....
										assert(false);
									}
								} 
								else seen_concrete_type = is_type->get_concrete_type();
							}
						} // end for all contexts
						// when we get here, we may have identified some
						// type expectations, or we may not.

						bool is_indirect = false;
						if (seen_concrete_type 
							&& seen_concrete_type->get_tag() == DW_TAG_pointer_type)
						{
							is_indirect = true;
							seen_concrete_type = dynamic_pointer_cast<pointer_type_die>(
								seen_concrete_type)->get_type();
						}

						if (seen_concrete_type)
						{
							assert(source_module);

							// okay, check for unique corresponding type
							auto found_unique_from_sink_to_source
							 = unique_corresponding_dwarf_type(
								seen_concrete_type,
								source_module,
								true);
							auto found_unique_from_source_to_sink
							 = unique_corresponding_dwarf_type(
								seen_concrete_type,
								source_module,
								false);

							if (found_unique_from_sink_to_source
							 && found_unique_from_sink_to_source
								== found_unique_from_source_to_sink)
							{
								// success!
								auto encap_fp = dynamic_pointer_cast<encap::formal_parameter_die>(
									can_have_type);
								auto fp_type = 
									(is_indirect) 
									? source_module->ensure_dwarf_type(
											build_ast(
												GET_FACTORY(ast),
												CAKE_TOKEN(KEYWORD_PTR),
												"ptr",
												(vector<antlr::tree::Tree*>){
													build_ast(
														GET_FACTORY(ast),
														CAKE_TOKEN(IDENT),
														/* HACK */ *seen_concrete_type->get_name(),
														vector<antlr::tree::Tree*>()
													)
												}
											)
										)
									: found_unique_from_sink_to_source;

								encap_fp->set_type(found_unique_from_sink_to_source);

								is_type = fp_type;
							}
							else
							{
								cerr << "No unique corresponding type to " 
									<< *seen_concrete_type
									<< " so not filling in missing caller type information."
									<< endl;
							}
						}
					} // end if contexts.size() > 0

				} // end if can_have_type && !is_type

				assert(is_type);

				// if we are dealing with a virtual data type, we do things differently
				auto interp_head = GET_PARENT(target_type_in_ast);
				assert(GET_TYPE(interp_head) == CAKE_TOKEN(KEYWORD_AS)
				   ||  GET_TYPE(interp_head) == CAKE_TOKEN(KEYWORD_IN_AS)
				   ||  GET_TYPE(interp_head) == CAKE_TOKEN(KEYWORD_OUT_AS)
				   ||  GET_TYPE(interp_head) == CAKE_TOKEN(KEYWORD_INTERPRET_AS));
				if (GET_TYPE(GET_CHILD(interp_head, 1))
					== CAKE_TOKEN(VALUE_CONSTRUCT) 
					&& GET_CHILD_COUNT(GET_CHILD(interp_head, 1)) > 0)
				{
					antlr::tree::Tree *val_construct = GET_CHILD(interp_head, 1);

					// for now, we only do this for pointers
					if (is_type->get_concrete_type()->get_tag() != DW_TAG_pointer_type)
					{
						cerr << "Bad type: " << *is_type->get_concrete_type() << endl;
						cerr << "For interpretation: " << CCP(TO_STRING_TREE(interp_head)) << endl;
						cerr << "Referenced by: " << *has_type->get_parent() << endl;
						cerr << "All referer's children: " << endl;
						for (auto i_child = has_type->get_parent()->children_begin();
							i_child != has_type->get_parent()->children_end();
							++i_child)
						{
							cerr << **i_child;
						}
						assert(false);
					}
					shared_ptr<encap::pointer_type_die> is_pointer_type
					 = dynamic_pointer_cast<encap::pointer_type_die>(is_type);
					// also for now: only do this for void-typed pointers!
					// (saves us having to replace the target type with our new fake type)
					assert(!is_pointer_type->get_type());

					auto created = p_module->create_empty_structure_type(
						CCP(GET_TEXT(target_type_in_ast)));
					dynamic_pointer_cast<encap::structure_type_die>(created)
						->set_artificial(true);
					/* We create a reference-typed member for each argument. */
					{
						INIT;
						FOR_ALL_CHILDREN(val_construct)
						{
							ALIAS3(n, named_field, IDENT);
							string field_name = unescape_ident(CCP(GET_TEXT(named_field)));
							// look for a parameter of this name, and grab its type
							shared_ptr<formal_parameter_die> found_fp;
							shared_ptr<type_die> found_type;
							for (auto i_child = subprogram->formal_parameter_children_begin();
								i_child != subprogram->formal_parameter_children_end();
								++i_child)
							{
								if ((*i_child)->get_name() && *(*i_child)->get_name()
									== field_name)
								{
									if (!(*i_child)->get_type())
									{
										RAISE(named_field, 
											"virtual data types require parameter type information"
										);
									}
									found_fp = *i_child;
									found_type = (*i_child)->get_type();
									break;
								}
							}
							if (!found_fp)
							{
								cerr << "Problem with " << *subprogram << endl;
								RAISE(named_field, "no such field in subprogram");
							}

							// now we have a type; add a member
							auto member_type = p_module->ensure_reference_type_with_target(
								arg_is_indirect(found_fp) 
								? dynamic_pointer_cast<pointer_type_die>(found_type)->get_type()
								: found_type
							);
							auto member = dwarf::encap::factory::for_spec(
								dwarf::spec::DEFAULT_DWARF_SPEC
							).create_die(DW_TAG_member,
								dynamic_pointer_cast<encap::structure_type_die>(created),
								opt<string>(field_name)
							);
							dynamic_pointer_cast<encap::member_die>(member)->set_type(
								dynamic_pointer_cast<spec::type_die>(member_type));
							cerr << "Set type of member named " << field_name
								<< " to " << *member_type << endl;
							cerr << "Referent type is " << *found_type << endl;
							cerr << "In cxx: " << compiler.cxx_declarator_from_type_die(
								found_type).first << endl;

						}
					}


					//is_pointer_type->set_type(dynamic_pointer_cast<spec::type_die>(created));
					//has_type->set_type(p_module->ensure_pointer_type_with_target(


					cerr << "Created opaque structure named " 
						<< CCP(GET_TEXT(target_type_in_ast))
						<< endl;
					cerr << "Really! " << *created  << endl;
					cerr << "Enclosing CU: " << created->enclosing_compile_unit()->summary()
						<< endl;
				}
				else // not a vconstruct case
				{
					// artificial data type: now handle implicit indirection (pointerness)
					assert(is_type->get_concrete_type());
					if (is_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type
						&& GET_TYPE(target_type_in_ast) != CAKE_TOKEN(KEYWORD_PTR))
					{
						// the DWARF context wants a pointer, and our AST doesn't
						// say it's a pointer. Infer that it *is* a pointer.
						// The typedef should alias the pointed-to type.
						shared_ptr<pointer_type_die> is_pointer_type
						 = dynamic_pointer_cast<pointer_type_die>(is_type->get_concrete_type());
						shared_ptr<type_die> pointed_to_type = is_pointer_type->get_type();
						cerr << "Found alias of void pointer type: " << is_pointer_type->summary()
							<< endl;
						if (!pointed_to_type)
						{
							cerr << "Found alias of void pointer type: " << is_pointer_type->summary()
								<< endl;
							 RAISE(target_type_in_ast,
								"cannot apply indirect interpretation on void pointer type");
						}
						is_type = pointed_to_type;
					}
					// converse case: the DWARF context doesn't want a pointer,
					// but our AST says it is a pointer: this is probably bad Cake,
					// and we catch it by asserting that our corresps never take pointer types.

					// now create a typedef DIE pointing at that type
					auto created = p_module->create_typedef(is_type, CCP(GET_TEXT(target_type_in_ast)));
					cerr << "Created typedef of " << *is_type << " named " << CCP(GET_TEXT(target_type_in_ast))
						<< endl;
					cerr << "Really! " << *created << endl;
				} // end else
			} // end catch
		} // end for interpretation
	}
	
	void link_derivation::add_event_corresp(
		module_ptr source, 
		antlr::tree::Tree *source_pattern,
		antlr::tree::Tree *source_infix_stub,
		module_ptr sink,
		antlr::tree::Tree *sink_expr,
		antlr::tree::Tree *sink_infix_stub,
		antlr::tree::Tree *return_leg,
		antlr::tree::Tree *corresp_head,
		bool free_source,
		bool free_sink,
		bool init_only,
		bool is_compiler_generated)
	{
		auto key = sorted(make_pair(source, sink));
		assert(all_iface_pairs.find(key) != all_iface_pairs.end());
		
		antlr::tree::Tree *sink_expr_as_stub = 0;
		sink_expr_as_stub = sink_expr;
		assert(GET_TYPE(sink_expr) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
		
		auto corresp = (struct ev_corresp){ /*.source = */ source, // source is the *requirer*
							/*.source_pattern = */ source_pattern,
							/*.source_infix_stub = */ source_infix_stub,
							/*.sink = */ sink, // sink is the *provider*
							/*.sink_expr = */ sink_expr_as_stub,
							/*.sink_infix_stub = */ sink_infix_stub,
							return_leg,
							corresp_head,
							/*.source_pattern_to_free = */ (free_source) ? source_pattern : 0,
							/*.sink_pattern_to_free = */ (free_sink) ? sink_expr : 0 };

		ensure_all_artificial_data_types(source_pattern, source, corresp);
		if (source_infix_stub) ensure_all_artificial_data_types(source_infix_stub, source, corresp);
		ensure_all_artificial_data_types(sink_expr_as_stub, sink, corresp);
		if (sink_infix_stub) ensure_all_artificial_data_types(sink_infix_stub, sink, corresp);
		if (return_leg) ensure_all_artificial_data_types(return_leg, source, corresp);
		
		// If we're compiler-generated, do a sanity check and discard
		// if it looks bogus. This is used for pattern rules: the
		// front-end has created patterns for every matching caller,
		// regardless of whether the stub will compiler. So we wil
		// discard those with single-invocation stubs.
		if (is_compiler_generated && !stub_is_sane(sink_expr_as_stub, sink))
		{
			cerr << "Warning: skipping compiler-generated event correspondence with non-sane stub expr "
				 << CCP(TO_STRING_TREE(sink_expr_as_stub)) << endl;
			return;
		}
		
		ev_corresps.insert(make_pair(key, corresp));
		
		/* print debugging info: what type expectations did we find in the stubs? */
		//if (sink_expr)
		//{
		
// 			multimap< string, shared_ptr<dwarf::spec::type_die> > out;
// 			find_type_expectations_in_stub(sink,f
// 				sink_expr_as_stub, shared_ptr<dwarf::spec::type_die>(), // FIXME: use return type
// 				out);
// 			cerr << "stub: " << CCP(TO_STRING_TREE(sink_expr_as_stub))
// 				<< " implies type expectations as follows: " << endl;
// 			if (out.size() == 0) cerr << "(no type expectations inferred)" << endl;
// 			else for (auto i_exp = out.begin(); i_exp != out.end(); ++i_exp)
// 			{
// 				cerr << i_exp->first << " as " << *i_exp->second << endl;
// 			}
			
		//}
	}

	/* This version is called from processing the pairwise block AST.
	 * It differs from the canonical version only in that 
	 * it accepts the source and sink as definite_member_names,
	 * not DIE pointers. */
	link_derivation::val_corresp_map_t::iterator
	link_derivation::add_value_corresp(
		module_ptr source, 
		antlr::tree::Tree *source_data_type_mn,
		antlr::tree::Tree *source_infix_stub,
		module_ptr sink,
		antlr::tree::Tree *sink_data_type_mn,
		antlr::tree::Tree *sink_infix_stub,
		antlr::tree::Tree *refinement,
		bool source_is_on_left,
		antlr::tree::Tree *corresp,
		bool init_only
	)
	{
		if (source_infix_stub) ensure_all_artificial_data_types(source_infix_stub, source);
		if (sink_infix_stub) ensure_all_artificial_data_types(sink_infix_stub, sink);

		auto source_mn = read_definite_member_name(source_data_type_mn);
		auto source_data_type_opt = dynamic_pointer_cast<dwarf::spec::type_die>(
			source->get_ds().toplevel()->visible_resolve(
			source_mn.begin(), source_mn.end()));
		if (!source_data_type_opt) RAISE(corresp, "could not resolve source data type");
		assert(module_of_die(source_data_type_opt) == source);
		auto sink_mn = read_definite_member_name(sink_data_type_mn);
		auto sink_data_type_opt = dynamic_pointer_cast<dwarf::spec::type_die>(
			sink->get_ds().toplevel()->visible_resolve(
			sink_mn.begin(), sink_mn.end()));
		if (!sink_data_type_opt) RAISE(corresp, "could not resolve sink data type");
		assert(module_of_die(sink_data_type_opt) == sink);
		
		// we should never have corresps between pointer types
		assert(source_data_type_opt->get_concrete_type()->get_tag() != DW_TAG_pointer_type);
		assert(sink_data_type_opt->get_concrete_type()->get_tag() != DW_TAG_pointer_type);
		
		return add_value_corresp(source, 
			source_data_type_opt, 
			source_infix_stub,
			sink, 
			sink_data_type_opt, 
			sink_infix_stub,
			refinement, source_is_on_left, corresp, init_only);
	}
	
	/* This version is used to add implicit dependencies. There is no
	 * refinement or corresp or any of the other syntactic stuff. */
	bool link_derivation::ensure_value_corresp(module_ptr source, 
		shared_ptr<dwarf::spec::type_die> source_data_type,
		module_ptr sink,
		shared_ptr<dwarf::spec::type_die> sink_data_type,
		bool source_is_on_left)
	{
		auto key = sorted(make_pair(module_of_die(source_data_type), 
			module_of_die(sink_data_type)));
		assert(all_iface_pairs.find(key) != all_iface_pairs.end());
		assert(source_data_type);
		assert(sink_data_type);

		auto iter_pair = val_corresps.equal_range(key);
		for (auto i = iter_pair.first; i != iter_pair.second; i++)
		{
			if (i->second->source_data_type == source_data_type
				&& i->second->sink_data_type == sink_data_type)
			{
				return false; // no need to add
			}
		}
		// If we got here, we didn't find one
		auto source_name_parts = compiler.fq_name_parts_for(source_data_type);
		auto sink_name_parts = compiler.fq_name_parts_for(sink_data_type);
		add_value_corresp(module_of_die(source_data_type), 
			source_data_type,
			0,
			module_of_die(sink_data_type),
			sink_data_type,
			0,
			0,
			true, // source_is_on_left -- irrelevant as we have no refinement
			make_simple_corresp_expression(source_is_on_left ?
				 	source_name_parts : sink_name_parts,
				 source_is_on_left ?
				 	sink_name_parts : source_name_parts ));
		return true;

		assert(false);
	}

	
	/* This is the "canonical" version, called from implicit name-matching */
	link_derivation::val_corresp_map_t::iterator
	link_derivation::add_value_corresp(
		module_ptr source, 
		shared_ptr<dwarf::spec::type_die> source_data_type,
		antlr::tree::Tree *source_infix_stub,
		module_ptr sink,
		shared_ptr<dwarf::spec::type_die> sink_data_type,
		antlr::tree::Tree *sink_infix_stub,
		antlr::tree::Tree *refinement,
		bool source_is_on_left,
		antlr::tree::Tree *corresp,
		bool init_only
	)
	{
// 		cerr << "Adding value corresp from source module @" << &*source
// 			<< " source data type @" << &*source_data_type << " " << *source_data_type
// 			<< " source infix stub @" << source_infix_stub
// 			<< " to sink module @" << &*sink
// 			<< " sink data type @" << &*sink_data_type << " " << *sink_data_type
// 			<< " sink infix stub @" << sink_infix_stub
// 			<< " refinement @" << refinement
// 			<< " source on " << (source_is_on_left ? "left" : "right")
// 			<< " corresp @" << corresp << endl;
		
		// remember which typedefs matter
		if (source_data_type->get_concrete_type() != source_data_type)
		{
			significant_typedefs.insert(source_data_type);
		}
		if (sink_data_type->get_concrete_type() != sink_data_type)
		{
			significant_typedefs.insert(sink_data_type);
		}
		
		/* Handling dependencies:
		 * Value correspondences may have dependencies on other value correspondences. 
		 * Because they're emitted as template specialisations, they are sensitive to order. 
		 * We want to forward-declare each specialisation. 
		 * This means we should do two passes.
		 * However, we *don't* need to do dependency analysis / topsort.
		 
		 * To ensure that depended-upon value corresps are generated, 
		 * each value_conversion object can generate a list 
		 * describing the other conversions that it implicitly depends upon. 
		 * *After* we've added all explicitly-described *and* all name-matched
		 * correspondences to the map, we do a fixed-point iteration to add
		 * all the dependencies.
		 */
		
		auto key = sorted(make_pair(source, sink));
		assert(all_iface_pairs.find(key) != all_iface_pairs.end());
		assert(source_data_type);
		assert(module_of_die(source_data_type) == source);
		assert(sink_data_type);
		assert(module_of_die(sink_data_type) == sink);
//     	val_corresps.insert(make_pair(key,

		auto basic = (struct basic_value_conversion){ /* .source = */ source,
							/* .source_data_type = */ source_data_type,
							/* .source_infix_stub = */ source_infix_stub,
							/* .sink = */ sink,
							/* .sink_data_type = */ sink_data_type,
							/* .source_infix_stub = */ sink_infix_stub,
							/* .refinement = */ refinement,
							/* .source_is_on_left = */ source_is_on_left,
							/* .corresp = */ corresp,
							/* .init_only = */ init_only };
		
		// we *only* emit conversions between concrete types
		auto source_concrete_type = source_data_type->get_concrete_type();
		auto sink_concrete_type = sink_data_type->get_concrete_type();
	
		// corresps involving virtual data types: catch them early
		if ((source_concrete_type && source_concrete_type->get_tag() == DW_TAG_structure_type
			&& source_concrete_type->get_artificial() && *source_concrete_type->get_artificial())
			|| (sink_concrete_type && sink_concrete_type->get_tag() == DW_TAG_structure_type
			&& sink_concrete_type->get_artificial() && *sink_concrete_type->get_artificial()))
		{
			cerr << "Creating virtual value correspondence from " 
				<< get_type_name(source_data_type)
				<< " to " << get_type_name(sink_data_type)
				<< endl;
			auto ret = val_corresps.insert(make_pair(key,
				dynamic_pointer_cast<value_conversion>(
					make_shared<virtual_value_conversion>(wrap_code, basic))));
			return ret;
		}
	
		// skip incomplete (void) typedefs and other incompletes
		if (!(source_concrete_type && compiler.cxx_is_complete_type(source_concrete_type))
		|| !(sink_concrete_type && compiler.cxx_is_complete_type(sink_concrete_type)))
		{
			cerr << "Warning: skipping value conversion from " << get_type_name(source_data_type)
				<< " to " << get_type_name(sink_data_type)
				<< " because one or other is an incomplete type." << endl;
			//m_out << "// (skipped because of incomplete type)" << endl << endl;
			auto ret = val_corresps.insert(make_pair(key, 
				dynamic_pointer_cast<value_conversion>(
					make_shared<skipped_value_conversion>(wrap_code, 
					basic, string("incomplete type")))));
			return ret;
		}
		
		// now we can compute the concrete type names 
		auto from_typename = get_type_name(source_concrete_type);
		auto to_typename = get_type_name(sink_concrete_type);

		// skip pointers and references
		if (source_concrete_type->get_tag() == DW_TAG_pointer_type
		|| sink_concrete_type->get_tag() == DW_TAG_pointer_type
		|| source_concrete_type->get_tag() == DW_TAG_reference_type
		|| sink_concrete_type->get_tag() == DW_TAG_reference_type)
		{
			cerr << "Warning: skipping value conversion from " << from_typename
				<< " to " << to_typename
				<< " because one or other is an pointer or reference type." << endl;
			//m_out << "// (skipped because of pointer or reference type)" << endl << endl;
			auto ret = val_corresps.insert(make_pair(key, 
				dynamic_pointer_cast<value_conversion>(
					make_shared<skipped_value_conversion>(wrap_code, 
					basic, "pointer or reference type"))));
			return ret;
		}
		// skip subroutine types
		if (source_concrete_type->get_tag() == DW_TAG_subroutine_type
		|| sink_concrete_type->get_tag() == DW_TAG_subroutine_type)
		{
			cerr << "Warning: skipping value conversion from " << from_typename
				<< " to " << to_typename
				<< " because one or other is a subroutine type." << endl;
			//m_out << "// (skipped because of subroutine type)" << endl << endl;
			auto ret = val_corresps.insert(make_pair(key, 
				dynamic_pointer_cast<value_conversion>(
					make_shared<skipped_value_conversion>(wrap_code, 
					basic, "subroutine type"))));
			return ret;
		}
		
		// from this point, we will generate a candidate for an init rule
		shared_ptr<value_conversion> init_candidate;
			
		bool emit_as_reinterpret = false;
		val_corresp_map_t::iterator ret; 
		if (source_concrete_type->is_rep_compatible(sink_concrete_type)
			&& (!refinement || GET_CHILD_COUNT(refinement) == 0)
			&& (!source_infix_stub || GET_CHILD_COUNT(source_infix_stub) == 0)
			&& (!sink_infix_stub || GET_CHILD_COUNT(sink_infix_stub) == 0))
		{
			// two rep-compatible cases
			if (compiler.cxx_assignable_from(sink_concrete_type, source_concrete_type))
			{
				cerr << "Skipping generation of value conversion from "
					<< from_typename << " to " << to_typename
					<< " because of rep-compatibility and C++-assignability." << endl;
				//m_out << "// (skipped because of rep-compatibility and C++-assignability)" << endl << endl;
				val_corresps.insert(make_pair(key, 
					init_candidate = dynamic_pointer_cast<value_conversion>(
						make_shared<skipped_value_conversion>(
							wrap_code, basic, 
							"rep-compatibility and C++-assignability"
						)
					)
				));
				//return;
				goto add_init_candidate;
			}			
			else
			{
				cerr << "Generating a reinterpret_cast value conversion from "
					<< from_typename << " to " << to_typename
					<< " as they are rep-compatible but not C++-assignable." << endl;
				emit_as_reinterpret = true;
			}
		}
		else
		{
			// rep-incompatible cases are the same in effect but we report them individually
			if (!source_concrete_type->is_rep_compatible(sink_concrete_type))
			{
				cerr << "Generating value conversion from "
					<< from_typename << " to " << to_typename
					<< " as they are not rep-compatible." << endl;
			}
			else
			{
				// FIXME: refinements that just do field renaming
				// are okay -- can recover rep-compatibility so
				// should use reinterpret conversion in these cases
				// + propagate to run-time by generating artificial matching field names
				// for fields whose renaming enables rep-compatibility
				cerr << "Generating value conversion from "
					<< from_typename << " to " << to_typename
					<< " as they have infix stubs and/or nonempty refinement." << endl;
			}
		}

	#define TAG_PAIR(t1, t2) ((t1)<<((sizeof (Dwarf_Half))*8) | (t2))
		// temporary HACK -- emit enumerations as reinterprets, until we have tables
		if (TAG_PAIR(source_data_type->get_concrete_type()->get_tag(), 
				sink_data_type->get_concrete_type()->get_tag())
			== TAG_PAIR(DW_TAG_enumeration_type, DW_TAG_enumeration_type))
		{
			emit_as_reinterpret = true;
		}

		// here goes the value conversion logic
		if (!emit_as_reinterpret)
		{
			// -- dispatch to a function based on the DWARF tags of the two types...
			// ... in *CONCRETE* form
			switch(TAG_PAIR(source_data_type->get_concrete_type()->get_tag(), 
				sink_data_type->get_concrete_type()->get_tag()))
			{
				case TAG_PAIR(DW_TAG_structure_type, DW_TAG_structure_type): {
					//emit_structural_conversion_body(source_data_type, sink_data_type,
					//	refinement, source_is_on_left);
					bool init_and_update_are_identical;
					ret = val_corresps.insert(make_pair(key, 
						init_candidate = dynamic_pointer_cast<value_conversion>(
							make_shared<structural_value_conversion>(
								wrap_code, basic, /*false*/ init_only, 
								init_and_update_are_identical
							)
						)
					));
					// if we need to generate a separate init rule, do so, overriding init_candidate
					if (!init_and_update_are_identical)
					{
						auto new_basic = basic;
						new_basic.init_only = !init_only; // i.e. the converse case
						auto inserted = ret = val_corresps.insert(make_pair(key, 
							dynamic_pointer_cast<value_conversion>(
								make_shared<structural_value_conversion>(
									wrap_code, new_basic, !init_only, 
									init_and_update_are_identical
								)
							)
						));
						if (!init_only)
						{
							// this means we've just added a candidate init rule,
							// so override init_candidate
							init_candidate = inserted->second;
						}
					}
				} break;
				/* If we are corresponding two base types, it's probably because we have 
				 * some infix stub expressions or something. */
				case TAG_PAIR(DW_TAG_base_type, DW_TAG_base_type): {
					bool init_and_update_are_identical;
					ret = val_corresps.insert(make_pair(key, 
						init_candidate = dynamic_pointer_cast<value_conversion>(
							make_shared<primitive_value_conversion>(
								wrap_code, basic, /* false */ init_only, 
								init_and_update_are_identical
							)
						)
					));
					// if we need to generate a separate init rule, do so, 
					// overriding init_candidate
					if (!init_and_update_are_identical)
					{
						auto new_basic = basic;
						new_basic.init_only = !init_only; // i.e. the converse case
						auto inserted = ret = val_corresps.insert(make_pair(key, 
							dynamic_pointer_cast<value_conversion>(
								make_shared<primitive_value_conversion>(
									wrap_code, new_basic, !init_only, 
									init_and_update_are_identical
								)
							)
						));
						if (!init_only)
						{
							// this means we've just added a candidate init rule,
							// so override init_candidate
							init_candidate = inserted->second;
						}
					}
				} break;
				case TAG_PAIR(DW_TAG_enumeration_type, DW_TAG_enumeration_type): {
					// FIXME: just emit a reinterpret for now -- see above
					assert(false);
				}
				default:
					cerr << "Warning: didn't know how to generate conversion between "
						<< *source_data_type << " and " << *sink_data_type << endl;
					//RAISE_INTERNAL(corresp, "unsupported value correspondence");
					return val_corresps.end();
			}
	#undef TAG_PAIR
		}
		else
		{
			//emit_reinterpret_conversion_body(source_data_type, sink_data_type);
			ret = val_corresps.insert(make_pair(key, 
				init_candidate = dynamic_pointer_cast<value_conversion>(
					make_shared<reinterpret_value_conversion>(wrap_code, 
					basic))));
		}
		
	add_init_candidate:
		assert(init_candidate);
		init_rules_key_t init_tbl_key = (init_rules_key_t) {
				(init_candidate->source == key.first),
				init_candidate->source_data_type
			};
		candidate_init_rules_tbl_keys[key].insert(init_tbl_key);
		candidate_init_rules[key].insert(make_pair(
			init_tbl_key,
			init_candidate
		));
		return ret;
	}
	
	shared_ptr<type_die> 
	link_derivation::first_significant_type(shared_ptr<type_die> t)
	{
		while (t->get_concrete_type() != t
			&& significant_typedefs.find(t) == significant_typedefs.end())
		{
			shared_ptr<spec::type_chain_die> tc = dynamic_pointer_cast<spec::type_chain_die>(t);
			assert(tc);
			assert(tc->get_type());
			t = tc->get_type();
		}
		return t;
	}
	
	// Get the names of all functions provided by iface1
	void link_derivation::add_implicit_corresps(iface_pair ifaces)
	{
		cerr << "Adding implicit correspondences between module " 
			<< ifaces.first->get_filename() << " and " << ifaces.second->get_filename()
			<< endl;
	  	
		// find functions required by iface1 and provided by iface2
		name_match_required_and_provided(ifaces, ifaces.first, ifaces.second);
		// find functions required by iface2 and provided by iface1
		name_match_required_and_provided(ifaces, ifaces.second, ifaces.first);
		
		// find DWARF types of matching name
		name_match_types(ifaces);
	}

	void link_derivation::name_match_required_and_provided(
		iface_pair ifaces,
		module_ptr requiring_iface, 
		module_ptr providing_iface)
	{
		assert((requiring_iface == ifaces.first && providing_iface == ifaces.second)
			|| (requiring_iface == ifaces.second && providing_iface == ifaces.first));
			
		/* Search dwarf info */
//		shared_ptr<encap::fiile_toplevel_die> requiring_info
//			= requiring_iface->all_compile_units();

//		shared_ptr<encap::file_toplevel_die> providing_info
//			= providing_iface->all_compile_units();
		
		auto r_toplevel = requiring_iface->get_ds().toplevel();
		auto r_cus = make_pair(r_toplevel->compile_unit_children_begin(),
			r_toplevel->compile_unit_children_end());
		auto r_subprograms = make_shared<
			srk31::conjoining_sequence<
				spec::compile_unit_die::subprogram_iterator
				>
			>();
		for (auto i_cu = r_cus.first; i_cu != r_cus.second; ++i_cu)
		{
			assert(!(*i_cu)->subprogram_children_end().base().base().base().p_policy->is_undefined());
			r_subprograms->append(
				(*i_cu)->subprogram_children_begin(),
				(*i_cu)->subprogram_children_end());
		}
		// if our sequence is empty, we can exit early
		if (r_subprograms->is_empty()) return;
		pair<subprograms_in_file_iterator, subprograms_in_file_iterator> r_subprograms_seq
			= make_pair(
				r_subprograms->begin(/*r_subprograms*/), 
				r_subprograms->end(/*r_subprograms*/)
				);

		auto p_toplevel = providing_iface->get_ds().toplevel();
		auto p_cus = make_pair(p_toplevel->compile_unit_children_begin(),
			p_toplevel->compile_unit_children_end());
		auto p_subprograms = make_shared<
			srk31::conjoining_sequence<
				spec::compile_unit_die::subprogram_iterator
				>
			>();
		for (auto i_cu = p_cus.first; i_cu != p_cus.second; ++i_cu)
		{
			assert(!(*i_cu)->subprogram_children_end().base().base().base().p_policy->is_undefined());
			p_subprograms->append(
				(*i_cu)->subprogram_children_begin(),
				(*i_cu)->subprogram_children_end());
			assert(
				(*i_cu)->subprogram_children_end()
			==  p_subprograms->end().base());
		}
		// if our sequence is empty, we can exit early
		if (p_subprograms->is_empty()) return;
		pair<subprograms_in_file_iterator, subprograms_in_file_iterator> p_subprograms_seq
			= make_pair(
				p_subprograms->begin(/*p_subprograms*/),
				p_subprograms->end(/*p_subprograms*/)
				);
		
		/* */
		required_funcs_iter r_iter(
			r_subprograms_seq.first,
			r_subprograms_seq.second
			);
		required_funcs_iter r_end(
			//r_subprograms->begin(r_subprograms), // requiring_info->subprogram_children_begin(),
			r_subprograms_seq.second,//requiring_info->subprogram_children_end(),
			r_subprograms_seq.second//requiring_info->subprogram_children_end()
			);
		provided_funcs_iter p_iter(
			p_subprograms_seq.first, //providing_info->subprogram_children_begin(), 
			p_subprograms_seq.second//providing_info->subprogram_children_end());
			);
		provided_funcs_iter p_end(
			//p_subprograms->begin(p_subprograms),
			p_subprograms_seq.second,
			p_subprograms_seq.second 	// FIXME: rationalise this nonsense
												// using enable_shared_from_this
			//providing_info->subprogram_children_begin(), 
			//providing_info->subprogram_children_end(),
			//providing_info->subprogram_children_end()
			);
	
		// FIXME: below....
		for (; r_iter != r_end; r_iter++)
		{        
			// cerr << "Found a required subprogram!" << endl;
			// cerr << **r_iter;
			
			for (; p_iter != p_end; p_iter++) 
			{
				if ((*r_iter)->get_name() == (*p_iter)->get_name())
				{
					/* Only add a correspondence if we haven't already got 
					 * *either* a correspondence for the source event,
					 * *or* a correspondence for sink event. 
					 * We haven't computed wrappers yet, so we can't use
					 * that. */
					 
					// add a correspondence
					cerr << "Matched name " << *((*r_iter)->get_name())
						<< " in modules " << *((*r_iter)->get_parent()->get_name())
						<< " and " << *((*p_iter)->get_parent()->get_name())
						<< endl;
					// don't add if explicit rules have touched this event
					if (touched_events[requiring_iface].find(
							definite_member_name(vector<string>(1, *(*r_iter)->get_name())))
						!= touched_events[requiring_iface].end())
					{
						cerr << "Matched name " << *((*r_iter)->get_name())
							<< " already touched by an explicit correspondence, so skipping."
							<< endl;
						continue;
					}

					antlr::tree::Tree *tmp_source_pattern = 
						make_simple_event_pattern_for_call_site(
							*((*r_iter)->get_name()));
					antlr::tree::Tree *tmp_sink_pattern = 
						make_simple_sink_expression_for_event_name(
							string(*((*p_iter)->get_name())) /*+ string("(...)")*/);
					// we should have just generated an event pattern and a function invocation
					assert(GET_TYPE(tmp_source_pattern) == CAKE_TOKEN(EVENT_PATTERN));
					assert(GET_TYPE(tmp_sink_pattern) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
					
					ensure_all_artificial_data_types(tmp_source_pattern,
						requiring_iface);
					ensure_all_artificial_data_types(tmp_sink_pattern,
						providing_iface);

					add_event_corresp(requiring_iface, // source is the requirer 
						tmp_source_pattern,
						0, // no infix stub
						providing_iface, // sink is the provider
						tmp_sink_pattern, 
						0, // no infix stub
						0, // no return event
						0, // no corresp head
						true, true);
					
					// also add interpretations from this corresp
					
					
				}
			}
		}
	}
	
	void link_derivation::extract_type_synonymy(module_ptr module,
		map<vector<string>, shared_ptr<dwarf::spec::type_die> >& synonymy)
	{
		// synonymy map is from synonym to concrete
		
		for (auto i_die = module->get_ds().begin();
			i_die != module->get_ds().end();
			++i_die)
		{
			auto p_typedef = dynamic_pointer_cast<dwarf::spec::typedef_die>(*i_die);
			if (!p_typedef) continue;
			if (p_typedef == p_typedef->get_concrete_type()) continue;
			
			if (!p_typedef->get_concrete_type())
			{
				cerr << "FIXME: typedef "
					<< *p_typedef
					<< " has no concrete type -- we should add it to synonymy map,"
					<< " but skipping for now." 
					<< endl;
			}
			else
			{
				auto p_ds = &p_typedef->get_ds();
				assert(module_of_die(p_typedef) == module);
				auto concrete = p_typedef->get_concrete_type();
				assert(concrete);
				auto p_concrete_ps = &concrete->get_ds();
				assert(module_of_die(p_typedef->get_concrete_type()) == module);
				synonymy.insert(make_pair(
					*p_typedef->ident_path_from_cu(), 
					p_typedef->get_concrete_type()));
				//cerr << "synonymy within " << module->filename << ": "
				//	<< definite_member_name(*p_typedef->ident_path_from_cu())
				//	<< " ----> " << *p_typedef->get_concrete_type() << endl;
			}
		}
	}
	
	optional<link_derivation::val_corresp_map_t::iterator>
	link_derivation::find_value_correspondence(
		module_ptr source, shared_ptr<dwarf::spec::type_die> source_type,
		module_ptr sink, shared_ptr<dwarf::spec::type_die> sink_type)
	{
		auto iter_pair = val_corresps.equal_range(sorted(make_pair(source, sink)));
		for (auto i = iter_pair.first; i != iter_pair.second; i++)
		{
			if (i->second->source == source && i->second->sink == sink &&
				i->second->source_data_type == source_type &&
				i->second->sink_data_type == sink_type)
			{
				return i;
			}
		}
		return false;
	}

	void link_derivation::name_match_types(
		iface_pair ifaces)
	{
		/* This name-matching is complicated by typedefs and other synonymy features. 
		 * 
		 * We can group named types by their concrete type. 
		 * Opaque types, that have no concrete types, degenerate into
		 * equivalence classes of *names*. 
		 *
		 * Complication: type names are also used for value correspondence selection,
		 * where a particular synonym (typedef) may be given distinguished treatment.
		 * So when we do this name-matching, we should be careful to avoid establishing
		 * correspondences between concrete types for which a matching synonym pair
		 * has been found but where there is an incompatible distinguished treatment 
		 * for some synonym of the same concrete type. FIXME: I don't know what counts
		 * as incompatible yet, so just avoid *any* name-matching if a member of the
		 * synonym set is used in an "as" annotation.
		 * 
		 * A hopefully-final complication: opaque types should be distinguished.
		 * By mapping everything to a concrete type, the null concrete type will
		 * collect all synonyms of all opaque data types. This isn't what we want.
		 * Instead, we want each root (e.g. "struct") opaque type to be distinguished,
		 * i.e. to collect only the synonyms that map to it. FIXME: I need to change
		 * the synonymy map structure to do this. */
	
		struct found_type 
		{ 
			module_ptr module;
			shared_ptr<dwarf::spec::type_die> t;
		};
		multimap<vector<string>, found_type> found_types;
		std::set<vector<string> > keys;
		
		multimap<vector<string>, shared_ptr<type_die> > first_found_types;
		multimap<vector<string>, shared_ptr<type_die> > second_found_types;
		
		map<vector<string>, shared_ptr<dwarf::spec::type_die> > first_synonymy;
		extract_type_synonymy(ifaces.first, first_synonymy);
		map<vector<string>, shared_ptr<dwarf::spec::type_die> > second_synonymy;
		extract_type_synonymy(ifaces.second, second_synonymy);
		
		// traverse whole dieset depth-first, remembering DIEs that 
		// -- 1. are type DIEs, and
		// -- 2. have an ident path from root (i.e. have *names*)
		for (auto i_mod = ifaces.first;
					i_mod;
					i_mod = (i_mod == ifaces.first) ? ifaces.second : module_ptr())
		{
			for (auto i_die = i_mod->get_ds().begin();
				i_die != i_mod->get_ds().end();
				++i_die)
			{
				auto p_type = dynamic_pointer_cast<dwarf::spec::type_die>(*i_die);
				if (!p_type) continue;
				
				// skip declaration-only types
				if (p_type->get_declaration() && *p_type->get_declaration()) continue;

				auto opt_path = p_type->ident_path_from_cu();
				if (!opt_path) continue;

				// deduplication...
				auto already_found = found_types.equal_range(*opt_path);
				// have we found any types in this module already?
				auto i_found =  already_found.first;
				bool have_already_found = false;
				for (; i_found != already_found.second; ++i_found)
				{
					if (i_found->second.module == i_mod)
					{
						// check for rep-compatibility
						if (compiler.cxx_is_complete_type(i_found->second.t)
						&&  compiler.cxx_is_complete_type(p_type)
						&&  !i_found->second.t->get_concrete_type()->is_rep_compatible(p_type->get_concrete_type()))
						{
							cerr << "First type: " << *p_type->get_concrete_type() << endl;
							cerr << "Second type: " << *i_found->second.t->get_concrete_type() << endl;
							cerr << "Both aliased as: " << definite_member_name(*opt_path) << endl;
							RAISE(this->ast, 
								"contains rep-incompatible toplevel-visible types with the same fq names");
						}
						else have_already_found = true;
					}
				}
				if (i_found == already_found.second && !have_already_found) 
				{
					found_types.insert(make_pair(*opt_path, (found_type){ i_mod, p_type }));
					assert(module_of_die(*i_die) == i_mod);
					keys.insert(*opt_path);
					((i_mod == ifaces.first) ? first_found_types : second_found_types).insert(make_pair(
						*opt_path, p_type));
				}
			}
		}
		
		set< shared_ptr<dwarf::spec::type_die> > seen_concrete_types;
		set< shared_ptr<dwarf::spec::type_die> > seen_types;

		typedef set<
			pair< // this is any pair involving a base type -- needn't both be base
				dwarf::tool::cxx_compiler::base_type,
				shared_ptr<dwarf::spec::type_die> 
			>
		> base_type_pairset_t;
		typedef set<
			pair<
				dwarf::tool::cxx_compiler::base_type,
				dwarf::tool::cxx_compiler::base_type
			>
		> base_base_pairset_t;

		base_type_pairset_t seen_first_base_type_pairs;
		base_type_pairset_t seen_second_base_type_pairs;
		base_base_pairset_t seen_base_base_pairs;
		// Now look for names that have exactly two entries in the multimap,
		// (i.e. exactly two <vector, found_type> pairs for a given vector,
		//  i.e. exactly two types were found having a given name-vector)
		// and where the module of each is different.
		
		for (auto i_k = keys.begin(); i_k != keys.end(); ++i_k)
		{
			auto iter_pair = found_types.equal_range(*i_k);
			
			auto count_matched = srk31::count(iter_pair.first, iter_pair.second);
			bool exactly_two_matched = (count_matched == 2);
			
			bool different_modules = (iter_pair.second--,
				iter_pair.first->second.module != iter_pair.second->second.module);
			
			// must be concretely non-void, non-array, non-pointer, non-subroutine types!
			bool correspondable_types = (iter_pair.first->second.t->get_concrete_type() && 
				iter_pair.second->second.t->get_concrete_type() &&
				iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_array_type &&
				iter_pair.second->second.t->get_concrete_type()->get_tag() != DW_TAG_array_type &&
				iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_pointer_type &&
				iter_pair.second->second.t->get_concrete_type()->get_tag() != DW_TAG_pointer_type &&
				iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_subroutine_type &&
				iter_pair.second->second.t->get_concrete_type()->get_tag() != DW_TAG_subroutine_type);
			
			// must not have been matched before!
			// HMM -- why not?
			// Ditch the "concrete" requirement -- only avoid them if they 
			// have not been matched before in a nominally identical way.
			auto found_first_matched_before =  std::find_if(
				seen_types.begin(), seen_types.end(),
				[iter_pair](shared_ptr<type_die> item) {
					return data_types_are_identical(
						item,
						iter_pair.first->second.t
					);
				}
			);
			auto found_second_matched_before =  std::find_if(
				seen_types.begin(), seen_types.end(),
				[iter_pair](shared_ptr<type_die> item) {
					return data_types_are_identical(
						item,
						iter_pair.second->second.t
					);
				}
			);
				
			bool not_dwarf_types_matched_before //= true;
// 			 = (seen_concrete_types.find(iter_pair.first->second.t->get_concrete_type())
// 					== seen_concrete_types.end()
// 			&& seen_concrete_types.find(iter_pair.second->second.t->get_concrete_type()) 
// 					== seen_concrete_types.end());
			 = (found_first_matched_before == seen_types.end()
			&&  found_second_matched_before == seen_types.end());
			
			// if base types, pair must not be equivalent to any seen before! 
			// (else template specializations will collide)
			// NOTE: this is because DWARF info often includes "char" and "signed char"
			// and these are distinct in DWARF-land but not in C++-land
			
			bool not_void = iter_pair.first->second.t->get_concrete_type()
				&& iter_pair.second->second.t->get_concrete_type();
			if (!not_void)
			{
				cerr << "Warning: skipping name-matching types with one or both void: ";
				if (!iter_pair.first->second.t->get_concrete_type()) 
				{
					cerr << iter_pair.first->second.t->summary() << endl;
				}
				if (!iter_pair.second->second.t->get_concrete_type()) 
				{
					cerr << iter_pair.second->second.t->summary() << endl;
				}
				continue;
			}
			
			auto base_type_not_in_set = [](const base_type_pairset_t& s, shared_ptr<type_die> p_t) {
				using dwarf::tool::cxx_compiler;
				using dwarf::spec::base_type_die;
				auto concrete_t = p_t->get_concrete_type();
				auto base_t = dynamic_pointer_cast<base_type_die>(concrete_t);
				return !concrete_t // typedef of void
					|| concrete_t->get_tag() != DW_TAG_base_type
					|| std::find_if(
							s.begin(), 
							s.end(),
							[base_t](const pair<cxx_compiler::base_type, shared_ptr<type_die> >& p) {
								assert(base_t);
								return 
									data_types_are_identical(
										base_t,
										p.second
									)
								&& cxx_compiler::base_type(base_t) == p.first;
							}) == s.end();
			};
			
			
			bool not_base_types_matched_before
			= (
				base_type_not_in_set(seen_first_base_type_pairs, iter_pair.first->second.t)
// 					iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_base_type
// 					|| seen_first_base_type_pairs.find(make_pair(
// 						dwarf::tool::cxx_compiler::base_type(
// 							dynamic_pointer_cast<dwarf::spec::base_type_die>(
// 								iter_pair.first->second.t->get_concrete_type())),
// 							iter_pair.second->second.t->get_concrete_type())) ==
// 							seen_first_base_type_pairs.end()
				)
			&&
				(
					base_type_not_in_set(seen_second_base_type_pairs, iter_pair.second->second.t)
//					->get_concrete_type()->get_tag() != DW_TAG_base_type
//					|| seen_second_base_type_pairs.find(make_pair(
//						dwarf::tool::cxx_compiler::base_type(
//							dynamic_pointer_cast<dwarf::spec::base_type_die>(
//								iter_pair.second->second.t->get_concrete_type())),
//						iter_pair.first->second.t->get_concrete_type()))
//						== seen_second_base_type_pairs.end()
				)
			&& (!(iter_pair.first->second.t->/*get_concrete_type()->*/get_tag() == DW_TAG_base_type &&
				  iter_pair.second->second.t->/*get_concrete_type()->*/get_tag() == DW_TAG_base_type)
				 || seen_base_base_pairs.find(
				 	make_pair(
						dwarf::tool::cxx_compiler::base_type(
							dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.first->second.t/*->get_concrete_type()*/)),
						dwarf::tool::cxx_compiler::base_type(
							dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.second->second.t/*->get_concrete_type()*/))
					))
					== seen_base_base_pairs.end()
				);
			// don't match built-in types, because they don't appear in our dwarfhpp headers
			bool neither_is_builtin
			 = (!compiler.is_builtin(iter_pair.first->second.t->get_concrete_type())
				&& !compiler.is_builtin(iter_pair.second->second.t->get_concrete_type()));

			ostringstream namestream;
			namestream << definite_member_name(*i_k);
			string friendly_name = namestream.str(); 
				
			if (exactly_two_matched
			 && different_modules)
			{
				cerr << "data type " << friendly_name
					<< " exists in both modules ";
				
				if (correspondable_types)
				{
					cerr << " and is correspondable";
					if (not_dwarf_types_matched_before)
					{
						cerr << " and has not been matched already as a DWARF type";
					
						if (not_base_types_matched_before)
						{
							cerr << " and has not been matched already as a base type";
							if (neither_is_builtin)
							{
								cerr << " and is not builtin, so adding a correspondence." << endl;
								// 1. update our bookkeeping structures
								seen_concrete_types.insert(iter_pair.first->second.t->get_concrete_type());
								seen_types.insert(iter_pair.first->second.t);
								seen_concrete_types.insert(iter_pair.second->second.t->get_concrete_type());
								seen_types.insert(iter_pair.second->second.t);
								if (iter_pair.first->second.t->get_tag() == DW_TAG_base_type)
									seen_first_base_type_pairs.insert(
										make_pair(
											dwarf::tool::cxx_compiler::base_type(
												dynamic_pointer_cast<dwarf::spec::base_type_die>(
													iter_pair.first->second.t->get_concrete_type())),
												iter_pair.second->second.t->get_concrete_type())
											);
								if (iter_pair.second->second.t->get_tag() == DW_TAG_base_type)
									seen_second_base_type_pairs.insert(
										make_pair(
											dwarf::tool::cxx_compiler::base_type(
												dynamic_pointer_cast<dwarf::spec::base_type_die>(
													iter_pair.second->second.t->get_concrete_type())),
											iter_pair.first->second.t->get_concrete_type()));
								if (iter_pair.first->second.t/*->get_concrete_type()*/->get_tag() == DW_TAG_base_type &&
								  iter_pair.second->second.t/*->get_concrete_type()*/->get_tag() == DW_TAG_base_type)
								{
									cerr << "remembering a base-base pair " 
										<< definite_member_name(*i_k) << "; size was " 
										<< seen_base_base_pairs.size();
				 					seen_base_base_pairs.insert(make_pair(
										dwarf::tool::cxx_compiler::base_type(
											dynamic_pointer_cast<dwarf::spec::base_type_die>(
												iter_pair.first->second.t/*->get_concrete_type()*/)),
										dwarf::tool::cxx_compiler::base_type(
											dynamic_pointer_cast<dwarf::spec::base_type_die>(
												iter_pair.second->second.t/*->get_concrete_type()*/))
									));
									cerr << "; now " << seen_base_base_pairs.size() << endl;
								}

								// iter_pair points to a pair of like-named types in differing modules
								// add value correspondences in *both* directions
								// *** FIXME: ONLY IF not already present already...
								// i.e. the user might have supplied their own

								// 2. two-iteration for loop
								for (pair<module_ptr, module_ptr> source_sink_pair = 
										make_pair(ifaces.first, ifaces.second), orig_source_sink_pair = source_sink_pair;
										source_sink_pair != pair<module_ptr, module_ptr>();
										source_sink_pair =
											(source_sink_pair == orig_source_sink_pair) 
												? make_pair(ifaces.second, ifaces.first) : make_pair(module_ptr(), module_ptr()))
								{
// 									// each of these maps a set of synonyms mapping to their concrete type
// 									map<vector<string>, shared_ptr<dwarf::spec::type_die> > &
// 										source_synonymy = (source_sink_pair.first == ifaces.first) ? first_synonymy : second_synonymy;
// 									map<vector<string>, shared_ptr<dwarf::spec::type_die> > &
// 										sink_synonymy = (source_sink_pair.second == ifaces.first) ? first_synonymy : second_synonymy;
// 
// 
// 									bool source_is_synonym = false;
// 									shared_ptr<dwarf::spec::basic_die> source_found;
// 									bool sink_is_synonym = false;
// 									shared_ptr<dwarf::spec::basic_die> sink_found;
// 									/* If the s... data type is a synonym, we will set s..._type
// 									 * to the *concrete* type and set the flag. 
// 									 * Otherwise we will try to get the data type DIE by name lookup. */
// 									auto source_type = (source_synonymy.find(*i_k) != source_synonymy.end()) ?
// 											(source_is_synonym = true, source_synonymy[*i_k]) : /*, // *i_k, // */ 
// 											dynamic_pointer_cast<dwarf::spec::type_die>(
// 												source_found = source_sink_pair.first->get_ds().toplevel()->visible_resolve(
// 													i_k->begin(), i_k->end()));
// 									auto sink_type = (sink_synonymy.find(*i_k) != sink_synonymy.end()) ?
// 											(sink_is_synonym = true, sink_synonymy[*i_k]) : /*, // *i_k, // */ 
// 											dynamic_pointer_cast<dwarf::spec::type_die>(
// 												sink_found = source_sink_pair.second->get_ds().toplevel()->visible_resolve(
// 													i_k->begin(), i_k->end()));
// 									const char *matched_name = i_k->at(0).c_str();
// 
// 				// 					cerr << "Two-cycle for loop: source module @" << &*source_sink_pair.first << endl;
// 				// 					cerr << "Two-cycle for loop: sink module @" << &*source_sink_pair.second << endl;
// 				// 					cerr << "Two-cycle for loop: source synonymy @" << &source_synonymy << endl;
// 				// 					cerr << "Two-cycle for loop: sink synonymy @" << &sink_synonymy << endl;
// 
// 									// this happens if name lookup fails 
// 									// (e.g. not visible (?))
// 									// or doesn't yield a type
// 									if (!source_type || !sink_type)
// 									{
// 										shared_ptr<dwarf::spec::basic_die> source_synonym;
// 										shared_ptr<dwarf::spec::basic_die> sink_synonym;
// 										if (source_is_synonym) source_synonym = source_synonymy[*i_k];
// 										if (sink_is_synonym) sink_synonym = sink_synonymy[*i_k];
// 
// 										cerr << "Skipping correspondence for matched data type named " 
// 										<< definite_member_name(*i_k)
// 										<< " because (FIXME) the type is probably incomplete"
// 										<< " where source " << *i_k 
// 										<< (source_is_synonym ? " was found to be a synonym " : " was not resolved to a type ")
// 										<< ((!source_is_synonym && source_found) ? " but was resolved to a non-type" : "")
// 										<< " and sink " << *i_k 
// 										<< (sink_is_synonym ? " was found to be a synonym " : " was not resolved to a type ")
// 										<< ((!sink_is_synonym && sink_found) ? " but was resolved to a non-type" : "")
// 										<< endl;
// 										if (source_synonym) cerr << "source synonym: " << source_synonym
// 											<< endl;
// 										if (sink_synonym) cerr << "sink synonym: " << sink_synonym
// 											<< endl;
// 										assert(definite_member_name(*i_k).at(0) != "int");
// 										continue;
// 									}	

									auto source_type = (source_sink_pair.first == 
										iter_pair.first->second.module)
									?  iter_pair.first->second.t
									:  iter_pair.second->second.t;
									auto sink_type = (source_sink_pair.second == 
										iter_pair.first->second.module)
									?  iter_pair.first->second.t
									:  iter_pair.second->second.t;

									// if no previous value correspondence exists between these 
									// name-matched types.....
									if (!find_value_correspondence(source_sink_pair.first, 
											source_type, 
											source_sink_pair.second, 
											sink_type))
									{
										cerr << "Adding name-matched value corresp from"
											//<< " source module @" << source_sink_pair.first.get()
											<< " source data type " 
											//<< *source_type 
											<< (source_type->get_name() ? *source_type->get_name() : "(anonymous)" )
											<< " at " << std::hex << source_type->get_offset() << std::dec
											<< " to" 
											//<< " sink module @" << source_sink_pair.second.get()
											<< " sink data type " 
											<< (sink_type->get_name() ? *sink_type->get_name() : "(anonymous)" )
											<< " at " << std::hex << sink_type->get_offset() << std::dec
											//<< *sink_type 
											<< endl;

										add_value_corresp(
											source_sink_pair.first,
											source_type,
											0,
											source_sink_pair.second,
											sink_type,
											0,
											0, // refinement
											true, // source_is_on_left -- irrelevant as we have no refinement
											make_simple_corresp_expression(*i_k) // corresp
										);
									} // end if not already exist
									else cerr << "Skipping correspondence for matched data type named " 
										<< definite_member_name(*i_k)
										<< " because type synonyms processed earlier already defined a correspondence"
										<< endl;
								} // end two-cycle for loop
							}
							else // if neither_is_builtin
							{
								cerr << " but one or other is builtin." << endl;
							}
						}
						else
						{
							cerr << " but has been matched before as a base type." << endl;
						}
					}
					else
					{
						cerr << " but has been matched before as a DWARF type." << endl;
					}
				}
				else
				{
					cerr << " but is not of correspondable DWARF types." << endl;
				}
			}
			else
			{
				cerr << "data type " << friendly_name
						<< " exists only in one (really: " << count_matched << ") module (of {" 
						<< ifaces.first->get_filename()
						<< ", " << ifaces.second->get_filename() 
						<< "})" << endl;
			}
		}
	}

	vector<shared_ptr<dwarf::spec::type_die> >
	link_derivation::
	corresponding_dwarf_types(shared_ptr<dwarf::spec::type_die> type,
		module_ptr corresp_module,
		bool flow_from_type_module_to_corresp_module)
	{
		vector<shared_ptr<dwarf::spec::type_die> > found;
		auto ifaces = sorted(make_pair(corresp_module,
			module_of_die(type)));
		auto iters = val_corresps.equal_range(ifaces);
		for (auto i_corresp = iters.first;
			i_corresp != iters.second;
			++i_corresp)
		{
			if (flow_from_type_module_to_corresp_module)
			{
				if (i_corresp->second->source_data_type == type) found.push_back(i_corresp->second->sink_data_type);
			}
			else // flow is from corresp module to type module
			{
				if (i_corresp->second->sink_data_type == type) found.push_back(i_corresp->second->source_data_type);
			}
		}
		return found;
	}	
	shared_ptr<dwarf::spec::type_die>
	link_derivation::unique_corresponding_dwarf_type(
		shared_ptr<dwarf::spec::type_die> type,
		module_ptr corresp_module,
		bool flow_from_type_module_to_corresp_module)
	{
		auto result = this->corresponding_dwarf_types(
			type,
			corresp_module,
			flow_from_type_module_to_corresp_module);
		if (result.size() == 1) return result.at(0);
		else return shared_ptr<dwarf::spec::type_die>();
	}

	void link_derivation::compute_wrappers() 
	{
		/* We need to synthesise a set of wrapper from our event correspondences. 
	 	* For now, we generate a wrapper for every call-site calling a corresponding
	 	* function. We may need to generate additional wrappers for address-taken
	 	* functions.
	 	*
	 	* Wrappers need to be nonempty if direct oblivious binding is no good.
	 	* Direct oblivious binding is good iff 
	 	*    the corresponding symbol exists in the sink component
	 	* for all possible calling components,
	 	*    the called symbol name matches (or the call is indirect => has no symbol name); and
	 	*    after argument renaming/reordering/rebinding,
	 	*        all arguments are rep-compatible and definitely shareable
	 	* (note this is also the rep-compatibility test for functions-as-objects).
	 	*/
	 	
		// For each call-site (required function) in one of our correspondences,
		// generate a wrapper. The same wrapper may have many relevant rules.
		
		for (ev_corresp_map_t::iterator i_ev = ev_corresps.begin();
				i_ev != ev_corresps.end(); ++i_ev)
		{
			string called_function_name = get_event_pattern_call_site_name(
				i_ev->second.source_pattern);
			
			cerr << "Function " << called_function_name << " may be wrapped!" << endl;
			wrappers[called_function_name].push_back(
					&(*i_ev) // push a pointer to the ev_corresp map entry
				);
		}
	}
	
	// explicit instantiations
	//class hash<module_ptr>;
	//class hash<shared_ptr<type_die> >;
	
	void link_derivation::complete_value_corresps()
	{
		/* We want to end up with a "complete" set of value corresps, in the sense that 
		 * given a correspondence in one direction, we can always do something (even if
		 * just a no-op) in the other direction. 
		 * 
		 * This means:
		 * 1. if we have only an init rule in a given direction, we add an update rule;
		 * 2. if, for a given four-tuple (val_corresp_group_key), we don't have a rule
		 *    in one or other direction, we create one (an update rule).
		 * 
		 * We will probably have to do something about artificials / tagstrings in due
		 * course, but just hit the concrete case to begin with.
		 */
		
		for (auto i_pair = all_iface_pairs.begin(); i_pair != all_iface_pairs.end(); ++i_pair)
		{
			/* First we go through the supergroups, and make sure that for 
			 * each source-to-sink rule,
			 * there is some sink-to-source rule (which will be in a different supergroup). */
			
			map<val_corresp_group_key, bool> to_add_dir;
			
			auto supergroup_multimap = val_corresp_supergroups[*i_pair];
			for (auto i_supergroup = val_corresp_supergroup_keys[*i_pair].begin();
				i_supergroup != val_corresp_supergroup_keys[*i_pair].end(); ++i_supergroup)
			{
				auto rule_seq = supergroup_multimap.equal_range(*i_supergroup);
				for (auto i_rule = rule_seq.first;
				          i_rule != rule_seq.second;
				          ++i_rule)
				{
					auto p_corresp = i_rule->second;
					auto half_key = i_rule->first;
					assert(half_key == *i_supergroup);
				
					// make the supergroup key for the *other* half of this rule
					auto other_half_key = make_pair(
						(p_corresp->source == half_key.first) ? p_corresp->sink : p_corresp->source,
						canonicalise_type(
							(p_corresp->source == half_key.first) ? p_corresp->sink_data_type : 
								p_corresp->source_data_type,
							(p_corresp->source == half_key.first) ? p_corresp->sink : p_corresp->source,
							compiler
						)
					);
					
					// which direction does this rule go in?
					bool rule_is_outer_to_inner = (p_corresp->source == half_key.first);
					
					// ... now check that a rule exists in the other direction
					auto other_supergroup_seq = val_corresp_supergroups[*i_pair].equal_range(
						other_half_key
					);
					
// 					std::find(
// 						supergroups_seq.first,  // i.e. all supergroups with this iface pair
// 						supergroups_seq.second,
// 						[other_half_key](const val_corresp_supergroups_tbl_t::value_type& entry) {
// 							return entry.second.first == other_half_key;
// 							// i.e. select the unique supergroup whose half-key is the other key
// 							// -- this supergroup is itself a multimap
// 						}
// 					);

					//if (found_other_supergroup != supergroups_seq.second)
						auto i_other_rule = other_supergroup_seq.first;
						for (;
							i_other_rule != other_supergroup_seq.second;
							++i_other_rule)
						{
							auto i_p_corresp = i_other_rule->second;
							// we succeed if this rule maps
							// to the half-key data type
							// and is in the opposite direction
							// and is not init-only
							if (!i_p_corresp->init_only 
							&&  (rule_is_outer_to_inner ? // if so, we want inner to outer
									(i_p_corresp->sink == half_key.first
								&& data_types_are_identical(
										canonicalise_type(i_p_corresp->sink_data_type, i_p_corresp->sink, compiler),
										canonicalise_type(half_key.second, half_key.first, compiler)))
							: 		(i_p_corresp->source == half_key.first
								&& data_types_are_identical(
										canonicalise_type(i_p_corresp->source_data_type, i_p_corresp->source, compiler),
										canonicalise_type(half_key.second, half_key.first, compiler)))
							))
							{
								// okay, we found one -- can break from this loop
								break;
							}
						}
						if (i_other_rule == other_supergroup_seq.second) goto failed;
						else continue; // get on with the next rule in the supergroup

				failed:
					cerr << "INCOMPLETENESS: we think that rule " << *p_corresp
						<< " has no update rule in the opposite direction" << endl;
					cerr << "Will add one." << endl;
					auto to_add_key = (val_corresp_group_key){
						p_corresp->sink,
						p_corresp->source,
						canonicalise_type(p_corresp->sink_data_type, p_corresp->sink, compiler),
						canonicalise_type(p_corresp->source_data_type, p_corresp->source, compiler)
					};
					if (to_add_dir.find(to_add_key) == to_add_dir.end())
					{
						to_add_dir.insert(make_pair(to_add_key, !p_corresp->source_is_on_left));
					}
				}
			}
			
			for (auto i_to_add = to_add_dir.begin(); i_to_add != to_add_dir.end(); ++i_to_add)
			{
				auto inserted = add_value_corresp(
					/* module_ptr source, */i_to_add->first.source_module,
					/* shared_ptr<dwarf::spec::type_die> source_data_type, */ i_to_add->first.source_data_type,
					/* antlr::tree::Tree *source_infix_stub, */ 0,
					/* module_ptr sink, */ i_to_add->first.sink_module,
					/* shared_ptr<dwarf::spec::type_die> sink_data_type, */ i_to_add->first.sink_data_type, 
					/* antlr::tree::Tree *sink_infix_stub, */ 0,
					/* antlr::tree::Tree *refinement, */ 0, 
					/* bool source_is_on_left, */ i_to_add->second /*source_is_on_left*/, // doesn't matter because we pass no ASTs
					/* antlr::tree::Tree *corresp, */ make_simple_corresp_expression( // left, right
						i_to_add->second /*source_is_on_left */
							? *i_to_add->first.source_data_type->ident_path_from_cu()
							: *i_to_add->first.sink_data_type->ident_path_from_cu(),
						i_to_add->second /*source_is_on_left */
							? *i_to_add->first.sink_data_type->ident_path_from_cu()
							: *i_to_add->first.source_data_type->ident_path_from_cu()	), 
					/* bool init_only */ false
						);
				cerr << "Added " << *inserted->second << endl;
				// we also have to add it to the other tables
				auto p_c = inserted->second;
				auto info = get_table_info_for_corresp(p_c);
				info.group.push_back(p_c.get());

				info.supergroup_tbl.insert(make_pair(info.source_k, p_c.get()));
				info.supergroup_tbl.insert(make_pair(info.sink_k, p_c.get()));

				val_corresp_supergroup_keys[info.ifaces].insert(info.source_k);
				val_corresp_supergroup_keys[info.ifaces].insert(info.sink_k);
				val_corresp_group_keys_by_supergroup[info.ifaces][info.source_k].insert(info.k);
				val_corresp_group_keys_by_supergroup[info.ifaces][info.sink_k].insert(info.k);
			}
		
			auto groups_seq = val_corresp_groups[*i_pair];
			set<val_corresp_group_key> to_add;
			for (auto i_group = groups_seq.begin(); i_group != groups_seq.end(); ++i_group)
			{
				/* Each group has a unique key,
				 * fixing its source and sink type.
				 * So each group is entirely first-to-second or entirely second-to-first.
				 * We make sure there is at least one update rule. */
				
				optional<bool> group_is_first_to_second;
				
				bool group_has_update = false;
				unsigned group_init_only_rule_count = 0;
				
				for (auto i_p_corresp = i_group->second.begin(); 
					i_p_corresp != i_group->second.end(); 
					++i_p_corresp)
				{
					bool this_rule_is_first_to_second
					 = ((*i_p_corresp)->source == i_pair->first);
					 
					if (!group_is_first_to_second.is_initialized()) 
					{
						group_is_first_to_second = optional<bool>(this_rule_is_first_to_second);
					} else assert(*group_is_first_to_second == this_rule_is_first_to_second);
					
					if (!(*i_p_corresp)->init_only) group_has_update = true;
					else ++group_init_only_rule_count;
				}
				
				if (!group_has_update && group_init_only_rule_count > 0)
				{
					cerr << "INCOMPLETENESS: we think that group <"
						<< "source module " << name_of_module(i_group->first.source_module)
						<< ", source concrete type " << i_group->first.source_data_type->summary()
						<< ", sink module " << name_of_module(i_group->first.sink_module)
						<< ", sink concrete type " << i_group->first.sink_data_type->summary()
						<< "> has no update rule (has " << group_init_only_rule_count 
						<< " init-only rules)." << endl;
					cerr << "Will add one." << endl;
					to_add.insert(i_group->first);
				}
			} // end for group
				
			for (auto i_group_key = to_add.begin(); i_group_key != to_add.end(); ++i_group_key)
			{
				auto inserted = add_value_corresp(
				/* module_ptr source, */ i_group_key->source_module,
				/* shared_ptr<dwarf::spec::type_die> source_data_type, */ i_group_key->source_data_type,
				/* antlr::tree::Tree *source_infix_stub, */ 0,
				/* module_ptr sink, */ i_group_key->sink_module,
				/* shared_ptr<dwarf::spec::type_die> sink_data_type, */ i_group_key->sink_data_type, 
				/* antlr::tree::Tree *sink_infix_stub, */ 0,
				/* antlr::tree::Tree *refinement, */ 0, 
				/* bool source_is_on_left, */ false, // doesn't matter because we pass no ASTs
				/* antlr::tree::Tree *corresp, */  make_simple_corresp_expression( // left, right
					*i_group_key->sink_data_type->ident_path_from_cu(),
					*i_group_key->source_data_type->ident_path_from_cu()), 
				/* bool init_only */ false
				);
				cerr << "Added " << *inserted->second << endl;
				auto p_c = inserted->second;
				auto info = get_table_info_for_corresp(p_c);
				info.group.push_back(p_c.get());

				info.supergroup_tbl.insert(make_pair(info.source_k, p_c.get()));
				info.supergroup_tbl.insert(make_pair(info.sink_k, p_c.get()));

				val_corresp_supergroup_keys[info.ifaces].insert(info.source_k);
				val_corresp_supergroup_keys[info.ifaces].insert(info.sink_k);
				val_corresp_group_keys_by_supergroup[info.ifaces][info.source_k].insert(info.k);
				val_corresp_group_keys_by_supergroup[info.ifaces][info.sink_k].insert(info.k);
			}
		} // end for iface pair
	}
		
	link_derivation::table_info_for_corresp
	link_derivation::get_table_info_for_corresp(shared_ptr<val_corresp> p_c)
	{
		auto ifaces = sorted(make_pair(p_c->source, p_c->sink));
		auto k = (val_corresp_group_key) {
				p_c->source, 
				p_c->sink, 
				canonicalise_type(p_c->source_data_type, p_c->source, compiler), 
				canonicalise_type(p_c->sink_data_type, p_c->sink, compiler)
			};
		return (table_info_for_corresp) {
			/* iface_pair ifaces; */ ifaces,
			/* val_corresp_group_key k; */ k,
			/* val_corresp_group_t& group; */ val_corresp_groups[ifaces][k],
			/* val_corresp_supergroup_key source_key; */ make_pair(p_c->source,
					canonicalise_type(p_c->source_data_type, p_c->source, compiler)),
			/* val_corresp_supergroup_key sink_key; */ make_pair(p_c->sink, 
					canonicalise_type(p_c->sink_data_type, p_c->sink, compiler)),
			/* val_corresp_supergroup_t& supergroup_tbl; */ val_corresp_supergroups[ifaces]
		};
	}

	void link_derivation::assign_value_corresp_numbers()
	{
// 		auto hash_function = [](const val_corresp_group_key& k) {
// //		struct h
// //		{ unsigned operator()(const key& k) const {
// 			//hash<module_ptr> h1;
// 			//hash<shared_ptr<type_die> > h2;
// 			
// 			// try to force instantiation of these functions
// 			//&hash<module_ptr>::operator() ;
// 			//&hash<shared_ptr<type_die> >::operator() ; 
// 			
// 			// return h1(k.source_module) ^ h1(k.sink_module) ^ h2(k.source_type) ^ h2(k.sink_type);
// 			
// 			// HACK: avoid probable g++ bug (link error for these instances of hash::operator())
// 			return reinterpret_cast<unsigned long>(k.source_module.get())
// 				^  reinterpret_cast<unsigned long>(k.sink_module.get())
// 				^  reinterpret_cast<unsigned long>(k.source_data_type.get())
// 				^  reinterpret_cast<unsigned long>(k.sink_data_type.get());
// //		} } hash_function;
// 		};
// 		
// 		unordered_map<val_corresp_group_key, int, __typeof(hash_function)> counts(100, hash_function);

		map<val_corresp_group_key, int> counts;
		
		for (auto i = val_corresps.begin(); i != val_corresps.end(); ++i)
		{
			auto p_c = i->second;
			auto info = get_table_info_for_corresp(p_c);
			
			// 1. put it in the group table
			cerr << "Group (previous size " << info.group.size() 
					<< "): source module " << name_of_module(info.k.source_module)
					<< ", source type " << info.k.source_data_type->summary()
					<< ", sink module " << name_of_module(info.k.sink_module)
					<< ", sink type " << info.k.sink_data_type->summary() 
					<< " gaining a corresp: " << *p_c << endl;
			
			// here we increase the size of info.vec by one
			info.group.push_back(p_c.get());
			
			// 1a. put it in the supergroup table, twice
			// val_corresp_groups is a table of tables
			// ifaces -> key -> corresps
			// val_corresp_supergroups is also a table of tables
			// ifaces -> half_key -> corresps
			// BUT each corresp can be found under multiple half-keys! Two, to be precise
			info.supergroup_tbl.insert(make_pair(info.source_k, p_c.get()));
			info.supergroup_tbl.insert(make_pair(info.sink_k, p_c.get()));
			
			val_corresp_supergroup_keys[info.ifaces].insert(info.source_k);
			val_corresp_supergroup_keys[info.ifaces].insert(info.sink_k);
			val_corresp_group_keys_by_supergroup[info.ifaces][info.source_k].insert(info.k);
			val_corresp_group_keys_by_supergroup[info.ifaces][info.sink_k].insert(info.k);
		}
		
		// complete the set of val corresps
		complete_value_corresps();
		
		// go round again, assigning numbers
		for (auto i = val_corresps.begin(); i != val_corresps.end(); ++i)
		{
			auto p_c = i->second;
			auto k = (val_corresp_group_key) {
				p_c->source, 
				p_c->sink, 
				canonicalise_type(p_c->source_data_type, p_c->source, compiler), 
				canonicalise_type(p_c->sink_data_type, p_c->sink, compiler)
			};
			auto ifaces = sorted(make_pair(p_c->source, p_c->sink));
			val_corresp_group_t& group_tbl = val_corresp_groups[ifaces];
			vector<val_corresp *>& vec = group_tbl[k];

			// the two counts are now redundant, but sanity-check for now
			// here we increase counts[k] by one
			int assigned = counts[k]++;
			
// 			cerr << "Assigned number " << assigned
// 				<< " to rule relating source data type "
// 				<< p_c->source_data_type->summary()
// 				<< " with sink data type "
// 				<< p_c->sink_data_type->summary()
// 				<< " where counts[...] is now " << counts[k] << endl;
			val_corresp_numbering.insert(make_pair(p_c, assigned));
		}
		
		// finally, sanity check
		for (auto i = val_corresps.begin(); i != val_corresps.end(); ++i)
		{
			auto p_c = i->second;
			auto k = (val_corresp_group_key) {
				p_c->source, 
				p_c->sink, 
				canonicalise_type(p_c->source_data_type, p_c->source, compiler), 
				canonicalise_type(p_c->sink_data_type, p_c->sink, compiler)
			};
			auto ifaces = sorted(make_pair(p_c->source, p_c->sink));
			val_corresp_group_t& group_tbl = val_corresp_groups[ifaces];
			vector<val_corresp *>& vec = group_tbl[k];
		
			assert((signed) vec.size() == counts[k]);
		}
	}

//	void link_derivation::output_rep_conversions() {}		
		
//	void link_derivation::output_symbol_renaming_rules() {}		
//	void link_derivation::output_formgens() {}
		
//	void link_derivation::output_wrappergens() {}		
//	void link_derivation::output_static_co_objects() {}	
}
