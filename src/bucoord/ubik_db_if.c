/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Interface and supporting routines for the backup system's ubik database */

#include <afsconfig.h>
#include <afs/stds.h>

#include <roken.h>
#include <afs/cmd.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#define UBIK_LEGACY_CALLITER 1
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/volser.h>
#include <afs/volser_prototypes.h>
#include <afs/afsutil.h>
#include <afs/bubasics.h>
#include <afs/budb_client.h>
#include <afs/budb.h>
#include <afs/com_err.h>

#include "bc.h"
#include "error_macros.h"
#include "bucoord_internal.h"
#include "bucoord_prototypes.h"

extern char *whoami;

/* -------------------------------------
 * Globals
 * -------------------------------------
 */

struct udbHandleS udbHandle;

/* -------------------------------------
 * interface routines (alphabetic)
 * -------------------------------------
 */

afs_int32 bcdb_AddVolume(struct budb_volumeEntry *veptr)
{
    afs_int32 code;

    code = ubik_BUDB_AddVolume(udbHandle.uh_client, 0, veptr);
    return (code);
}

afs_int32 bcdb_AddVolumes(struct budb_volumeEntry *veptr, afs_int32 count)
{
    struct budb_volumeList volumeList;
    afs_int32 code;

    volumeList.budb_volumeList_len = count;
    volumeList.budb_volumeList_val = veptr;
    code = ubik_BUDB_AddVolumes(udbHandle.uh_client, 0, &volumeList);
    return (code);
}


afs_int32 bcdb_CreateDump(struct budb_dumpEntry *deptr)
{
    afs_int32 code;

    code = ubik_BUDB_CreateDump(udbHandle.uh_client, 0, deptr);
    return (code);
}

afs_int32 bcdb_deleteDump(afs_int32 dumpID, afs_int32 fromTime, afs_int32 toTime,
  budb_dumpsList *dumps)
{
    afs_int32 code;
    budb_dumpsList dumpsList, *dumpsPtr;

    dumpsList.budb_dumpsList_len = 0;
    dumpsList.budb_dumpsList_val = 0;
    dumpsPtr = (dumps ? dumps : &dumpsList);

    code =
	ubik_BUDB_DeleteDump(udbHandle.uh_client, 0, dumpID, fromTime,
		  toTime, dumpsPtr);
    if (dumpsList.budb_dumpsList_val)
	free(dumpsList.budb_dumpsList_val);
    return (code);
}

afs_int32 bcdb_listDumps (afs_int32 sflags, afs_int32 groupId,
			  afs_int32 fromTime, afs_int32 toTime,
			  budb_dumpsList *dumps, budb_dumpsList *flags)
{
    afs_int32 code;
    budb_dumpsList dumpsList, *dumpsPtr;
    budb_dumpsList flagsList, *flagsPtr;

    dumpsList.budb_dumpsList_len = 0;
    dumpsList.budb_dumpsList_val = 0;
    dumpsPtr = (dumps ? dumps : &dumpsList);

    flagsList.budb_dumpsList_len = 0;
    flagsList.budb_dumpsList_val = 0;
    flagsPtr = (flags ? flags : &flagsList);

    code =
	ubik_BUDB_ListDumps(udbHandle.uh_client, 0, sflags, "", groupId,
		  fromTime, toTime, dumpsPtr, flagsPtr);

    if (dumpsList.budb_dumpsList_val)
	free(dumpsList.budb_dumpsList_val);
    if (flagsList.budb_dumpsList_val)
	free(flagsList.budb_dumpsList_val);

    if (code == 0 &&
	dumpsPtr->budb_dumpsList_len != flagsPtr->budb_dumpsList_len) {
	code = BUDB_INTERNALERROR;
    }

    return (code);
}


afs_int32 bcdb_DeleteVDP(char *dumpSetName, char *dumpPath, afs_int32 dumpID)
{
    afs_int32 code;

    code =
	ubik_BUDB_DeleteVDP(udbHandle.uh_client, 0, dumpSetName,
		  dumpPath, dumpID);
    return (code);
}

/* bcdb_FindClone
 *      Returns the clone time of a volume by going up the parent chain.
 *      If no clone time is found, a clone time of 0 is returned, forcing
 *      a full dump.
 * entry:
 *      dumpID - of the first dump to examine.
 *      volName - name of the volume for whom a clone time is required
 *      clonetime - ptr to vbl for returning result
 * exit:
 *      0 - clonetime set appropriately
 *      -1 - error occured in traversing chain, clone time set to 0.
 *      -2 - no clone times found, clone time set to 0
 */

afs_int32 bcdb_FindClone(afs_int32 dumpID, char *volName, afs_int32 *clonetime)
{
    afs_int32 code;
    code =
	ubik_BUDB_FindClone(udbHandle.uh_client, 0, dumpID, volName,
		  clonetime);
    return (code);
}

