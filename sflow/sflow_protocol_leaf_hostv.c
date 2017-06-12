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
 * Progammer Notice
 * for the sake of reducing redundant code, code of virtual hosts has
 * dependency over code for physical hosts.
 */

#include <kstat.h>
#include <libdladm.h>
#include <libnetcfg.h>
#include <libdllink.h>
#include <zonestat.h>
#include <errno.h>
#include <zonestat_impl.h>
#include <sys/buf.h>
#include <time.h>
#include <pthread.h>

#include "sflow_util.h"
#include "sflow_protocol_leaf_hostphy.h" /* dependency */
#include "sflow_protocol_leaf_hostv.h"

/*
 * zone stat uitilities:
 * provide API for getting one stat for one zone.
 */
#define	CACHE_OK 3 /* allow 3 seconds cache */
zs_zone_t *g_zone_list = NULL;
pthread_mutex_t	sflow_zlock = PTHREAD_MUTEX_INITIALIZER;
time_t time_stamp; /* of the zon list */
int g_zone_num = 0;
static zs_usage_t usage = NULL;

static int
sflow_update_zone_stat(boolean_t force) {
	zs_ctl_t g_zsctl;
	time_t now;
	double td;
	int num;

	/* if not a force update, return if the cache is ok */
	if (g_zone_list != NULL && force == B_FALSE) {
		time(&now);
		td = difftime(now, time_stamp);
		lg("\n\nlast time fecthing usage was %u", (int)td);
		if ((int)td < CACHE_OK) {
			lg("\ncache ok, return ");
			return (0);
		}
	}
	lg("\nfecthing zone list\n");
	/* Open zone statistics */
	g_zsctl = zs_open();
	if (g_zsctl == NULL) {
		if (errno == EPERM)
			return (-1);
		if (errno == EINTR || errno == ESRCH) {
			return (-1);
		}
		if (errno == ENOTSUP)
			return (-1);
		return (-2);
	}

	if (usage != NULL)
		zs_usage_free(usage);

	usage = zs_usage_read(g_zsctl);

	if (usage == NULL) {
		if (g_zsctl != NULL)
			zs_close(g_zsctl);
		return (-3);
	}

	while (1) {
		num = zs_zone_list(usage, g_zone_list, g_zone_num);
		if (num > g_zone_num) {
			if (g_zone_list != NULL)
				free(g_zone_list);
			g_zone_list = (zs_zone_t *)
			    malloc(sizeof (zs_zone_t) * num);
			g_zone_num = num;
			}
		else
			break;
	}

	time(&time_stamp);

	if (g_zsctl != NULL)
		zs_close(g_zsctl);
	return (0);
}



static int
sflow_update_zone_stat_safe(boolean_t force) {
	int err;
	pthread_mutex_lock(&sflow_zlock);
	err = (sflow_update_zone_stat(force));
	pthread_mutex_unlock(&sflow_zlock);
	return (err);
}


static int
sflow_get_zone_stat(zoneid_t zid, const char *zname, zs_resource_t stat,
    void *buf, size_t sz) {
	zs_zone_t z;

	int num, i;
	zs_property_t prop;
	uint64_t buf_64;
	uint_t buf_uint;

	/* make sure the cache is good enough */
	lg("\n LOOKING AT zone id %d\n", zid);
		/* get the zone list */
	lg("\n%d\n", sflow_update_zone_stat_safe(B_FALSE));
	lg("hello1");

	/* now get the stat for one zone */
	lg("search for name %s", zname);
	pthread_mutex_lock(&sflow_zlock);
	z = NULL;
	for (i = 0; i < g_zone_num; i++) {
		z = g_zone_list[i];
		assert(z != NULL);
		prop = zs_zone_property(z, ZS_ZONE_PROP_NAME);
		if (strncmp(zs_property_string(prop), zname,
		    strlen(zs_property_string(prop))) == 0) {
			lg("find match!\n");
			break;
		}
		z = NULL;
	}
	pthread_mutex_unlock(&sflow_zlock);
	/* now z should point to the zone we need */
	if (z == NULL) {
		/* likely cache is old */
		sflow_update_zone_stat_safe(B_TRUE);
		/* force update so next time will give the right value */
		return (-1);
	}
	/* add entries here! */
	switch (stat) {
	case ZS_RESOURCE_RAM_RSS:
		buf_64 = zs_resource_used_zone_uint64(z, ZS_RESOURCE_RAM_RSS);
		memset(buf, 0, sz);
		memcpy(buf, &buf_64, sz);
		break;
	case ZS_RESOURCE_CPU:
		buf_uint = zs_resource_used_zone_pct(z, ZS_RESOURCE_CPU);
		memset(buf, 0, sz);
		memcpy(buf, &buf_uint, sz);
		break;
	default:
		return (-1);
		break;
	}

	return (0);
}

