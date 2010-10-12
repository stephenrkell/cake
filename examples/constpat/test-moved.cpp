#include <assert.h>

typedef struct { int data; } wordsize_type;

wordsize_type __wrap_wide(wordsize_type a,  wordsize_type b);
char *wide(int a, int b);

static void call_wrap_wide(void)
{
	wordsize_type ret = __wrap_wide({12}, {34});
}

static void call_wide(void)
{
	char *ret = wide(12, 34);
}

int main()
{
	assert(sizeof(wordsize_type) == sizeof (int));
	call_wide();
    call_wrap_wide();
}
