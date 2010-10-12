#ifndef __CAKE_MODULE_HPP
#define __CAKE_MODULE_HPP

#include <string>
#include <map>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include <fileno.hpp>
#include <dwarfpp/lib.hpp>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/encap_adt.hpp>
#include <indenting_ostream.hpp>
#include "parser.hpp"

using namespace dwarf::lib;

namespace antlr { namespace tree { typedef ANTLR3_BASE_TREE Tree; } }

namespace cake
{
	class definite_member_name;
    class derivation;
	class module
	{
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
    	// FIXME: make some of the below private
        // FIXME: DWARF-agnostify the interface
		typedef bool (cake::described_module::* eval_event_handler_t)(antlr::tree::Tree *, Dwarf_Off);
        
		virtual bool do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual bool check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual bool declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual bool override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual	bool internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual eval_event_handler_t handler_for_claim_strength(antlr::tree::Tree *strength)
        {
		return
			GET_TYPE(strength) == CAKE_TOKEN(KEYWORD_CHECK) 	? &cake::described_module::check_handler
		: 	GET_TYPE(strength) == CAKE_TOKEN(KEYWORD_DECLARE) 	? &cake::described_module::declare_handler
		: 	GET_TYPE(strength) == CAKE_TOKEN(KEYWORD_OVERRIDE) ? &cake::described_module::override_handler : 0;
        }
		virtual bool eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
			unsigned long long current_position) = 0;
		void process_exists_claims(antlr::tree::Tree *existsBody);
		void process_supplementary_claim(antlr::tree::Tree *claimGroup);
		virtual void process_claimgroup(antlr::tree::Tree *claimGroup) = 0;
				
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
    : public described_module,  
      public boost::enable_shared_from_this<module_described_by_dwarf>
	{
    protected:
		static const Dwarf_Off private_offsets_begin; 
		dwarf::encap::dieset& dies;
		Dwarf_Off private_offsets_next;
		Dwarf_Off next_private_offset() { return private_offsets_next++; }
        virtual const dwarf::spec::abstract_def& get_spec() = 0;
	public: 
        boost::shared_ptr<module_described_by_dwarf> shared_this() { return this->shared_from_this(); }
    	dwarf::encap::Die_encap_all_compile_units& all_compile_units() 
        { return dies.all_compile_units(); }
		bool do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);

		Dwarf_Off ensure_non_toplevel_falsifier(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) const;

		void process_claimgroup(antlr::tree::Tree *claimGroup);

		bool eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
			Dwarf_Off current_die);
        
        virtual dwarf::spec::abstract_dieset& get_ds() = 0;
        
        module_described_by_dwarf(const std::string& filename, dwarf::encap::dieset& ds) 
         : 	described_module(filename), dies(ds),
         	private_offsets_next(private_offsets_begin) {}
	};

	class elf_module : 	private ifstream_holder, 
    					public module_described_by_dwarf, 
                        private dwarf::encap::file
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
    
    class derived_module : public module_described_by_dwarf
    {
    	// here we actually instantiate a dieset; our base class just keeps a reference
    	dwarf::encap::dieset dies;
    	derivation& m_derivation;
    protected:
    	const dwarf::spec::abstract_def& get_spec() { return dwarf::spec::dwarf3; }
        const std::string m_id;
        
    public:
    	derived_module(derivation& d, const std::string id, const std::string& filename) 
         :	module_described_by_dwarf(filename, dies),
         	dies(get_spec()),
            m_derivation(d),
            m_id(id)
         	{}
	    dwarf::spec::abstract_dieset& get_ds() { return dies; }
    };
}

#endif
