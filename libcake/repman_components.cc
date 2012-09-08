extern "C"
{
	#include "repman.h"
	
	// define the table pointers
	void *components_table;
	void *component_pairs_table;
	
	// we need this function, whether or not it's in malloc.h
	unsigned long malloc_usable_size(void *obj);
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
using std::shared_ptr;
using std::string;
using std::vector;
using std::cerr;
using std::endl;
using dwarf::spec::compile_unit_die;
using dwarf::spec::type_die;
using pmirror::process_image;
using pmirror::self;
using namespace dwarf;

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

// helper
static void 
decode_cake_component_string(
	const string&, vector<string>&, vector<string>& compile_dirs,  vector<string>&
);

static string 
name_parts_to_string(
	const vector<string>& name_parts
);
static string 
name_parts_to_string(
	const vector<string>& name_parts
)
{
	std::ostringstream s;
	for (auto i = name_parts.begin(); i != name_parts.end(); ++i)
	{
		if (i != name_parts.begin()) s << " :: ";
		s << *i;
	}
	return s.str();
}

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
		auto symbols = self./*all_*/static_symbols(i_file);
		std::cerr << "Looking for Cake component definitions in symbols of file " << i_file->first
			<< std::endl;
		for (auto i_sym = symbols.first; i_sym != symbols.second; ++i_sym)
		{
			Elf *elf = i_sym.base().origin->elf;
			pmirror::symbols_iteration_state *origin = i_sym.base().origin.get();
			size_t strtab_scndx = origin->shdr.sh_link;
			size_t offset_in_strtab = (size_t)i_sym->st_name;
			const char *strptr = elf_strptr(elf,
				strtab_scndx, 
				offset_in_strtab);
			if (!strptr)
			{
				/* null symbol name -- continue */
				continue;
			}
			std::string name(strptr);
			//cerr << "Considering symbol " << name << endl;
			
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
				
				vector<string> source_fnames;
				vector<string> compile_dirs;
				vector<string> producers;
				decode_cake_component_string(sym_data, source_fnames, compile_dirs, producers);
				// we should get some stuff out
				assert(source_fnames.size() > 0);
				
				/* For each CU in the current file, test whether it 
				 * is an element of this component,
				 * and if so, put it in the relevant place in the components table. */
				vector<bool> matched;
				for (auto i_cu = i_file->second.p_ds->toplevel()->compile_unit_children_begin();
					i_cu != i_file->second.p_ds->toplevel()->compile_unit_children_end();
					++i_cu)
				{
					if (!(*i_cu)->get_name()
					||  !(*i_cu)->get_producer())
					{
						std::cerr << "Skipping CU as it has no source filename and/or producer: " 
							<< **i_cu << std::endl;
						continue;
					}
					
					string cu_source_fname = *(*i_cu)->get_name();
					string cu_producer = *(*i_cu)->get_producer();
					string cu_compile_dir = ((*i_cu)->get_comp_dir() 
						? *(*i_cu)->get_comp_dir()
						: "(unknown directory)");
					
					/* For each CU we decoded from the component string, try to match
					 * it against this CU. */
					
					for (unsigned i = 0; i < source_fnames.size(); ++i)
					{
						if (cu_source_fname == source_fnames[i]
						&&  cu_compile_dir == compile_dirs[i]
						&&  cu_producer == producers[i])
						{
							int component_rep = get_rep_for_component_name(
								component_name.c_str());
							assert(component_rep != -1);
							(*p_components)[i_cu.base().base().base()] = component_rep;
							cerr << "Added CU " << (**i_cu) << " to components table with rep "
								<< component_rep << endl;
							matched.resize(i+1);
							matched[i] = true;
							break;
						}
					}
				}
				/* To succeed, we should have matched every element in the decoded
				 * component string with a CU. */
				bool success = true;
				auto reducer = [&success](bool arg) { success &= arg; };
				std::for_each(matched.begin(), matched.end(), reducer);
				assert(success);
				
			}
		}
	}
	
	components_table_inited = 1;
}

