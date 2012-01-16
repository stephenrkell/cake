#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>

using boost::dynamic_pointer_cast;
using namespace dwarf::spec;
using boost::shared_ptr;
using std::ostringstream;

namespace cake
{
	const std::string wrapper_file::wrapper_arg_name_prefix = "__cake_arg";
	const std::string wrapper_file::NO_VALUE = "__cake_arg";

	codegen_context::codegen_context(wrapper_file& w, module_ptr source, module_ptr sink, 
		const environment& initial_env)
	: req(w.m_r), derivation(w.m_d), ns_prefix(w.ns_prefix), 
	  modules({source, sink, source}), opt_source(), 
	  dwarf_context((dwarf_context_s)
			        {source->get_ds().toplevel(),
			         sink->get_ds().toplevel()}), 
	  env(initial_env) {}

	bool wrapper_file::treat_subprogram_as_untyped(
		boost::shared_ptr<dwarf::spec::subprogram_die> subprogram)
	{
		auto args_begin 
			= subprogram->formal_parameter_children_begin();
		auto args_end
			= subprogram->formal_parameter_children_end();
		return (args_begin == args_end
						 && subprogram->unspecified_parameters_children_begin() !=
						subprogram->unspecified_parameters_children_end());
	}	
	bool wrapper_file::subprogram_returns_void(
		shared_ptr<subprogram_die> subprogram)
	{
		if (!subprogram->get_type())
		{
			if (treat_subprogram_as_untyped(subprogram))
			{
				std::cerr << "Warning: assuming function " << *subprogram << " is void-returning."
					<< std::endl;
			}
			return true;
		}
		return false;
	}			
	
   void wrapper_file::write_function_header(
        antlr::tree::Tree *event_pattern,
        const std::string& function_name_to_use,
        shared_ptr<subprogram_die> subprogram,
        const std::string& arg_name_prefix,
        module_ptr calling_module, // HACK: don't do this; make a pre-pass to guess signature & store in caller's DWARF info
        bool emit_types /*= true*/,
		boost::shared_ptr<dwarf::spec::subprogram_die> unique_called_subprogram)
    {
    	auto ret_type = subprogram->get_type();
        auto args_begin = subprogram->formal_parameter_children_begin();
        auto args_end = subprogram->formal_parameter_children_end();
        bool ignore_dwarf_args = treat_subprogram_as_untyped(subprogram);
        
        if (emit_types)
        {
        	if (subprogram_returns_void(subprogram)) m_out << "void";
            else if (treat_subprogram_as_untyped(subprogram) && !unique_called_subprogram) m_out << 
            	/* " ::cake::unspecified_wordsize_type"; */ " ::cake::wordsize_integer_type"; // HACK!
			else if (treat_subprogram_as_untyped(subprogram) && unique_called_subprogram)
			{
				// look for a _unique_ _corresponding_ type and use that
				assert(unique_called_subprogram->get_type());
				auto found_vec = m_d.corresponding_dwarf_types(
					unique_called_subprogram->get_type(),
					calling_module,
					true /* flow_from_type_module_to_corresp_module */);
				if (found_vec.size() == 1)
				{
					// we're in luck
					m_out << get_type_name(found_vec.at(0));
				}
				else 
				{
					m_out << " ::cake::unspecified_wordsize_type";
				}
			}
            else m_out << get_type_name(ret_type);
             
            m_out << ' ';
        }
        m_out << function_name_to_use << '(';
       
        auto i_arg = args_begin;
		dwarf::spec::subprogram_die::formal_parameter_iterator i_fp = 
			unique_called_subprogram ? unique_called_subprogram->formal_parameter_children_begin()
			: dwarf::spec::subprogram_die::formal_parameter_iterator();
        
        // actually, iterate over the pattern
        assert(GET_TYPE(event_pattern) == CAKE_TOKEN(EVENT_PATTERN));
        INIT;
        BIND3(event_pattern, eventContext, EVENT_CONTEXT);
        BIND2(event_pattern, memberNameExpr); // name of call being matched -- can ignore this here
        BIND3(event_pattern, eventCountPredicate, EVENT_COUNT_PREDICATE);
        BIND3(event_pattern, eventParameterNamesAnnotation, KEYWORD_NAMES);
		int argnum = 0;
        int pattern_args;
        switch(GET_CHILD_COUNT(event_pattern))
        {
            case 4: // okay, we have no argument list (simple renames only)
            	if (ignore_dwarf_args && !unique_called_subprogram)
                {  		
                	// problem: no source of argument info! Assume none
                	std::cerr << "Warning: wrapping function " << function_name_to_use
                	    << " declared with unspecified parameters"
                        << " and no event pattern arguments. "
                        << "Assuming empty argument list." << std::endl; 
				}
				else if (ignore_dwarf_args && unique_called_subprogram)
				{ 
// 						/* We will use the DWARF info on the *callee* subprogram. */
// 						//goto use_unique_called_subprogram_args;
// 					// get argument at position argnum from the subprogram;
// 					/*auto*/ i_fp = unique_called_subprogram->formal_parameter_children_begin();
// 					//i_arg = unique_called_subprogram->formal_parameter_children_begin();
// 					for (int i = 0; i < argnum; i++) ++i_fp;
// 	//xxxxxxxxxxxxxxxxxxxxxxx
// 					if (emit_types)
// 					{
// 						if ((*i_fp)->get_type())
// 						{
// 							// look for a _unique_ _corresponding_ type and use that
// 							auto found_vec = m_d.corresponding_dwarf_types(
// 								(*i_fp)->get_type(),
// 								calling_module,
// 								false /* flow_from_type_module_to_corresp_module */);
// 							if (found_vec.size() == 1)
// 							{
// 								std::cerr << "Found unique corresponding type" << std::endl;
// 								// we're in luck
// 								m_out << get_type_name(found_vec.at(0));
// 							}
// 							else 
// 							{
// 								std::cerr << "Didn't find unique corresponding type" << std::endl;
// 								if (treat_subprogram_as_untyped(unique_called_subprogram))
// 								{ m_out << " ::cake::unspecified_wordsize_type"; }
// 								else m_out << get_type_name((*i_fp)->get_type());
// 							}
// 						}
// 						else  // FIXME: remove duplication here ^^^ vvv
// 						{
// 							if (treat_subprogram_as_untyped(unique_called_subprogram))
// 							{ m_out << " ::cake::unspecified_wordsize_type"; }
// 							else m_out << get_type_name((*i_fp)->get_type());
// 						}
// 					}
// 	//				else
// 	//				{
// 	//				
// 	//xxxxxxxxxxxxxxxxxxxxxxxx				
// 	//                     m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
// 	//                        (*i_fp)->get_type()));
// 	//					}
// 					if ((*i_fp)->get_name())
// 					{
//                     	// output the variable name, prefixed 
//                     	m_out << ' ' << arg_name_prefix << argnum /*<< '_' << *(*i_fp)->get_name()*/;
// 					}
// 					else
// 					{
//                     	// output the argument type and a dummy name
//                     	//if (emit_types) m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
//                     	//    (*i_fp)->get_type()));
//                     	m_out << ' ' << arg_name_prefix << argnum /*<< "_dummy"/* << argnum*/;
// 					}
// 					goto next;
assert(false && "disabled support for inferring positional argument mappings");
				}
				else if (!ignore_dwarf_args && unique_called_subprogram)
				{ /* FIXME: Check that they're consistent! */ }
				break;
			default: { // must be >=5
				pattern_args = GET_CHILD_COUNT(event_pattern) - 4; /* ^ number of bindings above! */
				assert(pattern_args >= 1);
				FOR_REMAINING_CHILDREN(event_pattern)
				{
					if (!ignore_dwarf_args && i_arg == args_end)
					{
						std::ostringstream msg;
						msg << "argument pattern has too many arguments for subprogram "
							<< subprogram;
						RAISE(event_pattern, msg.str());
					}
					ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
					{
						INIT;
						BIND2(n, valuePattern)
						switch(GET_TYPE(valuePattern))
						{
							// these are all okay -- we don't care 
							case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
								assert(false);
							case CAKE_TOKEN(NAME_AND_INTERPRETATION):
								{
									INIT;
									BIND3(valuePattern, definiteMemberName, DEFINITE_MEMBER_NAME);
									definite_member_name mn = 
										read_definite_member_name(definiteMemberName);
									if (mn.size() > 1) RAISE(valuePattern, "may not be compound");
									// output the variable type, or unspecified_wordsize_type
									if (emit_types) m_out << ((ignore_dwarf_args || !(*i_arg)->get_type()) ? " ::cake::unspecified_wordsize_type" : get_type_name(
										(*i_arg)->get_type()));
									// output the variable name, prefixed 
									m_out << ' ' << arg_name_prefix << argnum /*<< '_' << mn.at(0)*/;
								} break;
							case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
							case CAKE_TOKEN(METAVAR):
							case CAKE_TOKEN(KEYWORD_CONST):
								// output the argument type and a dummy name
								if (emit_types) m_out << ((ignore_dwarf_args || !(*i_arg)->get_type()) ? "::cake::unspecified_wordsize_type" : get_type_name(
									(*i_arg)->get_type()));
								m_out << ' ' << arg_name_prefix << argnum /*<< "_dummy" << argnum*/;
								break;
							default: RAISE_INTERNAL(valuePattern, "not a value pattern");
						} // end switch
					} // end ALIAS3 
					// advance to the next
				next:
					// work out whether we need a comma
					if (!ignore_dwarf_args) 
					{	
						//std::cerr << "advance DWARF caller arg cursor from " << **i_arg;
						++i_arg; // advance DWARF caller arg cursor
						//std::cerr << " to ";
						//if (i_arg != args_end) std::cerr << **i_arg; else std::cerr << "(sentinel)";
						//std::cerr << std::endl;
					}
                	argnum++; // advance our arg count
					if (ignore_dwarf_args && unique_called_subprogram)
					{
						++i_fp;
						// use DWARF callee arg cursor
						if (i_fp != unique_called_subprogram->formal_parameter_children_end())
						{
							m_out << ", ";
						}
					}
					else if (ignore_dwarf_args) // && !unique_called_subprogram
					{
						if (argnum != pattern_args) m_out << ", ";
					}
					else 
					{
                		if (i_arg != args_end) m_out << ", ";
					}
					
				} // end FOR_REMAINING_CHILDREN
			}	// end default; fall through!
			
// 			break; // no, break!
// 			// FIXME: this code isn't used!
// 			use_unique_called_subprogram_args:
// 				// get argument at position argnum from the subprogram;
// 				/*auto*/ i_fp = unique_called_subprogram->formal_parameter_children_begin();
// 				//i_arg = unique_called_subprogram->formal_parameter_children_begin();
// 				for (int i = 0; i < argnum; i++) ++i_fp;
// //xxxxxxxxxxxxxxxxxxxxxxx
// 				if (emit_types)
// 				{
// 					if ((*i_fp)->get_type())
// 					{
// 						// look for a _unique_ _corresponding_ type and use that
// 						auto found_vec = m_d.corresponding_dwarf_types(
// 							*(*i_fp)->get_type(),
// 							calling_module,
// 							false /* flow_from_type_module_to_corresp_module */);
// 						if (found_vec.size() == 1)
// 						{
// 							std::cerr << "Found unique corresponding type" << std::endl;
// 							// we're in luck
// 							m_out << get_type_name(found_vec.at(0));
// 						}
// 						else 
// 						{
// 							std::cerr << "Didn't find unique corresponding type" << std::endl;
// 							m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
//                         		*(*i_fp)->get_type()));
// 						}
// 					}
// 					else  // FIXME: remove duplication here ^^^ vvv
// 					{
// 						m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
//                         	*(*i_fp)->get_type()));
// 					}
// 				}
// //				else
// //				{
// //				
// //xxxxxxxxxxxxxxxxxxxxxxxx				
// //                     m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
// //                        (*i_fp)->get_type()));
// //					}
// 				if ((*i_fp)->get_name())
// 				{
//                     // output the variable name, prefixed 
//                     m_out << ' ' << arg_name_prefix << argnum /*<< '_' << *(*i_fp)->get_name()*/;
// 				}
// 				else
// 				{
//                     // output the argument type and a dummy name
//                     //if (emit_types) m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
//                     //    *(*i_fp)->get_type()));
//                     m_out << ' ' << arg_name_prefix << argnum/* << "_dummy"/* << argnum*/;
// 				}
// 				goto next;
//             next:
//                 // work out whether we need a comma
//                 if (!ignore_dwarf_args) 
// 				{	
// 					std::cerr << "advance DWARF caller arg cursor from " << **i_arg;
// 					++i_arg; // advance DWARF caller arg cursor
// 					std::cerr << " to ";
// 					if (i_arg != args_end) std::cerr << **i_arg; else std::cerr << "(sentinel)";
// 					std::cerr << std::endl;
// 				}
//                 argnum++; // advance our arg count
// 				if (ignore_dwarf_args && unique_called_subprogram)
// 				{
// 					++i_fp;
// 					// use DWARF callee arg cursor
// 					if (i_fp != unique_called_subprogram->formal_parameter_children_end())
// 					{
// 						m_out << ", ";
// 					}
// 				}
// 				else if (ignore_dwarf_args) // && !unique_called_subprogram
// 				{
// 					if (argnum != pattern_args) m_out << ", ";
// 				}
// 				else 
// 				{
//                 	if (i_arg != args_end) m_out << ", ";
// 				}
// 			break;
        } // end switch GET_CHILD_COUNT
			
		// if we have spare arguments at the end, something's wrong
        if (!ignore_dwarf_args && i_arg != args_end)
        {
            std::ostringstream msg;
            msg << "argument pattern has too few arguments for subprogram: "
                << *subprogram
				<< ": processed " << (argnum + 1) << " arguments, and ";
			int count = 0;
			for (auto i_arg_ctr = subprogram->formal_parameter_children_begin();
				i_arg_ctr != subprogram->formal_parameter_children_end();
				++i_arg_ctr) { count++; }
			msg << "subprogram has " << count << " arguments (first uncovered: "
				<< **i_arg << ").";
            RAISE(event_pattern, msg.str());
        }

        //} // end switch
                        
        m_out << ')';

        m_out.flush();
    }

