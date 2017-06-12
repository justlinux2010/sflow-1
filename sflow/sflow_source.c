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

#include <pthread.h>
#include <sys/zone.h>

#include "sflow_door.h"
#include "sflow_source.h"
#include "sflow_util.h"
#include "expanding_array.h"
#include "sflow_protocol_leaf_flowsample.h"
#include "sflow_protocol_leaf_ifgeneric.h"
#include "sflow_protocol_leaf_hostphy.h"
#include "sflow_protocol_leaf_hostv.h"
#include "sflow_protocol_leaf_flowstat.h"


/* the sources for select to monitor, indexed by associated fds */
int maxfd = -1;
fd_set	to_read, to_watch;
expanding_array_t *sflow_fd_sources;
pthread_mutex_t	sflow_inter_src_mod_mtx =
    PTHREAD_MUTEX_INITIALIZER;

/* the list of all the sources */
list_t *sflow_all_sources;
pthread_mutex_t	sflow_all_src_mod_mtx =
    PTHREAD_MUTEX_INITIALIZER;



static void
remove_space(char *prop)
{
	int	index1 = 0, index2 = 0;

	while (prop[index1] != '\0') {
		if (prop[index1] == '\t' || prop[index1++] == ' ')
			continue;
		prop[index2++] = prop[index1 -1];
	}
	prop[index2] = '\0';
}


/*
 * called after removed space
 * get one atribure
 */
static char *
sflow_find_attribute(char *prop, char *target)
{
	char	*end = NULL;
	char	*index = prop;
	char	*buf = calloc(1, SF_ATTR_LEN);

	while (1) {
		end = strstr(index, ",");
		if (strncmp(index, target, strlen(target)) == 0) {
			strncpy(buf, index +strlen(target),
			    (end == NULL)?
			    (prop + strlen(prop))- (index +strlen(target)):
			    end-(index +strlen(target)));
			memset(index +strlen(target), ' ',
			    (end == NULL)?
			    (prop + strlen(prop))- (index +strlen(target)):
			    end-(index +strlen(target)));
			break;
		}
		if (end == NULL)
			break;
		index = end + 1;
		continue;
	}
	remove_space(prop);
	return (buf);
}

