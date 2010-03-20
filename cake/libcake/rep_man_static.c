#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rep_man-shared.h"
#include "rep_man_tables.h"

/* extern const struct co_object_group *const STATIC_HEAD; */
extern const struct co_object_group cog_rightclick;
struct co_object_group *head = &cog_rightclick; /* hard-wire the static entries */

int invalidate_co_object(void *object, int rep)
{
	struct co_object_group *recp;
	struct co_object_group *prevp;
	void *retval = find_co_object(object, rep, rep, &recp, 0);
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
		struct co_object_group **co_object_rec_out, int expected_size_words)
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
				if (p->co_object_info[co_object_rep].length_words < expected_size_words)
				{
					fprintf(stderr, "Warning: co-object at %p has only %d words stored (form %s, rep %d), less than %d expected by the caller!\n",
							co_object, p->co_object_info[co_object_rep].length_words,
							object_forms[p->form], co_object_rep, expected_size_words);
				}
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
			rep_conv_funcs[from_rep][to_rep][p->form](p->reps[from_rep], p->reps[to_rep]);
		}
		prev = p;
	}
}

void allocate_co_object_idem(int do_not_use, void *object, int form, int object_rep, int co_object_rep)
{
	if (object == NULL) return; /* nothing to do */
	
	void *co_object;
	struct co_object_group *co_object_rec = NULL;
	co_object = find_co_object(object, object_rep, co_object_rep, &co_object_rec,
			FORM_STORED_SIZE(co_object_rep, form));
	if (co_object != NULL) return; /* the co-object already exists */	
	
	/* the co-object doesn't exist, so allocate it -- use calloc for harmless defaults */
	co_object = calloc(1, object_rep_layout_sizes[co_object_rep][form]);
	if (co_object == NULL) { fprintf(stderr, "Error: malloc failed\n"); exit(42); }
	fprintf(stderr, "Allocating a co-object in rep %d for object at %p (rep %d, form %s)\n", co_object_rep, object, object_rep, object_forms[form]);
	/* also add it to the list! */
	if (co_object_rec == NULL)
	{
		co_object_rec = calloc(1, sizeof (struct co_object_group)); 
		if (co_object_rec == NULL) { fprintf(stderr, "Error: malloc failed\n"); exit(42); }
		/* add info about the existing object */
		co_object_rec->reps[object_rep] = object;
		co_object_rec->co_object_info[object_rep].length_words = FORM_STORED_SIZE(object_rep, form);
		co_object_rec->co_object_info[object_rep].allocated_by = ALLOC_BY_USER; /* not by us */
		co_object_rec->form = form;
		co_object_rec->next = head;
		head = co_object_rec;
	}
	/* add info about the newly allocated co-object */
	co_object_rec->reps[co_object_rep] = co_object;
	co_object_rec->co_object_info[co_object_rep].length_words = FORM_STORED_SIZE(co_object_rep,form);
	co_object_rec->co_object_info[co_object_rep].allocated_by = ALLOC_BY_REP_MAN; /* by us */
}

void do_nothing(int do_not_use, void *object, int form, int object_rep, int co_object_rep)
{
	/* Used for testing walk_bfs */
}

void init_co_object_from_object(int object_rep, void *object,
		int co_object_rep, void *co_object, int form)
{
	/* Initialising an object: we must initialise all cared-about fields. It doesn't matter
	 * what order we do the contents and the pointers, as long as the pointed-to objects
	 * have already been allocated. Our helper functions *only* do contents and opaque
	 * pointers; they don't do deep copying. They *do* do subobjects. Rather, they handle
	 * deep-copying by assuming that a co-object has already been allocated, that it may
	 * or may not have been initialised (but eventually will be); they simply look it up. */
	
	rep_conv_funcs[object_rep][co_object_rep][form](object, co_object);
}

void init_co_object(int do_not_use, void *object, int form, int from_rep, int to_rep)
{
	init_co_object_from_object(from_rep, object, to_rep, 
			find_co_object(object, from_rep, to_rep, NULL, FORM_STORED_SIZE(to_rep, form)), form);
}

int object_is_live(struct co_object_group *rec)
{
	return 1;
	/* This function tells us whether a co-object (any rep) represents a live object.
	 * When retrieving a co-object, we should check that it represents a live object.
	 * An object is not live if any of its representations has been deallocated. For
	 * heap objects, this happens when free() is called. For stack objects, this happens
	 * when the creating stack frame is deallocated. For static objects, this never
	 * happens. 
	 
	 * We detect stack deallocation approximately by testing whether the allocating
	 * frame still exists at a given address, and still refers to an instance of the
	 * same function. This fails to detect cases where another instance of the same
	 * function has started up again at the same place on the stack. We believe this
	 * can safely be ignored. The only harm (apart from wasted memory) could come if
	 * we write to the stack location, believing it to be the co-object that has actually
	 * been deallocated. The only way this could happen is if we're
	 
	 * HMM. Actually this is a problem. Suppose a call-out and return leaves us with a
	 * co-object allocated for a particular stack object, but that stack object was
	 * deallocated after the return. Now we do a sync_all on the return path of some 
	 * completely unrelated call-out ***from the same calling function*** 
	 * (but taking a different path within that function) and clobber the stack location,
	 * just because it happens to
	 * - be at the same location as a previous stack frame activation of that function
	 * - and therefore contain the same kind of object, but one completely unrelated
	 *     as far as the program logic is concerned.
	 *
	 /* void f()
		{
			struct my_object o;
			if (some_cond)
			{
				o.field = blah;
				call_rep_mismatched_library(&o); // redirects to __wrap_..., and allocates co_o
				return; // o is deallocated, but co_o still hangs around
			}
			else
			{
				o.field = some_other_blah;
				another_call_to_rep_mismatched_library(); // may or may not pass o....
				local_call(&o); // this call will see a clobbered version of o! (o.field == blah)
					// ... assuming we did sync_all on the return, being conservative
					// If instead we were precise with our sync, then there *would* be no
					// problem, because (hint: this reasoning is WRONG! there IS a problem)
					// -- only objects that were mutated were synced, and
					// -- if we mutated co_o, then it's because
					//    -- we were passed the new o, in which case co_o got updated from that; *** but which fields? all fields? opacity comes in again
					//	  -- the target code saved a pointer to co_o from earlier, which is a bug because
					//       the earlier o got deallocated (so it wasn't okay to save this ptr)
					//       *** could use a pointer annotation to capture this interface characteristic:
					//			 for each pointer argument,
					//			 we want a flag to say "okay to save"
					//			(strictly speaking "okay to save until frame X deallocated")
					//			(this is a statement about the *future validity* of a pointer, i.e.
					//			 its lifetime, and can be expressed by linking it to the lifetime of
					//			 another object, where stack frames (activations) are objects)
					}
		}
		int main ()
		{
			f();
			f();
		}
					
	 *
	
	 */
}

int stack_object_is_live(void *object, struct co_object_group *rec, int rep)
{
	return 1;
}

int heap_object_is_live(void *object, struct co_object_group *rec, int rep)
{
	return 1;
}
int static_object_is_live(void *object, struct co_object_group *rec, int rep)
{
	return 1;
}


/*
void init_callee_co_object_from_caller_object(int do_not_use, void *object, int form)
{
	init_co_object_from_object(CALLER_REP, object, CALLEE_REP, 
			find_co_object(object, CALLER_REP, CALLEE_REP, NULL, FORM_STORED_SIZE(CALLEE_REP, form)), form);
}*/
