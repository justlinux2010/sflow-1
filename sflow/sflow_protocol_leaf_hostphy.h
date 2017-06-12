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

#ifndef _SFLOW_HOST_H
#define	_SFLOW_HOST_H
/*
 * PROGRAMMING WARNING
 *
 * The order of the struct fields matches sflow specification
 * DO NOT RE-ORDER fields of any struct
 */

#include "sflow_protocol_branches.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct sflow_cr_host_s {
	sflow_structure_flat_fn *flat;
} __attribute__((packed)) sflow_cr_host_t;

typedef enum {
	MT_UNKNOWN	= 0,
	MT_OTHER	= 1,
	MT_X86		= 2,
	MT_X86_64	= 3,
	MT_IA64		= 4,
	MT_SPARC	= 5,
	MT_ALPHA	= 6,
	MT_POWERPC	= 7,
	MT_M68K		= 8,
	MT_MIPS		= 9,
	MT_ARM		= 10,
	MT_HPPA		= 11,
	MT_S360		= 12
} machine_type_t;

typedef enum {
	OS_UNKOWN	= 0,
	OS_OTHER	= 1,
	OS_LINUX	= 2,
	OS_WIN		= 3,
	OS_DARWIN	= 4,
	OS_HPUX		= 5,
	OS_AIX		= 6,
	OS_DRAGONFLY	= 7,
	OS_FREEBSD	= 8,
	OS_NETBSD	= 9,
	OS_OPENBSD	= 10,
	OS_OSF		= 11,
	OS_SOLARIS	= 12
} os_name_t;


/* Physical or virtual host description */
/* counter_data; enterprise = 0; format	= 2000 */
typedef struct {
	uint32_t	hd_name_len;
	char		hd_name[60];
	/* hostname, empty if unknown */
	char		hd_uuid[16];
	/* 16 bytes binary UUID, empty if unknown */
	machine_type_t	hd_machine_type;
	/* the processor family */
	os_name_t	hd_os_name;
	/* Operating system */
	uint32_t	hd_os_release_len;
	char		hd_os_release[28];
	/* e.g. 2.6.9-42.ELsmp,xp-sp3, empty if unknown */
	sflow_structure_flat_fn *flat;
} __attribute__((packed))  sflow_cr_host_descr_t;


/* Physical or virtual network adapter NIC/vNIC */
typedef struct {
	char	ha_mac[8];
	/* Adapter MAC address(es) */
} __attribute__((packed)) sflow_cr_host_adapter_mac_t;

typedef struct {
	uint32_t	ha_ifIndex;
	/*
	 * ifIndex associated with adapter
	 * Must match ifIndex of vSwitch
	 * port if vSwitch is exporting sFlow
	 * 0 = unknown
	 */
	uint32_t	ha_mac_count;
} __attribute__((packed))  sflow_cr_host_adapter_t;

/*
 * Set of adapters associated with entity.
 * A physical server will identify the physical network adapters
 * associated with it and a virtual server will identify its virtual
 * adapters.
 */
/* counter_data; enterprise= 0; format= 2001 */
typedef struct {
	sflow_structure_flat_fn *flat;
	uint32_t	has_len;
	/* the length of the counter record */
	uint32_t	has_count;
	/* adapter(s) associated with entity */
} __attribute__((packed))  sflow_cr_host_adapters_t;

/*
 * Define containment hierarchy between logical and physical
 * entities. Only a single, strict containment tree is permitted,
 * each entity must be contained within a single parent, but a parent
 * may contain more than one child. The host_parent record is used
 * by the child to identify its parent. Physical entities form the roots
 * of the tree and do not send host_parent structures.
 */

/* opaque = counter_data; enterprise = 0; format = 2002 */

typedef struct {
	uint32_t	hp_container_type;
	/* sFlowDataSource type */
	uint32_t	hp_container_index;
	/* sFlowDataSource index */
	sflow_structure_flat_fn *flat;
} __attribute__((packed))  sflow_cr_host_parent_t;


/*
 * Physical server performance metrics
 */

/* Physical Server CPU */
/* opaque= counter_data; enterprise= 0; format= 2003 */

typedef struct {
	uint32_t	hc_load_one_float;
	/* 1 minute load avg., -1.0= unknown */
	uint32_t	hc_load_five_float;
	/* 5 minute load avg., -1.0= unknown */
	uint32_t	hc_load_fifteen_float;
	/* 15 minute load avg., -1.0= unknown */
	uint32_t	hc_proc_run;
	/* total number of running processes */
	uint32_t	hc_proc_total;
	/* total number of processes */
	uint32_t	hc_cpu_num;
	/* number of CPUs */
	uint32_t	hc_cpu_speed;
	/* speed in MHz of CPU */
	uint32_t	hc_uptime;
	/* seconds since last reboot */
	uint32_t	hc_cpu_user;
	/* user time (ms) */
	uint32_t	hc_cpu_nice;
	/* nice time (ms) */
	uint32_t	hc_cpu_system;
	/* system time (ms) */
	uint32_t	hc_cpu_idle;
	/* idle time (ms) */
	uint32_t	hc_cpu_wio;
	/* time waiting for I/O to complete (ms) */
	uint32_t	hc_cpu_intr;
	/* time servicing interrupts (ms) */
	uint32_t	hc_cpu_sintr;
	/* time servicing soft interrupts (ms) */
	uint32_t	hc_interrupts;
	/* interrupt count */
	uint32_t	hc_contexts;
	/* context switch count */
	sflow_structure_flat_fn *flat;
} __attribute__((packed))  sflow_cr_host_cpu_t;

