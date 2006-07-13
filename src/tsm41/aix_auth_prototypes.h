/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_TSM41_AIX_AUTH_PROTOTYPES_H
#define _AFS_TSM41_AIX_AUTH_PROTOTYPES_H

extern int afs_authenticate(char *userName, 
			    char *response, 
			    int *reenter, 
			    char **message);
extern int afs_chpass(char *userName, char *oldPasswd, 
		      char *newPasswd, char **message);
extern int afs_passwdexpired(char *userName, char **message);
extern int afs_passwdrestrictions(char *userName, char *newPasswd, 
				  char *oldPasswd, char ** message);
extern char * afs_getpasswd(char * userName);
extern void aix_ktc_setup_ticket_file(char * userName);

#endif /* _AFS_TSM41_AIX_AUTH_PROTOTYPES_H */
