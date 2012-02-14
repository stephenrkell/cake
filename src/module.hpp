#ifndef __CAKE_MODULE_HPP
#define __CAKE_MODULE_HPP

#include <string>
#include <set>
#include <map>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include <fileno.hpp>
#include <dwarfpp/lib.hpp>
#include <dwarfpp/encap.hpp>
#include <indenting_ostream.hpp>
#include "parser.hpp"


namespace antlr { namespace tree { typedef ANTLR3_BASE_TREE Tree; } }

namespace cake
{
	using namespace dwarf::lib;
	using namespace antlr::tree;
	using dwarf::spec::type_die;
	using dwarf::spec::basic_die;
	using boost::shared_ptr;
	using boost::dynamic_pointer_cast;
	
	class definite_member_name;
	class derivation;
	class link_derivation;
	class module
	{
		friend class link_derivation;
	protected:
		std::string filename;
	
		typedef std::pair<const std::string, const std::string> constructor_map_entry;
		static constructor_map_entry known_constructor_extensions[];
	
		// map used to generate filenames of correct extensions for derived modules	
		static std::map<std::string, std::string> known_constructors;
	
	public:
		module(const std::string& filename) : filename(filename) {}
		std::string& get_filename() { return filename; }
		static std::string extension_for_constructor(std::string& module_constructor_name)
		{ return known_constructors[module_constructor_name]; }		
	};
	
	class described_module : public module
	{
	protected: 
		// debugging output infrastructure
		srk31::indenting_ostream& debug_out;
								
	public: 
		// NOTE: this class existed to support description other than DWARF...
		// ... which has now been abandoned. So it is now redundant.
				
		described_module(const std::string& filename) 
		 : 	module(filename), debug_out(srk31::indenting_cerr) {}
	};
	
	
	/* This is a simple class which opens a std::ifstream. We use it so that
	 * we can apply the RAII style to base classes whose constructor 
	 * requires an open file descriptor. By putting this as the first base class,
	 * we can /acquire/ the file descriptor before the other base is initialised. */
	class ifstream_holder 
	{ 
		std::ifstream this_ifstream;
	public:
		ifstream_holder(std::string& filename);
		int fileno() { return ::fileno(this_ifstream);  }
	};

	class module_described_by_dwarf 
	:	public described_module,  
		public boost::enable_shared_from_this<module_described_by_dwarf>
	{
	protected:
		dwarf::encap::dieset& dies;
		Dwarf_Off m_greatest_preexisting_offset;
		Dwarf_Off private_offsets_next;
		Dwarf_Off next_private_offset() { return private_offsets_next++; }
		virtual const dwarf::spec::abstract_def& get_spec() = 0;
	public: 
		//typedef shared_ptr<basic_die> (*name_resolver)(
		//	const definite_member_name& mn
		//);
		struct name_resolver_t
		{
			virtual shared_ptr<spec::basic_die> 
			resolve(const definite_member_name& mn) = 0;
		};
		typedef name_resolver_t *name_resolver_ptr;
		
		typedef bool (module_described_by_dwarf::* eval_event_handler_t)(
			Tree *, 
			shared_ptr<basic_die>,
			Tree *,
			name_resolver_ptr);
		virtual bool do_nothing_handler(Tree *falsifiable, 
			shared_ptr<basic_die> falsifier, Tree *missing, name_resolver_ptr p_resolver);
		virtual bool check_handler(Tree *falsifiable,
			shared_ptr<basic_die> falsifier, Tree *missing, name_resolver_ptr p_resolver);
		virtual bool declare_handler(Tree *falsifiable,
			shared_ptr<basic_die> falsifier, Tree *missing, name_resolver_ptr p_resolver);
		virtual bool override_handler(Tree *falsifiable, 
			shared_ptr<basic_die> falsifier, Tree *missing, name_resolver_ptr p_resolver);
		virtual bool internal_check_handler(Tree *falsifiable, 
			shared_ptr<basic_die> falsifier, Tree *missing, name_resolver_ptr p_resolver);
		virtual eval_event_handler_t handler_for_claim_strength(antlr::tree::Tree *strength);

		shared_ptr<basic_die>
		ensure_non_toplevel_falsifier(
			Tree *falsifiable, 
			shared_ptr<basic_die> falsifier
		) const;

		void process_claimgroup(Tree *claimGroup);
		void process_exists_claims(antlr::tree::Tree *existsBody);
		void process_supplementary_claim(antlr::tree::Tree *claimGroup);


		bool eval_claim_depthfirst(
			antlr::tree::Tree *claim, 
			shared_ptr<spec::basic_die> p_d, 
			name_resolver_ptr p_resolver,
			eval_event_handler_t handler);
		// specialised claim handlers
		bool eval_claim_for_subprogram_and_FUNCTION_ARROW(
			antlr::tree::Tree *claim, 
			shared_ptr<spec::subprogram_die> p_d, 
			name_resolver_ptr p_resolver,
			eval_event_handler_t handler);
		bool eval_claim_for_with_named_children_and_MEMBERSHIP_CLAIM(
			antlr::tree::Tree *claim, 
			shared_ptr<spec::with_named_children_die> p_d, 
			name_resolver_ptr p_resolver,
			eval_event_handler_t handler);
		
		virtual dwarf::spec::abstract_dieset& get_ds() = 0;
		
		Dwarf_Off greatest_preexisting_offset() const 
		{ return m_greatest_preexisting_offset; }

		module_described_by_dwarf(const std::string& filename, dwarf::encap::dieset& ds) 
		 : 	described_module(filename), dies(ds),
			m_greatest_preexisting_offset(
				(dies.map_end() == dies.map_begin()) ? 0UL : (--dies.map_end())->first),
			private_offsets_next(m_greatest_preexisting_offset + 1) {}
		
	protected:
		std::set<Dwarf_Off> touched_dies;
		void updated_dwarf()
		{
			m_greatest_preexisting_offset = 
				(dies.map_end() == dies.map_begin()) ? 0UL : (--dies.map_end())->first;
			private_offsets_next = m_greatest_preexisting_offset + 1;
		}
		
	public:
		vector< shared_ptr<type_die> > 
		all_existing_dwarf_types(antlr::tree::Tree *t);
		
		shared_ptr<type_die> existing_dwarf_type(antlr::tree::Tree *t);
		shared_ptr<type_die> create_dwarf_type(antlr::tree::Tree *t);
		shared_ptr<type_die> ensure_dwarf_type(antlr::tree::Tree *t);
		shared_ptr<type_die> create_typedef(shared_ptr<type_die> p_d, const string& name);
		
		shared_ptr<spec::structure_type_die> create_empty_structure_type(const string& name);
		shared_ptr<spec::reference_type_die>
		ensure_reference_type_with_target(shared_ptr<spec::type_die> t);
		shared_ptr<spec::reference_type_die>
		create_reference_type_with_target(shared_ptr<spec::type_die> t);
		shared_ptr<spec::pointer_type_die>
		ensure_pointer_type_with_target(shared_ptr<spec::type_die> t);
		shared_ptr<spec::pointer_type_die>
		create_pointer_type_with_target(shared_ptr<spec::type_die> t);
	};

