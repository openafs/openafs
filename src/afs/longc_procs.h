/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_longc_procs_h_
#define	_longc_procs_h_		1

#if !defined(LONGCALL_DEFS) && defined(KERNEL) && defined(DYNEL) && defined(AFS_DECOSF_ENV)

#define LONGCALL_DEFS

struct afs_longcall_procs {
    int (*LC_xdr_u_short) ();
    int (*LC_xdr_short) ();
    int (*LC_xdr_opaque) ();
    int (*LC_xdr_array) ();
    int (*LC_xdr_bytes) ();
    int (*LC_xdr_u_long) ();
    int (*LC_xdr_string) ();
    int (*LC_xdr_long) ();
    int (*LC_xdr_int) ();
    int (*LC_xdr_u_int) ();
    int (*LC_ioctl) ();
    int (*LC_copyout) ();
    int (*LC_biodone) ();
    int (*LC_ureadc) ();
    int (*LC_strcmp) ();
    int (*LC_uiomove) ();
    int (*LC_uprintf) ();
    int (*LC_strncpy) ();
    int (*LC_flock) ();
    int (*LC_setgroups) ();
    struct ucred *(*LC_crcopy) ();
    struct ucred *(*LC_crget) ();
    int (*LC_crhold) ();
    int (*LC_ip_stripoptions) ();
    int (*LC_in_cksum) ();
    struct mbuf *(*LC_m_pullup) ();
    int (*LC_resettodr) ();
    int (*LC_untimeout) ();
    int (*LC_timeout) ();
    int (*LC_mpurge) ();
    int (*LC_xrele) ();
    int (*LC_m_free) ();
    int (*LC_m_clalloc) ();
    int (*LC_splimp) ();
    int (*LC_m_freem) ();
    int (*LC_soclose) ();
    int (*LC_sobind) ();
    struct mbuf *(*LC_m_get) ();
    int (*LC_socreate) ();
    int (*LC_soreserve) ();
#if	defined(AFS_DECOSF_ENV)
    int (*LC_getf) ();
#else				/* AFS_DECOSF_ENV */
    struct file *(*LC_getf) ();
#endif
    int (*LC_splx) ();
    int (*LC_microtime) ();
    int (*LC_splnet) ();
    int (*LC_bcmp) ();
#ifdef XDR_CHAR_IN_KERNEL
    int (*LC_xdr_char) ();
#endif
    int (*LC_iunlock) ();
    int (*LC_ilock) ();
    int (*LC_iput) ();
    struct inode *(*LC_ialloc) ();
    int (*LC_vno_close) ();
    int (*LC_kmem_alloc) ();
    int (*LC_kmem_free) ();

    int (*LC_m_cpytoc) ();

    int (*LC_ufs_brelse) ();
    int (*LC_lookupname) ();
    int (*LC_vn_rele) ();
    int (*LC_vn_rdwr) ();
    int (*LC_mapout) ();
    struct mount *(*LC_getmp) ();
    struct inode *(*LC_iget) ();
    struct mbuf *(*LC_m_more) ();
    int (*LC__spl1) ();
    int (*LC_rdwri) ();
    struct file *(*LC_falloc) ();
    int (*LC_rmfree) ();
    int (*LC_mapin) ();
    long (*LC_rmalloc) ();
    struct pte *(*LC_vtopte) ();
    int (*LC_vattr_null) ();
    int (*LC_strlen) ();
    int (*LC_bcopy) ();
    int (*LC_brelse) ();
    struct buf *(*LC_geteblk) ();
    int (*LC_panic) ();
    int (*LC_strcpy) ();
    int (*LC_printf) ();
    int (*LC_copyinstr) ();
    int (*LC_copyin) ();
    int (*LC_sleep) ();
    int (*LC_wakeup) ();
    int (*LC_bzero) ();
    int (*LC_suser) ();
    int (*LC_crfree) ();
#if 0
    int (*LC_igetinode) ();
#endif
#if	defined(AFS_DECOSF_ENV)
    void (*LC_assert_wait) ();
    int (*LC_closef) ();
    int (*LC_fake_inode_init) ();
    int (*LC_getnewvnode) ();
    struct mount *(*LC_getvfs) ();
    int (*LC_idrop) ();
    int (*LC_insmntque) ();
    int (*LC_ioctl_base) ();
      caddr_t(*LC_kalloc) ();
    void (*LC_kfree) ();
    void (*LC_lock_done) ();
    struct mbuf *(*LC_m_getclr) ();
    struct mbuf *(*LC_m_retry) ();
    struct mbuf *(*LC_m_retryhdr) ();
    int (*LC_mpsleep) ();
    int (*LC_namei) ();
    unsigned int (*LC_nuxi_32) ();
    unsigned short (*LC_nuxi_16) ();
    int (*LC_setgroups_base) ();
    int (*LC_substitute_real_creds) ();
    int (*LC_swap_ipl) ();
    void (*LC_thread_block) ();
    /* XXX - should be kern_return_t */
    int (*LC_u_vp_create) ();
    int (*LC_ubc_bufalloc) ();
    int (*LC_ubc_flush_sync) ();
    int (*LC_ubc_invalidate) ();
    int (*LC_ubc_lookup) ();
    int (*LC_ubc_page_dirty) ();
    int (*LC_ubc_page_release) ();
    int (*LC_ubc_sync_iodone) ();
    int (*LC_vgetm) ();
    int (*LC_vgone) ();
    int (*LC_vn_close) ();
/*
    int (*LC_vn_ioctl) ();
    int (*LC_vn_read) ();
    int (*LC_vn_select) ();
    int (*LC_vn_write) ();
*/
    void (*LC_vrele) ();
    int (*LC_xdr_char) ();
#endif				/* AFS_DECOSF_ENV */
};
extern struct afs_longcall_procs afs_longcall_procs;

