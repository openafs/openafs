/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KPROC_H
#define	_OSI_LINUX_KPROC_H

#define OSI_IMPLEMENTS_PROC 1

typedef struct task_struct * osi_proc_p;
typedef struct task_struct osi_proc_t;
typedef pid_t osi_proc_id_t;


osi_static osi_inline osi_datamodel_t
osi_proc_datamodel(void)
{
    /* first deal with the 32-bit only platforms */
#if !defined(AFS_LINUX_64BIT_KERNEL)
    return OSI_DATAMODEL_ILP32;
#else /* AFS_LINUX_64BIT_KERNEL */

    /* now deal with the 64-bit only platforms */
#if defined(AFS_ALPHA_LINUX20_ENV) || defined(AFS_IA64_LINUX20_ENV)
    return OSI_DATAMODEL_LP64;
#else /* !AFS_ALPHA_LINUX20_ENV && !AFS_IA64_LINUX20_ENV */

    /* now deal with the variable platforms */
#if defined(AFS_SPARC64_LINUX26_ENV)
    if (test_thread_flag(TIF_32BIT)) {
#elif defined(AFS_SPARC64_LINUX24_ENV)
    if (current->thread.flags & SPARC_FLAG_32BIT) {
#elif defined(AFS_SPARC64_LINUX20_ENV)
    if (current->tss.flags & SPARC_FLAG_32BIT) {
#elif defined(AFS_AMD64_LINUX26_ENV)
    if (test_thread_flag(TIF_IA32)) {
#elif defined(AFS_AMD64_LINUX20_ENV)
    if (current->thread.flags & THREAD_IA32) {
#elif defined(AFS_PPC64_LINUX26_ENV)
    if (current->thread_info->flags & _TIF_32BIT) {
#elif defined(AFS_PPC64_LINUX20_ENV)
    if (current->thread.flags & PPC_FLAG_32BIT) {
#elif defined(AFS_S390X_LINUX26_ENV)
    if (test_thread_flag(TIF_31BIT)) {
#elif defined(AFS_S390X_LINUX20_ENV)
    if (current->thread.flags & S390_FLAG_31BIT) {
#else
#error "process datamodel detection not implemented for this platform; please file a bug"
#endif
	return OSI_DATAMODEL_ILP32;
    } else {
	return OSI_DATAMODEL_LP64;
    }
#endif /* !AFS_ALPHA_LINUX20_ENV && !AFS_IA64_LINUX20_ENV */
#endif /* AFS_LINUX_64BIT_KERNEL */
}

#define osi_proc_current() (current)
/* 
 * let me translate linux-speak to normal unix kernel-speak
 * (hopefully making your head hurt a little less)
 * 
 * linux speak  ->  unix kernel speak
 *
 * tgid  ->  pid
 * pid   ->  tid
 */
#define osi_proc_id(x) ((x)->tgid)
#define osi_proc_current_id() (osi_proc_id(osi_proc_current()))

#endif /* _OSI_LINUX_KPROC_H */
