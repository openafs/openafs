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

#define FORCE_NOAUTH 1
#define FORCE_SECURE 2
#define FORCE_MODE 63
#define FORCE_K5CC 64
#define FORCE_KTC 128
#define FORCE_RXKAD 256
#define FORCE_RXK5 512

#ifndef IPPROTO_MAX
	/* get sockaddr_in */
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#endif
#endif

#define	MAXCELLCHARS	64
#define	MAXHOSTCHARS	64
#define MAXHOSTSPERCELL  8

/*
 * Return codes.
 */
#define	AFSCONF_SUCCESS	  0	/* worked */
#if 0
#define	AFSCONF_FAILURE	  1	/* mysterious failure */
#define	AFSCONF_NOTFOUND  2	/* could not find entry */
#define	AFSCONF_UNKNOWN	  3	/* do not know that information */
#define	AFSCONF_NOCELL	  4	/* line appears before a cell has been defined */
#define	AFSCONF_SYNTAX	  5	/* syntax error */
#define	AFSCONF_NODB	  6	/* a database file is missing */
#define	AFSCONF_FULL	  7	/* no more entries */
#endif

/*
 * Complete server info for one cell.
 */
struct afsconf_cell {
    char name[MAXCELLCHARS];	/*Cell name */
    short numServers;		/*Num active servers for the cell */
    short flags;		/* useful flags */
    struct sockaddr_in hostAddr[MAXHOSTSPERCELL];	/*IP addresses for cell's servers */
    char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];	/*Names for cell's servers */
    char *linkedCell;		/* Linked cell name, if any */
    int timeout;		/* Data timeout, if non-zero */
};

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

struct afsconf_dir {
    char *name;			/* pointer to dir prefix */
    char *cellName;		/* cell name, if any, we're in */
    struct afsconf_entry *entries;	/* list of cell entries */
    struct afsconf_keys *keystr;	/* structure containing keys */
    afs_int32 timeRead;		/* time stamp of file last read */
    struct afsconf_aliasentry *alias_entries;	/* cell aliases */
};

struct afsconf_servPair {
    char *name;
    int port;
};

extern struct afsconf_dir *afsconf_Open(register const char *adir);
extern int afsconf_CellApply(struct afsconf_dir *adir,
			     int (*aproc) (struct afsconf_cell * cell,
					   char *arock,
					   struct afsconf_dir * dir),
			     char *arock);
extern int afsconf_CellAliasApply(struct afsconf_dir *adir,
				  int (*aproc) (struct afsconf_cellalias *
						alias, char *arock,
						struct afsconf_dir * dir),
				  char *arock);
extern int afsconf_GetExtendedCellInfo(struct afsconf_dir *adir,
				       char *acellName, char *aservice,
				       struct afsconf_cell *acellInfo,
				       char clones[]);
extern int afsconf_GetAfsdbInfo(char *acellName, char *aservice,
				struct afsconf_cell *acellInfo);
extern int afsconf_GetCellInfo(struct afsconf_dir *adir, char *acellName,
			       char *aservice,
			       struct afsconf_cell *acellInfo);
extern int afsconf_GetLocalCell(register struct afsconf_dir *adir,
				char *aname, afs_int32 alen);
extern int afsconf_Close(struct afsconf_dir *adir);
extern int have_afs_keyfile(struct afsconf_dir *adir);
extern int afsconf_IntGetKeys(struct afsconf_dir *adir);
extern int afsconf_GetKeys(struct afsconf_dir *adir,
			   struct afsconf_keys *astr);
struct ktc_encryptionKey;
extern afs_int32 afsconf_GetLatestKey(struct afsconf_dir *adir,
			  afs_int32 * avno, struct ktc_encryptionKey *akey);
extern int afsconf_GetKey(void *adir, afs_int32 avno,
			  struct ktc_encryptionKey *akey);
extern int afsconf_AddKey(struct afsconf_dir *adir, afs_int32 akvno,
			  char akey[8], afs_int32 overwrite);
extern int afsconf_DeleteKey(struct afsconf_dir *adir, afs_int32 akvno);

struct rx_securityClass;
extern afs_int32 afsconf_ClientAuth(struct afsconf_dir *adir,
				    struct rx_securityClass **astr,
				    afs_int32 * aindex);             
extern afs_int32
afsconf_ClientAuthSecure(struct afsconf_dir *adir, struct rx_securityClass **astr,
     afs_int32 *aindex);                    
extern afs_int32 afsconf_ClientAuthEx(struct afsconf_dir *adir,
				    struct rx_securityClass **astr,
				    afs_int32 * aindex,
				    afs_int32 flags);
extern afs_int32 afsconf_ServerAuth(void *,
				    struct rx_securityClass **,
				    afs_int32);

extern afs_int32 afsconf_SuperUser();

struct rx_call;
extern int afsconf_CheckAuth(void *,
			    struct rx_call *);

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
