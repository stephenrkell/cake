#include <iostream>
#include <vector>
#include <map>
#include <boost/filesystem/path.hpp>

#include "request.hpp"
#include "parser.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"

namespace cake
{
	struct satisfies_dep
	{
		value_conversion::dep m_dep;
		satisfies_dep(const value_conversion::dep& dep) : m_dep(dep) {}
		
		bool operator ()(const std::pair< link_derivation::iface_pair, 
				boost::shared_ptr<value_conversion> > candidate) const
		{
// 			if (candidate.second->source_data_type->get_name() &&
// 				*candidate.second->source_data_type->get_name() == "int"
// 				&& candidate.second->sink_data_type->get_name() &&
// 				*candidate.second->sink_data_type->get_name() == "int"
// 				&& m_dep.first->get_name() == m_dep.second->get_name()
// 				&& m_dep.first->get_name() && *m_dep.first->get_name() == "int")
// 			{
// 				bool retval = candidate.second->source_data_type == m_dep.first
// 				 && candidate.second->sink_data_type == m_dep.second;
// 				std::cerr << "Would have returned " << std::boolalpha << retval << std::endl;
// 				std::cerr << "candidate source data type: " << *candidate.second->source_data_type
// 					<< " @" << &*candidate.second->source_data_type << std::endl;
// 				std::cerr << "candidate sink data type: " << *candidate.second->sink_data_type
// 					<< " @" << &*candidate.second->sink_data_type << std::endl;
// 				std::cerr << "m_dep.first: " << *m_dep.first
// 					<< " @" << &*m_dep.first << std::endl;
// 				std::cerr << "m_dep.second: " << *m_dep.second
// 					<< " @" << &*m_dep.second << std::endl;
// 				
// 				assert(false);
// 			}			
			return candidate.second->source_data_type == m_dep.first
			 && candidate.second->sink_data_type == m_dep.second;
		}
	};
		
	link_derivation::link_derivation(cake::request& r, antlr::tree::Tree *t,
		const std::string& id,
		const std::string& output_module_filename) 
	 : 	derivation(r, t), 
	 	compiler(std::vector<std::string>(1, std::string("g++"))),
	 	output_namespace("link_" + id + "_"), 
	 	wrap_file_makefile_name(
			boost::filesystem::path(id + "_wrap.cpp").string()),
	 	wrap_file_name((boost::filesystem::path(r.in_filename).branch_path() 
				/ wrap_file_makefile_name).string()),
	 	wrap_file(wrap_file_name.c_str()),
	 	p_wrap_code(new wrapper_file(*this, compiler, wrap_file)), wrap_code(*p_wrap_code)
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
		
		{
			INIT;
			std::cerr << "Link expression at " << t << " links modules: ";
			FOR_ALL_CHILDREN(identList)
			{
				std::cerr << CCP(GET_TEXT(n)) << " ";
				request::module_tbl_t::iterator found = r.module_tbl.find(
					std::string(CCP(GET_TEXT(n))));
				if (found == r.module_tbl.end()) RAISE(n, "module not defined!");
				input_modules.push_back(found->second);
			}
			std::cerr << std::endl;
		}

		// enumerate all interface pairs
		for (std::vector<module_ptr>::iterator i_mod = input_modules.begin();
				i_mod != input_modules.end();
				i_mod++)
		{
			std::vector<module_ptr>::iterator copy_of_i_mod = i_mod;
			for (std::vector<module_ptr>::iterator j_mod = ++copy_of_i_mod;
				j_mod != input_modules.end();
				j_mod++)
			{
				all_iface_pairs.insert(sorted(std::make_pair(*i_mod, *j_mod)));
			}
		}
		
