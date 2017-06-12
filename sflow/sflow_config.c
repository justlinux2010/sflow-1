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
#include "sflow_config.h"
#include "sflow_err_code.h"

#define	CFG_FILE	"./sflow_cfg"

collector_dest_t *collector_dest = NULL;
static FILE *sflow_cfg_file;

int
sflow_load_cfg() {
	char line[128];

	sflow_cfg_file = fopen(CFG_FILE, "r");
	/* file not exist, ask for cfg */
	if (sflow_cfg_file == NULL) {
		lg("Can't open config file!\n");
	/* return (sflow_ask_cfg()); */
		return (SF_NO_CFG_FILE);
	}
	/* file exists */
	if (collector_dest != NULL) {
		free(collector_dest);
		collector_dest = NULL;
	}
	collector_dest = calloc(1, sizeof (collector_dest_t));

	/* read each line */
	while (fgets(line, sizeof (line), sflow_cfg_file) != NULL) {
		if (strstr(line, "Collector IPv4:") != NULL) {
			if (sscanf(line, "Collector IPv4:[%s]",
			    &collector_dest->ipv4) == 1) {
				lg(line);
			}
		}
		if (strstr(line, "Collector Port:") != NULL) {
			if (sscanf(line, "Collector Port:[%d]",
			    &collector_dest->port) == 1) {
				lg(line);
			}
		}
	}
	/* some information missing */
	if (collector_dest->ipv4[0] == '\0' || collector_dest->port == 0)
		return (SF_BAD_CFG_FILE);
	return (0);
}

int
sflow_save_cfg() {
	int status = 0;

	return (status);
}

/* should not happen as this is a daemon */
int
sflow_ask_cfg() {
	int status = 0;

	return (status);
}
