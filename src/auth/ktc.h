/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_AUTH_KTC_H
#define _AFS_AUTH_KTC_H

extern char * ktc_tkt_string(void);
extern char * ktc_tkt_string_uid(afs_uint32);
extern void ktc_set_tkt_string(char *);


#endif /* _AFS_AUTH_KTC_H */
