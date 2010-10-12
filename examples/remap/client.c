#include <stdio.h>

extern int div(int dividend, int divisor);

int main(void)
{
	printf("Dividing 50 by 7; quotient  is %d\n", div(50, 7));
	printf("Dividing 88 by 9; quotient  is %d\n", div(88, 9));    
	return 0;
}
