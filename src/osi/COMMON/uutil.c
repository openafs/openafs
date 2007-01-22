/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/* this code was pulled from rx_user.c */

#include <osi/osi_impl.h>
#include <afs/afsutil.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <osi/osi_var_args.h>


#if !defined(OSI_UTIL_OVERRIDE_PANIC)
void
osi_Panic(char *msg, int a1, int a2, int a3)
{
    (osi_Msg msg, a1, a2, a3);
    fflush(stderr);
    fflush(stdout);
    osi_Abort();
}
#endif /* !OSI_UTIL_OVERRIDE_PANIC */


/*
 * osi_util_AssertFail() -- used by the osi_Assert() macro.
 */

#if !defined(OSI_UTIL_OVERRIDE_ASSERT_FAIL)
void
osi_util_AssertFail(const char *expr, const char *file, int line)
{
    (osi_Msg "assertion failed: %s, file: %s, line %d\n", expr, file, line);
    osi_Panic("process aborting...", 0, 0, 0);
}
#endif /* !OSI_UTIL_OVERRIDE_ASSERT_FAIL */

#if !defined(OSI_UTIL_OVERRIDE_MSG_CONSOLE)
#define OSI_UTIL_MSG_BUF_LEN 4096
void 
osi_Msg_console(const char * msg, ...)
{
    osi_fd_t fd;
    osi_va_list args;
    char buf[OSI_UTIL_MSG_BUF_LEN];

    osi_va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    osi_va_end(args);
    buf[sizeof(buf)-1] = '\0';

    fd = open("/dev/console", O_RDWR);
    if (fd >= 0) {
	write(fd, buf, strlen(buf));
	close(fd);
    }
}
#endif /* !OSI_UTIL_OVERRIDE_MSG_CONSOLE */

#if !defined(OSI_UTIL_OVERRIDE_MSG_USER)
void
osi_Msg_user(const char * msg, ...)
{
    osi_va_list args;

    osi_va_start(args, msg);
    vfprintf(stderr, msg, args);
    osi_va_end(args);
}
#endif /* !OSI_UTIL_OVERRIDE_MSG_CONSOLE */
