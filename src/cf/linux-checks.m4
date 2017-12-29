AC_DEFUN([OPENAFS_LINUX_CHECKS],[
case $AFS_SYSNAME in *_linux* | *_umlinux*)

		# Add (sub-) architecture-specific paths needed by conftests
		case $AFS_SYSNAME  in
			*_umlinux26)
				UMLINUX26_FLAGS="-I$LINUX_KERNEL_PATH/arch/um/include"
				UMLINUX26_FLAGS="$UMLINUX26_FLAGS -I$LINUX_KERNEL_PATH/arch/um/kernel/tt/include"
 				UMLINUX26_FLAGS="$UMLINUX26_FLAGS -I$LINUX_KERNEL_PATH/arch/um/kernel/skas/include"
				CPPFLAGS="$CPPFLAGS $UMLINUX26_FLAGS"
		esac

		if test "x$enable_kernel_module" = "xyes"; then
		 if test "x$enable_debug_kernel" = "xno"; then
			LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fomit-frame-pointer"
		 fi
		 OPENAFS_GCC_SUPPORTS_MARCH
		 AC_SUBST(P5PLUS_KOPTS)
		 OPENAFS_GCC_NEEDS_NO_STRENGTH_REDUCE
		 OPENAFS_GCC_NEEDS_NO_STRICT_ALIASING
		 OPENAFS_GCC_SUPPORTS_NO_COMMON
		 OPENAFS_GCC_SUPPORTS_PIPE
		 AC_SUBST(LINUX_GCC_KOPTS)

		 dnl Setup the kernel build environment
		 LINUX_KBUILD_USES_EXTRA_CFLAGS
		 LINUX_KERNEL_COMPILE_WORKS

		 dnl Operation signature checks
		 AC_CHECK_LINUX_OPERATION([inode_operations], [follow_link], [no_nameidata],
					  [#include <linux/fs.h>],
					  [const char *],
					  [struct dentry *dentry, void **link_data])
		 AC_CHECK_LINUX_OPERATION([inode_operations], [put_link], [no_nameidata],
					  [#include <linux/fs.h>],
					  [void],
					  [struct inode *inode, void *link_data])
		 AC_CHECK_LINUX_OPERATION([inode_operations], [rename], [takes_flags],
					  [#include <linux/fs.h>],
					  [int],
					  [struct inode *oinode, struct dentry *odentry,
						struct inode *ninode, struct dentry *ndentry,
						unsigned int flags])

		 dnl Check for header files
		 AC_CHECK_LINUX_HEADER([cred.h])
		 AC_CHECK_LINUX_HEADER([config.h])
		 AC_CHECK_LINUX_HEADER([completion.h])
		 AC_CHECK_LINUX_HEADER([exportfs.h])
		 AC_CHECK_LINUX_HEADER([freezer.h])
		 AC_CHECK_LINUX_HEADER([key-type.h])
		 AC_CHECK_LINUX_HEADER([semaphore.h])
		 AC_CHECK_LINUX_HEADER([seq_file.h])
		 AC_CHECK_LINUX_HEADER([sched/signal.h])
		 AC_CHECK_LINUX_HEADER([uaccess.h])

		 dnl Type existence checks
		 AC_CHECK_LINUX_TYPE([struct vfs_path], [dcache.h])
		 AC_CHECK_LINUX_TYPE([kuid_t], [uidgid.h])

		 dnl Check for structure elements
		 AC_CHECK_LINUX_STRUCT([address_space], [backing_dev_info], [fs.h])
		 AC_CHECK_LINUX_STRUCT([address_space_operations],
				       [write_begin], [fs.h])
		 AC_CHECK_LINUX_STRUCT([backing_dev_info], [name],
				       [backing-dev.h])
		 AC_CHECK_LINUX_STRUCT([cred], [session_keyring], [cred.h])
		 AC_CHECK_LINUX_STRUCT([ctl_table], [ctl_name], [sysctl.h])
		 AC_CHECK_LINUX_STRUCT([dentry], [d_u.d_alias], [dcache.h])
		 AC_CHECK_LINUX_STRUCT([dentry_operations], [d_automount], [dcache.h])
		 AC_CHECK_LINUX_STRUCT([group_info], [gid], [cred.h])
		 AC_CHECK_LINUX_STRUCT([inode], [i_alloc_sem], [fs.h])
		 AC_CHECK_LINUX_STRUCT([inode], [i_blkbits], [fs.h])
		 AC_CHECK_LINUX_STRUCT([inode], [i_blksize], [fs.h])
		 AC_CHECK_LINUX_STRUCT([inode], [i_mutex], [fs.h])
		 AC_CHECK_LINUX_STRUCT([inode], [i_security], [fs.h])
		 AC_CHECK_LINUX_STRUCT([file], [f_path], [fs.h])
		 AC_CHECK_LINUX_STRUCT([file_operations], [flock], [fs.h])
		 AC_CHECK_LINUX_STRUCT([file_operations], [iterate], [fs.h])
		 AC_CHECK_LINUX_STRUCT([file_operations], [read_iter], [fs.h])
		 AC_CHECK_LINUX_STRUCT([file_operations], [sendfile], [fs.h])
		 AC_CHECK_LINUX_STRUCT([file_system_type], [mount], [fs.h])
		 AC_CHECK_LINUX_STRUCT([inode_operations], [truncate], [fs.h])
		 AC_CHECK_LINUX_STRUCT([inode_operations], [get_link], [fs.h])
		 AC_CHECK_LINUX_STRUCT([key], [payload.value], [key.h])
		 AC_CHECK_LINUX_STRUCT([key_type], [instantiate_prep], [key-type.h])
		 AC_CHECK_LINUX_STRUCT([key_type], [match_preparse], [key-type.h])
		 AC_CHECK_LINUX_STRUCT([key_type], [preparse], [key-type.h])
		 AC_CHECK_LINUX_STRUCT([msghdr], [msg_iter], [socket.h])
		 AC_CHECK_LINUX_STRUCT([nameidata], [path], [namei.h])
		 AC_CHECK_LINUX_STRUCT([proc_dir_entry], [owner], [proc_fs.h])
		 AC_CHECK_LINUX_STRUCT([super_block], [s_bdi], [fs.h])
		 AC_CHECK_LINUX_STRUCT([super_block], [s_d_op], [fs.h])
		 AC_CHECK_LINUX_STRUCT([super_operations], [alloc_inode],
				       [fs.h])
		 AC_CHECK_LINUX_STRUCT([super_operations], [evict_inode],
				       [fs.h])
                 AC_CHECK_LINUX_STRUCT([task_struct], [cred], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [exit_state], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [parent], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [real_parent], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [rlim], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [sig], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [sighand], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [sigmask_lock], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [tgid], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [thread_info], [sched.h])
		 AC_CHECK_LINUX_STRUCT([task_struct], [total_link_count], [sched.h])
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM

		 dnl Check for typed structure elements
		 AC_CHECK_LINUX_TYPED_STRUCT([read_descriptor_t],
				     	     [buf], [fs.h])

		 dnl Function existence checks

		 AC_CHECK_LINUX_FUNC([__vfs_write],
				     [#include <linux/fs.h>],
				     [__vfs_write(NULL, NULL, 0, NULL);])
		 AC_CHECK_LINUX_FUNC([kernel_write],
				     [#include <linux/fs.h>],
				     [kernel_write(NULL, NULL, 0, NULL);])
                 AC_CHECK_LINUX_FUNC([bdi_init],
				     [#include <linux/backing-dev.h>],
				     [bdi_init(NULL);])
                 AC_CHECK_LINUX_FUNC([super_setup_bdi],
                                     [#include <linux/backing-dev.h>],
                                     [struct super_block *sb;
				      super_setup_bdi(sb);])
                 AC_CHECK_LINUX_FUNC([PageChecked],
				     [#include <linux/mm.h>
#include <linux/page-flags.h>],
				     [struct page *_page;
                                      int bchecked = PageChecked(_page);])
                 AC_CHECK_LINUX_FUNC([PageFsMisc],
				     [#include <linux/mm.h>
#include <linux/page-flags.h>],
				     [struct page *_page;
                                      int bchecked = PageFsMisc(_page);])
		 AC_CHECK_LINUX_FUNC([clear_inode],
				     [#include <linux/fs.h>],
				     [clear_inode(NULL);])
		 AC_CHECK_LINUX_FUNC([current_kernel_time],
				     [#include <linux/time.h>],
			             [struct timespec s;
				      s = current_kernel_time();])
		 AC_CHECK_LINUX_FUNC([d_alloc_anon],
				     [#include <linux/fs.h>],
				     [d_alloc_anon(NULL);])
		 AC_CHECK_LINUX_FUNC([d_count],
				     [#include <linux/dcache.h>],
				     [d_count(NULL);])
		 AC_CHECK_LINUX_FUNC([d_make_root],
				     [#include <linux/fs.h>],
				     [d_make_root(NULL);])
		 AC_CHECK_LINUX_FUNC([do_sync_read],
				     [#include <linux/fs.h>],
				     [do_sync_read(NULL, NULL, 0, NULL);])
		 AC_CHECK_LINUX_FUNC([file_dentry],
				     [#include <linux/fs.h>],
				     [struct file *f; file_dentry(f);])
		 AC_CHECK_LINUX_FUNC([find_task_by_pid],
				     [#include <linux/sched.h>],
				     [pid_t p; find_task_by_pid(p);])
		 AC_CHECK_LINUX_FUNC([generic_file_aio_read],
				     [#include <linux/fs.h>],
				     [generic_file_aio_read(NULL,NULL,0,0);])
		 AC_CHECK_LINUX_FUNC([grab_cache_page_write_begin],
				     [#include <linux/pagemap.h>],
				     [grab_cache_page_write_begin(NULL, 0, 0);])
		 AC_CHECK_LINUX_FUNC([hlist_unhashed],
				     [#include <linux/list.h>],
				     [hlist_unhashed(0);])
		 AC_CHECK_LINUX_FUNC([ihold],
				     [#include <linux/fs.h>],
				     [ihold(NULL);])
		 AC_CHECK_LINUX_FUNC([i_size_read],
				     [#include <linux/fs.h>],
				     [i_size_read(NULL);])
		 AC_CHECK_LINUX_FUNC([inode_setattr],
				     [#include <linux/fs.h>],
				     [inode_setattr(NULL, NULL);])
		 AC_CHECK_LINUX_FUNC([iter_file_splice_write],
				     [#include <linux/fs.h>],
				     [iter_file_splice_write(NULL,NULL,NULL,0,0);])
		 AC_CHECK_LINUX_FUNC([kernel_setsockopt],
				     [#include <linux/net.h>],
				     [kernel_setsockopt(NULL, 0, 0, NULL, 0);])
		 AC_CHECK_LINUX_FUNC([locks_lock_file_wait],
				     [#include <linux/fs.h>],
				     [locks_lock_file_wait(NULL, NULL);])
		 AC_CHECK_LINUX_FUNC([page_follow_link],
				     [#include <linux/fs.h>],
				     [page_follow_link(0,0);])
		 AC_CHECK_LINUX_FUNC([page_get_link],
				     [#include <linux/fs.h>],
				     [page_get_link(0,0,0);])
		 AC_CHECK_LINUX_FUNC([page_offset],
				     [#include <linux/pagemap.h>],
				     [page_offset(NULL);])
		 AC_CHECK_LINUX_FUNC([pagevec_lru_add_file],
				     [#include <linux/pagevec.h>],
				     [__pagevec_lru_add_file(NULL);])
		 AC_CHECK_LINUX_FUNC([path_lookup],
				     [#include <linux/fs.h>
				      #include <linux/namei.h>],
				     [path_lookup(NULL, 0, NULL);])
		 AC_CHECK_LINUX_FUNC([proc_create],
				     [#include <linux/proc_fs.h>],
				     [proc_create(NULL, 0, NULL, NULL);])
		 AC_CHECK_LINUX_FUNC([rcu_read_lock],
				     [#include <linux/rcupdate.h>],
				     [rcu_read_lock();])
		 AC_CHECK_LINUX_FUNC([set_nlink],
				     [#include <linux/fs.h>],
				     [set_nlink(NULL, 1);])
		 AC_CHECK_LINUX_FUNC([setattr_prepare],
				     [#include <linux/fs.h>],
				     [setattr_prepare(NULL, NULL);])
		 AC_CHECK_LINUX_FUNC([sock_create_kern],
				     [#include <linux/net.h>],
				     [sock_create_kern(0, 0, 0, NULL);])
		 AC_CHECK_LINUX_FUNC([sock_create_kern_ns],
				     [#include <linux/net.h>],
				     [sock_create_kern(NULL, 0, 0, 0, NULL);])
		 AC_CHECK_LINUX_FUNC([splice_direct_to_actor],
				     [#include <linux/splice.h>],
				     [splice_direct_to_actor(NULL,NULL,NULL);])
		 AC_CHECK_LINUX_FUNC([default_file_splice_read],
				     [#include <linux/fs.h>],
				     [default_file_splice_read(NULL,NULL,NULL, 0, 0);])
		 AC_CHECK_LINUX_FUNC([svc_addr_in],
				     [#include <linux/sunrpc/svc.h>],
				     [svc_addr_in(NULL);])
		 AC_CHECK_LINUX_FUNC([zero_user_segments],
				     [#include <linux/highmem.h>],
				     [zero_user_segments(NULL, 0, 0, 0, 0);])
		 AC_CHECK_LINUX_FUNC([noop_fsync],
				     [#include <linux/fs.h>],
				     [void *address = &noop_fsync; printk("%p\n", address)];)
		 AC_CHECK_LINUX_FUNC([kthread_run],
				     [#include <linux/kernel.h>
				      #include <linux/kthread.h>],
				     [kthread_run(NULL, NULL, "test");])
		 AC_CHECK_LINUX_FUNC([inode_nohighmem],
				     [#include <linux/fs.h>],
				     [inode_nohighmem(NULL);])
		 AC_CHECK_LINUX_FUNC([inode_lock],
				     [#include <linux/fs.h>],
				     [inode_lock(NULL);])

		 dnl Consequences - things which get set as a result of the
		 dnl                above tests
		 AS_IF([test "x$ac_cv_linux_func_d_alloc_anon" = "xno"],
		       [AC_DEFINE([AFS_NONFSTRANS], 1,
				  [define to disable the nfs translator])])

		 dnl Assorted more complex tests
		 LINUX_AIO_NONVECTOR
		 LINUX_EXPORTS_PROC_ROOT_FS
                 LINUX_KMEM_CACHE_INIT
		 LINUX_HAVE_KMEM_CACHE_T
                 LINUX_KMEM_CACHE_CREATE_TAKES_DTOR
		 LINUX_KMEM_CACHE_CREATE_CTOR_TAKES_VOID
		 LINUX_D_PATH_TAKES_STRUCT_PATH
		 LINUX_NEW_EXPORT_OPS
		 LINUX_INODE_SETATTR_RETURN_TYPE
		 LINUX_IOP_I_CREATE_TAKES_NAMEIDATA
		 LINUX_IOP_I_LOOKUP_TAKES_NAMEIDATA
		 LINUX_IOP_I_PERMISSION_TAKES_FLAGS
	  	 LINUX_IOP_I_PERMISSION_TAKES_NAMEIDATA
	  	 LINUX_IOP_I_PUT_LINK_TAKES_COOKIE
		 LINUX_DOP_D_DELETE_TAKES_CONST
	  	 LINUX_DOP_D_REVALIDATE_TAKES_NAMEIDATA
	  	 LINUX_FOP_F_FLUSH_TAKES_FL_OWNER_T
	  	 LINUX_FOP_F_FSYNC_TAKES_DENTRY
		 LINUX_FOP_F_FSYNC_TAKES_RANGE
	  	 LINUX_AOP_WRITEBACK_CONTROL
		 LINUX_FS_STRUCT_FOP_HAS_SPLICE
		 LINUX_KERNEL_POSIX_LOCK_FILE_WAIT_ARG
		 LINUX_KERNEL_PAGEVEC_INIT_COLD_ARG
		 LINUX_POSIX_TEST_LOCK_RETURNS_CONFLICT
		 LINUX_POSIX_TEST_LOCK_CONFLICT_ARG
		 LINUX_KERNEL_SOCK_CREATE
		 LINUX_EXPORTS_KEY_TYPE_KEYRING
		 LINUX_KEYS_HAVE_SESSION_TO_PARENT
		 LINUX_NEED_RHCONFIG
		 LINUX_RECALC_SIGPENDING_ARG_TYPE
		 LINUX_EXPORTS_TASKLIST_LOCK
		 LINUX_GET_SB_HAS_STRUCT_VFSMOUNT
		 LINUX_STATFS_TAKES_DENTRY
		 LINUX_REFRIGERATOR
		 LINUX_HAVE_TRY_TO_FREEZE
		 LINUX_LINUX_KEYRING_SUPPORT
		 LINUX_KEY_ALLOC_NEEDS_STRUCT_TASK
		 LINUX_KEY_ALLOC_NEEDS_CRED
		 LINUX_INIT_WORK_HAS_DATA
		 LINUX_REGISTER_SYSCTL_TABLE_NOFLAG
		 LINUX_HAVE_DCACHE_LOCK
		 LINUX_D_COUNT_IS_INT
		 LINUX_IOP_GETATTR_TAKES_PATH_STRUCT
		 LINUX_IOP_MKDIR_TAKES_UMODE_T
		 LINUX_IOP_CREATE_TAKES_UMODE_T
		 LINUX_EXPORT_OP_ENCODE_FH_TAKES_INODES
		 LINUX_KMAP_ATOMIC_TAKES_NO_KM_TYPE
		 LINUX_DENTRY_OPEN_TAKES_PATH
		 LINUX_D_ALIAS_IS_HLIST
		 LINUX_HLIST_ITERATOR_NO_NODE
		 LINUX_IOP_I_CREATE_TAKES_BOOL
		 LINUX_DOP_D_REVALIDATE_TAKES_UNSIGNED
		 LINUX_IOP_LOOKUP_TAKES_UNSIGNED
		 LINUX_D_INVALIDATE_IS_VOID
		 LINUX_KERNEL_READ_OFFSET_IS_LAST

		 dnl If we are guaranteed that keyrings will work - that is
		 dnl  a) The kernel has keyrings enabled
		 dnl  b) The code is new enough to give us a key_type_keyring
		 dnl then we just disable syscall probing unless we've been
		 dnl told otherwise

		 AS_IF([test "$enable_linux_syscall_probing" = "maybe"],
		   [AS_IF([test "$ac_cv_linux_keyring_support" = "yes" -a "$ac_cv_linux_exports_key_type_keyring" = "yes"],
			  [enable_linux_syscall_probing="no"],
			  [enable_linux_syscall_probing="yes"])
                 ])

		 dnl Syscall probing needs a few tests of its own, and just
		 dnl won't work if the kernel doesn't export init_mm
		 AS_IF([test "$enable_linux_syscall_probing" = "yes"], [
                   LINUX_EXPORTS_INIT_MM
		   AS_IF([test "$ac_cv_linux_exports_init_mm" = "no"], [
                     AC_MSG_ERROR(
		       [Can't do syscall probing without exported init_mm])
                   ])
		   LINUX_EXPORTS_SYS_CHDIR
	           LINUX_EXPORTS_SYS_OPEN
		   AC_DEFINE(ENABLE_LINUX_SYSCALL_PROBING, 1,
			     [define to enable syscall table probes])
		 ])

		 dnl Packaging and SMP build
		 if test "x$with_linux_kernel_packaging" != "xyes" ; then
		   LINUX_WHICH_MODULES
		 else
		   AC_SUBST(MPS,'SP')
		 fi

		 dnl Syscall probing
                 if test "x$ac_cv_linux_config_modversions" = "xno" -o $AFS_SYSKVERS -ge 26; then
		   AS_IF([test "$enable_linux_syscall_probing" = "yes"], [
                     AC_MSG_WARN([Cannot determine sys_call_table status. assuming it isn't exported])
		   ])
                   ac_cv_linux_exports_sys_call_table=no
		   if test -f "$LINUX_KERNEL_PATH/include/asm/ia32_unistd.h"; then
		     ac_cv_linux_exports_ia32_sys_call_table=yes
		   fi
                 else
                   LINUX_EXPORTS_KALLSYMS_ADDRESS
                   LINUX_EXPORTS_KALLSYMS_SYMBOL
                   LINUX_EXPORTS_SYS_CALL_TABLE
                   LINUX_EXPORTS_IA32_SYS_CALL_TABLE
                   if test "x$ac_cv_linux_exports_sys_call_table" = "xno"; then
                         linux_syscall_method=none
                         if test "x$ac_cv_linux_exports_init_mm" = "xyes"; then
                            linux_syscall_method=scan
                            if test "x$ac_cv_linux_exports_kallsyms_address" = "xyes"; then
                               linux_syscall_method=scan_with_kallsyms_address
                            fi
                         fi
                         if test "x$ac_cv_linux_exports_kallsyms_symbol" = "xyes"; then
                            linux_syscall_method=kallsyms_symbol
                         fi
                         if test "x$linux_syscall_method" = "xnone"; then
			    AC_MSG_WARN([no available sys_call_table access method -- guessing scan])
                            linux_syscall_method=scan
                         fi
                   fi
                 fi
		 if test -f "$LINUX_KERNEL_PATH/include/linux/in_systm.h"; then
		  AC_DEFINE(HAVE_IN_SYSTM_H, 1, [define if you have in_systm.h header file])
	         fi
		 if test -f "$LINUX_KERNEL_PATH/include/linux/mm_inline.h"; then
		  AC_DEFINE(HAVE_MM_INLINE_H, 1, [define if you have mm_inline.h header file])
	         fi
		 if test "x$ac_cv_linux_func_page_get_link" = "xyes" -o "x$ac_cv_linux_func_i_put_link_takes_cookie" = "xyes"; then
		  AC_DEFINE(USABLE_KERNEL_PAGE_SYMLINK_CACHE, 1, [define if your kernel has a usable symlink cache API])
		 else
		  AC_MSG_WARN([your kernel does not have a usable symlink cache API])
		 fi
		 if test "x$ac_cv_linux_func_page_get_link" != "xyes" -a "x$ac_cv_linux_struct_inode_operations_has_get_link" = "xyes"; then
			AC_MSG_ERROR([Your kernel does not use follow_link - not supported without symlink cache API])
			exit 1
		 fi
                :
		fi
		if test "x$enable_linux_d_splice_alias_extra_iput" = xyes; then
		    AC_DEFINE(D_SPLICE_ALIAS_LEAK_ON_ERROR, 1, [for internal use])
		fi
dnl Linux-only, but just enable always.
		AC_DEFINE(AFS_CACHE_BYPASS, 1, [define to activate cache bypassing Unix client])
esac
])
