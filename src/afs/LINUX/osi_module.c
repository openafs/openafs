/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux module support routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/unistd.h"		/* For syscall numbers. */
#include "h/mm.h"

#ifdef AFS_AMD64_LINUX20_ENV
#include "../asm/ia32_unistd.h"
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#include <linux/sched.h>
#endif
#if !defined(EXPORTED_SYS_CALL_TABLE) && defined(HAVE_KERNEL_LINUX_SYSCALL_H)
#include <linux/syscall.h>
#endif

#ifdef AFS_SPARC64_LINUX24_ENV
#define __NR_setgroups32      82	/* This number is not exported for some bizarre reason. */
#endif

#if !defined(AFS_LINUX24_ENV)
asmlinkage int (*sys_settimeofdayp) (struct timeval * tv,
				     struct timezone * tz);
#endif
asmlinkage long (*sys_setgroupsp) (int gidsetsize, gid_t * grouplist);
#ifdef EXPORTED_SYS_CALL_TABLE
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
extern unsigned int sys_call_table[];	/* changed to uint because SPARC64 and S390X have syscalltable of 32bit items */
#else
extern void *sys_call_table[];	/* safer for other linuces */
#endif
#else /* EXPORTED_SYS_CALL_TABLE */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
static unsigned int *sys_call_table;	/* changed to uint because SPARC64 and S390X have syscalltable of 32bit items */
#else
static void **sys_call_table;	/* safer for other linuces */
#endif
#endif

#if defined(AFS_S390X_LINUX24_ENV)
#define _S(x) ((x)<<1)
#else
#define _S(x) x
#endif

extern struct file_system_type afs_fs_type;

#if !defined(AFS_LINUX24_ENV)
static long get_page_offset(void);
#endif

#if defined(AFS_LINUX24_ENV)
DECLARE_MUTEX(afs_global_lock);
#else
struct semaphore afs_global_lock = MUTEX;
#endif
int afs_global_owner = 0;
#if !defined(AFS_LINUX24_ENV)
unsigned long afs_linux_page_offset = 0;	/* contains the PAGE_OFFSET value */
#endif

/* Since sys_ni_syscall is not exported, I need to cache it in order to restore
 * it.
 */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
static unsigned int afs_ni_syscall = 0;
#else
static void *afs_ni_syscall = 0;
#endif

#ifdef AFS_AMD64_LINUX20_ENV
#ifdef EXPORTED_IA32_SYS_CALL_TABLE
extern void *ia32_sys_call_table[];
#else
static void **ia32_sys_call_table;
#endif

static void *ia32_ni_syscall = 0;
asmlinkage long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
#if defined(__NR_ia32_setgroups32)
asmlinkage long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
#endif /* __NR_ia32_setgroups32 */
#endif /* AFS_AMD64_LINUX20_ENV */

#ifdef AFS_PPC64_LINUX20_ENV
asmlinkage long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */

#ifdef AFS_SPARC64_LINUX20_ENV
static unsigned int afs_ni_syscall32 = 0;
asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
				    __kernel_gid_t32 * grouplist);
#if defined(__NR_setgroups32)
asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
				      __kernel_gid_t32 * grouplist);
#endif /* __NR_setgroups32 */
#ifdef EXPORTED_SYS_CALL_TABLE
extern unsigned int sys_call_table32[];
#else /* EXPORTED_SYS_CALL_TABLE */
static unsigned int *sys_call_table32;
#endif /* EXPORTED_SYS_CALL_TABLE */

asmlinkage int
afs_syscall32(long syscall, long parm1, long parm2, long parm3, long parm4,
	      long parm5)
{
    __asm__ __volatile__("srl %o4, 0, %o4\n\t" "mov %o7, %i7\n\t"
			 "call afs_syscall\n\t" "srl %o5, 0, %o5\n\t"
			 "ret\n\t" "nop");
}
#endif /* AFS_SPARC64_LINUX20_ENV */

static int afs_ioctl(struct inode *, struct file *, unsigned int,
		     unsigned long);

static struct file_operations afs_syscall_fops = {
    .ioctl = afs_ioctl,
};

int
csdbproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, j;
    struct afs_q *cq, *tq;
    struct cell *tc;
    char tbuffer[16];
    afs_uint32 addr;
    
    len = 0;
    ObtainReadLock(&afs_xcell);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	len += sprintf(buffer + len, ">%s #(%d/%d)\n", tc->cellName, 
		       tc->cellNum, tc->cellIndex);
	for (j = 0; j < MAXCELLHOSTS; j++) {
	    if (!tc->cellHosts[j]) break;
	    addr = ntohl(tc->cellHosts[j]->addr->sa_ip);
	    sprintf(tbuffer, "%d.%d.%d.%d", 
		    (int)((addr>>24) & 0xff), (int)((addr>>16) & 0xff),
		    (int)((addr>>8)  & 0xff), (int)( addr      & 0xff));
            len += sprintf(buffer + len, "%s #%s\n", tbuffer, tbuffer);
	}
    }
    ReleaseReadLock(&afs_xcell);
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

