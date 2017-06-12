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

#ifndef _SFLOW_PROTOCOL_BRANCH_H
#define	_SFLOW_PROTOCOL_BRANCH_H

/*
 * PROGRAMMING WARNING
 *
 * The order of the struct fields matches sflow specification
 * DO NOT RE-ORDER fields of any struct
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#include "sflow_data_list.h"
#include "sflow_source.h"

/*
 * function to flat a sflow data structure to
 * continuous chunk of raw data for sending
 */
typedef	uint32_t
sflow_structure_flat_fn(char **, void *);
typedef uint64_t hyper;

/* sflow data format */
typedef uint32_t sflow_datafmt_t;


#define	SFLOW_SET_DATAFORMAT(sdf, ent, fmt) {	\
	(sdf) |= ((ent) << 20);			\
	(sdf) |= ((fmt) & 0x000FFFFF);		\
}
#define	SFLOW_GET_DATA_ENT(sdf)	(sdf >> 20)
#define	SFLOW_GET_DATA_FMT(sdf)	(sdf & 0x000FFFFF)


/* sflow data source */
typedef uint32_t sflow_datasrc_t;

#define	SFLOW_SET_DATASOURCE(sds, typ, val) {	\
	(sds) |= ((typ) << 24);			\
	(sds) |= ((val) & 0x00FFFFFF);		\
}


/* sflow counter record */
typedef struct sflow_cr_s {
	sflow_datafmt_t	scr_dfmt;
	uint32_t	scr_len;
	void		*scr_data;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_cr_t;

/* sflow flow record */
typedef struct sflow_fr_s {
	sflow_datafmt_t	sfr_dfmt;
	uint32_t	sfr_len;
	void		*sfr_data;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_fr_t;

/* sflow counter sample data */
typedef struct sflow_cs_s {
	uint32_t	scs_seq_no;
	sflow_datasrc_t	scs_dsrc;
	uint32_t	scs_count;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_cs_t;

/* sflow flow sample data */
typedef struct sflow_fs_s {
	uint32_t	sfs_seq_no;
	sflow_datasrc_t	sfs_dsrc;
	uint32_t	sfs_rate;
	uint32_t 	sfs_pool;
	uint32_t	sfs_drops;
	uint32_t	input_if;
	uint32_t	output_if;
	uint32_t	sfs_count;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_fs_t;

/* sflow  sample  */
typedef struct sflow_sp_s {
	sflow_datafmt_t	ssp_dfmt;
	uint32_t	len;
	void		*data;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_sp_t;

/* sflow datagram */
typedef struct sflow_dg_s {
	uint32_t	sflow_ver;
	uint32_t	ip_ver;
	union {
	    char v4[4];
	    char v6[16];
	}		ip_addr;
	uint32_t	sub_agent_id;
	uint32_t	dg_seq;
	uint32_t	up_time;
	uint32_t	sample_num;
	/*
	 * the pouint32_ters to samples should be
	 * stored in the immediate memory space
	 * after associated sflow_dg_t structure
	 */
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_dg_t;

sflow_dg_t *
sflow_datagram_pack(sflow_data_list_t *sample_list);

sflow_fr_t *
sflow_flow_record_pack(void *, sflow_source_t *);

sflow_fs_t *
sflow_flow_sample_pack(sflow_data_list_t *, sflow_source_t *);

sflow_sp_t *
sflow_sample_pack(void *sample_data, int ent, int fmt);


sflow_cr_t *
sflow_counter_record_pack(void *counter_data, int ent, int fmt);

sflow_cs_t *
sflow_counter_sample_pack(sflow_data_list_t *record_list,
    sflow_source_t *src);

#define	DFT_INTERVAL_CT 10

#define	DFT_RATE_FS 200

#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_PROTOCOL_BRANCH_H */
