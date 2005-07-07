/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "stdafx.h"
#include <errno.h>
#include <time.h>

#include "gui2fs.h"
#include "msgs.h"
#include "results_dlg.h"
#include "volume_inf.h"
#include "mount_points_dlg.h"
#include "hourglass.h"
#include "down_servers_dlg.h"

extern "C" {
#include <afs/param.h>
#include <osi.h>
#include "fs.h"
#include "fs_utils.h"
#include <afsint.h>
#include <afs/auth.h>
}


#define PCCHAR(str)		((char *)(const char *)(str))


#define	MAXHOSTS 13
#define	OMAXHOSTS 8
#define MAXNAME 100
#define	MAXSIZE	2048
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */
#define VMSGSIZE 128      /* size of msg buf in volume hdr */

#define MAXCELLCHARS		64
#define MAXHOSTCHARS		64
#define MAXHOSTSPERCELL		8

struct afsconf_cell {
	char name[MAXCELLCHARS];
    short numServers;
    short flags;
    struct sockaddr_in hostAddr[MAXHOSTSPERCELL];
    char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];
    char *linkedCell;
};

static char space[MAXSIZE];
static char tspace[1024];

// #define	LOGGING_ON		// Enable this to log certain pioctl calls

#ifdef	LOGGING_ON
static char *szLogFileName = "afsguilog.txt";
#endif


FILE *OpenFile(char *file, char *rwp)
{
    char wdir[256];
    long code;
    long tlen;
    FILE *fp;

    code = GetWindowsDirectory(wdir, sizeof(wdir));
    if (code == 0 || code > sizeof(wdir)) 
        return FALSE;

    /* add trailing backslash, if required */
    tlen = strlen(wdir);
    if (wdir[tlen - 1] != '\\')
        strcat(wdir, "\\");

    strcat(wdir, file);

    fp = fopen(wdir, rwp);

    return fp;
}       

CString StripPath(CString& strPath)
{
    int nIndex = strPath.ReverseFind('\\');

    CString strFile = strPath.Mid(nIndex + 1);
    if (strFile.IsEmpty())
        return strPath;

    return strFile;
}

CStringArray& StripPath(CStringArray& files)
{
    for (int i = 0; i < files.GetSize(); i++)
        files[i] = StripPath(files[i]);

    return files;
}

void Flush(const CStringArray& files)
{
    register LONG code;
    struct ViceIoctl blob;
    int error = 0;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        blob.in_size = blob.out_size = 0;

        code = pioctl(PCCHAR(files[i]), VIOCFLUSH, &blob, 0);
        if (code) {
            error = 1;
            if (errno == EMFILE)
                ShowMessageBox(IDS_FLUSH_FAILED, MB_ICONEXCLAMATION, IDS_FLUSH_FAILED, files[i]);
            else 
                ShowMessageBox(IDS_FLUSH_ERROR, MB_ICONEXCLAMATION, IDS_FLUSH_ERROR, files[i], strerror(errno));
        }
    }   

    if (!error)
        ShowMessageBox(IDS_FLUSH_OK, MB_ICONEXCLAMATION, IDS_FLUSH_OK);
}       

void FlushVolume(const CStringArray& files)
{
    register LONG code;
    struct ViceIoctl blob;
    int error = 0;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        blob.in_size = blob.out_size = 0;

        code = pioctl(PCCHAR(files[i]), VIOC_FLUSHVOLUME, &blob, 0);
        if (code) {
            error = 1;
            ShowMessageBox(IDS_FLUSH_VOLUME_ERROR, MB_ICONEXCLAMATION, IDS_FLUSH_VOLUME_ERROR, files[i], strerror(errno));
        }
    }   

    if (!code)
        ShowMessageBox(IDS_FLUSH_VOLUME_OK, MB_ICONEXCLAMATION, IDS_FLUSH_VOLUME_OK);
}       

void WhichCell(CStringArray& files)
{
    register LONG code;
    struct ViceIoctl blob;
    int error;
    CString str;
    CString str2;

    CStringArray results;

    error = 0;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        blob.in_size = 0;
        blob.out_size = MAXSIZE;
        blob.out = space;

        code = pioctl(PCCHAR(files[i]), VIOC_FILE_CELL_NAME, &blob, 1);
        if (code) {
            if (code == ENOENT) {
                LoadString (str, IDS_CANT_GET_CELL);
                results.Add(str);
            } else
                results.Add(GetAfsError(errno));
        } else
            results.Add(space);
    }       

    LoadString (str, IDS_SHOW_CELL);
    LoadString (str2, IDS_SHOW_CELL_COLUMN);
    CResultsDlg dlg(SHOW_CELL_HELP_ID);
    dlg.SetContents(str, str2, StripPath(files), results);
    dlg.DoModal();
}

void WSCellCmd()
{
    register LONG code;
    struct ViceIoctl blob;
    
    HOURGLASS hourglass;

    blob.in_size = 0;
    blob.in = (char *) 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl((char *) 0, VIOC_GET_WS_CELL, &blob, 1);

    if (code) {
        //Die(errno, (char *) 0);
    }
    //else
    //printf("This workstation belongs to cell '%s'\n", space);
}

BOOL CheckVolumes()
{
    register LONG code;
    struct ViceIoctl blob;
    
    blob.in_size = 0;
    blob.out_size = 0;
    code = pioctl(0, VIOCCKBACK, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_CHECK_VOLUMES_ERROR, MB_ICONEXCLAMATION, IDS_CHECK_VOLUMES_ERROR, GetAfsError(errno, CString()));
        return FALSE;
    }

    ShowMessageBox(IDS_CHECK_VOLUMES_OK, MB_OK, IDS_CHECK_VOLUMES_OK);

    return TRUE;
}

void SetCacheSizeCmd(LONG nNewCacheSize)
{
    register LONG code;
    struct ViceIoctl blob;
    
    HOURGLASS hourglass;

    blob.in = (char *) &nNewCacheSize;
    blob.in_size = sizeof(LONG);
    blob.out_size = 0;

    code = pioctl(0, VIOCSETCACHESIZE, &blob, 1);
    //if (code)
    //	Die(errno, (char *) 0);
    //else
    //	printf("New cache size set.\n");
}

