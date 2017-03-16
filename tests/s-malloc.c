#include <stdio.h>

#define ALIGN(n, a)  (((n) + (a) - 1) & ~((a) - 1))

#define MALLOC_BUFSIZE  (1024 * 1024 * 1024)

int malloc_count;
int free_count;

void *malloc(size_t size)
{
	static char buf[MALLOC_BUFSIZE];
	static unsigned alloc_size;
	void *ptr;

	size = ALIGN(size, 8);
	if (alloc_size + size > sizeof(buf))
		return NULL;

	ptr = buf + alloc_size;
	alloc_size += size;

	malloc_count++;
	return ptr;
}

void free(void *ptr)
{
	free_count++;
}

int main(void)
{
	free(malloc(16));
	return 0;
}
