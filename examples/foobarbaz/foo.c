extern int bum;
extern int bar(int);
extern int crazy;
int crazy = 42;

int foo(int a)
{
	if (a <= 0)
	{
		return bar(a-1);
	}
	else return bum;
}

static const char *not_external(void)
{
	return "Thou shalt not see this!";
}
