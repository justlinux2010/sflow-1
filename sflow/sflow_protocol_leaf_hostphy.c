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

#include <stdio.h>
#include <stdlib.h>
#include <libnetcfg.h>
#include <sys/systeminfo.h>
#include <libdladm.h>
#include <libdllink.h>
#include <dirent.h>
#include <procfs.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/swap.h>

#include "sflow_util.h"
#include "sflow_protocol_leaf_hostphy.h"


/* Physical or virtual host description */
/* counter_data; enterprise = 0; format	= 2000 */
uint32_t
sflow_cr_host_descr_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_host_descr_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_host_descr_t) - SFLOW_PSIZE);
}
sflow_cr_host_descr_t *
sflow_cr_host_descr_pack(sflow_source_t *source)
{
	sflow_cr_host_descr_t	*h =
	    calloc(1, sizeof (sflow_cr_host_descr_t));
	sysinfo(SI_HOSTNAME, h->hd_name, sizeof (h->hd_name));
	h->hd_name_len = SFLOW_HTONL(sizeof (h->hd_name));
	sysinfo(SI_RELEASE, h->hd_os_release, sizeof (h->hd_os_release));
	h->hd_os_release_len = SFLOW_HTONL(sizeof (h->hd_os_release));
	sysinfo(SI_HW_SERIAL, h->hd_uuid, sizeof (h->hd_uuid));
	h->hd_os_name = SFLOW_HTONL(OS_SOLARIS);

	h->hd_machine_type = SFLOW_HTONL(get_host_phys_machine_type());
	h->flat = &sflow_cr_host_descr_flat;
	return (h);
}

/* call back funcs */
static int
sflow_gather_adapter(dladm_handle_t h, dladm_datalink_info_t *di, void *arg)
{
	int err;
	char    *valptr[1];
	sflow_cr_host_adapter_t		*ad;
	sflow_cr_host_adapters_t	**a
	    = (sflow_cr_host_adapters_t **)arg;
	char	buff[128];
	uint_t	valcnt = 1;

	/* for now get one mac address */
	valptr[0] = buff;
	err = dladm_get_linkprop(h, di->di_linkid, DLADM_PROP_VAL_CURRENT,
	    "mac-address", (char **)valptr, &valcnt);
	lg("call back %d, %s %s\n", di->di_linkid, di->di_linkname, buff);

	(*a)->has_len = sizeof ((*a)->has_count) + ((*a)->has_count + 1) *
	    (sizeof (sflow_cr_host_adapter_t) +
	    sizeof (sflow_cr_host_adapter_mac_t));
	(*a) = realloc((*a), (*a)->has_len + sizeof (sflow_cr_host_adapters_t) -
	    sizeof ((*a)->has_count));
	lg("has len %d, strcu len %d\n", (*a)->has_len,
	    (*a)->has_len + sizeof (sflow_cr_host_adapters_t)
	    - sizeof ((*a)->has_count));

	ad = (sflow_cr_host_adapter_t *)
	    ((char *)((*a) + 1) + ((sizeof (sflow_cr_host_adapter_t) +
	    sizeof (sflow_cr_host_adapter_mac_t))* ((*a)->has_count++)));
	ad->ha_ifIndex = SFLOW_HTONL(di->di_linkid);
	ad->ha_mac_count = SFLOW_HTONL(valcnt);

	mac_convert_str_to_char((char *)(ad + 1), buff);
	return (DLADM_WALK_CONTINUE);
}

