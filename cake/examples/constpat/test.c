#include <stdio.h>
/*#include <signal.h>*/
#include <assert.h>
#define UNW_LOCAL_ONLY
/*#include <libunwind.h>*/

/*void show_backtrace (void) {
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip, sp;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);
  }
}*/

struct wordsize { int data; };
typedef struct wordsize wordsize_type;

extern wordsize_type __wrap_wide(wordsize_type a,  wordsize_type b);
wordsize_type __wrap_wide_callee(wordsize_type a,  wordsize_type b) { /*show_backtrace();*/ return (struct wordsize){0}; }
extern char *wide(int a, int b);
char *wide_callee(int a, int b) { /*show_backtrace();*/ return "hello"; }

static void call_wrap_wide(void)
{
	wordsize_type ret = __wrap_wide((struct wordsize){12}, (struct wordsize){34});
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
