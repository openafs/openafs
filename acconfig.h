@BOTTOM@

#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

#undef HAVE_CONNECT
#undef HAVE_GETHOSTBYNAME
#undef HAVE_RES_SEARCH
#undef HAVE_SOCKET

#if ENDIANESS_IN_SYS_PARAM_H
#  include <sys/types.h>
#  include <sys/param.h>
#  if BYTE_ORDER == BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#  endif
#endif

#undef AFS_AFSDB_ENV
#undef AFS_NAMEI_ENV
#undef BOS_RESTRICTED_MODE
