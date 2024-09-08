// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

pthread_mutex_t mutex_create_threads;

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t;

	t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;		// the function
	t->argument = arg;		// arguments for the function
	t->destroy_arg = destroy_arg;	// destroy argument function

	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	/* TODO: Enqueue task to the shared task queue. Use synchronization. */
	pthread_mutex_lock(&(tp->mutex_for_tasks));
	list_add_tail(&(tp->head), &(t->list));
	pthread_cond_signal(&tp->cond); // schimb semnalul ca s a introdus ceva in coada
	pthread_mutex_unlock(&(tp->mutex_for_tasks));
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */

os_task_t *dequeue_task(os_threadpool_t *tp)
{
	os_task_t *t = NULL;

	pthread_mutex_lock(&tp->mutex_for_variables);
	if (tp->verify_num_nodes == 0 && tp->number_tasks == -1)
		pthread_cond_wait(&tp->cond, &tp->mutex_for_variables);
	pthread_mutex_unlock(&tp->mutex_for_variables);

	pthread_mutex_lock(&(tp->mutex_for_tasks));

	if (tp->number_tasks > 0 && tp->verify_num_nodes != -1) {
		t = list_entry(tp->head.next, os_task_t, list);
		list_del(tp->head.next);

		pthread_mutex_lock(&tp->mutex_for_variables);
		tp->number_tasks--;
		pthread_mutex_unlock(&tp->mutex_for_variables);
	}

	pthread_mutex_unlock(&(tp->mutex_for_tasks));

	return t;
}

static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *)arg;

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);
		if (t == NULL)
			break;

		t->action(t->argument);
		destroy_task(t);
	}

	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	/* TODO: Wait for all worker threads. Use synchronization. */
	// trebuie sa astept pana cand nu mai este niciun thread in derulare

	while (1) { // daca mai sunt taskuri de rulat
		pthread_mutex_lock(&tp->mutex_for_variables);
		if (!(tp->number_tasks > 0 && tp->verify_num_nodes == 0)) {
			pthread_mutex_unlock(&tp->mutex_for_variables);
			break;
		}
		pthread_mutex_unlock(&tp->mutex_for_variables);
	}

	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = NULL;
	int rc;

	tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	/* TODO: Initialize synchronization data. */

	pthread_mutex_init(&(tp->mutex_for_tasks), NULL);
	pthread_mutex_init(&(tp->mutex_for_variables), NULL);
	pthread_cond_init(&(tp->cond), NULL);

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));

	tp->verify_num_nodes = 0;
	tp->number_tasks = -1;

	DIE(tp->threads == NULL, "malloc");
	for (unsigned int i = 0; i < num_threads; ++i) {
		rc = pthread_create(&(tp->threads[i]), NULL, &thread_loop_function, (void *)tp);
		DIE(rc < 0, "pthread_create");
	}

	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;

	/* TODO: Cleanup synchronization data. */
	pthread_mutex_destroy(&(tp->mutex_for_tasks));
	pthread_mutex_destroy(&(tp->mutex_for_variables));
	pthread_cond_destroy(&(tp->cond));

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
