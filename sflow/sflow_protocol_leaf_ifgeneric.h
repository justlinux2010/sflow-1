
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

#ifndef _SFLOW_CR_IF_GENERIC_H
#define	_SFLOW_CR_IF_GENERIC_H
/*
 * PROGRAMMING WARNING
 *
 * The order of the struct fields matches sflow specification
 * DO NOT RE-ORDER fields of any struct
 */

#ifdef __cplusplus
extern "C" {
#endif


typedef struct sflow_cr_if_generic_s {
	uint32_t	ifIndex;
	uint32_t	ifType;
	hyper		ifSpeed;
	uint32_t	ifDirection;
	uint32_t	ifStatus;
	hyper		ifInOctets;
	uint32_t	ifInUcastPkts;
	uint32_t	ifInMulticastPkts;
	uint32_t	ifInBroadcastPkts;
	uint32_t	ifInDiscards;
	uint32_t	ifInErrors;
	uint32_t	ifInUnknownProtos;
	hyper		ifOutOctets;
	uint32_t	ifOutUcastPkts;
	uint32_t	ifOutMulticastPkts;
	uint32_t	ifOutBroadcastPkts;
	uint32_t	ifOutDiscards;
	uint32_t	ifOutErrors;
	uint32_t	ifPromiscuousMode;
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_cr_if_generic_t;

typedef struct source_specific_data_ifgeneric_s {
	int	ifg_datalink;
} ssd_ifgeneric_t;


sflow_sp_t *
get_sample_ifgeneric(sflow_source_t *source);



#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_CR_IF_GENERIC_H */
