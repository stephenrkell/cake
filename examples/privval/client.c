#include <stdio.h>

struct priv
{
	double hidden;
};

struct val
{
	void *private_field;
};

double get_hidden(struct val *v)
{
	return ((struct priv *) v->private_field)->hidden;
}

void set_hidden(struct val *v, double i)
{
	((struct priv *) v->private_field)->hidden = i;
}

int pass(struct val *v);

int main(void)
{
	struct priv p = { 42.0 };
	struct val v = { &p };
	
	pass(&v);
	
	return 0;
}