/* bcdb_FindDump
 *      scan entire database for latest volume dump before adate.  Optimize
 *      further by reading only the first line of the dump and if it is older
 *      than the oldest acceptable dump we've found so far, we don't bother
 *      scanning the dump file we've just opened
 *
 *      Used by restore code when restoring a user requested volume(s)
 * entry:
 *      volumeName - name of volume to match on
 *      beforeDate - look for dumps older than this date
 * exit:
 *      deptr - desciptor of most recent dump
 * notes:
 *      should be able to implement this in a single call rather than
 *      the current multiple bcdb_ call algorithm.
 */

int
bcdb_FindDump(char *volumeName, afs_int32 beforeDate,
	      struct budb_dumpEntry *deptr)
{
    afs_int32 code;
    code =
	ubik_BUDB_FindDump(udbHandle.uh_client, 0, volumeName,
		  beforeDate, deptr);
    return (code);
}

/* bcdb_FindDumpByID
 *	find a dump by id. Currently insists on a single return value.
 * entry:
 *	dumpID - id to lookup
 * exit:
 */
int
bcdb_FindDumpByID(afs_int32 dumpID, struct budb_dumpEntry *deptr)
{
    afs_int32 code;
    afs_int32 nextindex;
    afs_int32 dbTime;
    budb_dumpList dl;

    /* initialize the dump list */
    dl.budb_dumpList_len = 0;
    dl.budb_dumpList_val = 0;

    /* outline algorithm */
    code = ubik_BUDB_GetDumps(udbHandle.uh_client, 0, BUDB_MAJORVERSION, BUDB_OP_DUMPID, "",	/* no name */
		     dumpID,	/* start */
		     0,		/* end */
		     0,		/* index */
		     &nextindex, &dbTime, &dl);

    if ((code != 0)
	|| (dl.budb_dumpList_len != 1)	/* single retn val expected */
	) {
/*	printf("bcdb_FindDumpByID: code %d, nvalues %d\n",
	       code, dl.budb_dumpList_len); */
	if (code == 0)
	    code = 1;		/* multiple id's */
	goto error;
    }

    memcpy(deptr, dl.budb_dumpList_val, sizeof(*deptr));

  exit:
    if (dl.budb_dumpList_val) {
	/* free any allocated structures */
	free(dl.budb_dumpList_val);
    }
    return (code);

  error:
    memset(deptr, 0, sizeof(*deptr));
    goto exit;
}

/* bcdb_FindLastVolClone
 *      Returns the clone time, from the most recent dump of volName, when
 *      dumped in the volume set volSetName, with dump schedule dumpName.
 *      The clone time can be used to check if the volume has been correctly
 *      re-cloned, and also is used as the time from which to do the current
 *      incremental dump.
 * entry:
 *      volSetName - name of volume set
 *      dumpName - full path of dump node
 *      volName - name of volume for whom a clone time is required
 *      clonetime - ptr to vbl for result
 * exit:
 *      0 - clonetime set appropriately
 * notes:
 *      used only for warning generation. Suggest that this be omitted.
 */

afs_int32
bcdb_FindLastVolClone(char *volSetName, char *dumpName, char *volName,
		      afs_int32 *clonetime)
{
    /* server notes
     * search by dumpName
     * match on volumeset and dump path
     * search for the volume name
     */
    return (0);
}

/* bcdb_FindLatestDump
 *      find the latest dump with volume set component avname and the
 *      specified dump pathname. Used to find a dump, relative to which an
 *      incremental dump can be done. Defines the parent <-> child relations
 *      for restores.
 * entry:
 *      avname: volume set name
 *      dumpPath: full path of dump node
 * exit:
 *      0:  adentry: dump entry structure filled in.
 *      -1: probably an internal error
 *      2: no such entry
 * Notes for 4.0:
 *      Need to store volumeset name in dump in order to implement this.
 *      Need new routine since params are two strings
 */

int
bcdb_FindLatestDump(char *volSetName, char *dumpPath,
		    struct budb_dumpEntry *deptr)
{
    afs_int32 code;
    code =
	ubik_BUDB_FindLatestDump(udbHandle.uh_client, 0, volSetName,
		  dumpPath, deptr);
    return (code);
}


/* bcdb_FindTape
 *	find a tape
 * entry:
 *	dumpid: dump id to which tape beint32s
 *	tapeName: name of tape
 */

int
bcdb_FindTape(afs_int32 dumpid, char *tapeName,
	      struct budb_tapeEntry *teptr)
{
    budb_tapeList tl;
    afs_int32 next;
    afs_int32 dbTime;
    afs_int32 code = 0;

    memset(teptr, 0, sizeof(*teptr));
    tl.budb_tapeList_len = 0;
    tl.budb_tapeList_val = 0;

    code =
	ubik_BUDB_GetTapes(udbHandle.uh_client, 0, BUDB_MAJORVERSION,
		  BUDB_OP_TAPENAME | BUDB_OP_DUMPID, tapeName, dumpid, 0, 0,
		  &next, &dbTime, &tl);

    if (code)
	ERROR(code);

    if (tl.budb_tapeList_len != 1)
	ERROR(BC_NOTUNIQUE);	/* expecting a single descriptor */

    memcpy(teptr, tl.budb_tapeList_val, sizeof(*teptr));

  error_exit:
    if (tl.budb_tapeList_val)
	free(tl.budb_tapeList_val);
    return (code);
}

