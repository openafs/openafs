/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef AFS_AIX32_ENV
#include <sys/audit.h>
#else
#define AUDIT_OK 0
#define AUDIT_FAIL 1
#define AUDIT_FAIL_AUTH 2
#define AUDIT_FAIL_ACCESS 3
#define AUDIT_FAIL_PRIV 4
#endif /* AFS_AIX32_ENV */

#include <afs/opr.h>
#include "afs/afsint.h"
#include "afs/butc.h"
#include <rx/rx.h>
#include <rx/rxkad.h>
#include "audit.h"
#include "audit-api.h"
#include "lock.h"

#include <afs/afsutil.h>

extern struct osi_audit_ops audit_file_ops;
#ifdef HAVE_SYS_IPC_H
extern struct osi_audit_ops audit_sysvmq_ops;
#endif

static struct {
    void *rock;
    int (*islocal)(void *rock, char *name, char *inst, char *cell);
} audit_user_check = { NULL, NULL };

static struct {
    const char *name;
    const struct osi_audit_ops *ops;
} audit_interfaces[] = {

    { "file", &audit_file_ops },
#ifdef HAVE_SYS_IPC_H
    { "sysvmq", &audit_sysvmq_ops },
#endif
};
/* default_interface indexes into the audit_interfaces list. */
static int default_interface = 0; /* Set default to the file interface */

#define N_INTERFACES (sizeof(audit_interfaces) / sizeof(audit_interfaces[0]))

/*
 * Audit interface calling sequence:
 * osi_audit_interface - sets the default audit interface
 * osi_audit_file
 *    create_instance - Called during command arg processing (-auditlog)
 *    open_file - Called during command arg processing (-auditlog)
 * osi_audit_open_interface
 *    open_interface - Called after thread environment has been established
 * osi_audit_close_interface
 *    close_interface - Called during main process shutdown
 * osi_audit
 *    send_msg - Called during audit events
 */
struct audit_log {
    struct opr_queue link;
    const struct osi_audit_ops *audit_ops;
    int auditout_open;
    void *context;
};

struct audit_msg {
    char buf[OSI_AUDIT_MAXMSG];
    size_t len; /* length of the string in 'buf' (not including the trailing '\0') */
    int truncated; /* was this message truncated during formatting? */
};

#ifdef AFS_PTHREAD_ENV
static pthread_mutex_t audit_lock;
static pthread_once_t audit_lock_once = PTHREAD_ONCE_INIT;
#endif

/* Chain of active interfaces */
static struct opr_queue audit_logs = {&audit_logs, &audit_logs};

static int osi_audit_all = (-1);	/* Not determined yet */
static int osi_echo_trail = (-1);

static int auditout_open = 0;		/* True if any interface is open */

static int osi_audit_check(void);

/*
 * Send the message to all the interfaces
 * @pre audit_lock held
 */
static void
multi_send_msg(struct audit_msg *msg)
{
    struct opr_queue *cursor;

    if (msg->len < 1) /* Don't send empty strings */
	return;

    for (opr_queue_Scan(&audit_logs, cursor)) {
	struct audit_log *alog;
	alog = opr_queue_Entry(cursor, struct audit_log, link);
	if (alog->auditout_open) {
	    alog->audit_ops->send_msg(alog->context, msg->buf, msg->len,
				      msg->truncated);
	}
    }
}
static void
append_msg(struct audit_msg *msg, const char *format, ...)
{
    va_list ap;
    int remaining, printed;

    if (msg->truncated) {
	return;
    }

    if (msg->len >= OSI_AUDIT_MAXMSG - 1) {
	/* Make sure we have at least some space left */
	msg->truncated = 1;
	return;
    }

    remaining = OSI_AUDIT_MAXMSG - msg->len;

    va_start(ap, format);

    printed = vsnprintf(&msg->buf[msg->len], remaining, format, ap);

    if (printed < 0) {
	/* Error during formatting. */
	msg->truncated = 1;
	printed = 0;

    } else if (printed >= remaining) {
	/* We tried to write more characters than we had space for. */
	msg->truncated = 1;
	printed = remaining - 1;

	/* Make sure we're still terminated. */
	msg->buf[OSI_AUDIT_MAXMSG - 1] = '\0';
    }

    msg->len += printed;
    opr_Assert(msg->len < OSI_AUDIT_MAXMSG);

    va_end(ap);
}
#ifdef AFS_AIX32_ENV
static char *bufferPtr;
static int bufferLen;

