#ifndef CAKE_WRAPSRC_HPP_
#define CAKE_WRAPSRC_HPP_

#include <sstream>
#include <map>
#include <vector>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>

#include "link.hpp"
#include "valconv.hpp"

namespace cake
{
	using std::cerr;
	using std::string;
	using std::pair;
	using std::make_pair;
	using std::map;
	using boost::optional;
	using boost::shared_ptr;
	using boost::dynamic_pointer_cast;
	using namespace dwarf;
	using dwarf::tool::cxx_compiler;
	using srk31::indenting_ostream;
	
    class wrapper_file
    {
		friend class link_derivation;
		friend class value_conversion;
		friend class structural_value_conversion;
		friend class primitive_value_conversion;
		friend class reinterpret_value_conversion;
		friend class codegen_context;
	
        cxx_compiler& compiler;
        link_derivation& m_d;
        request& m_r;
        indenting_ostream m_out;
        const string ns_prefix;
        int binding_count;

        //std::string first_module_namespace_name(link_derivation::iface_pair p)
        //{ return m_d.name_of_module(p.first); }
        //std::string second_module_namespace_name(link_derivation::iface_pair p)
        //{ return m_d.name_of_module(p.second); }
        
        // We need a structure to hold bound names. What can be bound?
        // - Formal parameters of a wrapper's caller.
        // - Intermediate results in a stub
        typedef pair<string, string> cu_ident;
        cu_ident get_cu_ident(shared_ptr<spec::basic_die> d)
        { return make_pair(*d->enclosing_compile_unit()->get_name(), 
        	*d->enclosing_compile_unit()->get_comp_dir());
    	}
        typedef pair<cu_ident, Dwarf_Off> stable_die_ident;
        stable_die_ident get_stable_die_ident(shared_ptr<spec::basic_die> d)
        {
        	return make_pair(get_cu_ident(d), d->get_offset());
        }
        map<stable_die_ident, unsigned> arg_counts;
    public:
		typedef ::cake::bound_var_info bound_var_info;
		typedef ::cake::environment environment;
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
		
		typedef codegen_context context;

		// HACK: these are somewhat utility functions
		bool subprogram_returns_void(
			shared_ptr<spec::subprogram_die> subprogram);
		
		bool treat_subprogram_as_untyped(
			shared_ptr<spec::subprogram_die> subprogram);

		// HACK: these are _even_ more utility functions
		module_ptr module_of_die(shared_ptr<spec::basic_die> p_d);
			
		string get_type_name(
			shared_ptr<spec::type_die> t/*,
				const std::string& namespace_prefix*/);
				
	private:
		
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
			//int rule_tag,
			shared_ptr<spec::type_die> from_type, // most precise
			shared_ptr<spec::type_die> to_type, 
			optional<string> from_typeof = optional<string>(), // mid-precise
			optional<string> to_typeof = optional<string>(),
			module_ptr from_module = module_ptr(),
			module_ptr to_module = module_ptr()
		);
		
		void close_value_conversion();
		
		std::vector<
			pair< 	spec::subprogram_die::formal_parameter_iterator,
					spec::subprogram_die::formal_parameter_iterator 
			> 
		>
		name_match_parameters(
			shared_ptr< spec::subprogram_die > first,
			shared_ptr< spec::subprogram_die > second);
		
		string component_pair_classname(link_derivation::iface_pair p);
		
		string new_ident(const string& prefix)
		{
			static int count = 0;
			std::ostringstream out;
			out << prefix << "_" << count++;
			return out.str();
		}
		static const string wrapper_arg_name_prefix;
		
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
			string result_fragment;
			string success_fragment;
			environment new_bindings;
		};
		static const string NO_VALUE;
		
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
			const string& function_name_to_use,
			shared_ptr<spec::subprogram_die> subprogram,
			const string& arg_name_prefix,
			module_ptr caller_context,
			bool emit_types = true,
			shared_ptr<spec::subprogram_die> 
				unique_called_subprogram = shared_ptr<spec::subprogram_die>());

		string get_dwarf_ident(
			const context& ctxt,
			antlr::tree::Tree *definite_member_name);

		string get_dwarf_ident(
			const context& ctxt,
			const std::string& ident);

		string constant_expr_eval_and_cxxify(
			const context& ctxt,
			antlr::tree::Tree *constant_expr);

		long double eval_const_expr(
			const context& ctxt,
			antlr::tree::Tree *expr);

		optional<string> 
		extract_artificial_data_type(
			shared_ptr<spec::type_die> source_data_type, 
			const context& ctxt);
	
		int 
		lookup_rule_tag(
			shared_ptr<spec::type_die> source_data_type, 
			shared_ptr<spec::type_die> sink_data_type, 
			const string& source_artificial_identifier,
			const string& sink_artificial_identifier,
			bool is_init);

    public:
        wrapper_file(link_derivation& d, cxx_compiler& c, std::ostream& out) 
        : 	compiler(c),
        	m_d(d), m_r(d.r), m_out(out), ns_prefix("cake::" + m_d.namespace_name()), 
            binding_count(0) {}

		/* Main interface. Note that
         * - one wrapper may collect together logic from several interface pairings! 
         *   (because might have multiple calling components) */
        void emit_wrapper(
        	const string& wrapped_symname, 
	        link_derivation::ev_corresp_pair_ptr_list& corresps); // module->name mapping

        void
        emit_value_conversion(
        	module_ptr source,
            shared_ptr<spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            shared_ptr<spec::type_die> sink_data_type,
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
