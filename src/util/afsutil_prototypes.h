/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_AFSUTIL_PROTOTYPES_H
#define _AFSUTIL_PROTOTYPES_H

/* afs_atomlist.c */


/* afs_lhash.c */


/* assert.c */


/* base32.c */


/* base64.c */


/* casestrcpy.c */


/* dirpath.c */


/* errmap_nt.c */


/* fileutil.c */


/* flipbase64.c */


/* get_krbrlm.c */


/* hostparse.c */
extern struct hostent *hostutil_GetHostByName(register char *ahost);
extern char *hostutil_GetNameByINet(afs_uint32 addr);
extern afs_uint32 extractAddr(char *line, int maxSize);
extern char *afs_inet_ntoa(afs_uint32 addr);
extern char *afs_inet_ntoa_r(afs_uint32 addr, char *buf);
extern char *gettmpdir(void);

/* hputil.c */


/* isathing.c */


/* kreltime.c */
extern afs_int32 ktimeRelDate_ToInt32(struct ktime_date *kdptr);
extern int Int32To_ktimeRelDate(afs_int32 int32Date, struct ktime_date *kdptr);
extern int ktimeDate_FromInt32(afs_int32 timeSecs, struct ktime_date *ktimePtr);
extern afs_int32 ParseRelDate(char *dateStr, struct ktime_date *relDatePtr);
extern char *RelDatetoString(struct ktime_date *datePtr);
extern afs_int32 Add_RelDate_to_Time(struct ktime_date *relDatePtr, afs_int32 atime);


/* ktime.c */


/* netutils.c */
extern afs_uint32 extract_Addr(char *line, int maxSize);
extern int parseNetRestrictFile(afs_uint32 outAddrs[], afs_uint32 *mask, afs_uint32 *mtu,   
        afs_uint32 maxAddrs, afs_uint32 *nAddrs, char reason[], const char *fileName);
extern int ParseNetInfoFile(afs_uint32 *final, afs_uint32 *mask, afs_uint32 *mtu, 
        int max, char reason[], const char *fileName);
extern int filterAddrs(afs_uint32 addr1[], afs_uint32 addr2[], 
        afs_uint32 mask1[], afs_uint32 mask2[], 
        afs_uint32 mtu1[], afs_uint32 mtu2[], 
        int n1, int n2);
extern int parseNetFiles(afs_uint32 addrbuf[], afs_uint32 maskbuf[], afs_uint32 mtubuf[], 
        afs_uint32 max, char reason[], const char *niFileName, const char *nrFileName);


/* pthread_glock.c */


/* readdir_nt.c */


/* regex.c */


/* secutil_nt.c */


/* serverLog.c */


/* snprintf.c */


/* sys.c */


/* uuid.c */
extern afs_int32 afs_uuid_equal(afsUUID *u1, afsUUID *u2);
extern afs_int32 afs_uuid_is_nil(afsUUID *u1);
extern void afs_htonuuid(afsUUID *uuidp);
extern void afs_ntohuuid(afsUUID *uuidp);
extern afs_int32 afs_uuid_create (afsUUID *uuid);
extern u_short afs_uuid_hash (afsUUID *uuid);


/* volparse.c */


/* winsock_nt.c */


#endif /* _AFSUTIL_PROTOTYPES_H */