		// add explicit correspondences from the Cake syntax tree
		{
			INIT;
			std::cerr << "Link expression at " << t << " has pairwise blocks as follows: ";
			FOR_ALL_CHILDREN(linkRefinement)
			{
				INIT;
				assert(GET_TYPE(linkRefinement) == CAKE_TOKEN(PAIRWISE_BLOCK_LIST));
				ALIAS3(n, arrow, BI_DOUBLE_ARROW);
				// now walk each pairwise block and add the correspondences
				{
					INIT;
					BIND3(arrow, leftChild, IDENT);
					BIND3(arrow, rightChild, IDENT);
					BIND3(arrow, pairwiseCorrespondenceBody, CORRESP);
					std::cerr << CCP(GET_TEXT(leftChild))
						<< " <--> "
						<< CCP(GET_TEXT(rightChild));
					request::module_tbl_t::iterator found_left = 
						r.module_tbl.find(std::string(CCP(GET_TEXT(leftChild))));
					if (found_left == r.module_tbl.end()) RAISE(n, "module not defined!");
   					request::module_tbl_t::iterator found_right = 
						r.module_tbl.find(std::string(CCP(GET_TEXT(rightChild))));
					if (found_right == r.module_tbl.end()) RAISE(n, "module not defined!");
					
					add_corresps_from_block(
						found_left->second, 
						found_right->second,
						pairwiseCorrespondenceBody);
				}
			}
			std::cerr << std::endl;
			
			// add implicit correpsondences *last*, s.t. explicit ones can take priority
			for (auto i_pair = all_iface_pairs.begin(); 
				i_pair != all_iface_pairs.end();
				i_pair++)
			{
				add_implicit_corresps(*i_pair);
			}
		} // end INIT block
		
//         // remember each interface pair and add implicit corresps
//         for (std::vector<module_ptr>::iterator i_mod = input_modules.begin();
//         		i_mod != input_modules.end();
//                 i_mod++)
//         {
//         	std::vector<module_ptr>::iterator copy_of_i_mod = i_mod;
//         	for (std::vector<module_ptr>::iterator j_mod = ++copy_of_i_mod;
//         		j_mod != input_modules.end();
//                 j_mod++)
// 			{
//             	all_iface_pairs.insert(sorted(std::make_pair(*i_mod, *j_mod)));
// 		        add_implicit_corresps(std::make_pair(*i_mod, *j_mod));
//             }
// 			// now add corresps generated by dependency
//         	for (auto i_pair = all_iface_pairs.begin();
// 					i_pair != all_iface_pairs.end();
// 					i_pair++)
// 			{
// 				//i_pair
// 			}			
//         }
		// now add corresps generated by dependency
		for (auto i_pair = all_iface_pairs.begin();
				i_pair != all_iface_pairs.end();
				i_pair++)
		{
			// FIXME: this code is SLOW!
			for (auto i_val_corresp = this->val_corresps.equal_range(*i_pair).first;
					i_val_corresp != this->val_corresps.equal_range(*i_pair).second;
					i_val_corresp++)
			{
				// get the dependencies of this rule
				auto deps_vec = i_val_corresp->second->get_dependencies();
				
				// check that they are satisfied
				for (auto i_dep = deps_vec.begin(); i_dep != deps_vec.end(); i_dep++)
				{
					auto found = std::find_if(
						this->val_corresps.equal_range(*i_pair).first,
						this->val_corresps.equal_range(*i_pair).second,
						satisfies_dep(*i_dep));
					// if we found a satisfying corresp, carry on...
					if (found != this->val_corresps.equal_range(*i_pair).second) continue;
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
							std::cerr << "Falling back on C++-assignability to convert from "
								<< wrap_code.get_type_name(i_dep->first) 
								<< " to " << wrap_code.get_type_name(i_dep->second) << std::endl;
							continue;
						}

						std::cerr << "Found unsatisfied dependency: from "
							<< compiler.fq_name_for(i_dep->first)
							<< " to " << compiler.fq_name_for(i_dep->second)
							<< " required by rule "
							<< &*i_val_corresp // FIXME: operator<<
							<< std::endl;
						auto source_data_type = i_dep->first;
						auto sink_data_type = i_dep->second;
						//assert(false);
						auto source_name_parts = compiler.fq_name_parts_for(source_data_type);
						auto sink_name_parts = compiler.fq_name_parts_for(sink_data_type);
						add_value_corresp(wrap_code.module_of_die(source_data_type), 
							source_data_type,
							0,
							wrap_code.module_of_die(sink_data_type),
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
		
		// propagate guessed argument info
		merge_guessed_argument_info_at_callsites();
		
		// which value corresps should we use as initialization rules?
		compute_init_rules();
		
		// generate wrappers
		compute_wrappers();
	}
	
	link_derivation::~link_derivation() 
	{ wrap_file.flush(); wrap_file.close(); delete p_wrap_code; }

	void link_derivation::extract_definition()
	{
	
	}
	
	void link_derivation::compute_init_rules()
	{
		for (auto i_pair = all_iface_pairs.begin();
			i_pair != all_iface_pairs.end();
			i_pair++)
		{
			auto candidate_groups = candidate_init_rules[*i_pair];
			std::cerr << "For interface pair (" << name_of_module(i_pair->first)
				<< ", " << name_of_module(i_pair->second) << ") there are "
				<< candidate_groups.size() << " value corresps that are candidate init rules"
				<< std::endl;
			
			for (auto i_key = candidate_init_rules_tbl_keys[*i_pair].begin();
				i_key != candidate_init_rules_tbl_keys[*i_pair].end();
				i_key++)
			{
				// for each key, we scan its candidates looking for either
				// -- a unique init rule, and any number of update rules
				// -- a unique update rule, and no init rule
				
				auto candidates = candidate_groups.equal_range(*i_key);
				unsigned candidates_count = srk31::count(candidates.first, candidates.second);
				std::cerr << "For data type " << *i_key->source_type <<
					" there are " << candidates_count << " candidate init rules." << std::endl;
				
				if (candidates_count == 1)
				{
					// we have a unique rule, so we're good
					init_rules_tbl[*i_pair][candidates.first->first]
					 = candidates.first->second;
				}
				else
				{
					boost::optional<
						std::pair<init_rules_key_t,
							init_rules_value_t>
						> found_init;
					
					for (auto i_candidate = candidates.first;
						i_candidate != candidates.second;
						i_candidate++)
					{
						
						// we want a unique init rule in here
						if (i_candidate->second->init_only)
						{
							if (found_init) RAISE(i_candidate->second->corresp,
								"multiple initialization rules for the same type "
								"not currently supported");
							else found_init = *i_candidate;
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
	
	void link_derivation::merge_guessed_argument_info_at_callsites()
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
		 * HMM... sounds like we want to refactor the stub emission code
		 * s.t. we can re-use the logic for expanding in_args...
		 * emission is somewhat separate from walking?
		 * Some aspects of emission are done in a pre-pass?
		 * Want the "type analysis" walking here too: 
		 * find places where an ident is interpreted "as"
		 * or used in a context where the originating or expected type is a typedef
		 * having distinct correspondences. */
		 
		for (auto i_mod = r.module_tbl.begin(); i_mod != r.module_tbl.end(); i_mod++)
		{
			for (auto i_dfs = i_mod->second->get_ds().begin();
				i_dfs != i_mod->second->get_ds().end();
				i_dfs++)
			{
				if ((*i_dfs)->get_tag() == DW_TAG_subprogram)
				{
					auto subprogram = boost::dynamic_pointer_cast<
						dwarf::spec::subprogram_die>(*i_dfs);
					if (subprogram->get_declaration()
						&& *subprogram->get_declaration())
					{
						if (subprogram->formal_parameter_children_begin()
							== subprogram->formal_parameter_children_end()
							&& subprogram->unspecified_parameters_children_begin()
							!= subprogram->unspecified_parameters_children_end())
						{
							// we're on! search for event patterns
							struct pattern_info
							{
								std::vector<boost::optional<std::string> > call_argnames;
								antlr::tree::Tree *pattern;
								ev_corresp *corresp;
							};
							std::multimap<std::string, pattern_info> patterns;
							std::vector<std::string> callnames;
							// patterns could be in any link block featuring this module...
							// ... so we just want to check against the source module
							for (auto i_corresp = ev_corresps.begin();
								i_corresp != ev_corresps.end();
								i_corresp++)
							{
								if (i_corresp->second.source == i_mod->second
									&& i_corresp->second.source_pattern)
								{
									auto p = i_corresp->second.source_pattern;
									assert(GET_TYPE(p) == CAKE_TOKEN(EVENT_PATTERN));
									switch (GET_TYPE(p))
									{
										case CAKE_TOKEN(EVENT_WITH_CONTEXT_SEQUENCE): {
// 											INIT;
// 											BIND3(p, sequenceHead, CONTEXT_SEQUENCE);
// 											{
// 												// descend looking for atomic patterns
// 												FOR_ALL_CHILDREN(sequenceHead)
// 												{
// 													switch(GET_TYPE(n))
// 													{
// 														case CAKE_TOKEN(KEYWORD_LET):
// 															
// 
// 														case CAKE_TOKEN(ELLIPSIS):
// 															continue;
// 														default: RAISE_INTERNAL(n, 
// 															"not a context predicate");
// 												}
// 											}
											assert(false); // see to this later
										}
										case CAKE_TOKEN(EVENT_PATTERN):
										{
											INIT;
											BIND3(p, eventContext, EVENT_CONTEXT);
											BIND3(p, memberNameExpr, DEFINITE_MEMBER_NAME); // name of call being matched -- can ignore this here
											BIND3(p, eventCountPredicate, EVENT_COUNT_PREDICATE);
											BIND3(p, eventParameterNamesAnnotation, KEYWORD_NAMES);
											std::vector<boost::optional<std::string> > argnames;
											auto dmn = read_definite_member_name(memberNameExpr);
											if (subprogram->get_name() && dmn.size() == 1 
												&& dmn.at(0) == *subprogram->get_name())
											{
												FOR_REMAINING_CHILDREN(p)
												{
													INIT;
													ALIAS3(n, annotatedValueBindingPatternHead, 
														ANNOTATED_VALUE_PATTERN);
													{
														INIT;
														BIND2(annotatedValueBindingPatternHead, arg);
														assert(arg);
														switch(GET_TYPE(arg))
														{
															case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
															{
																std::ostringstream s;
																s << read_definite_member_name(arg);
																argnames.push_back(s.str());
																break;
															}
															case CAKE_TOKEN(KEYWORD_CONST):
																// no information about arg name here

																argnames.push_back(
																	boost::optional<std::string>());
																break;
															default: assert(false);

														}
													}
												}
												// add this pattern
												patterns.insert(std::make_pair(
													dmn,
													(pattern_info) {
													argnames,
													p,
													&i_corresp->second
													}
												));
												callnames.push_back(dmn);
											} // end if name matches
										} // end case EVENT_PATTERN
										break;
										default: RAISE(p, "didn't understand event pattern");
									} // end switch(GET_TYPE(p))
								} // end if we found a source pattern in this module
							} // end for all corresps
							
							// now we've gathered all the patterns we can, 
							// iterate through them by call names
							for (auto i_callname = callnames.begin(); i_callname != callnames.end();
								i_callname++)
							{
								auto patterns_seq = patterns.equal_range(*i_callname);
								boost::optional<std::vector<boost::optional< std::string> > >
									seen_argnames;
								bool argnames_identical = true;
								for (auto i_pattern = patterns_seq.first;
									i_pattern != patterns_seq.second;
									i_pattern++)
								{
									if (!seen_argnames) seen_argnames = i_pattern->second.call_argnames;
									else if (i_pattern->second.call_argnames != *seen_argnames)
									{
										// HMM: seen non-equivalent argnames for the same arg
										argnames_identical = false;
										std::cerr << "Warning: different patterns use non-identical argnames "
											<< "for subprogram " 
											<< *subprogram->get_name() << std::endl;
										seen_argnames = boost::optional<std::vector<boost::optional<std::string> > >();
									}
								}
								if (argnames_identical)
								{
									/* Okay, go ahead and add name attrs */
									assert(seen_argnames);
									auto encap_subprogram =
										boost::dynamic_pointer_cast<dwarf::encap::subprogram_die>(
											subprogram);
									for (auto i_name = seen_argnames->begin();
										i_name != seen_argnames->end();
										i_name++)
									{
										auto created =
											dwarf::encap::factory::get_factory(
												dwarf::spec::DEFAULT_DWARF_SPEC
											).create<DW_TAG_formal_parameter>(
												encap_subprogram,
												*i_name ? 
												boost::optional<const std::string&>(**i_name)
												: boost::optional<const std::string&>());
										std::cerr << "created fp of subprogram "
											<< *subprogram->get_name()
											<< " with name " <<  (*i_name ? **i_name : "(no name)")
											<< " at offset " << created->get_offset()
											<< std::endl;
									}
								}

								for (auto i_pattern = patterns_seq.first;
									i_pattern != patterns_seq.second;
									i_pattern++)
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
									for (auto i_arg = i_pattern->second.call_argnames.begin();
										i_arg != i_pattern->second.call_argnames.end();
										i_arg++)
									{
										std::vector<antlr::tree::Tree *> contexts;
										if (!*i_arg) continue;
										find_usage_contexts(**i_arg,
											i_pattern->second.corresp->sink_expr,
											contexts);
										if (contexts.size() > 0)
										{
											std::cerr << "Considering type expectations "
												<< "for stub uses of identifier " 
												<< **i_arg << std::endl;
											for (auto i_ctxt = contexts.begin(); 
												i_ctxt != contexts.end();
												i_ctxt++)
											{
												auto node = *i_ctxt;
												antlr::tree::Tree *prev_node = 0;
												assert(GET_TYPE(node) == CAKE_TOKEN(IDENT));
												while (node && ((node = /*GET_PARENT(node)*/ NULL /* HACK: getParent is broken!  2011-4-22 */) != NULL))
												{
													std::cerr << "Considering subtree " << CCP(TO_STRING_TREE(node))
														<< std::endl;
													switch (GET_TYPE(node))
													{
														case CAKE_TOKEN(INVOKE_WITH_ARGS):
															// this is the interesting case
															std::cerr << "FIXME: found a stub function call using ident " 
																<< **i_arg << std::endl;

														case CAKE_TOKEN(IDENT):
														case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
														case CAKE_TOKEN(MULTIVALUE):
															continue;

														default: node = NULL; break; // signal exit
													}
												}
												// when we get here, we may have identified some
												// type expectations, or we may not.
												// FIXME: finish this code by
												// - checking all the type expectations are
												// the same
												// - invoking unique_correpsonding_type
												// - filling in the output of this in the fp die

											}
										} // end if contexts.size() > 0
									}
								} // end for i_pattern
							}
						}
					}
				}
			}
		}
	}
	
	void 
	link_derivation::find_usage_contexts(const std::string& ident,
		antlr::tree::Tree *t, std::vector<antlr::tree::Tree *>& out)
	{
		INIT;
		// first check ourselves
		if (GET_TYPE(t) == CAKE_TOKEN(IDENT))
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
	link_derivation::find_type_expectations_in_stub(module_ptr module,
		antlr::tree::Tree *stub, 
		boost::shared_ptr<dwarf::spec::type_die> current_type_expectation,
		std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> >& out)
	{
		/* We walk the stub AST structure, resolving names against the module.
		 * Roughly, where there are static type annotations in the module,
		 * e.g. type info for C function signatures,
		 * we infer C++ static type requirements for any idents used.
		 * This might include typedefs, i.e. we don't concretise. */
		std::map<int, boost::shared_ptr<dwarf::spec::type_die> > child_type_expectations;
		antlr::tree::Tree *parent_whose_children_to_walk = stub;
		switch(GET_TYPE(stub))
		{
			case CAKE_TOKEN(INVOKE_WITH_ARGS):
				{
					INIT;
					BIND3(stub, argsExpr, MULTIVALUE);
					parent_whose_children_to_walk = argsExpr;
					BIND2(stub, functionExpr);
					boost::optional<definite_member_name> function_dmn;
					switch(GET_TYPE(functionExpr))
					{
						case IDENT:
							function_dmn = definite_member_name(1, CCP(GET_TEXT(functionExpr)));
							break;
						case DEFINITE_MEMBER_NAME:
							{
								definite_member_name dmn(functionExpr);
								function_dmn = dmn;
							}
							break;
						default:
						{
							std::cerr << "Warning: saw a functionExpr that wasn't of simple form: "
								<< CCP(GET_TEXT(functionExpr)) << std::endl;
							break;
						}
					}
					if (function_dmn) 
					{
						/* Resolve the function name against the module. */
						auto found = module->get_ds().toplevel()->visible_resolve(
							function_dmn->begin(), function_dmn->end());
						if (found)
						{
							auto subprogram_die = boost::dynamic_pointer_cast<
								dwarf::spec::subprogram_die>(found);
							if (found)
							{
								auto i_arg = subprogram_die->formal_parameter_children_begin();
								INIT;
								FOR_ALL_CHILDREN(argsExpr)
								{
									if (i_arg == subprogram_die->formal_parameter_children_end())
									{
										RAISE(stub, "too many args for function");
									}
									if ((*i_arg)->get_type())
									{
										/* We found a type expectation */
										child_type_expectations[i] = *(*i_arg)->get_type();
									}
									
									++i_arg;
								}
							}
							else
							{
								RAISE(stub, "invokes nonexistent function");
								// FIXME: this will need to be tweaked to support function ptrs
							}
						}
					}
				}
				goto walk_children;
			case CAKE_TOKEN(IDENT):
				if (current_type_expectation) out.insert(std::make_pair(
					CCP(GET_TEXT(stub)), current_type_expectation));
				break; /* idents have no children */
			default:
				/* In all other cases, we have no expectation, so we leave
				 * the child_type_expectations at default values. */
			walk_children:
			{
				INIT;
				FOR_ALL_CHILDREN(parent_whose_children_to_walk)
				{
					/* recurse */
					find_type_expectations_in_stub(
						module,
						n, /* child */
						child_type_expectations[i],
						out
					);
				}
			}
				
		} // end switch
	}	
//     std::string link_derivation::namespace_name()
//     {
//     	std::ostringstream s;
//         s << "link_" << r.module_inverse_tbl[output_module] << '_';
//         return s.str();
// 	}

	void link_derivation::write_makerules(std::ostream& out)
	{
		// implicit rule for making hpp files
		out << "%.o.hpp: %.o" << std::endl
			<< '\t' << "dwarfhpp \"$<\" > \"$@\"" << std::endl;
		// dependencies for generated cpp file
		out << wrap_file_makefile_name << ".d: " << wrap_file_makefile_name << std::endl
			<< '\t' << "$(CXX) $(CXXFLAGS) -MM -MG -I. -c \"$<\" > \"$@\"" << std::endl;
		out << "-include " << wrap_file_makefile_name << ".d" << std::endl;

		out << output_module->get_filename() << ":: ";
		for (std::vector<module_ptr>::iterator i = input_modules.begin();
			i != input_modules.end(); i++)
		{
			out << (*i)->get_filename() << ' ';
		}
		
		// output the wrapper file header
		wrap_file << "// generated by Cake version " << CAKE_VERSION << std::endl;
		wrap_file << "#include <cake/prelude.hpp>" << std::endl;
		wrap_file << "extern \"C\" {\n#include <libcake/repman.h>\n}" << std::endl;

		// for each component, include its dwarfpp header in its own namespace
		for (std::vector<module_ptr>::iterator i = input_modules.begin();
				i != input_modules.end();
				i++)
		{
			wrap_file << "namespace cake { namespace " << namespace_name()
					<< " { namespace " << r.module_inverse_tbl[*i] << " {" << std::endl;
					
			wrap_file << "\t#include \"" << (*i)->get_filename() << ".hpp\"" << std::endl;
			// also define a marker class, and code to grab a rep_id at init time
			wrap_file << "\tclass marker {}; // used for per-component template specializations" 
	  				<< std::endl;
			wrap_file << "\tstatic int rep_id;" << std::endl;
			wrap_file << "\tstatic void get_rep_id(void) __attribute__((constructor)); static void get_rep_id(void) { rep_id = next_rep_id++; rep_component_names[rep_id] = \""
			<< r.module_inverse_tbl[*i] << "\"; }" << std::endl; // FIXME: C-escape this
			// also define the Cake component as a set of compilation units
			wrap_file << "extern \"C\" {" << std::endl;
			wrap_file << "\tconst char *__cake_component_" << r.module_inverse_tbl[*i]
				<< " = ";
				/* output a bunch of string literals of the form
				 * <compilation-unit-name>-<full-compil-directory-name>-<compiler-ident> */
				for (auto i_cu = (*i)->get_ds().toplevel()->compile_unit_children_begin();
						i_cu != (*i)->get_ds().toplevel()->compile_unit_children_end();
						i_cu++)
				{
					wrap_file << "\t\t\"^" // FIXME: escape these!
						<< ((*i_cu)->get_name() ? *(*i_cu)->get_name() : "(anonymous)" )
						<< '^' // FIXME: complain if this char is used elsewhere
						<< ((*i_cu)->get_comp_dir() ? *(*i_cu)->get_comp_dir(): "(unknown directory)")
						<< '^'
						<< ((*i_cu)->get_producer() ? *(*i_cu)->get_producer() : "(unknown producer)")
						<< "^\""
						<< std::endl;
				}
				wrap_file << ";" << std::endl;
			wrap_file << "} /* end extern \"C\" */" << std::endl;
			wrap_file << "} } }" << std::endl; 

		}
		
		// FIXME: collapse type synonymy
		// (AFTER doing name-matching, s.t. can match maximally)

		// for each pair of components, forward-declare the value conversions
		wrap_file << "namespace cake {" << std::endl;
		for (auto i_pair = all_iface_pairs.begin(); i_pair != all_iface_pairs.end();
			i_pair++)
		{
			// emit each as a value_convert template
			auto all_value_corresps = val_corresps.equal_range(*i_pair);
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				i_corresp++)
			{
				wrap_file << "// forward declaration: " << CCP(TO_STRING_TREE(i_corresp->second->corresp)) << std::endl;
				i_corresp->second->emit_forward_declaration();
			}
		}
		wrap_file << "} // end namespace cake" << std::endl;

		// output the base (unspecialized) templates for corresponding-type lookup
		wrap_file << "namespace cake {"
<< std::endl << "    template <"
<< std::endl << "        typename ComponentPair, "
<< std::endl << "        typename InFirst, "
<< std::endl << "        int RuleTag,"
<< std::endl << "        bool DirectionIsFromFirstToSecond"
<< std::endl << "    > struct corresponding_type_to_first"
<< std::endl << "    {}; /* we specialize this for various InSeconds */ "
<< std::endl << "    template <"
<< std::endl << "        typename ComponentPair, "
<< std::endl << "        typename InSecond, "
<< std::endl << "        int RuleTag,"
<< std::endl << "        bool DirectionIsFromSecondToFirst"
<< std::endl << "    > struct corresponding_type_to_second"
<< std::endl << "    {}; /* we specialize this for various InFirsts */ "
<< std::endl << "} // end namespace cake" << std::endl;
		// output the pointer specializations
		wrap_file << "namespace cake {"
<< std::endl << "    template <"
<< std::endl << "        typename ComponentPair, "
<< std::endl << "        typename InFirstIsAPtr, "
<< std::endl << "        int RuleTag,"
<< std::endl << "        bool DirectionIsFromFirstToSecond"
<< std::endl << "    > struct corresponding_type_to_first <ComponentPair, InFirstIsAPtr*, RuleTag, DirectionIsFromFirstToSecond>"
<< std::endl << "    { typedef void *in_second; }; /* we specialize this for various InSeconds */ "
<< std::endl << "    template <"
<< std::endl << "        typename ComponentPair, "
<< std::endl << "        typename InSecondIsAPtr, "
<< std::endl << "        int RuleTag,"
<< std::endl << "        bool DirectionIsFromSecondToFirst"
<< std::endl << "    > struct corresponding_type_to_second<ComponentPair, InSecondIsAPtr*, RuleTag, DirectionIsFromSecondToFirst> "
<< std::endl << "    { typedef void *in_first; }; /* we specialize this for various InFirsts */ "
<< std::endl << "} // end namespace cake" << std::endl;

		wrap_file << "// we have " << all_iface_pairs.size() << " iface pairs" << std::endl;
		
		// for each pair of components, output the value conversions
		for (auto i_pair = all_iface_pairs.begin(); i_pair != all_iface_pairs.end();
			i_pair++)
		{
			// first emit the component_pair specialisation which describes the rules
			// applying for this pair of components
			wrap_file << "namespace cake {" << std::endl;
			wrap_file << "\ttemplate<> struct component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker> {" << std::endl;

			// FIXME: emit mapping
			wrap_file 
<< std::endl << "        template <"
<< std::endl << "            typename To,"
<< std::endl << "            typename From /* = ::cake::unspecified_wordsize_type */, "
<< std::endl << "            int RuleTag = 0"
<< std::endl << "        >"
<< std::endl << "        static"
<< std::endl << "        To"
<< std::endl << "        value_convert_from_first_to_second(const From& arg)"
<< std::endl << "        {"
<< std::endl << "            return value_convert<From, "
<< std::endl << "                To,"
<< std::endl << "                RuleTag"
<< std::endl << "                >().operator()(arg);"
<< std::endl << "        }"
<< std::endl << "        template <"
<< std::endl << "            typename To,"
<< std::endl << "            typename From /* = ::cake::unspecified_wordsize_type */, "
<< std::endl << "            int RuleTag = 0"
<< std::endl << "        >"
<< std::endl << "        static "
<< std::endl << "        To"
<< std::endl << "        value_convert_from_second_to_first(const From& arg)"
<< std::endl << "        {"
<< std::endl << "            return value_convert<From, "
<< std::endl << "                To,"
<< std::endl << "                RuleTag"
<< std::endl << "                >().operator()(arg);"
<< std::endl << "        }	"
<< std::endl << "        static conv_table_t conv_table_first_to_second;"
<< std::endl << "        static conv_table_t conv_table_second_to_first;"
<< std::endl << "        static init_table_t init_table_first_to_second;"
<< std::endl << "        static init_table_t init_table_second_to_first;"
<< std::endl << "        static void init_conv_tables() __attribute__((constructor));"
<< std::endl << "    }; // end component_pair specialization"
<< std::endl << "\tconv_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::conv_table_first_to_second;"
<< std::endl << "\tconv_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::conv_table_second_to_first;"
<< std::endl << "\tinit_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::init_table_first_to_second;"
<< std::endl << "\tinit_table_t component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::init_table_second_to_first;"
<< std::endl;

			/* Now output the correspondence for unspecified_wordsize_type
			 * (FIXME: make these emissions just invoke macros in the prelude) */
			/* Given a pair of modules
			 * with a pair of bidirectionally corresponding types, 
			 * we need how many specializations? 
			 * given second, in first, 1-->2
			 * given second, in first, 1<--2
			 * given first, in second, 1-->2
			 * given first, in second, 1<--2 */
				wrap_file << "// unspecified_wordsize_type correspondences -- defined for each module" << std::endl <<
				"    template <>"
<< std::endl << "    struct corresponding_type_to_second<" 
<< std::endl << "        component_pair<" 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, "
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, " 
                      << " ::cake::unspecified_wordsize_type"
                      << ", 0, " // RuleTag
                      << "true" << ">" // DirectionIsFromSecondToFirst
<< std::endl << "    {"
<< std::endl << "         typedef ::cake::unspecified_wordsize_type in_first;"
<< std::endl << "    };"
<< std::endl;
				wrap_file << 
				"    template <>"
<< std::endl << "    struct corresponding_type_to_first<" 
<< std::endl << "        component_pair<" 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, "
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, " 
                      << " ::cake::unspecified_wordsize_type"
                      << ", 0, " // RuleTag
                      << "false" << ">" // DirectionIsFromFirstToSecond
<< std::endl << "    {"
<< std::endl << "         typedef ::cake::unspecified_wordsize_type in_second;"
<< std::endl << "    };"
<< std::endl;
				wrap_file <<
				"    template <>"
<< std::endl << "    struct corresponding_type_to_second<" 
<< std::endl << "        component_pair<" 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, " 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, "
                      << " ::cake::unspecified_wordsize_type"
                      << ", 0, " // RuleTag
                      << "false" << ">" // DirectionIsFromSecondToFirst
<< std::endl << "    {"
<< std::endl << "         typedef ::cake::unspecified_wordsize_type in_first;"
<< std::endl << "    };"
<< std::endl;
				wrap_file << 
				"    template <>"
<< std::endl << "    struct corresponding_type_to_first<" 
<< std::endl << "        component_pair<" 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, " 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, "
                      << " ::cake::unspecified_wordsize_type"
                      << ", 0, " // RuleTag
                      << "true" << ">" // DirectionIsFromFirstToSecond
<< std::endl << "    {"
<< std::endl << "         typedef ::cake::unspecified_wordsize_type in_second;"
<< std::endl << "    };"
<< std::endl;
			auto all_value_corresps = val_corresps.equal_range(*i_pair);
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				i_corresp++)
			{
				// first output the correpsonding_type specializations:
				// there is a sink-to-source and source-to-sink relationship
				wrap_file << "// " << wrap_code.get_type_name(
                             i_corresp->second->source == i_pair->first 
                                ? i_corresp->second->source_data_type
                                : i_corresp->second->sink_data_type)
					<< ( (i_corresp->second->source == i_pair->first) ? " --> " : " <-- " )
					<< wrap_code.get_type_name(
                             i_corresp->second->sink == i_pair->second
                                ? i_corresp->second->sink_data_type
                                : i_corresp->second->source_data_type)
					<< std::endl;
				wrap_file << 
				"    template <>"
<< std::endl << "    struct corresponding_type_to_second<" 
<< std::endl << "        component_pair<" 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, "
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, " 
                      << wrap_code.get_type_name(
                             i_corresp->second->source == i_pair->second 
                                ? i_corresp->second->source_data_type
                                : i_corresp->second->sink_data_type)
                      << ", 0, " // RuleTag
                      << std::boolalpha << (i_corresp->second->source == i_pair->second) << ">" // DirectionIsFromSecondToFirst
<< std::endl << "    {"
<< std::endl << "         typedef "
                       << wrap_code.get_type_name(
                              i_corresp->second->source == i_pair->first
                                 ? i_corresp->second->source_data_type
                                  : i_corresp->second->sink_data_type)
                       << " in_first;"
<< std::endl << "    };"
<< std::endl;
				wrap_file << 
				"    template <>"
<< std::endl << "    struct corresponding_type_to_first<" 
<< std::endl << "        component_pair<" 
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->first] << "::marker, "
<< std::endl << "            " << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second] << "::marker>, " 
                      << wrap_code.get_type_name(
                             i_corresp->second->source == i_pair->first 
                                ? i_corresp->second->source_data_type
                                : i_corresp->second->sink_data_type)
                      << ", 0, " // RuleTag
                      << std::boolalpha << (i_corresp->second->source == i_pair->first) << ">" // DirectionIsFromFirstToSecond
<< std::endl << "    {"
<< std::endl << "         typedef "
                       << wrap_code.get_type_name(
                              i_corresp->second->source == i_pair->second
                                 ? i_corresp->second->source_data_type
                                 : i_corresp->second->sink_data_type)
                       << " in_second;"
<< std::endl << "    };"
<< std::endl;
			} // end for all value corresps 
			
			/* Now emit the table */
wrap_file    << "\tvoid component_pair<" 
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
				<< "::marker, "
				<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
				<< "::marker>::init_conv_tables()"
<< std::endl << "\t{\n";
			int hack_ctr = 0;
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				i_corresp++)
			{
				// HACK: instantiate all the template function instnaces we need
				// -- will be unnecessary once gcc bug 49609 is fixed
				std::ostringstream hack_varname;
				hack_varname << "_hack_" << hack_ctr++;
				wrap_file << "\t\tstatic ";
				i_corresp->second->emit_cxx_function_ptr_type(hack_varname.str());
				wrap_file << " = &(";
				i_corresp->second->emit_function_name();
				wrap_file << ");" << std::endl;
				
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

					wrap_file << ".insert(std::make_pair((conv_table_key) {" << std::endl;
					// output the fq type name as an initializer list
					wrap_file << "\t\t\t{ ";
					auto source_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->source_data_type);
					for (auto i_source_name_piece = source_fq_name.begin();
						i_source_name_piece != source_fq_name.end();
						i_source_name_piece++)
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
						i_sink_name_piece++)
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
						<< std::endl << "\t\t\t (conv_table_value) {";
					// output the size of the object -- hey, we can use sizeof
					wrap_file << " sizeof ( ::cake::" << namespace_name() << "::"
						<< name_of_module(i_corresp->second->sink) << "::"
						<< compiler.local_name_for(i_corresp->second->sink_data_type, false) << "), ";
					// now output the address 
					wrap_file << "reinterpret_cast<void*(*)(void*,void*)>(&";
					i_corresp->second->emit_function_name();
					wrap_file << "\t\t )}));" << std::endl;

				} // end if not init-only
				else
				{
					wrap_file << "\t\t// init-only rule" << std::endl;
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

					wrap_file << ".insert(std::make_pair((init_table_key) {" << std::endl;
					// output the fq type name as an initializer list
					wrap_file << "\t\t\t{ ";
					auto source_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->source_data_type);
					for (auto i_source_name_piece = source_fq_name.begin();
						i_source_name_piece != source_fq_name.end();
						i_source_name_piece++)
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
						<< std::endl << "\t\t\t (init_table_value) {";
					// output the size of the object -- hey, we can use sizeof
					wrap_file << " sizeof ( ::cake::" << namespace_name() << "::"
						<< name_of_module(i_corresp->second->sink) << "::"
						<< compiler.local_name_for(i_corresp->second->sink_data_type, false) << "), ";
					wrap_file << std::endl << "\t\t\t{ ";
					auto sink_fq_name
					 = compiler.fq_name_parts_for(i_corresp->second->sink_data_type);
					for (auto i_sink_name_piece = sink_fq_name.begin();
						i_sink_name_piece != sink_fq_name.end();
						i_sink_name_piece++)
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
					wrap_file << "\t\t )}));" << std::endl;
				}
				else
				{
					wrap_file << "\t\t// not an init rule" << std::endl;
				}
			}

wrap_file  
<< std::endl << "\t} /* end conv table initializer */" << std::endl;
wrap_file << "extern \"C\" {" << std::endl;
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
				<< "::marker>:: init_table_first_to_second };\n" << std::endl;
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
				<< "::marker>:: init_table_second_to_first };\n" << std::endl;
wrap_file << "} /* end extern \"C\" */" << std::endl;

