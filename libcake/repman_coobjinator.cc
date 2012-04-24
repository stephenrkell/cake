/* The coobjinator -- a memtable for recording co-objects. 
 * 
 * We want to be able to look up from pointers to their co-object groups. 
 * 
 * We use a memtable whose entries are std::map<
 *     void*,
 *     shared_ptr<co_object_group> 
 *  >. These are 48 bytes in size for me. 
 *
 * i.e. co-object groups are allocated in the heap. We use a lot of small maps
 * of to reduce the constant factor in the log(n) term to something small, and
 * the memtable itself only contributes an O(1) time (and linear space).
 *
 * For a max-sized 2^46-byte memtable, that means each 48-byte entry should cover
 * 48 * 2^18 bytes in a 64-bit AS, i.e. 12MB. Let's knock that down by 1/4, so
 * each entry covers 48MB. This means thousands of objects in one entry.
 *
 * Note we could optimise the std::map by keying on small uints instead of void*.
 */

extern "C" {
#include <memtable.h>
#include "repman.h"
}
#include <map>
#include <set>
#include <cstdio>
#include <cstdarg>
#include <strings.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
#include "repman_image.hh"
using pmirror::process_image;

// HACK: replace shared_ptr and weak_ptr with regular pointers until we can test this properly

typedef std::map<void*, co_object_group* > map_t;
struct entry {
	unsigned present:1;
	map_t m;
} __attribute__((packed));

static std::set<map_t*> maps; // FIXME: reclaim these
static std::set</*boost::weak_ptr< co_object_group> */ co_object_group* > groups;

entry *coobjinator_region;
#define entry_coverage_in_bytes (64*1024*1024)
// 64MB regions; there are 2^38 such in a 64-bit address space

using boost::shared_ptr;
using std::cerr;
using std::endl;

static map_t *ensure_map_for_addr(void *addr);

static void init() __attribute__((constructor));
static void init()
{
	size_t mapping_size = MEMTABLE_MAPPING_SIZE_WITH_TYPE(entry,
		entry_coverage_in_bytes, 0, 0 /* both 0 => cover full address range */);

	assert (mapping_size <= BIGGEST_MMAP_ALLOWED);
	coobjinator_region = MEMTABLE_NEW_WITH_TYPE(entry, 
		entry_coverage_in_bytes, 0, 0);
	assert(coobjinator_region != MAP_FAILED);
}

static void check_maps_and_groups_consistency(void)
{
	/* For every map entry, the group should agree. */
	for (auto i_map = maps.begin(); i_map != maps.end(); ++i_map)
	{
		fprintf(stderr, "Map at %p has { ", *i_map);
		bool printed = false;
		for (auto i_ent = (*i_map)->begin(); i_ent != (*i_map)->end(); ++i_ent)
		{
			auto p_obj = i_ent->first;
			auto p_group = i_ent->second;
			
			if (printed) fprintf(stderr, ", ");
			fprintf(stderr, "(object %p, group %p)", p_obj, p_group);
			printed = true;
			
			// some rep in the group should be this object
			bool found = false;
			for (int i = 0; i < MAX_REPS; ++i)
			{
				if (p_group->reps[i] == p_obj) { found = true; break; }
			}
			assert(found); // if this fails, it means the map entry is stale
		}
		fprintf(stderr, " }\n");
	}
	/* For every group entry, the maps should agree. */
	for (auto i_group = groups.begin(); i_group != groups.end(); ++i_group)
	{
		if ((*i_group)->delete_me) 
		{
			fprintf(stderr, "Group record at %p is marked for deletion.\n", *i_group);
		}
		else
		{
			fprintf(stderr, "Group record at %p has { ", *i_group);
			bool printed = false;
			for (int i = 0; i < MAX_REPS; ++i)
			{
				void *p_obj = (*i_group)->reps[i];
				if (p_obj)
				{
					if (printed) fprintf(stderr, ", ");
					fprintf(stderr, "rep %d: %p", i, p_obj);
					printed = true;
					auto p_map = ensure_map_for_addr(p_obj);
					assert(p_map);
					auto found = p_map->find(p_obj);
					assert(found != p_map->end());
					auto p_group = found->second;
					assert(p_group == *i_group);
				}
			}
			fprintf(stderr, "}\n");
		}		
		fflush(stderr);
	}
}

entry *entry_for_object(void *obj)
{
	entry *e = MEMTABLE_ADDR_WITH_TYPE(coobjinator_region, entry, entry_coverage_in_bytes,
		0, 0, obj);
	return e;
}

