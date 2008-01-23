AC_DEFUN([LINUX_EXPORTS_TASKLIST_LOCK], [
  AC_MSG_CHECKING([for exported tasklist_lock])
  AC_CACHE_VAL([ac_cv_linux_exports_tasklist_lock], [
    AC_TRY_KBUILD(
[
#include <linux/sched.h>],
[
extern rwlock_t tasklist_lock __attribute__((weak)); 
read_lock(&tasklist_lock);
],
      ac_cv_linux_exports_tasklist_lock=yes,
      ac_cv_linux_exports_tasklist_lock=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_tasklist_lock)])


AC_DEFUN([LINUX_CONFIG_H_EXISTS], [
  AC_MSG_CHECKING([for linux/config.h existance])
  AC_CACHE_VAL([ac_cv_linux_config_h_exists], [
    AC_TRY_KBUILD(
[#include <linux/config.h>],
[return;],
      ac_cv_linux_config_h_exists=yes,
      ac_cv_linux_config_h_exists=no)])
  AC_MSG_RESULT($ac_cv_linux_config_h_exists)
  if test "x$ac_cv_linux_config_h_exists" = "xyes"; then
    AC_DEFINE([CONFIG_H_EXISTS], 1, [define if linux/config.h exists])
  fi])


AC_DEFUN([LINUX_COMPLETION_H_EXISTS], [
  AC_MSG_CHECKING([for linux/completion.h existance])
  AC_CACHE_VAL([ac_cv_linux_completion_h_exists], [
    AC_TRY_KBUILD(
[#include <linux/version.h>
#include <linux/completion.h>],
[struct completion _c;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,8)
lose
#endif],
      ac_cv_linux_completion_h_exists=yes,
      ac_cv_linux_completion_h_exists=no)])
  AC_MSG_RESULT($ac_cv_linux_completion_h_exists)])

AC_DEFUN([LINUX_EXPORTFS_H_EXISTS], [
  AC_MSG_CHECKING([for linux/exportfs.h existence])
  AC_CACHE_VAL([ac_cv_linux_exportfs_h_exists], [
    AC_TRY_KBUILD(
[#include <linux/exportfs.h>],
[return;],
      ac_cv_linux_exportfs_h_exists=yes,
      ac_cv_linux_exportfs_h_exists=no)])
  AC_MSG_RESULT($ac_cv_linux_exportfs_h_exists)])

AC_DEFUN([LINUX_DEFINES_FOR_EACH_PROCESS], [
  AC_MSG_CHECKING([for defined for_each_process])
  AC_CACHE_VAL([ac_cv_linux_defines_for_each_process], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[#ifndef for_each_process
#error for_each_process not defined
#endif],
      ac_cv_linux_defines_for_each_process=yes,
      ac_cv_linux_defines_for_each_process=no)])
  AC_MSG_RESULT($ac_cv_linux_defines_for_each_process)])


AC_DEFUN([LINUX_DEFINES_PREV_TASK], [
  AC_MSG_CHECKING([for defined prev_task])
  AC_CACHE_VAL([ac_cv_linux_defines_prev_task], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[#ifndef prev_task
#error prev_task not defined
#endif],
      ac_cv_linux_defines_prev_task=yes,
      ac_cv_linux_defines_prev_task=no)])
  AC_MSG_RESULT($ac_cv_linux_defines_prev_task)])


AC_DEFUN([LINUX_EXPORTS_INIT_MM], [
  AC_MSG_CHECKING([for exported init_mm])
  AC_CACHE_VAL([ac_cv_linux_exports_init_mm], [
    AC_TRY_KBUILD(
[#include <linux/modversions.h>],
[#ifndef __ver_init_mm
#error init_mm not exported
#endif],
      ac_cv_linux_exports_init_mm=yes,
      ac_cv_linux_exports_init_mm=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_init_mm)])


AC_DEFUN([LINUX_EXPORTS_KALLSYMS_ADDRESS], [
  AC_MSG_CHECKING([for exported kallsyms_address_to_symbol])
  AC_CACHE_VAL([ac_cv_linux_exports_kallsyms_address], [
    AC_TRY_KBUILD(
[#include <linux/modversions.h>],
[#ifndef __ver_kallsyms_address_to_symbol
#error kallsyms_address_to_symbol not exported
#endif],
      ac_cv_linux_exports_kallsyms_address=yes,
      ac_cv_linux_exports_kallsyms_address=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_kallsyms_address)])


AC_DEFUN([LINUX_EXPORTS_KALLSYMS_SYMBOL], [
  AC_MSG_CHECKING([for exported kallsyms_symbol_to_address])
  AC_CACHE_VAL([ac_cv_linux_exports_kallsyms_symbol], [
    AC_TRY_KBUILD(
[#include <linux/modversions.h>],
[#ifndef __ver_kallsyms_symbol_to_address
#error kallsyms_symbol_to_address not exported
#endif],
      ac_cv_linux_exports_kallsyms_symbol=yes,
      ac_cv_linux_exports_kallsyms_symbol=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_kallsyms_symbol)])

AC_DEFUN([LINUX_EXPORTS_SYS_CALL_TABLE], [
  AC_MSG_CHECKING([for exported sys_call_table])
  AC_CACHE_VAL([ac_cv_linux_exports_sys_call_table], [
    AC_TRY_KBUILD(
[#include <linux/modversions.h>],
[#ifndef __ver_sys_call_table
#error sys_call_table not exported
#endif],
      ac_cv_linux_exports_sys_call_table=yes,
      ac_cv_linux_exports_sys_call_table=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_sys_call_table)])


AC_DEFUN([LINUX_EXPORTS_IA32_SYS_CALL_TABLE], [
  AC_MSG_CHECKING([for exported ia32_sys_call_table])
  AC_CACHE_VAL([ac_cv_linux_exports_ia32_sys_call_table], [
    AC_TRY_KBUILD(
[#include <linux/modversions.h>],
[#ifndef __ver_ia32_sys_call_table
#error ia32_sys_call_table not exported
#endif],
      ac_cv_linux_exports_ia32_sys_call_table=yes,
      ac_cv_linux_exports_ia32_sys_call_table=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_ia32_sys_call_table)])


AC_DEFUN([LINUX_EXPORTS_SYS_CHDIR], [
  AC_MSG_CHECKING([for exported sys_chdir])
  AC_CACHE_VAL([ac_cv_linux_exports_sys_chdir], [
    AC_TRY_KBUILD(
[extern asmlinkage long sys_chdir(void) __attribute__((weak));],
[void *address = &sys_chdir;
printk("%p\n", address);],
      ac_cv_linux_exports_sys_chdir=yes,
      ac_cv_linux_exports_sys_chdir=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_sys_chdir)])


AC_DEFUN([LINUX_EXPORTS_SYS_CLOSE], [
  AC_MSG_CHECKING([for exported sys_close])
  AC_CACHE_VAL([ac_cv_linux_exports_sys_close], [
    AC_TRY_KBUILD(
[extern asmlinkage long sys_close(void) __attribute__((weak));],
[void *address = &sys_close;
printk("%p\n", address);],
      ac_cv_linux_exports_sys_close=yes,
      ac_cv_linux_exports_sys_close=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_sys_close)])


AC_DEFUN([LINUX_EXPORTS_SYS_OPEN], [
  AC_MSG_CHECKING([for exported sys_open])
  AC_CACHE_VAL([ac_cv_linux_exports_sys_open], [
    AC_TRY_KBUILD(
[extern asmlinkage long sys_open(void) __attribute__((weak));],
[void *address = &sys_open;
printk("%p\n", address);],
      ac_cv_linux_exports_sys_open=yes,
      ac_cv_linux_exports_sys_open=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_sys_open)])


AC_DEFUN([LINUX_EXPORTS_SYS_WAIT4], [
  AC_MSG_CHECKING([for exported sys_wait4])
  AC_CACHE_VAL([ac_cv_linux_exports_sys_wait4], [
    AC_TRY_KBUILD(
[extern asmlinkage long sys_wait4(void) __attribute__((weak));],
[void *address = &sys_wait4;
printk("%p\n", address);],
      ac_cv_linux_exports_sys_wait4=yes,
      ac_cv_linux_exports_sys_wait4=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_sys_wait4)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_BLKSIZE], [
  AC_MSG_CHECKING([for i_blksize in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_blksize], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_blksize);],
      ac_cv_linux_fs_struct_inode_has_i_blksize=yes,
      ac_cv_linux_fs_struct_inode_has_i_blksize=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_blksize)])

AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_BLKBITS], [
  AC_MSG_CHECKING([for i_blkbits in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_blkbits], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_blkbits);],
      ac_cv_linux_fs_struct_inode_has_i_blkbits=yes,
      ac_cv_linux_fs_struct_inode_has_i_blkbits=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_blkbits)
  if test "x$ac_cv_linux_fs_struct_inode_has_i_blkbits" = "xyes"; then
    AC_DEFINE(STRUCT_INODE_HAS_I_BLKBITS, 1, [define if your struct inode has i_blkbits])
  fi])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_CDEV], [
  AC_MSG_CHECKING([for i_cdev in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_cdev], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_cdev);],
      ac_cv_linux_fs_struct_inode_has_i_cdev=yes,
      ac_cv_linux_fs_struct_inode_has_i_cdev=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_cdev)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_DEVICES], [
  AC_MSG_CHECKING([for i_devices in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_devices], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_devices);],
      ac_cv_linux_fs_struct_inode_has_i_devices=yes,
      ac_cv_linux_fs_struct_inode_has_i_devices=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_devices)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_DIRTY_DATA_BUFFERS], [
  AC_MSG_CHECKING([for i_dirty_data_buffers in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_dirty_data_buffers);],
      ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers=yes,
      ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_INOTIFY_LOCK], [
  AC_MSG_CHECKING([for inotify_lock in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_inotify_lock], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.inotify_lock);],
      ac_cv_linux_fs_struct_inode_has_inotify_lock=yes,
      ac_cv_linux_fs_struct_inode_has_inotify_lock=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_inotify_lock)])

AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_INOTIFY_SEM], [
  AC_MSG_CHECKING([for inotify_sem in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_inotify_sem], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%x\n", _inode.inotify_sem);],
      ac_cv_linux_fs_struct_inode_has_inotify_sem=yes,
      ac_cv_linux_fs_struct_inode_has_inotify_sem=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_inotify_sem)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_MAPPING_OVERLOAD], [
  AC_MSG_CHECKING([for i_mapping_overload in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_mapping_overload], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_mapping_overload);],
      ac_cv_linux_fs_struct_inode_has_i_mapping_overload=yes,
      ac_cv_linux_fs_struct_inode_has_i_mapping_overload=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mapping_overload)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_MMAP_SHARED], [
  AC_MSG_CHECKING([for i_mmap_shared in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_mmap_shared], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_mmap_shared);],
      ac_cv_linux_fs_struct_inode_has_i_mmap_shared=yes,
      ac_cv_linux_fs_struct_inode_has_i_mmap_shared=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mmap_shared)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_MUTEX], [
  AC_MSG_CHECKING([for i_mutex in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_mutex], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_mutex);],
      ac_cv_linux_fs_struct_inode_has_i_mutex=yes,
      ac_cv_linux_fs_struct_inode_has_i_mutex=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mutex)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_SECURITY], [
  AC_MSG_CHECKING([for i_security in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_security], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_security);],
      ac_cv_linux_fs_struct_inode_has_i_security=yes,
      ac_cv_linux_fs_struct_inode_has_i_security=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_security)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_SB_LIST], [
  AC_MSG_CHECKING([for i_sb_list in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_sb_list], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_sb_list);],
      ac_cv_linux_fs_struct_inode_has_i_sb_list=yes,
      ac_cv_linux_fs_struct_inode_has_i_sb_list=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_sb_list)])


AC_DEFUN([LINUX_RECALC_SIGPENDING_ARG_TYPE], [
  AC_MSG_CHECKING([for recalc_sigpending arg type])
  AC_CACHE_VAL([ac_cv_linux_func_recalc_sigpending_takes_void], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[recalc_sigpending();],
      ac_cv_linux_func_recalc_sigpending_takes_void=yes,
      ac_cv_linux_func_recalc_sigpending_takes_void=no)])
  AC_MSG_RESULT($ac_cv_linux_func_recalc_sigpending_takes_void)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_PARENT], [
  AC_MSG_CHECKING([for parent in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_parent], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.parent);],
      ac_cv_linux_sched_struct_task_struct_has_parent=yes,
      ac_cv_linux_sched_struct_task_struct_has_parent=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_parent)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_TGID], [
  AC_MSG_CHECKING([for tgid in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_tgid], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.tgid);],
      ac_cv_linux_sched_struct_task_struct_has_tgid=yes,
      ac_cv_linux_sched_struct_task_struct_has_tgid=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_tgid)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_REAL_PARENT], [
  AC_MSG_CHECKING([for real_parent in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_real_parent], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.real_parent);],
      ac_cv_linux_sched_struct_task_struct_has_real_parent=yes,
      ac_cv_linux_sched_struct_task_struct_has_real_parent=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_real_parent)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIG], [
  AC_MSG_CHECKING([for sig in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_sig], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.sig);],
      ac_cv_linux_sched_struct_task_struct_has_sig=yes,
      ac_cv_linux_sched_struct_task_struct_has_sig=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_sig)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGMASK_LOCK], [
  AC_MSG_CHECKING([for sigmask_lock in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_sigmask_lock], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.sigmask_lock);],
      ac_cv_linux_sched_struct_task_struct_has_sigmask_lock=yes,
      ac_cv_linux_sched_struct_task_struct_has_sigmask_lock=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_sigmask_lock)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGHAND], [
  AC_MSG_CHECKING([for sighand in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_sighand], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.sighand);],
      ac_cv_linux_sched_struct_task_struct_has_sighand=yes,
      ac_cv_linux_sched_struct_task_struct_has_sighand=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_sighand)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_RLIM], [
  AC_MSG_CHECKING([for rlim in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_rlim], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.rlim);],
      ac_cv_linux_sched_struct_task_struct_has_rlim=yes,
      ac_cv_linux_sched_struct_task_struct_has_rlim=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_rlim)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM], [
  AC_MSG_CHECKING([for signal->rlim in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_signal_rlim], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.signal->rlim);],
      ac_cv_linux_sched_struct_task_struct_has_signal_rlim=yes,
      ac_cv_linux_sched_struct_task_struct_has_signal_rlim=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_signal_rlim)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_EXIT_STATE], [
  AC_MSG_CHECKING([for exit_state in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_exit_state], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.exit_state);],
      ac_cv_linux_sched_struct_task_struct_has_exit_state=yes,
      ac_cv_linux_sched_struct_task_struct_has_exit_state=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_exit_state)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_THREAD_INFO], [
  AC_MSG_CHECKING([for thread_info in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_thread_info], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.thread_info);],
      ac_cv_linux_sched_struct_task_struct_has_thread_info=yes,
      ac_cv_linux_sched_struct_task_struct_has_thread_info=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_thread_info)])


