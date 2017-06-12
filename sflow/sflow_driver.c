/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 */

#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>

#include "sflow_util.h"
#include "sflow_sender.h"
#include "sflow_util.h"
#include "sflow_data_list.h"
#include "expanding_array.h"
#include "sflow_source.h"


/* important values */

/* the list for buffered samples to send */
sflow_data_list_t *sflow_buffered_samples;
/* key for thread specific data */
pthread_key_t sflow_thread_data_key;


/* used to communicate failure to parent process, which spawned the daemon */
static int			sflow_pfds[2];


/*
 * == == == == == == == == == = threads routines
 */
static void
sflowd_refresh()
{
	/*
	 * things might have changed
	 *  - sampling rate
	 *  - sampling interval
	 *  - new datasource, et al.. incorporate these changes
	 */
}

static void
sflowd_fini(void)
{
	/*
	 * free up any resources used
	 * TODO close file
	 */

}

static int
sflow_init_sig() {
	int			err;
	sigset_t	new;

	err = sigfillset(&new);
	if (err < 0)
		return (err);
	err = pthread_sigmask(SIG_BLOCK, &new, NULL);
	return (err);
}

static int
sflow_init_data() {
	sflow_buffered_samples = sflow_data_list_create("samples to send");
	sflow_fd_sources = expanding_array_create();
	sflow_all_sources = calloc(1, sizeof (list_t));
	list_create(sflow_all_sources, sizeof (sflow_source_list_node_t), 0);
	return (0);
}

static int
sflow_init_sender() {
	int err;

	err = sflow_sender_init();
	return (err);
}

/*
 * ARGSUSED
 * This function do select() on all the sample sockets
 */
#define	SFLOW_BIGBUFFER	1024 * 2

/*
 * this single thread keeps checking all the interrupt sources file descriptors,
 * using select().
 * when an interrupt src have data, then this thread will request the get_sample
 * function of the such src to pack it to be a sample and append it to global
 * sample buffer list.
 */
void *
sflow_interrupt_sources_master_thread(void *arg)
{
	int				i;
	int				err;
	int				len = -1;
	struct			timeval tv;
	char			buffer[SFLOW_BIGBUFFER];
	sflow_source_t	*source;

	tv.tv_sec = 0;
	tv.tv_usec = 20;
	FD_ZERO(&to_read);
	FD_ZERO(&to_watch);
	lg("flow sample all : thread is up\n");
	for (;;) {
		if (maxfd < 0) {
				sleep(1);
				continue;
		}
		pthread_mutex_lock(&sflow_inter_src_mod_mtx);
		to_read = to_watch;
		err = select(maxfd+1, &to_read, NULL, NULL, &tv);
		if (err == -1) {
				lg("select error! %s\n", strerror(errno));
				pthread_mutex_unlock(&sflow_inter_src_mod_mtx);
				continue;
		}
		if (err == 0) {
			pthread_mutex_unlock(&sflow_inter_src_mod_mtx);
			usleep(1000); /* let others grab the lock */
			continue;
		}
		for (i = 0; i <= maxfd; i++) {
			if (FD_ISSET(i, &to_read)) {
				len = read(i, buffer, SFLOW_BIGBUFFER);
				if (len >= 0) {
					source =
					    sflow_fd_sources->
					    get(sflow_fd_sources, i);
					lg("\r%d sample is collected: ",
					    source->ss_count++);
					lg("data %u \t fd %u \t name [%s]",
					    len, i, source->ss_name);
					/* should be a inter src */
					assert(source->ss_type <=
					    SF_SRC_FLOWSAMPLE);
					/* request the src type to pack */
					sflow_append_sample_to_send
					    (source->ss_src.ss_i.ss_get_sample(
					    source, buffer, len));
				}
			}
		}
	pthread_mutex_unlock(&sflow_inter_src_mod_mtx);
	continue;
	}
}


void
sflow_cleanup_thread_data(void* data)
{
	lg("Clean thread specific data for \n");
	free(data);
}