static int
sflow_gather_link_total(dladm_handle_t h, dladm_datalink_info_t *di, void *arg)
{
	sflow_cr_host_net_io_t *n =
	    (sflow_cr_host_net_io_t *)arg;
	n->hnio_bytes_in	+=
	    sflow_get_link_stat_64(di->di_linkname, "rbytes64");
	n->hnio_bytes_out	+=
	    sflow_get_link_stat_64(di->di_linkname, "obytes64");
	n->hnio_errs_in		+=
	    sflow_get_link_stat_32(di->di_linkname, "ierrors");
	n->hnio_errs_out	+=
	    sflow_get_link_stat_32(di->di_linkname, "oerrors");
	n->hnio_pkts_in		+=
	    sflow_get_link_stat_32(di->di_linkname, "ipackets");
	n->hnio_errs_out	+=
	    sflow_get_link_stat_32(di->di_linkname, "opackets");
	lg("%s (%u, %u)\n", di->di_linkname, n->hnio_errs_in, n->hnio_pkts_in);
		/* missing two drops counts */
	return (DLADM_WALK_CONTINUE);
}

static int
sflow_walk_links(int linkid, void *a, dladm_walk_datalink_cb_t *cb)
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
	    0, NULL);

	lg("status %d\n", status);
	dladm_close(handle);
	if (status == DLADM_STATUS_OK)
		return (0);

	return (status);
}

/* counter_data; enterprise= 0; format= 2001 */
uint32_t
sflow_cr_host_adapters_flat(char **buf, void *cr)
{
	int i;
	sflow_cr_host_adapters_t *ads;
	sflow_cr_host_adapter_t *ad;
	ads = (sflow_cr_host_adapters_t *)cr;

	memcpy(*buf, (char *)(ads + 1) - sizeof (ads->has_count), ads->has_len);
	lg("get if number %d\n", ads->has_count);
	for (i = 0; i < SFLOW_NTOHL(ads->has_count); i++) {
		ad = ((sflow_cr_host_adapter_t *)
		    ((char *)(ads+1) +
		    (sizeof (sflow_cr_host_adapter_mac_t) +
		    sizeof (sflow_cr_host_adapter_t)) * i));
		lg("index %d, mac count %d", ad->ha_ifIndex, ad->ha_mac_count);
	}

	free(cr);
	return (ads->has_len);
}

sflow_cr_host_adapters_t *
sflow_cr_host_adapters_pack(sflow_source_t *source)
{
	int i, ii;

	sflow_cr_host_adapters_t	*a =
	    calloc(1, sizeof (sflow_cr_host_adapters_t));
	a->has_count = 0;
	a->has_len = sizeof (uint32_t);
	sflow_walk_links(DATALINK_ALL_LINKID, &a, &sflow_gather_adapter);
	lg("count is %d\n", a->has_count);
	a->has_count = SFLOW_HTONL(a->has_count);
	a->flat = &sflow_cr_host_adapters_flat;
	return (a);
}

#define	MAX_PROCFS_PATH	40
static int
count_process(uint32_t *total, uint32_t *runnning)
{
	DIR *procdir;
	dirent_t *direntp;
	char *pidstr;
	pid_t pid;
	psinfo_t	psinfo;

	int fd;
	char procfile[MAX_PROCFS_PATH];

	*total = 0;
	if ((procdir = opendir("/proc")) == NULL) {
		lg("can not open /proc");
		return (-1);
	}
	for (rewinddir(procdir); (direntp = readdir(procdir)); ) {
		pidstr = direntp->d_name;
		if (pidstr[0] == '.')	/* skip "." and ".."  */
			continue;
		pid = atoi(pidstr);
		if (pid == 0 || pid == 2 || pid == 3)
			continue;	/* skip sched, pageout and fsflush */

		(void) snprintf(procfile, MAX_PROCFS_PATH,
		    "/proc/%s/psinfo", pidstr);
		if ((fd = open(procfile, O_RDONLY)) == NULL)
			return (1);
		if (pread(fd, &psinfo, sizeof (psinfo_t), 0)
		    != sizeof (psinfo_t)) {
			close(fd);
			return (1);
		}
		(*total)++;
		if (psinfo.pr_lwp.pr_sname == 'R' ||
		    psinfo.pr_lwp.pr_sname == 'O') /* prutil.c */
			(*runnning)++;
		close(fd);
	}
	closedir(procdir);

	return (0);
}