	/* This is the main wrapper generation function. */
	void wrapper_file::emit_wrapper(
			const std::string& wrapped_symname, 
			link_derivation::ev_corresp_pair_ptr_list& corresps)
	{
		// ensure a nonempty corresp list
		if (corresps.size() == 0) 
		{
			std::cerr << "No corresps for symbol " << wrapped_symname 
				<< " so skipping wrapper." << std::endl;
			return;
		}

		// 1. emit wrapper prototype
		/* We take *any* source module, and assume that its call-site is
		 * definitive. This *should* give us args and return type, but may not.
		 * If our caller's DWARF doesn't specify arguments and return type,
		 * we guess from the pattern AST. If the pattern AST doesn't include
		 * arguments, we just emit "..." and an int return type.
		 */
		std::cerr << "Building wrapper for symbol " << wrapped_symname << std::endl;

		// get the subprogram description of the wrapped function in an arbitrary source module
		auto subp = 
			corresps.at(0)->second.source->all_compile_units() 
				->visible_named_child(wrapped_symname);
		if (!subp) 
		{
			std::cerr << "Bailing from wrapper generation -- no subprogram!" << std::endl; 
			return; 
		}

		// convenience reference alias of subprogram
		auto callsite_signature = boost::dynamic_pointer_cast<dwarf::spec::subprogram_die>(subp);
		
		// if we have a unique sink action, and it's a simple function call,
		// we can use it as an extra source of description for the call arguments.
		boost::shared_ptr<dwarf::spec::subprogram_die> unique_called_subprogram;
		if (corresps.size() == 1
			&& sink_expr_is_simple_function_name(corresps.at(0)->second.sink_expr))
		{
			std::string called_name = *sink_expr_is_simple_function_name(corresps.at(0)->second.sink_expr);
			std::vector<std::string> called_name_vec(1, called_name);
			unique_called_subprogram 
			 = boost::dynamic_pointer_cast<dwarf::spec::subprogram_die>(
			 		corresps.at(0)->second.sink->get_ds().toplevel()->visible_resolve(
			 			called_name_vec.begin(), called_name_vec.end()
					)
				);
		} else unique_called_subprogram = boost::shared_ptr<dwarf::spec::subprogram_die>();

		// output prototype for __real_
		m_out << "extern \"C\" { " << std::endl;
		m_out << "extern ";
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__real_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, module_of_die(callsite_signature), true, unique_called_subprogram);
		m_out << " __attribute__((weak));" << std::endl;
		// output prototype for __wrap_
		m_out << "namespace cake { namespace " << m_d.namespace_name() << " {" << std::endl;
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__wrap_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, module_of_die(callsite_signature), true, unique_called_subprogram);
		m_out << ';' << std::endl;
		m_out << "} } // end cake and link namespaces" << std::endl;
		m_out << "} // end extern \"C\"" << std::endl;
		// output wrapper -- put it in the link block's namespace, so it can see definitions etc.
		m_out << "namespace cake { namespace " << m_d.namespace_name() << " {" << std::endl;
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__wrap_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, module_of_die(callsite_signature), true, unique_called_subprogram);
		m_out << std::endl;
		m_out << " {";
		m_out.inc_level();
		m_out << std::endl;
		
		/* NOTE: by lumping together call-sites from diverse components which happen to share
		 * the same symbol name, all under one wrapper, we are inadvertently requiring that
		 * they also have identical argument lists (signatures). We can live with this for now.
		 * We could either make every wrapper a varargs function, and output some code to scrape
		 * out the correct arguments once we've demuxed by caller, OR (better) do a pre-pass 
		 * which renames all undefined symbols in a given component to make them unique. Then we
		 * can wrap them one component at a time, and make the signature explicit without 
		 * requiring other components to share it.
		 */

		// 3. emit wrapper definition -- this is a sequence of "if" statements
		for (link_derivation::ev_corresp_pair_ptr_list::iterator i_pcorresp = corresps.begin();
				i_pcorresp != corresps.end(); ++i_pcorresp)
		{
			antlr::tree::Tree *pattern = (*i_pcorresp)->second.source_pattern;
			antlr::tree::Tree *action = (*i_pcorresp)->second.sink_expr;
			antlr::tree::Tree *return_leg = (*i_pcorresp)->second.return_leg;
			antlr::tree::Tree *source_infix_stub = (*i_pcorresp)->second.source_infix_stub;
			antlr::tree::Tree *sink_infix_stub = (*i_pcorresp)->second.sink_infix_stub;
			module_ptr source = (*i_pcorresp)->second.source;
			module_ptr sink = (*i_pcorresp)->second.sink;

			assert(GET_TYPE(action) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
			antlr::tree::Tree *stub = GET_CHILD(action, 0);

			// create a code generation context
			vector<antlr::tree::Tree *> exprs = { sink_infix_stub, stub };
			context ctxt(*this, 
				source, sink,
				initial_environment(pattern, source, exprs, sink));
			ctxt.opt_source = (context::source_info_s){ callsite_signature, 0 };
			
			// emit a condition
			m_out << "if (";
			write_pattern_condition(ctxt, pattern); // emit if-test
				
			m_out << ")" << std::endl;
			m_out << "{";
			m_out.inc_level();
			m_out << std::endl;
			
			// our context now has a pattern too
			ctxt.opt_source->opt_pattern = pattern;
			
			// here goes the pre-arrow infix stub
			auto status1 = 
				(source_infix_stub && GET_CHILD_COUNT(source_infix_stub) > 0) ?
					emit_stub_expression_as_statement_list(
						ctxt,
						source_infix_stub/*,
						shared_ptr<type_die>()*/) // FIXME: really no type expectations here?
				: (post_emit_status){NO_VALUE, "true", environment()};
			// now update the environment to add new let-bindings and "it"
			// FIXME: handle failure of the infix stub here
			auto saved_env1 = ctxt.env;
			auto new_env1 = merge_environment(ctxt.env, status1.new_bindings);
			if (status1.result_fragment != NO_VALUE) new_env1["__cake_it"] = (bound_var_info) {
				status1.result_fragment,
				status1.result_fragment,  //shared_ptr<type_die>(),
				ctxt.modules.source,
				false
			};
				

			// crossover point
			ctxt.modules.current = ctxt.modules.sink;
			m_out << "// source->sink crossover point" << std::endl;
			std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> > constraints;
			if (sink_infix_stub) m_d.find_type_expectations_in_stub(
				sink, sink_infix_stub, boost::shared_ptr<dwarf::spec::type_die>(), constraints);
			m_d.find_type_expectations_in_stub(
				sink, action, boost::shared_ptr<dwarf::spec::type_die>(), constraints);
			ctxt.env = crossover_environment_and_sync(source, new_env1, sink, constraints, true);

			// FIXME: here goes the post-arrow infix stub
			auto status2 = 
				(sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub) > 0) ?
					emit_stub_expression_as_statement_list(
						ctxt,
						sink_infix_stub
					) // FIXME: really no type expectations here?
					: (post_emit_status){NO_VALUE, "true", environment()};
			auto saved_env2 = ctxt.env;
			auto new_env2 = merge_environment(ctxt.env, status2.new_bindings);
			if (status2.result_fragment != NO_VALUE) new_env2["__cake_it"] = (bound_var_info) {
				status2.result_fragment,
				status2.result_fragment, //shared_ptr<type_die>(),
				ctxt.modules.sink,
				false
			};
			
			// emit sink action, *NOT* including return statement
			std::cerr << "Processing stub action: " << CCP(TO_STRING_TREE(action)) << std::endl;
			post_emit_status status3;

			std::cerr << "Emitting event correspondence stub: "
				<< CCP(TO_STRING_TREE(stub)) << std::endl;
			m_out << "// " << CCP(TO_STRING_TREE(stub)) << std::endl;

			// the sink action is defined by a stub, so evaluate that 
			std::cerr << "Event sink stub is: " << CCP(TO_STRING_TREE(stub)) << std::endl;
			assert(GET_TYPE(stub) == CAKE_TOKEN(INVOKE_WITH_ARGS));
			status3 = emit_stub_expression_as_statement_list(
				ctxt, stub/*, cxx_expected_type*/);

			// update environment
			auto new_env3 = merge_environment(ctxt.env, status3.new_bindings);
			if (status3.result_fragment != NO_VALUE) new_env3["__cake_it"] = (bound_var_info) {
				status3.result_fragment,
				status3.result_fragment, //shared_ptr<type_die>(),
				ctxt.modules.sink,
				false,
			};
			
			// crossover point
			m_out << "// source<-sink crossover point" << std::endl;
			// update environment
			std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> > 
				return_constraints;
			if (return_leg && GET_CHILD_COUNT(return_leg) > 0)
			{
				/* Any functions called during the return leg
				 * on things in the env 
				 * may bring type expectations
				 * that we should account for now, when converting. */
				m_d.find_type_expectations_in_stub(
					source, return_leg, boost::shared_ptr<dwarf::spec::type_die>(), 
					return_constraints);
			}
			else
			{	
				/* If there's no return leg, we're generating the return value *now*. */
				if (!subprogram_returns_void(ctxt.opt_source->signature))
				{
					m_out << "// generating return value here, constrained to type "
						<< compiler.name_for(ctxt.opt_source->signature->get_type())
						<< std::endl;
					return_constraints.insert(std::make_pair("__cake_it",
						ctxt.opt_source->signature->get_type()));
				}
				else
				{
					m_out << "// crossover logic thinks there's no return value" << std::endl;
				}
			}

			ctxt.modules.current = ctxt.modules.source;
			ctxt.env = crossover_environment_and_sync(sink, new_env3, source, return_constraints, false);
			
			std::string final_success_fragment = status3.success_fragment;
			
			m_out << "// begin return leg of rule" << std::endl;
			// emit the return leg, if there is one; otherwise, status is the old status
			if (return_leg && GET_CHILD_COUNT(return_leg) > 0)
			{
				auto status4 = 
						emit_stub_expression_as_statement_list(
						ctxt,
						return_leg);
				auto new_env4 = merge_environment(ctxt.env, status4.new_bindings);
				// update environment -- just "it"
				if (status4.result_fragment != NO_VALUE) 
				{
					new_env4["__cake_it"] = (bound_var_info) {
						status4.result_fragment,
						status4.result_fragment, //shared_ptr<type_die>(),
						ctxt.modules.source,
						false
					};
				}
				ctxt.env = new_env4;
				final_success_fragment = status4.success_fragment;
			}
			m_out << "// end return leg of rule" << std::endl;
			
			// emit the return statement
			m_out << "// begin return logic" << std::endl;
			if (!subprogram_returns_void(ctxt.opt_source->signature)) 
			{
				/* Main problem here is that we don't know the C++ type of the
				 * stub's result value. So we have to make value_convert available
				 * as a function template here. Hopefully this will work. */
				 
				if (ctxt.env.find("__cake_it") == ctxt.env.end())
				{
					RAISE(ctxt.opt_source->opt_pattern, 
						"cannot synthesise a return value out of no value (void)");
				}

				// now return from the wrapper as appropriate for the stub's exit status
				m_out << "if (" << final_success_fragment << ") return "
					<< ctxt.env["__cake_it"].cxx_name;

				m_out << ";" << std::endl;

				m_out << "else return __cake_failure_with_type_of(" 
					<< ctxt.env["__cake_it"].cxx_name << ");" << std::endl;
				//m_out << "return "; // don't dangle return
			}
			else
			{
				// but for safety, to avoid fall-through in the wrapper
				m_out << "return;" << std::endl; 
			}

			/* The sink action should leave us with a return value (if any)
			 * and a success state / error value. We then encode these and
			 * output the return statement *here*. */
			m_out << "// end return logic" << std::endl;
			m_out.dec_level();
			m_out << "}" << std::endl;
		}
		
		// 4. if none of the wrapper conditions was matched 
		m_out << "else ";
		m_out << "return ";
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__real_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, module_of_die(callsite_signature), false, unique_called_subprogram);
		m_out << ';' << std::endl; // end of return statement
		m_out.dec_level();
		m_out << '}' << std::endl; // end of block
		m_out << "} } // end link and cake namespaces" << std::endl;
	}

	wrapper_file::environment 
	wrapper_file::crossover_environment_and_sync(
		module_ptr old_module,
		const environment& env,
		module_ptr new_module,
		const std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> >& constraints,
		bool outward,
		bool do_not_sync /* = false */
		)
	{
		/* Here we:
		 * - emit a bunch of code generating new cxx names for each binding;
		 * - this code also applies value conversions;
		 * - we resolve immediate aliasing, since we may have multiple bindings
		 *   for the same C++ variable;
		 * - we don't (FIXME) do anything about indirect aliasing yet. This is
		 *   where different C++ variables reach the same object(s).
		 *   We only want to apply a given value correspondence
		 *   (update or initialization) once per object per crossing.
		 *   Ultimately this means giving each crossing a timestamp,
		 *   and remembering, for any co-object,
		 *   at which timestamp it was last updated.
		 *   The update_co_objects pass, which happens after us, can then avoid
		 *   re-running any rules which we have only just applied here. 
		 *   This is the "object graph semantics" in the thesis: these semantics
		 *   apply for any given crossover point (although the thesis doesn't
		 *   use the phrase "crossover point", sadly). */
		 
		/* NOTE: 
		 * Making the crossover means doing value correspondence selection.
		 * The type that we select for a local in the new context should
		 * depend on annotations, from the following sources:
		 * * type annotations, respecting typedefs, in the (unique) *source* of the value;
		 * * explicit "as" annotations in the stub code
		 * * type annotations, respecting typedefs, in the (possibly many) *sinks* of the value.
		 *
		 * The many sinks must all be consistent.
		 * If the value is a pointer, there's an interesting question about the 
		 * reached objects: does the pointer's static type imply constraints on
		 * what these objects' corresponding types should be? Answer: no! Cake's
		 * dynamic semantics mean that we don't use the static type of the pointer
		 * directly. Currently we use it indirectly in the dynamic points-to
		 * analysis. */
		
		// If I get all this working, what will happen in my foobarbaz case?
		// We'll determine that the crossed-over arg needs to be an "int",
		// hence avoiding use of __typeof,
		// and just invoking value_convert directly from wordsize_type to int
		// -- which will work because of the default conversions we added
		// in the prelude.
		// We don't need a nonzero RuleTag -- this only happens when we have
		// typedefs.
		
		// SO -- do x86-64 "int" arguments get promoted to 64-bit width?
		// NO -- they stay at 32-bit width! BUT pointers are 64-bit width. Argh.

		environment new_env;
		
		// for deduplicating multiple Cake aliases of the same cxxname...
		std::map<std::string, std::set<std::string > > bindings_by_cxxname;
		for (auto i_binding = env.begin(); i_binding != env.end(); ++i_binding)
		{
			bindings_by_cxxname[i_binding->second.cxx_name].insert(i_binding->first);
		}

		// if inward, do the sync here
		if (!outward && !do_not_sync)
		{
			m_out << "sync_all_co_objects(REP_ID(" << m_d.name_of_module(old_module)
				<< "), REP_ID(" << m_d.name_of_module(new_module) << "), ";
			for (auto i_cxxname = bindings_by_cxxname.begin(); 
				i_cxxname != bindings_by_cxxname.end(); ++i_cxxname)
			{
				// now we have a group of bindings
				assert(i_cxxname->second.begin() != i_cxxname->second.end());
				environment::iterator i_binding = new_env.end();;
				// search for the corresponding bound name to this cxxname
				for (auto i_cakename = i_cxxname->second.begin();
					i_cakename != i_cxxname->second.end();
					++i_cakename)
				{
					auto found = new_env.find(*i_cakename);
					if (found != new_env.end()) 
					{
						i_binding = found;
						break;
					}
					else cerr << "cakename " << *i_cakename 
						<< " of cxxname " << i_cxxname->first 
						<< " not found in the new environment" << endl;
				}
				// some Cake names, like "__cake_it", will not be in the new env? hm.
				if (i_binding == new_env.end()) continue;
				
				if (i_binding->second.indirect_local_tagstring_out
					|| i_binding->second.indirect_remote_tagstring_out)
				{
					m_out << "// override for binding " << i_binding->first << endl;
					// NOTE that whether ther actually *is* an effective override
					// depends on whether the rule tag selected by the two tagstrings
					// actually is different from the default -- and a similar for init
					auto funcname = make_value_conversion_funcname(
							link_derivation::sorted(make_pair(old_module, new_module)),
							*i_binding,
							true,
							shared_ptr<spec::type_die>(),
							shared_ptr<spec::type_die>(),
							"*" + i_binding->first, // NOTE: relies on static typing!
							optional<string>(),
							old_module,
							new_module);
					m_out << i_binding->second.cxx_name << ", " << funcname << ", " << funcname << ", ";
					// NOTE that whether ther actually *is* an effective override
					// depends on whether the rule tag selected by the two tagstrings
					// actually is different from the default -- and a similar for init
				}
			}
			m_out << "NULL, NULL, NULL);" << std::endl;
		}
		
		// for each unique cxxname...
		for (auto i_cxxname = bindings_by_cxxname.begin(); 
			i_cxxname != bindings_by_cxxname.end();
			++i_cxxname)
		{
			/* In this loop we are going to:
			 * - emit a new cxx variable that is the crossed-over old one; 
			 * - emit a call to allocate co-objects, in the case of pointers
			 * - initialise it using the type constraints */
		
			// assert -- at least one binding exists
			assert(i_cxxname->second.begin() != i_cxxname->second.end());
			
			auto i_first_binding = env.find(*i_cxxname->second.begin());
			assert(i_first_binding != env.end());
		
			// sanity check -- for all bindings covered by this cxx name,
			// check that they are valid in the module context we're crossing over *from*
			// also, skip if they're all marked no-crossover
			bool no_crossover = true;
			for (auto i_binding_name = bindings_by_cxxname[i_cxxname->first].begin();
				i_binding_name != bindings_by_cxxname[i_cxxname->first].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());
				no_crossover &= i_binding->second.do_not_crossover;
				assert(i_binding->second.valid_in_module == old_module);
			}
			if (no_crossover) continue;
			
			// create a new cxx ident for the binding
			auto ident = new_ident("xover_" + i_first_binding->first);
			
			// collect constraints, over all aliases
			std::set<boost::shared_ptr<dwarf::spec::type_die> > all_constraints;
			for (auto i_binding_name = bindings_by_cxxname[i_cxxname->first].begin();
				i_binding_name != bindings_by_cxxname[i_cxxname->first].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());

				auto constraint_iters = constraints.equal_range(i_binding->first);
				for (auto i_constraint = constraint_iters.first;
					i_constraint != constraint_iters.second;
					++i_constraint)
				{
					all_constraints.insert(i_constraint->second);
				}
			}

			/* Work out the target type expectations, using constraints */
			boost::shared_ptr<dwarf::spec::type_die> precise_to_type;
			
			// get the constraints defined for this Cake name
			//auto iter_pair = constraints.equal_range(i_binding->first);
			for (auto i_type = all_constraints.begin(); i_type != all_constraints.end(); ++i_type)
			{
				/* There are a few cases here. 
				 * - all identical, all concrete: no problem
				 * - all identical, all *the same* artificial: no problem
				 * - different, all *concretely* the same but one artificial type:
				 *   go with the artificial.
				 *
				 * FIXME: for now, we don't handle the artificial cases. */
				if (precise_to_type)
				{
					if (precise_to_type->iterator_here() == (*i_type)->iterator_here())
					{
						// okay, same DIE, so continue
						continue;
					}
					else
					{
						assert(false);
					}
				}
				else 
				{	
					m_out << "// in crossover environment, " << i_first_binding->first 
						<< " has been constrained to type " 
						<< compiler.name_for(*i_type)
						<< std::endl;
					precise_to_type = *i_type;
				}
			}

			// collect pointerness
			bool is_a_pointer = false;
			for (auto i_binding_name = bindings_by_cxxname[i_cxxname->first].begin();
				i_binding_name != bindings_by_cxxname[i_cxxname->first].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());
				
				is_a_pointer |= (precise_to_type &&
					 precise_to_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type);
			}
			
			// collect typeof
			boost::optional<std::string> collected_cxx_typeof;
			for (auto i_binding_name = bindings_by_cxxname[i_cxxname->first].begin();
				i_binding_name != bindings_by_cxxname[i_cxxname->first].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());
				
				if (!collected_cxx_typeof) collected_cxx_typeof = i_binding->second.cxx_typeof;
				else assert(*collected_cxx_typeof == i_binding->second.cxx_typeof);
			}
			
			// output co-objects allocation call, if we have a pointer
			if (is_a_pointer)
			{
				m_out << "ensure_co_objects_allocated(REP_ID("
					<< m_d.name_of_module(old_module) << "), ";
				// make sure we invoke the pointer specialization
				/*if (is_a_pointer_this_time)*/ m_out <<
					"ensure_is_a_pointer(";
				m_out << i_cxxname->first; 
				/*if (is_a_pointer_this_time)*/ m_out << ")";
				m_out << ", REP_ID("
					<< m_d.name_of_module(new_module)
					<<  "));" << std::endl;
			}