			// emit each as a value_convert template
			for (auto i_corresp = all_value_corresps.first;
				i_corresp != all_value_corresps.second;
				i_corresp++)
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
// 				auto p_from_type = boost::dynamic_pointer_cast<dwarf::spec::type_die>(opt_from_type);
// 				auto p_to_type = boost::dynamic_pointer_cast<dwarf::spec::type_die>(opt_to_type);
//				 if (!p_from_type) RAISE(i_corresp->second.corresp, 
//					 "named source of value correspondence is not a DWARF type");
//				 if (!p_to_type) RAISE(i_corresp->second.corresp, 
//					 "named target of value correspondence is not a DWARF type");

				wrap_file << "// " << CCP(TO_STRING_TREE(i_corresp->second->corresp)) << std::endl;
				i_corresp->second->emit();
				
// 				wrap_code.emit_value_conversion(
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
			wrap_file << "} // end namespace cake" << std::endl;
			
		}

		bool wrapped_some = false;
		/*std::ostringstream*/
		std::vector<std::string> linker_args;
		std::vector<std::string> symbols_to_protect;
		// output wrapped symbol names (and the wrappers, to a separate file)
		for (wrappers_map_t::iterator i_wrap = wrappers.begin(); i_wrap != wrappers.end();
				i_wrap++)
		{
			// i_wrap->first is the wrapped symbol name
			// i_wrap->second is a list of pointer to the event correspondences
			//   whose *source* pattern invokes that symbol
			
			// ... however, if we have no source specifying an argument pattern,
			// don't emit a wrapper (because we can't provide args to invoke the __real_ function),
			// just --defsym __wrap_ = __real_ (i.e. undo the wrapping)
			bool can_simply_rebind = true;
			boost::optional<std::string> symname_bound_to;
			for (ev_corresp_pair_ptr_list::iterator i_corresp_ptr = i_wrap->second.begin();
						i_corresp_ptr != i_wrap->second.end();
						i_corresp_ptr++)
			{
				// we can only simply rebind if our symbol is only invoked
				// with simple name-only patterns mapped to an argument-free
				// simple expression.
				// AND if all arguments are rep-compatible
				// AND return value too
				boost::optional<std::string> source_symname =
					source_pattern_is_simple_function_name((*i_corresp_ptr)->second.source_pattern);
				boost::optional<std::string> sink_symname =
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
						i_sink_arg++)
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
						boost::shared_ptr<dwarf::spec::formal_parameter_die> source_arg, sink_arg;
						source_arg = boost::dynamic_pointer_cast<dwarf::spec::formal_parameter_die>
							(*i_source_arg);
						assert(source_arg && source_arg->get_type());
						sink_arg = boost::dynamic_pointer_cast<dwarf::spec::formal_parameter_die>
							(*i_sink_arg);
						assert(sink_arg && sink_arg->get_type());
						if (!(*source_arg->get_type())->is_rep_compatible(*sink_arg->get_type()))
						{
							std::cerr << "Detected that required symbol " << i_wrap->first
								<< " is not a simple rebinding of a required symbol "
								<< " because arguments are not rep-compatible." << std::endl;
							can_simply_rebind = false;
							//can't continue
							break;
						}
						else { /* okay, continue */ }
						
						i_source_arg++;
					}
							
				}
				else 
				{
					std::cerr << "Detected that required symbol " << i_wrap->first
						<< " is not a simple rebinding of a required symbol "
						<< " because source pattern "
						<< CCP(TO_STRING_TREE((*i_corresp_ptr)->second.source_pattern))
						<< " is not a simple function name." << std::endl;
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
					std::cerr << "Detected that required symbol " << i_wrap->first
						<< " is not a simple rebinding of a required symbol "
						<< " because sink pattern "
						<< CCP(TO_STRING_TREE((*i_corresp_ptr)->second.sink_expr))
						<< " is not a simple function name." << std::endl;
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

				/* Generate the wrapper */
				wrap_code.emit_wrapper(i_wrap->first, i_wrap->second);
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
			wrap_file.flush();
		} // end for each wrapper
		
		// Now output the linker args.
		// If wrapped some, first add the wrapper as a dependency (and an argument)
		if (wrapped_some)
		{
			out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< std::endl*/;
			// output the first objcopy
			out << std::endl << '\t' << "objcopy ";
			for (auto i_sym = symbols_to_protect.begin();
				i_sym != symbols_to_protect.end();
				i_sym++)
			{
				out << "--redefine-sym " << *i_sym << "=__cake_protect_" << *i_sym << " ";
			}
			out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< std::endl*/;
			out << std::endl << '\t' << "ld -r -o " << output_module->get_filename() << ' ';
			for (auto i_linker_arg = linker_args.begin(); 
				i_linker_arg != linker_args.end();
				i_linker_arg++)
			{
				out << *i_linker_arg << ' ';
			}
			out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< std::endl*/;
			// add the other object files to the input file list
			for (std::vector<module_ptr>::iterator i = input_modules.begin();
				i != input_modules.end(); i++)
			{
				out << (*i)->get_filename() << ' ';
			}
			// output the second objcopy
			out << std::endl << '\t' << "objcopy ";
			for (auto i_sym = symbols_to_protect.begin();
				i_sym != symbols_to_protect.end();
				i_sym++)
			{
				out << "--redefine-sym __cake_protect_" << *i_sym << "=" << *i_sym << " ";
			}
			out << output_module->get_filename() << std::endl;
		}  // Else just output the args
		else 
		{
			out << std::endl << '\t' << "ld -r -o " << output_module->get_filename();
			for (auto i_linker_arg = linker_args.begin(); 
				i_linker_arg != linker_args.end();
				i_linker_arg++)
			{
				out << *i_linker_arg << ' ';
			}
			out << ' ';
			// add the other object files to the input file list
			for (std::vector<module_ptr>::iterator i = input_modules.begin();
				i != input_modules.end(); i++)
			{
				out << (*i)->get_filename() << ' ';
			}
			out << std::endl;
		}
		
		

