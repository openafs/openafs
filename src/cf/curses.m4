dnl _OPENAFS_CURSES_HEADERS
dnl abstracting out the preprocessor gunk to #include curses
AC_DEFUN([_OPENAFS_CURSES_HEADERS],[
#if defined(HAVE_NCURSES_H)
# include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
# include <ncurses/ncurses.h>
#elif defined(HAVE_CURSES_H)
# include <curses.h>
#endif
])

dnl OPENAFS_CURSES_LIB([ACTION-IF-SUCCESS], [ACTION_IF_FAILURE])
dnl check for a working curses library and set $LIB_curses if so
AC_DEFUN([OPENAFS_CURSES_LIB],
[AC_CACHE_VAL([openafs_cv_curses_lib],
   [save_LIBS="$LIBS"
    openafs_cv_curses_lib=
    AC_CHECK_LIB([ncurses], [initscr], [openafs_cv_curses_lib=-lncurses])
    AS_IF([test "x$openafs_cv_curses_lib" = x],
	  [AC_CHECK_LIB([Hcurses], [initscr], [openafs_cv_curses_lib=-lHcurses])])
    AS_IF([test "x$openafs_cv_curses_lib" = x],
	  [AC_CHECK_LIB([curses], [initscr], [openafs_cv_curses_lib=-lcurses])])
    LIBS="$save_LIBS"])
 LIB_curses="$openafs_cv_curses_lib"
 AC_SUBST(LIB_curses)
 AC_MSG_NOTICE([checking for curses library... ${openafs_cv_curses_lib:-none}])
 AS_IF([test "x$openafs_cv_curses_lib" != x], [$1], [$2])])

dnl _OPENAFS_CURSES_GETMAXYX_XTI([ACTION-IF-SUCCESS], [ACTION_IF_FAILURE])
dnl test for XTI-standard getmaxyx()
AC_DEFUN([_OPENAFS_CURSES_GETMAXYX_XTI],
[dnl paranoid check because of complex macros in various curses impls
 AC_CACHE_CHECK([getmaxyx macro], [openafs_cv_curses_getmaxyx],
   [save_LIBS="$LIBS"
    LIBS="$LIBS $LIB_curses"
    AC_TRY_LINK(_OPENAFS_CURSES_HEADERS,
		[int mx, my; initscr(); getmaxyx(stdscr, my, mx); endwin();],
		[openafs_cv_curses_getmaxyx=yes],
		[openafs_cv_curses_getmaxyx=no])
    LIBS="$save_LIBS"])
 AS_IF([test x"$openafs_cv_curses_getmaxyx" = xyes],
       [AC_DEFINE(HAVE_GETMAXYX, 1,
		  [define if your curses has the getmaxyx() macro])
	$1],
       [$2])])

dnl _OPENAFS_CURSES_GETMAXYX_BSD43([ACTION-IF-SUCCESS], [ACTION_IF_FAILURE])
dnl test for getmaxx() and getmaxy() as from later BSD curses
AC_DEFUN([_OPENAFS_CURSES_GETMAXYX_BSD43],
[OPENAFS_CURSES_LIB
 save_LIBS="$LIBS"
 LIBS="$LIBS $LIB_curses"
 _openafs_curses_bsd43=yes
 dnl possibly this may need to be done as above in some cases?
 AC_CHECK_FUNCS([getmaxx getmaxy], [], [_openafs_curses_bsd43=no])
 LIBS="$save_LIBS"
 AS_IF([$_openafs_curses_bsd43 = yes], [$1], [$2])])

dnl _OPENAFS_CURSES_GETMAXYX_BSDVI([ACTION-IF-SUCCESS], [ACTION_IF_FAILURE])
dnl test for getmaxx() and getmaxy() as from BSD curses as bodily ripped
dnl out of the original vi sources
AC_DEFUN([_OPENAFS_CURSES_GETMAXYX_BSDVI],
[OPENAFS_CURSES_LIB
 save_LIBS="$LIBS"
 LIBS="$LIBS $LIB_curses"
 _openafs_curses_bsdvi=yes
 AC_CHECK_MEMBERS([WINDOW._maxx WINDOW._maxy], [], [_openafs_curses_bsdvi=no],
		  _OPENAFS_CURSES_HEADERS)
 LIBS="$save_LIBS"
 AS_IF([$_openafs_curses_bsdvi = yes], [$1], [$2])])

dnl OPENAFS_CURSES_WINDOW_EXTENTS([ACTION-IF-SUCCESS], [ACTION_IF_FAILURE])
dnl check for standard getmaxyx macro. failing that, try the
dnl older getmaxx and getmaxy functions/macros; failing those,
dnl try the 4.2BSD _maxx and _maxy fields
AC_DEFUN([OPENAFS_CURSES_WINDOW_EXTENTS],
[OPENAFS_CURSES_LIB
 openafs_curses_extent=none
 _OPENAFS_CURSES_GETMAXYX_XTI([openafs_curses_extent=xti])
 AS_IF([test $openafs_curses_extent = none],
       [_OPENAFS_CURSES_GETMAXYX_BSD43([openafs_curses_extent=bsd])])
 AS_IF([test $openafs_curses_extent = none],
       [_OPENAFS_CURSES_GETMAXYX_BSDVI([openafs_curses_extent=vi])],)
 AS_IF([test $openafs_curses_extent != none], [$1], [$2])])

dnl The top level curses group
AC_DEFUN([OPENAFS_CURSES],
[AC_ARG_ENABLE([gtx],
    [AS_HELP_STRING([--disable-gtx], [disable gtx curses-based terminal tools])])
 AS_IF([test "x$enable_gtx" != "xno"],
       [OPENAFS_CURSES_LIB([], [enable_gtx=no])])
 AS_IF([test "x$enable_gtx" != "xno"],
       [OPENAFS_CURSES_WINDOW_EXTENTS([], [enable_gtx=no])])])
