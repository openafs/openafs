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


AC_DEFUN([LINUX_COMPLETION_H_EXISTS], [
  AC_CACHE_CHECK([for linux/completion.h], [ac_cv_linux_completion_h_exists],
   [AC_TRY_KBUILD(
[#include <linux/version.h>
#include <linux/completion.h>],
[struct completion _c;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,8)
lose
#endif],
      ac_cv_linux_completion_h_exists=yes,
      ac_cv_linux_completion_h_exists=no)])
  AS_IF([test "x$ac_linux_completion_h_exists" = xyes],
	[AC_DEFINE(HAVE_LINUX_COMPLETION_H, 1,
		   [Define if your kernel has a usable linux/completion.h])])
])

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
[extern struct mm_struct init_mm;],
[void *address = &init_mm;
printk("%p\n", address);],
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


AC_DEFUN([LINUX_RECALC_SIGPENDING_ARG_TYPE], [
  AC_MSG_CHECKING([for recalc_sigpending arg type])
  AC_CACHE_VAL([ac_cv_linux_func_recalc_sigpending_takes_void], [
    AC_TRY_KBUILD(
[#include <linux/sched.h>],
[recalc_sigpending();],
      ac_cv_linux_func_recalc_sigpending_takes_void=yes,
      ac_cv_linux_func_recalc_sigpending_takes_void=no)])
  AC_MSG_RESULT($ac_cv_linux_func_recalc_sigpending_takes_void)])


AC_DEFUN([LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM], [
  AC_CACHE_CHECK([for signal->rlim in struct task_struct],
   [ac_cv_linux_sched_struct_task_struct_has_signal_rlim],
   [AC_TRY_KBUILD(
[#include <linux/sched.h>],
[struct task_struct _tsk;
printk("%d\n", _tsk.signal->rlim);],
      ac_cv_linux_struct_task_struct_has_signal_rlim=yes,
      ac_cv_linux_struct_task_struct_has_signal_rlim=no)])
    AS_IF([test "x$ac_cv_linux_struct_task_struct_has_signal_rlim" = "xyes"],
	  [AC_DEFINE(STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM, 1,
	             [define if your struct task_struct has signal->rlim])])
   ])


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
#ifdef HAVE_LINUX_FREEZER_H
#include <linux/freezer.h>
#endif],
[refrigerator(PF_FREEZE);],
      ac_cv_linux_func_refrigerator_takes_pf_freeze=yes,
      ac_cv_linux_func_refrigerator_takes_pf_freeze=no)])
  AC_MSG_RESULT($ac_cv_linux_func_refrigerator_takes_pf_freeze)
  if test "x$ac_cv_linux_func_refrigerator_takes_pf_freeze" = "xyes"; then
    AC_DEFINE([LINUX_REFRIGERATOR_TAKES_PF_FREEZE], 1, [define if your refrigerator takes PF_FREEZE])
  fi])

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


