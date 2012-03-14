extern "C"
{
	#include "repman.h"
	
	// define the table pointers
	void *components_table;
	void *component_pairs_table;
}
#include "cake/cxx_target.hpp"
#include "cake/prelude/runtime.hpp"

#include <dwarfpp/spec_adt.hpp>
#include <dwarfpp/cxx_model.hpp>
#include <libreflect.hpp>
#include <map>
#include <string>
#include <sstream>

extern "C"
{
int components_table_inited;
int component_pairs_table_inited;
}

namespace cake {

//process_image image(-1); // now in libreflect
cake_cxx_target compiler;

}

using boost::dynamic_pointer_cast;
using boost::shared_ptr;
using std::string;
using std::vector;
using dwarf::spec::compile_unit_die;
using dwarf::spec::type_die;
using pmirror::process_image;
using pmirror::self;

/* the actual tables -- type-correct aliases */
typedef std::map<dwarf::spec::abstract_dieset::position, int> components_table_t;
static components_table_t *p_components __attribute__((alias("components_table")));

struct component_pair_tables
{
	cake::conv_table_t *convs_from_first_to_second;
	cake::conv_table_t *convs_from_second_to_first;
	cake::init_table_t *inits_from_first_to_second;
	cake::init_table_t *inits_from_second_to_first;
};
	
typedef std::map<
	std::pair<int, int>, 
	component_pair_tables
> component_pairs_table_t;
static component_pairs_table_t *p_component_pairs __attribute__((alias("component_pairs_table")));

