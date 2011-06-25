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
			std::string cxx_name;
			std::string cxx_typeof; // names a STATIC immediate type 
			module_ptr valid_in_module;
        };
        //typedef std::map<std::string, bound_var_info> environment;
		struct environment : public std::map<std::string, bound_var_info>
		{
		private:
			typedef std::map<std::string, bound_var_info> super;
		public:
			const bound_var_info& operator[](const std::string& k) const
			{ auto found = this->find(k); assert(found != this->end()); return found->second; }
			bound_var_info& operator[](const std::string& k) 
			{ return this->super::operator[](k);  }
		};
        typedef environment::value_type binding;
//         instd::string cxx_name_for_binding(const binding& binding)
//         {
//         	/* FIXME: this logic needs to match the logic used in emit_function_header
//              * in the case of arguments. */
//             std::ostringstream ident_str;
//             ident_str << "__cake_" << binding.second.prefix << binding.second.count 
//             	<< "_" << binding.first;
//             return ident_str.str();
//         }
		
		/* All code-generation functions take one of these as an argument. 
		 * However, which of its members must be present for a given language
		 * feature to work is given a more fine-grained dynamic treatment. */
		struct context
		{
			// these just point to the fields of the wrapper_file
			request& req;
			link_derivation& derivation;
			const std::string& ns_prefix;
			
			// source and sink modules -- all code has these, but they vary within one file
			struct
			{
				module_ptr source;
				module_ptr sink;
				module_ptr current;
			} modules;
			
			// source event 
			struct source_info_s
			{
				// we have at least a signature (if we have a source at all)
				boost::shared_ptr<dwarf::spec::subprogram_die> signature;
				
				// we may have an event pattern
				antlr::tree::Tree *opt_pattern;
				
				// FIXME: how do we get at the names bound?
			};
			boost::optional<source_info_s> opt_source;
			
			// DWARF context -- used for name resolution once after the environment
			struct dwarf_context_s
			{
				boost::shared_ptr<dwarf::spec::with_named_children_die> source_decl;
				boost::shared_ptr<dwarf::spec::with_named_children_die> sink_decl;
			} dwarf_context;
			
			// environment 
			environment env;
			
			context(wrapper_file& w, module_ptr source, module_ptr sink, 
				const environment& initial_env) 
			: req(w.m_r), derivation(w.m_d), ns_prefix(w.ns_prefix), 
			  modules({source, sink, source}), opt_source(), 
			  dwarf_context((dwarf_context_s)
			                {source->get_ds().toplevel(),
			                 sink->get_ds().toplevel()}), 
			  env(initial_env) {}
		};
		
		bool subprogram_returns_void(
			boost::shared_ptr<dwarf::spec::subprogram_die> subprogram);
		
		bool treat_subprogram_as_untyped(
			boost::shared_ptr<dwarf::spec::subprogram_die> subprogram);

		module_ptr module_of_die(boost::shared_ptr<dwarf::spec::basic_die> p_d);
			
		std::string get_type_name(
			boost::shared_ptr<dwarf::spec::type_die> t/*,
				const std::string& namespace_prefix*/);
		
		// Cake high-level constructs
		environment initial_environment(
			antlr::tree::Tree *pattern,
			module_ptr source_module);
		environment merge_environment(
			const environment& env,
			const environment& new_bindings
			);
		environment crossover_environment(
			module_ptr old_module_context,
			const environment& env,
			module_ptr new_module_context,
			const std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> >& constraints
			);

		void open_value_conversion(
			link_derivation::iface_pair ifaces_context,
			int rule_tag,
			boost::shared_ptr<dwarf::spec::type_die> from_type, // most precise
			boost::shared_ptr<dwarf::spec::type_die> to_type, 
			boost::optional<std::string> from_typeof = boost::optional<std::string>(), // mid-precise
			boost::optional<std::string> to_typeof = boost::optional<std::string>(),
			module_ptr from_module = module_ptr(),
			module_ptr to_module = module_ptr()
		);
		
		void close_value_conversion();
		
		std::vector<
			std::pair< 	dwarf::spec::subprogram_die::formal_parameter_iterator,
						dwarf::spec::subprogram_die::formal_parameter_iterator 
			> 
		>
		name_match_parameters(
			boost::shared_ptr< dwarf::spec::subprogram_die > first,
			boost::shared_ptr< dwarf::spec::subprogram_die > second);
		
		std::string component_pair_classname(link_derivation::iface_pair p);
		
		std::string new_ident(const std::string& prefix)
		{
			static int count = 0;
			std::ostringstream out;
			out << prefix << "_" << count++;
			return out.str();
		}
		static const std::string wrapper_arg_name_prefix;
		
//    	void emit_event_pattern_as_function_call(
//        	antlr::tree::Tree *pattern,
//            const request::module_name_pair& sink_context, // sink module
//            const request::module_name_pair& source_context,
//            const dwarf::encap::Die_encap_subprogram& source_signature,
//            environment env);
		// 
	    void write_pattern_condition(
			const context& ctxt,
            antlr::tree::Tree *pattern); 
            // FIXME: also need context about naming environment, to determine 
            // which names are new (to bind) and which denote preexisting values.
//        	const link_derivation::ev_corresp_entry& corresp_pair);
        
		struct post_emit_status
		{
			std::string result_fragment;
			std::string success_fragment;
			environment new_bindings;
		};
		static const std::string NO_VALUE;
		
        //std::pair<std::string, std::string>
        //emit_event_corresp_stub(
		//	const context& ctxt,
    	//    antlr::tree::Tree *stub);

		post_emit_status
		emit_stub_expression_as_statement_list(
			const context& ctxt,
			antlr::tree::Tree *expr/*,
			boost::shared_ptr<dwarf::spec::type_die> cxx_expected_type*/);   

		post_emit_status
		emit_stub_function_call(
			const context& ctxt,
			antlr::tree::Tree *call_expr/*,
			boost::shared_ptr<dwarf::spec::type_die> cxx_expected_type*/);

//		//void 
//		std::string
//		//emit_bound_var_rvalue(
//			reference_bound_variable(
//			const context& ctxt, 
//			const binding& bound_var);

		// C++ primitives.
		void write_function_header(
			antlr::tree::Tree *event_pattern,
			const std::string& function_name_to_use,
			boost::shared_ptr<dwarf::spec::subprogram_die> subprogram,
			const std::string& arg_name_prefix,
			module_ptr caller_context,
			bool emit_types = true,
			boost::shared_ptr<dwarf::spec::subprogram_die> 
				unique_called_subprogram = boost::shared_ptr<dwarf::spec::subprogram_die>());

		std::string get_dwarf_ident(
			const context& ctxt,
			antlr::tree::Tree *definite_member_name);

		std::string get_dwarf_ident(
			const context& ctxt,
			const std::string& ident);

		std::string constant_expr_eval_and_cxxify(
			const context& ctxt,
			antlr::tree::Tree *constant_expr);

		long double eval_const_expr(
			const context& ctxt,
			antlr::tree::Tree *expr);

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
	        link_derivation::ev_corresp_pair_ptr_list& corresps); // module->name mapping

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
