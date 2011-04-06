#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"
#include <sstream>
#include <cmath>

namespace cake
{
	bool wrapper_file::treat_subprogram_as_untyped(
        const dwarf::encap::Die_encap_subprogram& subprogram)
    {
    	dwarf::encap::Die_encap_subprogram& nonconst_subprogram
         = const_cast<dwarf::encap::Die_encap_subprogram&>(subprogram);
        dwarf::encap::formal_parameters_iterator args_begin 
        	= nonconst_subprogram.formal_parameters_begin();
        dwarf::encap::formal_parameters_iterator args_end
        	= nonconst_subprogram.formal_parameters_end();
        return (args_begin == args_end
        				 && nonconst_subprogram.unspecified_parameters_begin() !=
                    	nonconst_subprogram.unspecified_parameters_end());
    }
	// new version!
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
        const dwarf::encap::Die_encap_subprogram& subprogram)
	{
		if (!subprogram.get_type())
        {
        	if (treat_subprogram_as_untyped(subprogram))
            {
	            std::cerr << "Warning: assuming function " << subprogram << " is void-returning."
                	<< std::endl;
            }
        	return true;
        }
        return false;
	}        	
	
	void wrapper_file::emit_component_type_name(boost::shared_ptr<dwarf::spec::type_die> t)
	{
		// get the fq name; then test whether it's a keyword
		auto fq_name = compiler.fq_name_for(t);
		if (compiler.is_reserved(fq_name))
		{
			m_out << fq_name;
		}
		else m_out << ns_prefix << "::" 
			<< m_r.module_inverse_tbl[module_of_die(t)] << "::" << fq_name;
	}
	
    void wrapper_file::emit_function_header(
        antlr::tree::Tree *event_pattern,
        const std::string& function_name_to_use,
        dwarf::encap::Die_encap_subprogram& subprogram,
        const std::string& arg_name_prefix,
        const request::module_name_pair& caller_context,
        bool emit_types /*= true*/,
		boost::shared_ptr<dwarf::spec::subprogram_die> unique_called_subprogram)
    {
    	auto ret_type = subprogram.get_type();
        auto args_begin = subprogram.formal_parameter_children_begin();
        auto args_end = subprogram.formal_parameter_children_end();
        bool ignore_dwarf_args = treat_subprogram_as_untyped(subprogram);
        
        if (emit_types)
        {
        	if (subprogram_returns_void(subprogram)) m_out << "void";
            else if (treat_subprogram_as_untyped(subprogram) && !unique_called_subprogram) m_out << 
            	" ::cake::unspecified_wordsize_type";
			else if (treat_subprogram_as_untyped(subprogram) && unique_called_subprogram)
			{
				// look for a _unique_ _corresponding_ type and use that
				assert(unique_called_subprogram->get_type());
				auto found_vec = m_d.corresponding_dwarf_types(
					*unique_called_subprogram->get_type(),
					caller_context.first,
					true /* flow_from_type_module_to_corresp_module */);
				if (found_vec.size() == 1)
				{
					// we're in luck
					emit_component_type_name(found_vec.at(0));
				}
				else 
				{
					m_out << " ::cake::unspecified_wordsize_type";
				}
			}
            else emit_component_type_name(*ret_type);
             
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
						/* We will use the DWARF info on the *callee* subprogram. */
						//goto use_unique_called_subprogram_args;
					// get argument at position argnum from the subprogram;
					/*auto*/ i_fp = unique_called_subprogram->formal_parameter_children_begin();
					//i_arg = unique_called_subprogram->formal_parameter_children_begin();
					for (int i = 0; i < argnum; i++) i_fp++;
	//xxxxxxxxxxxxxxxxxxxxxxx
					if (emit_types)
					{
						if ((*i_fp)->get_type())
						{
							// look for a _unique_ _corresponding_ type and use that
							auto found_vec = m_d.corresponding_dwarf_types(
								*(*i_fp)->get_type(),
								caller_context.first,
								false /* flow_from_type_module_to_corresp_module */);
							if (found_vec.size() == 1)
							{
								std::cerr << "Found unique corresponding type" << std::endl;
								// we're in luck
								emit_component_type_name(found_vec.at(0));
							}
							else 
							{
								std::cerr << "Didn't find unique corresponding type" << std::endl;
								if (treat_subprogram_as_untyped(unique_called_subprogram))
								{ m_out << " ::cake::unspecified_wordsize_type"; }
								else emit_component_type_name(*(*i_fp)->get_type());
							}
						}
						else  // FIXME: remove duplication here ^^^ vvv
						{
							if (treat_subprogram_as_untyped(unique_called_subprogram))
							{ m_out << " ::cake::unspecified_wordsize_type"; }
							else emit_component_type_name(*(*i_fp)->get_type());
						}
					}
	//				else
	//				{
	//				
	//xxxxxxxxxxxxxxxxxxxxxxxx				
	//                     m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
	//                        *(*i_fp)->get_type()));
	//					}
					if ((*i_fp)->get_name())
					{
                    	// output the variable name, prefixed 
                    	m_out << ' ' << arg_name_prefix << argnum << '_' << *(*i_fp)->get_name();
					}
					else
					{
                    	// output the argument type and a dummy name
                    	//if (emit_types) m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
                    	//    *(*i_fp)->get_type()));
                    	m_out << ' ' << arg_name_prefix << argnum << "_dummy"/* << argnum*/;
					}
					goto next;
				}
				else if (!ignore_dwarf_args && unique_called_subprogram)
				{ /* FIXME: Check that they're consistent! */ }
                break;
			use_event_pattern_args:
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
                                    if (emit_types) m_out << (ignore_dwarf_args ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
                                	    *(*i_arg)->get_type()));
                                    // output the variable name, prefixed 
                                    m_out << ' ' << arg_name_prefix << argnum << '_' << mn.at(0);
	                            } break;
                            case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    case CAKE_TOKEN(METAVAR):
                            case CAKE_TOKEN(KEYWORD_CONST):
                            	// output the argument type and a dummy name
                                if (emit_types) m_out << (ignore_dwarf_args ? "::cake::unspecified_wordsize_type" : compiler.name_for(
                                	*(*i_arg)->get_type()));
                                m_out << ' ' << arg_name_prefix << argnum << "_dummy"/* << argnum*/;
                                break;
                            default: RAISE_INTERNAL(valuePattern, "not a value pattern");
                        } // end switch
					} // end ALIAS3 
				} // end FOR_REMAINING_CHILDREN
			}	// end default; fall through!
            next:
                // work out whether we need a comma
                if (!ignore_dwarf_args) i_arg++; // advance DWARF caller arg cursor
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
			break;
			use_unique_called_subprogram_args:
				// get argument at position argnum from the subprogram;
				/*auto*/ i_fp = unique_called_subprogram->formal_parameter_children_begin();
				//i_arg = unique_called_subprogram->formal_parameter_children_begin();
				for (int i = 0; i < argnum; i++) i_fp++;
