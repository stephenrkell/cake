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
        return false;
	}        	
    
    void wrapper_file::emit_function_header(
        antlr::tree::Tree *event_pattern,
        const std::string& function_name_to_use,
        dwarf::encap::Die_encap_subprogram& subprogram,
        const std::string& arg_name_prefix,
        const request::module_name_pair& caller_context,
        bool emit_types /*= true*/)
    {
    	auto ret_type = subprogram.get_type();
        auto args_begin = subprogram.formal_parameter_children_begin();
        auto args_end = subprogram.formal_parameter_children_end();
        bool ignore_dwarf_args = treat_subprogram_as_untyped(subprogram);
        
        if (emit_types)
        {
        	if (subprogram_returns_void(subprogram)) m_out << "void";
            else if (treat_subprogram_as_untyped(subprogram)) m_out << 
            	" ::cake::unspecified_wordsize_type";     
            else m_out << ns_prefix << "::" << caller_context.second << "::"
            	<< compiler.fq_name_for(*ret_type);
             
            m_out << ' ';
        }
        m_out << function_name_to_use << '(';
       
        auto i_arg = args_begin;
        
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
            	if (ignore_dwarf_args)
                {  		
                	// problem: no source of argument info! Assume none
                	std::cerr << "Warning: wrapping function " << function_name_to_use
                	    << " declared with unspecified parameters"
                        << " and no event pattern arguments. "
                        << "Assuming empty argument list." << std::endl; }
                break;
            default: // must be >=5
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

		// output prototype for __real_
        m_out << "extern \"C\" { " << std::endl;
        m_out << "extern ";
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__real_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name);
        m_out << " __attribute__((weak));" << std::endl;
        // output prototype for __wrap_
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__wrap_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name);
        m_out << ';' << std::endl;
        m_out << "} // end extern \"C\"" << std::endl;
        // output wrapper
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__wrap_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name);
        m_out << std::endl;
        m_out << " {";
        m_out.inc_level();
        m_out << std::endl;

        // 3. emit wrapper definition
        emit_wrapper_body(wrapped_symname, subprogram, corresps, request_context);
        
        // 4. emit_wrapper_body leaves us a dangling "else", so clear that up
        m_out << "return ";
        emit_function_header(
                corresps.at(0)->second.source_pattern,
        		"__real_" + *(subprogram.get_name()),
                subprogram,
                "__cake_arg", caller_module_and_name, false);
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
            emit_pattern_condition(pattern, source_module,
            	&env); // add any bound names to the environment
            m_out << ")" << std::endl;
            m_out << "{";
            m_out.inc_level();
            m_out << std::endl;
            emit_sink_action(action, 
            	wrapper_sig,
            	sink_module,
                source_module,
                env);
            m_out.dec_level();
            m_out << "}" << std::endl;
            m_out << "else ";
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
                        case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                        	bound_name = "dummy";
                        	break;
                        default: RAISE(valuePattern, "unexpected token");
                    }
                    /* Now insert the binding. Which constructor we use depends on
                     * whether the argument in the caller is described by a formal_parameter
                     * or unspecified_parameters DIE. */
                    if (origin_as_fp)
                    {
                        out_env->insert(std::make_pair(bound_name, bound_var_info(
                            *this, //"arg", 
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
                    
                    /* Now actually emit a condition, if necessary. */ 
                    switch(GET_TYPE(valuePattern))
				    {
                	    case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
                    	    // no conditions -- match anything (and bind)
                        } break;
                        case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    // no conditions -- match anything (and don't bind)
                        break;
                        case CAKE_TOKEN(KEYWORD_CONST):
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
        // this is a crossover point, so insert conversions according to context
        assert(GET_TYPE(action) == CAKE_TOKEN(EVENT_SINK_AS_STUB));
        INIT;
        //BIND3(action, eventPattern, EVENT_PATTERN);
        BIND2(action, stub);
        
        //emit_event_pattern_as_function_call(eventPattern, sink_context, 
        //	source_context, wrapper_sig, env);
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
    
    void
    wrapper_file::emit_value_conversion(module_ptr source,
            boost::shared_ptr<dwarf::spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
            antlr::tree::Tree *sink_infix_stub,
            antlr::tree::Tree *refinement,
			bool source_is_on_left,
			antlr::tree::Tree *corresp)
    {
		// can't handle infix stubs, yet
		assert((!source_infix_stub || GET_CHILD_COUNT(source_infix_stub) == 0)
		&& (!sink_infix_stub || GET_CHILD_COUNT(sink_infix_stub) == 0));
	
		// we *only* emit conversions between concrete types
		auto source_concrete_type = source_data_type->get_concrete_type();
		auto sink_concrete_type = sink_data_type->get_concrete_type();
	
        auto from_typename = get_type_name(source_concrete_type);
        auto to_typename = get_type_name(sink_concrete_type);

		// skip incomplete (void) typedefs and other incompletes
		if (!compiler.cxx_is_complete_type(source_concrete_type)
		|| !compiler.cxx_is_complete_type(sink_concrete_type))
		{
			std::cerr << "Warning: skipping value conversion from " << from_typename
				<< " to " << to_typename
				<< " because one or other is an incomplete type." << std::endl;
			return;
		}
		// skip pointers and references
		if (source_concrete_type->get_tag() == DW_TAG_pointer_type
		|| sink_concrete_type->get_tag() == DW_TAG_pointer_type
		|| source_concrete_type->get_tag() == DW_TAG_reference_type
		|| sink_concrete_type->get_tag() == DW_TAG_reference_type)
		{
			std::cerr << "Warning: skipping value conversion from " << from_typename
				<< " to " << to_typename
				<< " because one or other is an pointer or reference type." << std::endl;
			return;
		}
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
        		return;
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

        m_out << "template <>\n"
            << "struct value_convert<"
            << from_typename // From
            << ", "
            << to_typename // To
            << ", "
            << "0" // RuleTag
            << ">" << std::endl
            << "{" << std::endl;
        m_out.inc_level();

        m_out << to_typename << " operator()(const " << from_typename << "& __cake_from, " 
			<< to_typename << "*__cake_p_to = 0) const"
            << std::endl << "{" << std::endl;
        m_out.inc_level();

		// here goes the value conversion logic
		if (!emit_as_reinterpret)
		{
			// -- dispatch to a function based on the DWARF tags of the two types
	#define TAG_PAIR(t1, t2) ((t1)<<((sizeof (Dwarf_Half))*8) | (t2))
			switch(TAG_PAIR(source_data_type->get_tag(), sink_data_type->get_tag()))
			{
				case TAG_PAIR(DW_TAG_structure_type, DW_TAG_structure_type):
					emit_structural_conversion_body(source_data_type, sink_data_type,
						refinement, source_is_on_left);
				break;
				default:
					std::cerr << "Warning: didn't know how to generate conversion between "
						<< *source_data_type << " and " << *sink_data_type << std::endl;
				break;
			}
	#undef TAG_PAIR
		}
		else
		{
			emit_reinterpret_conversion_body(source_data_type, sink_data_type);
		}

        m_out.dec_level();
        m_out << "}" << std::endl;

        m_out.dec_level();
        m_out << "};" << std::endl;
    }

	struct member_has_name
	 : public std::unary_function<boost::shared_ptr<dwarf::spec::member_die> , bool>
	{
		std::string m_name;
		member_has_name(const std::string& name) : m_name(name) {}
		bool operator()(boost::shared_ptr<dwarf::spec::member_die> p_member) const
		{
			return p_member->get_name() && (*p_member->get_name() == m_name);
		}
	};
	
	void
	wrapper_file::emit_structural_conversion_body(
		boost::shared_ptr<dwarf::spec::type_die> source_type,
		boost::shared_ptr<dwarf::spec::type_die> target_type,
		antlr::tree::Tree *refinement, 
		bool source_is_on_left)
	{
		/* Find explicitly assigned-to fields */
		std::map<definite_member_name, member_mapping_rule> field_corresps;
		if (refinement)
		{
			INIT;
			FOR_ALL_CHILDREN(refinement)
			{
				ALIAS2(n, refinementRule);
				switch(GET_TYPE(refinementRule))
				{
					case (CAKE_TOKEN(BI_DOUBLE_ARROW)):
					{
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						field_corresps.insert(std::make_pair(
							read_definite_member_name(
								source_is_on_left ? rightValuePattern : leftValuePattern),
								(member_mapping_rule)
								{
									/* definite_member_name target; */ 
										read_definite_member_name(source_is_on_left ? rightValuePattern : leftValuePattern),
									/* antlr::tree::Tree *stub; */
										source_is_on_left ? leftValuePattern : rightValuePattern,
									/* module_ptr pre_context; */
										source_is_on_left ? module_of_die(source_type) : module_of_die(target_type),
									/* antlr::tree::Tree *pre_stub; */
										source_is_on_left ? leftInfixStub : rightInfixStub,
									/* module_ptr post_context; */
										source_is_on_left ? module_of_die(target_type) : module_of_die(source_type),
									/* antlr::tree::Tree *post_stub; */
										source_is_on_left ? rightInfixStub : leftInfixStub
								}));
					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW)):
					{
						assert(source_is_on_left);
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						field_corresps.insert(std::make_pair(
							read_definite_member_name(rightValuePattern),
							(member_mapping_rule)
							{
								/* definite_member_name target; */ 
									read_definite_member_name(rightValuePattern),
								/* antlr::tree::Tree *stub; */
									leftStubExpr,
								/* module_ptr pre_context; */
									module_of_die(source_type),
								/* antlr::tree::Tree *pre_stub; */
									leftInfixStub,
								/* module_ptr post_context; */
									module_of_die(target_type),
								/* antlr::tree::Tree *post_stub; */
									rightInfixStub
							}));

					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW)):
					{
						assert(!source_is_on_left);
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);
						field_corresps.insert(std::make_pair(
							read_definite_member_name(leftValuePattern),
							(member_mapping_rule)
							{
								/* definite_member_name target; */ 
									read_definite_member_name(leftValuePattern),
								/* antlr::tree::Tree *stub; */
									rightStubExpr,
								/* module_ptr pre_context; */
									module_of_die(target_type),
								/* antlr::tree::Tree *pre_stub; */
									rightInfixStub,
								/* module_ptr post_context; */
									module_of_die(source_type),
								/* antlr::tree::Tree *post_stub; */
									leftInfixStub
							}));

					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW_Q)):
					{
						assert(source_is_on_left);
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						
						assert(false);
					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW_Q)):
					{
						assert(!source_is_on_left);
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);

						assert(false);
					}
					break;
					default: RAISE(refinementRule, "expected a correspondence operator");
					add_this_one:

					break;
				}
			}
		}
		
		/* Which of these are toplevel? */
		std::map<std::string, member_mapping_rule*> toplevel_mappings;
		for (auto i_mapping = field_corresps.begin(); i_mapping != field_corresps.end();
			i_mapping++)
		{
			toplevel_mappings.insert(
				std::make_pair(
					i_mapping->first.at(0), &i_mapping->second));
		}
		
		/* Name-match toplevel subtrees not mentioned so far. */
		auto source_module = module_of_die(source_type);
		auto target_module = module_of_die(target_type);
		auto modules = link_derivation::sorted(std::make_pair(source_module, target_module));
		std::map<std::string, member_mapping_rule> name_matched_mappings;
		for (auto i_member
			 = boost::dynamic_pointer_cast<dwarf::spec::structure_type_die>(target_type)
			 	->member_children_begin();
			i_member != boost::dynamic_pointer_cast<dwarf::spec::structure_type_die>(target_type)
			 	->member_children_end();
			i_member++)
		{
			assert((*i_member)->get_name());
			if (name_matched_mappings.find(*(*i_member)->get_name())
				== name_matched_mappings.end())
			{
				/* We have a name-matching candidate -- look for like-named
				 * field in opposing structure. */
				
				auto source_as_struct = boost::dynamic_pointer_cast<dwarf::spec::structure_type_die>(
					source_type);
				
				auto found = std::find_if(source_as_struct->member_children_begin(),
					source_as_struct->member_children_end(),
					member_has_name(*(*i_member)->get_name()));
				if (found != source_as_struct->member_children_end())
				{
					std::cerr << "Matched a name " << *(*i_member)->get_name()
						<< " in DIEs " << *source_as_struct
						<< " and " << *target_type
						<< std::endl;
					name_matched_mappings.insert(std::make_pair(
						*(*i_member)->get_name(),
						(member_mapping_rule){
							/* definite_member_name target; */ 
								definite_member_name(1, *(*i_member)->get_name()),
							/* antlr::tree::Tree *stub; */ 
								make_definite_member_name_expr(
									definite_member_name(1, *(*i_member)->get_name())),
							/* module_ptr pre_context; */ 
								source_module,
							/* antlr::tree::Tree *pre_stub; */ 
								0,
							/* module_ptr post_context; */ 
								target_module,
							/* antlr::tree::Tree *post_stub; */ 
								0
						}));
				}
				else
				{
					std::cerr << "Didn't match name " << *(*i_member)->get_name()
						<< " from DIE " << *target_type
						<< " in DIE " << *source_as_struct
						<< std::endl;
				}
				
			}
		}
		/* Create or find buffer */
		m_out << get_type_name(target_type) << " __cake_tmp, *__cake_p_buf;" << std::endl
			<< "if (__cake_p_to) __cake_p_buf = __cake_p_to; else __cake_p_buf = &__cake_tmp;" << std::endl;
		
		/* Emit toplevel mappings right now */
		for (auto i_name_matched = name_matched_mappings.begin();
				i_name_matched != name_matched_mappings.end();
				i_name_matched++)
		{
			auto pre_context_pair = std::make_pair(i_name_matched->second.pre_context, 
				m_d.name_of_module(i_name_matched->second.pre_context));

/* bound_var_info(
    	wrapper_file& w,
        const std::string& prefix,
        boost::shared_ptr<dwarf::spec::type_die> type,
	    const request::module_name_pair& defining_module,
	    boost::shared_ptr<dwarf::spec::program_element_die> origin) */		
			std::string from_ident;
			environment env;
			env.insert(std::make_pair("__cake_from", 
				bound_var_info(
					*this,
					"from",
					source_type,
					pre_context_pair,
					boost::shared_ptr<dwarf::spec::program_element_die>(source_type)
				)));
/*     wrapper_file::emit_stub_expression_as_statement_list(
    		antlr::tree::Tree *expr,
    		link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& context, // sink module
            boost::shared_ptr<dwarf::spec::type_die> cxx_result_type,
            environment env)
*/				
			auto names = emit_stub_expression_as_statement_list(
        		i_name_matched->second.stub, modules, pre_context_pair, 
            	boost::shared_ptr<dwarf::spec::type_die>(), env);
			m_out << "assert(" << names.first << ");" << std::endl;
			m_out << "__cake_p_buf->" << i_name_matched->first
				<< " = ";
			emit_component_pair_classname(modules);
			m_out << "::value_convert_from_"
				<< ((modules.first == source_module) ? "first_to_second" : "second_to_first")
				<< "<" << "__typeof(__cake_p_buf->" << i_name_matched->first << ")"
				//get_type_name(/*target_type*/ i_name_matched->second. )
				<< ">(__cake_from." << i_name_matched->first << ");" << std::endl;
		}
		
		/* Recurse on others */
		// FIXME

		// output return statement
		m_out << "return *__cake_p_buf;" << std::endl;
	}
	
	void
	wrapper_file::emit_reinterpret_conversion_body(
		boost::shared_ptr<dwarf::spec::type_die> source_type,
		boost::shared_ptr<dwarf::spec::type_die> target_type)
	{ 
		m_out << "if (__cake_p_to) *__cake_p_to = *reinterpret_cast<const " 
			<< get_type_name(target_type) << "*>(&__cake_from);" << std::endl
			<< "return *reinterpret_cast<const " 
			<< get_type_name(target_type) << "*>(&__cake_from);" << std::endl;
	}

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