void WhereIs(CStringArray& files)
{
    register LONG code;
    struct ViceIoctl blob;
    CStringArray servers;
    CStringArray resultFiles;
    CString str;
    CString str2;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        blob.out_size = MAXSIZE;
        blob.in_size = 0;
        blob.out = space;
        memset(space, 0, sizeof(space));

        code = pioctl(PCCHAR(files[i]), VIOCWHEREIS, &blob, 1);
        if (code) {
            resultFiles.Add(StripPath(files[i]));
            servers.Add(GetAfsError(errno));
            continue;
        }

        LONG *hosts = (LONG *)space;
        BOOL bFirst = TRUE;
        str = "";

        for (int j = 0; j < MAXHOSTS; j++) {
            if (hosts[j] == 0)
                break;
            char *hostName = hostutil_GetNameByINet(hosts[j]);
            if (bFirst) {
                resultFiles.Add(StripPath(files[i]));
                bFirst = FALSE;
            } else
                resultFiles.Add(" ");
            servers.Add(hostName);
        }
    }

    LoadString (str, IDS_SHOW_FS);
    LoadString (str2, IDS_SHOW_FS_COLUMN);
    CResultsDlg dlg(SHOW_FILE_SERVERS_HELP_ID);
    dlg.SetContents(str, str2, resultFiles, servers);
    dlg.DoModal();
}       

CString GetAfsError(int code, const char *filename)
{
    CString strMsg;

    if (code == EINVAL) {
        if (filename)
            strMsg.Format("Invalid argument; it is possible that the file is not in AFS");
        else 
            strMsg.Format("Invalid argument");
    } else if (code == ENOENT) {
        if (filename) 
            strMsg.Format("The file does not exist");
        else 
            strMsg.Format("No such file returned");
    } else if (code == EROFS)  {
        strMsg.Format("You can not change a backup or readonly volume");
    } else if (code == EACCES || code == EPERM) {
        strMsg.Format("You do not have the required rights to do this operation");
    } else if (code == ENODEV) {
        strMsg.Format("AFS service may not have started");
    } else if (code == ESRCH) {
        strMsg.Format("Cell name not recognized");
    } else if (code == ETIMEDOUT) {
        strMsg.Format("Connection timed out");
    } else if (code == EPIPE) {
        strMsg.Format("Volume name or ID not recognized");
    } else {
        strMsg.Format("Error 0x%x occurred", code);
    }

    return strMsg;
}


/************************************************************************
************************** ACL Code *************************************
************************************************************************/

typedef char sec_rgy_name_t[1025];	/* A DCE definition */

struct AclEntry {
    struct AclEntry *next;
    char name[MAXNAME];
    LONG rights;
};

struct Acl {
    int dfs;			//	Originally true if a dfs acl; now also the type
                                //	of the acl (1, 2, or 3, corresponding to object,
				//	initial dir, or initial object).
    sec_rgy_name_t cell;	//	DFS cell name
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

int foldcmp (register char *a, register char *b)
{
    register char t, u;
    while (1) {
        t = *a++;
        u = *b++;
        if (t >= 'A' && t <= 'Z') t += 0x20;
        if (u >= 'A' && u <= 'Z') u += 0x20;
        if (t != u) return 1;
        if (t == 0) return 0;
    }
}

extern "C" void ZapList(struct AclEntry *alist)
{
    register struct AclEntry *tp, *np;

    for (tp = alist; tp; tp = np) {
        np = tp->next;
        free(tp);
    }
}

extern "C" void ZapAcl (struct Acl *acl)
{
    ZapList(acl->pluslist);
    ZapList(acl->minuslist);
    free(acl);
}

extern "C" int PruneList (struct AclEntry **ae, int dfs)
{
    struct AclEntry **lp = ae;
    struct AclEntry *te, *ne;
    LONG ctr = 0;
    
    for (te = *ae; te; te = ne) {
        if ((!dfs && te->rights == 0) || te->rights == -1) {
            *lp = te->next;
            ne = te->next;
            free(te);
            ctr++;
        }
        else {
            ne = te->next;
            lp = &te->next;
        }
    }
    
    return ctr;
}

char *SkipLine (register char *astr)
{
    while (*astr != '\n') 
        astr++;
    
    astr++;
    
    return astr;
}

/* tell if a name is 23 or -45 (digits or minus digits), which are bad names we must prune */
static int BadName(register char *aname)
{
    register int tc;

    /* all must be '-' or digit to be bad */
    while (tc = *aname++) {
        if ((tc != '-') && (tc < '0' || tc > '9')) 
            return 0;
    }

    return 1;
}

CString GetRightsString(register LONG arights, int dfs)
{
    CString str;

    if (!dfs) {
        if (arights & PRSFS_READ) str += "r";
        if (arights & PRSFS_LOOKUP) str += "l";
        if (arights & PRSFS_INSERT) str += "i";
        if (arights & PRSFS_DELETE) str += "d";
        if (arights & PRSFS_WRITE) str += "w";
        if (arights & PRSFS_LOCK) str += "k";
        if (arights & PRSFS_ADMINISTER) str += "a";
    } else {
        ASSERT(FALSE);
/*
		if (arights & DFS_READ) str += "r"; else str += "-";
		if (arights & DFS_WRITE) str += "w"; else printf("-");
		if (arights & DFS_EXECUTE) str += "x"; else printf("-");
		if (arights & DFS_CONTROL) str += "c"; else printf("-");
		if (arights & DFS_INSERT) str += "i"; else printf("-");
		if (arights & DFS_DELETE) str += "d"; else printf("-");
		if (arights & (DFS_USRALL)) str += "+";
*/
    }	

    return str;
}

char *AclToString(struct Acl *acl)
{
    static char mydata[MAXSIZE];
    char tstring[MAXSIZE];
    char dfsstring[30];
    struct AclEntry *tp;
    
    if (acl->dfs)
        sprintf(dfsstring, " dfs:%d %s", acl->dfs, acl->cell);
    else
        dfsstring[0] = '\0';
    sprintf(mydata, "%d%s\n%d\n", acl->nplus, dfsstring, acl->nminus);
    
    for(tp = acl->pluslist; tp; tp = tp->next) {
        sprintf(tstring, "%s %d\n", tp->name, tp->rights);
        strcat(mydata, tstring);
    }
    
    for(tp = acl->minuslist; tp; tp = tp->next) {
        sprintf(tstring, "%s %d\n", tp->name, tp->rights);
        strcat(mydata, tstring);
    }
    
    return mydata;
}

struct Acl *EmptyAcl(const CString& strCellName)
{
    register struct Acl *tp;
    
