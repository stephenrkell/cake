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

namespace cake
{
	const std::string wrapper_file::wrapper_arg_name_prefix = "__cake_arg";
	const std::string wrapper_file::NO_VALUE = "__cake_arg";

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
					*unique_called_subprogram->get_type(),
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
            else m_out << get_type_name(*ret_type);
             
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
// 					for (int i = 0; i < argnum; i++) i_fp++;
// 	//xxxxxxxxxxxxxxxxxxxxxxx
// 					if (emit_types)
// 					{
// 						if ((*i_fp)->get_type())
// 						{
// 							// look for a _unique_ _corresponding_ type and use that
// 							auto found_vec = m_d.corresponding_dwarf_types(
// 								*(*i_fp)->get_type(),
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
// 								else m_out << get_type_name(*(*i_fp)->get_type());
// 							}
// 						}
// 						else  // FIXME: remove duplication here ^^^ vvv
// 						{
// 							if (treat_subprogram_as_untyped(unique_called_subprogram))
// 							{ m_out << " ::cake::unspecified_wordsize_type"; }
// 							else m_out << get_type_name(*(*i_fp)->get_type());
// 						}
// 					}
// 	//				else
// 	//				{
// 	//				
// 	//xxxxxxxxxxxxxxxxxxxxxxxx				
// 	//                     m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
// 	//                        *(*i_fp)->get_type()));
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
//                     	//    *(*i_fp)->get_type()));
//                     	m_out << ' ' << arg_name_prefix << argnum /*<< "_dummy"/* << argnum*/;
// 					}
// 					goto next;
assert(false);
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
                                {
                                	definite_member_name mn = 
                                    	read_definite_member_name(valuePattern);
                                    if (mn.size() > 1) RAISE(valuePattern, "may not be compound");
                            	    // output the variable type, or unspecified_wordsize_type
                                    if (emit_types) m_out << ((ignore_dwarf_args || !(*i_arg)->get_type()) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
                                	    *(*i_arg)->get_type()));
                                    // output the variable name, prefixed 
                                    m_out << ' ' << arg_name_prefix << argnum /*<< '_' << mn.at(0)*/;
	                            } break;
                            case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    case CAKE_TOKEN(METAVAR):
                            case CAKE_TOKEN(KEYWORD_CONST):
                            	// output the argument type and a dummy name
                                if (emit_types) m_out << ((ignore_dwarf_args || !(*i_arg)->get_type()) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
                                	*(*i_arg)->get_type()));
                                m_out << ' ' << arg_name_prefix << argnum /*<< "_dummy"/* << argnum*/;
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
						i_arg++; // advance DWARF caller arg cursor
						//std::cerr << " to ";
						//if (i_arg != args_end) std::cerr << **i_arg; else std::cerr << "(sentinel)";
						//std::cerr << std::endl;
					}
                	argnum++; // advance our arg count
					if (ignore_dwarf_args && unique_called_subprogram)
					{
						i_fp++;
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
// 				for (int i = 0; i < argnum; i++) i_fp++;
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
// //                        *(*i_fp)->get_type()));
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
// 					i_arg++; // advance DWARF caller arg cursor
// 					std::cerr << " to ";
// 					if (i_arg != args_end) std::cerr << **i_arg; else std::cerr << "(sentinel)";
// 					std::cerr << std::endl;
// 				}
//                 argnum++; // advance our arg count
// 				if (ignore_dwarf_args && unique_called_subprogram)
// 				{
// 					i_fp++;
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
				i_arg_ctr++) { count++; }
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
				.visible_named_child(wrapped_symname);
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
				i_pcorresp != corresps.end(); i_pcorresp++)
		{
			antlr::tree::Tree *pattern = (*i_pcorresp)->second.source_pattern;
			antlr::tree::Tree *action = (*i_pcorresp)->second.sink_expr;
			antlr::tree::Tree *return_leg = (*i_pcorresp)->second.return_leg;
			antlr::tree::Tree *source_infix_stub = (*i_pcorresp)->second.source_infix_stub;
			antlr::tree::Tree *sink_infix_stub = (*i_pcorresp)->second.sink_infix_stub;
			module_ptr source = (*i_pcorresp)->second.source;
			module_ptr sink = (*i_pcorresp)->second.sink;

			// create a code generation context
			context ctxt(*this, 
				source, sink,
				initial_environment(pattern, source));
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
				ctxt.modules.source };
				

			// crossover point
			ctxt.modules.current = ctxt.modules.sink;
			m_out << "// source->sink crossover point" << std::endl;
			std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> > constraints;
			if (sink_infix_stub) m_d.find_type_expectations_in_stub(
				sink, sink_infix_stub, boost::shared_ptr<dwarf::spec::type_die>(), constraints);
			m_d.find_type_expectations_in_stub(
				sink, action, boost::shared_ptr<dwarf::spec::type_die>(), constraints);
			ctxt.env = crossover_environment(new_env1, sink, constraints);
			m_out << "sync_all_co_objects(REP_ID(" << m_d.name_of_module(source)
				<< "), REP_ID(" << m_d.name_of_module(sink) << "));" << std::endl;
			// -- we need to modify the environment:

			// FIXME: here goes the post-arrow infix stub
			auto status2 = 
				(sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub) > 0) ?
					emit_stub_expression_as_statement_list(
					ctxt,
					sink_infix_stub/*,
					shared_ptr<type_die>()*/) // FIXME: really no type expectations here?
					: (post_emit_status){NO_VALUE, "true", environment()};
			auto saved_env2 = ctxt.env;
			auto new_env2 = merge_environment(ctxt.env, status2.new_bindings);
			if (status2.result_fragment != NO_VALUE) new_env2["__cake_it"] = (bound_var_info) {
				status2.result_fragment,
				status2.result_fragment, //shared_ptr<type_die>(),
				ctxt.modules.sink };
			
			// emit sink action, *NOT* including return statement
			std::cerr << "Processing stub action: " << CCP(TO_STRING_TREE(action)) << std::endl;
			assert(GET_TYPE(action) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
			post_emit_status status3;
			{
				INIT;
				BIND2(action, stub);

				std::cerr << "Emitting event correspondence stub: "
					<< CCP(TO_STRING_TREE(stub)) << std::endl;
				m_out << "// " << CCP(TO_STRING_TREE(stub)) << std::endl;

				// the sink action is defined by a stub, so evaluate that 
				
				// -- the cxx_expected_type is FIXME for now, the unique
				// corresponding type of the return value
				// -- this is broken when the return leg might change the cxx type of "it"
// 				boost::shared_ptr<dwarf::spec::type_die> cxx_expected_type 
// 					= treat_subprogram_as_untyped(ctxt.opt_source->signature) 
// 						? boost::shared_ptr<dwarf::spec::type_die>()
// 						: m_d.unique_corresponding_dwarf_type(
// 							*ctxt.opt_source->signature->get_type(),
// 							ctxt.modules.sink,
// 							true /* flow_from_type_module_to_corresp_module */);
				std::cerr << "Event sink stub is: " << CCP(TO_STRING_TREE(stub)) << std::endl;
				assert(GET_TYPE(stub) == CAKE_TOKEN(INVOKE_WITH_ARGS));
				status3 = emit_stub_expression_as_statement_list(
					ctxt, stub/*, cxx_expected_type*/);
			}
			// update environment
			auto new_env3 = merge_environment(ctxt.env, status3.new_bindings);
			if (status3.result_fragment != NO_VALUE) new_env3["__cake_it"] = (bound_var_info) {
				status3.result_fragment,
				status3.result_fragment, //shared_ptr<type_die>(),
				ctxt.modules.sink };
			
			// crossover point
			m_out << "// source<-sink crossover point" << std::endl;
			m_out << "sync_all_co_objects(REP_ID(" << m_d.name_of_module(sink)
				<< "), REP_ID(" << m_d.name_of_module(source) << "));" << std::endl;
			ctxt.modules.current = ctxt.modules.source;
			// update environment
			std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> > 
			return_constraints;
			if (return_leg) m_d.find_type_expectations_in_stub(
				source, return_leg, boost::shared_ptr<dwarf::spec::type_die>(), return_constraints);
			ctxt.env = crossover_environment(new_env3, source, return_constraints);
			
			m_out << "// begin return leg of rule" << std::endl;
			// emit the return leg, if there is one; otherwise, status is the old status
			auto status4 = 
				(return_leg && GET_CHILD_COUNT(return_leg) > 0) ?
					emit_stub_expression_as_statement_list(
					ctxt,
					return_leg/*,
					subprogram_returns_void(ctxt.opt_source->signature) ? 
						shared_ptr<type_die>()
					:	*ctxt.opt_source->signature->get_type()*/)
					: status3;
			// update environment -- just "it"
			auto new_env4 = merge_environment(ctxt.env, status4.new_bindings);
			if (status4.result_fragment != NO_VALUE) new_env4["__cake_it"] = (bound_var_info) {
				status4.result_fragment,
				status4.result_fragment, //shared_ptr<type_die>(),
				ctxt.modules.source };
			ctxt.env = new_env4;
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
				m_out << "if (" << status4.success_fragment << ") return "
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

//     wrapper_file::bound_var_info::bound_var_info(
//     	wrapper_file& w,
//         boost::shared_ptr<dwarf::spec::type_die> type,
// 	    module_ptr defining_module,
// 	    const std::string& cxx_name)
//         : prefix(cxx_name), type(type), defining_module(defining_module), origin(origin)
//         {
//             // this version is *not* for formal parameters! since they should
//             // have different names
//             assert((origin->get_tag() != DW_TAG_formal_parameter)
//                 || (std::cerr << *origin, false));
//             this->count = w.binding_count++; // ignored
// 			std::ostringstream s;
// 			s << prefix << count;
// 			this->cxx_name = s.str();
//         }
//     // special version for formal parameters! works out count and prefix
//     // -- note: not virtual, so make sure we only use this for formal_parameter DIEs
//     wrapper_file::bound_var_info::bound_var_info(
//     	wrapper_file& w,
//         //const std::string& prefix,
//         boost::shared_ptr<dwarf::spec::type_die> type,
// 	    module_ptr defining_module,
// 	    boost::shared_ptr<dwarf::spec::formal_parameter_die> origin)
//         : prefix("arg"), type(type), defining_module(defining_module), origin(origin) 
//     {
//         assert(origin || (std::cerr << std::hex << &origin << std::dec << std::endl, false));
//         assert(origin->get_parent() || (std::cerr << *origin, false));
//         auto p_subprogram = boost::dynamic_pointer_cast<dwarf::spec::subprogram_die>(
//             origin->get_parent());
//         int i = 0;
//         bool found = false;
//         for (auto i_fp = p_subprogram->formal_parameter_children_begin();
//             i_fp != p_subprogram->formal_parameter_children_end();
//             i_fp++)
//         {
//             if ((*i_fp)->get_offset() == origin->get_offset())
//             {
//                 found = true;
//                 break;
//             }
//             else ++i;
//         }
//         if (!found) throw InternalError(0, "couldn't discover argument position");
// 
//         count = i;
// 		std::ostringstream s;
// 		s << prefix << count;
// 		this->cxx_name = s.str();
//     }
//     wrapper_file::bound_var_info::bound_var_info(
//     	wrapper_file& w,
//         //const std::string& prefix,
//         boost::shared_ptr<dwarf::spec::type_die> type,
// 	    module_ptr defining_module,
// 	    boost::shared_ptr<dwarf::spec::unspecified_parameters_die> origin)
//         : prefix("arg"), type(type), defining_module(defining_module), origin(origin) 
//     {
//         // We keep in the wrapper_file a map keyed on a stable identifier
//         // for the DIE, which counts the arguments that we've bound from
//         // this particular unspecified_parameters die, so that we can issue
//         // a new arg number for each one.
// 
//         // NOTE: this assumes that in the same wrapper_file, we don't 
//         // ever do more than one round of binding names to the same arguments
//         // -- otherwise we'll get a continuation of the previous sequence
//         // of numbers.
// 
//         // FIXME: doesn't work for varargs at the moment!
//         // We need to get the number of declared arguments, and start
//         // the count there rather than at zero (at present).
//         count = w.arg_counts[w.get_stable_die_ident(origin)]++;
// 		std::ostringstream s;
// 		s << prefix << count;
// 		this->cxx_name = s.str();
//     }

	wrapper_file::environment 
	wrapper_file::crossover_environment(
		const environment& env,
		module_ptr new_module_context,
		const std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> >& constraints
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
		for (auto i_binding = env.begin(); i_binding != env.end(); i_binding++)
		{
			// sanity check
			assert(i_binding->second.valid_in_module != new_module_context);
			
			// create a new cxx ident for the binding
			auto ident = new_ident("xover_" + i_binding->first);
			
			/* Work out the target type expectations, using constraints */
			boost::shared_ptr<dwarf::spec::type_die> precise_to_type;
			int rule_tag = 0; // FIXME: handle annotations / typedefs in the *source*
			auto iter_pair = constraints.equal_range(i_binding->first);
			for (auto i_type = iter_pair.first; i_type != iter_pair.second; i_type++)
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
					if (precise_to_type->iterator_here() == i_type->second->iterator_here())
					{
						// okay, same DIE, so continue
						continue;
					}
					else
					{
						assert(false);
					}
				}
				else precise_to_type = i_type->second; // 
			}
			
			// output its initialization
			m_out << "auto " << ident << " = ";
			open_value_conversion(
				link_derivation::sorted(new_module_context, i_binding->second.valid_in_module),
				rule_tag, // defaults to 0, but may have been set above
				boost::shared_ptr<dwarf::spec::type_die>(), // no precise from type
				precise_to_type, // defaults to no precise to type, but may have been set above
				i_binding->second.cxx_typeof, // from typeof is in the binding
				boost::optional<std::string>(), // NO precise to typeof, 
				   // BUT maybe we could start threading a context-demanded type through? 
				   // It's not clear how we'd get this here -- scan future uses of each xover'd binding?
				   // i.e. we only get it *later* when we try to emit some stub logic that uses this binding
				i_binding->second.valid_in_module, // from_module is 
				new_module_context
			);
			
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
			m_out << i_binding->second.cxx_name;
			close_value_conversion();
			m_out << ";" << std::endl;
			
			// add it to the new environment
			new_env[i_binding->first] = (bound_var_info) {
				ident,
				ident,
				new_module_context
			};
		}
		// sanity check
		assert(new_env.size() == env.size());
		
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
				i_new++)
		{
			if (seen_module)
			{
				assert(i_new->second.valid_in_module == seen_module);
			}
			else seen_module = i_new->second.valid_in_module;
			
			new_env.insert(*i_new);
		}
		
		return new_env;
	}

	wrapper_file::environment 
	wrapper_file::initial_environment(
		antlr::tree::Tree *pattern,
		module_ptr source_module
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
					switch(GET_TYPE(valuePattern))
					{
						case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
							// could match anything, so bind name and continue
							definite_member_name mn = read_definite_member_name(valuePattern);
							if (mn.size() != 1) RAISE(valuePattern, "may not be compound");
							friendly_name = mn.at(0);
						} break;
						case CAKE_TOKEN(KEYWORD_CONST):
						case CAKE_TOKEN(INDEFINITE_MEMBER_NAME): {
							// we will bind a basic name but not a friendly one
						} break;
						default: RAISE(valuePattern, "unexpected token");
					}
					out_env->insert(std::make_pair(basic_name, 
						(bound_var_info) { basic_name, // use the same name for both
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module 
						}));
					if (friendly_name) out_env->insert(std::make_pair(*friendly_name, 
						(bound_var_info) { basic_name,
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module
						}));
				} // end ALIAS3(annotatedValuePattern
				++argnum;
				if (i_caller_arg != caller_subprogram->formal_parameter_children_end()) i_caller_arg++;
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
			
			int argnum = 0;

			//int dummycount = 0;
			FOR_REMAINING_CHILDREN(eventPattern)
			{
				ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
				{
					INIT;
					BIND2(annotatedValuePattern, valuePattern);

					/* Now actually emit a condition, if necessary. */ 
					switch(GET_TYPE(valuePattern))
					{
						case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
							// no conditions -- match anything (and bind)
							// Assert that binding has *already* happened,
							// when we created the initial environment.
							definite_member_name mn = read_definite_member_name(valuePattern);
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
    
	void wrapper_file::open_value_conversion(
		link_derivation::iface_pair ifaces,
		int rule_tag,
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
		std::string from_typestring = from_type ? get_type_name(from_type) 
			: (std::string("__typeof(") + *from_typeof + ")");
		// ... this is NOT true for to_type: if we don't have a to_type or a to_typeof, 
		// we will use use the corresponding_type template
       
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
		
        	// nowe we ALWAYS use the function template
            //m_out << component_pair_classname(ifaces);
			
			std::string to_typestring;
			if (to_type) to_typestring = get_type_name(to_type);
			else if (to_typeof) to_typestring = "__typeof(" + *to_typeof + ")";
			else
			{
				to_typestring = std::string("::cake::") + "corresponding_type_to_"
					+ ((to_module == ifaces.first) ? ("second< " + component_pair_classname(ifaces) + ", " + from_typestring + ", 0, true>::in_first")
					                               : ("first< " + component_pair_classname(ifaces) + ", " + from_typestring + ", 0, true>::in_second"));
			}
//             
//             if (ifaces.first == from_module)
//             {
//             	assert(ifaces.second == to_module);
                
                //m_out << "::value_convert_from_first_to_second< " 
                ///<< to_typestring //(" ::cake::unspecified_wordsize_type" )
                //<< ", " << rule_tag << ">(";
				m_out << "::cake::value_convert<" << std::endl
					<< from_typestring << ", " << std::endl
					<< to_typestring << ", " << std::endl
					<< rule_tag << ">()(";
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
    
    /* We can emit stub expressions as *either* expressions or statements. As a statement 
     * just means that we bind names to the result and success values. Sequencing expressions
     * cannot be emitted as expressions. If we run into a nested sequencing expression... WHAT? 
     * We have to backtrack: emit the nested expression *first*, bind a name to it,
     * then reference it by name in the current expression. If we run into a nested
     * success-conditional sequencing expression, WHAT?
     * If we run into a SUCCESS or FAIL expression, WHAT?
     * If we run into a CONDITIONAL, what?
     * We could use the comma operator, but that would be horrible to debug. 
     
     PROBLEM: if we want to backtrack and emit temporaries, we can't assume that the
     output stream is in an appropriate position for this: what if we'd just done
     call_function( ) 
     
     and then wanted to insert expressions for each argument? 
     Is calling functions special?  Clearly we do value conversions there where we
     otherwise don't. (Actually: we do so when accessing names that are bound to
     data originating in other components).
     
     I vote for a complete flattening approach: all subexpressions are evaluated in
     their own statements and given names. Then we can reference their result by name.
     This will make debugging the generated code easier, although perhaps verbose. */

	wrapper_file::post_emit_status
	wrapper_file::emit_stub_expression_as_statement_list(
			const context& ctxt,
			antlr::tree::Tree *expr/*,
			shared_ptr<type_die> expected_cxx_type*/)
// 			,
// 			link_derivation::iface_pair ifaces_context,
// 			const request::module_name_pair& context, // sink module
// 			const request::module_name_pair& source_context, // source module
// 			///*boost::shared_ptr<dwarf::spec::type_die>*/ const std::string& cxx_result_type_name,
// 			boost::shared_ptr<dwarf::spec::type_die> cxx_result_type,
// 			environment env)
	{
		std::string ident;
		switch(GET_TYPE(expr))
		{
			case CAKE_TOKEN(INVOKE_WITH_ARGS): // REMEMBER: algorithms are special
				// and so is assert!
				return emit_stub_function_call(
					ctxt,
					expr/*,
					expected_cxx_type*/);
					
//					expr,
//					ifaces_context,
//					context, // sink module
//					source_context,
//					//cxx_result_type_name,
//					cxx_result_type,
//					env);
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
				//std::string ident = new_ident("temp");
				//m_out << "auto " << ident << " = ";
				//m_out << "cake::style_traits<0>::void_value()";
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
// 					/* If it's in our environment, it might need value conversion. */
// 					std::string s = CCP(GET_TEXT(expr));
// 					if (ctxt.env.find(s) != ctxt.env.end())
// 					{
// 						ident = new_ident("temp");
// 						m_out << "auto " << ident << " = ";
// 						emit_bound_var_rvalue(ctxt, *ctxt.env.find(s)/*, ifaces_context, context, 
// 							*env.find(s), env, cxx_result_type*/);
// 						m_out << ";" << std::endl;
// 						return std::make_pair("true", ident);
// 					}
// 					/* Even if it isn't, it might need value conversion. */
// 					/* Otherwise it might be an argument (hence need value conversion)
// 					 * or a global/static thing (defined in the dwarfhpp-generated headers). */
// 					else return std::make_pair("true", 
// 						ctxt.ns_prefix + "::" // namespace for the current link block
// 						+ /*callee_namespace_name*//*context.second*/ name_for_module(ctxt.modules.sink) + "::" // namespace for the current module
// 						+ CCP(TO_STRING(expr)) // the ident itself
// 						); 
// 						// FIXME: should resolve in DWARF info
					assert(ctxt.env.find(CCP(GET_TEXT(expr))) != ctxt.env.end());
					/* When do we get asked to emit an IDENT?
					 * Suppose we're recursively evaluating a big expression.
					 * Eventually we will get down to the idents.
					 * There's no point expanding an ident to an ident.
					 * So broaden our interface slightly:
					 * just as "names.second" can include "true" for trivial successes,
					 * so "names.first" can include a simple ident reference or
					 * value conversion. */
					//assert(false);
					
					//emit_bound_var_rvalue(ctxt, *ctxt.env.find(CCP(GET_TEXT(expr))));
					assert(ctxt.env[CCP(GET_TEXT(expr))].valid_in_module
						== ctxt.modules.current);
					return (post_emit_status){ ctxt.env[CCP(GET_TEXT(expr))].cxx_name,
						"true", environment() };
					//return std::make_pair(
					//	reference_bound_variable(ctxt, *ctxt.env.find(CCP(GET_TEXT(expr)))),
					//	"true");
				}
			case CAKE_TOKEN(KEYWORD_THIS):
			case CAKE_TOKEN(KEYWORD_THAT):

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
			
			ambiguous_arity_ops:
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

			unary_al_ops:
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
			
			binary_al_ops:
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
					GET_CHILD(expr, 0)/*,
					shared_ptr<type_die>()*/);
				auto resultR = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1)/*,
					shared_ptr<type_die>()*/);
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
			
			sequencing_ops:
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
			default:
				assert(false);
		}
	}

