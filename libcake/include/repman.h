#include <sys/types.h>

#ifndef LIBCAKE_REPMAN_H_
#define LIBCAKE_REPMAN_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define MAX_REPS 8
extern int next_rep_id; // the next rep ID to issue
extern const char *rep_component_names[MAX_REPS];

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
/*#define FORM_STORED_SIZE(r,f) (MIN(object_rep_layout_sizes[(r)][(f)] / sizeof (int), 4095))*/
#define FORM_STORED_SIZE(r,f) (MIN(get_object_rep_layout_size((r), (f)) / sizeof (int), 4095))

#define ALLOC_BY_REP_MAN 0
#define ALLOC_BY_USER 1

typedef long wordsize_integer;
typedef void *(*conv_func_t)(void *, void *);

// TODO: remove these
typedef void (*rep_sync_func_t)(void *from, void *to);
typedef void (*rep_conv_func_t)(void *from, void *to);

struct co_object_info {
	unsigned allocated_by:1; /* 0 == by rep_man; 1 == by user code */
};

/* Table of co-object groups; each group is an array */
/* TODO: use a less stupid data structure (C++ map maybe) */
struct co_object_group {
	void *reps[MAX_REPS];
	/* int form; */
	struct co_object_info co_object_info[MAX_REPS];
	struct co_object_group *next;
};

extern void *components_table;
extern void *component_pairs_table;
extern int components_table_inited;
extern int component_pairs_table_inited;
void init_components_table(void);
void init_component_pairs_table(void);

void print_object(void *);
int object_is_live(struct co_object_group *rec);
int invalidate_co_object(void *object, int rep);
void *find_co_object(void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out/*, int expected_size_words*/);		
void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object);
void sync_all_co_objects(int from_rep, int to_rep);
void walk_bfs(int object_rep, void *object, int co_object_rep,
	void (*on_blacken)(void*, int, int), int arg_n_minus_1, int arg_n);
void allocate_co_object_idem(void *object, int object_rep, int co_object_rep);
void init_co_object(void *object, int from_rep, int to_rep);


void allocate_co_object_idem_caller_rep(int do_not_use, void *object, int form);
void allocate_co_object_idem_callee_rep(int do_not_use, void *object, int form);

/* from rep_man_tables.h */

rep_sync_func_t get_rep_conv_func(int from_rep, int to_rep, void *source_object, void *target_object);
/* get_rep_conv_func(from_rep, to_rep, p->reps[from_rep])(p->reps[from_rep], p->reps[to_rep]);  */
               
size_t get_co_object_size(void *obj, int obj_rep, int co_obj_rep);
/* object_rep_layout_sizes[co_object_rep][form]      */
         
//int get_subobject_form(int rep, int form, int index) __attribute__((weak));
// /* get_subobject_form(rep, start_subobject_form, i) */
                
size_t get_subobject_offset(int rep, int form, int index);
/* get_subobject_offset(rep, start_subobject_form, i) != (size_t) -1; */
                
//int get_derefed_form(int rep, int form, int index) __attribute__((weak));
// /* get_derefed_form(rep, start_subobject_form, i) */

size_t get_derefed_offset(int rep, int form, int index);
/* get_derefed_offset(rep, start_subobject_form, i) != (size_t) -1; */

const char *get_component_name_for_rep(int rep);
int get_rep_for_component_name(const char *name);

// const char *get_form_name(int form) __attribute__((weak));
// /* get_object_form(start_subobject_form) */

#if defined(__cplusplus) || defined(c_plusplus)
} /* end extern "C" */
#endif

#endif
