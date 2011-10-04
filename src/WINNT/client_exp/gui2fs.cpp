/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <errno.h>
#include <time.h>

#include "gui2fs.h"
#include "msgs.h"
#include "results_dlg.h"
#include "volume_inf.h"
#include "mount_points_dlg.h"
#include "symlinks_dlg.h"
#include "hourglass.h"
#include "down_servers_dlg.h"

extern "C" {
#include <rx/rx_globals.h>
#include "fs.h"
#include "fs_utils.h"
#include <afs/afsint.h>
#include <afs/afs_consts.h>
#include <afs/cellconfig.h>
#include <afs/vldbint.h>
#include <afs/volser.h>
#include <afs/auth.h>
#include <WINNT\afsreg.h>
#include <cm.h>
#include <cm_nls.h>
#include <osi.h>
#include <cm_user.h>
#include <cm_scache.h>
#include <cm_ioctl.h>
}

#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#define PCCHAR(str)		((char *)(const char *)(str))
#define VL_NOENT                (363524L)

#define MAXNAME 100
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */
#define VMSGSIZE 128      /* size of msg buf in volume hdr */

#define MAXCELLCHARS		64
#define MAXHOSTCHARS		64
#define MAXHOSTSPERCELL		8

static struct ubik_client *uclient;
static int rxInitDone = 0;
static char pn[] = "fs";

// #define	LOGGING_ON		// Enable this to log certain pioctl calls

#ifdef	LOGGING_ON
static char *szLogFileName = "afsguilog.txt";
#endif

#ifdef UNICODE
class CStringUtf8 : public CStringA
{
public:
    CStringUtf8(const CStringW& csw) : CStringA()
    {
        SetString(csw);
    }

    CStringUtf8(const char * cp) : CStringA(cp) {}

    CStringUtf8() :CStringA() {}

    void SetString(const CStringW& csw)
    {
        char buffer[1024];
        int rv;

        rv = WideCharToMultiByte(CP_UTF8, 0, csw, -1,
                                 buffer, sizeof(buffer),
                                 NULL, FALSE);

        if (rv != 0) {
            CStringA::SetString(buffer);
        } else {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                int cb_req;

                cb_req = WideCharToMultiByte(CP_UTF8, 0, csw, -1, NULL, 0, NULL, FALSE);
                if (cb_req != 0) {
                    cb_req ++;

                    WideCharToMultiByte(CP_UTF8, 0, csw, -1, CStringA::GetBuffer(cb_req), cb_req, NULL, FALSE);
                    CStringA::ReleaseBuffer();
                }
            } else {
#ifdef DEBUG
                DebugBreak();
#endif
            }
        }
    }

    static CString _Utf8ToCString(const char * ustr)
    {
        CString cs;
        int cch;

        cch = MultiByteToWideChar(CP_UTF8, 0, ustr, -1, NULL, 0);
        if (cch == 0) {
            cs.Empty();
            return cs;
        }

        cch++;
        cch = MultiByteToWideChar(CP_UTF8, 0, ustr, -1, cs.GetBuffer(cch), cch);
        cs.ReleaseBuffer();

        return cs;
    }
};

long pioctl_T(const CString& path, long opcode, struct ViceIoctl * blob, int follow)
{
    CStringUtf8 upath(path);

    return pioctl_utf8(PCCHAR(upath), opcode, blob, follow);
}

#define Utf8ToCString(cs) CStringUtf8::_Utf8ToCString(cs)
#else
#define pioctl_T(path, op, vblob, follow) pioctl(PCCHAR(path), op, vblob, follow)
#define Utf8ToCString(cs) (cs)
#endif



static int
VLDBInit(int noAuthFlag, struct afsconf_cell *info)
{
    afs_int32 code;

    code = ugen_ClientInit(noAuthFlag, (char *)AFSDIR_CLIENT_ETC_DIRPATH,
			   info->name, 0, &uclient,
                           NULL, pn, rxkad_clear,
                           VLDB_MAXSERVERS, AFSCONF_VLDBSERVICE, 50,
                           0, 0, USER_SERVICE_ID);
    rxInitDone = 1;
    return code;
}

static FILE *
OpenFile(char *file, char *rwp)
{
    char wdir[256];
    long code;
    long tlen;
    FILE *fp;

    code = GetWindowsDirectoryA(wdir, sizeof(wdir));
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
    LONG code;
    struct ViceIoctl blob;
    int error = 0;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        blob.in_size = blob.out_size = 0;

        code = pioctl_T(files[i], VIOCFLUSH, &blob, 0);
        if (code) {
            error = 1;
            if (errno == EMFILE)
                ShowMessageBox(IDS_FLUSH_FAILED, MB_ICONERROR, IDS_FLUSH_FAILED, files[i]);
            else
                ShowMessageBox(IDS_FLUSH_ERROR, MB_ICONERROR, IDS_FLUSH_ERROR, files[i], strerror(errno));
        }
    }

    if (!error)
        ShowMessageBox(IDS_FLUSH_OK, MB_ICONINFORMATION, IDS_FLUSH_OK);
}

void FlushVolume(const CStringArray& files)
{
    LONG code;
    struct ViceIoctl blob;
    int error = 0;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        blob.in_size = blob.out_size = 0;

        code = pioctl_T(files[i], VIOC_FLUSHVOLUME, &blob, 0);
        if (code) {
            error = 1;
            ShowMessageBox(IDS_FLUSH_VOLUME_ERROR, MB_ICONERROR, IDS_FLUSH_VOLUME_ERROR, files[i], strerror(errno));
        }
    }

    if (!code)
        ShowMessageBox(IDS_FLUSH_VOLUME_OK, MB_ICONINFORMATION, IDS_FLUSH_VOLUME_OK);
}

