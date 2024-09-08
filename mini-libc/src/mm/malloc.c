// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

void* malloc(size_t size) {
	void* mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (!mem)
		return (void*)0;

	if (mem_list_add(mem, size)) {
		munmap(mem, size);
		return (void*)0;
	}

	return mem;
}

void* calloc(size_t nmemb, size_t size) {
	size_t total_size = nmemb * size;

	void* mem = malloc(total_size);

	if (!mem)
		return (void*)0;

	if (mem)
		memset(mem, 0, total_size);

	return mem;
}

void free(void* ptr) {
	if (!ptr) {
		return;
	}

	struct mem_list* item = mem_list_find(ptr); //caut in mem_list unde se afla ptr

	munmap(ptr, item->len); //dealoc memoria lui ptr

	mem_list_del(ptr);
}

void* realloc(void* ptr, size_t size) {
	free(ptr);
	ptr = malloc(size);

	return ptr;
}

void* reallocarray(void* ptr, size_t nmemb, size_t size) {
	return realloc(ptr, nmemb * size);;
}
