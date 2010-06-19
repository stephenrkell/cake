#include "process.hpp"
#include <fstream>
#include <sstream>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fileno.hpp>
//#define _GNU_SOURCE
#include <link.h>
#include <gelf.h>
#include <libunwind.h>

using namespace dwarf;
/*using namespace dwarf::lib;*/ // omitted to remove Elf ambiguity
using std::string;

void process_image::update()
{
    std::ostringstream filename;
    filename << "/proc/" << m_pid << "/maps";
    std::vector<std::string> map_contents;
    {
    	string line;
	    std::ifstream map_file(filename.str());
        while (map_file)
        {
        	std::getline(map_file, line, '\n'); 
            map_contents.push_back(line);
        }
    }
    
    if (map_contents == seen_map_lines) return;
    else
    {
    	seen_map_lines = map_contents;
        rebuild_map(seen_map_lines);
    }
}

void process_image::rebuild_map(const std::vector<string>& lines)
{
	// open the process map for the file
    char seg_descr[PATH_MAX + 1];
	std::map<entry_key, entry> new_objects; // replacement map
    
    for (auto i = lines.begin(); i != lines.end(); i++)
    {
		#undef NUM_FIELDS
		#define NUM_FIELDS 11
        entry_key k;
        entry e;
        int fields_read = sscanf(i->c_str(), 
        	"%p-%p %c%c%c%c %8x %2x:%2x %d %s\n",
    	    &k.first, &k.second, &e.r, &e.w, &e.x, &e.p, &e.offset, &e.maj, &e.min, &e.inode, 
            seg_descr);

		// we should only get an empty line at the end
		if (fields_read == EOF) { assert(++i == lines.end()); return; }
        if (fields_read < (NUM_FIELDS-1)) throw string("Bad maps data! ") + *i;

        if (fields_read == NUM_FIELDS) e.seg_descr = seg_descr;
        else e.seg_descr = std::string();
        
        if (objects.find(k) == objects.end() // common case: adding a new object
        	|| string(objects.find(k)->second.seg_descr) != objects[k].seg_descr) 
             // less common case: same start/end but different libname
        {
        	if (seg_descr[0] == '/' && files.find(seg_descr) == files.end())
        	{
            	files[seg_descr].p_if = boost::make_shared<std::ifstream>(seg_descr);
                if (*files[seg_descr].p_if)
                {
	                int fd = fileno(*files[seg_descr].p_if);
    	            if (fd != -1)
                    {
                    	try
                        {
                    		files[seg_descr].p_df = boost::make_shared<lib::file>(fd);
                            files[seg_descr].p_ds = boost::make_shared<lib::dieset>(
                        	    *files[seg_descr].p_df);
                    	}
                        catch (dwarf::lib::No_entry)
                        {
                        	files[seg_descr].p_df = boost::shared_ptr<lib::file>();
                            files[seg_descr].p_ds = boost::shared_ptr<lib::dieset>();
                        }
                    }
                    else 
                    {
                    	files[seg_descr].p_if->close();
                    }
                }
            }
        }
        // now we can assign the new entry to the map
        new_objects[k] = e;
    }
    objects = new_objects;
    
    /* FIXME: if a mapping goes away, we remove its entry but leave its 
     * file open. This does no harm, but would be nice to delete it. */
}

/* Utility function: search multiple diesets for the first 
 * DIE matching a predicate. */
boost::shared_ptr<spec::basic_die> resolve_first(
    std::vector<string> path,
    std::vector<boost::shared_ptr<spec::with_named_children_die > > starting_points,
    bool(*pred)(spec::basic_die&) /*=0*/)
{
	for (auto i_start = starting_points.begin(); i_start != starting_points.end(); i_start++)
    {
    	std::vector<boost::shared_ptr<spec::basic_die> > results;
        (*i_start)->scoped_resolve_all(path.begin(), path.end(), results);
        for (auto i_result = results.begin(); i_result != results.end(); i_result++)
        {
        	if (!pred || pred(**i_result)) return *i_result;
		}
    }
}

