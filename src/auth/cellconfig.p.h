/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
cellconfig.h:

    Interface to the routines used by the FileServer to manipulate the cell/server database
    for the Cellular Andrew system, along with an operation to determine the name of the
    local cell.  Included are a string variable used to hold the local cell name, definitions for
    the database file format and routines for:
        1) Acquiring the local cell name.
        2) Reading in the cell/server database from disk.
        3) Reporting the set of servers associated with a given cell name.
        4) Printing out the contents of the cell/server database.
        5) Reclaiming the space used by an in-memory database.

Creation date:
    17 August 1987

--------------------------------------------------------------------------------------------------------------*/

#ifndef __CELLCONFIG_AFS_INCL_
#define	__CELLCONFIG_AFS_INCL_	1

#ifndef IPPROTO_MAX
	/* get sockaddr_in */
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#endif
#endif
#include <rx/rx_opaque.h>
#include <opr/queue.h>

#include <rx/rxgk_types.h>

#define	MAXCELLCHARS	64
#define	MAXHOSTCHARS	64
#define MAXHOSTSPERCELL  8

/*
 * Return codes.
 */
#define	AFSCONF_SUCCESS	  0	/* worked */

/*
 * Complete server info for one cell.
 */
struct afsconf_cell {
    char name[MAXCELLCHARS];	/*Cell name */
    short numServers;		/*Num active servers for the cell */
    short flags;		/* useful flags */
    struct sockaddr_in hostAddr[MAXHOSTSPERCELL];	/*IP addresses for cell's servers */
    char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];	/*Names for cell's servers */
    char clone[MAXHOSTSPERCELL];			/*Indicates which ones are clones */
    char *linkedCell;		/* Linked cell name, if any */
    int timeout;		/* Data timeout, if non-zero */
};

#define AFSCONF_CELL_FLAG_DNS_QUERIED   1

struct afsconf_cellalias {
    char aliasName[MAXCELLCHARS];
    char realName[MAXCELLCHARS];
};

struct afsconf_entry {
    struct afsconf_entry *next;	/* next guy in afsconf_dir */
    struct afsconf_cell cellInfo;	/* info for this cell */
};

struct afsconf_aliasentry {
    struct afsconf_aliasentry *next;
    struct afsconf_cellalias aliasInfo;
};

/*!
 * A set of bit flags to control the selection of a security object
 */
#define AFSCONF_SECOPTS_NOAUTH        0x1
#define AFSCONF_SECOPTS_LOCALAUTH     0x2
#define AFSCONF_SECOPTS_ALWAYSENCRYPT 0x4
#define AFSCONF_SECOPTS_FALLBACK_NULL 0x8
#define AFSCONF_SECOPTS_NEVERENCRYPT  0x10
#define AFSCONF_SECOPTS_ALWAYSCLEAR   0x20
#define AFSCONF_SECOPTS_RXGK          0x40
typedef afs_uint32 afsconf_secflags;

struct afsconf_dir {
    char *name;			/* pointer to dir prefix */
    char *cellName;		/* cell name, if any, we're in */
    char *cellservDB;		/* pathname of the CellServDB file */
    struct afsconf_entry *entries;	/* list of cell entries */
    struct opr_queue keyList;		/* list of keys */
    afs_int32 timeRead;		/* time stamp of file last read */
    afs_int32 timeCheck;	/* time of last check for update */
    struct afsconf_aliasentry *alias_entries;	/* cell aliases */
    afsconf_secflags securityFlags;
    struct afsconf_realms *local_realms;        /* local realms */
    struct afsconf_realms *exclusions;          /* excluded principals */
};

extern afs_int32 afsconf_FindService(const char *aname);
extern const char *afsconf_FindIANAName(const char *aname);
extern struct afsconf_dir *afsconf_Open(const char *adir);
extern int afsconf_CellApply(struct afsconf_dir *adir,
			     int (*aproc) (struct afsconf_cell * cell,
					   void *arock,
					   struct afsconf_dir * dir),
			     void *arock);
extern int afsconf_CellAliasApply(struct afsconf_dir *adir,
				  int (*aproc) (struct afsconf_cellalias *
						alias, void *arock,
						struct afsconf_dir * dir),
				  void *arock);
extern int afsconf_GetExtendedCellInfo(struct afsconf_dir *adir,
				       char *acellName, char *aservice,
				       struct afsconf_cell *acellInfo,
				       char clones[]);
extern int afsconf_GetAfsdbInfo(char *acellName, char *aservice,
				struct afsconf_cell *acellInfo);
extern int afsconf_GetCellInfo(struct afsconf_dir *adir, char *acellName,
			       char *aservice,
			       struct afsconf_cell *acellInfo);
extern int afsconf_GetCellName(struct afsconf_dir *adir, char *acellName,
			       char *buf, size_t buf_size);
extern int afsconf_GetLocalCell(struct afsconf_dir *adir,
				char *aname, afs_int32 alen);
