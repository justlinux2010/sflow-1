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
#include <getopt.h>
#include <stdint.h>
#include <libintl.h>
#include <stdarg.h>
#include <getopt.h>
#include <door.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "../cmd-inet/usr.lib/sflowd/sflow_door.h"
#include <iso/string_iso.h>
#include <locale.h>
#include <ofmt.h>
#include <stddef.h>
#include <assert.h>
static char *progname;

static void	die(const char *, ...);
static void	die_opterr(int, int, const char *);

/* callback functions for printing output */
typedef void cmdfunc_t(int, char **, const char *);
static cmdfunc_t do_add_source, do_remove_source, do_show_source;
static cmdfunc_t do_show_help;

typedef struct	cmd {
	char		*c_name;	/* command name */
	cmdfunc_t	*c_fn;		/* handler function */
	const char	*c_usage;	/* usage string */
} cmd_t;

static cmd_t	cmds[] = {
	{ "show-source",	do_show_source,
	    "\tshow-source [-t <source-type>] [<source-name>]" },
	{ "add-source",		do_add_source,
	    "\tadd-source  -t <source-type> <source-name> "
	    "[-p {prop=<val>[,...]}[,...]] " },
	{ "remove-source",	do_remove_source,
	    "\tremove-source [-t <source-type>] [<source-name>]" },

	/* keep this last */
	{ "help", do_show_help, "\thelp\t[<subcommand>]" }
};

static const struct option lopts[] = {
	{"prop",	required_argument,	0, 'p'  },
	{"type",	required_argument,	0, 'T'  },
	{ 0, 0, 0, 0 }
};


