#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


include ../../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -DAFS_PTHREAD_ENV -I${SRCDIR}include ${MT_CFLAGS}


########################################################################
# build tests

CFGTESTLIBS =\
	$(DESTDIR)/lib/afs/libafsadminutil.a \
	$(DESTDIR)/lib/afs/libclientadmin.a \
	$(DESTDIR)/lib/afs/libcfgadmin.a \
	$(DESTDIR)/lib/afs/libbosadmin.a \
	$(DESTDIR)/lib/afs/libvosadmin.a \
	$(DESTDIR)/lib/afs/libkasadmin.a \
	$(DESTDIR)/lib/afs/libptsadmin.a \
	$(DESTDIR)/lib/libafsauthent.a \
	$(DESTDIR)/lib/libafsrpc.a \
	$(DESTDIR)/lib/afs/libcmd.a  # static library

cfgtest: cfgtest.o $(CFGTESTLIBS)
	$(CC) $(CFLAGS) -o cfgtest cfgtest.o -LDEST/lib/afs $(CFGTESTLIBS) $(XLIBS)

#######################################################################
test tests: cfgtest

clean:
	$(RM) -f *.o cfgtest core