static int
walk_cpu_instances_for_cpu_counters(sflow_cr_host_cpu_t *h)
{
	int status;
	int i;
	char name_buf[16];
	char buf_big[128];
	long buf_long;
	uint64_t buf_64;
	uint32_t buf_32;

	status = sflow_query_kstat("unix", 0, "system_misc",
	    "ncpus", KSTAT_DATA_UINT32, &(h->hc_cpu_num));
	lg("we have %d cpus\n", h->hc_cpu_num);

	/* get AVERAGE OF ALL CPUS */
	for (i = 0; i < h->hc_cpu_num; i++) {
		/* cpu speed */
		memset(name_buf, 0, sizeof (name_buf));
		strncpy(name_buf, "cpu_info", sizeof ("cpu_info"));
		itoa(i, name_buf + strlen(name_buf));

		lg("size of %s %d\n", name_buf, sizeof (name_buf));

		status = sflow_query_kstat("cpu_info", i, name_buf,
	    "supported_frequencies_Hz", KSTAT_DATA_STRING, &(buf_big));
		buf_long = atol(buf_big);
		lg("%d speed %s, %lu\n", status,  buf_big, buf_long);
		h->hc_cpu_speed += (uint32_t)(buf_long/h->hc_cpu_num);


		/* user time */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "sys",
	    "cpu_nsec_user", KSTAT_DATA_UINT64, &(buf_64));
		lg("cpu user time%llu \n", buf_64);
		h->hc_cpu_user += (uint32_t)(buf_64/1000000000/h->hc_cpu_num);

		/* idele time */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "sys",
	    "cpu_nsec_idle", KSTAT_DATA_UINT64, &(buf_64));
		lg("cpu idel time%llu \n", buf_64);
		h->hc_cpu_idle += (uint32_t)(buf_64/1000000000/h->hc_cpu_num);

		/* sys time */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "sys",
	    "cpu_nsec_kernel", KSTAT_DATA_UINT64, &(buf_64));
		lg("cpu kernel time%llu \n", buf_64);
		h->hc_cpu_system += (uint32_t)(buf_64/1000000000/h->hc_cpu_num);

		/* intr time */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "sys",
	    "cpu_nsec_intr", KSTAT_DATA_UINT64, &(buf_64));
		lg("cpu intr time%llu \n", buf_64);
		h->hc_cpu_intr += (uint32_t)(buf_64/1000000000/h->hc_cpu_num);

		/* iowait time */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "sys",
	    "iowait", KSTAT_DATA_UINT64, &(buf_64));
		lg("cpu iowait time%llu \n", buf_64);
		h->hc_cpu_wio += (uint32_t)(buf_64/1000000000/h->hc_cpu_num);

		/* interupt count */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "sys",
	    "intr", KSTAT_DATA_UINT64, &(buf_64));
		lg("intr count%llu \n", buf_64);
		h->hc_interrupts += (uint32_t)(buf_64/h->hc_cpu_num);
	}
	return (0);
}

/* hierarchy info  */
/* counter_data; enterprise = 0; format	= 2002 */
uint32_t
sflow_cr_host_parent_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_host_parent_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_host_parent_t) - SFLOW_PSIZE);
}
sflow_cr_host_parent_t *
sflow_cr_host_parent_pack(sflow_source_t *source)
{
	sflow_cr_host_parent_t *p =
	    calloc(1, sizeof (sflow_cr_host_parent_t));
	p->flat = &sflow_cr_host_parent_flat;
	return (p);
}


