#define _GNU_SOURCE /* so that we get MAP_ANONYMOUS when including malloc.h */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include "repman.h"
#include <processimage/heap_index.h>

/* static co-object relation */

// extern const struct co_object_group *const STATIC_HEAD; 
extern const struct co_object_group __libcake_first_co_object_group __attribute__((weak));
struct co_object_group *head = &__libcake_first_co_object_group; /* hard-wire the static entries */

/* counter used to issue */
int next_rep_id;

int invalidate_co_object(void *object, int rep)
{
	struct co_object_group *recp;
	struct co_object_group *prevp;
	void *retval = find_co_object(object, rep, rep, &recp /*, 0*/);
	if (retval == NULL) return -1; /* co-object not found */
	prevp = head;
	while (prevp != NULL && prevp->next != NULL && prevp->next != recp) prevp = prevp->next;
	assert(prevp->next == recp); /* ERROR: recp really should be in the list */
	
	/* Remove this co-object from the list. */
	prevp->next = recp->next;
	
	/* Deallocate any co-objects in rec that were alloc'd by us. */
	for (int i = 0; i < MAX_REPS; i++)
	{
		if (recp->co_object_info[i].allocated_by == ALLOC_BY_REP_MAN)
		{
			free(recp->reps[i]);
		}
	}
	
	/* Deallocate the list node */
	free(recp);
	
	return 0;
}

void *find_co_object(void *object, int object_rep, int co_object_rep, 
		struct co_object_group **co_object_rec_out/*, int expected_size_words*/)
{
	if (object == NULL)
	{
		return NULL; /* NULL is NULL in all representations */
	}
	void *co_object = NULL;
	/* fprintf(stderr, "Searching for co-object in rep %d of object at %p (rep %d)\n",
			co_object_rep, object, object_rep); */
	
	/* FIXME: use a less stupid search algorithm */
	for (struct co_object_group *p = head; p != NULL; p = p->next)
	{
		if (p->reps[object_rep] == object)
		{
			if (co_object_rec_out != NULL) *co_object_rec_out = p; /* pass output parameter */
			co_object = p->reps[co_object_rep];
			if (co_object != NULL)
			{
				/* the co-object already exists, so return it */
				/* fprintf(stderr, "Found co-object at %p in rep %d of object at %p (rep %d, form %s)\n",
					co_object, co_object_rep, object, object_rep, object_forms[p->form]); */
				/* length check */
				//if (p->co_object_info[co_object_rep].length_words < expected_size_words)
				//{
				//	fprintf(stderr, "Warning: co-object at %p has only %d words stored (rep %d), less than %d expected by the caller!\n",
				//			co_object, p->co_object_info[co_object_rep].length_words,
				//			co_object_rep, expected_size_words);
				//}
				return co_object;
			}
			else
			{
				/* the co-object in the required form is NULL, so return that */
				fprintf(stderr, "Found null co-object in rep %d of object at %p (rep %d)\n",
					co_object_rep, object, object_rep);
				return NULL;
			}
		}
	}
	fprintf(stderr, "Failed to find co-object in rep %d of object at %p (rep %d)\n",
			co_object_rep, object, object_rep);
	return NULL; /* caller must use co_object_rec_out to distinguish no-rec case from no-co-obj case */
}

void sync_all_co_objects(int from_rep, int to_rep)
{
	struct co_object_group *p = NULL;
	struct co_object_group *prev = NULL;
	for (p = head; p != NULL; p = p->next)
	{
		if (p->reps[from_rep] != NULL && p->reps[to_rep] != NULL)
		{
			/*rep_conv_funcs[from_rep][to_rep][p->form](p->reps[from_rep], p->reps[to_rep]);*/
            get_rep_conv_func(from_rep, to_rep, p->reps[from_rep])(p->reps[from_rep], p->reps[to_rep]);
		}
		prev = p;
	}
}

void allocate_co_object_idem(void *object, int object_rep, int co_object_rep)
{
	if (object == NULL) return; /* nothing to do */
	
	void *co_object;
	struct co_object_group *co_object_rec = NULL;
	co_object = find_co_object(object, object_rep, co_object_rep, &co_object_rec/*,
			FORM_STORED_SIZE(co_object_rep, form)*/);
	if (co_object != NULL) return; /* the co-object already exists */	
	
	/* the co-object doesn't exist, so allocate it -- use calloc for harmless defaults */
	co_object = calloc(1, get_co_object_size(object, object_rep, co_object_rep));
	if (co_object == NULL) { fprintf(stderr, "Error: malloc failed\n"); exit(42); }
	fprintf(stderr, "Allocating a co-object in rep %d for object at %p (rep %d)\n", 
            co_object_rep, object, object_rep);
	/* also add it to the list! */
	if (co_object_rec == NULL)
	{
		co_object_rec = calloc(1, sizeof (struct co_object_group)); 
		if (co_object_rec == NULL) { fprintf(stderr, "Error: malloc failed\n"); exit(42); }
		/* add info about the existing object */
		co_object_rec->reps[object_rep] = object;
		co_object_rec->co_object_info[object_rep].allocated_by = ALLOC_BY_USER; /* not by us */
		/* co_object_rec->form = form; */
		co_object_rec->next = head;
		head = co_object_rec;
	}
	/* add info about the newly allocated co-object */
	co_object_rec->reps[co_object_rep] = co_object;
	co_object_rec->co_object_info[co_object_rep].allocated_by = ALLOC_BY_REP_MAN; /* by us */
}

void do_nothing(void *object, int object_rep, int co_object_rep)
{
	/* Used for testing walk_bfs */
}

void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object)
{
	/* Initialising an object: we must initialise all cared-about fields. It doesn't matter
	 * what order we do the contents and the pointers, as long as the pointed-to objects
	 * have already been allocated. Our helper functions *only* do contents and opaque
	 * pointers; they don't do deep copying. They *do* do subobjects. Rather, they handle
	 * deep-copying by assuming that a co-object has already been allocated, that it may
	 * or may not have been initialised (but eventually will be); they simply look it up. */
	
	get_rep_conv_func(object_rep, co_object_rep, object)(object, co_object);
}

void init_co_object(void *object, int from_rep, int to_rep)
{
	init_co_object_from_object(
			from_rep, object, to_rep, 
			find_co_object(object, from_rep, to_rep, NULL));
}

int object_is_live(struct co_object_group *rec)
{
	return 1;
}
