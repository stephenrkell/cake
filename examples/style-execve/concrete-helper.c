#include <stdio.h>

void concrete_outcall(const char *arg) /* should not run! */
{
	printf("Received the string: %s\n", arg);
}
