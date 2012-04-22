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
using std::istringstream;
using std::cerr;
using std::endl;
using std::clog;
using std::set;

namespace cake
{
	// FIXME: put this somewhere and declare it in a header
	string make_typeof_fragment(const string& cxx_typeof);
	string make_typeof_fragment(const string& cxx_typeof)
	{
		string retval;
		assert(cxx_typeof.length() > 0);
		if (cxx_typeof.at(0) == '*')
		{
			retval = " REMOVE_REF(REMOVE_PTR( __typeof(" 
				+ cxx_typeof.substr(1)
				+ ")))";
		}
		else retval = " __typeof(" + cxx_typeof + ") ";
		
		return retval;
	}
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
			if (subprogram_returns_void(subprogram)) *p_out << "void";
			else if (treat_subprogram_as_untyped(subprogram) && !unique_called_subprogram) *p_out << 
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
					*p_out << get_type_name(found_vec.at(0));
				}
				else 
				{
					*p_out << " ::cake::unspecified_wordsize_type";
				}
			}
            else *p_out << get_type_name(ret_type);
             
            *p_out << ' ';
        }
        *p_out << function_name_to_use << '(';
       
        //auto i_arg = args_begin;
		//dwarf::spec::subprogram_die::formal_parameter_iterator i_fp = 
		//	unique_called_subprogram ? unique_called_subprogram->formal_parameter_children_begin()
		//	: dwarf::spec::subprogram_die::formal_parameter_iterator();
        
        // actually, iterate over the pattern
        assert(GET_TYPE(event_pattern) == CAKE_TOKEN(EVENT_PATTERN));
        INIT;
        BIND3(event_pattern, eventContext, EVENT_CONTEXT);
        BIND2(event_pattern, memberNameExpr); // name of call being matched -- can ignore this here
        BIND3(event_pattern, eventCountPredicate, EVENT_COUNT_PREDICATE);
        BIND3(event_pattern, eventParameterNamesAnnotation, KEYWORD_NAMES);
		int argnum = -1;
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
// 								*p_out << get_type_name(found_vec.at(0));
// 							}
// 							else 
// 							{
// 								std::cerr << "Didn't find unique corresponding type" << std::endl;
// 								if (treat_subprogram_as_untyped(unique_called_subprogram))
// 								{ *p_out << " ::cake::unspecified_wordsize_type"; }
// 								else *p_out << get_type_name((*i_fp)->get_type());
// 							}
// 						}
// 						else  // FIXME: remove duplication here ^^^ vvv
// 						{
// 							if (treat_subprogram_as_untyped(unique_called_subprogram))
// 							{ *p_out << " ::cake::unspecified_wordsize_type"; }
// 							else *p_out << get_type_name((*i_fp)->get_type());
// 						}
// 					}
// 	//				else
// 	//				{
// 	//				
// 	//xxxxxxxxxxxxxxxxxxxxxxxx				
// 	//                     *p_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
// 	//                        (*i_fp)->get_type()));
// 	//					}
// 					if ((*i_fp)->get_name())
// 					{
//                     	// output the variable name, prefixed 
//                     	*p_out << ' ' << arg_name_prefix << argnum /*<< '_' << *(*i_fp)->get_name()*/;
// 					}
// 					else
// 					{
//                     	// output the argument type and a dummy name
//                     	//if (emit_types) *p_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
//                     	//    (*i_fp)->get_type()));
//                     	*p_out << ' ' << arg_name_prefix << argnum /*<< "_dummy"/* << argnum*/;
// 					}
// 					goto next;
assert(false && "disabled support for inferring positional argument mappings");
				}
				else if (!ignore_dwarf_args && unique_called_subprogram)
				{ /* FIXME: Check that they're consistent! */ }
				break;
			default: { // must be >=5
				bool seen_event_pattern_ellipsis = false;
				std::vector<antlr::tree::Tree *> pattern_args;
				/* 
				 * We want to output a signature declaring a number of arguments
				 * that is the maximum of
				 * - the event pattern argcount, and
				 * - the DWARF caller-side arguments.
				 * Since we now merge info from stubs into caller-side DWARF earlier
				 * in the process, we don't need to consider callee-side arguments.
				 */
				FOR_REMAINING_CHILDREN(event_pattern)
				{
					if (GET_TYPE(n) == CAKE_TOKEN(ELLIPSIS))
					{
						/* This means our event pattern need not cover our DWARF args. 
						 * We should keep emitting the DWARF args anyway. */
						seen_event_pattern_ellipsis = true;
						break;
					}
					else pattern_args.push_back(n);
				}
				//pattern_args = GET_CHILD_COUNT(event_pattern) - 4; /* ^ number of bindings above! */
				assert(pattern_args.size() >= 1);
				
				unsigned dwarf_args_count = srk31::count(args_begin, args_end);
				
				// Just write the arguments.
				auto i_fp = args_begin;
				for (unsigned j = 0; j < std::max(dwarf_args_count, (unsigned) pattern_args.size()); ++j)
				{
					if (j > 0) *p_out << ", ";
					if (emit_types)
					{
						if (i_fp != args_end
							&& (*i_fp)->get_type() && !ignore_dwarf_args)
						{
							*p_out << get_type_name(	(*i_fp)->get_type());
						}
						else *p_out << " ::cake::unspecified_wordsize_type";

						*p_out << ' ';
					}
					*p_out << basic_name_for_argnum(j) /*<< "_dummy" << argnum*/;
					
					if (i_fp != args_end) ++i_fp;
				}
// 
// 				FOR_REMAINING_CHILDREN(event_pattern)
// 				{
// 					++argnum;
// 					if (GET_TYPE(n) == CAKE_TOKEN(ELLIPSIS))
// 					{
// 						/* This means our event pattern need not cover our DWARF args. 
// 						 * We should keep emitting the DWARF args anyway. */
// 						seen_event_pattern_ellipsis = true;
// 					}
// 					if (seen_event_pattern_ellipsis)
// 					{
// 						// we just go with default names
// 						// FIXME: could use DWARF info more here? already use it a bit, below
// 						goto write_one_arg_with_default_name;
// 					}
// 					if (!ignore_dwarf_args && i_arg == args_end)
// 					{
// 						std::ostringstream msg;
// 						msg << "argument pattern has too many arguments for subprogram "
// 							<< subprogram;
// 						RAISE(event_pattern, msg.str());
// 					}
// 					{ // protect gotos from initialization
// 						ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
// 						{
// 							INIT;
// 							BIND2(n, valuePattern)
// 							switch(GET_TYPE(valuePattern))
// 							{
// 								// these are all okay -- we don't care 
// 								case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
// 									assert(false);
// 								case CAKE_TOKEN(NAME_AND_INTERPRETATION):
// 								{
// 									INIT;
// 									BIND2(valuePattern, definiteMemberName);
// 									if (GET_TYPE(definiteMemberName) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
// 									{
// 										definite_member_name mn = 
// 											read_definite_member_name(definiteMemberName);
// 										if (mn.size() > 1) RAISE(valuePattern, "may not be compound");
// 										// output the variable type, or unspecified_wordsize_type
// 									} else assert(GET_TYPE(definiteMemberName) == CAKE_TOKEN(ANY_VALUE));
// 									// output the variable name, prefixed 
// 									//*p_out << ' ' << arg_name_prefix << argnum /*<< '_' << mn.at(0)*/;
// 									// NO -- now done by FALL THROUGH
// 								} // FALL THROUGH
// 								case CAKE_TOKEN(ANY_VALUE):
// 								case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
// 								case CAKE_TOKEN(METAVAR):
// 								case CAKE_TOKEN(KEYWORD_CONST):
// 									goto write_one_arg_with_default_name;
// 								default: RAISE_INTERNAL(valuePattern, "not a value pattern");
// 							} // end switch
// 						} // end ALIAS3 
// 					} // end protect initialization
// 					// advance to the next
// 				write_one_arg_with_default_name:
// 					// output the argument type and a dummy name
// 					if (emit_types) *p_out << ((ignore_dwarf_args || !(*i_arg)->get_type()) ? "::cake::unspecified_wordsize_type" : get_type_name(
// 						(*i_arg)->get_type()));
// 					*p_out << ' ' << arg_name_prefix << argnum /*<< "_dummy" << argnum*/;
// 					// HACK: for initial_environment, make sure we have an fp at this position
// 				next:
// 					// work out whether we need a comma
// 					if (!ignore_dwarf_args) 
// 					{	
// 						//std::cerr << "advance DWARF caller arg cursor from " << **i_arg;
// 						++i_arg; // advance DWARF caller arg cursor
// 						//std::cerr << " to ";
// 						//if (i_arg != args_end) std::cerr << **i_arg; else std::cerr << "(sentinel)";
// 						//std::cerr << std::endl;
// 					}
// 					if (ignore_dwarf_args && unique_called_subprogram)
// 					{
// 						++i_fp;
// 						// use DWARF callee arg cursor
// 						if (i_fp != unique_called_subprogram->formal_parameter_children_end())
// 						{
// 							*p_out << ", ";
// 						}
// 					}
// 					else if (ignore_dwarf_args) // && !unique_called_subprogram
// 					{
// 						if (argnum != pattern_args) *p_out << ", ";
// 					}
// 					else 
// 					{
//                 		if (i_arg != args_end) *p_out << ", ";
// 					}
// 					
// 				} // end FOR_REMAINING_CHILDREN
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
// 							*p_out << get_type_name(found_vec.at(0));
// 						}
// 						else 
// 						{
// 							std::cerr << "Didn't find unique corresponding type" << std::endl;
// 							*p_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
//                         		*(*i_fp)->get_type()));
// 						}
// 					}
// 					else  // FIXME: remove duplication here ^^^ vvv
// 					{
// 						*p_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
//                         	*(*i_fp)->get_type()));
// 					}
// 				}
// //				else
// //				{
// //				
// //xxxxxxxxxxxxxxxxxxxxxxxx				
// //                     *p_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
// //                        (*i_fp)->get_type()));
// //					}
// 				if ((*i_fp)->get_name())
// 				{
//                     // output the variable name, prefixed 
//                     *p_out << ' ' << arg_name_prefix << argnum /*<< '_' << *(*i_fp)->get_name()*/;
// 				}
// 				else
// 				{
//                     // output the argument type and a dummy name
//                     //if (emit_types) *p_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
//                     //    *(*i_fp)->get_type()));
//                     *p_out << ' ' << arg_name_prefix << argnum/* << "_dummy"/* << argnum*/;
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
// 						*p_out << ", ";
// 					}
// 				}
// 				else if (ignore_dwarf_args) // && !unique_called_subprogram
// 				{
// 					if (argnum != pattern_args) *p_out << ", ";
// 				}
// 				else 
// 				{
//                 	if (i_arg != args_end) *p_out << ", ";
// 				}
// 			break;
		} // end switch GET_CHILD_COUNT
			