/* Physical or virtual host description */
/* counter_data; enterprise = 0; format	= 2003 */
uint32_t
sflow_cr_host_cpu_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_host_cpu_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_host_cpu_t) - SFLOW_PSIZE);
}
sflow_cr_host_cpu_t *
sflow_cr_host_cpu_pack(sflow_source_t *source)
{
	sflow_cr_host_cpu_t	*h =
	    calloc(1, sizeof (sflow_cr_host_cpu_t));
	double		loadavg[3];
	float		loadavgf[3];

	/* get load info */
	(void) getloadavg(loadavg, 3);
	loadavgf[0] = (float)loadavg[0];
	loadavgf[1] = (float)loadavg[1];
	loadavgf[2] = (float)loadavg[2];
	lg("load average: %.2f, %.2f, %.2f\n",
	    loadavgf[0], loadavgf[1], loadavgf[2]);
	memcpy(&h->hc_load_one_float,		&loadavgf[0], sizeof (float));
	memcpy(&h->hc_load_five_float,		&loadavgf[1], sizeof (float));
	memcpy(&h->hc_load_fifteen_float,	&loadavgf[2], sizeof (float));
	h->hc_load_one_float =		SFLOW_HTONL(h->hc_load_one_float);
	h->hc_load_five_float =		SFLOW_HTONL(h->hc_load_five_float);
	h->hc_load_fifteen_float =	SFLOW_HTONL(h->hc_load_fifteen_float);

	/* count process */
	count_process(&h->hc_proc_total, &h->hc_proc_run);
	lg("prcess count is %d, running %d\n", h->hc_proc_total,
	    h->hc_proc_run);
	h->hc_proc_run		=	SFLOW_HTONL(h->hc_proc_run);
	h->hc_proc_total	=	SFLOW_HTONL(h->hc_proc_total);

	/* get up time */
	h->hc_uptime = get_up_time_sec_safe();
	lg("get time %u", h->hc_uptime);
	h->hc_uptime = SFLOW_HTONL(h->hc_uptime);

	/* walk cpu */
	walk_cpu_instances_for_cpu_counters(h);
	h->hc_cpu_num	= SFLOW_HTONL(h->hc_cpu_num);
	h->hc_cpu_speed	= SFLOW_HTONL(h->hc_cpu_speed);
	h->hc_cpu_user	= SFLOW_HTONL(h->hc_cpu_user);
	h->hc_cpu_system	= SFLOW_HTONL(h->hc_cpu_system);
	h->hc_cpu_idle	= SFLOW_HTONL(h->hc_cpu_idle);
	h->hc_cpu_intr	= SFLOW_HTONL(h->hc_cpu_intr);
	h->hc_cpu_wio	= SFLOW_HTONL(h->hc_cpu_wio);
	h->hc_interrupts	= SFLOW_HTONL(h->hc_interrupts);
	h->flat = &sflow_cr_host_cpu_flat;
	return (h);
}

static int
walk_cpu_instances_for_mem_counters(sflow_cr_host_memory_t *h)
{
	int		i;
	int		status;
	uint32_t	cpu_num;
	uint64_t	buf_64;
	uint32_t	buf_32;

	status = sflow_query_kstat("unix", 0, "system_misc",
	    "ncpus", KSTAT_DATA_UINT32, &(cpu_num));
	lg("we have %d cpus\n", cpu_num);

	/* get AVERAGE OF ALL CPUS */
	for (i = 0; i < cpu_num; i++) {
		/* page in count */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "vm",
	    "pgpgin", KSTAT_DATA_UINT64, &(buf_64));
		lg("pgpgin count%llu \n", buf_64);
		h->hm_page_in += (uint32_t)(buf_64/cpu_num);

		/* page out count */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "vm",
	    "pgout", KSTAT_DATA_UINT64, &(buf_64));
		lg("pgout count%llu \n", buf_64);
		h->hm_page_out += (uint32_t)(buf_64/cpu_num);

		/* swap in */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "vm",
	    "swapin", KSTAT_DATA_UINT64, &(buf_64));
		lg("swapin count%llu \n", buf_64);
		h->hm_swap_in += (uint32_t)(buf_64/cpu_num);

		/* swap out */
		buf_64 = 0;
		status = sflow_query_kstat("cpu", i, "vm",
	    "swapout", KSTAT_DATA_UINT64, &(buf_64));
		lg("swapout count%llu \n", buf_64);
		h->hm_swap_out += (uint32_t)(buf_64/cpu_num);
	}
	return (0);
}

