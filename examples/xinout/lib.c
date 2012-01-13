#include <stdio.h>

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

int send(struct lockable_obj *u)
{
	printf("Received an object, and it is %s\n", 
		u->locked ? "locked" : "unlocked");
	
	return 0;
}
