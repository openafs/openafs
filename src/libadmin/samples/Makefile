#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}


########################################################################
# build tests

SAMPLEPROGS=\
	cm_client_config \
	cm_list_cells \
	cm_local_cell \
	cm_server_prefs \
	rxdebug_basic_stats \
	rxdebug_conns \
	rxdebug_peers \
	rxdebug_rx_stats \
	rxdebug_supported_stats \
	rxdebug_version \
	rxstat_clear_peer \
	rxstat_clear_process \
	rxstat_disable_peer \
	rxstat_disable_process \
	rxstat_enable_peer \
	rxstat_enable_process \
	rxstat_get_peer \
	rxstat_get_process \
	rxstat_get_version \
	rxstat_query_peer \
	rxstat_query_process

SAMPLELIBS =\
	${DESTDIR}/lib/afs/libafsadminutil.a \
	${DESTDIR}/lib/afs/libclientadmin.a \
	${DESTDIR}/lib/afs/libvosadmin.a \
	${DESTDIR}/lib/afs/libbosadmin.a \
	${DESTDIR}/lib/libafsauthent.a \
	${DESTDIR}/lib/libafsrpc.a

cm_client_config: cm_client_config.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

cm_list_cells: cm_list_cells.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

cm_local_cell: cm_local_cell.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

cm_server_prefs: cm_server_prefs.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxdebug_basic_stats: rxdebug_basic_stats.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxdebug_conns: rxdebug_conns.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxdebug_peers: rxdebug_peers.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxdebug_rx_stats: rxdebug_rx_stats.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxdebug_supported_stats: rxdebug_supported_stats.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxdebug_version: rxdebug_version.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_clear_peer: rxstat_clear_peer.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_clear_process: rxstat_clear_process.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_disable_peer: rxstat_disable_peer.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_disable_process: rxstat_disable_process.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_enable_peer: rxstat_enable_peer.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_enable_process: rxstat_enable_process.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_get_peer: rxstat_get_peer.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_get_process: rxstat_get_process.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_get_version: rxstat_get_version.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_query_peer: rxstat_query_peer.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

rxstat_query_process: rxstat_query_process.o $(SAMPLELIBS)
	$(CC) $(LDFLAGS) -o $@ $@.o $(SAMPLELIBS) $(MTLIBS) $(XLIBS)

#######################################################################
install test tests: $(SAMPLEPROGS)

clean:
	$(RM) -f *.o $(SAMPLEPROGS) core