static 
void
decode_cake_component_string
(
	const string& sym_data, 
	vector<string>& source_fnames, 
	vector<string>& compile_dirs, 
	vector<string>& producers
)
{
	// HACK: make this robust
	size_t startpos = 0;
	string source_fname;
	string compile_dir;
	string producer;
	while (startpos != string::npos)
	{
		cerr << "Beginning loop with: " << sym_data.substr(startpos) << endl;
		std::string::size_type caret_pos1 = sym_data.find('^', startpos);
		assert(caret_pos1 == startpos);
		std::string::size_type caret_pos2 = sym_data.find('^', caret_pos1 + 1);
		std::string::size_type caret_pos3 = sym_data.find('^', caret_pos2 + 1);
		std::string::size_type caret_pos4 = sym_data.find('^', caret_pos3 + 1);
		assert(caret_pos2 > caret_pos1 && caret_pos3 > caret_pos2 && caret_pos4 > caret_pos3);

		if (caret_pos4 == string::npos
		||  caret_pos3 == string::npos
		||  caret_pos2 == string::npos
		||  caret_pos1 == string::npos
		) break; // early exit if we got nothing
		startpos = (caret_pos4 == string::npos) ? string::npos : caret_pos4 + 1;
		if (startpos >= sym_data.length()) startpos = string::npos;
		
		std::string source_fname = sym_data.substr(caret_pos1 + 1, caret_pos2 - caret_pos1 - 1);
		std::string compile_dir = sym_data.substr(caret_pos2 + 1, caret_pos3 - caret_pos2 - 1);
		std::string producer = sym_data.substr(caret_pos3 + 1, caret_pos4 - caret_pos3 - 1);
		cerr << "Source fname: " << source_fname << endl;
		cerr << "Compile dir: " << compile_dir << endl;
		cerr << "Producer: " << producer << endl;
		source_fnames.push_back(source_fname);
		compile_dirs.push_back(compile_dir);
		producers.push_back(producer);
	}
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
		auto symbols = self.static_symbols(i_file);
		for (auto i_sym = symbols.first; i_sym != symbols.second; ++i_sym)
		{
			auto name_ptr = elf_strptr(i_sym.base().origin->elf,
				i_sym.base().origin->shdr.sh_link, 
				(size_t)i_sym->st_name);
			if (!name_ptr) continue; // null name
			std::string name(name_ptr);
			//cerr << "Considering symbol " << name << endl;
			
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
	if (!discovered)
	{
		cerr << "Aborting init table lookup for object at " << obj << endl;
		return 0;
	}
	
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
	
	auto key = (const init_table_key) {
			cake::compiler.fq_name_parts_for(discovered),
			from_first_to_second
		};
	
	if (p_table->find(key) != p_table->end()) return &(*p_table)[key];
	else return 0;
}	

static void get_object_size(
	void *obj,
	int obj_rep,
	unsigned *out_size, unsigned *out_count
	);
static void get_object_size(
	void *obj,
	int obj_rep,
	unsigned *out_size, unsigned *out_count)
{
	/* How many objects does "obj" point to? 
	 * We assume that obj points to the start of an object or subobject or array thereof. */
	auto discovered = self.discover_object_descr(
		(process_image::addr_t) obj /* FIXME: can thread through from previous discovery? */
	);
	memory_kind k = pmirror::self.discover_object_memory_kind(
		(process_image::addr_t) obj);
	if (!discovered)
	{
		goto return_opaque;
	}
	else
	{
		auto as_type = dynamic_pointer_cast<type_die>(discovered);
		assert(as_type);
		auto opt_byte_size = as_type->calculate_byte_size();
		if (as_type->get_concrete_type()->get_tag() != DW_TAG_array_type
			&& k != memory_kind::HEAP)
		{
			if (!opt_byte_size)
			{
				goto return_opaque;
			}
			else
			{
				*out_size = *opt_byte_size;
				*out_count = 1;
				return;
			}
		}
		else if (k == memory_kind::HEAP)
		{
			/* heap case */
			if (!opt_byte_size) 
			{
				goto return_opaque;
			}
			else
			{
				/* FIXME: get the heap block's start addr first -- can't assume obj is it. */
				*out_count = malloc_usable_size(obj) / *opt_byte_size;
				*out_size = *opt_byte_size;
				return;
			}
		}
		else 
		{
			assert(as_type->get_concrete_type()->get_tag() == DW_TAG_array_type);
			/* array case */
			auto as_array = dynamic_pointer_cast<spec::array_type_die>(as_type);
			assert(as_array);
			auto opt_count = as_array->element_count();
			assert(opt_count); // will fail for allocas, but hopefully not other stack/static cases
			// FIXME: short count for multidimensional arrays! we need to count how many
			// nested arrays we go through, and multiply out the element counts
			auto opt_element_size
			 = as_array->ultimate_element_type()->calculate_byte_size();
			assert(opt_element_size);
			*out_size = *opt_element_size;
			*out_count = *opt_count;
			return;
		}
	}
	assert(false); 
	
return_opaque:
	*out_size = 0;
	*out_count = 1;
	return;
}

void get_co_object_size(void *obj, int obj_rep, int co_obj_rep, unsigned *out_size, unsigned *out_count)
{
	auto init_tbl_ent = get_init_table_entry(obj, obj_rep, co_obj_rep);
	if (init_tbl_ent)
	{
		unsigned out_obj_size; 
		unsigned out_obj_count;/* How many objects does "obj" point to? */ 
		get_object_size(obj, obj_rep, &out_obj_size, &out_obj_count);
		/* For co-objs, we assume the number is the same as objs, 
		 * but the size may be different. */
		*out_size = init_tbl_ent->to_size;
		*out_count = out_obj_count;
	}
	else 
	{
		*out_size = 0; // use a zero-size co-object for uncorresponded types
		*out_count = 1;
	}
}

/* This is for co-objects that we have allocated. Their allocation sites are
 * no good for discovery, so we inform process_image of their type explicitly. */
void set_co_object_type(void *object, int obj_rep, void *co_object, int co_obj_rep)
{
	auto init_tbl_ent = get_init_table_entry(object, obj_rep, co_obj_rep);
	vector<string> name_parts;
	if (init_tbl_ent) name_parts = init_tbl_ent->to_typename;
	cerr << "Looking for type DIE given name parts " 
		<< name_parts_to_string(name_parts) << endl;
	
	/* Look through the CUs to find a definition of this typename. */
	std::cerr << "Searching component CU map of " << p_components->size() 
		<< " entries." << std::endl;
	unsigned count_tried = 0;
	for (auto i_component_entry = p_components->begin(); 
		i_component_entry != p_components->end();
		++i_component_entry)
	{
		cerr << "Found a component entry of rep id " << i_component_entry->second << endl;
		if (i_component_entry->second == co_obj_rep)
		{
			// found a CU of this component
			auto cu_pos = i_component_entry->first;
			auto cu = dynamic_pointer_cast<compile_unit_die>(
				(*cu_pos.p_ds)[cu_pos.off]);
			cerr << "Considering CU " << cu->summary() << " belonging to component "
				<< get_component_name_for_rep(co_obj_rep) << " (rep " << co_obj_rep << ")"
				<< endl;
			++count_tried;

			shared_ptr<type_die> found;
			if (name_parts.size() > 0)
			{
				auto found_basic = cu->resolve(name_parts.begin(), name_parts.end());
				found = dynamic_pointer_cast<type_die>(found_basic);

				// if we find it, it should be a type
				assert((found_basic && found) || (!found_basic && !found));
			}
				
			// if we didn't get an init table entry, it means "void" -- still okay
			if (found) cerr << "Informing process_image that co-object at " << (void*) co_object
				<< " has type " << found->summary() << endl;
			else cerr << "Informing process_image that co-object at " << (void*) co_object
				<< " has void/opaque type." << endl;
			pmirror::self.inform_heap_object_descr(
				(process_image::addr_t) co_object, found);
				
			return;
		}
	}
	
	// if we got here, the name parts don't resolve to a type
	// in any of the component's CUs.
	cerr << "Name parts " << name_parts_to_string(name_parts) << " did not resolve to a data type "
		<< "in any CU known for rep id " << co_obj_rep << endl;
	
	assert(false);

}

int allocating_component(void *obj)
{
	/* How to implement this? It's like discovering an object, but then
	 * we map to the CU to a component. */

	auto p_cu = pmirror::self.discover_allocating_cu_for_object((pmirror::addr_t) obj);
	assert(p_cu);
	dwarf::spec::abstract_dieset::position pos = { &p_cu->get_ds(), p_cu->get_offset() };
	cerr << "We believe that object " << obj << " was allocated in CU " 
		<< *p_cu;
	auto found = p_components->find(pos);
	assert(found != p_components->end());
	return found->second;
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
	if (discovered_source) std::cerr << endl << "(" << *discovered_source << ")";
	else std::cerr << " (assumed void type)";
	std::cerr << " to object: ";
	self.print_object(std::cerr, target_object);
	if (discovered_target) std::cerr << endl << "(" << *discovered_target << ")";
	else std::cerr << " (assumed void type)";
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
		auto source_name_parts = cake::compiler.name_parts_for(discovered_source_type);
		auto target_name_parts = cake::compiler.name_parts_for(discovered_target_type);

		cerr << "Looking in convs table at " << (void*) convs_table
			<< " for source name parts " << name_parts_to_string(source_name_parts) 
			<< ", target name parts " << name_parts_to_string(target_name_parts) 
			<< endl;
		auto found = convs_table->find(
			(cake::conv_table_key) {
				source_name_parts,
				target_name_parts,
				from_first_to_second,
				0 // FIXME
				});
		assert(found != convs_table->end());
		cerr << "Found a conv func at " << (void*) found->second.func << endl;
		return found->second.func;
	}
	else 
	{
		cerr << "Using noop conv func." << endl;
		return noop_conv_func;
	}
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
	std::cerr << endl << "(" << discovered_source->summary() << ")"
		<< " to object: ";
	self.print_object(std::cerr, target_object);
	std::cerr << endl << " (NEW INIT)"
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
	
	auto name_parts = cake::compiler.name_parts_for(discovered_source_type);
	cerr << "Looking in init table at " << (void*) init_table
		<< " for name parts " << name_parts_to_string(name_parts) << endl;
	auto found = init_table->find(
		(cake::init_table_key) {
			name_parts,
			from_first_to_second});
			
	if (found != init_table->end())
	{
		cerr << "Found init func at " << (void*) found->second.func << endl;
		return found->second.func;
	}
	else
	{
		cerr << "Didn't find init func; returning noop_conv_func." << endl;
		return noop_conv_func;
	}
}

void *noop_conv_func(void *arg1, void *arg2)
{
	return arg2;
}
