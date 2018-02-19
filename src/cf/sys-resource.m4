AC_DEFUN([OPENAFS_SYS_RESOURCE_CHECKS],[
OPENAFS_HAVE_STRUCT_FIELD(struct rusage, ru_idrss,
[#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif])
])
