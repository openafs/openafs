#
# @(#)Makefile	1.2 87/11/09 3.9 RPCSRC
#
# Makefile for rpc protocol compiler
# Copyright (C) 1987, Sun Microsystems, Inc.
#
SHELL = /bin/sh
COMPONENT=rxgen
include ../config/Makefile.${SYS_NAME}

UKERNELDIR=../libuafs/
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

SRCS= rpc_main.c rpc_hout.c rpc_cout.c rpc_parse.c rpc_scan.c rpc_util.c \
	rpc_svcout.c rpc_clntout.c
HDRS= rpc_util.h rpc_parse.h rpc_scan.h
OBJS= rpc_main.o rpc_hout.o rpc_cout.o rpc_parse.o rpc_scan.o rpc_util.o \
	rpc_svcout.o rpc_clntout.o

GOAL=rxgen
CFLAGS = ${DBUG} -I. -I${DESTDIR}include ${XCFLAGS}

USNS=cellname
SD=../../../src/CML
VN=AFS_component_version_number
ID=``

include ../config/Makefile.version

$(VN).o: $(VN).c

.c.o:
	$(SPEW) $(CC) $(CFLAGS) -c $<

$(GOAL): $(OBJS) AFS_component_version_number.c
	$(SPEW) $(CC) $(CFLAGS) $(OBJS) $(XLIBS) -o $@

all:	$(GOAL)

system: install0

kinstall: install0

ukinstall webinstall: install0
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	$(INSTALL) rxgen_consts.h $(UKERNELDIR)afs

$(DESTDIR)/bin/$(GOAL): $(GOAL)
	@echo "Installing the Rx Protocol Compiler (rxgen)"
	$(INSTALL) rxgen $(DESTDIR)bin

install0: $(DESTDIR)/bin/$(GOAL)
	$(INSTALL) rxgen_consts.h $(DESTDIR)include/afs

install: install0 ukinstall

lint: $(SRCS) $(HDRS)
	lint $(SRCS)

clean:
	rm -f $(GOAL) $(OBJS) core AFS_component_version_number.c

depend: $(SRCS) $(HDRS)
	@${CC} ${CFLAGS} -M ${SRCS} > makedep
	@echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	@echo '$$r makedep' >>eddep
	@echo 'w' >>eddep
	@cp Makefile makefile.bak
	@ed - Makefile < eddep
	@rm eddep makedep makefile.bak

rpc_main.o: AFS_component_version_number.c
# DO NOT DELETE THIS LINE
rpc_main.o: rpc_main.c 
rpc_main.o: ./rpc_util.h
rpc_main.o: ./rpc_parse.h
rpc_main.o: ./rpc_scan.h
rpc_hout.o: rpc_hout.c
rpc_hout.o: ./rpc_util.h
rpc_hout.o: ./rpc_parse.h
rpc_cout.o: rpc_cout.c
rpc_cout.o: ./rpc_util.h
rpc_cout.o: ./rpc_parse.h
rpc_parse.o: rpc_parse.c
rpc_parse.o: ./rpc_util.h
rpc_parse.o: ./rpc_scan.h
rpc_parse.o: ./rpc_parse.h
rpc_scan.o: rpc_scan.c
rpc_scan.o: ./rpc_scan.h
rpc_scan.o: ./rpc_util.h
rpc_util.o: rpc_util.c
rpc_util.o: ./rpc_scan.h
rpc_util.o: ./rpc_parse.h
rpc_util.o: ./rpc_util.h
rpc_svcout.o: rpc_svcout.c
rpc_svcout.o: ./rpc_parse.h
rpc_svcout.o: ./rpc_util.h
rpc_clntout.o: rpc_clntout.c
rpc_clntout.o: ./rpc_parse.h
rpc_clntout.o: ./rpc_util.h