extern int afsconf_Close(struct afsconf_dir *adir);
extern int afsconf_UpToDate(void *rock);
int afsconf_SetExtendedCellInfo(struct afsconf_dir *adir, const char *apath,
				struct afsconf_cell *acellInfo, char clones[]);
int afsconf_SetCellInfo(struct afsconf_dir *adir, const char *apath,
		        struct afsconf_cell *acellInfo);

struct afsconf_keys;
extern int afsconf_GetKeys(struct afsconf_dir *adir,
			   struct afsconf_keys *astr);

struct ktc_encryptionKey;
extern afs_int32 afsconf_GetLatestKey(struct afsconf_dir *adir,
				      afs_int32 * avno,
				      struct ktc_encryptionKey *akey);
extern afs_int32 afsconf_GetLatestRXGKKey(struct afsconf_dir *adir,
					  afs_int32 *avno, afs_int32 *enctype,
					  rxgk_key *key);
extern int afsconf_GetKey(void *rock, int avno,
			  struct ktc_encryptionKey *akey);
extern int afsconf_GetRXGKKey(void *rock, afs_int32 *avno, afs_int32 *enctype,
			      rxgk_key *key);
extern int afsconf_AddKey(struct afsconf_dir *adir, afs_int32 akvno,
			  char akey[8], afs_int32 overwrite);
extern int afsconf_DeleteKey(struct afsconf_dir *adir, afs_int32 akvno);

struct afsconf_typedKey;
struct afsconf_typedKeyList {
    int nkeys;
    struct afsconf_typedKey **keys;
};

typedef enum {
    afsconf_rxkad = 0,
    afsconf_rxgk  =1,
    afsconf_rxkad_krb5  =2
} afsconf_keyType;

extern struct afsconf_typedKey *
	afsconf_typedKey_get(struct afsconf_typedKey *);
extern void afsconf_typedKey_put(struct afsconf_typedKey **);
extern struct afsconf_typedKey *
	afsconf_typedKey_new(afsconf_keyType type, int kvno,
			     int subType, struct rx_opaque *key);
extern void afsconf_typedKey_free(struct afsconf_typedKey **);

extern void afsconf_typedKey_values(struct afsconf_typedKey *key,
				  afsconf_keyType *type,
				  int *kvno,
				  int *minorType,
				  struct rx_opaque **keyMaterial);

extern int afsconf_CountKeys(struct afsconf_dir *);
extern int afsconf_GetAllKeys(struct afsconf_dir *,
			      struct afsconf_typedKeyList **);
extern int afsconf_GetKeysByType(struct afsconf_dir *dir,
				 afsconf_keyType type, int kvno,
				 struct afsconf_typedKeyList **);
extern int afsconf_GetKeyByTypes(struct afsconf_dir *dir,
				 afsconf_keyType type, int kvno, int subType,
				 struct afsconf_typedKey **);
extern int afsconf_GetLatestKeysByType(struct afsconf_dir *dir,
				       afsconf_keyType type,
				       struct afsconf_typedKeyList **);
extern int afsconf_GetLatestKeyByTypes(struct afsconf_dir *dir,
				       afsconf_keyType type, int subType,
				       struct afsconf_typedKey **);
extern void afsconf_PutTypedKeyList(struct afsconf_typedKeyList **keys);
extern int afsconf_AddTypedKey(struct afsconf_dir *dir,
			       struct afsconf_typedKey *key,
			       int overwrite);
extern int afsconf_DeleteKeyByType(struct afsconf_dir *dir,
				   afsconf_keyType type, int kvno);
extern int afsconf_DeleteKeyBySubType(struct afsconf_dir *dir,
				      afsconf_keyType type, int kvno,
				      int subType);

/* authcon.c */
struct rx_securityClass;
struct rx_identity;
extern afs_int32 afsconf_ServerAuth(void *arock,
				    struct rx_securityClass **,
				    afs_int32 *);
extern afs_int32 afsconf_ClientAuth(void *arock,
				    struct rx_securityClass **astr,
				    afs_int32 * aindex);
extern afs_int32 afsconf_ClientAuthSecure(void *arock,
				          struct rx_securityClass **astr,
				          afs_int32 * aindex);
extern afs_int32 afsconf_ClientAuthRXGKClear(void *arock,
					     struct rx_securityClass **aclass,
					     afs_int32 *aindex);
extern afs_int32 afsconf_ClientAuthRXGKAuth(void *arock,
					    struct rx_securityClass **aclass,
					    afs_int32 *aindex);
extern afs_int32 afsconf_ClientAuthRXGKCrypt(void *arock,
					     struct rx_securityClass **aclass,
					     afs_int32 *aindex);


extern afs_int32 afsconf_ClientAuthToken(struct afsconf_cell *info,
					 afsconf_secflags flags,
					 struct rx_securityClass **sc,
					 afs_int32 *scIndex,
					 time_t *expires);


