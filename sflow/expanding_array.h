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

/*
 * This is an array that expands automatically.
 * It should only stores pointers.
 * OOP implementation with function pointers.
 */


#ifndef _EXPANDING_ARRAY
#define	_EXPANDING_ARRAY
#define	_PSIZE		sizeof (char *)
#define	INIT_SIZE	8
typedef struct expanding_array {
	int	ea_capacity;
	char	*ea_array;
	void	*(*get)(struct expanding_array *, int);
	struct	expanding_array *(*set)
	    (struct expanding_array *, int, void *);
} expanding_array_t;

expanding_array_t *
expanding_array_create();

void
expanding_array_destroy(expanding_array_t *);

expanding_array_t *
expanding_array_set(expanding_array_t *, int, void *);

void *
expanding_array_get(expanding_array_t *, int);
#endif	/* _EXPANDING_ARRAY */
