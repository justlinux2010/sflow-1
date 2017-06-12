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

#ifndef _SFLOW_SOURCE_H
#define	_SFLOW_SOURCE_H

/*
 * This is definition for data sources in sflow.
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <pthread.h>
#include <sys/list.h>

#include "sflow_door.h"
#include "expanding_array.h"

/* zone name can get long */
#define	SF_NAME_LEN 64
#define	SF_ATTR_LEN 32

typedef enum {
	SF_SRC_FLOWSAMPLE	= 0,
	SF_SRC_INTER_POLL_SEPERATION, /* line between inter and poll */
	SF_SRC_IFGENERIC,
	SF_SRC_PHYHOST,
	SF_SRC_VHOST,
	SF_SRC_FLOWSTAT
} sflow_source_type_t;

typedef struct sflow_source_s sflow_source_t;
struct sflow_sp_s;

typedef	struct sflow_sp_s *
get_sample_poll(sflow_source_t *);

typedef	struct sflow_sp_s *
get_sample_interrupt(sflow_source_t *, char *, int);

typedef struct {
	pthread_t ss_thread;
	int ss_interval;
	get_sample_poll *ss_get_sample;
} sflow_source_poll_t;

typedef struct {
	int ss_fd;
	int ss_rate;
	get_sample_interrupt *ss_get_sample;
} sflow_source_interrupt_t;


struct sflow_source_s {
	char ss_name[SF_NAME_LEN];
	sflow_source_type_t ss_type;
	union {
		sflow_source_poll_t ss_p;
		sflow_source_interrupt_t ss_i;
	} ss_src;
	int	ss_count;
	int	ss_id_type;
	int	ss_id;
	int	ss_ent;
	int	ss_fmt;
	/* optional */
	void * src_specific_data;
};

typedef struct {
    list_node_t		ssln_node;
    sflow_source_t	*ssln_src;
} sflow_source_list_node_t;


/* functions */
int
sflow_add_source(sflowd_door_asrc_t *);

extern int maxfd;
/* the sources for select to monitor, indexed by associated fds */
extern expanding_array_t *sflow_fd_sources;
/* the list of all the sources */
extern list_t *sflow_all_sources;

extern fd_set	to_read, to_watch;
extern pthread_mutex_t	sflow_inter_src_mod_mtx;


/* thread specific data */
typedef struct sflow_instance_thread_data
{
	sflow_source_t *src;
} sflow_instance_thread_data_t;



#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_SOURCE_H */
