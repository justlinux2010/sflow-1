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

#ifndef _SFLOW_FR_RAW_H
#define	_SFLOW_FR_RAW_H
#include "sflow_protocol_branches.h"

/*
 * PROGRAMMING WARNING
 *
 * The order of the struct fields matches sflow specification
 * DO NOT RE-ORDER fields of any struct
 */

#ifdef __cplusplus
extern "C" {
#endif


/* sflow record data  0 1 raw pkt header */
typedef struct sflow_fr_raw_pkt_header_s {
	uint32_t	fr_header_protocol;
	uint32_t	fr_frame_length;
	uint32_t	fr_stripped;
	uint32_t	fr_header_size;
	void		*fr_header;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_fr_raw_pkt_header_t;

typedef struct source_specific_data_flowsample_s {
	int		fsp_datalink;
	uint32_t	fsp_pool;
	uint32_t	fsp_drops;
} ssd_flowsample_t;

sflow_sp_t *
get_sample_flowsample(sflow_source_t *, char *, int);

int
fd_provider_flowsample(sflow_source_t *);



#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_FR_RAW_H */