/* Virtual Node Statistics */
/* opaque= counter_data; enterprise= 0; format= 2100 */
uint32_t
sflow_cr_virt_leaf_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_virt_leaf_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_virt_leaf_t) - SFLOW_PSIZE);
}
sflow_cr_virt_leaf_t *
sflow_cr_virt_node_pack(sflow_source_t *source)
{
	sflow_cr_virt_leaf_t	*v = calloc(1, sizeof (sflow_cr_virt_leaf_t));
	sflow_cr_host_cpu_t		*c = sflow_cr_host_cpu_pack(source);
	sflow_cr_host_memory_t	*m = sflow_cr_host_memory_pack(source);

	/* get cpu info */
	v->vn_cpus	= SFLOW_NTOHL(c->hc_cpu_num);
	v->vn_mhz	= SFLOW_NTOHL(c->hc_cpu_speed) / 1000000;
	free(c);
	/* get mem info */
	v->vn_memory	= m->hm_total;
	v->vn_memory_free	= m->hm_free;
	free(m);

	/* count the number of zones */
	zone_list(NULL, &v->vn_num_domains);

	/* coverting */
	v->vn_cpus	= SFLOW_HTONL(v->vn_cpus);
	v->vn_mhz	= SFLOW_HTONL(v->vn_mhz);
	v->vn_num_domains--; /* no global zone ! */
	v->vn_num_domains	= SFLOW_HTONL(v->vn_num_domains);
	/*
	 * already is
	 * v->vn_memory	= SFLOW_HTONL(v->vn_memory);
	 * v->vn_memory_free	= SFLOW_HTONL(v->vn_memory_free);
	 */
	v->flat = &sflow_cr_virt_leaf_flat;
	return (v);
}



/* Virtual Domain CPU statistics */
/* See libvirt, struct virtDomainInfo */
/* opaque= counter_data; enterprise= 0; format= 2101 */
uint32_t
sflow_cr_virt_cpu_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_virt_cpu_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_virt_cpu_t) - SFLOW_PSIZE);
}
sflow_cr_virt_cpu_t *
sflow_cr_virt_cpu_pack(sflow_source_t *source)
{
	sflow_cr_virt_cpu_t *p =
	    calloc(1, sizeof (sflow_cr_virt_cpu_t));
	char buf[ZONE_VCPUSTR_MAX];
	uint32_t buf_32;
	uint_t buf_int;

	/* cpu state */
	if (zone_getattr(((ssd_hostv_t *)
	    (source->src_specific_data))->zone_id, ZONE_ATTR_STATUS,
	    &(p->vc_state), sizeof (p->vc_state)) == -1) {
			return (NULL);
		}
	lg("cpu state of %s", source->ss_name);
	lg(" is %d\n", p->vc_state);

	/* the number of cpus */
	if (zone_getattr(((ssd_hostv_t *)
	    (source->src_specific_data))->zone_id, ZONE_ATTR_VCPUS,
	    &(buf), ZONE_VCPUSTR_MAX) == -1) {
		return (NULL);
		}
	lg("cpu number of %s", source->ss_name);
	lg(" is [%s]\n", buf);

	/* calculate the cpu time */
	/* how I calculate : cpu time = usage percent * uptime */

	/* get up time */
	buf_32 = get_up_time_sec_safe();
	/* get the usage percentage */
	sflow_get_zone_stat(((ssd_hostv_t *)
	    (source->src_specific_data))->zone_id,
	    source->ss_name, ZS_RESOURCE_CPU,
	    &(buf_int), sizeof (buf_int));

	p->vc_cpuTime = buf_32 * buf_int/10000;
	/* 10000 equals 100.00 percent. */
	lg("thetime %u, %u\n", buf_32, p->vc_cpuTime);

	/* convert */
	p->vc_cpuTime = SFLOW_NTOHL(p->vc_cpuTime);
	p->vc_state = SFLOW_NTOHL(p->vc_state);
	p->vc_nrVirtCpu = SFLOW_NTOHL(p->vc_nrVirtCpu);
	p->flat = &sflow_cr_virt_cpu_flat;
	return (p);
}


