void concrete_call(const char *arg); /* the actual provided function */

void concrete_outcall(const char *arg); /* a required function */

void concrete_call(const char *arg)
{
	concrete_outcall(arg); /* our lens code should bind this to the abstract implementation */
}
