#ifndef LIBCAKE_PROCESS_HPP
#define LIBCAKE_PROCESS_HPP

#include <string>
#include <map>
#include <functional>

#include <sys/types.h>

#include <boost/shared_ptr.hpp>

#include <dwarfpp/spec.hpp>
#include <dwarfpp/attr.hpp>
#include <dwarfpp/lib.hpp>
#include <dwarfpp/adt.hpp>

#include <libunwind.h>
 
template <typename Target>
class unw_read_ptr
{
    unw_addr_space_t as;
    void *priv;
    Target *ptr;
    mutable Target buf;
public:
    typedef unw_read_ptr<Target> self_type;
    unw_read_ptr(unw_addr_space_t as, void *priv, Target *ptr) : as(as), priv(priv), ptr(ptr) {}
    Target operator*() const 
    { 
        Target tmp; 
        assert(sizeof tmp % sizeof (unw_word_t) == 0); // simplifying assumption
        unw_word_t *tmp_base = reinterpret_cast<unw_word_t*>(&tmp);
        for (unw_word_t *tmp_tgt = reinterpret_cast<unw_word_t*>(&tmp);
            tmp_tgt - tmp_base < sizeof tmp / sizeof (unw_word_t);
            tmp_tgt++)
        {
            off_t byte_offset 
             = reinterpret_cast<char*>(tmp_tgt) - reinterpret_cast<char*>(tmp_base);
            unw_get_accessors(as)->access_mem(as, 
                reinterpret_cast<unw_word_t>(reinterpret_cast<char*>(ptr) + byte_offset), 
                tmp_tgt,
                0,
                priv);
		}            
        return tmp;
    }
    // hmm... does this work? FIXME
    Target *operator->() const { this->buf = this->operator*(); return &this->buf; } 
    self_type& operator++() // prefix
    { ptr++; return *this; }
    self_type  operator++(int) // postfix ++
    { Target *tmp; ptr--; return self_type(as, tmp); }
    self_type& operator--() // prefix
    { ptr++; return *this; }
    self_type  operator--(int) // postfix ++
    { Target *tmp; ptr--; return self_type(as, tmp); }
    
    // we have two flavours of equality comparison: against ourselves,
    // and against unadorned pointers (risky, but useful for NULL testing)
    bool operator==(const self_type arg) { 
    	return this->as == arg.as
        && this->priv == arg.priv
        && this->ptr == arg.ptr; 
    }
    bool operator==(void *arg) { return this->ptr == arg; }
    
    bool operator!=(const self_type arg) { return !(*this == arg); }
    bool operator!=(void *arg) { return !(this->ptr == arg); }

	// default operator= and copy constructor work for us
    // but add another: construct from a raw ptr
    self_type& operator=(Target *ptr) { this->ptr = ptr; return *this; }

    self_type operator+(int arg)
    { return self_type(as, priv, ptr + arg); }

    self_type operator-(int arg)
    { return self_type(as, priv, ptr - arg); }

    ptrdiff_t operator-(const self_type arg)
    { return this->ptr - arg.ptr; }
    
    operator void*() { return ptr; }

};

        
/* Utility function: search multiple diesets for the first 
 * DIE matching a predicate. */
boost::shared_ptr<dwarf::spec::basic_die> resolve_first(
    std::vector<std::string> path,
    std::vector<boost::shared_ptr<dwarf::spec::with_named_children_die> > starting_points,
    bool(*pred)(dwarf::spec::basic_die&) = 0);


struct process_image
{
	/* We maintain a map of loaded objects, so that we can maintain
     * a dieset open on each one. We must keep this map in sync with the
     * actual process map. */
	typedef std::pair<void*, void*> entry_key;

    struct entry
    {
        char r, w, x, p;
        int offset;
        int maj, min;
        int inode;
        std::string seg_descr;
    };
    enum memory_kind
    {
	    UNKNOWN,
        STACK,
        HEAP,
        STATIC
    };
	static const char *name_for_memory_kind(/*memory_kind*/ int k); // relaxation for ltrace++
    
    struct file_entry
    {
    	boost::shared_ptr<std::ifstream> p_if;
        boost::shared_ptr<dwarf::lib::file> p_df;
        boost::shared_ptr<dwarf::lib::dieset> p_ds;
    };
    
	std::map<entry_key, entry> objects;
    std::map<entry_key, entry>::iterator objects_iterator;
    std::map<std::string, file_entry> files;
    typedef std::map<std::string, file_entry>::iterator files_iterator;

private:
	pid_t m_pid;
    unw_addr_space_t unw_as;
    unw_accessors_t unw_accessors;
    void *unw_priv;
    unw_context_t unw_context;
    std::vector<std::string> seen_map_lines;
public:
    process_image(pid_t pid = -1) 
    : m_pid(pid == -1 ? getpid() : pid),
      unw_as(pid == -1 ? 
      	unw_local_addr_space : 
        unw_create_addr_space(&_UPT_accessors/*&unw_accessors*/, 0))
    {
    	int retval = unw_getcontext(&unw_context);
        assert(retval == 0);
    	if (pid == -1)
        {
        	unw_accessors = unw_local_accessors;
            unw_priv = 0;
        }
        else 
        {
        	unw_accessors = _UPT_accessors;
        	unw_priv = _UPT_create(m_pid);
	    }
    	update();
    }
    void update();
    std::map<std::string, file_entry>::iterator find_file_by_realpath(const std::string& path);
    memory_kind discover_object_memory_kind(void *addr);
    void *get_dieset_base(dwarf::lib::abstract_dieset& ds);
    void *get_library_base(const std::string& path);
private:
    void *get_library_base_local(const std::string& path);
    void *get_library_base_remote(const std::string& path);
public:
	void *get_object_from_die(boost::shared_ptr<dwarf::spec::with_runtime_location_die> d,
		dwarf::lib::Dwarf_Addr vaddr);
    boost::shared_ptr<dwarf::spec::basic_die> discover_object_descr(void *addr,
    	boost::shared_ptr<dwarf::spec::type_die> imprecise_static_type,
        void **out_object_start_addr);
    boost::shared_ptr<dwarf::spec::basic_die> discover_stack_object(void *addr,
        void **out_object_start_addr);
    boost::shared_ptr<dwarf::spec::basic_die> discover_heap_object(void *addr,
    	boost::shared_ptr<dwarf::spec::type_die> imprecise_static_type,
        void **out_object_start_addr);
    boost::shared_ptr<dwarf::spec::with_runtime_location_die> discover_object(
    	void *addr,
        void **out_object_start_addr);
private:
    void rebuild_map(const std::vector<std::string>& map_data);
};

std::ostream& operator<<(std::ostream& s, const process_image::memory_kind& k);

#endif