AC_DEFUN([LINUX_FS_STRUCT_SUPER_HAS_ALLOC_INODE], [
  AC_MSG_CHECKING([for alloc_inode in struct super_operations])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_super_has_alloc_inode], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct super_operations _super;
printk("%p\n", _super.alloc_inode);],
      ac_cv_linux_fs_struct_super_has_alloc_inode=yes,
      ac_cv_linux_fs_struct_super_has_alloc_inode=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_super_has_alloc_inode)])


AC_DEFUN([LINUX_KERNEL_POSIX_LOCK_FILE_WAIT_ARG], [
  AC_MSG_CHECKING([for 3rd argument in posix_lock_file found in new kernels])
  AC_CACHE_VAL([ac_cv_linux_kernel_posix_lock_file_wait_arg], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[posix_lock_file(0,0,0);],
      ac_cv_linux_kernel_posix_lock_file_wait_arg=yes,
      ac_cv_linux_kernel_posix_lock_file_wait_arg=no)])
  AC_MSG_RESULT($ac_cv_linux_kernel_posix_lock_file_wait_arg)])


AC_DEFUN([LINUX_KERNEL_SOCK_CREATE], [
  AC_MSG_CHECKING([for 5th argument in sock_create found in some SELinux kernels])
  AC_CACHE_VAL([ac_cv_linux_kernel_sock_create_v], [
    AC_TRY_KBUILD(
[#include <linux/net.h>],
[sock_create(0,0,0,0,0);],
      ac_cv_linux_kernel_sock_create_v=yes,
      ac_cv_linux_kernel_sock_create_v=no)])
  AC_MSG_RESULT($ac_cv_linux_kernel_sock_create_v)])


AC_DEFUN([LINUX_KERNEL_PAGE_FOLLOW_LINK], [
  AC_MSG_CHECKING([for page_follow_link_light vs page_follow_link])
  AC_CACHE_VAL([ac_cv_linux_kernel_page_follow_link], [
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -Werror-implicit-function-declaration"
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[page_follow_link(0,0);],
      ac_cv_linux_kernel_page_follow_link=yes,
      ac_cv_linux_kernel_page_follow_link=no)
    CPPFLAGS="$save_CPPFLAGS"])
  AC_MSG_RESULT($ac_cv_linux_kernel_page_follow_link)])