    tp = (struct Acl *)malloc(sizeof (struct Acl));
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    strcpy(tp->cell, strCellName);

    return tp;
}

struct Acl *ParseAcl(char *astr)
{
    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;

    ta = (struct Acl *) malloc (sizeof (struct Acl));
    ta->dfs = 0;
    sscanf(astr, "%d dfs:%d %s", &ta->nplus, &ta->dfs, ta->cell);
    astr = SkipLine(astr);
    sscanf(astr, "%d", &ta->nminus);
    astr = SkipLine(astr);

    nplus = ta->nplus;
    nminus = ta->nminus;

    last = 0;
    first = 0;
    for(i = 0; i < nplus; i++) {
        sscanf(astr, "%100s %d", tname, &trights);
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        if (!first)
			first = tl;
        strcpy(tl->name, tname);
        tl->rights = trights;
        tl->next = 0;
        if (last)
			last->next = tl;
        last = tl;
    }
    ta->pluslist = first;

    last = 0;
    first = 0;
    for(i=0; i < nminus; i++) {
        sscanf(astr, "%100s %d", tname, &trights);
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        if (!first) 
			first = tl;
        strcpy(tl->name, tname);
        tl->rights = trights;
        tl->next = 0;
        if (last) 
			last->next = tl;
        last = tl;
    }
    ta->minuslist = first;

    return ta;
}

/* clean up an access control list of its bad entries; return 1 if we made
   any changes to the list, and 0 otherwise */
extern "C" int CleanAcl(struct Acl *aa)
{
    register struct AclEntry *te, **le, *ne;
    int changes;

    HOURGLASS hourglass;

    /* Don't correct DFS ACL's for now */
    if (aa->dfs)
        return 0;

    /* prune out bad entries */
    changes = 0;	    /* count deleted entries */
    le = &aa->pluslist;
    for(te = aa->pluslist; te; te = ne) {
        ne = te->next;
        if (BadName(te->name)) {
            /* zap this dude */
            *le = te->next;
            aa->nplus--;
            free(te);
            changes++;
        }
        else
            le = &te->next;
    }

    le = &aa->minuslist;
    
    for(te = aa->minuslist; te; te = ne) {
        ne = te->next;
        if (BadName(te->name)) {
            /* zap this dude */
            *le = te->next;
            aa->nminus--;
            free(te);
            changes++;
        }
        else
            le = &te->next;
    }   

    return changes;
}

void CleanACL(CStringArray& names)
{
    register LONG code;
    register struct Acl *ta;
    struct ViceIoctl blob;
    int changes;

    ShowMessageBox(IDS_CLEANACL_MSG, MB_OK, IDS_CLEANACL_MSG);

    HOURGLASS hourglass;

    for (int i = 0; i < names.GetSize(); i++) {
        blob.out_size = MAXSIZE;
        blob.in_size = 0;
        blob.out = space;

        code = pioctl(PCCHAR(names[i]), VIOCGETAL, &blob, 1);
        if (code) {
            ShowMessageBox(IDS_CLEANACL_ERROR, MB_ICONEXCLAMATION, 0, names[i], GetAfsError(errno));
            continue;
        }

        ta = ParseAcl(space);
        if (ta->dfs) {
            ShowMessageBox(IDS_CLEANACL_NOT_SUPPORTED, MB_ICONEXCLAMATION, IDS_CLEANACL_NOT_SUPPORTED, names[i]);
            continue;
        }

        changes = CleanAcl(ta);
        if (!changes)
            continue;

        /* now set the acl */
        blob.in = AclToString(ta);
        blob.in_size = strlen((char *)blob.in) + 1;
        blob.out_size = 0;
		
        code = pioctl(PCCHAR(names[i]), VIOCSETAL, &blob, 1);
        if (code) {
            if (errno == EINVAL) {
                ShowMessageBox(IDS_CLEANACL_INVALID_ARG, MB_ICONEXCLAMATION, IDS_CLEANACL_INVALID_ARG, names[i]);
                continue;
            }
            else {
                ShowMessageBox(IDS_CLEANACL_ERROR, MB_ICONEXCLAMATION, 0, names[i], GetAfsError(errno));
                continue;
            }
        }
    }
}       

// Derived from fs.c's ListAclCmd
BOOL GetRights(const CString& strDir, CStringArray& strNormal, CStringArray& strNegative)
{
    register LONG code;
    register struct Acl *ta;
    struct ViceIoctl blob;
    struct AclEntry *te;
    int idf = 0; //getidf(as, parm_listacl_id);

    HOURGLASS hourglass;

    blob.out_size = MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;
	
    code = pioctl(PCCHAR(strDir), VIOCGETAL, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_GETRIGHTS_ERROR, MB_ICONEXCLAMATION, IDS_GETRIGHTS_ERROR, strDir, GetAfsError(errno));
        return FALSE;
    }

    ta = ParseAcl(space);
    if (ta->dfs) {
        ShowMessageBox(IDS_DFSACL_ERROR, MB_ICONEXCLAMATION, IDS_DFSACL_ERROR);
        return FALSE;
    }

//	if (ta->dfs)
//		printf("  Default cell = %s\n", ta->cell);

    CString strRight;

    if (ta->nplus > 0) {
        for (te = ta->pluslist; te; te = te->next) {
            strNormal.Add(te->name);
            strNormal.Add(GetRightsString(te->rights, ta->dfs));
        }
    }

    if (ta->nminus > 0) {
        for (te = ta->minuslist; te; te = te->next) {
            strNegative.Add(te->name);
            strNegative.Add(GetRightsString(te->rights, ta->dfs));
        }
    }

    return TRUE;
}

struct AclEntry *FindList(register struct AclEntry *pCurEntry, const char *entryName)
{
    while (pCurEntry) {
        if (!foldcmp(pCurEntry->name, PCCHAR(entryName)))
            return pCurEntry;
        pCurEntry = pCurEntry->next;
    }
    
    return 0;
}