int
sflow_door_call(void *arg, size_t asize, void *rbufp, size_t rsize)
{
	int err = sflow_door_dyncall(arg, asize, &rbufp, &rsize);
	if (err != 0) {
		free(rbufp);
		die("do not expect size change in door call buffer");
	}
	return (err);
}
int
sflow_door_dyncall(void *arg, size_t asize, void **rbufp, size_t *rsize)
{
	int i;
	door_arg_t	darg;
	int			door_fd, err;

	darg.data_ptr = arg;
	darg.data_size = asize;
	darg.desc_ptr = NULL;
	darg.desc_num = 0;
	darg.rbuf = *rbufp;
	darg.rsize = *rsize;
	door_fd = open(SFLOW_DOOR, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
	if (door_fd == -1)
		return (errno);

	if (door_call(door_fd, &darg) == -1) {
		err = errno;
		(void) close(door_fd);
		return (errno);
	}
	(void) close(door_fd);
	printf("returned size is %d, expected %d\n", darg.rsize, *rsize);
	if (darg.rbuf != *rbufp || darg.rsize != *rsize) {
			/* allocated memory will be freed by the caller */
			if ((*rbufp = realloc(*rbufp, darg.rsize)) == NULL) {
				return (ENOMEM);
			} else {
				(void) memcpy(*rbufp, darg.rbuf, darg.rsize);
			}
		/* munmap() the door buffer */
		(void) munmap(darg.rbuf, darg.rsize);
		return (EBADE);
	}
	return (*((uint_t *)(darg.rbuf)));
}

static void
do_show_help(int argc, char *argv[], const char *use)
{
	int		i, num;
	cmd_t	*cmdp;

	if (argc > 2)
		die("invalid arguments\nusage:\n%s", use);
	if (argc == 1) {
		(void) fprintf(stderr,
		    gettext("The following subcommands are supported:\n"));
		(void) fprintf(stderr, gettext(
		    "Data-Source\t: add-source\t remove-source\t show-source\n"
		    "For more info, run: "
		    "sfadm help <subcommand>\n"));
		return;
	}

	num = (sizeof (cmds) / sizeof (cmds[0]));
	for (i = 0; i < num; i++) {
		cmdp = &cmds[i];
		if (strcmp(cmdp->c_name, argv[1]) == 0 &&
		    cmdp->c_usage != NULL) {
			(void) fprintf(stderr, gettext("usage:\n%s\n"),
			    gettext(cmdp->c_usage));
			break;
		}
	}
	if (i == num) {
		(void) fprintf(stderr,
		    gettext("Unrecognized subcommand: '%s'\n"), argv[1]);
		(void) fprintf(stderr,
		    gettext("For more info, run: evsadm help\n"));
		exit(EXIT_FAILURE);
	}
}

static void
do_sfadm_default_output()
{
	(void) fprintf(stderr,
	    gettext("The following subcommands are supported:\n"));
	(void) fprintf(stderr, gettext(
	    "Data-Source\t: add-source\t "
	    "remove-source\t show-source\n"
	    "For more info, run: "
	    "sfadm help <subcommand>\n"));
}

int
main(int argc, char *argv[])
{
	int			i, err;
	cmd_t		*cmdp;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;
	if (argc >= 2 && strcmp(argv[1], "help") == 0) {
		int helpindex;
		helpindex = (sizeof (cmds) /sizeof (cmds[0])) - 1;
		cmdp = &cmds[helpindex];
		do_show_help(argc - 1, &argv[1], gettext(cmdp->c_usage));
		return (EXIT_SUCCESS);
	}
	if (argc == 1) {
		do_sfadm_default_output();
		return (EXIT_SUCCESS);
	}
	for (i = 0; i < sizeof (cmds) / sizeof (cmds[0]); i++) {
		cmdp = &cmds[i];
		if (strcmp(argv[1], cmdp->c_name) == 0) {
			cmdp->c_fn(argc - 1,  &argv[1], gettext(cmdp->c_usage));
			return (EXIT_SUCCESS);
		}
	}
	(void) fprintf(stderr, gettext("Unrecognized subcommand: '%s'\n"),
	    argv[1]);
	(void) fprintf(stderr, gettext("For more info, run: sfadm help\n"));
	return (EXIT_FAILURE);
}

/* PRINTFLIKE1 */
static void
die(const char *format, ...)
{
	va_list alist;

	format = gettext(format);
	(void) fprintf(stderr, "%s: ", progname);
	va_start(alist, format);
	(void) vfprintf(stderr, format, alist);
	va_end(alist);
	(void) putc('\n', stderr);
	exit(EXIT_FAILURE);
}


static void
die_opterr(int opt, int opterr, const char *usage)
{
	switch (opterr) {
	case ':':
		die("option '-%c' requires a value\nusage:\n%s", opt,
		    gettext(usage));
		break;
	case '?':
	default:
		die("unrecognized option '-%c'\nusage:\n%s", opt,
		    gettext(usage));
		break;
	}
}

/* ARGSUSED */
static void
do_add_source(int argc, char *argv[], const char *use)
{
	int		err;
	int		option;
	char	propstr[SFADM_PROPSTRLEN];
	char	typestr[SFADM_PROPSTRLEN];
	sflowd_retval_t		rval;
	sflowd_door_asrc_t	asrc;

	propstr[0] = '\0';
	typestr[0] = '\0';
	while ((option = getopt_long(argc, argv, "p:T:",
	    lopts, NULL)) != -1) {
		switch (option) {
		case 'p':
			if (propstr[0] != '\0')
				(void) strlcat(propstr, ",", SFADM_PROPSTRLEN);
			if (strlcat(propstr, optarg, SFADM_PROPSTRLEN) >=
			    SFADM_PROPSTRLEN) {
				die("property list too long '%s'", propstr);
			}
			break;
		case 'T':
			if (typestr[0] != '\0')
				die("can only specify one type!\n");
			if (strlcat(typestr, optarg, SFADM_TYPESTRLEN) >=
			    SFADM_TYPESTRLEN) {
				die("property list too long '%s'", typestr);
			}
			break;
		default:
			die_opterr(optopt, option, use);
			break;
		}
	}
	/* source name is required */
	if (optind != argc - 1 || typestr[0] == '\0')
		die("usage:\n%s", use);
	/* use dladm_name2info() to verify linkname */
	asrc.sd_cmd = SFLOWD_CMD_ADD_SOURCE;
	if (strlcpy(asrc.sd_srcname, argv[optind], sizeof (asrc.sd_srcname)) >=
	    MAX_LINK_NAMELEN) {
		die("invalid linkname");
	}
	(void) strlcpy(asrc.sd_propstr, propstr, sizeof (asrc.sd_propstr));
	(void) strlcpy(asrc.sd_typestr, typestr, sizeof (asrc.sd_typestr));
	err = sflow_door_call(&asrc, sizeof (asrc), &rval, sizeof (rval));
	if (err != 0)
		die("addition of source failed: %s", strerror(err));
}

static void
do_remove_source(int argc, char *argv[], const char *use)
{
	int		err;
	int		option;
	char	typestr[SFADM_PROPSTRLEN];
	sflowd_retval_t		rval;
	sflowd_door_rsrc_t	rsrc;
	memset(&rsrc, 0, sizeof (rsrc));

	typestr[0] = '\0';
	while ((option = getopt_long(argc, argv, "T:",
	    lopts, NULL)) != -1) {
		switch (option) {
		case 'T':
			if (typestr[0] != '\0')
				die("can only specify one type!\n");
			if (strlcat(typestr, optarg, SFADM_TYPESTRLEN) >=
			    SFADM_TYPESTRLEN) {
				die("property list too long '%s'", typestr);
			}
			break;
		default:
			die_opterr(optopt, option, use);
			break;
		}
	}

	/* use dladm_name2info() to verify linkname */
	rsrc.sd_cmd = SFLOWD_CMD_REMOVE_SOURCE;
	if (optind == argc - 1) /* specified name */
	{
		if (strlcpy(rsrc.sd_srcname, argv[optind],
		    sizeof (rsrc.sd_srcname)) >=
		    MAX_LINK_NAMELEN) {
		die("invalid linkname");
		}
	}
	else
	{
		strlcpy(rsrc.sd_srcname, SFLOW_CMD_ALL,
		    sizeof (rsrc.sd_srcname));
	}
	if (typestr[0] == '\0')
		(void) strlcpy(rsrc.sd_typestr, SFLOW_CMD_ALL,
		    sizeof (rsrc.sd_typestr));
	else
		(void) strlcpy(rsrc.sd_typestr, typestr,
		    sizeof (rsrc.sd_typestr));
	err = sflow_door_call(&rsrc, sizeof (rsrc), &rval, sizeof (rval));
	if (err != 0)
		die("addition of source failed: %s", strerror(err));
}

/*
 * default output callback function that, when invoked,
 * prints string which is offset by ofmt_arg->ofmt_id within buf.
 */
static boolean_t
print_default_cb(ofmt_arg_t *ofarg, char *buf, uint_t bufsize)
{
	char *value;

	value = (char *)ofarg->ofmt_cbarg + ofarg->ofmt_id;
	(void) strlcpy(buf, value, bufsize);
	return (B_TRUE);
}

static const ofmt_field_t sflow_link_fields[] = {
/* name, width, index, callback */
{ "NAME",	20,
	offsetof(sflow_src_fields_t, ssf_src_name), print_default_cb},
{ "TYPE",	20,
	offsetof(sflow_src_fields_t, ssf_type), print_default_cb},
{ "RATE",	10,
	offsetof(sflow_src_fields_t, ssf_rate), print_default_cb},
{ "INTERVAL",	10,
	offsetof(sflow_src_fields_t, ssf_interval), print_default_cb},
{ NULL,		0, 0, NULL}}
;

static 	ofmt_handle_t		ofmt;
static int
sflow_print_source(void *buf)
{
	char *fields_str = "name,type,rate,interval";
	uint_t		ofmtflags = 0;
	int err = 0;

	if (ofmt == NULL)
		err = ofmt_open(fields_str,
		    sflow_link_fields, ofmtflags, 80, &ofmt);
	if (err != 0)
		return (err);
	ofmt_print(ofmt, buf);
}

static void
do_show_source(int argc, char *argv[], const char *use)
{
	int		err, i;
	int		option;
	char	typestr[SFADM_PROPSTRLEN];
	size_t	rval_size;
	sflowd_ssr_t		*rval;
	sflowd_door_ssrc_t	ssrc;
	sflow_src_fields_t	*to_show;

	rval_size = sizeof (sflowd_ssr_t);
	memset(&ssrc, 0, sizeof (ssrc));

	typestr[0] = '\0';
	while ((option = getopt_long(argc, argv, "T:",
	    lopts, NULL)) != -1) {
		switch (option) {
		case 'T':
			if (typestr[0] != '\0')
				die("can only specify one type!\n");
			if (strlcat(typestr, optarg, SFADM_TYPESTRLEN) >=
			    SFADM_TYPESTRLEN) {
				die("property list too long '%s'", typestr);
			}
			break;
		default:
			die_opterr(optopt, option, use);
			break;
		}
	}

	/* use dladm_name2info() to verify linkname */
	ssrc.sd_cmd = SFLOWD_CMD_SHOW_SOURCE;
	if (optind == argc - 1) /* specified name */
	{
		if (strlcpy(ssrc.sd_srcname, argv[optind],
		    sizeof (ssrc.sd_srcname)) >=
		    MAX_LINK_NAMELEN) {
		die("invalid linkname");
		}
	}
	else
	{
		strlcpy(ssrc.sd_srcname, SFLOW_CMD_ALL,
		    sizeof (ssrc.sd_srcname));
	}
	if (typestr[0] == '\0')
		(void) strlcpy(ssrc.sd_typestr, SFLOW_CMD_ALL,
		    sizeof (ssrc.sd_typestr));
	else
		(void) strlcpy(ssrc.sd_typestr, typestr,
		    sizeof (ssrc.sd_typestr));

	err = sflow_door_dyncall(&ssrc,
	    sizeof (ssrc), (void **)&rval, &rval_size);

	/* buffer changed */
	if (err == EBADE) {
		printf("%d sources present:\n", rval->ssr_link_count);
		to_show = (sflow_src_fields_t *)(rval + 1);
		for (i = 0; i < rval->ssr_link_count; i++) {
			sflow_print_source(to_show+i);
		}
		printf("\n");
		return;
	} else if (err == 0) {
		assert(rval->ssr_link_count == 0);
		printf("Soure not found!\n");
		return;
	}

	if (err != 0)
		die("Failed: %d, %s", err, strerror(err));
}
