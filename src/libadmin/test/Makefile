# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}


########################################################################
# build tests

AFSCPLIBS =\
	$(DESTDIR)/lib/afs/libafsadminutil.a \
	$(DESTDIR)/lib/afs/libclientadmin.a \
	$(DESTDIR)/lib/afs/libbosadmin.a \
	$(DESTDIR)/lib/afs/libvosadmin.a \
	$(DESTDIR)/lib/afs/libkasadmin.a \
	$(DESTDIR)/lib/afs/libptsadmin.a \
	$(DESTDIR)/lib/libafsauthent.a \
	$(DESTDIR)/lib/libafsrpc.a \
	$(DESTDIR)/lib/afs/libcmd.a  # static library

AFSCPOBJS =\
	bos.o \
	client.o \
	kas.o \
	pts.o \
	util.o \
	vos.o

afscp: afscp.o $(AFSCPOBJS) $(AFSCPLIBS)
	$(CC) $(CFLAGS) -o afscp afscp.o $(AFSCPOBJS) \
	-LDEST/lib/afs $(AFSCPLIBS) ${MTLIBS} $(XLIBS)

#######################################################################
install test tests: afscp

clean:
	$(RM) -f *.o afscp core