AC_DEFUN([LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_GFP_MASK], [
  AC_MSG_CHECKING([for gfp_mask in struct address_space])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_address_space_has_gfp_mask], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct address_space _a;
printk("%d\n", _a.gfp_mask);],
      ac_cv_linux_fs_struct_address_space_has_gfp_mask=yes,
      ac_cv_linux_fs_struct_address_space_has_gfp_mask=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_address_space_has_gfp_mask)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_BYTES], [
  AC_MSG_CHECKING([for i_bytes in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_bytes], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
printk("%d\n", _inode.i_bytes);],
      ac_cv_linux_fs_struct_inode_has_i_bytes=yes,
      ac_cv_linux_fs_struct_inode_has_i_bytes=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_bytes)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_ALLOC_SEM], [
  AC_MSG_CHECKING([for i_alloc_sem in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_alloc_sem], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _i;
printk("%x\n", _i.i_alloc_sem);],
      ac_cv_linux_fs_struct_inode_has_i_alloc_sem=yes,
      ac_cv_linux_fs_struct_inode_has_i_alloc_sem=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_alloc_sem)])


AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_TRUNCATE_SEM], [
  AC_MSG_CHECKING([for i_truncate_sem in struct inode])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_inode_has_i_truncate_sem], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _i;
printk("%x\n", _i.i_truncate_sem);],
      ac_cv_linux_fs_struct_inode_has_i_truncate_sem=yes,
      ac_cv_linux_fs_struct_inode_has_i_truncate_sem=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_truncate_sem)])


