#include <stdio.h>
#include <stdlib.h>

struct lockable_obj
{
	int a;
	int b;
	int locked;
};

void lock(struct lockable_obj *u)
{
	printf("Locking object at %p\n", u);
	u->locked = 1;
}
void unlock(struct lockable_obj *u)
{
	printf("Unlocking object at %p\n", u);
	u->locked = 0;
}

struct lockable_obj *global;

void print_state()
{
	printf("Lockable object at %p is now in state %s\n",
		global, global->locked ? "locked" : "unlocked");
}

int atomic(struct lockable_obj *u)
{
	printf("Received an object, and it is %s\n", 
		u->locked ? "locked" : "unlocked");
	
	return 0;
}

int nonatomic(struct lockable_obj *u)
{
	printf("Received an object, and it is %s\n", 
		u->locked ? "locked" : "unlocked");
	
	return 0;
}

int locked_on_return(struct lockable_obj *u)
{
	printf("Received an object, and it is %s\n", 
		u->locked ? "locked" : "unlocked");
	printf("Locking an object in locked_on_return.\n");
	
	u->locked = 1;
	
	global = u;
	atexit(print_state);
	
	return 0;
}
