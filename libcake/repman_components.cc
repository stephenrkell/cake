extern "C"
{
	#include "repman.h"
	
	// define the table pointers
	void *components_table;
	void *component_pairs_table;
}
#include "cake/prelude/runtime.hpp"

#include <dwarfpp/spec_adt.hpp>
#include <processimage/process.hpp>

/* process image */
#include <map>
#include <string>
#include <sstream>
#include <processimage/process.hpp>
#include <dwarfpp/spec_adt.hpp>

extern "C"
{
int components_table_inited;
int component_pairs_table_inited;
}

namespace cake {

process_image image(-1);


/* the actual tables -- type-correct aliases */
static std::map<dwarf::spec::abstract_dieset::position, std::string> 
*p_components __attribute__((alias("components_table")));
static std::map<std::pair<std::string, std::string>, conv_table_t *> 
*p_component_pairs __attribute__((alias("component_pairs_table")));
} // end namespace cake

void init_components_table(void)
{
	if (components_table_inited) return;
	
	components_table = new std::map<dwarf::spec::abstract_dieset::position, std::string> ();
	// FIXME: delete this at program end
	
	/* This is not a constructor, because we want it to run *after* all the 
	 * wrapper constructors have run. So we leave it as a "call before first use"
	 * function for the runtime to call by itself. */

	/* Search for symbols of the form __cake_component_<name>, and 
	 * use each to build a table mapping DWARF compile units
	 * to Cake components. In this way, we can select the correct
	 * wrapper conversion table (which are emitted one-per-component-pair).
	 * It indexes these tables by
	 * the canonicalised <fq-name> data type identifiers. (NOTE: we've ditched
	 * the use of comp-dir and producer-string in these tables! Recovering
	 * the Cake component, and assuming well-matchedness in here, is enough.) 
	 * The table is a hash_map<cu-position, component-name*> *
	 * where a conversions-table*/
	using namespace cake;	
	
	for (auto i_file = image.files.begin(); i_file != image.files.end(); i_file++)
	{
		auto symbols = image.all_symbols(i_file);
		std::cerr << "Looking for Cake component definitions in symbols of file " << i_file->first
			<< std::endl;
		for (auto i_sym = symbols.first; i_sym != symbols.second; i_sym++)
		{
			const char *strptr = elf_strptr(i_sym.base().origin->elf,
				i_sym.base().origin->shdr.sh_link, 
				(size_t)i_sym->st_name);
			assert(strptr);
			std::string name(strptr);
			
			std::string::size_type off;
			if ((off = name.find("__cake_component_")) == 0)
			{
				std::cerr << "Found Cake component definition: " << name << std::endl;
				/* Found it! Grab the component name. */
				std::string component_name = name.substr(off);
				/* Now grab the string. */
				process_image::addr_t base = image.get_dieset_base(*i_file->second.p_ds);
				std::string sym_data(*reinterpret_cast<const char**>((char*)base + i_sym->st_value));
					
				/* Now populate this component's compile units list 
				 * with the strings we found. */
				std::cerr << "Symbol data is " << sym_data << std::endl;
				
				// HACK: make this robust
				std::string::size_type caret_pos1 = sym_data.find('^');
				assert(caret_pos1 == 0);
				std::string::size_type caret_pos2 = sym_data.find('^', caret_pos1 + 1);
				std::string::size_type caret_pos3 = sym_data.find('^', caret_pos2 + 1);
				assert(caret_pos2 > caret_pos1 && caret_pos3 > caret_pos2);
				
				std::string source_fname = sym_data.substr(caret_pos1 + 1, caret_pos2 - caret_pos1 - 1);
				std::string compile_dir = sym_data.substr(caret_pos2 + 1, caret_pos3 - caret_pos2 - 1);
				std::string producer = sym_data.substr(caret_pos3 + 1, sym_data.size() - caret_pos3 - 2);
				
				/* Search the file containing this component 
				 * for a CU that matches. */
				bool success = false;
				for (auto i_cu = i_file->second.p_ds->toplevel()->compile_unit_children_begin();
					i_cu != i_file->second.p_ds->toplevel()->compile_unit_children_end();
					i_cu++)
				{
					if (
						(*i_cu)->get_name() && *(*i_cu)->get_name() == source_fname
					&&  (*i_cu)->get_comp_dir() && *(*i_cu)->get_comp_dir() == compile_dir
					&&  (*i_cu)->get_producer() && *(*i_cu)->get_producer() == producer
					)
					{
						(*p_components)[i_cu.base().base().base()] = component_name;
						success = true;
						break;
					}
				}
				assert(success);
				
			}
		}
	}
	
	components_table_inited = 1;
}

void init_component_pairs_table(void)
{
	using namespace cake;

	if (component_pairs_table_inited) return;
	
	component_pairs_table = new std::map<std::pair<std::string, std::string>, conv_table_t *> ();
	// FIXME: delete this
	
	/* We're building a table of component pairs, pointing to their conversion table. */
	for (auto i_file = image.files.begin(); i_file != image.files.end(); i_file++)
	{
		auto symbols = image.symbols(i_file);
		for (auto i_sym = symbols.first; i_sym != symbols.second; i_sym++)
		{
			std::string name = elf_strptr(i_sym.origin->elf,
				i_sym.origin->shdr.sh_link, 
				(size_t)i_sym->st_name);
			
			std::string::size_type off;
			if ((off = name.find("__cake_componentpair_")) == 0)
			{
				/* Found it! Grab the component names. */
				std::string component_names_string = name.substr(off);
				
				// HMM: how to split apart the pair? use a length-prefixing thing
				std::istringstream s(component_names_string);
				int len1;
				s >> len1;
				std::string c1;
				while (len1 > 0)
				{
					char c;
					s >> c;
					c1 += c;
					len1--;
				}
				int len2;
				s >> len2;
				std::string c2;
				while (len2 > 0)
				{
					char c;
					s >> c;
					c2 += c;
					len2--;
				}
				
				// FIXME: more here
				std::cerr << "Discovered component pair (" << c1 << ", " << c2 << ")" << std::endl;
			}
		}
	}
	
	component_pairs_table_inited = 1;
}