AC_DEFUN([LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK], [
  AC_MSG_CHECKING([for page_lock in struct address_space])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_address_space_has_page_lock], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct address_space _a_s;
printk("%x\n", _a_s.page_lock);],
      ac_cv_linux_fs_struct_address_space_has_page_lock=yes,
      ac_cv_linux_fs_struct_address_space_has_page_lock=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_address_space_has_page_lock)])


AC_DEFUN([LINUX_INODE_SETATTR_RETURN_TYPE], [
  AC_MSG_CHECKING([for inode_setattr return type])
  AC_CACHE_VAL([ac_cv_linux_func_inode_setattr_returns_int], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
struct iattr _iattr;
int i;
i = inode_setattr(&_inode, &_iattr);],
      ac_cv_linux_func_inode_setattr_returns_int=yes,
      ac_cv_linux_func_inode_setattr_returns_int=no)])
  AC_MSG_RESULT($ac_cv_linux_func_inode_setattr_returns_int)])


AC_DEFUN([LINUX_WRITE_INODE_RETURN_TYPE], [
  AC_MSG_CHECKING([for write_inode return type])
  AC_CACHE_VAL([ac_cv_linux_func_write_inode_returns_int], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
struct super_operations _sops;
int i;
i = _sops.write_inode(&_inode, 0);],
      ac_cv_linux_func_write_inode_returns_int=yes,
      ac_cv_linux_func_write_inode_returns_int=no)])
  AC_MSG_RESULT($ac_cv_linux_func_write_inode_returns_int)])


