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

#ifndef _COUNTED_LIST
#define	_COUNTED_LIST

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include <sys/list.h>
/*
 * OO style wrapped for list decleared in sys/list.h,
 * with optional count and data size
 */

#define	NAMELEN 32
#define	CHECK_DATA -1 /* size is recorded in data */

typedef struct sflow_data_list_node
{
	char	sdln_name[NAMELEN];
	void	*sdln_data;
	int	sdln_data_size;
	struct	list_node sdln_node;
}sflow_data_list_node_t;

sflow_data_list_node_t *
sflow_data_list_node_create(const char *name_in, void *data_in,
    int data_size_in);

void
sflow_data_list_node_free(sflow_data_list_node_t  *node_in);

typedef struct sflow_data_list
{
	/* tostring() */
	char		sdl_name[NAMELEN];
	uint32_t	sdl_count;
	int		sdl_data_size;
	struct list	sdl_list;
}sflow_data_list_t;

sflow_data_list_t *
sflow_data_list_create(char *name_in);

/* this frees an empty list */
void
sflow_data_list_free(sflow_data_list_t *list);

sflow_data_list_t *
sflow_data_list_append(sflow_data_list_t *list, \
sflow_data_list_node_t *sdln_node);

sflow_data_list_t *
sflow_data_list_read(sflow_data_list_t *list, \
sflow_data_list_node_t **sdln_node);

#endif	/* _COUNTED_LIST */
