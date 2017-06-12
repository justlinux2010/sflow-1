#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
#

PROG =		sflowd

COBJS =		expanding_array.o
COBJS +=	sflow_data_list.o
COBJS +=	sflow_util.o
COBJS +=	sflow_sender.o
COBJS +=	sflow_door.o
COBJS +=	sflow_protocol_branches.o
COBJS +=	sflow_protocol_leaf_flowsample.o
COBJS +=	sflow_protocol_leaf_ifgeneric.o
COBJS +=	sflow_protocol_leaf_flowstat.o
COBJS +=	sflow_protocol_leaf_hostphy.o
COBJS +=	sflow_protocol_leaf_hostv.o
COBJS +=	sflow_source.o
COBJS +=	sflow_config.o
COBJS +=	sflow_driver.o

OBJS =		$(COBJS)
SRCS =		$(COBJS:%.o=%.c)

MANIFEST =	sflow.xml
SVCMETHOD =	svc-sflowd

include		../../../Makefile.cmd

ROOTMANIFESTDIR =	$(ROOTSVCNETWORK)

LDLIBS +=	-lumem
LDLIBS +=	-ldlpi -lzonestat
LDLIBS +=	-lsocket -lnsl -lcmdutils -ldladm -ldlpi -lkstat -linetutil

#
# Instrument sflowd with CTF data to ease debugging.
#
include		../../../Makefile.ctf
CTFCONVERT_HOOK = && $(CTFCONVERT_O)
CTFMERGE_HOOK = && $(CTFMERGE) -L VERSION -o $@ $(OBJS)
$(OBJS) := CFLAGS += $(CTF_FLAGS)
$(PARFAIT_BUILD)__GNUC=
$(PARFAIT_BUILD)__GNUC64=

.KEEP_STATE_FILE: .make.state.$(MACH)

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS) $(CTFMERGE_HOOK)
	$(POST_PROCESS)

%.o:	%.c $(DTRACE_HEADER)
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

install: all $(ROOTLIBPROG) $(ROOTMANIFEST) $(ROOTSVCMETHOD)

check:	$(CHKMANIFEST)

clean:
	$(RM) $(OBJS) $(PROG)

lint:	lint_SRCS

include		../../../Makefile.targ