static void
audmakebuf(char *audEvent, va_list vaList)
{
    int code;
    int vaEntry;
    int vaInt;
    afs_int32 vaLong;
    char *vaStr;
    struct AFSFid *vaFid;

    vaEntry = va_arg(vaList, int);
    while (vaEntry != AUD_END) {
	switch (vaEntry) {
	case AUD_STR:		/* String */
        case AUD_NAME:          /* Name */
        case AUD_ACL:           /* ACL */
	    vaStr = (char *)va_arg(vaList, char *);
	    if (vaStr) {
		strcpy(bufferPtr, vaStr);
		bufferPtr += strlen(vaStr) + 1;
	    } else {
		strcpy(bufferPtr, "");
		bufferPtr++;
	    }
	    break;
	case AUD_INT:		/* Integer */
        case AUD_ID:            /* ViceId */
	    vaInt = va_arg(vaList, int);
	    *(int *)bufferPtr = vaInt;
	    bufferPtr += sizeof(vaInt);
	    break;
	case AUD_DATE:		/* Date    */
	case AUD_HOST:		/* Host ID */
	case AUD_LONG:		/* long    */
	    vaLong = va_arg(vaList, afs_int32);
	    *(afs_int32 *) bufferPtr = vaLong;
	    bufferPtr += sizeof(vaLong);
	    break;
	case AUD_FID:		/* AFSFid - contains 3 entries */
	    vaFid = (struct AFSFid *)va_arg(vaList, struct AFSFid *);
	    if (vaFid) {
		memcpy(bufferPtr, vaFid, sizeof(struct AFSFid));
	    } else {
		memset(bufferPtr, 0, sizeof(struct AFSFid));
	    }
	    bufferPtr += sizeof(struct AFSFid);
	    break;

	    /* Whole array of fids-- don't know how to handle variable length audit
	     * data with AIX audit package, so for now we just store the first fid.
	     * Better one than none. */
	case AUD_FIDS:
	    {
		struct AFSCBFids *Fids;

		Fids = (struct AFSCBFids *)va_arg(vaList, struct AFSCBFids *);
		if (Fids && Fids->AFSCBFids_len) {
		    *((u_int *) bufferPtr) = Fids->AFSCBFids_len;
		    bufferPtr += sizeof(u_int);
		    memcpy(bufferPtr, Fids->AFSCBFids_val,
			   sizeof(struct AFSFid));
		} else {
		    *((u_int *) bufferPtr) = 0;
		    bufferPtr += sizeof(u_int);
		    memset(bufferPtr, 0, sizeof(struct AFSFid));
		}
		bufferPtr += sizeof(struct AFSFid);
		break;
	    }
	/* butc tape label */
	case AUD_TLBL:
	    {
		struct tc_tapeLabel *label;

		label = (struct tc_tapeLabel *)va_arg(vaList,
						      struct tc_tapeLabel *);
		if (label)
		    memcpy(bufferPtr, label, sizeof(*label));
		else
		    memset(bufferPtr, 0, sizeof(*label));
		bufferPtr += sizeof(label);
		break;
	    }
	/* butc dump interface */
	case AUD_TDI:
	    {
		struct tc_dumpInterface *di;

		di = (struct tc_dumpInterface *)
			va_arg(vaList, struct tc_dumpInterface *);
		if (di)
		    memcpy(bufferPtr, di, sizeof(*di));
		else
		    memset(bufferPtr, 0, sizeof(*di));
		bufferPtr += sizeof(*di);
		break;
	    }
	/*
	 * butc dump array
	 * An array of dump descriptions, but the AIX audit package assumes fixed
	 * length, so we can only do the first one for now.
	 */
	case AUD_TDA:
	    {
		struct tc_dumpArray *da;

		da = (struct tc_dumpArray *)
			va_arg(vaList, struct tc_dumpArray *);
		if (da && da->tc_dumpArray_len) {
		    memcpy(bufferPtr, &da->tc_dumpArray_len, sizeof(u_int));
		    bufferPtr += sizeof(u_int);
		    memcpy(bufferPtr, da->tc_dumpArray_val,
			   sizeof(da->tc_dumpArray_val[0]));
		} else {
		    memset(bufferPtr, 0, sizeof(u_int));
		    bufferPtr += sizeof(u_int);
		    memset(bufferPtr, 0, sizeof(da->tc_dumpArray_val[0]));
		}
		bufferPtr += sizeof(da->tc_dumpArray_val[0]);
		break;
	    }
	/*
	 * butc restore array
	 * An array of restore descriptions, but the AIX audit package assumes
	 * fixed length, so we can only do the first one for now.
	 */
	case AUD_TRA:
	    {
		struct tc_restoreArray *ra;

		ra = (struct tc_restoreArray *)
			va_arg(vaList, struct tc_restoreArray *);
		if (ra && ra->tc_restoreArray_len) {
		    memcpy(bufferPtr, &ra->tc_restoreArray_len, sizeof(u_int));
		    bufferPtr += sizeof(u_int);
		    memcpy(bufferPtr, ra->tc_restoreArray_val,
			   sizeof(ra->tc_restoreArray_val[0]));
		} else {
		    memset(bufferPtr, 0, sizeof(u_int));
		    bufferPtr += sizeof(u_int);
		    memset(bufferPtr, 0, sizeof(ra->tc_restoreArray_val[0]));
		}
		bufferPtr += sizeof(ra->tc_restoreArray_val[0]);
		break;
	    }
	/* butc tape controller status */
	case AUD_TSTT:
	    {
		struct tciStatusS *status;

		status = (struct tciStatusS *)va_arg(vaList,
						     struct tciStatusS *);
		if (status)
		    memcpy(bufferPtr, status, sizeof(*status));
		else
		    memset(bufferPtr, 0, sizeof(*status));
		bufferPtr += sizeof(*status);
		break;
	    }
	default:
#ifdef AFS_AIX32_ENV
	    code =
		auditlog("AFS_Aud_EINVAL", (-1), audEvent,
			 (strlen(audEvent) + 1));
#endif
	    return;
	    break;
	}			/* end switch */

	vaEntry = va_arg(vaList, int);
    }				/* end while */
}
#endif