map_t *map_ptr_for_object(void *obj)
{
	entry *e = entry_for_object(obj);
	if (e && e->present) return &e->m;
	else return 0;
}

static /*shared_ptr<co_object_group>*/ co_object_group* group_for_object(void *obj)
{
	entry *e = entry_for_object(obj);
	if (e && e->present)
	{
		auto found = e->m.find(obj);
		if (found != e->m.end()) return found->second;
		else return 0; //shared_ptr<co_object_group>();
	}
	else return 0; //shared_ptr<co_object_group>();
}


/* This is called whenever a co-object is freed. We assume that this should free
 * all other co-objects. So we delete the whole group. */
/* Using the shared_ptr implementation of groups, it's not necessary to explicitly
 * delete the group record. But we should remove the map entries that point to it,
 * and delete any objects we allocated. */
int invalidate_co_object(void *object, int rep)
{
	auto group = group_for_object(object);
	//if (!group || !group->reps[rep] || group->reps[rep] != object) return -1; /* co-object not found */
	assert(group && group->reps[rep] && group->reps[rep] == object);

	/* Using the shared_ptr implementation of groups, it's not necessary to explicitly
	 * delete the group record. But we should remove the map entries that point to it,
	 * and delete any objects we allocated. */

	for (int i = 0; i < MAX_REPS; i++)
	{
		if (group->reps[i])
		{
			map_ptr_for_object(group->reps[i])->erase(group->reps[i]);
			if (group->co_object_info[i].allocated_by == ALLOC_BY_REP_MAN)
			{
				fprintf(stderr, "Freeing co-object at %p\n", group->reps[i]);
				free(group->reps[i]);
			} 
			else 
			{
				fprintf(stderr, "Not freeing user-allocated co-object at %p\n", group->reps[i]);
			}
			// we null the pointer in both cases
			group->reps[i] = 0;
		}
	}
	
	// don't do this, because it's likely we are being called mid-traversal
	//groups.erase(group);
	// instead, mark for deletion
	group->delete_me = 1;
	
	check_maps_and_groups_consistency();
	
	return 0;
}

void *find_co_object(const void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out/*, int expected_size_words*/)
{
	struct co_object_group *tmp_co_object_group_ptr = 0;
	struct co_object_group **rec_out_ptr
	 = co_object_rec_out ? co_object_rec_out : & tmp_co_object_group_ptr;
	
	auto found = find_co_object_opaque(object, object_rep, co_object_rep, rec_out_ptr);
	if (found)
	{
		assert(*rec_out_ptr);
		assert(!(*rec_out_ptr)->co_object_info[co_object_rep].opaque_in_this_rep);
	}
	return found;
}

void *find_co_object_opaque(const void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out/*, int expected_size_words*/)
{
	if (object == NULL)
	{
		return NULL; /* NULL is NULL in all representations */
	}
	
	auto group = group_for_object(const_cast<void*>(object));
	if (!group) 
	{
		fprintf(stderr, "Failed to find co-object group (looking for rep %d) for object at %p (rep %d)\n",
				co_object_rep, object, object_rep);
		return 0; /* caller must use co_object_rec_out to distinguish no-rec case from no-co-obj case */
	}
	else 
	{
		/* If the group is marked for deletion, we should have removed its maps entries. */
		assert(!group->delete_me);
		assert(group->reps[object_rep] == object);
		if (co_object_rec_out) *co_object_rec_out = group/*.get()*/;
		if (group->reps[co_object_rep] == 0)
		{
			/* the co-object in the required form is NULL, so print a message */
			fprintf(stderr, "Found NULL co-object in rep %d of object at %p (rep %d)\n",
				co_object_rep, object, object_rep);
		}
		return group->reps[co_object_rep];
	}
}

static map_t *ensure_map_for_addr(void *addr)
{
	entry *e = entry_for_object(addr);
	if (!e->present) 
	{
		new (&e->m) map_t();
		maps.insert(&e->m); // FIXME: reclaim these
		e->present = 1;
	}
	return &e->m;
}

struct co_object_group *register_co_object(
	void *existing_object, int existing_rep,
	void *co_object, int co_object_rep, int alloc_by, unsigned array_len)
{
	auto group = group_for_object(existing_object);
	assert(group);
	assert(group->reps[co_object_rep] == 0);
	group->reps[co_object_rep] = co_object;
	group->co_object_info[co_object_rep].allocated_by = alloc_by;
	ensure_map_for_addr(co_object)->insert(std::make_pair(co_object, group));
	for (auto i_rep = 0; i_rep < MAX_REPS; ++i_rep)
	{
		if (group->reps[i_rep])
		{
			assert(group->array_len == array_len);
			return group;//.get();
		}
	}
	// if we got here, no array len is recorded, so record it
	group->array_len = array_len;
	
	check_maps_and_groups_consistency();
	
	return group;//.get();
}