void *process_image::get_dieset_base(dwarf::lib::abstract_dieset& ds)
{
    int retval;
    /* First get the filename of the dieset, by searching through
     * the files map. */
    files_iterator found;
    for (auto i = files.begin(); i != files.end(); i++)
    {
    	if (i->second.p_ds.get() == &ds) { found = i; break; }
    }
    if (found == files.end()) 
    {
		std::cerr << "Warning: failed to find library for some DIE..." << std::endl;
    	return 0; // give up
    }

	return get_library_base(found->first.c_str());
}

struct callback_in_out
{
	const char *name_in;
    void *load_addr_out;
};
static int phdr_callback(struct dl_phdr_info *info, size_t size, void *data)
{
	const char *name_sought = ((callback_in_out *)data)->name_in;
    size_t sought_namelen = strlen(name_sought);
    size_t cur_obj_namelen = strlen(info->dlpi_name);
    // search for a *suffix match*
    // FIXME: fix s.t. prefix must end in '/'
	if (strncmp(info->dlpi_name + (cur_obj_namelen - sought_namelen),
    	name_sought, sought_namelen) == 0)
    {
    	((callback_in_out *)data)->load_addr_out = (void*) info->dlpi_addr;
    	return 1;
    }
    else return 0; // 0 means "carry on searching"
}
void *process_image::get_library_base(const std::string& path)
{
	char real_path[PATH_MAX]; 
    int retval;
    char *retpath = realpath(path.c_str(), real_path);
    assert(retpath != NULL);
    if (m_pid == getpid())
    {
    	/* use the local version */
        return get_library_base_local(std::string(real_path));
    }
    else
    {
    	/* use the remote version */
        return get_library_base_remote(std::string(real_path));
    }
}

void *process_image::get_library_base_local(const std::string& path)
{
    callback_in_out obj = { path.c_str()/*"libcake.so"*/, 0 };
    int retval = dl_iterate_phdr(phdr_callback, &obj);
    if (retval)
    {
    	// result -- we found the library
        void *library_base = obj.load_addr_out;
        return library_base;
    }
    else
    {
    	// not a result: didn't find the library
		std::cerr << "Warning: failed to find library for some DIE..." << std::endl;
    	return 0;
	}
}

void *process_image::get_library_base_remote(const std::string& path)
{
	/* We should have the executable open already -- find it. */
    std::ostringstream filename;
    filename << "/proc/" << m_pid << "/exe";
    //char link_target[PATH_MAX];
    //int retval;
    //retval = readlink(filename.str().c_str(), link_target, PATH_MAX);
    //assert(retval != -1);
    char real_exec[PATH_MAX];
    char *retpath;
    retpath = realpath(/*link_target*/filename.str().c_str(), real_exec);
    assert(retpath != NULL);
    files_iterator found = files.end();
    for (auto i = files.begin(); i != files.end(); i++)
    {
    	if (i->first == std::string(real_exec))
        {
			/* Found the executable */
            found = i;
            break;
		}
	}
    assert(found != files.end());
    int fd = fileno(*found->second.p_if);
    assert(fd != -1);
	void *dyn_addr = 0;
    if (elf_version(EV_CURRENT) == EV_NONE)
	{
		/* library out of date */
		/* recover from error */
        assert(0);
	}
	Elf *e = elf_begin(fd, ELF_C_READ, NULL);
	GElf_Ehdr ehdr;
	if (e != NULL && elf_kind(e) == ELF_K_ELF && gelf_getehdr(e, &ehdr))
    {
	    for (int i = 1; i < ehdr.e_shnum; ++i) 
        {
		    Elf_Scn *scn;
		    GElf_Shdr shdr;
		    const char *name;

		    scn = elf_getscn(e, i);
		    if (scn != NULL && gelf_getshdr(scn, &shdr))
            {
			    name = elf_strptr(e, ehdr.e_shstrndx, shdr.sh_name);
                switch(shdr.sh_type)
                {
                    case SHT_DYNAMIC:
                    {
                    	dyn_addr = (void*)(unsigned) shdr.sh_addr;
                        break;
                    }
                    default: continue;
                }
            }
        }
    }
    assert(dyn_addr != 0);
    /* Search the dynamic section for the DT_DEBUG tag. */
    int done = 0, i = 0;
	ElfW(Dyn) entry;
    
	unw_read_ptr<ElfW(Dyn)> search_ptr(this->unw_as, this->unw_priv, dyn_addr);
	
    void *dbg_addr;
    do
    {
     	entry = *search_ptr;
		if (entry.d_tag == DT_DEBUG) {
			done = 1;
			dbg_addr = static_cast<void*>(entry.d_un.d_val);
		}
        search_ptr += sizeof (entry);
    } while (!done && entry.d_tag != DT_NULL && 
                ++i < ELF_MAX_SEGMENTS);
    
    r_debug *rdbg = new r_debug();
	if (!rdbg) {
		return NULL;
	}
    unw_read_ptr<r_debug> dbg_ptr(this->unw_as, this->unw_priv, dbg_addr);
    *rdbg = *dbg_ptr;
	/* If we don't have a r_debug, this might segfault! */
    fprintf(stderr, "Found r_debug structure at %p\n", rdbg);

	/*return rdbg;*/
    return 0;
    
}

