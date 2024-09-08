// SPDX-License-Identifier: BSD-3-Clause

#include "block.h"
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

struct block_meta *head;

size_t padding(size_t size)
{
	int value = (int)(size % 8) - 8;
	size_t dimension = (size_t)abs(value);

	if (dimension == 8)
		return 0;

	return dimension;
}

void list_block_init(size_t size)
{
	// vreau sa initializez lista cu blockuri
	head = sbrk(MMAP_THRESHOLD); //aloca dinamic si returneaza pointer la inceputul adresei
	head->size = size + padding(size); // retin in head size ul plus paddingul
	head->status = STATUS_ALLOC;

	struct block_meta *ptr = (struct block_meta *)((char *)head + head->size + SIZE_STRUCT);

	size_t dimensiune_ptr = MMAP_THRESHOLD - head->size - SIZE_STRUCT; // dimensiunea ramasa libera fara SIZE_STRUCT

	if (dimensiune_ptr > SIZE_STRUCT) { // daca dimensiunea este diferita de 0 (am padding)
		head->next = ptr;
		head->prev = ptr;
		ptr->next = head;
		ptr->prev = head;

		ptr->status = STATUS_FREE;
		ptr->size = dimensiune_ptr - SIZE_STRUCT;
	} else {
		head->next = head;
		head->prev = head;
		head->size = MMAP_THRESHOLD - SIZE_STRUCT; // retin intreaga dimensiune de TRASHHOLD mai putin structura
	}
}

/*inseamnca ca nu a mai fost alocat pana acm niciun nod cu sbrk deci e primul dupa un TRASHOLD*/
struct block_meta *aloc_primul_sbrk(size_t size)
{
	struct block_meta *node = head;

	node = sbrk(MMAP_THRESHOLD); //aloca dinamic si returneaza pointer la inceputul adresei
	node->size = size + padding(size); // retin in head size ul plus paddingul
	node->status = STATUS_ALLOC;

	node->next = head;
	node->prev = head->prev;
	head->prev->next = node; // am facut legaturile ca sa adaug un nod la finalul listei
	head->prev = node;

	size_t dimensiune_ptr = MMAP_THRESHOLD - node->size - SIZE_STRUCT; // dimensiunea ramasa libera fara SIZE_STRUCT

	if (dimensiune_ptr > SIZE_STRUCT) { // daca dimensiunea paddingului este mai mare de 32
		struct block_meta *ptr = (struct block_meta *)((char *)node + node->size + SIZE_STRUCT);

		ptr->next = head;
		ptr->prev = head->prev;
		head->prev->next = ptr; // am facut legaturile ca sa adaug nodul cu dimensiunea libera la finalul listei
		head->prev = ptr;

		ptr->status = STATUS_FREE;
		ptr->size = dimensiune_ptr - SIZE_STRUCT; // retin in ptr paddingul si adaug si size_structul
		return (struct block_meta *)((char *)node + SIZE_STRUCT);
	} // daca nu mai am spatiu liber sa mai bag inca un nod
	node->size = MMAP_THRESHOLD - SIZE_STRUCT; // retin intreaga dimensiune de TRASHHOLD mai putin structura
	return (struct block_meta *)((char *)node + SIZE_STRUCT);
}

struct block_meta *primul_nod_dupa_sbrk(struct block_meta *node)
{
	// returneaza primul nod alocat cu sbrk
	if (node == head)
		return NULL;

	do {
		if (node->status != STATUS_MAPPED)
			break;

		node = node->next;
	} while (node != head);
	if (node == head)
		return NULL; // inseamnca ca nu a mai gasit nimic dupa nodul meu care sa fie alocat cu sbrk

	return node;
}

struct block_meta *primul_nod_inainte_sbrk(struct block_meta *node)
{
	if (node->next == head)
		return NULL;

	do {
		if (node->status != STATUS_MAPPED)
			break;

		node = node->prev;
	} while (node != head->prev);
	if (node == head->prev)
		return NULL; // inseamnca ca nu a mai gasit nimic dupa nodul meu care sa fie alocat cu sbrk

	return node;
}

/*caut ultimul nod din lista alocat cu sbrk*/
struct block_meta *ultimul_nod_sbrk(void)
{
	struct block_meta *node = head->prev; // iau un nod ca sa parcurg lista intre blockuri

	do {
		if (node->status != STATUS_MAPPED)
			break;

		node = node->prev;
	} while (node->next != head);
	return node;
}

