/*
 * Copyright (c) 2005 Christophe Varoqui
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>

#include <memory.h>

#include "log_pthread.h"
#include "log.h"

static void
sigusr1 (int sig)
{
	pthread_mutex_lock(logq_lock);
	log_reset("multipathd");
	pthread_mutex_unlock(logq_lock);
}

void log_safe (int prio, const char * fmt, va_list ap)
{
	pthread_mutex_lock(logq_lock);
	log_enqueue(prio, fmt, ap);
	pthread_mutex_unlock(logq_lock);

	pthread_mutex_lock(logev_lock);
	pthread_cond_signal(logev_cond);
	pthread_mutex_unlock(logev_lock);
}

static void flush_logqueue (void)
{
	int empty;

	do {
		pthread_mutex_lock(logq_lock);
		empty = log_dequeue(la->buff);
		pthread_mutex_unlock(logq_lock);
		if (!empty)
			log_syslog(la->buff);
	} while (empty == 0);
}

static void * log_thread (void * et)
{
	struct sigaction sig;

	sig.sa_handler = sigusr1;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = 0;
	if (sigaction(SIGUSR1, &sig, NULL) < 0)
		logdbg(stderr, "Cannot set signal handler");

	mlockall(MCL_CURRENT | MCL_FUTURE);
	logdbg(stderr,"enter log_thread\n");

	while (1) {
		pthread_mutex_lock(logev_lock);
		pthread_cond_wait(logev_cond, logev_lock);
		pthread_mutex_unlock(logev_lock);

		flush_logqueue();
	}
}

void log_thread_start (void)
{
	pthread_attr_t attr;
	size_t stacksize;

	logdbg(stderr,"enter log_thread_start\n");

	if (pthread_attr_init(&attr)) {
		fprintf(stderr,"can't initialize log thread\n");
		exit(1);
	}

	if (pthread_attr_getstacksize(&attr, &stacksize) != 0)
		stacksize = PTHREAD_STACK_MIN;

	/* Check if the stacksize is large enough */
	if (stacksize < (64 * 1024))
		stacksize = 64 * 1024;

	/* Set stacksize and try to reinitialize attr if failed */
	if (stacksize > PTHREAD_STACK_MIN &&
	    pthread_attr_setstacksize(&attr, stacksize) != 0 &&
	    pthread_attr_init(&attr)) {
		fprintf(stderr,"can't set log thread stack size\n");
		exit(1);
	}

	logq_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	logev_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	logev_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));

	pthread_mutex_init(logq_lock, NULL);
	pthread_mutex_init(logev_lock, NULL);
	pthread_cond_init(logev_cond, NULL);

	if (log_init("multipathd", 0)) {
		fprintf(stderr,"can't initialize log buffer\n");
		exit(1);
	}
	pthread_create(&log_thr, &attr, log_thread, NULL);

	return;
}

void log_thread_stop (void)
{
	logdbg(stderr,"enter log_thread_stop\n");

	pthread_mutex_lock(logq_lock);
	pthread_cancel(log_thr);
	pthread_mutex_unlock(logq_lock);
	pthread_join(log_thr, NULL);

	flush_logqueue();

	pthread_mutex_destroy(logq_lock);
	pthread_mutex_destroy(logev_lock);
	pthread_cond_destroy(logev_cond);

	free(logq_lock);
	logq_lock = NULL;
	free(logev_lock);
	logev_lock = NULL;
	free(logev_cond);
	logev_cond = NULL;
	free_logarea();
}

void log_thread_reset (void)
{
	pthread_kill(log_thr, SIGUSR1);
}
