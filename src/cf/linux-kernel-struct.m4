AC_DEFUN([OPENAFS_LINUX_KERNEL_STRUCT_CHECKS],[
dnl Check for structure elements
AC_CHECK_LINUX_STRUCT([address_space], [backing_dev_info], [fs.h])
AC_CHECK_LINUX_STRUCT([address_space_operations],
                      [write_begin], [fs.h])
dnl linux 5.18 replaced set_page_dirty with dirty_folio
AC_CHECK_LINUX_STRUCT([address_space_operations], [dirty_folio], [fs.h])
dnl linux 5.18 replaced readpages with readahead (introduced in 5.8)
AC_CHECK_LINUX_STRUCT([address_space_operations], [readahead], [fs.h])
dnl linux 5.18 replaced readpage with read_folio
AC_CHECK_LINUX_STRUCT([address_space_operations], [read_folio], [fs.h])
AC_CHECK_LINUX_STRUCT([backing_dev_info], [name],
                      [backing-dev.h])
AC_CHECK_LINUX_STRUCT([cred], [session_keyring], [cred.h])
AC_CHECK_LINUX_STRUCT([ctl_table], [ctl_name], [sysctl.h])
AC_CHECK_LINUX_STRUCT([dentry], [d_u.d_alias], [dcache.h])
dnl linux 2.6.16 moved dentry->d_child to dentry->d_u.d_child
dnl linux 3.19 moved it back to dentry->d_child
AC_CHECK_LINUX_STRUCT([dentry], [d_u.d_child], [dcache.h])
dnl linux 6.8 uses hlist for dentry children and renamed
dnl d_subdirs/d_child to d_childern/d_sib
AC_CHECK_LINUX_STRUCT([dentry], [d_children], [dcache.h])
AC_CHECK_LINUX_STRUCT([dentry_operations], [d_automount], [dcache.h])
AC_CHECK_LINUX_STRUCT([group_info], [gid], [cred.h])
AC_CHECK_LINUX_STRUCT([inode], [i_alloc_sem], [fs.h])
AC_CHECK_LINUX_STRUCT([inode], [i_blkbits], [fs.h])
AC_CHECK_LINUX_STRUCT([inode], [i_blksize], [fs.h])
AC_CHECK_LINUX_STRUCT([inode], [i_mutex], [fs.h])
AC_CHECK_LINUX_STRUCT([inode], [i_security], [fs.h])
AC_CHECK_LINUX_STRUCT([file], [f_path], [fs.h])
AC_CHECK_LINUX_STRUCT([file_operations], [flock], [fs.h])
AC_CHECK_LINUX_STRUCT([file_operations], [iterate_shared], [fs.h])
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
AC_CHECK_LINUX_STRUCT([nameidata], [path], [namei.h])
AC_CHECK_LINUX_STRUCT([proc_dir_entry], [owner], [proc_fs.h])
AC_CHECK_LINUX_STRUCT([proc_ops], [proc_compat_ioctl], [proc_fs.h])
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
dnl Linux 6.16 changed page.index to page.__folio_index;
AC_CHECK_LINUX_STRUCT([page], [__folio_index], [mm_types.h])

LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM

dnl Check for typed structure elements
AC_CHECK_LINUX_TYPED_STRUCT([read_descriptor_t],
                            [buf], [fs.h])
])