void WhichCell(CStringArray& files)
{
    LONG code;
    struct ViceIoctl blob;
    int error;
    CString str;
    CString str2;

    CStringArray results;

    error = 0;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        char space[AFS_PIOCTL_MAXSIZE];

        blob.in_size = 0;
        blob.out_size = AFS_PIOCTL_MAXSIZE;
        blob.out = space;

        code = pioctl_T(files[i], VIOC_FILE_CELL_NAME, &blob, 1);
        if (code) {
            if (code == ENOENT) {
                LoadString (str, IDS_CANT_GET_CELL);
                results.Add(str);
            } else
                results.Add(GetAfsError(errno));
        } else {
            space[AFS_PIOCTL_MAXSIZE - 1] = '\0';
            results.Add(Utf8ToCString(space));
        }
    }

    LoadString (str, IDS_SHOW_CELL);
    LoadString (str2, IDS_SHOW_CELL_COLUMN);
    CResultsDlg dlg(SHOW_CELL_HELP_ID);
    dlg.SetContents(str, str2, StripPath(files), results);
    dlg.DoModal();
}

void WSCellCmd()
{
    char space[AFS_PIOCTL_MAXSIZE];
    LONG code;
    struct ViceIoctl blob;

    HOURGLASS hourglass;

    blob.in_size = 0;
    blob.in = (char *) 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
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
    LONG code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.out_size = 0;
    code = pioctl(0, VIOCCKBACK, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_CHECK_VOLUMES_ERROR, MB_ICONERROR, IDS_CHECK_VOLUMES_ERROR, GetAfsError(errno, CString()));
        return FALSE;
    }

    ShowMessageBox(IDS_CHECK_VOLUMES_OK, MB_OK|MB_ICONINFORMATION, IDS_CHECK_VOLUMES_OK);

    return TRUE;
}

