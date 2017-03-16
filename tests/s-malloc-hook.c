#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>

void * (*real_malloc)(size_t sz);
void (*real_free)(void *ptr);

#define ALIGN(n, a)  (((n) + (a) - 1) & ~((a) - 1))

#define MALLOC_BUFSIZE  (1024 * 1024 * 1024)
/* this is needed for optimized binaries */
static char buf[MALLOC_BUFSIZE];

void *malloc(size_t sz)
{
	static unsigned alloc_size;
	void *ptr;

	if (real_malloc)
		return real_malloc(sz);

	sz = ALIGN(sz, 8);
	if (alloc_size + sz > sizeof(buf))
		return NULL;

	ptr = buf + alloc_size;
	alloc_size += sz;

	return ptr;
}

void free(void *ptr)
{
	char *p = ptr;

	if (buf <= p && p < &buf[MALLOC_BUFSIZE])
		return;

	if (real_free)
		real_free(ptr);
}

static void hook(void)
{
	real_malloc = dlsym(RTLD_NEXT, "malloc");
	real_free   = dlsym(RTLD_NEXT, "free");
}

static __attribute__((section(".preinit_array")))
void (*preinit_func_table[])(void) = { hook, };

int main(void)
{
	free(malloc(16));
	return 0;
}