int
bcdb_FindTapeSeq(afs_int32 dumpid, afs_int32 tapeSeq,
		 struct budb_tapeEntry *teptr)
{
    budb_tapeList tl;
    afs_int32 next;
    afs_int32 dbTime;
    afs_int32 code = 0;

    memset(teptr, 0, sizeof(*teptr));
    tl.budb_tapeList_len = 0;
    tl.budb_tapeList_val = 0;

    code =
	ubik_BUDB_GetTapes(udbHandle.uh_client, 0, BUDB_MAJORVERSION,
		  BUDB_OP_TAPESEQ | BUDB_OP_DUMPID, "", dumpid, tapeSeq, 0,
		  &next, &dbTime, &tl);
    if (code)
	ERROR(code);

    if (tl.budb_tapeList_len != 1)
	ERROR(BC_NOTUNIQUE);	/* expecting a single descriptor */

    memcpy(teptr, tl.budb_tapeList_val, sizeof(*teptr));

  error_exit:
    if (tl.budb_tapeList_val)
	free(tl.budb_tapeList_val);
    return (code);
}

/* bcdb_FindVolumes
 * notes:
 *	- this is part of dblookup. The existing semantics will not work since
 *	they do lookups based on dump id.
 *	- in the restore code, it uses this to extract information about
 *	the volume. Need current semantics. Could filter the output, selecting
 *	on the dumpid.
 *	- Suggest that the lookup be based on volume name only, with optional
 *	match on backup, and readonly volumes.
 *	- Further, need to check if the volume structure returns enough
 *	information
 */

afs_int32
bcdb_FindVolumes(afs_int32 dumpID, char *volumeName,
		 struct budb_volumeEntry *returnArray,
		 afs_int32 last, afs_int32 *next, afs_int32 maxa,
		 afs_int32 *nEntries)
{
    budb_volumeList vl;
    afs_int32 dbTime;
    afs_int32 code;

    vl.budb_volumeList_len = maxa;
    vl.budb_volumeList_val = returnArray;

    /* outline algorithm */
    code = ubik_BUDB_GetVolumes(udbHandle.uh_client, 0, BUDB_MAJORVERSION, BUDB_OP_VOLUMENAME | BUDB_OP_DUMPID, volumeName,	/* name */
		     dumpID,	/* start */
		     0,		/* end */
		     last,	/* index */
		     next,	/* nextindex */
		     &dbTime, &vl);

    *nEntries = vl.budb_volumeList_len;
    return (code);
}

int
bcdb_FinishDump(struct budb_dumpEntry *deptr)
{
    afs_int32 code;
    code = ubik_BUDB_FinishDump(udbHandle.uh_client, 0, deptr);
    return (code);
}

int
bcdb_FinishTape(struct budb_tapeEntry *teptr)
{
    afs_int32 code;
    code = ubik_BUDB_FinishTape(udbHandle.uh_client, 0, teptr);
    return (code);

}

/* bcdb_LookupVolumes
 */

afs_int32
bcdb_LookupVolume(char *volumeName, struct budb_volumeEntry *returnArray,
		  afs_int32 last, afs_int32 *next, afs_int32 maxa,
		  afs_int32 *nEntries)
{
    budb_volumeList vl;
    afs_int32 dbTime;
    afs_int32 code;

    vl.budb_volumeList_len = maxa;
    vl.budb_volumeList_val = returnArray;

    /* outline algorithm */
    code = ubik_BUDB_GetVolumes(udbHandle.uh_client, 0, BUDB_MAJORVERSION, BUDB_OP_VOLUMENAME, volumeName,	/* name */
		     0,		/* start */
		     0,		/* end */
		     last,	/* index */
		     next,	/* nextindex */
		     &dbTime, &vl);
    if (code) {
	*nEntries = 0;
	return (code);
    }
    *nEntries = vl.budb_volumeList_len;
    return (0);
}

int
bcdb_UseTape(struct budb_tapeEntry *teptr, afs_int32 *newFlag)
{
    afs_int32 code;
    code = ubik_BUDB_UseTape(udbHandle.uh_client, 0, teptr, newFlag);
    return (code);
}


/* ---- text configuration handling routines ----
 *
 * notes:
 *	The caller should pass in/out a fid for an unlinked, open file to prevent
 *	tampering with the files contents;
 */

/* bcdb_GetTextFile
 *	extract the specified textType and put it in a temporary, local
 *	file.
 * entry:
 *	ctPtr - ptr to client structure with all the required information
 */

