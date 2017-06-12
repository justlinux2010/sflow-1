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


#include <sys/socket.h>

#include "sflow_sender.h"
#include "sflow_protocol_branches.h"
#include "sflow_source.h"
#include "sflow_util.h"
#include "sflow_protocol_leaf_ifgeneric.h"
#include "sflow_protocol_leaf_flowsample.h"
#include "sflow_protocol_leaf_hostphy.h"
#include "sflow_protocol_leaf_hostv.h"
#include "sflow_protocol_leaf_flowstat.h"

/* counter record flat */
uint32_t
sflow_flow_record_flat(char **buf, void *sflow_fr)
{
	int			buf_off = 0;
	char		*start_point;
	sflow_fr_t	*fr = (sflow_fr_t *)sflow_fr;

	memcpy(*buf, fr, sizeof (sflow_fr_t) - 2 * SFLOW_PSIZE);
	buf_off += sizeof (sflow_fr_t) - 2 * SFLOW_PSIZE;
	start_point = *buf + buf_off;
	if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(fr->sfr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(fr->sfr_dfmt)) == 1) {
		buf_off += ((sflow_fr_raw_pkt_header_t *)(fr->sfr_data))->
		    flat(&start_point, fr->sfr_data);
	}
	free(sflow_fr);
	return	(buf_off);
}

/* counter record enpacking */
sflow_fr_t *
sflow_flow_record_pack(void *flow_data, sflow_source_t *source)
{
	sflow_fr_t		*fr = calloc(1, sizeof (sflow_fr_t));

	SFLOW_SET_DATAFORMAT((fr->sfr_dfmt), source->ss_ent,
	    source->ss_fmt);
	fr->sfr_dfmt = SFLOW_HTONL(fr->sfr_dfmt);
	/* case separation */
	/* 0 1 is the RAW header sample */
	if (source->ss_ent == 0 && source->ss_fmt == 1) {
		fr->sfr_len =
		    sizeof (sflow_fr_raw_pkt_header_t) - SFLOW_PSIZE * 2
		    + SFLOW_NTOHL(((sflow_fr_raw_pkt_header_t *)flow_data)
		    ->fr_header_size);
	}
	fr->sfr_len = SFLOW_HTONL(fr->sfr_len);
	fr->sfr_data = flow_data;
	fr->flat = &sflow_flow_record_flat;
	return (fr);
}

/* sflow sample data : flow sample (0 1) flatten */
uint32_t
sflow_flow_sample_flat(char **buf, void *sflow_fs)
{
	/* copy the header */
	sflow_fr_t	*fr;
	int			i = 0;
	int			len = 0;
	sflow_fs_t	*fs = (sflow_fs_t *)sflow_fs;

	memcpy(*buf, fs, sizeof (sflow_fs_t) - SFLOW_PSIZE);
	len += sizeof (sflow_fs_t) - SFLOW_PSIZE;
	/* flat the data in sequence */
	for (; i < SFLOW_NTOHL(fs->sfs_count); i++) {
		char *start_point = (*buf + len);
		fr = *(sflow_fr_t **)((char *)fs +
		    sizeof (sflow_fs_t) + SFLOW_PSIZE * i);
		len += fr->flat(&start_point, fr);
	}
	free(fs);
	return (len);
}

/* sflow sample data : flow sample (0 1) packing */
sflow_fs_t *
sflow_flow_sample_pack(sflow_data_list_t *record_list,
    sflow_source_t *source)
{
	int			i;
	sflow_fs_t	*fs = calloc(1, sizeof (sflow_fs_t) +
	    SFLOW_PSIZE * record_list->sdl_count);

	fs->sfs_seq_no = SFLOW_HTONL(source->ss_count);
	/* note that data source and ent/fmt is not same */
	SFLOW_SET_DATASOURCE(fs->sfs_dsrc, source->ss_id_type,
	    source->ss_id);
	fs->sfs_dsrc	= SFLOW_HTONL(fs->sfs_dsrc);
	fs->sfs_rate	= SFLOW_HTONL(source->ss_src.ss_i.ss_rate);
	fs->sfs_pool	= SFLOW_HTONL(((ssd_flowsample_t *)source
	    ->src_specific_data)->fsp_pool);
	fs->sfs_drops	= SFLOW_HTONL(((ssd_flowsample_t *)source
	    ->src_specific_data)->fsp_drops);
	fs->input_if	= SFLOW_UNKOWN;
	fs->output_if	= SFLOW_UNKOWN; /* to do */
	fs->sfs_count	= SFLOW_HTONL(record_list->sdl_count);
	fs->flat		= &sflow_flow_sample_flat;
	/* piggy back the pointers to the sample records */
	for (i = 0; i < record_list->sdl_count; i++) {
		sflow_data_list_node_t *node;
		sflow_data_list_read(record_list, &node);
		/* copy the value of the pointer to the data */
		*((void **)((char *)fs +
		    sizeof (sflow_fs_t) + i * SFLOW_PSIZE)) = node->sdln_data;
		sflow_data_list_node_free(node);
		}
	sflow_data_list_free(record_list);
	return (fs);
}