/*vreau sa inserez un nod si un nod de tip padding la finalul listei*/
struct block_meta *add_block_final(size_t size)
{
	// inseamna ca e NULL si ori il bag la final ori ii extind dimensiunea
	struct block_meta *node = ultimul_nod_sbrk();

	// se opreste cand a gasit primul nod free/alloc sau cand toate erau mapped
	if (node->status == STATUS_FREE) {
		sbrk(size + padding(size) - node->size);
		node->size = size + padding(size);
		node->status = STATUS_ALLOC;
		return (struct block_meta *)((char *)node + SIZE_STRUCT);
	}
	// altfel trebuie sa aloc un nod nou
	size_t dimensiune_noua = size + padding(size); // dimensiunea blockului

	node = sbrk(dimensiune_noua + SIZE_STRUCT); // creez un block de intreaga dimensiune
	node->size = dimensiune_noua; // retin intreaga dimensiune
	node->status = STATUS_ALLOC;

	node->next = head;
	node->prev = head->prev;
	head->prev->next = node; // am facut legaturile ca sa adaug un nod la finalul listei
	head->prev = node;

	return (struct block_meta *)((char *)node + SIZE_STRUCT);
}

/*trebuie micsorata dimensiunea unui nod (padding ul unui nod) si inserat inca un nod dupa*/
//bag spatiu doar pe prima portiune de nod si pe restul o impart in padding
void add_block_free(size_t size, struct block_meta *node)
{
	size_t dimensiune_noua = size + padding(size); // retin size ul si padd ingul

	// sa vad daca imi intra toata dimensiunea in size ul deja alocat
	if (node->size - dimensiune_noua <= SIZE_STRUCT) {
		node->status = STATUS_ALLOC; // inseamna ca pot introduce toata memoria in nodul meu
	} else {
		/*vreau sa creez un nod nou care sa mi retina free_block-ul*/
		size_t dimensiune_veche = node->size;

		node->size = dimensiune_noua;
		struct block_meta *ptr = (struct block_meta *)((char *)node + SIZE_STRUCT + node->size); // padding

		ptr->size = dimensiune_veche - dimensiune_noua - SIZE_STRUCT; // retin cat a ramas in urmatorul nod

		ptr->next = node->next;
		ptr->prev = node;
		node->next->prev = ptr;
		node->next = ptr;

		ptr->status = STATUS_FREE;
		node->status = STATUS_ALLOC;
	}
}

void coalesce_stanga(struct block_meta *node, struct block_meta *node_prev)
{ // unesc nodul din stanga liber cu nodul meu
// pastrez nodul din stanga cel liber
	node_prev->size += node->size + SIZE_STRUCT; // adaug la nodul din urma size ul nodului curent
	/*vreau sa sterg legaturile nodului meu*/
	node_prev->next = node->next;
	node->next->prev = node_prev;
}

void coalesce_dreapta(struct block_meta *node, struct block_meta *node_next)
{
	// unesc nodul meu cu nodul din dreapta liber
	node->status = STATUS_FREE;
	node->size += node_next->size + SIZE_STRUCT;

	node->next = node_next->next;
	node_next->next->prev = node;
}

/*caut cea mai apropiata dimensiune de size ul meu*/
struct block_meta *dimensiune_apropiata(size_t size)
{
	size_t size_min = MMAP_THRESHOLD;

	struct block_meta *node = head;
	struct block_meta *node_selectat;

	do {
		if (node->status == STATUS_FREE && (size + padding(size) <= node->size) && node->size < size_min) {
			size_min = node->size;
			node_selectat = node;
		}

		node = node->next;
	} while (node != head);


	if (size_min == MMAP_THRESHOLD)
		return NULL;
	else
		return node_selectat;
}

/*vreau sa vad daca are loc in alta parte*/
struct block_meta *best_fit(size_t size)
{
	struct block_meta *node = dimensiune_apropiata(size);

	if (!node) { // inseamna ca n a gasit un best fit
		return NULL;
	}
	size_t dimensiune_veche = node->size;

	add_block_free(size, node);
	if (node->size == dimensiune_veche)
		return (struct block_meta *)((char *)node + SIZE_STRUCT);
	// inseamna ca pot face un nod alocat si unul free, care se poate coalesce cu unul free din dreapta
	struct block_meta *node_next = primul_nod_dupa_sbrk(node->next->next);

	if (node_next && node_next->status == STATUS_FREE) // sa vad daca exista urmatorul nod alocat cu sbrk
		coalesce_dreapta(node->next, node_next);
	return (struct block_meta *)((char *)node + SIZE_STRUCT);
}
