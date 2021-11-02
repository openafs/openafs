AC_DEFUN([OPENAFS_AUTOHEADER_TOP],[
    AH_TOP([
#ifndef __AFSCONFIG_H
#define __AFSCONFIG_H 1])
])
AC_DEFUN([OPENAFS_AUTOHEADER_BOTTOM],[
    AH_BOTTOM([
#undef HAVE_RES_SEARCH
#undef STRUCT_SOCKADDR_HAS_SA_LEN
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# if ENDIANESS_IN_SYS_PARAM_H
#  ifndef KERNEL
#   include <sys/types.h>
#   include <sys/param.h>
#   if BYTE_ORDER == BIG_ENDIAN
#    define WORDS_BIGENDIAN 1
#   endif
#  else
#   if defined(AUTOCONF_FOUND_BIGENDIAN)
#    define WORDS_BIGENDIAN 1
#   else
#    undef WORDS_BIGENDIAN
#   endif
#  endif
# else
#  if defined(AUTOCONF_FOUND_BIGENDIAN)
#   define WORDS_BIGENDIAN 1
#  else
#   undef WORDS_BIGENDIAN
#  endif
# endif
#else
# if defined(__BIG_ENDIAN__)
#  define WORDS_BIGENDIAN 1
# else
#  undef WORDS_BIGENDIAN
# endif
#endif
#ifdef UKERNEL
/*
 * Always use 64-bit file offsets for UKERNEL code. Needed for UKERNEL stuff to
 * play nice with some other interfaces like FUSE. We technically only would
 * need to define this when building for such interfaces, but set it always to
 * try and reduce potential confusion.
 */
# define _FILE_OFFSET_BITS 64
# define AFS_CACHE_VNODE_PATH
#endif

#undef AFS_NAMEI_ENV
#undef BITMAP_LATER
#undef FAST_RESTART
#undef DEFINED_FOR_EACH_PROCESS
#undef DEFINED_PREV_TASK
#undef EXPORTED_SYS_CALL_TABLE
#undef EXPORTED_IA32_SYS_CALL_TABLE
#undef IRIX_HAS_MEM_FUNCS
#undef RECALC_SIGPENDING_TAKES_VOID
#undef STRUCT_FS_HAS_FS_ROLLED
#undef ssize_t
/* glue for RedHat kernel bug */
#undef ENABLE_REDHAT_BUILDSYS
#if defined(ENABLE_REDHAT_BUILDSYS) && defined(KERNEL) && defined(REDHAT_FIX)
# include "redhat-fix.h"
#endif

/*
 * AC_HEADER_TIME, which set TIME_WITH_SYS_TIME, has been marked as obsolete
 * prior to autoconf 2.64 and autoconf 2.70 flags its use with a warning.
 * However the external roken code still relies on the definition.
 */
#define TIME_WITH_SYS_TIME 1

#endif /* __AFSCONFIG_H */])
])
