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

#define TIMESTAMP_BUFFER_SIZE 26  /* including the null */
void
opr_AssertionFailed(const char *file, int line)
{
    char tdate[TIMESTAMP_BUFFER_SIZE];
    time_t when;
    struct tm tm;

    when = time(NULL);
    strftime(tdate, sizeof(tdate), "%a %b %d %H:%M:%S %Y",
	     localtime_r(&when, &tm));
    fprintf(stderr, "%s Assertion failed! file %s, line %d.\n", tdate, file,
	    line);
    fflush(stderr);
    opr_abort();
}
