# $Id$

include $(top_srcdir)/Makefile.am.common

afsincludedir = $(includedir)/afs
afsinclude_HEADERS = 	\
	stds.h		\
	icl.h		\
	afs_args.h	\
	venus.h		\
	debug.h		\
	param.h 	\
	afs_sysnames.h

afsbuild_HEADERZ=$(afsinclude_HEADERS)

param.h:	param.$(SYSNAME).h
	cp $(srcdir)/param.$(SYSNAME).h param.h