void init_components_table(void)
{
	if (components_table_inited) return;
	
	p_components = new components_table_t ();
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
	
	self.update();
	for (auto i_file = self.files.begin(); i_file != self.files.end(); ++i_file)
	{
		auto symbols = self.all_symbols(i_file);
		std::cerr << "Looking for Cake component definitions in symbols of file " << i_file->first
			<< std::endl;
		for (auto i_sym = symbols.first; i_sym != symbols.second; ++i_sym)
		{
			const char *strptr = elf_strptr(i_sym.base().origin->elf,
				i_sym.base().origin->shdr.sh_link, 
				(size_t)i_sym->st_name);
			assert(strptr);
			std::string name(strptr);
			
			std::string::size_type off;
			if (name.find("__cake_component_") == 0)
			{
				std::cerr << "Found Cake component definition: " << name << std::endl;
				/* Found it! Grab the component name. */
				std::string component_name = name.substr(std::string("__cake_component_").length());
				/* Now grab the string. */
				process_image::addr_t base = self.get_dieset_base(*i_file->second.p_ds);
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
					++i_cu)
				{
					if (
					    (*i_cu)->get_name() && *(*i_cu)->get_name() == source_fname
					&&  (*i_cu)->get_comp_dir() && *(*i_cu)->get_comp_dir() == compile_dir
					&&  (*i_cu)->get_producer() && *(*i_cu)->get_producer() == producer
					)
					{
						int component_rep = get_rep_for_component_name(
							component_name.c_str());
						assert(component_rep != -1);
						(*p_components)[i_cu.base().base().base()] = component_rep;
						success = true;
						break;
					}
					else
					{
						std::cerr << "Skipping CU: " << **i_cu << std::endl;
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
	
	p_component_pairs = new component_pairs_table_t ();
	// FIXME: delete this object
	
	/* We're building a table of component pairs, pointing to their conversion table. */
	self.update();
	for (auto i_file = self.files.begin(); i_file != self.files.end(); ++i_file)
	{
		auto symbols = self.all_symbols(i_file);
		for (auto i_sym = symbols.first; i_sym != symbols.second; ++i_sym)
		{
			std::string name = elf_strptr(i_sym.base().origin->elf,
				i_sym.base().origin->shdr.sh_link, 
				(size_t)i_sym->st_name);
			
			std::string::size_type off;
			if ((off = name.find("__cake_componentpair_")) == 0)
			{
				/* Found it! Grab the component names. */
				std::string component_names_string = name.substr(
					std::string("__cake_componentpair_").length());
				
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
				char underscore;
				s >> underscore;
				assert(underscore == '_');
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
				
				auto reps_pair = std::make_pair(
					get_rep_for_component_name(c1.c_str()),
					get_rep_for_component_name(c2.c_str()));
				
				// this may be a pair we've already seen
				if (p_component_pairs->find(reps_pair) != p_component_pairs->end())
				{
					continue;
				}
				
				std::cerr << "Discovered component pair (" << c1 << ", " << c2 << ")" << std::endl;
				std::string remainder;
				s >> remainder;
				std::string other_table_symname;
				bool first_is_first_to_second;
				if (remainder == "_first_to_second")
				{
					first_is_first_to_second = true;
					other_table_symname = name.replace(
						name.find("_first_to_second"), std::string("_first_to_second").length(),
						"_second_to_first");
				}
				else if (remainder == "_second_to_first")
				{
					first_is_first_to_second = false;
					other_table_symname = name.replace(
						name.find("_second_to_first"), std::string("_second_to_first").length(),
						"_first_to_second");
				}
				else assert(false);

				// search for this symbols, then we can terminate
				process_image::addr_t base = self.get_dieset_base(*i_file->second.p_ds);
				for (auto i_othersym = i_sym; i_othersym != symbols.second; ++i_othersym)
				{
					std::string name = elf_strptr(i_othersym.base().origin->elf,
						i_othersym.base().origin->shdr.sh_link, 
						(size_t)i_othersym->st_name);
					if (name == other_table_symname)
					{
						// now we can build the table entry
						(*p_component_pairs)[std::make_pair(
							get_rep_for_component_name(c1.c_str()), 
							get_rep_for_component_name(c2.c_str())
						)] = 
						(component_pair_tables) {
								(conv_table_t*)reinterpret_cast<void**>((char*)base 
									+ (first_is_first_to_second ? i_sym : i_othersym)->st_value)[0],
								(conv_table_t*)reinterpret_cast<void**>((char*)base 
									+ (first_is_first_to_second ? i_othersym : i_sym)->st_value)[0],
								(init_table_t*)reinterpret_cast<void**>((char*)base 
									+ (first_is_first_to_second ? i_sym : i_othersym)->st_value)[1],
								(init_table_t*)reinterpret_cast<void**>((char*)base 
									+ (first_is_first_to_second ? i_othersym : i_sym)->st_value)[1]
						};
						break;
					}
				}
				assert(p_component_pairs->find(reps_pair) != p_component_pairs->end());
			}
		}
	}
	
	component_pairs_table_inited = 1;
}

static cake::init_table_t::mapped_type *
get_init_table_entry(void *obj, int obj_rep, int co_obj_rep)
{
	using namespace cake;
	
	auto discovered = self.discover_object_descr((process_image::addr_t) obj);
	
	component_pairs_table_t::iterator i;
	init_table_t *p_table;
	bool from_first_to_second;
	if ((i = p_component_pairs->find(std::make_pair(obj_rep, co_obj_rep))) != 
		p_component_pairs->end())
	{
		p_table = i->second.inits_from_first_to_second;
		from_first_to_second = true;
	}
	else if ((i = p_component_pairs->find(std::make_pair(co_obj_rep, obj_rep))) != 
		p_component_pairs->end())
	{
		p_table = i->second.inits_from_second_to_first;
		from_first_to_second = false;
	}
	else assert(false);
	
	assert(discovered);

	auto key = (const init_table_key) {
			cake::compiler.fq_name_parts_for(discovered),
			from_first_to_second
		};
	
	if (p_table->find(key) != p_table->end()) return &(*p_table)[key];
	else return 0;
}	

size_t get_co_object_size(void *obj, int obj_rep, int co_obj_rep)
{
	auto init_tbl_ent = get_init_table_entry(obj, obj_rep, co_obj_rep);
	if (init_tbl_ent) return init_tbl_ent->to_size;
	else return 0; // use a zero-size co-object for uncorresponded types
}

/* This is for co-objects that we have allocated. Their allocation sites are
 * no good for discovery, so we inform process_image of their type explicitly. */
void set_co_object_type(void *object, int obj_rep, void *co_object, int co_obj_rep)
{
	auto init_tbl_ent = get_init_table_entry(object, obj_rep, co_obj_rep);
	vector<string> name_parts;
	if (init_tbl_ent) name_parts = init_tbl_ent->to_typename;
	
	/* Look through the CUs to find a definition of this typename. */
	std::cerr << "Searching component CU map of " << p_components->size() 
		<< " entries." << std::endl;
	for (auto i_component_entry = p_components->begin(); 
		i_component_entry != p_components->end();
		++i_component_entry)
	{
		if (i_component_entry->second == co_obj_rep)
		{
			// found a CU of this component
			auto cu_pos = i_component_entry->first;
			auto cu = dynamic_pointer_cast<compile_unit_die>(
				(*cu_pos.p_ds)[cu_pos.off]);

			shared_ptr<type_die> found;
			if (name_parts.size() > 0)
			{
				auto found_basic = cu->resolve(name_parts.begin(), name_parts.end());
				found = dynamic_pointer_cast<type_die>(found_basic);

				// if we find it, it should be a type
				assert((found_basic && found) || (!found_basic && !found));
			}
				
			// if we didn't get an init table entry, it means "void" -- still okay
			pmirror::self.inform_heap_object_descr(
				(process_image::addr_t) co_object, found);
				
			return;
		}
	}
	
	assert(false);

}

conv_func_t get_rep_conv_func(int from_rep, int to_rep, void *source_object, void *target_object)
{
	using pmirror::self;
	
	auto discovered_source = self.discover_object_descr((process_image::addr_t) source_object);
	auto discovered_target = self.discover_object_descr((process_image::addr_t) target_object);
	
	auto discovered_source_type = dynamic_pointer_cast<type_die>(discovered_source);
	assert((discovered_source && discovered_source_type) || (!discovered_source && !discovered_source_type));
	auto discovered_target_type = dynamic_pointer_cast<type_die>(discovered_target);
	assert((discovered_target && discovered_target_type) || (!discovered_target && !discovered_target_type));
	
	/* We should assert that these two objects are in the same
	 * co-object group. */
	
	std::cerr << "Getting rep conv func from object: ";
	self.print_object(std::cerr, source_object);
	if (discovered_source) std::cerr << *discovered_source;
	else std::cerr << "(assumed void type)";
	std::cerr << " to object: ";
	self.print_object(std::cerr, target_object);
	if (discovered_target) std::cerr << *discovered_target;
	else std::cerr << "(assumed void type)";
	std::cerr << std::endl;
	
	bool from_first_to_second = 
		(p_component_pairs->find(std::make_pair(from_rep, to_rep)) 
			!= p_component_pairs->end());
			
	auto component_pair = from_first_to_second 
		? std::make_pair(from_rep, to_rep) : std::make_pair(to_rep, from_rep);
	
	auto found_table = p_component_pairs->find(component_pair);
	assert(found_table != p_component_pairs->end());
	
	auto convs_table = from_first_to_second ? 
		found_table->second.convs_from_first_to_second
		: found_table->second.convs_from_second_to_first;
	
	if (discovered_source_type && discovered_target_type)
	{
		auto found = convs_table->find(
			(cake::conv_table_key) {
				cake::compiler.name_parts_for(discovered_source_type),
				cake::compiler.name_parts_for(discovered_target_type),
				from_first_to_second,
				0 // FIXME
				});
		assert(found != convs_table->end());
		return found->second.func;
	}
	else return noop_conv_func;
}

conv_func_t get_init_func(int from_rep, int to_rep, void *source_object, void *target_object)
{
	using pmirror::self;
	
	auto discovered_source = self.discover_object_descr((process_image::addr_t) source_object);
	auto discovered_source_type = dynamic_pointer_cast<type_die>(discovered_source);
	assert(discovered_source_type);
	
	/* We should assert that these two objects are in the same
	 * co-object group. */
	
	std::cerr << "Getting init func from object: ";
	self.print_object(std::cerr, source_object);
	std::cerr << discovered_source->summary()
		<< " to object: ";
	self.print_object(std::cerr, target_object);
	std::cerr << " (NEW INIT)"
		<< std::endl;
	
	bool from_first_to_second = 
		(p_component_pairs->find(std::make_pair(from_rep, to_rep)) 
			!= p_component_pairs->end());
			
	auto component_pair = from_first_to_second 
		? std::make_pair(from_rep, to_rep) : std::make_pair(to_rep, from_rep);
	
	auto found_table = p_component_pairs->find(component_pair);
	assert(found_table != p_component_pairs->end());
		
	auto init_table = from_first_to_second ? 
		found_table->second.inits_from_first_to_second
		: found_table->second.inits_from_second_to_first;
	
	auto found = init_table->find(
		(cake::init_table_key) {
			cake::compiler.name_parts_for(discovered_source_type),
			from_first_to_second});
			
	if (found != init_table->end()) return found->second.func;
	else return noop_conv_func;
}

void *noop_conv_func(void *arg1, void *arg2)
{
	return arg2;
}