int
bcdb_GetTextFile(udbClientTextP ctPtr)
{
    afs_int32 bufferSize;
    afs_int32 offset, nextOffset;
    charListT charList;
    afs_int32 code = 0;

    /* Initialize charlistT_val. We try to deallocate this structure based on
     * this */
    memset((void *)&charList, 0, sizeof(charList));

    /* check params and cleanup any previous state */
    if (ctPtr->lockHandle == 0)
	ERROR(BUDB_INTERNALERROR);

    if (ctPtr->textStream == NULL)	/* Should have an open stream */
	ERROR(BUDB_INTERNALERROR);

    /* allocate a buffer */
    bufferSize = 1024;
    charList.charListT_val = malloc(bufferSize);
    if (charList.charListT_val == 0)
	ERROR(BUDB_INTERNALERROR);
    charList.charListT_len = bufferSize;

    nextOffset = 0;
    ctPtr->textSize = 0;
    while (nextOffset != -1) {
	offset = nextOffset;
	charList.charListT_len = bufferSize;
	code =
	    ubik_BUDB_GetText(udbHandle.uh_client, 0, ctPtr->lockHandle,
		      ctPtr->textType, bufferSize, offset, &nextOffset,
		      &charList);

	if (code)
	    ERROR(code);

	code =
	    fwrite(charList.charListT_val, sizeof(char),
		   charList.charListT_len, ctPtr->textStream);
	if (ferror(ctPtr->textStream))
	    ERROR(BUDB_INTERNALERROR);

	ctPtr->textSize += charList.charListT_len;
    }

    /* get text version */
    code =
	ubik_BUDB_GetTextVersion(udbHandle.uh_client, 0,
		  ctPtr->textType, &ctPtr->textVersion);
    if (code)
	ERROR(code);

  normal_exit:
    fflush(ctPtr->textStream);	/* debug */

    /* exit, leaving the configuration text file open */
    if (charList.charListT_val)
	free(charList.charListT_val);
    return (code);

  error_exit:
    if (ctPtr->textStream != NULL) {
	fclose(ctPtr->textStream);
	ctPtr->textStream = NULL;
    }
    goto normal_exit;
}


/* bcdb_SaveTextFile
 *	save the text file in ubik database
 * entry:
 *	textType - identifies type of configuration file
 *	filename - where to get the text from
 */

int
bcdb_SaveTextFile(udbClientTextP ctPtr)
{
    afs_int32 bufferSize;
    afs_int32 offset, chunkSize, fileSize;
    charListT charList;
    afs_int32 code = 0;

    /* allocate a buffer */
    bufferSize = 1024;
    charList.charListT_val = malloc(bufferSize);
    if (charList.charListT_val == 0)
	ERROR(BUDB_INTERNALERROR);
    charList.charListT_len = bufferSize;

    if (ctPtr->textStream == NULL)
	ERROR(BUDB_INTERNALERROR);
    rewind(ctPtr->textStream);

    fileSize = (afs_int32) filesize(ctPtr->textStream);

    afs_dprintf(("filesize is %d\n", fileSize));

    rewind(ctPtr->textStream);

    /* special case empty files */
    if (fileSize == 0) {
	charList.charListT_len = 0;
	code =
	    ubik_BUDB_SaveText(udbHandle.uh_client, 0,
		      ctPtr->lockHandle, ctPtr->textType, 0,
		      BUDB_TEXT_COMPLETE, &charList);
	goto error_exit;
    }

    offset = 0;
    while (fileSize != 0) {
	chunkSize = min(fileSize, bufferSize);
	code =
	    fread(charList.charListT_val, sizeof(char), chunkSize,
		  ctPtr->textStream);

	if (code != chunkSize)
	    printf("code = %d\n", code);
	if (ferror(ctPtr->textStream))
	    ERROR(BUDB_INTERNALERROR);

	charList.charListT_len = chunkSize;
	code =
	    ubik_BUDB_SaveText(udbHandle.uh_client, 0,
		      ctPtr->lockHandle, ctPtr->textType, offset,
		      (chunkSize == fileSize) ? BUDB_TEXT_COMPLETE : 0,
		      &charList);
	if (code)
	    ERROR(code);

	fileSize -= chunkSize;
	offset += chunkSize;
    }

  error_exit:
    /* if ( ctPtr->textStream >= 0 )
     * close(ctPtr->textStream); */
    if (charList.charListT_val)
	free(charList.charListT_val);
    return (code);
}

int
bcdb_FindLastTape(afs_int32 dumpID, struct budb_dumpEntry *dumpEntry,
		  struct budb_tapeEntry *tapeEntry,
		  struct budb_volumeEntry *volEntry)
{
    return (ubik_BUDB_FindLastTape(udbHandle.uh_client, 0, dumpID, dumpEntry,
	     tapeEntry, volEntry));
}

int
bcdb_MakeDumpAppended(afs_int32 appendedDumpID, afs_int32 initialDumpID,
		      afs_int32 startTapeSeq)
{
    return (ubik_BUDB_MakeDumpAppended(udbHandle.uh_client, 0, appendedDumpID,
	     initialDumpID, startTapeSeq));
}


/* -------------------------------------
 * misc. support routines
 * -------------------------------------
 */

afs_int32
filesize(FILE *stream)
{
    afs_int32 offset;
    afs_int32 size;

    offset = ftell(stream);
    fseek(stream, (afs_int32) 0, 2);	/* end of file */
    size = ftell(stream);
    fseek(stream, offset, 0);
    return (size);
}


/* ------------------------------------
 * misc. support routines - general text management
 * ------------------------------------
 */


/* bc_LockText
 *	locks the text described by the ctPtr
 * entry:
 *	ctptr - client text ptr
 * exit:
 *	0 - success
 *	n - fail
 */

