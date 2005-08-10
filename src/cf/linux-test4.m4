AC_DEFUN([LINUX_COMPLETION_H_EXISTS], [
AC_MSG_CHECKING(for linux/completion.h existance)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_completion_h_exists,
[
AC_TRY_COMPILE(
[#include <linux/completion.h>
#include <linux/version.h>],
[struct completion _c;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,8)
lose
#endif
],
ac_cv_linux_completion_h_exists=yes,
ac_cv_linux_completion_h_exists=no)])
AC_MSG_RESULT($ac_cv_linux_completion_h_exists)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_DEFINES_FOR_EACH_PROCESS], [
AC_MSG_CHECKING(for defined for_each_process)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_defines_for_each_process,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[#ifndef for_each_process(p)
#error for_each_process not defined
#endif],
ac_cv_linux_defines_for_each_process=yes,
ac_cv_linux_defines_for_each_process=no)])
AC_MSG_RESULT($ac_cv_linux_defines_for_each_process)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_DEFINES_PREV_TASK], [
AC_MSG_CHECKING(for defined prev_task)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_defines_prev_task,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[#ifndef prev_task(p)
#error prev_task not defined
#endif],
ac_cv_linux_defines_prev_task=yes,
ac_cv_linux_defines_prev_task=no)])
AC_MSG_RESULT($ac_cv_linux_defines_prev_task)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_INIT_MM], [
AC_MSG_CHECKING(for exported init_mm)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_init_mm,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_init_mm
#error init_mm not exported
#endif],
ac_cv_linux_exports_init_mm=yes,
ac_cv_linux_exports_init_mm=no)])
AC_MSG_RESULT($ac_cv_linux_exports_init_mm)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_KALLSYMS_ADDRESS], [
AC_MSG_CHECKING(for exported kallsyms_address_to_symbol)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_kallsyms_address,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_kallsyms_address_to_symbol
#error kallsyms_address_to_symbol not exported
#endif],
ac_cv_linux_exports_kallsyms_address=yes,
ac_cv_linux_exports_kallsyms_address=no)])
AC_MSG_RESULT($ac_cv_linux_exports_kallsyms_address)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_KALLSYMS_SYMBOL], [
AC_MSG_CHECKING(for exported kallsyms_symbol_to_address)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_kallsyms_symbol,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_kallsyms_symbol_to_address
#error kallsyms_symbol_to_address not exported
#endif],
ac_cv_linux_exports_kallsyms_symbol=yes,
ac_cv_linux_exports_kallsyms_symbol=no)])
AC_MSG_RESULT($ac_cv_linux_exports_kallsyms_symbol)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_SYS_CALL_TABLE], [
AC_MSG_CHECKING(for exported sys_call_table)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_sys_call_table,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_sys_call_table
#error sys_call_table not exported
#endif],
ac_cv_linux_exports_sys_call_table=yes,
ac_cv_linux_exports_sys_call_table=no)])
AC_MSG_RESULT($ac_cv_linux_exports_sys_call_table)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_IA32_SYS_CALL_TABLE], [
AC_MSG_CHECKING(for exported ia32_sys_call_table)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_ia32_sys_call_table,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_ia32_sys_call_table
#error ia32_sys_call_table not exported
#endif],
ac_cv_linux_exports_ia32_sys_call_table=yes,
ac_cv_linux_exports_ia32_sys_call_table=no)])
AC_MSG_RESULT($ac_cv_linux_exports_ia32_sys_call_table)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_SYS_CHDIR], [
AC_MSG_CHECKING(for exported sys_chdir)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_sys_chdir,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_sys_chdir
#error sys_chdir not exported
#endif],
ac_cv_linux_exports_sys_chdir=yes,
ac_cv_linux_exports_sys_chdir=no)])
AC_MSG_RESULT($ac_cv_linux_exports_sys_chdir)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_SYS_CLOSE], [
AC_MSG_CHECKING(for exported sys_close)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_sys_close,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_sys_close
#error sys_close not exported
#endif],
ac_cv_linux_exports_sys_close=yes,
ac_cv_linux_exports_sys_close=no)])
AC_MSG_RESULT($ac_cv_linux_exports_sys_close)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_EXPORTS_SYS_WAIT4], [
AC_MSG_CHECKING(for exported sys_wait4)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_sys_wait4,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_sys_wait4
#error sys_wait4 not exported
#endif],
ac_cv_linux_exports_sys_wait4=yes,
ac_cv_linux_exports_sys_wait4=no)])
AC_MSG_RESULT($ac_cv_linux_exports_sys_wait4)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_CDEV], [
AC_MSG_CHECKING(for i_cdev in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_cdev, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_cdev);], 
ac_cv_linux_fs_struct_inode_has_i_cdev=yes,
ac_cv_linux_fs_struct_inode_has_i_cdev=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_cdev)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_DEVICES], [
AC_MSG_CHECKING(for i_devices in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_cdev, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_devices);], 
ac_cv_linux_fs_struct_inode_has_i_devices=yes,
ac_cv_linux_fs_struct_inode_has_i_devices=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_devices)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_DIRTY_DATA_BUFFERS], [
AC_MSG_CHECKING(for i_dirty_data_buffers in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_dirty_data_buffers);], 
ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers=yes,
ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_INOTIFY_LOCK], [
AC_MSG_CHECKING(for inotify_lock in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_inotify_lock, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.inotify_lock);], 
ac_cv_linux_fs_struct_inode_has_inotify_lock=yes,
ac_cv_linux_fs_struct_inode_has_inotify_lock=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_inotify_lock)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_INOTIFY_SEM], [
AC_MSG_CHECKING(for inotify_sem in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_inotify_sem, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%x\n", _inode.inotify_sem);], 
ac_cv_linux_fs_struct_inode_has_inotify_sem=yes,
ac_cv_linux_fs_struct_inode_has_inotify_sem=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_inotify_sem)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_MAPPING_OVERLOAD], [
AC_MSG_CHECKING(for i_mapping_overload in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_mapping_overload, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_mapping_overload);], 
ac_cv_linux_fs_struct_inode_has_i_mapping_overload=yes,
ac_cv_linux_fs_struct_inode_has_i_mapping_overload=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mapping_overload)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_MMAP_SHARED], [
AC_MSG_CHECKING(for i_mmap_shared in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_mmap_shared,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_mmap_shared);],
ac_cv_linux_fs_struct_inode_has_i_mmap_shared=yes,
ac_cv_linux_fs_struct_inode_has_i_mmap_shared=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mmap_shared)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_SECURITY], [
AC_MSG_CHECKING(for i_security in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_security, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_security);], 
ac_cv_linux_fs_struct_inode_has_i_security=yes,
ac_cv_linux_fs_struct_inode_has_i_security=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_security)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_SB_LIST], [
AC_MSG_CHECKING(for i_sb_list in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_sb_list, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_sb_list);], 
ac_cv_linux_fs_struct_inode_has_i_sb_list=yes,
ac_cv_linux_fs_struct_inode_has_i_sb_list=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_sb_list)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_RECALC_SIGPENDING_ARG_TYPE],[
AC_MSG_CHECKING(for recalc_sigpending arg type)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_func_recalc_sigpending_takes_void,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[recalc_sigpending();],
ac_cv_linux_func_recalc_sigpending_takes_void=yes,
ac_cv_linux_func_recalc_sigpending_takes_void=no)])
AC_MSG_RESULT($ac_cv_linux_func_recalc_sigpending_takes_void)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_PARENT], [
AC_MSG_CHECKING(for parent in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_parent,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.parent);],
ac_cv_linux_sched_struct_task_struct_has_parent=yes,
ac_cv_linux_sched_struct_task_struct_has_parent=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_parent)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_REAL_PARENT], [
AC_MSG_CHECKING(for real_parent in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_real_parent,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.real_parent);],
ac_cv_linux_sched_struct_task_struct_has_real_parent=yes,
ac_cv_linux_sched_struct_task_struct_has_real_parent=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_real_parent)
CPPFLAGS="$save_CPPFLAGS"])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIG], [
AC_MSG_CHECKING(for sig in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_sig,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.sig);],
ac_cv_linux_sched_struct_task_struct_has_sig=yes,
ac_cv_linux_sched_struct_task_struct_has_sig=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_sig)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGMASK_LOCK], [
AC_MSG_CHECKING(for sigmask_lock in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_sigmask_lock,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.sigmask_lock);],
ac_cv_linux_sched_struct_task_struct_has_sigmask_lock=yes,
ac_cv_linux_sched_struct_task_struct_has_sigmask_lock=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_sigmask_lock)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGHAND], [
AC_MSG_CHECKING(for sighand in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_sighand,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.sighand);],
ac_cv_linux_sched_struct_task_struct_has_sighand=yes,
ac_cv_linux_sched_struct_task_struct_has_sighand=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_sighand)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_RLIM], [
AC_MSG_CHECKING(for rlim in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_rlim,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.rlim);],
ac_cv_linux_sched_struct_task_struct_has_rlim=yes,
ac_cv_linux_sched_struct_task_struct_has_rlim=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_rlim)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM], [
AC_MSG_CHECKING(for signal->rlim in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_signal_rlim,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.signal->rlim);],
ac_cv_linux_sched_struct_task_struct_has_signal_rlim=yes,
ac_cv_linux_sched_struct_task_struct_has_signal_rlim=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_signal_rlim)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_EXIT_STATE], [
AC_MSG_CHECKING(for exit_state in struct task_struct)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_sched_struct_task_struct_has_exit_state,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printf("%d\n", _tsk.exit_state);],
ac_cv_linux_sched_struct_task_struct_has_exit_state=yes,
ac_cv_linux_sched_struct_task_struct_has_exit_state=no)])
AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_exit_state)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_FS_STRUCT_SUPER_HAS_ALLOC_INODE], [
AC_MSG_CHECKING(for alloc_inode in struct super_operations)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_super_has_alloc_inode, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct super_operations _super;
printf("%p\n", _super.alloc_inode);], 
ac_cv_linux_fs_struct_super_has_alloc_inode=yes,
ac_cv_linux_fs_struct_super_has_alloc_inode=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_super_has_alloc_inode)
CPPFLAGS="$save_CPPFLAGS"])
