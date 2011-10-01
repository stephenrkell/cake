#include <stdio.h>

/* the interface we target */

typedef double length;
typedef int hundredths_t;

int raw(double arg);
hundredths_t frob(length arg);

int main(void)
{
	printf("Sending raw length 69,105....\n");
	int raw_ret = raw(69105.0);
	printf("Received %d in return.\n", raw_ret);
	
	printf("Frobbing with length 69,105...\n");
	hundredths_t frob_ret = frob(69105.0);
	printf("Received %d hundredths in return.\n", frob_ret);
	
	return 0;
}
