@BOTTOM@
#undef PACKAGE
#undef VERSION
 
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

#undef HAVE_CONNECT
#undef HAVE_GETHOSTBYNAME
#undef HAVE_RES_SEARCH
#undef HAVE_SOCKET

#if ENDIANESS_IN_SYS_PARAM_H
# ifndef KERNEL
#  include <sys/types.h>
#  include <sys/param.h>
#  if BYTE_ORDER == BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#  endif
# endif
#endif

#undef AFS_AFSDB_ENV
#undef AFS_NAMEI_ENV

#undef BITMAP_LATER
#undef BOS_RESTRICTED_MODE
#undef FAST_RESTART
#undef FULL_LISTVOL_SWITCH

#undef INODE_SETATTR_NOT_VOID
#undef STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK

/* glue for RedHat kernel bug */
#undef ENABLE_REDHAT_BUILDSYS

#if defined(ENABLE_REDHAT_BUILDSYS) && defined(KERNEL) && defined(REDHAT_FIX)
#include "redhat-fix.h"
#endif
