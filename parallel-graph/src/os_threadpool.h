/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __OS_THREADPOOL_H__
#define __OS_THREADPOOL_H__	1

#include <pthread.h>
#include "os_list.h"

typedef struct {
	void *argument; // vector cu argumentele taskului
	void (*action)(void *arg);
	void (*destroy_arg)(void *arg);
	os_list_node_t list; // list entry functie care o sa pointeze la inceputul structurii
} os_task_t;

typedef struct os_threadpool { // un pool de threaduri
	unsigned int num_threads; // numarul de threaduri
	pthread_t *threads; // vector de threaduri

	/*
	 * Head of queue used to store tasks.
	 * First item is head.next, if head.next != head (i.e. if queue
	 * is not empty).
	 * Last item is head.prev, if head.prev != head (i.e. if queue
	 * is not empty).
	 */
	os_list_node_t head; // headul listei de threaduri

	/* TODO: Define threapool / queue synchronization data. */
	pthread_mutex_t mutex_for_tasks;
	pthread_mutex_t mutex_for_variables;
	int number_tasks;
	int verify_num_nodes;
	pthread_cond_t cond;
} os_threadpool_t;

os_task_t *create_task(void (*f)(void *), void *arg, void (*destroy_arg)(void *));
void destroy_task(os_task_t *t);

os_threadpool_t *create_threadpool(unsigned int num_threads);
void destroy_threadpool(os_threadpool_t *tp);

void enqueue_task(os_threadpool_t *q, os_task_t *t);
os_task_t *dequeue_task(os_threadpool_t *tp);
void wait_for_completion(os_threadpool_t *tp);

#endif