AC_DEFUN([LINUX_AOP_WRITEBACK_CONTROL], [
  AC_MSG_CHECKING([whether address_space_operations.writepage takes a writeback_control])
  AC_CACHE_VAL([ac_cv_linux_func_a_writepage_takes_writeback_control], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/writeback.h>],
[struct address_space_operations _aops;
struct page _page;
struct writeback_control _writeback_control;
(void)_aops.writepage(&_page, &_writeback_control);],
      ac_cv_linux_func_a_writepage_takes_writeback_control=yes,
      ac_cv_linux_func_a_writepage_takes_writeback_control=no)])
  AC_MSG_RESULT($ac_cv_linux_func_a_writepage_takes_writeback_control)])


AC_DEFUN([LINUX_REFRIGERATOR], [
  AC_MSG_CHECKING([whether refrigerator takes PF_FREEZE])
  AC_CACHE_VAL([ac_cv_linux_func_refrigerator_takes_pf_freeze], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>
#ifdef FREEZER_H_EXISTS
#include <linux/freezer.h>
#endif],
[refrigerator(PF_FREEZE);],
      ac_cv_linux_func_refrigerator_takes_pf_freeze=yes,
      ac_cv_linux_func_refrigerator_takes_pf_freeze=no)])
  AC_MSG_RESULT($ac_cv_linux_func_refrigerator_takes_pf_freeze)])


AC_DEFUN([LINUX_IOP_I_CREATE_TAKES_NAMEIDATA], [
  AC_MSG_CHECKING([whether inode_operations.create takes a nameidata])
  AC_CACHE_VAL([ac_cv_linux_func_i_create_takes_nameidata], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode;
struct dentry _dentry;
struct nameidata _nameidata;
(void)_inode.i_op->create(&_inode, &_dentry, 0, &_nameidata);],
      ac_cv_linux_func_i_create_takes_nameidata=yes,
      ac_cv_linux_func_i_create_takes_nameidata=no)])
  AC_MSG_RESULT($ac_cv_linux_func_i_create_takes_nameidata)])


AC_DEFUN([LINUX_IOP_I_LOOKUP_TAKES_NAMEIDATA], [
  AC_MSG_CHECKING([whether inode_operations.lookup takes a nameidata])
  AC_CACHE_VAL([ac_cv_linux_func_i_lookup_takes_nameidata], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode;
struct dentry _dentry;
struct nameidata _nameidata;
(void)_inode.i_op->lookup(&_inode, &_dentry, &_nameidata);],
      ac_cv_linux_func_i_lookup_takes_nameidata=yes,
      ac_cv_linux_func_i_lookup_takes_nameidata=no)])
  AC_MSG_RESULT($ac_cv_linux_func_i_lookup_takes_nameidata)])


AC_DEFUN([LINUX_IOP_I_PERMISSION_TAKES_NAMEIDATA], [
  AC_MSG_CHECKING([whether inode_operations.permission takes a nameidata])
  AC_CACHE_VAL([ac_cv_linux_func_i_permission_takes_nameidata], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode;
struct dentry _dentry;
struct nameidata _nameidata;
(void)_inode.i_op->permission(&_inode, 0, &_nameidata);],
      ac_cv_linux_func_i_permission_takes_nameidata=yes,
      ac_cv_linux_func_i_permission_takes_nameidata=no)])
  AC_MSG_RESULT($ac_cv_linux_func_i_permission_takes_nameidata)])


AC_DEFUN([LINUX_IOP_I_PUT_LINK_TAKES_COOKIE], [
  AC_MSG_CHECKING([whether inode_operations.put_link takes an opaque cookie])
  AC_CACHE_VAL([ac_cv_linux_func_i_put_link_takes_cookie], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode;
struct dentry _dentry;
struct nameidata _nameidata;
void *cookie;
(void)_inode.i_op->put_link(&_dentry, &_nameidata, cookie);],
      ac_cv_linux_func_i_put_link_takes_cookie=yes,
      ac_cv_linux_func_i_put_link_takes_cookie=no)])
  AC_MSG_RESULT($ac_cv_linux_func_i_put_link_takes_cookie)])


