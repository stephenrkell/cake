#ifndef CAKE_WRAPSRC_HPP_
#define CAKE_WRAPSRC_HPP_

#include <sstream>
#include <map>
#include <vector>
#include <dwarfpp/abstract.hpp>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>

#include "link.hpp"

namespace cake
{
    class wrapper_file
    {
        dwarf::tool::cxx_compiler compiler;
        derivation& m_d;
        std::ostream& m_out;
        const std::string ns_prefix;
        
        // We need a structure to hold bound names. What can be bound?
        // - Formal parameters of a wrapper's caller.
        // - Intermediate results in a stub
        struct bound_var_info
        {
        	boost::optional<dwarf::encap::Die_encap_is_type&> type;
            const request::module_name_pair& module;
            dwarf::encap::Die_encap_is_program_element& origin;
            bound_var_info(boost::optional<dwarf::encap::Die_encap_is_type&> type,
	            const request::module_name_pair& module,
	            dwarf::encap::Die_encap_is_program_element& origin)
                : type(type), module(module), origin(origin) {}
        };
        typedef std::map<std::string, bound_var_info> environment;
        typedef environment::value_type binding;
        
        // About context:

        // Emitters that are specific to a single module take a 
        // module_name_pair as request context.

        // Emitters that generate code interacting with multiple
        // modules take a module_inverse_tbl_t as request context.

        // DWARF context is used where nested expression ASTs may be
        // resolving names against some non-toplevel context in the
        // DWARF info, e.g. tuple field names are resolved against
        // the relevant type DIE.
        
		bool subprogram_returns_void(
        	const dwarf::encap::Die_encap_subprogram& subprogram);
            
		bool treat_subprogram_as_untyped(
        	const dwarf::encap::Die_encap_subprogram& subprogram);
            
		void emit_wrapper_body(
        	const std::string& wrapped_symname, 
            const dwarf::encap::Die_encap_subprogram& wrapper_sig, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps,
            const request::module_inverse_tbl_t& request_context);

		// Cake high-level constructs
	    void emit_pattern_condition(
            antlr::tree::Tree *pattern,
            const request::module_name_pair& request_context,
            environment *out_env); 
            // FIXME: also need context about naming environment, to determine 
            // which names are new (to bind) and which denote preexisting values.
//        	const link_derivation::ev_corresp_entry& corresp_pair);

	    void emit_sink_action(
        	antlr::tree::Tree *action,
            const dwarf::encap::Die_encap_subprogram& wrapper_sig, 
            const request::module_name_pair& sink_context,
            const request::module_name_pair& source_context,
            environment env);

    	void emit_type_name(
            const dwarf::encap::Die_encap_is_type& t,
            const std::string& namespace_prefix);
            
        void open_value_conversion(
    	    boost::optional<dwarf::encap::Die_encap_is_type&> from_type,
            const std::string& from_namespace,
            boost::optional<dwarf::encap::Die_encap_is_type&> to_type,
            const std::string& to_namespace);
        
        void close_value_conversion();

    	void emit_event_pattern_as_function_call(
        	antlr::tree::Tree *pattern,
            const request::module_name_pair& sink_context, // sink module
            const request::module_name_pair& source_context,
            const dwarf::encap::Die_encap_subprogram& source_signature,
            environment env);
        
        // C++ primitives.
		void emit_function_header(
            antlr::tree::Tree *event_pattern,
        	const std::string& function_name_to_use,
            dwarf::encap::Die_encap_subprogram& subprogram,
            bool emit_types = true);

        void emit_symbol_reference_expr_from_dwarf_ident(
            antlr::tree::Tree *definite_member_name, 
    	    const request::module_name_pair& request_context, 
            dwarf::abstract::Die_abstract_has_named_children<dwarf::encap::die>& dwarf_context);

        void emit_constant_expr(
        	antlr::tree::Tree *constant_expr,
    	    const request::module_name_pair& request_context);
            
        long double eval_const_expr(
	    	antlr::tree::Tree *expr,
    	    const request::module_name_pair& request_context);



    public:
        wrapper_file(derivation& d, std::ostream& out) 
        : 	compiler(std::vector<std::string>(1, std::string("g++"))),
        	m_d(d), m_out(out), ns_prefix("cake::" + m_d.namespace_name()) {}

        void emit_wrapper(
        	const std::string& wrapped_symname, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps,
            const request::module_inverse_tbl_t& request_context);
    };
}

#endif