void ChangeList (struct Acl *pAcl, BYTE bNormalRights, const char *entryName, LONG nEntryRights)
{
    ASSERT(pAcl);
    ASSERT(entryName);
    
    struct AclEntry *pEntry;

    HOURGLASS hourglass;

    pEntry = (bNormalRights ? pAcl->pluslist : pAcl->minuslist);
    pEntry = FindList(pEntry, entryName);

    /* Found the item already in the list. */
    if (pEntry) {
        pEntry->rights = nEntryRights;
        if (bNormalRights)
            pAcl->nplus -= PruneList(&pAcl->pluslist, pAcl->dfs);
        else
            pAcl->nminus -= PruneList(&pAcl->minuslist, pAcl->dfs);
        return;
    }

    /* Otherwise we make a new item and plug in the new data. */
    pEntry = (struct AclEntry *) malloc(sizeof (struct AclEntry));
    ASSERT(pEntry);
	
    strcpy(pEntry->name, entryName);
    pEntry->rights = nEntryRights;
    
    if (bNormalRights) {
        pEntry->next = pAcl->pluslist;
        pAcl->pluslist = pEntry;
        pAcl->nplus++;
        if (nEntryRights == 0 || nEntryRights == -1)
            pAcl->nplus -= PruneList(&pAcl->pluslist, pAcl->dfs);
    }
    else {
        pEntry->next = pAcl->minuslist;
        pAcl->minuslist = pEntry;
        pAcl->nminus++;
        if (nEntryRights == 0)
            pAcl->nminus -= PruneList(&pAcl->minuslist, pAcl->dfs);
    }
}

enum rtype {add, destroy, deny};

LONG Convert(const register char *arights, int dfs, enum rtype *rtypep)
{
    register int i, len;
    LONG mode;
    register char tc;

    *rtypep = add;	/* add rights, by default */

    if (!strcmp(arights,"read")) 
        return PRSFS_READ | PRSFS_LOOKUP;
    if (!strcmp(arights, "write")) 
        return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK;
    if (!strcmp(arights, "mail")) 
        return PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
    if (!strcmp(arights, "all")) 
        return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    
    if (!strcmp(arights, "none")) {
        *rtypep = destroy; /* Remove entire entry */
        return 0;
    }

    len = strlen(arights);
    mode = 0;

    for (i = 0; i < len; i++) {
        tc = *arights++;
        if (tc == 'r') mode |= PRSFS_READ;
        else if (tc == 'l') mode |= PRSFS_LOOKUP;
        else if (tc == 'i') mode |= PRSFS_INSERT;
        else if (tc == 'd') mode |= PRSFS_DELETE;
        else if (tc == 'w') mode |= PRSFS_WRITE;
        else if (tc == 'k') mode |= PRSFS_LOCK;
        else if (tc == 'a') mode |= PRSFS_ADMINISTER;
        else {
            fprintf(stderr, "illegal rights character '%c'.\n", tc);
            exit(1);
        }
    }   
    return mode;
}

BOOL SaveACL(const CString& strCellName, const CString& strDir, const CStringArray& normal, const CStringArray& negative)
{
    register LONG code;
    struct ViceIoctl blob;
    struct Acl *pAcl;
    LONG rights;
    enum rtype rtype;

    HOURGLASS hourglass;

    // Create a new ACL
    pAcl = EmptyAcl(strCellName);

    // Set its normal rights
    int i;
    for (i = 0; i < normal.GetSize(); i += 2) {
        rights = Convert(normal[i + 1], 0, &rtype);
        ChangeList(pAcl, TRUE, normal[i], rights);
    }

    // Set its negative rights
    for (i = 0; i < negative.GetSize(); i += 2) {
        rights = Convert(negative[i + 1], 0, &rtype);
        ChangeList(pAcl, FALSE, negative[i], rights);
    }

    // Write the ACL
    blob.in = AclToString(pAcl);
    blob.out_size = 0;
    blob.in_size = 1 + strlen((const char *)blob.in);

    code = pioctl(PCCHAR(strDir), VIOCSETAL, &blob, 1);
    if (code) {
        if (errno == EINVAL)
            ShowMessageBox(IDS_SAVE_ACL_EINVAL_ERROR, MB_ICONEXCLAMATION, IDS_SAVE_ACL_EINVAL_ERROR, strDir);
        else
            ShowMessageBox(IDS_SAVE_ACL_ERROR, MB_ICONEXCLAMATION, IDS_SAVE_ACL_ERROR, strDir, GetAfsError(errno, strDir));
    }       

    ZapAcl(pAcl);

    return (code == 0);
}

BOOL CopyACL(const CString& strToDir, const CStringArray& normal, const CStringArray& negative, BOOL bClear)
{
    register LONG code;
    struct ViceIoctl blob;
    struct Acl *pToAcl;
    int idf = 0; // getidf(as, parm_copyacl_id);

    HOURGLASS hourglass;

    // Get ACL to copy to
    blob.out_size = MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;
	
    code = pioctl(PCCHAR(strToDir), VIOCGETAL, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_ACL_READ_ERROR, MB_ICONEXCLAMATION, IDS_ACL_READ_ERROR, strToDir, GetAfsError(errno, strToDir));
        return FALSE;
    }
	
    if (bClear) 
        pToAcl = EmptyAcl(space);
    else 
        pToAcl = ParseAcl(space);

    CleanAcl(pToAcl);

    if (pToAcl->dfs) {
        ShowMessageBox(IDS_NO_DFS_COPY_ACL, MB_ICONEXCLAMATION, IDS_NO_DFS_COPY_ACL, strToDir);
        ZapAcl(pToAcl);
        return FALSE;
    }

    enum rtype rtype;

    // Set normal rights
    int i;
    for (i = 0; i < normal.GetSize(); i += 2) {
        LONG rights = Convert(normal[i + 1], 0, &rtype);
        ChangeList(pToAcl, TRUE, normal[i], rights);
    }

    // Set negative rights
    for (i = 0; i < negative.GetSize(); i += 2) {
        LONG rights = Convert(negative[i + 1], 0, &rtype);
        ChangeList(pToAcl, FALSE, normal[i], rights);
    }

    // Save modified ACL
    blob.in = AclToString(pToAcl);
    blob.out_size = 0;
    blob.in_size = 1 + strlen((char *)blob.in);

    code = pioctl(PCCHAR(strToDir), VIOCSETAL, &blob, 1);
    if (code) {
        ZapAcl(pToAcl);
        if (errno == EINVAL)
            ShowMessageBox(IDS_COPY_ACL_EINVAL_ERROR, MB_ICONEXCLAMATION, IDS_COPY_ACL_EINVAL_ERROR, strToDir);
        else 
            ShowMessageBox(IDS_COPY_ACL_ERROR, MB_ICONEXCLAMATION, IDS_COPY_ACL_ERROR, strToDir, GetAfsError(errno, strToDir));
        return FALSE;
    }

    ZapAcl(pToAcl);

    ShowMessageBox(IDS_COPY_ACL_OK, MB_OK, IDS_COPY_ACL_OK);

    return TRUE;
}

