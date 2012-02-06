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
	printf("Setting resid (of uio at %p) to %lu\n", target, resid);
	target->resid = resid;
	printf("Setting off (of uio at %p) to %lu\n", target, off);
	target->off = off;
	return target;
}

unsigned long uio_getresid(struct uio *target)
{
	printf("Retrieving resid %lu from uio %p\n", target->resid, target);
	return target->resid;
}

unsigned long uio_getoff(struct uio *target)
{
	printf("Retrieving offset %lu from uio %p\n", target->off, target);
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

