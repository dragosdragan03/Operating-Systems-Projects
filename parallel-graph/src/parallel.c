// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_threadpool_t *tp;
pthread_mutex_t mutex;
static os_graph_t *graph;
/* TODO: Define graph synchronization mechanisms. */

/* TODO: Define graph task argument. */
void action(void *idx)
{
	os_node_t *node;

	node = graph->nodes[*(int *)idx];
	pthread_mutex_lock(&mutex);
	sum += node->info;
	graph->visited[*(int *)idx] = DONE;
	pthread_mutex_unlock(&mutex);


	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&mutex);

		pthread_mutex_lock(&tp->mutex_for_variables);
		if (tp->number_tasks == -1)
			tp->number_tasks = 0;

		pthread_mutex_unlock(&tp->mutex_for_variables);

		if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
			graph->visited[node->neighbours[i]] = PROCESSING;
			os_task_t *t = create_task(action, &node->neighbours[i], NULL);

			enqueue_task(tp, t); // bag un task in queue
			pthread_mutex_lock(&tp->mutex_for_variables);
			tp->number_tasks++;
			pthread_mutex_unlock(&tp->mutex_for_variables);
		}
		pthread_mutex_unlock(&mutex);
	}
}

static void process_node(unsigned int idx)
{
	/* TODO: Implement thread-pool based processing of graph. */

	if (graph->num_edges == 0) { // verific sa vad daca graful meu are legaturi
		pthread_mutex_lock(&tp->mutex_for_variables);
		tp->verify_num_nodes = -1;
		pthread_cond_broadcast(&tp->cond);
		pthread_mutex_unlock(&tp->mutex_for_variables);
	}

	action((void *)&idx);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}