struct co_object_group *
new_co_object_record(void *initial_object, int initial_rep, int initial_alloc_by, 
	int is_uninit, unsigned array_len)
{
	assert(!group_for_object(initial_object));
	auto p_m = ensure_map_for_addr(initial_object);
	co_object_group* group = new co_object_group(); //boost::make_shared<co_object_group>();
	bzero(group, sizeof (co_object_group));
	cerr << "Groups previously had size " << groups.size() << endl;
	cerr << "Inserting " << group/*.get()*/ << endl;
	groups.insert(group);
	cerr << "Groups now has size " << groups.size() << endl;
	group->reps[initial_rep] = initial_object;
	group->co_object_info[initial_rep].allocated_by = initial_alloc_by;
	group->co_object_info[initial_rep].initialized = !is_uninit;
	group->array_len = array_len;
	
	p_m->insert(std::make_pair(initial_object, group));
	
	check_maps_and_groups_consistency();
	
	return group/*.get()*/;
}

void sync_all_co_objects(addr_change_cb_t cb, void* cb_arg, int from_rep, int to_rep, ...)
{
	/* We allow the caller to pass a list of objects (in the from_rep domain) and the
	 * conversion functions that should be used for them, terminated by "NULL, NULL, NULL".
	 * How does this interact with the init rule stuff? HMM. The caller is selecting
	 * a specific function, so the caller should know whether we want to initialize
	 * or not. BUT the caller is not necessarily in a place to find out! SO the caller
	 * actually passes *three* arguments: object, conv, init. */
	
	struct val
	{
		conv_func_t conv;
		conv_func_t init;
	};
	std::map<void*, val> overrides;
	va_list args;
	va_start(args, to_rep);
	void *obj;
	conv_func_t conv;
	conv_func_t init;
	do
	{
		obj = va_arg(args, __typeof(obj));
		conv = va_arg(args, __typeof(conv));
		init = va_arg(args, __typeof(init));
		if (obj)
		{
			assert(conv && init);
			overrides[obj] = (struct val) { conv, init };
		}
	} while (!(!obj && !conv && !init));
	va_end(args);
	 
	cerr << "Walking the set of " << groups.size() << " groups." << endl;
	for (auto i_group = groups.begin(); i_group != groups.end(); ++i_group)
	{
		if (!(*i_group)->delete_me && /*!i_group->expired()*/ true)
		{
			auto p_group = (*i_group);//.lock();
			cerr << "Group record at " << p_group;
			if (p_group->reps[from_rep] && p_group->reps[to_rep])
			{
				cerr << " has rep " << get_component_name_for_rep(from_rep) << "(" << from_rep << ")"
					<< " at " << p_group->reps[from_rep]
					<< " and rep " << get_component_name_for_rep(to_rep) << "(" << to_rep << ")"
					<< " at " << p_group->reps[to_rep] << endl;
				/*rep_conv_funcs[from_rep][to_rep][p->form](p->reps[from_rep], p->reps[to_rep]);*/
				bool is_overridden = (overrides.find(p_group->reps[from_rep]) != overrides.end());
				
				/* objects can be their own co-objects in other reps, using the 
				 * opaqueness feature. In this case we don't run default corresps,
				 * but we do run overrides. This is a bit of a hack -- the real fix
				 * is to make convs work correctly when run on aliased ptrs.
				 * Only the pointer handling is particularly difficult.  */
				if (!is_overridden
					&& p_group->reps[from_rep] == p_group->reps[to_rep]) continue;
				
				/* from_rep may or may not be initialized. */
				if (p_group->co_object_info[from_rep].initialized)
				{
					if (p_group->co_object_info[to_rep].initialized)
					{
						auto p_func = is_overridden 
							? overrides[p_group->reps[from_rep]].conv 
							: get_rep_conv_func(
								from_rep, to_rep, 
								p_group->reps[from_rep],
								p_group->reps[to_rep]
							);
						p_func(p_group->reps[from_rep], p_group->reps[to_rep]);
					}
					else // not initialized
					{
						/* A quirk here is that init rule conversion functions are allowed 
						 * to replace their co-object with a user-allocated one. Wrapper
						 * functions might have the old addr on their stack, so we notify
						 * them using a callback. 
						 * FIXME: in general, this is quite hard. How to we identify all
						 * copies of the pointer that have been issued so far? To stop the
						 * old pointer value escaping, we want to run these init rules as
						 * soon as possible. We can improve things by doing all the inits
						 * first. But what about inits that depend on inits? We can push this
						 * to the user: the user shouldn't write co-object replacements that
						 * have dependencies being initialized in the same wrapper.
						 * That's pretty horrible, but it will have to do for now. */
						void *old_to_rep = p_group->reps[to_rep];
						auto p_func = is_overridden 
							? overrides[p_group->reps[from_rep]].init 
							: get_init_func(
								from_rep, to_rep, 
								p_group->reps[from_rep],
								p_group->reps[to_rep]
							);
						p_func(p_group->reps[from_rep], p_group->reps[to_rep]);
						if (p_group->reps[to_rep] != old_to_rep) 
						{
							fprintf(stderr, 
								"Init rule at %p replaced its co-object.\n", p_func);
							if (cb) cb(cb_arg, old_to_rep, p_group->reps[to_rep]);
							fprintf(stderr,
								"After callback, rep 0 is %p and rep 1 is %p\n",
								p_group->reps[0], p_group->reps[1]);
						}

						p_group->co_object_info[to_rep].initialized = 1;
					}
				}
			}
		}
		else
		{
// 			cerr << "Groups previously had size " << groups.size() 
// 				<< "; erasing group at " << /*i_group->lock().get()*/ *i_group << endl;
// 			groups.erase(i_group); // means an expired/deallocated group
// 			cerr << "Groups now has size " << groups.size() << endl;
		}
	} /* end for i_group */
	
	/* Now walk again, erasing any items that should be deleted. */
	auto i_group = groups.begin();
	while (i_group != groups.end())
	{
		if ((*i_group)->delete_me)
		{
			fprintf(stderr, "Erasing group at %p\n", *i_group);
			i_group = groups.erase(i_group);
		} else ++i_group;
	}
	fprintf(stderr, "After purging deleted groups, groups has size %u\n", groups.size());
	check_maps_and_groups_consistency();
}