static void
printbuf(int rec, char *audEvent, char *afsName, afs_int32 hostId,
	 afs_int32 errCode, va_list vaList)
{
    int vaEntry;
    int vaInt;
    afs_int32 vaLong;
    char *vaStr;
    struct AFSFid *vaFid;
    struct AFSCBFids *vaFids;
    struct tc_tapeLabel *vaLabel;
    struct tc_dumpInterface *vaDI;
    struct tc_dumpArray *vaDA;
    struct tc_restoreArray *vaRA;
    struct tciStatusS *vaTCstatus;
    int num = LogThreadNum();
    struct in_addr hostAddr;
    time_t currenttime;
    char tbuffer[26];
    struct tm tm;
    struct audit_msg *msg = calloc(1, sizeof(*msg));

    if (!msg) {
	return;
    }

    /* Don't print the timestamp or thread id if we recursed */
    if (rec == 0) {
	currenttime = time(0);
	if (strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y ",
		     localtime_r(&currenttime, &tm)) !=0)
	    append_msg(msg, tbuffer);

	if (num > -1)
	    append_msg(msg, "[%d] ", num);
    }

    append_msg(msg, "EVENT %s CODE %d ", audEvent, errCode);

    if (afsName) {
	hostAddr.s_addr = hostId;
	append_msg(msg, "NAME %s HOST %s ", afsName, inet_ntoa(hostAddr));
    }

    vaEntry = va_arg(vaList, int);
    while (vaEntry != AUD_END) {
	switch (vaEntry) {
	case AUD_STR:		/* String */
	    vaStr = (char *)va_arg(vaList, char *);
	    if (vaStr)
		append_msg(msg, "STR %s ", vaStr);
	    else
		append_msg(msg, "STR <null>");
	    break;
	case AUD_NAME:		/* Name */
	    vaStr = (char *)va_arg(vaList, char *);
	    if (vaStr)
		append_msg(msg, "NAME %s ", vaStr);
	    else
		append_msg(msg, "NAME <null>");
	    break;
	case AUD_ACL:		/* ACL */
	    vaStr = (char *)va_arg(vaList, char *);
	    if (vaStr)
		append_msg(msg, "ACL %s ", vaStr);
	    else
		append_msg(msg, "ACL <null>");
	    break;
	case AUD_INT:		/* Integer */
	    vaInt = va_arg(vaList, int);
	    append_msg(msg, "INT %d ", vaInt);
	    break;
	case AUD_ID:		/* ViceId */
	    vaInt = va_arg(vaList, int);
	    append_msg(msg, "ID %d ", vaInt);
	    break;
	case AUD_DATE:		/* Date    */
	    vaLong = va_arg(vaList, afs_int32);
	    append_msg(msg, "DATE %u ", vaLong);
	    break;
	case AUD_HOST:		/* Host ID */
	    vaLong = va_arg(vaList, afs_int32);
            hostAddr.s_addr = vaLong;
	    append_msg(msg, "HOST %s ", inet_ntoa(hostAddr));
	    break;
	case AUD_LONG:		/* afs_int32    */
	    vaLong = va_arg(vaList, afs_int32);
	    append_msg(msg, "LONG %d ", vaLong);
	    break;
	case AUD_FID:		/* AFSFid - contains 3 entries */
	    vaFid = va_arg(vaList, struct AFSFid *);
	    if (vaFid)
		append_msg(msg, "FID %u:%u:%u ", vaFid->Volume, vaFid->Vnode,
			   vaFid->Unique);
	    else
		append_msg(msg, "FID %u:%u:%u ", 0, 0, 0);
	    break;
	case AUD_FIDS:		/* array of Fids */
	    vaFids = va_arg(vaList, struct AFSCBFids *);

	    if (vaFids) {
                unsigned int i;

                vaFid = vaFids->AFSCBFids_val;

                if (vaFid) {
		    append_msg(msg, "FIDS %u ", vaFids->AFSCBFids_len);
                    for ( i = 1; i <= vaFids->AFSCBFids_len; i++, vaFid++ )
			append_msg(msg, "FID %u:%u:%u ", vaFid->Volume,
				   vaFid->Vnode, vaFid->Unique);
                } else
		    append_msg(msg, "FIDS 0 FID 0:0:0 ");

            }
	    break;
	case AUD_TLBL:		/* butc tape label */
	    vaLabel = va_arg(vaList, struct tc_tapeLabel *);

	    if (vaLabel) {
		append_msg(msg, "TAPELABEL %d:%.*s:%.*s:%u ",
			   vaLabel->size,
			   TC_MAXTAPELEN, vaLabel->afsname,
			   TC_MAXTAPELEN, vaLabel->pname,
			   vaLabel->tapeId);
	    } else {
		append_msg(msg, "TAPELABEL <null>");
	    }
	    break;
	case AUD_TDI:
	    vaDI = va_arg(vaList, struct tc_dumpInterface *);

	    if (vaDI) {
		append_msg(msg,
    "TCDUMPINTERFACE %.*s:%.*s:%.*s:%d:%d:%d:%d:%.*s:%.*s:%d:%d:%d:%d:%d ",
    TC_MAXDUMPPATH, vaDI->dumpPath, TC_MAXNAMELEN, vaDI->volumeSetName,
    TC_MAXNAMELEN, vaDI->dumpName, vaDI->parentDumpId, vaDI->dumpLevel,
    vaDI->doAppend,
    vaDI->tapeSet.id, TC_MAXHOSTLEN, vaDI->tapeSet.tapeServer,
    TC_MAXFORMATLEN, vaDI->tapeSet.format, vaDI->tapeSet.maxTapes,
    vaDI->tapeSet.a, vaDI->tapeSet.b, vaDI->tapeSet.expDate,
    vaDI->tapeSet.expType);
	    } else {
		append_msg(msg, "TCDUMPINTERFACE <null>");
	    }
	    break;
	case AUD_TDA:
	    vaDA = va_arg(vaList, struct tc_dumpArray *);

	    if (vaDA) {
		u_int i;
		struct tc_dumpDesc *desc;
		struct in_addr hostAddr;

		desc = vaDA->tc_dumpArray_val;
		if (desc) {
		    append_msg(msg, "DUMPS %d ", vaDA->tc_dumpArray_len);
		    for (i = 0; i < vaDA->tc_dumpArray_len; i++, desc++) {
			hostAddr.s_addr = desc->hostAddr;
			append_msg(msg, "DUMP %d:%d:%.*s:%d:%d:%d:%s ",
				   desc->vid, desc->vtype, TC_MAXNAMELEN, desc->name,
				   desc->partition, desc->date, desc->cloneDate,
				   inet_ntoa(hostAddr));
		    }
		} else {
		    append_msg(msg, "DUMPS 0 DUMP 0:0::0:0:0:0.0.0.0");
		}
	    }
	    break;
	case AUD_TRA:
	    vaRA = va_arg(vaList, struct tc_restoreArray *);

	    if (vaRA) {
		u_int i;
		struct tc_restoreDesc *desc;
		struct in_addr hostAddr;

		desc = vaRA->tc_restoreArray_val;
		if (desc) {
		    append_msg(msg, "RESTORES %d ",
					  vaRA->tc_restoreArray_len);
		    for(i = 0; i < vaRA->tc_restoreArray_len; i++, desc++) {
			hostAddr.s_addr = desc->hostAddr;
			append_msg(msg,
				   "RESTORE %d:%.*s:%d:%d:%d:%d:%d:%d:%d:%s:%.*s:%.*s ",
				   desc->flags, TC_MAXTAPELEN, desc->tapeName,
				   desc->dbDumpId, desc->initialDumpId,
				   desc->position, desc->origVid, desc->vid,
				   desc->partition, desc->dumpLevel,
				   inet_ntoa(hostAddr), TC_MAXNAMELEN,
				   desc->oldName, TC_MAXNAMELEN, desc->newName);
		    }
		} else {
		    append_msg(msg,
			"RESTORES 0 RESTORE 0::0:0:0:0:0:0:0:0.0.0.0::: ");
		}
	    }
	    break;
	case AUD_TSTT:
	    vaTCstatus = va_arg(vaList, struct tciStatusS *);

	    if (vaTCstatus)
		append_msg(msg, "TCSTATUS %.*s:%d:%d:%d:%d:%.*s:%d:%d ",
			   TC_MAXNAMELEN, vaTCstatus->taskName,
			   vaTCstatus->taskId, vaTCstatus->flags,
			   vaTCstatus->dbDumpId, vaTCstatus->nKBytes,
			   TC_MAXNAMELEN, vaTCstatus->volumeName,
			   vaTCstatus->volsFailed,
			   vaTCstatus->lastPolled);
	    else
		append_msg(msg, "TCSTATUS <null>");
	    break;
	default:
	    append_msg(msg, "--badval-- ");
	    break;
	}			/* end switch */
	vaEntry = va_arg(vaList, int);
    }				/* end while */

    MUTEX_ENTER(&audit_lock);
    multi_send_msg(msg);
    MUTEX_EXIT(&audit_lock);

    free(msg);
}

