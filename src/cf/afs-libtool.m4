dnl Change the default 'nm' in some situations
AC_DEFUN([AFS_LT_PATH_NM],
  [AC_REQUIRE([AC_CANONICAL_HOST])
   AS_CASE([$host_os],

dnl Starting around Solaris 11.3, the default 'nm' tool starts reporting
dnl symbols with the 'C' code when given '-p'. Libtool currently cannot deal
dnl with this, and it fails to figure out how to extract symbols from object
dnl files (see libtool bug #22373). To work around this, try to use the GNU nm
dnl instead by default, which is either always or almost always available.
dnl libtool, of course, works with that just fine.
     [solaris2.11*],
     [AS_IF([test x"$NM" = x && test -x /usr/sfw/bin/gnm],
       [NM=/usr/sfw/bin/gnm])])])

dnl Our local wrapper around LT_INIT, to work around some bugs/limitations in
dnl libtool.
AC_DEFUN([AFS_LT_INIT],
  [AC_REQUIRE([AFS_LT_PATH_NM])

   LT_INIT

dnl If libtool cannot figure out how to extract symbol names from 'nm', then it
dnl will log a failure and lt_cv_sys_global_symbol_pipe will be unset, but it
dnl will not cause configure to bail out. Later on, when we try to link with
dnl libtool, it will cause a very confusing error message (see libtool bug
dnl #20947).  To try and avoid that bad error message, try to catch that
dnl situation here, closer to when the error actually occurs.
   AS_IF([test x"$lt_cv_sys_global_symbol_pipe" = x],
     [AC_MSG_ERROR([libtool cannot figure out how to extract symbol names; look above for failures involving 'nm'])])])
