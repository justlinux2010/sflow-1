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

#ifndef _SFLOW_UTIL_H
#define	_SFLOW_UTIL_H

/*
 * PROGRAMMING NOTE
 *
 * If certain data is needed by more than one type of sources, it is
 * advisable to implement the data collection function in this utility
 * file, to reduce duplicated code in various source types.
 */
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "sflow_protocol_branches.h"
#include "sflow_protocol_leaf_hostphy.h"

#define	ST	sflow_show_time()

#define	SFLOW_NHSWITCH_ON
#ifdef	SFLOW_NHSWITCH_ON
#define	SFLOW_HTONL(h)	htonl(h) /* htonl switch */
#define	SFLOW_NTOHL(h)	ntohl(h) /* ntohl switch */
#else
#define	SFLOW_HTONL(h)	(h) /* htonl switch */
#define	SFLOW_NTOHL(h)	(h) /* ntohl switch */
#endif
#define	SFLOW_VER	(5)
#define	SFLOW_IPVER	(4)
#define	SFLOW_DUMMY	SFLOW_HTONL(52)
#define	SFLOW_UNKOWN	SFLOW_HTONL(~((uint32_t)0))
#define	SFLOW_DUMMY64	htonH((hyper)52)
#define	SFLOW_UNKOWN64	htonH(~((hyper)0))

#define	SFLOW_PSIZE	(sizeof (void *))



void
lg(const char *fmt, ...);

void
sflow_show_time();

/* hyper to network format */
hyper
htonH(hyper num);

char *
sflow_get_agent_ip();

int
sflow_unit_test();

uint32_t
sflow_get_link_stat_32(const char *name, const char *stat);

uint64_t
sflow_get_link_stat_64(const char *name, const char *stat);

int
sflow_get_if_link_id(char *if_name);


int
sflow_get_one_kstat(const char *, const char *, uint8_t,
    void *, boolean_t);

char *
toCharArray(int num, int len);

void
mac_convert_str_to_char(char *buf, const char *str);


extern	FILE	*sflow_log_file;
extern	char	*sflow_log_file_path;

/* making dummy data */
void
randomize(void *start, size_t s);

/* print raw memory */
void
mem_print(const char *name, void *start, size_t len);

void
itoa(long n, char *s); /* from cmd/priocntl/subr.c " */

uint32_t
get_up_time_sec();

/* get physical machine type */
machine_type_t
get_host_phys_machine_type();

/* get physical page size */
uint32_t
get_host_phys_page_size();

/* get physical total memory */
hyper
get_host_phys_mem_bytes();

#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_UTIL_H */