#ifdef AFS_PTHREAD_ENV
static void
osi_audit_init_lock(void)
{
    MUTEX_INIT(&audit_lock, "audit", MUTEX_DEFAULT, 0);
}
#endif

void
osi_audit_init(void)
{
#ifdef AFS_PTHREAD_ENV
    pthread_once(&audit_lock_once, osi_audit_init_lock);
#endif /* AFS_PTHREAD_ENV */
}

/* ************************************************************************** */
/* The routine that acually does the audit call.
 * ************************************************************************** */
static int
osi_audit_internal(char *audEvent,	/* Event name (15 chars or less) */
		   afs_int32 errCode,	/* The error code */
		   char *afsName,
		   afs_int32 hostId,
		   va_list vaList)
{
#ifdef AFS_AIX32_ENV
    afs_int32 code;
    afs_int32 err;
    static char BUFFER[32768];
    int result;
#endif

#ifdef AFS_PTHREAD_ENV
    /* i'm pretty sure all the server apps now call osi_audit_init(),
     * but to be extra careful we'll leave this in here for a
     * while to make sure */
    pthread_once(&audit_lock_once, osi_audit_init_lock);
#endif /* AFS_PTHREAD_ENV */

    if ((osi_audit_all < 0) || (osi_echo_trail < 0))
	osi_audit_check();
    if (!osi_audit_all && !auditout_open)
	return 0;

#ifdef AFS_AIX32_ENV
    switch (errCode) {
    case 0:
	result = AUDIT_OK;
	break;
    case KANOAUTH:		/* kautils.h   */
    case RXKADNOAUTH:		/* rxkad.h     */
	result = AUDIT_FAIL_AUTH;
	break;
    case EPERM:		/* errno.h     */
    case EACCES:		/* errno.h     */
    case PRPERM:		/* pterror.h   */
	result = AUDIT_FAIL_ACCESS;
	break;
    case VL_PERM:		/* vlserver.h  */
    case BUDB_NOTPERMITTED:	/* budb_errs.h */
    case BZACCESS:		/* bnode.h     */
    case VOLSERBAD_ACCESS:	/* volser.h    */
	result = AUDIT_FAIL_PRIV;
	break;
    default:
	result = AUDIT_FAIL;
	break;
    }
#endif

#ifdef AFS_AIX32_ENV
    MUTEX_ENTER(&audit_lock);
    bufferPtr = BUFFER;

    /* Put the error code into the buffer list */
    *(int *)bufferPtr = errCode;
    bufferPtr += sizeof(errCode);

    audmakebuf(audEvent, vaList);

    bufferLen = (int)((afs_int32) bufferPtr - (afs_int32) & BUFFER[0]);
    code = auditlog(audEvent, result, BUFFER, bufferLen);
    MUTEX_EXIT(&audit_lock);
#else
    if (auditout_open) {
	printbuf(0, audEvent, afsName, hostId, errCode, vaList);
    }
#endif

    return 0;
}
int
osi_audit(char *audEvent,	/* Event name (15 chars or less) */
	  afs_int32 errCode,	/* The error code */
	  ...)
{
    va_list vaList;

    if ((osi_audit_all < 0) || (osi_echo_trail < 0))
	osi_audit_check();
    if (!osi_audit_all && !auditout_open)
	return 0;

    va_start(vaList, errCode);
    osi_audit_internal(audEvent, errCode, NULL, 0, vaList);
    va_end(vaList);

    return 0;
}

