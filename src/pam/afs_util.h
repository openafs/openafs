/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef PAM_AFS_UTIL_H
#define PAM_AFS_UTIL_H


extern char *pam_afs_ident;
extern char *pam_afs_lh;


void lc_cleanup(pam_handle_t * pamh, void *data, int pam_end_status);

void nil_cleanup(pam_handle_t * pamh, void *data, int pam_end_status);

extern char *cv2string(char *ttp, unsigned long aval);
extern int do_klog(const char *user, const char *password,
		   const char *lifetime, const char *cell_name);
extern afs_int32 getPAG(void);

#define KLOG "/usr/afsws/bin/klog"
#define KLOGKRB "/usr/afsws/bin/klog.krb"
#define UNLOG "/usr/afsws/bin/unlog"
#define IGNORE_MAX 1000

#if	defined(AFS_HPUX_ENV)

#if !defined(AFS_HPUX110_ENV)
#define	PAM_NEW_AUTHTOK_REQD	PAM_AUTHTOKEN_REQD
#endif /* ! AFS_HPUX110_ENV */
#define vsyslog(a,b,c)		syslog(a,b,c)
#define pam_get_user(a,b,c)	pam_get_item(a, PAM_USER, (void **)b)
#define pam_putenv(a,b)		!PAM_SUCCESS

#endif /* AFS_HPUX_ENV */

#endif
