#include <stdio.h>

extern void foo_do_xyzzy(int arg0, int arg1);
extern void foo_do_plugh(int arg0, int arg1, int fromp);

int main(void)
{
	printf("Doing xyzzy(frotz: %d, blorb: %d)\n", 12, 34);
	foo_do_xyzzy(12, 34);
	printf("Doing plugh(frotz: %d, blorb: %d, fromp: %d)\n", 56, 78, 90);
	foo_do_plugh(56, 78, 90);

	return 0;
}
