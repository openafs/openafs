/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __SMB_IOCTL_H_ENV__
#define __SMB_IOCTL_H_ENV__ 1

/* magic file name for ioctl opens */
#define SMB_IOCTL_FILENAME	"\\_._AFS_IOCTL_._"	/* double backslashes for C compiler */
#define SMB_IOCTL_FILENAME_NOSLASH "_._AFS_IOCTL_._"

/* max parms for ioctl, in either direction */
#define SMB_IOCTL_MAXDATA	8192

#define SMB_IOCTL_MAXPROCS	64			/* max # of calls */

/* procedure implementing an ioctl */
typedef long (smb_ioctlProc_t)(smb_ioctl_t *, struct cm_user *userp);

extern void smb_InitIoctl(void);

extern void smb_SetupIoctlFid(smb_fid_t *fidp, cm_space_t *prefix);

extern long smb_IoctlRead(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_IoctlWrite(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_IoctlV3Read(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

#ifndef DJGPP
extern long smb_IoctlReadRaw(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp);
#else /* DJGPP */
extern long smb_IoctlReadRaw(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp, dos_ptr rawBuf);
#endif /* !DJGPP */

#endif /*  __SMB_IOCTL_H_ENV__ */