// 		// if it's a link:
// 		compute_function_bindings(); // event correspondences: which calls should be bound (possibly indirectly) to which definitions?
// 		
// 		compute_form_value_correspondences(); 
// 			// use structural + lossless rules, plus provided correspondences
// 			// not just the renaming-only correspondences!
// 			// since other provided correspondences (e.g. use of "as") might bring compatibility "back"
// 			// in other words we need to traverse *all* correspondences, forming implicit ones by name-equiv
// 			// How do we enumerate all correspondences? need a starting set + transitive closure
// 			// starting set is set of types passed in bound functions?
// 			// *** no, just walk through the DWARF info -- it's finite. add explicit ones first?
// 			
// 			// in the future, XSLT-style notion of correspondences, a form correspondence can be
// 			// predicated on a context... is this sufficient expressivity for
// 			// 1:N, N:1 and N:M object identity conversions? we'll need to tweak the notion of
// 			// co-object relation also, clearly
// 
// 		compute_static_value_correspondences();
// 			// -- note that when we support stubs and sequences, they will imply some nontrivial
// 			// value correspondences between function values. For now it's fairly simple 
// 			// -- wrapped if not in same rep domain, else not wrapped
// 		
// 		compute_dwarf_type_compatibility(); 
// 			// of all the pairs of corresponding forms, which are binary-compatible?
// 		
// 		compute_rep_domains(); // graph colouring ahoy
// 		
// 		output_rep_conversions(); // for the non-compatible types that need to be passed
// 		
// 		compute_interposition_points(); // which functions need wrapping between which domains
// 		
// 		output_symbol_renaming_rules(); // how to implement the above for static references, using objcopy and ld options
// 		
// 		output_formgens(); // one per rep domain
// 		
// 		output_wrappergens(); // one per inter-rep call- or return-direction, i.e. some multiple of 2!
// 		
// 		output_static_co_objects(); // function and global references may be passed dynamically, 
// 			// as may stubs and wrappers, so the co-object relation needs to contain them.
// 			// note that I really mean "references to statically-created objects", **but** actually
// 			// globals need not be treated any differently, unless there are two pre-existing static
// 			// obejcts we want to correspond, or unless they should be treated differently than their
// 			// DWARF-implied form would entail. 
// 			// We treat functions specially so that we don't have to generate rep-conversion functions
// 			// for functions -- we expand all the rep-conversion ahead-of-time by generating wrappers.
// 			// This works because functions are only ever passed by reference (identity). If we could
// 			// construct functions at run-time, things would be different!
// 			
// 		// output_stubs(); -- compile stubs from stub language expressions
		
	}
	
	void link_derivation::add_corresps_from_block(
		module_ptr left,
		module_ptr right,
		antlr::tree::Tree *corresps)
	{
		std::cerr << "Adding explicit corresps from block: " << CCP(TO_STRING_TREE(corresps))
			<< std::endl;
		assert(GET_TYPE(corresps) == CAKE_TOKEN(CORRESP));
		INIT;
		FOR_ALL_CHILDREN(corresps)
		{
			switch(GET_TYPE(n))
			{
				case CAKE_TOKEN(EVENT_CORRESP):
					{
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
										BIND2(sourcePattern, memberNameExpr);
										touched_events[left].insert(
											read_definite_member_name(memberNameExpr));
									}
									BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
									BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
									BIND2(correspHead, sinkExpr);
									BIND3(correspHead, returnEvent, RETURN_EVENT);
									add_event_corresp(left, sourcePattern, sourceInfixStub,
										right, sinkExpr, sinkInfixStub, returnEvent);
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
										BIND2(sourcePattern, memberNameExpr);
										touched_events[right].insert(
											read_definite_member_name(memberNameExpr));
									}
									BIND3(correspHead, returnEvent, RETURN_EVENT);
									add_event_corresp(right, sourcePattern, sourceInfixStub,
										left, sinkExpr, sinkInfixStub, returnEvent);
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
										BIND2(leftPattern, memberNameExpr);
										touched_events[left].insert(
											read_definite_member_name(memberNameExpr));
									}
									BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
									BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
									BIND3(correspHead, rightPattern, EVENT_PATTERN);
									{
										INIT;
										BIND2(rightPattern, eventContext);
										BIND2(rightPattern, memberNameExpr);
										touched_events[right].insert(
											read_definite_member_name(memberNameExpr));
									}
									BIND3(correspHead, returnEvent, RETURN_EVENT);
									add_event_corresp(left, leftPattern, leftInfixStub, 
										right, rightPattern, rightInfixStub, returnEvent);
									add_event_corresp(right, rightPattern, rightInfixStub,
										left, leftPattern, leftInfixStub, returnEvent);
								}
								break;
							default: RAISE(correspHead, "expected a double-stemmed arrow");
						}
					}					
					break;
				case CAKE_TOKEN(KEYWORD_VALUES):
					{
						INIT;
						BIND2(n, correspHead); // some kind of correspondence operator
						std::string ruleName;
						if (GET_CHILD_COUNT(n) > 1)
						{
							BIND3(n, ruleNameIdent, IDENT);
							ruleName = CCP(GET_TEXT(ruleNameIdent));
						}
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
								ALIAS3(leftValDecl, leftMember, DEFINITE_MEMBER_NAME);
								ALIAS3(rightValDecl, rightMember, DEFINITE_MEMBER_NAME);
								// each add_value_corresp call denotes a 
								// value conversion function that needs to be generated
								add_value_corresp(left, leftMember, leftInfixStub,
									right, rightMember, rightInfixStub,
									valueCorrespondenceRefinement, true, correspHead);
								add_value_corresp(right, rightMember, rightInfixStub,
									left, leftMember, leftInfixStub, 
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
								ALIAS3(leftValDecl, leftMember, DEFINITE_MEMBER_NAME);
								ALIAS3(rightValDecl, rightMember, DEFINITE_MEMBER_NAME);
								
								// FIXME: pay attention to question marks
								switch(GET_TYPE(correspHead))
								{
									case CAKE_TOKEN(LR_DOUBLE_ARROW):
									case CAKE_TOKEN(LR_DOUBLE_ARROW_Q):
										add_value_corresp(left, leftMember, leftInfixStub,
											right, rightMember, rightInfixStub,
											valueCorrespondenceRefinement, true, correspHead);
										goto record_touched;
									case CAKE_TOKEN(RL_DOUBLE_ARROW):
									case CAKE_TOKEN(RL_DOUBLE_ARROW_Q):
										add_value_corresp(right, rightMember, rightInfixStub,
											left, leftMember, leftInfixStub, 
											valueCorrespondenceRefinement, false, correspHead);
										goto record_touched;
									record_touched:
										touched_data_types[left].insert(
											read_definite_member_name(leftMember));
										touched_data_types[right].insert(
											read_definite_member_name(rightMember));
										break;
									default: assert(false);
								}
							}
							break;
							default: assert(false);
						}
					}
					break;
				default: RAISE(n, "expected an event correspondence or a value correspondence block");
			}
		}
	}
	
	void link_derivation::add_event_corresp(
		module_ptr source, 
		antlr::tree::Tree *source_pattern,
		antlr::tree::Tree *source_infix_stub,
		module_ptr sink,
		antlr::tree::Tree *sink_expr,
		antlr::tree::Tree *sink_infix_stub,
		antlr::tree::Tree *return_leg,
		bool free_source,
		bool free_sink,
		bool init_only)
	{
		assert(GET_TYPE(sink_expr) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
		auto key = sorted(std::make_pair(source, sink));
		assert(all_iface_pairs.find(key) != all_iface_pairs.end());
		ev_corresps.insert(std::make_pair(key, 
						(struct ev_corresp){ /*.source = */ source, // source is the *requirer*
							/*.source_pattern = */ source_pattern,
							/*.source_infix_stub = */ source_infix_stub,
							/*.sink = */ sink, // sink is the *provider*
							/*.sink_expr = */ sink_expr,
							/*.sink_infix_stub = */ sink_infix_stub,
							return_leg,
							/*.source_pattern_to_free = */ (free_source) ? source_pattern : 0,
							/*.sink_pattern_to_free = */ (free_sink) ? sink_expr : 0 }));
		/* print debugging info: what type expectations did we find in the stubs? */
		if (sink_expr)
		{
			std::cerr << "stub: " << CCP(TO_STRING_TREE(sink_expr))
				<< " implies type expectations as follows: " << std::endl;
			std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> > out;
			find_type_expectations_in_stub(sink,
				sink_expr, boost::shared_ptr<dwarf::spec::type_die>(), // FIXME: use return type
				out);
			if (out.size() == 0) std::cerr << "(no type expectations inferred)" << std::endl;
			else for (auto i_exp = out.begin(); i_exp != out.end(); i_exp++)
			{
				std::cerr << i_exp->first << " as " << *i_exp->second << std::endl;
			}
			
		}
	}

	/* This version is called from processing the pairwise block AST.
	 * It differs from the canonical version only in that 
	 * it accepts the source and sink as definite_member_names,
	 * not DIE pointers. */
	void link_derivation::add_value_corresp(
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
		auto source_mn = read_definite_member_name(source_data_type_mn);
		auto source_data_type_opt = boost::dynamic_pointer_cast<dwarf::spec::type_die>(
			source->get_ds().toplevel()->visible_resolve(
			source_mn.begin(), source_mn.end()));
		if (!source_data_type_opt) RAISE(corresp, "could not resolve data type");
		auto sink_mn = read_definite_member_name(sink_data_type_mn);
		auto sink_data_type_opt = boost::dynamic_pointer_cast<dwarf::spec::type_die>(
			sink->get_ds().toplevel()->visible_resolve(
			sink_mn.begin(), sink_mn.end()));
		if (!sink_data_type_opt) RAISE(corresp, "could not resolve data type");
		add_value_corresp(source, 
			source_data_type_opt, 
			source_infix_stub,
			sink, 
			sink_data_type_opt, 
			sink_infix_stub,
			refinement, source_is_on_left, corresp);
	}
	
	/* This version is used to add implicit dependencies. There is no
	 * refinement or corresp or any of the other syntactic stuff. */
	bool link_derivation::ensure_value_corresp(module_ptr source, 
		boost::shared_ptr<dwarf::spec::type_die> source_data_type,
		module_ptr sink,
		boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
		bool source_is_on_left)
	{
		auto key = sorted(std::make_pair(wrap_code.module_of_die(source_data_type), 
			wrap_code.module_of_die(sink_data_type)));
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
		add_value_corresp(wrap_code.module_of_die(source_data_type), 
			source_data_type,
			0,
			wrap_code.module_of_die(sink_data_type),
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
	void link_derivation::add_value_corresp(
		module_ptr source, 
		boost::shared_ptr<dwarf::spec::type_die> source_data_type,
		antlr::tree::Tree *source_infix_stub,
		module_ptr sink,
		boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
		antlr::tree::Tree *sink_infix_stub,
		antlr::tree::Tree *refinement,
		bool source_is_on_left,
		antlr::tree::Tree *corresp,
		bool init_only
	)
	{
// 		std::cerr << "Adding value corresp from source module @" << &*source
// 			<< " source data type @" << &*source_data_type << " " << *source_data_type
// 			<< " source infix stub @" << source_infix_stub
// 			<< " to sink module @" << &*sink
// 			<< " sink data type @" << &*sink_data_type << " " << *sink_data_type
// 			<< " sink infix stub @" << sink_infix_stub
// 			<< " refinement @" << refinement
// 			<< " source on " << (source_is_on_left ? "left" : "right")
// 			<< " corresp @" << corresp << std::endl;
	
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
		
		auto key = sorted(std::make_pair(source, sink));
		assert(all_iface_pairs.find(key) != all_iface_pairs.end());
		assert(source_data_type);
		assert(sink_data_type);
//     	val_corresps.insert(std::make_pair(key,

		auto basic = (struct basic_value_conversion){ /* .source = */ source,
							/* .source_data_type = */ source_data_type,
							/* .source_infix_stub = */ source_infix_stub,
							/* .sink = */ sink,
							/* .sink_data_type = */ sink_data_type,
							/* .source_infix_stub = */ sink_infix_stub,
							/* .refinement = */ refinement,
							/* .source_is_on_left = */ source_is_on_left,
							/* .corresp = */ corresp };
		
		// can't handle infix stubs, yet
		assert((!source_infix_stub || GET_CHILD_COUNT(source_infix_stub) == 0)
		&& (!sink_infix_stub || GET_CHILD_COUNT(sink_infix_stub) == 0));
	
		// we *only* emit conversions between concrete types
		auto source_concrete_type = source_data_type->get_concrete_type();
		auto sink_concrete_type = sink_data_type->get_concrete_type();
	
		// skip incomplete (void) typedefs and other incompletes
		if (!(source_concrete_type && compiler.cxx_is_complete_type(source_concrete_type))
		|| !(sink_concrete_type && compiler.cxx_is_complete_type(sink_concrete_type)))
		{
			std::cerr << "Warning: skipping value conversion from " << wrap_code.get_type_name(source_data_type)
				<< " to " << wrap_code.get_type_name(sink_data_type)
				<< " because one or other is an incomplete type." << std::endl;
			//m_out << "// (skipped because of incomplete type)" << std::endl << std::endl;
			val_corresps.insert(std::make_pair(key, 
				boost::dynamic_pointer_cast<value_conversion>(
					boost::make_shared<skipped_value_conversion>(wrap_code, wrap_code.m_out, 
					basic, std::string("incomplete type")))));
			return;
		}
		
		// now we can compute the concrete type names 
		auto from_typename = wrap_code.get_type_name(source_concrete_type);
		auto to_typename = wrap_code.get_type_name(sink_concrete_type);

		// skip pointers and references
		if (source_concrete_type->get_tag() == DW_TAG_pointer_type
		|| sink_concrete_type->get_tag() == DW_TAG_pointer_type
		|| source_concrete_type->get_tag() == DW_TAG_reference_type
		|| sink_concrete_type->get_tag() == DW_TAG_reference_type)
		{
			std::cerr << "Warning: skipping value conversion from " << from_typename
				<< " to " << to_typename
				<< " because one or other is an pointer or reference type." << std::endl;
			//m_out << "// (skipped because of pointer or reference type)" << std::endl << std::endl;
			val_corresps.insert(std::make_pair(key, 
				boost::dynamic_pointer_cast<value_conversion>(
					boost::make_shared<skipped_value_conversion>(wrap_code, wrap_code.m_out, 
					basic, "pointer or reference type"))));
			return;
		}
		// skip subroutine types
		if (source_concrete_type->get_tag() == DW_TAG_subroutine_type
		|| sink_concrete_type->get_tag() == DW_TAG_subroutine_type)
		{
			std::cerr << "Warning: skipping value conversion from " << from_typename
				<< " to " << to_typename
				<< " because one or other is a subroutine type." << std::endl;
			//m_out << "// (skipped because of subroutine type)" << std::endl << std::endl;
			val_corresps.insert(std::make_pair(key, 
				boost::dynamic_pointer_cast<value_conversion>(
					boost::make_shared<skipped_value_conversion>(wrap_code, wrap_code.m_out, 
					basic, "subroutine type"))));
			return;
		}
		
		// from this point, we will generate a candidate for an init rule
		boost::shared_ptr<value_conversion> init_candidate;
			
		bool emit_as_reinterpret = false;
		if (source_concrete_type->is_rep_compatible(sink_concrete_type)
			&& (!refinement || GET_CHILD_COUNT(refinement) == 0))
		{
			// two rep-compatible cases
			if (compiler.cxx_assignable_from(sink_concrete_type, source_concrete_type))
			{
				std::cerr << "Skipping generation of value conversion from "
					<< from_typename << " to " << to_typename
					<< " because of rep-compatibility and C++-assignability." << std::endl;
				//m_out << "// (skipped because of rep-compatibility and C++-assignability)" << std::endl << std::endl;
				val_corresps.insert(std::make_pair(key, 
					init_candidate = boost::dynamic_pointer_cast<value_conversion>(
						boost::make_shared<skipped_value_conversion>(wrap_code, wrap_code.m_out, 
						basic, "rep-compatibility and C++-assignability"))));
				//return;
				goto add_init_candidate;
			}			
			else
			{
				std::cerr << "Generating a reinterpret_cast value conversion from "
					<< from_typename << " to " << to_typename
					<< " as they are rep-compatible but not C++-assignable." << std::endl;
				emit_as_reinterpret = true;
			}
		}
		else
		{
			// rep-incompatible cases are the same in effect but we report them individually
			if (!refinement || GET_CHILD_COUNT(refinement) == 0)
			{
				std::cerr << "Generating value conversion from "
					<< from_typename << " to " << to_typename
					<< " as they are not rep-compatible." << std::endl;
			}
			else
			{
				// FIXME: refinements that just do field renaming
				// are okay -- can recover rep-compatibility so
				// should use reinterpret conversion in these cases
				// + propagate to run-time by generating artificial matching field names
				// for fields whose renaming enables rep-compatibility
				std::cerr << "Generating value conversion from "
					<< from_typename << " to " << to_typename
					<< " as they have nonempty refinement." << std::endl;
			}
		}

		// here goes the value conversion logic
		if (!emit_as_reinterpret)
		{
			// -- dispatch to a function based on the DWARF tags of the two types
	#define TAG_PAIR(t1, t2) ((t1)<<((sizeof (Dwarf_Half))*8) | (t2))
			switch(TAG_PAIR(source_data_type->get_tag(), sink_data_type->get_tag()))
			{
				case TAG_PAIR(DW_TAG_structure_type, DW_TAG_structure_type):
					//emit_structural_conversion_body(source_data_type, sink_data_type,
					//	refinement, source_is_on_left);
					val_corresps.insert(std::make_pair(key, 
						init_candidate = boost::dynamic_pointer_cast<value_conversion>(
							boost::make_shared<structural_value_conversion>(wrap_code, wrap_code.m_out, 
							basic))));
				break;
				default:
					std::cerr << "Warning: didn't know how to generate conversion between "
						<< *source_data_type << " and " << *sink_data_type << std::endl;
				return;
			}
	#undef TAG_PAIR
		}
		else
		{
			//emit_reinterpret_conversion_body(source_data_type, sink_data_type);
			val_corresps.insert(std::make_pair(key, 
				init_candidate = boost::dynamic_pointer_cast<value_conversion>(
					boost::make_shared<reinterpret_value_conversion>(wrap_code, wrap_code.m_out, 
					basic))));
		}
		
	add_init_candidate:
		assert(init_candidate);
		init_rules_key_t init_tbl_key = (init_rules_key_t) {
				(init_candidate->source == key.first),
				init_candidate->source_data_type
			};
		candidate_init_rules_tbl_keys[key].insert(init_tbl_key);
		candidate_init_rules[key].insert(std::make_pair(
			init_tbl_key,
			init_candidate
		));
	}
	
	// Get the names of all functions provided by iface1
	void link_derivation::add_implicit_corresps(iface_pair ifaces)
	{
		std::cerr << "Adding implicit correspondences between module " 
			<< ifaces.first->get_filename() << " and " << ifaces.second->get_filename()
			<< std::endl;
	  	
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
			
		/* Search dwarf info*/
		dwarf::encap::Die_encap_all_compile_units& requiring_info
			= requiring_iface->all_compile_units();

		dwarf::encap::Die_encap_all_compile_units& providing_info
			= providing_iface->all_compile_units();
			
		required_funcs_iter r_end(
			requiring_info.subprograms_begin(), requiring_info.subprograms_end(),
			requiring_info.subprograms_end());
		provided_funcs_iter p_iter(
			providing_info.subprograms_begin(), providing_info.subprograms_end());
		provided_funcs_iter p_end(
			providing_info.subprograms_begin(), providing_info.subprograms_end(),
			providing_info.subprograms_end());
	
		for (required_funcs_iter r_iter(
				requiring_info.subprograms_begin(), 
				requiring_info.subprograms_end()); r_iter != r_end; r_iter++)
		{        
			std::cerr << "Found a required subprogram!" << std::endl;
			std::cerr << **r_iter;
			
			for (provided_funcs_iter p_iter(
					providing_info.subprograms_begin(), 
					providing_info.subprograms_end()); p_iter != p_end; p_iter++) 
			{
				if ((*r_iter)->get_name() == (*p_iter)->get_name())
				{
					/* Only add a correspondence if we haven't already got 
					 * *either* a correspondence for the source event,
					 * *or* a correspondence for sink event. 
					 * We haven't computed wrappers yet, so we can't use
					 * that. */
					 
					// add a correspondence
					std::cerr << "Matched name " << *((*r_iter)->get_name())
						<< " in modules " << *((*r_iter)->parent().get_name())
						<< " and " << *((*p_iter)->parent().get_name())
						<< std::endl;
					// don't add if explicit rules have touched this event
					if (touched_events[requiring_iface].find(
							definite_member_name(std::vector<std::string>(1, *(*r_iter)->get_name())))
						!= touched_events[requiring_iface].end())
					{
						std::cerr << "Matched name " << *((*r_iter)->get_name())
							<< " already touched by an explicit correspondence, so skipping."
							<< std::endl;
						continue;
					}

					antlr::tree::Tree *tmp_source_pattern = 
						make_simple_event_pattern_for_call_site(
							*((*r_iter)->get_name()));
					antlr::tree::Tree *tmp_sink_pattern = 
						make_simple_sink_expression_for_event_name(
							std::string(*((*p_iter)->get_name())) /*+ std::string("(...)")*/);
					// we should have just generated an event pattern and a function invocation
					assert(GET_TYPE(tmp_source_pattern) == CAKE_TOKEN(EVENT_PATTERN));
					assert(GET_TYPE(tmp_sink_pattern) == CAKE_TOKEN(EVENT_SINK_AS_STUB));

					add_event_corresp(requiring_iface, // source is the requirer 
						tmp_source_pattern,
						0, // no infix stub
						providing_iface, // sink is the provider
						tmp_sink_pattern, 
						0, // no infix stub
						0, // no return event
						true, true);
				}
			}
		}
	}
	
	void link_derivation::extract_type_synonymy(module_ptr module,
		std::map<std::vector<std::string>, boost::shared_ptr<dwarf::spec::type_die> >& synonymy)
	{
		// synonymy map is from synonym to concrete
		
		for (auto i_die = module->get_ds().begin();
			i_die != module->get_ds().end();
			i_die++)
		{
			auto p_typedef = boost::dynamic_pointer_cast<dwarf::spec::typedef_die>(*i_die);
			if (!p_typedef) continue;
			if (p_typedef == p_typedef->get_concrete_type()) continue;
			
			if (!p_typedef->get_concrete_type())
			{
				std::cerr << "FIXME: typedef "
					<< *p_typedef
					<< " has no concrete type -- we should add it to synonymy map,"
					<< " but skipping for now." 
					<< std::endl;
			}
			else
			{
				synonymy.insert(std::make_pair(
					*p_typedef->ident_path_from_cu(), 
					p_typedef->get_concrete_type()));
				std::cerr << "synonymy within " << module->filename << ": "
					<< definite_member_name(*p_typedef->ident_path_from_cu())
					<< " ----> " << *p_typedef->get_concrete_type() << std::endl;
			}
		}
	}
	
	boost::optional<link_derivation::val_corresp_map_t::iterator>
	link_derivation::find_value_correspondence(
		module_ptr source, boost::shared_ptr<dwarf::spec::type_die> source_type,
		module_ptr sink, boost::shared_ptr<dwarf::spec::type_die> sink_type)
	{
		auto iter_pair = val_corresps.equal_range(sorted(std::make_pair(source, sink)));
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
			boost::shared_ptr<dwarf::spec::type_die> t;
		};
		std::multimap<std::vector<std::string>, found_type> found_types;
		std::set<std::vector<std::string> > keys;
		
		std::map<std::vector<std::string>, boost::shared_ptr<dwarf::spec::type_die> > first_synonymy;
		extract_type_synonymy(ifaces.first, first_synonymy);
		std::map<std::vector<std::string>, boost::shared_ptr<dwarf::spec::type_die> > second_synonymy;		
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
				i_die++)
			{
				auto p_type = boost::dynamic_pointer_cast<dwarf::spec::type_die>(*i_die);
				if (!p_type) continue;

				auto opt_path = p_type->ident_path_from_cu();
				if (!opt_path) continue;

				found_types.insert(std::make_pair(*opt_path, (found_type){ i_mod, p_type }));
				keys.insert(*opt_path);
			}
		}
		
		std::set< boost::shared_ptr<dwarf::spec::type_die> > seen_concrete_types;

		std::set< std::pair< // this is any pair which is  a base type -- needn't both be base
			              dwarf::tool::cxx_compiler::base_type,
			              boost::shared_ptr<dwarf::spec::type_die> 
			              >
			    > seen_first_base_type_pairs;
		std::set< std::pair< // this is any pair *involving* a base type -- needn't both be base
			               boost::shared_ptr<dwarf::spec::type_die> ,
			               dwarf::tool::cxx_compiler::base_type
			               >
			    > seen_second_base_type_pairs;
		std::set< std::pair< dwarf::tool::cxx_compiler::base_type,
			                 dwarf::tool::cxx_compiler::base_type 
			               >
			    > seen_base_base_pairs;
		// Now look for names that have exactly two entries in the multimap,
		// (i.e. exactly two <vector, found_type> pairs for a given vector,
		//  i.e. exactly two types were found having a given name-vector)
		// and where the module of each is different.
		for (auto i_k = keys.begin(); i_k != keys.end(); i_k++)
		{
			auto iter_pair = found_types.equal_range(*i_k);
			if (
				srk31::count(iter_pair.first, iter_pair.second) == 2 // exactly two
			&&  // different modules
			(iter_pair.second--, iter_pair.first->second.module != iter_pair.second->second.module)
			&& // must be concrete, non-array, non-subroutine types!
			iter_pair.first->second.t->get_concrete_type() && 
				iter_pair.second->second.t->get_concrete_type() &&
				iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_array_type &&
				iter_pair.second->second.t->get_concrete_type()->get_tag() != DW_TAG_array_type &&
				iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_subroutine_type &&
				iter_pair.second->second.t->get_concrete_type()->get_tag() != DW_TAG_subroutine_type
			&& // must not have been matched before!
				seen_concrete_types.find(iter_pair.first->second.t->get_concrete_type())
					== seen_concrete_types.end()
			&& seen_concrete_types.find(iter_pair.second->second.t->get_concrete_type()) 
					== seen_concrete_types.end()
			&& // if base types, pair must not be equivalent to any seen before! 
			   // (else template specializations will collide)
			   // NOTE: this is because DWARF info often includes "char" and "signed char"
			   // and these are distinct in DWARF-land but not in C++-land
				(
					iter_pair.first->second.t->get_concrete_type()->get_tag() != DW_TAG_base_type
					|| seen_first_base_type_pairs.find(std::make_pair(
						dwarf::tool::cxx_compiler::base_type(
							boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.first->second.t->get_concrete_type())),
							iter_pair.second->second.t->get_concrete_type())) ==
							seen_first_base_type_pairs.end()
				)
			&&
				(
					iter_pair.second->second.t->get_concrete_type()->get_tag() != DW_TAG_base_type
					|| seen_second_base_type_pairs.find(std::make_pair(
						iter_pair.first->second.t->get_concrete_type(),
						dwarf::tool::cxx_compiler::base_type(
							boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.second->second.t->get_concrete_type()))))
						== seen_second_base_type_pairs.end()
				)
			&& (!(iter_pair.first->second.t->get_concrete_type()->get_tag() == DW_TAG_base_type &&
				  iter_pair.second->second.t->get_concrete_type()->get_tag() == DW_TAG_base_type)
				 || seen_base_base_pairs.find(
				 	std::make_pair(
						dwarf::tool::cxx_compiler::base_type(
							boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.first->second.t->get_concrete_type())),
						dwarf::tool::cxx_compiler::base_type(
							boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.second->second.t->get_concrete_type()))
					))
					== seen_base_base_pairs.end()
				)
			&& // don't match built-in types, because they don't appear in our dwarfhpp headers
				!compiler.is_builtin(iter_pair.first->second.t->get_concrete_type())
				&& !compiler.is_builtin(iter_pair.second->second.t->get_concrete_type())
			)
			{
				std::cerr << "data type " << definite_member_name(*i_k)
					<< " exists in both modules" << std::endl;
				seen_concrete_types.insert(iter_pair.first->second.t->get_concrete_type());
				seen_concrete_types.insert(iter_pair.second->second.t->get_concrete_type());
				if (iter_pair.first->second.t->get_tag() == DW_TAG_base_type)
					seen_first_base_type_pairs.insert(
						std::make_pair(
							dwarf::tool::cxx_compiler::base_type(
								boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
									iter_pair.first->second.t->get_concrete_type())),
								iter_pair.second->second.t->get_concrete_type())
							);
				if (iter_pair.second->second.t->get_tag() == DW_TAG_base_type)
					seen_second_base_type_pairs.insert(
						std::make_pair(
							iter_pair.first->second.t->get_concrete_type(),
							dwarf::tool::cxx_compiler::base_type(
								boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
									iter_pair.second->second.t->get_concrete_type()))));
				if (iter_pair.first->second.t->get_concrete_type()->get_tag() == DW_TAG_base_type &&
				  iter_pair.second->second.t->get_concrete_type()->get_tag() == DW_TAG_base_type)
				{
					std::cerr << "remembering a base-base pair " 
						<< definite_member_name(*i_k) << "; size was " 
						<< seen_base_base_pairs.size();
				 	seen_base_base_pairs.insert(std::make_pair(
						dwarf::tool::cxx_compiler::base_type(
							boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.first->second.t->get_concrete_type())),
						dwarf::tool::cxx_compiler::base_type(
							boost::dynamic_pointer_cast<dwarf::spec::base_type_die>(
								iter_pair.second->second.t->get_concrete_type()))
					));
					std::cerr << "; now " << seen_base_base_pairs.size() << std::endl;
				}
					
				// iter_pair points to a pair of like-named types in differing modules
				// add value correspondences in *both* directions
				// *** FIXME: ONLY IF not already present already...
				// i.e. the user might have supplied their own
				
				// We always add corresps between concrete types,
				// i.e. dereferencing synonyms -- BUT this can
				// introduce conflicts/ambiguities, so need to refine this later.
				// ALSO, concrete types don't always have 
				// ident paths from CU! i.e. can be anonymous. So
				// instead of using ident_path_from_cu -- FIXME: what?
				
				// two-iteration for loop
				for (std::pair<module_ptr, module_ptr> source_sink_pair = 
						std::make_pair(ifaces.first, ifaces.second), orig_source_sink_pair = source_sink_pair;
						source_sink_pair != std::pair<module_ptr, module_ptr>();
						source_sink_pair =
							(source_sink_pair == orig_source_sink_pair) 
								? std::make_pair(ifaces.second, ifaces.first) : std::make_pair(module_ptr(), module_ptr()))
				{
					// each of these maps a set of synonyms mapping to their concrete type
					std::map<std::vector<std::string>, boost::shared_ptr<dwarf::spec::type_die> > &
						source_synonymy = (source_sink_pair.first == ifaces.first) ? first_synonymy : second_synonymy;
					std::map<std::vector<std::string>, boost::shared_ptr<dwarf::spec::type_die> > &
						sink_synonymy = (source_sink_pair.second == ifaces.first) ? first_synonymy : second_synonymy;
					

					bool source_is_synonym = false;
					boost::shared_ptr<dwarf::spec::basic_die> source_found;
					bool sink_is_synonym = false;
					boost::shared_ptr<dwarf::spec::basic_die> sink_found;
					/* If the s... data type is a synonym, we will set s..._type
					 * to the *concrete* type and set the flag. 
					 * Otherwise we will try to get the data type DIE by name lookup. */
					auto source_type = (source_synonymy.find(*i_k) != source_synonymy.end()) ?
							(source_is_synonym = true, source_synonymy[*i_k]) : /*, // *i_k, // */ 
							boost::dynamic_pointer_cast<dwarf::spec::type_die>(
								source_found = source_sink_pair.first->get_ds().toplevel()->visible_resolve(
									i_k->begin(), i_k->end()));
					auto sink_type = (sink_synonymy.find(*i_k) != sink_synonymy.end()) ?
							(sink_is_synonym = true, sink_synonymy[*i_k]) : /*, // *i_k, // */ 
							boost::dynamic_pointer_cast<dwarf::spec::type_die>(
								sink_found = source_sink_pair.second->get_ds().toplevel()->visible_resolve(
									i_k->begin(), i_k->end()));
					const char *matched_name = i_k->at(0).c_str();
					
// 					std::cerr << "Two-cycle for loop: source module @" << &*source_sink_pair.first << std::endl;
// 					std::cerr << "Two-cycle for loop: sink module @" << &*source_sink_pair.second << std::endl;
// 					std::cerr << "Two-cycle for loop: source synonymy @" << &source_synonymy << std::endl;
// 					std::cerr << "Two-cycle for loop: sink synonymy @" << &sink_synonymy << std::endl;
					
					// this happens if name lookup fails 
					// (e.g. not visible (?))
					// or doesn't yield a type
					if (!source_type || !sink_type)
					{
						boost::shared_ptr<dwarf::spec::basic_die> source_synonym;
						boost::shared_ptr<dwarf::spec::basic_die> sink_synonym;
						if (source_is_synonym) source_synonym = source_synonymy[*i_k];
						if (sink_is_synonym) sink_synonym = sink_synonymy[*i_k];

						std::cerr << "Skipping correspondence for matched data type named " 
						<< definite_member_name(*i_k)
						<< " because (FIXME) the type is probably incomplete"
						<< " where source " << *i_k 
						<< (source_is_synonym ? " was found to be a synonym " : " was not resolved to a type ")
						<< ((!source_is_synonym && source_found) ? " but was resolved to a non-type" : "")
						<< " and sink " << *i_k 
						<< (sink_is_synonym ? " was found to be a synonym " : " was not resolved to a type ")
						<< ((!sink_is_synonym && sink_found) ? " but was resolved to a non-type" : "")
						<< std::endl;
						if (source_synonym) std::cerr << "source synonym: " << source_synonym
							<< std::endl;
						if (sink_synonym) std::cerr << "sink synonym: " << sink_synonym
							<< std::endl;
						assert(definite_member_name(*i_k).at(0) != "int");
						continue;
					}	
													
					// if no previous value correspondence exists between these 
					// name-matched types.....
					if (!find_value_correspondence(source_sink_pair.first, 
							source_type, 
							source_sink_pair.second, 
							sink_type))
					{
		std::cerr << "Adding value corresp from source module @" << source_sink_pair.first.get()
			<< " source data type " << *source_type << " " 
			<< " to sink module @" << source_sink_pair.second.get()
			<< " sink data type " << *sink_type << std::endl;
	
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
						else std::cerr << "Skipping correspondence for matched data type named " 
							<< definite_member_name(*i_k)
							<< " because type synonyms processed earlier already defined a correspondence"
							<< std::endl;
				} // end two-cycle for loop
			}
			else std::cerr << "data type " << definite_member_name(*i_k)
					<< " exists only in one module" << std::endl;
		}
	}

 	std::vector<boost::shared_ptr<dwarf::spec::type_die> >
	link_derivation::
	corresponding_dwarf_types(boost::shared_ptr<dwarf::spec::type_die> type,
		module_ptr corresp_module,
		bool flow_from_type_module_to_corresp_module)
	{
		std::vector<boost::shared_ptr<dwarf::spec::type_die> > found;
		auto ifaces = sorted(std::make_pair(corresp_module,
			wrap_code.module_of_die(type)));
		auto iters = val_corresps.equal_range(ifaces);
		for (auto i_corresp = iters.first;
			i_corresp != iters.second;
			i_corresp++)
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
	boost::shared_ptr<dwarf::spec::type_die>
	link_derivation::unique_corresponding_dwarf_type(
		boost::shared_ptr<dwarf::spec::type_die> type,
		module_ptr corresp_module,
		bool flow_from_type_module_to_corresp_module)
	{
		auto result = this->corresponding_dwarf_types(
			type,
			corresp_module,
			flow_from_type_module_to_corresp_module);
		if (result.size() == 1) return result.at(0);
		else return boost::shared_ptr<dwarf::spec::type_die>();
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
				i_ev != ev_corresps.end(); i_ev++)
		{
			std::string called_function_name = get_event_pattern_call_site_name(
				i_ev->second.source_pattern);
			
			std::cerr << "Function " << called_function_name << " may be wrapped!" << std::endl;
			wrappers[called_function_name].push_back(
					&(*i_ev) // push a pointer to the ev_corresp map entry
				);
		}
	}

//	void link_derivation::output_rep_conversions() {}		
		
//	void link_derivation::output_symbol_renaming_rules() {}		
//	void link_derivation::output_formgens() {}
		
//	void link_derivation::output_wrappergens() {}		
//	void link_derivation::output_static_co_objects() {}	
}