AC_DEFUN([LINUX_LINUX_KEYRING_SUPPORT], [
  AC_MSG_CHECKING([for linux kernel keyring support])
  AC_CACHE_VAL([ac_cv_linux_keyring_support], [
    AC_TRY_KBUILD(
[#include <linux/rwsem.h>
#ifdef HAVE_LINUX_KEY_TYPE_H
#include <linux/key-type.h>
#endif
#include <linux/key.h>
#include <linux/keyctl.h>],
[#ifdef CONFIG_KEYS
request_key(NULL, NULL, NULL);
#if !defined(KEY_POS_VIEW) || !defined(KEY_POS_SEARCH) || !defined(KEY_POS_SETATTR) 
#error "Your linux/key.h does not contain KEY_POS_VIEW or KEY_POS_SEARCH or KEY_POS_SETATTR"
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
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -Werror -Wno-pointer-arith"
    AC_TRY_KBUILD(
[#include <linux/rwsem.h>
#include <linux/key.h>
],
[struct task_struct *t=NULL;
(void) key_alloc(NULL, NULL, 0, 0, t, 0, 0);],
      ac_cv_key_alloc_needs_struct_task=yes,
      ac_cv_key_alloc_needs_struct_task=no)
    CPPFLAGS="$save_CPPFLAGS"])
  AC_MSG_RESULT($ac_cv_key_alloc_needs_struct_task)
  if test "x$ac_cv_key_alloc_needs_struct_task" = "xyes"; then
    AC_DEFINE([KEY_ALLOC_NEEDS_STRUCT_TASK], 1, [define if key_alloc takes a struct task *])
  fi])

AC_DEFUN([LINUX_KEY_ALLOC_NEEDS_CRED], [
  AC_MSG_CHECKING([if key_alloc() takes credentials])
  AC_CACHE_VAL([ac_cv_key_alloc_needs_cred], [
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -Werror -Wno-pointer-arith"
    AC_TRY_KBUILD(
[#include <linux/rwsem.h>
#include <linux/key.h>
],
[struct cred *c = NULL;
(void) key_alloc(NULL, NULL, 0, 0, c, 0, 0);],
      ac_cv_key_alloc_needs_cred=yes,
      ac_cv_key_alloc_needs_cred=no)
    CPPFLAGS="$save_CPPFLAGS"])
  AC_MSG_RESULT($ac_cv_key_alloc_needs_cred)
  if test "x$ac_cv_key_alloc_needs_cred" = "xyes"; then
    AC_DEFINE([KEY_ALLOC_NEEDS_CRED], 1, [define if key_alloc takes credentials])
  fi])

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

AC_DEFUN([LINUX_KMEM_CACHE_CREATE_CTOR_TAKES_VOID], [
  AC_MSG_CHECKING([whether kmem_cache_create constructor function takes a void pointer argument])
  AC_CACHE_VAL([ac_cv_linux_kmem_cache_create_ctor_takes_void], [
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -Werror"
    AC_TRY_KBUILD(
[#include <linux/slab.h>],
[void _ctor(void *v) { };
kmem_cache_create(NULL, 0, 0, 0, _ctor);],
      ac_cv_linux_kmem_cache_create_ctor_takes_void=yes,
      ac_cv_linux_kmem_cache_create_ctor_takes_void=no)
    CPPFLAGS="$save_CPPFLAGS"
])
  AC_MSG_RESULT($ac_cv_linux_kmem_cache_create_ctor_takes_void)
  if test "x$ac_cv_linux_kmem_cache_create_ctor_takes_void" = "xyes"; then
    AC_DEFINE([KMEM_CACHE_CTOR_TAKES_VOID], 1, [define if kmem_cache_create constructor function takes a single void pointer argument])
  fi])

AC_DEFUN([LINUX_HAVE_SVC_ADDR_IN], [
  AC_MSG_CHECKING([whether svc_addr_in exists])
  AC_CACHE_VAL([ac_cv_linux_have_svc_addr_in], [
    AC_TRY_KBUILD(
[#include <linux/sunrpc/svc.h>],
[svc_addr_in(NULL);],
      ac_cv_linux_have_svc_addr_in=yes,
      ac_cv_linux_have_svc_addr_in=no)])
  AC_MSG_RESULT($ac_cv_linux_have_svc_addr_in)])

dnl This function checks not just the existence of the splice functions,
dnl but also that the signature matches (they gained an extra argument
dnl around 2.6.17)
AC_DEFUN([LINUX_FS_STRUCT_FOP_HAS_SPLICE], [
  AC_CACHE_CHECK([for splice_write and splice_read in struct file_operations],
    [ac_cv_linux_fs_struct_fop_has_splice], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct file_operations _fop;
_fop.splice_write(NULL, NULL, NULL, 0, 0);
_fop.splice_read(NULL, NULL, NULL, 0, 0);],
      ac_cv_linux_fs_struct_fop_has_splice=yes,
      ac_cv_linux_fs_struct_fop_has_splice=no)])
  AS_IF([test "x$ac_cv_linux_fs_struct_fop_has_splice" = "xyes"],
        [AC_DEFINE(STRUCT_FILE_OPERATIONS_HAS_SPLICE, 1,
		   [define if struct file_operations has splice functions])])
  ])

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
[ extern int sysctl_check_table(int) __attribute__((weak));
sysctl_check_table(NULL);],
 ac_cv_linux_sysctl_table_checking=no,
 ac_cv_linux_sysctl_table_checking=yes)])
AC_MSG_RESULT($ac_cv_linux_sysctl_table_checking)])