// 		// if we have spare DWARF arguments at the end, something's wrong,
// 		// unless the event pattern has "..."
// 		if (!ignore_dwarf_args && i_arg != args_end)
// 		{
// 			std::ostringstream msg;
// 			msg << "event pattern "
// 				<< CCP(TO_STRING_TREE(event_pattern))
// 				<< " has too few arguments for subprogram: "
// 				<< *subprogram
// 				<< ": processed " << (argnum + 1) << " arguments, and ";
// 			unsigned count = srk31::count(subprogram->formal_parameter_children_begin(),
// 				subprogram->formal_parameter_children_end());
// 			msg << "subprogram has " << count << " arguments (first uncovered: "
// 				<< **i_arg << ").";
// 			RAISE(event_pattern, msg.str());
// 		}

		//} // end switch
		
		*p_out << ')';

		p_out->flush();
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
			corresps.at(0)->second.source->get_ds().toplevel()
				->visible_named_grandchild(wrapped_symname);
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
			 		corresps.at(0)->second.sink->get_ds().toplevel()->resolve_visible(
			 			called_name_vec.begin(), called_name_vec.end()
					)
				);
		} else unique_called_subprogram = boost::shared_ptr<dwarf::spec::subprogram_die>();

		// output prototype for __real_
		*p_out << "extern \"C\" { " << std::endl;
		*p_out << "extern ";
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__real_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, m_d.module_of_die(callsite_signature), true, unique_called_subprogram);
		*p_out << " __attribute__((weak));" << std::endl;
		// output prototype for __wrap_
		*p_out << "namespace cake { namespace " << m_d.namespace_name() << " {" << std::endl;
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__wrap_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, m_d.module_of_die(callsite_signature), true, unique_called_subprogram);
		*p_out << ';' << std::endl;
		*p_out << "} } // end cake and link namespaces" << std::endl;
		*p_out << "} // end extern \"C\"" << std::endl;
		// output wrapper -- put it in the link block's namespace, so it can see definitions etc.
		*p_out << "namespace cake { namespace " << m_d.namespace_name() << " {" << std::endl;
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__wrap_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, m_d.module_of_die(callsite_signature), true, unique_called_subprogram);
		*p_out << std::endl;
		*p_out << " {";
		p_out->inc_level();
		*p_out << std::endl;
		
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
			auto saved_initial_environment = ctxt.env;
			ctxt.opt_source = (context::source_info_s){ callsite_signature, 0 };
			
			// emit a condition
			*p_out << "if (";
			write_pattern_condition(ctxt, pattern); // emit if-test
				
			*p_out << ")" << std::endl;
			*p_out << "{";
			p_out->inc_level();
			*p_out << std::endl;
			
			// our context now has a pattern too
			ctxt.opt_source->opt_pattern = pattern;
			
			// here goes the pre-arrow infix stub
			auto status1 = 
				(source_infix_stub && GET_CHILD_COUNT(source_infix_stub) > 0) ?
					emit_stub_expression_as_statement_list(
						ctxt,
						GET_TYPE(source_infix_stub) == CAKE_TOKEN(INFIX_STUB_EXPR) ? 
							GET_CHILD(source_infix_stub, 0) : source_infix_stub/*,
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
			*p_out << "// source->sink crossover point" << std::endl;
			multimap< string, pair< antlr::tree::Tree *, boost::shared_ptr<dwarf::spec::type_die> > >
			 constraints;
			// for each ident, find constraints
			if (sink_infix_stub) m_d.find_type_expectations_in_stub(ctxt.env, 
				sink, sink_infix_stub, boost::shared_ptr<dwarf::spec::type_die>(), constraints);
			m_d.find_type_expectations_in_stub(ctxt.env, 
				sink, action, boost::shared_ptr<dwarf::spec::type_die>(), constraints);
			/* We also want to know whether any output bindings
			 * -- currently marked as is_pointer_to_uninit --
			 * will be filled by output parameters in the crossed-over stub.
			 * If so, then we should defer allocating co-objects for them.
			 * (This is *not* just marking them as leaf objects.)
			 * Then, when the output is generated on stack,
			 * we cross it over backwards.
			 * Do we need to go through the co-object relation? YES,
			 * because the immediate object may contain pointers that we
			 * care about. */
			vector<string> deferred_out_bindings;
			vector<string> deferred_out_caller_cxxnames;
			identify_and_mark_deferred_out_bindings(
				new_env1,
				action,
				sink_infix_stub,
				deferred_out_bindings,
				deferred_out_caller_cxxnames
			);
				
			ctxt.env = crossover_environment_and_sync(source, new_env1, sink, constraints,
				/* direction_is_out */ false, 
				/* do_not_sync */ false//,
				///* skip_co_objs */ out_bindings
				);

			// here goes the post-arrow infix stub
			auto status2 = 
				(sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub) > 0) ?
					emit_stub_expression_as_statement_list(
						ctxt,
						GET_TYPE(sink_infix_stub) == CAKE_TOKEN(INFIX_STUB_EXPR) ?
							GET_CHILD(sink_infix_stub, 0) : sink_infix_stub
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
			*p_out << "// " << CCP(TO_STRING_TREE(stub)) << std::endl;

			// the sink action is defined by a stub, so evaluate that 
			std::cerr << "Event sink stub is: " << CCP(TO_STRING_TREE(stub)) << std::endl;
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
			*p_out << "// source<-sink crossover point" << std::endl;
			// update environment
			multimap< string, pair< antlr::tree::Tree *, shared_ptr<type_die> > >
				return_constraints;
			if (return_leg && GET_CHILD_COUNT(return_leg) > 0)
			{
				/* Any functions called during the return leg
				 * on things in the env 
				 * may bring type expectations
				 * that we should account for now, when converting. */
				m_d.find_type_expectations_in_stub(ctxt.env, 
					source, return_leg, boost::shared_ptr<dwarf::spec::type_die>(), 
					return_constraints);
			}
			else
			{	
				/* If there's no return leg, we're generating the return value *now*. */
				if (!subprogram_returns_void(ctxt.opt_source->signature))
				{
					*p_out << "// generating return value here, constrained to type "
						<< compiler.name_for(ctxt.opt_source->signature->get_type())
						<< std::endl;
					return_constraints.insert(std::make_pair("__cake_it",
						make_pair((antlr::tree::Tree*)0, ctxt.opt_source->signature->get_type())));
				}
				else
				{
					*p_out << "// crossover logic thinks there's no return value" << std::endl;
				}
			}

			//auto returned_inward = do_virtual_crossover(sink, new_env3, source);
			auto removed_env = reconcile_deferred_out_bindings(new_env3, // <-- this is *modified* 
				sink,
				source,
				deferred_out_bindings, deferred_out_caller_cxxnames);
			ctxt.modules.current = ctxt.modules.source;
			ctxt.env = crossover_environment_and_sync(sink, new_env3, source, return_constraints, 
				true);
			// we pass the old context etc. for cleanup
			cleanup_deferred_out_bindings(removed_env,
				sink,
				source,
				deferred_out_bindings, deferred_out_caller_cxxnames);
			
			std::string final_success_fragment = status3.success_fragment;
			
			*p_out << "// begin return leg of rule" << std::endl;
			// emit the return leg, if there is one; otherwise, status is the old status
			if (return_leg && GET_CHILD_COUNT(return_leg) > 0)
			{
				// HACK-ish: we might have crossed over some value incorrectly on the way out.
				// In particular, if a pointer argument is not used on the sink side, 
				// it will have no type expectations and will be treated as an integer
				// (unspecified_wordsize_type). 
				// So, merge the environment with our initial environment
				// to get the original binding back for these.
				*p_out << "// merging initial environment to avoid so-far-unused pointer args being intified" << endl;
				ctxt.env = merge_environment(ctxt.env, saved_initial_environment);
				if (GET_TYPE(return_leg) == CAKE_TOKEN(RETURN_EVENT))
				{
					return_leg = GET_CHILD(return_leg, 0);
				}
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
			*p_out << "// end return leg of rule" << std::endl;
			
			// emit the return statement
			*p_out << "// begin return logic" << std::endl;
			if (!subprogram_returns_void(ctxt.opt_source->signature)) 
			{
				/* Main problem here is that we don't know the C++ type of the
				 * stub's result value. So we have to make value_convert available
				 * as a function template here. Hopefully this will work. */
				 
				if (ctxt.env.find("__cake_it") == ctxt.env.end())
				{
					// if we have a return value to generate, but nothing to
					// generate it from, generate success with the relevant return value type
					
					assert(ctxt.opt_source->signature->get_type());
					
					*p_out << "return ::cake::success< " 
						<< get_type_name(ctxt.opt_source->signature->get_type())
						<< " >()(); " << endl;
					
					//RAISE(ctxt.opt_source->opt_pattern, 
					//	"cannot synthesise a return value out of no value (void)");
				}
				else
				{
					// now return from the wrapper as appropriate for the stub's exit status
					*p_out << "if (" << final_success_fragment << ") return "
						<< ctxt.env["__cake_it"].cxx_name;

					*p_out << ";" << std::endl;

					*p_out << "else return __cake_failure_with_type_of(" 
						<< ctxt.env["__cake_it"].cxx_name << ");" << std::endl;
					//*p_out << "return "; // don't dangle return
				}
			}
			else
			{
				// but for safety, to avoid fall-through in the wrapper
				*p_out << "return;" << std::endl; 
			}

			/* The sink action should leave us with a return value (if any)
			 * and a success state / error value. We then encode these and
			 * output the return statement *here*. */
			*p_out << "// end return logic" << std::endl;
			p_out->dec_level();
			*p_out << "}" << std::endl;
		}
		
		// 4. if none of the wrapper conditions was matched 
		*p_out << "else ";
		*p_out << "return ";
		write_function_header(
				corresps.at(0)->second.source_pattern,
				"__real_" + *callsite_signature->get_name(),
				callsite_signature,
				wrapper_arg_name_prefix, m_d.module_of_die(callsite_signature), false, unique_called_subprogram);
		*p_out << ';' << std::endl; // end of return statement
		p_out->dec_level();
		*p_out << '}' << std::endl; // end of block
		*p_out << "} } // end link and cake namespaces" << std::endl;
	}
	
	void
	wrapper_file::identify_and_mark_deferred_out_bindings(
		environment& env,
		antlr::tree::Tree *action,
		antlr::tree::Tree *sink_infix_stub,
		vector<string>& out_deferred_out_bindings,
		vector<string>& out_deferred_out_caller_cxxnames
	)
	{
		set<string> out_bindings;
		vector<antlr::tree::Tree *> out_binding_contexts;
		auto grab_out_bindings = [&out_bindings](antlr::tree::Tree *t) {
			if (GET_TYPE(t) == CAKE_TOKEN(KEYWORD_OUT)
			 && GET_TYPE(GET_CHILD(t, 0)) == CAKE_TOKEN(IDENT))
			{
				out_bindings.insert(
					unescape_ident(CCP(GET_TEXT(GET_CHILD(t, 0))))
				);
				return true;
			}
			return false; 
		};
		walk_ast_depthfirst(
			action, 
			out_binding_contexts, 
			grab_out_bindings
		);
		if (sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub) > 0) walk_ast_depthfirst(
			sink_infix_stub, 
			out_binding_contexts, 
			grab_out_bindings
		);
		// can we just use do_not_crossover for this? let's try it
		set<string> cxxnames;
		for (auto i_ident = out_bindings.begin();
			i_ident != out_bindings.end(); ++i_ident)
		{
			auto found = env.find(*i_ident);
			if (found != env.end())
			{
				// for all bindings to this cxxname, set
				cxxnames.insert(found->second.cxx_name);
			}
		} 
		// now for all cxxnames, set do_not_crossover
		for (auto i_b = env.begin(); i_b != env.end(); ++i_b)
		{
			if (cxxnames.find(i_b->second.cxx_name) != cxxnames.end())
			{
				out_deferred_out_bindings.push_back(i_b->first);
				out_deferred_out_caller_cxxnames.push_back(i_b->second.cxx_name);
				i_b->second.do_not_crossover = true;
			}
		}
	
	}
	
	environment
	wrapper_file::reconcile_deferred_out_bindings(
		environment& env,
		module_ptr current_module,
		module_ptr caller_module,
		const vector<string>& deferred_out_bindings,
		const vector<string>& deferred_out_caller_cxxnames
	)
	{
		environment return_env;
		assert(current_module != caller_module);
		/* We're just about to cross over an env back to the caller side. 
		 * Currently there is no relation between the callee-output value
		 * and any caller-side object. So the relevant correspondences
		 * will not be run. Fix this by 
		 * - setting up a co-object relationship between the on-stack out obj
		 *   and the caller-passed pointer. 
		 * - graph-walking will do the rest...? */
		 
		assert(deferred_out_bindings.size() == deferred_out_caller_cxxnames.size());
		for (unsigned i = 0; i < deferred_out_bindings.size(); ++i)
		{
			string cakename = deferred_out_bindings.at(i);
			string cxxname = deferred_out_caller_cxxnames.at(i);
			auto env_found = env.find(cakename);
			// some cakenames, like __cake_argn, we can ignore
			if (env_found == env.end())
			{
				*p_out << "// Warning: was expecting cakename " << cakename 
					<< " in env; okay if we find some other binding with its "
						"cxxname (" << cxxname << ")" << endl;
				continue;
			}
			*p_out << "// handling deferred co-objectification of " 
				<< cakename << endl;
			auto ident = new_ident("rec");
			*p_out << "co_object_group *" << ident << ";" << endl;
			if (env[cakename].guard_cxxname) 
			{
				*p_out << "if (" << *env[cakename].guard_cxxname << ")" << endl;
				*p_out << "{" << endl;
				p_out->inc_level();
			}
			*p_out << ident << " = new_co_object_record(" 
				<< /* initial_object */ "&" << env[cakename].cxx_name << ", "
				<< /* initial_rep */ "REP_ID(" << m_d.name_of_module(current_module) << "), "
				<< /* initial_alloc_by */ "ALLOC_BY_USER, "
				<< /* is_uninit */ "false, "
				<< /* array_length */ "1"
				<< ");" << endl;
			// also add the caller-side object to the group
			*p_out << ident << "->reps[REP_ID(" << m_d.name_of_module(caller_module) << ")] = "
				<< deferred_out_caller_cxxnames.at(i) << ";" << endl;
			*p_out << ident << "->co_object_info[REP_ID(" 
				<< m_d.name_of_module(caller_module) << ")] = "
				<< "(co_object_info){ ALLOC_BY_USER, /* initialized */ 0 };" << endl;
			// HACK: initialized == 0 here is wrong -- we actually don't know whether the
			// pointed-to object has been initialized or not. 
			
			// HACK again: use the stackptr_helper thingy
			auto id = new_ident("stackptr_helper");
			*p_out << "__typeof( &" << env[cakename].cxx_name << " ) *" << id << ";" << endl;
			
			*p_out << "ensure_co_objects_allocated(REP_ID(" 
				<< m_d.name_of_module(current_module) << "), "
				<< "&" << env[cakename].cxx_name << ", "
				<< "&" << id << ", " // <-- stackptr_helper
				<< "REP_ID("
				<< m_d.name_of_module(caller_module) << "), false);" << endl;
				
			if (env[cakename].guard_cxxname) 
			{
				p_out->dec_level();
				*p_out << "}" << endl;
			}
			
			// now we REMOVE the env entry!
			// Why? This is the env entry for the output object. It has now 
			// done its bit. In particular, we *don't* want it to get caught
			// up in the normal crossover logic. 
			// Before removal, save the binding for later cleanup.
			return_env.insert(*env_found);
			env.erase(env_found);
			// When can we invalidate the co-object relationship we just created?
			// Presumably not yet, because we have yet to sync?
			
		}
		return return_env;
	}
	
	void
	wrapper_file::cleanup_deferred_out_bindings(
		environment& env,
		module_ptr current_module,
		module_ptr caller_module,
		const vector<string>& deferred_out_bindings,
		const vector<string>& deferred_out_caller_cxxnames
	)
	{
		for (unsigned i = 0; i < deferred_out_bindings.size(); ++i)
		{
			string cakename = deferred_out_bindings.at(i);
			string cxxname = deferred_out_caller_cxxnames.at(i);
			auto env_found = env.find(cakename);
			if (env_found == env.end())
			{
				continue;
			}
			*p_out << "// handling deferred co-objectification of " 
				<< cakename << endl;
			/* After the sync has happened... remove the 
			 * temporary co-object relationship we created before crossover. */
			assert(env.find(cakename) != env.end());
		
			if (env[cakename].guard_cxxname) 
			{
				*p_out << "if (" << *env[cakename].guard_cxxname << ")" << endl;
				*p_out << "{" << endl;
				p_out->inc_level();
			}
		
			*p_out << "invalidate_co_object("
				<< "&" << env[cakename].cxx_name << ", "
				<< "REP_ID(" << m_d.name_of_module(current_module) << ")"
				<< ");" << endl;
				
			if (env[cakename].guard_cxxname) 
			{
				p_out->dec_level();
				*p_out << "}" << endl;
			}
			
			
		}
	}

	wrapper_file::environment 
	wrapper_file::crossover_environment_and_sync(
		module_ptr old_module,
		const environment& env,
		module_ptr new_module,
		const multimap< string, pair< antlr::tree::Tree*, shared_ptr<type_die> > >& constraints,
		bool direction_is_out,
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
		std::map<std::string, std::set<std::string > > bindings_by_cxxname
		 = group_bindings_by_cxxname(env);

//		// if coming back, do the sync here
//		if (direction_is_out && !do_not_sync)
//		{
//		
//		}
		
		map<string, set< pair<antlr::tree::Tree*, shared_ptr<spec::type_die> > > > 
		constraints_by_cxxname;
		
		// we will put any idents that are pointers that we have called
		// ensure_co_objects_allocated on in here
		vector<string> replaceable_co_obj_ptr_idents;
		
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

			string cur_cxxname = i_cxxname->first;
			
			auto i_first_binding = env.find(*i_cxxname->second.begin());
			assert(i_first_binding != env.end());
			binding representative_binding = *i_first_binding;
		
			// collect virtualness
			bool is_virtual = false;
			for (auto i_binding_name = bindings_by_cxxname[cur_cxxname].begin();
				i_binding_name != bindings_by_cxxname[cur_cxxname].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				
				is_virtual |= i_binding->second.crossover_by_wrapper;
			}

			// sanity check -- for all bindings covered by this cxx name,
			// check that they are valid in the module context we're crossing over *from*
			// also, skip if they're all marked no-crossover
			bool no_crossover = true;
			for (auto i_binding_name = bindings_by_cxxname[cur_cxxname].begin();
				i_binding_name != bindings_by_cxxname[cur_cxxname].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());
				no_crossover &= i_binding->second.do_not_crossover;
				assert(i_binding->second.valid_in_module == old_module);
			}
			if (!is_virtual && no_crossover) continue;
			
			// create a new cxx ident for the binding
			auto ident = new_ident("xover_" + representative_binding.first);
			
			// collect constraints, over all aliases
			set< pair<antlr::tree::Tree*, shared_ptr<spec::type_die> > > all_constraints;
			for (auto i_binding_name = bindings_by_cxxname[cur_cxxname].begin();
				i_binding_name != bindings_by_cxxname[cur_cxxname].end();
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
			constraints_by_cxxname[cur_cxxname] = all_constraints;

			/* Work out the target type expectations, using constraints */
			boost::shared_ptr<dwarf::spec::type_die> precise_to_type;

			// get the constraints defined for this Cake name
			//auto iter_pair = constraints.equal_range(i_binding->first);
			cerr << "Constraints for cxxname " << cur_cxxname << ":" << endl;
			for (auto i_type = all_constraints.begin(); i_type != all_constraints.end(); ++i_type)
			{
				cerr << "Constrained to-type: " << *(*i_type).second << endl;
				cerr << "Concrete: " << *(*i_type).second->get_concrete_type() << endl;
				cerr << "Originating AST: " << ((*i_type).first ? CCP(TO_STRING_TREE((*i_type).first)) : "(none)") << endl;
			}
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
					if (data_types_are_identical(precise_to_type, (*i_type).second)
					||  /* ... or both pointers */ 
					   (precise_to_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type
					 && (*i_type).second->get_concrete_type()->get_tag() == DW_TAG_pointer_type))
					{
						// okay, same DIE, so continue
						continue;
					}
					else
					{
						cerr << "Error: conflicting constraints." << endl;
						cerr << "Established to-type: " << *precise_to_type  << endl;
						cerr << "Concrete: " << *precise_to_type->get_concrete_type() << endl;
						cerr << "Constrained to-type: " << *(*i_type).second << endl;
						cerr << "Concrete: " << *(*i_type).second->get_concrete_type() << endl;
						assert(false);
					}
				}
				else 
				{	
					*p_out << "// in crossover environment, " << representative_binding.first 
						<< " has been constrained to type " 
						<< compiler.name_for(i_type->second)
						<< std::endl;
					// same again, for our log file
					cerr << "in crossover environment, " << representative_binding.first 
						<< " has been constrained to type " 
						<< compiler.name_for(i_type->second)
						<< std::endl;

					precise_to_type = i_type->second;
				}
			}

			// collect pointerness
			bool is_a_pointer = !is_virtual && precise_to_type &&
					 precise_to_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type;
			if (is_a_pointer) cerr << "Cxxname " << cur_cxxname 
				<< " is a pointer because of precise_to_type " << precise_to_type->summary()
				<< endl;
			for (auto i_binding_name = bindings_by_cxxname[cur_cxxname].begin();
				i_binding_name != bindings_by_cxxname[cur_cxxname].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());
				
				// if any binding says we're a pointer, then we are....
				bool this_binding_pointerness = (i_binding->second.pointerness == bound_var_info::IS_A_POINTER);
				if (this_binding_pointerness) cerr << "Cxxname " << cur_cxxname << " is a pointer because of binding "
					<< *i_binding_name << endl;
				is_a_pointer |= this_binding_pointerness;
				
				// BUT this is a problem for virtual objs, which are not pointers,
				// but may still have lingering pointer bindings (from the underlying arg)
			}
			
			// collect typeof
			boost::optional<std::string> collected_cxx_typeof;
			for (auto i_binding_name = bindings_by_cxxname[cur_cxxname].begin();
				i_binding_name != bindings_by_cxxname[cur_cxxname].end();
				++i_binding_name)
			{
				auto i_binding = env.find(*i_binding_name);
				assert(i_binding != env.end());
				
				if (!collected_cxx_typeof) collected_cxx_typeof = i_binding->second.cxx_typeof;
				else assert(*collected_cxx_typeof == i_binding->second.cxx_typeof);
			}
			
			// output co-objects allocation call, if we have a pointer
			if (is_a_pointer && !is_virtual)
			{
				replaceable_co_obj_ptr_idents.push_back(ident);
				
				auto id = new_ident("stackptr_helper");
				*p_out << "__typeof(ensure_is_a_pointer(" << cur_cxxname << ")) *" 
					<< id << ";" << endl;
			
				*p_out << "ensure_co_objects_allocated(REP_ID("
					<< m_d.name_of_module(old_module) << "), ";
				// make sure we invoke the pointer specialization
				/*if (is_a_pointer_this_time)*/ *p_out <<
					"ensure_is_a_pointer(";
				*p_out << cur_cxxname;
				/*if (is_a_pointer_this_time)*/ *p_out << "), ";
				*p_out << "&" << id << ", "; // <-- stackptr_helper
				*p_out << "REP_ID("
					<< m_d.name_of_module(new_module)
					<<  "), "
					// is this an output parameter?
					<< (representative_binding.second.is_pointer_to_uninit ? "true" : "false")
					<< ");" << std::endl;
			}
			
			shared_ptr<type_die> saved_precise_to_type = precise_to_type;
			if (is_virtual && precise_to_type)
			{
				// ditch the precise to-type -- this allows us to get a non-pointer
				// out of the value_convert and then fix things up by taking its address.
				*p_out << "// Discarding precise to-type for virtual corresp" << endl;
				precise_to_type = shared_ptr<type_die>();
			}
			
			string cxxname_to_use = cur_cxxname;
			optional<string> reuse_old_variable;
			// collect replacements 
			for (auto i_cakename = bindings_by_cxxname[cur_cxxname].begin();
					i_cakename != bindings_by_cxxname[cur_cxxname].end();
					++i_cakename)
			{
				// if we are xovering a vxover *back* to a virtual d.t. inst, fix this up
				string cakename_to_use = *i_cakename;
				if (!env[*i_cakename].cxx_name.compare(0, std::min(env[*i_cakename].cxx_name.length(),
				                                         sizeof "vxover_replace_with_" - 1),
				                                                "vxover_replace_with_"))
				{
					std::istringstream in(env[*i_cakename].cxx_name.substr(sizeof "vxover_replace_with_" - 1,
						   string::npos));
					unsigned len;
					in >> len;
					char und;
					in >> und;
					string remainder;
					in >> remainder;
					
					string replacement = remainder.substr(0, len);
					*p_out << "// REPLACE: " << env[*i_cakename].cxx_name 
						<< " with " 
						<< replacement << endl;
					// What was the ident we used before the last crossover? 
					if (!replacement.compare(0, sizeof "xover_" - 1, "xover_"))
					{
						reuse_old_variable = "__cake_" + *i_cakename; // replacement.substr(sizeof "xover_" - 1, string::npos);
						*p_out << "// SUPER HACK: revert to previous ident " << *reuse_old_variable << endl;
					}
						
					// This means that 
					// - any binding using the current cxxname (namely cur_cxxname)
					//   ... should now use "replacement".
					// We *would* just overwrite the cxx_name and cxx_typeof
					// but env is const. So copy i_first_binding above, and use that
					representative_binding.second.cxx_name = replacement;
					representative_binding.second.cxx_typeof = replacement;
					representative_binding.second.do_not_crossover = false;
					cxxname_to_use = replacement;
					collected_cxx_typeof = replacement;
				}
			}
			
			// output initialization
			if (!reuse_old_variable)
			{
				*p_out << "auto" << (is_virtual ? "& " : " ") << ident << " = ";
			}
			// else we use the "to" argument
			/* IF (and only if) we are going to override this guy, 
			 * hack its static type to be more precise than void*. 
			 * (WHY not do this all the time?)
			 * If we are dealing with the outward (i.e. "in" values) case, then we 
			 * have to look ahead to the inward ("out" values),
			 * because if we have an output-only rule, 
			 * we will try to take __typeof( *the-pointer-we're-making-here ) later on. */
			bool will_override = //true; // HACK for ALWAYS_OVERRIDE, but has no effect
				(direction_is_out && (representative_binding.second.indirect_local_tagstring_out
					|| representative_binding.second.indirect_remote_tagstring_out))
			||  (!direction_is_out && (representative_binding.second.indirect_local_tagstring_in
					|| representative_binding.second.indirect_remote_tagstring_in
					|| representative_binding.second.indirect_local_tagstring_out
					|| representative_binding.second.indirect_remote_tagstring_out
					));
			auto ifaces = link_derivation::sorted(new_module, representative_binding.second.valid_in_module);
			bool old_module_is_first = (old_module == ifaces.first);
			// an approximation of non-void pointers
			bool should_reinterpret = is_a_pointer 
				&& representative_binding.second.cxx_typeof != "((void*)0)"
				&& !precise_to_type;
			if (/*will_override*/ should_reinterpret )
			{
				*p_out << "reinterpret_cast< typename ";
				// the "more precise type" is the corresponding type
				// ... to that of *cur_cxxname
				// ... when flowing from the old module to the new module
				// ... using the indirect tagstrings we have for 
				*p_out << "::cake::corresponding_type_to_"
					<< (old_module_is_first ? "first" : "second")
					<< "<" << m_d.component_pair_typename(ifaces) << ", "
					// instead of __typeof(* xxx ) ,
					// which gives an error "not a pointer to object type" in gcc...
					// << "__typeof(*" << cur_cxxname << "), "
					// we use boost::remove_pointer< > :: type
					<< "REMOVE_REF(REMOVE_CV(REMOVE_PTR( __typeof(" << cur_cxxname << ")))), "
					<< (old_module_is_first ? /* DirectionIsFromFirstToSecond */ "true"
					                        : /* DirectionIsFromSecondToFirst */ "true")
					<< ">::"
					<< make_tagstring(direction_is_out ? representative_binding.second.indirect_local_tagstring_out : representative_binding.second.indirect_local_tagstring_in)
					<< "_to_"
					<< make_tagstring(direction_is_out ? representative_binding.second.indirect_remote_tagstring_out : representative_binding.second.indirect_remote_tagstring_in)
					<< "_in_" << (old_module_is_first ? "second" : "first")
					<< " *";
				*p_out << ">(";
			}
			 
			open_value_conversion(
				ifaces,
				//rule_tag, // defaults to 0, but may have been set above
				representative_binding, // from_artificial_tagstring is in our binding -- easy
				direction_is_out,
				false, // is_indirect
				boost::shared_ptr<dwarf::spec::type_die>(), // no precise from type
				precise_to_type, // defaults to "no precise to type", but may have been set above
				((is_a_pointer && !is_virtual) ? std::string("((void*)0)") : *collected_cxx_typeof), // from typeof
				boost::optional<std::string>(), // NO precise to typeof, 
				   // BUT maybe we could start threading a context-demanded type through? 
				   // It's not clear how we'd get this here -- scan future uses of each xover'd binding?
				   // i.e. we only get it *later* when we try to emit some stub logic that uses this binding
				representative_binding.second.valid_in_module, // from_module is in our binding
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
			if (is_a_pointer && !is_virtual) *p_out << "ensure_is_a_pointer(";
			*p_out << cxxname_to_use;
			if (is_a_pointer && !is_virtual) *p_out << ")";
			if (reuse_old_variable) *p_out << ", &" << *reuse_old_variable;
			close_value_conversion();
			if (should_reinterpret) *p_out << ")";
			*p_out << ";" << std::endl;
			
			// for each Cake name bound to the current cxxname,
			// add it to the new environment, bound to some new cxxname
			for (auto i_cakename = bindings_by_cxxname[cur_cxxname].begin();
					i_cakename != bindings_by_cxxname[cur_cxxname].end();
					++i_cakename)
			{
				new_env[*i_cakename] = (bound_var_info) {
					ident,
					ident,
					new_module,
					false,
					env[*i_cakename].pointerness,
					// we swap over the local and remote tagstrings
					/* local */
						(assert(env.find(*i_cakename) != env.end()), 
					env[*i_cakename].remote_tagstring),
					/* remote */ env[*i_cakename].local_tagstring,
					/* indirect local in  */ env[*i_cakename].indirect_remote_tagstring_in,
					/* indirect local out */ env[*i_cakename].indirect_remote_tagstring_out,
					/* remote local in    */ env[*i_cakename].indirect_local_tagstring_in,
					/* remote local out   */ env[*i_cakename].indirect_local_tagstring_out
				};
				// if we are xovering the address of a virtual d.t. inst, fix this up
				if (is_virtual && saved_precise_to_type)
				{
					// also create a pointer alias
					ostringstream s; s << "vxover_replace_with_" << ident.length() << "_" << ident;
					auto pointer_alias_override = new_ident(s.str());
					// copy the old binding, just using the plain old ident...
					new_env[ident] = new_env[*i_cakename];
					new_env[ident].do_not_crossover = true;
					// define a new variable holding its address
					*p_out << "auto " << pointer_alias_override << " = &" << ident << ";" << endl;
					// the new variable should bear the cakename
					new_env[*i_cakename].cxx_name = pointer_alias_override;
					new_env[*i_cakename].cxx_typeof = pointer_alias_override;
				}
			}
		}
		
		// output a summary comment
		*p_out << "/* crossover: " << std::endl;
		for (auto i_el = new_env.begin(); i_el != new_env.end(); ++i_el)
		{
			auto i_old_el = env.find(i_el->first);
			bool is_new = (i_old_el == env.end());
		
			if (is_new) *p_out
				<< "\tNEW: " << i_el->first << " is " << i_el->second.cxx_name << " (";
			else 
			*p_out << "\t" << i_el->first << " is now " << i_el->second.cxx_name << " (";
			*p_out 
				<< (i_el->second.local_tagstring ? " local: " + *i_el->second.local_tagstring : "")
				<< (i_el->second.remote_tagstring ? " remote: " + *i_el->second.remote_tagstring : "")
				<< (i_el->second.indirect_local_tagstring_in ? " local indirect in: " + *i_el->second.indirect_local_tagstring_in : "")
				<< (i_el->second.indirect_remote_tagstring_in ? " remote indirect in: " + *i_el->second.indirect_remote_tagstring_in : "")
				<< (i_el->second.indirect_local_tagstring_out ? " local indirect out: " + *i_el->second.indirect_local_tagstring_out : "")
				<< (i_el->second.indirect_remote_tagstring_out ? " remote indirect out: " + *i_el->second.indirect_remote_tagstring_out : "")
				<< ")" << endl;
			if (!is_new) *p_out << "\t\t" << " having been " << i_old_el->second.cxx_name << " ("
				<< (i_old_el->second.local_tagstring ? " local: " + *i_old_el->second.local_tagstring : "")
				<< (i_old_el->second.remote_tagstring ? " remote: " + *i_old_el->second.remote_tagstring : "")
				<< (i_old_el->second.indirect_local_tagstring_in ? " local indirect in: " + *i_old_el->second.indirect_local_tagstring_in : "")
				<< (i_old_el->second.indirect_remote_tagstring_in ? " remote indirect in: " + *i_old_el->second.indirect_remote_tagstring_in : "")
				<< (i_old_el->second.indirect_local_tagstring_out ? " local indirect out: " + *i_old_el->second.indirect_local_tagstring_out : "")
				<< (i_old_el->second.indirect_remote_tagstring_out ? " remote indirect out: " + *i_old_el->second.indirect_remote_tagstring_out : "")
				<< ")" << endl;
		}
		*p_out << "*/" << endl;
		
		// do the sync
		if (/*!direction_is_out && */ !do_not_sync)
		{
			emit_sync_and_overrides(
				old_module,
				new_module,
				env,
				new_env,
				direction_is_out,
				constraints_by_cxxname,
				replaceable_co_obj_ptr_idents
				);
		}
		
		return new_env;
	}
	
