#ifndef CAKE_VALCONV_HPP_
#define CAKE_VALCONV_HPP_

#include <sstream>
#include <map>
#include <vector>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>
#include "link.hpp"

namespace cake
{
	using std::string;
	using std::map;
	using std::vector;
	using boost::optional;
	using boost::shared_ptr;
	using namespace dwarf;
	using boost::shared_ptr;
	
	class wrapper_file;

	struct bound_var_info
	{
		/** The C++ name of the bound value. (The Cake name is not stored here. Rather,
		 *  it is the key associated with this entry in the environment map.) */
		string cxx_name;
		
		/** The C++ type of the bound value ("names a STATIC immediate type"), in the form of
		 *  an expression or variable name that has the same time (i.e. not C++ type syntax). */
		string cxx_typeof;
		
		/** The unique module for which this binding is meaningful. */
		module_ptr valid_in_module;
		
		/** Whether to discard this binding at the next crossover point. Used for temporaries. */
		bool do_not_crossover;
		
		//shared_ptr<with_type_describing_layout_die> dwarf_origin;
		
		/** The key used to look up value conversion. This is the name of a top-level typedef 
		 *  in the module, or an artificial typename used in Cake source code. */
		optional<string> local_tagstring;
		/** Similar, but for remote uses. */
		optional<string> remote_tagstring;
		
		optional<string> indirect_local_tagstring_in;
		optional<string> indirect_local_tagstring_out;
		optional<string> indirect_remote_tagstring_in;
		optional<string> indirect_remote_tagstring_out;
		
		bool is_pointer_to_uninit;
		
		/** Whether the wrapper will handle crossover. Only meaningful if do_not_crossover. */
		bool crossover_by_wrapper;
		
		/** Whether the binding comes from an ellipsis, and if so, what position
		 within the ellipsis it corresponds to, or -1 if name-only ellipsis. */
		optional<int> from_ellipsis;
	};
	//typedef std::map<std::string, bound_var_info> environment;
	struct environment : public map<string, bound_var_info>
	{
	private:
		typedef map<string, bound_var_info> super;
	public:
		const bound_var_info& operator[](const string& k) const
		{ auto found = this->find(k); assert(found != this->end()); return found->second; }
		bound_var_info& operator[](const string& k) 
		{ return this->super::operator[](k);  }
		environment(std::initializer_list< super::value_type > l)
		: super(l) {}
		environment() {}
		environment(const environment& c) : super(c) {}
	};
	
	struct post_emit_status
	{
		string result_fragment;
		string success_fragment;
		environment new_bindings;
	};
	
	/* All code-generation functions take one of these as an argument. 
	 * However, which of its members must be present for a given language
	 * feature to work is given a more fine-grained dynamic treatment. */
	struct codegen_context
	{
		// these just point to the fields of the wrapper_file
		request& req;
		link_derivation& derivation;
		const string& ns_prefix;

		// source and sink modules -- all code has these, but they vary within one file
		struct
		{
			module_ptr source;
			module_ptr sink;
			module_ptr current;
		} modules;
		link_derivation::iface_pair get_ifaces() const;
		
		// source event 
		struct source_info_s
		{
			// we have at least a signature (if we have a source at all)
			shared_ptr<spec::subprogram_die> signature;

			// we may have an event pattern
			antlr::tree::Tree *opt_pattern;
			
			// our event pattern may have included an ellipsis
			optional<unsigned> ellipsis_begin_argpos;
		};
		optional<source_info_s> opt_source;

		// val corresp
		struct val_info_s
		{
			// val corresp context
			antlr::tree::Tree *rule;
		};
		optional<val_info_s> opt_val_corresp;

		// DWARF context -- used for name resolution once after the environment
		struct dwarf_context_s
		{
			shared_ptr<spec::with_named_children_die> source_decl;
			shared_ptr<spec::with_named_children_die> sink_decl;
		} dwarf_context;

		// environment 
		environment env;

		codegen_context(wrapper_file& w, module_ptr source, module_ptr sink, 
			const environment& initial_env);
	};
	
	struct basic_value_conversion
	{
		module_ptr source;

		boost::shared_ptr<dwarf::spec::type_die> source_data_type;
		antlr::tree::Tree *source_infix_stub;
		module_ptr sink;
		boost::shared_ptr<dwarf::spec::type_die> sink_data_type;
		antlr::tree::Tree *sink_infix_stub;
		antlr::tree::Tree *refinement;
		bool source_is_on_left;
		antlr::tree::Tree *corresp; // for generating errors
		bool init_only;
		
		friend std::ostream& operator<<(std::ostream& s, const basic_value_conversion& c);
	}; 
	std::ostream& operator<<(std::ostream& s, const basic_value_conversion& c);
	