int
peerproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, i, j;
    struct rx_peer *sep;
    
    len = 0;
    for (i = 0, j = 0; i < 256; i++) {
        for (sep = rx_peerHashTable[i]; sep; sep = sep->next, j++) {
	    len += sprintf(buffer + len, "%lx: next=0x%lx, host=0x%x, ", (unsigned long)sep,
		   (unsigned long)sep->next, sep->host);
            len += sprintf(buffer + len, "ifMTU=%d, natMTU=%d, maxMTU=%d\n", sep->ifMTU,
                   sep->natMTU, sep->maxMTU);
            len += sprintf(buffer + len, "\trtt=%d:%d, timeout(%d:%d), nSent=%d, reSends=%d\n",
                   sep->rtt, sep->rtt_dev, sep->timeout.sec,
                   sep->timeout.usec, sep->nSent, sep->reSends);
            len += sprintf(buffer + len, "\trefCount=%d, port=%d, idleWhen=0x%x\n",
                   sep->refCount, sep->port, sep->idleWhen);
            len += sprintf(buffer + len, "\tCongestionQueue (0x%lx:0x%lx), inPacketSkew=0x%x, outPacketSkew=0x%x\n",
                 (unsigned long)sep->congestionQueue.prev, (unsigned long)sep->congestionQueue.next,
                 sep->inPacketSkew, sep->outPacketSkew);
#ifdef RX_ENABLE_LOCKS
            len += sprintf(buffer + len, "\tpeer_lock=%d\n", sep->peer_lock);
#endif /* RX_ENABLE_LOCKS */
        }
    }
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

int
rxstatsproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, i;
    
    len = 0;
    len += sprintf(buffer + len, "packetRequests = %d\n", rx_stats.packetRequests);
    len += sprintf(buffer + len, "noPackets[%d] = %d\n", RX_PACKET_CLASS_RECEIVE,
           rx_stats.receivePktAllocFailures);
    len += sprintf(buffer + len, "noPackets[%d] = %d\n", RX_PACKET_CLASS_SEND,
           rx_stats.sendPktAllocFailures);
    len += sprintf(buffer + len, "noPackets[%d] = %d\n", RX_PACKET_CLASS_SPECIAL,
           rx_stats.specialPktAllocFailures);
    len += sprintf(buffer + len, "noPackets[%d] = %d\n", RX_PACKET_CLASS_RECV_CBUF,
           rx_stats.receiveCbufPktAllocFailures);
    len += sprintf(buffer + len, "noPackets[%d] = %d\n", RX_PACKET_CLASS_SEND_CBUF,
           rx_stats.sendCbufPktAllocFailures);
    len += sprintf(buffer + len, "socketGreedy = %d\n", rx_stats.socketGreedy);
    len += sprintf(buffer + len, "bogusPacketOnRead = %d\n", rx_stats.bogusPacketOnRead);
    len += sprintf(buffer + len, "bogusHost = %d\n", rx_stats.bogusHost);
    len += sprintf(buffer + len, "noPacketOnRead = %d\n", rx_stats.noPacketOnRead);
    len += sprintf(buffer + len, "noPacketBuffersOnRead = %d\n",
           rx_stats.noPacketBuffersOnRead);
    len += sprintf(buffer + len, "selects = %d\n", rx_stats.selects);
    len += sprintf(buffer + len, "sendSelects = %d\n", rx_stats.sendSelects);
    for (i = 0; i < RX_N_PACKET_TYPES; i++)
        len += sprintf(buffer + len, "packetsRead[%d] = %d\n", i, rx_stats.packetsRead[i]);
    len += sprintf(buffer + len, "dataPacketsRead = %d\n", rx_stats.dataPacketsRead);
    len += sprintf(buffer + len, "ackPacketsRead = %d\n", rx_stats.ackPacketsRead);
    len += sprintf(buffer + len, "dupPacketsRead = %d\n", rx_stats.dupPacketsRead);
    len += sprintf(buffer + len, "spuriousPacketsRead = %d\n", rx_stats.spuriousPacketsRead);
    for (i = 0; i < RX_N_PACKET_TYPES; i++)
        len += sprintf(buffer + len, "packetsSent[%d] = %d\n", i, rx_stats.packetsSent[i]);
    len += sprintf(buffer + len, "ackPacketsSent = %d\n", rx_stats.ackPacketsSent);
    len += sprintf(buffer + len, "pingPacketsSent = %d\n", rx_stats.pingPacketsSent);
    len += sprintf(buffer + len, "abortPacketsSent = %d\n", rx_stats.abortPacketsSent);
    len += sprintf(buffer + len, "busyPacketsSent = %d\n", rx_stats.busyPacketsSent);
    len += sprintf(buffer + len, "dataPacketsSent = %d\n", rx_stats.dataPacketsSent);
    len += sprintf(buffer + len, "dataPacketsReSent = %d\n", rx_stats.dataPacketsReSent);
    len += sprintf(buffer + len, "dataPacketsPushed = %d\n", rx_stats.dataPacketsPushed);
    len += sprintf(buffer + len, "ignoreAckedPacket = %d\n", rx_stats.ignoreAckedPacket);
    len += sprintf(buffer + len, "totalRtt = %d sec, %d usec\n", rx_stats.totalRtt.sec,
           rx_stats.totalRtt.usec);
    len += sprintf(buffer + len, "minRtt = %d sec, %d usec\n", rx_stats.minRtt.sec,
           rx_stats.minRtt.usec);
    len += sprintf(buffer + len, "maxRtt = %d sec, %d usec\n", rx_stats.maxRtt.sec,
           rx_stats.maxRtt.usec);
    len += sprintf(buffer + len, "nRttSamples = %d\n", rx_stats.nRttSamples);
    len += sprintf(buffer + len, "nServerConns = %d\n", rx_stats.nServerConns);
    len += sprintf(buffer + len, "nClientConns = %d\n", rx_stats.nClientConns);
    len += sprintf(buffer + len, "nPeerStructs = %d\n", rx_stats.nPeerStructs);
    len += sprintf(buffer + len, "nCallStructs = %d\n", rx_stats.nCallStructs);
    len += sprintf(buffer + len, "nFreeCallStructs = %d\n", rx_stats.nFreeCallStructs);
    len += sprintf(buffer + len, "netSendFailures  = %d\n", rx_stats.netSendFailures);
    len += sprintf(buffer + len, "fatalErrors      = %d\n", rx_stats.fatalErrors);
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