// 	//void
// 	std::string
// 	//wrapper_file::emit_bound_var_rvalue(
// 	wrapper_file::reference_bound_variable(
// 		const context& ctxt, 
// 		const binding& bound_var) // FIXME: need cxx_result_type too
// 	{
// 		/* There are a few cases here:
// 		 * -- the bound var was defined as an argument;
// 		 * -- the bound var was defined in an infix stub on the remote side;
// 		 * -- the bound var was defined in an infix stub on the local side;
// 		 * -- the bound var was defined in the local stub itself;
// 		 * -- FIXME: stuff to do with return events 
// 		 
// 		 * We disambiguate using our args & other context, 
// 		 * deduce the C++ variable name/prefix that we require,
// 		 * and insert value conversions as necessary. */
// 		
// 		if (ctxt.modules.sink == bound_var.second.defining_module)
// 		{
// 			/* We don't need a value conversion in this case. */
// 			std::string name = cxx_name_for_binding(bound_var);
// 			m_out << name;
// 			//return std::make_pair("true", name);
// 		}
// 		else
// 		{
// 			/* If we need to name a target type every time we open a value conversion,
// 			 * this could get difficult. Or perhaps not -- this could/should be the
// 			 * only place where we do it. So what do we really need to know? Thesis says:
// 			 *
// 			 * "All value correspondences are implicitly invoked from within an event
// 			 * correspondence or another value correspondence. In these contexts, both a
// 			 * "from" and a "to" data type are usually available. Ambiguity in either one
// 			 * of these may therefore be tolerated, if their pairing is sufficient to
// 			 * select a unique correspondence."
// 			 *
// 			 * In other words, whenever we're evaluating an expression, we should have a 
// 			 " "to" data type in mind. But we'll need it only in a few cases. 
// 			 * We hopefully won't need it for intermediate results of the same expression,
// 			 * because we won't be applying value correspondences there.
// 			 * But WAIT: what if our bound variables are deep within some complex stub 
// 			 * expression structure? What's the relationship between the C++ type that
// 			 * we have to convert the bound variable to, and the "overall" required C++ type
// 			 * for the expression? Can we compute this as we're recursing down the
// 			 * expression? It's possible, yes. This means we're doing type-checking of our
// 			 * stub code whether we like it or not! Note that types are always simple
// 			 * because we're not following indirection---pointers are opaque.
// 			 * What about polymorphic operators like arithmetic? If our stub says:
// 			 * foo(a, b) --> { 2 * (a + b) }
// 			 * then how do we disambiguate between potentially many correspondences
// 			 * between typeof(a) and typeof(b) in the RHS component?
// 			 * I think the answer is that for operators, we fall back on "auto" in C++-land
// 			 * which is good for picking up the compiler's treatment of primitive types,
// 			 * but this has the knock-on effect that we require a unique corresponding type
// 			 * for typeof(a) and typeof(b).
// 			 * Can argue that this is reasonable: polymorphic operators are more likely
// 			 * to be working on base types,
// 			 * whereas user-defined types are more likely to have their own specific
// 			 * functions for dealing with them.
// 			 * It'd be a different story if we have foo(a,b) --> f(a,b)
// 			 * because we have a type expectation for each argument.
// 			 * So we are in fact making use of type constraints in our given interfaces
// 			 * to select among value correspondences
// 			 * Python would not solve this problem,
// 			 * and arguably has it worse? Hmm, probably not.
// 			 * This thing about propagating type information should really be in the
// 			 * thesis. I mean, it is, in the paragraph that we quote, but it's not clear
// 			 * enough. A one-sentence addition would be helpful.
// 			 * We could get around this "cleanly" i.e. arse-coveringly
// 			 * by decreeing
// 			 * (1) that Cake's built-in operators are only defined for particular DWARF types;
// 			 * (2) that these must correspond uniquely across components, and
// 			 * (3) not just uniquely, but "sanely" i.e. meaning-preservingly.
// 			 */
// 			assert(result_type);
// 			open_value_conversion(
// 				//ifaces_context,
// 				link_derivation::sorted(ctxt.modules.source, ctxt.modules.sink),
// 				
// 				bound_var.second.type,
// 				bound_var.second.defining_module,
// 				result_type, 
// 				module_of_die(result_type)
// 				);
// 				
// 			std::string name = cxx_name_for_binding(bound_var);
// 			m_out << name;
// 			
// 			close_value_conversion();
// 		}
// 	}
	
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
				i_first++)
		{
			for (auto i_second = second->formal_parameter_children_begin();
				i_second != second->formal_parameter_children_end();
				i_second++)
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
			m_out << get_type_name(*callee_subprogram->get_type())
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
							i_out++)
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
						for (auto i_out = matched_names.begin(); i_out != matched_names.end(); i_out++)
						{
							auto i_next_matched_name = i_out; i_next_matched_name++;
							auto i_next_callee_parameter = i_out->second; i_next_callee_parameter++;
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
							i_arg++;
						} // end do
						while (--args_for_this_ast_node > 0);
					finished_argument_eval_for_current_ast_node:
						break;
				} // end switch
			} // end FOR_ALL_CHILDREN
		} // end INIT argsMultiValue
		m_out << "// end argument eval" << std::endl;
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
			i_result != arg_results.end(); i_result++)
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
					 : get_type_name(*callee_subprogram->get_type())) //return_type_name
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
            