/* counter record flat */
uint32_t
sflow_counter_record_flat(char **buf, void *sflow_cr)
{
	int			buf_off = 0;
	char		*start_point;
	sflow_cr_t	*cr = (sflow_cr_t *)sflow_cr;

	memcpy(*buf, cr, sizeof (uint32_t) * 2);
	buf_off += sizeof (uint32_t) * 2;
	start_point = *buf + buf_off;
	lg("couter record at %u\n", sflow_cr);
	if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 1) {
		buf_off += ((sflow_cr_if_generic_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2000) {
		buf_off += ((sflow_cr_host_descr_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2001) {
		buf_off += ((sflow_cr_host_adapters_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2002) {
		buf_off += ((sflow_cr_host_parent_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2003) {
		buf_off += ((sflow_cr_host_cpu_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2004) {
		buf_off += ((sflow_cr_host_memory_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2005) {
		buf_off += ((sflow_cr_host_disk_io_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2006) {
		buf_off += ((sflow_cr_host_net_io_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2100) {
		buf_off += ((sflow_cr_virt_leaf_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2101) {
		buf_off += ((sflow_cr_virt_cpu_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2102) {
		buf_off += ((sflow_cr_virt_memory_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 2104) {
		buf_off += ((sflow_cr_virt_net_io_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(cr->scr_dfmt)) == 52 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(cr->scr_dfmt)) == 52) {
		buf_off += ((sflow_cr_flow_stat_t *)(cr->scr_data))->
		    flat(&start_point, cr->scr_data);
	}
	free(sflow_cr);
	return	(buf_off);
}

/* counter record enpacking */
sflow_cr_t *
sflow_counter_record_pack(void *counter_data, int ent, int fmt)
{
	sflow_cr_t	*cr = calloc(1, sizeof (sflow_cr_t));

	SFLOW_SET_DATAFORMAT((cr->scr_dfmt), ent, fmt);
	cr->scr_dfmt = SFLOW_HTONL(cr->scr_dfmt);
	/* case separation */
	if (ent == 0 && fmt == 1) {
		cr->scr_len = sizeof (sflow_cr_if_generic_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2000) {
		cr->scr_len = sizeof (sflow_cr_host_descr_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2001) {
		cr->scr_len =
		    ((sflow_cr_host_adapters_t *)counter_data)->has_len;
	} else if (ent == 0 && fmt == 2002) {
		cr->scr_len = sizeof (sflow_cr_host_parent_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2003) {
		cr->scr_len = sizeof (sflow_cr_host_cpu_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2004) {
		cr->scr_len = sizeof (sflow_cr_host_memory_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2005) {
		cr->scr_len = sizeof (sflow_cr_host_disk_io_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2006) {
		cr->scr_len = sizeof (sflow_cr_host_net_io_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2100) {
		cr->scr_len = sizeof (sflow_cr_virt_leaf_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2101) {
		cr->scr_len = sizeof (sflow_cr_virt_cpu_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2102) {
		cr->scr_len = sizeof (sflow_cr_virt_memory_t) - SFLOW_PSIZE;
	} else if (ent == 0 && fmt == 2104) {
		cr->scr_len = sizeof (sflow_cr_virt_net_io_t) - SFLOW_PSIZE;
	} else if (ent == 52 && fmt == 52) { /* flow stat */
		cr->scr_len = sizeof (sflow_cr_flow_stat_t) - SFLOW_PSIZE;
	}
	lg("%d, %d : %d\n", ent, fmt, cr->scr_len);
	cr->scr_len = SFLOW_HTONL(cr->scr_len);
	cr->scr_data = counter_data;
	cr->flat = &sflow_counter_record_flat;
	return (cr);
}

/* sflow sample data : counter sample (0 2) flatten */
uint32_t
sflow_counter_sample_flat(char **buf, void *sflow_cs)
{
	/* copy the header */
	int			len = 0;
	int			i = 0;
	sflow_cr_t	*cr;
	sflow_cs_t	*cs = (sflow_cs_t *)sflow_cs;

	memcpy(*buf, cs, sizeof (uint32_t) * 3);
	len += sizeof (uint32_t) * 3;
	/* flat the data in sequence */
	lg("counter num : %d\n", SFLOW_NTOHL(cs->scs_count));
	for (; i < SFLOW_NTOHL(cs->scs_count); i++) {
		char *start_point = (*buf + len);
		cr = *(sflow_cr_t **)((char *)cs +
		    sizeof (sflow_cs_t) + SFLOW_PSIZE * i);
		lg("address %u\n", cr);
		len += cr->flat(&start_point, cr);
	}
	free(sflow_cs);
	return (len);
}

/* sflow sample data : counter sample (0 2) packing */
sflow_cs_t *
sflow_counter_sample_pack(sflow_data_list_t *record_list,
    sflow_source_t *src)
{
	int			i, to;
	sflow_cs_t	*cs = calloc(1, sizeof (sflow_cs_t) +
	    SFLOW_PSIZE * record_list->sdl_count);

	cs->scs_seq_no = SFLOW_HTONL(src->ss_count++);
	/* note that data source and ent/fmt is not same */
	SFLOW_SET_DATASOURCE(cs->scs_dsrc, src->ss_id_type, src->ss_id);
	cs->scs_dsrc = SFLOW_HTONL(cs->scs_dsrc);
	cs->scs_count = record_list->sdl_count;
	cs->scs_count = SFLOW_HTONL(cs->scs_count);
	cs->flat = &sflow_counter_sample_flat;
	/* piggy back the pointers to the sample records */
	to = record_list->sdl_count;
	for (i = 0; i < to; i++) {
		sflow_data_list_node_t *node;
		sflow_data_list_read(record_list, &node);
		/* copy the value of the pointer to the data */
		*((void **)((char *)cs +
		    sizeof (sflow_cs_t) + i * SFLOW_PSIZE)) = node->sdln_data;
		sflow_data_list_node_free(node);
		/* node is suppose to be automatic obj */
		}
		sflow_data_list_free(record_list);
	return (cs);
}
/*
 * sflow_sample data flatten
 */
uint32_t
sflow_sample_flat(char ** buf, void * sflow_sp)
{	char			*start_point;
	int				i = 0;
	sflow_sp_t		*sp  = (sflow_sp_t *)sflow_sp;
	uint32_t		tmp = 0;

	/* copy the header */
	memcpy(*buf, sp, sizeof (uint32_t) * 2);
	tmp += sizeof (uint32_t) * 2;
	/* flatten the sflow sample data */
	start_point = *buf + sizeof (uint32_t) * 2;

	if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(sp->ssp_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(sp->ssp_dfmt)) == 1) {
		tmp += ((sflow_fs_t *)(sp->data))->flat(&start_point, sp->data);
	} else if (SFLOW_GET_DATA_ENT(SFLOW_NTOHL(sp->ssp_dfmt)) == 0 &&
	    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(sp->ssp_dfmt)) == 2) {
		tmp += ((sflow_cs_t *)(sp->data))->flat(&start_point, sp->data);
	} else {
		lg(" fformating error %u, %u!!\n",
		    SFLOW_GET_DATA_ENT(SFLOW_NTOHL(sp->ssp_dfmt)),
		    SFLOW_GET_DATA_FMT(SFLOW_NTOHL(sp->ssp_dfmt)));
	}
	free(sflow_sp);
	return (tmp);
}

/* sflow_sample en-packing */
sflow_sp_t *
sflow_sample_pack(void *sample_data, int ent, int fmt)
{
	sflow_sp_t	*sflow_sp = calloc(1, sizeof (sflow_sp_t));

	sflow_sp->data = sample_data;
	sflow_sp->flat = &sflow_sample_flat;
	SFLOW_SET_DATAFORMAT((sflow_sp->ssp_dfmt), ent, fmt);
	sflow_sp->ssp_dfmt = SFLOW_HTONL(sflow_sp->ssp_dfmt);
	sflow_sp->len = 0;
	/* we have to seperate the cases */
	if (ent == 0 && fmt == 1) {
		int i = 0;
		sflow_fs_t *sflow_fs = (sflow_fs_t *)sample_data;
		sflow_sp->len += sizeof (sflow_fs_t) - SFLOW_PSIZE;
		for (; i < SFLOW_NTOHL(sflow_fs->sfs_count); i++) {
			sflow_sp->len +=
			    SFLOW_NTOHL((*((sflow_fr_t **)((char *)sflow_fs
			    + sizeof (sflow_fs_t) + SFLOW_PSIZE * i)))->sfr_len)
			    + sizeof (sflow_fr_t) - SFLOW_PSIZE *2;
		}
	} else if (ent == 0 && fmt == 2) { /* counter sample */
		int i = 0;
		sflow_cs_t *sflow_cs = (sflow_cs_t *)sample_data;
		sflow_sp->len += sizeof (sflow_cs_t) - SFLOW_PSIZE;
		for (; i < SFLOW_NTOHL(sflow_cs->scs_count); i++) {
			sflow_sp->len +=
			    SFLOW_NTOHL((*((sflow_cr_t **)((char *)sflow_cs
			    + sizeof (sflow_cs_t) + SFLOW_PSIZE * i)))->scr_len)
			    + sizeof (sflow_cr_t) - SFLOW_PSIZE *2;
		}
	}
	sflow_sp->len = SFLOW_HTONL(sflow_sp->len);
	return (sflow_sp);
}
/*
 * sflow_datagram flatten
 * This is a little complex mainly because the
 * size difference of IPv4 and IPv6 and the need
 * to figure out the size of the entire data to
 * wrap in the UDP packet to alloc the buffer
 * for the entire packet
 */
uint32_t
sflow_datagram_flat(char ** buf, void * sflow_dg)
{
	int			i = 0;
	uint32_t	off_dg = 0;
	uint32_t	off_buf = 0;
	sflow_dg_t	*dg = (sflow_dg_t *)sflow_dg;
	/* calculate the size of the entire sflow datagram */
	/* function pointer will not be sent */
	uint32_t	size = sizeof (sflow_dg_t) - SFLOW_PSIZE;

	/* union by default takes max room */
	if (SFLOW_IPVER == 4)
		size -= 12;
	/* get the size of samples */
	for (; i < SFLOW_NTOHL(dg->sample_num); i++) {
		size += sizeof (sflow_sp_t) - SFLOW_PSIZE * 2;
		size += SFLOW_NTOHL((*((sflow_sp_t **)((char *)dg +
		    sizeof (sflow_dg_t) + i *SFLOW_PSIZE)))->len);
	}
	*buf = calloc(1, size);
	/* flat this header */
	memcpy(*buf, dg, sizeof (uint32_t) * 2);
	off_buf += sizeof (uint32_t) * 2;
	off_dg += sizeof (uint32_t) * 2;
	if (SFLOW_IPVER == 4) {
		memcpy(*buf + off_buf, dg->ip_addr.v4, sizeof (dg->ip_addr.v4));
		off_buf += sizeof (dg->ip_addr.v4);
	} else {
		memcpy(*buf + off_buf, dg->ip_addr.v6, sizeof (dg->ip_addr.v6));
		off_buf += sizeof (dg->ip_addr.v6);
	}
	off_dg += 16;
	memcpy(*buf + off_buf, (char *)dg + off_dg, sizeof (uint32_t) * 4);
	off_buf += sizeof (uint32_t) * 4;
	for (i = 0; i < SFLOW_NTOHL(dg->sample_num); i++) {
		sflow_sp_t *sample_data =
		    *((sflow_sp_t **)((char *)dg +
		    sizeof (sflow_dg_t) + i *SFLOW_PSIZE));
		/* every struct flat itself recursively */
		char *start_point = (*buf + off_buf);
		off_buf += sample_data->flat(&start_point, sample_data);

	}
	free(sflow_dg);
	return (size);
}

/* sflow_datagram en-packing */
static int datagram_count = 0;
sflow_dg_t *
sflow_datagram_pack(sflow_data_list_t *sample_list)
{
	int			i, to;
	uint32_t	size =
	    sizeof (sflow_dg_t) + sample_list->sdl_count * SFLOW_PSIZE;
	sflow_dg_t	*sflow_datagram = calloc(1, size);

	sflow_datagram->dg_seq = SFLOW_HTONL(datagram_count++);
	sflow_datagram->flat = &sflow_datagram_flat;
	sflow_datagram->ip_ver = SFLOW_HTONL(SFLOW_IPVER == 4 ? 1 : 2);
	sflow_datagram->sflow_ver = SFLOW_HTONL(SFLOW_VER);
	if (SFLOW_IPVER == 4) {
		inet_pton(AF_INET, sflow_agent_ip_v4,
		    &(sflow_datagram->ip_addr.v4));
		} else {
		inet_pton(AF_INET6, "", &(sflow_datagram->ip_addr.v6));
		}
	sflow_datagram->sample_num = SFLOW_HTONL(sample_list->sdl_count);
	sflow_datagram->sub_agent_id = SFLOW_DUMMY;
	sflow_datagram->up_time = SFLOW_HTONL(get_up_time_sec());
	/* piggy back the pointers to the samples */
	/* lg("read list, count %u!\n", sample_list->sdl_count); */
	to = sample_list->sdl_count;
	for (i = 0; i < to; i++) {
		sflow_data_list_node_t *node;
		sflow_data_list_read(sample_list, &node);
		/* copy the value of the pointer to the data */
		*((void **)((char *)sflow_datagram +
		    sizeof (sflow_dg_t) + i * SFLOW_PSIZE))
		    = node->sdln_data;
		sflow_data_list_node_free(node);
		/* automatic obj outside the func */
		}
	sflow_data_list_free(sample_list);
	return (sflow_datagram);
}