void *
sflow_polling_source_instance_container_thread(void *arg);
/* spawn a thread */
static int
sflow_spawn_poll_thread(sflow_source_t  *src)
{
	int				err;
	pthread_attr_t	attr;
	pthread_t		thr;
	sflow_instance_thread_data_t *instance_attr;

	lg("request poll instance to start\n");
	(void) pthread_attr_init(&attr);
	(void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	instance_attr = calloc(1, sizeof (sflow_instance_thread_data_t));
	instance_attr->src = src;
	err = pthread_create(&thr, &attr,
	    &sflow_polling_source_instance_container_thread,
	    (void*) instance_attr);
	if (err)
		goto clean;
	src->ss_src.ss_p.ss_thread = thr;
	lg("new polling thread has spawned\n");
	goto clean;

clean:
	(void) pthread_attr_destroy(&attr);

	lg("init return  %d\n", err);
	return (err);
}


/* add src pipe line 4 (last) : enable src */
int
sflow_add_source_type(sflow_source_t *src)
{
	int err = 0;
	int sockfd;
	sflow_source_list_node_t *sf_node;

	lg("adding [%s]\n", src->ss_name);
	switch (src->ss_type) {
	/* inter */
	case SF_SRC_FLOWSAMPLE:
		sockfd = src->ss_src.ss_i.ss_fd;
		/* adding to fd list */
		pthread_mutex_lock(&sflow_inter_src_mod_mtx);
		FD_SET(sockfd, &to_watch);
		if (sockfd > maxfd)
			maxfd = sockfd;
		sflow_fd_sources->set(sflow_fd_sources, sockfd, src);
		pthread_mutex_unlock(&sflow_inter_src_mod_mtx);

	break;
	/* poll */
	case SF_SRC_IFGENERIC:
	case SF_SRC_PHYHOST:
	case SF_SRC_VHOST:
	case SF_SRC_FLOWSTAT:
		lg("poll thread! \n");
		err = sflow_spawn_poll_thread(src);
		if (err < 0)
			return (err);
	break;
	}
	/* adding to all src */
	sf_node = calloc(1, sizeof (sflow_source_list_node_t));
	sf_node->ssln_src = src;
	pthread_mutex_lock(&sflow_all_src_mod_mtx);
	list_insert_tail(sflow_all_sources, sf_node);
	pthread_mutex_unlock(&sflow_all_src_mod_mtx);

	return (err);
}

/* add src pipe line 3: making src struct */
int
sflow_make_add_one_source_type(sflowd_door_asrc_t *asrc,
    sflow_source_type_t src_to_add_type) {
	int	err;
	char *prop = NULL;
	sflow_source_t *src_to_add = calloc(1, sizeof (sflow_source_t));
	/*
	 * step 1: fill in general information
	 * However this is very agile, we can move any general
	 * information later to source specific and assign values
	 * to them in switch block, yet still in this function
	 */
	src_to_add->ss_count = 0;
	strncpy(src_to_add->ss_name, asrc->sd_srcname,
	    strlen(asrc->sd_srcname));
	/*
	 * step 2: fill in src specific info
	 */
	switch (src_to_add_type) {
	case SF_SRC_FLOWSAMPLE:
		/* section 1 */
		src_to_add->ss_id_type = 0;
		src_to_add->ss_type = src_to_add_type;
		src_to_add->ss_ent = 0;
		src_to_add->ss_fmt = 1;
		/* section 2 */
		src_to_add->src_specific_data =
				calloc(1, sizeof (ssd_flowsample_t));
		((ssd_flowsample_t *)(src_to_add->src_specific_data))
		    ->fsp_pool = 0;
		err = sflow_get_if_link_id(src_to_add->ss_name);
		if (err < 0)
			return (err);
		((ssd_flowsample_t *)(src_to_add->src_specific_data))
		    ->fsp_datalink = err;
		src_to_add->ss_id = err;
		/* section 3 */
		prop = sflow_find_attribute(asrc->sd_propstr, "rate=");
		lg("get rate %s\n", prop);
		if (*prop == '\0')
			src_to_add->ss_src.ss_i.ss_rate =	DFT_RATE_FS;
		else
			src_to_add->ss_src.ss_i.ss_rate =	atoi(prop);
		src_to_add->ss_src.ss_i.ss_get_sample =
		    &get_sample_flowsample;
		err = fd_provider_flowsample(src_to_add);
		if (err < 0)
			return (err);
		src_to_add->ss_src.ss_i.ss_fd = err;

		break;
	case SF_SRC_IFGENERIC:
		/* section 1 */
		src_to_add->ss_id_type = 0;

		src_to_add->ss_type = src_to_add_type;
		src_to_add->ss_ent = 0;
		src_to_add->ss_fmt = 2;
		/* section 2 */
		src_to_add->src_specific_data =
				calloc(1, sizeof (ssd_ifgeneric_t));
		err = sflow_get_if_link_id(src_to_add->ss_name);
		if (err < 0)
			return (err);
		((ssd_ifgeneric_t *)(src_to_add->src_specific_data))
		    ->ifg_datalink = err;
		src_to_add->ss_id = err;
		/* section 3 */
		src_to_add->ss_src.ss_p.ss_get_sample =
		    &get_sample_ifgeneric;
		prop = sflow_find_attribute(asrc->sd_propstr, "interval=");
		if (*prop == '\0')
			src_to_add->ss_src.ss_p.ss_interval =	DFT_INTERVAL_CT;
		else
			src_to_add->ss_src.ss_p.ss_interval =	atoi(prop);
		break;
	case SF_SRC_FLOWSTAT:
		/* section 1 */
		src_to_add->ss_type = src_to_add_type;
		src_to_add->ss_ent = 0;
		src_to_add->ss_fmt = 2;
		/* section 2 */
		src_to_add->src_specific_data = NULL;
		err = sflow_get_if_link_id(src_to_add->ss_name);
		if (err < 0)
			return (err);
		src_to_add->ss_id_type = 0;
		src_to_add->ss_id = err;
		/* section 3 */
		src_to_add->ss_src.ss_p.ss_get_sample =
		    &get_sample_flow_stat;
		prop = sflow_find_attribute(asrc->sd_propstr, "interval=");
		if (*prop == '\0')
			src_to_add->ss_src.ss_p.ss_interval =	DFT_INTERVAL_CT;
		else
			src_to_add->ss_src.ss_p.ss_interval =	atoi(prop);
		break;
	case SF_SRC_PHYHOST:
		/* section 1 */
		src_to_add->ss_id_type = 2;
		src_to_add->ss_id = 0; /* global zone */
		src_to_add->ss_type = src_to_add_type;
		src_to_add->ss_ent = 0;
		src_to_add->ss_fmt = 2;
		/* section 2 */
		src_to_add->src_specific_data = NULL;
		/* section 3 */
		src_to_add->ss_src.ss_p.ss_get_sample =
		    &get_sample_hostphy;
		prop = sflow_find_attribute(asrc->sd_propstr, "interval=");
		if (*prop == '\0')
			src_to_add->ss_src.ss_p.ss_interval =	DFT_INTERVAL_CT;
		else
			src_to_add->ss_src.ss_p.ss_interval =	atoi(prop);
		break;
	case SF_SRC_VHOST:
		/* section 1 */
		src_to_add->ss_id_type = 3;
		src_to_add->ss_id = 0; /* change to zone id */
		src_to_add->ss_type = src_to_add_type;
		src_to_add->ss_ent = 0;
		src_to_add->ss_fmt = 2;
		/* section 2 */
		src_to_add->src_specific_data =
				calloc(1, sizeof (ssd_hostv_t));
		((ssd_hostv_t *)(src_to_add->src_specific_data))->zone_id =
				atoi(asrc->sd_srcname);
		src_to_add->ss_id =
		    ((ssd_hostv_t *)(src_to_add->src_specific_data))->zone_id;
		if (zone_getattr(((ssd_hostv_t *)
		    (src_to_add->src_specific_data))->zone_id, ZONE_ATTR_NAME,
		    &(src_to_add->ss_name),
		    sizeof (src_to_add->ss_name)) == -1) {
			return (-1);
		}
		lg("zone id %d matches name %s",
		    (int)(((ssd_hostv_t *)
		    (src_to_add->src_specific_data))->zone_id),
		    src_to_add->ss_name);
		/* section 3 */
		src_to_add->ss_src.ss_p.ss_get_sample =
		    &get_sample_hostv;
		prop = sflow_find_attribute(asrc->sd_propstr, "interval=");
		if (*prop == '\0')
			src_to_add->ss_src.ss_p.ss_interval =	DFT_INTERVAL_CT;
		else
			src_to_add->ss_src.ss_p.ss_interval =	atoi(prop);
		break;

	default:
		lg("do not recognize type %d", src_to_add_type);
		return (-1);
		break;
	}

	/* step 3, add the src */
	if (prop != NULL)
		free(prop);
	err = sflow_add_source_type(src_to_add);

	lg("result : %d", err);
	return (err);
}

/* add src pipe line 2: selectively support "all" as name */
/* todo: maybe add name checking here? */
int
sflow_make_add_source_type(sflowd_door_asrc_t *asrc,
    sflow_source_type_t src_to_add_type) {
	int status = 0;
	int zone_cnt = 4;
	zoneid_t *zone_ids = calloc(sizeof (zoneid_t), zone_cnt);
	int zone_cnt_now;
	int i;

	switch (src_to_add_type) {
		case SF_SRC_VHOST: /* we support adding all zones */
			if (strncmp(asrc->sd_srcname, SFLOW_CMD_ALL,
			    strlen(SFLOW_CMD_ALL)) == 0) {
				lg("adding all zones as sources.\n");
				/* Get the current list of running zones */
				for (;;) {
					zone_cnt_now = zone_cnt;
					(void) zone_list
					    (zone_ids, &zone_cnt_now);
					if (zone_cnt_now <= zone_cnt)
						break;
					if ((zone_ids = (zoneid_t *)
					    realloc(zone_ids,
						(zone_cnt_now) *
					    sizeof (zoneid_t)))
					    != NULL) {
						zone_cnt = zone_cnt_now;
					} else {
						/*
						 * Could not allocate to
						 * get new zone list.
						 */
						if (zone_ids != NULL)
							free(zone_ids);
						zone_ids = NULL;
						return (-1);
					}
				}

				/* skip 0 which is global zone */
				for (i = 1; i < zone_cnt_now; i++) {
					lg("add src for zone id %d\n",
					    zone_ids[i]);
					/*
					 * use the id as name for now, swap
					 * to real name
					 * in pipe line step3 (making src)
					 */
					memset(asrc->sd_srcname, 0,
					    sizeof (asrc->sd_srcname));
					itoa((long)zone_ids[i],
					    asrc->sd_srcname);
					status =
					    sflow_make_add_one_source_type(asrc,
					    src_to_add_type);
				}
			}
			break;
		default:
			status =
			    sflow_make_add_one_source_type
			    (asrc, src_to_add_type);
		break;
	}
	/* free(asrc); */
	if (zone_ids != NULL)
		free(zone_ids);
	zone_ids = NULL;

	return (status);
}
/* add src pipe line 1: filter the type information */
int
sflow_add_source(sflowd_door_asrc_t *asrc)
{
	int			err;
	sflow_source_type_t src_to_add_type;

	remove_space(asrc->sd_srcname);
	remove_space(asrc->sd_propstr);
	remove_space(asrc->sd_typestr);

	lg("Adding source:[%s][%s][%s]\n",
	    asrc->sd_srcname, asrc->sd_typestr, asrc->sd_propstr);

	if (strncmp(asrc->sd_typestr, "flow_sample",
	    strlen("flow_sample")) == 0) {
		src_to_add_type = SF_SRC_FLOWSAMPLE;
		return (sflow_make_add_source_type(asrc, src_to_add_type));
	} else if (strncmp(asrc->sd_typestr, "if_generic",
	    strlen("if_generic")) == 0) {
		src_to_add_type = SF_SRC_IFGENERIC;
		return (sflow_make_add_source_type(asrc, src_to_add_type));
	} else if (strncmp(asrc->sd_typestr, "flow_stat",
	    strlen("flow_stat")) == 0) {
		src_to_add_type = SF_SRC_FLOWSTAT;
		return (sflow_make_add_source_type(asrc, src_to_add_type));
	} else if (strncmp(asrc->sd_typestr, "host_phys",
	    strlen("host_phys")) == 0) {
		src_to_add_type = SF_SRC_PHYHOST;
		return (sflow_make_add_source_type(asrc, src_to_add_type));
	} else if (strncmp(asrc->sd_typestr, "host_virt",
	    strlen("host_virt")) == 0) {
		src_to_add_type = SF_SRC_VHOST;
		return (sflow_make_add_source_type(asrc, src_to_add_type));
	} else {
	/* invalid type */
	}
	return (err);
}

int
sflow_stop_source(sflow_source_t *src)
{
	int err;
	int i;
	sflow_source_t *tmp;

	switch (src->ss_type) {
	case SF_SRC_FLOWSAMPLE: /* or any inter src */
		/* remove fd from the fd sources */
		pthread_mutex_lock(&sflow_inter_src_mod_mtx);
		FD_CLR(src->ss_src.ss_i.ss_fd, &to_watch);
		sflow_fd_sources->set(sflow_fd_sources,
		    src->ss_src.ss_i.ss_fd, NULL);
		if (maxfd == src->ss_src.ss_i.ss_fd) {
			for (i = sflow_fd_sources->ea_capacity - 1;
			    i >= 0; i--) {
			tmp =  sflow_fd_sources->get(sflow_fd_sources, i);
				if (tmp != NULL) {
					maxfd = i;
					break;
				}
			}
		}
		pthread_mutex_unlock(&sflow_inter_src_mod_mtx);
		close(src->ss_src.ss_i.ss_fd);
	break;
	case SF_SRC_IFGENERIC:
	case SF_SRC_PHYHOST:
	case SF_SRC_VHOST:
	case SF_SRC_FLOWSTAT:
		/* or any other poll src */
		err = pthread_cancel(src->ss_src.ss_p.ss_thread);
		if (err != 0)
			return (err);
		break;
	}
	free(src->src_specific_data);
	free(src);
	return (0);
}

/* remove src, supports "all" for both type and name */
int
sflow_remove_source(sflowd_door_rsrc_t *rsrc)
{
	sflow_source_list_node_t *node_cur = NULL;
	sflow_source_list_node_t *node_pre = NULL;
	sflow_source_t *src;
	sflow_source_type_t src_to_rm_type;
	boolean_t type_all = B_FALSE;
	boolean_t name_all = B_FALSE;

	remove_space(rsrc->sd_srcname);
	remove_space(rsrc->sd_typestr);

	lg("removing source:[%s][%s]\n",
	    rsrc->sd_srcname, rsrc->sd_typestr);

	/* type string to int */
	if (strncmp(rsrc->sd_typestr, "flow_sample",
	    strlen("flow_sample")) == 0) {
		src_to_rm_type = SF_SRC_FLOWSAMPLE;
	} else if (strncmp(rsrc->sd_typestr, "if_generic",
	    strlen("if_generic")) == 0) {
		src_to_rm_type = SF_SRC_IFGENERIC;
	} else if (strncmp(rsrc->sd_typestr, "flow_stat",
	    strlen("flow_stat")) == 0) {
		src_to_rm_type = SF_SRC_FLOWSTAT;
	} else if (strncmp(rsrc->sd_typestr, "host_phys",
	    strlen("host_phys")) == 0) {
		src_to_rm_type = SF_SRC_PHYHOST;
	} else if (strncmp(rsrc->sd_typestr, "host_virt",
	    strlen("host_virt")) == 0) {
		src_to_rm_type = SF_SRC_VHOST;
	} else if (strncmp(rsrc->sd_typestr, SFLOW_CMD_ALL,
	    strlen(SFLOW_CMD_ALL)) == 0) {
		type_all = B_TRUE;
	} else {
	/* invalid type */
	}

	/* all name? */
	if (strncmp(rsrc->sd_srcname, SFLOW_CMD_ALL,
	    strlen(SFLOW_CMD_ALL)) == 0) {
		name_all = B_TRUE;
	}

	do {
		if (node_pre == NULL)
			node_cur = list_head(sflow_all_sources);
		else
			node_cur = list_next(sflow_all_sources, node_pre);

		if (node_cur == NULL)
			break;
		src = node_cur->ssln_src;
		lg("Looking at src %s, type %d\n", src->ss_name, src->ss_type);

		if ((name_all == B_TRUE ||
		    0 == strncmp(src->ss_name, rsrc->sd_srcname,
		    strlen(rsrc->sd_srcname))) &&
		    (type_all == B_TRUE || src->ss_type == src_to_rm_type)) {
			sflow_stop_source(node_cur->ssln_src);
			list_remove(sflow_all_sources, node_cur);
			free(node_cur);
			lg("src is stopped and removed\n");
		} else {
			node_pre = node_cur;
		}
	} while (node_cur != NULL);
	return (0);
}

/* show src, supports "all" for both type and name */
int
sflow_show_src(sflowd_door_ssrc_t *ssrc, sflowd_ssr_t **buf, int *len)
{
	int		i;
	char	*str_val;
	sflow_source_t		*src;
	sflow_src_fields_t *to_add;
	sflow_source_list_node_t *node_cur = NULL;
	sflow_source_type_t src_to_show_type;
	boolean_t type_all = B_FALSE;
	boolean_t name_all = B_FALSE;

	*len = sizeof (sflowd_ssr_t);
	*buf = calloc(1, *len);
	(*buf)->ssr_link_count = 0;

	remove_space(ssrc->sd_srcname);
	remove_space(ssrc->sd_typestr);

	lg("showing source:[%s][%s]\n",
	    ssrc->sd_srcname, ssrc->sd_typestr);
	/* type string to int */
	if (strncmp(ssrc->sd_typestr, "flow_sample",
	    strlen("flow_sample")) == 0) {
		src_to_show_type = SF_SRC_FLOWSAMPLE;
	} else if (strncmp(ssrc->sd_typestr, "if_generic",
	    strlen("if_generic")) == 0) {
		src_to_show_type = SF_SRC_IFGENERIC;
	} else if (strncmp(ssrc->sd_typestr, "flow_stat",
	    strlen("flow_stat")) == 0) {
		src_to_show_type = SF_SRC_FLOWSTAT;
	} else if (strncmp(ssrc->sd_typestr, "host_phys",
	    strlen("host_phys")) == 0) {
		src_to_show_type = SF_SRC_PHYHOST;
	} else if (strncmp(ssrc->sd_typestr, "host_virt",
	    strlen("host_virt")) == 0) {
		src_to_show_type = SF_SRC_VHOST;
	} else if (strncmp(ssrc->sd_typestr, SFLOW_CMD_ALL,
	    strlen(SFLOW_CMD_ALL)) == 0) {
		type_all = B_TRUE;
	} else {
	/* invalid type */
	}
	/* all name? */
	if (strncmp(ssrc->sd_srcname, SFLOW_CMD_ALL,
	    strlen(SFLOW_CMD_ALL)) == 0) {
		name_all = B_TRUE;
	}

	while (1) {
		if (node_cur == NULL)
			node_cur = list_head(sflow_all_sources);
		else
			node_cur = list_next(sflow_all_sources, node_cur);

		if (node_cur == NULL)
			break;
		src = node_cur->ssln_src;
		if ((name_all == B_TRUE ||
		    0 == strncmp(src->ss_name, ssrc->sd_srcname,
		    strlen(ssrc->sd_srcname))) &&
		    (type_all == B_TRUE || src->ss_type == src_to_show_type)) {
			*len += (sizeof (sflow_src_fields_t));
			(*buf) = realloc(*buf, *len);
			(*buf)->ssr_link_count++;
			to_add = (sflow_src_fields_t *)
			    ((char *)(*buf) + *len -
			    sizeof (sflow_src_fields_t));
			memset((char *)to_add, 0, sizeof (sflow_src_fields_t));
			strncpy(to_add->ssf_src_name, src->ss_name,
			    sizeof (to_add->ssf_src_name));
			switch (src->ss_type) {
			case SF_SRC_FLOWSAMPLE:
				strncpy(to_add->ssf_type, "flow_sample",
				    sizeof (to_add->ssf_type));
				break;
			case SF_SRC_IFGENERIC:
				strncpy(to_add->ssf_type, "if_generic",
				    sizeof (to_add->ssf_type));
				break;
			case SF_SRC_FLOWSTAT:
				strncpy(to_add->ssf_type, "flow_stat",
				    sizeof (to_add->ssf_type));
				break;
			case SF_SRC_PHYHOST:
				strncpy(to_add->ssf_type, "host_phys",
				    sizeof (to_add->ssf_type));
				break;
			case SF_SRC_VHOST:
				strncpy(to_add->ssf_type, "host_virt",
				    sizeof (to_add->ssf_type));
				break;
			default:
				strncpy(to_add->ssf_type, "???",
				    sizeof (to_add->ssf_type));
				break;
			}

			if (src->ss_type > SF_SRC_INTER_POLL_SEPERATION) {
			str_val = toCharArray(src->ss_src.ss_p.ss_interval,
			    sizeof (to_add->ssf_interval));
			strncpy(to_add->ssf_interval, str_val,
			    sizeof (to_add->ssf_interval));
			} else  {
			str_val = toCharArray(src->ss_src.ss_i.ss_rate,
			    sizeof (to_add->ssf_rate));
			strncpy(to_add->ssf_rate, str_val,
			    sizeof (to_add->ssf_rate));
			}
		}
	}
	return (0);
}
