# Determine the version of compiler used by the kernel kbuild process.
# Retrieve what was used for building the kernel itself.
# Determine what the current compiler is when building a kernel module.

# Check the compiler versions and warn if they are different.
# Minor version changes should be okay, but a different compiler or a major
# version change may end up with unexpected results.
AC_DEFUN([LINUX_KBUILD_COMPILER_CHECK],[
    AC_MSG_CHECKING([kernel compiler versions])
    _LINUX_GET_KBUILD_CONFIG_COMPILER_VERSION
    _LINUX_GET_KBUILD_COMPILER_VERSION
    AS_IF([test x"$LINUX_KERNEL_CC_TEXT" = x"$LINUX_KERNEL_CONFIG_CC_TEXT" || \
           ( test x"$LINUX_KERNEL_CC_TYPE" = x"$LINUX_KERNEL_CONFIG_CC_TYPE" && \
             test x"$LINUX_KERNEL_CC_MAJOR" = x"$LINUX_KERNEL_CONFIG_CC_MAJOR" && \
             test x"$LINUX_KERNEL_CC_MINOR" = x"$LINUX_KERNEL_CONFIG_CC_MINOR" )],
        [AC_MSG_RESULT([same])],
        [AC_MSG_NOTICE([kernel compiler versions are different, see config.log])
         echo "kernel compiled with : $LINUX_KERNEL_CONFIG_CC_TEXT" >&AS_MESSAGE_LOG_FD
         echo "kmod compiling with  : $LINUX_KERNEL_CC_TEXT" >&AS_MESSAGE_LOG_FD
         AS_IF([test x"$LINUX_KERNEL_CC_TYPE" != x"$LINUX_KERNEL_CONFIG_CC_TYPE"],
             [AC_MSG_WARN([Different compilers, kernel module may be unusable])])
         AS_IF([test x"$LINUX_KERNEL_CC_MAJOR" != x"$LINUX_KERNEL_CONFIG_CC_MAJOR"],
             [AC_MSG_WARN([Compiler major versions differ, kernel module may be unusable])])
         AC_MSG_RESULT([different])])
])

# Helper to parse compiler version text.
AC_DEFUN([_LINUX_PARSE_COMPILER_VERSION],
    [AS_VAR_SET([ac_linux_parse_compiler_type], ["unknown"])
     AS_VAR_SET([ac_linux_parse_compiler_ver], ["unknown"])
     AS_VAR_SET([ac_linux_parse_compiler_major], [0])
     AS_VAR_SET([ac_linux_parse_compiler_minor], [0])
     AS_IF([test "x$1" != "x"],
         [AS_IF([ echo "$1" | grep -qi "gcc"],
              [AS_VAR_SET([ac_linux_parse_compiler_type],[gcc])])
          AS_IF([ echo "$1" | grep -qi "clang"],
              [AS_VAR_SET([ac_linux_parse_compiler_type],[clang])])
          AS_VAR_SET([ac_linux_parse_compiler_ver],  [$(echo "$1" | grep -oE '[[0-9]]+\.[[0-9]]+' | head -n1)])
          AS_VAR_SET([ac_linux_parse_compiler_major],[$(echo "$ac_linux_parse_compiler_ver" | cut -d. -f1)])
          AS_VAR_SET([ac_linux_parse_compiler_minor],[$(echo "$ac_linux_parse_compiler_ver" | cut -d. -f2)])],
         [ echo "unable to parse the compiler version" >&AS_MESSAGE_LOG_FD ])

     AS_IF([test -z "$ac_linux_parse_compiler_major"],
         [AS_VAR_SET([ac_linux_parse_compiler_major], [0])])
     AS_IF([test -z "$ac_linux_parse_compiler_minor"],
         [AS_VAR_SET([ac_linux_parse_compiler_minor], [0])])
])

