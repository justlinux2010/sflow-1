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

/* log redirection ------- */
/* #define	SFLOW_LOG */
/* #define	SFLOW_LOGFILE */

#include <kstat.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <libnetcfg.h>
#include <ifaddrs.h>
#include <time.h>
#include <sys/socket.h>
#include <libdladm.h>
#include <libdllink.h>
#include <libdlpi.h>
#include <utmpx.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>

#include "sflow_util.h"
#include "expanding_array.h"
#include "sflow_protocol_branches.h"


FILE			*sflow_log_file = NULL;
char			*sflow_log_file_path = "/tmp/sflow_logs.txt";

#define		SFLOW_FG
void
lg(const char *fmt, ...) {
	va_list		args;

	va_start(args, fmt);
#ifdef		SFLOW_LOG
	vsyslog(LOG_NOTICE, fmt, args);
#endif
#ifdef SFLOW_LOGFILE
	if (sflow_log_file != NULL) {
		vfprintf(sflow_log_file, fmt, args);
		fflush(sflow_log_file);
	}
#endif
#ifdef		SFLOW_FG
	vfprintf(stdout, fmt, args);
	fflush(stdout);
#endif
	va_end(args);
}

void
sflow_show_time()
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	lg("now: %d-%d-%d %d:%d:%d\n",
	    tm.tm_year + 1900, tm.tm_mon + 1,
	    tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/* hyper to network format */
hyper
htonH(hyper num)
{
	char		temp;
	static int	endian_type = 0;
	union {
		char chars[8];
		hyper hyper_value;
	}			buf;

	if (endian_type == 0) {
		buf.hyper_value = 1;
		endian_type = ((uint8_t)buf.chars[7] > 0) ? 1 : 2;
	}
	if (endian_type == 2) {
		buf.hyper_value = num;
		temp = buf.chars[0];
		buf.chars[0] = buf.chars[7];
		buf.chars[7] = temp;
		temp = buf.chars[1];
		buf.chars[1] = buf.chars[6];
		buf.chars[6] = temp;
		temp = buf.chars[2];
		buf.chars[2] = buf.chars[5];
		buf.chars[5] = temp;
		temp = buf.chars[3];
		buf.chars[3] = buf.chars[4];
		buf.chars[4] = temp;
		return (buf.hyper_value);
		}
	else
		return (num);
}

/* for now return the first ipv4 address */
char *
sflow_get_agent_ip()
{
	char			*addr_buffer;
	struct ifaddrs	*ifa = NULL;
	void			*temp_add = NULL;
	struct ifaddrs	*if_add_s = NULL;

	getifaddrs(&if_add_s);
	for (ifa = if_add_s; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa ->ifa_addr->sa_family == AF_INET) {
			temp_add =
			    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			addr_buffer = calloc(1, INET_ADDRSTRLEN);
			inet_ntop(AF_INET, temp_add, addr_buffer,
			    INET_ADDRSTRLEN);
			lg("%s IPv4 Address %s\n", ifa->ifa_name, addr_buffer);
			if (strncmp(addr_buffer, "127.0.0.1", 9) != 0) {
				if (if_add_s != NULL) freeifaddrs(if_add_s);
					return (addr_buffer);
				} else {
				free(addr_buffer);
				addr_buffer = NULL;
				}

	} else if (ifa->ifa_addr->sa_family == AF_INET6) {
			temp_add =\
			    &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
			addr_buffer = calloc(1, INET6_ADDRSTRLEN);
			inet_ntop(AF_INET6, temp_add, addr_buffer,
			    INET6_ADDRSTRLEN);
			lg("%s IPv6 Address %s, ignored\n", ifa->ifa_name,
			    addr_buffer);
			free(addr_buffer);
			addr_buffer = NULL;
		}
	}

	if (if_add_s != NULL) freeifaddrs(if_add_s);
	assert(0);
	exit(-1); /* do not even have a valid if, out of game */
}


int
sflow_unit_test()
{
	/* macro unit tests */
	/* function unit tests */
	int i;
	uint64_t ifspeed = 0;
	expanding_array_t *arr = expanding_array_create();

	arr->set(arr, 0, arr);
	arr->set(arr, 7, arr);
	arr->set(arr, 10, arr);
	arr->set(arr, 17, arr);
	arr->set(arr, 16, arr);
	for (i = 0; i < arr->ea_capacity; i++) {
		lg("Array records[%u] %u\n", i, (arr->get(arr, i)));
	}
	lg("%u", arr);
	expanding_array_destroy(arr);
	(void) sflow_get_one_kstat("net0", "ifspeed", KSTAT_DATA_UINT64,
	    &ifspeed, B_TRUE);
	lg("if speed is %u", ifspeed);
	return (0);
}


