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
dnl - fatal_signal_pending introduced in 2.6.25
dnl - moved from linux/sched.h to linux/sched/signal.h in 4.11
AC_CHECK_LINUX_FUNC([fatal_signal_pending],
                    [#include <linux/sched.h>
                     #ifdef HAVE_LINUX_SCHED_SIGNAL_H
                     # include <linux/sched/signal.h>
                     #endif],
                    [fatal_signal_pending(NULL);])
AC_CHECK_LINUX_FUNC([file_dentry],
                    [#include <linux/fs.h>],
                    [struct file *f; file_dentry(f);])
AC_CHECK_LINUX_FUNC([find_task_by_pid],
                    [#include <linux/sched.h>],
                    [pid_t p; find_task_by_pid(p);])
AC_CHECK_LINUX_FUNC([generic_file_aio_read],
                    [#include <linux/fs.h>],
                    [generic_file_aio_read(NULL,NULL,0,0);])
dnl - linux 5.19 removed the flags parameter, need to test
dnl - with and without the flags parameter
AC_CHECK_LINUX_FUNC([grab_cache_page_write_begin_withflags],
                    [#include <linux/pagemap.h>],
                    [grab_cache_page_write_begin(NULL, 0, 0);])
AC_CHECK_LINUX_FUNC([grab_cache_page_write_begin_noflags],
                    [#include <linux/pagemap.h>],
                    [grab_cache_page_write_begin(NULL, 0);])
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
AC_CHECK_LINUX_FUNC([ktime_get_real_ts64],
                    [#include <linux/ktime.h>],
                    [struct timespec64 *s;
                    ktime_get_real_ts64(s);])
AC_CHECK_LINUX_FUNC([locks_lock_file_wait],
		    [#ifdef HAVE_LINUX_FILELOCK_H
		     # include <linux/filelock.h>
		     #else
		     # include <linux/fs.h>
		     #endif],
                    [locks_lock_file_wait(NULL, NULL);])
AC_CHECK_LINUX_FUNC([override_creds],
                    [#include <linux/cred.h>],
                    [override_creds(0);])
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
		    [rcu_read_lock();
		     rcu_read_unlock();])
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

dnl lru_cache_add_file added to Linux 2.6.28.
dnl                    removed in Linux 5.8
AC_CHECK_LINUX_FUNC([lru_cache_add_file],
                    [#include <linux/swap.h>],
                    [lru_cache_add_file(NULL);])

dnl Linux 4.6 introduced in_compat_syscall as replacement for is_compat_task
dnl for certain platforms.
AC_CHECK_LINUX_FUNC([in_compat_syscall],
                    [#include <linux/compat.h>],
                    [in_compat_syscall();])

dnl lru_cache_add exported in Linux 5.8
dnl    replaces lru_cache_add_file
dnl removed in linux 6.1.  folio_add_lru is a replacement
AC_CHECK_LINUX_FUNC([lru_cache_add],
                    [#include <linux/swap.h>],
                    [lru_cache_add(NULL);])

dnl Linux 5.16 added folio_add_lru as a replacement for
dnl lru_cache_add
AC_CHECK_LINUX_FUNC([folio_add_lru],
                    [#include <linux/swap.h>],
		    [folio_add_lru(NULL);])

dnl Linux 5.8 replaced kernel_setsockopt with helper functions
dnl e.g. ip_sock_set_mtu_discover, ip_sock_set_recverr
AC_CHECK_LINUX_FUNC([ip_sock_set],
                    [#include <net/ip.h>],
                    [ip_sock_set_mtu_discover(NULL, 0);])

dnl Linux 5.17 renamed complete_and_exit to kthread_complete_and_exit
AC_CHECK_LINUX_FUNC([kthread_complete_and_exit],
                    [#include <linux/kernel.h>
                     #include <linux/kthread.h>],
                    [kthread_complete_and_exit(0, 0);])

dnl Linux 6.0 removed add_to_page_cache.  It's replacement, filemap_add_folio,
dnl was added in 5.15 and is GPL-only, but has a NON-GPL wrapper called
dnl add_to_page_cache_lru.
dnl Note prior to 5.15, add_to_page_cache_lru was either not exported or
dnl or exported as GPL-only.
AC_CHECK_LINUX_FUNC([add_to_page_cache_lru],
                    [#include <linux/kernel.h>
                     #include <linux/pagemap.h>],
                    [add_to_page_cache_lru(NULL, NULL, 0, 0);])

dnl RHEL9.1 partially added support for address_space_operations' dirty_folio
dnl it did not add block_dirty_folio
AC_CHECK_LINUX_FUNC([block_dirty_folio],
    		    [#include <linux/kernel.h>
    		     #include <linux/buffer_head.h>],
    		    [block_dirty_folio(NULL, NULL);])

dnl Linux 6.5 removed the Linux function register_sysctl_table(), which
dnl was deprecated in Linux 6.3 in favor of register_sysctl() which was
dnl introduced in Linux 3.3
dnl Linux 6.6 changed the function register_sysctl to a macro that requires
dnl an array of ctl_table structures as its 2nd parameter
AC_CHECK_LINUX_FUNC([register_sysctl],
		    [#include <linux/kernel.h>
		     #include <linux/sysctl.h>],
		    [[static struct ctl_table cf_sysctl_table[1];
		    (void)register_sysctl(NULL, cf_sysctl_table);]])

dnl Linux 6.5 removed the file_operations method 'iterate'.  Filesystems should
dnl using the iterate_shared method (introduced in linux 4.6).  Linux 6.4
dnl provides a wrapper that can be used for filesystems that haven't fully
dnl converted to meet the iterate_shared requirements.

AC_CHECK_LINUX_FUNC([wrap_directory_iterator],
    		    [#include <linux/kernel.h>
    		     #include <linux/fs.h>],
    		    [(void)wrap_directory_iterator(NULL, NULL, NULL);])

dnl Linux 6.6 requires the use of a getter/setter for accessing a inode's
dnl ctime member.  Test for the setter inode_set_ctime
AC_CHECK_LINUX_FUNC([inode_set_ctime],
		    [#include <linux/fs.h>],
		    [inode_set_ctime(NULL, 0, 0);])

dnl Linux 6.7 requires the use of a getter/setter for accessing a inode's
dnl atime and mtime members.  Test for the setters.  Assummes that the
dnl getters are present if the setters are.
AC_CHECK_LINUX_FUNC([inode_atime_mtime_accessors],
		    [#include <linux/fs.h>],
		    [inode_set_atime(NULL, 0, 0);
		     inode_set_mtime(NULL, 0, 0);])

dnl Linux 6.8 removed the strlcpy() function.  We test to see if we can redefine
dnl a strlcpy() function.  We use a totally different function signature to
dnl to ensure that this fails when the kernel does provide strlcpy().
AC_CHECK_LINUX_FUNC([no_strlcpy],
		    [[#include <linux/string.h>
		     size_t strlcpy(char *d);
		     size_t strlcpy(char *d) { return strlen(d); }]],
		    [[static char buff[10];
		     size_t s;
		     s = strlcpy(buff);]])

dnl Linux 5.15 introduced filemap_alloc_folio() as a replacement for
dnl page_cache_alloc().  page_cache_alloc() was updated to become just a
dnl wrapper for filemap_alloc_folio().
dnl Linux 6.10 removed page_cache_alloc().
AC_CHECK_LINUX_FUNC([filemap_alloc_folio],
		    [#include <linux/kernel.h>
		     #include <linux/pagemap.h>],
		    [[static struct folio *folio;
		      folio = filemap_alloc_folio(0, 0);]])

dnl Linux 6.3 introduced filemap_splice_read(), which is a replacement for
dnl generic_file_splice_read(). But filemap_splice_read() at this point was not
dnl fully supported for use with all filesystems.
dnl
dnl Linux 6.5 removed generic_file_splice_read(), and changed
dnl filemap_splice_read() to be supported for all filesystems.
dnl
dnl To decide which function to use, we cannot simply check for
dnl filemap_splice_read(), since we need to avoid using filemap_splice_read()
dnl until it was fixed in Linux 6.5. We cannot simply check the Linux version
dnl number, since some downstream kernels fix filemap_splice_read() and remove
dnl generic_file_splice_read() before Linux 6.5 (e.g., openSUSE 15.6).
dnl
dnl To decide what function to use, we check if generic_file_splice_read() is
dnl missing. If generic_file_splice_read() is missing, use
dnl filemap_splice_read(); otherwise, use generic_file_splice_read().
dnl
dnl To check if generic_file_splice_read() is missing, define our own
dnl generic_file_splice_read() function with a conflicting signature. If we can
dnl define it, the real generic_file_splice_read() must be missing.
AC_CHECK_LINUX_FUNC([no_generic_file_splice_read],
		    [[#include <linux/fs.h>
		      char *generic_file_splice_read(char *p);
		      char *generic_file_splice_read(char *p) { return p + 1; }]],
		    [[static char buff[10], *ap;
		      ap = generic_file_splice_read(buff); ]])

dnl Linux 6.12 removed the PG_error flag from the page flags along with the
dnl associated functions ClearPageError() and SetPageError().  Check to see if
dnl these functions are present in the kernel.
dnl
dnl To check if ClearPageError() and SetPageError() are missing, define our own
dnl functions with the same name but with a conflicting signature. If we can
dnl define them, the real functions must be missing.
AC_CHECK_LINUX_FUNC([no_setpageerror],
		    [[#include <asm/page.h>
		      #include <linux/page-flags.h>
		      static inline char ClearPageError(char c) { return c;}
		      static inline char SetPageError(char c) { return c;}]],
		    [[static char r;
		      r = ClearPageError('x');
		      r = SetPageError('x');]])

dnl Linux 6.12 changed the signatgure for the address_space_operations members
dnl write_begin and write_end to use a folio instead of a page.
AC_CHECK_LINUX_FUNC([write_begin_end_folio],
		    [[#include <linux/fs.h>
		      static struct file *file;
		      static struct address_space *mapping;
		      static struct folio *foliop;
		      static void *fsdata;
		      static struct address_space_operations *aops;]],
		    [[aops->write_begin(file, mapping, 0, 0, &foliop, fsdata);
		      aops->write_end(file, mapping, 0, 0, 0, foliop, fsdata);]])

dnl Linux 5.16 added folio_wait_locked and updated wait_on_page_locked to be
dnl just a wrapper for folio_wait_locked.  Linux 6.15 removed wait_on_paged_locked
AC_CHECK_LINUX_FUNC([folio_wait_locked],
		    [[#include <linux/pagemap.h>
		      #include <linux/fs.h>]],
		    [[folio_wait_locked(NULL);]])

dnl Linux 5.16 added __filemap_get_folio to replace grab_cache_page_write_begin
dnl Linux 6.15 removed grab_cache_page_write_begin
AC_CHECK_LINUX_FUNC([filemap_get_folio],
                    [[#include <linux/pagemap.h>
                      #include <linux/fs.h>
                      static struct folio *folio;]],
                    [[folio = __filemap_get_folio(NULL, 0, 0, 0);]])

dnl Consequences - things which get set as a result of the
dnl                above tests
AS_IF([test "x$ac_cv_linux_func_d_alloc_anon" = "xno"],
    [AC_DEFINE([AFS_NONFSTRANS], 1,
        [define to disable the nfs translator])])
])
