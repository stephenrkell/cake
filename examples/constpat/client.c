#include <stdio.h>

extern char *wide(long int a, long int b);
       char *wider_still(long int a, long int b, long int c_unused);
extern char *sometimes(long int a);
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

/* FIXME: we can't call this just "wider_still" because then the compiler
 * will omit an internal reference which is impervious to the linker's
 * --wrap option, so our wrapper code won't get run. By avoiding name-
 * -matching in this compilation unit, we can defer the binding until 
 * link time. A better Cake compiler would emit make rules that undid the
 * binding, perhaps using my patched objcopy's --unbind-sym feature. */
char *__real_wider_still(long int a, long int b, long int c_unused)
{
	return "fell through to the real wider_still!";   
}
