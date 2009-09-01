// note: this file is included by cake.hpp

#include <string>
#include <map>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <fstream>
#include <fileno.hpp>
#include <dwarf.h>
#include <dwarfpp.h>
#include <dwarfpp_simple.hpp>
#include <dwarfpp_util.hpp>
#include <boost/iostreams/concepts.hpp>    // input_filter
#include <boost/iostreams/operations.hpp>  // get()
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

namespace cake
{
	class definite_member_name;
	class module
	{
		std::string filename;
	
		typedef std::pair<const std::string, const std::string> constructor_map_entry;
		static constructor_map_entry known_constructor_extensions[];		
		static std::map<std::string, std::string> known_constructors;
		
	protected: // debugging output infrastructure
		struct newline_tabbing_filter : boost::iostreams::output_filter {
			static int indent_level; // HACK: make static for now!
			newline_tabbing_filter() /*: indent_level(0)*/ {}
    		template<typename Sink>
    		bool put_char(Sink& dest, int c)
    		{
        		if (!boost::iostreams::put(dest, c)) return false;
        		if (c == '\n')
				{
					for (int i = indent_level; i > 0; i--) 
					{
						if (!boost::iostreams::put(dest, '\t')) return false;
					}
				}
        		return true;
    		}
			template<typename Sink>
    		bool put(Sink& dest, int c) 
    		{
        		return put_char(dest, c);
    		}
		};
		newline_tabbing_filter debug_out_filter;
		boost::iostreams::filtering_ostreambuf debug_outbuf;
		std::ostream debug_out;
								
	public: // FIXME: make some of the below private
		typedef bool (cake::module::* eval_event_handler_t)(antlr::tree::Tree *, Dwarf_Off);
		virtual bool do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual bool check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual bool declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		virtual bool override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
		//virtual bool build_value_description_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
			// FIXME: build_value_description_handler doesn't really belong here, but pointer-to-member
			// type-checking rules demand that it is here. Work out a more satisfactory solution.
		virtual	bool internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) = 0;
				
		module(std::string& filename);
		std::string& get_filename() { return filename; }
		void process_exists_claims(antlr::tree::Tree *existsBody);
		void process_supplementary_claim(antlr::tree::Tree *claimGroup);
		void process_claimgroup(antlr::tree::Tree *claimGroup);
		virtual eval_event_handler_t handler_for_claim_strength(antlr::tree::Tree *strength) = 0;
		virtual bool eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
			unsigned long long current_position) = 0;
		static std::string extension_for_constructor(std::string& module_constructor_name)
		{ return known_constructors[module_constructor_name]; }		
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
	
	class elf_module : private ifstream_holder, public module, private dwarf::file
	{
		dwarf::abi_information info;
		dwarf::dieset& dies; // = info.get_dies();
		boost::shared_ptr<std::ifstream> input_stream;
		
		static const Dwarf_Off private_offsets_begin; 
		Dwarf_Off private_offsets_next;
		Dwarf_Off next_private_offset() { return private_offsets_next++; }
		
		bool do_nothing_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool declare_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool override_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		//virtual bool build_value_description_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);
		bool internal_check_handler(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier);

		void debug_print_artificial_dies();
		boost::optional<Dwarf_Off> find_immediate_container(const definite_member_name& mn, 
			Dwarf_Off context) const;
		Dwarf_Off ensure_non_toplevel_falsifier(antlr::tree::Tree *falsifiable, Dwarf_Off falsifier) const;
		//virtual dwarf::encap::die::attribute_map default_subprogram_attributes();
		boost::optional<Dwarf_Off> find_containing_cu(Dwarf_Off context);
		Dwarf_Off follow_typedefs(Dwarf_Off off);
		boost::optional<Dwarf_Off> find_nearest_containing_die_having_tag(Dwarf_Off context, Dwarf_Half tag);		
		boost::optional<Dwarf_Off> find_nearest_type_named(Dwarf_Off context, const char *name);
		Dwarf_Off create_new_die(const Dwarf_Off parent, const Dwarf_Half tag, 
			const dwarf::encap::die::attribute_map& attrs, const dwarf::die_off_list& children);		
		Dwarf_Off create_dwarf_type_from_value_description(antlr::tree::Tree *valueDescription, 
			Dwarf_Off context, boost::optional<std::string> name);
		void build_subprogram_die_children(antlr::tree::Tree *valueDescriptionExpr, Dwarf_Off subprogram_die_off);

		virtual Dwarf_Unsigned make_default_dwarf_location_expression_for_arg(int argn);
		Dwarf_Off ensure_dwarf_type(antlr::tree::Tree *description, 
			Dwarf_Off context, boost::optional<std::string> name);
		dwarf::die_off_list *find_dwarf_types_satisfying(antlr::tree::Tree *description,
			dwarf::die_off_list& list_to_search);
		bool dwarf_type_satisfies(antlr::tree::Tree *description, Dwarf_Off type_offset);
		bool dwarf_subprogram_satisfies_description(Dwarf_Off subprogram_offset, antlr::tree::Tree *description);
		bool dwarf_arguments_satisfy_description(Dwarf_Off subprogram_offset, antlr::tree::Tree *description);
		bool dwarf_variable_satisfies_description(Dwarf_Off variable_offset, antlr::tree::Tree *description);
		dwarf::die_off_list *find_dwarf_type_named(antlr::tree::Tree *ident, Dwarf_Off context);
		boost::optional<std::string> type_name_from_value_description(antlr::tree::Tree *);
		
		eval_event_handler_t handler_for_claim_strength(antlr::tree::Tree *strength);
	
		bool eval_claim_depthfirst(antlr::tree::Tree *claim, eval_event_handler_t handler,
			Dwarf_Off current_die);
	
	protected:
		static const dwarf::die_off_list empty_child_list;
		static const dwarf::encap::die::attribute_map empty_attribute_map;
		
		static const dwarf::encap::die::attribute_map::value_type default_subprogram_attr_entries[];
		static const dwarf::encap::die::attribute_map default_subprogram_attributes;
	
	public:
		elf_module(std::string filename);
	
		//virtual void make_default_subprogram(dwarf::encap::die &die_to_modify);
	
		void print_abi_info();
	};
	
	class elf_reloc_module : public elf_module
	{
	
	public:
		elf_reloc_module(std::string filename) : elf_module(filename) {}
		 
	};
	
	class elf_external_sharedlib_module : public elf_module
	{

	public:
		elf_external_sharedlib_module(std::string filename) : elf_module(filename) {}
	};
}
