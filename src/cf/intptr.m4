AC_DEFUN([AC_TYPE_INTPTR_T],
[
  AC_CHECK_TYPE([intptr_t],
    [AC_DEFINE([HAVE_INTPTR_T], 1,
       [Define to 1 if the system has the type `intptr_t'.])],
    [
	if test "$ac_cv_type_intptr_t" != yes; then
	  AC_MSG_CHECKING(for type equivalent to intptr_t)
	  case $ac_cv_sizeof_void_p in
	    2) openafs_cv_type_intptr_t=afs_int16 ;;
	    4) openafs_cv_type_intptr_t=afs_int32 ;;
	    8) openafs_cv_type_intptr_t=afs_int64 ;;
	    *) AC_MSG_ERROR(no equivalent for intptr_t);;
	  esac
	  AC_DEFINE_UNQUOTED([intptr_t], [$openafs_cv_type_intptr_t],
	  [Define to the type of a signed integer type wide enough to
             hold a pointer, if such a type exists, and if the system
             does not define it.])
	  AC_MSG_RESULT($openafs_cv_type_intptr_t)
	fi
     ])
])

AC_DEFUN([AC_TYPE_UINTPTR_T],
[
  AC_CHECK_TYPE([uintptr_t],
    [AC_DEFINE([HAVE_UINTPTR_T], 1,
       [Define to 1 if the system has the type `uintptr_t'.])],
    [
	if test "$ac_cv_type_uintptr_t" != yes; then
	  AC_MSG_CHECKING(for type equivalent to uintptr_t)
	  case $ac_cv_sizeof_void_p in 
	    2) openafs_cv_type_uintptr_t=afs_uint16 ;;
	    4) openafs_cv_type_uintptr_t=afs_uint32 ;;
	    8) openafs_cv_type_uintptr_t=afs_uint64 ;;
	    *) AC_MSG_ERROR(no equivalent for uintptr_t);;
	  esac
	  AC_DEFINE_UNQUOTED([uintptr_t], [$openafs_cv_type_uintptr_t],
	  [Define to the type of a signed integer type wide enough to
             hold a pointer, if such a type exists, and if the system
             does not define it.])
	  AC_MSG_RESULT($openafs_cv_type_uintptr_t)
	fi
  ])
])