// 	environment
// 	do_virtual_crossover(
// 		module_ptr old_module_context,
// 		const environment& env,
// 		module_ptr new_module_context
// 	)
// 	{
// 		/* Here we explicitly invoke any virtual correspondences required by
// 		 * bindings in the environment. */
// 		
// 		auto bindings_by_cxxname = group_bindings_by_cxxname(env);
// 		for (auto i_cxxname = bindings_by_cxxname.begin(); 
// 			i_cxxname != bindings_by_cxxname.end();
// 			++i_cxxname)
// 		{
// 			auto i_first_binding = env.find(*i_cxxname->second.begin());
// 			assert(i_first_binding != env.end());
// 		
// 			bool is_virtual = false;
// 			for (auto i_binding_name = bindings_by_cxxname[i_cxxname->first].begin();
// 				i_binding_name != bindings_by_cxxname[i_cxxname->first].end();
// 				++i_binding_name)
// 			{
// 				auto i_binding = env.find(*i_binding_name);
// 				
// 				is_virtual |= i_binding->second.crossover_by_wrapper;
// 			}
// 			
// 			if (!is_virtual) continue;
// 			
// 			environment out_env;
// 			auto ident = new_ident("vxover_" + i_first_binding->first);
// 			*p_out << "auto " << ident << " = ";
// 			
// 			open_value_conversion(
// 				link_derivation::iface_pair ifaces,
// 				//int rule_tag,
// 				*i_first_binding,
// 				direction_is_out,
// 				is_indirect,
// 				boost::shared_ptr<dwarf::spec::type_die> from_type, // most precise
// 				boost::shared_ptr<dwarf::spec::type_die> to_type, 
// 				boost::optional<std::string> from_typeof /* = boost::optional<std::string>()*/, // mid-precise
// 				boost::optional<std::string> to_typeof/* = boost::optional<std::string>()*/,
// 				module_ptr from_module/* = module_ptr()*/,
// 				module_ptr to_module/* = module_ptr()*/)
// 			out_env.insert(make_pair(i_first_binding->first, (bound_var_info) {
// 			
// 			}));
// 				
// 
// 		}
// 	}

	std::map<string, std::set<string> > 
	wrapper_file::group_bindings_by_cxxname(const environment& env)
	{
		std::map<std::string, std::set<std::string > > bindings_by_cxxname;
		for (auto i_binding = env.begin(); i_binding != env.end(); ++i_binding)
		{
			bindings_by_cxxname[i_binding->second.cxx_name].insert(i_binding->first);
		}
		return bindings_by_cxxname;
	}
	
	void
	wrapper_file::emit_sync_and_overrides(
		module_ptr old_module,
		module_ptr new_module,
		const environment& old_env,
		const environment& new_env,
		bool direction_is_out,
		const map< string, 
			set< pair<antlr::tree::Tree *, shared_ptr<type_die> > >
		>& constraints_by_cxxname, 
		const vector<string>& replaceable_co_obj_ptr_idents
	)
	{
		auto old_bindings_by_cxxname = group_bindings_by_cxxname(old_env);
		auto new_bindings_by_cxxname = group_bindings_by_cxxname(new_env);
		
		auto notifier_ident = new_ident("notifier");
		*p_out << "co_obj_replacement_notifier " << notifier_ident << " = {";
		for (auto i_ident = replaceable_co_obj_ptr_idents.begin(); 
				i_ident != replaceable_co_obj_ptr_idents.end(); 
				++i_ident)
		{
			if (i_ident != replaceable_co_obj_ptr_idents.begin()) *p_out << ", ";
			*p_out << "(void **) &" << *i_ident;
		}
		*p_out << " };" << endl;
		*p_out << "sync_all_co_objects(make_replacer_cb(" << notifier_ident << "), &" << notifier_ident
			<< ", REP_ID(" << m_d.name_of_module(old_module)
			<< "), REP_ID(" << m_d.name_of_module(new_module) << "), ";
		// for each binding that is being crossed over, and that has an indirect tagstring,
		// we add it to the list
		// NOTE: we use the *old* cxxnames! WHY?
		// because the old environment has the "from" cxxnames
		for (auto i_cxxname = old_bindings_by_cxxname.begin(); 
			i_cxxname != old_bindings_by_cxxname.end(); ++i_cxxname)
		{
			// now we have a group of bindings
			// HACK: just look at the first one, for now
			assert(i_cxxname->second.begin() != i_cxxname->second.end());
			
			auto i_binding = old_env.find(*i_cxxname->second.begin());
			assert(i_binding != old_env.end());

			auto found_constraints = constraints_by_cxxname.find(i_cxxname->first);
			set< pair<antlr::tree::Tree *, shared_ptr<spec::type_die> > > empty_constraints;
			auto& constraints = (found_constraints != constraints_by_cxxname.end())
				? found_constraints->second : empty_constraints;

			// do the constraints analysis up-front, because it reveals more pointerness
			auto precise_to_type = shared_ptr<spec::type_die>();
			shared_ptr<type_die> constrained_to_type;
			if (constraints.size() > 0)
			{
				/* The constraints reflect the pointer, whereas we want 
				 * the pointed-to. */
				constrained_to_type = constraints.begin()->second;
				auto constrained_to_pointer_type = dynamic_pointer_cast<spec::pointer_type_die>(
					constrained_to_type);
				if (constrained_to_type && constrained_to_pointer_type
					&& constrained_to_pointer_type->get_type())
				{
					precise_to_type = constrained_to_pointer_type->get_type();
					*p_out << "// Constrained " << i_cxxname->first
						<< " to " << precise_to_type->summary() << endl;
				}
				else
				{
					*p_out << "// Not constraining because " << constrained_to_type->summary() 
						<< " is not a pointer-to-object type" << endl;
				}
				// FIXME: try other constraints here
				if (constraints.size() > 1)
				{
					*p_out << "// Warning: ignored additional constraints" << endl;
				}
			}

			if ( // HACK for ALWAYS_OVERRIDE
				(direction_is_out && (i_binding->second.indirect_local_tagstring_out
					|| i_binding->second.indirect_remote_tagstring_out))
			||  (!direction_is_out && (i_binding->second.indirect_local_tagstring_in
					|| i_binding->second.indirect_remote_tagstring_in))
				// we just want "is_a_pointer"
// 				i_binding->second.indirect_local_tagstring_out
// 				|| i_binding->second.indirect_local_tagstring_in
// 				|| i_binding->second.pointerness == bound_var_info::IS_A_POINTER
// 				|| (constrained_to_type && constrained_to_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type)
				)
			{
				*p_out << "// override for Cake name " << i_binding->first 
					<< " (source cxx name: " << i_cxxname->first << ")" << endl;
				
				// given some tagstrings and a direction,
				// can we get a C++ expression
				// whose type is the actual pointed-to type
				// NOT if they're both __cake_default... but yes, otherwise
// 				cxx_typeof_given_tagstrings(
// 					ifaces,
// 					first_tagstring,
// 					second_tagstring,
// 					direction);
				
// 				string override_source_pointer_type;
// 				// Since we're overriding, we must know what DWARF type
// 				// is denoted by a particular tagstring. 
// 				// BUT only one or other tagstring need be set. 
// 				definite_member_name mn(1, direction_is_out 
// 					? *i_binding->second.indirect_local_tagstring_out
// 					: *i_binding->second.indirect_local_tagstring_in);
// 				auto found = old_module->get_ds().toplevel()->resolve_visible(mn.begin(), mn.end());
// 				assert(found);
// 				auto found_type = dynamic_pointer_cast<spec::type_die>(found);
// 				assert(found_type);
// 				// get the cxx declarator of a pointer to that thing
// 				// HACK!!
// 				override_source_pointer_type = compiler.cxx_declarator_from_type_die(found_type) + "*";
// 				 
// 				
// 				assert(override_source_pointer_type != "");
				// NOTE that whether ther actually *is* an effective override
				// depends on whether the rule tag selected by the two tagstrings
				// actually is different from the default -- and a similar for init
				
				// NOTE: we're applying "*" to the bound cxxname, so 
				// we no longer know anything about its pointerness.
				// In fact, it should *not* be a pointer....
				auto modified_binding = *i_binding;
				modified_binding.second.pointerness = bound_var_info::UNDEFINED;
				
				
				auto funcname = make_value_conversion_funcname(
						link_derivation::sorted(make_pair(old_module, new_module)),
						modified_binding,
						direction_is_out,
						true, // is_indirect ,i.e. use the indirect tagstrings
						shared_ptr<spec::type_die>(), // from_type
						precise_to_type, // to_type -- likely to be null
						"*" + i_cxxname->first, // must NOT be void* --> requires precise static typing
						optional<string>(),
						old_module,
						new_module);
				*p_out << i_binding->second.cxx_name << ", " << funcname << ", " << funcname << ", ";
			}
		}
		*p_out << "NULL, NULL, NULL);" << std::endl;
		
		*p_out << "// HACK: also mark initialized any co-objects of uninit'd objects" << endl;
		*p_out << "// They won't actually be initialized at least until the stub code has run," << endl;
		*p_out << "// but it makes no difference to us, and saves on compiler pain." << endl;
		for (auto i_cxxname = old_bindings_by_cxxname.begin(); 
			i_cxxname != old_bindings_by_cxxname.end(); ++i_cxxname)
		{
			assert(i_cxxname->second.begin() != i_cxxname->second.end());
			auto i_binding = old_env.find(*i_cxxname->second.begin());
			assert(i_binding != old_env.end());
		
			if (i_binding->second.is_pointer_to_uninit)
			{
				assert(new_env.find(i_binding->first) != new_env.end());
				*p_out << "mark_object_as_initialized("
					<< new_env[i_binding->first].cxx_name
					<< ", REP_ID(" << m_d.name_of_module(new_module) << "));" << endl;
			}
		}
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
				*p_out << "// warning: merging environment hides old binding of " 
					<< i_new->first << std::endl;
			}
			new_env[i_new->first] = i_new->second;
		}
		
		return new_env;
	}
	
	void
	wrapper_file::infer_tagstrings(
		antlr::tree::Tree *in_ast,                 // optional
		shared_ptr<type_die> contextual_dwarf_type, // also optional
		module_ptr p_module, // not optional
		optional<string>& direct_tagstring,
		optional<string>& indirect_tagstring_in,
		optional<string>& indirect_tagstring_out
		)
	{
		assert(p_module);
		
		cerr << "called infer_tagstrings(" << endl
			<< (in_ast ? CCP(TO_STRING_TREE(in_ast)) : "(no ast)") << ", ";
		if (contextual_dwarf_type) cerr << *contextual_dwarf_type;
		else cerr << "(no DWARF type)";
		cerr << ")" << endl;
			

		// output is
		// a named target (perhaps null)
		// a direct tagstring (perhaps none)
		// an indirect tagstring in (perhaps none)
		// an indirect tagstring out (perhaps none)

		shared_ptr<type_die> interpretation_named_target;
		shared_ptr<type_die> interpretation_unqualified_target;
		shared_ptr<type_die> interpretation_concrete_target;

		// 1. check AST
		if (in_ast)
		{
			// extract tagstring
			interpretation_named_target
			 = p_module->ensure_dwarf_type(GET_CHILD(in_ast, 0));
		}

		// 2. check DWARF, and warn if overridden by AST
		// Is there a typedef in the caller-side arguments? treat it the same if so
		if (contextual_dwarf_type 
		&& contextual_dwarf_type->get_concrete_type()
			 != contextual_dwarf_type->get_unqualified_type())
		{
			if (interpretation_named_target)
			{
				// emit a warning
				cerr << "Warning: artificial typename " 
					<< *interpretation_named_target->get_name()
					<< " overrides typedef "
					<< *dynamic_pointer_cast<typedef_die>(
							contextual_dwarf_type->get_unqualified_type()
						)
					<< endl;
			}
			else
			{
				interpretation_named_target = contextual_dwarf_type;
			}
		}

		if (interpretation_named_target)
		{
			interpretation_unqualified_target
			 = interpretation_named_target->get_unqualified_type();
			interpretation_concrete_target 
			 = interpretation_named_target->get_concrete_type();
		}

		// does the interpreted-to type refer to the target of a pointer?
		// YES if it is not a pointer, but we are. 
		// Only do this for interps coming from the AST, not from DWARF typedefs.
		bool interp_is_indirect
		 = in_ast && 
			(contextual_dwarf_type->get_concrete_type()->get_tag() 
			 == DW_TAG_pointer_type)
			&& (interpretation_concrete_target->get_tag()
					 != DW_TAG_pointer_type);

		optional<string> tagstring_to_assign = 
				// we only set the tagstrings if the interpretation
				// is directing us to a non-concrete type. 
			// (cf. just an annotation)
			(interpretation_named_target && 
			   (interpretation_unqualified_target
			   != interpretation_concrete_target))
			? /* is a typedef, or qualified typedef */ interpretation_named_target->get_name()
			: optional<string>();

		if (tagstring_to_assign)
		{
			cerr << "about to assign tagstring: " << *tagstring_to_assign
				<< ", indirect? " << interp_is_indirect << endl;
		}

		if (!in_ast || (
			GET_TYPE(in_ast) == CAKE_TOKEN(KEYWORD_AS)
		|| GET_TYPE(in_ast) == CAKE_TOKEN(KEYWORD_INTERPRET_AS)
		|| GET_TYPE(in_ast) == CAKE_TOKEN(KEYWORD_IN_AS)))
		{
			(interp_is_indirect ? indirect_tagstring_in : direct_tagstring)
			 = tagstring_to_assign;
		}
		if (!in_ast || ( 
		   GET_TYPE(in_ast) == CAKE_TOKEN(KEYWORD_AS)
		|| GET_TYPE(in_ast) == CAKE_TOKEN(KEYWORD_INTERPRET_AS)
		|| GET_TYPE(in_ast) == CAKE_TOKEN(KEYWORD_OUT_AS)))
		{
			(interp_is_indirect ? indirect_tagstring_out : direct_tagstring)
			 = tagstring_to_assign;
		}

	}
	
	string wrapper_file::basic_name_for_argnum(int argnum)
	{
		std::ostringstream s; s << wrapper_arg_name_prefix << argnum;
		return s.str();
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

		cerr << "Creating initial environment for event pattern " 
			<< CCP(TO_STRING_TREE(pattern))
			<< endl;

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
			auto caller = source_module->get_ds().toplevel()->resolve_visible(
				call_mn.begin(), call_mn.end());
			if (!caller) RAISE(memberNameExpr, "does not name a visible function");
			if ((*caller).get_tag() != DW_TAG_subprogram) 
				RAISE(memberNameExpr, "does not name a visible function"); 
			auto caller_subprogram = boost::dynamic_pointer_cast<subprogram_die>(caller);
			
			struct virtual_value_construct
			{
				antlr::tree::Tree *expr;
				optional<string> overall_cake_name;
				string virtual_typename;
				vector<string> idents;
				vector<string> cxxnames;
				vector<string> cake_basic_names;
				vector<optional<string> > cake_friendly_names;
				vector<bool> indirectnesses;
			};
			
			vector<virtual_value_construct> virtual_value_constructs;
			
			int argnum = 0;
			auto i_caller_arg = caller_subprogram->formal_parameter_children_begin();
			//int dummycount = 0;
			bool arg_is_outparam = false;
			bool add_excess_dwarf_args = false;
			FOR_REMAINING_CHILDREN(eventPattern)
			{
				//boost::shared_ptr<dwarf::spec::type_die> p_arg_type = boost::shared_ptr<dwarf::spec::type_die>();
				boost::shared_ptr<dwarf::spec::program_element_die> p_arg_origin;
				if (GET_TYPE(n) == CAKE_TOKEN(ELLIPSIS))
				{
					/* This means we need to add any arguments that are in the DWARF 
					 * signature but not explicitly in the event pattern. */
					add_excess_dwarf_args = true;
					break;
				}
				
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
				if (origin_as_fp)
				{
					// HACK: our disgusting way of encoding output parameters
					if (origin_as_fp->get_is_optional() && *origin_as_fp->get_is_optional()
					 && origin_as_fp->get_variable_parameter() && *origin_as_fp->get_variable_parameter()
					 && origin_as_fp->get_const_value() && *origin_as_fp->get_const_value())
					{
						arg_is_outparam = true;
					}
				}
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
					string basic_name = basic_name_for_argnum(argnum);
					
					antlr::tree::Tree *interpretation_ast = 0;
					
					optional<string> local_tagstring;
					optional<string> indirect_local_tagstring_in;
					optional<string> indirect_local_tagstring_out;
					switch(GET_TYPE(valuePattern))
					{
						case CAKE_TOKEN(DEFINITE_MEMBER_NAME): assert(false);
						case CAKE_TOKEN(NAME_AND_INTERPRETATION): {
							// could match anything, so bind name and continue
							INIT;
							BIND2(valuePattern, memberName);
							if (GET_TYPE(memberName) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
							{
								definite_member_name mn = read_definite_member_name(memberName);
								if (mn.size() != 1) RAISE(memberName, "may not be compound");
								friendly_name = mn.at(0);
							}
							else assert(GET_TYPE(memberName) == CAKE_TOKEN(ANY_VALUE));
							
							if (GET_CHILD_COUNT(valuePattern) > 1)
							{
								BIND2(valuePattern, interpretation); assert(interpretation);
								interpretation_ast = interpretation;
								assert(GET_TYPE(interpretation) == CAKE_TOKEN(KEYWORD_AS)
									|| GET_TYPE(interpretation) == CAKE_TOKEN(KEYWORD_INTERPRET_AS)
									|| GET_TYPE(interpretation) == CAKE_TOKEN(KEYWORD_IN_AS)
									|| GET_TYPE(interpretation) == CAKE_TOKEN(KEYWORD_OUT_AS));
								// is there a "VALUE_CONSTRUCT" here?
								// If so, we will suppress or remove some items from the env here
								if (GET_CHILD_COUNT(interpretation) > 1)
								{
									INIT;
									BIND2(interpretation, type_interpreted_to);
									BIND3(interpretation, value_construct, VALUE_CONSTRUCT);
									if (GET_CHILD_COUNT(value_construct) > 0)
									{
										assert(GET_TYPE(type_interpreted_to) == CAKE_TOKEN(IDENT));
										virtual_value_constructs.push_back((virtual_value_construct) {
											/* .expr = */ value_construct,
											/* .overall_cake_name = */ friendly_name,
											/*.virtual_typename = */ unescape_ident(CCP(GET_TEXT(type_interpreted_to))),
										});
									}
									FOR_ALL_CHILDREN(value_construct)
									{
										auto ident = unescape_ident(CCP(GET_TEXT(n)));
										virtual_value_constructs.back().idents.push_back(
											ident);
										/* To turn an ident into a basic name, we search
										 * for a fp with this ident, and use its "__cake_arg<num>"
										 * name. */
										int found_argnum = 0;
										string basic_name;
										subprogram_die::formal_parameter_iterator i_search_fp;
										for (i_search_fp = caller_subprogram->formal_parameter_children_begin();
											i_search_fp != caller_subprogram->formal_parameter_children_end();
											++i_search_fp, ++found_argnum)
										{
											if ((*i_search_fp)->get_name()
											 && *(*i_search_fp)->get_name() == ident)
											{
												basic_name = basic_name_for_argnum(found_argnum);
												break;
											}
										}
										if (i_search_fp == caller_subprogram->formal_parameter_children_end())
										{
											RAISE(n, "does not match any argument");
										}
										
										virtual_value_constructs.back().cxxnames.push_back(
											basic_name);
										virtual_value_constructs.back().cake_basic_names.push_back(
											basic_name);
										virtual_value_constructs.back().cake_friendly_names.push_back(
											friendly_name);
										virtual_value_constructs.back().indirectnesses.push_back(
											arg_is_indirect(*i_search_fp));
									}
									/* The names used inside the VALUE_CONSTRUCT come from
									 * the subprogram. We map them to 
									 * - the cxxnames that will be emitted for them;
									 * - the (non-friendly) Cake names that will be emitted for them;
									 * This will allow us to emit the virtual structure
									 * *and* suppress the Cake names from the environment.
									 * Actually, we work by removing unfriendly names. */
								}
							}
							break;
						} break;
						case CAKE_TOKEN(KEYWORD_CONST):
						case CAKE_TOKEN(ANY_VALUE):
						case CAKE_TOKEN(INDEFINITE_MEMBER_NAME): {
							// we will bind a basic name but not a friendly one
						} break;
						default: RAISE(valuePattern, "unexpected token");
					} // end switch
					
					// infer source (local) tagstrings
					infer_tagstrings(
						interpretation_ast,
						i_caller_arg != caller_subprogram->formal_parameter_children_end()
							?  (*i_caller_arg)->get_type()
							: shared_ptr<type_die>(),
						source_module,
						local_tagstring,
						indirect_local_tagstring_in,
						indirect_local_tagstring_out
					);
					
					/* Now we have assigned local tagstrings, but not remote tagstrings.
					 * To get the remote tagstring, we scan for uses of 
					 *  the bound name (Cake name) in the right-hand side. */
					vector<antlr::tree::Tree *> out;
					optional<string> remote_tagstring;
					optional<string> indirect_remote_tagstring_in;
					optional<string> indirect_remote_tagstring_out;
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
			
					// now reduce them to a single one --
					// by unanimity for now (FIXME: change to union a.k.a. "once is enough"?)
					shared_ptr<type_die> found_type;
					optional<unsigned> seen_interp_flavour;
					antlr::tree::Tree *seen_interp_type_ast = 0;
					antlr::tree::Tree *representative_ast = 0;
					for (auto i_ctxt = out.begin(); i_ctxt != out.end(); ++i_ctxt)
					{
						cerr << "context is " << CCP(GET_TEXT(*i_ctxt))
							<< ", full tree: " << CCP(TO_STRING_TREE(*i_ctxt)) 
							<< ", with parent: " <<  CCP(TO_STRING_TREE(GET_PARENT(*i_ctxt))) 
							<< endl;
						assert(CCP(GET_TEXT(*i_ctxt)) == basic_name
						||     (friendly_name && CCP(GET_TEXT(*i_ctxt)) == *friendly_name));
						
						/* Just like before, we care about both the DWARF context
						 * *and* any interpretations ("as") in the AST, and we look for
						 * the AST ones first. */
						if (!seen_interp_flavour) seen_interp_flavour = GET_TYPE(GET_PARENT(*i_ctxt));
						else if (*seen_interp_flavour != GET_TYPE(GET_PARENT(*i_ctxt)))
						{ RAISE(*i_ctxt, "interpretation does not agree with previous uses of ident"); }
						
						/* If the context of the use of this argname is an interpretation... */
						if (GET_TYPE(GET_PARENT(*i_ctxt)) == CAKE_TOKEN(KEYWORD_AS)
						||  GET_TYPE(GET_PARENT(*i_ctxt)) == CAKE_TOKEN(KEYWORD_INTERPRET_AS)
						||  GET_TYPE(GET_PARENT(*i_ctxt)) == CAKE_TOKEN(KEYWORD_IN_AS)
						||  GET_TYPE(GET_PARENT(*i_ctxt)) == CAKE_TOKEN(KEYWORD_OUT_AS))
						{
							antlr::tree::Tree *interp_type_ast = GET_CHILD(GET_PARENT(*i_ctxt), 1);
							if (!seen_interp_type_ast)
							{
								seen_interp_type_ast = interp_type_ast;
								// set it to point to the top of the interpretation
								representative_ast = GET_PARENT(*i_ctxt);
							}
							else
							{
								assert(representative_ast);
								if (string(CCP(GET_TEXT(seen_interp_type_ast)))
								   != string(CCP(GET_TEXT(interp_type_ast))))
								{
									RAISE(*i_ctxt, 
										"interpretation does not agree with previous uses of ident");
								}
							}
						}
						else /* the argname's usage is not in an interpretation, but 
						        we remember it anyway. */
						{
							// We might still want to keep this AST  (-----WHY??)
							// ... set it to point to the function expression argument
							if (!representative_ast) representative_ast = *i_ctxt;
							else 
							{
								// discard the representative if we're not in consensus?
								// Or keep the earlier one, since we seem to want 
								// representative_ast to be set if 
								// 
								if (*i_ctxt != representative_ast)
								{
									//representative_ast = 0;
									//break;
									cerr << "Warning: usage " 
										<< CCP(TO_STRING_TREE(*i_ctxt))
										<< " does not perform consistent interpretation "
										<< "of an underlying ident, "
										<< "w.r.t. context(s) seen earlier: " 
										<< CCP(TO_STRING_TREE(representative_ast)) << endl;
								}
							}
						}
					}
					// now we have a single representative AST node (or null)
					// that we can use to infer what tagstrings apply to this bound value
					shared_ptr<spec::basic_die> found;
					if (representative_ast) found = map_ast_context_to_dwarf_element(
						representative_ast, remote_module, false);
					shared_ptr<formal_parameter_die> found_fp
					 = dynamic_pointer_cast<formal_parameter_die>(found);
					infer_tagstrings(
						seen_interp_type_ast ? representative_ast : 0, // only pass interp ASTs, not vanilla parameter ASTs
						found_fp ? found_fp->get_type() : shared_ptr<type_die>(),
						remote_module,
						remote_tagstring,
						indirect_remote_tagstring_in,
						indirect_remote_tagstring_out
					);
					
					bool is_definitely_a_pointer
					 = indirect_local_tagstring_in
					 || indirect_local_tagstring_out
					 || indirect_remote_tagstring_in
					 || indirect_remote_tagstring_out
					 || (origin_as_fp && origin_as_fp->get_type() 
					 	&& origin_as_fp->get_type()->get_concrete_type()
						&& origin_as_fp->get_type()->get_concrete_type()->get_tag()
							== DW_TAG_pointer_type)
					 || (found_fp && found_fp->get_type() 
					 	&& found_fp->get_type()->get_concrete_type()
					 	&& found_fp->get_type()->get_concrete_type()->get_tag()
							 == DW_TAG_pointer_type);
					cerr << "Argument " << basic_name 
						<< " generated by pattern " << CCP(TO_STRING_TREE(pattern))
						<< " is definitely " << (is_definitely_a_pointer ? "" : "NOT ")
						<< " a pointer; found_fp: " << (found_fp ? found_fp->summary() : "none")
						<< "; origin_as_fp: " << (origin_as_fp ? origin_as_fp->summary() : "none")
						<< endl;
					
					out_env->insert(std::make_pair(basic_name, 
						(bound_var_info) { basic_name, // use the same name for both
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module,
						false, // do_not_crossover
						// pointerness
						is_definitely_a_pointer ? bound_var_info::IS_A_POINTER : bound_var_info::UNDEFINED,
						local_tagstring,
						remote_tagstring, 
						indirect_local_tagstring_in,
						indirect_local_tagstring_out,
						indirect_remote_tagstring_in,
						indirect_remote_tagstring_out,
						arg_is_outparam /* is_pointer_to_uninit */}));
					if (friendly_name) out_env->insert(std::make_pair(*friendly_name, 
						(bound_var_info) { basic_name,
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module,
						false, // do_not_crossover
						// pointerness
						is_definitely_a_pointer ? bound_var_info::IS_A_POINTER : bound_var_info::UNDEFINED,
						local_tagstring,
						remote_tagstring, 
						indirect_local_tagstring_in,
						indirect_local_tagstring_out,
						indirect_remote_tagstring_in,
						indirect_remote_tagstring_out,
						arg_is_outparam /* is pointer to uninit */}));
				} // end ALIAS3(annotatedValuePattern
				++argnum;
				if (i_caller_arg != caller_subprogram->formal_parameter_children_end()) ++i_caller_arg;
			} // end FOR_REMAINING_CHILDREN(eventPattern
			if (add_excess_dwarf_args)
			{
				int count = 0;
				
				while (i_caller_arg != caller_subprogram->formal_parameter_children_end())
				{
					auto basic_name = basic_name_for_argnum(argnum);
					out_env->insert(std::make_pair(basic_name, 
						(bound_var_info) { basic_name, // use the same name for both
						basic_name, // p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
						source_module,
						false,
						bound_var_info::UNDEFINED,
						optional<string>(),
						optional<string>(),
						optional<string>(),
						optional<string>(),
						optional<string>(),
						optional<string>(),
						false,
						false,
						optional<int>(count++)
						 })); 
					++i_caller_arg;
					++argnum;
				}
			}
			cerr << "/* Initial environment before virtualisation: " << endl;
			for (auto i_el = out_env->begin(); i_el != out_env->end(); ++i_el)
			{
				cerr << "\t" << i_el->first << " is now " << i_el->second.cxx_name << " ("
					<< (i_el->second.local_tagstring ? " local: " + *i_el->second.local_tagstring : "")
					<< (i_el->second.remote_tagstring ? " remote: " + *i_el->second.remote_tagstring : "")
					<< (i_el->second.indirect_local_tagstring_in ? " local indirect in: " + *i_el->second.indirect_local_tagstring_in : "")
					<< (i_el->second.indirect_remote_tagstring_in ? " remote indirect in: " + *i_el->second.indirect_remote_tagstring_in : "")
					<< (i_el->second.indirect_local_tagstring_out ? " local indirect out: " + *i_el->second.indirect_local_tagstring_out : "")
					<< (i_el->second.indirect_remote_tagstring_out ? " remote indirect out: " + *i_el->second.indirect_remote_tagstring_out : "")
					<< ")" << std::endl;
			}
			cerr << "*/" << endl;
			
			/* Now process the virtual value constructs. */
			for (auto i_virt = virtual_value_constructs.begin(); 
				i_virt != virtual_value_constructs.end(); 
				++i_virt)
			{
// 				antlr::tree::Tree *expr;
// 				optional<string> overall_cake_name;
// 				string virtual_typename;
// 				vector<string> idents;
// 				vector<string> cxxnames;
// 				vector<string> cake_basic_names;
// 				vector<optional<string> > cake_friendly_names;

				// fix up the overall name from the environment, if there
				assert(i_virt->overall_cake_name);
				
				//{
					auto virtual_fq_typename =  ns_prefix + "::" 
						+ m_d.name_of_module(source_module) + "::" + i_virt->virtual_typename;
					auto found = out_env->find(*i_virt->overall_cake_name);
					assert(found != out_env->end());
					// To what should the overall Cake name now refer? 
					// To the new object, of course!
					found->second.cxx_name = " __cake_" + *i_virt->overall_cake_name;
					found->second.cxx_typeof = "(*((" + virtual_fq_typename + "*)(void*)0))";
					// Set as "do not crossover" --
					// We will manage its crossover
					found->second.do_not_crossover = true;
					found->second.crossover_by_wrapper = true;
					found->second.pointerness = bound_var_info::NOT_A_POINTER;
				
				// Now delete any other bindings of this cakename, because they will
				// have wrong info.
				++found;
				while (found != out_env->end())
				{
					if (found->first == *i_virt->overall_cake_name)
					{
						cerr << "Removing non-virtual binding of " << 
							*i_virt->overall_cake_name
							<< " to " << found->second.cxx_name << endl;
						found = out_env->erase(found);
					}
					else ++found;
				}
				
				
				//}
				
				// begin declaration of virtual struct
				*p_out << virtual_fq_typename << " __cake_" << *i_virt->overall_cake_name 
					<< " = " /* "(" << virtual_fq_typename << ") " */ " {" << endl;
				definite_member_name virtual_typename_dmn(1, i_virt->virtual_typename);
				auto virtual_type_die_found = source_module->get_ds().toplevel()->resolve_visible(
					virtual_typename_dmn.begin(), virtual_typename_dmn.end());
				assert(virtual_type_die_found);
				auto virtual_type_die = dynamic_pointer_cast<structure_type_die>(virtual_type_die_found);
				assert(virtual_type_die);
				p_out->inc_level();
				
				// for each param
				vector<string>::iterator i_cxxname
				 = i_virt->cxxnames.begin();
				vector<string>::iterator i_cake_basic_name
				 = i_virt->cake_basic_names.begin();
				vector<optional<string> >::iterator i_cake_friendly_name
				 = i_virt->cake_friendly_names.begin();
				structure_type_die::member_iterator i_memb
				 = virtual_type_die->member_children_begin();
				vector<bool>::iterator i_indirect 
				 = i_virt->indirectnesses.begin();
				 	
				/* IMPORTANT: we must visit the idents in the same order that we did
				 * when creating the struct, i.e. in syntax order. */
				for (auto i_ident = i_virt->idents.begin();
					i_ident != i_virt->idents.end();
					++i_ident, ++i_cxxname, ++i_cake_basic_name, ++i_cake_friendly_name,
					++i_memb, ++i_indirect)
				{
					assert(i_memb != virtual_type_die->member_children_end());
					
					// we don't erase the basic name, because the event pattern test
					// will want to use it. But we set it as do-not-crossover
					//assert(out_env->find(*i_cake_basic_name) != out_env->end());
					auto found_binding = out_env->find(*i_cake_basic_name);
					//out_env->erase(*i_cake_basic_name);
					found_binding->second.do_not_crossover = true;
					
					//if (*i_cake_friendly_name) out_env->erase(**i_cake_friendly_name);
					
					// output an initialization line for this member
					if (i_ident != i_virt->idents.begin()) *p_out << ", ";
					*p_out << endl;
					*p_out << "/* ." << *i_ident << " = */"; 
					//         ^--uncomment when C99-style initializers come to c++
					
					// insert reinterpret_cast if necessary
					assert((*i_memb)->get_type());
					auto member_type_as_reference_type
					 = dynamic_pointer_cast<reference_type_die>((*i_memb)->get_type());
					assert(member_type_as_reference_type);
					cerr << "Member type-of-reference-target: " 
						<< *member_type_as_reference_type->get_type()->get_concrete_type() << endl;
					// we reinterpret-cast pointer references to make sure they compile
					bool do_reinterpret
					 = (member_type_as_reference_type->get_type()->get_concrete_type()->get_tag()
						== DW_TAG_pointer_type);
					if (do_reinterpret)
					{
						*p_out << "reinterpret_cast< "
						// HACK: we might not have added this type until after the .o.hpp #includes,
						// so write out its declarator directly.
							//<< ns_prefix << "::" << m_d.name_of_module(source_module) << "::"
							<< get_type_name(member_type_as_reference_type->get_type()) //<< "&"
							//	compiler.cxx_declarator_from_type_die(
							//		member_type_as_reference_type->get_concrete_type(),
							//		optional<const string &>(),
							//		true,
							//		optional<const string &>(),
							//		false)
							<< " &>(";
					}
					
					// if this binding is indirect, dereference it.
					// FIXME: support nulls 
					/*if (found_binding->second.indirect_local_tagstring_in
					 || found_binding->second.indirect_remote_tagstring_in)*/
					 if (*i_indirect) *p_out << "*";
					 *p_out << *i_cake_basic_name;
					 
					if (do_reinterpret) *p_out << ")";
				}
				
				p_out->dec_level();
				*p_out << endl << "};" << endl;
			}
			
		} // end ALIAS3(pattern, eventPattern, EVENT_PATTERN)
		
		/* Write a comment to the output file, describing the initial environment. */
		cerr << "/* Initial environment: " << endl;
		for (auto i_el = out_env->begin(); i_el != out_env->end(); ++i_el)
		{
			cerr << "\t" << i_el->first << " is now " << i_el->second.cxx_name << " ("
				<< (i_el->second.local_tagstring ? " local: " + *i_el->second.local_tagstring : "")
				<< (i_el->second.remote_tagstring ? " remote: " + *i_el->second.remote_tagstring : "")
				<< (i_el->second.indirect_local_tagstring_in ? " local indirect in: " + *i_el->second.indirect_local_tagstring_in : "")
				<< (i_el->second.indirect_remote_tagstring_in ? " remote indirect in: " + *i_el->second.indirect_remote_tagstring_in : "")
				<< (i_el->second.indirect_local_tagstring_out ? " local indirect out: " + *i_el->second.indirect_local_tagstring_out : "")
				<< (i_el->second.indirect_remote_tagstring_out ? " remote indirect out: " + *i_el->second.indirect_remote_tagstring_out : "")
				<< ")" << std::endl;
		}
		cerr << "*/" << endl;
		
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
				if (GET_TYPE(n) == CAKE_TOKEN(ELLIPSIS)) break;
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
							BIND2(valuePattern, memberName);
							if (GET_TYPE(memberName) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
							{
								definite_member_name mn = read_definite_member_name(memberName);
								bound_name = mn.at(0);
							}
							else
							{
								assert(GET_TYPE(memberName) == CAKE_TOKEN(ANY_VALUE)
								||     GET_TYPE(memberName) == CAKE_TOKEN(INDEFINITE_MEMBER_NAME));
								bound_name = basic_name_for_argnum(argnum);
							}
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
							if (emitted) *p_out << " && ";
							*p_out << "cake::equal<";
							//if (p_arg_type) *p_out << get_type_name(p_arg_type);
							//else *p_out << " ::cake::unspecified_wordsize_type";
							*p_out << make_typeof_fragment(bound_name);
							*p_out << ", "
 << (	(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(STRING_LIT)) ? " ::cake::style_traits<0>::STRING_LIT" :
		(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(CONST_ARITH)) ? " ::cake::style_traits<0>::CONST_ARITH" :
		" ::cake::unspecified_wordsize_type" );
							*p_out << ">()(";
							//*p_out << "arg" << argnum << ", ";
							*p_out << ctxt.env[bound_name].cxx_name << ", ";
							*p_out << constant_expr_eval_and_cxxify(ctxt, valuePattern);
							*p_out << ")";
							emitted = true;
						} break;
						default: assert(false); 
						break;
					} // end switch
				} // end ALIAS3
			}
		}
		if (!emitted) *p_out << "true";
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
// 		*p_out << "if (__cake_p_to) *__cake_p_to = *reinterpret_cast<const " 
// 			<< get_type_name(target_type) << "*>(&__cake_from);" << std::endl
// 			<< "return *reinterpret_cast<const " 
// 			<< get_type_name(target_type) << "*>(&__cake_from);" << std::endl;
// 	}
	
	wrapper_file::value_conversion_params_t
	wrapper_file::resolve_value_conversion_params(
		link_derivation::iface_pair ifaces,
		//int rule_tag,
		const binding& source_binding,
		bool direction_is_out,
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
		if (!from_module && from_type) from_module = m_d.module_of_die(from_type);
		if (!to_module && to_type) to_module = m_d.module_of_die(to_type);
		assert(from_module);
		assert(to_module);
		// check consistency
		assert((!from_type || m_d.module_of_die(from_type) == from_module)
		&& (!to_type || m_d.module_of_die(to_type) == to_module));
		// we must have either a from_type of a from_typeof
		assert(from_type || from_typeof);
		// ... this is NOT true for to_type: if we don't have a to_type or a to_typeof, 
		// we will use use the corresponding_type template
		std::string from_typestring = from_type ? ("REMOVE_REF(" + get_type_name(from_type) + ")")
			: //(std::string("__typeof(") + *from_typeof + ")");
			  make_typeof_fragment(*from_typeof);
       
//         // if we have a "from that" type, use it directly
//         if (from_type)
//         {
//             *p_out << "cake::value_convert<";
//             if (from_type) *p_out << get_type_name(from_type/*, ns_prefix + "::" + from_namespace_unprefixed*/);
//             else *p_out << " ::cake::unspecified_wordsize_type";
//             *p_out << ", ";
//             if (to_type) *p_out << get_type_name(to_type/*, ns_prefix + "::" + to_namespace_unprefixed*/); 
//             else *p_out << " ::cake::unspecified_wordsize_type";
//             *p_out << ">()(";
//         }
//         else
//         {
		
		/* If we want to select a rule mapping artificial types, then the
		 * user may have specified it as a specific from_type. Is that the
		 * only way? If they give us a typestring (typeof),
		 * it might denote a typedef too. */
		optional<string> from_artificial_tagstring = 
			is_indirect 
			? ((direction_is_out) 
				? source_binding.second.indirect_local_tagstring_out
				: source_binding.second.indirect_local_tagstring_in)
			: source_binding.second.local_tagstring;
		optional<string> to_artificial_tagstring = 
			is_indirect
			? ((direction_is_out)
			    ? source_binding.second.indirect_remote_tagstring_out
				: source_binding.second.indirect_remote_tagstring_in) 
			: source_binding.second.remote_tagstring;
		
		// GIANT HACK!
		bool is_a_pointer
			 = (source_binding.second.pointerness == bound_var_info::IS_A_POINTER)
			/*|| is_indirect */
			|| (from_typeof && *from_typeof == "((void*)0)");
		
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
        //*p_out << component_pair_classname(ifaces);

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
			to_typestring = make_typeof_fragment(*to_typeof);
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
						+ (is_a_pointer ? "/* pointer, so */__cake_default" : make_tagstring(from_artificial_tagstring))
						 + "_to_" 
						 + (is_a_pointer ? "__cake_default" :  make_tagstring(to_artificial_tagstring))
						 + "_in_first")
					: ("first< " + component_pair_classname(ifaces) + ", " + from_typestring + ", true>"
						+ "::" 
						+ (is_a_pointer ? "/* pointer, so */__cake_default" : make_tagstring(from_artificial_tagstring))
						+ "_to_" 
						+ (is_a_pointer ? "__cake_default" : make_tagstring(to_artificial_tagstring))
						+ "_in_second"));
						// this one --^ is WHAT?       this one --^ is WHAT?
						// answer: art.tag in source   answer: art.tag in sink
		}


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
		string rule_tag_str;
		if (is_a_pointer) rule_tag_str = "0 /* it's a pointer */";
		else rule_tag_str = std::string(" ::cake::") + "corresponding_type_to_"
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

            //*p_out << "::value_convert_from_first_to_second< " 
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
//                 *p_out << "::value_convert_from_second_to_first< " 
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
		bool direction_is_out,
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
			direction_is_out,
			is_indirect,
			from_type,
			to_type,
			from_typeof,
			to_typeof,
			from_module,
			to_module
		);
		*p_out << "::cake::value_convert<" << std::endl
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
		bool direction_is_out,
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
			direction_is_out,
			is_indirect,
			from_type,
			to_type,
			from_typeof,
			to_typeof,
			from_module,
			to_module
		); // we can't use REMOVE_CV here because the commas  in some nested typenames confuse cpp,
		// and extra brackets left by REMOVE_CV(( )) confuse the C++ parser
		return string("::cake::value_convert_function<") + "\n"
			+ "\t/* from type: */ boost::remove_const< " + params.from_typestring + ">::type, " + "\n"
			+ "\t/* to type: */ boost::remove_const< " + params.to_typestring + ">::type, " + "\n"
			+ "\t/* FromComponent: */ " + params.from_component_class + ", " + "\n"
			+ "\t/* ToComponent: */ " + params.to_component_class + ", " + "\n"
			+ "\t/* rule tag: */ " + params.rule_tag_str + "\n"
			+ ">";
	}

	void wrapper_file::close_value_conversion()
	{
		*p_out << ")";
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
			context& ctxt,
			antlr::tree::Tree *expr)
	{
		cerr << "Emitting a stub expr " << CCP(TO_STRING_TREE(expr)) << ", environment is: " << endl
			<< ctxt.env;
		std::string ident;
		post_emit_status retval;
#define RETURN_VALUE(v) do { retval = (v); goto out; } while(0)
		switch(GET_TYPE(expr))
		{
			case CAKE_TOKEN(INVOKE_WITH_ARGS): // REMEMBER: algorithms are special
				// and so is assert!
				return emit_stub_function_call(
					ctxt,
					expr);
			case CAKE_TOKEN(STRING_LIT):
				ident = new_ident("temp");
				*p_out << "auto " << ident << " = ";
				*p_out << "cake::style_traits<0>::string_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			case CAKE_TOKEN(INT):
				ident = new_ident("temp");
				*p_out << "auto " << ident << " = ";
				*p_out << "cake::style_traits<0>::int_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			case CAKE_TOKEN(FLOAT):
				ident = new_ident("temp");
				*p_out << "auto " << ident << " = ";
				*p_out << "cake::style_traits<0>::float_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			case CAKE_TOKEN(NAME_AND_INTERPRETATION):
				// just recurse on the name
				expr = GET_CHILD(expr, 0);
				RETURN_VALUE((emit_stub_expression_as_statement_list(ctxt, expr)));
			case CAKE_TOKEN(KEYWORD_VOID):
				RETURN_VALUE(((post_emit_status){NO_VALUE, "true", environment()}));
			case CAKE_TOKEN(KEYWORD_NULL):
				ident = new_ident("temp");
				*p_out << "auto " << ident << " = ";
				*p_out << "cake::style_traits<0>::null_value();" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			case CAKE_TOKEN(KEYWORD_TRUE):
				ident = new_ident("temp");
				*p_out << "auto " << ident << " = ";
				*p_out << "cake::style_traits<0>::true_value();" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			case CAKE_TOKEN(KEYWORD_FALSE):
				ident = new_ident("temp");
				*p_out << "auto " << ident << " = ";
				*p_out << "cake::style_traits<0>::false_value();" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
				assert(read_definite_member_name(expr).size() == 1);
				expr = GET_CHILD(expr, 0);
				// fall through
			case CAKE_TOKEN(IDENT):
				{
					if (ctxt.env.find(unescape_ident(CCP(GET_TEXT(expr)))) == ctxt.env.end())
					{
						std::cerr << "Used name " 
							<< unescape_ident(CCP(GET_TEXT(expr)))
							<< " not present in the environment. Environment is: "
							<< std::endl;
						std::cerr << ctxt.env;
						assert(false);
					}
					assert(ctxt.env[unescape_ident(CCP(GET_TEXT(expr)))].valid_in_module
						== ctxt.modules.current);
					RETURN_VALUE(((post_emit_status){ ctxt.env[unescape_ident(CCP(GET_TEXT(expr)))].cxx_name,
						"true", environment() }));
				}
			case CAKE_TOKEN(KEYWORD_THIS):
				assert(ctxt.env.find("__cake_this") != ctxt.env.end());
				RETURN_VALUE(((post_emit_status){ ctxt.env["__cake_this"].cxx_name,
						"true", environment() }));
			case CAKE_TOKEN(KEYWORD_THAT):
				assert(ctxt.env.find("__cake_that") != ctxt.env.end());
				RETURN_VALUE(((post_emit_status){ ctxt.env["__cake_that"].cxx_name,
						"true", environment() }));
			case CAKE_TOKEN(KEYWORD_HERE):
				assert(ctxt.env.find("__cake_here") != ctxt.env.end());
				RETURN_VALUE(((post_emit_status){ ctxt.env["__cake_here"].cxx_name,
						"true", environment() }));
			case CAKE_TOKEN(KEYWORD_THERE):
				assert(ctxt.env.find("__cake_there") != ctxt.env.end());
				RETURN_VALUE(((post_emit_status){ ctxt.env["__cake_there"].cxx_name,
						"true", environment() }));

			case CAKE_TOKEN(KEYWORD_SUCCESS):
				RETURN_VALUE(((post_emit_status) { "((void)0)", "true", environment() }));

			case CAKE_TOKEN(KEYWORD_OUT):
				/* We should have processed "out" as part of the enclosing function expr. */
				assert(false);

			case CAKE_TOKEN(MEMBER_SELECT):
			case CAKE_TOKEN(INDIRECT_MEMBER_SELECT): {
				// FIXME: these should have dynamic semantics, but instead they have
				// C++-static-typed semantics for now.
				auto resultR = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1));
				assert(GET_TYPE(GET_CHILD(expr, 0)) == CAKE_TOKEN(IDENT));
				ident = new_ident("temp");

				*p_out << "auto " << ident << " = "
					<< resultR.result_fragment 
					<< " " << CCP(TO_STRING(expr)) << " "
					<< unescape_ident(CCP(GET_TEXT(GET_CHILD(expr, 0)))) << ";" << std::endl;
				RETURN_VALUE(( (post_emit_status) { ident, 
					resultR.success_fragment, 
						// no *new* failures added, so delegate failure
					environment() } ));
			}
			case CAKE_TOKEN(ELLIPSIS): /* ellipsis is 'access associated' */
			case CAKE_TOKEN(ARRAY_SUBSCRIPT):

			// memory management
			case CAKE_TOKEN(KEYWORD_DELETE):
			case CAKE_TOKEN(KEYWORD_NEW):
			case CAKE_TOKEN(KEYWORD_TIE):
				cerr << "Unimplemented expression head: " << CCP(TO_STRING_TREE(expr)) << endl;
				assert(false);

			// these affect the expected cxx type, but
			// they are processed by scanning the AST -- we don't need to do anything
			case CAKE_TOKEN(KEYWORD_AS):
			case CAKE_TOKEN(KEYWORD_IN_AS):
			case CAKE_TOKEN(KEYWORD_OUT_AS):
				RETURN_VALUE((emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 2))));
			
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
				*p_out << "auto " << ident << " = "
					<< CCP(TO_STRING(expr)) << result.result_fragment << ";" << std::endl;
				RETURN_VALUE(((post_emit_status) { ident, 
					result.success_fragment, // no *new* failures added, so delegate failure
					environment() }));
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

				*p_out << "auto " << ident << " = "
					<< resultL.result_fragment 
					<< " " << CCP(TO_STRING(expr)) << " "
					<< resultR.result_fragment << ";" << std::endl;
				RETURN_VALUE(((post_emit_status) { ident, 
					"(" + resultL.success_fragment + " && " + resultR.success_fragment + ")", 
						// no *new* failures added, so delegate failure
					environment() }));
			}

			// these have short-circuit semantics
			case CAKE_TOKEN(LOGICAL_AND):
			case CAKE_TOKEN(LOGICAL_OR):
				assert(false);
			
			case CAKE_TOKEN(CONDITIONAL): // $cond $caseTrue $caseFalse 
			{
				auto resultCond = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 0));
				
				/* We have a problem: how to unify the typeofs for the two
				 * arms?
				 * We do this by 
				 * - generating "delayed" code for both arms, using lambdas;
				 * - using a "unify_types" template to unify types;
				 * - avoiding opening a new scope for the results of the cond/t/f
				 *   (it's okay if they open scopes to eval subexpressions) 
				 * - using a "default_value_with_type<>() helper template
				 * - thread through a "guard" fragment to eval_stub_expression_as_statement_list
				 *   -- if this fragment is false, it will just yield the default value
				 * - FIXME: implement this
				 */
				
				// delayed true
				auto true_lambda_ident = new_ident("delayed_true");
				*p_out << "auto " << true_lambda_ident << " = [=]() {" << endl;
				p_out->inc_level();
				auto resultTrue = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1));
				*p_out << "return std::make_pair("
					<< resultTrue.success_fragment << ", "
					<< resultTrue.result_fragment << ");" << endl;
				p_out->dec_level();
				*p_out << "};" << endl;
				
				// delayed false
				auto false_lambda_ident = new_ident("delayed_false");
				*p_out << "auto " << false_lambda_ident << " = [=]() {" << endl;
				p_out->inc_level();
				auto resultFalse = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 2));
				*p_out << "return std::make_pair(" 
					<< resultFalse.success_fragment << ", "
					<< resultFalse.result_fragment << ");" << endl;
				p_out->dec_level();
				*p_out << "};" << endl;
				
				// declare result
				auto conditional_result_ident = new_ident("result");
				*p_out << " ::cake::unify_types< __typeof( " << true_lambda_ident << "().second), __typeof( "
					<< false_lambda_ident << "().second) >::type " 
					<< conditional_result_ident << ";" << endl;
				
				// did the condition evaluate successfully?
				*p_out << "if (" << resultCond.success_fragment << ")" << endl
					<< "{";
				// we now evaluate the delayed_true or delayed_false fragment
				p_out->inc_level();
				*p_out << endl << "if (" << resultCond.result_fragment << ")"
					<< endl << "{";
				p_out->inc_level();
				
				auto true_result_pair = new_ident("true_result"); 
				*p_out << "auto " << true_result_pair << " = " << true_lambda_ident << "();" << endl;
				
				*p_out << "if (" << true_result_pair << ".first" << ") " 
					<< conditional_result_ident << " = " 
					<< true_result_pair << ".second;" << endl;
				
				p_out->dec_level();
				*p_out << "}" << endl
					<< "else" << endl << "{";
				p_out->inc_level();
				
				auto false_result_pair = new_ident("false_result"); 
				*p_out << "auto " << false_result_pair << " = " << false_lambda_ident << "();" << endl;
				
				*p_out << "if (" << false_result_pair << ".first" << ") " 
					<< conditional_result_ident << " = " 
					<< false_result_pair << ".second;" << endl;
				
				p_out->dec_level();
				*p_out << "} /* end else */" << endl;
				p_out->dec_level();
				*p_out << "} /* end if condition succeeded */" << endl;
				
				RETURN_VALUE(((post_emit_status) { 
				/* result fragment */  
					conditional_result_ident
					,
				/* success fragment */
					"(" + resultCond.success_fragment 
						+ " && (" + resultCond.result_fragment 
							+ " ? (" + true_result_pair + ".first"
							+ ") : (" + false_result_pair + ".first" + ")))",
						// no *new* failures added, so delegate failure
					environment() }));
			}
			
			case CAKE_TOKEN(KEYWORD_FN):
				assert(false);
				
			case CAKE_TOKEN(KEYWORD_LET): {
				INIT;
				BIND2(expr, boundNameOrNames);
				switch(GET_TYPE(boundNameOrNames))
				{
					case CAKE_TOKEN(IDENT): {
						ALIAS3(boundNameOrNames, boundName, IDENT);
						BIND2(expr, subExpr);
						auto result = emit_stub_expression_as_statement_list(
							ctxt,
							subExpr
						);
						RETURN_VALUE(((post_emit_status){ 
							result.result_fragment,
							result.success_fragment,
							(environment) { 
								make_pair(unescape_ident(CCP(GET_TEXT(boundName))), (bound_var_info){
									result.result_fragment,
									result.result_fragment, 
									ctxt.modules.current,
									false
								})
							}
						}));
					}	// end case IDENT
					case CAKE_TOKEN(IDENTS_TO_BIND): {
						BIND2(expr, subExpr);
						/* Our expression is likely to output multiple values. We will
						 * bind an ident to each one.
						 * How to encode multiple return/output values in a
						 * C++ stub expr result?  We need our "result" ident
						 * to have a known DWARF type, not just C++ type. Then
						 * we need to iterate over its fields. Can we do the
						 * iteration at C++ level somehow? No, because each 
						 * Cakename needs its own "auto ..." decl, and each one
						 * of those is independent in C++ terms. Actually, YES,
						 * if we use "." in the function outputs. */
						auto result = emit_stub_expression_as_statement_list(
							ctxt,
							subExpr
						);
						environment new_bindings;
						string cxx_expr;
						
						string multivalue_cxxname;
						vector< sig_output_arginfo_t > multivalue_outargs;
						vector< sig_output_arginfo_t >::iterator i_outarg;
						
						if (result.multivalue)
						{
							multivalue_cxxname = result.multivalue->first;
							multivalue_outargs = result.multivalue->second;
							i_outarg = multivalue_outargs.begin();
							if (GET_CHILD_COUNT(boundNameOrNames) > multivalue_outargs.size())
							{  RAISE(boundNameOrNames, "too many idents to bind"); }
						}
						else cxx_expr = result.result_fragment;
						
						/* multivalue is an instance of a wrapper-local struct type
						 * -- we want to bind a new Cakename to each element. */
						FOR_ALL_CHILDREN(boundNameOrNames)
						{
							if (result.multivalue)
							{
								cxx_expr = "*" + multivalue_cxxname + "->" + i_outarg->argname;
							}
							else cxx_expr = result.result_fragment;
							string ident_to_bind;
							optional<string> interp_string;
							int interp_type = 0;
							switch(GET_TYPE(n))
							{
								case CAKE_TOKEN(IDENT): 
									ident_to_bind = unescape_ident(CCP(GET_TEXT(n)));
									break;
								case CAKE_TOKEN(NAME_AND_INTERPRETATION): {
									INIT;
									BIND3(n, boundName, IDENT);
									ident_to_bind = unescape_ident(CCP(GET_TEXT(boundName)));
									if (GET_CHILD_COUNT(n) > 1)
									{
										BIND2(n, interp);
										interp_type = GET_TYPE(interp);
										{
											INIT;
											BIND3(interp, ident, IDENT);
											interp_string = unescape_ident(CCP(GET_TEXT(ident)));
											BIND3(interp, value_construct, VALUE_CONSTRUCT);
										}
									}
								} break;
								default: RAISE_INTERNAL(n, "unexpected token");
							}
							optional<string> local_tagstring;
							optional<string> indirect_local_tagstring_in;
							optional<string> indirect_local_tagstring_out;
							if (interp_type == CAKE_TOKEN(KEYWORD_AS)
							 || interp_type == CAKE_TOKEN(KEYWORD_INTERPRET_AS))
							{ local_tagstring = interp_string; }
							else if (interp_type == CAKE_TOKEN(KEYWORD_IN_AS))
							{ indirect_local_tagstring_in = interp_string; }
							else if (interp_type == CAKE_TOKEN(KEYWORD_OUT_AS))
							{ indirect_local_tagstring_out = interp_string; }
							
							new_bindings.insert(make_pair(ident_to_bind, (bound_var_info){
									cxx_expr,
									cxx_expr,
									ctxt.modules.current,
									false,
									bound_var_info::UNDEFINED, // pointerness
									local_tagstring,
									optional<string>(),
									indirect_local_tagstring_in,
									indirect_local_tagstring_out
								})
							);
							if (result.multivalue) ++i_outarg;

						} // end for bound ident
						RETURN_VALUE(((post_emit_status){ 
							result.result_fragment,
							result.success_fragment,
							new_bindings
						}));
					} // end case IDENTS_TO_BIND
					default: assert(false);
				} // end switch let expr case
			} // end case KEYWORD_LET
			
			sequencing_ops: //__attribute__((unused))
			case CAKE_TOKEN(SEMICOLON): {
				auto result1 = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 0)
				);
				// we need to merge any added bindings
				auto saved_env = ctxt.env;
				ctxt.env = merge_environment(ctxt.env, result1.new_bindings);
				// we discard the success, because we're semicolon
				auto result2 = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1)
				);
				ctxt.env = saved_env;
				result2.new_bindings = merge_environment(result2.new_bindings, result1.new_bindings);
				RETURN_VALUE(result2);
			} break;
			case CAKE_TOKEN(ANDALSO_THEN): {
				// these are the only place where we add to the environment, since
				// "let" expressions are no use in other contexts
				auto result1 = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 0)
				);
				// we need to merge any added bindings
				auto saved_env = ctxt.env;
				ctxt.env = merge_environment(ctxt.env, result1.new_bindings);
				// we heed the success, because we're ;&
				auto failure_label = new_ident("andalso_short_label");
				*p_out << "if (" << result1.success_fragment << ") { " 
					/*<< failure_label << ";"*/ << endl;
				p_out->inc_level();

				auto result2 = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1)
				);
				//ctxt.env = merge_environment(ctxt.env, result2.new_bindings);
				
				//*p_out << failure_label << ":" << endl;
				p_out->dec_level();
				*p_out << "}" << endl;
				
				ctxt.env = saved_env;
				result2.new_bindings = merge_environment(result2.new_bindings, result1.new_bindings);
				RETURN_VALUE(((post_emit_status){
					/* result fragment */ "((" + result1.success_fragment + ") ? (" + result2.result_fragment + ") : (" + result1.result_fragment + "))",
					/* success fragment */ "((" + result1.success_fragment + ") && (" + result2.success_fragment + "))",
					/* new bindings */ result2.new_bindings
				}));
			}
			case CAKE_TOKEN(ORELSE_THEN): {
				auto result1 = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 0)
				);
				// we need to merge any added bindings
				/* To merge the two new environments, we have some requirements:
				 * - The right-hand expression might want to use stuff bound by the left-hand.
				 * - The right-hand expression can be used to make bindings that failed
				 *   on the right-hand side. 
				 * We really want full dynamic binding. But without reifying the environment
				 * at runtime, we permit a simpler version here, to make our implementation simpler.
				 * The only bindings that will survive from the right-hand expr
				 * are ones that were attempted in the left-hand expr. 
				 * Since we don't track which bindings succeeded, we sloppily allow
				 * rebinding.
				 */
				auto saved_env = ctxt.env;
				ctxt.env = merge_environment(ctxt.env, result1.new_bindings);
				// we heed the success, because we're ;|
				auto early_success_label = new_ident("orelse_short_label");
				*p_out << "if (!(" << result1.success_fragment << "))  "; 
					//<< early_success_label << ";" << endl;
				cerr << "new bindings from left of ;|: " << result1.new_bindings;
				*p_out /*<< "else"*/ << " {";
				p_out->inc_level();
				auto result2 = emit_stub_expression_as_statement_list(
					ctxt,
					GET_CHILD(expr, 1)
				);
				cerr << "new bindings from right of ;|: " << result2.new_bindings;
				//ctxt.env = merge_environment(ctxt.env, result2.new_bindings);
				// Now, any bindings that were new from the RHS we
				// output an assignment for.
				for (auto i_rhs = result2.new_bindings.begin(); i_rhs != result2.new_bindings.end();
					++i_rhs)
				{
					if (result1.new_bindings.find(i_rhs->first) != result1.new_bindings.end())
					{
						/* This means that *if* we took the right-hand path, we should
						 * propagate its value. If we didn't, we are jumping down to the
						 * label below, so there's no need to output a guard if-test. */
						cerr << "Merging binding for " << i_rhs->first << endl;
						*p_out << result1.new_bindings[i_rhs->first].cxx_name 
							<< " = " << i_rhs->second.cxx_name << ";" << endl;
					}
					else cerr << "Not merging binding for " << i_rhs->first << endl;
				}
				p_out->dec_level();
				*p_out << "}" << endl;
				
				//*p_out << early_success_label << ":" << endl;
				
				ctxt.env = saved_env;
				RETURN_VALUE(((post_emit_status){
					/* result fragment */ "((" + result1.success_fragment + ") ? (" + result1.result_fragment + ") : (" + result2.result_fragment + "))",
					/* success fragment */ "((" + result1.success_fragment + ") || (" + result2.success_fragment + "))",
					/* new bindings */ result1.new_bindings
				}));
			}
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
				*p_out << "auto " << ident << " = ";
				*p_out << constant_expr_eval_and_cxxify(ctxt, expr)  << ";" << std::endl;
				RETURN_VALUE(((post_emit_status){ident, "true", environment()}));
			}
			default:
				cerr << "Don't know how to emit expression: " << CCP(TO_STRING_TREE(expr)) << endl;
				assert(false);
		}
		assert(false);
	out:
		cerr << "Finished emitting stub expr " << CCP(TO_STRING_TREE(expr)) 
			<< ", environment is: " << endl << ctxt.env
			<< "New bindings: " << endl
			<< retval.new_bindings;
		return retval;

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
			context& ctxt,
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
		auto callee = ctxt.modules.current->get_ds().toplevel()->resolve_visible(
					mn.begin(), mn.end());
		if (!callee || callee->get_tag() != DW_TAG_subprogram)
		{
			// HACK
			// cerr << ctxt.modules.current->get_ds() << endl;
			RAISE(functionNameTree, "does not name a visible function");
		}
		auto callee_subprogram
		 = boost::dynamic_pointer_cast<subprogram_die>(callee);
		
		cerr << "callee_subprogram is " << *callee_subprogram << endl;

		/* Scan the argument expressions for any that are "out".
		 * Declare the output object for those arguments now. */
		auto success_ident = new_ident("success"); // this will be the guard for these guys
		environment new_bindings;
		auto current_fp = callee_subprogram->formal_parameter_children_begin();
		int current_argnum = 0;
		map<int, string> output_parameter_idents;
		map<string, int> output_parameter_argnums;
		map<int, optional<unsigned> > output_parameter_is_array;
		{
			INIT;
			FOR_ALL_CHILDREN(argsMultiValue)
			{
				INIT;
				ALIAS2(n, argExpr);
				string ident = new_ident("outarg");
				optional< out_arg_info > decl_and_cakename;
				if (current_fp == callee_subprogram->formal_parameter_children_end())
				{
					cerr << "Warning: no DWARF info for argument " << current_argnum 
						<< " of subprogram "
						<< callee_subprogram->summary() 
						<< "; assumed not to be output-only." << endl;
					goto continue_loop;
				}
				if (!(*current_fp)->get_type())
				{
					cerr << "Warning: no type info for argument " << **current_fp
						<< "; assumed not to be output-only." << endl;
					goto continue_loop;
				}				
				decl_and_cakename = is_out_arg_expr(
					argExpr,
					*current_fp,
					ident
				);
				if (!decl_and_cakename)
				{
					// The user didn't write "out" in their expression for this fp...
					// ... but it might still be an output arg, if the DWARF says so.
					if (!arg_is_output_only(*current_fp)) goto continue_loop;
					
					// EVEN if it's an output-only arg, we needn't interfere too much.
					// If the user didn't write "out",
					// it means they are taking care of the destination of this
					// argument, by supplying an expression that points to some
					// destination for the data.
					// We need to remember this expressions' cxxname, so that
					// we can put it in the outargs structure. But otherwise,
					// we leave the user alone.
					// HMM: is this okay?
					goto continue_loop;
					
					/* Otherwise, if we get here, we have to make our own decl
					 * from the DWARF type of the the fp. We can also make up
					 * our own Cakename, since the user hasn't given one
					 * (they have to use "out" to bind one)
					 * but they might later bind an identifier using the 
					 * let (ident, ident, ...) syntax. We choose the outarg name
					 * with the __cake_ prefix. */
					auto fp_pointer_type = dynamic_pointer_cast<pointer_type_die>(
								(*current_fp)->get_type()->get_concrete_type()
							);
					assert(fp_pointer_type);
					//decl_and_cakename = (out_arg_info){
					//	get_type_name(fp_pointer_type->get_type()) + " " + ident + ";",
					//	string("__cake_" + ident)
					//};
				}
				
				// now we're definitely an optional arg expr
				//  with no user-supplied destination
				assert(decl_and_cakename);
				*p_out << decl_and_cakename->decl << /*" " << ident << ";" <<*/ endl;
				new_bindings.insert(make_pair(decl_and_cakename->cakename, (bound_var_info) {
					ident,
					ident,
					ctxt.modules.current,
					false
				}));
				new_bindings[decl_and_cakename->cakename].guard_cxxname = success_ident;

				output_parameter_idents[current_argnum] = ident;
				output_parameter_argnums[ident] = current_argnum;
				output_parameter_is_array[current_argnum] = decl_and_cakename->is_array;

				// FIXME: also scan for "out_as" and set tagstrings for the binding
				
			continue_loop:
				++current_argnum;
				if (current_fp != callee_subprogram->formal_parameter_children_end()) ++current_fp;
			} // end FOR_ALL_CHILDREN
			/* As later, we might have more DWARF args than in the AST */
			while (current_fp != callee_subprogram->formal_parameter_children_end())
			{
				string ident = new_ident("outarg");
				if (arg_is_output_only(*current_fp))
				{
					/* Unlike the above case, it we get here it means the user
					 * didn't say anything directly about where to put the 
					 * output value, but they might want to reference it in a
					 * let-multivalue expression. */
					clog << *current_fp;
					clog.flush();
					assert(*current_fp);
					assert((*current_fp)->get_type());
					auto fp_pointer_type = dynamic_pointer_cast<pointer_type_die>(
								(*current_fp)->get_type()->get_concrete_type()
							);
					assert(fp_pointer_type);
					optional< out_arg_info > decl_and_cakename = (out_arg_info) {
						get_type_name(fp_pointer_type->get_type()) + " " + ident + ";",
						string("__cake_" + ident)
					};
					*p_out << decl_and_cakename->decl << /*" " << ident << ";" <<*/ endl;
					new_bindings.insert(make_pair(decl_and_cakename->cakename, (bound_var_info) {
						ident,
						ident,
						ctxt.modules.current,
						false
					}));
					new_bindings[decl_and_cakename->cakename].guard_cxxname = success_ident;
					
					output_parameter_idents[current_argnum] = ident;
					output_parameter_argnums[ident] = current_argnum;
					output_parameter_is_array[current_argnum]
					 = decl_and_cakename->is_array;

				}
			
				++current_fp; ++current_argnum;
			}
		}

		auto callee_return_type
			= treat_subprogram_as_untyped(callee_subprogram) ?
			   0 : callee_subprogram->get_type();
			   
		/* evaluate the arguments and bind temporary names to them */
		*p_out << "bool " << success_ident << " = true; " << std::endl;
		std::string value_ident = new_ident("value");
		if (/*treat_subprogram_as_untyped(callee_subprogram)
			&& !*/subprogram_returns_void(callee_subprogram))
		{
			*p_out << /* "::cake::unspecified_wordsize_type" */ "cake::no_value:t" // HACK: this is what our fake DWARF will say
			 << ' ' << value_ident << " = ::cake::success< cake::no_value_t>()(); // trivial success" << std::endl;
		}
		else //if (!subprogram_returns_void(callee_subprogram))
		{
			*p_out << get_type_name(callee_subprogram->get_type())
			 << ' ' << value_ident << ";" << std::endl;
		}
		//*p_out << "do" << std::endl
		//	<< "{";
		string raw_result_ident = new_ident("result");
		string out_obj_ident = new_ident("outobj");
		// if we have a return value
		vector< sig_output_arginfo_t > output_arginfo;
		if (callee_subprogram->get_type() && callee_subprogram->get_type()->get_concrete_type())
		{
			output_arginfo.push_back(
				(sig_output_arginfo_t){"__cake_retval", callee_subprogram->get_type() }
			);
		}
		*p_out << "// begin multiple-outputs data type for the function call" << endl;
		map<string, int> output_argnums;
		auto out_type_tag = new_ident("out_type");
		{ 
			int argnum = -1;
			for (auto i_arg = callee_subprogram->formal_parameter_children_begin();
				i_arg != callee_subprogram->formal_parameter_children_end();
				++i_arg)
			{
				++argnum;
				// is this an inout or output parameter?
				if ((*i_arg)->get_type() &&
					(*i_arg)->get_type()->get_concrete_type()->get_tag() == DW_TAG_pointer_type)
				{
					shared_ptr<pointer_type_die> pt = dynamic_pointer_cast<pointer_type_die>(
						(*i_arg)->get_type()->get_concrete_type());
					assert(pt);
					if (!pt->get_type()) continue; // void ptrs don't count
					string name;
					if ((*i_arg)->get_name()) name = *(*i_arg)->get_name();
					else 
					{
						ostringstream s;
						s << "__cake_outarg" << argnum;
						name = s.str();
					}
					output_arginfo.push_back(
						(sig_output_arginfo_t){ name, pt->get_type(), *i_arg }
					);
					output_argnums[name] = argnum;
				}
			}
		}
		// now declare a struct type 
		*p_out << "struct " << out_type_tag << "{" << endl;
		for (auto i_arginfo = output_arginfo.begin(); i_arginfo != output_arginfo.end(); ++i_arginfo)
		{
			ostringstream s;
			// naive code for now
			s << "boost::optional< ";
			//<< get_type_name(i_arginfo->second);//compiler.cxx_declarator_from_type_die(i_arginfo->second).first
			/* We need to distinguish output arrays from pointers-to-singletons here. 
			 * Use typeof, which is more precise, if possible. 
			 * If we can use typeof, there will be an entry in output_parameter_idents,
			 * which maps argnum -> varname.
			 * We have output_arginfo, which maps callee-DWARF argument names 
			 * (or arbitrary stand-ins)
			 * to DWARF types. */
			int argnum = output_argnums.find(i_arginfo->argname) != output_argnums.end()
				? output_argnums[i_arginfo->argname] : -1;
			if (argnum != -1 &&
				output_parameter_idents.find(argnum) != output_parameter_idents.end())
			{
				s << make_typeof_fragment(output_parameter_idents[argnum]);
			}
			else 
			{
				s << get_type_name(i_arginfo->t);
			}
			s << "& "<< ">";
			*p_out << s.str() << " " << i_arginfo->argname << ";" << endl;
			i_arginfo->tn = s.str();
		}
		*p_out << "};" << endl;
		*p_out << "boost::optional< " << out_type_tag << " > " << out_obj_ident << ";" << endl;
		*p_out << "// end multiple-outputs alias for the function call" << endl;
		*p_out << "// begin argument expression eval" << std::endl;
		//p_out->inc_level();
		int arg_eval_subexpr_count = 0;
		vector< post_emit_status > arg_results;
		vector< optional<string> > arg_names_in_callee;
		map< string, string > arg_results_by_callee_name;
		map< string, int > argnums_by_callee_name;
		post_emit_status result;
		//dwarf::spec::subprogram_die::formal_parameter_iterator i_arg
		// = callee_subprogram->formal_parameter_children_begin();
		//unsigned argnum = 0;
		// iterate through multiValue (AST) and callee DWARF args in lock-step
		// NO! Lock-step is not flexible enough. 
		/* What features are we accommodating here?
		
			name matching of arguments using in_args
			explicit name-based mapping of arguments enumerated in the AST
			"one left over" matching
			output-only arguments (need not be in the AST)
		
		   Let's support (in this order)
		   
			explicit positional matching *from both ends* of an argument tuple
			... so we can have in_args accounting for a variable-length middle section;
			
			name-matching for in-args
			
			one-left-over matching and output-only args (if not one-left-over already,
			  try eliminating the output-only, then see if there's one left over)
			
		   What are we computing?
				- for each callee argument,
				    a source of its value
					  which can be
					    an AST subexpression (from the explicit args)
					    an AST subexpression computed from an implicit mapping
					    no AST expression, because it's output-only
				- i.e. pair< antlr::tree::Tree *, optional< > >


			We are also maintaining
			
				arg_results -- a vector of post_emit_statuses for each arg eval'd in turn
				arg_names_in_callee -- a vector of the callee-side names of each argument
				arg_results_by_callee_name -- a map from callee-side name to result fragment
				argnums_by_callee_name -- a map from callee-side names to their indices (in callee order)
			and the output parameter stuff:
				output_parameter_idents -- map from callee position to an output arg's local destination object, for output-*only* args
				output_parameter_argnums -- the inverse, currently ignored
				output_argnums -- similar to output_parameter_argnums, but not ignored
				output_parameter_is_array -- map from argnum to whether it's an array (syntax...)
				output_arginfo -- info we'll pass up to any enclosing let-tuple expression
			... can hopefully be left as-is for now.
				
		 */
		
		
		
