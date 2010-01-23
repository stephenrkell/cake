extern int foo(int);

int baz(int a)
{
	return foo(a-1);
}
