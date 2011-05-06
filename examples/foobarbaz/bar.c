extern long int baz(long int);

long int bar(long int a)
{
	return baz(a-1);
}