AC_DEFUN([LINUX_DOP_D_REVALIDATE_TAKES_NAMEIDATA], [
  AC_MSG_CHECKING([whether dentry_operations.d_revalidate takes a nameidata])
  AC_CACHE_VAL([ac_cv_linux_func_d_revalidate_takes_nameidata], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct dentry _dentry;
struct nameidata _nameidata;
(void)_dentry.d_op->d_revalidate(&_dentry, &_nameidata);],
      ac_cv_linux_func_d_revalidate_takes_nameidata=yes,
      ac_cv_linux_func_d_revalidate_takes_nameidata=no)])
  AC_MSG_RESULT($ac_cv_linux_func_d_revalidate_takes_nameidata)])

AC_DEFUN([LINUX_GET_SB_HAS_STRUCT_VFSMOUNT], [
  AC_MSG_CHECKING([for struct vfsmount * in get_sb_nodev()])
  AC_CACHE_VAL([ac_cv_linux_get_sb_has_struct_vfsmount], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[get_sb_nodev(0,0,0,0,0);],
      ac_cv_linux_get_sb_has_struct_vfsmount=yes,
      ac_cv_linux_get_sb_has_struct_vfsmount=no)])
  AC_MSG_RESULT($ac_cv_linux_get_sb_has_struct_vfsmount)])

AC_DEFUN([LINUX_STATFS_TAKES_DENTRY], [
  AC_MSG_CHECKING([for dentry in statfs])
  AC_CACHE_VAL([ac_cv_linux_statfs_takes_dentry], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>
#include <linux/statfs.h>],
[
extern int vfs_statfs(struct dentry *, struct kstatfs *);
],
      ac_cv_linux_statfs_takes_dentry=yes,
      ac_cv_linux_statfs_takes_dentry=no)])
  AC_MSG_RESULT($ac_cv_linux_statfs_takes_dentry)])


AC_DEFUN([LINUX_KEY_TYPE_H_EXISTS], [
  AC_MSG_CHECKING([for linux/key-type.h existance])
  AC_CACHE_VAL([ac_cv_linux_key_type_h_exists], [
    AC_TRY_KBUILD(
[#include <linux/key-type.h>],
[return;],
      ac_cv_linux_key_type_h_exists=yes,
      ac_cv_linux_key_type_h_exists=no)])
  AC_MSG_RESULT($ac_cv_linux_key_type_h_exists)
  if test "x$ac_cv_linux_key_type_h_exists" = "xyes"; then
    AC_DEFINE([KEY_TYPE_H_EXISTS], 1, [define if linux/key-type.h exists])
  fi])

AC_DEFUN([LINUX_LINUX_KEYRING_SUPPORT], [
  AC_MSG_CHECKING([for linux kernel keyring support])
  AC_CACHE_VAL([ac_cv_linux_keyring_support], [
    AC_TRY_KBUILD(
[#include <linux/rwsem.h>
#ifdef KEY_TYPE_H_EXISTS
#include <linux/key-type.h>
#endif
#include <linux/key.h>
#include <linux/keyctl.h>],
[#ifdef CONFIG_KEYS
request_key(NULL, NULL, NULL);
#if !defined(KEY_POS_VIEW) || !defined(KEY_POS_SEARCH)
#error "Your linux/key.h does not contain KEY_POS_VIEW or KEY_POS_SEARCH"
#endif
#else
#error rebuild your kernel with CONFIG_KEYS
#endif],
      ac_cv_linux_keyring_support=yes,
      ac_cv_linux_keyring_support=no)])
  AC_MSG_RESULT($ac_cv_linux_keyring_support)
  if test "x$ac_cv_linux_keyring_support" = "xyes"; then
    AC_DEFINE([LINUX_KEYRING_SUPPORT], 1, [define if your kernel has keyring support])
  fi])

AC_DEFUN([LINUX_KEY_ALLOC_NEEDS_STRUCT_TASK], [
  AC_MSG_CHECKING([if key_alloc() takes a struct task *])
  AC_CACHE_VAL([ac_cv_key_alloc_needs_struct_task], [
    AC_TRY_KBUILD(
[#include <linux/rwsem.h>
#include <linux/key.h>
],
[(void) key_alloc(NULL, NULL, 0, 0, NULL, 0, 0);],
      ac_cv_key_alloc_needs_struct_task=yes,
      ac_cv_key_alloc_needs_struct_task=no)])
  AC_MSG_RESULT($ac_cv_key_alloc_needs_struct_task)
  if test "x$ac_cv_key_alloc_needs_struct_task" = "xyes"; then
    AC_DEFINE([KEY_ALLOC_NEEDS_STRUCT_TASK], 1, [define if key_alloc takes a struct task *])
  fi])

AC_DEFUN([LINUX_DO_SYNC_READ], [
  AC_MSG_CHECKING([for linux do_sync_read()])
  AC_CACHE_VAL([ac_cv_linux_do_sync_read], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[do_sync_read(NULL, NULL, 0, NULL, 0);],
      ac_cv_linux_do_sync_read=no,
      ac_cv_linux_do_sync_read=maybe)
    if test "x$ac_cv_linux_do_sync_read" = "xmaybe"; then
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[do_sync_read(NULL, NULL, 0, NULL);],
      ac_cv_linux_do_sync_read=yes,
      ac_cv_linux_do_sync_read=no)])
    fi
  AC_MSG_RESULT($ac_cv_linux_do_sync_read)
  if test "x$ac_cv_linux_do_sync_read" = "xyes"; then
    AC_DEFINE([DO_SYNC_READ], 1, [define if your kernel has do_sync_read()])
  fi])

