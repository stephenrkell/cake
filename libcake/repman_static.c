#define _GNU_SOURCE /* so that we get MAP_ANONYMOUS when including malloc.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include "repman.h"
#include <heap_index.h> // from libheap_index_hooks

/* static co-object relation -- FIXME: reinstate this */
//struct co_object_group *head = &__libcake_first_co_object_group; /* hard-wire the static entries */

/* counter used to issue rep ids */
int next_rep_id;
const char *rep_component_names[MAX_REPS];

const char *get_component_name_for_rep(int rep)
{
	assert(next_rep_id <= MAX_REPS);
	return rep_component_names[rep];
}

int get_rep_for_component_name(const char *name)
{
	for (int i = 0; i < next_rep_id; i++)
	{
		assert(i < MAX_REPS);
		if (strcmp(rep_component_names[i], name) == 0) return i;
	}
	return -1;
}

void do_nothing(void *object, int object_rep, int co_object_rep, int is_leaf)
{
	/* Used for testing walk_bfs */
}

void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object, int is_leaf)
{
	/* Initialising an object: we must initialise all cared-about fields. It doesn't matter
	 * what order we do the contents and the pointers, as long as the pointed-to objects
	 * have already been allocated. Our helper functions *only* do contents and opaque
	 * pointers; they don't do deep copying. They *do* do subobjects. Rather, they handle
	 * deep-copying by assuming that a co-object has already been allocated, that it may
	 * or may not have been initialised (but eventually will be); they simply look it up. */
	
	assert(0);
	get_rep_conv_func(object_rep, co_object_rep, object, co_object)(object, co_object);
}

void init_co_object(void *object, int from_rep, int to_rep, int is_leaf)
{
	init_co_object_from_object(
			from_rep, object, to_rep, 
			find_co_object(object, from_rep, to_rep, NULL),
			is_leaf);
}

int object_is_live(struct co_object_group *rec)
{
	return 1;
}