	// base class
	class value_conversion : public boost::enable_shared_from_this<value_conversion>, 
		public basic_value_conversion
	{
		friend class link_derivation;
	protected:
		wrapper_file& w;
		srk31::indenting_ostream& m_out;
		boost::shared_ptr<dwarf::spec::type_die> source_concrete_type;
		boost::shared_ptr<dwarf::spec::type_die> sink_concrete_type;
		std::string from_typename;
		std::string to_typename;
		
		void emit_preamble();
		void emit_postamble()
		{
        	m_out.dec_level();
        	m_out << "}" << std::endl;
		}
		
	protected:
		std::string source_fq_namespace() const;
		std::string sink_fq_namespace() const;
		virtual void emit_header(boost::optional<std::string> return_typename, 
			bool emit_struct_keyword = true, bool emit_template_prefix = true,
			bool emit_return_typename = true);
		virtual void emit_signature(bool emit_return_type = true, bool emit_default_argument = true);
		
	public:
		typedef std::pair < boost::shared_ptr<dwarf::spec::type_die>,
		                         boost::shared_ptr<dwarf::spec::type_die>
								> dep;
		value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out,
			const basic_value_conversion& basic);
		virtual void emit() { emit_preamble(); emit_body(); emit_postamble(); }
		virtual void emit_body() = 0;
		virtual void emit_forward_declaration();
		virtual void emit_function_name();
		virtual void emit_cxx_function_ptr_type(boost::optional<const std::string&>);
		virtual std::vector< std::pair < boost::shared_ptr<dwarf::spec::type_die>,
		                                 boost::shared_ptr<dwarf::spec::type_die>
										>
								> get_dependencies();
		
		static vector<shared_ptr<value_conversion> >
		dep_is_satisfied(
			link_derivation::val_corresp_iterator begin,
			link_derivation::val_corresp_iterator end,
			const value_conversion::dep& m_dep);
	};
		
	// null impl
	class skipped_value_conversion : public value_conversion
	{
		const std::string reason;
	public:
		skipped_value_conversion(wrapper_file& w, 
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic, const std::string& reason) 
		: value_conversion(w, out, basic), reason(reason) {}
		
		void emit() { emit_body(); }
		void emit_body()
		{
			m_out << "// (skipped because of " << reason << ")" << std::endl << std::endl;
		}
		void emit_forward_declaration() {}
	};

	// reinterpret_cast impl
	class reinterpret_value_conversion : public value_conversion
	{
	public:
		reinterpret_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic) 
		: value_conversion(w, out, basic) {}
		
		void emit_body();
	};
	
	// primitive_value_conversion impl
	class primitive_value_conversion : public virtual value_conversion
	{
	public:
		primitive_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic,
			bool make_init_conversion,
			bool& out_init_and_update_are_identical);
		
		void emit_body();
	protected:
		module_ptr source_module;
		module_ptr target_module;
		std::pair<module_ptr, module_ptr> modules;
		virtual void emit_source_object_alias();
		virtual void emit_target_buffer_declaration();
		virtual void emit_initial_declarations();
		virtual void write_single_field(
			codegen_context& ref_ctxt,
			string target_field_selector,
			optional<string> unique_source_field_selector,
			antlr::tree::Tree *source_expr,
			antlr::tree::Tree *source_infix,
			antlr::tree::Tree *sink_infix,
			bool write_void_target = false);
	};
	
	// structural impl
	class structural_value_conversion : public virtual value_conversion, 
		private primitive_value_conversion // we share code with this guy
	{
		struct member_mapping_rule_base
		{
			structural_value_conversion *owner;
			definite_member_name target;
			boost::optional<definite_member_name> unique_source_field;
			antlr::tree::Tree *stub;
			module_ptr pre_context;
			antlr::tree::Tree *pre_stub;
			module_ptr post_context;
			antlr::tree::Tree *post_stub;
			antlr::tree::Tree *rule_ast;
		};
		
		struct member_mapping_rule : public member_mapping_rule_base
		{
			member_mapping_rule(const member_mapping_rule_base& val);
			
			void check_sanity();
		};
		
// 		struct explicit_member_mapping_rule : member_mapping_rule
// 		{
// 			definite_member_name source;
// 			antlr::tree::Tree *details; // pairwise sub-block
// 		};
		
		// this includes all EXPLICIT corresps only, 
		// but INCLUDING non-toplevel entries of the form foo.bar <--> blah.blah
	protected:
		std::map<definite_member_name, member_mapping_rule> explicit_field_corresps;
		
		// this includes all EXPLICIT corresps, projected to TOPLEVEL -- need not be unique
		typedef std::multimap<std::string, member_mapping_rule*> explicit_toplevel_mappings_t;
		explicit_toplevel_mappings_t explicit_toplevel_mappings;
		
		// FIXME: if we have a group of nontoplevel rules, we should synthesise a 
		// pseudo-correspondence and just use that. Anything wrong with this?
		
		// this includes only NAME-MATCHED IMPLICIT corresps only
		std::map<std::string, member_mapping_rule> name_matched_mappings;
		
		//std::map<std::string, member_mapping_rule> explicit_mappings;
		
		// hooks for subclasses to add extra behaviour
		virtual void post_sink_stub_hook(
			const environment& env, 
			const post_emit_status& return_status
		) {}

	public:
		structural_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic,
			bool make_init_conversion,
			bool& out_init_and_update_are_identical);
		
		void emit_body();
		vector<dep> get_dependencies();
	};
	
	// virtual data types
	class virtual_value_conversion : public structural_value_conversion
	{
	protected:
		void post_sink_stub_hook(
					const environment& env, 
					const post_emit_status& return_status
				);
	public:
		virtual_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic,
			bool dummy = false) 
		: value_conversion(w, out, basic), 
		  structural_value_conversion(w, out, basic, false, dummy) {}
		
		void emit_body();
		vector<dep> get_dependencies()
		{ return vector<dep>(); }
		void emit_target_buffer_declaration();
		void emit_header(optional<string> return_typename, 
			bool emit_struct_keyword/* = true */, bool emit_template_prefix/* = true */,
			bool emit_return_typename/* = true*/);
		void emit_signature(bool emit_return_type /* = true */, 
			bool emit_default_argument /* = true */);
		bool treat_target_type_as_user_allocated();
	};
	
} // end namespace cake

#endif