static void
sflow_cleanup_handler(void *arg)
{
	lg("Thread exiting!\n");
	/* potential clean ups */
}
/*
 * this is a container thread for polling sources, each polling source will
 * have a thread, which sleeps most of the time, but polls data every interval
 */

void *
sflow_polling_source_instance_container_thread(void *arg)
{
	sflow_instance_thread_data_t	*data;
	sflow_source_t *src;

	ST;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	lg("instance thread is initializing!\n");
	pthread_cleanup_push(sflow_cleanup_handler, NULL);

	assert(!pthread_setspecific(sflow_thread_data_key, arg));
	data = (sflow_instance_thread_data_t *)
	    pthread_getspecific(sflow_thread_data_key);
	assert(data != NULL);
	lg("thread data stored: %s\n", data);

	src = data->src;
	/* has to be a poll src */
	assert(src->ss_type > SF_SRC_FLOWSAMPLE);
	for (;;) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		sleep(src->ss_src.ss_p.ss_interval);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		ST;
		lg("\"%s\" polls data!\n", data->src->ss_name);
		/* request the src type to pack */
		sflow_append_sample_to_send
		    (src->ss_src.ss_p.ss_get_sample(src));
	}
	pthread_cleanup_pop(0);
	return (NULL);
}

static int
sflow_init_threads() {
	int err;
	pthread_attr_t	attr;
	err = pthread_key_create(&sflow_thread_data_key,
	    sflow_cleanup_thread_data);

	if (err < 0) goto clean;
	(void) pthread_attr_init(&attr);
	(void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	err = pthread_create(NULL, &attr, sflow_interrupt_sources_master_thread,
	    NULL);
	if (err < 0) goto clean;
	return (err);

clean:
	(void) pthread_attr_destroy(&attr);
	return (err);
}

static int
sflow_init_door() {
	int err;

	err = sflow_open_door();
	return (err);
}


static int
sflowd_init(void)
{
	int				err;

	ST;
	lg("sflow service is initializing!\n");
	err = sflow_init_sig();
	if (err < 0)
		goto clean;
	err = sflow_init_data();
	if (err < 0)
		goto clean;
	err = sflow_init_sender();
	if (err < 0)
		goto clean;
	err = sflow_init_threads();
	if (err < 0)
		goto clean;
	err = sflow_init_door();
	if (err < 0)
		goto clean;

	return (err);

clean:
	return (err);
}



/*
 * == == == == == == == == == = need not touch below code for now
 */

/* ARGSUSED */
static void *
sighandler(void *arg)
{
	int			sig;
	sigset_t	sigset;

	(void) sigfillset(&sigset);
	for (;;) {
		sig = sigwait(&sigset);
		switch (sig) {
		case SIGHUP:
			sflowd_refresh();
			break;
		case SIGALRM:
			/*
			 * THIS SHOULD NOT TAKE LONGER THAN THE RANGE OF
			 * MILLISECONDS
			 * WE ARE NOT SUPPOSE TO HAVE NEW SIGALARM QUICKLY
			 * AFTER A PREVIOUS ONE EITHER
			 */
			lg("time up , send!");
			pthread_mutex_lock(&sflow_buffered_samples_mtx);
			send_samples();
			pthread_mutex_unlock(&sflow_buffered_samples_mtx);
			lg("....sent\n!");
			break;
		default:
			/* clean up before exiting */
			(void) close(sflow_pfds[1]);
			exit(EXIT_FAILURE);
		}
	}
	/* NOTREACHED */
	return (NULL);
}

static int
sflowd_init_signal_handling(void)
{
	int				err;
	sigset_t		new;
	pthread_attr_t	attr;

	sflow_unit_test();
	(void) sigfillset(&new);
	(void) pthread_sigmask(SIG_BLOCK, &new, NULL);
	(void) pthread_attr_init(&attr);
	(void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	err = pthread_create(NULL, &attr, sighandler, NULL);
	(void) pthread_attr_destroy(&attr);
	return (err);
}

/*
 * This is called by the child process to inform the parent process to
 * exit with the given return value.
 */
static void
sflowd_inform_parent_exit(int rv)
{
	if (write(sflow_pfds[1], &rv, sizeof (int)) != sizeof (int)) {
		lg("failed to inform parent process of status: %s",
		    strerror(errno));
		(void) close(sflow_pfds[1]);
		exit(EXIT_FAILURE);
	}
	(void) close(sflow_pfds[1]);
}
/*
 * Keep the sflow_pfds fd open, close other fds.
 */

/*ARGSUSED*/
static int
closefunc(void *arg, int fd)
{
	if (fd != sflow_pfds[1])
		(void) close(fd);
	return (0);
}

/*
 * We cannot use libc's daemon() because the door we create is associated with
 * the process ID. If we create the door before the call to daemon(), it will
 * be associated with the parent and it's incorrect. On the other hand if we
 * create the door later, after the call to daemon(), parent process exits
 * early and gives a false notion to SMF that 'sflowd' is up and running,
 * which is incorrect. So, we have our own daemon() equivalent.
 */
static boolean_t
sflowd_daemonize(void)
{
	int		rv;
	pid_t	pid;

	if (pipe(sflow_pfds) < 0) {
		lg("%s: pipe() failed: %s\n",
		    getprogname(), strerror(errno));
		exit(EXIT_FAILURE);
	}
	if ((pid = fork()) == -1) {
		lg("%s: fork() failed: %s\n",
		    getprogname(), strerror(errno));
		exit(EXIT_FAILURE);
	} else if (pid > 0) { /* Parent */
		(void) close(sflow_pfds[1]);
		/*
		 * Parent should not exit early, it should wait for the child
		 * to return Success/Failure. If the parent exits early, then
		 * SMF will think 'sflowd' is up and would start all the
		 * depended services.
		 *
		 * If the child process exits unexpectedly, read() returns -1.
		 */
		if (read(sflow_pfds[0], &rv, sizeof (int)) != sizeof (int)) {
			(void) kill(pid, SIGKILL);
			rv = EXIT_FAILURE;
		}
		(void) close(sflow_pfds[0]);
		exit(rv);
	}
	/* Child */
	(void) setsid();
	/* close all files except sflow_pfds[1] */
	(void) fdwalk(closefunc, NULL);
	(void) chdir("/");
	/* open log */
	openlog(getprogname(), LOG_PID | LOG_NDELAY, LOG_DAEMON);
	/* open log file */
	sflow_log_file = fopen(sflow_log_file_path, "w");

	return (B_TRUE);
}

int
main(int argc, char *argv[])
{
	int			opt;
	boolean_t	fg = B_FALSE;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	/* Process options */
	while ((opt = getopt(argc, argv, "f")) != EOF) {
		switch (opt) {
		case 'f':
			fg = B_TRUE;
			break;
		default:
			fprintf(stderr, "Usage: %s [-f]\n",
			    getprogname());
			exit(EXIT_FAILURE);
		}
	}
	if (!fg && getenv("SMF_FMRI") == NULL) {
		fprintf(stderr,
		    "sflowd is a smf(5) managed "
		    "service and cannot be run "
		    "from the command line. Aborted\n");
		exit(EXIT_FAILURE);
	}
	if (!fg && !sflowd_daemonize()) {
		fprintf(stderr, "could not daemonize sflowd: %s",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}
	lg("printing syslog message");
	/* initialize signal handling thread */
	if (sflowd_init_signal_handling() != 0) {
		lg("Could not start signal handling thread");
		goto child_out;
	}
	if (sflowd_init() != 0) {
		lg("Could not initialize the sflow daemon");
		goto child_out;
	}
	/* Inform the parent process that it can successfully exit */
	if (!fg)
		sflowd_inform_parent_exit(EXIT_SUCCESS);
	for (;;)
		(void) pause();
child_out:
	/* return from main() forcibly exits an MT process */
	if (!fg)
		sflowd_inform_parent_exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}
