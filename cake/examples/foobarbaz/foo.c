extern int bar(int);

int foo(int a)
{
	if (a <= 0)
	{
		return bar(a-1);
	}
	else return 0;
}
