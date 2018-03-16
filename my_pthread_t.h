// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>
#include "mylib.h"

// typedef uint my_pthread_t;
typedef void * my_pthread_t;

typedef struct threadControlBlock {
	/* add something here */
	ucontext_t context;
	char done;
	void * retVal;
	struct threadControlBlock * waiter;
	int priorityLevel;
	struct timeval start;
} tcb; 

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
	/* add something here */
	char guard;
	tcb * locker;
	struct queue * waiters;
} my_pthread_mutex_t;

/* define your data structures here: */

struct queueNode {
	void * data;
	struct queueNode * next;
	struct queueNode * previous;
};

struct queue {
	struct queueNode * head;
	struct queueNode * tail;
};

struct priorityQueue {
	struct queue queue;
	unsigned int timeSlice;
};

// Feel free to add your own auxiliary data structures
extern tcb * currentTcb;

/* Function Declarations: */

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

#endif

#define USE_MY_PTHREAD 1 (comment it if you want to use real pthread)

#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#endif
