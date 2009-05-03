AC_DEFUN([COMPILER_HAS_FUNCTION_MACRO], [
AC_MSG_CHECKING(for __FUNCTION__ and __LINE__ macros)
AC_CACHE_VAL(ac_cv_compiler_has_function_macro,
[
AC_TRY_COMPILE(
[#include <stdio.h>],
[printf("%s:%d", __FUNCTION__, __LINE__);],
ac_cv_compiler_has_function_macro=yes,
ac_cv_compiler_has_function_macro=no)])
AC_MSG_RESULT($ac_cv_compiler_has_function_macro)
if test "$ac_cv_compiler_has_function_macro" = "yes"; then
  AC_DEFINE([HAVE_FUNCTION_MACRO], 1, [define if compiler has __FUNCTION__])
fi
])

