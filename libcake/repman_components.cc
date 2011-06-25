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

namespace cake {

process_image image(-1);

/* the actual tables -- type-correct aliases */
static std::map<dwarf::spec::abstract_dieset::position, std::string> 
*p_components __attribute__((alias("components_table")));
static std::map<std::pair<std::string, std::string>, conv_table_t *> 
*p_component_pairs __attribute__((alias("component_pairs_table")));

void init_components_table(void)
{
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
	
	for (auto i_file = image.files.begin(); i_file != image.files.end(); i_file++)
	{
		auto symbols = image.symbols(i_file);
		for (auto i_sym = symbols.first; i_sym != symbols.second; i_sym++)
		{
			std::string name = elf_strptr(i_sym.origin->elf,
				i_sym.origin->shdr.sh_link, 
				(size_t)i_sym->st_name);
			
			std::string::size_type off;
			if ((off = name.find("__cake_component_")) == 0)
			{
				/* Found it! Grab the component name. */
				std::string component_name = name.substr(off);
				/* Now grab the string. */
				process_image::addr_t base = image.get_dieset_base(*i_file->second.p_ds);
				const char *sym_data = reinterpret_cast<const char*>(base) + i_sym->st_value;
					
				/* Now populate this component's compile units list 
				 * with the strings we found. */
				char *source_fname;
				char *compile_dir;
				char *producer;
				int ret = sscanf(sym_data, "^%as^%as^%as^", 
					&source_fname, &compile_dir, &producer);
					
				if (ret > 0)
				{
					// success -- assume full success now, for simplicit
					assert(ret == 3);
					
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
					
					if (ret >= 1) free(source_fname);
					if (ret >= 2) free(compile_dir);
					if (ret >= 3) free(producer);
				}
				else
				{
					// failed
					assert(false);
				}
				
			}
		}
	}

}

void init_component_pairs_table(void)
{
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
			if ((off = name.find("__cake_component_pair_")) == 0)
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
				}
				int len2;
				s >> len2;
				std::string c2;
				while (len2 > 0)
				{
					char c;
					s >> c;
					c2 += c;
				}
				
				// FIXME: more here
			}
		}
	}
}

} // end namespace cake
