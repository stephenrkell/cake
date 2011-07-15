#ifndef CAKE_VALCONV_HPP_
#define CAKE_VALCONV_HPP_

#include <sstream>
#include <map>
#include <vector>
#include <dwarfpp/abstract.hpp>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>

namespace cake
{
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
	}; 
	
	// base class
	class value_conversion : public basic_value_conversion
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
		void emit_header(boost::optional<std::string> return_typename, 
			bool emit_struct_keyword = true, bool emit_template_prefix = true,
			bool emit_return_typename = true);
		void emit_signature(bool emit_return_type = true, bool emit_default_argument = true);
		
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
	// structural impl
	class structural_value_conversion : public value_conversion
	{
		module_ptr source_module;
		module_ptr target_module;
		std::pair<module_ptr, module_ptr> modules;

		struct member_mapping_rule
		{
			definite_member_name target;
			antlr::tree::Tree *stub;
			module_ptr pre_context;
			antlr::tree::Tree *pre_stub;
			module_ptr post_context;
			antlr::tree::Tree *post_stub;
		};
// 		struct explicit_member_mapping_rule : member_mapping_rule
// 		{
// 			definite_member_name source;
// 			antlr::tree::Tree *details; // pairwise sub-block
// 		};
		
		// this includes all EXPLICIT corresps only, 
		// but INCLUDING non-toplevel entries of the form foo.bar <--> blah.blah
		std::map<definite_member_name, member_mapping_rule> explicit_field_corresps;
		
		// this includes all EXPLICIT corresps, projected to TOPLEVEL -- need not be unique
		std::multimap<std::string, member_mapping_rule*> explicit_toplevel_mappings;
		
		// this includes only NAME-MATCHED IMPLICIT corresps only
		std::map<std::string, member_mapping_rule> name_matched_mappings;
		
		//std::map<std::string, member_mapping_rule> explicit_mappings;
	public:
		structural_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic);
		
		void emit_body();
		std::vector< dep
				> get_dependencies();
	};
	
} // end namespace cake

#endif
