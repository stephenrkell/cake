#include <stdio.h>

struct ops
{
	int (*do_something)(double);
};

extern struct ops my_ops;

int main(void)
{
	struct ops *p_ops = &my_ops; // more realistically: loaded from plugin
	
	int returned = p_ops->do_something(11.21);
	
	return 0;
}
