#include <stdio.h>

/* the interface we target */

struct safe_obj
{
	int b;
	int a;
};
extern int send(int, struct safe_obj *u);

int main(void)
{
	struct safe_obj u;
	printf("Sending 0, u inout ....\n");
	int ret0 = send(0, &u);
	printf("Received retval %d.\n", ret0);
	printf("Sending 1, u inout ....\n");
	int ret1 = send(1, &u);
	printf("Received retval %d.\n", ret1);
	printf("Sending 2, u inout ....\n");
	int ret2 = send(2, &u);
	printf("Received retval %d.\n", ret2);

	return 0;
}
