#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


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