// 			/* Now insert code to ensure co-objects are allocated. We might pass
// 			 * over the same cxxname multiple times. If we've seen it before,
// 			 * we *might* still be interested -- if it wasn't a pointer then, but
// 			 * is now. This is for unspecified_wordsize_type et al. */
// 			auto found = seen_cxxnames.find(i_binding->second.cxx_name);
// 			if (found == seen_cxxnames.end()
// 			 || found->second == false)
// 			{
// 				bool was_a_pointer_last_time = 
// 					found != seen_cxxnames.end() && found->second;
// 				bool is_a_pointer_this_time = 
// 					(precise_to_type &&
// 					 precise_to_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type);
// 				seen_cxxnames[i_binding->second.cxx_name] =
// 					was_a_pointer_last_time || is_a_pointer_this_time;
// 				
// 				// if this is the first time we've seen this cxxname,
// 				// or if it's now a pointer, 
// 				// we make the co-objects alloc call
// 				if (found == seen_cxxnames.end() || (
// 					!was_a_pointer_last_time && is_a_pointer_this_time))
// 				{
// 					m_out << "ensure_co_objects_allocated(REP_ID("
// 						<< m_d.name_of_module(old_module) << "), ";
// 					// make sure we invoke the pointer specialization
// 					if (is_a_pointer_this_time) m_out <<
// 						"ensure_is_a_pointer(";
// 					m_out << i_binding->second.cxx_name;
// 					if (is_a_pointer_this_time) m_out << ")";
// 					m_out << ", REP_ID("
// 						<< m_d.name_of_module(new_module)
// 						<<  "));" << std::endl;
// 				}
// 			}
			
			
			// output its initialization
			m_out << "auto " << ident << " = ";
			open_value_conversion(
				link_derivation::sorted(new_module, i_first_binding->second.valid_in_module),
				//rule_tag, // defaults to 0, but may have been set above
				*i_first_binding, // from_artificial_tagstring is in our binding -- easy
				false,
				boost::shared_ptr<dwarf::spec::type_die>(), // no precise from type
				precise_to_type, // defaults to "no precise to type", but may have been set above
				(is_a_pointer ? std::string("((void*)0)") : *collected_cxx_typeof), // from typeof
				boost::optional<std::string>(), // NO precise to typeof, 
				   // BUT maybe we could start threading a context-demanded type through? 
				   // It's not clear how we'd get this here -- scan future uses of each xover'd binding?
				   // i.e. we only get it *later* when we try to emit some stub logic that uses this binding
				i_first_binding->second.valid_in_module, // from_module is in our binding
				new_module
			);
			
			// -- when we create a new binding, we set its artificial name to
			// that determined by its context of use
			// -- we also record its crossover artificial name as the original artificial name...
			// ... BUT then we can do things differently if we have out_as and in_as.
			
			// looking for contexts of use:
			// it depends on what kind of correspondence we're in. 
			// 
			// in the reverse direction:
			// 
			
			/* Specifying value conversions:
			 * we have a source typeof (implicit in the argument), 
			 * a target module, perhaps strengthened by a contextual typeof requirement
			 * (i.e. could be "auto", or could be a field type)
			 * maybe a rule ID 
			 * (capturing the information lost in erasing typedefs from source/target types).
			 */
			 
			/* NOTE: at some point we are going to have to start caring about
			 * typedefs versus their concrete data types,
			 * and use this distinction to choose the appropriate RuleTag. 
			 * Can we get back from a typeof string to a typedef? Not easily. 
			 * We'll have to propagate the RuleIds from wherever typedefs
			 * or artificial data types creep in: 
			 * in function arguments,
			 * in "as" expressions,
			 * in field accesses
			 * -- we can't just pass the names of these things to __typeof,
			 * because that will only select the concrete type (cxx compiler doesn't distinguish).
			 * Instead, we need to generate both the __typeof *and* the RuleTag
			 * at the same time. This might be a problem when we do propagation of
			 * unique_corresponding_type... although val corresps may be expressed
			 * in terms of typedefs, so perhaps not. */
			if (is_a_pointer) m_out << "ensure_is_a_pointer(";
			m_out <<i_cxxname->first;
			if (is_a_pointer) m_out << ")";
			close_value_conversion();
			m_out << ";" << std::endl;
			
			// for each Cake name, add it to the new environment
			for (auto i_cakename = bindings_by_cxxname[i_cxxname->first].begin();
					i_cakename != bindings_by_cxxname[i_cxxname->first].end();
					++i_cakename)
			{
				new_env[*i_cakename] = (bound_var_info) {
					ident,
					ident,
					new_module,
					false,
					// we swap over the local and remote tagstrings
					(assert(env.find(*i_cakename) != env.end()), env[*i_cakename].remote_tagstring),
					(assert(env.find(*i_cakename) != env.end()), env[*i_cakename].local_tagstring),
					(assert(env.find(*i_cakename) != env.end()), env[*i_cakename].indirect_remote_tagstring_in),
					(assert(env.find(*i_cakename) != env.end()), env[*i_cakename].indirect_local_tagstring_in),
					(assert(env.find(*i_cakename) != env.end()), env[*i_cakename].indirect_remote_tagstring_out),
					(assert(env.find(*i_cakename) != env.end()), env[*i_cakename].indirect_local_tagstring_out),
				};
			}
		}
		
		// output a summary comment
		m_out << "/* crossover: " << std::endl;
		for (auto i_el = new_env.begin(); i_el != new_env.end(); ++i_el)
		{
			m_out << "\t" << i_el->first << " is now " << i_el->second.cxx_name 
				<< " (local: " << i_el->second.local_tagstring
				<< ", remote: " << i_el->second.remote_tagstring
				<< ")" << std::endl;
		}
		m_out << "*/" << std::endl;
		
		// do the crossover here
		if (outward && !do_not_sync)
		{
			m_out << "sync_all_co_objects(REP_ID(" << m_d.name_of_module(old_module)
				<< "), REP_ID(" << m_d.name_of_module(new_module) << "), ";
			// for each binding that is being crossed over, and that has an indirect tagstring,
			// we add it to the list
			// NOTE: we use the *old* env!
			for (auto i_cxxname = bindings_by_cxxname.begin(); 
				i_cxxname != bindings_by_cxxname.end(); ++i_cxxname)
			{
				// now we have a group of bindings
				// HACK: just look at the first one, for now
				assert(i_cxxname->second.begin() != i_cxxname->second.end());
				auto i_binding = env.find(*i_cxxname->second.begin());
				assert(i_binding != env.end());
				
				if (i_binding->second.indirect_local_tagstring_out
					|| i_binding->second.indirect_remote_tagstring_out)
				{
					m_out << "// override for binding " << i_binding->first << endl;
					// NOTE that whether ther actually *is* an effective override
					// depends on whether the rule tag selected by the two tagstrings
					// actually is different from the default -- and a similar for init
					auto funcname = make_value_conversion_funcname(
							link_derivation::sorted(make_pair(old_module, new_module)),
							*i_binding,
							true,
							shared_ptr<spec::type_die>(),
							shared_ptr<spec::type_die>(),
							"*" + i_binding->first, // NOTE: requires precise static typing
							optional<string>(),
							old_module,
							new_module);
					m_out << i_binding->second.cxx_name << ", " << funcname << ", " << funcname << ", ";
				}
			}
			m_out << "NULL, NULL, NULL);" << std::endl;
		}
		
		return new_env;
	}

	wrapper_file::environment 
	wrapper_file::merge_environment(
		const environment& env,
		const environment& new_bindings
		)
	{
		environment new_env = env;
	
		// sanity check: all elements should be valid in the same module
		module_ptr seen_module = 
			(env.size() > 0)  ? env.begin()->second.valid_in_module : module_ptr();
		
		for (auto i_new = new_bindings.begin();
				i_new != new_bindings.end();
				++i_new)
		{
			if (seen_module)
			{
				assert(i_new->second.valid_in_module == seen_module);
			}
			else seen_module = i_new->second.valid_in_module;
			
			if (new_env.find(i_new->first) != new_env.end())
			{
				m_out << "// warning: merging environment hides old binding of " 
					<< i_new->first << std::endl;
			}
			new_env.insert(*i_new);
		}
		
		return new_env;
	}

	wrapper_file::environment 
	wrapper_file::initial_environment(
		antlr::tree::Tree *pattern,
		module_ptr source_module,
		const std::vector<antlr::tree::Tree *>& exprs,
		module_ptr remote_module
		)
	{
		environment env, *out_env = &env;
		
		/* We need to generate a new wrapper_sig that actually
		 * contains information about the arguments.
		 * Recall: this is to solve the problem where our
		 * pass(gizmo ptr) --> pass(gadget ptr)
		 * wrapper doesn't know that what's being passed is a gizmo. 
		 * So it can't insert the conversion.
		 * Previously we have just been taking our lead from 
		 * the event pattern, and treating everything as an
		 * unspecified_wordsize_type. 
		 * This is okay in some cases
		 * - where we have an event pattern
		 * - where prelude-supplied conversions on unspecified_wordsize_type
		 *     will do the right thing
		 * but not otherwise:
		 * - where we don't have an event pattern (bindings formed by name-matching)
		 * - where prelude-supplied conversions aren't appropriate.
		 * Effectively we are doing a very primitive form of 
		 * (imprecise) type inference.
		 * The killer is the absence of an event pattern -- without this, we have
		 * no idea how many argument words to read off the stack.
		 * So we instead make use of the sink pattern
		 * and of uniqueness within the available value correspondences. */
		 
		/* To test this, I need a quick short-cut hack that I can
		 * remove later, which supplies the information that "g" is
		 * a "gadget". How to do this?
		 * 1. Only address trivial function-for-function mappings.
		 * 2. Assume therefore that the #arguments are equal.
		 * 3. Require that the arguments are all wordsize (can *check* in the callee)
		 * 4. Pull exactly this many off the stack.
		 * 5. Require that
		 
		 * NOTE that this will result in bogus compositions
		 * in cases where a like-named function is name-matched across two modules
		 * but where the argument lists of these differ in length. */
		 
		/* Can we dynamically determine the number of arguments passed
		 * on the stack? Perhaps by subtracting the stack pointer from
		 * the frame pointer? I don't think this works.
		 * Recall the varargs protocol:
		 * arguments are pushed from right to left 
		 * so that leftmore arguments end up *nearest* the frame pointer
		 * and the rightmore arguments just keep on going deeper into the
		 * calling frame. 
		 * We *could* use analysis of the previous frame's locals
		 * to infer which words are definitely *either* pushed 
		 * *or* working temporaries. This would risk "passing" extra arguments
		 * but then these might never be used. Can argue this is harmless? Hmm.
		 * Not if it can introduce e.g. random divide-by-zero errors by running
		 * inappropriate conversion logic. */

		//m_out << "true"; //CCP(GET_TEXT(corresp_pair.second.source_pattern));
		// for each position in the pattern, emit a test
		bool emitted = false;
		INIT;
		ALIAS3(pattern, eventPattern, EVENT_PATTERN);
		{
			INIT;
			BIND2(pattern, eventContext);
			BIND2(pattern, memberNameExpr);
			BIND3(pattern, eventCountPredicate, EVENT_COUNT_PREDICATE);
			BIND3(pattern, eventParameterNamesAnnotation, KEYWORD_NAMES);
			definite_member_name call_mn = read_definite_member_name(memberNameExpr);
			if (call_mn.size() != 1) RAISE(memberNameExpr, "may not be compound");			
			auto caller = source_module->get_ds().toplevel()->visible_resolve(
				call_mn.begin(), call_mn.end());
			if (!caller) RAISE(memberNameExpr, "does not name a visible function");
			if ((*caller).get_tag() != DW_TAG_subprogram) 
				RAISE(memberNameExpr, "does not name a visible function"); 
			auto caller_subprogram = boost::dynamic_pointer_cast<subprogram_die>(caller);
			
			int argnum = 0;
			auto i_caller_arg = caller_subprogram->formal_parameter_children_begin();
			//int dummycount = 0;
			FOR_REMAINING_CHILDREN(eventPattern)
			{
				//boost::shared_ptr<dwarf::spec::type_die> p_arg_type = boost::shared_ptr<dwarf::spec::type_die>();
				boost::shared_ptr<dwarf::spec::program_element_die> p_arg_origin;
				
				if (i_caller_arg == caller_subprogram->formal_parameter_children_end())
				{
					if (caller_subprogram->unspecified_parameters_children_begin() !=
						caller_subprogram->unspecified_parameters_children_end())
					{
						p_arg_origin = *caller_subprogram->unspecified_parameters_children_begin();
					}
					else RAISE(eventPattern, "too many arguments for function");
				}
				else
				{
					//p_arg_type = *(*i_caller_arg)->get_type();
					p_arg_origin = boost::dynamic_pointer_cast<dwarf::spec::program_element_die>(
						(*i_caller_arg)->get_this());
				}
				auto origin_as_fp = boost::dynamic_pointer_cast<dwarf::spec::formal_parameter_die>(
						p_arg_origin);
				auto origin_as_unspec = boost::dynamic_pointer_cast<dwarf::spec::unspecified_parameters_die>(
						p_arg_origin);
				assert(origin_as_fp || origin_as_unspec || (std::cerr << *p_arg_origin, false));

				ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
				{
					INIT;
					BIND2(annotatedValuePattern, valuePattern);
					
					/* No matter what sort of value pattern we find, we generate at
					 * least one binding for the argument of the wrapper function. If the
					 * pattern doesn't provide a friendly name, only the default
					 * __cake_arg name will be generated. Otherwise, *two* names will be
					 * generated. */
					boost::optional<std::string> friendly_name;
					std::ostringstream s; s << wrapper_arg_name_prefix << argnum;
					std::string basic_name = s.str();
					optional<string> local_tagstring;
					optional<string> indirect_local_tagstring_in;
					optional<string> indirect_local_tagstring_out;
					optional<string> indirect_remote_tagstring_in;
					optional<string> indirect_remote_tagstring_out;
					switch(GET_TYPE(valuePattern))
					{
						case CAKE_TOKEN(DEFINITE_MEMBER_NAME): assert(false);
						case CAKE_TOKEN(NAME_AND_INTERPRETATION): {
							// could match anything, so bind name and continue
							INIT;
							BIND3(valuePattern, definiteMemberName, DEFINITE_MEMBER_NAME);
							definite_member_name mn = read_definite_member_name(definiteMemberName);
							if (mn.size() != 1) RAISE(definiteMemberName, "may not be compound");
							friendly_name = mn.at(0);
							bool refers_to_pointer
							 = (*i_caller_arg)->get_type() && 
							   (*i_caller_arg)->get_type()->get_concrete_type()->get_tag() 
							     == DW_TAG_pointer_type;
							shared_ptr<type_die> interpretation_target_type;
							if (GET_CHILD_COUNT(valuePattern) > 1)
							{
								BIND2(valuePattern, interpretation); assert(interpretation);
								interpretation_target_type = source_module->ensure_dwarf_type(
									GET_CHILD(interpretation, 0));
								bool use_artificial;
								if (interpretation_target_type->get_unqualified_type()
									== interpretation_target_type->get_concrete_type())
								{
									// this means it's a NOT a typedef
									use_artificial = false;
								} else use_artificial = true;
								switch (GET_TYPE(interpretation))
								{
									/* We want to resolve the named type, and be sensitive
									 * to whether the pattern is referring to a pointer or
									 * not. */
									case CAKE_TOKEN(KEYWORD_AS):
									case CAKE_TOKEN(KEYWORD_INTERPRET_AS):  {
										assert(interpretation_target_type);
										// we only set the tagstrings if the interpretation
										// is directing us to a non-concrete type
										// (cf. just an annotation)

										// ... even then, if there is no special behaviour
										// defined for this typedef, we should not generate
										// any different behaviour (but will still use the
										// tagstring, and hope it's equivalent to __cake_default).
										
										// if this created a pointer type,
										// and we were not declared as a pointer,
										// it means the parameter really is a pointer
										// and we should use the indirect string
										if (interpretation_target_type->get_concrete_type()->get_tag()
										     == DW_TAG_pointer_type)
										{
											if (!refers_to_pointer) refers_to_pointer = true;
											auto pointed_to_type = 
												dynamic_pointer_cast<pointer_type_die>(
													interpretation_target_type->get_concrete_type()
													)->get_type();
											assert(pointed_to_type);
											auto pointed_to_typename = pointed_to_type->get_name();
											assert(pointed_to_typename);
											// we only set the tagstrings if the interpretation
											// is directing us to a non-concrete type. 
											if (use_artificial) indirect_local_tagstring_in = 
												indirect_local_tagstring_out = 
													*pointed_to_typename;
										}
										else if (use_artificial) local_tagstring
										 = *interpretation_target_type->get_name();
									} break;
									case CAKE_TOKEN(KEYWORD_IN_AS):
										assert(interpretation_target_type->get_concrete_type()->get_tag()
											== DW_TAG_pointer_type);
										assert(interpretation_target_type->get_name());
										if (use_artificial)
										{ 
											indirect_local_tagstring_in
											 = *interpretation_target_type->get_name(); 
										}
										break;
									case CAKE_TOKEN(KEYWORD_OUT_AS):
										assert(interpretation_target_type->get_concrete_type()->get_tag()
											== DW_TAG_pointer_type);
										assert(interpretation_target_type->get_name());
										if (use_artificial) 
										{
											indirect_local_tagstring_out
											 = *interpretation_target_type->get_name();
										}
										break;
									default: assert(false);
								}
							}
						} break;
						case CAKE_TOKEN(KEYWORD_CONST):
						case CAKE_TOKEN(INDEFINITE_MEMBER_NAME): {
							// we will bind a basic name but not a friendly one
						} break;
						default: RAISE(valuePattern, "unexpected token");
					}
					
					// check for typedefs
					if (i_caller_arg != caller_subprogram->formal_parameter_children_end()
						&& (*i_caller_arg)->get_type()
						&& (*i_caller_arg)->get_type()->get_concrete_type() != (*i_caller_arg)->get_type())
					{
						auto unqual_t = (*i_caller_arg)->get_type()->get_unqualified_type();
						assert(unqual_t->get_tag() == DW_TAG_typedef);
						if (local_tagstring)
						{
							cerr << "Warning: artificial typename " << local_tagstring
								<< " overrides typedef " 
								<< *dynamic_pointer_cast<typedef_die>(unqual_t)->get_name()
								<< endl;
						}
						else
						{
							auto tdef = dynamic_pointer_cast<typedef_die>(unqual_t);
							assert(tdef);
							auto opt_name = tdef->get_name();
							if (!opt_name) RAISE(valuePattern, "unnamed typedef");
							local_tagstring = *opt_name;
						}
					}
					
					optional<string> remote_tagstring;
					/** To get the remote artificial string, we scan for uses of 
					 *  the bound name (Cake name) in the right-hand side. */
					vector<antlr::tree::Tree *> out;
					for (auto i_expr = exprs.begin(); i_expr != exprs.end(); ++i_expr)
					{
						m_d.find_usage_contexts(basic_name,
							*i_expr, out);
						if (friendly_name) m_d.find_usage_contexts(*friendly_name,
							*i_expr, out);
					}
					cerr << "found " << out.size() << " usage contexts of Cake names {"
						<< basic_name << (friendly_name ? (", " + *friendly_name) : "")
						<< "}" << endl;
					
					// now reduce them -- by unanimity?
					shared_ptr<type_die> found_type;
					for (auto i_ctxt = out.begin(); i_ctxt != out.end(); ++i_ctxt)
					{
						cerr << "context is " << CCP(GET_TEXT(*i_ctxt))
							<< ", full tree: " << CCP(TO_STRING_TREE(*i_ctxt)) << endl;
						assert(CCP(GET_TEXT(*i_ctxt)) == basic_name
						||     (friendly_name && CCP(GET_TEXT(*i_ctxt)) == *friendly_name));
						auto found_die = map_stub_context_to_dwarf_element(
							*i_ctxt,
							remote_module // dwarf context is the remote module
							);
						cerr << "Finished searching for DWARF element, status: "
							<< std::boolalpha << (found_die ? true : false) << endl;
						// if we didn't find a DIE, that means... what?
						shared_ptr<formal_parameter_die> found_fp
						 = dynamic_pointer_cast<formal_parameter_die>(found_die);
						assert(found_die); assert(found_fp);

						if (found_fp)
						{
							if (!found_type)
							{
								found_type = found_fp->get_type();
							} else if (found_type != found_fp->get_type()) RAISE(
							*i_ctxt, "non-unanimous types for usage contexts");
							
							cerr << "found type at offset 0x" << std::hex << found_type->get_offset()
								<< std::dec << ", name: " 
								<< (found_type->get_name() ? *found_type->get_name() : "(no name)")
								<< endl;
						}
					}
					shared_ptr<type_die> unq_t = found_type->get_unqualified_type();
					if (unq_t != found_type->get_concrete_type())
					{
						auto tdef = dynamic_pointer_cast<spec::typedef_die>(unq_t);
						assert(tdef);
						auto opt_name = tdef->get_name();
						assert(opt_name);
						remote_tagstring = *opt_name;
					}
					
					out_env->insert(std::make_pair(basic_name, 
						(bound_var_info) { basic_name, // use the same name for both
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module,
						false,
						local_tagstring,
						remote_tagstring, 
						indirect_local_tagstring_in,
						indirect_local_tagstring_out,
						indirect_remote_tagstring_in,
						indirect_remote_tagstring_out }));
					if (friendly_name) out_env->insert(std::make_pair(*friendly_name, 
						(bound_var_info) { basic_name,
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module,
						false,
						local_tagstring,
						remote_tagstring, 
						indirect_local_tagstring_in,
						indirect_local_tagstring_out,
						indirect_remote_tagstring_in,
						indirect_remote_tagstring_out }));
				} // end ALIAS3(annotatedValuePattern
				++argnum;
				if (i_caller_arg != caller_subprogram->formal_parameter_children_end()) ++i_caller_arg;
			} // end FOR_REMAINING_CHILDREN(eventPattern
		} // end ALIAS3(pattern, eventPattern, EVENT_PATTERN)
		
		return env;
	} // end 
					
	void wrapper_file::write_pattern_condition(
			const context& ctxt,
			antlr::tree::Tree *pattern)
	{
		bool emitted = false;
		INIT;
		ALIAS3(pattern, eventPattern, EVENT_PATTERN);
		{
			INIT;
			BIND2(pattern, eventContext);
			BIND2(pattern, memberNameExpr);
			BIND3(pattern, eventCountPredicate, EVENT_COUNT_PREDICATE);
			BIND3(pattern, eventParameterNamesAnnotation, KEYWORD_NAMES);
			definite_member_name call_mn = read_definite_member_name(memberNameExpr);
			std::string bound_name;
			
			int argnum = -1;

			//int dummycount = 0;
			FOR_REMAINING_CHILDREN(eventPattern)
			{
				++argnum;
				ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
				{
					INIT;
					BIND2(annotatedValuePattern, valuePattern);

					/* Now actually emit a condition, if necessary. */ 
					switch(GET_TYPE(valuePattern))
					{
						case CAKE_TOKEN(DEFINITE_MEMBER_NAME): assert(false);
						case CAKE_TOKEN(NAME_AND_INTERPRETATION): {
							// no conditions -- match anything (and bind)
							// Assert that binding has *already* happened,
							// when we created the initial environment.
							INIT;
							BIND3(valuePattern, definiteMemberName, DEFINITE_MEMBER_NAME);
							definite_member_name mn = read_definite_member_name(definiteMemberName);
							bound_name = mn.at(0);
							assert(ctxt.env.find(bound_name) != ctxt.env.end());
						} break;
						case CAKE_TOKEN(INDEFINITE_MEMBER_NAME): {
							// names beginning "dummy" are *not* in the environment
							std::ostringstream s; s << wrapper_arg_name_prefix << argnum;
							bound_name = s.str();
							// no conditions -- match anything (and don't bind)
						} break;
						case CAKE_TOKEN(KEYWORD_CONST): {
							// here we are matching a constant argument
							std::ostringstream s; s << // "dummy"; s << dummycount++;
								wrapper_arg_name_prefix << argnum;
							bound_name = s.str();
							boost::shared_ptr<dwarf::spec::type_die> p_arg_type;
							// find bound val's type, if it's a formal parameter
							assert(ctxt.env.find(bound_name) != ctxt.env.end());
							
							// recover argument type, if we have one
							//p_arg_type = ctxt.env[bound_name].cxx_type;
							// we have a condition to output
							if (emitted) m_out << " && ";
							m_out << "cake::equal<";
							//if (p_arg_type) m_out << get_type_name(p_arg_type);
							//else m_out << " ::cake::unspecified_wordsize_type";
							m_out << "__typeof(" << bound_name << ")";
							m_out << ", "
 << (	(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(STRING_LIT)) ? " ::cake::style_traits<0>::STRING_LIT" :
		(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(CONST_ARITH)) ? " ::cake::style_traits<0>::CONST_ARITH" :
		" ::cake::unspecified_wordsize_type" );
							m_out << ">()(";
							//m_out << "arg" << argnum << ", ";
							m_out << ctxt.env[bound_name].cxx_name << ", ";
							m_out << constant_expr_eval_and_cxxify(ctxt, valuePattern);
							m_out << ")";
							emitted = true;
						} break;
						default: assert(false); 
						break;
					} // end switch
				} // end ALIAS3
			}
		}
		if (!emitted) m_out << "true";
	}
	
