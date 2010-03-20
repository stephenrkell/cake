#include <stdio.h>

extern char *wide(int a, int b);
       char *wider_still(int a, int b, int c_unused);
extern char *sometimes(int a);
extern char *stringlit(char *s);
int main(void)
{
	printf("wide(12, 34) gave us: %s\n", wide(12, 34));
    printf("wider_still(56, 78, 0) gave us: %s\n", wider_still(56, 78, 0));
    printf("wider_still(90, 10, 2) gave us: %s\n", wider_still(90, 10, 2));
    printf("sometimes(100) gave us: %s\n", sometimes(100));
    printf("stringlit(\"hello\") gave us: %s\n", stringlit("hello"));
	return 0;
}

char *wider_still(int a, int b, int c_unused)
{
	return "fell through to the real wider_still!";   
}