int
bc_LockText(udbClientTextP ctPtr)
{
    afs_int32 code;
    afs_int32 timeout, j = 0;

    if (ctPtr->lockHandle != 0)
	return (1);		/* already locked */

    timeout =
	((ctPtr->textSize == 0) ? 30 : ((ctPtr->textSize / 50000) + 10));

    while (1) {
	code =
	    ubik_BUDB_GetLock(udbHandle.uh_client, 0,
		      udbHandle.uh_instanceId, ctPtr->textType, timeout,
		      &ctPtr->lockHandle);
	if ((code != BUDB_LOCKED) && (code != BUDB_SELFLOCKED)) {
	    break;
	}

	/* Mention something every 30 seconds */
	if (++j >= 30) {
	    afs_com_err(whoami, code,
		    "; Waiting for db configuration text unlock");
	    j = 0;
	}
#ifdef AFS_PTHREAD_ENV
	sleep(1);
#else
	IOMGR_Sleep(1);
#endif
    }

    /* cleanup */
    if (code)
	ctPtr->lockHandle = 0;
    return (code);
}

/* bc_UnlockText
 *	unlocks the text described by the ctPtr
 * entry:
 *	ctptr - client text ptr
 * exit:
 *	0 - success
 *	n - fail
 */

int
bc_UnlockText(udbClientTextP ctPtr)
{
    afs_int32 code = 0;

    if (ctPtr->lockHandle == 0)
	return (0);

    code =
	ubik_BUDB_FreeLock(udbHandle.uh_client, 0, ctPtr->lockHandle);
    ctPtr->lockHandle = 0;

    /* Don't try to analyse the error. Let the lock timeout */
    return (code);
}

/* bc_CheckTextVersion
 * exit:
 *	0 - version # ok
 *	n - out of date or error
 */

int
bc_CheckTextVersion(udbClientTextP ctPtr)
{
    afs_int32 code;
    afs_uint32 tversion;

    if (ctPtr->textVersion == -1)
	return (BC_VERSIONMISMATCH);

    code =
	ubik_BUDB_GetTextVersion(udbHandle.uh_client, 0,
		  ctPtr->textType, &tversion);
    if (code)
	return (code);
    if (tversion != ctPtr->textVersion)
	return (BC_VERSIONMISMATCH);
    return (0);
}

/* -------------------------------------
 * initialization routines
 * -------------------------------------
 */

static afsconf_secflags
parseSecFlags(int noAuthFlag, int localauth, const char **confdir) {
    afsconf_secflags secFlags;

    secFlags = 0;
    if (noAuthFlag)
	secFlags |= AFSCONF_SECOPTS_NOAUTH;

    if (localauth) {
	secFlags |= AFSCONF_SECOPTS_LOCALAUTH;
	*confdir = AFSDIR_SERVER_ETC_DIRPATH;
    } else {
	*confdir = AFSDIR_CLIENT_ETC_DIRPATH;
    }
    return secFlags;
}

/* vldbClientInit
 *      Initialize a client for the vl ubik database.
 */
int
vldbClientInit(int noAuthFlag, int localauth, char *cellName,
	       struct ubik_client **cstruct,
	       time_t *expires)
{
    afs_int32 code = 0;
    struct afsconf_dir *acdir;
    struct rx_securityClass *sc;
    afs_int32 i, scIndex = RX_SECIDX_NULL;
    struct afsconf_cell info;
    struct rx_connection *serverconns[VLDB_MAXSERVERS];
    afsconf_secflags secFlags;
    const char *confdir;

    secFlags = parseSecFlags(noAuthFlag, localauth, &confdir);
    secFlags |= AFSCONF_SECOPTS_FALLBACK_NULL;

    /* This just preserves old behaviour of using the default cell when
     * passed an empty string */
    if (cellName && cellName[0] == '\0')
	cellName = NULL;

    /* Find out about the given cell */
    acdir = afsconf_Open(confdir);
    if (!acdir) {
	afs_com_err(whoami, 0, "Can't open configuration directory '%s'", confdir);
	ERROR(BC_NOCELLCONFIG);
    }

    code = afsconf_GetCellInfo(acdir, cellName, AFSCONF_VLDBSERVICE, &info);
    if (code) {
	afs_com_err(whoami, code, "; Can't find cell %s's hosts in %s/%s",
		    cellName, confdir, AFSDIR_CELLSERVDB_FILE);
	ERROR(BC_NOCELLCONFIG);
    }

    code = afsconf_PickClientSecObj(acdir, secFlags, &info, cellName,
				    &sc, &scIndex, expires);
    if (code) {
	afs_com_err(whoami, code, "(configuring connection security)");
	ERROR(BC_NOCELLCONFIG);
    }
    if (scIndex == RX_SECIDX_NULL && !noAuthFlag)
	afs_com_err(whoami, 0, "Can't get tokens - running unauthenticated");

    /* tell UV module about default authentication */
    UV_SetSecurity(sc, scIndex);

    if (info.numServers > VLDB_MAXSERVERS) {
	afs_com_err(whoami, 0,
		"Warning: %d VLDB servers exist for cell '%s', can only remember the first %d",
		info.numServers, cellName, VLDB_MAXSERVERS);
	info.numServers = VLDB_MAXSERVERS;
    }

    for (i = 0; i < info.numServers; i++)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, USER_SERVICE_ID, sc,
			     scIndex);
    serverconns[i] = 0;

    *cstruct = 0;
    code = ubik_ClientInit(serverconns, cstruct);
    if (code) {
	afs_com_err(whoami, code, "; Can't initialize ubik connection to vldb");
	ERROR(code);
    }

  error_exit:
    if (acdir)
	afsconf_Close(acdir);
    return (code);
}

