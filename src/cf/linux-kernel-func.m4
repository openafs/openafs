AC_DEFUN([OPENAFS_LINUX_KERNEL_FUNC_CHECKS],[
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
AC_CHECK_LINUX_FUNC([ktime_get_coarse_real_ts64],
                    [#include <linux/ktime.h>],
                    [struct timespec64 *s;
                    ktime_get_coarse_real_ts64(s);])
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
])
