#include <stdio.h>

struct direct
{
	double d;
};

int pass (struct direct *d)
{
	printf("Received a direct, value: %lf\n", d->d);
}