int
rxproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, i;
    
    len = 0;
    len += sprintf(buffer + len, "rx_extraQuota = %d\n", rx_extraQuota);
    len += sprintf(buffer + len, "rx_extraPackets = %d\n", rx_extraPackets);
    len += sprintf(buffer + len, "rx_stackSize = %d\n", rx_stackSize);
    len += sprintf(buffer + len, "rx_connDeadTime = %d\n", rx_connDeadTime);
    len += sprintf(buffer + len, "rx_idleConnectionTime = %d\n", rx_idleConnectionTime);
    len += sprintf(buffer + len, "rx_idlePeerTime = %d\n", rx_idlePeerTime);
    len += sprintf(buffer + len, "rx_initSendWindow = %d\n", rx_initSendWindow);
    len += sprintf(buffer + len, "rxi_nSendFrags = %d\n", rxi_nSendFrags);
    len += sprintf(buffer + len, "rx_nPackets = %d\n", rx_nPackets);
    len += sprintf(buffer + len, "rx_nFreePackets = %d\n", rx_nFreePackets);
    len += sprintf(buffer + len, "rx_socket = 0x%lx\n", (unsigned long)rx_socket);
    len += sprintf(buffer + len, "rx_Port = %d\n", rx_port);
    for (i = 0; i < RX_N_PACKET_CLASSES; i++)
	len += sprintf(buffer + len, "\trx_packetQuota[%d] = %d\n", i, rx_packetQuota[i]);

    len += sprintf(buffer + len, "rx_nextCid = 0x%x\n", rx_nextCid);
    len += sprintf(buffer + len, "rx_epoch = 0u%u\n", rx_epoch);
    len += sprintf(buffer + len, "rx_waitingForPackets = %x\n", rx_waitingForPackets);
    len += sprintf(buffer + len, "rxi_nCalls = %d\n", rxi_nCalls);
    len += sprintf(buffer + len, "rxi_dataQuota = %d\n", rxi_dataQuota);
    len += sprintf(buffer + len, "rxi_availProcs = %d\n", rxi_availProcs);
    len += sprintf(buffer + len, "rxi_totalMin = %d\n", rxi_totalMin);
    len += sprintf(buffer + len, "rxi_minDeficit = %d\n", rxi_minDeficit);

    len += sprintf(buffer + len, "rxevent_nFree = %d\nrxevent_nPosted = %d\n", rxevent_nFree, rxevent_nPosted);
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

int
connproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, i, j;
    struct rx_connection *sep;
    
    len = 0;
    for (i = 0, j = 0; i < 256; i++) {
        for (sep = rx_connHashTable[i]; sep; sep = sep->next, j++) {
	    len += sprintf(buffer + len, "%lx: next=0x%lx, peer=0x%lx, epoch=0x%x, cid=0x%x, ackRate=%d\n",
			   (unsigned long)sep, (unsigned long)sep->next, (unsigned long)sep->peer,
			   sep->epoch, sep->cid, sep->ackRate);
	    len += sprintf(buffer + len, "\tcall[%lx=%d, %lx=%d, %lx=%d, %lx=%d]\n", 
			   (unsigned long)sep->call[0], sep->callNumber[0],
			   (unsigned long)sep->call[1], sep->callNumber[1],
			   (unsigned long)sep->call[2], sep->callNumber[2],
			   (unsigned long)sep->call[3], sep->callNumber[3]);
            len += sprintf(buffer + len, "\ttimeout=%d, flags=0x%x, type=0x%x, serviceId=%d, service=0x%lx, refCount=%d\n",
			   sep->timeout, sep->flags, sep->type, 
			   sep->serviceId, (unsigned long)sep->service, sep->refCount);
            len += sprintf(buffer + len, "\tserial=%d, lastSerial=%d, secsUntilDead=%d, secsUntilPing=%d, secIndex=%d\n",
			   sep->serial, sep->lastSerial, sep->secondsUntilDead,
			   sep->secondsUntilPing, sep->securityIndex);
            len += sprintf(buffer + len, "\terror=%d, secObject=0x%lx, secData=0x%lx, secHeaderSize=%d, secmaxTrailerSize=%d\n",
			   sep->error, (unsigned long)sep->securityObject,
			   (unsigned long)sep->securityData,
			   sep->securityHeaderSize, sep->securityMaxTrailerSize);
            len += sprintf(buffer + len, "\tchallEvent=0x%lx, lastSendTime=0x%x, maxSerial=%d, hardDeadTime=%d\n",
			   (unsigned long)sep->challengeEvent, sep->lastSendTime, 
			   sep->maxSerial, sep->hardDeadTime);
            if (sep->flags & RX_CONN_MAKECALL_WAITING)
		len += sprintf(buffer + len, "\t***** Conn in RX_CONN_MAKECALL_WAITING state *****\n");
#ifdef RX_ENABLE_LOCKS
            len += sprintf(buffer + len, "\tcall_lock=%d, call_cv=%d, data_lock=%d, refCount=%d\n",
			   sep->conn_call_lock, sep->conn_call_cv, 
			   sep->conn_data_lock, sep->refCount);
#endif /* RX_ENABLE_LOCKS */
        }
    }
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

