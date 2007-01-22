dnl Copyright 2006-2007, Sine Nomine Associates and others.
dnl All Rights Reserved.
dnl 
dnl This software has been released under the terms of the IBM Public
dnl License.  For details, see the LICENSE file in the top-level source
dnl directory or online at http://www.openafs.org/dl/license10.html

dnl
dnl $Id$
dnl 

dnl
dnl The Solaris kernel uses a specialized type information
dnl ELF section called CTF.  The utilities to build these
dnl sections from DWARF debugging data are unfortunately
dnl private to the ON build consolidation.  However, you
dnl can download the SUNWonbld package from opensolaris.org
dnl and pass --with-ctf-tools=/opt/onbld/bin/`uname -p`
dnl
dnl Future releases of FreeBSD and Darwin will also support
dnl CTF as part of their DTrace ports.
dnl

AC_DEFUN([AC_CTF_TOOLS], [
SOLARIS_ONBLD_PATH="/opt/onbld/bin/`uname -p`"
_CTF_PATH="$ac_with_ctf_tools:$SOLARIS_ONBLD_PATH:$PATH"
AC_PATH_PROG(CTFCONVERT, ctfconvert, "", ${_CTF_PATH})
CTFCONVERT_FLAGS=""

AC_PATH_PROG(CTFMERGE, ctfmerge, "", ${_CTF_PATH})
CTFMERGE_FLAGS=""

])
