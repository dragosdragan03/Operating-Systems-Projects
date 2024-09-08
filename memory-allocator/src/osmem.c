// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include "block.h"
#include "block_meta.h"

int ok; // sa vad daca am alocat pana acm un nod cu sbrk

void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */
	if (!size)
		return NULL;

	if (size + padding(size) < MMAP_THRESHOLD) { // cazul cu brk
		if (!head) { // daca nu a mai fost alocat headul pana acm
			list_block_init(size); // initializez lista si retin payload ul ce dimensiune are
			ok = 1;
			return (char *)head + SIZE_STRUCT;
		} else if (ok) { // daca nu e primul nod pe care il aloc
			struct block_meta *ptr = best_fit(size); // returnez NULL daca n am gasit unde sa pun sau ptr daca am terminat

			if (ptr)
				return ptr;

			return add_block_final(size);
		}
		ok = 1;
	} else { // trebuie implementat cu mmap
		size_t dimensiune_block = size + padding(size);

		if (!head) {
			head = mmap(NULL, dimensiune_block + SIZE_STRUCT, 1 | 2, 2 | 32, -1, 0); // aloc dinamic head ul
			head->next = head;
			head->prev = head;
			head->size = dimensiune_block;
			head->status = STATUS_MAPPED;
			return (char *)head + SIZE_STRUCT;
		}
		struct block_meta *node = mmap(NULL, dimensiune_block + SIZE_STRUCT, 1 | 2, 2 | 32, -1, 0);

		node->size = dimensiune_block; // retin intreaga dimensiune
		node->next = head;
		node->prev = head->prev;
		head->prev->next = node; // am facut legaturile ca sa adaug un nod la finalul listei
		head->prev = node;
		node->status = STATUS_MAPPED;
		return (char *)node + SIZE_STRUCT;
	}

	return aloc_primul_sbrk(size);
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
	if (!ptr)
		return;

	struct block_meta *node = (struct block_meta *)ptr;
	char *char_ptr = (char *)node;

	char_ptr -= SIZE_STRUCT;
	node = (struct block_meta *)char_ptr;

	if (node->status == STATUS_ALLOC) {
		struct block_meta *node_next = primul_nod_dupa_sbrk(node->next); // imi returneaza primul block alocat cu sbrk
		struct block_meta *node_prev = primul_nod_inainte_sbrk(node->prev);

		if (node_next != NULL && node_next->status == STATUS_FREE) { // daca exista node alocat cu sbrk dupa nodul meu
			coalesce_dreapta(node, node_next);
		}
		if (node_prev != NULL && node_prev->status == STATUS_FREE) {
			coalesce_stanga(node, node_prev);
		} else {
			node->status = STATUS_FREE;
			return;
		}
	} else if (node->status == STATUS_MAPPED) { // e ptr de tip MMAP
		if (node == head && node->next == head) { // daca singurul nod in lista este headul
			munmap(node, node->size + SIZE_STRUCT);
			head = NULL;
			return;
		} else if (node == head && node->next != head) { // vreau sa sterg headul
			head->prev->next = head->next;
			head->next->prev = head->prev;

			head = head->next;

			head->size = head->next->size;
			head->status = head->next->status;
			munmap(node, node->size + SIZE_STRUCT);
			return;
		} // vreau sa sterg node si sa refac legaturile
		node->next->prev = node->prev;
		node->prev->next = node->next; // refac legaturile
		munmap(node, node->size + SIZE_STRUCT);
		return;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	if (size * nmemb == 0)
		return NULL;

	size = size * nmemb;

	if (size + padding(size) + SIZE_STRUCT < (size_t)getpagesize()) { // cazul cu brk
		if (!head) { // daca nu a mai fost alocat headul pana acm
			list_block_init(size); // initializez lista si retin payload ul ce dimensiune are
			ok = 1;
			memset((char *)head + SIZE_STRUCT, 0, head->size);
			return (char *)head + SIZE_STRUCT;
		}
		if (ok) { // daca nu e primul nod pe care il aloc
			struct block_meta *ptr = best_fit(size); // returnez NULL daca n am gasit unde sa pun sau ptr daca am terminat

			if (ptr) {
				memset(ptr, 0, size + padding(size));
				return ptr;
			}
			struct block_meta *node = add_block_final(size);

			memset(node, 0, size + padding(size));
			return node;
		}

		ok = 1;
	} else { // trebuie implementat cu mmap
		size_t dimensiune_block = size + padding(size);

		if (!head) {
			head = mmap(NULL, dimensiune_block + SIZE_STRUCT, 1 | 2, 2 | 32, -1, 0); // aloc dinamic head ul
			head->next = head;
			head->prev = head;
			head->size = dimensiune_block;
			head->status = STATUS_MAPPED;
			memset((char *)head + SIZE_STRUCT, 0, head->size);
			return (char *)head + SIZE_STRUCT;
		}
		struct block_meta *node = mmap(NULL, dimensiune_block + SIZE_STRUCT, 1 | 2, 2 | 32, -1, 0);

		node->size = dimensiune_block; // retin intreaga dimensiune
		node->next = head;
		node->prev = head->prev;
		head->prev->next = node; // am facut legaturile ca sa adaug un nod la finalul listei
		head->prev = node;
		node->status = STATUS_MAPPED;
		memset((char *)node + SIZE_STRUCT, 0, node->size);
		return (char *)node + SIZE_STRUCT;
	}
	struct block_meta *ptr = aloc_primul_sbrk(size);

	memset(ptr, 0, size + padding(size));
	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	if (!ptr) // daca nu exista pointerul
		return os_malloc(size);

	if (!size) { //daca size ul este 0 si pointerul exista
		os_free(ptr);
		return NULL;
	}

	struct block_meta *node = (struct block_meta *)ptr;
	char *char_ptr = (char *)node;

	char_ptr -= SIZE_STRUCT;
	node = (struct block_meta *)char_ptr;

	if (node->status == STATUS_FREE)
		return NULL;

	// sa vad daca este deja alocat cu sbrk si ii aloc o dimensiune mai mica
	if (node->status == STATUS_ALLOC && size + padding(size) <= node->size) {
		size_t dimensiune_veche = node->size;

		add_block_free(size, node);
		if (node->size == dimensiune_veche)
			return (char *)node + SIZE_STRUCT;
		// inseamna ca pot face un nod alocat si unul free, care se poate coalesce cu unul free din dreapta
		struct block_meta *node_next = primul_nod_dupa_sbrk(node->next->next);

		if (node_next && node_next->status == STATUS_FREE) // sa vad daca exista urmatorul nod alocat cu sbrk
			coalesce_dreapta(node->next, node_next);
		return (char *)node + SIZE_STRUCT;
	} // sa vad daca este deja alocat cu sbrk si ii aloc o dimensiune mai mare
	if (node->status == STATUS_ALLOC && size + padding(size) > node->size) {
		if (size + padding(size) >= MMAP_THRESHOLD) {
			void *p = os_malloc(size);

			memmove(p, ptr, node->size);
			os_free(ptr);
			return p;
		}
		if (node == ultimul_nod_sbrk()) { // daca e ultimul nod, mai aloc memorie in plus
			sbrk(size + padding(size) - node->size);
			node->size = size + padding(size);
			return ptr;
		}
		// daca urmatorul block e liber mai iau spatiu din el
		size_t dimensiune_de_mutat = node->size;
		struct block_meta *node_next = primul_nod_dupa_sbrk(node->next);

		if (node_next && node_next->status == STATUS_FREE) // sa vad daca exista urmatorul nod alocat cu sbrk
			coalesce_dreapta(node, node_next);
		if (node->size >= size + padding(size)) { // sa vad daca am loc sa iau tot nodul din dreapta
			size_t dimensiune_veche = node->size;

			add_block_free(size, node);
			if (node->size == dimensiune_veche)
				return ptr;
			struct block_meta *node_next = primul_nod_dupa_sbrk(node->next->next);

			if (node_next && node_next->status == STATUS_FREE) // sa vad daca exista urmatorul nod alocat cu sbrk
				coalesce_dreapta(node->next, node_next);

		} else { // caut in alta parte si l fac p asta free
			void *p = os_malloc(size);

			node->status = STATUS_ALLOC;
			memmove(p, ptr, dimensiune_de_mutat);
			os_free(ptr);
			return p;
		}
	} else {
		if (node->status == STATUS_MAPPED) { // sa vad daca este alocat cu map si vreau sa aloc mai multa dimensiune
			// daca singurul nod in lista este headul
			void *ptr_aux;

			ptr_aux = os_malloc(size); // retin pointerul unde a fost alocat
			os_free(ptr);
			return ptr_aux;
		}
	}
	return ptr;
}