# Obtain the compiler version from the Linux configuration.
# Current linux versions place the compiler version within the .config file
# while older versions placed kernel version within the compile.h header file.
AC_DEFUN([_LINUX_GET_KBUILD_CONFIG_COMPILER_VERSION],
    [AS_IF([test -f "$LINUX_KERNEL_BUILD/.config"],
        [LINUX_KERNEL_CONFIG_CC_TEXT=$(grep "^CONFIG_CC_VERSION_TEXT=" "$LINUX_KERNEL_BUILD/.config" |\
                                       sed 's/^CONFIG_CC_VERSION_TEXT="\(.*\)"$/\1/')])

     AS_IF([test "x$LINUX_KERNEL_CONFIG_CC_TEXT" = "x"],
        [LINUX_KERNEL_CONFIG_CC_TEXT=$(find ${LINUX_KERNEL_BUILD}/include -name 'compile.h' -exec grep '#define LINUX_COMPILER' {} \; |\
                                             head -n1 | sed 's/#define LINUX_COMPILER "\(.*\)"/\1/')])

     AS_IF([test "x$LINUX_KERNEL_CONFIG_CC_TEXT" != "x"],
        [_LINUX_PARSE_COMPILER_VERSION([$LINUX_KERNEL_CONFIG_CC_TEXT])
         LINUX_KERNEL_CONFIG_CC_TYPE=$ac_linux_parse_compiler_type
         LINUX_KERNEL_CONFIG_CC_VER=$ac_linux_parse_compiler_ver
         LINUX_KERNEL_CONFIG_CC_MAJOR=$ac_linux_parse_compiler_major
         LINUX_KERNEL_CONFIG_CC_MINOR=$ac_linux_parse_compiler_minor
         AC_SUBST(LINUX_KERNEL_CONFIG_CC_TEXT)
         AC_SUBST(LINUX_KERNEL_CONFIG_CC_TYPE)
         AC_SUBST(LINUX_KERNEL_CONFIG_CC_VER)
         AC_SUBST(LINUX_KERNEL_CONFIG_CC_MAJOR)
         AC_SUBST(LINUX_KERNEL_CONFIG_CC_MINOR)],
        [echo "Unable to determine the version of compiler from kernel config" >&AS_MESSAGE_LOG_FD ])
])

# Determine the compiler version that is currently being used for the kernel.
# Need to run the compiler within the kernel's build process to obtain the
# the version of the compiler that will be used.
# Note, the kernel build process may perform its own version comparison.
AC_DEFUN([_LINUX_GET_KBUILD_COMPILER_VERSION],
    [rm -fr conftest.dir conftest.err conftest.out
     mkdir conftest.dir
     cat >conftest.dir/Makefile <<_ACEOF
COMPILER_VERSION := \$(shell \$(CC) --version 2>/dev/null | head -n 1)
\$(info COMPILER_VERSION=\$(COMPILER_VERSION))

_ACEOF

     make -C $LINUX_KERNEL_BUILD M=$TOP_OBJDIR/conftest.dir KBUILD_VERBOSE=1 >conftest.out 2>conftest.err
     AS_IF([test $? -ne 0 || grep -qvi -E 'warning:|error:' conftest.err ],
         [cat conftest.err >&AS_MESSAGE_LOG_FD])
     LINUX_KERNEL_CC_TEXT=$(grep 'COMPILER_VERSION=' conftest.out | head -n1 | cut -d= -f2-)

     AS_IF([test "x$LINUX_KERNEL_CC_TEXT" != "x"],
         [_LINUX_PARSE_COMPILER_VERSION([$LINUX_KERNEL_CC_TEXT])
          LINUX_KERNEL_CC_TYPE=$ac_linux_parse_compiler_type
          LINUX_KERNEL_CC_VER=$ac_linux_parse_compiler_ver
          LINUX_KERNEL_CC_MAJOR=$ac_linux_parse_compiler_major
          LINUX_KERNEL_CC_MINOR=$ac_linux_parse_compiler_minor
          AC_SUBST(LINUX_KERNEL_CC_TEXT)
          AC_SUBST(LINUX_KERNEL_CC_TYPE)
          AC_SUBST(LINUX_KERNEL_CC_VER)
          AC_SUBST(LINUX_KERNEL_CC_MAJOR)
          AC_SUBST(LINUX_KERNEL_CC_MINOR)],
         [echo "Unable to determine the version of compiler used to build the kernel module" >&AS_MESSAGE_LOG_FD ])
     rm -rf conftest.err conftest.dir conftest.out
])
