// note: this file is included by cake.hpp

#include <gcj/cni.h>
#include <string>
#include <map>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <fstream>
#include <fileno.hpp>
#include <dwarf.h>
#include <dwarfpp.h>
#include <dwarfpp_simple.hpp>

namespace cake
{
	class module
	{
		std::string filename;

	
		typedef std::pair<const std::string, const std::string> constructor_map_entry;
		static constructor_map_entry known_constructor_extensions[];		
		static std::map<std::string, std::string> known_constructors;
				
	public:
		enum claim_strength { CHECK, DECLARE, OVERRIDE };
		module(std::string& filename) : filename(filename) {}
		std::string& get_filename() { return filename; }
		void process_exists_claims(antlr::tree::Tree *existsBody);
		void process_claimgroup(antlr::tree::Tree *claimGroup);			
		virtual void process_claim_list(claim_strength s, antlr::tree::Tree *claimGroup) = 0;
		
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
		ifstream_holder(std::string& filename) : this_ifstream(filename.c_str(), std::ios::in) 
		{
			if (!this_ifstream) 
			{ 
				throw new SemanticError(
					0, 
					JvNewStringUTF(
						"file does not exist! ")->concat(
						JvNewStringUTF(filename.c_str())));
			}
		}
		int fileno() { return ::fileno(this_ifstream);  }
	};
	
	class elf_module : private ifstream_holder, public module, private dwarf::file
	{
		dwarf::abi_information info;
		boost::shared_ptr<std::ifstream> input_stream;
	
	public:
		elf_module(std::string filename) :
			ifstream_holder(filename),
			module(filename),
			dwarf::file(fileno()),
			info(*this)
		{
			//print_abi_info();
		}
		void process_claim_list(claim_strength s, antlr::tree::Tree *claimGroup);
		
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
