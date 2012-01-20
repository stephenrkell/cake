struct buf
{
	int val;
};

void fillme(int, struct buf *);

int main(void)
{
	struct buf mybuf = { 42 }; /* Here "42" stands for "uninitialized". */
	
	fillme(0, &mybuf);
	
	printf("After filling, mybuf contains %d.\n", mybuf.val);
	
	return 0;
}
