/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	AFS_SRC_RXKAD_PROTO_H
#define AFS_SRC_RXKAD_PROTO_H

#include "../rx/rx.h"

/* rxk_clnt.c */
extern struct rx_securityClass *
rxkad_NewClientSecurityObject(rxkad_level level,   
                              struct ktc_encryptionKey *sessionkey,
                              afs_int32 kvno,
                              int ticket_len,
                              char *ticket);

/* rxk_info.c */
extern afs_int32 rxkad_GetServerInfo (struct rx_connection *aconn, 
        rxkad_level *level, afs_uint32 *expiration, char *name, char *instance, 
        char *cell, afs_int32 *kvno);

/* rxk_serv.c */
extern struct rx_securityClass *rxkad_NewServerSecurityObject (
        rxkad_level min_level, void *appl_data, 
	int (*get_key)(void *appl_data,
		       int kvno,
		       struct ktc_encryptionKey *serverKey), 
	int (*user_ok)(char *name,
		       char *inst,
		       char *realm,
		       int kvno));

/* v4.c */
extern afs_uint32 life_to_time (afs_uint32 start, int life_);
extern int time_to_life (afs_uint32 start, afs_uint32 end);
extern int tkt_CheckTimes (afs_uint32 start, afs_uint32 end, afs_uint32 now);
extern int tkt_MakeTicket (char *ticket, int *ticketLen, 
        struct ktc_encryptionKey *key, char *name, char *inst, char *cell,
        afs_uint32 start, afs_uint32 end, struct ktc_encryptionKey *sessionKey, 
        afs_uint32 host, char *sname, char *sinst);
extern int tkt_DecodeTicket (char *ticket, afs_int32 ticketLen, 
        struct ktc_encryptionKey *key, char *name, char *inst, char *cell, 
        char *sessionKey, afs_int32 *host, afs_int32 *start, afs_int32 *end);
extern afs_int32 ktohl (char flags, afs_int32 l);


#endif /* AFS_SRC_RXKAD_PROTO_H */
