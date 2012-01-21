#include <stdio.h>

struct buf 
{
	int padding;
	int val;
};

void fillme(int foo, struct buf *p_buf)
{
	printf("fillme(%d, %p) called\n", foo, p_buf);
	p_buf->val = 1000;
	return;
}
