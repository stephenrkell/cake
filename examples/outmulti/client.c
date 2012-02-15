#include <stdio.h>

struct givemestruct
{
	int a1;
	double a2;
	double a3;
};

void cb(void *s, int a1, double a2, double a3)
{
	((struct givemestruct *) s)->a1 = a1;
	((struct givemestruct *) s)->a2 = a2;
	((struct givemestruct *) s)->a3 = a3;
}

int get(void *);

int main(void)
{
	struct givemestruct s;
	get(&s);
	printf("Got %d, %f, %f\n", s.a1, s.a2, s.a3);
	return 0;
}
