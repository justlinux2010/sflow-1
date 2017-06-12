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

#ifndef _SFLOW_VHOST_H
#define	_SFLOW_VHOST_H

#ifdef __cplusplus
extern "C" {
#endif

/* a little dependency for container */
#include <sys/zone.h>

#include "sflow_protocol_leaf_hostphy.h"
#include "sflow_protocol_branches.h"

/* Hypervisor and virtual machine performance metrics */
/* Virtual Node Statistics */
/* See libvirt, struct virtNodeInfo */
/* opaque= counter_data; enterprise= 0; format= 2100 */
typedef struct {
	uint32_t	vn_mhz;
	/* expected CPU frequency */
	uint32_t	vn_cpus;
	/* the number of active CPUs */
	hyper	vn_memory;
	/* memory size in bytes */
	hyper	vn_memory_free;
	/* unassigned memory in bytes */
	uint32_t	vn_num_domains;
	/* number of active domains */
	sflow_structure_flat_fn *flat;
} sflow_cr_virt_leaf_t;


/* Virtual Domain CPU statistics */
/* See libvirt, struct virtDomainInfo */
/* opaque= counter_data; enterprise= 0; format= 2101 */
typedef struct {
	uint32_t	vc_state;
	/* virtDomainState */
	uint32_t	vc_cpuTime;
	/* the CPU time used (ms) */
	uint32_t	vc_nrVirtCpu;
	/* number of virtual CPUs for the domain */
	sflow_structure_flat_fn *flat;
} sflow_cr_virt_cpu_t;

/* Virtual Domain Memory statistics */
/* See libvirt, struct virtDomainInfo */
/* opaque= counter_data; enterprise= 0; format= 2102 */
typedef struct {
	hyper	vm_memory;
	/* memory in bytes used by domain */
	hyper	vm_maxMemory;
	/* memory in bytes allowed */
	sflow_structure_flat_fn *flat;
} sflow_cr_virt_memory_t;

/* Virtual Domain Disk statistics */
/* See libvirt, struct virtDomainBlockInfo */
/* See libvirt, struct virtDomainBlockStatsStruct */
/* opaque= counter_data; enterprise= 0; format= 2103 */
typedef struct {
	hyper	vdio_capacity;
	/* logical size in bytes */
	hyper	vdio_allocation;
	/* current allocation in bytes */
	hyper	vdio_available;
	/* remaining free bytes */
	uint32_t	vdio_rd_req;
	/* number of read requests */
	hyper	vdio_rd_bytes;
	/* number of read bytes */
	uint32_t	vdio_wr_req;
	/* number of write requests */
	hyper	vdio_wr_bytes;
	/* number of  written bytes */
	uint32_t	vdio_errs;
	/* read/write errors */
} sflow_cr_virt_disk_io_t;

/* Virtual Domain Network statistics */
/* See libvirt, struct virtDomainInterfaceStatsStruct */
/* opaque = counter_data; enterprise = 0; format = 2104 */
typedef struct {
	hyper	vnio_rx_bytes;
	/* total bytes received */
	uint32_t	vnio_rx_packets;
	/* total packets received */
	uint32_t	vnio_rx_errs;
	/* total receive errors */
	uint32_t	vnio_rx_drop;
	/* total receive drops */
	hyper	vnio_tx_bytes;
	/* total bytes transmitted */
	uint32_t	vnio_tx_packets;
	/* total packets transmitted */
	uint32_t	vnio_tx_errs;
	/* total transmit errors */
	uint32_t	vnio_tx_drop;
	/* total transmit drops */
	sflow_structure_flat_fn *flat;
} sflow_cr_virt_net_io_t;

typedef struct source_specific_data_hostv_s {
	zoneid_t zone_id;
} ssd_hostv_t;



sflow_sp_t *
get_sample_hostv(sflow_source_t *source);

#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_VHOST_H */
