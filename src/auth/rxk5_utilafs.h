/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#ifndef RXK5_UTILAFS_H
#define RXK5_UTILAFS_H

#ifdef USING_K5SSL
#include "k5ssl.h"
#else
#ifdef USING_SHISHI
#include <shishi.h>
#else
#ifdef private
#undef private
#endif
#include <krb5.h>
#if defined(AFS_NT40_ENV) && defined(USING_MIT)
#include <rx/rxk5_ntfixprotos.h>
#include <afs/afskfw_funcs.h>
#endif
#endif
#endif

/* Format a full path to the AFS keytab, caller must free */
char* get_afs_rxk5_keytab(char *confdir_name);

/* Returns the AFS service principal for the chosen cell/realm (currently the default realm),
  the caller must free */
char* get_afs_krb5_svc_princ(struct afsconf_cell *);

/* Returns
	FORCE_RXK5|FORCE_RXKAD|FORCE_KTC	if AFS_RXK5_DEFAULT is not set,
	FORCE_RXK5|FORCE_KTC		if AFS_RXK5_DEFAULT is 1 or UPPER('yes')
	FORCE_RXK5|FORCE_K5CC		if AFS_RXK5_DEFAULT is 2 or UPPER('k5cc')
	FORCE_RXKAD|FORCE_KTC		otherwise
*/
int env_afs_rxk5_default(void);

#if 0
/* Forge a krb5 ticket from a keytab entry, return it in creds, which caller
   must free */ 
  
int afs_rxk5_k5forge(krb5_context context,
    char* keytab,
    char* service,
    char* client,
    time_t starttime,
    time_t endtime,
    int *allowed_enctypes,
    int *paddress,
    krb5_creds** out_creds /* out */ );

int default_afs_rxk5_forge( krb5_context context, struct afsconf_dir *adir,
			    char* service, krb5_creds* in_creds );
#endif

int have_afs_rxk5_keytab(char *);
char* get_afs_rxk5_keytab(char *);
char* get_afs_krb5_svc_princ(struct afsconf_cell *);
int afs_rxk5_parse_name_k5(struct afsconf_dir *, const char *, char **, int);
int afs_rxk5_split_name_instance(char *, char **, char **);

#ifdef AFS_NT40_ENV
void rxk5_KFW_initialize(void);
#endif

#endif /* RXK5_UTILAFS_H */
