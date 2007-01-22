/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_syscall.h>
#include <osi/COMMON/usyscall.h>
#include <afs/afs_args.h>

#if defined(OSI_DARWIN80_ENV)

osi_static osi_result
osi_darwin_ioctl_syscall(long call, long p1, long p2, long p3, long p4, int * rval);

osi_static osi_result
osi_darwin_ioctl_syscall_open(osi_fd_t *);
osi_static osi_result
osi_darwin_ioctl_syscall_close(osi_fd_t);

osi_result
osi_syscall_mux_call(long call, long p1, long p2, long p3, long p4, int * rval)
{
    osi_result res = OSI_OK;
    int code;

    if (osi_syscall_state.sys_online) {
	*rval = code = syscall(AFS_SYSCALL, call, p1, p2, p3, p4);
	if (osi_compiler_expect_false(code < 0)) {
	    if (errno == ENOSYS) {
		res = OSI_ERROR_NOSYS;
	    } else {
		res = OSI_FAIL;
	    }
	}
    } else if (osi_syscall_state.ioctl_online) {
	res = osi_darwin_ioctl_syscall(call, p1, p2, p3, p4, rval);
    } else {
	res = OSI_ERROR_NOSYS;
    }

    return res;
}

osi_static osi_result
osi_darwin_ioctl_syscall(long call, 
			 long p1, long p2, long p3, long p4, 
			 int * rval)
{
    osi_result res = OSI_OK;
    int code;
    struct afssysargs syscall_data;
    osi_fd_t fd;

    syscall_data.syscall = syscall;
    syscall_data.param1 = p1;
    syscall_data.param2 = p2;
    syscall_data.param3 = p3;
    syscall_data.param4 = p4;
    syscall_data.param5 = 0;
    syscall_data.param6 = 0;

    if (osi_syscall_state.ioctl_persistent) {
	fd = osi_syscall_state.ioctl_fd;
    } else {
	res = osi_darwin_ioctl_syscall_open(&fd);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto done;
	}
    }
    
    code = ioctl(fd, 
		 VIOC_SYSCALL, 
		 &syscall_data);

    if (!osi_syscall_state.ioctl_persistent) {
	(void)osi_darwin_ioctl_syscall_close(fd);
    }

    if (osi_compiler_expect_false(code)) {
	goto error;
    }

    *rval = syscall_data.retval;

 done:
    return res;

 error:
    if (errno == ENOSYS) {
	res = OSI_ERROR_NOSYS;
    } else {
	res = OSI_FAIL;
    }
    goto done;
}

osi_static osi_result
osi_darwin_ioctl_syscall_open(osi_fd_t * fd_out)
{
    osi_result res = OSI_OK;
    osi_fd_t fd;

    *fd_out = fd = open(SYSCALL_DEV_FNAME, O_RDWR);

    if (osi_compiler_expect_false(fd < 0)) {
	res = OSI_ERROR_NOSYS;
    }

    return res;
}

osi_static osi_result
osi_darwin_ioctl_syscall_close(osi_fd_t fd)
{
    if (osi_compiler_expect_true(fd >= 0)) {
	close(fd);
    }
    return OSI_OK;
}

osi_result
osi_syscall_user_os_PkgInit(void)
{
    osi_result res, code;
    osi_options_val_t opt;
    osi_fd_t fd;

    res = osi_config_options_Get(OSI_OPTION_IOCTL_SYSCALL_PERSISTENT_FD,
				 &opt);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto option_msg;
    }
    if (osi_compiler_expect_false(opt.type != OSI_OPTION_VAL_TYPE_BOOL)) {
	goto option_msg;
    }
    if (opt.val.v_bool == OSI_TRUE) {
	osi_syscall_state.ioctl_persistent = 1;
    }

 probe:
    code = osi_darwin_ioctl_syscall_open(&fd);
    if (OSI_RESULT_FAIL(code)) {
	goto done;
    }
    osi_syscall_state.ioctl_online = 1;

    if (osi_syscall_state.ioctl_persistent) {
	osi_syscall_state.ioctl_fd = fd;
    } else {
	osi_darwin_ioctl_syscall_close(fd);
    }

 done:
    return code;

 option_msg:
    (osi_Msg "%s: failed to get value for option IOCTL_SYSCALL_PERSISTENT_FD\n", 
     __osi_func__);
    goto probe;
}

osi_result
osi_syscall_user_os_PkgShutdown(void)
{
    osi_fd_t fd = osi_syscall_state.ioctl_fd;

    if (fd >= 0) {
	osi_syscall_state.ioctl_online = 0;
	osi_syscall_state.ioctl_fd = -1;
	osi_darwin_ioctl_syscall_close(fd);
    }
    return OSI_OK;
}

#endif /* OSI_DARWIN80_ENV */