CString ParseMountPoint(const CString strFile, CString strMountPoint)
{
    CString strType;
    CString strVolume;
    CString strCell;
    CString strMountPointInfo;

    if (strMountPoint[0] == '#')
        strType = "Regular";
    else if (strMountPoint[0] == '%')
        strType = "Read/Write";

    int nColon = strMountPoint.Find(':');
    if (nColon >= 0) {
        strCell = strMountPoint.Mid(1, nColon - 1);
        strVolume = strMountPoint.Mid(nColon + 1);
    } else
        strVolume = strMountPoint.Mid(1);

    strMountPointInfo = strFile + "\t" + strVolume + "\t" + strCell + "\t" + strType;

    return strMountPointInfo;
}       

BOOL ListMount(CStringArray& files)
{
    register LONG code;
    struct ViceIoctl blob;
    int error;
    char orig_name[1024];			/* Original name, may be modified */
    char true_name[1024];			/* ``True'' dirname (e.g., symlink target) */
    char parent_dir[1024];			/* Parent directory of true name */
    register char *last_component;	/* Last component of true name */
    CStringArray mountPoints;
    
    HOURGLASS hourglass;

    error = 0;

    for (int i = 0; i < files.GetSize(); i++) {
        strcpy(orig_name, files[i]);
        strcpy(true_name, orig_name);

        /*
         * Find rightmost slash, if any.
         */
        last_component = (char *)strrchr(true_name, '\\');
        if (last_component) {
            /*
             * Found it.  Designate everything before it as the parent directory,
             * everything after it as the final component.
             */
            strncpy(parent_dir, true_name, last_component - true_name + 1);
            parent_dir[last_component - true_name + 1] = 0;
            last_component++;   /* Skip the slash */
        }
        else {
            /*
             * No slash appears in the given file name.  Set parent_dir to the current
             * directory, and the last component as the given name.
             */
            fs_ExtractDriveLetter(true_name, parent_dir);
            strcat(parent_dir, ".");
            last_component = true_name;
            fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
        }

        blob.in = last_component;
        blob.in_size = strlen(last_component) + 1;
        blob.out_size = MAXSIZE;
        blob.out = space;
        memset(space, 0, MAXSIZE);

        code = pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &blob, 1);
        if (code == 0) {
            int nPos = strlen(space) - 1;
            if (space[nPos] == '.')
                space[nPos] = 0;
            mountPoints.Add(ParseMountPoint(StripPath(files[i]), space));
        } else {
            error = 1;
            if (errno == EINVAL)
                mountPoints.Add(GetMessageString(IDS_NOT_MOUNT_POINT_ERROR, StripPath(files[i])));
            else
                mountPoints.Add(GetMessageString(IDS_LIST_MOUNT_POINT_ERROR, GetAfsError(errno, StripPath(files[i]))));
        }
    }

    CMountPointsDlg dlg;
    dlg.SetMountPoints(mountPoints);
    dlg.DoModal();

    return !error;
}

BOOL IsPathInAfs(const CHAR *strPath)
{
    struct ViceIoctl blob;
    int code;

    HOURGLASS hourglass;

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl((LPTSTR)((LPCTSTR)strPath), VIOC_FILE_CELL_NAME, &blob, 1);
    if (code)
        return FALSE;
    return TRUE;
}

/* return a static pointer to a buffer */
static char *Parent(char *apath)
{
    register char *tp;

    strcpy(tspace, apath);
    tp = strrchr(tspace, '\\');
    if (tp) {
        *(tp+1) = 0;	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k:" */
    }
    else {
        fs_ExtractDriveLetter(apath, tspace);
    	strcat(tspace, ".");
    }
    
    return tspace;
}

BOOL MakeMount(const CString& strDir, const CString& strVolName, const CString& strCellName, BOOL bRW)
{
    register LONG code;
    register char *cellName;
    char localCellName[1000];
    struct ViceIoctl blob;

    HOURGLASS hourglass;

    ASSERT(strVolName.GetLength() < 64);

    /*

    defect #3069

    if (as->parms[5].items && !as->parms[2].items) {
	fprintf(stderr,"fs: must provide cell when creating cellular mount point.\n");
	return FALSE;
    }
    */

    if (strCellName.GetLength() > 0)	/* cell name specified */
        cellName = PCCHAR(strCellName);
    else
        cellName = (char *) 0;

    if (!IsPathInAfs(Parent(PCCHAR(strDir)))) {
        ShowMessageBox(IDS_MAKE_MP_NOT_AFS_ERROR, MB_ICONEXCLAMATION, IDS_MAKE_MP_NOT_AFS_ERROR);
        return FALSE;
    }

    if (cellName) {
        blob.in_size = 0;
        blob.out_size = MAXSIZE;
        blob.out = space;
        code = pioctl(Parent(PCCHAR(strDir)), VIOC_FILE_CELL_NAME, &blob, 1);
    }   

    strcpy(localCellName, (cellName? cellName : space));

    if (bRW)	/* if -rw specified */
        strcpy(space, "%");
    else
        strcpy(space, "#");

    /* If cellular mount point, prepend cell prefix */
    if (cellName) {
        strcat(space, localCellName);
        strcat(space, ":");
    }   

    strcat(space, strVolName);	/* append volume name */
    strcat(space, ".");		/* stupid convention; these end with a period */

    /* create symlink with a special pioctl for Windows NT, since it doesn't
     * have a symlink system call.
     */
    blob.out_size = 0;
    blob.in_size = 1 + strlen(space);
    blob.in = space;
    blob.out = NULL;
    code = pioctl(PCCHAR(strDir), VIOC_AFS_CREATE_MT_PT, &blob, 0);

    if (code) {
        ShowMessageBox(IDS_MOUNT_POINT_ERROR, MB_ICONEXCLAMATION, IDS_MOUNT_POINT_ERROR, GetAfsError(errno, strDir));
        return FALSE;
    }
    
    return TRUE;
}