static int
walk_swap_instances_for_mem_counters(sflow_cr_host_memory_t *m)
{
	struct swaptable 	*st;
	struct swapent	*swapent;
	int	i;
	char		*path, *path_for_free;
	int		num;

	if ((num = swapctl(SC_GETNSWP, NULL)) == -1) {
		return (2);
	}
	if (num == 0) {
		lg("No swap devices configured\n");
		return (1);
	}

	if ((st = malloc(num * sizeof (swapent_t) + sizeof (int)))
	    == NULL) {
		lg("Malloc failed. Please try later.\n");
		return (2);
	}
	if ((path = malloc(num * MAXPATHLEN)) == NULL) {
		lg("Malloc failed. Please try later.\n");
		free(st);
		return (2);
	}
	path_for_free = path;
	swapent = st->swt_ent;
	for (i = 0; i < num; i++, swapent++) {
		swapent->ste_path = path;
		path += MAXPATHLEN;
	}

	st->swt_n = num;
	if ((num = swapctl(SC_LIST, st)) == -1) {
		free(st);
		free(path_for_free);
		return (2);
	}
	swapent = st->swt_ent;
	for (i = 0; i < num; i++, swapent++) {
		int diskblks_per_page =
		    (int)(sysconf(_SC_PAGESIZE) >> DEV_BSHIFT);
		m->hm_swap_total	+=
		    swapent->ste_pages * diskblks_per_page * DEV_BSIZE;
		m->hm_swap_free		+=
		    swapent->ste_free * diskblks_per_page * DEV_BSIZE;
	}
	lg("swapping pages in total %llu\n", m->hm_swap_total);
	lg("swapping pages free %llu\n", m->hm_swap_total);
	free(st);
	free(path_for_free);
	return (0);
}


/* counter_data; enterprise = 0; format	= 2004 */
uint32_t
sflow_cr_host_memory_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_host_memory_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_host_memory_t) - SFLOW_PSIZE);
}
sflow_cr_host_memory_t *
sflow_cr_host_memory_pack(sflow_source_t *source)
{
	int		status;
	uint32_t	buf_32;
	uint64_t	buf_64;
	sflow_cr_host_memory_t	*m =
	    calloc(1, sizeof (sflow_cr_host_memory_t));

	/* total mem */
	m->hm_total = get_host_phys_mem_bytes();
	/* free mem */
	buf_32 = 0;
	status = sflow_query_kstat("unix", 0, "system_pages",
	    "pagesfree", KSTAT_DATA_UINT32, &(buf_32));
	lg("pages free count%u \n", buf_32);
	m->hm_free = (hyper)buf_32 * (hyper)get_host_phys_page_size();

	walk_cpu_instances_for_mem_counters(m);
	walk_swap_instances_for_mem_counters(m);

	/* htonl */
	m->hm_total		= htonH(m->hm_total);
	m->hm_free		= htonH(m->hm_free);
	m->hm_swap_free		= htonH(m->hm_swap_free);
	m->hm_swap_total	= htonH(m->hm_swap_total);
	m->hm_swap_in	= SFLOW_HTONL(m->hm_swap_in);
	m->hm_swap_out	= SFLOW_HTONL(m->hm_swap_out);
	m->hm_page_in	= SFLOW_HTONL(m->hm_page_in);
	m->hm_page_out	= SFLOW_HTONL(m->hm_page_out);

	m->flat = &sflow_cr_host_memory_flat;
	return (m);
}

/* counter_data; enterprise = 0; format	= 2005 */
uint32_t
sflow_cr_host_disk_io_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_host_disk_io_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_host_disk_io_t) - SFLOW_PSIZE);
}

