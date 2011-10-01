#include <stdio.h>

typedef double perimeter;
typedef int thousandths_t;

int raw(double arg)
{
	printf("Received a raw double, %f; returning 42\n", arg);
	return 42;
}

thousandths_t frob(perimeter arg)
{
	printf("Received a perimeter of %f; returning 42 in thousandths\n", arg);
	return 42000;
}

