#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"
#include <sstream>

namespace cake
{
    void wrapper_file::emit_function_header(
        antlr::tree::Tree *event_pattern,
        const std::string& function_name_to_use,
        dwarf::encap::Die_encap_subprogram& subprogram,
        bool emit_types /*= true*/)
    {
    	boost::optional<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&> ret_type
        	= subprogram.get_type();
        dwarf::encap::die::formal_parameters_iterator args_begin 
        	= subprogram.formal_parameters_begin();
        dwarf::encap::die::formal_parameters_iterator args_end
        	= subprogram.formal_parameters_end();
        
        if (emit_types)
        {
            m_out << (ret_type ? compiler.name_for(
        					    dynamic_cast<dwarf::encap::Die_encap_is_type&>(*ret_type)) 
                           : "void" ) << ' ';
        }
        m_out << function_name_to_use << '(';
       
        dwarf::encap::die::formal_parameters_iterator i_arg = args_begin;
        
        // actually, iterate over the pattern
        assert(GET_TYPE(event_pattern) == CAKE_TOKEN(EVENT_PATTERN));
        INIT;
        BIND3(event_pattern, eventContext, EVENT_CONTEXT);
        BIND2(event_pattern, memberNameExpr); // we ignore this
        bool ignore_dwarf_args = (args_begin == args_end
        				 && subprogram.unspecified_parameters_begin() !=
                    	subprogram.unspecified_parameters_end());
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
                            	    // output the variable type, or "int"
                                    if (emit_types) m_out << (ignore_dwarf_args ? "int" : compiler.name_for(
                                	    dynamic_cast<dwarf::encap::Die_encap_is_type&>(*(*i_arg)->get_type())));
                                    m_out  << ' ' << mn.at(0);
	                            } break;
                            case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    case CAKE_TOKEN(METAVAR):
                            case CAKE_TOKEN(KEYWORD_CONST):
                            	// output the argument type and a dummy name
                                if (emit_types) m_out << (ignore_dwarf_args ? "int" : compiler.name_for(
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
        for (dwarf::encap::die::named_children_iterator i = 
        	corresps.at(0)->second.source->all_compile_units().named_children_begin();
            i != corresps.at(0)->second.source->all_compile_units().named_children_end();
            i++)
        {
        	std::cerr << "Named child in all_compile_units: " << *((*i)->get_name()) << std::endl;
        }

		// get the subprogram description of the wrapped
        // function in the source module
	    boost::optional<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&> opt_subp = 
            corresps.at(0)->second.source->all_compile_units()
                .named_child(wrapped_symname);
		if (!opt_subp) 
        { std::cerr << "Bailing from wrapper generation -- no subprogram!" << std::endl; 
          return; }
        
        // convenience alias of subprogram
        dwarf::encap::Die_encap_subprogram& subprogram = 
        	dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*opt_subp);

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
        emit_wrapper_body(wrapped_symname, corresps, request_context);
        
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
	        link_derivation::ev_corresp_pair_ptr_list& corresps,
            const request::module_inverse_tbl_t& request_context)
    {
    	// for each corresp, match its pattern
        for (link_derivation::ev_corresp_pair_ptr_list::iterator i_pcorresp = corresps.begin();
        		i_pcorresp != corresps.end(); i_pcorresp++)
        {
            antlr::tree::Tree *pattern = (*i_pcorresp)->second.source_pattern;
            antlr::tree::Tree *action = (*i_pcorresp)->second.sink_expr;
        	m_out 	<< "if (";
            emit_pattern_condition(pattern, *request_context.find((*i_pcorresp)->second.source));
            m_out << ") {" << std::endl;
            emit_sink_action(action, 
            	*request_context.find((*i_pcorresp)->second.sink));
            m_out << "}" << std::endl;
            m_out << " else ";
        }
    }
    
    void wrapper_file::emit_pattern_condition(
            antlr::tree::Tree *pattern,
            const request::module_name_pair& request_context)
    {
        m_out << "true"; //CCP(GET_TEXT(corresp_pair.second.source_pattern));
    }

    void wrapper_file::emit_sink_action(
        	antlr::tree::Tree *action,
            const request::module_name_pair& context)
    {
        m_out << "return ";
        assert(GET_TYPE(action) == CAKE_TOKEN(EVENT_SINK_AS_PATTERN));
        INIT;
        BIND3(action, eventPattern, EVENT_PATTERN);
        
        emit_event_pattern_as_function_call(eventPattern, context);
        m_out << ';' << std::endl;
    }
    
    void wrapper_file::emit_event_pattern_as_function_call(
        	antlr::tree::Tree *pattern,
            const request::module_name_pair& context)
    {
		assert(GET_TYPE(pattern) == CAKE_TOKEN(EVENT_PATTERN));
		INIT;
        BIND2(pattern, eventContext);
        BIND3(pattern, memberNameExpr, DEFINITE_MEMBER_NAME);
        
        // emit the function name, as a symbol reference
        emit_symbol_reference_expr_from_dwarf_ident(
        	memberNameExpr,
            context,
            context.first->all_compile_units());        
        m_out << '(';
        
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
                            m_out << ' ' << mn.at(0);
	                    } break;
                    case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	RAISE(valuePattern, "may not be indefinite");
                    case CAKE_TOKEN(METAVAR):
                    	RAISE(valuePattern, "metavars not supported");
                    case CAKE_TOKEN(KEYWORD_CONST):
                        // output the argument type and a dummy name
                        emit_constant_expr(valuePattern, context);
                        break;
                    default: RAISE_INTERNAL(valuePattern, "not a value pattern");
                }
            }
            if (i+1 < childcount) m_out << ", ";
        }
        m_out << ')';
    }
    
    void wrapper_file::emit_constant_expr(
        	antlr::tree::Tree *constant_expr,
    	    const request::module_name_pair& request_context)
    {
    	m_out << CCP(TO_STRING_TREE(constant_expr));
    }
    
    void wrapper_file::emit_symbol_reference_expr_from_dwarf_ident(
            antlr::tree::Tree *dmn, 
    	    const request::module_name_pair& request_context, 
            dwarf::abstract::Die_abstract_has_named_children<dwarf::encap::die>& dwarf_context)
    {
    	definite_member_name mn(dmn);
        
        // FIXME: don't bother emitting these ns names because we're inside them already!
    	m_out << "cake::" 
        	<< m_d.namespace_name() << "::" // namespace enclosing this module's wrapper definitions
            << request_context.second << "::" // namespace specifically for this module
            << definite_member_name(
            		*(
                    	((*(dwarf_context.resolve(
                    		mn.begin(), mn.end())
                          )).path_from_cu())
                     )
                 );
    }
}