/* Virtual Domain Memory statistics */
/* See libvirt, struct virtDomainInfo */
/* opaque= counter_data; enterprise= 0; format= 2102 */
uint32_t
sflow_cr_virt_memory_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_virt_memory_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_virt_memory_t) - SFLOW_PSIZE);
}
sflow_cr_virt_memory_t *
sflow_cr_virt_memory_pack(sflow_source_t *source)
{
	sflow_cr_virt_memory_t *p =
	    calloc(1, sizeof (sflow_cr_virt_memory_t));

	/* allowed memory  */
	if (zone_getattr(((ssd_hostv_t *)
	    (source->src_specific_data))->zone_id, ZONE_ATTR_PHYS_MCAP,
	    &(p->vm_maxMemory), sizeof (p->vm_maxMemory)) == -1) {
		return (NULL);
		}
	if (p->vm_maxMemory == 0) /* no cap */
		p->vm_maxMemory = get_host_phys_mem_bytes();
	lg("allowed memory  of %s", source->ss_name);
	lg(" is [%llu]\n", p->vm_maxMemory);

	sflow_get_zone_stat(((ssd_hostv_t *)
	    (source->src_specific_data))->zone_id,
	    source->ss_name, ZS_RESOURCE_RAM_RSS,
	    &(p->vm_memory), sizeof (p->vm_memory));

	lg("useded memory  of %s", source->ss_name);
	lg(" is [%llu]\n", p->vm_memory);

	p->vm_memory = htonH(p->vm_memory);
	p->vm_maxMemory = htonH(p->vm_maxMemory);
	p->flat = &sflow_cr_virt_memory_flat;
	return (p);
}



static int
gather_zone_link_total(dladm_handle_t h, dladm_datalink_info_t *di, void *arg)
{
	sflow_cr_virt_net_io_t *n =
	    (sflow_cr_virt_net_io_t *)arg;

	n->vnio_rx_bytes	+=
	    sflow_get_link_stat_64(di->di_linkname, "rbytes64");
	n->vnio_tx_bytes	+=
	    sflow_get_link_stat_64(di->di_linkname, "obytes64");
	n->vnio_rx_errs		+=
	    sflow_get_link_stat_32(di->di_linkname, "ierrors");
	n->vnio_tx_errs	+=
	    sflow_get_link_stat_32(di->di_linkname, "oerrors");
	n->vnio_rx_packets		+=
	    sflow_get_link_stat_32(di->di_linkname, "ipackets");
	n->vnio_tx_packets	+=
	    sflow_get_link_stat_32(di->di_linkname, "opackets");
	lg("zone if %s \n", di->di_linkname);
	return (DLADM_WALK_CONTINUE);
}

static int
walk_one_zone_links(int linkid, void *a, dladm_walk_datalink_cb_t *cb,
    zoneid_t zid)
{
	int status;
	dladm_handle_t		handle;

	if ((status = dladm_open(&handle,
	    NETADM_ACTIVE_PROFILE)) != DLADM_STATUS_OK) {
			lg("could not open /dev/dld");
			return (-1);
	}
	lg("walk\n");
	status = dladm_walk_datalinks(handle, linkid, cb,
	    a, DATALINK_CLASS_ALL, DATALINK_ANY_MEDIATYPE,
	    DLADM_OPT_ACTIVE | DLADM_OPT_PERSIST | DLADM_OPT_ALLZONES,
	    1, &zid);

	lg("status %d\n", status);
	dladm_close(handle);
	if (status == DLADM_STATUS_OK)
		return (0);

	return (status);
}


/* Virtual Domain Network statistics */
/* See libvirt, struct virtDomainInterfaceStatsStruct */
/* opaque = counter_data; enterprise = 0; format = 2104 */
uint32_t
sflow_cr_virt_net_io_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_virt_net_io_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_virt_net_io_t) - SFLOW_PSIZE);
}

