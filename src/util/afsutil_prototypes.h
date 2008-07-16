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
extern void AssertionFailed(char *file, int line);


/* base32.c */
extern char *int_to_base32(b32_string_t s, int a);
extern int base32_to_int(char *s);

/* base64.c */
extern char *int_to_base64(b64_string_t s, int a);
extern int base64_to_int(char *s);

/* casestrcpy.c */
extern char *lcstring(char *d, char *s, int n);
extern char *ucstring(char *d, char *s, int n);
extern char *strcompose(char *buf, size_t len, ...);

/* daemon.c */
#ifndef HAVE_DAEMON
int daemon(int nochdir, int noclose);
#endif

/* dirpath.c */
extern unsigned int initAFSDirPath(void);
extern const char *getDirPath(afsdir_id_t string_id);
extern int ConstructLocalPath(const char *cpath, const char *relativeTo,
			      char **fullPathBufp);
extern int ConstructLocalBinPath(const char *cpath, char **fullPathBufp);
extern int ConstructLocalLogPath(const char *cpath, char **fullPathBufp);

/* errmap_nt.c */
extern int nterr_nt2unix(long ntErr, int defaultErr);

/* fileutil.c */
extern int renamefile(const char *oldname, const char *newname);
extern void FilepathNormalizeEx(char *path, int slashType);
extern void FilepathNormalize(char *path);

/* flipbase64.c */
extern char *int_to_base32(b32_string_t s, int a);
extern int base32_to_int(char *s);
#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
/* base 64 converters for namei interface. Flip bits to differences are
 * early in name.
 */
#ifdef AFS_64BIT_ENV
#define int32_to_flipbase64(S, A) int64_to_flipbase64(S, (afs_uint64)(A))
extern char *int64_to_flipbase64(lb64_string_t s, afs_uint64 a);
extern afs_int64 flipbase64_to_int64(char *s);
#else
#define int32_to_flipbase64(S, A) int64_to_flipbase64(S, (u_int64_t)(A))
extern char *int64_to_flipbase64(lb64_string_t s, u_int64_t a);
extern int64_t flipbase64_to_int64(char *s);
#endif
#endif

/* get_krbrlm.c */
extern int afs_krb_get_lrealm(char *r, int n);
extern int afs_krb_exclusion(char *name);
extern int afs_is_foreign_ticket_name(char *tname, char *tinst, char * tcell, char *localrealm);
/* hostparse.c */
extern struct hostent *hostutil_GetHostByName(register char *ahost);
extern char *hostutil_GetNameByINet(afs_uint32 addr);
extern afs_uint32 extractAddr(char *line, int maxSize);
extern char *afs_inet_ntoa(afs_uint32 addr);
extern char *afs_inet_ntoa_r(afs_uint32 addr, char *buf);
extern char *gettmpdir(void);

/* hputil.c */
#ifdef AFS_HPUX_ENV
#ifndef AFS_HPUX102_ENV
extern int utimes(char *file, struct timeval tvp[2]);
#endif
#if !defined(AFS_HPUX110_ENV)
extern int random(void);
extern void srandom(int seed);
#endif
extern int getdtablesize(void);
extern void setlinebuf(FILE * file);
extern void psignal(unsigned int sig, char *s);
#endif

/* isathing.c */
extern int util_isint(char *str);

/* kreltime.c */
struct ktime;
struct ktime_date;
extern afs_int32 ktimeRelDate_ToInt32(struct ktime_date *kdptr);
extern int Int32To_ktimeRelDate(afs_int32 int32Date,
				struct ktime_date *kdptr);
extern int ktimeDate_FromInt32(afs_int32 timeSecs,
			       struct ktime_date *ktimePtr);
extern afs_int32 ParseRelDate(char *dateStr, struct ktime_date *relDatePtr);
extern char *RelDatetoString(struct ktime_date *datePtr);
extern afs_int32 Add_RelDate_to_Time(struct ktime_date *relDatePtr,
				     afs_int32 atime);

/* ktime.c */
extern char *ktime_DateOf(afs_int32 atime);
extern afs_int32 ktime_Str2int32(register char *astr);
extern int ktime_ParsePeriodic(char *adate, register struct ktime *ak);
extern int ktime_DisplayString(struct ktime *aparm, register char *astring);
extern afs_int32 ktime_next(struct ktime *aktime, afs_int32 afrom);
extern afs_int32 ktime_DateToInt32(char *adate, afs_int32 * aint32);
extern char *ktime_GetDateUsage(void);
extern afs_int32 ktime_InterpretDate(struct ktime_date *akdate);

/* netutils.c */
extern afs_uint32 extract_Addr(char *line, int maxSize);
extern int parseNetRestrictFile(afs_uint32 outAddrs[], afs_uint32 * mask,
				afs_uint32 * mtu, afs_uint32 maxAddrs,
				afs_uint32 * nAddrs, char reason[],
				const char *fileName);
extern int ParseNetInfoFile(afs_uint32 * final, afs_uint32 * mask,
			    afs_uint32 * mtu, int max, char reason[],
			    const char *fileName);
extern int filterAddrs(afs_uint32 addr1[], afs_uint32 addr2[],
		       afs_uint32 mask1[], afs_uint32 mask2[],
		       afs_uint32 mtu1[], afs_uint32 mtu2[], int n1, int n2);
extern int parseNetFiles(afs_uint32 addrbuf[], afs_uint32 maskbuf[],
			 afs_uint32 mtubuf[], afs_uint32 max, char reason[],
			 const char *niFileName, const char *nrFileName);


/* pthread_glock.c */


/* readdir_nt.c */


/* regex.c */


/* secutil_nt.c */


/* serverLog.c */
extern void WriteLogBuffer(char *buf, afs_uint32 len);
extern void SetDebug_Signal(int signo);
extern void ResetDebug_Signal(int signo);
extern void SetupLogSignals(void);
extern int OpenLog(const char *fileName);
extern int ReOpenLog(const char *fileName);
extern int LogThreadNum(void);

/* snprintf.c */


/* strl */
#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_STRLCAT
extern size_t strlcat(char *dst, const char *src, size_t siz);
#endif

/* strn */
extern size_t afs_strnlen(char * buf, size_t len);


/* sys.c */


/* uuid.c */
extern afs_int32 afs_uuid_equal(afsUUID * u1, afsUUID * u2);
extern afs_int32 afs_uuid_is_nil(afsUUID * u1);
extern void afs_htonuuid(afsUUID * uuidp);
extern void afs_ntohuuid(afsUUID * uuidp);
extern afs_int32 afs_uuid_create(afsUUID * uuid);
extern u_short afs_uuid_hash(afsUUID * uuid);
#if !defined(KERNEL) && !defined(UKERNEL)
extern int afsUUID_from_string(const char *str, afsUUID * uuid);
extern int afsUUID_to_string(const afsUUID * uuid, char *str, size_t strsz);
#endif

/* volparse.c */
extern afs_int32 volutil_GetPartitionID(char *aname);
extern char *volutil_PartitionName_r(int avalue, char *tbuffer, int buflen);
extern char *volutil_PartitionName(int avalue);
extern afs_int32 util_GetInt32(register char *as, afs_int32 * aval);
extern afs_uint32 util_GetUInt32(register char *as, afs_uint32 * aval);

/* winsock_nt.c */


#endif /* _AFSUTIL_PROTOTYPES_H */
