#/* Copyright (C) 1998  Transarc Corporation - All rights reserved */
#

include ../../config/Makefile.${SYS_NAME}

CFLAGS = $(DBUG) -I${SRCDIR}include ${XCFLAGS}

########################################################################
# build tests

LIBPMGT=DEST/lib/afs/libprocmgmt.a

pmgttest: pmgttest.o $(LIBPMGT)
	$(CC) $(CFLAGS) -o pmgttest pmgttest.o $(LIBPMGT) -lm $(XLIBS)

#######################################################################
test tests: pmgttest

clean:
	$(RM) -f *.o pmgttest core
