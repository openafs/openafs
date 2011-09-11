#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include "opr.h"

#ifdef AFS_NT40_ENV
void
opr_NTAbort(void)
{
    DebugBreak();
}
#endif