#ifndef LONGCALL_NO_MACROS

#if	defined(AFS_DECOSF_ENV)
#undef	kmem_alloc
#undef	kmem_free
#endif /* AFS_DECOSF_ENV */

#define crfree (*afs_longcall_procs.LC_crfree)
#define suser (*afs_longcall_procs.LC_suser)
#define bzero (*afs_longcall_procs.LC_bzero)
#define wakeup (*afs_longcall_procs.LC_wakeup)
#if	!defined(AFS_DECOSF_ENV)
#define sleep (*afs_longcall_procs.LC_sleep)
#endif
#define copyin (*afs_longcall_procs.LC_copyin)
#define copyinstr (*afs_longcall_procs.LC_copyinstr)
#define printf (*afs_longcall_procs.LC_printf)
#define panic (*afs_longcall_procs.LC_panic)
#define geteblk (*afs_longcall_procs.LC_geteblk)
#define brelse (*afs_longcall_procs.LC_brelse)
#define bcopy (*afs_longcall_procs.LC_bcopy)
#define strlen (*afs_longcall_procs.LC_strlen)
#define vtopte (*afs_longcall_procs.LC_vtopte)
#define rmalloc (*afs_longcall_procs.LC_rmalloc)

#define mapin (*afs_longcall_procs.LC_mapin)
#define strcpy (*afs_longcall_procs.LC_strcpy)
#define strncpy (*afs_longcall_procs.LC_strncpy)

#define rmfree (*afs_longcall_procs.LC_rmfree)
#define falloc (*afs_longcall_procs.LC_falloc)
#define rdwri (*afs_longcall_procs.LC_rdwri)

#ifdef XDR_CHAR_IN_KERNEL
#define xdr_char (*afs_longcall_procs.LC_xdr_char)
#endif

#define iunlock (*afs_longcall_procs.LC_iunlock)
#define ilock (*afs_longcall_procs.LC_ilock)
#define iput (*afs_longcall_procs.LC_iput)
#define ialloc (*afs_longcall_procs.LC_ialloc)
#if	!defined(AFS_DECOSF_ENV)
#define vno_close (*afs_longcall_procs.LC_vno_close)
#endif
#define kmem_alloc (*afs_longcall_procs.LC_kmem_alloc)
#define kmem_free (*afs_longcall_procs.LC_kmem_free)

#define m_cpytoc (*afs_longcall_procs.LC_m_cpytoc)

#define ufs_brelse (*afs_longcall_procs.LC_ufs_brelse)
#if	!defined(AFS_DECOSF_ENV)
#define lookupname (*afs_longcall_procs.LC_lookupname)
#endif
/* #define vn_rele (*afs_longcall_procs.LC_vn_rele) */
#define mapout (*afs_longcall_procs.LC_mapout)
#define getmp (*afs_longcall_procs.LC_getmp)
#define iget (*afs_longcall_procs.LC_iget)
#define m_more (*afs_longcall_procs.LC_m_more)
#define _spl1 (*afs_longcall_procs.LC__spl1)