/*
*/
long fs_ExtractDriveLetter(const char *inPathp, char *outPathp)
{
    if (inPathp[0] != 0 && inPathp[1] == ':') {
        /* there is a drive letter */
        *outPathp++ = *inPathp++;
        *outPathp++ = *inPathp++;
        *outPathp++ = 0;
    }
    else *outPathp = 0;

    return 0;
}       

/* strip the drive letter from a component */
long fs_StripDriveLetter(const char *inPathp, char *outPathp, long outSize)
{
    char tempBuffer[1000];
    strcpy(tempBuffer, inPathp);
    if (tempBuffer[0] != 0 && tempBuffer[1] == ':') {
        /* drive letter present */
        strcpy(outPathp, tempBuffer+2);
    }
    else {
        /* no drive letter present */
        strcpy(outPathp, tempBuffer);
    }
    return 0;
}       


BOOL RemoveSymlink(const char * linkName)
{
    BOOL error = FALSE;
    INT code=0;
    struct ViceIoctl blob;
    char tbuffer[1024];
    char lsbuffer[1024];
    char tpbuffer[1024];
    char *tp;
    
    HOURGLASS hourglass;

    tp = (char *) strrchr(linkName, '\\');
    if (!tp)
        tp = (char *) strrchr(linkName, '/');
    if (tp) {
        strncpy(tbuffer, linkName, code=tp-linkName+1);  /* the dir name */
        tbuffer[code] = 0;
        tp++;   /* skip the slash */
    }
    else {
        fs_ExtractDriveLetter(linkName, tbuffer);
        strcat(tbuffer, ".");
        fs_StripDriveLetter(tp, tpbuffer, 0);
        tp=tpbuffer;
    }
    blob.in = tp;
    blob.in_size = strlen(tp)+1;
    blob.out = lsbuffer;
    blob.out_size = sizeof(lsbuffer);
    code = pioctl(tbuffer, VIOC_LISTSYMLINK, &blob, 0);
    if (code)
        return FALSE;
    blob.out_size = 0;
    blob.in = tp;
    blob.in_size = strlen(tp)+1;
    return (pioctl(tbuffer, VIOC_DELSYMLINK, &blob, 0)==0);
}       

BOOL IsSymlink(const char * true_name)
{
    char parent_dir[MAXSIZE];		/*Parent directory of true name*/
    char strip_name[MAXSIZE];
    struct ViceIoctl blob;
    char *last_component;
    int code;

    HOURGLASS hourglass;

    last_component = (char *) strrchr(true_name, '\\');
    if (!last_component)
        last_component = (char *) strrchr(true_name, '/');
    if (last_component) {
        /*
         * Found it.  Designate everything before it as the parent directory,
         * everything after it as the final component.
         */
        strncpy(parent_dir, true_name, last_component - true_name + 1);
        parent_dir[last_component - true_name + 1] = 0;
        last_component++;   /*Skip the slash*/
    }
    else {
        /*
         * No slash appears in the given file name.  Set parent_dir to the current
         * directory, and the last component as the given name.
         */
        fs_ExtractDriveLetter(true_name, parent_dir);
        strcat(parent_dir, ".");
        last_component = strip_name;
        fs_StripDriveLetter(true_name, strip_name, sizeof(strip_name));
    }

    blob.in = last_component;
    blob.in_size = strlen(last_component)+1;
    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, MAXSIZE);
    code = pioctl(parent_dir, VIOC_LISTSYMLINK, &blob, 1);
    return (code==0);
}       


BOOL IsMountPoint(const char * name)
{
    register LONG code = 0;
    struct ViceIoctl blob;
    char tbuffer[1024];
    char lsbuffer[1024];
    register char *tp;
    char szCurItem[1024];
    strcpy(szCurItem, name);
	
    tp = (char *)strrchr(szCurItem, '\\');
    if (tp) {
        strncpy(tbuffer, szCurItem, code = tp - szCurItem + 1);  /* the dir name */
        tbuffer[code] = 0;
        tp++;   /* skip the slash */
    } else {
        fs_ExtractDriveLetter(szCurItem, tbuffer);
        strcat(tbuffer, ".");
        tp = szCurItem;
        fs_StripDriveLetter(tp, tp, 0);
    }

    blob.in = tp;
    blob.in_size = strlen(tp)+1;
    blob.out = lsbuffer;
    blob.out_size = sizeof(lsbuffer);

    code = pioctl(tbuffer, VIOC_AFS_STAT_MT_PT, &blob, 0);

    return (code==0);
}       


/*
 * Delete AFS mount points.  Variables are used as follows:
 *       tbuffer: Set to point to the null-terminated directory name of the mount point
 *	    (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the mount point to nuke.
 */
BOOL RemoveMount(CStringArray& files)
{
    register LONG code = 0;
    struct ViceIoctl blob;
    char tbuffer[1024];
    char lsbuffer[1024];
    register char *tp;
    BOOL error = FALSE;
    CStringArray results;
    CString str;
    CString str2;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        if (!IsMountPoint(files[i])) {
            error = TRUE;
            if (errno == EINVAL)
                results.Add(GetMessageString(IDS_NOT_MOUNT_POINT_ERROR, StripPath(files[i])));
            else
                results.Add(GetMessageString(IDS_ERROR, GetAfsError(errno, StripPath(files[i]))));
            continue;	// don't bother trying
        }

        blob.out_size = 0;
        blob.in = tp;
        blob.in_size = strlen(tp)+1;

        code = pioctl(tbuffer, VIOC_AFS_DELETE_MT_PT, &blob, 0);
        if (code) {
            error = TRUE;
            results.Add(GetMessageString(IDS_ERROR, GetAfsError(errno, StripPath(files[i]))));
        } else
            results.Add(GetMessageString(IDS_DELETED));
    }   

    LoadString (str, IDS_REMOVE_MP);
    LoadString (str2, IDS_REMOVE_MP_COLUMN);
    CResultsDlg dlg(REMOVE_MOUNT_POINTS_HELP_ID);
    dlg.SetContents(str, str2, StripPath(files), results);
    dlg.DoModal();

    return !error;
}

