#include <stdio.h>

/* Here we want to mock up the sort of code that would be linked 
   when adding a stylistic lens to a component. */

/* Firstly, entry point through a lens. */

void abstracted_call(int arg);
extern void concrete_call(const char *arg); /* the actual provided function */
void abstracted_call(int arg);
void abstracted_call(int arg) /* additional entry point */
{
	/* convert arg */
#define INT_MAX_STRLEN 100
	char concrete_arg[INT_MAX_STRLEN];
	snprintf(concrete_arg, INT_MAX_STRLEN, "%d", arg);

	/* make underlying call */
	concrete_call(concrete_arg);
}

/* For renaming-only transformations, does ELF support
 * symbol aliases? */

/* Now, call-site through a lens. 
 * The idea is that we
 * - recognise (wrap) calls to the original site
 * - test whether anyone linked against the weak symbol we define */

extern void abstracted_outcall(int arg) __attribute__((weak));
extern void __real_concrete_outcall(const char *arg) __attribute__((weak));
void __wrap_concrete_outcall(const char *arg)
{
	/* We're doing this to preserve the compositionality with original
	 * link targets, or possibly with other link targets. Does this compose?
	 * i.e. if I wanted to link many different lenses into the same object file,
	 * could I add each one just like this?
	 * ** not quite: the __wrap_ symbol name would be multiply defined. We'd
	 * have to hook them all in somehow, but this is tricky:
	 * --- if we have multiple lenses linked in, then should *all* the abstracted
	 * outcalls be made? 
	 * YES: this is also the answer to eliminating horrible hacky if--else logic:
	 * what we do is call *all* of them, *if* they're defined. So not-linked-against
	 * outcalls just do nothing. BUT note that we're doing something weird here:
	 * we're multicasting previously unicast function calls. So we should really
	 * warn if more than one of the outcalls is linked. (If not exactly 1, in fact.)
	 *
	 * What about compositionality in the sense of styles upon styles?
	 * __wrap__real_concrete_outcall
	 * __wrap__wrap_concrete_outcall */
	if (!abstracted_outcall)
	{
		__real_concrete_outcall(arg);
	}
	else
	{
		int abstracted_arg = atoi(arg);
		abstracted_outcall(abstracted_arg);
	}
	/* The abstracted entry points are named s.t. the style they
	 * were defined by is represented in the name. */
}