sflow_cr_host_disk_io_t *
sflow_cr_host_disk_io_pack(sflow_source_t *source)
{
	sflow_cr_host_disk_io_t	*d =
	    calloc(1, sizeof (sflow_cr_host_disk_io_t));
	randomize(d, sizeof (sflow_cr_host_disk_io_t));
	d->hd_write_time = SFLOW_HTONL(0);
	d->flat = &sflow_cr_host_disk_io_flat;
	return (d);
}

/* counter_data; enterprise = 0; format	= 2006 */
uint32_t
sflow_cr_host_net_io_flat(char **buf, void *cr)
{
	memcpy(*buf, cr, sizeof (sflow_cr_host_net_io_t) - SFLOW_PSIZE);
	free(cr);
	return (sizeof (sflow_cr_host_net_io_t) - SFLOW_PSIZE);
}

sflow_cr_host_net_io_t *
sflow_cr_host_net_io_pack(sflow_source_t *source)
{
	sflow_cr_host_net_io_t	*i =
	    calloc(1, sizeof (sflow_cr_host_net_io_t));
	/* walk and add up stats */
	sflow_walk_links(DATALINK_ALL_LINKID, i, &sflow_gather_link_total);
	lg("%u (%u, %u)\n", i, i->hnio_errs_in, i->hnio_pkts_in);
	i->hnio_bytes_in	= htonH(i->hnio_bytes_in);
	i->hnio_bytes_out	= htonH(i->hnio_bytes_out);
	i->hnio_drops_in	= SFLOW_HTONL(i->hnio_drops_in);
	i->hnio_drops_out	= SFLOW_HTONL(i->hnio_drops_out);
	i->hnio_errs_in		= SFLOW_HTONL(i->hnio_errs_in);
	i->hnio_errs_out	= SFLOW_HTONL(i->hnio_errs_out);
	i->hnio_pkts_in		= SFLOW_HTONL(i->hnio_pkts_in);
	i->hnio_pkts_out	= SFLOW_HTONL(i->hnio_pkts_out);
	i->flat = &sflow_cr_host_net_io_flat;
	return (i);
}

/*
 * == == == == == == == == == = sflow_sample thread
 */
sflow_sp_t *
get_sample_hostphy(sflow_source_t *source)
{

	int			i;
	int			err;
	void		*buf;
	/* sflow encapsulation */
	sflow_cr_t	*cr;
	sflow_cs_t	*cs;
	sflow_sp_t	*sp;
	sflow_cr_host_descr_t			*h;
	sflow_cr_host_adapters_t		*a;
	sflow_cr_host_cpu_t				*hc;
	sflow_cr_host_memory_t			*hm;
	sflow_cr_host_disk_io_t			*hd;
	sflow_cr_host_net_io_t			*hn;
	sflow_data_list_node_t			*temp_node;
	sflow_data_list_t				*temp_list;

	/* for now we assume a generic counter */
	temp_list = sflow_data_list_create("dummy list");

	h = sflow_cr_host_descr_pack(source);
	cr = sflow_counter_record_pack((void*)h, 0, 2000);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	a = sflow_cr_host_adapters_pack(source);
	cr = sflow_counter_record_pack((void*)a, 0, 2001);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	hc = sflow_cr_host_cpu_pack(source);
	cr = sflow_counter_record_pack((void*)hc, 0, 2003);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	hm = sflow_cr_host_memory_pack(source);
	cr = sflow_counter_record_pack((void*)hm, 0, 2004);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	hd = sflow_cr_host_disk_io_pack(source);
	cr = sflow_counter_record_pack((void*)hd, 0, 2005);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	hn = sflow_cr_host_net_io_pack(source);
	cr = sflow_counter_record_pack((void*)hn, 0, 2006);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	sflow_data_list_append(temp_list, temp_node);

	cs = sflow_counter_sample_pack(temp_list, source);
	sp = sflow_sample_pack((void *)cs, 0, 2);
	return (sp);
}
