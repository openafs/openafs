AC_DEFUN([OPENAFS_CTF_TOOLS_CHECKS],[

CTF_DEFAULT_PATH="$PATH:/usr/bin:/opt/onbld/bin/$HOST_CPU"

AC_ARG_WITH([ctf-tools],
        AS_HELP_STRING([--with-ctf-tools@<:@=DIR@:>@],
        [Location of the CTF tools]),
        [CTF_TOOLS="$withval"],
        [CTF_TOOLS="check"])

AS_CASE([$CTF_TOOLS],
        [check], [AC_PATH_PROG([CTFCONVERT], [ctfconvert], [], [$CTF_DEFAULT_PATH])
                  AC_PATH_PROG([CTFMERGE], [ctfmerge], [], [$CTF_DEFAULT_PATH])],

        [yes],   [AC_PATH_PROG([CTFCONVERT], [ctfconvert], [], [$CTF_DEFAULT_PATH])
                  AC_PATH_PROG([CTFMERGE], [ctfmerge], [], [$CTF_DEFAULT_PATH])
                  AS_IF([test "x$CTFCONVERT" = "x"], [AC_MSG_ERROR("ctfconvert not found")])
                  AS_IF([test "x$CTFMERGE" = "x"], [AC_MSG_ERROR("ctfmerge not found")])],

        [no],    [],

        [AC_PATH_PROG([CTFCONVERT], [ctfconvert], [], [$CTF_TOOLS])
         AC_PATH_PROG([CTFMERGE], [ctfmerge], [], [$CTF_TOOLS])
         AS_IF([test "x$CTFCONVERT" = "x"], [AC_MSG_ERROR("ctfconvert not found")])
         AS_IF([test "x$CTFMERGE" = "x"], [AC_MSG_ERROR("ctfmerge not found")])]
)

AC_ARG_VAR([CTFCONVERT], [Path to ctfconvert])
AC_ARG_VAR([CTFMERGE], [Path to ctfmerge])
])
