#include <stdio.h>

extern void *data;

/*char *wider_still(int a, int b, int c)
{
 	static char buf[4096];
    snprintf(buf, sizeof buf, "wider_still(%d, %d, %d)", a, b, c);
    return buf;
}*/
char *narrow(int a)
{
 	static char buf[4096];
    snprintf(buf, sizeof buf, "narrow(%d)", a); 
    return buf;  
}
char *narrowish(int a, int b)
{
 	static char buf[4096];
    snprintf(buf, sizeof buf, "narrowish(%d, %d)", a, b);
    return buf;
}
char *not_always(int a)
{
 	static char buf[4096];
    snprintf(buf, sizeof buf, "not_always(%d)", a);
    return buf;   
}
char *take_string(const char *s)
{
 	static char buf[4096];
    snprintf(buf, sizeof buf, "take_string(%s)", s);
    return buf;   
}
