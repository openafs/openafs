#
# $Id$
#

include $(top_srcdir)/Makefile.am.common

noinst_LTLIBRARIES = librxgkclient.la librxgkserver.la

noinst_PROGRAMS = test_client test_server

INCLUDES += -I. 			\
	-I$(srcdir)			\
	-I../include			\
	-I$(srcdir)/../include		\
	-I.. -I$(srcdir)/..		\
	$(INC_roken) 			\
	$(KRB5_INC_FLAGS)

librxgkclient_la_SOURCES =		\
	rxgk_locl.h			\
	rxgk_proto.h			\
	rxgk_clnt.c			\
	rxgk_info.c			\
	rxgk_crpc.c			\
	rxgk_crkrb.c			\
	rxgk_common.c			\
	rxgk_proto.ydr.c		\
	rxgk_proto.cs.c

librxgkserver_la_SOURCES =		\
	rxgk_locl.h			\
	rxgk_proto.h			\
	rxgk_serv.c			\
	rxgk_info.c			\
	rxgk_srpc.c			\
	rxgk_crkrb.c			\
	rxgk_common.c			\
	rxgk_proto.ydr.c		\
	rxgk_proto.ss.c

LIBYDR = 				\
	rxgk_proto.cs.c 		\
	rxgk_proto.ss.c 		\
	rxgk_proto.ydr.c		\
	rxgk_proto.h			\
	rxgk_proto.cs.h			\
	rxgk_proto.ss.h

LIBTYDR = 				\
	test.cs.h			\
	test.ss.h			\
	test.ss.c			\
	test.cs.c			\
	test.ydr.c			\
	test.h

common_LDADD = \
	../rx/librx.la \
	-L../lwp -llwp $(PLWP_LIB_FLAGS) \
	$(KRB5_LIB_FLAGS)

test_client_SOURCES = test_client.c test.cs.c
test_server_SOURCES = test_server.c test.ss.c

test_client_LDADD = librxgkclient.la $(common_LDADD)
test_server_LDADD = librxgkserver.la $(common_LDADD)

$(librxgkclient_la_OBJECTS) $(librxgkserver_la_OBJECTS): $(LIBYDR)

$(test_server_OBJECTS) $(test_client_OBJECTS): $(LIBTYDR)

$(LIBYDR): rxgk_proto.xg
	../ydr/ydr $(srcdir)/rxgk_proto.xg

$(LIBTYDR): test.xg
	../ydr/ydr $(srcdir)/test.xg

CLEANFILES = $(LIBYDR) $(LIBTYDR)

LDADD = $(KRB5_LIB_FLAGS)
