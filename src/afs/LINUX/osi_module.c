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
afsproc_init(void)
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
afsproc_exit(void)
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
    struct afsprocdata32 sysargs32;

    if (cmd != VIOC_SYSCALL) return -EINVAL;

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV)
#ifdef AFS_SPARC64_LINUX24_ENV
    if (current->thread.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_SPARC64_LINUX20_ENV)
    if (current->tss.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_AMD64_LINUX20_ENV)
#ifdef AFS_LINUX26_ENV
    if (test_thread_flag(TIF_IA32))
#else
    if (current->thread.flags & THREAD_IA32)
#endif
#elif defined(AFS_PPC64_LINUX20_ENV)
#ifdef AFS_PPC64_LINUX26_ENV
    if (current->thread_info->flags & _TIF_32BIT)
#else /*Linux 2.6 */
    if (current->thread.flags & PPC_FLAG_32BIT)
#endif
#elif defined(AFS_S390X_LINUX20_ENV)
    if (current->thread.flags & S390_FLAG_31BIT)
#else
#error Not done for this linux type
#endif
    {
	if (copy_from_user(&sysargs32, (void *)arg,
			   sizeof(struct afsprocdata32)))
	    return -EFAULT;

	return afs_syscall((unsigned long)sysargs32.syscall,
			   (unsigned long)sysargs32.param1,
			   (unsigned long)sysargs32.param2,
			   (unsigned long)sysargs32.param3,
			   (unsigned long)sysargs32.param4);
    } else
#endif
    {
	if (copy_from_user(&sysargs, (void *)arg, sizeof(struct afsprocdata)))
	    return -EFAULT;

	return afs_syscall(sysargs.syscall, sysargs.param1,
			   sysargs.param2, sysargs.param3, sysargs.param4);
    }
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
int __init
afs_init(void)
#else
int
init_module(void)
#endif
{
    int e;
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

    osi_Init();

    e = osi_syscall_init();
    if (e) return e;
    register_filesystem(&afs_fs_type);
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
    osi_syscall_clean();
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
