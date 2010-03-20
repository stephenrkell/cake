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
	}        	
    
    void wrapper_file::emit_function_header(
        antlr::tree::Tree *event_pattern,
        const std::string& function_name_to_use,
        dwarf::encap::Die_encap_subprogram& subprogram,
        bool emit_types /*= true*/)
    {
    	boost::optional<dwarf::encap::Die_encap_is_type&> ret_type
        	= subprogram.get_type();
        dwarf::encap::formal_parameters_iterator args_begin 
        	= subprogram.formal_parameters_begin();
        dwarf::encap::formal_parameters_iterator args_end
        	= subprogram.formal_parameters_end();
        bool ignore_dwarf_args = treat_subprogram_as_untyped(subprogram);
        
        if (emit_types)
        {
        	if (subprogram_returns_void(subprogram)) m_out << "void";
            else if (treat_subprogram_as_untyped(subprogram)) m_out << 
            	" ::cake::unspecified_wordsize_type";     
            else m_out << compiler.name_for(*ret_type);
             
            m_out << ' ';
        }
        m_out << function_name_to_use << '(';
       
        dwarf::encap::formal_parameters_iterator i_arg = args_begin;
        
        // actually, iterate over the pattern
        assert(GET_TYPE(event_pattern) == CAKE_TOKEN(EVENT_PATTERN));
        INIT;
        BIND3(event_pattern, eventContext, EVENT_CONTEXT);
        BIND2(event_pattern, memberNameExpr); // we ignore this
		int argnum = 0;
        int pattern_args;
        switch(GET_CHILD_COUNT(event_pattern))
        {
            case 2: // okay, we have no argument list (simple renames only)
            	if (ignore_dwarf_args)
                {  		
                	// problem: no source of argument info! Assume none
                	std::cerr << "Warning: wrapping function " << function_name_to_use
                	    << " declared with unspecified parameters"
                        << " and no event pattern arguments. "
                        << "Assuming empty argument list." << std::endl; }
                break;
            default: // must be >=3
                pattern_args = GET_CHILD_COUNT(event_pattern) - 2;
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
                                	    dynamic_cast<dwarf::encap::Die_encap_is_type&>(*(*i_arg)->get_type())));
                                    m_out  << ' ' << mn.at(0);
	                            } break;
                            case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    case CAKE_TOKEN(METAVAR):
                            case CAKE_TOKEN(KEYWORD_CONST):
                            	// output the argument type and a dummy name
                                if (emit_types) m_out << (ignore_dwarf_args ? "::cake::unspecified_wordsize_type" : compiler.name_for(
                                	dynamic_cast<dwarf::encap::Die_encap_is_type&>(*(*i_arg)->get_type())));
                                m_out << ' ' << "arg" << argnum;
                                break;
                            default: RAISE_INTERNAL(valuePattern, "not a value pattern");
                        }
                    }
                next:
                	// work out whether we need a comma
                    if (!ignore_dwarf_args) i_arg++;
                    argnum++;
                    if (ignore_dwarf_args ? (argnum != pattern_args) : (i_arg != args_end)) m_out << ", ";
                }
                
                if (!ignore_dwarf_args && i_arg != args_end)
                {
                	std::ostringstream msg;
                    msg << "argument pattern has too few arguments for subprogram: "
                    	<< subprogram;
                	RAISE(event_pattern, msg.str());
                }
                break;
        }
                        
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
	    boost::optional<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&> opt_subp = 
            corresps.at(0)->second.source->all_compile_units() // all corresps share same source mdl
                .named_child(wrapped_symname);
		if (!opt_subp) 
        { std::cerr << "Bailing from wrapper generation -- no subprogram!" << std::endl; 
          return; }
        
        // convenience alias of subprogram
        dwarf::encap::Die_encap_subprogram& subprogram = 
        	dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*opt_subp);
        
        // is the DWARF

		// output prototype for __real_
        m_out << "extern ";
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__real_" + *(subprogram.get_name()),
                subprogram);
        m_out << " __attribute__((weak));" << std::endl;
        // output prototype for __wrap_
        m_out << "extern \"C\" { " << std::endl;
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__wrap_" + *(subprogram.get_name()),
                subprogram);
        m_out << ';' << std::endl;
        m_out << "} // end extern \"C\"" << std::endl;
        // output wrapper
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__wrap_" + *(subprogram.get_name()),
                subprogram);
        m_out << " {" << std::endl;

        // 3. emit wrapper definition
        emit_wrapper_body(wrapped_symname, subprogram, corresps, request_context);
        
        // 4. emit_wrapper_body leaves us a dangling "else", so clear that up
        m_out << "return ";
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__real_" + *(subprogram.get_name()),
                subprogram, false);
        m_out << ';' << std::endl; // end of return statement
        m_out << '}' << std::endl; // end of block
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
            emit_pattern_condition(pattern, *request_context.find((*i_pcorresp)->second.source),
            	&env); // add any bound names to the environment
            m_out << ") {" << std::endl;
            emit_sink_action(action, 
            	wrapper_sig,
            	*request_context.find((*i_pcorresp)->second.sink),
                *request_context.find((*i_pcorresp)->second.source),
                env);
            m_out << "}" << std::endl;
            m_out << " else ";
        }
    }
    
    void wrapper_file::emit_pattern_condition(
            antlr::tree::Tree *pattern,
            const request::module_name_pair& request_context,
            environment *out_env)
    {
        //m_out << "true"; //CCP(GET_TEXT(corresp_pair.second.source_pattern));
        // for each position in the pattern, emit a test
        bool emitted = false;
        INIT;
        ALIAS3(pattern, eventPattern, EVENT_PATTERN);
        {
        	INIT;
            BIND2(pattern, eventContext);
            BIND2(pattern, memberNameExpr);
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
            FOR_REMAINING_CHILDREN(eventPattern)
            {
            	dwarf::encap::Die_encap_is_type *p_arg_type = 0;
	            dwarf::encap::Die_encap_is_program_element *p_arg_origin;
            	
            	if (i_caller_arg == caller_subprogram.formal_parameters_end())
                {
                	if (caller_subprogram.unspecified_parameters_begin() !=
                    	caller_subprogram.unspecified_parameters_end())
                    {
                    	p_arg_origin = &**caller_subprogram.unspecified_parameters_begin();
                    }
                    else RAISE(eventPattern, "too many arguments for function");
                }
                else
                {
                	p_arg_type = &*(*i_caller_arg)->get_type();
                    p_arg_origin = &**i_caller_arg;
                }
        	    ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
                {
                	INIT;
                    BIND2(annotatedValuePattern, valuePattern);
                    switch(GET_TYPE(valuePattern))
				    {
                	    case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
                    	    // could match anything, so bind name and continue
                            definite_member_name mn = read_definite_member_name(valuePattern);
                            if (mn.size() != 1) RAISE(valuePattern, "may not be compound");
                            out_env->insert(std::make_pair(mn.at(0), bound_var_info(
                            	p_arg_type ? *p_arg_type : boost::optional<dwarf::encap::Die_encap_is_type&>(),
                                request_context,
                                *p_arg_origin)));
                        } break;
                        case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    // means don't care, so continue
                        break;
                        case CAKE_TOKEN(METAVAR):
                    	    // FIXME: bind something here
                        break;
                        case CAKE_TOKEN(KEYWORD_CONST):
                        	if (emitted) m_out << " && ";
                            m_out << "cake::equal<";
                            if (p_arg_type) emit_type_name(*p_arg_type, ns_prefix + request_context.second);
                            else m_out << " ::cake::unspecified_wordsize_type";
                            m_out << ", "
 << (	(GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(STRING_LIT)) ? " ::cake::style_traits<0>::STRING_LIT" :
        (GET_TYPE(GET_CHILD(valuePattern, 0)) == CAKE_TOKEN(CONST_ARITH)) ? " ::cake::style_traits<0>::CONST_ARITH" :
        " ::cake::unspecified_wordsize_type" );
                            m_out << ">()(";
                            m_out << "arg" << argnum << ", ";
                            emit_constant_expr(valuePattern, request_context);
                            m_out << ")";
                            emitted = true;
                        break;
                        default: assert(false); 
                        break;
                    }
	            } // end ALIAS3
            	++argnum;
                if (i_caller_arg != caller_subprogram.formal_parameters_end()) i_caller_arg++;
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
        m_out << "return ";
        
        // this is a crossover point, so insert conversions according to context
        assert(GET_TYPE(action) == CAKE_TOKEN(EVENT_SINK_AS_PATTERN));
        INIT;
        BIND3(action, eventPattern, EVENT_PATTERN);
        
        emit_event_pattern_as_function_call(eventPattern, sink_context, 
        	source_context, wrapper_sig, env);
        m_out << ';' << std::endl;
    }
    
    void wrapper_file::emit_type_name(
            const dwarf::encap::Die_encap_is_type& t,
            const std::string& namespace_prefix)
    {
		m_out << ((t.get_tag() == DW_TAG_base_type) ?
        	compiler.local_name_for(t)
            : (namespace_prefix + "::" + compiler.fq_name_for(t)));
    }
    
    void wrapper_file::open_value_conversion(
    	boost::optional<dwarf::encap::Die_encap_is_type&> from_type,
        const std::string& from_namespace,
        boost::optional<dwarf::encap::Die_encap_is_type&> to_type,
        const std::string& to_namespace)
    {
        m_out << "cake::value_convert<";
        if (from_type) emit_type_name(*from_type, from_namespace);
        else m_out << " ::cake::unspecified_wordsize_type";
	    m_out << ", ";
        if (to_type) emit_type_name(*to_type, to_namespace); 
        else m_out << " ::cake::unspecified_wordsize_type";
        m_out << ">()(";
    }
    void wrapper_file::close_value_conversion()
    {
		m_out << ")";
	}
    
    void wrapper_file::emit_event_pattern_as_function_call(
        	antlr::tree::Tree *pattern,
            const request::module_name_pair& sink_context, // sink module
            const request::module_name_pair& source_context,
            const dwarf::encap::Die_encap_subprogram& source_signature,
            environment env)
    {
		assert(GET_TYPE(pattern) == CAKE_TOKEN(EVENT_PATTERN));
		INIT;
        BIND2(pattern, eventContext);
        BIND3(pattern, memberNameExpr, DEFINITE_MEMBER_NAME);
        auto mn = read_definite_member_name(memberNameExpr);
        auto callee(
        	sink_context.first->all_compile_units().visible_resolve(
                	mn.begin(), mn.end()
                    ));
        if (!callee) RAISE(memberNameExpr, "does not name a visible function");
        if ((*callee).get_tag() != DW_TAG_subprogram) RAISE(memberNameExpr, "does not name a function");
		dwarf::encap::Die_encap_subprogram& callee_subprogram
         = dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*callee);
        boost::optional<dwarf::encap::Die_encap_is_type&> callee_return_type
            = treat_subprogram_as_untyped(callee_subprogram) ? 0 : callee_subprogram.get_type();
        boost::optional<dwarf::encap::Die_encap_is_type&> convert_to 
        	= treat_subprogram_as_untyped(source_signature) ? 0 : source_signature.get_type();
        const std::string& caller_namespace_name = source_context.second;
        const std::string& callee_namespace_name = sink_context.second;    
        const std::string caller_prefix 
         = ns_prefix + "::" + caller_namespace_name;
        const std::string callee_prefix
         = ns_prefix + "::" + callee_namespace_name;
                    
        if (!subprogram_returns_void(callee_subprogram)) open_value_conversion(
        	callee_return_type, 
        	callee_prefix,
            convert_to, 
            caller_prefix);

        // emit the function name, as a symbol reference
        m_out << "cake::" << m_d.namespace_name() << "::";
        emit_symbol_reference_expr_from_dwarf_ident(
        	memberNameExpr,
            sink_context,
            sink_context.first->all_compile_units());        
        m_out << '(';
        
        dwarf::encap::formal_parameters_iterator i_callee_arg 
         = dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*callee).formal_parameters_begin();
        // emit the argument
        FOR_REMAINING_CHILDREN(pattern)
        {
        	ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
            {
            	INIT;
                BIND2(n, valuePattern);
                switch (GET_TYPE(valuePattern))
                {
                    case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
                        {
                        	// FIXME: better treatment of bound identifiers here
                            definite_member_name mn = 
                                read_definite_member_name(valuePattern);
                            if (mn.size() > 1) RAISE(valuePattern, "may not be compound");
                            // mn.at(0) is one of our argument names
                            open_value_conversion(
                            	// "from" is the calling argument type, "to" is the callee's
                                treat_subprogram_as_untyped(source_signature) ? 0 : 
                                	env.find(mn.at(0))->second.type,
                                ns_prefix + "::" + env.find(mn.at(0))->second.module.second, 
                                treat_subprogram_as_untyped(callee_subprogram) ? 0 : 
                                	(*i_callee_arg)->get_type(),
                                callee_prefix);
                            m_out << mn.at(0);
                            close_value_conversion();
	                    } break;
                    case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	RAISE(valuePattern, "may not be indefinite");
                    case CAKE_TOKEN(METAVAR):
                    	RAISE(valuePattern, "metavars not supported");
                    case CAKE_TOKEN(KEYWORD_CONST):
                        // output the argument type and a dummy name
                        emit_constant_expr(valuePattern, sink_context);
                        break;
                    default: RAISE_INTERNAL(valuePattern, "not a value pattern");
                }
            }
            if (i+1 < childcount) m_out << ", ";
            ++i_callee_arg;
        }
        m_out << ')';
        if (!subprogram_returns_void(callee_subprogram)) close_value_conversion();
    }
    
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
                          )).path_from_cu())
                     )
                 );
    }
}