int
servicesproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, i, j;
    struct rx_service *sentry;
    
    len = 0;
    for (i = 0, j = 0; i < RX_MAX_SERVICES; i++) {
        if ((sentry = rx_services[i])) {
            j++;
	    len += sprintf(buffer + len,
			   "\t%lx: serviceId=%d, port=%d, serviceName=%s, socket=0x%lx\n",
			   (unsigned long)sentry, sentry->serviceId, sentry->servicePort,
			   sentry->serviceName, (unsigned long)sentry->socket);
	    len += sprintf(buffer + len,
			   "\t\tnSecObj=%d, nReqRunning=%d, maxProcs=%d, minProcs=%d, connDeadTime=%d, idleDeadTime=%d\n",
			   sentry->nSecurityObjects, sentry->nRequestsRunning,
			   sentry->maxProcs, sentry->minProcs, 
			   sentry->connDeadTime, sentry->idleDeadTime);
	}
    }
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

int
callproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, i, j, k;
    struct rx_connection *sep;
    
    len = 0;
    for (i = 0, j = 0; i < 256; i++) {
        for (sep = rx_connHashTable[i]; sep; sep = sep->next) {
	    for (k = 0; k < RX_MAXCALLS; k++) {
                struct rx_call *call = sep->call[k];
                if (call) {
                    j++;
		    len += sprintf(buffer + len,
				   "%lx: conn=0x%lx, qiheader(0x%lx:0x%lx), tq(0x%lx:0x%lx), rq(0x%lx:0x%lx)\n",
				   (unsigned long)call, (unsigned long)call->conn,
				   (unsigned long)call->queue_item_header.prev,
				   (unsigned long)call->queue_item_header.next,
				   (unsigned long)call->tq.prev, (unsigned long)call->tq.next,
				   (unsigned long)call->rq.prev, (unsigned long)call->rq.next);
                    len += sprintf(buffer + len, 
				   "\t: curvec=%d, curpos=%lx, nLeft=%d, nFree=%d, currPacket=0x%lx, callNumber=0x%lx\n",
				   call->curvec, (unsigned long)call->curpos, call->nLeft,
				   call->nFree, (unsigned long)call->currentPacket,
				   (unsigned long)call->callNumber);
		    len += sprintf(buffer + len,
				   "\t: channel=%d, state=0x%x, mode=0x%x, flags=0x%x, localStatus=0x%x, remStatus=0x%x\n",
				   call->channel, call->state, call->mode,
				   call->flags, call->localStatus,
				   call->remoteStatus);
                    len += sprintf(buffer + len,
				   "\t: error=%d, timeout=0x%x, rnext=0x%x, rprev=0x%x, rwind=0x%x, tfirst=0x%x, tnext=0x%x\n",
				   call->error, call->timeout, call->rnext,
				   call->rprev, call->rwind, call->tfirst,
				   call->tnext);
                    len += sprintf(buffer + len,
				   "\t: twind=%d, resendEvent=0x%lx, timeoutEvent=0x%lx, keepAliveEvent=0x%lx, delayedAckEvent=0x%lx\n",
				   call->twind, (unsigned long)call->resendEvent,
				   (unsigned long)call->timeoutEvent,
				   (unsigned long)call->keepAliveEvent,
				   (unsigned long)call->delayedAckEvent);
		    len += sprintf(buffer + len,
				   "\t: lastSendTime=0x%x, lastReceiveTime=0x%x, lastAcked=0x%x, startTime=0x%x, startWait=0x%x\n",
				   call->lastSendTime, call->lastReceiveTime,
				   call->lastAcked, call->startTime.sec,
				   call->startWait);
                    if (call->flags & RX_CALL_WAIT_PROC)
                        len += sprintf(buffer + len,
				       "\t******** Call in RX_CALL_WAIT_PROC state **********\n");
                    if (call->flags & RX_CALL_WAIT_WINDOW_ALLOC)
                        len += sprintf(buffer + len,
				       "\t******** Call in RX_CALL_WAIT_WINDOW_ALLOC state **********\n");
                    if (call->flags & RX_CALL_READER_WAIT)
                        len += sprintf(buffer + len,
				       "\t******** Conn in RX_CALL_READER_WAIT state **********\n");
                    if (call->flags & RX_CALL_WAIT_PACKETS)
                        len += sprintf(buffer + len,
				       "\t******** Conn in RX_CALL_WAIT_PACKETS state **********\n");
#ifdef RX_ENABLE_LOCKS
                    len += sprintf(buffer + len,
				   "\t: lock=0x%x, cv_twind=0x%x, cv_rq=0x%x, refCount= %d\n",
				   call->lock, call->cv_twind, call->cv_rq,
				   call->refCount);
#endif /* RX_ENABLE_LOCKS */
                    len += sprintf(buffer + len, "\t: MTU=%d\n", call->MTU);
                }
	    }
        }
    }
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

static struct proc_dir_entry *openafs_procfs;

static int
afsproc_init()
{
    struct proc_dir_entry *entry1;
    struct proc_dir_entry *entry;

    openafs_procfs = proc_mkdir(PROC_FSDIRNAME, proc_root_fs);
    entry1 = create_proc_entry(PROC_SYSCALL_NAME, 0666, openafs_procfs);

    entry1->proc_fops = &afs_syscall_fops;

    entry1->owner = THIS_MODULE;

    entry = create_proc_read_entry(PROC_CELLSERVDB_NAME, (S_IFREG|S_IRUGO), openafs_procfs, csdbproc_read, NULL);

    entry = create_proc_read_entry(PROC_PEER_NAME, (S_IFREG|S_IRUGO), openafs_procfs, peerproc_read, NULL);

    entry = create_proc_read_entry(PROC_CONN_NAME, (S_IFREG|S_IRUGO), openafs_procfs, connproc_read, NULL);

    entry = create_proc_read_entry(PROC_CALL_NAME, (S_IFREG|S_IRUGO), openafs_procfs, connproc_read, NULL);

    entry = create_proc_read_entry(PROC_RX_NAME, (S_IFREG|S_IRUGO), openafs_procfs, rxproc_read, NULL);

    entry = create_proc_read_entry(PROC_SERVICES_NAME, (S_IFREG|S_IRUGO), openafs_procfs, servicesproc_read, NULL);

    entry = create_proc_read_entry(PROC_RXSTATS_NAME, (S_IFREG|S_IRUGO), openafs_procfs, rxstatsproc_read, NULL);

    return 0;
}

