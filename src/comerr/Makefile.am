# $Id$

include $(top_srcdir)/Makefile.am.common

YFLAGS = -d

afsincludedir = $(top_builddir)/include/afs
afsinclude_HEADERS = com_err.h error_table.h mit-sipb-cr.h
afsbuild_HEADERZ=$(afsinclude_HEADERS)

bin_PROGRAMS = compile_et

compile_et_SOURCES = compile_et.c parse.y lex.l 
compile_et_LDADD = -L../util -lutil

lib_LIBRARIES=libcom_err.a

libcom_err_a_SOURCES =  error_msg.c et_name.c com_err.c 
