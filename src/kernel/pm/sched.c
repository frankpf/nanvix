/*
 * Copyright(C) 2011-2016 Pedro H. Penna   <pedrohenriquepenna@gmail.com>
 *              2015-2016 Davidson Francis <davidsondfgl@hotmail.com>
 *
 * This file is part of Nanvix.
 *
 * Nanvix is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nanvix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nanvix. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nanvix/klib.h>
#include <nanvix/clock.h>
#include <nanvix/const.h>
#include <nanvix/hal.h>
#include <nanvix/pm.h>
#include <signal.h>

/*****************************************
 * FIFO QUEUE
 *****************************************/


void rb_init(struct ring_buffer *rb) {
	rb->count = 0;
	rb->head = -1;
	rb->tail = -1;
}

int rb_enqueue(struct ring_buffer *rb, rb_val *item) {
	if (rb_full(rb)) {
		return 1;
	}

	rb->count++;
	rb->tail = (rb->tail+1) % QUEUE_SIZE;
	rb->buffer[rb->tail] = item;
	return 0;
}

rb_val *rb_dequeue(struct ring_buffer *rb) {
	if (rb_empty(rb)) {
		return NULL;
	}

	rb->count--;
	rb->head = (rb->head+1) % QUEUE_SIZE;
	return rb->buffer[rb->head];
}

int rb_full(struct ring_buffer *rb) {
	return rb->count == QUEUE_SIZE;
}

int rb_empty(struct ring_buffer *rb) {
	return rb->count == 0;
}

struct ring_buffer *queues[NUM_QUEUES];

int QUEUES_INIT = 0;
void init_queues(void) {
	int i;
	for (i = 0; i < NUM_QUEUES; i++) {
		rb_init(queues[i]);
	}
	QUEUES_INIT = 1;
}

PUBLIC void enter_system(struct process *proc) {
	if (!QUEUES_INIT) init_queues();
	int initial_queue = 0;
	proc->state = PROC_READY;
	proc->queue = initial_queue;
	proc->counter = queue_quantum(initial_queue);
	rb_enqueue(queues[initial_queue], proc);
}

/**
 * @brief Stops the current running process.
 */
PUBLIC void stop(void)
{
	curr_proc->state = PROC_STOPPED;
	sndsig(curr_proc->father, SIGCHLD);
	yield();
}

PUBLIC void sched(struct process *proc)
{
	proc->state = PROC_READY;
	proc->counter = queue_quantum(proc->queue);
}

/**
 * @brief Resumes a process.
 * 
 * @param proc Process to be resumed.
 * 
 * @note The process must stopped to be resumed.
 */
PUBLIC void resume(struct process *proc)
{	
	/* Resume only if process has stopped. */
	if (proc->state == PROC_STOPPED)
		sched(proc);
}

/* Returns the next available index on a given queue */
PUBLIC int next_index(int queue) {
	struct process *p;
	int max = 0;
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		if (p->queue == queue && p->queue_position > max) {
			max = p->queue_position;
		}
	}
	return max+1;
}

/* Returns the quantum for the given queue */
PUBLIC int queue_quantum(int queue) {
	return (queue + 1) * BASE_QUANTUM;
}

/* Used to implement the queue feedback mechanism */
#define REARRANGE_PERIOD 32768
PRIVATE int YIELD_CALLS = 0;

/**
 * @brief Yields the processor.
 */
PUBLIC void yield(void)
{
	if (!QUEUES_INIT) init_queues();
	struct process *p;    /* Working process.     */
	struct process *next; /* Next process to run. */

	int err;

	/* Re-schedule process for execution. */
	if (curr_proc->state == PROC_RUNNING)
		curr_proc->state = PROC_READY;

	/* Remember this process. */
	last_proc = curr_proc;

	if (curr_proc->counter == 0) {
		if (curr_proc->queue < NUM_QUEUES) {
			/* Move down one queue if not on the last one */
			curr_proc->queue++;
		}
		/* Update the quantum to be the queue's quantum */
		curr_proc->counter = queue_quantum(curr_proc->queue);
		err = rb_enqueue(queues[curr_proc->queue], curr_proc);
		if (err == 1) {
			kprintf("ERROR: Queue %d full!", curr_proc->queue);
		}
	}

	/* Check alarm. */
	for (p = FIRST_PROC; p <= LAST_PROC; p++)
	{
		/* Skip invalid processes. */
		if (!IS_VALID(p))
			continue;
		
		/* Alarm has expired. */
		if ((p->alarm) && (p->alarm < ticks))
			p->alarm = 0, sndsig(p, SIGALRM);
	}


	next = IDLE;
	int i;
	for (i = 0; i < NUM_QUEUES; i++) {
		struct ring_buffer *queue = queues[i];
		struct process *item = rb_dequeue(queue);
		if (item != NULL) {
			next = item;
			break;
		}
	}

	YIELD_CALLS++;
	
	/* Switch to next process. */
	next->priority = PRIO_USER;
	next->state = PROC_RUNNING;
	switch_to(next);
}