BOOL GetVolumeInfo(CString strFile, CVolInfo& volInfo)
{
    register LONG code;
    struct ViceIoctl blob;
    struct VolumeStatus *status;
    char *name;

    HOURGLASS hourglass;

    volInfo.m_strFilePath = strFile;
    volInfo.m_strFileName = StripPath(strFile);

    /*
	volInfo.m_strName = "VolumeName";
	volInfo.m_nID = 10;
	volInfo.m_nQuota = 20 * 1024 * 1024;
	volInfo.m_nNewQuota = volInfo.m_nQuota;
	volInfo.m_nUsed = volInfo.m_nQuota / 2;
	volInfo.m_nPartSize = 50 * 1024 * 1024;
	volInfo.m_nPartFree = 30 * 1024 * 1024;
	volInfo.m_nDup = -1;
	return TRUE;
     */

    blob.out_size = MAXSIZE;
    blob.in_size = 0;
    blob.out = space;

    code = pioctl(PCCHAR(strFile), VIOCGETVOLSTAT, &blob, 1);
    if (code) {
        volInfo.m_strErrorMsg = GetAfsError(errno, strFile);
        return FALSE;
    }

    status = (VolumeStatus *)space;
    name = (char *)status + sizeof(*status);

    volInfo.m_strName = name;
    volInfo.m_nID = status->Vid;
    volInfo.m_nQuota = status->MaxQuota;
    volInfo.m_nNewQuota = status->MaxQuota;
    volInfo.m_nUsed = status->BlocksInUse;
    volInfo.m_nPartSize = status->PartMaxBlocks;
    volInfo.m_nPartFree = status->PartBlocksAvail;
    volInfo.m_nDup = -1;

    return TRUE;
}
	
BOOL SetVolInfo(CVolInfo& volInfo)
{
    register LONG code;
    struct ViceIoctl blob;
    struct VolumeStatus *status;
    char *input;

    HOURGLASS hourglass;

    blob.out_size = MAXSIZE;
    blob.in_size = sizeof(*status) + 3;	/* for the three terminating nulls */
    blob.out = space;
    blob.in = space;

    status = (VolumeStatus *)space;
    status->MinQuota = -1;
    status->MaxQuota = volInfo.m_nNewQuota;
	
    input = (char *)status + sizeof(*status);
    *(input++) = '\0';	/* never set name: this call doesn't change vldb */
    *(input++) = '\0';	// No offmsg
    *(input++) = '\0';	// No motd

#ifdef LOGGING_ON
    FILE *fp = OpenFile(szLogFileName, "a");
    if (fp) {
        fprintf(fp, "\nSetVolInfo() pioctl parms:\n");
        fprintf(fp, "\tpathp = %s\n\topcode = VIOCSETVOLSTAT (%d)\n\tblobp = %ld\n", PCCHAR(volInfo.m_strFilePath), VIOCSETVOLSTAT, &blob);
        fprintf(fp, "\t\tblobp.in = %ld (VolumeStatus *status)\n\t\tblobp.in_size = %ld\n\t\tblobp.out = %ld ((VolumeStatus *status))\n\t\tblobp.out_size = %ld\n", blob.in, blob.in_size, blob.out, blob.out_size);
        fprintf(fp, "\t\t\tstatus->MinQuota = %ld\n", status->MinQuota);
        fprintf(fp, "\t\t\tstatus->MaxQuota = %ld\n", status->MaxQuota);
        fprintf(fp, "\t\t\tOther status fields aren't set\n");
        fprintf(fp, "\t\t\t3 nulls follow the VolumeStatus structure.\n");
        fprintf(fp, "\tfollow = 1\n");
        fclose(fp);		
    }
#endif

    code = pioctl(PCCHAR(volInfo.m_strFilePath), VIOCSETVOLSTAT, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_SET_QUOTA_ERROR, MB_ICONEXCLAMATION, IDS_SET_QUOTA_ERROR, GetAfsError(errno, volInfo.m_strName));
        return FALSE;
    }

    return TRUE;
}

int GetCellName(char *cellNamep, struct afsconf_cell *infop)
{
    strcpy(infop->name, cellNamep);
    return 0;
}

BOOL CheckServers(const CString& strCellName, WHICH_CELLS nCellsToCheck, BOOL bFast)
{
    register LONG code;
    struct ViceIoctl blob;
    register LONG j;
    LONG temp = 0;
    struct afsconf_cell info;
    struct chservinfo checkserv;

    HOURGLASS hourglass;

    memset(&checkserv, 0, sizeof(struct chservinfo));
    blob.in_size = sizeof(struct chservinfo);
    blob.in = (caddr_t)&checkserv;

    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(LONG));	/* so we assure zero when nothing is copied back */

    /* prepare flags for checkservers command */
    if (nCellsToCheck == LOCAL_CELL)
        temp = 2;	/* default to checking local cell only */
    else if (nCellsToCheck == ALL_CELLS)
        temp &= ~2;	/* turn off local cell check */

    if (bFast)
        temp |= 1;	/* set fast flag */
    
    checkserv.magic = 0x12345678;	/* XXX */
    checkserv.tflags = temp;

    /* now copy in optional cell name, if specified */
    if (nCellsToCheck == SPECIFIC_CELL) {
        GetCellName(PCCHAR(strCellName), &info);
        strcpy(checkserv.tbuffer,info.name);
        checkserv.tsize = strlen(info.name) + 1;
    } else {
        strcpy(checkserv.tbuffer, "\0");
        checkserv.tsize = 0;
    }

    checkserv.tinterval = -1;	/* don't change current interval */

    code = pioctl(0, VIOCCKSERV, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_CHECK_SERVERS_ERROR, MB_ICONEXCLAMATION, IDS_CHECK_SERVERS_ERROR, GetAfsError(errno, CString()));
        return FALSE;
    }

    memcpy(&temp, space, sizeof(LONG));

    if (temp == 0) {
        ShowMessageBox(IDS_ALL_SERVERS_RUNNING, MB_OK, IDS_ALL_SERVERS_RUNNING);
        return TRUE;
    }

    CStringArray servers;
    for (j = 0; j < MAXHOSTS; j++) {
        memcpy(&temp, space + j * sizeof(LONG), sizeof(LONG));
        if (temp == 0)
            break;

        char *name = hostutil_GetNameByINet(temp);
        servers.Add(name);
    }

    CDownServersDlg dlg;
    dlg.SetServerNames(servers);
    dlg.DoModal();

    return TRUE;
}       