AC_DEFUN([LINUX_EXPORTS_PROC_ROOT_FS], [
  AC_MSG_CHECKING([if proc_root_fs is defined and exported])
  AC_CACHE_VAL([ac_cv_linux_exports_proc_root_fs], [
    AC_TRY_KBUILD(
[#include <linux/proc_fs.h>],
[struct proc_dir_entry *p = proc_root_fs;],
      ac_cv_linux_exports_proc_root_fs=yes,
      ac_cv_linux_exports_proc_root_fs=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_proc_root_fs)
  if test "x$ac_cv_linux_exports_proc_root_fs" = "xyes"; then
    AC_DEFINE([EXPORTED_PROC_ROOT_FS], 1, [define if proc_root_fs is exported])
  fi])
 
AC_DEFUN([LINUX_D_PATH_TAKES_STRUCT_PATH], [
  AC_MSG_CHECKING([if d_path() takes a struct path argument])
  AC_CACHE_VAL([ac_cv_linux_d_path_takes_struct_path], [
    AC_TRY_KBUILD(
[#include <linux/dcache.h>],
[struct path *p;
d_path(p, NULL, 0);],
      ac_cv_linux_d_path_takes_struct_path=yes,
      ac_cv_linux_d_path_takes_struct_path=no)])
  AC_MSG_RESULT($ac_cv_linux_d_path_takes_struct_path)
  if test "x$ac_cv_linux_d_path_takes_struct_path" = "xyes"; then
    AC_DEFINE([D_PATH_TAKES_STRUCT_PATH], 1, [define if d_path() takes a struct path argument])
  fi])
 
AC_DEFUN([LINUX_NEW_EXPORT_OPS], [
  AC_MSG_CHECKING([if kernel uses new export ops])
  AC_CACHE_VAL([ac_cv_linux_new_export_ops], [
    AC_TRY_KBUILD(
[#include <linux/exportfs.h>],
[struct export_operations _eops;
_eops.fh_to_parent(NULL, NULL, 0, 0);],
      ac_cv_linux_new_export_ops=yes,
      ac_cv_linux_new_export_ops=no)])
  AC_MSG_RESULT($ac_cv_linux_new_export_ops)
  if test "x$ac_cv_linux_new_export_ops" = "xyes"; then
    AC_DEFINE([NEW_EXPORT_OPS], 1, [define if kernel uses new export ops])
  fi])
 

AC_DEFUN([LINUX_POSIX_TEST_LOCK_RETURNS_CONFLICT], [
  AC_MSG_CHECKING([if posix_test_lock returns a struct file_lock])
  AC_CACHE_VAL([ac_cv_linux_posix_test_lock_returns_conflict], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[struct file_lock *lock;
 struct file * file;
lock = posix_test_lock(file, lock);],
      ac_cv_linux_posix_test_lock_returns_conflict=yes,
      ac_cv_linux_posix_test_lock_returns_conflict=no)])
  AC_MSG_RESULT($ac_cv_linux_posix_test_lock_returns_conflict)
  if test "x$ac_cv_linux_posix_test_lock_returns_conflict" = "xyes"; then
    AC_DEFINE([POSIX_TEST_LOCK_RETURNS_CONFLICT], 1, [define if posix_test_lock returns the conflicting lock])
  fi])

AC_DEFUN([LINUX_POSIX_TEST_LOCK_CONFLICT_ARG], [
  AC_MSG_CHECKING([if posix_test_lock takes a conflict argument])
  AC_CACHE_VAL([ac_cv_linux_posix_test_lock_conflict_arg], [
    AC_TRY_KBUILD(
[#include <linux/fs.h>],
[ struct file_lock *lock;
  struct file *file;
  posix_test_lock(file, lock, lock);],
      ac_cv_linux_posix_test_lock_conflict_arg=yes,
      ac_cv_lonuc_posix_test_lock_conflict_arg=no)])
  AC_MSG_RESULT($ac_cv_linux_posix_test_lock_conflict_arg)
  if test "x$ac_cv_linux_posix_test_lock_conflict_arg" = "xyes"; then
    AC_DEFINE([POSIX_TEST_LOCK_CONFLICT_ARG], 1, [define if posix_test_lock takes a conflict argument])
  fi])

AC_DEFUN([LINUX_EXPORTS_KEY_TYPE_KEYRING], [
  AC_MSG_CHECKING([for exported key_type_keyring])
  AC_CACHE_VAL([ac_cv_linux_exports_key_type_keyring], [
    AC_TRY_KBUILD(
[
#ifdef HAVE_LINUX_KEY_TYPE_H
#include <linux/key-type.h>
#endif
#include <linux/key.h>],
[
printk("%s", key_type_keyring.name);
],
      ac_cv_linux_exports_key_type_keyring=yes,
      ac_cv_linux_exports_key_type_keyring=no)])
  AC_MSG_RESULT($ac_cv_linux_exports_key_type_keyring)
  if test "x$ac_cv_linux_exports_key_type_keyring" = "xyes"; then
    AC_DEFINE([EXPORTED_KEY_TYPE_KEYRING], 1, [define if key_type_keyring is exported])
  fi])

AC_DEFUN([LINUX_KEYS_HAVE_SESSION_TO_PARENT], [
  AC_MSG_CHECKING([for KEYCTL_SESSION_TO_PARENT])
  AC_CACHE_VAL([ac_cv_linux_have_session_to_parent], [
    AC_TRY_KBUILD(
[ #include <linux/keyctl.h>],
[ int i = KEYCTL_SESSION_TO_PARENT;],
      ac_cv_linux_have_session_to_parent=yes,
      ac_cv_linux_have_session_to_parent=no)])
  AC_MSG_RESULT($ac_cv_linux_have_session_to_parent)
  if test "x$ac_cv_linux_have_session_to_parent" = "xyes"; then
    AC_DEFINE([HAVE_SESSION_TO_PARENT], 1, [define if keyctl has the KEYCTL_SESSION_TO_PARENT function])
  fi])

AC_DEFUN([LINUX_HAVE_TRY_TO_FREEZE], [
  AC_MSG_CHECKING([for try_to_freeze])
  AC_CACHE_CHECK([for try_to_freeze], [ac_cv_linux_have_try_to_freeze],
    [AC_TRY_KBUILD(
[#include <linux/sched.h>
#ifdef HAVE_LINUX_FREEZER_H
#include <linux/freezer.h>
#endif],
[#ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
   try_to_freeze(PF_FREEZE);
#else
   try_to_freeze();
#endif
],
      ac_cv_linux_have_try_to_freeze=yes,
      ac_cv_linux_have_try_to_freeze=no)])
  AS_IF([test "x$ac_cv_linux_have_try_to_freeze" = "xyes"],
        [AC_DEFINE([HAVE_TRY_TO_FREEZE], 1,
                   [define if your kernel has the try_to_freeze function])])
 ])