// 		{
// 			INIT;
// 			FOR_ALL_CHILDREN(argsMultiValue)
// 			{
// 				string argname = 
// 					(i_arg != callee_subprogram->formal_parameter_children_end() 
// 						&& (*i_arg)->get_name()
// 					) ? *(*i_arg)->get_name() : basic_name_for_argnum(argnum);
// 				// we might output zero or more arg expressions here -- tricky
// 				// don't bother with these checks for now
// // 				if (i_arg == callee_subprogram->formal_parameter_children_end())
// // 				{
// // 					cerr << "Calling function " << *callee_subprogram << endl;
// // 					RAISE(
// // 						argsMultiValue, "too many args for function");
// // 				}
// // 				if (!(*i_arg)->get_type()) 
// // 				{
// // 					cerr << "Calling function " << *callee_subprogram 
// // 						<< ", passing argument " << i << endl;
// // 					RAISE(
// // 						n, "no type information for argument");
// // 				}
// 						
// 				int args_for_this_ast_node = 1; // will be overridden in in_args handling
// 				// for in_args handling:
// 				std::vector<
// 					std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
// 								dwarf::spec::subprogram_die::formal_parameter_iterator 
// 					> 
// 				> matched_names;				
// 				std::vector<
// 					std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
// 								dwarf::spec::subprogram_die::formal_parameter_iterator 
// 					> 
// 				>::iterator i_matched_name;
// 				
// 				/* If we have an output_args object for this,
// 				 * then we short-circuit the eval process, because
// 				 * we can't easily identify output args from the AST (without scanning for
// 				 * KEYWORD_OUT, which might be buried deep). 
// 				 * FIXME: this is because the grammar is borked, since
// 				 * KEYWORD_OUT can appear almost anywhere in a stub expression. */
// 				if (output_parameter_idents.find(argnum) != output_parameter_idents.end())
// 				{
// 					//*p_out << "&" << output_parameter_idents[argnum];
// 					
// 					/* We want a &-fragment such that doing "*" on it
// 					 * gets us a reference of the exact address-taken type. 
// 					 * If output_parameter_idents[argnum] is an array, 
// 					 * it doesn't work to just do "*(&ident)". Rather, we want
// 					                  *(static_cast< __typeof(outvar) * >(&ident) 
// 					 */
// 					string arg_result
// 					 = "(static_cast< __typeof(" 
// 					       + output_parameter_idents[argnum]
// 					                     + ") *>(&" + output_parameter_idents[argnum] + "))";
// 					
// 					arg_results.push_back((post_emit_status){
// 						arg_result, //"((void)0)",
// 						"true",
// 						environment()
// 					});
// 					arg_names_in_callee.push_back((*i_arg)->get_name());
// 					
// 					
// 					arg_results_by_callee_name[argname] = arg_result;
// 					argnums_by_callee_name[argname] = argnum;
// 					goto next_arg_in_callee_sequence;
// 				}
// 				
// 				/* If the stub expression was a KEYWORD_IN_ARGS, then
// 				 * the stub code emitted  has yielded multiple outputs 
// 				 * and multiple successes. */
// 				switch (GET_TYPE(n))
// 				{
// 					case CAKE_TOKEN(KEYWORD_OUT_ARGS):
// 						assert(false);
// 					case CAKE_TOKEN(KEYWORD_IN_ARGS):
// 						/* When we see in_args, we eagerly match *by name*
// 						 * any argument in the source context (i.e. the event)
// 						 *  that has not already been evaluated,
// 						 * against any argument in the sink context
// 						 *  that is not already paired with a previously evaluated argument,
// 						 * and output them in their order of appearance in the callee.
// 						 * It is an error if the resulting order
// 						 * covers a noncontiguous range of arguments. */
// 						matched_names = name_match_parameters(
// 							ctxt.opt_source->signature,
// 							callee_subprogram);
// 							// FIXME: do we match names against the event pattern
// 							// (which may have made up its own names for an argument)
// 							// or against the DWARF info?
// 							// Well, a major use-case of in_args... is where we 
// 							// explicitly avoid naming any arguments and just say "bar(...)"
// 							// so we have to go with the DWARF. 
// 							// But FIXME: we should warn if event pattern names would give
// 							// a different mapping
// 						/* Now filter these matches:
// 						 * - discard any pair that precedes our current position in the callee; 
// 						 * - I think that's all? 
// 						 * And then 
// 						 * sort them by position in the callee arg list
// 						 * and check that they form a contiguous sequence 
// 						 * starting at our current pos. */
// 						for (auto i_out = matched_names.begin(); i_out != matched_names.end();
// 							++i_out)
// 						{
// 							dwarf::spec::subprogram_die::formal_parameter_iterator i_test;
// 							i_test = i_out->second;
// 							if (i_test < i_arg)
// 							{
// 								i_out = matched_names.erase(i_out);
// 								// erase returns the one after the erased item...
// 								i_out--; // ... which we want to come round next iteration
// 							}
// 						}
// 						// sort in order of the sink (callee) argument ordering
// 						std::sort(
// 							matched_names.begin(),
// 							matched_names.end(),
// 							[](const std::pair< 
// 									dwarf::spec::subprogram_die::formal_parameter_iterator,
// 									dwarf::spec::subprogram_die::formal_parameter_iterator >& a,
// 								const std::pair< 
// 									dwarf::spec::subprogram_die::formal_parameter_iterator,
// 									dwarf::spec::subprogram_die::formal_parameter_iterator >& b)
// 							{ return a.second < b.second; });
// 						// do they form a contiguous sequence starting at current pos?
// 						if (matched_names.begin() == matched_names.end())
// 						{
// 							// no arguments to name-match
// 							// HACK: special case: one unmatched arg is okay
// 							auto i_next_fp = i_arg; if (i_next_fp != callee_subprogram->formal_parameter_children_end()) ++i_next_fp;
// 							auto i_caller_fp = ctxt.opt_source->signature->formal_parameter_children_begin();
// 							auto count = srk31::count(ctxt.opt_source->signature->formal_parameter_children_begin(),
// 								ctxt.opt_source->signature->formal_parameter_children_end());
// 							/* Note: `i' comes from parser.hpp -- it is the index (zero-based)
// 							 * of our position in the argsMultiValue in the AST.  */
// 							cerr << "Source argcount is " << count 
// 								<< ", AST argcount is " << GET_CHILD_COUNT(argsMultiValue)
// 								<< ", current AST index: " << i << endl;
// 							for (unsigned j = 0; 
// 								i_caller_fp != ctxt.opt_source->signature->formal_parameter_children_end() 
// 									&& j < i; ++j) ++i_caller_fp;
// 							/* If we've not reached the end of the arglists... */
// 							if (i_arg != callee_subprogram->formal_parameter_children_end()
// 							&& (i_caller_fp != ctxt.opt_source->signature->formal_parameter_children_end()
// 							|| ctxt.opt_source->signature->unspecified_parameters_children_begin()
// 							!= ctxt.opt_source->signature->unspecified_parameters_children_end()))
// 							{
// 								/* ... push one unmatched name. */
// 								matched_names.push_back(make_pair(
// 										i_caller_fp, // might be END, because of unspecified_parameters I think
// 										i_arg)); // must not be END
// 								args_for_this_ast_node = 1;
// 							}
// 							// no arguments to name-match
// 							else 
// 							{
// 								args_for_this_ast_node = 0;
// 								goto finished_argument_eval_for_current_ast_node; // naughty goto
// 								/* This goto is here to skip over the check that the name-matched
// 								 * args are continuous and start in the right place
// 								 * in the callee DWARF arg sequence -- since we name-matched
// 								 * zero args. */
// 							}
// 						} else args_for_this_ast_node = matched_names.size();
// 						assert(args_for_this_ast_node > 0);
// 						if (matched_names.begin()->second != i_arg)
// 						{
// 							RAISE(n, "name-matching args do not start here");
// 						}
// 						for (auto i_out = matched_names.begin(); i_out != matched_names.end(); ++i_out)
// 						{
// 							auto i_next_matched_name = i_out; ++i_next_matched_name;
// 							auto i_next_callee_parameter = i_out->second; ++i_next_callee_parameter;
// 							if (i_next_matched_name != matched_names.end()
// 							// in the callee arg list, i.e. ->second, they must be contiguous
// 								&& i_next_matched_name->second != i_next_callee_parameter)
// 							{
// 								RAISE(n, "name-matching args are non-contiguous");
// 							}
// 						}
// 						// start the iterator
// 						i_matched_name = matched_names.begin();
// 						// now what? well, we simply evaluate them in order. What order?
// 						// the order they appear in the *callee*, so that we can keep
// 						// i_arg moving forward
// 						do
// 						{
// 							// what's the binding of the argument in the caller? 
// 							{
// 								std::ostringstream s;
// 								s << wrapper_arg_name_prefix << (i + (i_matched_name - matched_names.begin()));
// 
// 								// emit some stub code to evaluate this argument
// 								result = emit_stub_expression_as_statement_list(
// 								  ctxt, 
// 								  // we need to manufacture an AST node: IDENT(arg_name_in_caller)
// 								  make_ident_expr(s.str())//,
// 								  /* Result type is that of the *argument* that we're going to pass
// 								   * this subexpression's result to. */
// 								  /*(treat_subprogram_as_untyped(callee_subprogram) 
// 								  ? boost::shared_ptr<dwarf::spec::type_die>()
// 								  : *(*i_arg)->get_type())*/);
// 								// next time round we will handle the next matched name
// 								++i_matched_name;
// 							}
// 							// the rest is like the simple case
// 							goto remember_arg_names;
// 					default:
// 							// emit some stub code to evaluate this argument -- simple case
// 							result = emit_stub_expression_as_statement_list(
// 							  ctxt, n//,
// // 							  /* Result type is that of the *argument* that we're going to pass
// // 							   * this subexpression's result to. */
// // 							  (treat_subprogram_as_untyped(callee_subprogram) 
// // 							  ? boost::shared_ptr<dwarf::spec::type_die>()
// // 							  : *(*i_arg)->get_type())*/
// 							  );
// 							// remember the names used for the output of this evaluation
// 					remember_arg_names:
// 							arg_results.push_back(result);
// 
// 							/* If the stub expression was a KEYWORD_IN_ARGS, then
// 							 * the stub code emitted  has yielded multiple outputs 
// 							 * and multiple successes. */
// 
// 							// store the mapping to the callee argument
// 							arg_names_in_callee.push_back((*i_arg)->get_name());
// 							arg_results_by_callee_name[argname] = result.result_fragment;
// 							argnums_by_callee_name[argname] = argnum;
// 					output_control:
// 							*p_out << success_ident << " &= " << result.success_fragment << ";" << std::endl;
// 							*p_out << "if (" << success_ident << ") // okay to proceed with next arg?" 
// 								<< std::endl;
// 							*p_out << "{" << std::endl;
// 							++arg_eval_subexpr_count;
// 							p_out->inc_level();
// 					next_arg_in_callee_sequence:
// 							++i_arg; ++argnum;
// 						} // end do
// 						while (--args_for_this_ast_node > 0);
// 					finished_argument_eval_for_current_ast_node:
// 						break;
// 				} // end switch
// 			} // end FOR_ALL_CHILDREN

			/* Instead, do the much simpler assign_argument_expressions(). */
			map<
				int,
				pair<
					antlr::tree::Tree *,
					optional< string >
				>
			> assigned_argument_expressions;
			assign_argument_expressions(ctxt,
				callee_subprogram,
				argsMultiValue,
				assigned_argument_expressions);
			unsigned argnum = 0;
			auto i_arg = callee_subprogram->formal_parameter_children_begin();
			for (auto i_assigned = assigned_argument_expressions.begin();
				i_assigned != assigned_argument_expressions.end();
				++i_assigned, ++argnum, ++i_arg)
			{
				if (i_assigned->first != argnum)
				{
					cerr << "No binding for argument " << argnum << endl;
					RAISE(argsMultiValue, "no binding for one or more arguments");
				}
				
				// we always have an argname, whether generated or DWARF-supplied
				// NOTE: this is the name in the *callee*. So it's not supposed
				// to be valid in the current environment. 
				string argname = 
					(i_arg != callee_subprogram->formal_parameter_children_end() 
						&& (*i_arg)->get_name()
					) ? *(*i_arg)->get_name() : basic_name_for_argnum(argnum);
				
				/* Now we're free to output an expression. But we don't do this
				 * in the case of output-only args... we already have what we need.  */
				bool is_output_only
				 = (output_parameter_idents.find(argnum) != output_parameter_idents.end());
				auto result = (!is_output_only)
				 ? emit_stub_expression_as_statement_list(
					ctxt, 
					i_assigned->second.first
				)
				 : (post_emit_status){
						"(static_cast< __typeof(" 
						   + output_parameter_idents[argnum]
						   + ") *>(&" + output_parameter_idents[argnum] + "))",
						"true",
						environment()
					};

				arg_results.push_back(result);





				// store the mapping to the callee argument
				arg_names_in_callee.push_back(argname);
				arg_results_by_callee_name[argname] = result.result_fragment;
				argnums_by_callee_name[argname] = argnum;

				// output control logic
				*p_out << success_ident << " &= " << result.success_fragment << ";" << std::endl;
				*p_out << "if (" << success_ident << ") // okay to proceed with next arg?" 
					<< std::endl;
				*p_out << "{" << std::endl;
				++arg_eval_subexpr_count; // count used to return indent level back to norm
				p_out->inc_level();
			}
			
			/* If we have some output-only args, they may not appear
			 * in the arglist. Handle this case now. */
			while (i_arg != callee_subprogram->formal_parameter_children_end()
			 && arg_is_output_only(*i_arg))
			{
				string argname
				 = (*i_arg)->get_name() 
					? *(*i_arg)->get_name()
					: basic_name_for_argnum(argnum);
				
				// assert that we stashed an output parameter for this argname
				assert(output_parameter_idents.find(argnum) != output_parameter_idents.end());
				
				auto result = (post_emit_status){
					"(&" + output_parameter_idents[argnum] + ")",
					"true",
					environment()
				};
				arg_results.push_back(result);
				arg_names_in_callee.push_back((*i_arg)->get_name());
				arg_results_by_callee_name[argname] = result.result_fragment;
				argnums_by_callee_name[argname] = argnum;
				
				++i_arg; ++argnum;
			}
			