BOOL GetTokenInfo(CStringArray& tokenInfo)
{
    int cellNum;
    int rc;
    time_t current_time;
    time_t tokenExpireTime;
    char *expireString;
    char userName[100];
//	char s[100];
    struct ktc_principal serviceName, clientName;
    struct ktc_token token;

    CString strTokenInfo;
    CString strUserName;
    CString strCellName;
    CString strExpir;

//      tokenInfo.Add("");
//	return TRUE;


    HOURGLASS hourglass;

//	printf("\nTokens held by the Cache Manager:\n\n");
    cellNum = 0;
    current_time = time(0);

    while (1) {
        rc = ktc_ListTokens(cellNum, &cellNum, &serviceName);
        if (rc == KTC_NOENT) {
            /* end of list */
//	    printf("   --End of list --\n");
            break;
        }
        else if (rc == KTC_NOCM) {
            ShowMessageBox(IDS_GET_TOKENS_NO_AFS_SERVICE);
//	    printf("AFS service may not have started\n");
            break;
        }
        else if (rc) {
            ShowMessageBox(IDS_GET_TOKENS_UNEXPECTED_ERROR, MB_ICONEXCLAMATION, IDS_GET_TOKENS_UNEXPECTED_ERROR, rc);
            return FALSE;
//	    printf("Unexpected error, code %d\n", rc);
//	    exit(1);
        }
        else {
            rc = ktc_GetToken(&serviceName, &token, sizeof(token), &clientName);
            if (rc) {
                ShowMessageBox(IDS_GET_TOKENS_UNEXPECTED_ERROR2, MB_ICONEXCLAMATION, IDS_GET_TOKENS_UNEXPECTED_ERROR2, 
                                serviceName.name, serviceName.instance,	serviceName.cell, rc);
                continue;
            }

            tokenExpireTime = token.endTime;

            strcpy(userName, clientName.name);
            if (clientName.instance[0] != 0) {
                strcat(userName, ".");
                strcat(userName, clientName.instance);
            }

            BOOL bShowName = FALSE;

            if (userName[0] == '\0')
                ; //printf("Tokens");
// AFS ID is not returned at this time.
//	    else if (strncmp(userName, "AFS ID", 6) == 0)
//	    	printf("User's (%s) tokens", userName);
//	    	sscanf(userName, "(AFS ID %s)", szAfsID);
	    else if (strncmp(userName, "Unix UID", 8) == 0)
                ; //printf("Tokens");
            else
                strUserName = userName;
//		printf("User %s's tokens", userName);
			
//		printf(" for %s%s%s@%s ", serviceName.name, serviceName.instance[0] ? "." : "",	serviceName.instance, serviceName.cell);
            strCellName = serviceName.cell;
			
            if (tokenExpireTime <= current_time)
                strExpir = "[>> Expired <<]";
//		printf("[>> Expired <<]\n");
            else {
                expireString = ctime(&tokenExpireTime);
                expireString += 4;	 /* Skip day of week */
                expireString[12] = '\0'; /* Omit secs & year */
//		printf("[Expires %s]\n", expireString);
                strExpir.Format("%s", expireString);
            }

            strTokenInfo = strUserName + "\t" + strCellName + "\t" + strExpir + "\t" + strCellName;
            tokenInfo.Add(strTokenInfo);
        }
    }

//	printf("Press <Enter> or <Return> when finished: ");
//	gets(s);
    return TRUE;
}

UINT MakeSymbolicLink(const char *strName ,const char *strDir)
{
    struct ViceIoctl blob;
    char space[MAXSIZE];
    UINT code;

    HOURGLASS hourglass;

    /*lets confirm its a good symlink*/
    if (!IsPathInAfs(strDir))
        return 1;
    LPTSTR lpsz = new TCHAR[strlen(strDir)+1];
    _tcscpy(lpsz, strName);
    strcpy(space, strDir);
    blob.out_size = 0;
    blob.in_size = 1 + strlen(space);
    blob.in = space;
    blob.out = NULL;
    if ((code=pioctl(lpsz, VIOC_SYMLINK, &blob, 0))!=0)
        return code;
    return 0;
}

void ListSymbolicLinkPath(const char *strName,char *strPath,UINT nlenPath)
{
    ASSERT(nlenPath<MAX_PATH);
    struct ViceIoctl blob;
    char orig_name[MAX_PATH+1];		/*Original name, may be modified*/
    char true_name[MAX_PATH+1];		/*``True'' dirname (e.g., symlink target)*/
    char parent_dir[MAX_PATH+1];		/*Parent directory of true name*/
    char *last_component;	/*Last component of true name*/
    UINT code;    

    HOURGLASS hourglass;

    strcpy(orig_name, strName);
    strcpy(true_name, orig_name);
	/*
	 * Find rightmost slash, if any.
	 */
    last_component = (char *) strrchr(true_name, '\\');
    if (!last_component)
        last_component = (char *) strrchr(true_name, '/');
    if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.
	     */
        strncpy(parent_dir, true_name, last_component - true_name + 1);
        parent_dir[last_component - true_name + 1] = 0;
        last_component++;   /*Skip the slash*/
    }
    else {
        /*
         * No slash appears in the given file name.  Set parent_dir to the current
         * directory, and the last component as the given name.
         */
        fs_ExtractDriveLetter(true_name, parent_dir);
        strcat(parent_dir, ".");
        last_component = true_name;
        fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
    }
    blob.in = last_component;
    blob.in_size = strlen(last_component)+1;
    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, MAXSIZE);
    if ((code = pioctl(parent_dir, VIOC_LISTSYMLINK, &blob, 1)))
        strcpy(space,"???");
    ASSERT(strlen(space)<MAX_PATH);
    strncpy(strPath,space,nlenPath);
}       
