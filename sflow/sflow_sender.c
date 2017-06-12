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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "sflow_data_list.h"
#include "sflow_util.h"
#include "sflow_protocol_branches.h"

#define		SFLOW_HOST		"10.132.146.129"
#define		SFLOW_HOSTPORT	4096
#define		MAX_BUF_TIME	2
#define		SFLOW_MTU	1400


extern sflow_data_list_t *sflow_buffered_samples;

int			sk, slen;
char		*sflow_agent_ip_v4;
struct sockaddr_in	si;

/* packets to send in UDP */

sflow_data_list_t		*samples_data_to_send;
pthread_mutex_t			sflow_alarm_indicator =
						    PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t			sflow_buffered_samples_mtx =
						    PTHREAD_MUTEX_INITIALIZER;


int
sflow_sender_init()
{
	int err;

	sflow_agent_ip_v4 = sflow_get_agent_ip();
	err = init_udp_socket();
	return (err);
}

/* initialize sending socket */
int
init_udp_socket()
{
	slen = sizeof (si);
	memset((char *)&si, 0, sizeof (si));
	si.sin_port = htons(SFLOW_HOSTPORT);
	si.sin_family = AF_INET;
	inet_aton(SFLOW_HOST, &si.sin_addr);
	sk = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	return (sk);
}

/*
 * event driven sender
 * require locks before call
 */
void
send_samples()
{
	char		*to_send;
	sflow_dg_t	*sflow_dg;
	int			sent, size;

	pthread_mutex_unlock(&sflow_alarm_indicator);
	lg("sending event is trigger, now pack stuff!");
	sflow_dg = sflow_datagram_pack(sflow_buffered_samples);
	size = sflow_datagram_flat(&to_send, sflow_dg);
	sent = sendto(sk, to_send, size, 0,
	    (const struct sockaddr *)&si, slen);
	free(to_send);
	lg("> > > \t \t DATA %d SENT %d\n", size, sent);
	sflow_buffered_samples = sflow_data_list_create("lsit to send in udp");
}


/* append sample to list, and start alarm */
void
sflow_append_sample_to_send(sflow_sp_t *sample)
{
	sflow_data_list_node_t *sample_node;

	/* if alarm is not started , start it */
	if (pthread_mutex_trylock(&sflow_alarm_indicator) == 0) {
		alarm(MAX_BUF_TIME);
		lg("alarm start because first add data\n");
	}

	sample_node =
	    sflow_data_list_node_create("sample to send", (void *)sample,
	    SFLOW_NTOHL(sample->len) + SFLOW_PSIZE * 2);
	/* if buffered more than SFLOW_MTU */

	pthread_mutex_lock(&sflow_buffered_samples_mtx);
	if (sflow_buffered_samples->sdl_data_size + sample_node->sdln_data_size
	    + sizeof (sflow_dg_t) -SFLOW_PSIZE > SFLOW_MTU) {
		lg("have data %u, try to add %u\n",
		    sflow_buffered_samples->sdl_data_size,
		    sample_node->sdln_data_size +SFLOW_PSIZE *2);
		lg("buffered to SFLOW_MTU..send now\n");
		alarm(0); /* cancel */
		send_samples();
		/* for the new sample */
		if (pthread_mutex_trylock(&sflow_alarm_indicator) == 0) {
			alarm(MAX_BUF_TIME);
			}
		}
	sflow_data_list_append(sflow_buffered_samples, sample_node);
	/* size of a sample */
	pthread_mutex_unlock(&sflow_buffered_samples_mtx);
}
