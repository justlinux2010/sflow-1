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

/* pack into structures & flat to raw data ------ */
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <errno.h>

#include "sflow_protocol_branches.h"
#include "sflow_data_list.h"
#include "sflow_util.h"
#include "sflow_source.h"
#include "sflow_protocol_leaf_flowsample.h"

/* flow  data : generic (0 1) flatten */
uint32_t
sflow_fr_raw_pkt_flat(char **buf, void *r_pkt)
{
	int							buf_off = 0;
	sflow_fr_raw_pkt_header_t	*r =
	    (sflow_fr_raw_pkt_header_t *)r_pkt;

	memcpy(*buf, r, sizeof (sflow_fr_raw_pkt_header_t) - 2 * SFLOW_PSIZE);
	buf_off += sizeof (sflow_fr_raw_pkt_header_t) - 2 * SFLOW_PSIZE;
	memcpy(*buf + buf_off, r->fr_header, SFLOW_NTOHL(r->fr_header_size));
	buf_off += SFLOW_NTOHL(r->fr_header_size);
	assert(r);
	assert(r->fr_header);
	/* free(r->header); */
	free(r_pkt);
	return	(buf_off);
}

/* flow  data : generic (0 1) enpacking */
sflow_fr_raw_pkt_header_t *
sflow_fr_raw_pkt_pack(void *raw_header, int len)
{
	int							crop =
	    len > 128 ? 128 : len;
	sflow_fr_raw_pkt_header_t	*r =
	    calloc(1, sizeof (sflow_fr_raw_pkt_header_t));

	crop = crop - (crop % 4);
	r->fr_header = raw_header;
	r->fr_frame_length = SFLOW_HTONL((uint32_t)len);
	r->fr_header_protocol = SFLOW_HTONL(1); /* ether */
	r->fr_header_size = SFLOW_HTONL((uint32_t)crop);
	r->fr_stripped = SFLOW_HTONL((uint32_t)(len - crop));
	r->flat = & sflow_fr_raw_pkt_flat;
	return (r);
}




sflow_sp_t *
get_sample_flowsample(sflow_source_t *source, char *pkt, int len)
{
	sflow_fr_raw_pkt_header_t	*f;
	sflow_fr_t					*fr;
	sflow_fs_t					*fs;
	sflow_sp_t					*sp;
	sflow_data_list_t			*temp_list;
	sflow_data_list_node_t		*temp_node;

	/* mod the data a little bit */
	((ssd_flowsample_t *)source->src_specific_data)
	    ->fsp_pool = source->ss_count *
	    source->ss_src.ss_i.ss_rate; /* no drops */

	f = sflow_fr_raw_pkt_pack(pkt, len);
	fr = sflow_flow_record_pack((void*)f, source);
	temp_node = sflow_data_list_node_create("flow node", (void *)fr,
	    CHECK_DATA);
	temp_list = sflow_data_list_create("flow list");
	sflow_data_list_append(temp_list, temp_node);
	fs = sflow_flow_sample_pack(temp_list, source);
	sp = sflow_sample_pack((void *)fs, 0, 1);
	return (sp);
}

int
fd_provider_flowsample(sflow_source_t *src)
{
	int					err;
	int					sockfd;
	struct	sockaddr_ll	sll;

	if ((sockfd = socket(PF_PACKET, SOCK_RAW, ETH_P_ALL)) == -1) {
		lg("Couldn't create PF_PACKET socket\n");
		return (-1);
	}
	(void) memset(&sll, 0, sizeof (sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ((ssd_flowsample_t *)
	    (src->src_specific_data))->fsp_datalink;
	sll.sll_protocol = ETH_P_ALL;
	if (bind(sockfd, (struct sockaddr *)&sll, sizeof (sll)) == -1) {
		lg("Couldn't bind to the socket");
		(void) close(sockfd);
		return (-1);
	}
	/* set the private option for kernal sampling */
	lg("setting rate %d\n", src->ss_src.ss_i.ss_rate);
	err = setsockopt(sockfd, SOL_PACKET, PACKET_SAMPLE_RATE,
	    &(src->ss_src.ss_i.ss_rate), sizeof (uint32_t));
	if (err != 0) {
		lg("\nCouldn't set rate for socket( %d, %d, %s)\n", err, errno,
		    strerror(errno));
		(void) close(sockfd);
		return (-1);
	}
	return (sockfd);
}
