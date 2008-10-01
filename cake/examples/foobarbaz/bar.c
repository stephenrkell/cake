#include <string.h>

const char *baz(void);

static char barbaz[4096] = "bar";

char *bar(void)
{
	return strncat(barbaz, baz(), 4096 - sizeof "bar");
}