extern afs_int32 afsconf_PickClientSecObj(struct afsconf_dir *dir,
					  afsconf_secflags flags,
					  struct afsconf_cell *info,
					  char *cellName,
					  struct rx_securityClass **sc,
					  afs_int32 *scIndex,
					  time_t *expires);
extern afs_int32 afsconf_PickClientLocalSecObj(struct afsconf_dir *dir,
					       afsconf_secflags flags,
					       struct rx_identity *user,
					       struct rx_securityClass **sc,
					       afs_int32 *scIndex,
					       time_t *expires);

extern void afsconf_SetSecurityFlags(struct afsconf_dir *dir,
				     afsconf_secflags flags);

extern void afsconf_BuildServerSecurityObjects(void *,
					       struct rx_securityClass ***,
					       afs_int32 *);



/* userok.c */

struct rx_call;
extern int afsconf_CheckAuth(void *arock, struct rx_call *acall);
extern int afsconf_GetNoAuthFlag(struct afsconf_dir *adir);
extern void afsconf_SetNoAuthFlag(struct afsconf_dir *adir, int aflag);
extern int afsconf_DeleteUser(struct afsconf_dir *adir, char *auser);
extern int afsconf_DeleteIdentity(struct afsconf_dir *, struct rx_identity *);
extern int afsconf_GetNthUser(struct afsconf_dir *adir, afs_int32 an,
			      char *abuffer, afs_int32 abufferLen);
extern int afsconf_GetNthIdentity(struct afsconf_dir *, int,
				  struct rx_identity **);
extern int afsconf_AddUser(struct afsconf_dir *adir, char *aname);
extern int afsconf_AddIdentity(struct afsconf_dir *adir, struct rx_identity *);
extern int afsconf_SuperUser(struct afsconf_dir *adir, struct rx_call *acall,
                             char *namep);
extern int afsconf_SuperIdentity(struct afsconf_dir *, struct rx_call *,
				 struct rx_identity **);
extern int afsconf_IsSuperIdentity(struct afsconf_dir *, struct rx_identity *);
extern int afsconf_CheckRestrictedQuery(struct afsconf_dir *adir,
					struct rx_call *acall,
					int needed_level);

/*
 * Level constants for the -restricted_query option used by vlserver
 * and volser.  Once we have vlserver and volserver to ptserver
 * connection, we can add more access levels, like AUTHUSER or
 * AUTHANDFOREIGNUSER.
 */
#define RESTRICTED_QUERY_ANYUSER  0
#define RESTRICTED_QUERY_ADMIN    1

/* realms.c */
extern int afsconf_SetLocalRealm(const char *realm);
extern int afsconf_IsLocalRealmMatch(struct afsconf_dir *dir, afs_int32 * local,
				const char *name, const char *instance,
				const char *cell);

/* netrestrict.c */

extern int afsconf_ParseNetRestrictFile(afs_uint32 outAddrs[],
					afs_uint32 * mask, afs_uint32 * mtu,
					afs_uint32 maxAddrs, afs_uint32 * nAddrs,
					char reason[], const char *fileName);

extern int afsconf_ParseNetFiles(afs_uint32 addrbuf[], afs_uint32 maskbuf[],
				 afs_uint32 mtubuf[], afs_uint32 max,
				 char reason[], const char *niFileName,
				 const char *nrFileName);

/* some well-known ports and their names; new additions to table in cellconfig.c, too */
#define	AFSCONF_FILESERVICE		"afs"
#define	AFSCONF_FILEPORT		7000
#define	AFSCONF_CALLBACKSERVICE		"afscb"
#define	AFSCONF_CALLBACKPORT		7001
#define	AFSCONF_PROTSERVICE		"afsprot"
#define	AFSCONF_PROTPORT		7002
#define	AFSCONF_VLDBSERVICE		"afsvldb"
#define	AFSCONF_VLDBPORT		7003
#define	AFSCONF_KAUTHSERVICE		"afskauth"
#define	AFSCONF_KAUTHPORT		7004
#define	AFSCONF_VOLUMESERVICE		"afsvol"
#define	AFSCONF_VOLUMEPORT		7005
#define	AFSCONF_ERRORSERVICE		"afserror"
#define	AFSCONF_ERRORPORT		7006
#define	AFSCONF_NANNYSERVICE		"afsnanny"
#define	AFSCONF_NANNYPORT		7007
#define	AFSCONF_UPDATESERVICE		"afsupdate"
#define	AFSCONF_UPDATEPORT		7008
#define	AFSCONF_RMTSYSSERVICE		"afsrmtsys"
#define	AFSCONF_RMTSYSPORT		7009
#define AFSCONF_RSDBSERVICE             "afsres"
#define AFSCONF_RESPORT                 7010
#define AFSCONF_REMIODBSERVICE          "afsremio"
#define AFSCONF_REMIOPORT               7011

#endif /* __CELLCONFIG_AFS_INCL_ */