/* Physical Server Memory */
/* opaque= counter_data; enterprise= 0; format= 2004 */

typedef struct {
	hyper	hm_total;
	/* total bytes */
	hyper	hm_free;
	/* free bytes */
	hyper	hm_shared;
	/* shared bytes */
	hyper	hm_buffers;
	/* buffers bytes */
	hyper	hm_cached;
	/* cached bytes */
	hyper	hm_swap_total;
	/* swap total bytes */
	hyper	hm_swap_free;
	/* swap free bytes */
	uint32_t	hm_page_in;
	/* page in count */
	uint32_t	hm_page_out;
	/* page out count */
	uint32_t	hm_swap_in;
	/* swap in count */
	uint32_t	hm_swap_out;
	/* swap out count */
	sflow_structure_flat_fn *flat;
} __attribute__((packed))  sflow_cr_host_memory_t;

/* Physical Server Disk I/O */
/* opaque= counter_data; enterprise= 0; format= 2005 */

typedef struct {
	hyper	hd_total;
	/* total disk size in bytes */
	hyper	hd_free;
	/* total disk free in bytes */
	float	hd_part_max_used;
	/* utilization of most utilized partition */
	uint32_t	hd_reads;
	/* reads issued */
	hyper	hd_bytes_read;
	/* bytes read */
	uint32_t	hd_read_time;
	/* read time (ms) */
	uint32_t	hd_writes;
	/* writes completed */
	hyper	hd_bytes_written;
	/* bytes written */
	uint32_t	hd_write_time;
	/* write time (ms) */
	sflow_structure_flat_fn *flat;
}  __attribute__((packed)) sflow_cr_host_disk_io_t;

/* Physical Server Network I/O */
/* opaque= counter_data; enterprise= 0; format= 2006 */

typedef struct {
	hyper		hnio_bytes_in;
	uint32_t	hnio_pkts_in;
	/* total packets in */
	uint32_t	hnio_errs_in;
	/* total errors in */
	uint32_t	hnio_drops_in;
	/* total drops in */
	hyper		hnio_bytes_out;
	/* total bytes out */
	uint32_t	hnio_pkts_out;
	/* total packets out */
	uint32_t	hnio_errs_out;
	/* total errors out */
	uint32_t	hnio_drops_out;
	/* total drops out */
	sflow_structure_flat_fn *flat;
} __attribute__((packed))  sflow_cr_host_net_io_t;


/*
 * Extended socket information,
 * Must be filled in for all application transactions
 * associated with a network socket
 * Omit if transaction associated with non-network IPC
 */

/* IPv4 Socket */
/* opaque= flow_data; enterprise= 0; format= 2100 */
typedef struct {
	uint32_t	protocol;
	/* IP Protocol type (for example, TCP= 6, UDP= 17) */
	char	es4_local_ip[4];
	/* local IP address */
	char	es4_remote_ip[4];
	/* remote IP address */
	uint32_t	es4_local_port;
	/* TCP/UDP local port number or equivalent */
	uint32_t	es4_remote_port;
	/* TCP/UDP remote port number of equivalent */
} __attribute__((packed))  sflow_cr_extended_socket_ipv4_t;

/* IPv6 Socket */
/* opaque= flow_data; enterprise= 0; format= 2101 */
typedef struct {
	uint32_t	protocol;
	/* IP Protocol type (for example, TCP= 6, UDP= 17) */
	char	es6_local_ip[16];
	/* local IP address */
	char	es6_remote_ip[16];
	/* remote IP address */
	uint32_t	es6_local_port;
	/* TCP/UDP local port number or equivalent */
	uint32_t	es6_remote_port;
	/* TCP/UDP remote port number of equivalent */
} __attribute__((packed))  sflow_cr_extended_socket_ipv6_t;



sflow_sp_t *
get_sample_hostphy(sflow_source_t *source);

/* the following are exposed for vhost */
sflow_cr_host_cpu_t *
sflow_cr_host_cpu_pack(sflow_source_t *source);

sflow_cr_host_memory_t *
sflow_cr_host_memory_pack(sflow_source_t *source);

uint32_t
sflow_cr_host_parent_flat(char **buf, void *cr);
sflow_cr_host_parent_t *
sflow_cr_host_parent_pack(sflow_source_t *source);

uint32_t
sflow_cr_host_descr_flat(char **buf, void *cr);
sflow_cr_host_descr_t *
sflow_cr_host_descr_pack(sflow_source_t *source);

#ifdef __cplusplus
}
#endif

#endif /* _SFLOW_HOST_H */