//		} // end INIT argsMultiValue
		*p_out << "// end argument eval" << std::endl;
		
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
			msg << ") for call (min: " << min_args << ", max: " << max_args << ")" << endl;
			RAISE(call_expr, msg.str().c_str());
		}
		
		
		*p_out << "// begin function call" << std::endl;
		//*p_out << args_success_ident << " = true;" << std::endl;
		
		// result and success of overall function call
		//std::string success_ident = new_ident("success");
		//*p_out << "bool " << success_ident << ";" << std::endl;
		//*p_out << "if (" << success_ident << ")" << std::endl;
		//*p_out << "{" << std::endl;
		//p_out->inc_level();


		if (callee_subprogram->get_type() && callee_subprogram->get_type()->get_concrete_type())
		{
			*p_out << "auto " << raw_result_ident << " = ";
		}

		// emit the function name, as a symbol reference
		*p_out << "cake::" << m_d.namespace_name() << "::";
		*p_out << get_dwarf_ident(
			ctxt,
			function_name);		
		*p_out << '(';
		p_out->inc_level();
		spec::subprogram_die::formal_parameter_iterator i_fp
		 = callee_subprogram->formal_parameter_children_begin();
		bool ran_out_of_fps = false;
		
		auto emit_arg_expr_maybe_with_cast
		 = [&p_out, &compiler, this](shared_ptr<type_die> p_t, string expr,
		 	 bool do_not_cast = false) {
		 	bool inserting_cast = false;
			if (!do_not_cast	
				 && p_t 
				 && p_t->get_concrete_type()
				 && p_t->get_concrete_type()->get_tag() == DW_TAG_pointer_type)
			{
				inserting_cast = true;
				// HACK: don't use reinterpret_cast, because sometimes we want this case
				// to convert unspecified_wordsize_type --> void *,
				// and we can't do that with reinterpret,
				// but can do it with C-style casts
				// (with the help of the "operator void *" we defined).
//				*p_out << "reinterpret_cast< ";
				*p_out << "((";
				auto declarator = compiler.cxx_declarator_from_type_die(p_t,
					optional<const string&>(), true,
					get_type_name_prefix(p_t) + "::", false);
				*p_out << declarator.first;
//				*p_out << ">(";
				*p_out << ")";
			}
			
			*p_out << expr;
			
			if (inserting_cast) *p_out << ")";
		};
		
		for (auto i_result = arg_results.begin(); 
			i_result != arg_results.end(); ++i_result, ++i_fp)
		{
			if (i_result != arg_results.begin()) *p_out << ", ";
			
			if (i_fp == callee_subprogram->formal_parameter_children_end()) ran_out_of_fps = true;
			
			*p_out << std::endl; // begin each argument on a new line
			auto name_in_callee = arg_names_in_callee[i_result - arg_results.begin()];
			*p_out << "/* argument name in callee: " 
				<< (name_in_callee ? *name_in_callee : "(no name)")
				<< " */ ";
			// if the callee argument has a pointer type, we insert a cast
			emit_arg_expr_maybe_with_cast(
				(!ran_out_of_fps && (*i_fp)->get_type())
					? (*i_fp)->get_type() 
					: shared_ptr<type_die>(),
				i_result->result_fragment
			);
		}
		*p_out << ')';
		p_out->dec_level();
		
		*p_out << ";" << endl;
		*p_out << "// end function call" << endl;
		
		// convert the function raw result to a success and value pair,
		// in a style-dependent way
		*p_out << "// begin output/error split for the function call overall" << endl;
		if (callee_subprogram->get_type())
		{
			*p_out << success_ident << " = __cake_success_of(" 
				<< raw_result_ident << ");" << std::endl;
			*p_out << "if (" << success_ident << ")" << std::endl
			<< "{" << std::endl;
			p_out->inc_level();
				*p_out << value_ident << " = __cake_value_of("
					<< raw_result_ident << ");" << std::endl;
			p_out->dec_level();
			*p_out << "}" << std::endl;
			
		}
		else *p_out << success_ident << " = true;" << std::endl;
		// now populate the structure
		if (output_arginfo.begin() != output_arginfo.end())
		{
			*p_out << out_obj_ident << " = (" << out_type_tag << "){ " << endl;
			for (auto i_arginfo = output_arginfo.begin(); i_arginfo != output_arginfo.end(); ++i_arginfo)
			{
				if (i_arginfo != output_arginfo.begin()) *p_out << ", " << endl;
				// output an lvalue holding the result
				if (i_arginfo->argname == "__cake_retval") *p_out << raw_result_ident;
				else
				{
					assert(arg_results_by_callee_name.find(i_arginfo->argname)
						!= arg_results_by_callee_name.end());
					// the "*" here is because output args are always indirected,
					// but we want to reference their value
					// -- a complication is arrays, where *(&ident) =!= ident
					// but we handle this when building arg_results_by_callee_name
					// by some nasty static_cast-ing.
					// YET more complication: 
					// in the case of struct members, g++ can't infer the right
					// boost::optional constructor conversion,
					// so we have to guide it a bit. 
					
					// to find is_array, search for our argname in 
					auto i_found_argnum = argnums_by_callee_name.find(
						i_arginfo->argname);
					assert(i_found_argnum != argnums_by_callee_name.end());
					
					// tn is a classname which at the moment is either unset
					// or boost::optional<something>.
					// Here we arrange that if our call failed, the optional output
					// arguments are not set.
					if (i_arginfo->tn) 
					{	
						*p_out << success_ident << " ? ";
						*p_out << *i_arginfo->tn << "( *";
					}
					emit_arg_expr_maybe_with_cast(
						// this func needs the type of the fp -- will be noop for retval
						i_arginfo->p_fp ? i_arginfo->p_fp->get_type() : shared_ptr<type_die>(),
						arg_results_by_callee_name[i_arginfo->argname],
						(bool) output_parameter_is_array[i_found_argnum->second] /* skip the cast if it's an array, because
						 then the typeof will be precise enough, and if we write the
						 cast, it will just give us element [0] i.e. the element type. */
					);
					if (i_arginfo->tn)
					{
						*p_out << " )";
						*p_out << " : " << *i_arginfo->tn << "()";
					}
				}
			}
			*p_out << " }";
			*p_out << ";" << endl;
		}
		*p_out << "// end output/error split for the function call overall" << endl;

		// we opened argcount  extra lexical blocks in the argument eval
		for (int i = 0; i < arg_eval_subexpr_count; i++)
		{
			p_out->dec_level();
			*p_out << "} " /*"else " << success_ident << " = false;"*/ << std::endl;
		}
		
		// set failure result
		*p_out << "// begin calculate overall expression success/failure and output " << std::endl;
		*p_out << "if (!" << success_ident << ")" << std::endl;
		*p_out << "{" << std::endl;
		p_out->inc_level();
		if (!subprogram_returns_void(callee_subprogram)) // only get a value if it returns something
		{
			*p_out << value_ident << " = ::cake::failure<" 
				<<  (treat_subprogram_as_untyped(callee_subprogram)
					 ? /* "unspecified_wordsize_type " */ "int" // HACK! "int" is what our fake DWARF will say, for now
					 : get_type_name(callee_subprogram->get_type())) //return_type_name
				<< ">()();" << std::endl;
		}
		p_out->dec_level();
		*p_out << "}" << std::endl;
		*p_out << "// end calculate overall expression success/failure and output " << std::endl;
		
		//p_out->dec_level();
		//*p_out << "} while(0);" << std::endl;
		
		//*p_out << "if (!" << success_ident << ")" << std::endl
		//	<< "{";
		//p_out->inc_level();
		

		return (post_emit_status){value_ident, success_ident, new_bindings, 
			make_pair(out_obj_ident, output_arginfo)};
	}
	
	optional< wrapper_file::out_arg_info >
	wrapper_file::is_out_arg_expr(
		antlr::tree::Tree *argExpr, 
		shared_ptr<formal_parameter_die> p_fp,
		const string& ident,
		bool force_yes /* = false */
	)
	{
		if (!p_fp || !p_fp->get_type() 
			|| !dynamic_pointer_cast<pointer_type_die>(p_fp->get_type()->get_concrete_type()))
		{
			if (!force_yes) return optional< out_arg_info >();
			// otherwise it's a problem
			cerr << "Output parameter ";
			if (!p_fp) cerr << "(no fp)";
			else cerr << *p_fp;
			
			cerr << " has concrete type: ";
			if (!p_fp) cerr << "(no fp => no type)";
			else if (!p_fp->get_type()) cerr << "(no type)";
			else cerr << *p_fp->get_type()->get_concrete_type();
			cerr << endl;
			RAISE(
				argExpr, "output params must have pointer type");
		}
		shared_ptr<type_die> fp_type = p_fp->get_type();
		shared_ptr<pointer_type_die> fp_pointer_type = dynamic_pointer_cast<pointer_type_die>(fp_type);
		shared_ptr<type_die> pointer_target_type
		 = (fp_pointer_type && fp_pointer_type->get_type()) 
		 	? fp_pointer_type->get_type()->get_concrete_type() 
			: shared_ptr<type_die>();
		switch(GET_TYPE(argExpr))
		{
			case CAKE_TOKEN(ARRAY_SUBSCRIPT): {
				INIT;
				BIND2(argExpr, subscriptExpr);
				BIND2(argExpr, outObject);
				if (GET_TYPE(outObject) != CAKE_TOKEN(KEYWORD_OUT)) return optional< out_arg_info >();
				string cakename;
				{
					INIT;
					BIND3(outObject, outObjectName, IDENT);
					cakename = unescape_ident(CCP(GET_TEXT(outObjectName)));
				}
				unsigned array_bound;
				string array_bound_string = unescape_ident(CCP(GET_TEXT(subscriptExpr)));
				istringstream array_bound_stream(array_bound_string);
				array_bound_stream >> array_bound; // HACK
				string decl = get_type_name(pointer_target_type)
					+ " " + ident 
					+ "["
					+ array_bound_string
					+ "];";
				
				return (out_arg_info){ decl, cakename, array_bound };
			}
			break;
			case CAKE_TOKEN(KEYWORD_OUT):
				return is_out_arg_expr(GET_CHILD(argExpr, 0), p_fp, ident, true);
			case CAKE_TOKEN(IDENT): {
				INIT;
				// We only know it's an ident at this point; 
				// only say it's an output arg if we have force_yes
				// now we definitely need that pointer
				if (!force_yes) return optional< out_arg_info >();
				if (force_yes && !pointer_target_type) RAISE(argExpr, "cannot output through untyped pointer");
				// else force_yes && pointer_target_type
				string decl
				 = get_type_name(pointer_target_type)//compiler.cxx_declarator_from_type_die(pointer_target_type).first
					+ " " + ident + ";";
				return (out_arg_info){ decl, unescape_ident(CCP(GET_TEXT(argExpr))) };
			}
			break;
			default: return optional< out_arg_info >();
		}
	}
	
	std::string wrapper_file::constant_expr_eval_and_cxxify(
		const context& ctxt,
		antlr::tree::Tree *constant_expr)
	{
		//*p_out << CCP(TO_STRING_TREE(constant_expr));
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
				const_arith_t result = eval_const_expr(ctxt, constant_expr);
				s << result; // trivial cxxify :-)
				break;
			}			
			default: 
				RAISE(child, "expected a constant expression");
		}
		return s.str();
	}
    
    // FIXME: something better than this naive long double implementation please
    wrapper_file::const_arith_t
	wrapper_file::eval_const_expr(
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
#ifndef NO_LONG_DOUBLE
                    	* powl(2.0, eval_const_expr(ctxt, GET_CHILD(expr, 0)));
#else 
                    	* pow(2.0, eval_const_expr(ctxt, GET_CHILD(expr, 0)));
#endif
            case CAKE_TOKEN(SHIFT_RIGHT):
            	return 
                	eval_const_expr(ctxt, GET_CHILD(expr, 0))
#ifndef NO_LONG_DOUBLE
                    	* powl(2.0, eval_const_expr(ctxt, GET_CHILD(expr, 0)));
#else
                    	* pow(2.0, eval_const_expr(ctxt, GET_CHILD(expr, 0)));
#endif

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
	
	void 
	wrapper_file::assign_argument_expressions(
		context& ctxt,
		shared_ptr<subprogram_die> callee_subprogram,
		antlr::tree::Tree *argsMultiValue,
		map<
			int,
			pair<
				antlr::tree::Tree *,
				optional< string >
			>
		>& out
	)
	{
		set<int> ast_nodes_used; 
		set<int> caller_args_used;
		
		enum how_assigned_t { POSITION_LR, POSITION_RL, NAME_MATCHED, BY_TYPE, BY_LEFTOVER };
		map<int, how_assigned_t> how_assigned;
		auto name_for = [](how_assigned_t h) {
			switch(h)
			{
				case POSITION_LR: return "position left-to-right";
				case POSITION_RL: return "position right-to-left";
				case NAME_MATCHED: return "name matched";
				case BY_TYPE: return "by type";
				case BY_LEFTOVER: return "by leftover matching";
				default: assert(false);
			}
		};

		/* Pass 1: explicit positional matching. */
		/* 1a. From the left side.
		 * Keep going until we hit an in_args. */
		optional<unsigned> hit_in_args_ast_pos_pass1;
		optional<unsigned> hit_in_args_ast_pos_pass2;
		{
			INIT;
			FOR_ALL_CHILDREN(argsMultiValue)
			{
				switch (GET_TYPE(n))
				{
					case CAKE_TOKEN(KEYWORD_IN_ARGS):
						hit_in_args_ast_pos_pass1 = optional<unsigned>(i);
						goto pass1_loop_exit;
					default: /* i.e. any other stub expression */
						out[i] = make_pair(n, optional<string>());
						how_assigned[i] = POSITION_LR;
						caller_args_used.insert(i);
						ast_nodes_used.insert(i);
						break;
				}
			}
		pass1_loop_exit:
			if (hit_in_args_ast_pos_pass1)
			{
				// we have in_args
			}
		}

		/* Pass 2: explicit positional matching from right side.
		 * We can only do this if it's not a varargs function. */
		unsigned callee_fp_count = srk31::count(
			callee_subprogram->formal_parameter_children_begin(),
			callee_subprogram->formal_parameter_children_end());
		bool callee_is_varargs = !(callee_subprogram->unspecified_parameters_children_begin()
		 == callee_subprogram->unspecified_parameters_children_end());
		unsigned caller_fp_count = (ctxt.opt_source && ctxt.opt_source->signature)
		 ? srk31::count(
			ctxt.opt_source->signature->formal_parameter_children_begin(),
			ctxt.opt_source->signature->formal_parameter_children_end())
		 : 0;
		if (!callee_is_varargs && (callee_fp_count == caller_fp_count))
		{
			optional<unsigned> hit_in_args_pos;
			for (int i = callee_fp_count - 1;
					i >= 0; --i)
			{
				// we stop if we hit a position already filled by left-to-right
				if (out.find(i) != out.end()) break;
				
				// count forward to find the current fp
				unsigned pos = 0;
				auto i_fp = callee_subprogram->formal_parameter_children_begin();
				while ((signed) pos != i) { ++i_fp; ++pos; }

				// work out the corresponding AST node
				unsigned pos_from_rhs = callee_fp_count - 1 - i;
				antlr::tree::Tree *expr = 0;
				unsigned ast_pos = GET_CHILD_COUNT(argsMultiValue)
				 - pos_from_rhs;

				
				if (ast_nodes_used.find((int) ast_pos) 
					!= ast_nodes_used.end())
				{
					// this AST node is fair game
					antlr::tree::Tree *n = GET_CHILD(argsMultiValue,
						ast_pos);
					switch (GET_TYPE(n))
					{
						case CAKE_TOKEN(KEYWORD_IN_ARGS):
							hit_in_args_ast_pos_pass2 = optional<unsigned>(ast_pos);
							goto pass2_loop_exit;
						default: /* i.e. any other stub expression */
							out[i] = make_pair(n, optional<string>());
							how_assigned[i] = POSITION_RL;
							ast_nodes_used.insert(ast_pos);
							caller_args_used.insert(i); // HMM -- 
							// why need we be at the same position in caller and callee? 
							// We are assuming they have the same number of arguments.
							// But they might not, if some caller-supplied arguments are ignored.
							// Let's say this only works if caller and calle have same argcount.
							// Fixed by adding to the if-test above.
							break;
					}
				}

			}
		pass2_loop_exit:
			if (hit_in_args_ast_pos_pass2)
			{
				assert(hit_in_args_ast_pos_pass1);
				if (*hit_in_args_ast_pos_pass2
				!= *hit_in_args_ast_pos_pass1)
				{
					RAISE(GET_CHILD(argsMultiValue,
						*hit_in_args_ast_pos_pass1),
						"cannot use multiple instances of in_args "
						"in the same argument tuple");
				}
			}

		}

		/* NOTE: if we didn't hit in_args, we might have
		 * some unmatched args anyway if the caller
		 * supplied too few args. We only do the name-
		 * -matching if the caller put in_args somewhere. */
		auto pos_for_callee_arg_iter
		 = [callee_subprogram](
	 		dwarf::spec::subprogram_die::formal_parameter_iterator i_target
		)
		{
			dwarf::spec::subprogram_die::formal_parameter_iterator i_test
			 = callee_subprogram->formal_parameter_children_begin();
			unsigned matched_pos = 0;
			while (i_test != i_target
				&& i_test != callee_subprogram->formal_parameter_children_end())
			{ 
				++matched_pos; ++i_test;
				assert(i_test != callee_subprogram->formal_parameter_children_end());
			}
			return matched_pos;
		};
		auto argnum_for_caller_arg_name
		 = [ctxt](const string& name)
		{
			signed pos = 0;
			auto subp = ctxt.opt_source->signature;
			cerr << "Looking for argument named " << name << " in subprogram " 
				<< *subp << endl;
			auto i_fp = subp->formal_parameter_children_begin();
			for (;
				i_fp != subp->formal_parameter_children_end();
				++pos, ++i_fp)
			{
				if ((*i_fp)->get_name() 
				 && *(*i_fp)->get_name() == name) break;
			}
			if (i_fp == subp->formal_parameter_children_end())
			{
				// search failed
				return -1;
			} else return pos;
		};

		if (hit_in_args_ast_pos_pass1)
		{
			/* Now we match by name any leftover args */
			std::vector<
				std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
							dwarf::spec::subprogram_die::formal_parameter_iterator 
				> 
			> name_matched = name_match_parameters(
				ctxt.opt_source->signature,
				callee_subprogram
			);
			/* Now filter these matches:
			 * discard any pair that is already matched. */
			auto i_matched = name_matched.begin();
			while (i_matched != name_matched.end())
			{
				string name = *(*i_matched->first)->get_name();
				dwarf::spec::subprogram_die::formal_parameter_iterator i_test
				 = callee_subprogram->formal_parameter_children_begin();
				unsigned matched_pos
				 = pos_for_callee_arg_iter(i_matched->second);
				if (out.find((int) matched_pos) != out.end())
				{
					/* This means we've filled this pos already. */
					cerr << "Warning: arg pos " << matched_pos
						<< " matched with argument name " 
						<< name
						<< " already filled by positional matching "
						<< "with argument expression "
						<< CCP(TO_STRING_TREE(
							out[(int) matched_pos].first
							))
						<< endl;
					i_matched = name_matched.erase(i_matched);
				} else ++i_matched;
			}
			/* Sort them by position in the callee arg list.
			 * and check that they form a contiguous sequence 
			 * starting at our in_args pos. (HMM: really? don't check, for now)  */
			std::sort(
				name_matched.begin(),
				name_matched.end(),
				[](const std::pair< 
						dwarf::spec::subprogram_die::formal_parameter_iterator,
						dwarf::spec::subprogram_die::formal_parameter_iterator >& a,
					const std::pair< 
						dwarf::spec::subprogram_die::formal_parameter_iterator,
						dwarf::spec::subprogram_die::formal_parameter_iterator >& b)
				{ return a.second < b.second; });
			/* Now write the relevant entries to "out". */
			for (auto i_matched = name_matched.begin();
				i_matched != name_matched.end();
				++i_matched)
			{
				string name = *(*i_matched->first)->get_name();
				unsigned pos
				 = pos_for_callee_arg_iter(i_matched->second);
				signed caller_argnum
				 = argnum_for_caller_arg_name(name);
				assert(caller_argnum != -1); // should succeed
				out[pos] = make_pair(
					/* we make the AST ourselves! */
					make_ident_expr(basic_name_for_argnum(
						caller_argnum
						)),
					optional<string>()
				);
				caller_args_used.insert(caller_argnum);
				how_assigned[pos] = NAME_MATCHED;
			}
			/* We can now write that in_args has been used.
			 * HMM. What if we name-matched zero arguments? 
			 * Well, we still "used" this node. */
			ast_nodes_used.insert(*hit_in_args_ast_pos_pass1);
		}

		/* Now we still might have some callee args yet to be filled.
		 * First we try to fill ones that are not output-only,
		 * using the one-left-over approach.
		 * This means if we have a single AST-supplied expr
		 * remaining, we use it to fill a unique not-output-only
		 * callee arg position. */
		set<int> ast_nodes_unused;
		set<int> unused_caller_args;
		for (int i = 0; i < (signed) GET_CHILD_COUNT(argsMultiValue);
			++i)
		{
			if (ast_nodes_unused.find(i) == ast_nodes_used.end())
			{
				ast_nodes_unused.insert(i);
			}
		}
		for (int i = 0; i < (signed) caller_fp_count;
			++i)
		{
			if (caller_args_used.find(i) == caller_args_used.end())
			{
				unused_caller_args.insert(i);
			}
		}
		if 
		//(ast_nodes_unused.size() == 1) 
		//(ast_nodes_unused.size() == 1 || hit_in_args_ast_pos_pass1)
		//(unused_caller_args.size() == 1)
		(unused_caller_args.size() > 0)
		{
			/* We might have spare caller args even if all AST nodes are accounted for,
			 * e.g. if in_args covers two arguments but only one has been matched so far. */
			
			/* How can we enumerate the as-yet-unfilled callee
			 * arguments? If it's varargs, we have an infinity
			 * of them. Let's just enumerate the non-varargs ones. */
			set<int> unfilled_callee_non_output_only_args;
			set<int> unfilled_callee_args;
			int i = 0;
			for (auto i_fp = callee_subprogram->formal_parameter_children_begin();
				i_fp != callee_subprogram->formal_parameter_children_end();
				++i_fp, ++i)
			{
				if (out.find(i) == out.end())
				{
					unfilled_callee_args.insert(i);
					if (!arg_is_output_only(*i_fp))
					{
						unfilled_callee_non_output_only_args.insert(i);
					}
				}
			}
			cerr << "Trying to fill unused arguments (" 
				<< "not output only: " << unfilled_callee_non_output_only_args.size()
				<< ", total: " << unfilled_callee_args.size()
			<< ") using " << unused_caller_args.size() << " unused values from caller." << endl;
			
			if (unfilled_callee_non_output_only_args.size() > unused_caller_args.size())
			{
				RAISE(argsMultiValue, "not enough arguments for call");
			}

			/* This means we have a chance of filling the remaining callee arguments.
			 * In general, we use existence of a unique corresponding type and caller's
			 * provision of a suitable value. */
			auto caller_subprogram = ctxt.opt_source->signature;
			auto i_unfilled = unfilled_callee_non_output_only_args.begin();
			//for (auto i_unfilled = unfilled_callee_non_output_only_args.begin();
			//	i_unfilled != unfilled_callee_non_output_only_args.end();
			//	++i_unfilled)
			while (i_unfilled != unfilled_callee_non_output_only_args.end())
			{
				// get the callee argument
				auto i_callee_arg = srk31::nth_zero_based(
					callee_subprogram->formal_parameter_children_begin(),
					callee_subprogram->formal_parameter_children_end(),
					*i_unfilled);
				assert(i_callee_arg != callee_subprogram->formal_parameter_children_end());

				// what's the type of this argument?
				auto p_type = (*i_callee_arg)->get_type();

				// if there is no type, we can't do anything with this arg
				if (!p_type) 
				{
					cerr << "Callee argument " << (*i_callee_arg)->summary()
						<< " has no type info, so can't match by type." << endl;
					goto next_callee_arg;
				}
				else
				{

					// is it a pointer?
					bool is_a_pointer = (p_type->get_concrete_type() && 
						p_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type);
					shared_ptr<type_die> type_of_interest
					 = is_a_pointer 
					 	? dynamic_pointer_cast<pointer_type_die>(p_type->get_concrete_type())->get_type()
						: p_type;
					if (!type_of_interest)
					{
						cerr << "Callee argument " << (*i_callee_arg)->summary()
							<< " is a generic pointer, so can't match by type." << endl;
						goto next_callee_arg;
					}
					else 
					{
						// does this type have a unique corresponding type?
						auto corresponding_types = m_d.corresponding_dwarf_types(
								type_of_interest,
								m_d.module_of_die(caller_subprogram),
								/* flow_from_type_module_to_corresp_module */ false /* i.e. other way */,
								true /* canonicalise_before_add */);
						if (corresponding_types.size() != 1)
						{
							cerr << "Callee argument " << (*i_callee_arg)->summary()
								<< " has no unique corresponding type (candidates: ";
								if (corresponding_types.size() == 0) cerr << "none";
								else for (auto i_cand = corresponding_types.begin(); 
									i_cand != corresponding_types.end(); ++i_cand)
								{
									if (i_cand != corresponding_types.begin()) cerr << "; ";
									cerr << (*i_cand)->summary();
								}
								cerr << "), so can't match by type." << endl;
							goto next_callee_arg;
						}
						else
						{
							auto unique_corresponding_type = *corresponding_types.begin();
							assert(m_d.module_of_die(unique_corresponding_type)
							    == m_d.module_of_die(caller_subprogram));
							cerr << "Looking for a caller-supplied argument with type canonicalising to " 
								<< unique_corresponding_type->summary() << endl;

							// do we have a *unique* caller-side argument with this corresponding type?
							vector< pair<int, subprogram_die::formal_parameter_iterator > > candidates;
							for (auto i_caller_argnum = unused_caller_args.begin();
								i_caller_argnum != unused_caller_args.end();
								++i_caller_argnum)
							{
								auto i_caller_fp
								 = srk31::nth_zero_based(caller_subprogram->formal_parameter_children_begin(),
									caller_subprogram->formal_parameter_children_end(),
									*i_caller_argnum);
								assert(i_caller_fp != caller_subprogram->formal_parameter_children_end());

								if (!(*i_caller_fp)->get_type())
								{
									cerr << "Skipping untyped caller arg "
										<< (*i_caller_fp)->summary()
										<< endl;
									continue;
								}
								auto fp_concrete_type = (*i_caller_fp)->get_type()->get_concrete_type();

								if (!fp_concrete_type)
								{
									cerr << "Skipping void-typed caller arg (a bit strange) "
										<< (*i_caller_fp)->summary()
										<< endl;
									continue;
								}
								if (is_a_pointer)
								{
									if (fp_concrete_type->get_tag() != DW_TAG_pointer_type)
									{
										cerr << "Skipping non-pointer arg " << (*i_caller_fp)->summary()
											<< endl;
										continue;
									}

									auto fp_pointer_target_type = dynamic_pointer_cast<pointer_type_die>(
										fp_concrete_type)->get_type();

									assert(m_d.module_of_die(unique_corresponding_type) 
										== m_d.module_of_die(fp_pointer_target_type));
									auto caller_module = m_d.module_of_die(unique_corresponding_type);
									if (data_types_are_identical(
										canonicalise_type(unique_corresponding_type, caller_module, compiler),
										canonicalise_type(fp_pointer_target_type, caller_module, compiler)))
									{
										cerr << "Caller (pointer) argument " << (*i_caller_fp)->summary()
											<< " is a candidate." << endl;
										candidates.push_back(make_pair(*i_caller_argnum, i_caller_fp));
									}
									else
									{
										cerr << "Caller (pointer) argument " << (*i_caller_fp)->summary()
											<< " is not a candidate." << endl;
									}
								}
								else // !is_a_pointer
								{
									if (fp_concrete_type->get_tag() == DW_TAG_pointer_type)
									{
										cerr << "Skipping pointer arg " << (*i_caller_fp)->summary()
											<< endl;
										continue;
									}

									if (data_types_are_identical(
										canonicalise_type(unique_corresponding_type, m_d.module_of_die(unique_corresponding_type), compiler),
										canonicalise_type(fp_concrete_type, m_d.module_of_die(fp_concrete_type), compiler)))
									{
										cerr << "Caller (non-pointer) argument " << (*i_caller_fp)->summary()
											<< " is a candidate." << endl;
										candidates.push_back(make_pair(*i_caller_argnum, i_caller_fp));
									}
									else
									{
										cerr << "Caller (non-pointer) argument " << (*i_caller_fp)->summary()
											<< " is not a candidate, because type (corresp) "
											<< *unique_corresponding_type
											<< " and "
											<< *fp_concrete_type
											<< "are not identical." << endl;
									}
								}
							} // end for caller args
							if (candidates.size() == 1)
							{
								cerr << "Type-based matching gave a unique candidate for arg "
									<< (*i_callee_arg)->summary()
									<< " so assigning mapping." << endl;
								out[*i_unfilled]
								 = make_pair(
									/* we make the AST ourselves! */
									make_ident_expr(basic_name_for_argnum(
										candidates.begin()->first
										)),
									optional<string>()
									);
								how_assigned[*i_unfilled] = BY_TYPE; 
								int saved_unfilled_pos = *i_unfilled;
								i_unfilled = unfilled_callee_non_output_only_args.erase(i_unfilled);
								unfilled_callee_args.erase(unfilled_callee_args.find(saved_unfilled_pos));
								unused_caller_args.erase(unused_caller_args.find(candidates.begin()->first));
								continue; // i.e. successful continue, skipping increment
							}
							else
							{
								cerr << "Type-based matching gave no unique candidate (count: "
									<< candidates.size()
									<< ") for arg "
									<< (*i_callee_arg)->summary()
									<< endl;
							} // end if unique candidates
						} // end else
					} // end else
				} // end else
			next_callee_arg:
				++i_unfilled;
			} // end while unfilled  
			// i.e. end type-based matching
			
			auto set_as_string = [](const std::set<int>& s) {
				std::ostringstream str;
				str << "{";
				for (auto i_el = s.begin(); i_el != s.end(); ++i_el)
				{
					if (i_el != s.begin()) str << ", ";
					str << *i_el;
				}
				str << "}";
				return str.str();
			};
			cerr << "Beginning leftover matching with " 
				<< unfilled_callee_non_output_only_args.size() << " callee non-o-o args left (" 
				<< set_as_string(unfilled_callee_non_output_only_args) << ", "
				<< ", " << unfilled_callee_args.size() << " callee args total left ("
				<< set_as_string(unfilled_callee_args) << "), "
				<< ", " << unused_caller_args.size() << " caller args left ("
				<< set_as_string(unused_caller_args) << ")" << endl;
			
			//if (unfilled_callee_non_output_only_args.size() == 1)
			if (unfilled_callee_non_output_only_args.size() == unused_caller_args.size())
			{
				while (unfilled_callee_non_output_only_args.size() > 0)
				{
					auto pos = *unfilled_callee_non_output_only_args.begin();
					cerr << "Filling lone unfilled non-output-only callee arg in pos " << pos << endl;
					/* Fill this argument. For the moment, since we have ensured that
					 * there is at most one in_args, and in the non-variadic case
					 * we have processed things to both the left and the right of it,
					 * it can only come from extra callee args (i.e. not name-matched),
					 * not from the AST. */
					out[pos]
					 = make_pair(
						/* we make the AST ourselves! */
						make_ident_expr(basic_name_for_argnum(
							*unused_caller_args.begin()
							)),
						optional<string>()
						);
					how_assigned[pos] = BY_LEFTOVER;
					unfilled_callee_non_output_only_args.erase(
						unfilled_callee_non_output_only_args.begin());
					unused_caller_args.erase(unused_caller_args.begin());
				}
			}
			else if //(unfilled_callee_args.size() == 1)
				(unfilled_callee_args.size() == unused_caller_args.size())
			{
				while (unfilled_callee_args.size() > 0)
				{
					auto pos = *unfilled_callee_args.begin();
					cerr << "Filling lone unfilled callee arg in pos " << pos << endl;
					/* Fill this argument. Same comment as above. */
					out[pos]
					 = make_pair(
						//GET_CHILD(argsMultiValue, *ast_nodes_unused.begin()),
						make_ident_expr(basic_name_for_argnum(
							*unused_caller_args.begin()
							)),
						optional<string>()
						);
					how_assigned[pos] = BY_LEFTOVER;
					unfilled_callee_args.erase(unfilled_callee_args.begin());
					unused_caller_args.erase(unused_caller_args.begin());
				}
			}
		}
		else if (ast_nodes_unused.size() > 1)
		{
			/* FIXME: we might still want these, for varargs. */
			RAISE(argsMultiValue, "contains extraneous arguments");
		}
		else assert(ast_nodes_unused.size() == 0);
		
		cerr << "Summary of argument mappings for call to " << callee_subprogram->summary()
			<< ": " << endl;

		for (auto i_ent = out.begin(); i_ent != out.end(); ++i_ent)
		{
			cerr << "Argument position " << i_ent->first
				<< " has been filled by ";
			if (i_ent->second.first) cerr << "expression " 
				<< CCP(TO_STRING_TREE(i_ent->second.first));
			else cerr << "(no expression; must be output-only arg)";
			assert(how_assigned.find(i_ent->first) != how_assigned.end());
			cerr << " using match criterion " << name_for(how_assigned[i_ent->first]) << endl;
		}
	}
	
}
