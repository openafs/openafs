/*
 * This is a compatibility header for <afs/afs_lock.h> in OpenAFS. OpenAFS used
 * to include a header <lock.h>, but it was renamed to <afs/afs_lock.h>. This
 * header exists just to allow older source code built against <lock.h> to
 * still compile, and should be removed when possible.
 */
#include <afs/afs_lock.h>
