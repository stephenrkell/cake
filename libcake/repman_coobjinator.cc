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
#include "processimage/memtable.h"
#include "repman.h"
}
#include <map>
#include <set>
#include <cstdio>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
typedef std::map<void*, boost::shared_ptr<co_object_group> > map_t;
struct entry {
	unsigned present:1;
	map_t m;
} __attribute__((packed));	

entry *coobjinator_region;
#define entry_coverage_in_bytes (64*1024*1024)
// 64MB regions; there are 2^38 such in a 64-bit address space

using boost::shared_ptr;

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

static shared_ptr<co_object_group> group_for_object(void *obj)
{
	entry *e = entry_for_object(obj);
	if (e && e->present)
	{
		auto found = e->m.find(obj);
		if (found != e->m.end()) return found->second;
		else return shared_ptr<co_object_group>();
	}
	else return shared_ptr<co_object_group>();
}


/* This is called whenever a co-object is freed. We assume that this should free
 * all other co-objects. So we delete the whole group. */
/* Using the shared_ptr implementation of groups, it's not necessary to explicitly
 * delete the group record. But we should remove the map entries that point to it,
 * and delete any objects we allocated. */
int invalidate_co_object(void *object, int rep)
{
	auto group = group_for_object(object);
	if (!group || !group->reps[rep] || group->reps[rep] != object) return -1; /* co-object not found */

	/* Using the shared_ptr implementation of groups, it's not necessary to explicitly
	 * delete the group record. But we should remove the map entries that point to it,
	 * and delete any objects we allocated. */

	for (int i = 0; i < MAX_REPS; i++)
	{
		if (group->reps[i])
		{
			map_ptr_for_object(group->reps[i])->erase(object);
			if (group->co_object_info[i].allocated_by == ALLOC_BY_REP_MAN)
			{
				free(group->reps[i]);
			}
		}
	}
	
	return 0;
}

void *find_co_object(void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out/*, int expected_size_words*/)
{
	if (object == NULL)
	{
		return NULL; /* NULL is NULL in all representations */
	}
	
	auto group = group_for_object(object);
	if (!group) 
	{
		fprintf(stderr, "Failed to find co-object in rep %d of object at %p (rep %d)\n",
				co_object_rep, object, object_rep);
		return 0; /* caller must use co_object_rec_out to distinguish no-rec case from no-co-obj case */
	}
	else 
	{
		assert(group->reps[object_rep] == object);
		*co_object_rec_out = group.get();
		if (group->reps[co_object_rep] == 0)
		{
			/* the co-object in the required form is NULL, so print a message */
			fprintf(stderr, "Found null co-object in rep %d of object at %p (rep %d)\n",
				co_object_rep, object, object_rep);
		}
		return group->reps[co_object_rep];
	}
}

static std::set<map_t*> maps; // FIXME: reclaim these
static std::set<boost::weak_ptr<co_object_group> > groups;

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
	void *co_object, int co_object_rep, int alloc_by)
{
	auto group = group_for_object(existing_object);
	assert(group);
	group->reps[co_object_rep] = co_object;
	group->co_object_info[co_object_rep].allocated_by = alloc_by;
	ensure_map_for_addr(co_object)->insert(std::make_pair(co_object, group));
}

struct co_object_group *
new_co_object_record(void *initial_object, int initial_rep, int initial_alloc_by)
{
	assert(!group_for_object(initial_object));
	auto p_m = ensure_map_for_addr(initial_object);
	auto group = boost::make_shared<co_object_group>();
	groups.insert(group);
	group->reps[initial_rep] = initial_object;
	group->co_object_info[initial_rep].allocated_by = initial_alloc_by;
	
	p_m->insert(std::make_pair(initial_object, group));
	
	return group.get();
}

void sync_all_co_objects(int from_rep, int to_rep)
{
	for (auto i_group = groups.begin(); i_group != groups.end(); i_group++)
	{
		if (!i_group->expired())
		{
			auto p_group = (*i_group).lock();
			if (p_group->reps[from_rep] && p_group->reps[to_rep])
			{
				/*rep_conv_funcs[from_rep][to_rep][p->form](p->reps[from_rep], p->reps[to_rep]);*/
				if (p_group->co_object_info[to_rep].initialized)
				{
					get_rep_conv_func(
						from_rep, to_rep, 
						p_group->reps[from_rep],
						p_group->reps[to_rep]
					)(p_group->reps[from_rep], p_group->reps[to_rep]);
				}
				else // not initialized
				{
					get_init_func(
						from_rep, to_rep, 
						p_group->reps[from_rep],
						p_group->reps[to_rep]
					)(p_group->reps[from_rep], p_group->reps[to_rep]);
					
					p_group->co_object_info[to_rep].initialized = 1;
				}
			}
		}
		else
		{
			groups.erase(i_group); // means an expired/deallocated group
		}
	}
}

void allocate_co_object_idem(void *object, int object_rep, int co_object_rep)
{
	if (object == NULL) return; /* nothing to do */
	
	void *co_object;
	struct co_object_group *co_object_rec = NULL;
	
	co_object = find_co_object(object, object_rep, co_object_rep, &co_object_rec);
	if (co_object) return; /* the co-object already exists */	
	
	/* the co-object doesn't exist, so allocate it -- use calloc for harmless defaults */
	co_object = calloc(1, get_co_object_size(object, object_rep, co_object_rep));
	/* tell the image what the type of this heap object is */
	if (co_object == NULL) { fprintf(stderr, "Error: malloc failed\n"); exit(42); }
	fprintf(stderr, "Allocating a co-object in rep %d for object at %p (rep %d)\n", 
            co_object_rep, object, object_rep);
	
	/* we *may* need a new co object record */
	if (!co_object_rec) co_object_rec = new_co_object_record(object, object_rep, ALLOC_BY_USER);
	
	/* add info about the newly allocated co-object */
	register_co_object(object, object_rep,
		co_object, co_object_rep, ALLOC_BY_REP_MAN);
	
	/* tell the image what the type of this heap object is */
	set_co_object_type(object, object_rep, co_object, co_object_rep);
}