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

#include "fcntl.h"
#include <door.h>
#include <errno.h>

#include "sflow_door.h"
#include "sflow_source.h"


static int sflowd_door_fd = -1;

void
sflowd_door_handler(void *cookie, char *data, size_t data_size,
    door_desc_t *descptr, uint_t ndesc)
{
	int err, i;
	sflowd_ssr_t		*slr;
	int					slr_size;
	sflowd_door_asrc_t	*asrc;
	sflowd_door_rsrc_t	*rsrc;
	sflowd_door_ssrc_t	*ssrc;
	switch (*(int *)data) {
		case SFLOWD_CMD_ADD_SOURCE:
			asrc = (sflowd_door_asrc_t *)data;
			err = sflow_add_source(asrc);
			door_return((char *)&err, sizeof (err), NULL, 0);
		break;
		case SFLOWD_CMD_REMOVE_SOURCE:
			rsrc = (sflowd_door_rsrc_t *)data;
			err = sflow_remove_source(asrc);
			door_return((char *)&err, sizeof (err), NULL, 0);
		break;
		case SFLOWD_CMD_SHOW_SOURCE:
			ssrc = (sflowd_door_ssrc_t *)data;

			err = sflow_show_src(ssrc, &slr, &slr_size);
			if (err !=  0) {
				door_return((char *)&err,
				    sizeof (err), NULL, 0);
			} else {
				door_return((char *)slr, slr_size, NULL, 0);
			}
		break;
		default:
			lg("command type not recongnized\n");
	}
}

int
sflow_open_door()
{
	int		fd;
	int		err = 0;

	/* create the door file for sflowd */
	if ((fd = open(SFLOW_DOOR, O_CREAT|O_RDONLY, 0644)) == -1) {
		err = errno;
		lg("could not open %s: %s\n",
		    SFLOW_DOOR, strerror(err));
		return (err);
	}
	(void) close(fd);

	if ((sflowd_door_fd = door_create(sflowd_door_handler, NULL,
	    DOOR_REFUSE_DESC | DOOR_NO_CANCEL)) == -1) {
		err = errno;
		lg("failed to create door: %s\n", strerror(err));
		return (err);
	}
	/*
	 * fdetach first in case a previous daemon instance exited
	 * ungracefully.
	 */
	(void) fdetach(SFLOW_DOOR);
	if (fattach(sflowd_door_fd, SFLOW_DOOR) != 0) {
		err = errno;
		lg("failed to attach door to %s: %s",
		    SFLOW_DOOR, strerror(err));
		(void) door_revoke(sflowd_door_fd);
		sflowd_door_fd = -1;
	}
	return (err);
}
