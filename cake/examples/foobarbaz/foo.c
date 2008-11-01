#include <stdio.h>

char *bar(void);

int main(void)
{
	printf("Hello foo %s, world!\n", bar());
}

