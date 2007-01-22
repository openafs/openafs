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

/* this code was pulled from src/afs/afs_util.c */

#include <osi/osi_impl.h>
#include <osi/osi_var_args.h>

#define OSI_MSG_BUF_SIZE 1024

void
osi_Msg_console(const char * msg, ...)
{
    struct file *fd = osi_NULL;
    osi_va_list args;
    char * buf;
    ssize_t len;
    ssize_t count;
    int code;

    /* cf. console_printf() in oncplus/kernext/nfs/serv/shared.c */
    code = fp_open("/dev/console", 
		   O_WRONLY | O_NOCTTY | O_NDELAY, 
		   0666, 
		   0, 
		   FP_SYS,
		   &fd);
    if (osi_compiler_expect_false(code != 0)) {
	goto done;
    }

    buf = osi_mem_alloc(OSI_MSG_BUF_SIZE);
    if (osi_compiler_expect_false(buf == osi_NULL)) {
	goto error;
    }

    osi_va_start(args, msg);
    /* XXX this may not link
     * still need to verify which printf methods are defined
     * for all of the modern AIX kernels... */
    vsnprintf(buf, len, msg, args);
    osi_va_end(args);

    len = strlen(buf);
    fp_write(fd, buf, len, 0, UIO_SYSSPACE, &count);
    fp_close(fd);

 done:
    return;

 error:
    if (buf) {
	osi_mem_free(buf, OSI_MSG_BUF_SIZE);
    }
    if (fd) {
	fp_close(fd);
    }
    goto done;
}
