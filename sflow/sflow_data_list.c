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


#include "sflow_data_list.h"

sflow_data_list_node_t *
sflow_data_list_node_create(const char *name_in, void *data_in, \
int data_size_in)
{
	sflow_data_list_node_t *tr = (sflow_data_list_node_t *)
	    malloc(sizeof (sflow_data_list_node_t));

	strcpy(tr->sdln_name, name_in);
	tr->sdln_data = data_in;
	tr->sdln_data_size = data_size_in;
	tr->sdln_node.list_next = NULL;
	tr->sdln_node.list_prev = NULL;
	return (tr);
}

void
sflow_data_list_node_free(sflow_data_list_node_t *node_in)
{
		/* assert(node_in->data == NULL); */
		free(node_in);
}

sflow_data_list_t *
sflow_data_list_create(char *name_in)
{
	sflow_data_list_t *tr =
	    (sflow_data_list_t *)malloc(sizeof (sflow_data_list_t));

	strcpy(tr->sdl_name, name_in);
	tr->sdl_data_size = 0;
	tr->sdl_count = 0;
	list_create(&(tr->sdl_list), sizeof (sflow_data_list_node_t),
	    offsetof(sflow_data_list_node_t, sdln_node));
	return (tr);
}

/* this frees an empty list */
void
sflow_data_list_free(sflow_data_list_t *list)
{
		assert(list->sdl_count == 0);
		list_destroy(&(list->sdl_list));
		free(list);
}

sflow_data_list_t *
sflow_data_list_append(sflow_data_list_t *list, sflow_data_list_node_t *node)
{
		list->sdl_count++;
		if (node->sdln_data_size == CHECK_DATA)
			list->sdl_data_size = CHECK_DATA;
		else
			list->sdl_data_size += node->sdln_data_size;
		list_insert_tail(&(list->sdl_list), (void *)node);
		return (list);
}


sflow_data_list_t *
sflow_data_list_read(sflow_data_list_t *list, sflow_data_list_node_t **node)
{
		assert(list->sdl_count > 0);
		*node = (sflow_data_list_node_t *)
		    list_remove_head(&(list->sdl_list));
		list->sdl_count--;
		if (list->sdl_data_size != CHECK_DATA)
			list->sdl_data_size -= (*(node))->sdln_data_size;
		return (list);
}
