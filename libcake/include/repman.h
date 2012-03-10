#include <sys/types.h>

#ifndef LIBCAKE_REPMAN_H_
#define LIBCAKE_REPMAN_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define MAX_REPS 8
extern int next_rep_id; // the next rep ID to issue
extern const char *rep_component_names[MAX_REPS];

#define ALLOC_BY_REP_MAN 0
#define ALLOC_BY_USER 1

typedef long wordsize_integer;
typedef void *(*conv_func_t)(void *, void *);

void *noop_conv_func(void *, void *);

struct co_object_info {
	unsigned allocated_by:1; /* 0 == by rep_man; 1 == by user code */
	unsigned initialized:1;
} __attribute__((packed));

/* Table of co-object groups; each group is an array */
struct co_object_group {
	void *reps[MAX_REPS];
	/* int form; */
	struct co_object_info co_object_info[MAX_REPS];
	/* struct co_object_group *next; */
};

extern void *components_table;
extern void *component_pairs_table;
extern int components_table_inited;
extern int component_pairs_table_inited;

/* Initialization -- note that these are NOT constructors. */
void init_components_table(void);
void init_component_pairs_table(void);

/* Utility code. */
void print_object(const void *);

/* co-object bookkeeping interface */
int invalidate_co_object(void *object, int rep);
void *find_co_object(void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out/*, int expected_size_words*/);		
struct co_object_group *new_co_object_record(void *initial_object, int initial_rep, 
	int initial_alloc_by, int is_uninit);
int mark_object_as_initialized(void *, int);
int object_is_live(struct co_object_group *rec);

/* initialization and synchronisation */
void set_co_object_type(void *object, int obj_rep, void *co_object, int co_obj_rep);
void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object, int is_leaf);
void sync_all_co_objects(int from_rep, int to_rep, ...);
void *replace_co_object(void *existing_obj, void *new_obj, int existing_rep, int require_other_rep);

/* object graph walking */
void walk_bfs(int object_rep, void *object, int co_object_rep,
	void (*on_blacken)(void*, int, int, int), int arg_n_minus_1, int arg_n, int object_is_leaf);

/* Useful callbacks for graph walking */
void allocate_co_object_idem(void *object, int object_rep, int co_object_rep, int is_leaf);
/* FIXME: do we use these? */
void init_co_object(void *object, int from_rep, int to_rep, int is_leaf);
void allocate_co_object_idem_caller_rep(int do_not_use, void *object, int form, int is_leaf);
void allocate_co_object_idem_callee_rep(int do_not_use, void *object, int form, int is_leaf);

/* Table lookups */
conv_func_t get_rep_conv_func(int from_rep, int to_rep, void *source_object, void *target_object);
conv_func_t get_init_func(int from_rep, int to_rep, void *source_object, void *target_object);
size_t get_co_object_size(void *obj, int obj_rep, int co_obj_rep);

/* Components table lookups */
const char *get_component_name_for_rep(int rep);
int get_rep_for_component_name(const char *name);

#if defined(__cplusplus) || defined(c_plusplus)
} /* end extern "C" */
#endif

#endif