static void
afsproc_exit()
{
    remove_proc_entry(PROC_RXSTATS_NAME, openafs_procfs);
    remove_proc_entry(PROC_SERVICES_NAME, openafs_procfs);
    remove_proc_entry(PROC_RX_NAME, openafs_procfs);
    remove_proc_entry(PROC_CALL_NAME, openafs_procfs);
    remove_proc_entry(PROC_CONN_NAME, openafs_procfs);
    remove_proc_entry(PROC_PEER_NAME, openafs_procfs);
    remove_proc_entry(PROC_CELLSERVDB_NAME, openafs_procfs);
    remove_proc_entry(PROC_SYSCALL_NAME, openafs_procfs);
    remove_proc_entry(PROC_FSDIRNAME, proc_root_fs);
}

extern asmlinkage long
afs_syscall(long syscall, long parm1, long parm2, long parm3, long parm4);

static int
afs_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{

    struct afsprocdata sysargs;

    if (cmd != VIOC_SYSCALL) return -EINVAL;

    if (copy_from_user(&sysargs, (void *)arg, sizeof(struct afsprocdata)))
	return -EFAULT;

    return afs_syscall(sysargs.syscall, sysargs.param1,
		       sysargs.param2, sysargs.param3, sysargs.param4);
}

#ifdef AFS_IA64_LINUX20_ENV

asmlinkage long
afs_syscall_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
    __asm__ __volatile__("alloc r42 = ar.pfs, 8, 3, 6, 0\n\t" "mov r41 = b0\n\t"	/* save rp */
			 "mov out0 = in0\n\t" "mov out1 = in1\n\t" "mov out2 = in2\n\t" "mov out3 = in3\n\t" "mov out4 = in4\n\t" "mov out5 = gp\n\t"	/* save gp */
			 ";;\n" ".L1:    mov r3 = ip\n\t" ";;\n\t" "addl r15=.fptr_afs_syscall-.L1,r3\n\t" ";;\n\t" "ld8 r15=[r15]\n\t" ";;\n\t" "ld8 r16=[r15],8\n\t" ";;\n\t" "ld8 gp=[r15]\n\t" "mov b6=r16\n\t" "br.call.sptk.many b0 = b6\n\t" ";;\n\t" "mov ar.pfs = r42\n\t" "mov b0 = r41\n\t" "mov gp = r48\n\t"	/* restore gp */
			 "br.ret.sptk.many b0\n" ".fptr_afs_syscall:\n\t"
			 "data8 @fptr(afs_syscall)");
}

asmlinkage long
afs_xsetgroups_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
    __asm__ __volatile__("alloc r42 = ar.pfs, 8, 3, 6, 0\n\t" "mov r41 = b0\n\t"	/* save rp */
			 "mov out0 = in0\n\t" "mov out1 = in1\n\t" "mov out2 = in2\n\t" "mov out3 = in3\n\t" "mov out4 = in4\n\t" "mov out5 = gp\n\t"	/* save gp */
			 ";;\n" ".L2:    mov r3 = ip\n\t" ";;\n\t" "addl r15=.fptr_afs_xsetgroups - .L2,r3\n\t" ";;\n\t" "ld8 r15=[r15]\n\t" ";;\n\t" "ld8 r16=[r15],8\n\t" ";;\n\t" "ld8 gp=[r15]\n\t" "mov b6=r16\n\t" "br.call.sptk.many b0 = b6\n\t" ";;\n\t" "mov ar.pfs = r42\n\t" "mov b0 = r41\n\t" "mov gp = r48\n\t"	/* restore gp */
			 "br.ret.sptk.many b0\n" ".fptr_afs_xsetgroups:\n\t"
			 "data8 @fptr(afs_xsetgroups)");
}

struct fptr {
    void *ip;
    unsigned long gp;
};

#endif /* AFS_IA64_LINUX20_ENV */

#ifdef AFS_LINUX24_ENV
asmlinkage int (*sys_setgroups32p) (int gidsetsize,
				    __kernel_gid32_t * grouplist);
#endif /* AFS_LINUX24_ENV */