void SetCacheSizeCmd(LONG nNewCacheSize)
{
    LONG code;
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
    LONG code;
    struct ViceIoctl blob;
    CStringArray servers;
    CStringArray resultFiles;
    CString str;
    CString str2;

    HOURGLASS hourglass;

    for (int i = 0; i < files.GetSize(); i++) {
        char space[AFS_PIOCTL_MAXSIZE];

        blob.out_size = AFS_PIOCTL_MAXSIZE;
        blob.in_size = 0;
        blob.out = space;
        memset(space, 0, sizeof(space));

        code = pioctl_T(files[i], VIOCWHEREIS, &blob, 1);
        if (code) {
            resultFiles.Add(StripPath(files[i]));
            servers.Add(GetAfsError(errno));
            continue;
        }

        LONG *hosts = (LONG *)space;
        BOOL bFirst = TRUE;
        str = "";

        for (int j = 0; j < AFS_MAXHOSTS; j++) {
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

static int
CMtoUNIXerror(int cm_code)
{
    switch (cm_code) {
    case CM_ERROR_TIMEDOUT:
	return ETIMEDOUT;
    case CM_ERROR_NOACCESS:
	return EACCES;
    case CM_ERROR_NOSUCHFILE:
	return ENOENT;
    case CM_ERROR_INVAL:
	return EINVAL;
    case CM_ERROR_BADFD:
	return EBADF;
    case CM_ERROR_EXISTS:
	return EEXIST;
    case CM_ERROR_CROSSDEVLINK:
	return EXDEV;
    case CM_ERROR_NOTDIR:
	return ENOTDIR;
    case CM_ERROR_ISDIR:
	return EISDIR;
    case CM_ERROR_READONLY:
	return EROFS;
    case CM_ERROR_WOULDBLOCK:
	return EWOULDBLOCK;
    case CM_ERROR_NOSUCHCELL:
	return ESRCH;		/* hack */
    case CM_ERROR_NOSUCHVOLUME:
	return EPIPE;		/* hack */
    case CM_ERROR_NOMORETOKENS:
	return EDOM;		/* hack */
    case CM_ERROR_TOOMANYBUFS:
	return EFBIG;		/* hack */
    default:
	if (cm_code > 0 && cm_code < EILSEQ)
	    return cm_code;
	else
	    return ENOTTY;
    }
}

CString GetAfsError(int code, const TCHAR *filename)
{
    CString strMsg;

    code = CMtoUNIXerror(code);

    if (code == EINVAL) {
        if (filename)
            strMsg.Format(_T("Invalid argument; it is possible that the file is not in AFS"));
        else
            strMsg.Format(_T("Invalid argument"));
    } else if (code == ENOENT) {
        if (filename)
            strMsg.Format(_T("The file does not exist"));
        else
            strMsg.Format(_T("No such file returned"));
    } else if (code == EROFS)  {
        strMsg.Format(_T("You can not change a backup or readonly volume"));
    } else if (code == EACCES || code == EPERM) {
        strMsg.Format(_T("You do not have the required rights to do this operation"));
    } else if (code == ENODEV) {
        strMsg.Format(_T("AFS service may not have started"));
    } else if (code == ESRCH) {
        strMsg.Format(_T("Cell name not recognized"));
    } else if (code == ETIMEDOUT) {
        strMsg.Format(_T("Connection timed out"));
    } else if (code == EPIPE) {
        strMsg.Format(_T("Volume name or ID not recognized"));
    } else {
        strMsg.Format(_T("Error 0x%x occurred"), code);
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

int foldcmp (char *a, char *b)
{
    char t, u;
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
    struct AclEntry *tp, *np;

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

char *SkipLine (char *astr)
{
    while (*astr != '\n')
        astr++;

    astr++;

    return astr;
}

/* tell if a name is 23 or -45 (digits or minus digits), which are bad names we must prune */
static int BadName(char *aname)
{
    int tc;

    /* all must be '-' or digit to be bad */
    while (tc = *aname++) {
        if ((tc != '-') && (tc < '0' || tc > '9'))
            return 0;
    }

    return 1;
}

CString GetRightsString(LONG arights, int dfs)
{
    CString str;

    if (!dfs) {
        if (arights & PRSFS_READ) str += _T("r");
        if (arights & PRSFS_LOOKUP) str += _T("l");
        if (arights & PRSFS_INSERT) str += _T("i");
        if (arights & PRSFS_DELETE) str += _T("d");
        if (arights & PRSFS_WRITE) str += _T("w");
        if (arights & PRSFS_LOCK) str += _T("k");
        if (arights & PRSFS_ADMINISTER) str += _T("a");
    } else {
        ASSERT(FALSE);
/*
		if (arights & DFS_READ) str += _T("r"); else str += _T("-");
		if (arights & DFS_WRITE) str += _T("w"); else printf(_T("-"));
		if (arights & DFS_EXECUTE) str += _T("x"); else printf(_T("-"));
		if (arights & DFS_CONTROL) str += _T("c"); else printf(_T("-"));
		if (arights & DFS_INSERT) str += _T("i"); else printf(_T("-"));
		if (arights & DFS_DELETE) str += _T("d"); else printf(_T("-"));
		if (arights & (DFS_USRALL)) str += _T("+");
*/
    }

    return str;
}

char *AclToString(struct Acl *acl)
{
    static char mydata[AFS_PIOCTL_MAXSIZE];
    char tstring[AFS_PIOCTL_MAXSIZE];
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
    struct Acl *tp;
    CStringUtf8 ustrCell(strCellName);

    tp = (struct Acl *)malloc(sizeof (struct Acl));
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    StringCbCopyA(tp->cell, sizeof(tp->cell), ustrCell);

    return tp;
}

struct Acl *EmptyAcl(char *astr)
{
    struct Acl *tp;
    int junk;

    tp = (struct Acl *)malloc(sizeof (struct Acl));
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    if (astr == NULL || sscanf(astr, "%d dfs:%d %s", &junk, &tp->dfs, tp->cell) <= 0) {
        tp->dfs = 0;
        tp->cell[0] = '\0';
    }
    return tp;
}

struct Acl *
ParseAcl (char *astr)
{
    int nplus, nminus, i, trights, ret;
    char tname[MAXNAME];
    struct AclEntry *first, *next, *last, *tl;
    struct Acl *ta;

    ta = EmptyAcl(NULL);
    if (astr == NULL || strlen(astr) == 0)
        return ta;

    ret = sscanf(astr, "%d dfs:%d %s", &ta->nplus, &ta->dfs, ta->cell);
    if (ret <= 0) {
        free(ta);
        return NULL;
    }
    astr = SkipLine(astr);
    ret = sscanf(astr, "%d", &ta->nminus);
    if (ret <= 0) {
        free(ta);
        return NULL;
    }
    astr = SkipLine(astr);

    nplus = ta->nplus;
    nminus = ta->nminus;

    last = 0;
    first = 0;
    for(i=0;i<nplus;i++) {
        ret = sscanf(astr, "%100s %d", tname, &trights);
        if (ret <= 0)
            goto nplus_err;
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        if (tl == NULL)
            goto nplus_err;
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
    for(i=0;i<nminus;i++) {
        ret = sscanf(astr, "%100s %d", tname, &trights);
        if (ret <= 0)
            goto nminus_err;
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        if (tl == NULL)
            goto nminus_err;
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

  nminus_err:
    for (;first; first = next) {
        next = first->next;
        free(first);
    }
    first = ta->pluslist;

  nplus_err:
    for (;first; first = next) {
        next = first->next;
        free(first);
    }
    free(ta);
    return NULL;
}

/* clean up an access control list of its bad entries; return 1 if we made
   any changes to the list, and 0 otherwise */
extern "C" int CleanAcl(struct Acl *aa)
{
    struct AclEntry *te, **le, *ne;
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
    LONG code;
    struct Acl *ta;
    struct ViceIoctl blob;
    int changes;

    ShowMessageBox(IDS_CLEANACL_MSG, MB_OK|MB_ICONINFORMATION, IDS_CLEANACL_MSG);

    HOURGLASS hourglass;

    for (int i = 0; i < names.GetSize(); i++) {
        char space[AFS_PIOCTL_MAXSIZE];

        blob.out_size = AFS_PIOCTL_MAXSIZE;
        blob.in_size = 0;
        blob.out = space;

        code = pioctl_T(names[i], VIOCGETAL, &blob, 1);
        if (code) {
            ShowMessageBox(IDS_CLEANACL_ERROR, MB_ICONERROR, 0, names[i], GetAfsError(errno));
            continue;
        }

        ta = ParseAcl(space);
        if (ta == NULL) {
            ShowMessageBox(IDS_INVALID_ACL_DATA, MB_ICONERROR, IDS_INVALID_ACL_DATA);
            continue;
        }
        if (ta->dfs) {
            ShowMessageBox(IDS_CLEANACL_NOT_SUPPORTED, MB_ICONERROR, IDS_CLEANACL_NOT_SUPPORTED, names[i]);
            continue;
        }

        changes = CleanAcl(ta);
        if (!changes)
            continue;

        /* now set the acl */
        blob.in = AclToString(ta);
        blob.in_size = strlen((char *)blob.in) + 1;
        blob.out_size = 0;

        code = pioctl_T(names[i], VIOCSETAL, &blob, 1);
        if (code) {
            if (errno == EINVAL) {
                ShowMessageBox(IDS_CLEANACL_INVALID_ARG, MB_ICONERROR, IDS_CLEANACL_INVALID_ARG, names[i]);
                continue;
            }
            else {
                ShowMessageBox(IDS_CLEANACL_ERROR, MB_ICONERROR, 0, names[i], GetAfsError(errno));
                continue;
            }
        }
    }
}

// Derived from fs.c's ListAclCmd
BOOL GetRights(const CString& strDir, CStringArray& strNormal, CStringArray& strNegative)
{
    LONG code;
    struct Acl *ta;
    struct ViceIoctl blob;
    struct AclEntry *te;
    int idf = 0; //getidf(as, parm_listacl_id);
    char space[AFS_PIOCTL_MAXSIZE];
    HOURGLASS hourglass;

    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;

    code = pioctl_T(strDir, VIOCGETAL, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_GETRIGHTS_ERROR, MB_ICONERROR, IDS_GETRIGHTS_ERROR, strDir, GetAfsError(errno));
        return FALSE;
    }

    ta = ParseAcl(space);
    if (ta == NULL) {
        ShowMessageBox(IDS_INVALID_ACL_DATA, MB_ICONERROR, IDS_INVALID_ACL_DATA);
        return FALSE;
    }
    if (ta->dfs) {
        ShowMessageBox(IDS_DFSACL_ERROR, MB_ICONERROR, IDS_DFSACL_ERROR);
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

struct AclEntry *FindList(struct AclEntry *pCurEntry, const char *entryName)
{
    while (pCurEntry) {
        if (!foldcmp(pCurEntry->name, PCCHAR(entryName)))
            return pCurEntry;
        pCurEntry = pCurEntry->next;
    }

    return 0;
}

void ChangeList(struct Acl *pAcl, BYTE bNormalRights, const CString & entryName, LONG nEntryRights)
{
    ASSERT(pAcl);
    ASSERT(entryName);

    struct AclEntry *pEntry;
    CStringUtf8 uEntryName(entryName);

    HOURGLASS hourglass;

    pEntry = (bNormalRights ? pAcl->pluslist : pAcl->minuslist);
    pEntry = FindList(pEntry, uEntryName);

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

    strcpy(pEntry->name, uEntryName);
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

static LONG Convert(const CString& strRights, int dfs, enum rtype *rtypep)
{
    int i, len;
    LONG mode;

    *rtypep = add;	/* add rights, by default */

    if (strRights == _T("read"))
        return PRSFS_READ | PRSFS_LOOKUP;
    if (strRights == _T("write"))
        return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK;
    if (strRights == _T("mail"))
        return PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
    if (strRights == _T("all"))
        return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;

    if (strRights == _T("none")) {
        *rtypep = destroy; /* Remove entire entry */
        return 0;
    }

    len = strRights.GetLength();
    mode = 0;

    for (i = 0; i < len; i++) {
        TCHAR c = strRights[i];
        if (c == _T('r')) mode |= PRSFS_READ;
        else if (c == _T('l')) mode |= PRSFS_LOOKUP;
        else if (c == _T('i')) mode |= PRSFS_INSERT;
        else if (c == _T('d')) mode |= PRSFS_DELETE;
        else if (c == _T('w')) mode |= PRSFS_WRITE;
        else if (c == _T('k')) mode |= PRSFS_LOCK;
        else if (c == _T('a')) mode |= PRSFS_ADMINISTER;
        else {
            fprintf(stderr, "illegal rights character '%c'.\n", c);
            exit(1);
        }
    }
    return mode;
}

BOOL SaveACL(const CString& strCellName, const CString& strDir, const CStringArray& normal, const CStringArray& negative)
{
    LONG code;
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

    code = pioctl_T(strDir, VIOCSETAL, &blob, 1);
    if (code) {
        if (errno == EINVAL)
            ShowMessageBox(IDS_SAVE_ACL_EINVAL_ERROR, MB_ICONERROR, IDS_SAVE_ACL_EINVAL_ERROR, strDir);
        else
            ShowMessageBox(IDS_SAVE_ACL_ERROR, MB_ICONERROR, IDS_SAVE_ACL_ERROR, strDir, GetAfsError(errno, strDir));
    }

    ZapAcl(pAcl);

    return (code == 0);
}

BOOL CopyACL(const CString& strToDir, const CStringArray& normal, const CStringArray& negative, BOOL bClear)
{
    LONG code;
    struct ViceIoctl blob;
    struct Acl *pToAcl;
    int idf = 0; // getidf(as, parm_copyacl_id);
    char space[AFS_PIOCTL_MAXSIZE];
    HOURGLASS hourglass;

    // Get ACL to copy to
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;

    code = pioctl_T(strToDir, VIOCGETAL, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_ACL_READ_ERROR, MB_ICONERROR, IDS_ACL_READ_ERROR, strToDir, GetAfsError(errno, strToDir));
        return FALSE;
    }

    if (bClear)
        pToAcl = EmptyAcl(space);
    else
        pToAcl = ParseAcl(space);

    if (pToAcl == NULL) {
        ShowMessageBox(IDS_INVALID_ACL_DATA, MB_ICONERROR, IDS_INVALID_ACL_DATA);
        return FALSE;
    }

    CleanAcl(pToAcl);

    if (pToAcl->dfs) {
        ShowMessageBox(IDS_NO_DFS_COPY_ACL, MB_ICONERROR, IDS_NO_DFS_COPY_ACL, strToDir);
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

    code = pioctl_T(strToDir, VIOCSETAL, &blob, 1);
    if (code) {
        ZapAcl(pToAcl);
        if (errno == EINVAL)
            ShowMessageBox(IDS_COPY_ACL_EINVAL_ERROR, MB_ICONERROR, IDS_COPY_ACL_EINVAL_ERROR, strToDir);
        else
            ShowMessageBox(IDS_COPY_ACL_ERROR, MB_ICONERROR, IDS_COPY_ACL_ERROR, strToDir, GetAfsError(errno, strToDir));
        return FALSE;
    }

    ZapAcl(pToAcl);

    ShowMessageBox(IDS_COPY_ACL_OK, MB_OK|MB_ICONINFORMATION, IDS_COPY_ACL_OK);

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

    strMountPointInfo = strFile + _T("\t") + strVolume + _T("\t") + strCell + _T("\t") + strType;

    return strMountPointInfo;
}

CString ParseSymlink(const CString strFile, CString strSymlink)
{
    CString strSymlinkInfo;

    strSymlinkInfo = strFile + _T("\t") + strSymlink;

    return strSymlinkInfo;
}

BOOL IsPathInAfs(const CString & strPath)
{
    struct ViceIoctl blob;
    cm_ioctlQueryOptions_t options;
    cm_fid_t fid;
    int code;

    HOURGLASS hourglass;

    memset(&options, 0, sizeof(options));
    options.size = sizeof(options);
    options.field_flags |= CM_IOCTL_QOPTS_FIELD_LITERAL;
    options.literal = 1;
    blob.in_size = options.size;    /* no variable length data */
    blob.in = &options;
    blob.out_size = sizeof(cm_fid_t);
    blob.out = (char *) &fid;

    code = pioctl_T(strPath, VIOCGETFID, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
        return FALSE;
    }
    return TRUE;
}

static int
IsFreelanceRoot(const CString& apath)
{
    struct ViceIoctl blob;
    afs_int32 code;
    char space[AFS_PIOCTL_MAXSIZE];

    blob.in_size = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl_T(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code == 0)
        return !strcmp("Freelance.Local.Root",space);
    return 1;   /* assume it is because it is more restrictive that way */
}

static const char * NetbiosName(void)
{
    static char buffer[1024] = "AFS";
    HKEY  parmKey;
    DWORD code;
    DWORD dummyLen;
    DWORD enabled = 0;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(buffer);
        code = RegQueryValueExA(parmKey, "NetbiosName", NULL, NULL,
			       (LPBYTE)buffer, &dummyLen);
        RegCloseKey (parmKey);
    } else {
	strcpy(buffer, "AFS");
    }
    return buffer;
}

static void FixNetbiosPath(CString& path)
{
    if (!IsPathInAfs(path)) {
        CString nbroot;
        const char * nbname = NetbiosName();

        nbroot.Format(_T("\\\\%s\\"), nbname);

        if (nbroot.CompareNoCase(path) == 0) {
            path.Append(_T("all\\"));
        }
    }
}

#define AFSCLIENT_ADMIN_GROUPNAME "AFS Client Admins"

static BOOL IsAdmin (void)
{
    static BOOL fAdmin = FALSE;
    static BOOL fTested = FALSE;

    if (!fTested)
    {
        /* Obtain the SID for the AFS client admin group.  If the group does
         * not exist, then assume we have AFS client admin privileges.
         */
        PSID psidAdmin = NULL;
        DWORD dwSize, dwSize2;
        TCHAR pszAdminGroup[ MAX_COMPUTERNAME_LENGTH + sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2 ];
        TCHAR *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;

        dwSize = sizeof(pszAdminGroup);

        if (!GetComputerName(pszAdminGroup, &dwSize)) {
            /* Can't get computer name.  We return false in this case.
               Retain fAdmin and fTested. This shouldn't happen.*/
            return FALSE;
        }

        dwSize = 0;
        dwSize2 = 0;

        lstrcat(pszAdminGroup, _T("\\"));
        lstrcat(pszAdminGroup, _T(AFSCLIENT_ADMIN_GROUPNAME));

        LookupAccountName(NULL, pszAdminGroup, NULL, &dwSize, NULL, &dwSize2, &snu);
        /* that should always fail. */

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            /* if we can't find the group, then we allow the operation */
            fAdmin = TRUE;
            return TRUE;
        }

        if (dwSize == 0 || dwSize2 == 0) {
            /* Paranoia */
            fAdmin = TRUE;
            return TRUE;
        }

        psidAdmin = (PSID)malloc(dwSize); memset(psidAdmin,0,dwSize);
        pszRefDomain = (TCHAR *)malloc(dwSize2);

        if (!LookupAccountName(NULL, pszAdminGroup, psidAdmin, &dwSize, pszRefDomain, &dwSize2, &snu)) {
            /* We can't lookup the group now even though we looked it up earlier.
               Could this happen? */
            fAdmin = TRUE;
        } else {
            /* Then open our current ProcessToken */
            HANDLE hToken;

            if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {

                if (!CheckTokenMembership(hToken, psidAdmin, &fAdmin)) {
                    /* We'll have to allocate a chunk of memory to store the list of
                     * groups to which this user belongs; find out how much memory
                     * we'll need.
                     */
                    DWORD dwSize = 0;
                    PTOKEN_GROUPS pGroups;

                    GetTokenInformation (hToken, TokenGroups, NULL, dwSize, &dwSize);

                    pGroups = (PTOKEN_GROUPS)malloc(dwSize);

                    /* Allocate that buffer, and read in the list of groups. */
                    if (GetTokenInformation (hToken, TokenGroups, pGroups, dwSize, &dwSize))
                    {
                        /* Look through the list of group SIDs and see if any of them
                         * matches the AFS Client Admin group SID.
                         */
                        size_t iGroup = 0;
                        for (; (!fAdmin) && (iGroup < pGroups->GroupCount); ++iGroup)
                        {
                            if (EqualSid (psidAdmin, pGroups->Groups[ iGroup ].Sid)) {
                                fAdmin = TRUE;
                            }
                        }
                    }

                    if (pGroups)
                        free(pGroups);
                }

                /* if do not have permission because we were not explicitly listed
                 * in the Admin Client Group let's see if we are the SYSTEM account
                 */
                if (!fAdmin) {
                    PTOKEN_USER pTokenUser;
                    SID_IDENTIFIER_AUTHORITY SIDAuth = SECURITY_NT_AUTHORITY;
                    PSID pSidLocalSystem = 0;
                    DWORD gle;

                    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);

                    pTokenUser = (PTOKEN_USER)malloc(dwSize);

                    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize))
                        gle = GetLastError();

                    if (AllocateAndInitializeSid( &SIDAuth, 1,
                                                  SECURITY_LOCAL_SYSTEM_RID,
                                                  0, 0, 0, 0, 0, 0, 0,
                                                  &pSidLocalSystem))
                    {
                        if (EqualSid(pTokenUser->User.Sid, pSidLocalSystem)) {
                            fAdmin = TRUE;
                        }

                        FreeSid(pSidLocalSystem);
                    }

                    if ( pTokenUser )
                        free(pTokenUser);
                }
            }
        }

        free(psidAdmin);
        free(pszRefDomain);

        fTested = TRUE;
    }

    return fAdmin;
}

CString Parent(const CString& path)
{
    int last_slash = path.ReverseFind(_T('\\'));

    if (last_slash != -1) {
        CString ret = path.Left(last_slash + 1);
        return ret;
    } else {
        if (path.GetLength() >= 2 && path[1] == _T(':')) {
            CString ret = path.Left(2);
            ret.AppendChar(_T('.'));
            return ret;
        } else {
            CString ret = _T(".");
            return ret;
        }
    }
    }

CString LastComponent(const CString& path)
{
    int last_slash = path.ReverseFind(_T('\\'));

    if (last_slash != -1) {
        CString ret = path.Mid(last_slash + 1);
        return ret;
    } else {
        if (path.GetLength() >= 2 && path[1] == _T(':')) {
            CString ret = path.Mid(2);
            return ret;
        } else {
            CString ret = path;
            return ret;
        }
    }
}

static CString
GetCell(const CString & path)
{
    char cellname[MAXCELLCHARS];
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.out_size = sizeof(cellname);
    blob.out = cellname;

    code = pioctl_T(path, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
        CString s;
        s.Empty();

        return s;
    } else {
        return Utf8ToCString(cellname);
    }
}


BOOL ListMount(CStringArray& files)
{
    LONG code;
    struct ViceIoctl blob;
    int error;

    CString parent_dir;		/* Parent directory of true name */
    CStringUtf8 last_component;	        /* Last component of true name */

    CStringArray mountPoints;

    HOURGLASS hourglass;

    error = 0;

    for (int i = 0; i < files.GetSize(); i++) {
        int last_slash = files[i].ReverseFind(_T('\\'));
        char space[AFS_PIOCTL_MAXSIZE];

        if (last_slash != -1) {
            last_component.SetString( files[i].Mid(last_slash + 1) );
            parent_dir.SetString( files[i].Left(last_slash + 1) );
            FixNetbiosPath(parent_dir);
        } else {
            // The path is of the form "C:foo" or just "foo".  If
            // there is a drive, then use the current directory of
            // that drive.  Otherwise we just use '.'.

            if (files[i].GetLength() >= 2 && files[i][1] == _T(':')) {
                parent_dir.Format(_T("%c:."), files[i][0]);
                last_component.SetString( files[i].Mid(2) );
            } else {
                parent_dir.SetString( _T("."));
                last_component.SetString( files[i] );
	    }
        }

        blob.in_size = last_component.GetLength() + 1;
        blob.in = last_component.GetBuffer();
        blob.out_size = AFS_PIOCTL_MAXSIZE;
        blob.out = space;
        memset(space, 0, AFS_PIOCTL_MAXSIZE);

        code = pioctl_T(parent_dir, VIOC_AFS_STAT_MT_PT, &blob, 1);

        last_component.ReleaseBuffer();

        if (code == 0) {
            int nPos;
            space[AFS_PIOCTL_MAXSIZE - 1] = '\0';
            nPos = strlen(space) - 1;
            if (space[nPos] == '.')
                space[nPos] = 0;
            mountPoints.Add(ParseMountPoint(StripPath(files[i]), Utf8ToCString(space)));
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

BOOL
MakeMount(const CString& strDir,
          const CString& strVolName,
          const CString& strInCellName,
          BOOL bRW)
{
    LONG code;
    struct ViceIoctl blob;
    HOURGLASS hourglass;

    ASSERT(strVolName.GetLength() < 64);

    CString strParent = Parent(strDir);

    FixNetbiosPath(strParent);
    if (!IsPathInAfs(strParent)) {
	    ShowMessageBox(IDS_MAKE_MP_NOT_AFS_ERROR, MB_ICONERROR, IDS_MAKE_MP_NOT_AFS_ERROR);
	    return FALSE;
	}

    CString strPath = strParent + LastComponent(strDir);

    if ( IsFreelanceRoot(strParent) && !IsAdmin() ) {
        ShowMessageBox(IDS_NOT_AFS_CLIENT_ADMIN_ERROR, MB_ICONERROR,
                       IDS_NOT_AFS_CLIENT_ADMIN_ERROR);
	    return FALSE;
	}

    CString strMount;

    strMount.Format(_T("%c%s%s%s."),
                    ((bRW)?_T('%'):_T('#')),
                    strInCellName,
                    ((strInCellName.IsEmpty())?_T(""):_T(":")),
                    strVolName);

    CStringUtf8 ustrMount(strMount);

    blob.out_size = 0;
    blob.in_size = ustrMount.GetLength() + 1;
    blob.in = ustrMount.GetBuffer();
    blob.out = NULL;

    code = pioctl_T(strPath, VIOC_AFS_CREATE_MT_PT, &blob, 0);

    ustrMount.ReleaseBuffer();

    if (code) {
        ShowMessageBox(IDS_MOUNT_POINT_ERROR, MB_ICONERROR, IDS_MOUNT_POINT_ERROR, GetAfsError(errno, strDir));
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


BOOL RemoveSymlink(const CString& strName)
{
    BOOL error = FALSE;
    INT code=0;
    struct ViceIoctl blob;
    char lsbuffer[1024];

    HOURGLASS hourglass;

    CString strParent = Parent(strName);
    CStringUtf8 ustrLast(LastComponent(strName));
    FixNetbiosPath(strParent);

    if ( IsFreelanceRoot(strParent) && !IsAdmin() ) {
	ShowMessageBox(IDS_NOT_AFS_CLIENT_ADMIN_ERROR, MB_ICONERROR, IDS_NOT_AFS_CLIENT_ADMIN_ERROR);
	return FALSE;
    }

    blob.in_size = ustrLast.GetLength() + 1;
    blob.in = ustrLast.GetBuffer();
    blob.out = lsbuffer;
    blob.out_size = sizeof(lsbuffer);
    code = pioctl_T(strParent, VIOC_LISTSYMLINK, &blob, 0);
    ustrLast.ReleaseBuffer();
    if (code)
        return FALSE;
    blob.out_size = 0;
    blob.in_size = ustrLast.GetLength() + 1;
    blob.in = ustrLast.GetBuffer();

    code = pioctl_T(strParent, VIOC_DELSYMLINK, &blob, 0);

    ustrLast.ReleaseBuffer();

    return (code == 0);
}

BOOL IsSymlink(const CString& strName)
{
    struct ViceIoctl blob;
    int code;
    char space[AFS_PIOCTL_MAXSIZE];
    HOURGLASS hourglass;

    CStringUtf8 ustrLast(LastComponent(strName));
    CString strParent = Parent(strName);

    FixNetbiosPath(strParent);

    blob.in_size = ustrLast.GetLength() + 1;
    blob.in = ustrLast.GetBuffer();
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;
    memset(space, 0, AFS_PIOCTL_MAXSIZE);

    code = pioctl_T(strParent, VIOC_LISTSYMLINK, &blob, 1);

    ustrLast.ReleaseBuffer();

    return (code==0);
}


BOOL IsMountPoint(const CString& path)
{
    LONG code = 0;
    struct ViceIoctl blob;
    char lsbuffer[1024];

    HOURGLASS hourglass;

    CString parent = Parent(path);
    FixNetbiosPath(parent);

    CStringUtf8 mountpoint(LastComponent(path));

    blob.in_size = mountpoint.GetLength() + 1;
    blob.in = mountpoint.GetBuffer();
    blob.out = lsbuffer;
    blob.out_size = sizeof(lsbuffer);

    code = pioctl_T(parent, VIOC_AFS_STAT_MT_PT, &blob, 0);

    mountpoint.ReleaseBuffer();

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
    LONG code = 0;
    struct ViceIoctl blob;
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

        CString parent = Parent(files[i]);
        CStringUtf8 mountpoint(LastComponent(files[i]));
        FixNetbiosPath(parent);

        if ( IsFreelanceRoot(parent) && !IsAdmin() ) {
	    results.Add(GetMessageString(IDS_NOT_AFS_CLIENT_ADMIN_ERROR, StripPath(files[i])));
            error = TRUE;
            continue;   /* skip */
        }

        blob.out_size = 0;
        blob.in_size = mountpoint.GetLength() + 1;
        blob.in = mountpoint.GetBuffer();

        code = pioctl_T(parent, VIOC_AFS_DELETE_MT_PT, &blob, 0);

        mountpoint.ReleaseBuffer();

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
    LONG code;
    struct ViceIoctl blob;
    struct VolumeStatus *status;
    char *name;
    char space[AFS_PIOCTL_MAXSIZE];
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

    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.in_size = 0;
    blob.out = space;

    code = pioctl_T(strFile, VIOCGETVOLSTAT, &blob, 1);
    if (code || blob.out_size < sizeof(*status)) {
        volInfo.m_strErrorMsg = GetAfsError(errno, strFile);
        return FALSE;
    }

    status = (VolumeStatus *)space;
    name = (char *)status + sizeof(*status);

    volInfo.m_strName = Utf8ToCString(name);
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
    LONG code;
    struct ViceIoctl blob;
    struct VolumeStatus *status;
    char *input;
    char space[AFS_PIOCTL_MAXSIZE];
    HOURGLASS hourglass;

    blob.out_size = AFS_PIOCTL_MAXSIZE;
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

    code = pioctl_T(volInfo.m_strFilePath, VIOCSETVOLSTAT, &blob, 1);
    if (code || blob.out_size < sizeof(*status)) {
        ShowMessageBox(IDS_SET_QUOTA_ERROR, MB_ICONERROR, IDS_SET_QUOTA_ERROR, GetAfsError(errno, volInfo.m_strName));
        return FALSE;
    }

    return TRUE;
}

void GetCellName(const CString& cellNamep, struct afsconf_cell *infop)
{
    CStringUtf8 uCellName(cellNamep);

    StringCbCopyA(infop->name, sizeof(infop->name), uCellName);
}

BOOL CheckServers(const CString& strCellName, WHICH_CELLS nCellsToCheck, BOOL bFast)
{
    LONG code;
    struct ViceIoctl blob;
    LONG j;
    LONG temp = 0;
    struct afsconf_cell info;
    struct chservinfo checkserv;
    char space[AFS_PIOCTL_MAXSIZE];
    HOURGLASS hourglass;

    memset(&checkserv, 0, sizeof(struct chservinfo));
    blob.in_size = sizeof(struct chservinfo);
    blob.in = (caddr_t)&checkserv;

    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    if (nCellsToCheck == SPECIFIC_CELL) {
	temp = 2;
        GetCellName(strCellName, &info);
        strcpy(checkserv.tbuffer,info.name);
        checkserv.tsize = strlen(info.name) + 1;
    } else {
	if (nCellsToCheck != ALL_CELLS)
	    temp = 2;
        strcpy(checkserv.tbuffer, "\0");
        checkserv.tsize = 0;
    }
    if (bFast)
        temp |= 1;	/* set fast flag */

    checkserv.magic = 0x12345678;	/* XXX */
    checkserv.tflags = temp;
    checkserv.tinterval = -1;	/* don't change current interval */

    code = pioctl_utf8(0, VIOCCKSERV, &blob, 1);
    if (code) {
        ShowMessageBox(IDS_CHECK_SERVERS_ERROR, MB_ICONERROR, IDS_CHECK_SERVERS_ERROR, GetAfsError(errno, CString()));
        return FALSE;
    }

    memcpy(&temp, space, sizeof(LONG));

    if (temp == 0) {
        ShowMessageBox(IDS_ALL_SERVERS_RUNNING, MB_OK|MB_ICONINFORMATION, IDS_ALL_SERVERS_RUNNING);
        return TRUE;
    }

    CStringArray servers;
    for (j = 0; j < AFS_MAXHOSTS; j++) {
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
            ShowMessageBox(IDS_GET_TOKENS_UNEXPECTED_ERROR, MB_ICONERROR, IDS_GET_TOKENS_UNEXPECTED_ERROR, rc);
            return FALSE;
//	    printf("Unexpected error, code %d\n", rc);
//	    exit(1);
        }
        else {
            rc = ktc_GetToken(&serviceName, &token, sizeof(token), &clientName);
            if (rc) {
                ShowMessageBox(IDS_GET_TOKENS_UNEXPECTED_ERROR2, MB_ICONERROR, IDS_GET_TOKENS_UNEXPECTED_ERROR2,
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
#ifdef UNICODE
                strExpir.Format(_T("%S"), expireString);
#else
                strExpir.Format(_T("%s"), expireString);
#endif
            }

            strTokenInfo = strUserName + "\t" + strCellName + "\t" + strExpir + "\t" + strCellName;
            tokenInfo.Add(strTokenInfo);
        }
    }

//	printf("Press <Enter> or <Return> when finished: ");
//	gets(s);
    return TRUE;
}

UINT MakeSymbolicLink(const CString& strName, const CString& strTarget)
{
    struct ViceIoctl blob;
    UINT code;
    HOURGLASS hourglass;

    CString strParent = Parent(strName);
    FixNetbiosPath(strParent);

    if ( IsFreelanceRoot(strParent) && !IsAdmin() ) {
	ShowMessageBox(IDS_NOT_AFS_CLIENT_ADMIN_ERROR, MB_ICONERROR, IDS_NOT_AFS_CLIENT_ADMIN_ERROR);
	return FALSE;
    }

    CStringUtf8 ustrTarget(strTarget);

    blob.in_size = ustrTarget.GetLength() + 1;
    blob.in = ustrTarget.GetBuffer();
    blob.out_size = 0;
    blob.out = NULL;

    code = pioctl_T(strName, VIOC_SYMLINK, &blob, 0);

    ustrTarget.ReleaseBuffer();

    if (code != 0)
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
    char space[AFS_PIOCTL_MAXSIZE];
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

	if (!IsPathInAfs(parent_dir)) {
	    const char * nbname = NetbiosName();
	    int len = strlen(nbname);

	    if (parent_dir[0] == '\\' && parent_dir[1] == '\\' &&
		 parent_dir[len+2] == '\\' &&
		 parent_dir[len+3] == '\0' &&
		 !strnicmp(nbname,&parent_dir[2],len))
	    {
		sprintf(parent_dir,"\\\\%s\\all\\", nbname);
	    }
	}
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
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;
    memset(space, 0, AFS_PIOCTL_MAXSIZE);
    if ((code = pioctl(parent_dir, VIOC_LISTSYMLINK, &blob, 1)))
        strcpy(space,"???");
    ASSERT(strlen(space)<MAX_PATH);
    strncpy(strPath,space,nlenPath);
}

BOOL ListSymlink(CStringArray& files)
{
    LONG code;
    struct ViceIoctl blob;
    int error;
    CStringArray symlinks;
    char space[AFS_PIOCTL_MAXSIZE];
    HOURGLASS hourglass;

    error = 0;

    for (int i = 0; i < files.GetSize(); i++) {

        CString strParent = Parent(files[i]);
        CStringUtf8 ustrLast(LastComponent(files[i]));

        FixNetbiosPath(strParent);

        blob.in_size = ustrLast.GetLength() + 1;
        blob.in = ustrLast.GetBuffer();
        blob.out_size = AFS_PIOCTL_MAXSIZE;
        blob.out = space;
        memset(space, 0, AFS_PIOCTL_MAXSIZE);

        code = pioctl_T(strParent, VIOC_LISTSYMLINK, &blob, 1);

        ustrLast.ReleaseBuffer();

        if (code == 0) {
            CString syml;
            int len;

            space[AFS_PIOCTL_MAXSIZE - 1] = '\0';
            syml = Utf8ToCString(space);
            len = syml.GetLength();

            if (len > 0) {
                if (syml[len - 1] == _T('.'))
                    syml.Truncate(len - 1);
            }

            symlinks.Add(ParseSymlink(StripPath(files[i]), syml));

        } else {
            error = 1;
            if (errno == EINVAL)
                symlinks.Add(GetMessageString(IDS_NOT_SYMLINK_ERROR, StripPath(files[i])));
            else
                symlinks.Add(GetMessageString(IDS_LIST_MOUNT_POINT_ERROR, GetAfsError(errno, StripPath(files[i]))));
        }
    }

    CSymlinksDlg dlg;
    dlg.SetSymlinks(symlinks);
    dlg.DoModal();

    return !error;
}