int mark_object_as_initialized(void *object, int rep)
{
	auto group = group_for_object(object);
	if (!group) 
	{
		fprintf(stderr, "Failed to find co-object record for %p in rep %d\n",
				object, rep);
		return -1;
	}
	else 
	{
		assert(group->reps[rep] == object);
		int old_initialized = group->co_object_info[rep].initialized;
		group->co_object_info[rep].initialized = 1;
		return old_initialized;
	}
}

void allocate_co_object_idem(void *object, int object_rep, int co_object_rep, int is_leaf)
{
	if (object == NULL) return; /* nothing to do */
	
	void *co_object;
	struct co_object_group *co_object_rec = NULL;
	
	co_object = find_co_object(object, object_rep, co_object_rep, &co_object_rec);
	fprintf(stderr, "Co-object search for co-obj of %p (rep %d) in rep %d yielded %p\n",
		object, object_rep, co_object_rep, co_object);
	if (co_object != NULL) return; /* the co-object already exists */	
	fprintf(stderr, "No co-object yielded, so allocating a new one.\n");
	
	/* the co-object doesn't exist, so allocate it -- use calloc for harmless defaults */
	unsigned size;
	unsigned count;
	get_co_object_size(object, object_rep, co_object_rep, &size, &count);
	co_object = calloc(count, size);
	
	/* tell the image what the type of this heap object is */
	if (co_object == NULL) { fprintf(stderr, "Error: malloc failed\n"); exit(42); }
	fprintf(stderr, "Allocated a co-object in rep %d for object at %p (rep %d), co-object at %p\n", 
	        co_object_rep, object, object_rep, co_object);
	
	/* we *may* need a new co object record */
	if (!co_object_rec) co_object_rec = new_co_object_record(
		object, object_rep, ALLOC_BY_USER, is_leaf, count);
	
	/* add info about the newly allocated co-object */
	register_co_object(object, object_rep,
		co_object, co_object_rep, ALLOC_BY_REP_MAN, count);
	
	/* tell the image what the type of this heap object is */
	set_co_object_type(object, object_rep, co_object, co_object_rep);
	
	check_maps_and_groups_consistency();
}

