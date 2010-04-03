#define MAX_REPS 8

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
