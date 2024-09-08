/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <stdio.h>
#include "printf.h"
#include "../utils/block_meta.h"

#define MMAP_THRESHOLD (128 * 1024)
#define SIZE_STRUCT sizeof(struct block_meta)

/* Structure to hold memory block metadata */
extern struct block_meta *head;

size_t padding(size_t size);
void list_block_init(size_t size);
struct block_meta *aloc_primul_sbrk(size_t size);
struct block_meta *add_block_final(size_t size);
void add_block_free(size_t size, struct block_meta *node);
void coalesce_stanga(struct block_meta *node, struct block_meta *node_prev);
void coalesce_dreapta(struct block_meta *node, struct block_meta *node_next);
struct block_meta *best_fit(size_t size);
struct block_meta *primul_nod_inainte_sbrk(struct block_meta *node);
struct block_meta *primul_nod_dupa_sbrk(struct block_meta *node);
struct block_meta *ultimul_nod_sbrk();