#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
#define POINTER2SYSCALL (unsigned int)(unsigned long)
#define SYSCALL2POINTER (void *)(long)
#else
#define POINTER2SYSCALL (void *)
#define SYSCALL2POINTER (void *)
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
int __init
afs_init(void)
#else
int
init_module(void)
#endif
{
#if defined(AFS_IA64_LINUX20_ENV)
    unsigned long kernel_gp = 0;
    static struct fptr sys_setgroups;
#endif /* defined(AFS_IA64_LINUX20_ENV) */
    extern long afs_xsetgroups();
#if defined(__NR_setgroups32)
    extern int afs_xsetgroups32();
#endif /* __NR_setgroups32 */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined (AFS_AMD64_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
    extern int afs32_xsetgroups();
#if (defined(__NR_setgroups32) && defined(AFS_SPARC64_LINUX20_ENV))
    extern int afs32_xsetgroups32();
#endif /* defined(__NR_setgroups32) && defined(AFS_SPARC64_LINUX20_ENV) */
#if (defined(__NR_ia32_setgroups32) && defined(AFS_AMD64_LINUX20_ENV))
    extern int afs32_xsetgroups32();
#endif /* (defined(__NR_ia32_setgroups32) && defined(AFS_AMD64_LINUX20_ENV) */
#endif /* AFS_SPARC64_LINUX20_ENV || AFS_AMD64_LINUX20_ENV || AFS_PPC64_LINUX20_ENV */

#if !defined(EXPORTED_SYS_CALL_TABLE) || (defined(AFS_AMD64_LINUX20_ENV) && !defined(EXPORTED_IA32_SYS_CALL_TABLE))
    unsigned long *ptr;
    unsigned long offset=0;
    unsigned long datalen=0;
#if defined(EXPORTED_KALLSYMS_SYMBOL)
    unsigned long token=0;
#endif
#if defined(EXPORTED_KALLSYMS_SYMBOL) || defined(EXPORTED_KALLSYMS_ADDRESS)
    int ret;
    char *mod_name;
    unsigned long mod_start=0;
    unsigned long mod_end=0;
    char *sec_name;
    unsigned long sec_start=0;
    unsigned long sec_end=0;
    char *sym_name;
    unsigned long sym_start=0;
    unsigned long sym_end=0;
#endif
#endif /* EXPORTED_SYS_CALL_TABLE */

    RWLOCK_INIT(&afs_xosi, "afs_xosi");

#if !defined(AFS_LINUX24_ENV)
    /* obtain PAGE_OFFSET value */
    afs_linux_page_offset = get_page_offset();

#ifndef AFS_S390_LINUX22_ENV
    if (afs_linux_page_offset == 0) {
	/* couldn't obtain page offset so can't continue */
	printf("afs: Unable to obtain PAGE_OFFSET. Exiting..");
	return -EIO;
    }
#endif /* AFS_S390_LINUX22_ENV */
#endif /* !defined(AFS_LINUX24_ENV) */

#ifndef EXPORTED_SYS_CALL_TABLE
    sys_call_table = 0;

#ifdef EXPORTED_KALLSYMS_SYMBOL
    ret = 1;
    token = 0;
    while (ret) {
	sym_start = 0;
	ret =
	    kallsyms_symbol_to_address("sys_call_table", &token, &mod_name,
				       &mod_start, &mod_end, &sec_name,
				       &sec_start, &sec_end, &sym_name,
				       &sym_start, &sym_end);
	if (ret && !strcmp(mod_name, "kernel"))
	    break;
    }
    if (ret && sym_start) {
	sys_call_table = sym_start;
    }
#elif defined(EXPORTED_KALLSYMS_ADDRESS)
    ret =
	kallsyms_address_to_symbol((unsigned long)&init_mm, &mod_name,
				   &mod_start, &mod_end, &sec_name,
				   &sec_start, &sec_end, &sym_name,
				   &sym_start, &sym_end);
    ptr = (unsigned long *)sec_start;
    datalen = (sec_end - sec_start) / sizeof(unsigned long);
#elif defined(AFS_IA64_LINUX20_ENV)
    ptr = (unsigned long *)(&sys_close - 0x180000);
    datalen = 0x180000 / sizeof(ptr);
#elif defined(AFS_AMD64_LINUX20_ENV)
    ptr = (unsigned long *)&init_mm;
    datalen = 0x360000 / sizeof(ptr);
#else
    ptr = (unsigned long *)&init_mm;
    datalen = 16384;
#endif
    for (offset = 0; offset < datalen; ptr++, offset++) {
#if defined(AFS_IA64_LINUX20_ENV)
	unsigned long close_ip =
	    (unsigned long)((struct fptr *)&sys_close)->ip;
	unsigned long chdir_ip =
	    (unsigned long)((struct fptr *)&sys_chdir)->ip;
	unsigned long write_ip =
	    (unsigned long)((struct fptr *)&sys_write)->ip;
	if (ptr[0] == close_ip && ptr[__NR_chdir - __NR_close] == chdir_ip
	    && ptr[__NR_write - __NR_close] == write_ip) {
	    sys_call_table = (void *)&(ptr[-1 * (__NR_close - 1024)]);
	    break;
	}
#elif defined(EXPORTED_SYS_WAIT4) && defined(EXPORTED_SYS_CLOSE)
	if (ptr[0] == (unsigned long)&sys_close
	    && ptr[__NR_wait4 - __NR_close] == (unsigned long)&sys_wait4) {
	    sys_call_table = ptr - __NR_close;
	    break;
	}
#elif defined(EXPORTED_SYS_CHDIR) && defined(EXPORTED_SYS_CLOSE)
	if (ptr[0] == (unsigned long)&sys_close
	    && ptr[__NR_chdir - __NR_close] == (unsigned long)&sys_chdir) {
	    sys_call_table = ptr - __NR_close;
	    break;
	}
#elif defined(EXPORTED_SYS_OPEN)
	if (ptr[0] == (unsigned long)&sys_exit
	    && ptr[__NR_open - __NR_exit] == (unsigned long)&sys_open) {
	    sys_call_table = ptr - __NR_exit;
	    break;
	}
#else /* EXPORTED_SYS_OPEN */
	break;
#endif /* EXPORTED_KALLSYMS_ADDRESS */
    }
#ifdef EXPORTED_KALLSYMS_ADDRESS
    ret =
	kallsyms_address_to_symbol((unsigned long)sys_call_table, &mod_name,
				   &mod_start, &mod_end, &sec_name,
				   &sec_start, &sec_end, &sym_name,
				   &sym_start, &sym_end);
    if (ret && strcmp(sym_name, "sys_call_table"))
	sys_call_table = 0;
#endif /* EXPORTED_KALLSYMS_ADDRESS */
    if (!sys_call_table) {
	printf("Failed to find address of sys_call_table\n");
    } else {
	printf("Found sys_call_table at %lx\n", (unsigned long)sys_call_table);
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
	error cant support this yet.;
#endif /* AFS_SPARC64_LINUX20_ENV */
#endif /* EXPORTED_SYS_CALL_TABLE */

#ifdef AFS_AMD64_LINUX20_ENV
#ifndef EXPORTED_IA32_SYS_CALL_TABLE
	ia32_sys_call_table = 0;
#ifdef EXPORTED_KALLSYMS_SYMBOL
	ret = 1;
	token = 0;
	while (ret) {
	    sym_start = 0;
	    ret = kallsyms_symbol_to_address("ia32_sys_call_table", &token,
					     &mod_name, &mod_start, &mod_end,
					     &sec_name, &sec_start, &sec_end,
					     &sym_name, &sym_start, &sym_end);
	    if (ret && !strcmp(mod_name, "kernel"))
		break;
	}
	if (ret && sym_start) {
	    ia32_sys_call_table = sym_start;
	}
#else /* EXPORTED_KALLSYMS_SYMBOL */
#ifdef EXPORTED_KALLSYMS_ADDRESS
	ret = kallsyms_address_to_symbol((unsigned long)
					 &interruptible_sleep_on,
					 &mod_name, &mod_start, &mod_end,
					 &sec_name, &sec_start, &sec_end,
					 &sym_name, &sym_start, &sym_end);
	ptr = (unsigned long *)sec_start;
	datalen = (sec_end - sec_start) / sizeof(unsigned long);
#else /* EXPORTED_KALLSYMS_ADDRESS */
#if defined(AFS_AMD64_LINUX20_ENV)
	ptr = (unsigned long *)&interruptible_sleep_on;
	datalen = 0x180000 / sizeof(ptr);
#else /* AFS_AMD64_LINUX20_ENV */
	ptr = (unsigned long *)&interruptible_sleep_on;
	datalen = 16384;
#endif /* AFS_AMD64_LINUX20_ENV */
#endif /* EXPORTED_KALLSYMS_ADDRESS */
	for (offset = 0; offset < datalen; ptr++, offset++) {
	    if (ptr[0] == (unsigned long)&sys_exit
		&& ptr[__NR_ia32_open - __NR_ia32_exit] ==
		(unsigned long)&sys_open) {
		    ia32_sys_call_table = ptr - __NR_ia32_exit;
		    break;
	    }
	}
#ifdef EXPORTED_KALLSYMS_ADDRESS
	ret = kallsyms_address_to_symbol((unsigned long)ia32_sys_call_table,
					 &mod_name, &mod_start, &mod_end,
					 &sec_name, &sec_start, &sec_end,
					 &sym_name, &sym_start, &sym_end);
	if (ret && strcmp(sym_name, "ia32_sys_call_table"))
	    ia32_sys_call_table = 0;
#endif /* EXPORTED_KALLSYMS_ADDRESS */
#endif /* EXPORTED_KALLSYMS_SYMBOL */
	if (!ia32_sys_call_table) {
	    printf("Warning: Failed to find address of ia32_sys_call_table\n");
	} else {
	    printf("Found ia32_sys_call_table at %lx\n", (unsigned long)ia32_sys_call_table);
	}
#else
	printf("Found ia32_sys_call_table at %lx\n", (unsigned long)ia32_sys_call_table);
#endif /* IA32_SYS_CALL_TABLE */
#endif

	/* Initialize pointers to kernel syscalls. */
#if !defined(AFS_LINUX24_ENV)
	sys_settimeofdayp = SYSCALL2POINTER sys_call_table[_S(__NR_settimeofday)];
#endif /* AFS_IA64_LINUX20_ENV */

	/* setup AFS entry point. */
	if (
#if defined(AFS_IA64_LINUX20_ENV)
		SYSCALL2POINTER sys_call_table[__NR_afs_syscall - 1024]
#else
		SYSCALL2POINTER sys_call_table[_S(__NR_afs_syscall)]
#endif
		== afs_syscall) {
	    printf("AFS syscall entry point already in use!\n");
	    return -EBUSY;
	}
#if defined(AFS_IA64_LINUX20_ENV)
	afs_ni_syscall = sys_call_table[__NR_afs_syscall - 1024];
	sys_call_table[__NR_afs_syscall - 1024] =
		POINTER2SYSCALL((struct fptr *)afs_syscall_stub)->ip;
#else /* AFS_IA64_LINUX20_ENV */
	afs_ni_syscall = sys_call_table[_S(__NR_afs_syscall)];
	sys_call_table[_S(__NR_afs_syscall)] = POINTER2SYSCALL afs_syscall;
#ifdef AFS_SPARC64_LINUX20_ENV
	afs_ni_syscall32 = sys_call_table32[__NR_afs_syscall];
	sys_call_table32[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall32;
#endif
#endif /* AFS_IA64_LINUX20_ENV */
#ifdef AFS_AMD64_LINUX20_ENV
	if (ia32_sys_call_table) {
	    ia32_ni_syscall = ia32_sys_call_table[__NR_ia32_afs_syscall];
	    ia32_sys_call_table[__NR_ia32_afs_syscall] =
		    POINTER2SYSCALL afs_syscall;
	}
#endif /* AFS_S390_LINUX22_ENV */
#ifndef EXPORTED_SYS_CALL_TABLE
    }
#endif /* EXPORTED_SYS_CALL_TABLE */
    osi_Init();
    register_filesystem(&afs_fs_type);

    /* Intercept setgroups calls */
    if (sys_call_table) {
#if defined(AFS_IA64_LINUX20_ENV)
    sys_setgroupsp = (void *)&sys_setgroups;

    ((struct fptr *)sys_setgroupsp)->ip =
	SYSCALL2POINTER sys_call_table[__NR_setgroups - 1024];
    ((struct fptr *)sys_setgroupsp)->gp = kernel_gp;

    sys_call_table[__NR_setgroups - 1024] =
	POINTER2SYSCALL((struct fptr *)afs_xsetgroups_stub)->ip;
#else /* AFS_IA64_LINUX20_ENV */
    sys_setgroupsp = SYSCALL2POINTER sys_call_table[_S(__NR_setgroups)];
    sys_call_table[_S(__NR_setgroups)] = POINTER2SYSCALL afs_xsetgroups;
#ifdef AFS_SPARC64_LINUX20_ENV
    sys32_setgroupsp = SYSCALL2POINTER sys_call_table32[__NR_setgroups];
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL afs32_xsetgroups;
#endif /* AFS_SPARC64_LINUX20_ENV */
#if defined(__NR_setgroups32)
    sys_setgroups32p = SYSCALL2POINTER sys_call_table[__NR_setgroups32];
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL afs_xsetgroups32;
#ifdef AFS_SPARC64_LINUX20_ENV
	sys32_setgroups32p =
	    SYSCALL2POINTER sys_call_table32[__NR_setgroups32];
	sys_call_table32[__NR_setgroups32] =
	    POINTER2SYSCALL afs32_xsetgroups32;
#endif /* AFS_SPARC64_LINUX20_ENV */
#endif /* __NR_setgroups32 */
#ifdef AFS_AMD64_LINUX20_ENV
    if (ia32_sys_call_table) {
	sys32_setgroupsp =
	    SYSCALL2POINTER ia32_sys_call_table[__NR_ia32_setgroups];
	ia32_sys_call_table[__NR_ia32_setgroups] =
	    POINTER2SYSCALL afs32_xsetgroups;
#if defined(__NR_ia32_setgroups32)
	sys32_setgroups32p =
	    SYSCALL2POINTER ia32_sys_call_table[__NR_ia32_setgroups32];
	ia32_sys_call_table[__NR_ia32_setgroups32] =
	    POINTER2SYSCALL afs32_xsetgroups32;
#endif /* __NR_ia32_setgroups32 */
    }
#endif /* AFS_AMD64_LINUX20_ENV */
#endif /* AFS_IA64_LINUX20_ENV */

    }

    osi_sysctl_init();
    afsproc_init();

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
void __exit
afs_cleanup(void)
#else
void
cleanup_module(void)
#endif
{
    osi_sysctl_clean();
    if (sys_call_table) {
#if defined(AFS_IA64_LINUX20_ENV)
    sys_call_table[__NR_setgroups - 1024] =
	POINTER2SYSCALL((struct fptr *)sys_setgroupsp)->ip;
    sys_call_table[__NR_afs_syscall - 1024] = afs_ni_syscall;
#else /* AFS_IA64_LINUX20_ENV */
    sys_call_table[_S(__NR_setgroups)] = POINTER2SYSCALL sys_setgroupsp;
    sys_call_table[_S(__NR_afs_syscall)] = afs_ni_syscall;
# ifdef AFS_SPARC64_LINUX20_ENV
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL sys32_setgroupsp;
    sys_call_table32[__NR_afs_syscall] = afs_ni_syscall32;
# endif
# if defined(__NR_setgroups32)
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL sys_setgroups32p;
# ifdef AFS_SPARC64_LINUX20_ENV
	sys_call_table32[__NR_setgroups32] =
	    POINTER2SYSCALL sys32_setgroups32p;
# endif
# endif
#endif /* AFS_IA64_LINUX20_ENV */
#ifdef AFS_AMD64_LINUX20_ENV
    if (ia32_sys_call_table) {
	ia32_sys_call_table[__NR_ia32_setgroups] =
	    POINTER2SYSCALL sys32_setgroupsp;
	ia32_sys_call_table[__NR_ia32_afs_syscall] =
	    POINTER2SYSCALL ia32_ni_syscall;
# if defined(__NR_setgroups32)
	ia32_sys_call_table[__NR_ia32_setgroups32] =
	    POINTER2SYSCALL sys32_setgroups32p;
#endif
    }
#endif
    }
    unregister_filesystem(&afs_fs_type);

    osi_linux_free_inode_pages();	/* Invalidate all pages using AFS inodes. */
    osi_linux_free_afs_memory();

    afsproc_exit();
    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
module_init(afs_init);
module_exit(afs_cleanup);
#endif


#if !defined(AFS_LINUX24_ENV)
static long
get_page_offset(void)
{
#if defined(AFS_PPC_LINUX22_ENV) || defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV) || defined(AFS_ALPHA_LINUX20_ENV) || defined(AFS_S390_LINUX22_ENV) || defined(AFS_IA64_LINUX20_ENV) || defined(AFS_PARISC_LINUX24_ENV) || defined(AFS_AMD64_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
    return PAGE_OFFSET;
#else
    struct task_struct *p, *q;

    /* search backward thru the circular list */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    read_lock(&tasklist_lock);
#endif
    /* search backward thru the circular list */
#ifdef DEFINED_PREV_TASK
    for (q = current; p = q; q = prev_task(p)) {
#else
    for (p = current; p; p = p->prev_task) {
#endif
	if (p->pid == 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	    read_unlock(&tasklist_lock);
#endif
	    return p->addr_limit.seg;
	}
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    read_unlock(&tasklist_lock);
#endif
    return 0;
#endif
}
#endif /* !AFS_LINUX24_ENV */
