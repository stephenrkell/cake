#define MAX_REPS 8

#define REP_GTK_12 0
#define REP_GTK_20 1
/* const int REPS_REP_GTK_12 = REP_GTK_12; */
/* const int REPS_REP_GTK_20 = REP_GTK_20; */

#include "rep_man_forms.h"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#define FORM_STORED_SIZE(r,f) (MIN(object_rep_layout_sizes[(r)][(f)] / sizeof (int), 4095))

#define ALLOC_BY_REP_MAN 0
#define ALLOC_BY_USER 1

typedef void (*rep_sync_func_t)(void *from, void *to);
typedef void (*rep_conv_func_t)(void *from, void *to);

typedef struct opacity_bitmask {
	const unsigned char len_words;
	const int *mask;
} opacity_bitmask_t;

struct co_object_info {
	unsigned length_words :12; /* support up to 4095-word objects */
	unsigned opacity_map_index: 22; /* index into opacity map -- not used for now */
		/* opacity map is a separate structure having entries of one bit per word
		 * i.e. up to 4095 words (so max entry length 4095 bits <= 512 bytes). 
		 * A bit set denotes that that word is treated non-opaquely in that rep. */
	unsigned allocated_by:1; /* 0 == by rep_man; 1 == by user code */
	
	/* function pointer to a liveness test for this object*/
/*	int (*test_is_live)(void *object, struct co_object_info *rec, int rep);
	union {
		struct {
			void *containing_frame_base;
			void *containing_frame_function;
		} stack_liveness;
		struct {} heap_liveness;
		struct {} static_liveness;
	} liveness_data; */
};

/* Table of co-object groups; each group is an array */
/* TODO: use a less stupid data structure (C++ map maybe) */
struct co_object_group {
	void *reps[MAX_REPS];
	int form;
	/*int lifetime_master; /* which rep determines the object liveness, 
							or -1 for no master (test all)
							or -2 for no lifetime (always live) */
	struct co_object_info co_object_info[MAX_REPS];
	struct co_object_group *next;
};

int stack_object_is_live(void *object, struct co_object_group *rec, int rep);
int heap_object_is_live(void *object, struct co_object_group *rec, int rep);
int static_object_is_live(void *object, struct co_object_group *rec, int rep);

int object_is_live(struct co_object_group *rec);
int invalidate_co_object(void *object, int rep);
void *find_co_object(void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out, int expected_size_words);		
void allocate_co_object_idem(int do_not_use, void *object, int form, int object_rep, int co_object_rep);
void allocate_co_object_idem_caller_rep(int do_not_use, void *object, int form);
void allocate_co_object_idem_callee_rep(int do_not_use, void *object, int form);
void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object, int form);
/*void init_caller_co_object_from_callee_object(int do_not_use, void *object, int form);
void init_callee_co_object_from_caller_object(int do_not_use, void *object, int form);*/
void init_co_object(int do_not_use, void *object, int form, int from_rep, int to_rep);
void sync_all_co_objects(int from_rep, int to_rep);