void *
process_image::get_object_from_die(
  boost::shared_ptr<spec::with_runtime_location_die> p_d, 
  lib::Dwarf_Addr vaddr)
{
	/* From a DIE, return the address of the object it denotes. 
     * This only works for DIEs describing objects existing at
     * runtime. */

	unsigned char *base = reinterpret_cast<unsigned char *>(get_dieset_base(p_d->get_ds()));
    assert(p_d->get_runtime_location().size() == 1);
    auto loc_expr = p_d->get_runtime_location();
    lib::Dwarf_Unsigned result = dwarf::lib::evaluator(
        loc_expr,
        vaddr,
        p_d->get_spec()
        ).tos();
    unsigned char *retval = base + static_cast<size_t>(result);
    return retval;
}

process_image::memory_kind process_image::discover_object_memory_kind(void *addr)
{
	memory_kind ret = UNKNOWN;
	// for each range in the map...
	for (auto i_obj = objects.begin(); i_obj != objects.end(); i_obj++)
    {
    	void *begin = i_obj->first.first;
        void *end = i_obj->first.second;
        // see whether addr matches this range
        if (addr >= begin && addr < end)
        {
        	const char *seg_descr = i_obj->second.seg_descr.c_str();
            // what sort of line is this?
            switch(seg_descr[0])
            {
                case '[':
                    if (strcmp(seg_descr, "[stack]") == 0)
                    {
                        ret = STACK; break;
				    }
                    else if (strcmp(seg_descr, "[heap]") == 0)
                    {
                        ret = HEAP; break;
                    }
                    else if (strcmp(seg_descr, "[anon]") == 0)
                    {
                        // hmm... anon segments might be alloc'd specially...
                        // ... but it's probably "closest" to report them as heap
                        ret = HEAP; break;
				    }
                    else { ret = UNKNOWN; break; }
                    //break;
                case '/': ret = STATIC; break;
                    //break;
                case '\0': ret = HEAP; break; // treat nameless segments as heap too
                    //break;
                default: ret = UNKNOWN; break;
                    //break;
            }
        }
    }
	return ret;
}    

/* Clearly one of these will delegate to the other. 
 * Which way round do they go? Clearly, discover_object_descr must first
 * delegate to discover_object, in case the object has its own variable DIE,
 * which might have a customised DWARF type. If that fails, we use the
 * generic object discovery stuff based on memory kind. */
 
/* Discover a DWARF type for an arbitrary object in the program address space. */
boost::shared_ptr<spec::basic_die> 
process_image::discover_object_descr(void *addr, 
	boost::shared_ptr<spec::type_die> imprecise_static_type,
	void **out_object_start_addr)
{
	auto discovered_obj = discover_object(addr, out_object_start_addr);
    if (discovered_obj)
    {
    	if (discovered_obj->get_tag() == DW_TAG_variable)
	    	return *boost::dynamic_pointer_cast<
    	    	dwarf::spec::variable_die>(discovered_obj)->get_type();
    	else return discovered_obj; // HACK: return subprograms as their own descriptions
    }
    else
    {
    	switch(discover_object_memory_kind(addr))
        {
        	case STATIC:
            	std::cerr << 
                	"Warning: static object DIE search failed for static object at 0x" 
                    << addr << std::endl;
            case STACK:
            	return discover_stack_object(addr, out_object_start_addr);
            case HEAP:
            	return discover_heap_object(addr, imprecise_static_type, out_object_start_addr);
            default:
            case UNKNOWN:
            	std::cerr << "Warning: unknown kind of memory at 0x" << addr << std::endl;
            	return boost::shared_ptr<spec::basic_die>();
        }
    }
}

