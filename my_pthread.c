// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name: Ammaar Muhammad Iqbal
// username of iLab: ami76 rbp61
// iLab Server: grep.cs.rutgers.edu

/////////////////////////////////////////
// All macro times are in microseconds //
/////////////////////////////////////////
#define TEMP_SIZE 4096
// Number of priority queues
#define NUM_PRIORITY_LVLS 4
// Interval time to run scheduler
#define INTERRUPT_TIME 25000
// Time slice for highest priority, each
// priority level has a time slice x2 of
// the priority level above it
#define BASE_TIME_SLICE INTERRUPT_TIME

#include "my_pthread_t.h"

// Macros for making library malloc calls
#undef malloc
#undef free
#define malloc(x) myallocate(x, __FILE__, __LINE__, LIBRARYREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, LIBRARYREQ)

// Function from mylib.c for comunication
void * myallocate(size_t size, char * fileName, int lineNumber, int request);
void mydeallocate(void * ptr, char * fileName, int lineNumber, int request);
void protectAllPages(tcb * thread);
void unprotectAllPages(tcb * thread);

// Checks if library is properly initialized
char initialized = 0;
// Exit context
ucontext_t exitContext;
// Checks if sheduler should be blocked
char block = 0;
// Pointer to the currently running thread's tcb
tcb * currentTcb = NULL;
// High priority queue
struct priorityQueue PQs[NUM_PRIORITY_LVLS];

// Returns the time between <start> and <end>
suseconds_t getElapsedTime(struct timeval * start, struct timeval * end) {
	time_t seconds = end->tv_sec - start->tv_sec;
	suseconds_t microseconds = end->tv_usec - start->tv_usec;
	if (seconds == 0) { return microseconds; }
	else { return (seconds * 1000000) + microseconds; }
}

// Initializes <PQs>
void initializePQs() {
	int i;
	for (i = 0; i < NUM_PRIORITY_LVLS; i++) {
		if (i == 0) { PQs[i].timeSlice = BASE_TIME_SLICE; }
		else { PQs[i].timeSlice = PQs[i-1].timeSlice * 2; }
		PQs[i].queue.head = NULL;
		PQs[i].queue.tail = NULL;
	}
}

// Initializes a new tcb
tcb * getNewTcb() {
	tcb * ret = malloc(sizeof(tcb));
	ret->done = 0;
	ret->retVal = NULL;
	ret->waiter = NULL;
	ret->priorityLevel = 0;
	return ret;
}

// Enqueue <data> into <queue>
void enqueue(void * data, struct queue * queue) {

	struct queueNode * node = malloc(sizeof(struct queueNode));
	node->data = data;
	node->previous = NULL;

	if (queue->tail == NULL) {
		node->next = NULL;
		queue->head = node;
		queue->tail = node;

	} else {
		node->next = queue->head;
		queue->head->previous = node;
		queue->head = node;
	}
}

// Dequeues from <queue>
void * dequeue(struct queue * queue) {

	if (queue->tail == NULL) {
		return NULL;

	} else {
		void * data = queue->tail->data;
		struct queueNode * newTail = queue->tail->previous;
		free(queue->tail);
		queue->tail = newTail;
		return data;
	}
}

// Removes <data> from <queue> and returns 1,
// returns 0 if not found
char removeFromQueue(void * data, struct queue * queue) {

	struct queueNode * trav = queue->tail;

	while (trav != NULL) {

		if (trav->data == data) {

			if (trav->next != NULL) {
				trav->next->previous = trav->previous;
			}

			if (trav->previous != NULL) {
				trav->previous->next = trav->next;
			}

			free(trav);
			return 1;
		}

		trav = trav->previous;
	}

	return 0;
}

// Returns the next tcb and removes it from the queue,
// NULL if no threads in queue
tcb * getNextTcb() {
	int i;
	for (i = 0; i < NUM_PRIORITY_LVLS; i++) {
		tcb * ret = dequeue(&(PQs[i].queue));
		if (ret != NULL) { return ret; }
	}
	return NULL;
}