AC_DEFUN([LINUX_GENERIC_FILE_AIO_READ], [
  AC_MSG_CHECKING([for linux generic_file_aio_read()])
  AC_CACHE_VAL([ac_cv_linux_generic_file_aio_read], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[generic_file_aio_read(NULL, NULL, 0, 0, 0);],
      ac_cv_linux_generic_file_aio_read=no,
      ac_cv_linux_generic_file_aio_read=maybe)
    if test "x$ac_cv_linux_generic_file_aio_read" = "xmaybe"; then
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[generic_file_aio_read(NULL, NULL, 0, 0);],
      ac_cv_linux_generic_file_aio_read=yes,
      ac_cv_linux_generic_file_aio_read=no)])
    fi
  AC_MSG_RESULT($ac_cv_linux_generic_file_aio_read)
  if test "x$ac_cv_linux_generic_file_aio_read" = "xyes"; then
    AC_DEFINE([GENERIC_FILE_AIO_READ], 1, [define if your kernel has generic_file_aio_read()])
  fi])

AC_DEFUN([LINUX_FREEZER_H_EXISTS], [
  AC_MSG_CHECKING([for linux/freezer.h existance])
  AC_CACHE_VAL([ac_cv_linux_freezer_h_exists], [
    AC_TRY_KBUILD(
[#include <linux/freezer.h>],
[return;],
      ac_cv_linux_freezer_h_exists=yes,
      ac_cv_linux_freezer_h_exists=no)])
  AC_MSG_RESULT($ac_cv_linux_freezer_h_exists)
  if test "x$ac_cv_linux_freezer_h_exists" = "xyes"; then
    AC_DEFINE([FREEZER_H_EXISTS], 1, [define if linux/freezer.h exists])
  fi])

AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_TODO], [
  AC_MSG_CHECKING([for todo in struct task_struct])
  AC_CACHE_VAL([ac_cv_linux_sched_struct_task_struct_has_todo], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.todo);],
      ac_cv_linux_sched_struct_task_struct_has_todo=yes,
      ac_cv_linux_sched_struct_task_struct_has_todo=no)])
  AC_MSG_RESULT($ac_cv_linux_sched_struct_task_struct_has_todo)])

AC_DEFUN([LINUX_INIT_WORK_HAS_DATA], [
  AC_MSG_CHECKING([whether INIT_WORK has a _data argument])
  AC_CACHE_VAL([ac_cv_linux_init_work_has_data], [
    AC_TRY_KBUILD(
[#include <linux/kernel.h>
#include <linux/workqueue.h>],
[ 
void f(struct work_struct *w) {}
struct work_struct *w;
int *i;
INIT_WORK(w,f,i);],
      ac_cv_linux_init_work_has_data=yes,
      ac_cv_linux_init_work_has_data=no)])
  AC_MSG_RESULT($ac_cv_linux_init_work_has_data)])


AC_DEFUN([LINUX_FS_STRUCT_FOP_HAS_FLOCK], [
  AC_MSG_CHECKING([for flock in struct file_operations])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_fop_has_flock], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct file_operations _fop;
_fop.flock(NULL, 0, NULL);],
      ac_cv_linux_fs_struct_fop_has_flock=yes,
      ac_cv_linux_fs_struct_fop_has_flock=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_fop_has_flock)])

AC_DEFUN([LINUX_REGISTER_SYSCTL_TABLE_NOFLAG], [
  AC_MSG_CHECKING([whether register_sysctl_table has an insert_at_head flag argument])
  AC_CACHE_VAL([ac_cv_linux_register_sysctl_table_noflag], [
    AC_TRY_KBUILD(
[#include <linux/sysctl.h>],
[ctl_table *t;
register_sysctl_table (t);],
      ac_cv_linux_register_sysctl_table_noflag=yes,
      ac_cv_linux_register_sysctl_table_noflag=no)])
  AC_MSG_RESULT($ac_cv_linux_register_sysctl_table_noflag)])

AC_DEFUN([LINUX_FOP_F_FLUSH_TAKES_FL_OWNER_T], [
  AC_MSG_CHECKING([whether file_operations.flush takes a fl_owner_t])
  AC_CACHE_VAL([ac_cv_linux_func_f_flush_takes_fl_owner_t], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct inode _inode;
struct file _file;
fl_owner_t id;
(void)_inode.i_fop->flush(&_file, &id);],
      ac_cv_linux_func_f_flush_takes_fl_owner_t=yes,
      ac_cv_linux_func_f_flush_takes_fl_owner_t=no)])
  AC_MSG_RESULT($ac_cv_linux_func_f_flush_takes_fl_owner_t)])

