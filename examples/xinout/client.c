#include <stdio.h>

/* the interface we target */

struct safe_obj
{
	int b;
	int a;
};
extern int send(struct safe_obj *u);

int main(void)
{
	struct safe_obj u;
	printf("Sending u inout ....\n");
	int ret = send(&u);
	printf("Received retval %d.\n");

	return 0;
}