#define bcmp (*afs_longcall_procs.LC_bcmp)
#if	!defined(AFS_DECOSF_ENV)
#define splnet (*afs_longcall_procs.LC_splnet)
#define splx (*afs_longcall_procs.LC_splx)
#endif
#define microtime (*afs_longcall_procs.LC_microtime)
/* #define ldiv$$ (*afs_longcall_procs[34]) */
#define getf (*afs_longcall_procs.LC_getf)
#define soreserve (*afs_longcall_procs.LC_soreserve)
#define socreate (*afs_longcall_procs.LC_socreate)
#define m_get (*afs_longcall_procs.LC_m_get)
#define sobind (*afs_longcall_procs.LC_sobind)
#define soclose (*afs_longcall_procs.LC_soclose)
#define m_freem (*afs_longcall_procs.LC_m_freem)
#if	!defined(AFS_DECOSF_ENV)
#define splimp (*afs_longcall_procs.LC_splimp)
#endif
#define m_clalloc (*afs_longcall_procs.LC_m_clalloc)
#define m_free (*afs_longcall_procs.LC_m_free)
#define xrele (*afs_longcall_procs.LC_xrele)
#define mpurge (*afs_longcall_procs.LC_mpurge)
/* #define lmul$$ (*afs_longcall_procs[48]) */
/* #define timeout (*afs_longcall_procs.LC_timeout) */
#define untimeout (*afs_longcall_procs.LC_untimeout)
#define resettodr (*afs_longcall_procs.LC_resettodr)
#define m_pullup (*afs_longcall_procs.LC_m_pullup)
#define ip_stripoptions (*afs_longcall_procs.LC_ip_stripoptions)
#define in_cksum (*afs_longcall_procs.LC_in_cksum)
#define crcopy (*afs_longcall_procs.LC_crcopy)
#if !defined(AFS_DECOSF_ENV)
#define crhold (*afs_longcall_procs.LC_crhold)
#endif
#define crget (*afs_longcall_procs.LC_crget)
#define setgroups (*afs_longcall_procs.LC_setgroups)
/* #define flock (*afs_longcall_procs.LC_flock) */
#define uprintf (*afs_longcall_procs.LC_uprintf)
#define uiomove (*afs_longcall_procs.LC_uiomove)
#define strcmp (*afs_longcall_procs.LC_strcmp)
#define ureadc (*afs_longcall_procs.LC_ureadc)
#define biodone (*afs_longcall_procs.LC_biodone)
/* #define uldiv$$ (*afs_longcall_procs[68]) */
#define copyout (*afs_longcall_procs.LC_copyout)
#define ioctl (*afs_longcall_procs.LC_ioctl)
#define xdr_int (*afs_longcall_procs.LC_xdr_int)
#define xdr_long (*afs_longcall_procs.LC_xdr_long)
#define xdr_string (*afs_longcall_procs.LC_xdr_string)
#define xdr_u_long (*afs_longcall_procs.LC_xdr_u_long)
#define xdr_u_int (*afs_longcall_procs.LC_xdr_u_int)
#define xdr_bytes (*afs_longcall_procs.LC_xdr_bytes)
#define xdr_array (*afs_longcall_procs.LC_xdr_array)
#define xdr_opaque (*afs_longcall_procs.LC_xdr_opaque)
#define xdr_short (*afs_longcall_procs.LC_xdr_short)
#define xdr_u_short (*afs_longcall_procs.LC_xdr_u_short)

#if	defined(AFS_DECOSF_ENV)
#define assert_wait (*afs_longcall_procs.LC_assert_wait)
#define closef (*afs_longcall_procs.LC_closef)
#define fake_inode_init (*afs_longcall_procs.LC_fake_inode_init)
#define getnewvnode (*afs_longcall_procs.LC_getnewvnode)
#define getvfs (*afs_longcall_procs.LC_getvfs)
#define idrop (*afs_longcall_procs.LC_idrop)
#define insmntque (*afs_longcall_procs.LC_insmntque)
#define ioctl_base (*afs_longcall_procs.LC_ioctl_base)
#define kalloc (*afs_longcall_procs.LC_kalloc)
#define kfree (*afs_longcall_procs.LC_kfree)
#define lock_done (*afs_longcall_procs.LC_lock_done)
#define m_getclr (*afs_longcall_procs.LC_m_getclr)
#define m_retry (*afs_longcall_procs.LC_m_retry)
#define m_retryhdr (*afs_longcall_procs.LC_m_retryhdr)
#define mpsleep (*afs_longcall_procs.LC_mpsleep)
#define namei (*afs_longcall_procs.LC_namei)
#define nuxi_32 (*afs_longcall_procs.LC_nuxi_32)
#define nuxi_16 (*afs_longcall_procs.LC_nuxi_16)
#define setgroups_base (*afs_longcall_procs.LC_setgroups_base)
#define substitute_real_creds (*afs_longcall_procs.LC_substitute_real_creds)
#define swap_ipl (*afs_longcall_procs.LC_swap_ipl)
#define thread_block (*afs_longcall_procs.LC_thread_block)
#define u_vp_create (*afs_longcall_procs.LC_u_vp_create)
#define ubc_bufalloc (*afs_longcall_procs.LC_ubc_bufalloc)
#define ubc_flush_sync (*afs_longcall_procs.LC_ubc_flush_sync)
#define ubc_invalidate (*afs_longcall_procs.LC_ubc_invalidate)
#define ubc_lookup (*afs_longcall_procs.LC_ubc_lookup)
#define ubc_page_dirty (*afs_longcall_procs.LC_ubc_page_dirty)
#define ubc_page_release (*afs_longcall_procs.LC_ubc_page_release)
#define ubc_sync_iodone (*afs_longcall_procs.LC_ubc_sync_iodone)
#define vgetm (*afs_longcall_procs.LC_vgetm)
#define vgone (*afs_longcall_procs.LC_vgone)
#define vn_close (*afs_longcall_procs.LC_vn_close)
/*
#define vn_ioctl (*afs_longcall_procs.LC_vn_ioctl)
#define vn_read (*afs_longcall_procs.LC_vn_read)
#define vn_select (*afs_longcall_procs.LC_vn_select)
#define vn_write (*afs_longcall_procs.LC_vn_write)
*/
#define vrele (*afs_longcall_procs.LC_vrele)
#define xdr_char (*afs_longcall_procs.LC_xdr_char)
#endif /* AFS_DECOSF_ENV */

#endif /* LONGCALL_NO_MACROS */

#endif
#endif