/* ************************************************************************** */
/* Given a RPC call structure, this routine extracts the name and host id from the
 * call and includes it within the audit information.
 * ************************************************************************** */
int
osi_auditU(struct rx_call *call, char *audEvent, int errCode, ...)
{
    struct rx_connection *conn;
    struct rx_peer *peer;
    afs_int32 secClass;
    afs_int32 code;
    char afsName[MAXKTCNAMELEN + MAXKTCNAMELEN + MAXKTCREALMLEN + 3];
    afs_int32 hostId;
    va_list vaList;

    if (osi_audit_all < 0)
	osi_audit_check();
    if (!osi_audit_all && !auditout_open)
	return 0;

    strcpy(afsName, "--Unknown--");
    hostId = 0;

    if (call) {
	conn = rx_ConnectionOf(call);	/* call -> conn) */
	if (conn) {
            secClass = rx_SecurityClassOf(conn);	/* conn -> securityIndex */
	    if (secClass == RX_SECIDX_NULL) {	/* unauthenticated */
		osi_audit("AFS_Aud_Unauth", (-1), AUD_STR, audEvent, AUD_END);
		strcpy(afsName, "--UnAuth--");
	    } else if (secClass == RX_SECIDX_KAD || secClass == RX_SECIDX_KAE) {
		/* authenticated with rxkad */
                char tcell[MAXKTCREALMLEN];
                char name[MAXKTCNAMELEN];
                char inst[MAXKTCNAMELEN];

                code =
		    rxkad_GetServerInfo(conn, NULL, NULL, name, inst, tcell,
					NULL);
		if (code) {
		    osi_audit("AFS_Aud_NoAFSId", (-1), AUD_STR, audEvent, AUD_END);
		    strcpy(afsName, "--NoName--");
		} else {
		    afs_int32 islocal = 0;
		    if (audit_user_check.islocal) {
			islocal =
			    audit_user_check.islocal(audit_user_check.rock,
						     name, inst, tcell);
		    }
		    strlcpy(afsName, name, sizeof(afsName));
		    if (inst[0]) {
			strlcat(afsName, ".", sizeof(afsName));
			strlcat(afsName, inst, sizeof(afsName));
		    }
		    if (tcell[0] && !islocal) {
			strlcat(afsName, "@", sizeof(afsName));
			strlcat(afsName, tcell, sizeof(afsName));
		    }
		}
	    } else {		/* Unauthenticated and/or unknown */
		osi_audit("AFS_Aud_UnknSec", (-1), AUD_STR, audEvent, AUD_END);
                strcpy(afsName, "--Unknown--");
	    }
	    peer = rx_PeerOf(conn);	/* conn -> peer */
	    if (peer)
		hostId = rx_HostOf(peer);	/* peer -> host */
	    else
		osi_audit("AFS_Aud_NoHost", (-1), AUD_STR, audEvent, AUD_END);
	} else {		/* null conn */
	    osi_audit("AFS_Aud_NoConn", (-1), AUD_STR, audEvent, AUD_END);
	}
    } else {			/* null call */
	osi_audit("AFS_Aud_NoCall", (-1), AUD_STR, audEvent, AUD_END);
    }
    va_start(vaList, errCode);
    osi_audit_internal(audEvent, errCode, afsName, hostId, vaList);
    va_end(vaList);
    return 0;
}

