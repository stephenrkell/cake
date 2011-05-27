#define MAX_REPS 8
extern int next_rep_id; // the next rep ID to issue

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
/*#define FORM_STORED_SIZE(r,f) (MIN(object_rep_layout_sizes[(r)][(f)] / sizeof (int), 4095))*/
#define FORM_STORED_SIZE(r,f) (MIN(get_object_rep_layout_size((r), (f)) / sizeof (int), 4095))

#define ALLOC_BY_REP_MAN 0
#define ALLOC_BY_USER 1

typedef void (*rep_sync_func_t)(void *from, void *to);
typedef void (*rep_conv_func_t)(void *from, void *to);

struct co_object_info {
	unsigned length_words :12; /* support up to 4095-word objects */
	unsigned allocated_by:1; /* 0 == by rep_man; 1 == by user code */
};

/* Table of co-object groups; each group is an array */
/* TODO: use a less stupid data structure (C++ map maybe) */
struct co_object_group {
	void *reps[MAX_REPS];
	int form;
	struct co_object_info co_object_info[MAX_REPS];
	struct co_object_group *next;
};

int object_is_live(struct co_object_group *rec);
int invalidate_co_object(void *object, int rep);
void *find_co_object(void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out, int expected_size_words);		
void allocate_co_object_idem(int do_not_use, void *object, int form, int object_rep, int co_object_rep);
void allocate_co_object_idem_caller_rep(int do_not_use, void *object, int form);
void allocate_co_object_idem_callee_rep(int do_not_use, void *object, int form);
void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object, int form);
void init_co_object(int do_not_use, void *object, int form, int from_rep, int to_rep);
void sync_all_co_objects(int from_rep, int to_rep);

/* from rep_man_tables.h */

rep_sync_func_t get_rep_conv_func(int from_rep, int to_rep, int form) __attribute__((weak));
/* get_rep_conv_func(from_rep, to_rep, form)(p->reps[from_rep], p->reps[to_rep]);  */
               
size_t get_object_rep_layout_size(int rep, int form) __attribute__((weak));
/* object_rep_layout_sizes[co_object_rep][form]      */
         
int get_subobject_form(int rep, int form, int index) __attribute__((weak));
/* get_subobject_form(rep, start_subobject_form, i) */
                
size_t get_subobject_offset(int rep, int form, int index) __attribute__((weak));
/* get_subobject_offset(rep, start_subobject_form, i) != (size_t) -1; */
                
int get_derefed_form(int rep, int form, int index) __attribute__((weak));
/* get_derefed_form(rep, start_subobject_form, i) */

size_t get_derefed_offset(int rep, int form, int index) __attribute__((weak));
/* get_derefed_offset(rep, start_subobject_form, i) != (size_t) -1; */

const char *get_form_name(int form) __attribute__((weak));
/* get_object_form(start_subobject_form) */