// Schedules threads
void schedule(int signum) {

	// Only run if scheduler isn't blocked
	if (!__sync_val_compare_and_swap(&block, 0, 1)) {

		// Get the runtime of the previous thread
		struct timeval now;
		gettimeofday(&now, NULL);
		suseconds_t previousRunTime = 0;
		unsigned int previousTimeSlice = 0;
		if (currentTcb != NULL) {
			previousRunTime = getElapsedTime(&(currentTcb->start), &now);
			previousTimeSlice = PQs[currentTcb->priorityLevel].timeSlice;
		}

		// If thread ran long enough or no previous thread, context switch
		if (previousRunTime >= previousTimeSlice) {

			tcb * nextTcb = getNextTcb();

			// If there is a thread in the queue, schedule it next
			if (nextTcb != NULL) {

				tcb * previousTcb = currentTcb;
				currentTcb = nextTcb;
					
				// Run the next thread, and save the previous thread
				// if there is one
				if (previousTcb == NULL) { 
					gettimeofday(&(currentTcb->start), NULL);
					block = 0;
					unprotectAllPages(currentTcb);
					setcontext(&(currentTcb->context));

				} else {

					// Decrease the priority level of previous thread if not already
					// at the lowest priority, else increase to highest priority
					// as the maintenance cycle
					if (previousTcb->priorityLevel < (NUM_PRIORITY_LVLS - 1)) {
						(previousTcb->priorityLevel)++;
					} else { previousTcb->priorityLevel = 0; }

					// Swap the threads
					enqueue(previousTcb, &(PQs[previousTcb->priorityLevel].queue));
					gettimeofday(&(currentTcb->start), NULL);
					block = 0;
					protectAllPages(previousTcb);
					unprotectAllPages(currentTcb);
					swapcontext(&(previousTcb->context), &(currentTcb->context));
				}
		
			} else { block = 0; }

		}  else { block = 0; }
	}
}

