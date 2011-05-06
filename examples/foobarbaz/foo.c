extern long int bum;
extern long int bar(long int);
extern long int crazy;
long int crazy = 42;

long int foo(long int a)
{
	if (a > 0)
	{
		return bar(a-1);
	}
	else return bum;
}

static const char *not_external(void)
{
	return "Thou shalt not see this!";
}

int main(int argc, char **argv)
{
	return foo(argc);
}
