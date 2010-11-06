#ifndef CAKE_WRAPSRC_HPP_
#define CAKE_WRAPSRC_HPP_

#include <sstream>
#include <map>
#include <vector>
#include <dwarfpp/abstract.hpp>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>

#include "link.hpp"
#include "valconv.hpp"

namespace cake
{
    class wrapper_file
    {
		friend class link_derivation;
		friend class value_conversion;
		friend class structural_value_conversion;
		friend class reinterpret_value_conversion;
	
        dwarf::tool::cxx_compiler& compiler;
        link_derivation& m_d;
        request& m_r;
        srk31::indenting_ostream m_out;
        const std::string ns_prefix;
        int binding_count;

        //std::string first_module_namespace_name(link_derivation::iface_pair p)
        //{ return m_d.name_of_module(p.first); }
        //std::string second_module_namespace_name(link_derivation::iface_pair p)
        //{ return m_d.name_of_module(p.second); }
        
        // We need a structure to hold bound names. What can be bound?
        // - Formal parameters of a wrapper's caller.
        // - Intermediate results in a stub
        typedef std::pair<std::string, std::string> cu_ident;
        cu_ident get_cu_ident(boost::shared_ptr<dwarf::spec::basic_die> d)
        { return std::make_pair(*d->enclosing_compile_unit()->get_name(), 
        	*d->enclosing_compile_unit()->get_comp_dir());
    	}
        typedef std::pair<cu_ident, Dwarf_Off> stable_die_ident;
        stable_die_ident get_stable_die_ident(boost::shared_ptr<dwarf::spec::basic_die> d)
        {
        	return std::make_pair(get_cu_ident(d), d->get_offset());
        }
        std::map<stable_die_ident, unsigned> arg_counts;
    
	    struct bound_var_info
        {
        	boost::shared_ptr<dwarf::spec::type_die> type; // is a STATIC type (DWARF-supplied bound)
            const request::module_name_pair& defining_module;
            boost::shared_ptr<dwarf::spec::program_element_die> origin;
            std::string prefix;
            int count;
            bound_var_info(wrapper_file& w,
            	const std::string& prefix,
            	boost::shared_ptr<dwarf::spec::type_die> type,
	            const request::module_name_pair& defining_module,
	            boost::shared_ptr<dwarf::spec::program_element_die> origin);
            // special version for formal parameters! works out count and prefix
            // -- note: not virtual, so make sure we only use this for formal_parameter DIEs
            bound_var_info(wrapper_file& w,
            	//const std::string& prefix,
            	boost::shared_ptr<dwarf::spec::type_die> type,
	            const request::module_name_pair& defining_module,
	            boost::shared_ptr<dwarf::spec::formal_parameter_die> origin);
            bound_var_info(wrapper_file& w,
            	//const std::string& prefix,
            	boost::shared_ptr<dwarf::spec::type_die> type,
	            const request::module_name_pair& defining_module,
	            boost::shared_ptr<dwarf::spec::unspecified_parameters_die> origin);
        };
        typedef std::map<std::string, bound_var_info> environment;
        typedef environment::value_type binding;
        std::string cxx_name_for_binding(const binding& binding)
        {
        	/* FIXME: this logic needs to match the logic used in emit_function_header
             * in the case of arguments. */
            std::ostringstream ident_str;
            ident_str << "__cake_" << binding.second.prefix << binding.second.count 
            	<< "_" << binding.first;
            return ident_str.str();
        }
        
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
        bool treat_subprogram_as_untyped(
        	boost::shared_ptr<dwarf::spec::subprogram_die> subprogram);
			
		void emit_wrapper_body(
        	const std::string& wrapped_symname, 
            const dwarf::encap::Die_encap_subprogram& wrapper_sig, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps,
            const request::module_inverse_tbl_t& request_context);
			
		void emit_component_type_name(boost::shared_ptr<dwarf::spec::type_die> t);

		// Cake high-level constructs
		void extract_source_bindings(
            antlr::tree::Tree *pattern,
            const request::module_name_pair& request_context,
            environment *out_env,
			antlr::tree::Tree *sink_action);
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
        
        std::pair<std::string, std::string>
        emit_event_corresp_stub(
    	    antlr::tree::Tree *stub, 
            link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& sink_context, // sink module
            const request::module_name_pair& source_context,
            const dwarf::encap::Die_encap_subprogram& source_signature,
            environment env);
        
		module_ptr module_of_die(boost::shared_ptr<dwarf::spec::basic_die> p_d);
		
    	void emit_type_name(
            boost::shared_ptr<dwarf::spec::type_die> t/*,
            const std::string& namespace_prefix*/);
            