//xxxxxxxxxxxxxxxxxxxxxxx
				if (emit_types)
				{
					if ((*i_fp)->get_type())
					{
						// look for a _unique_ _corresponding_ type and use that
						auto found_vec = m_d.corresponding_dwarf_types(
							*(*i_fp)->get_type(),
							caller_context.first,
							false /* flow_from_type_module_to_corresp_module */);
						if (found_vec.size() == 1)
						{
							std::cerr << "Found unique corresponding type" << std::endl;
							// we're in luck
							emit_component_type_name(found_vec.at(0));
						}
						else 
						{
							std::cerr << "Didn't find unique corresponding type" << std::endl;
							m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
                        		*(*i_fp)->get_type()));
						}
					}
					else  // FIXME: remove duplication here ^^^ vvv
					{
						m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
                        	*(*i_fp)->get_type()));
					}
				}
//				else
//				{
//				
//xxxxxxxxxxxxxxxxxxxxxxxx				
//                     m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? " ::cake::unspecified_wordsize_type" : compiler.name_for(
//                        *(*i_fp)->get_type()));
//					}
				if ((*i_fp)->get_name())
				{
                    // output the variable name, prefixed 
                    m_out << ' ' << arg_name_prefix << argnum << '_' << *(*i_fp)->get_name();
				}
				else
				{
                    // output the argument type and a dummy name
                    //if (emit_types) m_out << (treat_subprogram_as_untyped(unique_called_subprogram) ? "::cake::unspecified_wordsize_type" : compiler.name_for(
                    //    *(*i_fp)->get_type()));
                    m_out << ' ' << arg_name_prefix << argnum << "_dummy"/* << argnum*/;
				}
				goto next;
        } // end switch
			
		// if we have spare arguments at the end, something's wrong
        if (!ignore_dwarf_args && i_arg != args_end)
        {
            std::ostringstream msg;
            msg << "argument pattern has too few arguments for subprogram: "
                << subprogram;
            RAISE(event_pattern, msg.str());
        }

        //} // end switch
                        
        m_out << ')';

        m_out.flush();
    }

    void wrapper_file::emit_wrapper(
        	const std::string& wrapped_symname, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps,
            const request::module_inverse_tbl_t& request_context)
    {
        // ensure a nonempty corresp list
        if (corresps.size() == 0) return;

	    // 1. emit wrapper prototype
        /* We take *any* source module, and assume that its call-site is
         * definitive. This *should* give us args and return type, but may not.
         * If our caller's DWARF doesn't specify arguments and return type,
         * we guess from the pattern AST. If the pattern AST doesn't include
         * arguments, we just emit "..." and an int return type.
         */
	    std::cerr << "Building wrapper for symbol " << wrapped_symname << std::endl;
        
        // iterate over all names children of the source module of the first corresp
        for (dwarf::encap::named_children_iterator i = 
        	corresps.at(0)->second.source->all_compile_units().named_children_begin();
            i != corresps.at(0)->second.source->all_compile_units().named_children_end();
            i++)
        {
        	std::cerr << "Named child in all_compile_units: " << *((*i)->get_name()) << std::endl;
        }

		// get the subprogram description of the wrapped
        // function in the source module
	    auto subp = 
            corresps.at(0)->second.source->all_compile_units() // all corresps share same source mdl
                .visible_named_child(wrapped_symname);
		if (!subp) 
		{
			std::cerr << "Bailing from wrapper generation -- no subprogram!" << std::endl; 
			return; 
		}

        // convenience reference alias of subprogram
        auto subprogram = 
        	*boost::dynamic_pointer_cast<dwarf::encap::subprogram_die>(subp->get_this());
        // caller module--name pair
        auto i_caller_module_and_name = request_context.find(module_of_die(subprogram.get_this()));
        assert(i_caller_module_and_name != request_context.end());
        auto caller_module_and_name = *i_caller_module_and_name;
		
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
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__real_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name, true, unique_called_subprogram);
        m_out << " __attribute__((weak));" << std::endl;
        // output prototype for __wrap_
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__wrap_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name, true, unique_called_subprogram);
        m_out << ';' << std::endl;
        m_out << "} // end extern \"C\"" << std::endl;
        // output wrapper
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__wrap_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name, true, unique_called_subprogram);
        m_out << std::endl;
        m_out << " {";
        m_out.inc_level();
        m_out << std::endl;
		
		/* FATAL-ish problem: 
		 * by lumping all call-sites using a particular symbol name
		 * into the same wrapper,
		 * we are inevitably requiring that different argument lists
		 * are captured by the same wrapper signature.
		 * This is really annoying.
		 * It's far better to make a wrapper
		 * per-component, per symbol name.
		 * For us right now it makes little difference.
		 * We can do this in the future by
		 * prefixing (renaming) all undefined symbols
		 * with their component name as a prefix.
		 */

        // 3. emit wrapper definition
        emit_wrapper_body(wrapped_symname, subprogram, corresps, request_context);
        
        // 4. emit_wrapper_body leaves us a dangling "else", so clear that up
        m_out << "return ";
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__real_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name, false, unique_called_subprogram);
        m_out << ';' << std::endl; // end of return statement
        m_out.dec_level();
        m_out << '}' << std::endl; // end of block
    }

    wrapper_file::bound_var_info::bound_var_info(
    	wrapper_file& w,
        const std::string& prefix,
        boost::shared_ptr<dwarf::spec::type_die> type,
	    const request::module_name_pair& defining_module,
	    boost::shared_ptr<dwarf::spec::program_element_die> origin)
        : prefix(prefix), type(type), defining_module(defining_module), origin(origin)
        {
            // this version is *not* for formal parameters! since they should
            // have different names
            assert((origin->get_tag() != DW_TAG_formal_parameter)
                || (std::cerr << *origin, false));
            this->count = w.binding_count++;
        }
    // special version for formal parameters! works out count and prefix
    // -- note: not virtual, so make sure we only use this for formal_parameter DIEs
    wrapper_file::bound_var_info::bound_var_info(
    	wrapper_file& w,
        //const std::string& prefix,
        boost::shared_ptr<dwarf::spec::type_die> type,
	    const request::module_name_pair& defining_module,
	    boost::shared_ptr<dwarf::spec::formal_parameter_die> origin)
        : prefix("arg"), type(type), defining_module(defining_module), origin(origin) 
    {
        assert(origin || (std::cerr << std::hex << &origin << std::dec << std::endl));
        assert(origin->get_parent() || (std::cerr << *origin, false));
        auto p_subprogram = boost::dynamic_pointer_cast<dwarf::spec::subprogram_die>(
            origin->get_parent());
        int i = 0;
        bool found = false;
        for (auto i_fp = p_subprogram->formal_parameter_children_begin();
            i_fp != p_subprogram->formal_parameter_children_end();
            i_fp++)
        {
            if ((*i_fp)->get_offset() == origin->get_offset())
            {
                found = true;
                break;
            }
            else ++i;
        }
        if (!found) throw InternalError(0, "couldn't discover argument position");

        count = i;
    }
    wrapper_file::bound_var_info::bound_var_info(
    	wrapper_file& w,
        //const std::string& prefix,
        boost::shared_ptr<dwarf::spec::type_die> type,
	    const request::module_name_pair& defining_module,
	    boost::shared_ptr<dwarf::spec::unspecified_parameters_die> origin)
        : prefix("arg"), type(type), defining_module(defining_module), origin(origin) 
    {
        // We keep in the wrapper_file a map keyed on a stable identifier
        // for the DIE, which counts the arguments that we've bound from
        // this particular unspecified_parameters die, so that we can issue
        // a new arg number for each one.

        // NOTE: this assumes that in the same wrapper_file, we don't 
        // ever do more than one round of binding names to the same arguments
        // -- otherwise we'll get a continuation of the previous sequence
        // of numbers.

        // FIXME: doesn't work for varargs at the moment!
        // We need to get the number of declared arguments, and start
        // the count there rather than at zero (at present).
        count = w.arg_counts[w.get_stable_die_ident(origin)]++;
    }

    
    void wrapper_file::emit_wrapper_body(
        	const std::string& wrapped_symname,
            const dwarf::encap::Die_encap_subprogram& wrapper_sig, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps,
            const request::module_inverse_tbl_t& request_context)
    {
    	environment outer_env;
        
    	// for each corresp, match its pattern
        for (link_derivation::ev_corresp_pair_ptr_list::iterator i_pcorresp = corresps.begin();
        		i_pcorresp != corresps.end(); i_pcorresp++)
        {
        	environment env = outer_env;
            antlr::tree::Tree *pattern = (*i_pcorresp)->second.source_pattern;
            antlr::tree::Tree *action = (*i_pcorresp)->second.sink_expr;
        	m_out << "if (";
            auto source_module = *request_context.find((*i_pcorresp)->second.source);
            auto sink_module = *request_context.find((*i_pcorresp)->second.sink);
			
			/* Emit a boolean expression which is true iff the event pattern
			 * is satisfied. PROBLEM: what if we don't have information about
			 * the values being passed? This is typically the case when debug info
			 * does not record the signature of subprograms *required* by code. 
			 * 
			 * We can use information in the sink expression to extract constraints
			 * on what the provided arguments might be. This allows us to avoid
			 * relying on the wrapper signature for argument information.
			 */
			
			extract_source_bindings(pattern, source_module, &env,
				action /* i.e. sink expression */); 
				// add any *pattern*-bound names to the environment
			
            emit_pattern_condition(pattern, source_module,
            	&env); // emit if-test
				
            m_out << ")" << std::endl;
            m_out << "{";
            m_out.inc_level();
            m_out << std::endl;
			
            emit_sink_action(action, 
            	wrapper_sig,
            	sink_module,
                source_module,
                env);
			/* The sink action should leave us with a return value (if any)
			 * and a success state / error value. We then encode these and
			 * output the return statement *here*. */
            m_out.dec_level();
            m_out << "}" << std::endl;
            m_out << "else ";
        }
    }
    
	void wrapper_file::extract_source_bindings(
            antlr::tree::Tree *pattern,
            const request::module_name_pair& request_context,
            environment *out_env,
			antlr::tree::Tree *sink_action)
	{
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
			auto caller = request_context.first->all_compile_units().visible_resolve(
				call_mn.begin(), call_mn.end());
			if (!caller) RAISE(memberNameExpr, "does not name a visible function");
			if ((*caller).get_tag() != DW_TAG_subprogram) 
				RAISE(memberNameExpr, "does not name a visible function"); 
			auto caller_subprogram = dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*caller);
			
			int argnum = 0;
			dwarf::encap::formal_parameters_iterator i_caller_arg 
			 = caller_subprogram.formal_parameters_begin();
			int dummycount = 0;
			FOR_REMAINING_CHILDREN(eventPattern)
			{
				boost::shared_ptr<dwarf::spec::type_die> p_arg_type = boost::shared_ptr<dwarf::spec::type_die>();
				boost::shared_ptr<dwarf::spec::program_element_die> p_arg_origin;
				
				if (i_caller_arg == caller_subprogram.formal_parameters_end())
				{
					if (caller_subprogram.unspecified_parameters_begin() !=
						caller_subprogram.unspecified_parameters_end())
					{
						p_arg_origin = *caller_subprogram.unspecified_parameters_children_begin();
					}
					else RAISE(eventPattern, "too many arguments for function");
				}
				else
				{
					p_arg_type = *(*i_caller_arg)->get_type();
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
					
					/* No matter what sort of value pattern we find, we generate a
					 * bound name for the argument of the wrapper function. If the
					 * pattern doesn't provide a name, it will be generated. So our
					 * chosen name depends on the kind of pattern. */
					std::string bound_name;
					switch(GET_TYPE(valuePattern))
					{
						case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
							// could match anything, so bind name and continue
							definite_member_name mn = read_definite_member_name(valuePattern);
							if (mn.size() != 1) RAISE(valuePattern, "may not be compound");
							bound_name = mn.at(0);
						} break;
						case CAKE_TOKEN(KEYWORD_CONST):
						case CAKE_TOKEN(INDEFINITE_MEMBER_NAME): {
							std::ostringstream s; s << "dummy"; s << dummycount++;
							bound_name = s.str();
						} break;
						default: RAISE(valuePattern, "unexpected token");
					}
					/* Now insert the binding. Which constructor we use depends on
					 * whether the argument in the caller is described by a formal_parameter
					 * or unspecified_parameters DIE. */
					if (origin_as_fp)
					{
						out_env->insert(std::make_pair(bound_name, bound_var_info(
							*this, //"arg",  -- this is inferred from the ^^^ constructor overload?
							p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
							request_context, // the *source* module
							origin_as_fp)));  // the DIE of the argument in the caller's info
					}
					else
					{
						out_env->insert(std::make_pair(bound_name, bound_var_info(
							*this, //"arg", 
							p_arg_type ? p_arg_type : boost::shared_ptr<dwarf::spec::type_die>(),
							request_context, // the *source* module
							origin_as_unspec)));  // the DIE of the argument in the caller's info
					}
				} // end ALIAS3(annotatedValuePattern
				++argnum;
				if (i_caller_arg != caller_subprogram.formal_parameters_end()) i_caller_arg++;
			} // end FOR_REMAINING_CHILDREN(eventPattern
		} // end ALIAS3(pattern, eventPattern, EVENT_PATTERN)
	} // end 
					
	void wrapper_file::emit_pattern_condition(
			antlr::tree::Tree *pattern,
			const request::module_name_pair& request_context,
			environment *out_env)
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
//			 if (call_mn.size() != 1) RAISE(memberNameExpr, "may not be compound");			
//			 auto caller = request_context.first->all_compile_units().visible_resolve(
//			 	call_mn.begin(), call_mn.end());
//			 if (!caller) RAISE(memberNameExpr, "does not name a visible function");
//			 if ((*caller).get_tag() != DW_TAG_subprogram) 
//			 	RAISE(memberNameExpr, "does not name a visible function"); 
//			 auto caller_subprogram = dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*caller);
			
			int argnum = 0;
			//dwarf::encap::formal_parameters_iterator i_caller_arg 
			// = caller_subprogram.formal_parameters_begin();
			int dummycount = 0;
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
							// Note that binding has *already* happened! We 
							// grabbed the names out of the event pattern in
							// extract_source_bindings. 
							definite_member_name mn = read_definite_member_name(valuePattern);
							bound_name = mn.at(0);
						} break;
						case CAKE_TOKEN(INDEFINITE_MEMBER_NAME): {
							std::ostringstream s; s << "dummy"; s << dummycount++;
							bound_name = s.str();
							// no conditions -- match anything (and don't bind)
						} break;
						case CAKE_TOKEN(KEYWORD_CONST): {
							std::ostringstream s; s << "dummy"; s << dummycount++;
							bound_name = s.str();
							boost::shared_ptr<dwarf::spec::type_die> p_arg_type;
							// find bound val's type, if it's a formal parameter
							assert(out_env->find(bound_name) != out_env->end());
							
							// recover argument type, if we have one
							if (out_env->find(bound_name)->second.origin->get_tag()
								== DW_TAG_formal_parameter)
							{
								auto origin_as_fp = boost::dynamic_pointer_cast<dwarf::spec::formal_parameter_die>(
									out_env->find(bound_name)->second.origin);
								if (origin_as_fp)
								{
									p_arg_type = *origin_as_fp->get_type();
								}
							}
							// we have a condition to output
							if (emitted) m_out << " && ";
							m_out << "cake::equal<";
							if (p_arg_type) emit_type_name(p_arg_type/*, 
								ns_prefix + request_context.second*/);
							else m_out << " ::cake::unspecified_wordsize_type";
							m_out << ", "
 << (	(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(STRING_LIT)) ? " ::cake::style_traits<0>::STRING_LIT" :
		(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(CONST_ARITH)) ? " ::cake::style_traits<0>::CONST_ARITH" :
		" ::cake::unspecified_wordsize_type" );
							m_out << ">()(";
							//m_out << "arg" << argnum << ", ";
							m_out << cxx_name_for_binding(*out_env->find(bound_name)) << ", ";
							emit_constant_expr(valuePattern, request_context);
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


	void wrapper_file::emit_sink_action(
			antlr::tree::Tree *action,
			const dwarf::encap::Die_encap_subprogram& wrapper_sig, 
			const request::module_name_pair& sink_context,
			const request::module_name_pair& source_context,
			environment env)
	{
		// this is a crossover point, so insert conversions according to context
		std::cerr << "Processing stub action: " << CCP(TO_STRING_TREE(action)) << std::endl;
		assert(GET_TYPE(action) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
		INIT;
		//BIND3(action, eventPattern, EVENT_PATTERN);
		BIND2(action, stub);
		
		//emit_event_pattern_as_function_call(eventPattern, sink_context, 
		//	source_context, wrapper_sig, env);
		
		std::cerr << "Emitting event correspondence stub: "
			<< CCP(TO_STRING_TREE(stub)) << std::endl;
		m_out << "// " << CCP(TO_STRING_TREE(stub)) << std::endl;
		
		auto names = emit_event_corresp_stub(
			stub, 
			link_derivation::sorted(std::make_pair(source_context.first, sink_context.first)),
			sink_context, 
			source_context, 
			wrapper_sig, 
			env);
		//m_out << "if (" << names.first << ") return " << names.second << ';' << std::endl;
		//m_out << "else return __cake_failure_with_type_of(" << names.second << ");" << std::endl;
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
        
    void wrapper_file::emit_type_name(
            boost::shared_ptr<dwarf::spec::type_die> t)
    {
    	m_out << get_type_name(t);
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
    	link_derivation::iface_pair ifaces_context,
    	boost::shared_ptr<dwarf::spec::type_die> from_type,
        module_ptr from_module,
        boost::shared_ptr<dwarf::spec::type_die> to_type,
        module_ptr to_module)
    {
    	assert((!from_type || module_of_die(from_type) == from_module)
        	&& (!to_type || module_of_die(to_type) == to_module));
        
        // if we have a "from that" type, use it directly
        if (from_type)
        {
            m_out << "cake::value_convert<";
            if (from_type) emit_type_name(from_type/*, ns_prefix + "::" + from_namespace_unprefixed*/);
            else m_out << " ::cake::unspecified_wordsize_type";
            m_out << ", ";
            if (to_type) emit_type_name(to_type/*, ns_prefix + "::" + to_namespace_unprefixed*/); 
            else m_out << " ::cake::unspecified_wordsize_type";
            m_out << ">()(";
        }
        else
        {
        	// use the function template
            emit_component_pair_classname(ifaces_context);
            
            if (ifaces_context.first == from_module)
            {
            	assert(ifaces_context.second == to_module);
                
                m_out << "::value_convert_from_first_to_second<" 
                << (to_type ? get_type_name(to_type) : " ::cake::unspecified_wordsize_type" )
                << ">(";
            }
            else 
            {
            	assert(ifaces_context.second == from_module);
            	assert(ifaces_context.first == to_module);

                m_out << "::value_convert_from_second_to_first<" 
                << (to_type ? get_type_name(to_type) : " ::cake::unspecified_wordsize_type" )
                << ">(";
            }
        }
    }
    
    void wrapper_file::close_value_conversion()
    {
		m_out << ")";
	}
    
    void wrapper_file::emit_component_pair_classname(link_derivation::iface_pair ifaces_context)
    {
    	m_out << "cake::component_pair< " 
        		<< ns_prefix << "::" << m_d.name_of_module(ifaces_context.first) << "::marker"
        		<< ", "
              	<< ns_prefix << "::" << m_d.name_of_module(ifaces_context.second) << "::marker"
                << ">";
	}
    
    std::pair<std::string, std::string> 
    wrapper_file::emit_event_corresp_stub(
    	antlr::tree::Tree *stub, 
        link_derivation::iface_pair ifaces_context,
        const request::module_name_pair& sink_context, // sink module
        const request::module_name_pair& source_context,
        const dwarf::encap::Die_encap_subprogram& source_signature,
        environment env)
    {
//     	auto cxx_result_type_name = source_signature.get_type() 
//               ? ns_prefix + "::" + source_context.second + "::" 
//               	+ compiler.fq_name_for(
//                 	boost::dynamic_pointer_cast<dwarf::spec::type_die>(
//                     	(*source_signature.get_type())->get_this()
//                     )
//                 )
//               : "::cake::unspecified_wordsize_type";
// this logic is WRONG because it doesn't handle void-returning funcs properly
// -- use convert_to instead
		boost::shared_ptr<dwarf::spec::type_die> convert_to 
			= treat_subprogram_as_untyped(source_signature) 
				? boost::shared_ptr<dwarf::spec::type_die>()
				: *source_signature.get_type();
		
		// call the function; "names" will contain (success-varname, output-varname)
		std::cerr << "Event sink stub is: " << CCP(TO_STRING_TREE(stub)) << std::endl;
		assert(GET_TYPE(stub) == CAKE_TOKEN(INVOKE_WITH_ARGS));
		auto names = emit_stub_expression_as_statement_list(
			stub, ifaces_context, sink_context, 
			/*cxx_result_type_name*/convert_to, env);

		const std::string& caller_namespace_name = source_context.second;
		const std::string& callee_namespace_name = sink_context.second;    
		link_derivation::iface_pair corresp_context
			= link_derivation::sorted(std::make_pair(source_context.first, sink_context.first));

		// FIXME: test success of result here

		//if (!subprogram_returns_void(callee_subp)) 
		if (!subprogram_returns_void(source_signature)) 
		{
			/* Main problem here is that we don't know the C++ type of the
			* stub's result value. So we have to make value_convert available
			* as a function template here. Hopefully this will work. */
			m_out << "return ";
			open_value_conversion(
				ifaces_context,
				/*callee_return_type ? 
				*callee_return_type 
				: */ boost::shared_ptr<dwarf::spec::type_die>(), 
				sink_context.first, 
				//callee_namespace_name,
				convert_to /*? *convert_to : boost::shared_ptr<dwarf::spec::type_die>()*//*, 
				caller_namespace_name*/ , source_context.first);

            // output result
            m_out << names.second;

            /*if (!subprogram_returns_void(callee_subprogram))*/ close_value_conversion();
            m_out << ";" << std::endl;
    	}
        return names;
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

	std::pair<std::string, std::string> 
    wrapper_file::emit_stub_expression_as_statement_list(
    		antlr::tree::Tree *expr,
    		link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& context, // sink module
            ///*boost::shared_ptr<dwarf::spec::type_die>*/ const std::string& cxx_result_type_name,
            boost::shared_ptr<dwarf::spec::type_die> cxx_result_type,
            environment env)
    {
    	std::string ident;
    	switch(GET_TYPE(expr))
        {
        	case CAKE_TOKEN(INVOKE_WITH_ARGS): // REMEMBER: algorithms are special
            	return emit_stub_function_call(
        	        expr,
                    ifaces_context,
                    context, // sink module
                    //cxx_result_type_name,
                    cxx_result_type,
                    env);
            case CAKE_TOKEN(STRING_LIT):
                ident = new_ident("bound");
                m_out << "auto " << ident << " = ";
            	m_out << "cake::style_traits<0>::string_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
                return std::make_pair("true", ident);
            case CAKE_TOKEN(INT):
                ident = new_ident("bound");
                m_out << "auto " << ident << " = ";
            	m_out << "cake::style_traits<0>::int_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
                return std::make_pair("true", ident);
            case CAKE_TOKEN(FLOAT):
                ident = new_ident("bound");
                m_out << "auto " << ident << " = ";
            	m_out << "cake::style_traits<0>::float_lit(" << CCP(GET_TEXT(expr)) << ");" << std::endl;
                return std::make_pair("true", ident);
            case CAKE_TOKEN(KEYWORD_VOID):
                //std::string ident = new_ident("bound");
                //m_out << "auto " << ident << " = ";
            	//m_out << "cake::style_traits<0>::void_value()";
                return std::make_pair("true", /*ident*/ "((void)0)");
            case CAKE_TOKEN(KEYWORD_NULL):
                ident = new_ident("bound");
                m_out << "auto " << ident << " = ";
	            m_out << "cake::style_traits<0>::null_value();" << std::endl;
                return std::make_pair("true", ident);
            case CAKE_TOKEN(KEYWORD_TRUE):
                ident = new_ident("bound");
                m_out << "auto " << ident << " = ";
	            m_out << "cake::style_traits<0>::true_value();" << std::endl;
                return std::make_pair("true", ident);
            case CAKE_TOKEN(KEYWORD_FALSE):
                ident = new_ident("bound");
                m_out << "auto " << ident << " = ";
	            m_out << "cake::style_traits<0>::false_value();" << std::endl;
                return std::make_pair("true", ident);
            case CAKE_TOKEN(IDENT):
                {
                	/* If it's in our environment, it might need value conversion. */
                    std::string s = CCP(GET_TEXT(expr));
                	if (env.find(s) != env.end())
                    {
                        ident = new_ident("bound");
                        m_out << "auto " << ident << " = ";
                        emit_bound_var_rvalue(expr, ifaces_context, context, 
                        	*env.find(s), env, cxx_result_type);
                        m_out << ";" << std::endl;
                    	return std::make_pair("true", ident);
                    }
                    /* Otherwise we're referencing something in the dwarfhpp-generated headers. */
                    else return std::make_pair("true", 
                        ns_prefix + "::" + /*callee_namespace_name*/context.second); 
                        // FIXME: should resolve in DWARF info
                }
            case CAKE_TOKEN(KEYWORD_THIS):
            case CAKE_TOKEN(KEYWORD_THAT):
            case CAKE_TOKEN(KEYWORD_SUCCESS):
            case CAKE_TOKEN(KEYWORD_OUT):
            case CAKE_TOKEN(MEMBER_SELECT):
            case CAKE_TOKEN(INDIRECT_MEMBER_SELECT):
            case CAKE_TOKEN(ELLIPSIS): /* ellipsis is 'access associated' */
            case CAKE_TOKEN(ARRAY_SUBSCRIPT):
            case CAKE_TOKEN(COMPLEMENT):
            case CAKE_TOKEN(NOT):
            case CAKE_TOKEN(MINUS): // may be unary or binary!
            case CAKE_TOKEN(PLUS): // may be unary or binary!
            // dereference
            case CAKE_TOKEN(MULTIPLY): // may be unary (dereference) or binary
            case CAKE_TOKEN(KEYWORD_DELETE):
            // may be unary (address-of) or binary
            case CAKE_TOKEN(BITWISE_AND):
            case CAKE_TOKEN(KEYWORD_NEW):
            case CAKE_TOKEN(KEYWORD_TIE):
            case CAKE_TOKEN(KEYWORD_AS):
            case CAKE_TOKEN(KEYWORD_IN_AS):
            case CAKE_TOKEN(KEYWORD_OUT_AS):
            case CAKE_TOKEN(DIVIDE):
            case CAKE_TOKEN(MODULO):
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
            case CAKE_TOKEN(LOGICAL_AND):
            case CAKE_TOKEN(LOGICAL_OR):
            case CAKE_TOKEN(CONDITIONAL): // $cond $caseTrue $caseFalse 
            case CAKE_TOKEN(KEYWORD_FN):
            case CAKE_TOKEN(SEMICOLON):
            case CAKE_TOKEN(ANDALSO_THEN):
            case CAKE_TOKEN(ORELSE_THEN):
            	assert(false);
            default:
            	assert(false);
        }
    }
            
	void
    wrapper_file::emit_bound_var_rvalue(antlr::tree::Tree *call_expr, 
        link_derivation::iface_pair ifaces_context,
        const request::module_name_pair& sink_context,
        //const request::
        const binding& bound_var, 
        environment env,
        boost::shared_ptr<dwarf::spec::type_die> result_type)
    {
    	/* There are a few cases here:
         * -- the bound var was defined as an argument;
         * -- the bound var was defined in an infix stub on the remote side;
         * -- the bound var was defined in an infix stub on the local side;
         * -- the bound var was defined in the local stub itself;
         * -- FIXME: stuff to do with return events 
         
         * We disambiguate using our args & other context, 
         * deduce the C++ variable name/prefix that we require,
         * and insert value conversions as necessary. */
        
        if (sink_context == bound_var.second.defining_module)
        {
        	/* We don't need a value conversion in this case. */
            std::string name = cxx_name_for_binding(bound_var);
            m_out << name;
            //return std::make_pair("true", name);
        }
        else
        {
        	assert(result_type);
        	open_value_conversion(
            	ifaces_context,
            	bound_var.second.type,
                bound_var.second.defining_module.first,
                //bound_var.second.defining_module.second,
                // convert to? whatever the corresponding type in this ns is...
                // means we need access to correspondences
                result_type/*, //boost::shared_ptr<dwarf::spec::type_die>(),
                sink_context.second*/
                , module_of_die(result_type)
                );
                
                //                  ^---- if the source code explicitly instantiated a corresp,
                //                   we might have a different number here?
                // -- yes, emit a table of these before the wrapper functions
                // note that cxx types are only part of the story! all this is
                // just to keep the compiler happy.
                // -- when we emit value correspondences,
                // we should encode the From and To types into the *function name*,
                // not 
                
                /* Do I want to keep the generated C++ as simple as possible, and
                 * use templates to do what I want with it? 
                 * Or generate closer to hand-written?
                 * 
                 * Harder to debug generated code, but 
                 * less recompilation of the Cake compiler. */
                
            // to-namespace? );
            std::string name = cxx_name_for_binding(bound_var);
            m_out << name;
            
            close_value_conversion();
        }
    }

    std::pair<std::string, std::string>
    wrapper_file::emit_stub_function_call(
        	antlr::tree::Tree *call_expr,
            link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& sink_context, // sink module
            //const request::module_name_pair& source_context,
            //const dwarf::encap::Die_encap_subprogram& source_signature,
            boost::shared_ptr<dwarf::spec::type_die> cxx_result_type, //*/ const std::string& cxx_result_type_name, // may be null
            environment env)
    {
		assert(GET_TYPE(call_expr) == CAKE_TOKEN(INVOKE_WITH_ARGS));
		INIT;
        // in new grammar, no longer a def. member name
        BIND3(call_expr, argsMultiValue, MULTIVALUE);
        // FIXME: function_name might actually be an expression! evaluate this first!
        BIND3(call_expr, functionNameTree/*, DEFINITE_MEMBER_NAME*/, IDENT);
        // auto mn = read_definite_member_name(memberNameExpr);
        
        auto return_type_name = //cxx_result_type_name;
        	(cxx_result_type 
            ? get_type_name(cxx_result_type) 
            : "::cake::unspecified_wordsize_type");

        std::string function_name = CCP(GET_TEXT(functionNameTree));
        std::vector<std::string> mn(1, function_name);
        auto callee(
        	sink_context.first->all_compile_units().visible_resolve(
                	mn.begin(), mn.end()
                    ));
        if (!callee || callee->get_tag() != DW_TAG_subprogram)
        {
        	RAISE(functionNameTree, "does not name a visible function");
        }

		dwarf::encap::Die_encap_subprogram& callee_subprogram
         = dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*callee);
        auto callee_return_type
            = treat_subprogram_as_untyped(callee_subprogram) ?
               0 : callee_subprogram.get_type();
               
        /* evaluate the arguments and bind temporary names to them */
        auto success_ident = new_ident("success");
        m_out << "bool " << success_ident << " = true; " << std::endl;
        std::string value_ident = new_ident("value");
        if (!callee_subprogram.get_type())
        {
        	m_out << "::cake::unspecified_wordsize_type"
        	 << ' ' << value_ident << "; // unused" << std::endl;
    	}
        else
        {
        	m_out << get_type_name(*callee_subprogram.get_type())
        	 << ' ' << value_ident << ";" << std::endl;
        }
        //m_out << "do" << std::endl
        //	<< "{";
        m_out << "// begin argument expression eval" << std::endl;
        //m_out.inc_level();
        std::vector<std::pair<std::string, std::string > > arg_names;
        std::pair<std::string, std::string> names;
        auto i_arg = callee_subprogram.formal_parameter_children_begin();
        {
            INIT;
            FOR_ALL_CHILDREN(argsMultiValue)
            {
            	assert(i_arg != callee_subprogram.formal_parameter_children_end()
                 && (*i_arg)->get_type());
                names = emit_stub_expression_as_statement_list(
                  n, ifaces_context, sink_context, 
                  /* Result type is that of the *argument* that we're going to pass
                   * this subexpression's result to. */
                  (treat_subprogram_as_untyped(callee_subprogram) 
                  ? boost::shared_ptr<dwarf::spec::type_die>()
                  : *(*i_arg)->get_type()),
                     //cxx_result_type/*_name*/, 
                  env);
                arg_names.push_back(names);
                m_out << success_ident << " &= " << names.first << ";" << std::endl;
                m_out << "if (" << success_ident << ") // okay to proceed with next arg?" 
                	<< std::endl;
                m_out << "{" << std::endl;
                m_out.inc_level();
                i_arg++;
            }
        }
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
		if (callee_subprogram.get_type())
        {
        	m_out << "auto " << raw_result_ident << " = ";
        }

        // emit the function name, as a symbol reference
        m_out << "cake::" << m_d.namespace_name() << "::";
        emit_symbol_reference_expr_from_dwarf_ident(
        	function_name,
            sink_context,
            sink_context.first->all_compile_units());        
        m_out << '(';
        
        for (auto i_name_pair = arg_names.begin(); i_name_pair != arg_names.end(); i_name_pair++)
        {
            if (i_name_pair != arg_names.begin()) m_out << ", ";
        	m_out << i_name_pair->second;
        }
        m_out << ')';
        
        m_out << ";" << std::endl;
        
        // convert the function raw result to a success and value pair,
        // in a style-dependent way
        if (callee_subprogram.get_type())
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

		// we opened argcount  extra lexical blocks in the argument eval
        for (unsigned i = 0; i < GET_CHILD_COUNT(argsMultiValue); i++)
        {
            m_out.dec_level();
        	m_out << "} " /*"else " << success_ident << " = false;"*/ << std::endl;
        }
        
        // set failure result
        m_out << "// test whether we failed " << std::endl;
        m_out << "if (!" << success_ident << ")" << std::endl;
        m_out << "{" << std::endl;
        m_out.inc_level();
        if (callee_subprogram.get_type()) // only get a value if it returns something
        {
            m_out << value_ident << " = ::cake::failure<" 
        	    <<  get_type_name(*callee_subprogram.get_type()) //return_type_name
                << ">()();" << std::endl;
    	}
        m_out.dec_level();
        m_out << "}" << std::endl;
        
        //m_out.dec_level();
        //m_out << "} while(0);" << std::endl;
        
        //m_out << "if (!" << success_ident << ")" << std::endl
        //	<< "{";
        //m_out.inc_level();
        

		return std::make_pair(success_ident, value_ident);
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
    
    void wrapper_file::emit_constant_expr(
        	antlr::tree::Tree *constant_expr,
    	    const request::module_name_pair& request_context)
    {
    	//m_out << CCP(TO_STRING_TREE(constant_expr));
        assert(GET_TYPE(constant_expr) == CAKE_TOKEN(KEYWORD_CONST));
        INIT;
        BIND2(constant_expr, child);
        switch(GET_TYPE(child))
        {
        	case CAKE_TOKEN(STRING_LIT):
            	m_out << CCP(GET_TEXT(child));
                break;
            case CAKE_TOKEN(KEYWORD_NULL):
            	m_out << "0";
                break;
            case CAKE_TOKEN(SET_CONST):
            	RAISE(child, "set constants not yet supported"); // FIXME            
            //case CAKE_TOKEN(CONST_ARITH):
            case CAKE_TOKEN(CONST_ARITH):
            {
            	long double result = eval_const_expr(constant_expr, request_context);
                m_out << result;
                break;
            }            
            default: 
            	RAISE(child, "expected a constant expression");
        }
    }
    
    // FIXME: something better than this naive long double implementation please
    long double wrapper_file::eval_const_expr(
    	antlr::tree::Tree *expr,
        const request::module_name_pair& request_context)
    {
    	switch (GET_TYPE(expr))
        {
        	case CAKE_TOKEN(INT):
            	return atoi(CCP(GET_TEXT(expr)));
            case CAKE_TOKEN(SHIFT_LEFT): 
            	return 
                	eval_const_expr(GET_CHILD(expr, 0), request_context) 
                    	* powl(2.0, eval_const_expr(GET_CHILD(expr, 0), request_context));
            case CAKE_TOKEN(SHIFT_RIGHT):
            	return 
                	eval_const_expr(GET_CHILD(expr, 0), request_context) 
                    	* powl(2.0, eval_const_expr(GET_CHILD(expr, 0), request_context));

            case CAKE_TOKEN(KEYWORD_CONST):
            case CAKE_TOKEN(CONST_ARITH):
            	return eval_const_expr(GET_CHILD(expr, 0), request_context);
            default: RAISE(expr, "unsupported constant expression syntax");
        }
    }
    
    void wrapper_file::emit_symbol_reference_expr_from_dwarf_ident(
            antlr::tree::Tree *dmn, 
    	    const request::module_name_pair& request_context, 
            dwarf::abstract::Die_abstract_has_named_children<dwarf::encap::die>& dwarf_context)
    {
    	definite_member_name mn(dmn);
        
        // don't bother emitting these ns names because we're inside them already!
//         << "cake::" 
//         	<< m_d.namespace_name() << "::" // namespace enclosing this module's wrapper definitions
            
    	m_out << request_context.second << "::" // namespace specifically for this module
            << definite_member_name(
            		*(
                    	((*(dwarf_context.resolve(
                    		mn.begin(), mn.end())
                          )).ident_path_from_cu())
                     )
                 );
    }
    
    void wrapper_file::emit_symbol_reference_expr_from_dwarf_ident(
            const std::string& ident, 
    	    const request::module_name_pair& request_context, 
            dwarf::abstract::Die_abstract_has_named_children<dwarf::encap::die>& dwarf_context)
    {
    	std::vector<std::string> mn(1, ident);
    	m_out << request_context.second << "::" // namespace specifically for this module
        	<< ident;
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