/* ************************************************************************** */
/* Determines whether auditing is on or off by looking at the Audit file.
 * If the string AFS_AUDIT_AllEvents is defined in the file, then auditing will be
 * enabled.
 * ************************************************************************** */

int
osi_audit_check(void)
{
    FILE *fds;
    int onoff;
    char event[257];

    osi_audit_all = 1;		/* say we made check (>= 0) */
    /* and assume audit all events (for now) */
    onoff = 0;			/* assume we will turn auditing off */
    osi_echo_trail = 0;		/* assume no echoing   */

    fds = fopen(AFSDIR_SERVER_AUDIT_FILEPATH, "r");
    if (fds) {
	while (fscanf(fds, "%256s", event) > 0) {
	    if (strcmp(event, "AFS_AUDIT_AllEvents") == 0)
		onoff = 1;

	    if (strcmp(event, "Echo_Trail") == 0)
		osi_echo_trail = 1;
	}
	fclose(fds);
    }

    /* Audit this event all of the time */
    if (onoff)
	osi_audit("AFS_Aud_On", 0, AUD_END);
    else
	osi_audit("AFS_Aud_Off", 0, AUD_END);

    /* Now set whether we audit all events from here on out */
    osi_audit_all = onoff;

    return 0;
}

/*!
 * Helper for processing cmd line options
 *
 * Process the parameters for the -audit-interface and -auditlog command line
 * options
 *
 * @param[in] default_iface
 * 	String for the default audit interface or NULL
 * @param[in] audit_loglist
 * 	Pointer to a cmd_item list of audit logs or NULL
 *
 * @return status
 * @retval	0	- success
 * @retval	-1	- Error (message printed to stderr)
 */