        std::string get_type_name(
        	boost::shared_ptr<dwarf::spec::type_die> t/*,
            	const std::string& namespace_prefix*/);
            
        void open_value_conversion(
	    	link_derivation::iface_pair ifaces_context,
    	    boost::shared_ptr<dwarf::spec::type_die> from_type,
            //const std::string& from_namespace,
            module_ptr from_module,
            boost::shared_ptr<dwarf::spec::type_die> to_type/*,
            const std::string& to_namespace*/
            , module_ptr to_module);
        
        void close_value_conversion();
        
        void emit_component_pair_classname(link_derivation::iface_pair p);

//    	void emit_event_pattern_as_function_call(
//        	antlr::tree::Tree *pattern,
//            const request::module_name_pair& sink_context, // sink module
//            const request::module_name_pair& source_context,
//            const dwarf::encap::Die_encap_subprogram& source_signature,
//            environment env);

	    std::pair<std::string, std::string> 
        emit_stub_expression_as_statement_list(
    		antlr::tree::Tree *expr,
        	link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& context, // sink module
            boost::shared_ptr<dwarf::spec::type_die> cxx_result_type, // may be null
            //const std::string& cxx_result_type_name,
            environment env);   
               
		std::pair<std::string, std::string> 
        emit_stub_function_call(
        	antlr::tree::Tree *call_expr,
        	link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& context, // sink module
            //const request::module_name_pair& source_context,
            //const dwarf::encap::Die_encap_subprogram& source_signature,
            boost::shared_ptr<dwarf::spec::type_die> cxx_result_type, // may be null
            //const std::string& cxx_result_type_name,
            environment env);

		void 
        emit_bound_var_rvalue(antlr::tree::Tree *call_expr, 
            link_derivation::iface_pair ifaces_context,
            const request::module_name_pair& sink_context,
            //const request::
            const binding& bound_var, 
            environment env,
            boost::shared_ptr<dwarf::spec::type_die> cxx_result_type);
        
        std::string new_ident(const std::string& prefix)
        {
        	static int count = 0;
            std::ostringstream out;
            out << prefix << "_" << count++;
            return out.str();
		}
        
        // C++ primitives.
		void emit_function_header(
            antlr::tree::Tree *event_pattern,
        	const std::string& function_name_to_use,
            dwarf::encap::Die_encap_subprogram& subprogram,
            const std::string& arg_name_prefix,
            const request::module_name_pair& caller_context,
            bool emit_types = true,
			boost::shared_ptr<dwarf::spec::subprogram_die> = boost::shared_ptr<dwarf::spec::subprogram_die>());

        void emit_symbol_reference_expr_from_dwarf_ident(
            antlr::tree::Tree *definite_member_name, 
    	    const request::module_name_pair& request_context, 
            dwarf::abstract::Die_abstract_has_named_children<dwarf::encap::die>& dwarf_context);

        void emit_symbol_reference_expr_from_dwarf_ident(
            const std::string& ident, 
    	    const request::module_name_pair& request_context, 
            dwarf::abstract::Die_abstract_has_named_children<dwarf::encap::die>& dwarf_context);

        void emit_constant_expr(
        	antlr::tree::Tree *constant_expr,
    	    const request::module_name_pair& request_context);
            
        long double eval_const_expr(
	    	antlr::tree::Tree *expr,
    	    const request::module_name_pair& request_context);



    public:
        wrapper_file(link_derivation& d, dwarf::tool::cxx_compiler& c, std::ostream& out) 
        : 	compiler(c),
        	m_d(d), m_r(d.r), m_out(out), ns_prefix("cake::" + m_d.namespace_name()), 
            binding_count(0) {}

		/* Main interface. Note that
         * - one wrapper may collect together logic from several interface pairings! 
         *   (because might have multiple calling components) */
        void emit_wrapper(
        	const std::string& wrapped_symname, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps,   // 
            const request::module_inverse_tbl_t& request_context); // module->name mapping

        void
        emit_value_conversion(
        	module_ptr source,
            boost::shared_ptr<dwarf::spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
            antlr::tree::Tree *sink_infix_stub,
            antlr::tree::Tree *refinement,
			bool source_is_on_left,
			antlr::tree::Tree *corresp);

	protected:
// 		void
// 		emit_structural_conversion_body(
// 			boost::shared_ptr<dwarf::spec::type_die> source_type,
// 			boost::shared_ptr<dwarf::spec::type_die> target_type,
// 			antlr::tree::Tree *refinement,
// 			bool source_is_on_left);
// 		void
// 		emit_reinterpret_conversion_body(
// 			boost::shared_ptr<dwarf::spec::type_die> source_type,
// 			boost::shared_ptr<dwarf::spec::type_die> target_type);    
	};
}

#endif
