#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdlib.h>

size_t (*orig_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t (*orig_fwrite)(const void *ptr, size_t size, size_t nmemb,
              FILE *stream);

static int initialized;

/*static void initialize(void) __attribute__((constructor));*/
static void initialize(void)
{
	printf("Hello from initialize()\n");
	orig_fread = dlsym("fread", RTLD_NEXT); printf("Hello from initialize()\n"); assert(dlerror() == NULL && orig_fread);
	printf("Hello from initialize()\n");
	orig_fwrite = dlsym("fwrite", RTLD_NEXT); assert(dlerror() == NULL && orig_fwrite);
	printf("Hello from initialize()\n");
	initialized = 1;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (!initialized) initialize(); 
	printf("Hello from fake fread()\n");
	return orig_fread(ptr, size, nmemb, stream);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb,
              FILE *stream)
{
	if (!initialized) initialize(); 
	printf("Hello from fake fwrite()\n");
	return orig_fwrite(ptr, size, nmemb, stream);
}
