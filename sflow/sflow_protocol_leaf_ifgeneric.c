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

#include "sflow_util.h"
#include "sflow_data_list.h"
#include "sflow_protocol_leaf_ifgeneric.h"

/* counter  data : generic (0 1) flatten */
uint32_t
sflow_counter_generic_flat(char **buf, void *cr_generic)
{
	memcpy(*buf, cr_generic, sizeof (sflow_cr_if_generic_t) - SFLOW_PSIZE);
	free(cr_generic);
	return (sizeof (sflow_cr_if_generic_t) - SFLOW_PSIZE);
}

/* counter  data : generic (0 1) enpacking */
sflow_cr_if_generic_t *
sflow_counter_generic_pack(sflow_source_t *source)
{
	const char				*name = source->ss_name;
	sflow_cr_if_generic_t	*g =
	    calloc(1, sizeof (sflow_cr_if_generic_t));

	g->ifIndex				=
	    SFLOW_HTONL(((ssd_ifgeneric_t *)source->
	    src_specific_data)->ifg_datalink);
	g->ifType				=
	    SFLOW_HTONL(source->ss_id_type);
	g->ifSpeed				=
	    htonH(sflow_get_link_stat_64(name, "ifspeed"));
	g->ifDirection			=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "link_duplex"));
	g->ifStatus				=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "link_state"));
	g->ifInOctets			=
	    htonH(sflow_get_link_stat_64(name, "rbytes64"));
	g->ifInUcastPkts		=
	    SFLOW_UNKOWN;
	g->ifInMulticastPkts	=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "multircv"));
	g->ifInBroadcastPkts	=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "brdcstrcv"));
	g->ifInDiscards			=
	    SFLOW_UNKOWN;
	g->ifInErrors			=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "ierrors"));
	g->ifInUnknownProtos	=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "unknowns"));
	g->ifOutOctets			=
	    htonH(sflow_get_link_stat_64(name, "obytes64"));
	g->ifOutUcastPkts		=
	    SFLOW_UNKOWN;
	g->ifOutMulticastPkts	=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "multixmt"));
	g->ifOutBroadcastPkts	=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "brdcstxmt"));
	g->ifOutDiscards		=
	    SFLOW_UNKOWN;
	g->ifOutErrors			=
	    SFLOW_HTONL(sflow_get_link_stat_32(name, "oerrors"));
	g->ifPromiscuousMode	=
	    SFLOW_HTONL(B_TRUE);
	g->flat = &sflow_counter_generic_flat;
	return (g);
}



/*
 * == == == == == == == == == = sflow_sample thread
 */
sflow_sp_t *
get_sample_ifgeneric(sflow_source_t *source)
{

	int			i;
	int			err;
	void		*buf;
	/* sflow encapsulation */
	sflow_cr_t	*cr;
	sflow_cs_t	*cs;
	sflow_sp_t	*sp;
	sflow_dg_t	*dg;
	sflow_cr_if_generic_t			*g;
	sflow_data_list_node_t			*temp_node;
	sflow_data_list_t				*temp_list;

	/* for now we assume a generic counter */
	g = sflow_counter_generic_pack(source);
	cr = sflow_counter_record_pack((void*)g, 0, 1);
	temp_node = sflow_data_list_node_create("dummy node",
	    (void *)cr, CHECK_DATA);
	temp_list = sflow_data_list_create("dummy list");
	sflow_data_list_append(temp_list, temp_node);
	cs = sflow_counter_sample_pack(temp_list, source);
	sp = sflow_sample_pack((void *)cs, 0, 2);
	return (sp);
}