	class elf_module : 	private ifstream_holder, 
						private dwarf::encap::file,
						public module_described_by_dwarf
	{
		boost::shared_ptr<std::ifstream> input_stream;
							
	protected:
		static const dwarf::encap::die_off_list empty_child_list;
		static const dwarf::encap::die::attribute_map empty_attribute_map;
		
		static const dwarf::encap::die::attribute_map::value_type default_subprogram_attr_entries[];
		static const dwarf::encap::die::attribute_map default_subprogram_attributes;

		const dwarf::spec::abstract_def& get_spec() 
		{ return static_cast<dwarf::encap::file*>(this)->get_spec(); }
	
	public:
		elf_module(std::string local_filename, std::string makefile_filename);
		//const dwarf::spec::abstract_def& get_spec() 
		//{ return static_cast<module_described_by_dwarf*>(this)->get_spec(); }
		
		dwarf::spec::abstract_dieset& get_ds() 
		{ return static_cast<dwarf::encap::file*>(this)->get_ds(); }
	
		//void print_abi_info();
	};
	
	class elf_reloc_module : public elf_module
	{
	
	public:
		elf_reloc_module(std::string local_filename, std::string makefile_filename) 
		: elf_module(local_filename, makefile_filename) {}
	};
	
	class elf_external_sharedlib_module : public elf_module
	{

	public:
		elf_external_sharedlib_module(std::string local_filename, std::string libname) 
		: elf_module(local_filename, libname) {}
	};

	class derived_module : private dwarf::encap::dieset, 
	                       public module_described_by_dwarf
	{
		derivation& m_derivation;
	protected:
		const dwarf::spec::abstract_def& get_spec() { return dwarf::spec::dwarf3; }
		const std::string m_id;

	public:
		derived_module(derivation& d, const std::string id, const std::string& filename) 
		:	dwarf::encap::dieset(get_spec()),
			module_described_by_dwarf(filename, *this),
			m_derivation(d),
			m_id(id)
		{}
		dwarf::spec::abstract_dieset& get_ds() { return dies; }
		void updated_dwarf()
		{
			this->module_described_by_dwarf::updated_dwarf();
		}
	};
	
	// HACK: declared in util.hpp
	template <typename Action>
	void
	for_all_identical_types(
		module_ptr p_mod,
		shared_ptr<type_die> p_t,
		const Action& action
	)
	{
		auto opt_ident_path = p_t->ident_path_from_cu();
		vector< shared_ptr<type_die> > ts;
		if (!opt_ident_path) ts.push_back(p_t);
		else
		{
			for (auto i_cu = p_mod->get_ds().toplevel()->compile_unit_children_begin();
				i_cu != p_mod->get_ds().toplevel()->compile_unit_children_end(); ++i_cu)
			{
				auto candidate = (*i_cu)->resolve(opt_ident_path->begin(), opt_ident_path->end());
				if (dynamic_pointer_cast<type_die>(candidate))
				{
					ts.push_back(dynamic_pointer_cast<type_die>(candidate));
				}
			}
		}
		for (auto i_t = ts.begin(); i_t != ts.end(); ++i_t)
		{
			action(*i_t);
		}
	}
}

#endif
