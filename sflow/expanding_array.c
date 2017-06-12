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

#include "expanding_array.h"
#include "stdlib.h"
#include <assert.h>

expanding_array_t *
expanding_array_create()
{
	expanding_array_t *arr = malloc(sizeof (expanding_array_t));

	arr->ea_capacity = INIT_SIZE;
	arr->ea_array = malloc(_PSIZE * arr->ea_capacity);
	memset(arr->ea_array, 0, arr->ea_capacity * _PSIZE);
	arr->get = &expanding_array_get;
	arr->set = &expanding_array_set;
	return (arr);
}

void
expanding_array_destroy(expanding_array_t *arr)
{
	free(arr->ea_array);
	free(arr);
}

expanding_array_t *
expanding_array_set(expanding_array_t *arr, int index, void *pdata)
{
	size_t old_cap = arr->ea_capacity;
	if (index >= arr->ea_capacity) {
		while (index >= arr->ea_capacity)
			arr->ea_capacity *= 2;
		arr->ea_array = realloc(arr->ea_array,
		    arr->ea_capacity * _PSIZE);
	}
	memset((char *)(arr->ea_array) + old_cap * _PSIZE,
	    0, (arr->ea_capacity - old_cap)* _PSIZE);
	memcpy(arr->ea_array + index * _PSIZE, &pdata, _PSIZE);
	return (arr);
}

void *
expanding_array_get(expanding_array_t *arr, int index)
{
	assert(index < arr->ea_capacity);
	return (*((void **)(arr->ea_array + index * _PSIZE)));
}