/* Discover a DWARF variable or subprogram for an arbitrary object in
 * the program. These will usually be static-alloc'd objects, but in
 * DwarfPython they could be heap-alloc'd objects that have been
 * specialised in their layout. Could they be stack-alloc'd? I guess so,
 * although you'd better hope that the C code which allocated them won't
 * be accessing them any more. */
boost::shared_ptr<spec::with_runtime_location_die> 
process_image::discover_object(void *addr, void **out_object_start_addr)
{



	return boost::shared_ptr<spec::with_runtime_location_die>();
}

boost::shared_ptr<dwarf::spec::basic_die> 
process_image::discover_heap_object(void *addr,
    boost::shared_ptr<dwarf::spec::type_die> imprecise_static_type,
    void **out_object_start_addr)
{
	return boost::shared_ptr<dwarf::spec::basic_die>();
}

//void *
boost::shared_ptr<dwarf::spec::basic_die>
process_image::discover_stack_object(void *addr, void **out_object_start_addr/*,
	unw_word_t top_frame_sp, unw_word_t top_frame_ip, unw_word_t top_frame_retaddr,
    const char *top_frame_fn_name*/)
{
// 	/* First we find the stack frame that contains the pointed-to
//      * address. */
// 	unw_cursor_t cursor, prev_cursor;
//     bool prev_cursor_init = false;
// 	unw_word_t prevframe_sp = 0, sp;
//     unw_word_t ip;
// 	char fn_name[100] = "<anonymous top frame>";
//     int unw_ret, step_ret;
// 
// 	unw_init_remote(&cursor, proc->unwind_as, proc->unwind_priv);
//     /* When we are called from ltrace argument output, 
//      * a new stack frame is mid-way through being set up, 
//      * so we get slightly odd results:
//      * - libunwind won't give us the top (being-set-up) frame
//      * - libunwind gives us "_init" for the name of the next frame?!
//      * Work around this by
//      * checking whether our IP (adjusted) is a breakpoint, and if so,
//      * hand-craft the first run of loop variables with 
//      * ltrace-derived values. */
// 	unw_ret = unw_get_reg(&cursor, UNW_REG_IP, &ip); assert(unw_ret == 0);
//     unw_ret = unw_get_reg(&cursor, UNW_REG_SP, &sp); assert(unw_ret == 0);
//     assert(reinterpret_cast<unw_word_t>(proc->stack_pointer) == sp);
//     assert(reinterpret_cast<unw_word_t>(proc->instruction_pointer) == ip);
// 
// 	// FIXME: detect the ltrace special case (can we?), or push it
//     // into the caller by passing the top frame info as arguments
//     Breakpoint *sbp = address2bpstruct(proc, proc->instruction_pointer - DECR_PC_AFTER_BREAK);
//     if (sbp) strncpy(fn_name, sbp->libsym->name, 100);
//     else strncpy(fn_name, "<anonymous top frame>", 100);
//     prevframe_sp = sp;
// 
// 	do {
//     	// test our location against the current frame
//         if (stack_loc < reinterpret_cast<void*>(prevframe_sp) 
//         	&& stack_loc >= reinterpret_cast<void*>(sp))
//         {
// 	        fprintf(options.output, "I think the object at %p resides in a %s stack frame with sp=%p, prevframe_sp=%p\n", 
//             	stack_loc, fn_name, sp, prevframe_sp);
//             /* We have found the frame, so we exit the loop. But note that we have
//              * already advanced the cursor to the previous frame, in order to read
//              * prevframe_sp! Later we will need the cursor pointing at *this* frame,
//              * so we use prev_cursor. */
//             break;
// 		}
//         else
//         {
//         	//fprintf(options.output, "I think the object at %p does NOT reside in a %s stack frame with sp=%p, prevframe_sp=%p\n", 
//             //	stack_loc, fn_name, sp, prevframe_sp);
//         }   
//         unw_ret = unw_get_reg(&cursor, UNW_REG_IP, &ip); if (unw_ret != 0) break;
//         unw_ret = unw_get_proc_name(&cursor, fn_name, 100, NULL); if (unw_ret != 0) break;
//         
//         // we've already got the ip, sp and fn_name for this frame, so
//         // advance to the next frame for the prevframe_sp
// 		sp = prevframe_sp;
//         if (sp == BEGINNING_OF_STACK) break;
//         
//         prev_cursor = cursor;
//         prev_cursor_init = true;
//         step_ret = unw_step(&cursor);
// 		if (step_ret > 0) unw_get_reg(&cursor, UNW_REG_SP, &prevframe_sp);
//         else prevframe_sp = BEGINNING_OF_STACK; /* beginning of stack */
//     } while  (1);
//     assert(prev_cursor_init); cursor = prev_cursor;
// #undef BEGINNING_OF_STACK
//     if (strlen(fn_name) == 0) return 0;
//     
//     /* Now use the instruction pointer to find the DIE for the 
//      * function which allocated the stack frame. */
//     boost::optional<dwarf::encap::Die_encap_subprogram&> found;
//     void **dbg_files = (void**) malloc(sizeof (void*) * library_num + 1);
// 	int i;
//     for (i = 0; i < library_num; i++)
//     {
//     	dbg_files[i] = library_dbg_info[i];
//     }
//     dbg_files[i] = exec_dbg_info;
//     
//     for (int j = 0; j <= library_num; j++)
//     {
//     	dwarf::encap::file *p_file 
//          = static_cast<dwarf::encap::file *>(dbg_files[j]);
//         if (!p_file) continue;
//         
//         if (!(p_file->get_ds().map_size() > 1)) break; // we have no info
//          
//         for (auto i_subp = p_file->get_ds().all_compile_units().subprograms_begin();
//         	i_subp != p_file->get_ds().all_compile_units().subprograms_end();
//             i_subp++)
//         {
//             if ((*i_subp)->get_name() && (*i_subp)->get_low_pc() && (*i_subp)->get_high_pc())
//             { 
//             	if (strcmp(fn_name, (*(*i_subp)->get_name()).c_str()) == 0)
//                 {
//             	    fprintf(options.output, "subprogram %s, low %p, high %p\n", 
//                 	    (*(*i_subp)->get_name()).c_str(), 
//                         (void*) *((*i_subp)->get_low_pc()), (void*) *((*i_subp)->get_high_pc()));
//                     std::cerr << **i_subp;
// 	            }
//             	if (ip >= *((*i_subp)->get_low_pc()) && ip < *((*i_subp)->get_high_pc()))
//                 {
//             	    found = **i_subp;
//             	    assert(strcmp(fn_name, (*(*i_subp)->get_name()).c_str()) == 0);
//                     break;
//                 }
// 	        }
//         }
//     }
//     free(dbg_files);
//     if (found)
//     {
//     	dwarf::encap::Die_encap_subprogram& subprogram = *found;
//     	fprintf(options.output, "Successfully tracked down subprogram: %s\n", 
//         	(*subprogram.get_name()).c_str());
//             
//         /* Calculate the "frame base" (stack pointer) for the location in
//          * the subprogram. First calculate the offset from the CU base. */
//         dwarf::lib::Dwarf_Off cu_offset = ip - *subprogram.enclosing_compile_unit().get_low_pc();
//         assert(cu_offset >= 0);
//         std::cerr << "Detected that current PC 0x"
//         	<< std::hex << ip
//            << " has CU offset 0x" << std::hex << cu_offset << std::endl;
//         /* Now select the DWARF expression (in the location list) which
//          * matches the CU offset. */
// 		class libunwind_regs : public dwarf::lib::regs
//         {
//         	unw_cursor_t *c;
//         public:
//             dwarf::lib::Dwarf_Signed get(int i)
//             {
//             	unw_word_t regval;
//             	switch(i)
//                 {
// // from libunwind-x86.h
// #define EAX     0
// #define ECX     1
// #define EDX     2
// #define EBX     3
// #define ESP     4
// #define EBP     5
// #define ESI     6
// #define EDI     7
// #define EIP     8
// #define EFLAGS  9
// #define TRAPNO  10
// #define ST0     11
//                 	case EAX: unw_get_reg(c, UNW_X86_EAX, &regval); break;
// 					case EDX: unw_get_reg(c, UNW_X86_EDX, &regval); break;
// 					case ECX: unw_get_reg(c, UNW_X86_ECX, &regval); break;
// 					case EBX: unw_get_reg(c, UNW_X86_EBX, &regval); break;
// 					case ESI: unw_get_reg(c, UNW_X86_ESI, &regval); break;
//                     case EDI: unw_get_reg(c, UNW_X86_EDI, &regval); break;
//                     case EBP: unw_get_reg(c, UNW_X86_EBP, &regval); 
//                     	std::cerr << "read EBP as 0x" << std::hex << regval << std::endl;
//                         break;
//                     case ESP: unw_get_reg(c, UNW_X86_ESP, &regval); 
//                     	std::cerr << "read ESP as 0x" << std::hex << regval << std::endl;                    
//                     	break;
//                     case EIP: unw_get_reg(c, UNW_X86_EIP, &regval); break;
//                     case EFLAGS: unw_get_reg(c, UNW_X86_EFLAGS, &regval); break;
//                     case TRAPNO: unw_get_reg(c, UNW_X86_TRAPNO, &regval); break;
//                     default:
//                     	throw dwarf::lib::Not_supported("unsupported register number");
//                 }
//                 return regval;
//             }
//         	libunwind_regs(unw_cursor_t *c) : c(c) {}
//         } my_regs(&cursor);
//         boost::optional<dwarf::encap::loc_expr> found;
//         for (auto i_loc_expr = subprogram.get_frame_base()->begin();
//         		i_loc_expr != subprogram.get_frame_base()->end();
//                 i_loc_expr++)
//         {
//           	std::cerr << "Testing whether CU offset 0x" 
//             	<< std::hex << cu_offset << " falls within loc_expr " << *i_loc_expr
//                 	<< std::endl;
// 
//         	if (cu_offset >= i_loc_expr->lopc 
//             	&& cu_offset < i_loc_expr->hipc)
//             {
//             	std::cerr << "Success" << std::endl;
//             	found = *i_loc_expr;
//                 break;
//             }
//         }
//         assert(found);
//         // now evaluate that expression to get the frame_base
//         dwarf::lib::Dwarf_Signed frame_base = dwarf::lib::evaluator(
//         		(*found).m_expr, 
//                 subprogram.get_ds().get_spec(),
//                 my_regs,
//                 0).tos();
//         std::cerr << "Calculated DWARF frame base: 0x" << std::hex << frame_base << std::endl;
//         
//         /* Now for the coup de grace: find the extents of the object 
//          * (local or formal parameter) that spans the pointed-to address.
//          * For robustness, we do this by iterating over all formal 
//          * parameters and locals, and matching their extents. */
//         for (auto i_fp = subprogram.formal_parameters_begin();
//         		i_fp != subprogram.formal_parameters_end();
//                 i_fp++)
//         {        	
//         	unsigned param_begin_addr = dwarf::lib::evaluator(
//         		(*i_fp)->get_location()->at(0).m_expr, 
//                 subprogram.get_ds().get_spec(),
//                 frame_base).tos();
//             assert((*i_fp)->get_type());
//             assert((*i_fp)->get_name());            
//             unsigned param_end_addr
//              = param_begin_addr + 
//              	dwarf::encap::Die_encap_is_type::calculate_byte_size(
//              		*(*i_fp)->get_type());
//             fprintf(options.output,
//             	"Parameter %s spans addresses [%p, %p)\n", 
//             	(*i_fp)->get_name()->c_str(),
//                 param_begin_addr,
//                 (void*)(int) param_end_addr);
//             if (stack_loc >= (void*)(int) param_begin_addr && stack_loc < (void*)(int) param_end_addr)
//             {
//             	if (out_object_start_addr != 0) *out_object_start_addr = (void*)(int) param_begin_addr;
//                 assert((*i_fp)->get_type());
//                 return &*(*i_fp)->get_type();
// 			}            
//         }
//         //for (auto i_var = subprogram.variables_begin();
//         //		i_var != subprogram.variables_end();
//         //        i_var++)
//         for (auto i_dfs = subprogram.depthfirst_begin();
//         		i_dfs != subprogram.depthfirst_end();
//                 i_dfs++)
//         {
//         	/* Here we explore child DIEs looking for DW_TAG_variables.
//              * We only want those defined in a lexical block that is
//              * currently active, so use nearest_enclosing. */
//             if (i_dfs->get_tag() != DW_TAG_variable) continue;
//             
//             /* Find the nearest enclosing lexical block (if any). */
//             auto opt_block = dynamic_cast<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&>(*i_dfs).
//             					nearest_enclosing<DW_TAG_lexical_block>();
//             if (!opt_block || (
//             		ip < *opt_block->get_high_pc() 
//                     && ip >= *opt_block->get_low_pc()))
//             {
//                 dwarf::encap::Die_encap_variable& variable
//                  = dynamic_cast<dwarf::encap::Die_encap_variable&>(*i_dfs);
//         	    unsigned var_begin_addr = dwarf::lib::evaluator(
//         		    variable.get_location()->at(0).m_expr, 
//                     subprogram.get_ds().get_spec(),
//                     my_regs,
//                     frame_base).tos();
//                 assert(variable.get_type());
//                 assert(variable.get_name());            
//                 unsigned var_end_addr
//                  = var_begin_addr + 
//              	    dwarf::encap::Die_encap_is_type::calculate_byte_size(
//              		    *variable.get_type());
//                 fprintf(options.output,
//             	    "Local %s spans addresses [%p, %p)\n", 
//             	    variable.get_name()->c_str(),
//                     var_begin_addr,
//                     (void*)(int) var_end_addr);
//                 if (stack_loc >= (void*)(int) var_begin_addr && stack_loc < (void*)(int) var_end_addr)
//                 {
//             	    if (out_object_start_addr != 0) *out_object_start_addr = (void*)(int) var_begin_addr;
//                     assert(variable.get_type());
//                     return &*variable.get_type();
// 			    }            
//             }
//         }
//         fprintf(options.output, "Failed to find a local or actual parameter for %p\n", stack_loc);
//     }
//     else
//     {
//     	fprintf(options.output, "Failed to track down subprogram for PC %p\n", ip);
//         /* We could get the frame by falling back on symtab.
//          * This isn't much good though. */
//     }
//     
	return boost::shared_ptr<dwarf::spec::basic_die>();
}


const char *process_image::name_for_memory_kind(int k) // relaxation for ltrace++
{
	switch(k)
    {
        case STACK: return "stack";
        case STATIC: return "static";
        case HEAP: return "heap";
    	case UNKNOWN: 
        default: return "unknown";
	}    
}
std::ostream& operator<<(std::ostream& s, const process_image::memory_kind& k)
{
	s << process_image::name_for_memory_kind(k);
    return s;
}

struct realpath_file_entry_cmp 
: public std::unary_function<std::pair<std::string, process_image::file_entry>, bool>
{
	char path_real[PATH_MAX];
	realpath_file_entry_cmp(const char *path) 
    { char *retval = realpath(path, path_real); assert(retval != NULL); }
	bool operator()(const std::pair<std::string, process_image::file_entry>& arg) const
    {
    	char arg_real[PATH_MAX];
        realpath(arg.first.c_str(), arg_real);
        return strcmp(arg_real, path_real) == 0;
    }
};
std::map<std::string, process_image::file_entry>::iterator 
process_image::find_file_by_realpath(const std::string& path)
{
	return std::find_if(this->files.begin(), this->files.end(), 
    	realpath_file_entry_cmp(path.c_str()));
}
