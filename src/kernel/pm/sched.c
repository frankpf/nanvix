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
	proc->counter = 0;
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

PUBLIC int NEXT_INDEX(int queue) {
	struct process *p;
	int max = 0;
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		if (p->queue == queue && p->queue_position > max) {
			max = p->queue_position;
		}
	}
	return max+1;
}

PUBLIC int QUEUE_QUANTUM(int queue) {
	return (queue + 1) * BASE_QUANTUM;
}

PUBLIC int YIELD_CALLS = 0;
PUBLIC int REARRANGE_PERIOD = 200;

/**
 * @brief Yields the processor.
 */
PUBLIC void yield(void)
{
	struct process *p;    /* Working process.     */
	struct process *next; /* Next process to run. */

    /* Move all jobs to the topmost queue */
    if (YIELD_CALLS >= REARRANGE_PERIOD) {
    	int i = 0;
    	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
    		p->queue = 0;
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


	/* Move down one queue */
	if (curr_proc->curr_quantum <= 0 && curr_proc->queue < MAX_QUEUE) {
		curr_proc->queue++;
		curr_proc->curr_quantum = QUEUE_QUANTUM(curr_proc->queue);
		curr_proc->counter = curr_proc->curr_quantum;
	} else {
		/* Update current process quantum */
		curr_proc->curr_quantum = curr_proc->counter;
	}

	/* Update queue position to last position */
	curr_proc->queue_position = NEXT_INDEX(curr_proc->queue);

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
	/* Find minimum queue */
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		if (IS_VALID(p) && p->state == PROC_READY && p->queue < min_queue) {
			min_queue = p->queue;
		}
	}
	
	/* Find minimum position */
	next = IDLE;
	int min_position = 1000;
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		/* Skip processes not in min. queue */
		if (p->queue != min_queue) {
			continue;
		}
	
		if (IS_VALID(p) && p->state == PROC_READY && p->queue_position < min_position) {
			min_position = p->queue_position;
			next = p;
		}
	}

	YIELD_CALLS++;
	
	/* Switch to next process. */
	next->priority = PRIO_USER;
	next->state = PROC_RUNNING;
	next->counter = next->curr_quantum;
	switch_to(next);
}