// Initializes the thread library
void initializeThreads() {

	// This block only runs on the first call to
	// my_pthread_create
	if (!initialized) {

		block = 1;

		// Initialize the priority queues
		initializePQs();

		// Save pthread_exit context
		getcontext(&exitContext);
		void * exitStack = malloc(TEMP_SIZE);
		exitContext.uc_link = NULL;
		exitContext.uc_stack.ss_size = TEMP_SIZE;
		exitContext.uc_stack.ss_sp = exitStack;
		makecontext(&exitContext, (void (*)(void)) my_pthread_exit, 1, NULL);

		// Catch itimer signal
		signal(SIGVTALRM, schedule);

		// Start itimer
		struct itimerval * timer = malloc(sizeof(struct itimerval));
		timer->it_value.tv_sec = 0;
		timer->it_value.tv_usec = INTERRUPT_TIME;
		timer->it_interval.tv_sec = 0;
		timer->it_interval.tv_usec = INTERRUPT_TIME;
		setitimer(ITIMER_VIRTUAL, timer, NULL);

		// Cretae tcb for first caller
		currentTcb = getNewTcb();

		initialized = 1;
		block = 0;
	}
}

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {

	initializeThreads();
	block = 1;

	// Create the new thread and add it to high priority
	tcb * newTcb = getNewTcb();
	getcontext(&(newTcb->context));
	void * newThreadStack = malloc(TEMP_SIZE);
	newTcb->context.uc_link = &exitContext;
	newTcb->context.uc_stack.ss_size = TEMP_SIZE;
	newTcb->context.uc_stack.ss_sp = newThreadStack;
	makecontext(&(newTcb->context), (void (*)(void)) function, 1, arg);
	*thread = newTcb;
	enqueue(newTcb, &(PQs[0].queue));

	block = 0;
	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {

	block = 1;

	tcb * nextTcb = getNextTcb();
	
	// If there is a thread in the queue, run that and save the
	// yielding thread with the same priority it had
	if (nextTcb != NULL) {
		tcb * previousTcb = currentTcb;
		currentTcb = nextTcb;
		gettimeofday(&(currentTcb->start), NULL);
		enqueue(previousTcb, &(PQs[previousTcb->priorityLevel].queue));
		protectAllPages(previousTcb);
		unprotectAllPages(currentTcb);
		block = 0;
		swapcontext(&(previousTcb->context), &(currentTcb->context));
	}

	block = 0;
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {

	block = 1;

	// Set the exiting thread to done, and save its
	// return value
	currentTcb->done = 1;
	currentTcb->retVal = value_ptr;

	// If the exiting thread has another thread
	// waiting on it, put the waiting thread in
	// the queue so it can be run later
	if (currentTcb->waiter != NULL) {
		currentTcb->waiter->priorityLevel = 0;
		enqueue(currentTcb->waiter, &(PQs[0].queue));
	}

	protectAllPages(currentTcb);
	currentTcb = NULL;

	block = 0;
	schedule(0);
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {

	block = 1;

	// Retrieve the tcb of the joining thread
	tcb * joining = thread;

	// If the the joining thread isn't done,
	// refrence the waiting (this) thread in
	// the joining thread's tcb and wait
	if (!(joining->done)) {

		// Swap the waiter with the next thread
		joining->waiter = currentTcb;
		currentTcb = getNextTcb();
		gettimeofday(&(currentTcb->start), NULL);
		protectAllPages(joining->waiter);
		unprotectAllPages(currentTcb);
		block = 0;
		swapcontext(&(joining->waiter->context), &(currentTcb->context));
	}

	block = 0;

	// If <value_ptr> is not null, make it point to the
	// joining thread's return value.
	if (value_ptr != NULL) { *value_ptr = joining->retVal; }

	// Release ressources of the joining thread
	free(joining->context.uc_stack.ss_sp);
	free(joining);

	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	mutex->locker = NULL;
	mutex->waiters = malloc(sizeof(struct queue));
	mutex->waiters->head = NULL;
	mutex->waiters->tail = NULL;
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {

	while (__sync_lock_test_and_set(&(mutex->guard), 1));

	if (mutex->locker != NULL) {

		// Swap the locked waiter with the next thread
		block = 1;
		tcb * previousTcb = currentTcb;
		currentTcb = getNextTcb();
		enqueue(previousTcb, mutex->waiters);
		mutex->guard = 0;

		// Increases priority of the locker to the priority of the waiter
		// if the waiter's priority is higher for priority inversion
		if (previousTcb->priorityLevel > mutex->locker->priorityLevel) {
			removeFromQueue(mutex->locker, &(PQs[mutex->locker->priorityLevel].queue));
			mutex->locker->priorityLevel = previousTcb->priorityLevel;
			enqueue(mutex->locker, &(PQs[mutex->locker->priorityLevel].queue));
		}

		gettimeofday(&(currentTcb->start), NULL);
		protectAllPages(previousTcb);
		unprotectAllPages(currentTcb);
		block = 0;
		swapcontext(&(previousTcb->context), &(currentTcb->context));

	} else {
		mutex->locker = currentTcb;
		mutex->guard = 0;
	}

	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {

	if (mutex->locker == currentTcb) {

		while (__sync_lock_test_and_set(&(mutex->guard), 1));

		tcb * waiter = dequeue(mutex->waiters);

		// If no thread is waiting on the lock then release it
		// otherwise put the waiter on the queue with the priority
		// level of the unloker aka the highest priority level of
		// all the waiters for the priority inversion
		if (waiter == NULL) {
			mutex->locker = NULL;

		} else {
			waiter->priorityLevel = mutex->locker->priorityLevel;
			mutex->locker = waiter;
			enqueue(waiter, &(PQs[waiter->priorityLevel].queue));
		}

		mutex->guard = 0;
	}

	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	free(mutex->waiters);
	return 0;
};
