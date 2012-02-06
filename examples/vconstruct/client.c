#include <stdio.h>

typedef unsigned char uint8_t;

extern void getstuff(
	uint8_t *buf,
	unsigned long *off,
	unsigned long *len
);

int main(void)
{
	#define BUFLEN 1024
	uint8_t my_buf[BUFLEN];
	unsigned long my_off = 0;
	unsigned long my_len = BUFLEN;

	printf("About to getstuff(%p, %p, %p)\n", my_buf, &my_off, &my_len);
	getstuff(my_buf, &my_off, &my_len);
	printf("Having called getstuff, now my_off == %lu and my_len == %lu\n",
		my_off, my_len);
	printf("Buf is: ");
	int i;
	for (i = 0; i < BUFLEN; ++i)
	{
		if (i % 24 == 0) printf("\n");
		printf("%02x ", my_buf[i]);
	}
	
	return 0;
}
