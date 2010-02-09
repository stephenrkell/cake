#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"
#include <sstream>

namespace cake
{
	std::string wrapper_file::function_header(
    		boost::optional<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&> ret_type,
        	const std::string& function_name,
            dwarf::encap::die::formal_parameters_iterator args_begin,
            dwarf::encap::die::formal_parameters_iterator args_end,
            antlr::tree::Tree *source_pattern,
            dwarf::encap::Die_encap_subprogram& subprogram)
    {
		std::ostringstream s;
        
        s << (ret_type ? compiler.name_for(
        					dynamic_cast<dwarf::encap::Die_encap_is_type&>(*ret_type)) 
                       : "void" ) << ' ';
        s << function_name << '(';
        
        
        //for (
        dwarf::encap::die::formal_parameters_iterator i_arg = args_begin;
	    //    	i_arg != args_end; i_arg++)
        
        // actually, iterate over the pattern
        assert(GET_TYPE(source_pattern) == CAKE_TOKEN(EVENT_PATTERN));
        INIT;
        BIND3(source_pattern, eventContext, EVENT_CONTEXT);
        BIND2(source_pattern, memberNameExpr); // we ignore this
        bool ignore_dwarf_args = (args_begin == args_end
        				 && subprogram.unspecified_parameters_begin() !=
                    	subprogram.unspecified_parameters_end());
		int argnum = 0;
        switch(GET_CHILD_COUNT(source_pattern))
        {
            case 2: // okay, we have no argument list (simple renames only)
            	if (ignore_dwarf_args)
                {  		
                	// problem: no source of argument info! Assume none
                	std::cerr << "Warning: wrapping function " << function_name
                	    << " declared with unspecified parameters"
                        << " and no event pattern arguments. "
                        << "Assuming empty argument list." << std::endl; }
                break;
            default: // must be >=3
            	assert(GET_CHILD_COUNT(source_pattern) >= 3);
                FOR_REMAINING_CHILDREN(source_pattern)
                {
                	if (!ignore_dwarf_args && i_arg == args_end)
                    {
                        std::ostringstream msg;
	                    msg << "argument pattern has too many arguments for subprogram "
                        	<< subprogram;
	                    RAISE(source_pattern, msg.str());
	                }
                    ALIAS3(n, annotatedValuePattern, ANNOTATED_VALUE_PATTERN);
                    {
                    	INIT;
                        BIND2(n, valuePattern)
                        switch(GET_TYPE(valuePattern))
                        {
                    	    // these are all okay -- we don't care 
                    	    case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
                            	// output the variable type, or "int"
                                s << ignore_dwarf_args ? compiler.name_for(
                                	dynamic_cast<dwarf::encap::Die_encap_is_type&>((*i_arg)->get_type())) : "int";
                                s << ' ' << GET_TEXT(valuePattern);
                                break;
                            case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
                    	    case CAKE_TOKEN(METAVAR):
                            case CAKE_TOKEN(KEYWORD_CONST):
                            	// output the argument type and a dummy name
                                s << ignore_dwarf_args ? compiler.name_for(
                                	dynamic_cast<dwarf::encap::Die_encap_is_type&>((*i_arg)->get_type())) : "int";
                                s << ' ' << "arg" << argnum;
                                break;                                
                            default: RAISE_INTERNAL(valuePattern, "not a value pattern");
                        }
                    }
                next:
                	// do we need a comma?
                    i_arg++;
                    argnum++;
                    if (i_arg != args_end) s << ", ";
                }
                
                if (!ignore_dwarf_args && i_arg != args_end)
                {
                	std::ostringstream msg;
                    msg << "argument pattern has too few arguments for subprogram: "
                    	<< subprogram;
                	RAISE(source_pattern, msg.str());
                }
                break;
        }
        
//         {
//         	// output argument type, or "int"
// 	        s << ((*i_arg)->has_attr(DW_AT_type) ?
//             	compiler.name_for(
//    					dynamic_cast<dwarf::encap::Die_encap_is_type&>(**i_arg))
//                        : "int" ) << ' ';
// 
// 			// output argument name
//                        
//             // do we need a comma?
//             
//         }
        s << ')';

		std::cerr << "Generated function header: " << s.str() << std::endl;
        s.flush();
		return s.str();
    }

    std::string wrapper_file::emit_wrapper(const std::string& wrapped_symname, 
        link_derivation::wrapper_corresp_list& corresps)
    {
        std::ostringstream src;

        // ensure a nonempty corresp list
        if (corresps.size() == 0) return "";

	    // 1. emit wrapper prototype
        /* We take *any* source module, and assume that its call-site is
         * definitive. This *should* give us args and return type, but may not.
         * If our caller's DWARF doesn't specify arguments and return type,
         * we guess from the pattern AST. If the pattern AST doesn't include
         * arguments, we just emit "..." and an int return type.
         */
	    std::cerr << "Building wrapper for symbol " << wrapped_symname << std::endl;
        
        for (dwarf::encap::die::named_children_iterator i = 
        	corresps.at(0)->second.source->all_compile_units().named_children_begin();
            i != corresps.at(0)->second.source->all_compile_units().named_children_end();
            i++)
        {
        	std::cerr << "Named child in all_compile_units: " << *((*i)->get_name()) << std::endl;
        }

	    boost::optional<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&> opt_subp = 
            corresps.at(0)->second.source->all_compile_units()
                .named_child(wrapped_symname);
		if (!opt_subp) 
        { std::cerr << "Bailing from wrapper generation -- no subprogram!" << std::endl; 
          return ""; }
        
        // convenience alias of subprogram
        dwarf::encap::Die_encap_subprogram& subprogram = 
        	dynamic_cast<dwarf::encap::Die_encap_subprogram&>(*opt_subp);

		// output prototype for __real_
        src << "extern " << function_header(subprogram.get_type(),
        		"__real_" + *(subprogram.get_name()),
                subprogram.formal_parameters_begin(),
                subprogram.formal_parameters_end(),
                corresps.at(0)->second.source_pattern,
                subprogram) << " __attribute__((weak));" << std::endl;
        // output prototype for __wrap_
        src << function_header(subprogram.get_type(),
        		"__wrap_" + *(subprogram.get_name()),
                subprogram.formal_parameters_begin(),
                subprogram.formal_parameters_end(),
                corresps.at(0)->second.source_pattern,
                subprogram) << ';' << std::endl;                    
        // output wrapper
        src << function_header(subprogram.get_type(),
        		"__wrap_" + *(subprogram.get_name()),
                subprogram.formal_parameters_begin(),
                subprogram.formal_parameters_end(),
                corresps.at(0)->second.source_pattern,
                subprogram) << '{' << std::endl;

        // 3. emit wrapper definition
        
        src << '}' << std::endl;
        assert(src.str().size() > 0);
        src.flush();
        return src.str();
    }
}