//         	    ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
//                 {
//             	    INIT;
//                     BIND2(n, valuePattern);
//                     switch (GET_TYPE(valuePattern))
//                     {
//                         case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
//                             {
//                         	    // FIXME: better treatment of bound identifiers here
//                                 definite_member_name mn = 
//                                     read_definite_member_name(valuePattern);
//                                 if (mn.size() > 1) RAISE(valuePattern, "may not be compound");
//                                 // mn.at(0) is one of our argument names
//                                 open_value_conversion(
//                             	    // "from" is the calling argument type, "to" is the callee's
//                                     treat_subprogram_as_untyped(source_signature) ? 
//                                         boost::shared_ptr<dwarf::spec::type_die>() : 
//                                 	    env.find(mn.at(0))->second.type,
//                                     ns_prefix + "::" + env.find(mn.at(0))->second.module.second, 
//                                     treat_subprogram_as_untyped(callee_subprogram) ? 
//                                         boost::shared_ptr<dwarf::spec::type_die>() : 
//                                         *(*i_callee_arg)->get_type(),
//                                     callee_prefix);
//                                 m_out << mn.at(0);
//                                 close_value_conversion();
// 	                        } break;
// 
// 
//                         case CAKE_TOKEN(KEYWORD_CONST):
//                             // output the argument type and a dummy name
//                             emit_constant_expr(valuePattern, sink_context);
//                             break;
//                         default: RAISE_INTERNAL(valuePattern, "not a value pattern");
//                     }
//                 }
//                 if (i+1 < childcount) m_out << ", ";
//                 ++i_callee_arg;
//             }
//         }
//         // grab iterator pointing at the first argument...
//         dwarf::encap::formal_parameters_iterator i_callee_arg 
//          = dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*callee).formal_parameters_begin();
// 
//         if (!subprogram_returns_void(callee_subprogram)) open_value_conversion(
//         	callee_return_type ? *callee_return_type : boost::shared_ptr<dwarf::spec::type_die>(), 
//         	callee_prefix,
//             convert_to ? *convert_retval_to : boost::shared_ptr<dwarf::spec::type_die>(), 
//             caller_prefix);
// 
//         
//         if (!subprogram_returns_void(callee_subprogram)) close_value_conversion();
//     }
    
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
}