/* udbClientInit
 *	initialize a client for the backup systems ubik database.
 */

afs_int32
udbClientInit(int noAuthFlag, int localauth, char *cellName)
{
    struct afsconf_cell info;
    struct afsconf_dir *acdir;
    const char *confdir;
    int i;
    afs_int32 secFlags;
    afs_int32 code = 0;

    secFlags = parseSecFlags(noAuthFlag, localauth, &confdir);
    secFlags |= AFSCONF_SECOPTS_FALLBACK_NULL;

    if (cellName && cellName[0] == '\0')
	cellName = NULL;

    acdir = afsconf_Open(confdir);
    if (!acdir) {
	afs_com_err(whoami, 0, "Can't open configuration directory '%s'",
		    confdir);
	ERROR(BC_NOCELLCONFIG);
    }

    code = afsconf_GetCellInfo(acdir, cellName, 0, &info);
    if (code) {
	afs_com_err(whoami, code, "; Can't find cell %s's hosts in %s/%s",
		    cellName, confdir, AFSDIR_CELLSERVDB_FILE);
	ERROR(BC_NOCELLCONFIG);
    }

    code = afsconf_PickClientSecObj(acdir, secFlags, &info, cellName,
				    &udbHandle.uh_secobj,
				    &udbHandle.uh_scIndex, NULL);
    if (code) {
	afs_com_err(whoami, code, "(configuring connection security)");
	ERROR(BC_NOCELLCONFIG);
    }
    if (udbHandle.uh_scIndex == RX_SECIDX_NULL && !noAuthFlag)
	afs_com_err(whoami, 0, "Can't get tokens - running unauthenticated");

    /* We have to have space for the trailing NULL that terminates the server
     * conneciton array - so we can only store MAXSERVERS-1 real elements in
     * that array.
     */
    if (info.numServers >= MAXSERVERS) {
	afs_com_err(whoami, 0,
		"Warning: %d BDB servers exist for cell '%s', can only remember the first %d",
		info.numServers, cellName, MAXSERVERS-1);
	info.numServers = MAXSERVERS - 1;
    }

    /* establish connections to the servers. Check for failed connections? */
    for (i = 0; i < info.numServers; i++) {
	udbHandle.uh_serverConn[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     htons(AFSCONF_BUDBPORT), BUDB_SERVICE,
			     udbHandle.uh_secobj, udbHandle.uh_scIndex);
    }
    udbHandle.uh_serverConn[i] = 0;

    code = ubik_ClientInit(udbHandle.uh_serverConn, &udbHandle.uh_client);
    if (code) {
	afs_com_err(whoami, code,
		"; Can't initialize ubik connection to backup database");
	ERROR(code);
    }

    /* Try to quickly find a good site by setting deadtime low */
    for (i = 0; i < info.numServers; i++)
	rx_SetConnDeadTime(udbHandle.uh_client->conns[i], 1);
    code =
	ubik_BUDB_GetInstanceId(udbHandle.uh_client, 0,
		  &udbHandle.uh_instanceId);

    /* Reset dead time back up to default */
    for (i = 0; i < info.numServers; i++)
	rx_SetConnDeadTime(udbHandle.uh_client->conns[i], 60);

    /* If did not find a site on first quick pass, try again */
    if (code == -1)
	code =
	    ubik_BUDB_GetInstanceId(udbHandle.uh_client, 0,
		      &udbHandle.uh_instanceId);
    if (code) {
	afs_com_err(whoami, code, "; Can't access backup database");
	ERROR(code);
    }

  error_exit:
    if (acdir)
	afsconf_Close(acdir);
    return (code);
}

/* -------------------------------------
 * specialized ubik support
 * -------------------------------------
 */

#include <rx/xdr.h>
#include <rx/rx.h>
#include <lock.h>

/* notes
 *	1) first call with SINGLESERVER set, record the server to be used.
 *	2) subsequent calls use that server. If a failure is encountered,
 *	   the state is cleaned up and the error returned back to the caller.
 *	3) upon completion, the user must make a dummy call with
 *	   END_SINGLESERVER set, to clean up state.
 *	4) if the vanilla ubik_Call is ever modified, the END_SINGLESERVER
 *	   flag can be discarded. The first call without SINGLESERVER set
 *	   can clean up the state.
 */

struct ubikCallState {
    afs_int32 ucs_flags;	/* state flags */
    afs_int32 ucs_selectedServer;	/* which server selected */
};

