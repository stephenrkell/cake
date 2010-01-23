extern int baz(int);

int bar(int a)
{
	return baz(a-1);
}