sflow_cr_virt_net_io_t *
sflow_cr_virt_net_io_pack(sflow_source_t *source)
{
	sflow_cr_virt_net_io_t *p =
	    calloc(1, sizeof (sflow_cr_virt_net_io_t));
	walk_one_zone_links(DATALINK_ALL_LINKID, p, &gather_zone_link_total,
	    ((ssd_hostv_t *)source->src_specific_data)->zone_id);
	/* convert */
	p->vnio_rx_bytes	= htonH(p->vnio_rx_bytes);
	p->vnio_rx_errs	= SFLOW_HTONL(p->vnio_rx_errs);
	p->vnio_rx_packets	= SFLOW_HTONL(p->vnio_rx_packets);
	p->vnio_tx_bytes	= htonH(p->vnio_tx_bytes);
	p->vnio_tx_errs	= SFLOW_HTONL(p->vnio_tx_errs);
	p->vnio_tx_packets	= SFLOW_HTONL(p->vnio_tx_packets);
	p->flat = &sflow_cr_virt_net_io_flat;
	return (p);
}


/* ****** wrappers over phys host struct packs and flat ****** */


/* Physical or virtual host description */
/* counter_data; enterprise = 0; format	= 2000 */
uint32_t
sflow_cr_host_descr_flat_v(char **buf, void *cr)
{
	return (sflow_cr_host_descr_flat(buf, cr));
}
sflow_cr_host_descr_t *
sflow_cr_host_descr_pack_v(sflow_source_t *source)
{
	char tmp[64];
	sflow_cr_host_descr_t	*h =
	    sflow_cr_host_descr_pack(source);

	/* change name and uuid */
	/* h->hd_name_len = strlen(source->ss_name);  zone name */
	bzero(h->hd_name, sizeof (h->hd_name));
	strncpy(h->hd_name, source->ss_name, strlen(source->ss_name));

	/* compose uuid with zone name and id for now */
	memset(&tmp, 0, sizeof (tmp));
	strncpy(tmp, h->hd_name, strlen(source->ss_name));
	itoa(((ssd_hostv_t *)(source->src_specific_data))->zone_id,
	    h->hd_uuid +strlen(source->ss_name));

	h->flat = &sflow_cr_host_descr_flat_v;
	return (h);
}



/* vhost wrapper */
/* hierarchy info  */
/* counter_data; enterprise = 0; format	= 2002 */
uint32_t
sflow_cr_host_parent_flat_v(char **buf, void *cr)
{
	return (sflow_cr_host_parent_flat(buf, cr));
}
sflow_cr_host_parent_t *
sflow_cr_host_parent_pack_v(sflow_source_t *source)
{
	sflow_cr_host_parent_t *pt = sflow_cr_host_parent_pack(NULL);

	/* parent is the global zone */
	pt->hp_container_type = SFLOW_HTONL(2);
	pt->hp_container_index = SFLOW_HTONL(0);
	pt->flat = &sflow_cr_host_parent_flat_v;
	return (pt);
}


sflow_sp_t *
get_sample_hostv(sflow_source_t *source)
{
	sflow_cr_t	*cr;
	sflow_cs_t	*cs;
	sflow_sp_t	*sp;
	sflow_cr_host_descr_t			*vd;
	sflow_cr_virt_leaf_t			*vn;
	sflow_cr_virt_cpu_t				*vc;
	sflow_cr_virt_memory_t			*vm;
	sflow_cr_virt_net_io_t			*vt;
	sflow_cr_host_parent_t			*pt;
	sflow_data_list_node_t			*temp_node;
	sflow_data_list_t				*temp_list;

	/* for now we assume a generic counter */
	temp_list = sflow_data_list_create("dummy list");

	/* HOST info */
	vd = sflow_cr_host_descr_pack_v(source);
	cr = sflow_counter_record_pack((void*)vd, 0, 2000);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	/* container info */
	pt = sflow_cr_host_parent_pack_v(NULL);
	cr = sflow_counter_record_pack((void*)pt, 0, 2002);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	vn = sflow_cr_virt_node_pack(source);
	cr = sflow_counter_record_pack((void*)vn, 0, 2100);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	vc = sflow_cr_virt_cpu_pack(source);
	cr = sflow_counter_record_pack((void*)vc, 0, 2101);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	vm = sflow_cr_virt_memory_pack(source);
	cr = sflow_counter_record_pack((void*)vm, 0, 2102);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	vt = sflow_cr_virt_net_io_pack(source);
	cr = sflow_counter_record_pack((void*)vt, 0, 2104);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	cs = sflow_counter_sample_pack(temp_list, source);
	sp = sflow_sample_pack((void *)cs, 0, 2);

	return (sp);
}