void *replace_co_object(void *existing_obj, 
	void *new_obj, 
	int existing_rep, 
	int require_other_rep)
{
	cerr << "Asked to replace existing co_object " << existing_obj
		<< " with object at " << new_obj
		<< " (for rep " << get_component_name_for_rep(existing_rep) << "[" << existing_rep << "], "
		<< " other rep " << get_component_name_for_rep(require_other_rep) 
		<< "[" << require_other_rep << "] with object ";

	struct co_object_group *found_existing_group;	 
	void *found = find_co_object(existing_obj, 
	   	existing_rep, 
		require_other_rep, 
		&found_existing_group);
	if (found)
	{
		cerr << found_existing_group->reps[require_other_rep] << ")" << endl;
		assert(found_existing_group->reps[require_other_rep]); // must be nonnull
		assert(found_existing_group->reps[existing_rep] == existing_obj);
		cerr << "Found existing co-object." << endl;
		// free old co-object if we allocated ti
		if (found_existing_group->reps[existing_rep] &&
			found_existing_group->co_object_info[existing_rep].allocated_by
			 == ALLOC_BY_REP_MAN)
		{
			cerr << "Freeing unwanted old co-object at " 
				<< found_existing_group->reps[existing_rep]
				<< endl;
			free(found_existing_group->reps[existing_rep]);
		}
		//found_existing_group->reps[existing_rep] = new_obj;
		//found_existing_group->co_object_info[existing_rep].allocated_by = ALLOC_BY_USER;
		//found_existing_group->co_object_info[existing_rep].initialized = 1;
		
		// delete the old co-obj record
		ensure_map_for_addr(existing_obj)->erase(existing_obj);
		// zero the old group entry
		found_existing_group->reps[existing_rep] = 0;
		// forget any descr
		pmirror::self.forget_heap_object_descr((process_image::addr_t) existing_rep);
		// now co-object search for the object we're replacing should yield null
		assert(!find_co_object(found_existing_group->reps[require_other_rep], require_other_rep,
			existing_rep, 0));
		
		auto p_rec = register_co_object(
			found_existing_group->reps[require_other_rep], // i.e. the obj we're co-obj of
			require_other_rep,
			new_obj, // i.e. the new co-object
			existing_rep,
			ALLOC_BY_USER,
			found_existing_group->array_len
		);
		p_rec->co_object_info[existing_rep].initialized = 1;
		assert(p_rec == found_existing_group);

		// sanity check that the existing object has really gone away
		// 1. co-object search for it yields nothing
		assert(find_co_object(existing_obj, existing_rep, require_other_rep, 0) == 0);
		// 2. no group contains it
		for (auto i_group = groups.begin(); i_group != groups.end(); ++i_group)
		{
			assert((*i_group)->reps[existing_rep] != existing_obj);
		}

		cerr << "Successfully replaced" << endl;
		check_maps_and_groups_consistency();
		return new_obj;
	}
	cerr << ")" << endl << "Did not find record, so not replacing." << endl;
	return existing_obj;
}

void ensure_opaque_co_obj_in_this_rep(void *ptr, int opaque_in_this_rep)
{
	assert(ptr);
	
	auto found_map_ent = ensure_map_for_addr(ptr)->find(ptr);
	assert(found_map_ent != ensure_map_for_addr(ptr)->end());
	auto p_group = found_map_ent->second;
	
	// relaxation: don't make the caller supply another rep
	//assert(p_group->reps[other_valid_rep] == ptr);
	// relaxation: we don't have to give the non-opaque rep as argument; any will do
	//assert(!p_group->co_object_info[nonopaque_in_this_rep].opaque_in_this_rep);
	
	// relaxation: if we already have a rep, do nothing (but it should be this ptr)
	if (!p_group->reps[opaque_in_this_rep])
	{
		p_group->reps[opaque_in_this_rep] = ptr;
		p_group->co_object_info[opaque_in_this_rep]
		 = (struct co_object_info) {
			 /*.allocated_by =*/ ALLOC_BY_USER, 
			 /*.initialized =*/ true, 
			 /*.opaque_in_this_rep =*/ true
		};
	} else assert(p_group->reps[opaque_in_this_rep] == ptr);
}

void ensure_allocating_component_has_rep_of(void *obj)
{
	int allocating_rep_id = allocating_component(obj);

	auto found_group = group_for_object(obj);
	if (found_group)
	{
		/* We just check that the group is set up appropriately. */
		assert(found_group->reps[allocating_rep_id] == obj);
		return;
	}
	else
	{
		struct co_object_group *group = new_co_object_record(
			obj,
			allocating_rep_id,
			ALLOC_BY_USER,
			/* is_uninit */ false, 
			1 // FIXME
			);
		ensure_map_for_addr(obj)->insert(std::make_pair(obj, group));
		check_maps_and_groups_consistency();
	}
}
