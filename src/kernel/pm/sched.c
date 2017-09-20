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

/**
 * @brief Schedules a process to execution.
 * 
 * @param proc Process to be scheduled.
 */
PUBLIC void sched(struct process *proc)
{
	proc->state = PROC_READY;
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

/* Returns the quantum for the given queue */
PUBLIC int queue_quantum(int queue) {
	return (queue * 10) + PROC_QUANTUM;
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

/* Used to implement the queue feedback mechanism */
#define REARRANGE_PERIOD 100000
PRIVATE int YIELD_CALLS = 0; /**< Number of times yield() has run */

/**
 * @brief Yields the processor.
 */
PUBLIC void yield(void)
{
	struct process *p;    /* Working process.     */
	struct process *next; /* Next process to run. */
	
	
	/* Shows current process name, remaining quantum and queue */
	if (curr_proc->counter != queue_quantum(curr_proc->queue) && (kstrcmp(curr_proc->name, "ls")==0)) {  
	 kprintf("%s->%d, %d", curr_proc->name, curr_proc->counter, curr_proc->queue);
	 }

    /* Move all jobs to the topmost queue */
    if (YIELD_CALLS >= REARRANGE_PERIOD) {
    	int i = 0;
    	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
    		p->queue = 0;
			p->counter = queue_quantum(0);
    		/* Workaround to prevent integer overflow */
    		p->queue_position = i;
    		i++;
    	}
    	YIELD_CALLS = 0;
    }

	/* Re-schedule process for execution. */
	if (curr_proc->state == PROC_RUNNING)
		sched(curr_proc);

	/* Remember this process. */
	last_proc = curr_proc;

	if (curr_proc->counter == 0) {
		if (curr_proc->queue < MAX_QUEUE) {
			/* Move down one queue if not on the last one */
			curr_proc->queue++;
		}
		/* Update the quantum to be the queue's quantum */
		curr_proc->counter = queue_quantum(curr_proc->queue);
	}
	/* Update queue position to next open position */
	curr_proc->queue_position = next_index(curr_proc->queue);

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

	int min_queue = MAX_QUEUE;
	/* Find queue with the highest priority (minimum queue) */
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		if ((p->state == PROC_READY || p->state == PROC_RUNNING) && p->queue < min_queue) {
			min_queue = p->queue;
		}
	}
	
	/* Find the first item of the chosen queue */
	next = IDLE;
	int min_position = INT_MAX;
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		/* Skip processes not in min. queue */
		if (p->queue != min_queue) {
			continue;
		}
	
		if ((p->state == PROC_READY || p->state == PROC_RUNNING) && p->queue_position < min_position) {
			min_position = p->queue_position;
			next = p;
		}
	}

	YIELD_CALLS++;
	
	/* Switch to next process. */
	next->priority = PRIO_USER;
	next->state = PROC_RUNNING;
	switch_to(next);
}
