extern long int foo(long int);
long int bum = 42;

long int baz(long int a)
{
	return foo(a-1);
}
