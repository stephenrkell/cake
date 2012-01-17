/* This file is both a client of the abstract interface, and
 * a provider of an abstract utility call. */
 
#include <stdio.h>

void abstracted_outcall(int arg);
void abstracted_outcall(int arg)
{
	printf("Received the integer %d\n", arg);
}
