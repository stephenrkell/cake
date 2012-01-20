#include <stdio.h>

struct buf 
{
	int padding;
	int val;
};

void fillme(int foo, struct buf *bar)
{
	printf("fillme(%d, %p) called\n", foo, bar);
	bar->val = 1000;
	return;
}