int
osi_audit_cmd_Options(char *default_iface, struct cmd_item *audit_loglist)
{

    if (default_iface) {
	if (osi_audit_interface(default_iface)) {
	    fprintf(stderr, "Invalid auditinterface '%s'\n", default_iface);
	    return -1;
	}
    }

    while (audit_loglist != NULL) {
	if (osi_audit_file(audit_loglist->data)) {
	    fprintf(stderr, "Error processing auditlog options '%s'\n",
		    audit_loglist->data);
	    return -1;
	}
	audit_loglist = audit_loglist->next;
    }
    return 0;
}

 /*
 * Handle parsing a string: [interface_name:]filespec[:options]
 * The string a:b will parse 'a' as the interface name and 'b' as the filespec.
 * Note that the string pointed by optionstr will be modified
 * by strtok_r by inserting '\0' between the tokens.
 * The values returned in interface_name, filespec and options
 * are pointers to the 'sub-strings' within optionstr.
 */
static int
parse_file_options(char *optionstr,
		   const char **interface_name,
		   char **filespec,
		   char **options)
{
    int code = 0;
    char *opt_cursor = optionstr;
    char *tok1 = NULL, *tok2 = NULL, *tok3 = NULL, *tokptrsave = NULL;

    /*
     * Handle the fact that strtok doesn't handle empty fields e.g. a::b
     * and will return tok1-> a tok2-> b
     */

    /* 1st field */
    if (*opt_cursor != ':') {
	tok1 = strtok_r(opt_cursor, ":", &tokptrsave);
	opt_cursor = strtok_r(NULL, "", &tokptrsave);
    } else  {
	/* Skip the ':' */
	opt_cursor++;
    }

    /* 2nd field */
    if (opt_cursor != NULL) {
	if (*opt_cursor != ':') {
	    tok2 = strtok_r(opt_cursor, ":", &tokptrsave);
	    opt_cursor = strtok_r(NULL, "", &tokptrsave);
	} else {
	    /* Skip the ':' */
	    opt_cursor++;
	}
    }

    /* 3rd field is just the remainder if any */
    tok3 = opt_cursor;

    if (tok1 == NULL || strlen(tok1) == 0) {
	fprintf(stderr, "Missing -auditlog parameter\n");
	code = EINVAL;
	goto done;
    }

    /* If only one token, then it's the filespec */
    if (tok2 == NULL && tok3 == NULL) {
	*filespec = tok1;
    } else {
	*interface_name = tok1;
	*filespec = tok2;
	*options = tok3;
    }

 done:
    return code;
}

/*
 * Parse the options looking for comma-seperated values.
 */
static int
parse_option_string(const struct osi_audit_ops *ops, void *rock, char *options)
{
    int code = 0;
    char *tok1, *tokptrsave = NULL;

    tok1 = strtok_r(options, ",", &tokptrsave);
    while (tok1) {
	/* Handle opt=val or just opt */
	char *opt, *val;
	char *optvalsave;

	opt = strtok_r(tok1, "=", &optvalsave);
	val = strtok_r(NULL, "", &optvalsave);

	code = ops->set_option(rock, opt, val);

	if (code) {
	    /* Bad option */
	    goto done;
	}
	tok1 = strtok_r(NULL, ",", &tokptrsave);
    }

 done:
    return code;
}

/*
 * Process -auditlog
 *  [interface]:filespec[:options]
 *      interface - interface name (optional - defaults to default_interface)
 *      filespec - depends on the interface (required)
 *      options - optional string passed to interface
 * Returns 0 - success
 *        EINVAL - option error
 *        ENOMEM - error allocating memory
 */