AC_DEFUN([LINUX_HAVE_KMEM_CACHE_T], [
  AC_MSG_CHECKING([whether kmem_cache_t exists])
  AC_CACHE_VAL([ac_cv_linux_have_kmem_cache_t], [
    AC_TRY_KBUILD(
[#include <linux/slab.h>],
[kmem_cache_t *k;],
      ac_cv_linux_have_kmem_cache_t=yes,
      ac_cv_linux_have_kmem_cache_t=no)])
  AC_MSG_RESULT($ac_cv_linux_have_kmem_cache_t)])

AC_DEFUN([LINUX_KMEM_CACHE_CREATE_TAKES_DTOR], [
  AC_MSG_CHECKING([whether kmem_cache_create takes a destructor argument])
  AC_CACHE_VAL([ac_cv_linux_kmem_cache_create_takes_dtor], [
    AC_TRY_KBUILD(
[#include <linux/slab.h>],
[kmem_cache_create(NULL, 0, 0, 0, NULL, NULL);],
      ac_cv_linux_kmem_cache_create_takes_dtor=yes,
      ac_cv_linux_kmem_cache_create_takes_dtor=no)])
  AC_MSG_RESULT($ac_cv_linux_kmem_cache_create_takes_dtor)])

AC_DEFUN([LINUX_FS_STRUCT_FOP_HAS_SENDFILE], [
  AC_MSG_CHECKING([for sendfile in struct file_operations])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_fop_has_sendfile], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct file_operations _fop;
_fop.sendfile(NULL, NULL, 0, 0, NULL);],
      ac_cv_linux_fs_struct_fop_has_sendfile=yes,
      ac_cv_linux_fs_struct_fop_has_sendfile=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_fop_has_sendfile)])

AC_DEFUN([LINUX_HAVE_SVC_ADDR_IN], [
  AC_MSG_CHECKING([whether svc_addr_in exists])
  AC_CACHE_VAL([ac_cv_linux_have_svc_addr_in], [
    AC_TRY_KBUILD(
[#include <linux/sunrpc/svc.h>],
[svc_addr_in(NULL);],
      ac_cv_linux_have_svc_addr_in=yes,
      ac_cv_linux_have_svc_addr_in=no)])
  AC_MSG_RESULT($ac_cv_linux_have_svc_addr_in)])

AC_DEFUN([LINUX_FS_STRUCT_FOP_HAS_SPLICE], [
  AC_MSG_CHECKING([for splice_write and splice_read in struct file_operations])
  AC_CACHE_VAL([ac_cv_linux_fs_struct_fop_has_splice], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct file_operations _fop;
_fop.splice_write(NULL, NULL, NULL, 0, 0);
_fop.splice_read(NULL, NULL, NULL, 0, 0);],
      ac_cv_linux_fs_struct_fop_has_splice=yes,
      ac_cv_linux_fs_struct_fop_has_splice=no)])
  AC_MSG_RESULT($ac_cv_linux_fs_struct_fop_has_splice)])

AC_DEFUN([LINUX_HAVE_CURRENT_KERNEL_TIME], [
  AC_MSG_CHECKING([for current_kernel_time()])
  AC_CACHE_VAL([ac_cv_linux_have_current_kernel_time], [
    AC_TRY_KBUILD(
[#include <linux/time.h>],
[struct timespec s = current_kernel_time();],
      ac_cv_linux_have_current_kernel_time=yes,
      ac_cv_linux_have_current_kernel_time=no)])
  AC_MSG_RESULT($ac_cv_linux_have_current_kernel_time)])

AC_DEFUN([LINUX_KMEM_CACHE_INIT], [
  AC_MSG_CHECKING([for new kmem_cache init function parameters])
  AC_CACHE_VAL([ac_cv_linux_kmem_cache_init], [
    AC_TRY_KBUILD(
[#include <linux/slab.h>],
[extern struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                        unsigned long,
                        void (*)(struct kmem_cache *, void *));
return;],
      ac_cv_linux_kmem_cache_init=yes,
      ac_cv_linux_kmem_cache_init=no)])
  AC_MSG_RESULT($ac_cv_linux_kmem_cache_init)])

AC_DEFUN([LINUX_SYSCTL_TABLE_CHECKING], [
  AC_MSG_CHECKING([for sysctl table checking])
  AC_CACHE_VAL([ac_cv_linux_sysctl_table_checking], [
    AC_TRY_KBUILD(
[#include <linux/sysctl.h>],
[ sysctl_check_table(NULL);],
      ac_cv_linux_sysctl_table_checking=yes,
      ac_cv_linux_sysctl_table_checking=no)])
  AC_MSG_RESULT($ac_cv_linux_sysctl_table_checking)])

