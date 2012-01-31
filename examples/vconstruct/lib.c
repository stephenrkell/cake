#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct uio
{
	int dir; 
	unsigned char *buf;
	unsigned long resid;
	unsigned long off;
};

struct uio *uio_setup(unsigned char *buf,
	unsigned long resid, unsigned long off)
{
	struct uio *target = (struct uio *) malloc(sizeof (struct uio));
	assert(target);
	target->buf = buf;
	target->resid = resid;
	target->off = off;
	return target;
}

unsigned long uio_getresid(struct uio *target)
{
	return target->resid;
}

unsigned long uio_getoff(struct uio *target)
{
	return target->off;
}

void uio_free(struct uio *obj)
{
	free(obj);
}

void readstuff(struct uio *outp)
{
	printf("Reading stuff from offset %lu, max length %zu, dest %p\n",
		(unsigned long) outp->off, outp->resid, outp->buf);
	unsigned long written = 0;
	while (written < outp->resid)
	{
		outp->buf[written] = (unsigned char)(written + outp->off);
		++written;
	}
}

