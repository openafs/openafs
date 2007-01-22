dnl Copyright 2006-2007, Sine Nomine Associates and others.
dnl All Rights Reserved.
dnl 
dnl This software has been released under the terms of the IBM Public
dnl License.  For details, see the LICENSE file in the top-level source
dnl directory or online at http://www.openafs.org/dl/license10.html

dnl
dnl $Id$
dnl 

dnl check for various posix threads extensions
dnl just spinlocks at the moment

AC_DEFUN([PTHREADS_HAS_SPINLOCK], [
AC_MSG_CHECKING(for pthread_spin_init)
AC_CACHE_VAL(ac_cv_pthread_spin_init,
[
LIBS_save=$LIBS
LIBS="${LIBS} -lpthread"
AC_TRY_LINK([#include <pthread.h>],
[pthread_spinlock_t lock;
pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE);],
ac_cv_pthread_spin_init=yes,
ac_cv_pthread_spin_init=no)])
AC_MSG_RESULT($ac_cv_pthread_spin_init)
if test "$ac_cv_pthread_spin_init" = "yes"; then
  AC_DEFINE(HAVE_PTHREAD_SPINLOCK, 1, [define if atomic_inc_8() exists])
fi
LIBS=${LIBS_save}
])


AC_DEFUN([PTHREADS_EXTENSIONS], [
PTHREADS_HAS_SPINLOCK
])