//	 void
//	 wrapper_file::create_value_conversion(module_ptr source,
//			 boost::shared_ptr<dwarf::spec::type_die> source_data_type,
//			 antlr::tree::Tree *source_infix_stub,
//			 module_ptr sink,
//			 boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
//			 antlr::tree::Tree *sink_infix_stub,
//			 antlr::tree::Tree *refinement,
// 			bool source_is_on_left,
// 			antlr::tree::Tree *corresp)
//	 {
//	}
	
// 	void
// 	wrapper_file::emit_structural_conversion_body(
// 		boost::shared_ptr<dwarf::spec::type_die> source_type,
// 		boost::shared_ptr<dwarf::spec::type_die> target_type,
// 		antlr::tree::Tree *refinement, 
// 		bool source_is_on_left)
// 	{
// 	}
	
// 	void
// 	wrapper_file::emit_reinterpret_conversion_body(
// 		boost::shared_ptr<dwarf::spec::type_die> source_type,
// 		boost::shared_ptr<dwarf::spec::type_die> target_type)
// 	{ 
// 		m_out << "if (__cake_p_to) *__cake_p_to = *reinterpret_cast<const " 
// 			<< get_type_name(target_type) << "*>(&__cake_from);" << std::endl
// 			<< "return *reinterpret_cast<const " 
// 			<< get_type_name(target_type) << "*>(&__cake_from);" << std::endl;
// 	}

	module_ptr 
	wrapper_file::module_of_die(boost::shared_ptr<dwarf::spec::basic_die> p_d)
	{
		//return dynamic_cast<module_described_by_dwarf&>(p_d->get_ds()).shared_this();
		return m_d.module_for_dieset(p_d->get_ds());
	}

	std::string 
	wrapper_file::get_type_name(
			boost::shared_ptr<dwarf::spec::type_die> t)
	{
		const std::string& namespace_prefix
		 = ns_prefix + "::" + m_d.name_of_module(module_of_die(t));
		 
		return /*m_out <<*/ ((t->get_tag() == DW_TAG_base_type) ?
			compiler.local_name_for(t)
			: (namespace_prefix + "::" + compiler.fq_name_for(t)));
	}
	
	wrapper_file::value_conversion_params_t
	wrapper_file::resolve_value_conversion_params(
		link_derivation::iface_pair ifaces,
		//int rule_tag,
		const binding& source_binding,
		bool is_indirect,
		boost::shared_ptr<dwarf::spec::type_die> from_type, // most precise
		boost::shared_ptr<dwarf::spec::type_die> to_type, 
		boost::optional<std::string> from_typeof /* = boost::optional<std::string>()*/, // mid-precise
		boost::optional<std::string> to_typeof/* = boost::optional<std::string>()*/,
		module_ptr from_module/* = module_ptr()*/,
		module_ptr to_module/* = module_ptr()*/)
	{
		assert((from_type || from_module)
			&& (to_type || to_module));
		// fill in less-precise info from more-precise
		if (!from_module && from_type) from_module = module_of_die(from_type);
		if (!to_module && to_type) to_module = module_of_die(to_type);
		assert(from_module);
		assert(to_module);
		// check consistency
		assert((!from_type || module_of_die(from_type) == from_module)
		&& (!to_type || module_of_die(to_type) == to_module));
		// we must have either a from_type of a from_typeof
		assert(from_type || from_typeof);
		// ... this is NOT true for to_type: if we don't have a to_type or a to_typeof, 
		// we will use use the corresponding_type template
		std::string from_typestring = from_type ? get_type_name(from_type) 
			: (std::string("__typeof(") + *from_typeof + ")");
       
//         // if we have a "from that" type, use it directly
//         if (from_type)
//         {
//             m_out << "cake::value_convert<";
//             if (from_type) m_out << get_type_name(from_type/*, ns_prefix + "::" + from_namespace_unprefixed*/);
//             else m_out << " ::cake::unspecified_wordsize_type";
//             m_out << ", ";
//             if (to_type) m_out << get_type_name(to_type/*, ns_prefix + "::" + to_namespace_unprefixed*/); 
//             else m_out << " ::cake::unspecified_wordsize_type";
//             m_out << ">()(";
//         }
//         else
//         {
		
		/* If we want to select a rule mapping artificial types, then the
		 * user may have specified it as a specific from_type. Is that the
		 * only way? If they give us a typestring (typeof),
		 * it might denote a typedef too. */
		optional<string> from_artificial_tagstring = 
			is_indirect 
			? source_binding.second.indirect_local_tagstring_in // FIXME: handle "out"
			: source_binding.second.local_tagstring;
		optional<string> to_artificial_tagstring = 
			is_indirect
			? source_binding.second.indirect_remote_tagstring_in // FIXME: handle "out"
			: source_binding.second.remote_tagstring;
		
// 		if (from_type)
// 		{
// 			from_artificial_tagstring = 
// 				(from_type->get_concrete_type() == from_type) 
// 					? "__cake_default" 
// 					: *m_d.first_significant_type(from_type)->get_name();
// 		}
// 		else
// 		{
// 			from_artificial_tagstring = "__cake_default";
// 		}
		
        // nowe we ALWAYS use the function template
        //m_out << component_pair_classname(ifaces);

		//ostringstream rule_tag_str; rule_tag_str << rule_tag;
		std::string to_typestring;
		//std::string to_artificial_tagstring;
		if (to_type) 
		{
			// this gives us concrete and artificial info in one go
			to_typestring = get_type_name(to_type);
			// extract artificial info
// 			to_artificial_tagstring = 
// 				(to_type->get_concrete_type() == to_type) 
// 					? "__cake_default" 
// 					: *m_d.first_significant_type(to_type)->get_name();
		}
		else if (to_typeof) 
		{
			// this only gives us concrete info! 
			to_typestring = "__typeof(" + *to_typeof + ")";
// 			// ... so artificial tagstring is just the default
// 			to_artificial_tagstring = "__cake_default";
		}
		else
		{
			/* Here we have to recover the "to" type from the "from" type. 
			 * It makes a difference if our "from" thing is typedef/artificial. */
			to_typestring = std::string("::cake::") + "corresponding_type_to_"
				+ ((to_module == ifaces.first) 
					? ("second< " + component_pair_classname(ifaces) + ", " + from_typestring + ", true>"
						+ "::" 
						+ make_tagstring(from_artificial_tagstring)
						 + "_to_" 
						 + make_tagstring(to_artificial_tagstring)
						 + "_in_first")
					: ("first< " + component_pair_classname(ifaces) + ", " + from_typestring + ", true>"
						+ "::" 
						+ make_tagstring(from_artificial_tagstring)
						+ "_to_" 
						+ make_tagstring(to_artificial_tagstring)
						+ "_in_second"));
						// this one --^ is WHAT?       this one --^ is WHAT?
						// answer: art.tag in source   answer: art.tag in sink
		}

		/* This is messed up.
		 * Since we rely on __typeof, we don't know the DWARF types of
		 * some variables. This means we can't use DWARF types to lookup
		 * the artificial data types.
		 * How should it work?
		 * We generate (and number) all the rules we can.
		 * Then we associate with each *ident* an (optional) artificial interpretation string,
		 * as we bind it?
		 * I think this will work, because even if we have "as" applied to arbitrary expressions,
		 * the effect of this is just to bind a new ident and store the artificial string in *that*.
		 * We need some way to map from __typeof() and artificial ident strings 
		 * to rule tags at runtime. 
		 * So our generated code will be rule_tag<T>::some_ident
		 *                          concrete type-^   ^-- artificial tag
		 * Seems like we can extend the corresponding_type_to_first (etc.) classes here.
		 * What do we do for typedefs? What are their artificial tags?
		 * Can we use the DWARF offset? i.e. typedef some_concrete_type __dwarf_typedef_at_0xbeef?
		 * YES. Then we assign the artificial string "__dwarf_typedef_at_0xbeef" to the ident
		 * whose type is typedef'd in the object file. 
		 * Actually our code will look like
		 * class rule_tag<T> { // or actually corresponding_type_to_second 
		 *  enum { 
		 *     some_artificial_ident = TAG; 
		 *  // ...
		 *   };
		 * };*/
		//int rule_tag; // = 0; // FIXME: handle annotations / typedefs in the *source*
		/* How do we choose the rule tag? 
		 * - from our source and sink types, if they are typedefs; 
		 * - from "as" annotations in the originating rules. 
		 *   How do we find out about these?
		 *   - If there are any, they will be attached to bound args/values (in event corresps)
		 *                                                or fields (in value corresps)
		 * ... in the AST. 
		 * The cxx types don't distinguish; by definition, they're the concrete types, which
		 * are all the same for all values of rule_tag. */

		/* What are these source_artificial_identifier and sink_artificial_identifier?
		 * Roughly, they're the ident used with "as", or an identifier for a typedef. 
		 * They're different across the two modules, because 
		 * they might be corresponded explicitly
		 * rather than by name-matching.
		 * them. */

		//optional<string> source_artificial_identifier
		// = (from_type
		//
		//;// = extract_artificial_data_type(
		//	///*source_expr*/ 0, ctxt);
		//optional<string> sink_artificial_identifier; // = extract_artificial_data_type(
		//	///*sink_expr*/ 0, ctxt);

		// PROBLEM: we need ctxt to tell us exactly which syntactic fragment
		// the current crossover is handling, so that we can scan the AST for
		// which "as" expressions are relevant. And what if no "as" expressions
		// are relevant, but instead, the DWARF typedefs bound to each identifer?

		// PROBLEM: we need to propagate type information through expressions.
		// Otherwise, how are we going to handle
		//          foo(a as LengthInMetres) --> bar(a+1); // bar(padded_length: LengthInCm)
		// What does this mean? 
		// When 'a' is introduced to the environment, it should be associated with its DWARF element,
		// hence its typedef.
		// When we do 
		// new_ident = a + 1;
		// we want to propagate the information to new_ident
		// What about if we had 
		// a+b?  (and only one of them was a typedef, the other being the corresponding concrete type)?
		// In other words,
		// we need to propagate type info through an environment somehow -- be it statically or dynamically.
		// The properly Cake-like way to do it is dynamically, if we are a dynamic language.
		// Save a full version of this for the DwarfPython / PIE version of Cake
		// This definitely needs tackling there.
		// Specifically, we need *rules for how typedefs propagate dynamically*!
		// This definitely isn't handled already.
		// Some strawman examples: always discard; use synonym dependency as specificity relation
		// and tie-break on leftmore operands of binary operations; others?
		// FOR NOW: 
		// we commit the horrible HACK of 
		// basically doing "always discard"
		// and extending bound_var_info with an "origin" field
		// that is a has_type_describing_layout DIE.
		// What about propagating this across crossovers (and through __cake_it)? Do we need to?
		// We're in the middle of a crossover when we get called right here. 
		// YES we want to propagate e.g. the typedef that the crossed-over a corresponds to
		// -- if there is one! There might not be, for artificial data types. OH, but there will be a
		// corresponding data type, and that might be artificial.


		//string source_artificial_fragment = source_artificial_identifier ? 
		//	*source_artificial_identifier : "__cake_default";
		//string sink_artificial_fragment = sink_artificial_identifier ? 
		//	*sink_artificial_identifier : "__cake_default";

		bool target_is_in_first = (to_module == ifaces.first);
		string rule_tag_str = std::string(" ::cake::") + "corresponding_type_to_"
			+ (target_is_in_first ? 
				("second< " + component_pair_classname(ifaces) + ", " + from_typestring + ", true>")
			  : ("first< " + component_pair_classname(ifaces) + ", " + from_typestring + ", true>"))
			+ "::rule_tag_in_" + (target_is_in_first ? "first" : "second" ) 
			+ "_given_" + (target_is_in_first ? "second" : "first" ) + "_artificial_name_"
			+ make_tagstring(from_artificial_tagstring)
			+ "::" + make_tagstring(to_artificial_tagstring);

		/* GAH: we can't use template specialisation to give us defaults
		 * for which typedef to use, because template specialisation is insensitive to typedefs. 
		 * Can we specialise on DWARF offsets, and give each "as" a fake DWARF offset? 
		 * Maybe, but there will be many DWARF offsets for a given typedef. */

		//(target_is_in_first ? "::in_first" : "::in_second")
		//lookup_rule_tag(
		//	source_data_type,
		//	sink_data_type,
		//	source_artificial_identifier,
		//	sink_artificial_identifier,
		//	false // is_init
		//);
//             
//             if (ifaces.first == from_module)
//             {
//             	assert(ifaces.second == to_module);

            //m_out << "::value_convert_from_first_to_second< " 
            ///<< to_typestring //(" ::cake::unspecified_wordsize_type" )
            //<< ", " << rule_tag << ">(";
			return (value_conversion_params_t) { 
				from_typestring,
				to_typestring,
				ns_prefix + "::" + m_d.name_of_module(from_module) + "::marker",
				ns_prefix + "::" + m_d.name_of_module(to_module) + "::marker",
				rule_tag_str
			};
//             }
//             else 
//             {
//             	assert(ifaces.second == from_module);
//             	assert(ifaces.first == to_module);
// 
//                 m_out << "::value_convert_from_second_to_first< " 
//                 << to_typestring //" ::cake::unspecified_wordsize_type" )
//                 << ", " << rule_tag << ">(";
//             }
//        }
    }
	
	void
	wrapper_file::open_value_conversion(
		link_derivation::iface_pair ifaces,
		//int rule_tag,
		const binding& source_binding,
		bool is_indirect,
		boost::shared_ptr<dwarf::spec::type_die> from_type, // most precise
		boost::shared_ptr<dwarf::spec::type_die> to_type, 
		boost::optional<std::string> from_typeof /* = boost::optional<std::string>()*/, // mid-precise
		boost::optional<std::string> to_typeof/* = boost::optional<std::string>()*/,
		module_ptr from_module/* = module_ptr()*/,
		module_ptr to_module/* = module_ptr()*/)
	{
		auto params = resolve_value_conversion_params(
			ifaces,
			source_binding,
			is_indirect,
			from_type,
			to_type,
			from_typeof,
			to_typeof,
			from_module,
			to_module
		);
		m_out << "::cake::value_convert<" << std::endl
			<< "\t/* from type: */ " << params.from_typestring << ", " << std::endl
			<< "\t/* to type: */ " << params.to_typestring << ", " << std::endl
			<< "\t/* FromComponent: */ " << params.from_component_class << ", " << std::endl
			<< "\t/* ToComponent: */ " << params.to_component_class << ", " << std::endl
			<< "\t/* rule tag: */ " << params.rule_tag_str << std::endl
			<<  ">()(";
	}
	string wrapper_file::make_value_conversion_funcname(
		link_derivation::iface_pair ifaces,
		const binding& source_binding,
		bool is_indirect,
		shared_ptr<spec::type_die> from_type, // most precise
		shared_ptr<spec::type_die> to_type, 
		optional<string> from_typeof /* = optional<string>() */, // mid-precise
		optional<string> to_typeof /* = optional<string>() */,
		module_ptr from_module /* = module_ptr() */,
		module_ptr to_module /* = module_ptr() */
	)
	{
		auto params = resolve_value_conversion_params(
			ifaces,
			source_binding,
			is_indirect,
			from_type,
			to_type,
			from_typeof,
			to_typeof,
			from_module,
			to_module
		);
		return string("::cake::value_convert_function<") + "\n"
			+ "\t/* from type: */ " + params.from_typestring + ", " + "\n"
			+ "\t/* to type: */ " + params.to_typestring + ", " + "\n"
			+ "\t/* FromComponent: */ " + params.from_component_class + ", " + "\n"
			+ "\t/* ToComponent: */ " + params.to_component_class + ", " + "\n"
			+ "\t/* rule tag: */ " + params.rule_tag_str + "\n"
			+ ">";
	}

	void wrapper_file::close_value_conversion()
	{
		m_out << ")";
	}

	std::string wrapper_file::component_pair_classname(link_derivation::iface_pair ifaces_context)
	{
		std::ostringstream s;
		s << "cake::component_pair< " 
				<< ns_prefix << "::" << m_d.name_of_module(ifaces_context.first) << "::marker"
				<< ", "
				<< ns_prefix << "::" << m_d.name_of_module(ifaces_context.second) << "::marker"
				<< ">";
		return s.str();
	}

	wrapper_file::post_emit_status
	wrapper_file::emit_stub_expression_as_statement_list(
			const context& ctxt,
			antlr::tree::Tree *expr)
	{
		std::string ident;
		switch(GET_TYPE(expr))
		{
			case CAKE_TOKEN(INVOKE_WITH_ARGS): // REMEMBER: algorithms are special
				// and so is assert!
				return emit_stub_function_call(
					ctxt,
					expr);
			case CAKE_TOKEN(STRING_LIT):
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << "cake::style_traits<0>::string_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			case CAKE_TOKEN(INT):
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << "cake::style_traits<0>::int_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			case CAKE_TOKEN(FLOAT):
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << "cake::style_traits<0>::float_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			case CAKE_TOKEN(KEYWORD_VOID):
				return (post_emit_status){NO_VALUE, "true", environment()};
			case CAKE_TOKEN(KEYWORD_NULL):
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << "cake::style_traits<0>::null_value();" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			case CAKE_TOKEN(KEYWORD_TRUE):
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << "cake::style_traits<0>::true_value();" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			case CAKE_TOKEN(KEYWORD_FALSE):
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << "cake::style_traits<0>::false_value();" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			case CAKE_TOKEN(IDENT):
				{
					if (ctxt.env.find(unescape_ident(CCP(GET_TEXT(expr)))) == ctxt.env.end())
					{
						std::cerr << "Used name " 
							<< unescape_ident(CCP(GET_TEXT(expr)))
							<< " not present in the environment. Environment is: "
							<< std::endl;
						for (auto i_el = ctxt.env.begin(); i_el != ctxt.env.end(); ++i_el)
						{
							std::cerr << i_el->first << " : " 
								<<  i_el->second.cxx_name << std::endl;
						}
						assert(false);
					}
					assert(ctxt.env[unescape_ident(CCP(GET_TEXT(expr)))].valid_in_module
						== ctxt.modules.current);
					return (post_emit_status){ ctxt.env[unescape_ident(CCP(GET_TEXT(expr)))].cxx_name,
						"true", environment() };
				}
			case CAKE_TOKEN(KEYWORD_THIS):
				assert(ctxt.env.find("__cake_this") != ctxt.env.end());
				return (post_emit_status){ ctxt.env["__cake_this"].cxx_name,
						"true", environment() };
			case CAKE_TOKEN(KEYWORD_THAT):
				assert(ctxt.env.find("__cake_that") != ctxt.env.end());
				return (post_emit_status){ ctxt.env["__cake_that"].cxx_name,
						"true", environment() };
			case CAKE_TOKEN(KEYWORD_HERE):
				assert(ctxt.env.find("__cake_here") != ctxt.env.end());
				return (post_emit_status){ ctxt.env["__cake_here"].cxx_name,
						"true", environment() };
			case CAKE_TOKEN(KEYWORD_THERE):
				assert(ctxt.env.find("__cake_there") != ctxt.env.end());
				return (post_emit_status){ ctxt.env["__cake_there"].cxx_name,
						"true", environment() };

			case CAKE_TOKEN(KEYWORD_SUCCESS):

			case CAKE_TOKEN(KEYWORD_OUT):

			case CAKE_TOKEN(MEMBER_SELECT):
			case CAKE_TOKEN(INDIRECT_MEMBER_SELECT):
			case CAKE_TOKEN(ELLIPSIS): /* ellipsis is 'access associated' */
			case CAKE_TOKEN(ARRAY_SUBSCRIPT):

			// memory management
			case CAKE_TOKEN(KEYWORD_DELETE):
			case CAKE_TOKEN(KEYWORD_NEW):
			case CAKE_TOKEN(KEYWORD_TIE):

			// these affect the expected cxx type
			case CAKE_TOKEN(KEYWORD_AS):
			case CAKE_TOKEN(KEYWORD_IN_AS):
			case CAKE_TOKEN(KEYWORD_OUT_AS):
				assert(false);
			
			ambiguous_arity_ops: //__attribute__((unused))
			// may be unary (address-of) or binary
			case CAKE_TOKEN(BITWISE_AND):
			case CAKE_TOKEN(MINUS): // may be unary or binary!
			case CAKE_TOKEN(PLUS): // may be unary or binary!
			// dereference
			case CAKE_TOKEN(MULTIPLY): // may be unary (dereference) or binary
			{
				switch(GET_CHILD_COUNT(expr))
				{
					case 2:
						goto binary_al_ops;
					case 1:
						goto unary_al_ops;
					default: RAISE_INTERNAL(expr, "bad AST structure -- must be unary or binary");
				}
			}
			
			/* HACK: for unary and binary arithmetic/logic ops, we just
			 * blast out the C++ code*/
			unary_al_ops: //__attribute__((unused))
			case CAKE_TOKEN(COMPLEMENT):
			case CAKE_TOKEN(NOT):
			// + others from ambiguous goto: BITWISE_AND, MINUS, PLUS, MULTIPLY
			{
				auto result = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 0)/*,
					shared_ptr<type_die>()*/);
				ident = new_ident("temp");
				m_out << "auto " << ident << " = "
					<< CCP(TO_STRING(expr)) << result.result_fragment << ";" << std::endl;
				return (post_emit_status) { ident, 
					result.success_fragment, // no *new* failures added, so delegate failure
					environment() };
			}
			
			binary_al_ops: //__attribute__((unused))
			case CAKE_TOKEN(DIVIDE):
			case CAKE_TOKEN(MODULO):
			// we could try to catch divide-by-zero errors here, but we don't
			case CAKE_TOKEN(SHIFT_LEFT):
			case CAKE_TOKEN(SHIFT_RIGHT):
			case CAKE_TOKEN(LESS):
			case CAKE_TOKEN(GREATER):
			case CAKE_TOKEN(LE):
			case CAKE_TOKEN(GE):
			case CAKE_TOKEN(EQ):
			case CAKE_TOKEN(NEQ):
			case CAKE_TOKEN(BITWISE_XOR):
			case CAKE_TOKEN(BITWISE_OR):
			// + others from ambiguous goto: BITWISE_AND, MINUS, PLUS, MULTIPLY
			{
				auto resultL = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 0));
				auto resultR = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1));
				ident = new_ident("temp");

				m_out << "auto " << ident << " = "
					<< resultL.result_fragment 
					<< " " << CCP(TO_STRING(expr)) << " "
					<< resultR.result_fragment << ";" << std::endl;
				return (post_emit_status) { ident, 
					"(" + resultL.success_fragment + " && " + resultR.success_fragment + ")", 
						// no *new* failures added, so delegate failure
					environment() };
			}

			// these have short-circuit semantics
			case CAKE_TOKEN(LOGICAL_AND):
			case CAKE_TOKEN(LOGICAL_OR):
			
			case CAKE_TOKEN(CONDITIONAL): // $cond $caseTrue $caseFalse 
			
			case CAKE_TOKEN(KEYWORD_FN):
			
			sequencing_ops: //__attribute__((unused))
			case CAKE_TOKEN(SEMICOLON):
			case CAKE_TOKEN(ANDALSO_THEN):
			case CAKE_TOKEN(ORELSE_THEN):
				// these are the only place where we add to the environment, since
				// "let" expressions are no use in other contexts
				assert(false);
			case CAKE_TOKEN(KEYWORD_IN_ARGS):
				// ACTUALLY this might happen, if we use in_args in its standalone sense
				// of a pseudo-structure
				
				/* This shouldn't happen! We only evaluate in_args in the contexts
				 * where it may appear. */
				 RAISE(expr, "cannot use in_args outside an argument eval context");
			case CAKE_TOKEN(KEYWORD_OUT_ARGS):
				/* ditto */
				 RAISE(expr, "cannot use out_args outside an argument eval context");
			case CAKE_TOKEN(KEYWORD_CONST): {
				ident = new_ident("temp");
				m_out << "auto " << ident << " = ";
				m_out << constant_expr_eval_and_cxxify(ctxt, expr)  << ";" << std::endl;
				return (post_emit_status){ident, "true", environment()};
			}
			default:
				assert(false);
		}
	}
	
	std::vector<
		std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
					dwarf::spec::subprogram_die::formal_parameter_iterator 
		> 
	>
	wrapper_file::name_match_parameters(
		boost::shared_ptr< dwarf::spec::subprogram_die > first,
		boost::shared_ptr< dwarf::spec::subprogram_die > second)
	{
		std::vector<
			std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
						dwarf::spec::subprogram_die::formal_parameter_iterator 
			> 
		> out;
		for (auto i_first = first->formal_parameter_children_begin();
				i_first != first->formal_parameter_children_end();
				++i_first)
		{
			for (auto i_second = second->formal_parameter_children_begin();
				i_second != second->formal_parameter_children_end();
				++i_second)
			{
				if ((*i_first)->get_name() && (*i_second)->get_name()
					&& *(*i_first)->get_name() == *(*i_second)->get_name())
				{
					out.push_back(std::make_pair(i_first, i_second));
				}
			}
		}
		return out;
	}

	wrapper_file::post_emit_status
	wrapper_file::emit_stub_function_call(
			const context& ctxt,
			antlr::tree::Tree *call_expr/*,
			shared_ptr<type_die> expected_cxx_type*/) 
	{
		assert(GET_TYPE(call_expr) == CAKE_TOKEN(INVOKE_WITH_ARGS));
		INIT;
		// in new grammar, no longer a def. member name
		BIND3(call_expr, argsMultiValue, MULTIVALUE);
		// FIXME: function_name might actually be an expression! evaluate this first!
		BIND3(call_expr, functionNameTree/*, DEFINITE_MEMBER_NAME*/, IDENT);
		// auto mn = read_definite_member_name(memberNameExpr);
		
		//auto return_type_name = //cxx_result_type_name;
		//	(cxx_result_type 
		//	? get_type_name(cxx_result_type) 
		//	: "::cake::unspecified_wordsize_type");

		std::string function_name = CCP(GET_TEXT(functionNameTree));
		std::vector<std::string> mn(1, function_name);
		auto callee = ctxt.modules.sink->get_ds().toplevel()->visible_resolve(
					mn.begin(), mn.end());
		if (!callee || callee->get_tag() != DW_TAG_subprogram)
		{
			RAISE(functionNameTree, "does not name a visible function");
		}

		auto callee_subprogram
		 = boost::dynamic_pointer_cast<subprogram_die>(callee);
		auto callee_return_type
			= treat_subprogram_as_untyped(callee_subprogram) ?
			   0 : callee_subprogram->get_type();
			   
		/* evaluate the arguments and bind temporary names to them */
		auto success_ident = new_ident("success");
		m_out << "bool " << success_ident << " = true; " << std::endl;
		std::string value_ident = new_ident("value");
		if (treat_subprogram_as_untyped(callee_subprogram)
			&& !subprogram_returns_void(callee_subprogram))
		{
			m_out << /* "::cake::unspecified_wordsize_type" */ "int" // HACK: this is what our fake DWARF will say
			 << ' ' << value_ident << "; // unused" << std::endl;
		}
		else if (!subprogram_returns_void(callee_subprogram))
		{
			m_out << get_type_name(callee_subprogram->get_type())
			 << ' ' << value_ident << ";" << std::endl;
		}
		//m_out << "do" << std::endl
		//	<< "{";
		m_out << "// begin argument expression eval" << std::endl;
		//m_out.inc_level();
		std::vector< post_emit_status > arg_results;
		std::vector< boost::optional<std::string> > arg_names_in_callee;
		post_emit_status result;
		dwarf::spec::subprogram_die::formal_parameter_iterator i_arg
		 = callee_subprogram->formal_parameter_children_begin();
		// iterate through multiValue and callee args in lock-step
		{
			INIT;
			FOR_ALL_CHILDREN(argsMultiValue)
			{
				assert(i_arg != callee_subprogram->formal_parameter_children_end()
				 && (*i_arg)->get_type());
				int args_for_this_ast_node = 1; // will be overridden in in_args handling
				// for in_args handling:
				std::vector<
					std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
								dwarf::spec::subprogram_die::formal_parameter_iterator 
					> 
				> matched_names;				
				std::vector<
					std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
								dwarf::spec::subprogram_die::formal_parameter_iterator 
					> 
				>::iterator i_matched_name;				
				/* If the stub expression was a KEYWORD_IN_ARGS, then
				 * the stub code emitted  has yielded multiple outputs 
				 * and multiple successes. */
				switch (GET_TYPE(n))
				{
					case CAKE_TOKEN(KEYWORD_OUT_ARGS):
						assert(false);
					case CAKE_TOKEN(KEYWORD_IN_ARGS):
						/* When we see in_args, we eagerly match *by name*
						 * any argument in the source context (i.e. the event)
						 *  that has not already been evaluated,
						 * against any argument in the sink context
						 *  that is not already paired with a previously evaluated argument,
						 * and output them in their order of appearance in the callee.
						 * It is an error if the resulting order
						 * covers a noncontiguous range of arguments. */
						matched_names = name_match_parameters(
							ctxt.opt_source->signature,
							callee_subprogram);
							// FIXME: do we match names against the event pattern
							// (which may have made up its own names for an argument)
							// or against the DWARF info?
							// Well, a major use-case of in_args... is where we 
							// explicitly avoid naming any arguments and just say "bar(...)"
							// so we have to go with the DWARF. 
							// But FIXME: we should warn if event pattern names would give
							// a different mapping
						/* Now filter these matches:
						 * - discard any pair that precedes our current position in the callee; 
						 * - I think that's all? 
						 * And then 
						 * sort them by position in the callee arg list
						 * and check that they form a contiguous sequence 
						 * starting at our current pos. */
						for (auto i_out = matched_names.begin(); i_out != matched_names.end();
							++i_out)
						{
							dwarf::spec::subprogram_die::formal_parameter_iterator i_test;
							i_test = i_out->second;
							if (i_test < i_arg)
							{
								i_out = matched_names.erase(i_out);
								// erase returns the one after the erased item...
								i_out--; // ... which we want to come round next iteration
							}
						}
						// sort in order of the sink (callee) argument ordering
						std::sort(
							matched_names.begin(),
							matched_names.end(),
							[](const std::pair< 
									dwarf::spec::subprogram_die::formal_parameter_iterator,
									dwarf::spec::subprogram_die::formal_parameter_iterator >& a,
								const std::pair< 
									dwarf::spec::subprogram_die::formal_parameter_iterator,
									dwarf::spec::subprogram_die::formal_parameter_iterator >& b)
							{ return a.second < b.second; });
						// do they form a contiguous sequence starting at current pos?
						if (matched_names.begin() == matched_names.end())
						{
							// no arguments to name-match
							goto finished_argument_eval_for_current_ast_node; // naughty
						}
						if (matched_names.begin()->second != i_arg)
						{
							RAISE(n, "name-matching args do not start here");
						}
						for (auto i_out = matched_names.begin(); i_out != matched_names.end(); ++i_out)
						{
							auto i_next_matched_name = i_out; ++i_next_matched_name;
							auto i_next_callee_parameter = i_out->second; ++i_next_callee_parameter;
							if (i_next_matched_name != matched_names.end()
							// in the callee arg list, i.e. ->second, they must be contiguous
								&& i_next_matched_name->second != i_next_callee_parameter)
							{
								RAISE(n, "name-matching args are non-contiguous");
							}
						}
						// override the count of args we're emitting in this AST iteration
						args_for_this_ast_node = matched_names.size();
						// start the iterator
						i_matched_name = matched_names.begin();
						// now what? well, we simply evaluate them in order. What order?
						// the order they appear in the *callee*, so that we can keep
						// i_arg moving forward
						do
						{
							// what's the binding of the argument in the caller? 
							{
								std::ostringstream s;
								s << wrapper_arg_name_prefix << (i_matched_name - matched_names.begin());

								// emit some stub code to evaluate this argument
								result = emit_stub_expression_as_statement_list(
								  ctxt, 
								  // we need to manufacture an AST node: IDENT(arg_name_in_caller)
								  make_ident_expr(s.str())//,
								  /* Result type is that of the *argument* that we're going to pass
								   * this subexpression's result to. */
								  /*(treat_subprogram_as_untyped(callee_subprogram) 
								  ? boost::shared_ptr<dwarf::spec::type_die>()
								  : *(*i_arg)->get_type())*/);
								// next time round we will handle the next matched name
								++i_matched_name;
							}
							// the rest is like the simple case
							goto remember_arg_names;
					default:
							// emit some stub code to evaluate this argument -- simple case
							result = emit_stub_expression_as_statement_list(
							  ctxt, n//,
// 							  /* Result type is that of the *argument* that we're going to pass
// 							   * this subexpression's result to. */
// 							  (treat_subprogram_as_untyped(callee_subprogram) 
// 							  ? boost::shared_ptr<dwarf::spec::type_die>()
// 							  : *(*i_arg)->get_type())*/
							  );
							// remember the names used for the output of this evaluation
					remember_arg_names:
							arg_results.push_back(result);

							/* If the stub expression was a KEYWORD_IN_ARGS, then
							 * the stub code emitted  has yielded multiple outputs 
							 * and multiple successes. */

							// store the mapping to the callee argument
							arg_names_in_callee.push_back((*i_arg)->get_name());
					output_control:
							m_out << success_ident << " &= " << result.success_fragment << ";" << std::endl;
							m_out << "if (" << success_ident << ") // okay to proceed with next arg?" 
								<< std::endl;
							m_out << "{" << std::endl;
							m_out.inc_level();
					next_arg_in_callee_sequence:
							++i_arg;
						} // end do
						while (--args_for_this_ast_node > 0);
					finished_argument_eval_for_current_ast_node:
						break;
				} // end switch
			} // end FOR_ALL_CHILDREN
		} // end INIT argsMultiValue
		m_out << "// end argument eval" << std::endl;
		
		// semantic check: did we output enough arguments for the callee?
		int min_args = 0;
		for (auto i_arg = callee_subprogram->formal_parameter_children_begin();
			i_arg != callee_subprogram->formal_parameter_children_end(); min_args++, ++i_arg);
		int max_args =
			(callee_subprogram->unspecified_parameters_children_begin() == 
				callee_subprogram->unspecified_parameters_children_end())
			? min_args : -1;
		if ((signed) arg_results.size() < min_args || 
			(max_args != -1 && (signed) arg_results.size() > max_args))
		{
			std::ostringstream msg;
			msg << "invalid number of arguments (" << arg_results.size();
			msg << ": ";
			for (auto i_result = arg_results.begin(); i_result != arg_results.end(); ++i_result)
			{
				if (i_result != arg_results.begin()) msg << ", ";
				msg << i_result->result_fragment;
			}
			msg << ") for call";
			RAISE(call_expr, msg.str().c_str());
		}
		
		
		m_out << "// begin function call" << std::endl;
		//m_out << args_success_ident << " = true;" << std::endl;
		
		// result and success of overall function call
		//std::string success_ident = new_ident("success");
		//m_out << "bool " << success_ident << ";" << std::endl;
		//m_out << "if (" << success_ident << ")" << std::endl;
		//m_out << "{" << std::endl;
		//m_out.inc_level();

		std::string raw_result_ident = new_ident("result");
		if (callee_subprogram->get_type())
		{
			m_out << "auto " << raw_result_ident << " = ";
		}

		// emit the function name, as a symbol reference
		m_out << "cake::" << m_d.namespace_name() << "::";
		m_out << get_dwarf_ident(
			ctxt,
			function_name);		
		m_out << '(';
		m_out.inc_level();
		for (auto i_result = arg_results.begin(); 
			i_result != arg_results.end(); ++i_result)
		{
			if (i_result != arg_results.begin()) m_out << ", ";
			m_out << std::endl; // begin each argument on a new line
			auto name_in_callee = arg_names_in_callee[i_result - arg_results.begin()];
			m_out << "/* argument name in callee: " 
				<< (name_in_callee ? *name_in_callee : "(no name)")
				<< " */ ";
			m_out << i_result->result_fragment;
		}
		m_out << ')';
		m_out.dec_level();
		
		m_out << ";" << std::endl;
		m_out << "// end function call" << std::endl;
		
		// convert the function raw result to a success and value pair,
		// in a style-dependent way
		m_out << "// begin output/error split for the function call overall" << std::endl;
		if (callee_subprogram->get_type())
		{
			m_out << success_ident << " = __cake_success_of(" 
				<< raw_result_ident << ");" << std::endl;
			m_out << "if (" << success_ident << ")" << std::endl
			<< "{" << std::endl;
			m_out.inc_level();
				m_out << value_ident << " = __cake_value_of("
					<< raw_result_ident << ");" << std::endl;
			m_out.dec_level();
			m_out << "}" << std::endl;
			
		}
		else m_out << success_ident << " = true;" << std::endl;
		m_out << "// end output/error split for the function call overall" << std::endl;

		// we opened argcount  extra lexical blocks in the argument eval
		for (unsigned i = 0; i < arg_results.size(); i++)
		{
			m_out.dec_level();
			m_out << "} " /*"else " << success_ident << " = false;"*/ << std::endl;
		}
		
		// set failure result
		m_out << "// begin calculate overall expression success/failure and output " << std::endl;
		m_out << "if (!" << success_ident << ")" << std::endl;
		m_out << "{" << std::endl;
		m_out.inc_level();
		if (!subprogram_returns_void(callee_subprogram)) // only get a value if it returns something
		{
			m_out << value_ident << " = ::cake::failure<" 
				<<  (treat_subprogram_as_untyped(callee_subprogram)
					 ? /* "unspecified_wordsize_type " */ "int" // HACK! "int" is what our fake DWARF will say, for now
					 : get_type_name(callee_subprogram->get_type())) //return_type_name
				<< ">()();" << std::endl;
		}
		m_out.dec_level();
		m_out << "}" << std::endl;
		m_out << "// end calculate overall expression success/failure and output " << std::endl;
		
		//m_out.dec_level();
		//m_out << "} while(0);" << std::endl;
		
		//m_out << "if (!" << success_ident << ")" << std::endl
		//	<< "{";
		//m_out.inc_level();
		

		return (post_emit_status){value_ident, success_ident, environment()};
	}

	std::string wrapper_file::constant_expr_eval_and_cxxify(
		const context& ctxt,
		antlr::tree::Tree *constant_expr)
	{
		//m_out << CCP(TO_STRING_TREE(constant_expr));
		std::ostringstream s;
		assert(GET_TYPE(constant_expr) == CAKE_TOKEN(KEYWORD_CONST));
		INIT;
		BIND2(constant_expr, child);
		switch(GET_TYPE(child))
		{
			case CAKE_TOKEN(STRING_LIT):
				s << CCP(GET_TEXT(child));
				break;
			case CAKE_TOKEN(KEYWORD_NULL):
				s << "0";
				break;
			case CAKE_TOKEN(SET_CONST):
				RAISE(child, "set constants not yet supported"); // FIXME			
			//case CAKE_TOKEN(CONST_ARITH):
			case CAKE_TOKEN(CONST_ARITH):
			{
				long double result = eval_const_expr(ctxt, constant_expr);
				s << result; // trivial cxxify :-)
				break;
			}			
			default: 
				RAISE(child, "expected a constant expression");
		}
		return s.str();
	}
    
    // FIXME: something better than this naive long double implementation please
    long double wrapper_file::eval_const_expr(
    	const context& ctxt,
		antlr::tree::Tree *expr)
    {
    	switch (GET_TYPE(expr))
        {
        	case CAKE_TOKEN(INT):
            	return atoi(CCP(GET_TEXT(expr)));
            case CAKE_TOKEN(SHIFT_LEFT): 
            	return 
                	eval_const_expr(ctxt, GET_CHILD(expr, 0))
                    	* powl(2.0, eval_const_expr(ctxt, GET_CHILD(expr, 0)));
            case CAKE_TOKEN(SHIFT_RIGHT):
            	return 
                	eval_const_expr(ctxt, GET_CHILD(expr, 0))
                    	* powl(2.0, eval_const_expr(ctxt, GET_CHILD(expr, 0)));

            case CAKE_TOKEN(KEYWORD_CONST):
            case CAKE_TOKEN(CONST_ARITH):
            	return eval_const_expr(ctxt, GET_CHILD(expr, 0));
            default: RAISE(expr, "unsupported constant expression syntax");
        }
    }
    
    std::string wrapper_file::get_dwarf_ident(
			const context& ctxt,
            antlr::tree::Tree *dmn)
    {
    	definite_member_name mn(dmn);
        
        // don't bother emitting these ns names because we're inside them already!
//         << "cake::" 
//         	<< m_d.namespace_name() << "::" // namespace enclosing this module's wrapper definitions
            
		std::ostringstream s;
		shared_ptr<with_named_children_die> scope
		 = (ctxt.modules.current == ctxt.modules.source)
		 	? ctxt.dwarf_context.source_decl
			: ctxt.dwarf_context.sink_decl;
		
    	s << m_d.name_of_module(ctxt.modules.current) << "::" // namespace specifically for this module
            << definite_member_name(
            		*(
                    	((*(scope->resolve(
                    		mn.begin(), mn.end())
                          )).ident_path_from_cu())
                     )
                 );
		return s.str();
    }
    
    std::string
	wrapper_file::get_dwarf_ident(
		const context& ctxt,
		const std::string& ident)
    {
    	std::vector<std::string> mn(1, ident);
		std::ostringstream s;
    	s << m_d.name_of_module(ctxt.modules.current) << "::" // namespace specifically for this module
        	<< ident;
		return s.str();
        // Just output the ident! -- 
        // why did we feed this into resolve and then generate the path again?
            //<< definite_member_name(
            //		*(
            //        	((*(dwarf_context.resolve(
            //        		mn.begin(), mn.end())
            //              )).ident_path_from_cu())
            //         )
            //     );
    }
	
	optional<string> 
	wrapper_file::extract_artificial_data_type(
		shared_ptr<spec::type_die> source_data_type, 
		const context& ctxt)
	{
		return optional<string>();
	}
	
}