/*
 * == == == == == == == == == = getting kernel stat
 */

int
sflow_kstat_value(kstat_t *ksp, const char *name, uint8_t type, void *buf)
{
	kstat_named_t   *knp;

	if ((knp = kstat_data_lookup(ksp, (char *)name)) == NULL) {
			lg("look up failed");
		return (-1);
		}

	if (knp->data_type != type) {
			lg("type error, should be type %d", knp->data_type);
			return (-1);
		}
	switch (type) {
	case KSTAT_DATA_UINT64:
		*(uint64_t *)buf = knp->value.ui64;
		break;
	case KSTAT_DATA_UINT32:
		*(uint32_t *)buf = knp->value.ui32;
		break;
		case KSTAT_DATA_STRING:
				strncpy(buf, knp->value.str.addr.ptr,
				    knp->value.str.len);
				break;
	default:
				lg("type not support %d\n", type);
		return (-1);
	}

	return (0);
}

int
sflow_query_kstat(char *module, int instance, const char *name, \
const char *stat, uint8_t type, void *val)
{
	kstat_t			*ksp;
	kstat_ctl_t		*kcp;

	if ((kcp = kstat_open()) == NULL) {
		lg("kstat open operation failed");
		return (-1);
	}
	/*
	 * If looking up a kstat in the 'link' module check if passed
	 * linkname has a zone prefix. For zone prefixed linknames, the kstat
	 * instance is the zone ID. kstats of NGZ datalinks are exported
	 * to the GZ under an instance matching the NGZ's zone ID.
	 */
	if (strcmp(module, "link") == 0 && instance == 0 &&
	    strchr(name, '/') != NULL) {
		char linkname[MAXLINKNAMELEN];
		zoneid_t	zid;

		if (!dlparse_zonelinkname(name, linkname, &zid))
			goto bail;
		ksp = kstat_lookup(kcp, module, zid, (char *)linkname);
	} else {
		ksp = kstat_lookup(kcp, module, instance, (char *)name);
	}

	if (ksp == NULL) {
		/*
		 * The kstat query could fail if the underlying MAC
		 * driver was already detached.
		 */
		goto bail;
	}

	if (kstat_read(kcp, ksp, NULL) == -1) {
		lg("kstat read failed !\n");

		goto bail;
	}

	if (sflow_kstat_value(ksp, stat, type, val) < 0) {
		lg("get value faied\n");
		goto bail;
	}
	(void) kstat_close(kcp);
	return (0);

bail:
	(void) kstat_close(kcp);
	return (-1);
}

int
sflow_get_one_kstat(const char *name, const char *stat, uint8_t type,
    void *val, boolean_t islink)
{
	char		module[DLPI_LINKNAME_MAX];
	uint_t		instance;

	if (islink) {
		return (sflow_query_kstat("link", 0, name, stat, type, val));
	} else {
		if (dlpi_parselink(name, module, sizeof (module),
		    &instance) != DLPI_SUCCESS) {
				return (-1);
	}

	return (sflow_query_kstat(module, instance, "phys", stat, type, val));
	}
}


uint32_t
sflow_get_link_stat_32(const char *name, const char *stat)
{
	uint32_t	buffer;

	(void) sflow_get_one_kstat(name, stat, KSTAT_DATA_UINT32,
	    &buffer, B_TRUE);
	return (buffer);
}

uint64_t
sflow_get_link_stat_64(const char *name, const char *stat)
{
	uint64_t	buffer;

	(void) sflow_get_one_kstat(name, stat, KSTAT_DATA_UINT64,
	    &buffer, B_TRUE);
	return (buffer);
}

int
sflow_get_if_link_id(char *if_name)
{
	int					status;
	datalink_id_t		linkid;
	datalink_class_t	class;
	uint32_t			media,	flags;
	dladm_handle_t		handle;

	if ((status = dladm_open(&handle,
	    NETADM_ACTIVE_PROFILE)) != DLADM_STATUS_OK) {
			lg("could not open /dev/dld");
			return (-1);
	}
	if (dladm_name2info(handle, if_name, &linkid, &flags, &class,
	    &media) != DLADM_STATUS_OK) {
		lg("could not find link for name %s\n", if_name);
		return (-1);
	}
	lg("the link id for %s is %d\n", if_name, linkid);
	dladm_close(handle);
	return (linkid);
}


char *
toCharArray(int num, int len)
{
	char *str = malloc(len);

	sprintf(str, "%d", num);
	return (str);
}