static struct ubikCallState uServer;

/* ubik_Call_SingleServer
 *	variant of ubik_Call. This is used by the backup system to initiate
 *	a series of calls to a single ubik server. The first call sets up
 *	process state (not recorded in ubik) that requires all further calls
 *	of that group to be made to the same server process.
 *
 *	call this instead of stub and we'll guarantee to find a host that's up.
 * 	in the future, we should also put in a protocol to find the sync site
 */

static afs_int32
ubik_Call_SingleServer(ubik_call_func aproc, struct ubik_client *aclient,
		       afs_int32 aflags, long p1, long p2, long p3,
		       long p4, long p5, long p6, long p7, long p8,
		       long p9, long p10, long p11, long p12, long p13,
		       long p14, long p15, long p16)
{
    afs_int32 code;
    afs_int32 someCode, newHost, thisHost;
    afs_int32 i;
    afs_int32 count;
    int chaseCount;
    int pass;
    struct rx_connection *tc;
    struct rx_peer *rxp;

    if ((aflags & (UF_SINGLESERVER | UF_END_SINGLESERVER)) != 0) {
	if (((aflags & UF_SINGLESERVER) != 0)
	    && ((uServer.ucs_flags & UF_SINGLESERVER) != 0)
	    ) {

	    /* have a selected server */
	    tc = aclient->conns[uServer.ucs_selectedServer];

	    code =
		(*aproc) (tc, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11,
			  p12, p13, p14, p15, p16);
	    if (code) {
		/* error. Clean up single server state */
		memset(&uServer, 0, sizeof(uServer));
	    }
	    return (code);
	} else if ((aflags & UF_END_SINGLESERVER) != 0) {
	    memset(&uServer, 0, sizeof(uServer));
	    return (0);
	}
    }

    someCode = UNOSERVERS;
    chaseCount = 0;
    pass = 0;
    count = 0;
    while (1) {			/*w */

	/* tc is the next conn to try */
	tc = aclient->conns[count];
	if (tc == 0) {
	    if (pass == 0) {
		pass = 1;	/* in pass 1, we look at down hosts, too */
		count = 0;
		continue;
	    } else
		break;		/* nothing left to try */
	}
	if (pass == 0 && (aclient->states[count] & CFLastFailed)) {
	    count++;
	    continue;		/* this guy's down, try someone else first */
	}

	code =
	    (*aproc) (tc, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12,
		      p13, p14, p15, p16);

	/* note that getting a UNOTSYNC error code back does *not* guarantee
	 * that there is a sync site yet elected.  However, if there is a
	 * sync site out there somewhere, and you're trying an operation that
	 * requires a sync site, ubik will return UNOTSYNC, indicating the
	 * operation won't work until you find a sync site
	 */
	if (code == UNOTSYNC) {	/*ns */
	    /* means that this requires a sync site to work */
	    someCode = code;	/* remember an error, if this fails */

	    /* now see if we can find the sync site host */
	    code = VOTE_GetSyncSite(tc, &newHost);
	    if (code == 0 && newHost != 0) {
		newHost = htonl(newHost);	/* convert back to network order */

		/* position count at the appropriate slot in the client
		 * structure and retry. If we can't find in slot, we'll just
		 * continue through the whole list
		 */
		for (i = 0; i < MAXSERVERS; i++) {	/*f */
		    rxp = rx_PeerOf(aclient->conns[i]);
		    if (!(thisHost = rx_HostOf(rxp))) {
			count++;	/* host not found, try the next dude */
			break;
		    }
		    if (thisHost == newHost) {
			/* avoid asking in a loop */
			if (chaseCount++ > 2)
			    break;
			count = i;	/* we were told to use this one */
			break;
		    }
		}		/*f */
	    } else
		count++;	/* not directed, keep looking for a sync site */
	    continue;
	} /*ns */
	else if (code == UNOQUORUM) {	/* this guy is still recovering */
	    someCode = code;
	    count++;
	    continue;
	} else if (code < 0) {	/* network errors */
	    someCode = code;
	    aclient->states[count] |= CFLastFailed;
	    count++;
	    continue;
	} else {
	    /* ok, operation worked */
	    aclient->states[count] &= ~CFLastFailed;
	    /* either misc ubik code, or misc application code (incl success)
	     */

	    /* if the call succeeded, setup connection state for subsequent
	     * calls
	     */
	    if ((code == 0)
		&& ((aflags & UF_SINGLESERVER) != 0)
		) {
		/* need to save state */
		uServer.ucs_flags = UF_SINGLESERVER;
		uServer.ucs_selectedServer = count;
	    }

	    return code;
	}
    }				/*w */
    return someCode;
}

int
ubik_Call_SingleServer_BUDB_GetVolumes(struct ubik_client *aclient,
				       afs_int32 aflags, afs_int32 majorVersion,
				       afs_int32 flags, const char *name,
				       afs_int32 start, afs_int32 end,
				       afs_int32 index, afs_int32 *nextIndex,
				       afs_int32 *dbUpdate,
				       budb_volumeList *volumes)
{
    return ubik_Call_SingleServer((ubik_call_func)BUDB_GetVolumes, aclient, aflags,
				  (long)majorVersion, (long)flags, (long)name,
				  (long)start, (long)end, (long)index,
				  (long)nextIndex, (long)dbUpdate,
				  (long)volumes, 0, 0, 0, 0, 0, 0, 0);
}

