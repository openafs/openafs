AC_DEFUN([OPENAFS_UUID_CHECKS],[
dnl Check for UUID library
AC_CHECK_HEADERS([uuid/uuid.h])
AC_CHECK_LIB(uuid, uuid_generate, LIBS_uuid="-luuid")
AC_CHECK_FUNCS([uuid_generate])
])