void
mac_convert_str_to_char(char *buf, const char *str)
{
	uint_t mac_int[6];
	int i;

	sscanf(str, "%x:%x:%x:%x:%x:%x",
	    &mac_int[0], &mac_int[1], &mac_int[2],
	    &mac_int[3], &mac_int[4], &mac_int[5]);
	for (i = 0; i < 6; i++)
		buf[i] = (unsigned char)mac_int[i];
}

void
randomize(void *start, size_t s)
{
	size_t off = 0;
	while (off < s) {
		*((char *)start + (off++)) = rand() % 256;
	}
}

void
mem_print(const char *name, void *start, size_t len)
{
	int i;

	lg("printing %s at %u\n", name, start);
	for (i = 0; i < len; i++) {
		lg("%d[%u]\t", i, *((char *)start + i));
	}
	lg("\n");
}

/*
 * reverse(): Reverse string s in place.
 */
static void
reverse(char *s)
{
	int c, i, j;

	for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = (char)c;
	}
}


/*
 * itoa(): Convert n to characters in s.
 */
void
itoa(long n, char *s)
{
	long i, sign;

	if ((sign = n) < 0)	/* record sign */
		n = -n;	/* make sign positive */
	i = 0;
	do {    /* generate digits in reverse order */
		s[i++] = n % 10 + '0';  /* get next digit */
	} while ((n /= 10) > 0);	/* delete it */
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
}


/*
 * get up time in seconds
 * have to do thread safety protection
 * because of setutxent() and endutxent()
 */
static pthread_mutex_t sflow_uptime_lock =
		PTHREAD_MUTEX_INITIALIZER;
static time_t start_time = 0;
uint32_t
get_up_time_sec()
{
	struct utmpx	*utp;
	struct stat	sbuf;
	int		days, hrs, mins;
	int		entries, i = 0;
	time_t	now;
	time_t	uptime;
	time_t	ret;

	if (start_time != 0) {
		lg("already knows strat time!");
		(void) time(&now);	/* get current time */
		uptime = now - start_time;
		uptime += 30;
		return (uptime);
	}

	if (stat(UTMPX_FILE, &sbuf) == -1) {
		lg("stat error\n");
		exit(1);
	}
	entries = sbuf.st_size / sizeof (struct futmpx);
	(void) utmpxname(UTMPX_FILE);

	setutxent();
	while ((i++ < entries) && ((utp = getutxent()) != NULL)) {
		if (utp->ut_type == USER_PROCESS) {
			} else if (utp->ut_type == BOOT_TIME) {
				(void) time(&now);	/* get current time */
				uptime = now - utp->ut_tv.tv_sec;
				start_time = utp->ut_tv.tv_sec;
				uptime += 30;
				lg("get time [%u]", uptime);
				ret = uptime;
				days = uptime / (60*60*24);
				uptime %= (60*60*24);
				hrs = uptime / (60*60);
				uptime %= (60*60);
				mins = uptime / 60;
					lg("up:");
				if (days > 0)
					lg(" %d day(s),", days);
				if (hrs > 0 && mins > 0) {
					lg(" %2d:%02d,", hrs, mins);
				} else {
					if (hrs > 0)
						lg(" %d hr(s),", hrs);
					if (mins > 0)
						lg(" %d min(s),", mins);
				break;
			}
		}
	}
	endutxent();

	return ((uint32_t)ret);
}

uint32_t
get_up_time_sec_safe()
{
	uint32_t time;
	pthread_mutex_lock(&sflow_uptime_lock);
	time = get_up_time_sec();
	pthread_mutex_unlock(&sflow_uptime_lock);
	return (time);
}



/* get physical machine type */
machine_type_t
get_host_phys_machine_type()
{
	char tmp[64];
	sysinfo(SI_ARCHITECTURE, tmp, 64);
	if (strncmp(tmp, "i386", strlen("i386")) == 0) {
		return (MT_X86);
	} else {
		return (MT_UNKNOWN);
	}
}
/* get physical page size */
uint32_t
get_host_phys_page_size()
{
	uint32_t page_size = 1024 * 4; /* 4k bytes */

	/* determine page size */
	switch (get_host_phys_machine_type()) {
	case MT_X86:
		page_size = 1024 * 4;
		break;
	case MT_SPARC:
		page_size = 1024 * 8;
		break;
	default:
		page_size = 1024 * 4;
		break;
	}
	return (page_size);
}

/* get physical total memory */
hyper
get_host_phys_mem_bytes() {
	uint32_t buf_32 = 0;

	sflow_query_kstat("unix", 0, "system_pages",
	    "physmem", KSTAT_DATA_UINT32, &(buf_32));
	return (hyper)buf_32 * (hyper)get_host_phys_page_size();
}