int
ubik_Call_SingleServer_BUDB_DumpDB(struct ubik_client *aclient,
				   afs_int32 aflags, int firstcall,
				   afs_int32 maxLength, charListT *charListPtr,
				   afs_int32 *flags)
{
    return ubik_Call_SingleServer((ubik_call_func)BUDB_DumpDB, aclient, aflags, (long)firstcall,
				  (long)maxLength, (long)charListPtr,
				  (long)flags, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				  0);
}

/* -------------------------------------
 * debug and test routines
 * -------------------------------------
 */

/* udbLocalInit
 *	For testing only. Open a connect to the database server running on
 *	the local host
 * exit:
 *	0 - ubik connection established in the global udbHandle structure
 *	n - error.
 */

int
udbLocalInit(void)
{
    afs_uint32 serverList[MAXSERVERS];
    char hostname[256];
    char *args[3];
    int i;
    afs_int32 code;

    /* get our host name */
    gethostname(hostname, sizeof(hostname));
    /* strcpy(hostname, "hops"); */

    args[0] = "";
    args[1] = "-servers";
    args[2] = hostname;

    code = ubik_ParseClientList(3, args, serverList);
    if (code) {
	afs_com_err(whoami, code, "; udbLocalInit: parsing ubik server list");
	return (-1);
    }

    udbHandle.uh_scIndex = RX_SECIDX_NULL;
    udbHandle.uh_secobj = (struct rx_securityClass *)
	rxnull_NewClientSecurityObject();

    for (i = 0; serverList[i] != 0; i++) {
	udbHandle.uh_serverConn[i] =
	    rx_NewConnection(serverList[i], htons(AFSCONF_BUDBPORT),
			     BUDB_SERVICE, udbHandle.uh_secobj,
			     udbHandle.uh_scIndex);
	if (udbHandle.uh_serverConn[i] == 0) {
	    afs_com_err(whoami, 0, "connection %d failed", i);
	    continue;
	}
    }
    udbHandle.uh_serverConn[i] = 0;
    code = ubik_ClientInit(udbHandle.uh_serverConn, &udbHandle.uh_client);
    if (code) {
	afs_com_err(whoami, code, "; in ubik_ClientInit");
	return (code);
    }

    code =
	ubik_BUDB_GetInstanceId(udbHandle.uh_client, 0,
		  &udbHandle.uh_instanceId);
    if (code) {
	afs_com_err(whoami, code, "; Can't estblish instance Id");
	return (code);
    }

   /* abort: */
    return (0);
}

/* bc_openTextFile - This function opens a temp file to read in the
 * config text recd from the bu server. On Unix, an unlink() is done on
 * the file as soon as it is opened, so when the program exits, the file will
 * be removed automatically, while being invisible while in use.
 * On NT, however, the file must be explicitly deleted after use with an unlink()
 * Input:
 *  Pointer to a udhClientTextP struct. The open stream ptr is stored in
 *           the udbClientTextP.textStream member.
 * Output: The temp file name is returned in tmpFileName. This should be used
 *   to delete the file when done with it.
 * Return Values:
 *     !0: error code
 *     0: Success.
 */
int
bc_openTextFile(udbClientTextP ctPtr, char *tmpFileName)
{
    int code = 0;
    int fd;

    if (ctPtr->textStream != NULL) {
	fclose(ctPtr->textStream);
	ctPtr->textStream = NULL;
    }

    sprintf(tmpFileName, "%s/bu_XXXXXX", gettmpdir());
    fd = mkstemp(tmpFileName);
    if (fd == -1)
	ERROR(BUDB_INTERNALERROR);
    ctPtr->textStream = fdopen(fd, "w+");
    if (ctPtr->textStream == NULL)
	ERROR(BUDB_INTERNALERROR);

#ifndef AFS_NT40_ENV		/* This can't be done on NT */
    /* make the file invisible to others */
    code = unlink(tmpFileName);
    if (code)
	ERROR(errno);
#endif

    afs_dprintf(("file is %s\n", tmpFileName));

  normal_exit:
    return code;

  error_exit:
    if (ctPtr->textStream != NULL) {
	fclose(ctPtr->textStream);
	ctPtr->textStream = NULL;
    }
    goto normal_exit;
}


/* bc_closeTextFile: This function closes any actual temp files associated with
 * a udbClientText structure.
 * Input: ctPtr->textStream - stream to close
 *        tmpFileName - temp file name to delete
 * RetVal:
 *    0  - Success
 *    !0 - error code
 */
int
bc_closeTextFile(udbClientTextP ctPtr, char *tmpFileName)
{
    int code = 0;

    if (ctPtr->textStream)
	fclose(ctPtr->textStream);

    if (*tmpFileName) {		/* we have a valid name */
	code = unlink(tmpFileName);
	if (code < 0)
	    code = errno;
    }
    return code;
}
