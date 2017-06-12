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

#ifndef _SFLOW_DOOR_DATA
#define	_SFLOW_DOOR_DATA

#define	MAX_LINK_NAMELEN	32
#define	SFADM_STRSIZE		1024
#define	SFADM_TYPESTRLEN	32
#define	SFADM_PROPSTRLEN	512
#define	SFLOW_DOOR		"/tmp/sflow_door"
#define	SFLOW_CMD_ALL		"all"

typedef enum {
	SFLOWD_CMD_ADD_SOURCE,
	SFLOWD_CMD_REMOVE_SOURCE,
	SFLOWD_CMD_SHOW_SOURCE
} sflowd_door_cmd_type_t;


typedef struct sflowd_door_asrc_s {
	int sd_cmd;
	char sd_srcname[MAX_LINK_NAMELEN];
	char sd_typestr[SFADM_TYPESTRLEN];
	char sd_propstr[SFADM_PROPSTRLEN];
} sflowd_door_asrc_t;

typedef struct sflowd_door_rsrc_s {
	int sd_cmd;
	char sd_srcname[MAX_LINK_NAMELEN];
	char sd_typestr[SFADM_TYPESTRLEN];
} sflowd_door_rsrc_t;

typedef struct sflowd_door_ssrc_s {
	int sd_cmd;
	char sd_srcname[MAX_LINK_NAMELEN];
	char sd_typestr[SFADM_TYPESTRLEN];
} sflowd_door_ssrc_t;


typedef struct sflowd_retval_s {
	uint_t	sr_err;	/* return error code from sflowd */
} sflowd_retval_t;

typedef struct sflow_src_fields_s {
	char	ssf_src_name[MAX_LINK_NAMELEN];
	char	ssf_type[MAX_LINK_NAMELEN];
	char	ssf_rate[MAX_LINK_NAMELEN];
	char	ssf_interval[MAX_LINK_NAMELEN];
}sflow_src_fields_t;


/* show link return struct */
typedef struct sflowd_show_src_ret_s {
	sflowd_retval_t	ssr_err;
	int		ssr_link_count;
} __attribute__((packed)) sflowd_ssr_t;

#endif /* _SFLOW_DOOR_DATA */