int
osi_audit_file(const char *fileplusoptions)
{

    int idx;
    int code;

    char *optionstr = NULL;

    const char *interface_name = NULL;
    char *filespec = NULL;
    char *options = NULL;

    const struct osi_audit_ops *ops = NULL;
    struct audit_log *new_alog = NULL;

    /* Use the default unless specified */
    interface_name = audit_interfaces[default_interface].name;

    /* dup of the input string so the parsing can safely modify it */
    optionstr = strdup(fileplusoptions);
    if (!optionstr) {
	code = ENOMEM;
	goto done;
    }

    code = parse_file_options(optionstr, &interface_name, &filespec, &options);
    if (code)
	goto done;

    if (interface_name && strlen(interface_name) != 0) {
	for (idx = 0; idx < N_INTERFACES; idx++) {
	    if (strcmp(interface_name, audit_interfaces[idx].name) == 0) {
		ops = audit_interfaces[idx].ops;
		break;
	    }
	}
	if (ops == NULL) {
	    /* Couldn't find the interface name */
	    fprintf(stderr, "Could not find the specified audit interface %s\n", interface_name);
	    code = EINVAL;
	    goto done;
	}
    } else {
	fprintf(stderr, "Missing interface name\n");
	code = EINVAL;
	goto done;
    }

    if (filespec == NULL || strlen(filespec) == 0) {
	fprintf(stderr, "Missing auditlog path for %s interface\n", interface_name);
	code = EINVAL;
	goto done;
    }

    opr_Assert(ops->create_interface != NULL);
    opr_Assert(ops->close_interface != NULL);
    opr_Assert(ops->open_file != NULL);
    opr_Assert(ops->send_msg != NULL);
    opr_Assert(ops->print_interface_stats != NULL);
    /* open_interface, set_option and close_interface are optional */

    new_alog = calloc(1, sizeof(*new_alog));
    if (!new_alog) {
	code = ENOMEM;
	goto done;
    }

    new_alog->audit_ops = ops;
    new_alog->auditout_open = 0;

    new_alog->context = ops->create_interface();
    if (new_alog->context == NULL) {
	    code = ENOMEM;
	    goto done;
    }

    if (options != NULL && ops->set_option != NULL) {
	/* Split the option string at commas */
	code = parse_option_string(ops, new_alog->context, options);
	if (code)
	    goto done;
    }

    code = ops->open_file(new_alog->context, filespec);
    if (code) {
	/* Error opening file */
	goto done;
    }

    new_alog->auditout_open = 1;
    auditout_open = 1;

    /* Add to chain of active interfaces */
    opr_queue_Append(&audit_logs, &new_alog->link);
    new_alog = NULL;

    code = 0;

 done:
     if (code) {
	 /* Error condition present.. */
	 if (new_alog) {
	     ops->close_interface(&new_alog->context);
	     free(new_alog);
	 }
     }

    if (optionstr)
	free(optionstr);
    return code;
}

/*
 * Set the default interface
 * return 0 for success
 *        EINVAL missing or invalid interface name
 */
int
osi_audit_interface(const char *interface)
{
    int idx;

    if (interface == NULL || strlen(interface) == 0)
	return EINVAL;

    for (idx = 0; idx < N_INTERFACES; idx++) {
	if (strcmp(interface, audit_interfaces[idx].name) == 0) {
	    default_interface = idx;
	    return 0;
	}
    }
    return EINVAL;
}

/*
 * Let the interfaces finish initialization
 */
void
osi_audit_open(void)
{
    struct opr_queue *cursor;

    for (opr_queue_Scan(&audit_logs, cursor)) {
	struct audit_log *alog;
	alog = opr_queue_Entry(cursor, struct audit_log, link);
	if (alog->auditout_open && alog->audit_ops->open_interface != NULL)
	    alog->audit_ops->open_interface(alog->context);
    }
}

/*
 * Shutdown the interfaces
 */
void
osi_audit_close(void)
{
    struct opr_queue *cursor, *cursorsave;

    for (opr_queue_ScanSafe(&audit_logs, cursor, cursorsave)) {
	struct audit_log *alog;
	alog = opr_queue_Entry(cursor, struct audit_log, link);
	alog->audit_ops->close_interface(&alog->context);
	opr_queue_Remove(&alog->link);
	free(alog);
    }
}

void
osi_audit_set_user_check(void *rock,
			 int (*islocal) (void *rock, char *name, char *inst,
					 char *cell))
{
    audit_user_check.rock = rock;
    audit_user_check.islocal = islocal;
}

void
audit_PrintStats(FILE *out)
{
    struct opr_queue *cursor;

    for (opr_queue_Scan(&audit_logs, cursor)) {
	struct audit_log *alog;
	alog = opr_queue_Entry(cursor, struct audit_log, link);
	if (alog->auditout_open)
	    alog->audit_ops->print_interface_stats(alog->context, out);
    }
}
