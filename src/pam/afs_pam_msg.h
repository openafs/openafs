/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_PAM_MSG_H
#define AFS_PAM_MSG_H


int pam_afs_printf(PAM_CONST struct pam_conv *pam_convp, int error, int fmt_msgid, ...);

int pam_afs_prompt(PAM_CONST struct pam_conv *pam_convp, char **response, int echo,
		   int fmt_msgid, ...);


#endif
